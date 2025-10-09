// #include "./test/doctest.h"
// #include "./glin/glin.h"
// #include <geos/geom/Geometry.h>
// #include <vector>
// #include "./src/core/alex.h"

// TEST_SUITE("GLIN Test Suite") {
//     TEST_CASE("GLIN Query Test") {
//         // 创建一个Glin索引
//         alex::Glin<double, geos::geom::Geometry*> index;
//         // poly_vec是一个包含几何对象的向量
//         std::vector<geos::geom::Geometry*> poly_vec;

//         // 创建几何工厂
//         auto pm = std::make_unique<geos::geom::PrecisionModel>();
//         geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(), -1);

//         geos::io::WKTReader reader(*global_factory);
//         std::vector<std::string> wkt_polygons = {
//             "POLYGON ((35 30, 40 30, 40 25, 35 25, 35 30))",// 矩形1
//             "POLYGON ((45 35, 50 35, 50 30, 45 30, 45 35))",// 矩形2
//             "POLYGON ((45 25, 50 25, 50 20, 45 20, 45 25))",// 矩形3
//             "POLYGON ((55 30, 60 30, 60 25, 55 25, 55 30))"// 矩形4
//         };

//         for (const auto& wkt : wkt_polygons) {
//             try {
//                 poly_vec.push_back(reader.read(wkt).release());// read 返回的智能指针std::unique_ptr<geos::geom::Geometry>,poly_vec只能接受原始指针
//             } catch (const geos::util::GEOSException& e) {
//                 std::cerr << "解析WKT失败: " << e.what() << std::endl;
//             }
//         }

//         double piecelimitation = 100.0; 
//         std::string curve_type = "z";// Z曲线填充
//         double cell_xmin = -100;
//         double cell_ymin = -90;
//         double cell_x_intvl = 1.0;
//         double cell_y_intvl = 1.0;
//         std::vector<std::tuple<double, double, double, double>> pieces;
//         // 加载数据到索引
//         index.glin_bulk_load(poly_vec, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

//         // 创建查询窗口（一个矩形）
//         geos::geom::Geometry *query_window;
//         std::vector<geos::geom::Geometry*> find_result;
//         int count_filter = 0;
//         std::string query_wkt = "POLYGON ((47 33, 48 33, 48 32, 47 32, 47 33))"; // 被矩形2包含

//         try {
//             query_window = reader.read(query_wkt).release();
//         } catch (const geos::util::GEOSException& e) {
//             std::cerr << "创建查询窗口失败: " << e.what() << std::endl;
//             REQUIRE(false); // 如果创建查询窗口失败，终止测试
//         }

//         // 用glin_find进行查询
//         index.glin_find(query_window, "z", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, find_result, count_filter);

//         // 输出查询结果
//         std::cout << "查询结果数量: " << find_result.size() << std::endl;

//         // 验证查询结果数量是否符合预期
//         CHECK(find_result.size() > 0); 

//         // 释放查询窗口内存
//         delete query_window;

//         // 释放多边形内存
//         for (auto geom : poly_vec) {
//             delete geom;
//         }
//     }
// }

#ifndef DOCTEST_INCLUDED
#define DOCTEST_INCLUDED
#include "./test/doctest.h"
#include "./glin/glin.h"
#include "./src/core/alex.h"
#include <geos/geom/Geometry.h>

#endif

#include <vector>

//endPointLess KModelSizeWeight shape_projection alex::cpu_supports_bmi() cal_error(int, double, double) curve_shape_projection(geos::geom::Geometry*,) 


// TEST_SUITE("GLIN Test Suite") {
//     TEST_CASE("GLIN Query Test") {
        // 创建一个Glin索引
    int main(){
        alex::Glin<double, geos::geom::Geometry*> index;
        // poly_vec是一个包含几何对象的向量
        std::vector<geos::geom::Geometry*> poly_vec;

        // 创建几何工厂
        auto pm = std::make_unique<geos::geom::PrecisionModel>();
        geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(), -1);

        geos::io::WKTReader reader(*global_factory);
        std::vector<std::string> wkt_polygons = {
            "POLYGON ((35 30, 40 30, 40 25, 35 25, 35 30))",// 矩形1
            "POLYGON ((45 35, 50 35, 50 30, 45 30, 45 35))",// 矩形2
            "POLYGON ((45 25, 50 25, 50 20, 45 20, 45 25))",// 矩形3
            "POLYGON ((55 30, 60 30, 60 25, 55 25, 55 30))"// 矩形4
        };

        for (const auto& wkt : wkt_polygons) {
            try {
                poly_vec.push_back(reader.read(wkt).release());// read 返回的智能指针std::unique_ptr<geos::geom::Geometry>,poly_vec只能接受原始指针
            } catch (const geos::util::GEOSException& e) {
                std::cerr << "解析WKT失败: " << e.what() << std::endl;
            }
        }

        double piecelimitation = 100.0; 
        std::string curve_type = "z";// Z曲线填充
        double cell_xmin = -100;
        double cell_ymin = -90;
        double cell_x_intvl = 1.0;
        double cell_y_intvl = 1.0;
        std::vector<std::tuple<double, double, double, double>> pieces;
        // 加载数据到索引
        index.glin_bulk_load(poly_vec, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

        // 创建查询窗口（一个矩形）
        geos::geom::Geometry *query_window;
        std::vector<geos::geom::Geometry*> find_result;
        int count_filter = 0;
        std::string query_wkt = "POLYGON ((47 33, 48 33, 48 32, 47 32, 47 33))"; // 被矩形2包含

        try {
            query_window = reader.read(query_wkt).release();
        } catch (const geos::util::GEOSException& e) {
            std::cerr << "创建查询窗口失败: " << e.what() << std::endl;
            REQUIRE(false); // 如果创建查询窗口失败，终止测试
        }

        // 用glin_find进行查询
        index.glin_find(query_window, "z", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, find_result, count_filter);

        // 输出查询结果
        std::cout << "查询结果数量: " << find_result.size() << std::endl;

        // 验证查询结果数量是否符合预期
        CHECK(find_result.size() > 0); 

        // 释放查询窗口内存
        delete query_window;

        // 释放多边形内存
        for (auto geom : poly_vec) {
            delete geom;
        }
    }
//     }
// }