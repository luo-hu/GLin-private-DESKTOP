
// #include "./../glin/glin.h"  // 包含修改后的GLIN-HF
// #include <geos/io/WKTReader.h>
// #include <chrono>
// #include <iostream>
// int main() {
//     // 1. 准备测试数据（1万个随机多边形）
//     geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
//     geos::io::WKTReader reader(factory.get());
//     std::vector<geos::geom::Geometry*> geoms;
//     for (int i = 0; i < 10; ++i) {
//         // 生成随机多边形（示例WKT）,避免重叠，步长为10
//         double x1 = i*5;
//         double y1 = i*5;
//         double x2 = x1 + 3;
//         double y2 = y1 + 3;//矩形的宽和高为5
//         //合法WKT：闭合矩形，首尾坐标相同
//         std::string wkt = "POLYGON((" + std::to_string(x1) + " " + std::to_string(y1) + "," + 
//                                         std::to_string(x1) + " " + std::to_string(y2) + "," +
//                                         std::to_string(x2) + " " + std::to_string(y2) + "," +
//                                         std::to_string(x2) + " " + std::to_string(y1) + "," +
//                                         std::to_string(x1) + " " + std::to_string(y1) + "))";
//         //geoms.push_back(reader.read(wkt).get());//解析合法WKT，get获取的原始指针，不能用智能指针
//         // 修复后（release()转移所有权，unique_ptr不再管理对象，避免提前释放）
//         std::unique_ptr<geos::geom::Geometry> geom_ptr = reader.read(wkt);
//         // 新增：检查几何对象是否生成成功
//         if (!geom_ptr) {
//             std::cerr << "错误：生成几何对象失败！WKT=" << wkt << std::endl;
//             return -1;  // 直接退出，避免后续错误
//         }
//         geoms.push_back(geom_ptr.release());  // 转移所有权到geoms，后续手动释放
//          std::cout << "生成第" << i << "个几何对象：" << wkt << std::endl;  // 新增日志：确认WKT正确
//     }
//  std::cout << "测试数据生成完成，共" << geoms.size() << "个对象" << std::endl;  // 日志：确认数据生成成功

//     // 2. 初始化GLIN和GLIN-HF
//     alex::Glin<double, geos::geom::Geometry*> glin_original;
//     alex::Glin<double, geos::geom::Geometry*> glin_hf;  // 带过滤器的版本

//     // 3. 加载数据
//     double piecelimitation = 100.0; 
//     std::string curve_type = "z";//Z曲线填充
//     double cell_xmin = 0;
//     double cell_ymin = 0;
//     double cell_x_intvl = 1.0;
//     double cell_y_intvl = 1.0;
//     std::cout << "开始加载数据到GLIN..." << std::endl;  // 日志：标记加载开始
//     std::vector<std::tuple<double, double, double, double>> pieces;
//     auto start_load = std::chrono::high_resolution_clock::now();
//     glin_original.glin_bulk_load(geoms, piecelimitation, "zorder", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
//     glin_hf.glin_bulk_load(geoms, piecelimitation, "zorder", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);  // 会初始化过滤器
//     auto end_load = std::chrono::high_resolution_clock::now();
//     std::cout << "加载时间: " << (end_load - start_load).count() << "ns\n";
//     std::cout << "数据加载完成，耗时：" << std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load).count() << "ms" << std::endl;  // 日志：标记加载完成
//     // 4. 执行查询（示例：100次随机窗口查询）
//     // int total_filter_original = 0, total_filter_hf = 0;
//     // std::vector<geos::geom::Geometry*> res_original, res_hf;
//     // auto start_query = std::chrono::high_resolution_clock::now();
//     // for (int i = 0; i < 100; ++i) {
//     //    // 生成合法查询窗口（比如中心在(500,500)、边长20的矩形）
//     //     double cx = 500, cy = 500, r = 10;
//     //     std::string query_wkt = "POLYGON((" +
//     //                            std::to_string(cx - r) + " " + std::to_string(cy - r) + ", " +
//     //                            std::to_string(cx - r) + " " + std::to_string(cy + r) + ", " +
//     //                            std::to_string(cx + r) + " " + std::to_string(cy + r) + ", " +
//     //                            std::to_string(cx + r) + " " + std::to_string(cy - r) + ", " +
//     //                            std::to_string(cx - r) + " " + std::to_string(cy - r) + "))";
//     //     // geos::geom::Geometry* query = reader.read(query_wkt).get();
//     //     // 修复后
//     //     std::unique_ptr<geos::geom::Geometry> query_ptr = reader.read(query_wkt);
//     //     geos::geom::Geometry* query = query_ptr.release();
//     //     // 原始GLIN查询
//     //     res_original.clear();
//     //     total_filter_original = 0;
//     //     glin_original.glin_find(query, "zorder", 0, 0, 1, 1, pieces, res_original, total_filter_original);

//     //     // GLIN-HF查询
//     //     res_hf.clear();
//     //     total_filter_hf = 0;
//     //     glin_hf.glin_find(query, "zorder", 0, 0, 1, 1, pieces, res_hf, total_filter_hf);
//     // }
//     // auto end_query = std::chrono::high_resolution_clock::now();
//     // std::cout << "查询时间: " << (end_query - start_query).count() << "ns\n";
//     // std::cout << "原始GLIN过滤数量: " << total_filter_original << "\n";
//     // std::cout << "GLIN-HF过滤数量: " << total_filter_hf << "\n";  // 预期更小
//         // 3. 执行查询（仅1次查询，简化窗口）
//     std::cout << "开始执行查询..." << std::endl;  // 日志：标记查询开始
//     int total_filter_original = 0, total_filter_hf = 0;
//     std::vector<geos::geom::Geometry*> res_original, res_hf;
//     auto start_query = std::chrono::high_resolution_clock::now();

//     // 极简查询窗口（坐标[10,10]到[20,20]的矩形，确保覆盖部分测试数据）
//     // std::string query_wkt = "POLYGON((10 10,10 20,20 20,20 10,10 10))";
//         //std::string query_wkt = "POLYGON((6 6,6 7,7 7,7 6,6 6))";//包含
//         std::string query_wkt = "POLYGON((4 4,4 9,9 9,9 4,4 4))";  // 完全包含几何对象1
//         //std::string query_wkt = "POLYGON((6 6,6 7,9 7,9 6,6 6))";//相交
//        // std::string query_wkt = "POLYGON((0 0,0 3,3 3,3 0,0 0))";

//     std::unique_ptr<geos::geom::Geometry> query_ptr = reader.read(query_wkt);
//     geos::geom::Geometry* query = query_ptr.release();
//     std::cout << "查询窗口：" << query_wkt << std::endl;  // 日志：确认查询窗口正确
//     // 仅执行1次查询（减少崩溃可能性）
//     res_original.clear();
//     total_filter_original = 0;
//     glin_original.glin_find(query, "zorder", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_original, total_filter_original);
//     std::cout << "原始GLIN查询完成，结果数：" << res_original.size() << std::endl;  // 日志：标记原始GLIN查询完成

//     res_hf.clear();
//     total_filter_hf = 0;
//     glin_hf.glin_find(query, "zorder", cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_hf, total_filter_hf);
//     std::cout << "GLIN-HF查询完成，结果数：" << res_hf.size() << std::endl;  // 日志：标记GLIN-HF查询完成

//     auto end_query = std::chrono::high_resolution_clock::now();
//     std::cout << "1次查询总时间：" << std::chrono::duration_cast<std::chrono::milliseconds>(end_query - start_query).count() << "ms" << std::endl;
//     std::cout << "原始GLIN过滤数量：" << total_filter_original << std::endl;
//     std::cout << "GLIN-HF过滤数量：" << total_filter_hf << std::endl;
//     return 0;
// }

// #include "./../glin/glin.h"  // 包含修改后的GLIN-HF
// #include <geos/io/WKTReader.h>
// #include <chrono>
// #include <iostream>
// int main() {
//     // 1. 准备测试数据（1万个随机多边形）
//     geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
//     geos::io::WKTReader reader(factory.get());
//     std::vector<geos::geom::Geometry*> geoms;
//     for (int i = 0; i < 10; ++i) {
//         // 生成随机多边形（示例WKT）,避免重叠，步长为10
//         double x1 = i*5;
//         double y1 = i*5;
//         double x2 = x1 + 3;
//         double y2 = y1 + 3;//矩形的宽和高为5
//         //合法WKT：闭合矩形，首尾坐标相同
//         std::string wkt = "POLYGON((" + std::to_string(x1) + " " + std::to_string(y1) + "," + 
//                                         std::to_string(x1) + " " + std::to_string(y2) + "," +
//                                         std::to_string(x2) + " " + std::to_string(y2) + "," +
//                                         std::to_string(x2) + " " + std::to_string(y1) + "," +
//                                         std::to_string(x1) + " " + std::to_string(y1) + "))";
//         //geoms.push_back(reader.read(wkt).get());//解析合法WKT，get获取的原始指针，不能用智能指针
//         // 修复后（release()转移所有权，unique_ptr不再管理对象，避免提前释放）
//         std::unique_ptr<geos::geom::Geometry> geom_ptr = reader.read(wkt);
//         // 新增：检查几何对象是否生成成功
//         if (!geom_ptr) {
//             std::cerr << "错误：生成几何对象失败！WKT=" << wkt << std::endl;
//             return -1;  // 直接退出，避免后续错误
//         }
//         geoms.push_back(geom_ptr.release());  // 转移所有权到geoms，后续手动释放
//          std::cout << "生成第" << i << "个几何对象：" << wkt << std::endl;  // 新增日志：确认WKT正确
//     }
//  std::cout << "测试数据生成完成，共" << geoms.size() << "个对象" << std::endl;  // 日志：确认数据生成成功

//     // 2. 初始化GLIN和GLIN-HF
//     alex::Glin<double, geos::geom::Geometry*> glin_original;
//     alex::Glin<double, geos::geom::Geometry*> glin_hf;  // 带过滤器的版本

//     // 3. 加载数据
//     double piecelimitation = 100.0; 
//     std::string curve_type = "z";//Z曲线填充
//     // 修改cell_xmin和cell_ymin为负值，确保能包含所有几何对象
//     // double cell_xmin = -1;
//     // double cell_ymin = -1;
//     // double cell_x_intvl = 0.1;  // 减小间隔以提高精度
//     // double cell_y_intvl = 0.1;
//         double cell_xmin = 0.0;    // 网格起点应与数据最小坐标对齐
//     double cell_ymin = 0.0;
//     double cell_x_intvl = 1.0; // 使用合适的单元格大小
//     double cell_y_intvl = 1.0;
//     std::cout << "开始加载数据到GLIN..." << std::endl;  // 日志：标记加载开始
//     std::vector<std::tuple<double, double, double, double>> pieces;
//     auto start_load = std::chrono::high_resolution_clock::now();
//     glin_original.glin_bulk_load(geoms, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
//     glin_hf.glin_bulk_load(geoms, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);  // 会初始化过滤器
//     auto end_load = std::chrono::high_resolution_clock::now();
//     std::cout << "加载时间: " << (end_load - start_load).count() << "ns\n";
//     std::cout << "数据加载完成，耗时：" << std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load).count() << "ms" << std::endl;  // 日志：标记加载完成
//     // 4. 执行查询（仅1次查询，简化窗口）
//     std::cout << "开始执行查询..." << std::endl;  // 日志：标记查询开始
//     int total_filter_original = 0, total_filter_hf = 0;
//     std::vector<geos::geom::Geometry*> res_original, res_hf;
//     auto start_query = std::chrono::high_resolution_clock::now();

//     // 使用应该与第二个几何对象相交的查询窗口
//     //std::string query_wkt = "POLYGON((4 4,4 9,9 9,9 4,4 4))";  // 应该包含几何对象1 (5,5,8,8) 
//     //std::string query_wkt = "POLYGON((4 4,4 9,8 9,8 4,4 4))";  // 应该与几何对象1 (5,5,8,8) 相交
//     //std::string query_wkt = "POLYGON((5 3,5 8,10 8,10 3,5 3))";  // 应该与几何对象1 (5,5,8,8) 相交
//     //std::string query_wkt = "POLYGON((5 5,5 8,8 8,8 5,5 5))";  // 应该与几何对象1 (5,5,8,8) 重合
//     //std::string query_wkt = "POLYGON((10 10,10 20,20 20,20 10,10 10))";
//         //std::string query_wkt = "POLYGON((6 6,6 7,7 7,7 6,6 6))";//被查询窗口包含，肯定是查询不到
//         //std::string query_wkt = "POLYGON((4 4,4 9,9 9,9 4,4 4))";  // 完全包含被查对象
//         //    std::string query_wkt = "POLYGON((6 6,6 7,9 7,9 6,6 6))";//相交
//         std::string query_wkt = "POLYGON((4 4,4 7,6 7,6 4,4 4))";//相交(查询框的左下角要小于被查对象的左下角)
//         //std::string query_wkt = "POLYGON((6 6,6 7,9 7,9 6,6 6))";//相交(查询框的左下角要大于被查对象的左下角)
//         //std::string query_wkt = "POLYGON((3 3,3 9,10 9,10 3,3 3))";//相交
//         //std::string query_wkt = "POLYGON((0 4,0 7,3 7,3 4,0 4))";//不相交
//     std::unique_ptr<geos::geom::Geometry> query_ptr = reader.read(query_wkt);
//     geos::geom::Geometry* query = query_ptr.release();
//     std::cout << "查询窗口：" << query_wkt << std::endl;  // 日志：确认查询窗口正确
//     // 仅执行1次查询（减少崩溃可能性）
//     res_original.clear();
//     total_filter_original = 0;
//     glin_original.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_original, total_filter_original);
//     std::cout << "原始GLIN查询完成，结果数：" << res_original.size() << std::endl;  // 日志：标记原始GLIN查询完成

//     res_hf.clear();
//     total_filter_hf = 0;
//     glin_hf.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_hf, total_filter_hf);
//     std::cout << "GLIN-HF查询完成，结果数：" << res_hf.size() << std::endl;  // 日志：标记GLIN-HF查询完成

//     auto end_query = std::chrono::high_resolution_clock::now();
//     std::cout << "1次查询总时间：" << std::chrono::duration_cast<std::chrono::milliseconds>(end_query - start_query).count() << "ms" << std::endl;
//     std::cout << "原始GLIN过滤数量：" << total_filter_original << std::endl;
//     std::cout << "GLIN-HF过滤数量：" << total_filter_hf << std::endl;
    
//     // 添加调试信息，打印所有找到的几何对象
//     std::cout << "原始GLIN查询结果：" << std::endl;
//     for (size_t i = 0; i < res_original.size(); ++i) {
//         std::cout << "  结果 " << i << ": " << res_original[i]->toString() << std::endl;
//     }
    
//     std::cout << "GLIN-HF查询结果：" << std::endl;
//     for (size_t i = 0; i < res_hf.size(); ++i) {
//         std::cout << "  结果 " << i << ": " << res_hf[i]->toString() << std::endl;
//     }
    
//     return 0;
// }



#include "./../glin/glin.h"  // 包含修改后的GLIN-HF
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>
int main() {
    // 1. 准备测试数据（1万个随机多边形）
    geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
    geos::io::WKTReader reader(factory.get());
    std::vector<geos::geom::Geometry*> geoms;
    for (int i = 0; i < 10; ++i) {
        // 生成随机多边形（示例WKT）,避免重叠，步长为10
        double x1 = i*5;
        double y1 = i*5;
        double x2 = x1 + 3;
        double y2 = y1 + 3;//矩形的宽和高为5
        //合法WKT：闭合矩形，首尾坐标相同
        std::string wkt = "POLYGON((" + std::to_string(x1) + " " + std::to_string(y1) + "," + 
                                        std::to_string(x1) + " " + std::to_string(y2) + "," +
                                        std::to_string(x2) + " " + std::to_string(y2) + "," +
                                        std::to_string(x2) + " " + std::to_string(y1) + "," +
                                        std::to_string(x1) + " " + std::to_string(y1) + "))";
        //geoms.push_back(reader.read(wkt).get());//解析合法WKT，get获取的原始指针，不能用智能指针
        // 修复后（release()转移所有权，unique_ptr不再管理对象，避免提前释放）
        std::unique_ptr<geos::geom::Geometry> geom_ptr = reader.read(wkt);
        // 新增：检查几何对象是否生成成功
        if (!geom_ptr) {
            std::cerr << "错误：生成几何对象失败！WKT=" << wkt << std::endl;
            return -1;  // 直接退出，避免后续错误
        }
        geoms.push_back(geom_ptr.release());  // 转移所有权到geoms，后续手动释放
         std::cout << "生成第" << i << "个几何对象：" << wkt << std::endl;  // 新增日志：确认WKT正确
    }
 std::cout << "测试数据生成完成，共" << geoms.size() << "个对象" << std::endl;  // 日志：确认数据生成成功

    // 2. 初始化GLIN和GLIN-HF
    alex::Glin<double, geos::geom::Geometry*> glin_original;
    alex::Glin<double, geos::geom::Geometry*> glin_hf;  // 带过滤器的版本

    // 3. 加载数据
    double piecelimitation = 100.0; 
    std::string curve_type = "z";//Z曲线填充
    // 修改cell_xmin和cell_ymin为负值，确保能包含所有几何对象
    // double cell_xmin = -1;
    // double cell_ymin = -1;
    // double cell_x_intvl = 0.1;  // 减小间隔以提高精度
    // double cell_y_intvl = 0.1;
        double cell_xmin = 0.0;    // 网格起点应与数据最小坐标对齐
    double cell_ymin = 0.0;
    double cell_x_intvl = 1.0; // 使用合适的单元格大小
    double cell_y_intvl = 1.0;
    std::cout << "开始加载数据到GLIN..." << std::endl;  // 日志：标记加载开始
    std::vector<std::tuple<double, double, double, double>> pieces;
    auto start_load = std::chrono::high_resolution_clock::now();
    glin_original.glin_bulk_load(geoms, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
    glin_hf.glin_bulk_load(geoms, piecelimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);  // 会初始化过滤器
    auto end_load = std::chrono::high_resolution_clock::now();
    std::cout << "加载时间: " << (end_load - start_load).count() << "ns\n";
    std::cout << "数据加载完成，耗时：" << std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load).count() << "ms" << std::endl;  // 日志：标记加载完成
    // 4. 执行查询（测试两个查询窗口）
    std::cout << "开始执行查询测试..." << std::endl;

    // 测试两个不同的查询窗口
    std::vector<std::string> test_queries = {
        "POLYGON((4 4,4 7,6 7,6 4,4 4))",  // 查询框左下角小于被查对象左下角
        "POLYGON((6 6,6 7,9 7,9 6,6 6))",  // 查询框左下角大于被查对象左下角
        "POLYGON((9 11,9 14,12 14,12 11,9 11))",//相交
        "POLYGON((11 9,11 11,12 11,12 9,11 9))",//相交
        "POLYGON((11 9,11 11,14 11,14 9,11 9))",//相交
        "POLYGON((2 2,2 6,6 6,6 2,2 2))",//与两个对象相交
        "POLYGON((6 6,6 11,11 11,11 6,6 6))",//与两个对象相交
        "POLYGON((4 4,4 16,16 16,16 4,4 4))"//与三个对象相交

        // "POLYGON((1 1,1 2,2 2,2 1,1 1))"//查询窗口被几何对象 (0,0,3,3) 完全包含
        // "POLYGON((5 5,5 8,8 8,8 5,5 5))", // 应该与几何对象 (5,5,8,8) 重合
        // "POLYGON((5 5,5 8,7 8,7 5,5 5))", // 应该与几何对象 (5,5,8,8) 部分重合
        // "POLYGON((4 4,4 9,9 9,9 4,4 4))", //完全包含几何对象(5,5,8,8) 
        // "POLYGON((4 4,4 15,15 15,15 4,4 4))" //完全包含几何对象(5,5,8,8) (10,10,13,13) 

    };

    for (size_t query_idx = 0; query_idx < test_queries.size(); ++query_idx) {
        std::string query_wkt = test_queries[query_idx];
        std::cout << "\n========== 测试查询窗口 " << (query_idx + 1) << " ==========" << std::endl;
        std::cout << "查询窗口：" << query_wkt << std::endl;

        int total_filter_original = 0, total_filter_hf = 0;
        std::vector<geos::geom::Geometry*> res_original, res_hf;
        auto start_query = std::chrono::high_resolution_clock::now();

        std::unique_ptr<geos::geom::Geometry> query_ptr = reader.read(query_wkt);
        geos::geom::Geometry* query = query_ptr.release();

        // 原始GLIN查询
        res_original.clear();
        total_filter_original = 0;
        glin_original.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_original, total_filter_original);
        std::cout << "原始GLIN查询完成，结果数：" << res_original.size() << std::endl;

        // GLIN-HF查询
        res_hf.clear();
        total_filter_hf = 0;
        glin_hf.glin_find(query, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces, res_hf, total_filter_hf);
        std::cout << "GLIN-HF查询完成，结果数：" << res_hf.size() << std::endl;

        auto end_query = std::chrono::high_resolution_clock::now();
        std::cout << "查询时间：" << std::chrono::duration_cast<std::chrono::milliseconds>(end_query - start_query).count() << "ms" << std::endl;
        std::cout << "原始GLIN过滤数量：" << total_filter_original << std::endl;
        std::cout << "GLIN-HF过滤数量：" << total_filter_hf << std::endl;

        // 打印找到的几何对象
        if (res_hf.size() > 0) {
            std::cout << "GLIN-HF找到的几何对象：" << std::endl;
            for (size_t i = 0; i < res_hf.size(); ++i) {
                std::cout << "  结果 " << i << ": " << res_hf[i]->toString() << std::endl;
            }
        }

        delete query;  // 清理查询对象
    }
  
    return 0;
}