// // #include "glin.h"
// // #include "../glin/glin.h"
// #include "./glin/glin.h"
// #include <geos/geom/Geometry.h>
// #include <vector>
// #include "./src/core/alex.h"


// int main()
// {
//     //创建一个Glin索引
//     alex::Glin<double,geos::geom::Geometry*>index;
//     //poly_vec是一个包含几何对象的向量
//     std::vector<geos::geom::Geometry*>poly_vec;
//     //std::vector<std::unique_ptr<geos::geom::Geometry >> poly_vec;

//     //创建一个简单的矩形多边形

//     //创建几何工厂
//     // std::unique_ptr<geos::geom::PrecisionModel> pm(new geos::geom::PrecisionModel());
//     // geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(),-1);
//     // std::vector<geos::geom::Geometry*>poly_vec;
//      // 创建几何工厂
//     //std::unique_ptr<geos::geom::PrecisionModel> pm(new geos::geom::PrecisionModel());
//     auto pm = std::make_unique<geos::geom::PrecisionModel>();
//     geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(), -1);
    
//     geos::io::WKTReader reader(*global_factory);
//     std::vector<std::string> wkt_polygons;
//     // wkt_polygons = {
//     //     // "POLYGON ((40 0, 40 10, 50 10, 50 0, 40 0))",
//     //     // "POLYGON ((60 0, 60 10, 70 10, 70 0, 60 0))"
//     //     "POLYGON ((35 30, 40 30, 40 25, 35 25, 35 30))",//矩形1
//     //     "POLYGON ((45 35, 50 35, 50 30, 45 30, 45 35))",//矩形2
//     //     "POLYGON ((45 25, 50 25, 50 20, 45 20, 45 25))",//矩形3
//     //     "POLYGON ((55 30, 60 30, 60 25, 55 25, 55 30))"//矩形4

//     // };
//     //大量数据还是需要从CSV文件中直接读取WKT字符串
//     std::ifstream inputFile("/mnt/hgfs/ShareFiles/AREAWATER.csv");
//     if(!inputFile.is_open())
//     {
//         std::cerr<<"打开AREAWATER.csv失败！"<<std::endl;
//     }
//     std::string line,wkt_string;
//     //while循环不断逐行读取，直到结束
//     while(getline(inputFile,line))
//     {
//         //首先要移除可能存在的 UTF-8 BOM符号
//         if(line.length() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF')
//         {
//             line = line.substr(3);//从第四个开始截取直到最后一个
//         }
        
//         //先初步去除 空格，制表符，换行符，回车符
//         line.erase(std::remove(line.begin(),line.end(),'\r'),line.end()); //删除字符串里所有的回车符
//         line.erase(0,line.find_first_not_of(" \t\n\r"));               //删除开头的空格/制表/换行/回车
//         line.erase(line.find_last_not_of(" \t\n\r") + 1);              //删除末尾的...
        
//         if(line.empty()) continue;
        
//         //根据引号来分离出WKT字符串
//         if(line.front() == '"')//如果WKT字符串被双引号包裹
//         {
//             size_t end_quote_pos = line.find('"',1);//从下标1开始找第二个引号
//             if(end_quote_pos != std::string::npos)
//             {
//                 //截取两个引号之间的内容
//                 wkt_string = line.substr(1,end_quote_pos - 1);
//             }
//             else continue;
//         }
//         else{
//             //第二种情况：没有被引号包裹，则从0开始一直截取到最后一个‘）’的位置
//             size_t last_paren_pos = line.rfind(')');
//             if(last_paren_pos != std::string::npos)
//             {
//                 wkt_string = line.substr(0,last_paren_pos + 1);
//                 // 清理可能存在的尾部空白
//                 wkt_string.erase(wkt_string.find_last_not_of(" \t\n\r") + 1);
//             }
//             else{
//                 continue;
//             }
//         }  
//         if(wkt_string.empty())
//         {
//             continue;
//         }
//         else{
//              wkt_polygons.push_back(wkt_string);
//         }

        
//     }

//     for (const auto& wkt : wkt_polygons) {
//         try {
//             poly_vec.push_back(reader.read(wkt).release());//read 返回的智能指针std::unique_ptr<geos::geom::Geometry>,poly_vec只能接受原始指针
//         } catch (const geos::util::GEOSException& e) {
//             std::cerr << "解析WKT失败: " << e.what() << std::endl;
//         }
//     }
//     std::cout<<"poly_vec.size():"<<poly_vec.size()<<std::endl;
    

//     // //添加多个多边形
//     // {
//     //     geos::io::WKTReader reader(*global_factory);
//     //     std::vector<std::string>wkt_polygons = {
//     //         "POLYGON ((40 0, 40 10, 50 10, 50, 0, 40 0))",
//     //         "POLYGON ((60 0, 60 10, 70 10, 70 0, 60 0))"
//     //     };
//     //     for(const auto& wkt:wkt_polygons){
//     //         try{
//     //             poly_vec.push_back(reader.read(wkt));
//     //         }catch (const geos::util::GEOSException& e){
//     //             std::cerr<<"解析WKT失败："<<e.what()<<std::endl;
//     //         }
//     //     }
//     // }
  

//     double piecelimitation = 100.0; 
//     std::string curve_type = "z";//Z曲线填充
//     double cell_xmin = -100;
//     double cell_ymin = -90;
//     double cell_x_intvl = 1.0;
//     double cell_y_intvl = 1.0;
//     std::vector<std::tuple<double,double,double,double>>pieces;
//     //加载数据到索引
//     std::cout<<"开始加载到索引........"<<std::endl;
//     index.glin_bulk_load(poly_vec,piecelimitation,curve_type,cell_xmin,cell_ymin,cell_x_intvl,cell_y_intvl,pieces);

//     // 创建查询窗口（一个矩形）
//     geos::geom::Geometry *query_window;
//     std::vector<geos::geom::Geometry*>find_result;
//     int count_filter = 0;
//     //std::string query_wkt = "POLYGON ((48 31, 51 31, 51 28, 48 28, 48 31))";//相交矩形2 
//     // std::string query_wkt = "POLYGON ((44 36, 52 36, 52 28, 44 28, 44 36))"; //包含矩形2
//     std::string query_wkt = "POLYGON ((47 33, 48 33, 48 32, 47 32, 47 33))"; //被矩形2包含
    
//     try {
//         query_window = reader.read(query_wkt).release();
//     } catch (const geos::util::GEOSException& e) {
//         std::cerr << "创建查询窗口失败: " << e.what() << std::endl;
//         return 1;
//     }
//     std::cout<<"开始查询........"<<std::endl;
//     //用glin_find进行查询
//     index.glin_find(query_window,"z",cell_xmin,cell_ymin,cell_x_intvl,cell_y_intvl,pieces,find_result,count_filter);

//     //输出查询结果
//     std::cout << "查询结果数量: " << find_result.size() << std::endl;

//     // 释放查询窗口内存
//     delete query_window;
//     return 0;


// }

//-------------------------------------------------------------------------------------------------------
#include "./glin/glin.h"
#include <geos/geom/Geometry.h>
#include <vector>
#include "./src/core/alex.h"


int main()
{
    //创建一个Glin索引
    alex::Glin<double,geos::geom::Geometry*>index;
    //poly_vec是一个包含几何对象的向量
    std::vector<geos::geom::Geometry*>poly_vec;
    //std::vector<std::unique_ptr<geos::geom::Geometry >> poly_vec;

    //创建一个简单的矩形多边形

    //创建几何工厂
    // std::unique_ptr<geos::geom::PrecisionModel> pm(new geos::geom::PrecisionModel());
    // geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(),-1);
    // std::vector<geos::geom::Geometry*>poly_vec;
     // 创建几何工厂
    //std::unique_ptr<geos::geom::PrecisionModel> pm(new geos::geom::PrecisionModel());
    auto pm = std::make_unique<geos::geom::PrecisionModel>();
    geos::geom::GeometryFactory::Ptr global_factory = geos::geom::GeometryFactory::create(pm.get(), -1);
    
    geos::io::WKTReader reader(*global_factory);
    std::vector<std::string> wkt_polygons;
    // wkt_polygons = {
    //     // "POLYGON ((40 0, 40 10, 50 10, 50 0, 40 0))",
    //     // "POLYGON ((60 0, 60 10, 70 10, 70 0, 60 0))"
    //     "POLYGON ((35 30, 40 30, 40 25, 35 25, 35 30))",//矩形1
    //     "POLYGON ((45 35, 50 35, 50 30, 45 30, 45 35))",//矩形2
    //     "POLYGON ((45 25, 50 25, 50 20, 45 20, 45 25))",//矩形3
    //     "POLYGON ((55 30, 60 30, 60 25, 55 25, 55 30))"//矩形4

    // };
    //大量数据还是需要从CSV文件中直接读取WKT字符串
    std::ifstream inputFile("/mnt/hgfs/sharedFolder/AREAWATER.csv");
    if(!inputFile.is_open())
    {
        std::cerr<<"打开AREAWATER.csv失败！"<<std::endl;
    }
    std::string line,wkt_string;
    int line_count = 0;
    //while循环不断逐行读取，直到结束
    std::cout<<"开始读取数据集..."<<std::endl;
    while(getline(inputFile,line))
    {
        line_count ++;
        if(line_count == 15000) break;
        //Alex库一次最多只能读取15000条数据，再多就会报错
        if(line_count % 20000 == 0)
        {
            std::cout<<"已处理"<<line_count<<"行"<<std::endl;
        }
        //首先要移除可能存在的 UTF-8 BOM符号
        if(line.length() >= 3 && line[0] == '\xEF' && line[1] == '\xBB' && line[2] == '\xBF')
        {
            line = line.substr(3);//从第四个开始截取直到最后一个
        }
        
        //先初步去除 空格，制表符，换行符，回车符
        line.erase(std::remove(line.begin(),line.end(),'\r'),line.end()); //删除字符串里所有的回车符
        line.erase(0,line.find_first_not_of(" \t\n\r"));               //删除开头的空格/制表/换行/回车
        line.erase(line.find_last_not_of(" \t\n\r") + 1);              //删除末尾的...
        
        if(line.empty()) continue;
        
        //根据引号来分离出WKT字符串
        if(line.front() == '"')//如果WKT字符串被双引号包裹
        {
            size_t end_quote_pos = line.find('"',1);//从下标1开始找第二个引号
            if(end_quote_pos != std::string::npos)
            {
                //截取两个引号之间的内容
                wkt_string = line.substr(1,end_quote_pos - 1);
            }
            else continue;
        }
        else{
            //第二种情况：没有被引号包裹，则从0开始一直截取到最后一个‘）’的位置
            size_t last_paren_pos = line.rfind(')');
            if(last_paren_pos != std::string::npos)
            {
                wkt_string = line.substr(0,last_paren_pos + 1);
                // 清理可能存在的尾部空白
                wkt_string.erase(wkt_string.find_last_not_of(" \t\n\r") + 1);
            }
            else{
                continue;
            }
        }  
        if(wkt_string.empty())
        {
            continue;
        }
        else{
             wkt_polygons.push_back(wkt_string);
        }

        
    }

    for (const auto& wkt : wkt_polygons) {
        try {
            poly_vec.push_back(reader.read(wkt).release());//read 返回的智能指针std::unique_ptr<geos::geom::Geometry>,poly_vec只能接受原始指针
        } catch (const geos::util::GEOSException& e) {
            std::cerr << "解析WKT失败: " << e.what() << std::endl;
        }
    }
    std::cout<<"poly_vec.size():"<<poly_vec.size()<<std::endl;
    

    // //添加多个多边形
    // {
    //     geos::io::WKTReader reader(*global_factory);
    //     std::vector<std::string>wkt_polygons = {
    //         "POLYGON ((40 0, 40 10, 50 10, 50, 0, 40 0))",
    //         "POLYGON ((60 0, 60 10, 70 10, 70 0, 60 0))"
    //     };
    //     for(const auto& wkt:wkt_polygons){
    //         try{
    //             poly_vec.push_back(reader.read(wkt));
    //         }catch (const geos::util::GEOSException& e){
    //             std::cerr<<"解析WKT失败："<<e.what()<<std::endl;
    //         }
    //     }
    // }
  

    double piecelimitation = 100.0; 
    std::string curve_type = "z";//Z曲线填充
    double cell_xmin = -100;
    double cell_ymin = -90;
    double cell_x_intvl = 1.0;
    double cell_y_intvl = 1.0;
    std::vector<std::tuple<double,double,double,double>>pieces;
    //加载数据到索引
    std::cout<<"开始加载到索引........"<<std::endl;
    index.glin_bulk_load(poly_vec,piecelimitation,curve_type,cell_xmin,cell_ymin,cell_x_intvl,cell_y_intvl,pieces);

    // 创建查询窗口（一个矩形）
    geos::geom::Geometry *query_window;
    std::vector<geos::geom::Geometry*>find_result;
    int count_filter = 0;
    //std::string query_wkt = "POLYGON ((48 31, 51 31, 51 28, 48 28, 48 31))";//相交矩形2 
    // std::string query_wkt = "POLYGON ((44 36, 52 36, 52 28, 44 28, 44 36))"; //包含矩形2
    // std::string query_wkt = "POLYGON ((47 33, 48 33, 48 32, 47 32, 47 33))"; //被矩形2包含
    std::string query_wkt = "POLYGON ((-86.91504 32.642045,-86.914891 32.641992,-86.914807 32.641953,-86.914762 32.641843,-86.914911 32.641866,-86.914952 32.641887,-86.915086 32.641956,-86.91504 32.642045))"; //被矩形2包含
    try {
        query_window = reader.read(query_wkt).release();
    } catch (const geos::util::GEOSException& e) {
        std::cerr << "创建查询窗口失败: " << e.what() << std::endl;
        return 1;
    }
    std::cout<<"开始查询........"<<std::endl;
    //用glin_find进行查询
    index.glin_find(query_window,"z",cell_xmin,cell_ymin,cell_x_intvl,cell_y_intvl,pieces,find_result,count_filter);

    //输出查询结果
    std::cout << "查询结果数量: " << find_result.size() << std::endl;

    // 释放查询窗口内存
    delete query_window;
    return 0;


}

//---------------------------------------------------------------------
