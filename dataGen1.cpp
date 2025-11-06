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
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/Envelope.h>
#include <geos/io/WKTReader.h>
#include <iomanip>  // 新增：用于 std::fixed 和 std::setprecision（控制double输出格式）
// 确保这些路径相对于你的编译环境是正确的
#include "../../glin/glin.h" // GLIN索引的头文件

// 新增：CSV字段转义函数（处理引号和特殊字符）
std::string escape_csv_field(const std::string& field) 
{
    std::string escaped = field;
    // CSV规则：字段中的单个引号替换为两个引号
    std::replace(escaped.begin(), escaped.end(), '"', '"');
    // 用引号包裹整个字段，避免逗号被误识别为分隔符
    return "\"" + escaped + "\"";
}
// 处理WKT多边形字符串，并转换为GEOS几何对象 (现在支持POLYGON和MULTIPOLYGON)
geos::geom::Geometry* createGeometryFromWKT(const std::string& wkt)
{
    geos::io::WKTReader reader;
    try {
        std::unique_ptr<geos::geom::Geometry> geom = reader.read(wkt);
        if (geom) {
            auto type = geom->getGeometryTypeId();
            // 同时检查 POLYGON 和 MULTIPOLYGON 类型
            if (type == geos::geom::GEOS_POLYGON || type == geos::geom::GEOS_MULTIPOLYGON) {
                return geom.release();
            }
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
        // 移除可能存在的 UTF-8 BOM  符号
        if (line.length() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF') {
            line = line.substr(3);
        }
        
        // 通过引号隔离WKT字符串

        // 1. 先进行初步的清理
        line.erase(std::remove(line.begin(), line.end(), '\r'), line.end());
        line.erase(0, line.find_first_not_of(" \t\n\r"));
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.empty()) continue;

        std::string wkt_string;

        // 2. 根据是否存在引号，用不同的逻辑来分离出WKT字符串
        if (line.front() == '"') {
            // 情况一：WKT字符串被引号包裹
            size_t end_quote_pos = line.find('"', 1);
            if (end_quote_pos != std::string::npos) {
                // 提取两个引号之间的内容
                wkt_string = line.substr(1, end_quote_pos - 1);
            } else {
                continue; // 格式错误，跳过
            }
        } else {
            // 情况二：WKT字符串没有被引号包裹  则提取直到最后一个括号的内容
            size_t last_paren_pos = line.rfind(')');
            if (last_paren_pos != std::string::npos) {
                wkt_string = line.substr(0, last_paren_pos + 1);
                // 清理可能存在的尾部空格
                wkt_string.erase(wkt_string.find_last_not_of(" \t\n\r") + 1);//只接收一个参数，表示删除从该参数指定的当前位置开始，到字符串末尾的所有字符
            } else {
                continue; // 无效行，跳过
            }
        }

        if (wkt_string.empty()) continue;

        // 使用新的函数，它可以处理 POLYGON 和 MULTIPOLYGON
        geos::geom::Geometry* geometry = createGeometryFromWKT(wkt_string);
        if (geometry) {
            // getEnvelopeInternal() 在基类 Geometry 中可用
            const geos::geom::Envelope* env = geometry->getEnvelopeInternal();
            if (env) {
                double dist_start, dist_end;
                curve_shape_projection(env, curve_type, xmin, ymin, cell_xintvl, cell_yinval, dist_start, dist_end);
                zAddresses.push_back(dist_start);
                // zAddresses.push_back(dist_end);只要Zmin即可
            }
            delete geometry; // 释放内存
        }
    }
    inputFile.close();

    // 将输出文件写入当前目录
    std::ofstream outputFile("./areawater_binary", std::ios::binary);
    if (!outputFile.is_open()) {
        std::cerr << "打开输出文件 ./areawater_binary 失败" << std::endl;
        return 1;
    }

    std::ofstream csv_Z_OutputFile("./areawater_z.csv");
    if (!csv_Z_OutputFile.is_open()) {
        std::cerr << "打开CSV输出文件 ./areawater_z.csv 失败" << std::endl;
        return 1;
    }
    // 写入CSV表头（可选但推荐，方便后续读取识别字段）
    csv_Z_OutputFile << "Key_min" << std::endl;
    //  立即检查表头是否写入成功（关键！定位是否此处失败）
    if (!csv_Z_OutputFile.good()) {
        std::cerr << "错误：表头\"Key_min\"写入失败！可能是磁盘权限或空间问题。" << std::endl;
        csv_Z_OutputFile.close();
        return 1;
    }
    // 控制double输出格式：固定小数位（避免科学计数法，保留6位小数，兼顾精度）
    csv_Z_OutputFile << std::fixed << std::setprecision(15);
    int k = 0;
    for (double zAddress : zAddresses) {
        if(k % 100000 == 0) std::cout<<"zAddress值已写入"<<k<<"个"<<"zAddress:"<<zAddress<<std::endl;
        //outputFile.write(reinterpret_cast<const char*>(&zAddress), sizeof(double));
        // 写入CSV文件（文本格式，每个zAddress占一行）
        csv_Z_OutputFile << zAddress << std::endl;
        k++;
    }

    outputFile.close();
    csv_Z_OutputFile.close();
    // std::cout << "处理完成，" << zAddresses.size() << " 个地址已写入二进制文件。" << std::endl;
    // std::cout << "有效WKT字符串已写入 CSV 文件：./processed_wkt.csv" << std::endl; // 新增：提示CSV路径
    std::cout << zAddresses.size() << " 个zAddress值已写入CSV文件 ./areawater_z.csv。" << std::endl;

    return 0;
}

