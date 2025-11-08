#include <vector>
#include <bitset>
#include <functional>

template <size_t N, size_t K>  // N: 位数组大小, K: 哈希函数数量
class BloomFilter {
private:
    std::bitset<N> bits;
    std::vector<std::function<size_t(const geos::geom::Geometry*)>> hash_funcs;

    // 生成K个哈希函数（基于几何对象的WKT字符串哈希）
    void init_hash_funcs() {
        for (size_t i = 0; i < K; ++i) {
            hash_funcs.emplace_back([i](const geos::geom::Geometry* g) {
                std::string wkt = g->toString();
                std::hash<std::string> hasher;
                return (hasher(wkt) + i) % N;  // 加盐哈希
            });
        }
    }

public:
    BloomFilter() { init_hash_funcs(); }

    // 插入几何对象
    void insert(const geos::geom::Geometry* g) {
        // 新增：检查g是否为空
        if (!g) {
            std::cerr << "BloomFilter插入失败：几何对象为空指针！" << std::endl;
            return;
        }
        for (const auto& hash : hash_funcs) {
            bits.set(hash(g));
        }
    }

    // 检查是否可能存在（false表示一定不存在）
    bool might_contain(const geos::geom::Geometry* query) {
        for (const auto& hash : hash_funcs) {
            if (!bits.test(hash(query))) {
                return false;
            }
        }
        return true;
    }
};