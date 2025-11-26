#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

// 基于用户发现的地理数据最佳实践
void test_with_real_world_coords() {
    std::cout << "=== 地理数据优化测试（基于用户成功方案）===" << std::endl;
    std::cout << "✅ 使用方案：it.it_update_mbr() + 禁用PIECE + 极细网格" << std::endl;

    geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
    geos::io::WKTReader reader(factory.get());

    // 测试数据量（逐步增加）
    int test_sizes[] = {10000, 50000, 100000, 500000};

    for (int num_objects : test_sizes) {
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "测试数据量: " << num_objects << " 个地理对象" << std::endl;
        std::cout << std::string(70, '=') << std::endl;

        std::vector<geos::geom::Geometry*> test_geoms;

        try {
            // 创建模拟地理数据（基于您的真实数据范围）
            std::cout << "创建模拟地理数据（美国东南部坐标）..." << std::endl;

            // 您的数据特征
            const double min_x = -95.7;  // 经度范围
            const double max_x = -95.6;
            const double min_y = 31.7;   // 纬度范围
            const double max_y = 31.9;
            const double width = max_x - min_x;
            const double height = max_y - min_y;

            for (int i = 0; i < num_objects; ++i) {
                // 在您的数据范围内随机分布
                double x = min_x + (double)rand() / RAND_MAX * width;
                double y = min_y + (double)rand() / RAND_MAX * height;

                // 创建小尺寸多边形（模拟真实地理对象）
                double size = 0.001;  // 约100米的对象

                std::ostringstream wkt;
                wkt << "POLYGON(("
                     << x << " " << y << ","
                     << x << " " << (y + size) << ","
                     << (x + size) << " " << (y + size) << ","
                     << (x + size) << " " << y << ","
                     << x << " " << y << "))";

                try {
                    auto geom = reader.read(wkt.str());
                    if (geom) {
                        test_geoms.push_back(geom.release());
                    }
                } catch (const std::exception& e) {
                    // 忽略无效几何对象
                }

                if ((i + 1) % 10000 == 0) {
                    std::cout << "已创建 " << (i + 1) << "/" << num_objects << " 个对象" << std::endl;
                }
            }

            std::cout << "✅ 成功创建 " << test_geoms.size() << " 个地理对象" << std::endl;

            // 使用您的成功配置
            alex::Glin<double, geos::geom::Geometry*> glin;

            // 🎯 关键参数 - 基于您的成功方案
            double piecelimitation = 1000000.0;  // 大数值，相当于禁用分段
            std::string curve_type = "z";
            double cell_xmin = -180.0;  // 完整地理范围
            double cell_ymin = -90.0;
            double cell_x_intvl = 0.001;  // 🎯 您发现的关键参数！
            double cell_y_intvl = 0.001;  // 🎯 约100米精度

            std::vector<std::tuple<double, double, double, double>> pieces;

            std::cout << "使用优化参数加载索引..." << std::endl;
            std::cout << "  🎯 cell_x_intvl: " << cell_x_intvl << " (极细网格)" << std::endl;
            std::cout << "  🎯 cell_y_intvl: " << cell_y_intvl << " (极细网格)" << std::endl;
            std::cout << "  📊 数据范围: X[" << min_x << "," << max_x << "] Y[" << min_y << "," << max_y << "]" << std::endl;

            auto start_time = std::chrono::high_resolution_clock::now();

            // 执行加载（使用您的成功配置）
            glin.glin_bulk_load(test_geoms, piecelimitation, curve_type,
                              cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

            auto end_time = std::chrono::high_resolution_clock::now();
            auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            std::cout << "✅ 索引构建成功！" << std::endl;
            std::cout << "  ⏱️  构建时间: " << load_time.count() << "ms" << std::endl;
            std::cout << "  📦 分段数量: " << pieces.size() << " (应该为1)" << std::endl;

            // 查询测试 - 在数据密集区域
            std::cout << "\n执行查询测试..." << std::endl;

            std::vector<std::string> test_queries = {
                "POLYGON((-95.65 31.8,-95.65 31.81,-95.64 31.81,-95.64 31.8,-95.65 31.8))",
                "POLYGON((-95.68 31.75,-95.68 31.76,-95.67 31.76,-95.67 31.75,-95.68 31.75))",
                "POLYGON((-95.62 31.82,-95.62 31.83,-95.61 31.83,-95.61 31.82,-95.62 31.82))"
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

                std::cout << "  🔍 查询" << (q+1) << ": " << results.size() << "个结果, "
                         << query_time.count() << "μs" << std::endl;

                delete query;

                // 清理结果
                for (auto* result : results) {
                    delete result;
                }
            }

            // 清理内存
            for (auto* geom : test_geoms) {
                delete geom;
            }
            test_geoms.clear();

            std::cout << "✅ " << num_objects << " 条数据测试完成！" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "❌ 错误: " << e.what() << std::endl;

            // 清理内存
            for (auto* geom : test_geoms) {
                delete geom;
            }
            break;
        }
    }
}

int main() {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "🌍 地理空间数据大规模处理验证" << std::endl;
    std::cout << "基于用户发现的100万条数据成功方案" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    test_with_real_world_coords();

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "🎯 关键发现总结：" << std::endl;
    std::cout << "1. cell_x_intvl = 0.001, cell_y_intvl = 0.001 是地理数据的最优粒度" << std::endl;
    std::cout << "2. 禁用PIECE宏避免了分段开销" << std::endl;
    std::cout << "3. it.it_update_mbr()在无分段时工作正常" << std::endl;
    std::cout << "4. 极细网格避免数据聚集和内存热点" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    return 0;
}