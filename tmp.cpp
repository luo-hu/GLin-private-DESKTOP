  
  
/*数据集太大，采用分批次构建GLIN索引，但是还是不行，这是Alex库本身的问题*/
#include "./glin/glin.h"
#include <geos/geom/Geometry.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/io/WKTReader.h>
#include <geos/util/GEOSException.h>
#include <geos/geom/Envelope.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <memory>
#include <algorithm>
#include <tuple>
#include <sstream>
#include <map>

#include "./src/core/alex.h"

// 【新结构体】将索引和其对应的pieces数据绑定在一起
struct GlinIndexWithPieces {
    std::unique_ptr<alex::Glin<double, geos::geom::Geometry*>> index;
    std::vector<std::tuple<double, double, double, double>> pieces;
};

// 函数：清理WKT字符串，使其更规范 (无变化)
std::string clean_wkt_string(const std::string& line) {
    std::string wkt_string;
    size_t first_quote_pos = line.find('"');
    if (first_quote_pos != std::string::npos) {
        size_t end_quote_pos = line.find('"', first_quote_pos + 1);
        if (end_quote_pos != std::string::npos) {
            return line.substr(first_quote_pos + 1, end_quote_pos - (first_quote_pos + 1));
        }
    } else {
        size_t start_pos = line.find("POLYGON");
        if (start_pos == std::string::npos) {
            start_pos = line.find("MULTIPOLYGON");
        }
        if (start_pos != std::string::npos) {
            size_t last_paren_pos = line.rfind(')');
            if (last_paren_pos != std::string::npos) {
                return line.substr(start_pos, last_paren_pos - start_pos + 1);
            }
        }
    }
    return "";
}

int main() {
    // --- 初始化 GEOS 组件 ---
    auto pm = std::make_unique<geos::geom::PrecisionModel>();
    geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(), -1);
    geos::io::WKTReader reader(*global_factory);

    // 存储轻量级的MBR（最小边界矩形）
    std::vector<std::unique_ptr<geos::geom::Geometry>> mbr_geometries;
    // 使用一个vector顺序存储WKT字符串
    std::vector<std::string> wkt_storage;
    // 使用一个map来映射MBR指针到它在vector中的ID（索引）
    std::map<geos::geom::Geometry*, size_t> mbr_ptr_to_id_map;

    // --- 阶段一: 预处理CSV文件，一次性读取所有MBR ---
    std::cout << "阶段一: 开始预处理CSV文件，提取MBR..." << std::endl;
    std::ifstream inputFile("/mnt/hgfs/ShareFiles/AREAWATER.csv");
    if (!inputFile.is_open()) {
        std::cerr << "打开 AREAWATER.csv 失败！" << std::endl;
        return 1;
    }
    
    mbr_geometries.reserve(2300000);
    wkt_storage.reserve(2300000);

    std::string line;
    long line_count = 0;
    while (getline(inputFile, line)) {
        line_count++;
        if(line_count % 100000 == 0) {
            std::cout << "已处理 " << line_count << " 行..." << std::endl;
        }

        std::string wkt_string = clean_wkt_string(line);
        if (wkt_string.empty()) continue;

        try {
            std::unique_ptr<geos::geom::Geometry> full_geom = reader.read(wkt_string);
            const geos::geom::Envelope* env = full_geom->getEnvelopeInternal();
            if (!env) continue;

            std::unique_ptr<geos::geom::Geometry> mbr_geom = global_factory->toGeometry(env);
            geos::geom::Geometry* mbr_geom_ptr = mbr_geom.get();

            mbr_geometries.push_back(std::move(mbr_geom));
            wkt_storage.push_back(wkt_string);
            mbr_ptr_to_id_map[mbr_geom_ptr] = wkt_storage.size() - 1;

        } catch (const geos::util::GEOSException& e) {
            // 忽略解析失败的行
        }
    }
    inputFile.close();
    std::cout << "MBR提取完成，共处理 " << mbr_geometries.size() << " 个有效几何对象。" << std::endl;

    if (mbr_geometries.empty()) {
        std::cerr << "没有加载到任何有效的几何对象，程序退出。" << std::endl;
        return 1;
    }

    // --- 【核心修改】阶段二: 分批次在MBR上构建多个Glin索引 ---

    // 定义一个安全的批处理大小
    const size_t BATCH_SIZE = 14000;
    std::vector<GlinIndexWithPieces> all_indices;

    std::vector<geos::geom::Geometry*> raw_mbr_vec;
    raw_mbr_vec.reserve(mbr_geometries.size());
    for (const auto& geo_ptr : mbr_geometries) {
        raw_mbr_vec.push_back(geo_ptr.get());
    }

    // 【新】在循环外只打开一次CDF文件
    std::ofstream single_cdf_file("./../zmin_cdf_all.csv");
    if (!single_cdf_file.is_open()) {
        std::cerr << "警告：无法创建 zmin_cdf_all.csv 文件！CDF数据将不会被保存。" << std::endl;
    }

    std::cout << "阶段二: 开始分批次构建Glin索引..." << std::endl;
    int batch_index = 0;
    for (size_t i = 0; i < raw_mbr_vec.size(); i += BATCH_SIZE) {
        size_t start = i;
        size_t end = std::min(i + BATCH_SIZE, raw_mbr_vec.size());
        
        std::vector<geos::geom::Geometry*> batch_vec(raw_mbr_vec.begin() + start, raw_mbr_vec.begin() + end);

        GlinIndexWithPieces indexed_batch;
        indexed_batch.index = std::make_unique<alex::Glin<double, geos::geom::Geometry*>>();
        
        double piecelimitation = 100.0, cell_xmin = -100, cell_ymin = -90, cell_x_intvl = 1.0, cell_y_intvl = 1.0;
        std::string curve_type = "z";
        
        // 【修改】调用 bulk_load，并传入文件流的引用
        // indexed_batch.index->glin_bulk_load(batch_vec, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, indexed_batch.pieces);

        indexed_batch.index->glin_bulk_load1(batch_vec, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, indexed_batch.pieces, batch_index, single_cdf_file);
        
        all_indices.push_back(std::move(indexed_batch));

        std::cout << "已为 " << start << " - " << end << " 区间的数据构建索引，当前共 " << all_indices.size() << " 个索引。" << std::endl;
        batch_index++;
    }

    // 【新】在所有批次处理完毕后，关闭文件
    if(single_cdf_file.is_open()) {
        single_cdf_file.close();
        std::cout << "CDF数据已全部写入 zmin_cdf_all.csv" << std::endl;
    }
    
    std::cout << "所有索引构建完成。" << std::endl;

    // --- 【核心修改】阶段三: 依次查询所有索引，并合并结果 ---
    std::unique_ptr<geos::geom::Geometry> query_window;
    std::string query_wkt = "POLYGON ((44 36, 52 36, 52 28, 44 28, 44 36))";
    try {
        query_window = reader.read(query_wkt);
    } catch (const geos::util::GEOSException& e) {
        std::cerr << "创建查询窗口失败: " << e.what() << std::endl;
        return 1;
    }

    std::vector<geos::geom::Geometry*> all_candidate_mbr_ptrs;
    int total_filter_count = 0;

    std::cout << "阶段三: 开始查询所有索引..." << std::endl;
    for(const auto& indexed_batch : all_indices) {
        std::vector<geos::geom::Geometry*> batch_candidates;
        int batch_filter_count = 0;
        
        double cell_xmin = -100, cell_ymin = -90, cell_x_intvl = 1.0, cell_y_intvl = 1.0;
        std::string curve_type = "z";
        auto pieces_for_find = indexed_batch.pieces;

        indexed_batch.index->glin_find(query_window.get(), "z", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces_for_find, batch_candidates, batch_filter_count);
        
        all_candidate_mbr_ptrs.insert(all_candidate_mbr_ptrs.end(), batch_candidates.begin(), batch_candidates.end());
        total_filter_count += batch_filter_count;
    }

    std::cout << "找到 " << all_candidate_mbr_ptrs.size() << " 个候选对象，开始精确优化..." << std::endl;
    std::vector<std::string> final_results_wkt;
    
    for (geos::geom::Geometry* mbr_ptr : all_candidate_mbr_ptrs) {
        auto it = mbr_ptr_to_id_map.find(mbr_ptr);
        if (it != mbr_ptr_to_id_map.end()) {
            size_t id = it->second;
            const std::string& original_wkt = wkt_storage[id];
            try {
                std::unique_ptr<geos::geom::Geometry> original_geom = reader.read(original_wkt);
                if (query_window->intersects(original_geom.get())) {
                    final_results_wkt.push_back(original_wkt);
                }
            } catch (const geos::util::GEOSException& e) {
                // 忽略解析错误
            }
        }
    }

    // --- 阶段四: 输出最终结果 ---
    std::cout << "\n查询完成。" << std::endl;
    std::cout << "索引过滤阶段检查的对象数量 (MBRs): " << total_filter_count << std::endl;
    std::cout << "精确计算的候选对象数量: " << all_candidate_mbr_ptrs.size() << std::endl;
    std::cout << "最终精确匹配的结果数量: " << final_results_wkt.size() << std::endl;

    return 0;
}