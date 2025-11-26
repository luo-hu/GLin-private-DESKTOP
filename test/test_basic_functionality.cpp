#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <limits>

int main() {
    std::cout << "=== 基础功能测试 ===" << std::endl;

    // 1. 创建测试数据
    geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
    geos::io::WKTReader reader(factory.get());
    std::vector<geos::geom::Geometry*> test_geoms;

    // 创建简单的测试几何对象
    std::cout << "创建测试几何对象..." << std::endl;
    for (int i = 0; i < 15000; ++i) {
        double x = i * 5.0;
        double y = i * 5.0;
        std::ostringstream wkt;
        wkt << "POLYGON((" << x << " " << y << "," << x << " " << (y+3) << ","
            << (x+3) << " " << (y+3) << "," << (x+3) << " " << y << "," << x << " " << y << "))";

        try {
            test_geoms.push_back(reader.read(wkt.str()).release());
        } catch (const std::exception& e) {
            std::cerr << "创建几何对象失败: " << e.what() << std::endl;
        }
    }

    std::cout << "创建了 " << test_geoms.size() << " 个测试几何对象" << std::endl;

    // 2. 分析数据范围
    double min_x = 0, max_x = 0, min_y = 0, max_y = 0;
    for (const auto* geom : test_geoms) {
        if (geom) {
            const auto* env = geom->getEnvelopeInternal();
            if (env && !env->isNull()) {
                min_x = std::min(min_x, env->getMinX());
                max_x = std::max(max_x, env->getMaxX());
                min_y = std::min(min_y, env->getMinY());
                max_y = std::max(max_y, env->getMaxY());
            }
        }
    }

    std::cout << "数据范围: X[" << min_x << "," << max_x << "] Y[" << min_y << "," << max_y << "]" << std::endl;

    // 3. 配置GLIN参数
    alex::Glin<double, geos::geom::Geometry*> glin;
    double piecelimitation = 100.0;  // 较小的值，确保分段
    std::string curve_type = "z";
    double cell_xmin = min_x - 10;   // 基于数据范围
    double cell_ymin = min_y - 10;
    double cell_x_intvl = 5.0;
    double cell_y_intvl = 5.0;

    std::cout << "GLIN参数:" << std::endl;
    std::cout << "  piecelimitation: " << piecelimitation << std::endl;
    std::cout << "  网格起点: [" << cell_xmin << "," << cell_ymin << "]" << std::endl;
    std::cout << "  网格大小: " << cell_x_intvl << "x" << cell_y_intvl << std::endl;

    // 4. 加载数据
    std::cout << "开始加载数据..." << std::endl;
    std::vector<std::tuple<double, double, double, double>> pieces;

    auto start_load = std::chrono::high_resolution_clock::now();

    try {
        glin.glin_bulk_load(test_geoms, piecelimitation, curve_type,
                          cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
    } catch (const std::exception& e) {
        std::cerr << "加载失败: " << e.what() << std::endl;

        // 清理内存
        for (auto* geom : test_geoms) {
            delete geom;
        }
        return -1;
    }

    auto end_load = std::chrono::high_resolution_clock::now();
    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load).count();

    std::cout << "数据加载成功！耗时: " << load_time << "ms" << std::endl;
    std::cout << "分段数量: " << pieces.size() << std::endl;

    // 5. 创建查询
    std::cout << "\n=== 开始查询测试 ===" << std::endl;

    // 基于数据范围创建查询
    double center_x = (min_x + max_x) / 2.0;
    double center_y = (min_y + max_y) / 2.0;
    double query_size = 20.0;  // 查询窗口大小

    std::ostringstream query_wkt;
    query_wkt << "POLYGON((" << (center_x - query_size/2) << " " << (center_y - query_size/2) << ","
               << (center_x + query_size/2) << " " << (center_y - query_size/2) << ","
               << (center_x + query_size/2) << " " << (center_y + query_size/2) << ","
               << (center_x - query_size/2) << " " << (center_y + query_size/2) << ","
               << (center_x - query_size/2) << " " << (center_y - query_size/2) << "))";

    std::cout << "查询窗口: " << query_wkt.str() << std::endl;

    // 6. 执行查询
    geos::geom::Geometry* query = nullptr;
    try {
        query = reader.read(query_wkt.str()).release();
    } catch (const std::exception& e) {
        std::cerr << "创建查询失败: " << e.what() << std::endl;

        // 清理内存
        for (auto* geom : test_geoms) {
            delete geom;
        }
        return -1;
    }

    std::cout << "执行查询..." << std::endl;
    int filter_count = 0;
    std::vector<geos::geom::Geometry*> results;

    auto start_query = std::chrono::high_resolution_clock::now();
    glin.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, results, filter_count);
    auto end_query = std::chrono::high_resolution_clock::now();

    auto query_time = std::chrono::duration_cast<std::chrono::microseconds>(end_query - start_query).count();

    std::cout << "\n=== 查询结果 ===" << std::endl;
    std::cout << "查询时间: " << query_time << " μs" << std::endl;
    std::cout << "找到结果数量: " << results.size() << std::endl;
    std::cout << "过滤候选数量: " << filter_count << std::endl;

    if (results.size() > 0) {
        std::cout << "找到的几何对象:" << std::endl;
        for (size_t i = 0; i < std::min(results.size(), size_t(5)); ++i) {
            const auto* env = results[i]->getEnvelopeInternal();
            if (env) {
                std::cout << "  结果 " << i << ": MBR[" << env->getMinX() << "," << env->getMaxX()
                         << "] x [" << env->getMinY() << "," << env->getMaxY() << "]" << std::endl;
            }
        }
    } else {
        std::cout << "警告：没有找到任何结果！" << std::endl;

        // 调试：显示查询范围和数据范围的对比
        const auto* query_env = query->getEnvelopeInternal();
        if (query_env) {
            std::cout << "查询MBR: [" << query_env->getMinX() << "," << query_env->getMaxX()
                     << "] x [" << query_env->getMinY() << "," << query_env->getMaxY() << "]" << std::endl;
        }
    }

    // 7. 清理内存
    delete query;
    for (auto* geom : test_geoms) {
        delete geom;
    }
    for (auto* result : results) {
        delete result;
    }

    std::cout << "\n=== 测试完成 ===" << std::endl;
    return 0;
}