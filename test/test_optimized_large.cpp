#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <sys/resource.h>  // 用于内存监控

// 内存使用监控函数
size_t get_memory_usage() {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss;  // 返回KB
}

// 优化的大数据集测试
int main() {
    std::cout << "=== 优化大数据集测试 ===" << std::endl;

    // 显示初始内存使用
    std::cout << "初始内存使用: " << get_memory_usage() << " KB" << std::endl;

    // 测试不同的数据量
    int test_sizes[] = {10000, 15000, 20000};

    for (int size : test_sizes) {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "测试数据量: " << size << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        try {
            // 1. 创建几何对象工厂
            geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
            geos::io::WKTReader reader(factory.get());
            std::vector<geos::geom::Geometry*> test_geoms;

            // 2. 分批创建几何对象，避免内存峰值
            std::cout << "创建" << size << "个几何对象..." << std::endl;
            const int batch_size = 1000;

            for (int batch = 0; batch < (size + batch_size - 1) / batch_size; ++batch) {
                int start_idx = batch * batch_size;
                int end_idx = std::min(start_idx + batch_size, size);

                for (int i = start_idx; i < end_idx; ++i) {
                    // 创建更分散的几何对象，避免聚集
                    double x = i * 10.0;  // 增加间距
                    double y = i * 8.0;   // 不同间距
                    std::ostringstream wkt;
                    wkt << "POLYGON((" << x << " " << y << "," << x << " " << (y+3) << ","
                        << (x+3) << " " << (y+3) << "," << (x+3) << " " << y << "," << x << " " << y << "))";

                    try {
                        auto geom = reader.read(wkt.str());
                        if (geom) {
                            test_geoms.push_back(geom.release());
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "创建几何对象失败[" << i << "]: " << e.what() << std::endl;
                    }
                }

                std::cout << "已完成 " << end_idx << "/" << size << " 个对象，内存使用: "
                         << get_memory_usage() << " KB" << std::endl;

                // 检查内存使用，如果过高则警告
                if (get_memory_usage() > 1000000) {  // 超过1GB
                    std::cout << "⚠️  内存使用过高，建议优化数据结构" << std::endl;
                }
            }

            std::cout << "✅ 成功创建 " << test_geoms.size() << " 个几何对象" << std::endl;

            // 3. 优化的GLIN配置
            alex::Glin<double, geos::geom::Geometry*> glin;

            // 优化参数减少内存使用
            double piecelimitation = 2000.0;  // 增大分段减少索引复杂度
            std::string curve_type = "z";
            double cell_xmin = -1000;
            double cell_ymin = -1000;
            double cell_x_intvl = 20.0;    // 增大网格单元
            double cell_y_intvl = 20.0;

            std::vector<std::tuple<double, double, double, double>> pieces;

            std::cout << "开始加载索引（使用优化参数）..." << std::endl;
            std::cout << "  piecelimitation: " << piecelimitation << std::endl;
            std::cout << "  网格大小: " << cell_x_intvl << "x" << cell_y_intvl << std::endl;

            auto start_time = std::chrono::high_resolution_clock::now();

            // 执行加载
            glin.glin_bulk_load(test_geoms, piecelimitation, curve_type,
                              cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            std::cout << "✅ 索引加载成功！" << std::endl;
            std::cout << "  加载时间: " << load_time.count() << "ms" << std::endl;
            std::cout << "  分段数量: " << pieces.size() << std::endl;
            std::cout << "  内存使用: " << get_memory_usage() << " KB" << std::endl;

            // 4. 查询测试
            std::cout << "\n执行查询测试..." << std::endl;

            // 创建多个测试查询
            std::vector<std::string> test_queries = {
                "POLYGON((0 0,0 20,20 20,20 0,0 0))",
                "POLYGON((5000 4000,5000 4020,5020 4020,5020 4000,5000 4000))",
                "POLYGON((10000 8000,10000 8020,10020 8020,10020 8000,10000 8000))"
            };

            for (size_t q = 0; q < test_queries.size(); ++q) {
                auto query_start = std::chrono::high_resolution_clock::now();

                auto query = reader.read(test_queries[q]).release();
                std::vector<geos::geom::Geometry*> results;
                int filter_count = 0;

                glin.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
                              pieces, results, filter_count);

                auto query_end = std::chrono::high_resolution_clock::now();
                auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start);

                std::cout << "  查询" << (q+1) << ": " << results.size() << "个结果, "
                         << query_time.count() << "μs" << std::endl;

                delete query;

                // 清理结果，避免内存累积
                for (auto* result : results) {
                    delete result;
                }
            }

            // 5. 内存清理
            std::cout << "\n清理内存..." << std::endl;
            for (auto* geom : test_geoms) {
                delete geom;
            }
            test_geoms.clear();

            std::cout << "✅ 测试" << size << "条数据完成" << std::endl;
            std::cout << "最终内存使用: " << get_memory_usage() << " KB" << std::endl;

        } catch (const std::bad_alloc& e) {
            std::cerr << "❌ 内存不足: " << e.what() << std::endl;
            std::cerr << "建议: 减少数据量或优化内存使用" << std::endl;
            break;
        } catch (const std::exception& e) {
            std::cerr << "❌ 其他错误: " << e.what() << std::endl;
            break;
        } catch (...) {
            std::cerr << "❌ 未知错误发生（可能段错误）" << std::endl;
            break;
        }
    }

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "测试完成" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    return 0;
}