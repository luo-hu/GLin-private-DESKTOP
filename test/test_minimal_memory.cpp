#include "./../glin/glin.h"
#include <geos/io/WKTReader.h>
#include <chrono>
#include <iostream>

int main() {
    std::cout << "=== 最小化内存测试 ===" << std::endl;

    try {
        // 创建最小测试数据
        geos::geom::GeometryFactory::Ptr factory = geos::geom::GeometryFactory::create();
        geos::io::WKTReader reader(factory.get());

        std::vector<geos::geom::Geometry*> test_geoms;

        // 只创建2个简单几何对象
        std::string wkt1 = "POLYGON((0 0,0 1,1 1,1 0,0 0))";
        std::string wkt2 = "POLYGON((2 2,2 3,3 3,3 2,2 2))";

        test_geoms.push_back(reader.read(wkt1).release());
        test_geoms.push_back(reader.read(wkt2).release());

        std::cout << "创建了" << test_geoms.size() << "个几何对象" << std::endl;

        // 配置GLIN
        alex::Glin<double, geos::geom::Geometry*> glin;
        double piecelimitation = 10.0;
        std::string curve_type = "z";
        double cell_xmin = -10;
        double cell_ymin = -10;
        double cell_x_intvl = 1.0;
        double cell_y_intvl = 1.0;

        std::vector<std::tuple<double, double, double, double>> pieces;

        std::cout << "开始加载..." << std::endl;
        glin.glin_bulk_load(test_geoms, piecelimitation, curve_type,
                          cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);

        std::cout << "加载成功！分段数量：" << pieces.size() << std::endl;

        // 清理内存
        for (auto* geom : test_geoms) {
            delete geom;
        }

        std::cout << "内存清理完成" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "异常: " << e.what() << std::endl;
        return -1;
    }

    std::cout << "=== 测试成功完成 ===" << std::endl;
    return 0;
}