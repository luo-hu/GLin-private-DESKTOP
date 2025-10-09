

// #include <iostream>
// #include <fstream>
// #include <sstream>
// #include <vector>
// #include <memory>
// #include <limits>
// #include <string>
// #include <algorithm>

// #include <geos/geom/GeometryFactory.h>
// #include <geos/geom/Polygon.h>
// #include <geos/geom/Envelope.h>
// #include <geos/io/WKTReader.h>

// // 确保这些路径相对于你的编译环境是正确的
// #include "glin/glin.h" // GLIN索引的头文件

// // 处理WKT多边形字符串，并转换为GEOS多边形对象
// geos::geom::Polygon* createPolygonFromWKT(const std::string& wkt)
// {
//     std::cout<<"wkt:"<<wkt<<std::endl;
//     geos::io::WKTReader reader;
//     try {
//         std::unique_ptr<geos::geom::Geometry> geom = reader.read(wkt);
//         if (geom && geom->getGeometryTypeId() == geos::geom::GEOS_POLYGON) {
//             //首先检查智能指针 geom是否为空（即是否成功创建了一个几何对象）。如果解析失败，geom可能为空指针。
//             //在确认对象存在后，进一步检查这个几何对象的类型是否为​​多边形
            
//             return dynamic_cast<geos::geom::Polygon*>(geom.release());
//             std::cout<<"wkt解析成功........."<<endl;
//         }
//     } catch (const std::exception& e) { // 捕获更具体的异常
//         std::cerr << "解析WKT字符串时发生异常: " << e.what() << " WKT: " << wkt << std::endl;
//     } catch (...) {
//         std::cerr << "解析WKT字符串时发生未知错误: " << wkt << std::endl;
//     }
//     return nullptr;
// }

// void curve_shape_projection(const geos::geom::Envelope* envelope, const std::string& curve_type, double cell_xmin, double cell_ymin, double cell_x_intvl, double cell_y_intvl, double& dist_start, double& dist_end)
// {
//     dist_start = std::numeric_limits<double>::max();
//     dist_end = std::numeric_limits<double>::min();

//     if (!envelope) return;

//     double minX = envelope->getMinX();
//     double minY = envelope->getMinY();
//     double maxX = envelope->getMaxX();
//     double maxY = envelope->getMaxY();

//     auto encoder = new Encoder<double>(cell_xmin, cell_x_intvl, cell_ymin, cell_y_intvl);

//     if (curve_type == "h") {
//         auto index_double_h = encoder->encode_h(minX, minY, maxX, maxY);
//         dist_start = index_double_h.first;
//         dist_end = index_double_h.second;
//     }
//     if (curve_type == "z") {
//         auto index_z = encoder->encode_z(minX, minY, maxX, maxY);
//         dist_start = index_z.first;
//         dist_end = index_z.second;
//     }
//     delete encoder;
// }

// int main()
// {
//     // 确保 testCSV.csv 文件在你的执行目录下
//     std::ifstream inputFile("/mnt/hgfs/ShareFiles/AREAWATER.csv");
//     if (!inputFile.is_open()) {
//         std::cerr << "打开输入文件 ./testCSV.csv 失败" << std::endl;
//         return 1;
//     }

//     std::vector<double> zAddresses;
//     auto factory = geos::geom::GeometryFactory::create();

//     // GLIN索引参数配置
//     double xmin = -180.0, ymin = -90.0;
//     double cell_xintvl = 0.0001, cell_yinval = 0.0001;
//     std::string curve_type = "z";

//     std::string line;
//     while (getline(inputFile, line)) {
//         // --- NEW FIX: 移除 UTF-8 BOM ---
//         // BOM由三个字节组成: EF BB BF
//         if (line.length() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') {
//             line = line.substr(3);//从第四个字符开始截取
//         }
//         // --- END OF NEW FIX ---
        
//         //从包含额外数据的列中分离出WKT字符串
//         //查找POLYGON定义的结尾’）)' 在这里截断字符串
//         size_t wkt_end_pos = line.find("))");
//         if(wkt_end_pos != std::string::npos)
//         {
//             line = line.substr(0,wkt_end_pos + 2);
//         }
//         else{
//             //否则跳过当前行
//             std::cout<<"continue................."<<std::endl;
//             continue;
//         }
        
//         // --- FIX 1: 清洗从CSV读取的字符串 ---
//         // 移除可能的回车符 (Windows换行符)
//         line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
//         // 移除前后的空格 制表符 换行符 回车符
//         line.erase(0, line.find_first_not_of(" \t\n\r"));
//         line.erase(line.find_last_not_of(" \t\n\r") + 1);
//         // 如果字符串被双引号包裹，则移除双引号
//         if (!line.empty() && line.front() == '"' && line.back() == '"') {
//             line = line.substr(1, line.length() - 2);
//         }
//         // --- END OF FIX 1 ---

//         if (line.empty())
//         {
//             std::cout<<"line is empty........."<<std::endl;
//             continue; // 如果是空行，则跳过
//         }

//         geos::geom::Polygon* polygon = createPolygonFromWKT(line);
//         if (polygon) {
//             const geos::geom::Envelope* env = polygon->getEnvelopeInternal();
//             if (env) {
//                 double dist_start, dist_end;
//                 curve_shape_projection(env, curve_type, xmin, ymin, cell_xintvl, cell_yinval, dist_start, dist_end);
//                 zAddresses.push_back(dist_start);
//                 zAddresses.push_back(dist_end);
//             }
//             delete polygon;
//         }
//     }
//     inputFile.close();

//     // --- FIX 2: 将输出文件写入当前目录 ---
//     std::ofstream outputFile("./areawater_binary", std::ios::binary);
//     if (!outputFile.is_open()) {
//         std::cerr << "打开输出文件 ./areawater_binary 失败" << std::endl;
//         return 1;
//     }
//     // --- END OF FIX 2 ---

//     for (double zAddress : zAddresses) {
//         outputFile.write(reinterpret_cast<const char*>(&zAddress), sizeof(double));
//     }
//     outputFile.close();

//     std::cout << "处理完成，" << zAddresses.size() << " 个地址已写入二进制文件。" << std::endl;

//     return 0;
// }

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <memory>
#include <limits>
#include <string>
#include <algorithm>

#include <geos/geom/GeometryFactory.h>
#include <geos/geom/Polygon.h>
#include <geos/geom/Envelope.h>
#include <geos/io/WKTReader.h>

// 确保这些路径相对于你的编译环境是正确的
#include "glin/glin.h" // GLIN索引的头文件

// 处理WKT多边形字符串，并转换为GEOS多边形对象
geos::geom::Polygon* createPolygonFromWKT(const std::string& wkt)
{
    geos::io::WKTReader reader;
    try {
        std::unique_ptr<geos::geom::Geometry> geom = reader.read(wkt);
        if (geom && geom->getGeometryTypeId() == geos::geom::GEOS_POLYGON) {
            return dynamic_cast<geos::geom::Polygon*>(geom.release());
        }
    } catch (const std::exception& e) { // 捕获更具体的异常
        std::cerr << "解析WKT字符串时发生异常: " << e.what() << " WKT: " << wkt << std::endl;
    } catch (...) {
        std::cerr << "解析WKT字符串时发生未知错误: " << wkt << std::endl;
    }
    return nullptr;
}

void curve_shape_projection(const geos::geom::Envelope* envelope, const std::string& curve_type, double cell_xmin, double cell_ymin, double cell_x_intvl, double cell_y_intvl, double& dist_start, double& dist_end)
{
    dist_start = std::numeric_limits<double>::max();
    dist_end = std::numeric_limits<double>::min();

    if (!envelope) return;

    double minX = envelope->getMinX();
    double minY = envelope->getMinY();
    double maxX = envelope->getMaxX();
    double maxY = envelope->getMaxY();

    auto encoder = new Encoder<double>(cell_xmin, cell_x_intvl, cell_ymin, cell_y_intvl);

    if (curve_type == "h") {
        auto index_double_h = encoder->encode_h(minX, minY, maxX, maxY);
        dist_start = index_double_h.first;
        dist_end = index_double_h.second;
    }
    if (curve_type == "z") {
        auto index_z = encoder->encode_z(minX, minY, maxX, maxY);
        dist_start = index_z.first;
        dist_end = index_z.second;
    }
    delete encoder;
}

int main()
{
    // 确保 testCSV.csv 文件在你的执行目录下
    std::ifstream inputFile("/mnt/hgfs/ShareFiles/AREAWATER.csv");
    if (!inputFile.is_open()) {
        std::cerr << "打开输入文件 /mnt/hgfs/ShareFiles/AREAWATER.csv 失败" << std::endl;
        return 1;
    }

    std::vector<double> zAddresses;
    auto factory = geos::geom::GeometryFactory::create();

    // GLIN索引参数配置
    double xmin = -180.0, ymin = -90.0;
    double cell_xintvl = 0.0001, cell_yinval = 0.0001;
    std::string curve_type = "z";

    std::string line;
    while (getline(inputFile, line)) {
        if(line.find("MULTIPOLYGON") != std::string::npos)
        {
            std::cout<<"line:"<<line<<std::endl;
        }
        // --- FIX: 移除 UTF-8 BOM ---
        if (line.length() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') {
            line = line.substr(3);
        }
        
        // --- NEW FIX: 从包含额外数据的行中分离出WKT字符串 ---
        // 查找POLYGON定义的结尾 '))' 并在此处截断字符串
        size_t wkt_end_pos = line.find("))");
        if (wkt_end_pos == std::string::npos)
        {
            continue;
        }

        // 判断“））”后面是否跟着‘”'
        if (wkt_end_pos + 2 < line.length() && line[wkt_end_pos + 2] == '"') {
            //如果是，需要把’”‘也一同截取
            line = line.substr(0, wkt_end_pos + 3);
        } else {
            line = line.substr(0, wkt_end_pos + 2);
        }

        // --- 清洗截取后的WKT字符串 ---
        // 移除可能的回车符 (Windows换行符)
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        // 移除前后的空格
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);
        // 如果字符串被双引号包裹，则移除双引号
        if (!line.empty() && line.front() == '"' && line.back() == '"') {
            line = line.substr(1, line.length() - 2);
        }

        if (line.empty()) continue; // 如果是空行，则跳过

        geos::geom::Polygon* polygon = createPolygonFromWKT(line);
        if (polygon) {
            const geos::geom::Envelope* env = polygon->getEnvelopeInternal();
            if (env) {
                double dist_start, dist_end;
                curve_shape_projection(env, curve_type, xmin, ymin, cell_xintvl, cell_yinval, dist_start, dist_end);
                zAddresses.push_back(dist_start);
                zAddresses.push_back(dist_end);
            }
            delete polygon;
        }
    }
    inputFile.close();

    // --- 将输出文件写入当前目录 ---
    std::ofstream outputFile("./areawater_binary", std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "打开输出文件 ./areawater_binary 失败" << std::endl;
        return 1;
    }

    for (double zAddress : zAddresses) {
        outputFile.write(reinterpret_cast<const char*>(&zAddress), sizeof(double));
    }
    outputFile.close();

    std::cout << "处理完成，" << zAddresses.size() << " 个地址已写入二进制文件。" << std::endl;

    return 0;
}

