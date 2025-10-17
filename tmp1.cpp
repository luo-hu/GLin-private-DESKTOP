

//----------------------------------------------------------

//----------------------------------------------------------

//------------------------------------

/*采用分批次构建GLIN索引一直出错（段错误 (核心已转储），问题的根源在于 glin/alex 这个库本身存在严重的设计缺陷。它在处理完一批数据后，内部状态没有被完全清理，导致任何后续操作（即使在新的进程中）都会因为状态污染而崩溃。

继续在这个不稳定的库上修补是徒劳的。为了彻底解决问题，我们必须更换核心的索引库。可以使用工业级标准库 Boost.Geometry.Rtree 的解决方案。这个库是专门为处理大规模空间数据而设计的，非常成熟和稳定，能够轻松处理数百万个几何对象
使用Boost库和使用ALEX库会对结果有影响吗？

    最终结果：不会。无论是使用ALEX还是Boost R-tree，它们都扮演着“过滤器”的角色，用于快速筛选出可能相交的候选对象。最后的精确判断都依赖于GEOS库的 intersects 函数。因此，只要索引库本身没有bug，最终得到的精确匹配结果应该是完全相同的。

    性能和稳定性：有巨大影响。Boost.Rtree是工业级的、经过千锤百炼的专业空间索引库，它在性能、内存管理和稳定性上都远远优于那个有问题的ALEX/GLIN库。我们切换到Boost是完全正确的选择*/
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <algorithm> // for std::sort
#include <cstdint>   // for uint64_t

// --- Boost Geometry Setup ---
// 1. 使用最小化的、特定的头文件，以避免 <boost/geometry.hpp> 可能带来的冲突
#include <boost/geometry/geometries/point.hpp>
#include <boost/geometry/geometries/box.hpp>
#include <boost/geometry/index/rtree.hpp>
#include <boost/geometry/geometries/register/point.hpp>

// --- GEOS Setup ---
#include <geos/geom/Geometry.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/io/WKTReader.h>
#include <geos/util/GEOSException.h>
#include <geos/geom/Envelope.h>


// 2. 定义一个自定义点结构体，以绕开旧版Boost中的bug
struct custom_point_t
{
    double x, y;
};

// 3. 使用Boost宏将我们的自定义结构体注册为2D点
BOOST_GEOMETRY_REGISTER_POINT_2D(custom_point_t, double, boost::geometry::cs::cartesian, x, y)


// 4. 定义将在R树中使用的类型，全部基于我们的自定义点类型
namespace bg = boost::geometry;
namespace bgi = boost::geometry::index;

typedef bg::model::box<custom_point_t> box_t;
typedef std::pair<box_t, size_t> value_t;


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

// 【新增】函数: 计算二维坐标的Z秩序曲线值 (Morton Code)
// 通过交错x和y坐标的二进制位来生成一个一维的值
uint64_t calculate_z_order(double x, double y, double min_x, double min_y, double range_x, double range_y) {
    // 将浮点坐标归一化并缩放到32位整数范围内
    uint32_t ix = static_cast<uint32_t>(((x - min_x) / range_x) * UINT32_MAX);
    uint32_t iy = static_cast<uint32_t>(((y - min_y) / range_y) * UINT32_MAX);

    // 交错位的辅助函数
    auto interleave = [](uint32_t n) {
        uint64_t val = n;
        val = (val | (val << 16)) & 0x0000FFFF0000FFFF;
        val = (val | (val << 8))  & 0x00FF00FF00FF00FF;
        val = (val | (val << 4))  & 0x0F0F0F0F0F0F0F0F;
        val = (val | (val << 2))  & 0x3333333333333333;
        val = (val | (val << 1))  & 0x5555555555555555;
        return val;
    };

    return interleave(ix) | (interleave(iy) << 1);
}


int main() {
    // --- 初始化 GEOS 组件 ---
    auto pm = std::make_unique<geos::geom::PrecisionModel>();
    geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(), -1);
    geos::io::WKTReader reader(*global_factory);

    // --- 阶段一: 从CSV文件读取数据并准备用于R树批量加载 ---
    std::cout << "阶段一: 开始预处理CSV文件，提取MBR..." << std::endl;
    std::ifstream inputFile("/mnt/hgfs/ShareFiles/AREAWATER.csv");
    if (!inputFile.is_open()) {
        std::cerr << "打开 AREAWATER.csv 失败！" << std::endl;
        return 1;
    }
    
    std::vector<value_t> rtree_values;
    std::vector<std::string> wkt_storage;
    // 【新增】用于存储CDF数据的Z秩序曲线值
    std::vector<uint64_t> z_values;
    
    rtree_values.reserve(2300000);
    wkt_storage.reserve(2300000);
    z_values.reserve(2300000);
    
    // 【新增】为Z秩序曲线计算设定全局范围，可以根据您的数据进行调整
    const double GLOBAL_MIN_X = -180.0, GLOBAL_MIN_Y = -90.0;
    const double GLOBAL_MAX_X = 180.0, GLOBAL_MAX_Y = 90.0;
    const double GLOBAL_RANGE_X = GLOBAL_MAX_X - GLOBAL_MIN_X;
    const double GLOBAL_RANGE_Y = GLOBAL_MAX_Y - GLOBAL_MIN_Y;

    std::string line;
    long line_count = 0;
    size_t current_id = 0;
    while (getline(inputFile, line)) {
        if(line_count++ % 100000 == 0) {
            std::cout << "已处理 " << line_count << " 行..." << std::endl;
        }

        std::string wkt_string = clean_wkt_string(line);
        if (wkt_string.empty()) continue;

        try {
            std::unique_ptr<geos::geom::Geometry> full_geom = reader.read(wkt_string);
            const geos::geom::Envelope* env = full_geom->getEnvelopeInternal();
            if (!env || env->isNull()) continue;

            box_t b;
            bg::assign_values(b, env->getMinX(), env->getMinY(), env->getMaxX(), env->getMaxY());
            
            rtree_values.push_back(std::make_pair(b, current_id));
            wkt_storage.push_back(wkt_string);
            
            // 【新增】为每个MBR的左下角计算并存储Z值
            z_values.push_back(calculate_z_order(env->getMinX(), env->getMinY(), GLOBAL_MIN_X, GLOBAL_MIN_Y, GLOBAL_RANGE_X, GLOBAL_RANGE_Y));
            
            current_id++;

        } catch (const geos::util::GEOSException& e) {
            // 忽略解析失败的行
        }
    }
    inputFile.close();
    std::cout << "MBR提取完成，共处理 " << rtree_values.size() << " 个有效几何对象。" << std::endl;

    if (rtree_values.empty()) {
        std::cerr << "没有加载到任何有效的几何对象，程序退出。" << std::endl;
        return 1;
    }

    // --- 【新增】阶段 1.5: 生成并写入CDF数据到CSV文件 ---
    std::cout << "\n阶段 1.5: 开始生成CDF数据并写入zmin_cdf.csv..." << std::endl;
    std::sort(z_values.begin(), z_values.end()); // 排序是计算CDF的前提
    std::ofstream cdf_file("zmin_cdf.csv");
    if (cdf_file.is_open()) {
        cdf_file << "z_value,cumulative_proportion\n"; // 写入表头
        for (size_t i = 0; i < z_values.size(); ++i) {
            double cdf = static_cast<double>(i + 1) / z_values.size();
            cdf_file << z_values[i] << "," << cdf << "\n";
        }
        cdf_file.close();
        std::cout << "CDF数据成功写入 zmin_cdf.csv" << std::endl;
    } else {
        std::cerr << "警告：无法创建 zmin_cdf.csv 文件！" << std::endl;
    }


    // --- 阶段二: 使用批量加载功能高效构建R树索引 ---
    std::cout << "\n阶段二: 开始构建Boost R-tree索引..." << std::endl;
    bgi::rtree<value_t, bgi::rstar<16>> rtree(rtree_values);
    std::cout << "R-tree索引构建完成。" << std::endl;
    
    rtree_values.clear();
    rtree_values.shrink_to_fit();
    z_values.clear(); // 同时清理Z值向量内存
    z_values.shrink_to_fit();

    // --- 阶段三: 执行查询 ---
    std::cout << "\n阶段三: 开始执行查询..." << std::endl;
    // std::string query_wkt = "POLYGON ((-89.968303 38.565946,-89.968206 38.565952,-89.96825 38.565886,-89.968303 38.565946))";
    std::string query_wkt = "MULTIPOLYGON (((-89.968303 38.565946,-89.968206 38.565952,-89.96825 38.565886,-89.968303 38.565946)),((-89.96968 38.565668,-89.969659 38.565685,-89.96961 38.565701,-89.969135 38.565586,-89.969048 38.565581,-89.968771 38.565564,-89.968567 38.565404,-89.968533 38.565378,-89.968407 38.565323,-89.96823 38.565275,-89.968121 38.565246,-89.968082 38.565219,-89.968067 38.565024,-89.968049 38.565017,-89.968051 38.565198,-89.96796 38.565136,-89.967783 38.565048,-89.967652 38.564983,-89.967379 38.564884,-89.967338 38.564851,-89.966974 38.564752,-89.966331 38.564676,-89.966247 38.564599,-89.966226 38.564538,-89.966261 38.564357,-89.966303 38.56428,-89.966386 38.564269,-89.966645 38.564368,-89.967051 38.564439,-89.96735 38.564515,-89.967463 38.564544,-89.96762 38.564617,-89.967604 38.56452,-89.967967 38.5647,-89.968046 38.564739,-89.968055 38.564877,-89.968184 38.564961,-89.968291 38.564984,-89.968414 38.56501,-89.968673 38.565037,-89.969134 38.565054,-89.96919 38.565081,-89.969148 38.565136,-89.968883 38.565158,-89.968771 38.565246,-89.968785 38.565268,-89.968897 38.565312,-89.969247 38.565355,-89.9694 38.56546,-89.969554 38.565526,-89.969647 38.565607,-89.969666 38.565624,-89.96968 38.565668)))";

    std::unique_ptr<geos::geom::Geometry> query_geom;
    try {
        query_geom = reader.read(query_wkt);
    } catch (const geos::util::GEOSException& e) {
        std::cerr << "创建查询窗口失败: " << e.what() << std::endl;
        return 1;
    }
    
    const geos::geom::Envelope* query_env = query_geom->getEnvelopeInternal();
    
    box_t query_box;
    bg::assign_values(query_box, query_env->getMinX(), query_env->getMinY(), query_env->getMaxX(), query_env->getMaxY());

    std::vector<value_t> candidate_results;
    rtree.query(bgi::intersects(query_box), std::back_inserter(candidate_results));

    std::cout << "索引过滤找到 " << candidate_results.size() << " 个候选对象，开始精确计算..." << std::endl;

    std::vector<std::string> final_results_wkt;
    for (const auto& v : candidate_results) {
        size_t id = v.second;
        const std::string& original_wkt = wkt_storage[id];
        try {
            std::unique_ptr<geos::geom::Geometry> original_geom = reader.read(original_wkt);
            if (query_geom->intersects(original_geom.get())) {
                final_results_wkt.push_back(original_wkt);
            }
        } catch (const geos::util::GEOSException& e) {
            // 忽略解析错误
        }
    }

    // --- 阶段四: 输出最终报告 ---
    std::cout << "\n查询完成。" << std::endl;
    std::cout << "精确计算的候选对象数量: " << candidate_results.size() << std::endl;
    std::cout << "最终精确匹配的结果数量: " << final_results_wkt.size() << std::endl;

    return 0;
}

