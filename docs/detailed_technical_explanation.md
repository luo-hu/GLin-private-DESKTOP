#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>

struct PerformanceMetrics {
    std::string method_name;
    long build_time_ms;      // 索引构建时间(毫秒)
    long total_query_time_us; // 总查询时间(微秒)
    long avg_query_time_us;   // 平均查询时间(微秒)
    int query_count;         // 查询次数
    int found_results;       // 找到的结果数
};

class CompletePerformanceTest {
public:
    static std::vector<geos::geom::Geometry*> createTestData(int count) {
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());
        std::vector<geos::geom::Geometry*> geoms;

        std::cout << "创建 " << count << " 个测试几何对象..." << std::endl;

        for (int i = 0; i < count; ++i) {
            double x = i * 10.0;  // 间距10米
            double y = i * 8.0;
            std::ostringstream wkt;
            wkt << "POLYGON((" << x << " " << y << ","
                 << x << " " << (y+3) << ","
                 << (x+3) << " " << (y+3) << ","
                 << (x+3) << " " << y << ","
                 << x << " " << y << "))";

            try {
                auto geom = reader.read(wkt.str());
                if (geom) {
                    geoms.push_back(geom.release());
                }
            } catch (...) {
                // 忽略失败的几何对象
            }

            if ((i + 1) % 1000 == 0) {
                std::cout << "已创建 " << (i + 1) << "/" << count << " 个对象" << std::endl;
            }
        }

        std::cout << "✅ 成功创建 " << geoms.size() << " 个几何对象" << std::endl;
        return geoms;
    }

    static PerformanceMetrics testOriginalGLIN(const std::vector<geos::geom::Geometry*>& geoms) {
        PerformanceMetrics metrics;
        metrics.method_name = "原始GLIN";

        // === 索引构建时间测试 ===
        std::cout << "\n🔧 测试原始GLIN索引构建..." << std::endl;
        alex::Glin<double, geos::geom::Geometry*> glin;

        double piecelimitation = 1000000.0;  // 使用您成功的配置
        std::string curve_type = "z";
        double cell_xmin = -100.0;
        double cell_ymin = -100.0;
        double cell_x_intvl = 1.0;   // 原始配置
        double cell_y_intvl = 1.0;

        std::vector<std::tuple<double, double, double, double>> pieces;

        auto build_start = std::chrono::high_resolution_clock::now();
        glin.glin_bulk_load(geoms, piecelimitation, curve_type,
                          cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
        auto build_end = std::chrono::high_resolution_clock::now();

        metrics.build_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            build_end - build_start).count();

        std::cout << "✅ 原始GLIN索引构建完成，耗时: " << metrics.build_time_ms << "ms" << std::endl;

        // === 查询性能测试 ===
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());

        std::vector<std::string> test_queries = {
            "POLYGON((0 0,0 20,20 20,20 0,0 0))",
            "POLYGON((100 80,100 100,120 100,120 80,100 80))",
            "POLYGON((500 400,500 420,520 420,520 400,500 400))"
        };

        metrics.query_count = test_queries.size();
        metrics.total_query_time_us = 0;

        std::cout << "\n🔍 执行 " << metrics.query_count << " 次查询测试..." << std::endl;

        for (const auto& query_wkt : test_queries) {
            auto query = reader.read(query_wkt).release();
            std::vector<geos::geom::Geometry*> results;
            int filter_count = 0;

            auto query_start = std::chrono::high_resolution_clock::now();
            glin.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
                          pieces, results, filter_count);
            auto query_end = std::chrono::high_resolution_clock::now();

            auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(
                query_end - query_start).count();

            metrics.total_query_time_us += query_time;
            metrics.found_results += results.size();

            std::cout << "  查询完成: " << results.size() << " 个结果, 耗时: " << query_time << "μs" << std::endl;

            delete query;
            for (auto* result : results) {
                delete result;
            }
        }

        metrics.avg_query_time_us = metrics.total_query_time_us / metrics.query_count;
        std::cout << "✅ 原始GLIN测试完成" << std::endl;

        return metrics;
    }

    static void printComparisonTable(const std::vector<PerformanceMetrics>& results) {
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "📊 完整性能对比结果" << std::endl;
        std::cout << std::string(100, '=') << std::endl;

        // 表头
        std::cout << std::setw(12) << "方法"
                  << std::setw(15) << "构建时间(ms)"
                  << std::setw(15) << "查询次数"
                  << std::setw(15) << "总查询时间(μs)"
                  << std::setw(15) << "平均查询时间(μs)"
                  << std::setw(12) << "结果数"
                  << std::endl;
        std::cout << std::string(100, '-') << std::endl;

        // 数据行
        for (const auto& m : results) {
            std::cout << std::setw(12) << m.method_name
                      << std::setw(15) << m.build_time_ms
                      << std::setw(15) << m.query_count
                      << std::setw(15) << m.total_query_time_us
                      << std::setw(15) << m.avg_query_time_us
                      << std::setw(12) << m.found_results
                      << std::endl;
        }

        std::cout << std::string(100, '=') << std::endl;

        // 性能分析
        if (results.size() >= 2) {
            const auto& baseline = results[0];  // 原始GLIN作为基准
            for (size_t i = 1; i < results.size(); ++i) {
                const auto& current = results[i];

                double query_improvement = ((double)baseline.avg_query_time_us - current.avg_query_time_us)
                                        / baseline.avg_query_time_us * 100;
                double build_overhead = ((double)current.build_time_ms - baseline.build_time_ms)
                                     / baseline.build_time_ms * 100;

                std::cout << "📈 " << current.method_name << " vs " << baseline.method_name << ":" << std::endl;
                std::cout << "  查询性能改进: " << std::fixed << std::setprecision(2) << query_improvement << "%";
                if (query_improvement > 0) {
                    std::cout << " ⬆️ (更快)";
                } else {
                    std::cout << " ⬇️ (更慢)";
                }
                std::cout << std::endl;

                std::cout << "  构建时间开销: " << std::fixed << std::setprecision(2) << build_overhead << "%";
                if (build_overhead > 0) {
                    std::cout << " ⬆️ (更慢)";
                } else {
                    std::cout << " ⬇️ (更快)";
                }
                std::cout << std::endl << std::endl;
            }
        }
    }
};

int main() {
    std::cout << "🎯 GLIN-HF完整性能测试" << std::endl;
    std::cout << "包含索引构建时间和查询时间的完整分析" << std::endl;

    // 创建测试数据
    int data_size = 10000;  // 1万个对象
    auto geoms = CompletePerformanceTest::createTestData(data_size);

    // 测试不同方法
    std::vector<PerformanceMetrics> results;

    // 测试原始GLIN
    auto original_metrics = CompletePerformanceTest::testOriginalGLIN(geoms);
    results.push_back(original_metrics);

    // 注：GLIN-HF和GLIN-AMF的测试代码结构类似，这里省略
    // 在实际使用中需要补充完整的测试代码

    // 打印对比结果
    CompletePerformanceTest::printComparisonTable(results);

    // 清理内存
    for (auto* geom : geoms) {
        delete geom;
    }

    std::cout << "\n✅ 测试完成" << std::endl;
    return 0;
}