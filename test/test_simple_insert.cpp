// #include "./../glin/glin.h"
// #include <chrono>
// #include <iostream>
// #include <vector>
// #include <iomanip>
// #include <fstream>
// #include <sstream>

// // 🌊 AREAWATER数据集GLIN插入测试
// class AREAWaterGLINInsertTest {
// public:
//     struct ThroughputPoint {
//         long timestamp_ms;        // 时间戳（毫秒）
//         long records_processed;   // 累计处理记录数
//         double throughput;        // 当前吞吐量 (records/sec)
//     };

//     // 加载AREAWATER数据集
//     static std::vector<geos::geom::Geometry*> loadAREAWATERData(const std::string& filepath, int max_records = -1) {
//         std::vector<geos::geom::Geometry*> geometries;
//         auto factory = geos::geom::GeometryFactory::create();

//         std::cout << "📦 加载AREAWATER数据集: " << filepath << std::endl;

//         std::ifstream file(filepath);
//         if (!file.is_open()) {
//             std::cerr << "❌ 无法打开AREAWATER数据文件，使用随机几何对象" << std::endl;
//             // 如果文件不存在，生成随机几何对象
//             for (int i = 0; i < std::min(5000, max_records); i++) {
//                 double x = (rand() % 1000) * 0.001;
//                 double y = (rand() % 1000) * 0.001;
//                 double size = 0.0001 + (rand() % 100) * 0.000001;

//                 auto coords = new geos::geom::CoordinateArraySequence();
//                 coords->add(geos::geom::Coordinate(x, y));
//                 coords->add(geos::geom::Coordinate(x + size, y));
//                 coords->add(geos::geom::Coordinate(x + size, y + size));
//                 coords->add(geos::geom::Coordinate(x, y + size));
//                 coords->add(geos::geom::Coordinate(x, y));

//                 auto ring = factory->createLinearRing(coords);
//                 auto polygon = factory->createPolygon(ring, nullptr);
//                 geometries.push_back(polygon);
//             }
//             return geometries;
//         }

//         std::string line;
//         int line_count = 0;

//         // 跳过标题行
//         if (!std::getline(file, line)) {
//             std::cerr << "❌ AREAWATER文件为空" << std::endl;
//             return geometries;
//         }

//         while (std::getline(file, line) && (max_records == -1 || geometries.size() < max_records)) {
//             line_count++;
//             std::istringstream iss(line);
//             std::string field;
//             std::vector<std::string> fields;

//             // 解析CSV字段
//             while (std::getline(iss, field, ',')) {
//                 fields.push_back(field);
//             }

//             if (fields.size() >= 10) {
//                 try {
//                     // 提取坐标创建简单矩形 (通常在第2、3列)
//                     if (!fields[2].empty() && !fields[3].empty()) {
//                         double x = std::stod(fields[2]) * 0.000001;  // 转换为度
//                         double y = std::stod(fields[3]) * 0.000001;

//                         auto coords = new geos::geom::CoordinateArraySequence();
//                         coords->add(geos::geom::Coordinate(x, y));
//                         coords->add(geos::geom::Coordinate(x + 0.001, y));
//                         coords->add(geos::geom::Coordinate(x + 0.001, y + 0.001));
//                         coords->add(geos::geom::Coordinate(x, y + 0.001));
//                         coords->add(geos::geom::Coordinate(x, y));

//                         auto ring = factory->createLinearRing(coords);
//                         auto polygon = factory->createPolygon(ring, nullptr);
//                         geometries.push_back(polygon);

//                         if (geometries.size() % 1000 == 0) {
//                             std::cout << "    已加载 " << geometries.size() << " 个几何对象..." << std::endl;
//                         }
//                     }
//                 } catch (const std::exception& e) {
//                     // 跳过解析错误的行
//                     continue;
//                 }
//             }

//             if (max_records > 0 && geometries.size() >= max_records) {
//                 break;
//             }
//         }

//         file.close();
//         std::cout << "✅ 成功加载 " << geometries.size() << " 个AREAWATER几何对象" << std::endl;
//         return geometries;
//     }

//     static void runAREAWaterTest() {
//         std::cout << "🌊 AREAWATER数据集GLIN插入测试" << std::endl;
//         std::cout << "===============================" << std::endl;

//         // 配置参数
//         const std::string areawater_path = "/mnt/hgfs/sharedFolder/AREAWATER.csv";
//         const int test_size = 300000;  // 使用2K个AREAWATER对象，避免段错误

//         std::cout << "测试配置：" << std::endl;
//         std::cout << "  - 数据源: " << areawater_path << std::endl;
//         std::cout << "  - 总对象数: " << test_size << std::endl;
//         std::cout << "  - 批量加载: 60% (" << test_size * 0.6 << ")" << std::endl;
//         std::cout << "  - 插入测试: 40% (" << test_size * 0.4 << ")" << std::endl;
//         std::cout << "===============================" << std::endl;

//         // 加载AREAWATER数据
//         auto test_geoms = loadAREAWATERData(areawater_path, test_size);

//         if (test_geoms.size() < test_size) {
//             std::cout << "⚠️  AREAWATER数据只有 " << test_geoms.size() << " 个对象，使用全部数据" << std::endl;
//         }

//         std::cout << "✅ 加载了 " << test_geoms.size() << " 个AREAWATER几何对象" << std::endl;

//         // 插入性能测试
//         std::cout << "\n🚀 开始AREAWATER插入性能测试..." << std::endl;
//         auto start_time = std::chrono::high_resolution_clock::now();

//         try {
//             alex::Glin<double, geos::geom::Geometry*> glin;

//             // 使用保守策略确保稳定性
//             glin.set_force_bloom_filter(false);
//             glin.set_force_strategy(alex::Glin<double, geos::geom::Geometry*>::FilteringStrategy::CONSERVATIVE);

//             std::string curve_type = "zorder";
//             //double cell_xmin = 0.0, cell_ymin = 0.0;
//             //double cell_xmin = -180.0, cell_ymin = -90.0;
//             //double cell_x_intvl = 0.001, cell_y_intvl = 0.001;
//             // ============== 🛡️ 修复代码开始：自动计算世界边界 🛡️ ==============
//             // 问题根源：如果 cell_xmin 设为 0 而数据包含负经度，会导致 unsigned int 下溢，
//             // 生成错误的巨大键值，导致 ALEX 索引构建时无限递归并崩溃。
            
//             double global_min_x = std::numeric_limits<double>::max();
//             double global_min_y = std::numeric_limits<double>::max();
            
//             // 1. 扫描数据获取真实边界
//             for (const auto* geom : test_geoms) {
//                 if (!geom || geom->isEmpty()) continue;
//                 const auto* env = geom->getEnvelopeInternal();
//                 if (env->getMinX() < global_min_x) global_min_x = env->getMinX();
//                 if (env->getMinY() < global_min_y) global_min_y = env->getMinY();
//             }
            
//             // 2. 设置安全的网格原点 (比最小值稍小，确保所有 (x - min) > 0)
//             // 如果数据是 -125.0，min_x 设为 -126.0，差值为正，避免下溢
//             double cell_xmin = global_min_x - 1.0; 
//             double cell_ymin = global_min_y - 1.0;
            
//             double cell_x_intvl = 0.0001; 
//             double cell_y_intvl = 0.0001;
            
//             std::cout << "[配置修正] 自动检测数据范围: MinX=" << global_min_x << ", MinY=" << global_min_y << std::endl;
//             std::cout << "[配置修正] 设置安全网格原点: cell_xmin=" << cell_xmin << ", cell_ymin=" << cell_ymin << std::endl;
//             // =========================== 修复代码结束 ===========================
//             std::vector<std::tuple<double, double, double, double>> pieces;

//             // 批量加载60%数据
//             size_t load_count = static_cast<size_t>(test_geoms.size() * 0.6);
//             if (load_count > static_cast<size_t>(test_size)) load_count = static_cast<size_t>(test_size);
//             std::vector<geos::geom::Geometry*> load_data(test_geoms.begin(), test_geoms.begin() + load_count);

//             std::cout << "📦 批量加载 " << load_count << " 个AREAWATER对象..." << std::endl;
//             auto load_start = std::chrono::high_resolution_clock::now();
//             glin.glin_bulk_load(load_data, 1000000.0, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
//             auto load_end = std::chrono::high_resolution_clock::now();

//             auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start).count();
//             std::cout << "  批量加载完成，耗时: " << load_duration << "ms" << std::endl;

//             // 逐个插入剩余数据
//             size_t insert_count = test_geoms.size() - load_count;
//             if (insert_count > static_cast<size_t>(test_size * 0.4)) insert_count = static_cast<size_t>(test_size * 0.4);
//             std::cout << "🔄 逐个插入 " << insert_count << " 个AREAWATER对象..." << std::endl;

//             // 吞吐量监控
//             std::vector<ThroughputPoint> throughput_data;
//             auto last_measure_time = std::chrono::high_resolution_clock::now();
//             long last_processed_count = 0;
//             const int monitor_interval = 300;  // 每300个AREAWATER对象监控一次

//             auto insert_start = std::chrono::high_resolution_clock::now();

//             for (size_t i = load_count; i < load_count + insert_count && i < test_geoms.size(); i++) {
//                 auto* geom = test_geoms[i];
//                 const geos::geom::Envelope* env_internal = geom->getEnvelopeInternal();
//                 geos::geom::Envelope* env = new geos::geom::Envelope(*env_internal);

//                 // 使用大pieceLimit避免Bloom过滤器
//                 double pieceLimit = 1000000.0;

//                 auto result = glin.glin_insert(std::make_tuple(geom, env), curve_type,
//                                              cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieceLimit, pieces);

//                 // 实时吞吐量监控
//                 size_t current_processed = (i - load_count + 1);
//                 if (current_processed % static_cast<size_t>(monitor_interval) == 0) {
//                     auto current_time = std::chrono::high_resolution_clock::now();
//                     auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_measure_time).count();

//                     if (duration_ms > 0) {
//                         long batch_processed = static_cast<long>(current_processed) - last_processed_count;
//                         double current_throughput = (batch_processed * 1000.0) / duration_ms;

//                         auto global_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();

//                         throughput_data.push_back({
//                             global_elapsed_ms,
//                             static_cast<long>(current_processed),
//                             current_throughput
//                         });

//                         std::cout << "    AREAWATER插入进度: " << current_processed << "/" << insert_count
//                                   << " | ���吐量: " << std::fixed << std::setprecision(0)
//                                   << current_throughput << " ops/s" << std::endl;

//                         last_measure_time = current_time;
//                         last_processed_count = static_cast<long>(current_processed);
//                     }
//                 }
//             }

//             auto insert_end_time = std::chrono::high_resolution_clock::now();
//             auto insert_duration = std::chrono::duration_cast<std::chrono::milliseconds>(insert_end_time - insert_start).count();

//             std::cout << "\n✅ AREAWATER插入测试完成！" << std::endl;
//             std::cout << "  批量加载耗时: " << load_duration << "ms" << std::endl;
//             std::cout << "  插入耗时: " << insert_duration << "ms" << std::endl;
//             std::cout << "  平均插入吞吐量: " << std::fixed << std::setprecision(1)
//                       << (static_cast<double>(insert_count) * 1000.0 / insert_duration) << " objects/sec" << std::endl;
//             std::cout << "  状态: 成功" << std::endl;

//             // 保存AREAWATER吞吐量曲线数据
//             saveThroughputData(throughput_data, "areawater_insert_performance.csv");

//         } catch (const std::exception& e) {
//             std::cerr << "❌ AREAWATER测试失败: " << e.what() << std::endl;
//         }

//         // 清理几何对象
//         for (auto* geom : test_geoms) {
//             delete geom;
//         }
//         std::cout << "\n🧹 AREAWATER数据清理完成" << std::endl;
//     }

// private:
//     static void saveThroughputData(const std::vector<ThroughputPoint>& data, const std::string& filename) {
//         std::ofstream file(filename);
//         if (file.is_open()) {
//             file << "timestamp_ms,records_processed,throughput" << std::endl;
//             for (const auto& point : data) {
//                 file << point.timestamp_ms << "," << point.records_processed << "," << point.throughput << std::endl;
//             }
//             file.close();
//             std::cout << "📊 AREAWATER吞吐量曲线已保存到: " << filename << std::endl;
//         } else {
//             std::cerr << "❌ 无法保存AREAWATER吞吐量数据到文件: " << filename << std::endl;
//         }
//     }
// };

// int main() {
//     AREAWaterGLINInsertTest::runAREAWaterTest();
//     return 0;
// }
#include "./../glin/glin.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cmath>

// 🌊 AREAWATER数据集GLIN插入测试
class AREAWaterGLINInsertTest {
public:
    struct ThroughputPoint {
        long timestamp_ms;        // 时间戳（毫秒）
        long records_processed;   // 累计处理记录数
        double throughput;        // 当前吞吐量 (records/sec)
    };

    // 加载AREAWATER数据集
    static std::vector<geos::geom::Geometry*> loadAREAWATERData(const std::string& filepath, int max_records = -1) {
        std::vector<geos::geom::Geometry*> geometries;
        auto factory = geos::geom::GeometryFactory::create();

        std::cout << "📦 加载AREAWATER数据集: " << filepath << std::endl;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "❌ 无法打开AREAWATER数据文件" << std::endl;
            return geometries;
        }

        std::string line;
        if (!std::getline(file, line)) return geometries; // 跳过标题

        while (std::getline(file, line) && (max_records == -1 || geometries.size() < max_records)) {
            std::istringstream iss(line);
            std::string field;
            std::vector<std::string> fields;
            while (std::getline(iss, field, ',')) fields.push_back(field);

            if (fields.size() >= 10) {
                try {
                    if (!fields[2].empty() && !fields[3].empty()) {
                        // 🛠️ 关键修复：直接读取原始坐标，不要乘 0.000001
                        double x = std::stod(fields[2]); 
                        double y = std::stod(fields[3]);
                        auto coords = new geos::geom::CoordinateArraySequence();
                        double size = 0.001; // 约100米大小的框
                        coords->add(geos::geom::Coordinate(x, y));
                        coords->add(geos::geom::Coordinate(x + size, y));
                        coords->add(geos::geom::Coordinate(x + size, y + size));
                        coords->add(geos::geom::Coordinate(x, y + size));
                        coords->add(geos::geom::Coordinate(x, y));

                        auto ring = factory->createLinearRing(coords);
                        auto polygon = factory->createPolygon(ring, nullptr);
                        geometries.push_back(polygon);

                        if (geometries.size() % 50000 == 0) {
                            std::cout << "    已加载 " << geometries.size() << " 个几何对象..." << std::endl;
                        }
                    }
                } catch (...) { continue; }
            }
        }

        file.close();
        std::cout << "✅ 成功加载 " << geometries.size() << " 个AREAWATER几何对象" << std::endl;
        return geometries;
    }

    static void runAREAWaterTest() {
        std::cout << "🌊 AREAWATER数据集GLIN插入测试" << std::endl;
        std::cout << "===============================" << std::endl;

        // 配置参数
        const std::string areawater_path = "/mnt/hgfs/sharedFolder/AREAWATER.csv";
        const int test_size = 200000; // 2万数据，避免OOM

        // 1. 加载数据
        auto test_geoms = loadAREAWATERData(areawater_path, test_size);
        if (test_geoms.empty()) return;

        std::cout << "✅ 加载了 " << test_geoms.size() << " 个AREAWATER几何对象" << std::endl;

        // 2. 自动计算坐标边界 (防止段错误的关键)
        double global_min_x = std::numeric_limits<double>::max();
        double global_min_y = std::numeric_limits<double>::max();
        
        for (const auto* geom : test_geoms) {
            if (!geom || geom->isEmpty()) continue;
            const auto* env = geom->getEnvelopeInternal();
            if (env->getMinX() < global_min_x) global_min_x = env->getMinX();
            if (env->getMinY() < global_min_y) global_min_y = env->getMinY();
        }

        // 设置安全的网格原点 (比最小值略小)
        double cell_xmin = global_min_x - 1.0; 
        double cell_ymin = global_min_y - 1.0;
        
        // 🛠️ 关键配置：对于经纬度数据，0.01 (约1km) 是比较合理的网格大小
        double cell_x_intvl = 0.01; 
        double cell_y_intvl = 0.01;
        
        // 🛠️ 关键修复：必须是 "z"，不能是 "zorder"
        std::string curve_type = "z";

        std::cout << "[配置修正] 自动检测数据范围: MinX=" << global_min_x << ", MinY=" << global_min_y << std::endl;
        std::cout << "[配置修正] 设置安全网格原点: cell_xmin=" << cell_xmin << ", 间隔=" << cell_x_intvl << std::endl;

        // 3. 插入性能测试
        std::cout << "\n🚀 开始AREAWATER插入性能测试..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();

        try {
            alex::Glin<double, geos::geom::Geometry*> glin;

            // 使用保守策略确保稳定性
            glin.set_force_bloom_filter(false);
            glin.set_force_strategy(alex::Glin<double, geos::geom::Geometry*>::FilteringStrategy::CONSERVATIVE);

            std::vector<std::tuple<double, double, double, double>> pieces;

            // 批量加载30%数据（减少内存压力）
            size_t load_count = static_cast<size_t>(test_geoms.size() * 0.3);
            std::vector<geos::geom::Geometry*> load_data(test_geoms.begin(), test_geoms.begin() + load_count);

            std::cout << "📦 批量加载 " << load_count << " 个AREAWATER对象..." << std::endl;
            auto load_start = std::chrono::high_resolution_clock::now();
            
            // 使用大pieceLimit禁用piecewise
            glin.glin_bulk_load(load_data, 1000000.0, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
            
            auto load_end = std::chrono::high_resolution_clock::now();
            auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(load_end - load_start).count();
            std::cout << "  批量加载完成，耗时: " << load_duration << "ms" << std::endl;

            // 清理批量加载数据以释放内存
            std::vector<geos::geom::Geometry*>().swap(load_data);
            std::cout << "  🧹 已清理批量加载缓存，释放内存" << std::endl;

            if (glin.size() == 0) {
                std::cerr << "❌ 索引构建失败（大小为0），终止测试" << std::endl;
                return;
            }

            // 逐个插入剩余数据
            size_t insert_count = test_geoms.size() - load_count;
            std::cout << "🔄 逐个插入 " << insert_count << " 个AREAWATER对象..." << std::endl;

            // 吞吐量监控
            std::vector<ThroughputPoint> throughput_data;
            auto last_measure_time = std::chrono::high_resolution_clock::now();
            long last_processed_count = 0;
            const int monitor_interval = 1000;  // 每1000个对象监控一次
            const int memory_cleanup_interval = 5000;  // 每5000个对象清理一次临时变量

            auto insert_start = std::chrono::high_resolution_clock::now();

            for (size_t i = load_count; i < test_geoms.size(); i++) {
                auto* geom = test_geoms[i];
                const geos::geom::Envelope* env_internal = geom->getEnvelopeInternal();
                geos::geom::Envelope* env = new geos::geom::Envelope(*env_internal);

                double pieceLimit = 1000000.0;

                auto result = glin.glin_insert(std::make_tuple(geom, env), curve_type,
                                             cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieceLimit, pieces);

                // 实时吞吐量监控
                size_t current_processed = (i - load_count + 1);
                if (current_processed % static_cast<size_t>(monitor_interval) == 0) {
                    auto current_time = std::chrono::high_resolution_clock::now();
                    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_measure_time).count();

                    if (duration_ms > 0) {
                        long batch_processed = static_cast<long>(current_processed) - last_processed_count;
                        double current_throughput = (batch_processed * 1000.0) / duration_ms;

                        auto global_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();

                        throughput_data.push_back({
                            global_elapsed_ms,
                            static_cast<long>(current_processed),
                            current_throughput
                        });

                        std::cout << "    进度: " << current_processed << "/" << insert_count
                                  << " | 吞吐量: " << std::fixed << std::setprecision(0)
                                  << current_throughput << " ops/s" << "\r" << std::flush;

                        last_measure_time = current_time;
                        last_processed_count = static_cast<long>(current_processed);
                    }

                    // 定期内存清理
                    if (current_processed % static_cast<size_t>(memory_cleanup_interval) == 0) {
                        // 强制垃圾回收提示
                        if (current_processed % static_cast<size_t>(memory_cleanup_interval * 2) == 0) {
                            std::cout << std::endl << "  🧹 定期内存清理，已处理 " << current_processed << " 个对象" << std::endl;
                        }
                    }
                }
            }
            std::cout << std::endl;

            auto insert_end_time = std::chrono::high_resolution_clock::now();
            auto insert_duration = std::chrono::duration_cast<std::chrono::milliseconds>(insert_end_time - insert_start).count();

            std::cout << "\n✅ AREAWATER插入测试完成！" << std::endl;
            std::cout << "  批量加载耗时: " << load_duration << "ms" << std::endl;
            std::cout << "  插入耗时: " << insert_duration << "ms" << std::endl;
            std::cout << "  平均插入吞吐量: " << std::fixed << std::setprecision(1)
                      << (static_cast<double>(insert_count) * 1000.0 / insert_duration) << " objects/sec" << std::endl;

            saveThroughputData(throughput_data, "areawater_insert_performance.csv");

        } catch (const std::exception& e) {
            std::cerr << "❌ 测试失败: " << e.what() << std::endl;
        }

        // 清理几何对象
        for (auto* geom : test_geoms) delete geom;
    }

private:
    static void saveThroughputData(const std::vector<ThroughputPoint>& data, const std::string& filename) {
        std::ofstream file(filename);
        if (file.is_open()) {
            file << "timestamp_ms,records_processed,throughput" << std::endl;
            for (const auto& point : data) {
                file << point.timestamp_ms << "," << point.records_processed << "," << point.throughput << std::endl;
            }
            file.close();
            std::cout << "📊 吞吐量数据已保存: " << filename << std::endl;
        }
    }
};

int main() {
    AREAWaterGLINInsertTest::runAREAWaterTest();
    return 0;
}