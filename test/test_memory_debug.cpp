#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// 内存调试和渐进式测试
void test_with_limit(int limit) {
    std::cout << "\n=== 测试数据量: " << limit << " ===" << std::endl;

    try {
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());
        std::vector<geos::geom::Geometry*> test_geoms;

        // 创建测试数据
        std::cout << "创建" << limit << "个几何对象..." << std::endl;
        for (int i = 0; i < limit; ++i) {
            double x = i * 5.0;
            double y = i * 5.0;
            std::ostringstream wkt;
            wkt << "POLYGON((" << x << " " << y << "," << x << " " << (y+3) << ","
                << (x+3) << " " << (y+3) << "," << (x+3) << " " << y << "," << x << " " << y << "))";

            try {
                test_geoms.push_back(reader.read(wkt.str()).release());
            } catch (const std::exception& e) {
                std::cerr << "创建几何对象失败[" << i << "]: " << e.what() << std::endl;
            }

            // 每1000个显示进度
            if ((i + 1) % 1000 == 0) {
                std::cout << "已创建 " << (i + 1) << "/" << limit << " 个对象" << std::endl;
            }
        }

        std::cout << "成功创建 " << test_geoms.size() << " 个几何对象" << std::endl;

        // 配置GLIN参数
        alex::Glin<double, geos::geom::Geometry*> glin;
        double piecelimitation = 500.0;  // 增大piecelimitation减少分段数
        std::string curve_type = "z";
        double cell_xmin = -100;
        double cell_ymin = -100;
        double cell_x_intvl = 5.0;   // 增大网格单元减少内存使用
        double cell_y_intvl = 5.0;

        std::vector<std::tuple<double, double, double, double>> pieces;

        // 监控内存使用
        std::cout << "开始加载到索引..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();

        glin.glin_bulk_load(test_geoms, piecelimitation, curve_type,
                          cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "✅ 加载成功！耗时: " << load_time.count() << "ms" << std::endl;
        std::cout << "分段数量: " << pieces.size() << std::endl;

        // 简单查询测试
        std::string query_wkt = "POLYGON((0 0,0 10,10 10,10 0,0 0))";
        auto query = reader.read(query_wkt).release();

        std::vector<geos::geom::Geometry*> results;
        int filter_count = 0;

        auto query_start = std::chrono::high_resolution_clock::now();
        glin.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, results, filter_count);
        auto query_end = std::chrono::high_resolution_clock::now();

        auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(query_end - query_start);

        std::cout << "✅ 查询成功！找到 " << results.size() << " 个结果，耗时: " << query_time.count() << "μs" << std::endl;

        // 清理内存
        delete query;
        for (auto* geom : test_geoms) {
            delete geom;
        }
        for (auto* result : results) {
            delete result;
        }

        std::cout << "✅ 内存清理完成" << std::endl;

    } catch (const std::bad_alloc& e) {
        std::cerr << "❌ 内存不足: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ 其他错误: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "❌ 未知段错误发生" << std::endl;
    }
}

int main() {
    std::cout << "=== 内存调试测试 ===" << std::endl;

    // 渐进式测试，找到崩溃点
    int test_limits[] = {5000, 10000, 15000, 20000, 25000, 30000};

    for (int limit : test_limits) {
        test_with_limit(limit);

        // 如果崩溃，程序会终止，不会继续后续测试
        std::cout << "测试 " << limit << " 条数据完成" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
    }

    return 0;
}