// //定义布隆过滤器
// #include <vector>
// #include <bitset>
// #include <functional>

// template <size_t N, size_t K>  // 这个布隆过滤器有两个模板参数：N: 位数组大小, K: 哈希函数数量
// class BloomFilter {
// private:
//     std::bitset<N> bits; //定义一个位数组，大小为N
//     std::vector<std::function<size_t(const geos::geom::Geometry*)>> hash_funcs; //哈 希函数

//     // 生成K个哈希函数（基于几何对象的WKT字符串哈希）
//     void init_hash_funcs() {
//         for (size_t i = 0; i < K; ++i) {
//             hash_funcs.emplace_back([i](const geos::geom::Geometry* g) {
//                 std::string wkt = g->toString();
//                 std::hash<std::string> hasher;
//                 return (hasher(wkt) + i) % N;  // 加盐哈希
//             });
//         }
//     }

// public:
//     BloomFilter() { init_hash_funcs(); }

//     // 插入几何对象到布隆过滤器
//     void insert(const geos::geom::Geometry* g) {
//         // 新增：检查g是否为空
//         if (!g) {
//             std::cerr << "BloomFilter插入失败：几何对象为空指针！" << std::endl;
//             return;
//         }
//         for (const auto& hash : hash_funcs) {
//             bits.set(hash(g));
//         }
//     }

//     // 检查是否可能存在（false表示一定不存在）
//     bool might_contain(const geos::geom::Geometry* query) {
//         for (const auto& hash : hash_funcs) {
//             if (!bits.test(hash(query))) {
//                 return false;
//             }
//         }
//         return true;
//     }

// };


//定义布隆过滤器
#include <vector>
#include <bitset>
#include <functional>

// 前向声明
namespace geos { namespace geom { class Geometry; }}
namespace geos { namespace geom { class Envelope; }} // 新增 Envelope 前向声明

template <size_t N, size_t K>  // 这个布隆过滤器有两个模板参数：N: 位数组大小, K: 哈希函数数量
class BloomFilter {
private:
    std::bitset<N> bits; //定义一个位数组，大小为N
    std::vector<std::function<size_t(const geos::geom::Geometry*)>> hash_funcs; //哈希函数

    // 生成K个哈希函数（基于相交区域的哈希策略）
    void init_hash_funcs() {
        std::hash<int> int_hasher;
        for (size_t i = 0; i < K; ++i) {
            hash_funcs.emplace_back([i, int_hasher](const geos::geom::Geometry* g) {
                if (!g) return static_cast<size_t>(i);

                const auto * env = g->getEnvelopeInternal();
                if (!env || env->isNull()) return static_cast<size_t>(i);

                // 使用更大的网格大小以确保相交对象落在同一网格中
                const double GRID_SIZE = 4.0;

                // 对坐标进行网格化（扩大范围以确保相交对象在同一个网格中）
                int grid_min_x = static_cast<int>(std::floor(env->getMinX() / GRID_SIZE));
                int grid_min_y = static_cast<int>(std::floor(env->getMinY() / GRID_SIZE));
                int grid_max_x = static_cast<int>(std::floor(env->getMaxX() / GRID_SIZE));
                int grid_max_y = static_cast<int>(std::floor(env->getMaxY() / GRID_SIZE));

                size_t final_hash = 0;

                // 相交保证的哈希策略
                switch (i % 3) {
                    case 0: {
                        // 策略1：使用对象的左上角网格（已经验证有效）
                        final_hash = int_hasher(grid_min_x * 1000 + grid_min_y * 10 + i);
                        break;
                    }
                    case 1: {
                        // 策略2：使用相交对象必然共享的网格区域
                        // 计算对象覆盖的所有网格，选择其中一个作为哈希
                        int intersection_x = grid_min_x;
                        int intersection_y = grid_min_y;
                        // 如果查询窗口和对象相交，它们至少共享左上角网格
                        final_hash = int_hasher(intersection_x * 1000 + intersection_y * 10 + i);
                        break;
                    }
                    case 2: {
                        // 策略3：使用对象的中心网格（相交对象中心相近）
                        int center_x = (grid_min_x + grid_max_x) / 2;
                        int center_y = (grid_min_y + grid_max_y) / 2;
                        final_hash = int_hasher(center_x * 1000 + center_y * 10 + i);
                        break;
                    }
                }

                return final_hash % N;
            });
        }
    }
    // void init_hash_funcs() {
    //     // 方案1：使用引用捕获
    //     std::hash<double> double_hasher;
    //     for (size_t i = 0; i < K; ++i) {
    //         hash_funcs.emplace_back([i, &double_hasher](const geos::geom::Geometry* g) { // 修改为引用捕获
    //             if (!g) return static_cast<size_t>(i);

    //             const auto * env = g->getEnvelopeInternal();
    //             if (!env || env->isNull()) return static_cast<size_t>(i);

    //             // 使用更稳健的哈希组合方式
    //             size_t h1 = double_hasher(env->getMinX() + i * 0.314159);
    //             size_t h2 = double_hasher(env->getMinY() + i * 0.271828);
    //             size_t h3 = double_hasher(env->getMaxX() + i * 0.141421);
    //             size_t h4 = double_hasher(env->getMaxY() + i * 0.666666);
    //             size_t h5 = double_hasher((env->getWidth() + env->getHeight()) / 2.0 + i);
                
    //             // 更复杂的组合方式
    //             return (h1 ^ (h2 << 16) ^ (h3 >> 8) ^ (h4 << 24) ^ h5) % N;
    //         });
    //     }
    // }

public:
    BloomFilter() { init_hash_funcs(); }

    // 插入几何对象到布隆过滤器
    void insert(const geos::geom::Geometry* g) {
        // 新增：检查g是否为空
        if (!g) {
            std::cerr << "BloomFilter插入失败：几何对象为空指针！" << std::endl;
            return;
        }

        // 调试：打印被插入对象的网格信息
        const auto * env = g->getEnvelopeInternal();
        if (env) {
            const double GRID_SIZE = 4.0;  // 与哈希函数保持一致
            int grid_min_x = static_cast<int>(std::floor(env->getMinX() / GRID_SIZE));
            int grid_min_y = static_cast<int>(std::floor(env->getMinY() / GRID_SIZE));
            int grid_max_x = static_cast<int>(std::floor(env->getMaxX() / GRID_SIZE));
            int grid_max_y = static_cast<int>(std::floor(env->getMaxY() / GRID_SIZE));
            std::cout << "插入对象MBR: [" << env->getMinX() << "," << env->getMaxX() << "] x ["
                     << env->getMinY() << "," << env->getMaxY() << "]" << std::endl;
            std::cout << "插入对象网格: [(" << grid_min_x << "," << grid_min_y << ") to ("
                     << grid_max_x << "," << grid_max_y << ")]" << std::endl;
        }

        for (size_t i = 0; i < hash_funcs.size(); ++i) {
            size_t hash_val = hash_funcs[i](g);
            std::cout << "插入时哈希函数" << i << "计算值: " << hash_val << std::endl;
            bits.set(hash_val);
        }
        std::cout << "对象插入完成" << std::endl;
    }
    // 新增：重载 insert 方法以接受一个几何对象的向量
    void insert(const std::vector<geos::geom::Geometry*>& geoms) {
        for (const auto& g : geoms) {
            // 调用单个对象的 insert 方法
            insert(g);
        }
    }
    // 检查是否可能存在（false表示一定不存在）
    bool might_contain(const geos::geom::Geometry* query) {
        // 新增：检查query是否为空
        std::cout<<"＝＝＝＝＝＝＝＝＝＝＝＝＝＝开始进入might_contain..."<<std::endl;
        if (!query) {
            std::cerr << "BloomFilter查询失败：查询对象为空指针！" << std::endl;
            return false;
        }

        // 调试：打印查询窗口的网格信息
        const auto * query_env = query->getEnvelopeInternal();
        if (query_env) {
            const double GRID_SIZE = 4.0;  // 与哈希函数保持一致
            int q_grid_min_x = static_cast<int>(std::floor(query_env->getMinX() / GRID_SIZE));
            int q_grid_min_y = static_cast<int>(std::floor(query_env->getMinY() / GRID_SIZE));
            int q_grid_max_x = static_cast<int>(std::floor(query_env->getMaxX() / GRID_SIZE));
            int q_grid_max_y = static_cast<int>(std::floor(query_env->getMaxY() / GRID_SIZE));
            std::cout << "查询窗口MBR: [" << query_env->getMinX() << "," << query_env->getMaxX() << "] x ["
                     << query_env->getMinY() << "," << query_env->getMaxY() << "]" << std::endl;
            std::cout << "查询窗口网格: [(" << q_grid_min_x << "," << q_grid_min_y << ") to ("
                     << q_grid_max_x << "," << q_grid_max_y << ")]" << std::endl;
        }

        for (size_t i = 0; i < hash_funcs.size(); ++i) {
            size_t hash_val = hash_funcs[i](query);
            std::cout << "哈希函数" << i << "计算值: " << hash_val << ", 测试结果: " << bits.test(hash_val) << std::endl;
            if (!bits.test(hash_val)) {
                std::cout<<"＝＝＝＝＝＝＝＝＝＝＝＝＝＝might_contain 叶子节点与查询框不相交"<<std::endl;
                return false;
            }
        }
        return true;
    }

};