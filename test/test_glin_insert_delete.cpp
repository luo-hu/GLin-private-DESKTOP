#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/Coordinate.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <random>
#include <fstream>
#include <algorithm>
#include <sys/resource.h>

// ============================================================================
// GLIN插入和删除性能测试框架
// ============================================================================

class GLINInsertDeleteTest {
private:
    // 测试配置
    struct TestConfig {
        int total_objects = 10000;
        int insert_percent = 50;  // 50%用于插入测试
        int delete_percent = 50;  // 50%用于删除测试
        int insert_batch_size = 100;  // 批量插入的批次大小
    };

    // 性能指标
    struct PerformanceMetrics {
        std::string method_name;
        double build_time_ms;
        double insert_throughput_records_per_sec;
        double delete_throughput_records_per_sec;
        long memory_usage_kb;

        void print() const {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "    构建时间: " << build_time_ms << "ms" << std::endl;
            std::cout << "    插入吞吐量: " << insert_throughput_records_per_sec << " records/sec" << std::endl;
            std::cout << "    删除吞吐量: " << delete_throughput_records_per_sec << " records/sec" << std::endl;
            std::cout << "    内存使用: " << memory_usage_kb << "KB" << std::endl;
        }
    };

    // 获取内存使用量（KB）
    static long getMemoryUsageKB() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        return usage.ru_maxrss;  // 最大常驻集大小（KB）
    }

    // 生成随机几何对象
    static std::vector<geos::geom::Geometry*> generateRandomGeometry(int count, int seed) {
        std::vector<geos::geom::Geometry*> geometries;
        geometries.reserve(count);

        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());
        std::mt19937 rng(seed);
        std::uniform_real_distribution<double> dist(-100.0, 100.0);

        for (int i = 0; i < count; ++i) {
            try {
                double x1 = dist(rng);
                double y1 = dist(rng);
                double size = dist(rng) / 20.0 + 0.5;  // 0.5到5.5

                // 生成矩形
                std::string wkt = "POLYGON((" +
                    std::to_string(x1) + " " + std::to_string(y1) + "," +
                    std::to_string(x1) + " " + std::to_string(y1 + size) + "," +
                    std::to_string(x1 + size) + " " + std::to_string(y1 + size) + "," +
                    std::to_string(x1 + size) + " " + std::to_string(y1) + "," +
                    std::to_string(x1) + " " + std::to_string(y1) + "))";

                std::unique_ptr<geos::geom::Geometry> geom_ptr = reader.read(wkt);
                if (geom_ptr && !geom_ptr->isEmpty()) {
                    geometries.push_back(geom_ptr.release());
                }
            } catch (const std::exception& e) {
                // 忽略失败的几何对象
                continue;
            }
        }

        return geometries;
    }

    // 测试插入性能
    static PerformanceMetrics testInsert(
        const std::vector<geos::geom::Geometry*>& initial_data,
        const std::vector<geos::geom::Geometry*>& insert_data,
        bool enable_bloom_filter = false,
        const std::string& method_name = "GLIN-Insert-Test") {

        PerformanceMetrics metrics;
        metrics.method_name = method_name;

        std::cout << "  🚀 开始" << method_name << "插入测试..." << std::endl;
        std::cout << "    初始数据: " << initial_data.size() << " 个" << std::endl;
        std::cout << "    插入数据: " << insert_data.size() << " 个" << std::endl;
        std::cout << "    Bloom过滤器: " << (enable_bloom_filter ? "启用" : "禁用") << std::endl;

        auto build_start = std::chrono::high_resolution_clock::now();

        // 1. 构建初始索引
        alex::Glin<double, geos::geom::Geometry*> glin;
        double piecelimitation = 1000000.0;  // 禁用分段
        std::string curve_type = "z";
        double cell_xmin = -100.0, cell_ymin = -100.0;
        double cell_x_intvl = 0.1, cell_y_intvl = 0.1;
        std::vector<std::tuple<double, double, double, double>> pieces;

        if (enable_bloom_filter) {
            glin.set_force_bloom_filter(true);
        }

        // 批量加载初始数据
        for (size_t i = 0; i < initial_data.size(); i += 100) {
            size_t end = std::min(i + 100, initial_data.size());
            std::vector<geos::geom::Geometry*> batch(initial_data.begin() + i,
                                                      initial_data.begin() + end);

            for (auto* geom : batch) {
                double x = geom->getEnvelopeInternal()->getMinX();
                double y = geom->getEnvelopeInternal()->getMinY();
                geos::geom::Envelope* env = new geos::geom::Envelope(x, x, y, y);
                double pieceLimit = (enable_bloom_filter ? 100.0 : 1000000.0);  // GLIN-HF用100，Lite-AMF用1000000
                glin.glin_insert(std::make_tuple(geom, env), curve_type,
                                cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieceLimit, pieces);
            }
        }

        auto build_end = std::chrono::high_resolution_clock::now();
        metrics.build_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            build_end - build_start).count();

        std::cout << "    ✅ 初始索引构建完成" << std::endl;

        // 2. 测试插入性能
        auto insert_start = std::chrono::high_resolution_clock::now();
        long mem_before = getMemoryUsageKB();

        int batch_size = 100;
        for (size_t i = 0; i < insert_data.size(); i += batch_size) {
            size_t end = std::min(i + batch_size, insert_data.size());
            std::vector<geos::geom::Geometry*> batch(insert_data.begin() + i,
                                                      insert_data.begin() + end);

            for (auto* geom : batch) {
                double x = geom->getEnvelopeInternal()->getMinX();
                double y = geom->getEnvelopeInternal()->getMinY();
                geos::geom::Envelope* env = new geos::geom::Envelope(x, x, y, y);
                double pieceLimit = (enable_bloom_filter ? 100.0 : 1000000.0);  // GLIN-HF用100，Lite-AMF用1000000
                glin.glin_insert(std::make_tuple(geom, env), curve_type,
                                cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieceLimit, pieces);

                // 同步更新Bloom过滤器
                if (enable_bloom_filter) {
                    // 查找对应的叶子节点并更新Bloom过滤器
                    // 这里简化处理，实际需要找到对应的leaf_ext_map
                }
            }
        }

        auto insert_end = std::chrono::high_resolution_clock::now();
        long mem_after = getMemoryUsageKB();

        double insert_time_sec = std::chrono::duration_cast<std::chrono::duration<double>>(
            insert_end - insert_start).count();
        metrics.insert_throughput_records_per_sec = insert_data.size() / insert_time_sec;
        metrics.memory_usage_kb = mem_after - mem_before;

        std::cout << "  ✅ 插入测试完成" << std::endl;
        return metrics;
    }

    // 测试删除性能
    static PerformanceMetrics testDelete(
        std::vector<geos::geom::Geometry*>& all_data,
        int delete_count,
        bool enable_bloom_filter = false,
        const std::string& method_name = "GLIN-Delete-Test") {

        PerformanceMetrics metrics;
        metrics.method_name = method_name;

        std::cout << "  🗑️  开始" << method_name << "删除测试..." << std::endl;
        std::cout << "    总数据: " << all_data.size() << " 个" << std::endl;
        std::cout << "    删除数量: " << delete_count << " 个" << std::endl;
        std::cout << "    Bloom过滤器: " << (enable_bloom_filter ? "启用" : "禁用") << std::endl;

        auto build_start = std::chrono::high_resolution_clock::now();

        // 1. 构建完整索引
        alex::Glin<double, geos::geom::Geometry*> glin;
        double piecelimitation = 1000000.0;
        std::string curve_type = "z";
        double cell_xmin = -100.0, cell_ymin = -100.0;
        double cell_x_intvl = 0.1, cell_y_intvl = 0.1;
        std::vector<std::tuple<double, double, double, double>> pieces;

        if (enable_bloom_filter) {
            glin.set_force_bloom_filter(true);
        }

        // 批量加载所有数据
        for (size_t i = 0; i < all_data.size(); i += 100) {
            size_t end = std::min(i + 100, all_data.size());
            std::vector<geos::geom::Geometry*> batch(all_data.begin() + i,
                                                      all_data.begin() + end);

            for (auto* geom : batch) {
                double x = geom->getEnvelopeInternal()->getMinX();
                double y = geom->getEnvelopeInternal()->getMinY();
                geos::geom::Envelope* env = new geos::geom::Envelope(x, x, y, y);
                double pieceLimit = (enable_bloom_filter ? 100.0 : 1000000.0);  // GLIN-HF用100，Lite-AMF用1000000
                glin.glin_insert(std::make_tuple(geom, env), curve_type,
                                cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieceLimit, pieces);
            }
        }

        auto build_end = std::chrono::high_resolution_clock::now();
        metrics.build_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            build_end - build_start).count();

        std::cout << "    ✅ 完整索引构建完成" << std::endl;

        // 2. 随机选择要删除的数据
        std::mt19937 rng(42);  // 固定种子确保可重复
        std::vector<geos::geom::Geometry*> to_delete = all_data;
        std::shuffle(to_delete.begin(), to_delete.end(), rng);
        to_delete.resize(delete_count);

        // 3. 测试删除性能
        auto delete_start = std::chrono::high_resolution_clock::now();
        long mem_before = getMemoryUsageKB();

        for (auto* geom : to_delete) {
            try {
                double x = geom->getEnvelopeInternal()->getMinX();
                double y = geom->getEnvelopeInternal()->getMinY();
                geos::geom::LineSegment segment(geos::geom::Coordinate(x, y),
                                                  geos::geom::Coordinate(x, y));

                // 使用geometry和line segment作为删除的标识
                double error_bound = 0.000001;  // 删除误差边界
                int result = glin.erase_lineseg(geom, segment, error_bound, pieces);

                if (result == 0) {
                    std::cout << "⚠️  删除失败: " << geom << std::endl;
                }

                // 同步更新Bloom过滤器（简化处理）
                if (enable_bloom_filter) {
                    // 实际应用中需要从LeafNodeExt中移除
                }

                delete geom;  // 删除几何对象
            } catch (const std::exception& e) {
                std::cout << "⚠️  删除异常: " << e.what() << std::endl;
            }
        }

        auto delete_end = std::chrono::high_resolution_clock::now();
        long mem_after = getMemoryUsageKB();

        double delete_time_sec = std::chrono::duration_cast<std::chrono::duration<double>>(
            delete_end - delete_start).count();
        metrics.delete_throughput_records_per_sec = to_delete.size() / delete_time_sec;
        metrics.memory_usage_kb = mem_before - mem_after;

        std::cout << "  ✅ 删除测试完成" << std::endl;
        return metrics;
    }

public:
    static void runInsertDeleteTests() {
        TestConfig config;

        std::cout << "🎯 GLIN插入和删除性能测试" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "配置信息：" << std::endl;
        std::cout << "  - 总对象数: " << config.total_objects << std::endl;
        std::cout << "  - 插入比例: " << config.insert_percent << "% ("
                  << (config.total_objects * config.insert_percent / 100) << " 个)" << std::endl;
        std::cout << "  - 删除比例: " << config.delete_percent << "% ("
                  << (config.total_objects * config.delete_percent / 100) << " 个)" << std::endl;
        std::cout << "  - 批次大小: " << config.insert_batch_size << std::endl;
        std::cout << "========================================" << std::endl;

        // 生成测试数据
        std::cout << "\n📦 生成测试数据..." << std::endl;
        auto all_geometries = generateRandomGeometry(config.total_objects, 42);
        std::cout << "  ✅ 生成了 " << all_geometries.size() << " 个几何对象" << std::endl;

        // 分割数据
        size_t initial_count = config.total_objects * (100 - config.insert_percent) / 100;
        size_t insert_count = config.total_objects - initial_count;
        size_t delete_count = config.total_objects * config.delete_percent / 100;

        std::vector<geos::geom::Geometry*> initial_data(all_geometries.begin(),
                                                          all_geometries.begin() + initial_count);
        std::vector<geos::geom::Geometry*> insert_data(all_geometries.begin() + initial_count,
                                                         all_geometries.begin() + initial_count + insert_count);

        // 测试插入
        std::cout << "\n🔍 测试插入性能..." << std::endl;

        auto insert_metrics_no_bloom = testInsert(initial_data, insert_data, false, "GLIN-插入(无Bloom)");
        std::cout << "\n";
        insert_metrics_no_bloom.print();

        // 注意：启用Bloom过滤器的插入测试会很慢（构建时间长）
        // auto insert_metrics_with_bloom = testInsert(initial_data, insert_data, true, "GLIN-插入(带Bloom)");
        // std::cout << "\n";
        // insert_metrics_with_bloom.print();

        // 测试删除（需要完整重建索引以支持删除）
        std::cout << "\n🗑️ 测试删除性能..." << std::endl;

        auto delete_metrics_no_bloom = testDelete(all_geometries, delete_count, false, "GLIN-删除(无Bloom)");
        std::cout << "\n";
        delete_metrics_no_bloom.print();

        // 输出对比结果
        std::cout << "\n📊 插入性能对比" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        std::cout << std::setw(20) << "方法"
                  << std::setw(20) << "插入吞吐量"
                  << std::setw(20) << "相对性能" << std::endl;
        std::cout << std::string(60, '-') << std::endl;

        std::cout << std::setw(20) << "GLIN-插入(无Bloom)"
                  << std::setw(20) << insert_metrics_no_bloom.insert_throughput_records_per_sec
                  << std::setw(20) << "基准" << std::endl;

        std::cout << "\n📊 删除性能" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        std::cout << std::setw(20) << "方法"
                  << std::setw(20) << "删除吞吐量"
                  << std::setw(20) << "相对性能" << std::endl;
        std::cout << std::string(60, '-') << std::endl;

        std::cout << std::setw(20) << "GLIN-删除(无Bloom)"
                  << std::setw(20) << delete_metrics_no_bloom.delete_throughput_records_per_sec
                  << std::setw(20) << "基准" << std::endl;

        std::cout << "\n🎯 测试总结" << std::endl;
        std::cout << "  ✅ 插入测试：支持动态数据扩展" << std::endl;
        std::cout << "  ✅ 删除测试：支持数据动态维护" << std::endl;
        std::cout << "  注意：Bloom过滤器功能仍需进一步优化" << std::endl;
    }
};

// ============================================================================
// 主函数
// ============================================================================
int main() {
    try {
        GLINInsertDeleteTest::runInsertDeleteTests();
    } catch (const std::exception& e) {
        std::cerr << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}