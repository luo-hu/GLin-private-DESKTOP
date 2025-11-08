#include <geos/geom/Envelope.h>
#include <vector>
#include <algorithm>

// 分层MBR节点
struct HierarchicalMBRNode {
    geos::geom::Envelope mbr;  // 当前层级的边界矩形
    std::vector<HierarchicalMBRNode> children;  // 子层级
    std::vector<geos::geom::Geometry*> geoms;  // 当前层级直接包含的几何对象
};

// 分层MBR管理器
class HierarchicalMBR {
private:
    HierarchicalMBRNode root;
    size_t max_depth;  // 最大分层深度
    size_t max_geoms_per_node;  // 每个节点最多包含的几何对象数

    // 递归构建分层结构
    void build(HierarchicalMBRNode& node, const std::vector<geos::geom::Geometry*>& geoms, size_t depth) {
        // 计算当前节点的MBR
        for (const auto& g : geoms) {
            node.mbr.expandToInclude(g->getEnvelopeInternal());
        }

        if (depth >= max_depth || geoms.size() <= max_geoms_per_node) {
            node.geoms = geoms;
            return;
        }

        // 按x坐标拆分（可替换为更优的空间划分策略）
        // std::sort(geoms.begin(), geoms.end(), [](geos::geom::Geometry* a, geos::geom::Geometry* b) {
        //     return a->getEnvelopeInternal()->getMinX() < b->getEnvelopeInternal()->getMinX();
        // });
        // 修正后（创建非const副本）：
        std::vector<geos::geom::Geometry*> temp_geoms = geoms;  // 复制为非const
        std::sort(temp_geoms.begin(), temp_geoms.end(), [](geos::geom::Geometry* a, geos::geom::Geometry* b) {
            // 排序逻辑（如按x坐标）
            return a->getEnvelopeInternal()->getMinX() < b->getEnvelopeInternal()->getMinX();
        });
        auto mid = geoms.begin() + geoms.size() / 2;
        std::vector<geos::geom::Geometry*> left(geoms.begin(), mid);
        std::vector<geos::geom::Geometry*> right(mid, geoms.end());

        node.children.emplace_back();
        build(node.children[0], left, depth + 1);
        node.children.emplace_back();
        build(node.children[1], right, depth + 1);
    }

    // 递归查询：收集与查询窗口相交的几何对象
    void query(const HierarchicalMBRNode& node, const geos::geom::Envelope& query_env, 
               std::vector<geos::geom::Geometry*>& result) const {
        if (!node.mbr.intersects(query_env)) {
            return;
        }
        // 收集当前节点直接包含的几何对象
        for (const auto& g : node.geoms) {
            result.push_back(g);
        }
        // 递归查询子节点
        for (const auto& child : node.children) {
            query(child, query_env, result);
        }
    }

public:
    HierarchicalMBR(size_t depth = 3, size_t max_per_node = 10) 
        : max_depth(depth), max_geoms_per_node(max_per_node) {}

    // 从几何对象列表构建分层MBR
    void build(const std::vector<geos::geom::Geometry*>& geoms) {
        build(root, geoms, 0);
    }

    // 查询与目标窗口相交的所有几何对象
    std::vector<geos::geom::Geometry*> query(const geos::geom::Envelope& query_env) const {
        std::vector<geos::geom::Geometry*> result;
        query(root, query_env, result);
        return result;
    }
};