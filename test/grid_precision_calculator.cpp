#include <iostream>
#include <cmath>
#include <iomanip>

// 网格精度计算器
class GridPrecisionCalculator {
private:
    static constexpr double EARTH_RADIUS = 6371000.0; // 地球半径，米
    static constexpr double M_TO_KM = 0.001;

public:
    // 计算指定纬度处1度经度对应的米数
    static double metersPerDegreeLongitude(double latitude) {
        double lat_rad = latitude * M_PI / 180.0;
        return EARTH_RADIUS * M_PI / 180.0 * cos(lat_rad);
    }

    // 计算1度纬度对应的米数（全球不变）
    static double metersPerDegreeLatitude() {
        return EARTH_RADIUS * M_PI / 180.0;
    }

    // 计算给定网格精度对应的实际地面距离
    struct GridSize {
        double longitude_meters;  // 经度方向的米数
        double latitude_meters;   // 纬度方向的米数
        double area_m2;          // 网格面积，平方米
        std::string description;
    };

    static GridSize calculateGridSize(double cell_x_intvl, double cell_y_intvl, double latitude = 0.0) {
        GridSize size;
        size.longitude_meters = cell_x_intvl * metersPerDegreeLongitude(latitude);
        size.latitude_meters = cell_y_intvl * metersPerDegreeLatitude();
        size.area_m2 = size.longitude_meters * size.latitude_meters;

        // 生成描述
        std::ostringstream desc;
        desc << "网格 " << cell_x_intvl << "°×" << cell_y_intvl << "° ≈ "
             << std::fixed << std::setprecision(1)
             << size.longitude_meters << "m × " << size.latitude_meters << "m";

        if (size.area_m2 < 1000) {
            desc << " (" << std::setprecision(1) << size.area_m2 << "m²)";
        } else {
            desc << " (" << std::setprecision(2) << (size.area_m2 * 0.0001) << "公顷)";
        }

        size.description = desc.str();
        return size;
    }

    // 基于应用场景推荐网格精度
    struct Application {
        std::string name;
        double cell_x_intvl;
        double cell_y_intvl;
        std::string description;
    };

    static std::vector<Application> getRecommendedGridSizes(double data_latitude) {
        return {
            {"建筑物级别", 0.0001, 0.0001, "约10m×10m，适合建筑���内部精度"},
            {"街区级别", 0.001, 0.001, "约100m×100m，适合城市街区分析"},
            {"社区级别", 0.005, 0.005, "约500m×500m，适合社区规划"},
            {"区域级别", 0.01, 0.01, "约1km×1km，适合区域统计"},
            {"城市级别", 0.1, 0.1, "约10km×10km，适合城市级分析"},
            {"省级级别", 1.0, 1.0, "约100km×100m，适合省级统计"}
        };
    }
};

int main() {
    std::cout << "=== GLIN-HF 网格精度计算器 ===" << std::endl;

    // 您的数据位置
    double data_latitude = 31.8; // 北纬31.8度
    std::cout << "数据纬度: " << data_latitude << "°N" << std::endl;

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "基础地理计算:" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    std::cout << "1度纬度 = " << GridPrecisionCalculator::metersPerDegreeLatitude() << " 米" << std::endl;
    std::cout << "1度经度(@" << data_latitude << "°N) = "
              << GridPrecisionCalculator::metersPerDegreeLongitude(data_latitude) << " 米" << std::endl;

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "不同网格精度的地面距离:" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    // 测试不同的网格精度
    std::vector<double> test_intervals = {0.0001, 0.0005, 0.001, 0.005, 0.01, 0.05, 0.1};

    for (double interval : test_intervals) {
        auto size = GridPrecisionCalculator::calculateGridSize(interval, interval, data_latitude);
        std::cout << std::setw(10) << interval << "° × " << std::setw(10) << interval << "° : "
                  << size.description << std::endl;
    }

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "应用场景推荐网格精度:" << std::endl;
    std::cout << std::string(80, '=') << std::endl;

    auto apps = GridPrecisionCalculator::getRecommendedGridSizes(data_latitude);
    for (const auto& app : apps) {
        auto size = GridPrecisionCalculator::calculateGridSize(app.cell_x_intvl, app.cell_y_intvl, data_latitude);
        std::cout << std::setw(12) << app.name << ": " << std::setw(10) << app.cell_x_intvl << "° × "
                  << std::setw(10) << app.cell_y_intvl << "° → " << size.description << std::endl;
        std::cout << "    描述: " << app.description << std::endl << std::endl;
    }

    std::cout << "🎯 分析建议:" << std::endl;
    std::cout << "您的成功配置 (0.001°×0.001°) 对应街区级别精度，" << std::endl;
    std::cout << "这是地理空间数据分析的最佳平衡点：" << std::endl;
    std::cout << "✅ 足够细：能区分不同建筑物和街道" << std::endl;
    std::cout << "✅ 足够高效：避免过多空网格和内存浪费" << std::endl;
    std::cout << "✅ 适合大多数GIS应用：城市管理、交通分析、环境监测等" << std::endl;

    return 0;
}