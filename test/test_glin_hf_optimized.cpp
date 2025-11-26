#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <sstream>
#include <limits>

// 内存优化的大数据集测试程序
int main() {
    std::cout << "=== GLIN-HF大数据集内存优化测试 ===" << std::endl;

    // 配置参数
    const int MAX_DATASET_SIZE = 15000;  // 适��的数据集大小，测试分段功能
    const int BATCH_SIZE = 1000;         // 批处理大小

    geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
    geos::io::WKTReader reader(factory.get());

    // 1. 读取数据集（带大小限制）
    std::vector<std::string> wkt_polygons;
    std::ifstream inputFile("/mnt/hgfs/sharedFolder/AREAWATER.csv");

    if (!inputFile.is_open()) {
        std::cerr << "错误：无法打开AREAWATER.csv文件" << std::endl;
        return -1;
    }

    std::cout << "开始读取数据集（最多" << MAX_DATASET_SIZE << "条）..." << std::endl;

    std::string line, wkt_string;
    int line_count = 0;
    int valid_count = 0;

    while (getline(inputFile, line) && valid_count < MAX_DATASET_SIZE) {
        line_count++;

        // 显示进度
        if (line_count % 20000 == 0) {
            std::cout << "已处理" << line_count << "行，有效数据" << valid_count << "条" << std::endl;
        }

        // 移除BOM标记
        if (line.length() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') {
            line = line.substr(3);
        }

        // 清理空白字符
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.empty()) continue;

        // 提取WKT字符串
        if (line.front() == '"') {
            size_t end_quote_pos = line.find('"', 1);
            if (end_quote_pos != std::string::npos) {
                wkt_string = line.substr(1, end_quote_pos - 1);
            } else continue;
        } else {
            size_t last_paren_pos = line.rfind(')');
            if (last_paren_pos != std::string::npos) {
                wkt_string = line.substr(0, last_paren_pos + 1);
                wkt_string.erase(wkt_string.find_last_not_of(" \t\n\r") + 1);
            } else continue;
        }

        if (!wkt_string.empty()) {
            wkt_polygons.push_back(wkt_string);
            valid_count++;
        }
    }

    inputFile.close();
    std::cout << "数据读取完成：共" << valid_count << "条有效WKT数据" << std::endl;

    // 2. 批量转换几何对象（内存优化）
    std::vector<geos::geom::Geometry*> geoms;
    geoms.reserve(valid_count);

    std::cout << "开始转换几何对象..." << std::endl;
    int failed_conversions = 0;

    for (int i = 0; i < wkt_polygons.size(); i++) {
        try {
            std::unique_ptr<geos::geom::Geometry> geom_ptr = reader.read(wkt_polygons[i]);
            if (geom_ptr) {
                geoms.push_back(geom_ptr.release());
            }
        } catch (const std::exception& e) {
            failed_conversions++;
            if (failed_conversions <= 10) {  // 只显示前10个错误
                std::cerr << "转换WKT失败[" << i << "]: " << e.what() << std::endl;
            }
        }

        // 显示进度
        if ((i + 1) % 10000 == 0) {
            std::cout << "已转换" << (i + 1) << "/" << wkt_polygons.size() << "个几何对象" << std::endl;
        }
    }

    std::cout << "几何对象转换完成：成功" << geoms.size() << "个，失败" << failed_conversions << "个" << std::endl;

    if (geoms.empty()) {
        std::cerr << "错误：没有有效的几何对象可用于测试" << std::endl;
        return -1;
    }

    // 分析数据范围
    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();
    double min_y = std::numeric_limits<double>::max();
    double max_y = std::numeric_limits<double>::lowest();

    for (const auto* geom : geoms) {
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

    std::cout << "\n=== 数据范围分析 ===" << std::endl;
    std::cout << "X坐标范围: [" << min_x << ", " << max_x << "]" << std::endl;
    std::cout << "Y坐标范围: [" << min_y << ", " << max_y << "]" << std::endl;
    std::cout << "数据宽度: " << (max_x - min_x) << std::endl;
    std::cout << "数据高度: " << (max_y - min_y) << std::endl;

    // 3. 内存检查和配置
    std::cout << "配置GLIN参数..." << std::endl;

    alex::Glin<double, geos::geom::Geometry*> glin_original;
    alex::Glin<double, geos::geom::Geometry*> glin_hf;

    double piecelimitation = 1000.0;  // 更大的piecelimitation，强制产生分段
    std::string curve_type = "z";
    double cell_xmin = -180.0;  // 使用完整世界坐标系范围
    double cell_ymin = -90.0;
    double cell_x_intvl = 1.0;   // 更小的网格单元，提高精度
    double cell_y_intvl = 1.0;

    std::cout << "数据量：" << geoms.size() << "个几何对象" << std::endl;
    std::cout << "网格参数：[" << cell_xmin << "," << cell_ymin << "] 大小：" << cell_x_intvl << "x" << cell_y_intvl << std::endl;
    std::cout << "piecelimitation：" << piecelimitation << std::endl;

    // 4. 执行加载（带内存监控）
    std::cout << "开始加载到GLIN索引..." << std::endl;
    std::vector<std::tuple<double, double, double, double>> pieces;

    auto start_load = std::chrono::high_resolution_clock::now();

    try {
        // 首先尝试加载原始GLIN
        std::cout << "加载原始GLIN..." << std::endl;
        std::cout << "内存检查：准备加载" << geoms.size() << "个几何对象" << std::endl;
        glin_original.glin_bulk_load(geoms, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
        std::cout << "原始GLIN加载成功" << std::endl;

        // 然后加载GLIN-HF
        std::cout << "加载GLIN-HF..." << std::endl;
        glin_hf.glin_bulk_load(geoms, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
        std::cout << "GLIN-HF加载成功" << std::endl;

    } catch (const std::bad_alloc& e) {
        std::cerr << "内存不足错误：" << e.what() << std::endl;
        std::cerr << "数据量过大，建议进一步减少MAX_DATASET_SIZE" << std::endl;

        // 清理内存
        for (auto* geom : geoms) {
            delete geom;
        }
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "其他错误：" << e.what() << std::endl;

        // 清理内存
        for (auto* geom : geoms) {
            delete geom;
        }
        return -1;
    } catch (...) {
        std::cerr << "未知错误发生" << std::endl;

        // 清理内存
        for (auto* geom : geoms) {
            delete geom;
        }
        return -1;
    }

    auto end_load = std::chrono::high_resolution_clock::now();
    auto load_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load).count();

    std::cout << "数据加载成功！耗时：" << load_time_ms << "ms" << std::endl;
    std::cout << "生成分段数量：" << pieces.size() << std::endl;

    // 5. 性能测试
    std::cout << "\n=== 开始性能测试 ===" << std::endl;

    // 创建基于数据范围的测试查询
    std::vector<std::string> query_wkts;

    // 基于实际数据范围生成查询
    double data_center_x = (min_x + max_x) / 2.0;
    double data_center_y = (min_y + max_y) / 2.0;
    double data_width = max_x - min_x;
    double data_height = max_y - min_y;
    double query_size = std::min(data_width, data_height) * 0.1; // 查询窗口为数据范围的10%

    // ��成查询窗口
    double q1_x = data_center_x - query_size/2;
    double q1_y = data_center_y - query_size/2;
    double q2_x = data_center_x + query_size/2;
    double q2_y = data_center_y + query_size/2;

    std::ostringstream query1, query2, query3;
    query1 << "POLYGON((" << q1_x << " " << q1_y << "," << q2_x << " " << q1_y << ","
            << q2_x << " " << q2_y << "," << q1_x << " " << q2_y << "," << q1_x << " " << q1_y << "))";

    // 大一点的查询窗口（数据的20%）
    double large_query_size = std::min(data_width, data_height) * 0.2;
    double q3_x1 = data_center_x - large_query_size/2;
    double q3_y1 = data_center_y - large_query_size/2;
    double q3_x2 = data_center_x + large_query_size/2;
    double q3_y2 = data_center_y + large_query_size/2;

    std::ostringstream query2_str;
    query2_str << "POLYGON((" << q3_x1 << " " << q3_y1 << "," << q3_x2 << " " << q3_y1 << ","
              << q3_x2 << " " << q3_y2 << "," << q3_x1 << " " << q3_y2 << "," << q3_x1 << " " << q3_y1 << "))";

    query_wkts.push_back(query1.str());
    query_wkts.push_back(query2_str.str());

    std::cout << "\n=== 生成的查询窗口 ===" << std::endl;
    std::cout << "查询1(小窗口): " << query_wkts[0] << std::endl;
    std::cout << "查询2(大窗口): " << query_wkts[1] << std::endl;

    std::vector<geos::geom::Geometry*> query_geoms;
    for (const auto& wkt : query_wkts) {
        try {
            query_geoms.push_back(reader.read(wkt).release());
        } catch (const std::exception& e) {
            std::cerr << "创建查询几何失败：" << e.what() << std::endl;
        }
    }

    if (query_geoms.empty()) {
        std::cerr << "错误：没有有效的查询几何对象" << std::endl;
        // 清理数据内存
        for (auto* geom : geoms) delete geom;
        return -1;
    }

    // 执行性能对比测试
    for (int q = 0; q < query_geoms.size(); q++) {
        std::cout << "\n--- 查询 " << (q + 1) << ": " << query_wkts[q] << " ---" << std::endl;

        auto query_start = std::chrono::high_resolution_clock::now();

        // 原始GLIN查询
        int total_filter_original = 0;
        std::vector<geos::geom::Geometry*> res_original;
        auto orig_start = std::chrono::high_resolution_clock::now();
        glin_original.glin_find(query_geoms[q], "zorder", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_original, total_filter_original);
        auto orig_end = std::chrono::high_resolution_clock::now();

        // GLIN-HF查询
        int total_filter_hf = 0;
        std::vector<geos::geom::Geometry*> res_hf;
        auto hf_start = std::chrono::high_resolution_clock::now();
        glin_hf.glin_find(query_geoms[q], "zorder", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_hf, total_filter_hf);
        auto hf_end = std::chrono::high_resolution_clock::now();

        auto query_end = std::chrono::high_resolution_clock::now();

        // 计算性能指标
        auto orig_time = std::chrono::duration_cast<std::chrono::microseconds>(orig_end - orig_start).count();
        auto hf_time = std::chrono::duration_cast<std::chrono::microseconds>(hf_end - hf_start).count();

        std::cout << "原始GLIN: " << res_original.size() << "个结果, "
                  << orig_time << "μs, 过滤" << total_filter_original << "个候选" << std::endl;
        std::cout << "GLIN-HF:   " << res_hf.size() << "个结果, "
                  << hf_time << "μs, 过滤" << total_filter_hf << "个候选" << std::endl;

        if (orig_time > 0) {
            double speedup = static_cast<double>(orig_time) / hf_time;
            std::cout << "性能提升: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        }

        // 验证结果一致性
        if (res_original.size() != res_hf.size()) {
            std::cout << "警告：结果数量不一致！" << std::endl;
        }
    }

    // 6. 清理内存
    std::cout << "\n清理内存..." << std::endl;
    for (auto* geom : geoms) {
        delete geom;
    }
    for (auto* query : query_geoms) {
        delete query;
    }

    std::cout << "=== 测试完成 ===" << std::endl;
    return 0;
}