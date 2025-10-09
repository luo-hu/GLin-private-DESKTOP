// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

/*
 * Simple benchmark that runs a mixture of point lookups and inserts on ALEX.
 */

#include "../core/alex.h"

#include <iomanip>

#include "flags.h"
#include "utils.h"

// Modify these if running your own workload
#define KEY_TYPE double
#define PAYLOAD_TYPE double

/*
 * Required flags：必要参数 5个
 * --keys_file              path to the file that contains keys                文件路径
 * --keys_file_type         file type of keys_file (options: binary or text)   文件类型（二进制或者文本）
 * --init_num_keys          number of keys to bulk load with                   批量家在的键数
 * --total_num_keys         total number of keys in the keys file              文件中键的总数
 * --batch_size             number of operations (lookup or insert) per batch  每个批处理操作（查找或插入）的数量
 *
 * Optional flags: 可选参数 4个
 * --insert_frac            fraction of operations that are inserts (instead of
 * lookups)                                                                     插入而非查询操作的比例
 * --lookup_distribution    lookup keys distribution (options: uniform or zipf) 查找键的分布类型
 * --time_limit             time limit, in minutes                              时间限制（分钟）
 * --print_batch_stats      whether to output stats for each batch              是否输出每个批处理的统计信息
 */
int main(int argc, char* argv[]) {
  auto flags = parse_flags(argc, argv);//数据解析函数，在flags.h中定义
  std::string keys_file_path = get_required(flags, "keys_file");
  std::string keys_file_type = get_required(flags, "keys_file_type");
  auto init_num_keys = stoi(get_required(flags, "init_num_keys"));
  auto total_num_keys = stoi(get_required(flags, "total_num_keys"));
  auto batch_size = stoi(get_required(flags, "batch_size"));
  auto insert_frac = stod(get_with_default(flags, "insert_frac", "0.5"));//默认每批操作（batch_size）的查询和插入操作各占一半
  std::string lookup_distribution =
      get_with_default(flags, "lookup_distribution", "zipf");
  auto time_limit = stod(get_with_default(flags, "time_limit", "0.5"));
  bool print_batch_stats = get_boolean_flag(flags, "print_batch_stats");

  // Read keys from file 从文件中读取键值
  auto keys = new KEY_TYPE[total_num_keys];
  if (keys_file_type == "binary") {
    load_binary_data(keys, total_num_keys, keys_file_path); //数据加载 在utils.h中定义
  } else if (keys_file_type == "text") {
    load_text_data(keys, total_num_keys, keys_file_path);
  } else {
    std::cerr << "--keys_file_type must be either 'binary' or 'text'"
              << std::endl;
    return 1;
  }

  // Combine bulk loaded keys with randomly generated payloads 将批量加载的键值与随机生成的有效载荷结合起来
  //简单讲就是将从文件加载的 “键（Key）” 与 “随机生成的值（Payload）” 组合成键值对（std::pair）数组，最终用于索引的初始化
  //开始动态分配键值对数组     KEY_TYPE 和 PAYLOAD_TYPE都是double类型,是基准测试的简化处理，非真实数据     在真实场景中（如unittest_glin.h的测试用例），PAYLOAD_TYPE会被定义为几何对象指针（如geos::geom::Geometry*）
  auto values = new std::pair<KEY_TYPE, PAYLOAD_TYPE>[init_num_keys];//我设置的初始值是50000
  std::mt19937_64 gen_payload(std::random_device{}());
  /*std::mt19937_64：这是 C++ 标准库中的64 位梅森旋转随机数生成器（性能高、随机性好，适合生成高质量随机数），变量名是 gen_payload（“生成 Payload 的生成器”）。
  std::random_device{}()：作为随机数生成器的 “种子（Seed）”。
  std::random_device 会尝试从系统硬件 / 环境获取 “真随机数”（如系统时间、硬件噪声等），确保每次程序运行时生成的随机值都不同（避免固定随机序列）。
  目的：为后续生成 “随机值（Payload）” 准备工具，模拟真实场景中 “键对应的数据”（比如数据库中 “主键” 对应的 “业务数据”）。*/

  for (int i = 0; i < init_num_keys; i++) {
    //开始给
    values[i].first = keys[i];
    values[i].second = static_cast<PAYLOAD_TYPE>(gen_payload());
    /*调用 gen_payload() 生成一个随机数，通过 static_cast 转换为 PAYLOAD_TYPE（这里是 double），赋值给 values[i] 的 “值部分”（std::pair 的 second 成员）。
    为什么用随机值？因为这个程序是性能基准测试（benchmark），不需要真实业务数据，随机值足以模拟 “键对应的数据”，同时避免数据相关性对性能测试的干扰。
    ALEX 索引的批量加载接口（bulk_load）需要接收 “有序的键值对数组” 作为输入（后面代码会对 values 排序），而这段代码的作用就是：
    “从文件加载的键” 与 “随机生成的值” 组装成符合接口要求的 “键值对数组”，让索引能一次性加载大量初始数据，避免逐次插入的开销，提升初始化效率。*/
  }

  // Create ALEX and bulk load 创建ALEX和批量加载
  alex::Alex<KEY_TYPE, PAYLOAD_TYPE> index;
  //对初始键值对values排序（因为ALEX索引的批量加载可能要求有序数据以优化性能）。
  std::sort(values, values + init_num_keys,
            [](auto const& a, auto const& b) { return a.first < b.first; });
  //将初始数据批量插入索引，作为测试的 “初始数据集”。
  index.bulk_load(values, init_num_keys);  

  // Run workload 运行工作负载
  int i = init_num_keys;//记录当前已插入的数量（初始值=初始加载的数量如500）
  long long cumulative_inserts = 0;
  long long cumulative_lookups = 0;
  int num_inserts_per_batch = static_cast<int>(batch_size * insert_frac);
  int num_lookups_per_batch = batch_size - num_inserts_per_batch;//默认每批操作（batch_size）的查询和插入各占一半
  double cumulative_insert_time = 0;
  double cumulative_lookup_time = 0;

  auto workload_start_time = std::chrono::high_resolution_clock::now();
  int batch_no = 0;
  PAYLOAD_TYPE sum = 0;
  std::cout << std::scientific;
  std::cout << std::setprecision(3);
  while (true) {//循环执行“插入+查询”批次操作（按batch_size分批次，总操作数由total_num_keys控制）
    batch_no++;

    // Do lookups 查询操作
    double batch_lookup_time = 0.0;
    if (i > 0) {
      KEY_TYPE* lookup_keys = nullptr;
      if (lookup_distribution == "uniform") {
        /*查询 key 不是随机生成的，而是从已插入的键（keys[0..i-1]） 中选取，确保查询有一定 “命中率”（避免全空查询导致性能数据失真）。
        具体生成方式由 --lookup_distribution 控制，对应两种分布,分别模拟均匀访问和热点访问
        */
        lookup_keys = get_search_keys(keys, i, num_lookups_per_batch);
      } else if (lookup_distribution == "zipf") {
        /*ALEX 作为学习型索引，会 “学习” 热门键的存储位置，优化其查找路径（如用更简单的模型预测热门键位置），因此在 zipf 分布下，ALEX 的吞吐量通常远高于传统 B + 树等结构。
        测试 zipf 分布能体现 GLIN（基于 ALEX）的核心优势。
        */
        lookup_keys = get_search_keys_zipf(keys, i, num_lookups_per_batch);
      } else {
        std::cerr << "--lookup_distribution must be either 'uniform' or 'zipf'"
                  << std::endl;
        return 1;
      }
      auto lookups_start_time = std::chrono::high_resolution_clock::now();//查询开始计时
      for (int j = 0; j < num_lookups_per_batch; j++) {//在一个查询批次内遍历，逐个查询，通过key找payload
        KEY_TYPE key = lookup_keys[j];//确定要查询的键
        PAYLOAD_TYPE* payload = index.get_payload(key);//获取查询键对应的值
        if (payload) {
          //找到值时进行累加，只是为了防止编译器优化
          sum += *payload;
        }
      }
      //查询结束计时
      auto lookups_end_time = std::chrono::high_resolution_clock::now();
      batch_lookup_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              lookups_end_time - lookups_start_time)
                              .count();
      //得到每个批次下的查询时间
      cumulative_lookup_time += batch_lookup_time;
      //总查询时间
      cumulative_lookups += num_lookups_per_batch;
      delete[] lookup_keys;
    }

    // Do inserts 插入(插入的位置不需要管，由ALEX索引底层自动计算)
    //计算本批能实际插入的数量（注意要考虑剩余数据量是否够一个批次）
    int num_actual_inserts = std::min( 
    num_inserts_per_batch, //单批次的数量
    total_num_keys - i); //剩余可插入的数量，所有插入操作（初始批量加载 + 后续批次插入）的总和必须等于total_num_keys。
    int num_keys_after_batch = i + num_actual_inserts;
    auto inserts_start_time = std::chrono::high_resolution_clock::now();
    //i是当前已插入的数量（包括最开始的批量插入和后续的逐个插入）
    for (; i < num_keys_after_batch; i++) {
      index.insert(keys[i], static_cast<PAYLOAD_TYPE>(gen_payload()));
    }
    auto inserts_end_time = std::chrono::high_resolution_clock::now();
    double batch_insert_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(inserts_end_time -
                                                             inserts_start_time)
            .count();
    cumulative_insert_time += batch_insert_time;
    cumulative_inserts += num_actual_inserts;

    if (print_batch_stats) {
      int num_batch_operations = num_lookups_per_batch + num_actual_inserts;
      double batch_time = batch_lookup_time + batch_insert_time;
      long long cumulative_operations = cumulative_lookups + cumulative_inserts;
      double cumulative_time = cumulative_lookup_time + cumulative_insert_time;
      std::cout << "Batch " << batch_no
                << ", cumulative ops: " << cumulative_operations
                << "\n\tbatch throughput:\t"
                << num_lookups_per_batch / batch_lookup_time * 1e9//单位需要换成纳秒
                << " lookups/sec,\t"
                << num_actual_inserts / batch_insert_time * 1e9
                << " inserts/sec,\t" << num_batch_operations / batch_time * 1e9
                << " ops/sec"
                << "\n\tcumulative throughput:\t"
                << cumulative_lookups / cumulative_lookup_time * 1e9
                << " lookups/sec,\t"
                << cumulative_inserts / cumulative_insert_time * 1e9
                << " inserts/sec,\t"
                << cumulative_operations / cumulative_time * 1e9 << " ops/sec"
                << std::endl;
    }

    // Check for workload end conditions
    if (num_actual_inserts < num_inserts_per_batch) {
      // End if we have inserted all keys in a workload with inserts
      break;
    }
    double workload_elapsed_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() - workload_start_time)
            .count();
    if (workload_elapsed_time > time_limit * 1e9 * 60) {
      break;
    }
  }

  long long cumulative_operations = cumulative_lookups + cumulative_inserts;
  double cumulative_time = cumulative_lookup_time + cumulative_insert_time;
  std::cout << "Cumulative stats: " << batch_no << " batches, "
            << cumulative_operations << " ops (" << cumulative_lookups
            << " lookups, " << cumulative_inserts << " inserts)"
            << "\n\tcumulative throughput:\t"
            << cumulative_lookups / cumulative_lookup_time * 1e9
            << " lookups/sec,\t"
            << cumulative_inserts / cumulative_insert_time * 1e9
            << " inserts/sec,\t"
            << cumulative_operations / cumulative_time * 1e9 << " ops/sec"
            << std::endl;

  delete[] keys;
  delete[] values;
}
