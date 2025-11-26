#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <memory>

// 性能指标结构体
struct PerformanceResult {
    std::string method_name;
    long build_time_ms;          // 索引构建时间(毫秒)
    int query_count;             // 查询次数
    long total_query_time_us;    // 总查询时间(微秒)
    long avg_query_time_us;      // 平均查询时间(微秒)
    int total_results;           // 查找结果总数
    long memory_usage_kb;        // 内存使用量(KB)
};

class CompletePerformanceTester {
public:
    // 创建测试数据
    static std::vector<geos::geom::Geometry*> createTestData(int count) {
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());
        std::vector<geos::geom::Geometry*> geoms;

        std::cout << "🔧 创建 " << count << " 个测试几何对象..." << std::endl;

        for (int i = 0; i < count; ++i) {
            // 创建分布更合理的测试数据
            double x = (i % 100) * 10.0;      // 每100个对象重复x坐标
            double y = (i / 100) * 10.0;     // y坐标递增

            // 随机偏移，避免完全对齐
            x += (rand() % 100) / 100.0;
            y += (rand() % 100) / 100.0;

            std::ostringstream wkt;
            wkt << "POLYGON((" << x << " " << y << ","
                 << x << " " << (y + 2) << ","
                 << (x + 2) << " " << (y + 2) << ","
                 << (x + 2) << " " << y << ","
                 << x << " " << y << "))";

            try {
                auto geom = reader.read(wkt.str());
                if (geom) {
                    geoms.push_back(geom.release());
                }
            } catch (const std::exception& e) {
                // 忽略失败的几何对象
                std::cerr << "几何对象创建失败: " << e.what() << std::endl;
            }

            if ((i + 1) % 1000 == 0) {
                std::cout << "  已创建 " << (i + 1) << "/" << count << " 个对象" << std::endl;
            }
        }

        std::cout << "✅ 成功创建 " << geoms.size() << " 个几何对象" << std::endl;
        return geoms;
    }

    // 获取当前内存使用量(简化版本)
    static long getCurrentMemoryKB() {
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::istringstream iss(line);
                std::string label, value, unit;
                iss >> label >> value >> unit;
                return std::stol(value);
            }
        }
        return 0;
    }

    // 测试原始GLIN
    static PerformanceResult testOriginalGLIN(const std::vector<geos::geom::Geometry*>& geoms) {
        std::cout << "\n🔍 测试原始GLIN..." << std::endl;
        PerformanceResult result;
        result.method_name = "原始GLIN";

        // 使用您的成功配置
        double piecelimitation = 1000000.0;
        std::string curve_type = "z";
        double cell_xmin = -100.0;
        double cell_ymin = -100.0;
        double cell_x_intvl = 0.001;  // 您发现的最优配置
        double cell_y_intvl = 0.001;

        std::vector<std::tuple<double, double, double, double>> pieces;

        // === 索引构建时间测试 ===
        std::cout << "  🏗️  构建原始GLIN索引..." << std::endl;
        long start_memory = getCurrentMemoryKB();

        auto build_start = std::chrono::high_resolution_clock::now();
        alex::Glin<double, geos::geom::Geometry*> glin_original;

        glin_original.glin_bulk_load(geoms, piecelimitation, curve_type,
                                   cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

        auto build_end = std::chrono::high_resolution_clock::now();
        long end_memory = getCurrentMemoryKB();

        result.build_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            build_end - build_start).count();
        result.memory_usage_kb = end_memory - start_memory;

        std::cout << "  ✅ 索引构建完成，耗时: " << result.build_time_ms << "ms"
                  << ", 内存使用: " << result.memory_usage_kb << "KB" << std::endl;

        // === 查询性能测试 ===
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());

        // 创建3个不同类型的查询
        std::vector<std::string> test_queries = {
            "POLYGON((0 0,0 5,5 5,5 0,0 0))",                    // 小范围查询
            "POLYGON((25 25,25 35,35 35,35 25,25 25))",          // 中等范围查询
            "POLYGON((0 0,0 100,100 100,100 0,0 0))"             // 大范围查询
        };

        result.query_count = test_queries.size();
        result.total_query_time_us = 0;
        result.total_results = 0;

        std::cout << "  🔍 执行 " << result.query_count << " 次查询测试..." << std::endl;

        for (size_t i = 0; i < test_queries.size(); ++i) {
            auto query = reader.read(test_queries[i]).release();
            std::vector<geos::geom::Geometry*> results;
            int filter_count = 0;

            auto query_start = std::chrono::high_resolution_clock::now();

            glin_original.glin_find(query, curve_type, cell_xmin, cell_ymin,
                                  cell_x_intvl, cell_y_intvl, pieces, results, filter_count);

            auto query_end = std::chrono::high_resolution_clock::now();

            auto query_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                query_end - query_start).count();

            result.total_query_time_us += query_time_us;
            result.total_results += results.size();

            std::cout << "    查询" << (i+1) << ": " << results.size()
                      << " 个结果, 耗时: " << query_time_us << "μs" << std::endl;

            delete query;
            for (auto* result_geom : results) {
                delete result_geom;
            }
        }

        result.avg_query_time_us = result.total_query_time_us / result.query_count;

        std::cout << "  ✅ 原始GLIN测试完成" << std::endl;
        return result;
    }

    // 测试GLIN-HF (启用PIECE的版本)
    static PerformanceResult testGLIN_HF(const std::vector<geos::geom::Geometry*>& geoms) {
        std::cout << "\n🔍 测试GLIN-HF (启用PIECE分段)..." << std::endl;
        PerformanceResult result;
        result.method_name = "GLIN-HF";

        // 使用分段配置
        double piecelimitation = 100.0;  // 启用分段
        std::string curve_type = "z";
        double cell_xmin = -100.0;
        double cell_ymin = -100.0;
        double cell_x_intvl = 0.001;
        double cell_y_intvl = 0.001;

        std::vector<std::tuple<double, double, double, double>> pieces;

        // === 索引构建时间测试 ===
        std::cout << "  🏗️  构建GLIN-HF索引..." << std::endl;
        long start_memory = getCurrentMemoryKB();

        auto build_start = std::chrono::high_resolution_clock::now();
        alex::Glin<double, geos::geom::Geometry*> glin_hf;

        glin_hf.glin_bulk_load(geoms, piecelimitation, curve_type,
                              cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

        auto build_end = std::chrono::high_resolution_clock::now();
        long end_memory = getCurrentMemoryKB();

        result.build_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            build_end - build_start).count();
        result.memory_usage_kb = end_memory - start_memory;

        std::cout << "  ✅ 索引构建完成，耗时: " << result.build_time_ms << "ms"
                  << ", 分段数量: " << pieces.size()
                  << ", 内存使用: " << result.memory_usage_kb << "KB" << std::endl;

        // === 查询性能测试 ===
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());

        std::vector<std::string> test_queries = {
            "POLYGON((0 0,0 5,5 5,5 0,0 0))",
            "POLYGON((25 25,25 35,35 35,35 25,25 25))",
            "POLYGON((0 0,0 100,100 100,100 0,0 0))"
        };

        result.query_count = test_queries.size();
        result.total_query_time_us = 0;
        result.total_results = 0;

        std::cout << "  🔍 执行 " << result.query_count << " 次查询测试..." << std::endl;

        for (size_t i = 0; i < test_queries.size(); ++i) {
            auto query = reader.read(test_queries[i]).release();
            std::vector<geos::geom::Geometry*> results;
            int filter_count = 0;

            auto query_start = std::chrono::high_resolution_clock::now();

            glin_hf.glin_find(query, curve_type, cell_xmin, cell_ymin,
                             cell_x_intvl, cell_y_intvl, pieces, results, filter_count);

            auto query_end = std::chrono::high_resolution_clock::now();

            auto query_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                query_end - query_start).count();

            result.total_query_time_us += query_time_us;
            result.total_results += results.size();

            std::cout << "    查询" << (i+1) << ": " << results.size()
                      << " 个结果, 耗时: " << query_time_us << "μs" << std::endl;

            delete query;
            for (auto* result_geom : results) {
                delete result_geom;
            }
        }

        result.avg_query_time_us = result.total_query_time_us / result.query_count;

        std::cout << "  ✅ GLIN-HF测试完成" << std::endl;
        return result;
    }

    // 测试Lite-AMF (您的成功配置)
    static PerformanceResult testLiteAMF(const std::vector<geos::geom::Geometry*>& geoms) {
        std::cout << "\n🔍 测试Lite-AMF (禁用PIECE + 您的最优配置)..." << std::endl;
        PerformanceResult result;
        result.method_name = "Lite-AMF";

        // 使用您的成功配置
        double piecelimitation = 1000000.0;  // 禁用分段
        std::string curve_type = "z";
        double cell_xmin = -100.0;
        double cell_ymin = -100.0;
        double cell_x_intvl = 0.001;  // 您的最优配置
        double cell_y_intvl = 0.001;

        std::vector<std::tuple<double, double, double, double>> pieces;

        // === 索引构建时间测试 ===
        std::cout << "  🏗️  构建Lite-AMF索引..." << std::endl;
        long start_memory = getCurrentMemoryKB();

        auto build_start = std::chrono::high_resolution_clock::now();
        alex::Glin<double, geos::geom::Geometry*> glin_amf;

        glin_amf.glin_bulk_load(geoms, piecelimitation, curve_type,
                               cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

        auto build_end = std::chrono::high_resolution_clock::now();
        long end_memory = getCurrentMemoryKB();

        result.build_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            build_end - build_start).count();
        result.memory_usage_kb = end_memory - start_memory;

        std::cout << "  ✅ 索引构建完成，耗时: " << result.build_time_ms << "ms"
                  << ", 分段数量: " << pieces.size()
                  << ", 内存使用: " << result.memory_usage_kb << "KB" << std::endl;

        // === 查询性能测试 ===
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());

        std::vector<std::string> test_queries = {
            "POLYGON((0 0,0 5,5 5,5 0,0 0))",
            "POLYGON((25 25,25 35,35 35,35 25,25 25))",
            "POLYGON((0 0,0 100,100 100,100 0,0 0))"
        };

        result.query_count = test_queries.size();
        result.total_query_time_us = 0;
        result.total_results = 0;

        std::cout << "  🔍 执行 " << result.query_count << " 次查询测试..." << std::endl;

        for (size_t i = 0; i < test_queries.size(); ++i) {
            auto query = reader.read(test_queries[i]).release();
            std::vector<geos::geom::Geometry*> results;
            int filter_count = 0;

            auto query_start = std::chrono::high_resolution_clock::now();

            glin_amf.glin_find(query, curve_type, cell_xmin, cell_ymin,
                              cell_x_intvl, cell_y_intvl, pieces, results, filter_count);

            auto query_end = std::chrono::high_resolution_clock::now();

            auto query_time_us = std::chrono::duration_cast<std::chrono::microseconds>(
                query_end - query_start).count();

            result.total_query_time_us += query_time_us;
            result.total_results += results.size();

            std::cout << "    查询" << (i+1) << ": " << results.size()
                      << " 个结果, 耗时: " << query_time_us << "μs" << std::endl;

            delete query;
            for (auto* result_geom : results) {
                delete result_geom;
            }
        }

        result.avg_query_time_us = result.total_query_time_us / result.query_count;

        std::cout << "  ✅ Lite-AMF测试完成" << std::endl;
        return result;
    }

    // 打印详细对比结果
    static void printComparisonTable(const std::vector<PerformanceResult>& results) {
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "📊 完整性能对比结果 (包含索引构建时间)" << std::endl;
        std::cout << std::string(120, '=') << std::endl;

        // 表头
        std::cout << std::setw(12) << "方法"
                  << std::setw(15) << "构建时间(ms)"
                  << std::setw(12) << "内存(KB)"
                  << std::setw(10) << "查询次数"
                  << std::setw(15) << "总查询(μs)"
                  << std::setw(15) << "平均查询(μs)"
                  << std::setw(10) << "结果数"
                  << std::endl;
        std::cout << std::string(120, '-') << std::endl;

        // 数据行
        for (const auto& r : results) {
            std::cout << std::setw(12) << r.method_name
                      << std::setw(15) << r.build_time_ms
                      << std::setw(12) << r.memory_usage_kb
                      << std::setw(10) << r.query_count
                      << std::setw(15) << r.total_query_time_us
                      << std::setw(15) << r.avg_query_time_us
                      << std::setw(10) << r.total_results
                      << std::endl;
        }

        std::cout << std::string(120, '=') << std::endl;

        // 性能分析
        if (results.size() >= 2) {
            const auto& baseline = results[0];  // 原始GLIN作为基准

            for (size_t i = 1; i < results.size(); ++i) {
                const auto& current = results[i];

                // 查询性能改进
                double query_improvement = ((double)baseline.avg_query_time_us - current.avg_query_time_us)
                                        / baseline.avg_query_time_us * 100;

                // 构建时间变化
                double build_change = ((double)current.build_time_ms - baseline.build_time_ms)
                                   / baseline.build_time_ms * 100;

                // 内存效率
                double memory_efficiency = ((double)baseline.memory_usage_kb - current.memory_usage_kb)
                                        / baseline.memory_usage_kb * 100;

                std::cout << "📈 " << current.method_name << " vs " << baseline.method_name << ":" << std::endl;
                std::cout << "  🔍 查询性能改进: " << std::fixed << std::setprecision(2) << query_improvement << "%";
                if (query_improvement > 0) {
                    std::cout << " ⬆️ (更快)";
                } else {
                    std::cout << " ⬇️ (更慢)";
                }
                std::cout << std::endl;

                std::cout << "  🏗️  构建时间变化: " << std::fixed << std::setprecision(2) << build_change << "%";
                if (build_change > 0) {
                    std::cout << " ⬆️ (更慢)";
                } else {
                    std::cout << " ⬇️ (更快)";
                }
                std::cout << std::endl;

                std::cout << "  💾 内存效率: " << std::fixed << std::setprecision(2) << memory_efficiency << "%";
                if (memory_efficiency > 0) {
                    std::cout << " ⬆️ (更省内存)";
                } else {
                    std::cout << " ⬇️ (更多内存)";
                }
                std::cout << std::endl << std::endl;
            }
        }
    }
};

int main() {
    std::cout << "🎯 GLIN-HF 完整性能测试 (包含索引构建时间统计)" << std::endl;
    std::cout << "📝 测试三种方法：原始GLIN, GLIN-HF (启用PIECE), Lite-AMF (您的配置)" << std::endl;

    try {
        // 1. 创建测试数据
        int data_size = 10000;  // 1万个几何对象
        auto geoms = CompletePerformanceTester::createTestData(data_size);

        // 2. 测试三种方法
        std::vector<PerformanceResult> results;

        // 测试原始GLIN
        auto original_result = CompletePerformanceTester::testOriginalGLIN(geoms);
        results.push_back(original_result);

        // 测试GLIN-HF (需要启用PIECE)
        auto hf_result = CompletePerformanceTester::testGLIN_HF(geoms);
        results.push_back(hf_result);

        // 测试Lite-AMF (您的成功配置)
        auto amf_result = CompletePerformanceTester::testLiteAMF(geoms);
        results.push_back(amf_result);

        // 3. 打印详细对比结果
        CompletePerformanceTester::printComparisonTable(results);

        // 4. 清理内存
        for (auto* geom : geoms) {
            delete geom;
        }
        geoms.clear();

        std::cout << "\n✅ 完整性能测试完成！" << std::endl;
        std::cout << "📋 关键发现:" << std::endl;
        std::cout << "   • 现在可以清楚看到每种方法的索引构建时间" << std::endl;
        std::cout << "   • 查询次数都统一为3次，便于公平对比" << std::endl;
        std::cout << "   • 包含内存使用情况的对比" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "❌ 测试过程中出现错误: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}