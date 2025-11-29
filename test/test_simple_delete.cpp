// #include "./../glin/glin.h"
// #include <chrono>
// #include <iostream>
// #include <vector>
// #include <iomanip>

// // 简化的GLIN删除测试（禁用所有调试输出）
// class SimpleGLINDeleteTest {
// public:
//     static void runSimpleTest() {
//         std::cout << "🎯 简化GLIN删除测试" << std::endl;
//         std::cout << "===================" << std::endl;

//         // 使用小规模数据集进行测试
//         const int test_size = 100;
//         auto factory = geos::geom::GeometryFactory::create();

//         // 创建简单的矩形几何对象
//         std::vector<geos::geom::Geometry*> test_geoms;
//         for (int i = 0; i < test_size; i++) {
//             auto coords = new geos::geom::CoordinateArraySequence();
//             coords->add(geos::geom::Coordinate(i * 0.1, i * 0.1));
//             coords->add(geos::geom::Coordinate(i * 0.1 + 0.05, i * 0.1));
//             coords->add(geos::geom::Coordinate(i * 0.1 + 0.05, i * 0.1 + 0.05));
//             coords->add(geos::geom::Coordinate(i * 0.1, i * 0.1 + 0.05));
//             coords->add(geos::geom::Coordinate(i * 0.1, i * 0.1));

//             auto ring = factory->createLinearRing(coords);
//             auto polygon = factory->createPolygon(ring, nullptr);
//             test_geoms.push_back(polygon);
//         }

//         std::cout << "✅ 生成了 " << test_geoms.size() << " 个测试几何对象" << std::endl;

//         // 测试GLIN删除
//         std::cout << "\n🚀 开始删除测试..." << std::endl;
//         auto start_time = std::chrono::high_resolution_clock::now();

//         try {
//             alex::Glin<double, geos::geom::Geometry*> glin;

//             // 强制禁用Bloom过滤器，使用保守策略
//             glin.set_force_bloom_filter(false);
//             glin.set_force_strategy(alex::Glin<double, geos::geom::Geometry*>::FilteringStrategy::CONSERVATIVE);

//             std::string curve_type = "zorder";
//             double cell_xmin = 0.0, cell_ymin = 0.0;
//             double cell_x_intvl = 0.001, cell_y_intvl = 0.001;
//             std::vector<std::tuple<double, double, double, double>> pieces;

//             // 先批量加载所有数据
//             std::cout << "📦 批量加载 " << test_size << " 个对象..." << std::endl;
//             glin.glin_bulk_load(test_geoms, 1000.0, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

//             // 逐个删除一半数据
//             int delete_count = test_size / 2;
//             std::cout << "🗑️  逐个删除 " << delete_count << " 个对象..." << std::endl;

//             int successful_deletes = 0;
//             for (int i = 0; i < delete_count; i++) {
//                 auto* geom = test_geoms[i];

//                 try {
//                     double x = geom->getEnvelopeInternal()->getMinX();
//                     double y = geom->getEnvelopeInternal()->getMinY();
//                     geos::geom::LineSegment segment(geos::geom::Coordinate(x, y),
//                                                   geos::geom::Coordinate(x, y));

//                     double error_bound = 0.000001;
//                     int result = glin.erase_lineseg(geom, segment, error_bound, pieces);

//                     if (result > 0) {
//                         successful_deletes++;
//                     }

//                     if (i % 10 == 0) {
//                         std::cout << "  删除进度: " << (i + 1) << "/" << delete_count
//                                   << " (成功: " << successful_deletes << ")" << std::endl;
//                     }
//                 } catch (const std::exception& e) {
//                     std::cerr << "删除第 " << i << " 个对象时出错: " << e.what() << std::endl;
//                 }
//             }

//             auto end_time = std::chrono::high_resolution_clock::now();
//             auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

//             std::cout << "\n✅ 删除测试完成！" << std::endl;
//             std::cout << "  总耗时: " << duration_ms << "ms" << std::endl;
//             std::cout << "  删除吞吐量: " << (successful_deletes * 1000.0 / duration_ms) << " objects/sec" << std::endl;
//             std::cout << "  成功删除: " << successful_deletes << "/" << delete_count << std::endl;
//             std::cout << "  状态: 成功" << std::endl;

//         } catch (const std::exception& e) {
//             std::cerr << "❌ 测试失败: " << e.what() << std::endl;
//         }

//         // 清理几何对象
//         for (auto* geom : test_geoms) {
//             delete geom;
//         }
//         std::cout << "\n🧹 清理完成" << std::endl;
//     }
// };

// int main() {
//     SimpleGLINDeleteTest::runSimpleTest();
//     return 0;
// }


#include "./../glin/glin.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cmath>

// 🌊 AREAWATER数据集GLIN删除测试
class AREAWaterGLINDeleteTest {
public:
    // 加载AREAWATER数据集
    static std::vector<geos::geom::Geometry*> loadAREAWATERData(const std::string& filepath, int max_records = -1) {
        std::vector<geos::geom::Geometry*> geometries;
        auto factory = geos::geom::GeometryFactory::create();

        std::cout << "📦 加载AREAWATER数据集: " << filepath << std::endl;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "❌ 无法打开AREAWATER数据文件" << std::endl;
            return geometries;
        }

        std::string line;
        if (!std::getline(file, line)) return geometries; // 跳过标题

        while (std::getline(file, line) && (max_records == -1 || geometries.size() < max_records)) {
            std::istringstream iss(line);
            std::string field;
            std::vector<std::string> fields;
            while (std::getline(iss, field, ',')) fields.push_back(field);

            if (fields.size() >= 10) {
                try {
                    if (!fields[2].empty() && !fields[3].empty()) {
                        // 🛠️ 修复：移除 * 0.000001 缩放，使用原始经纬度
                        double x = std::stod(fields[2]);
                        double y = std::stod(fields[3]);
                        
                        // 简单的矩形构造
                        auto coords = new geos::geom::CoordinateArraySequence();
                        double size = 0.001; // 约100米
                        coords->add(geos::geom::Coordinate(x, y));
                        coords->add(geos::geom::Coordinate(x + size, y));
                        coords->add(geos::geom::Coordinate(x + size, y + size));
                        coords->add(geos::geom::Coordinate(x, y + size));
                        coords->add(geos::geom::Coordinate(x, y));
                        geometries.push_back(factory->createPolygon(factory->createLinearRing(coords), nullptr));
                        
                        if (geometries.size() % 5000 == 0) 
                            std::cout << "    已加载 " << geometries.size() << " 条..." << std::endl;
                    }
                } catch (...) { continue; }
            }
        }
        file.close();
        std::cout << "✅ 成功加载 " << geometries.size() << " 个对象" << std::endl;
        return geometries;
    }

    static void runTest() {
        std::cout << "🗑️ AREAWATER数据集 删除性能测试" << std::endl;
        std::cout << "===============================" << std::endl;

        // 1. 配置参数
        const std::string areawater_path = "/mnt/hgfs/sharedFolder/AREAWATER.csv";
        const int test_size = 800000; // 测试规模
        
        // 2. 加载数据
        auto test_geoms = loadAREAWATERData(areawater_path, test_size);
        if (test_geoms.empty()) return;

        // 3. 自动计算坐标边界 (防止段错误的关键!)
        double global_min_x = std::numeric_limits<double>::max();
        double global_min_y = std::numeric_limits<double>::max();
        double global_max_x = std::numeric_limits<double>::lowest(); // 用于调试打印
        
        for (const auto* geom : test_geoms) {
            if (!geom || geom->isEmpty()) continue;
            const auto* env = geom->getEnvelopeInternal();
            if (env->getMinX() < global_min_x) global_min_x = env->getMinX();
            if (env->getMinY() < global_min_y) global_min_y = env->getMinY();
            if (env->getMaxX() > global_max_x) global_max_x = env->getMaxX();
        }
        
        // 设置安全的网格原点 (比最小值略小，防止负数索引)
        double cell_xmin = global_min_x - 1.0; 
        double cell_ymin = global_min_y - 1.0;
        
        // 设置合理的网格间隔 (对于经纬度，0.01 度约等于 1km)
        double cell_x_intvl = 0.01;
        double cell_y_intvl = 0.01;
        std::string curve_type = "z"; 

        std::cout << "[配置修正] 数据范围 MinX=" << global_min_x << " MaxX=" << global_max_x << std::endl;
        std::cout << "[配置修正] 网格原点 cell_xmin=" << cell_xmin << " 间隔=" << cell_x_intvl << std::endl;

        // 4. 初始化GLIN
        try {
            alex::Glin<double, geos::geom::Geometry*> glin;
            glin.set_force_bloom_filter(false);
            glin.set_force_strategy(alex::Glin<double, geos::geom::Geometry*>::FilteringStrategy::CONSERVATIVE);
            
            std::vector<std::tuple<double, double, double, double>> pieces;
            double pieceLimit = 1000000.0; 

            // 5. 批量加载 (构建索引)
            std::cout << "\n📦 正在构建索引 (Bulk Load) " << test_geoms.size() << " 个对象..." << std::endl;
            auto start_load = std::chrono::high_resolution_clock::now();
            
            glin.glin_bulk_load(test_geoms, pieceLimit, curve_type, 
                              cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
            
            auto end_load = std::chrono::high_resolution_clock::now();
            std::cout << "✅ 索引构建完成，耗时: " 
                      << std::chrono::duration_cast<std::chrono::milliseconds>(end_load - start_load).count() 
                      << " ms" << std::endl;

            if (glin.size() == 0) {
                std::cerr << "❌ 索引为空，无法进行删除测试！" << std::endl;
                return;
            }

            // 6. 删除测试  确定删除的比例
            int delete_start_idx = test_geoms.size() * 0.2;
            int delete_count = test_geoms.size() - delete_start_idx;
            
            std::cout << "\n🚀 开始删除测试 (删除最后 " << delete_count << " 个对象)..." << std::endl;
            
            int success_count = 0;
            auto start_del = std::chrono::high_resolution_clock::now();

            for (int i = delete_start_idx; i < test_geoms.size(); ++i) {
                int ret = glin.erase(test_geoms[i], curve_type, 
                                   cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, 
                                   pieceLimit, pieces);
                
                if (ret > 0) success_count++;
                
                if ((i - delete_start_idx + 1) % 2000 == 0) {
                    std::cout << "    已处理 " << (i - delete_start_idx + 1) << " 个删除请求..." << std::endl;
                }
            }

            auto end_del = std::chrono::high_resolution_clock::now();
            long duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_del - start_del).count();

            std::cout << "\n✅ 删除测试结束" << std::endl;
            std::cout << "  尝试删除: " << delete_count << std::endl;
            std::cout << "  成功删除: " << success_count << std::endl;
            std::cout << "  总耗时: " << duration_ms << " ms" << std::endl;
            std::cout << "  吞吐量: " << (delete_count * 1000.0 / (duration_ms + 1)) << " ops/sec" << std::endl;

        } catch (const std::exception& e) {
            std::cerr << "❌ 发生异常: " << e.what() << std::endl;
        }

        // 清理内存
        for (auto* g : test_geoms) delete g;
    }
};

int main() {
    AREAWaterGLINDeleteTest::runTest();
    return 0;
}