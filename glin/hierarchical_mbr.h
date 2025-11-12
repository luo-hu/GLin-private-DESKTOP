// #include <geos/geom/Envelope.h>
// #include <vector>
// #include <algorithm>

// // 分层MBR节点
// struct HierarchicalMBRNode {
//     geos::geom::Envelope mbr;  // 当前层级的边界矩形
//     std::vector<HierarchicalMBRNode> children;  // 子层级
//     std::vector<geos::geom::Geometry*> geoms;  // 当前层级直接包含的几何对象
// };

// // 分层MBR管理器
// class HierarchicalMBR {
// private:
//     HierarchicalMBRNode root;
//     size_t max_depth;  // 最大分层深度
//     size_t max_geoms_per_node;  // 每个节点最多包含的几何对象数
//     // 递归构建分层结构
//     void build(HierarchicalMBRNode& node, const std::vector<geos::geom::Geometry*>& geoms, size_t depth) {
//         // --- 修正开始 ---
//         // 增加安全检查，防止geoms为空
//         if (geoms.empty()) {
//             return;
//         }
        
//         // 计算当前节点的MBR
//         for (const auto& g : geoms) {
//             // 增加空指针检查
//             if (g) {
//                 node.mbr.expandToInclude(g->getEnvelopeInternal());
//             }
//         }

//         if (depth >= max_depth || geoms.size() <= max_geoms_per_node) {
//             node.geoms = geoms;
//             return;
//         }

//         // 按x坐标拆分（可替换为更优的空间划分策略）
//         // 修正：创建非const副本进行排序
//         std::vector<geos::geom::Geometry*> temp_geoms = geoms;  // 复制为非const
//         std::sort(temp_geoms.begin(), temp_geoms.end(), [](geos::geom::Geometry* a, geos::geom::Geometry* b) {
//             // 确保指针有效
//             if (!a || !a->getEnvelopeInternal()) return true;
//             if (!b || !b->getEnvelopeInternal()) return false;
//             return a->getEnvelopeInternal()->getMinX() < b->getEnvelopeInternal()->getMinX();
//         });

//         // 修正：必须使用排序后的 'temp_geoms' 来进行拆分
//         auto mid = temp_geoms.begin() + temp_geoms.size() / 2;
//         std::vector<geos::geom::Geometry*> left(temp_geoms.begin(), mid);
//         std::vector<geos::geom::Geometry*> right(mid, temp_geoms.end());
//         // --- 修正结束 ---

//         node.children.emplace_back();
//         build(node.children[0], left, depth + 1);
//         node.children.emplace_back();
//         build(node.children[1], right, depth + 1);
//     }

//     // 递归查询：收集与查询窗口相交的几何对象
//     void query(const HierarchicalMBRNode& node, const geos::geom::Envelope& query_env, 
//                std::vector<geos::geom::Geometry*>& result) const {
//         if (!node.mbr.intersects(query_env)) {
//             return;
//         }
//         // 收集当前节点直接包含的几何对象
//         for (const auto& g : node.geoms) {
//             result.push_back(g);
//         }
//         // 递归查询子节点
//         for (const auto& child : node.children) {
//             query(child, query_env, result);
//         }
//     }

// public:
//     HierarchicalMBR(size_t depth = 3, size_t max_per_node = 10) 
//         : max_depth(depth), max_geoms_per_node(max_per_node) {}

//     // 从几何对象列表构建分层MBR
//     void build(const std::vector<geos::geom::Geometry*>& geoms) {
//         build(root, geoms, 0);
//     }

//     // 查询与目标窗口相交的所有几何对象
//     std::vector<geos::geom::Geometry*> query(const geos::geom::Envelope& query_env) const {
//         std::vector<geos::geom::Geometry*> result;
//         query(root, query_env, result);
//         return result;
//     }
// };


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
        // --- 修正开始 ---
        // 增加安全检查，防止geoms为空
        if (geoms.empty()) {
            return;
        }
        
        // 计算当前节点的MBR
        for (const auto& g : geoms) {
            // 增加空指针检查
            if (g) {
                node.mbr.expandToInclude(g->getEnvelopeInternal());
            }
        }

        if (depth >= max_depth || geoms.size() <= max_geoms_per_node) {
            node.geoms = geoms;
            return;
        }

        // 按x坐标拆分（可替换为更优的空间划分策略）
        // 修正：创建非const副本进行排序
        std::vector<geos::geom::Geometry*> temp_geoms = geoms;  // 复制为非const
        std::sort(temp_geoms.begin(), temp_geoms.end(), [](geos::geom::Geometry* a, geos::geom::Geometry* b) {
            // 确保指针有效
            if (!a || !a->getEnvelopeInternal()) return true;
            if (!b || !b->getEnvelopeInternal()) return false;
            return a->getEnvelopeInternal()->getMinX() < b->getEnvelopeInternal()->getMinX();
        });

        // 修正：必须使用排序后的 'temp_geoms' 来进行拆分
        auto mid = temp_geoms.begin() + temp_geoms.size() / 2;
        std::vector<geos::geom::Geometry*> left(temp_geoms.begin(), mid);
        std::vector<geos::geom::Geometry*> right(mid, temp_geoms.end());
        // --- 修正结束 ---

        node.children.emplace_back();
        build(node.children[0], left, depth + 1);
        node.children.emplace_back();
        build(node.children[1], right, depth + 1);
    }

    // 递归查询：收集与查询窗口相交的几何对象
    void query(const HierarchicalMBRNode& node, const geos::geom::Envelope& query_env, 
               std::vector<geos::geom::Geometry*>& result) const {
        std::cout << "DEBUG H-MBR Query: Node MBR: " << node.mbr.toString() << ", Query Env: " << query_env.toString() << std::endl;
        // 剪枝：如果查询窗口与当前节点的MBR不相交，则直接返回
        if (!node.mbr.intersects(query_env)) {
            return;
        }

        // --- 修正开始：区分叶子节点和内部节点 ---
        // 如果是叶子节点 (geoms 列表不为空)
        if (!node.geoms.empty()) {
            // 遍历此叶子节点中的所有几何体
            for (const auto& g : node.geoms) {
                std::cout << "DEBUG H-MBR Query: Checking geom MBR: " << g->getEnvelopeInternal()->toString() << std::endl;
                std::cout << "DEBUG H-MBR Query: Geom MBR intersects Query Env: " << (g->getEnvelopeInternal()->intersects(query_env) ? "true" : "false") << std::endl;
                // 关键检查：只有当几何体自身的MBR也与查询窗口相交时，才将其加入候选列表
                if (g && g->getEnvelopeInternal()->intersects(query_env)) {
                    result.push_back(g);
                }
            }
        } 
        // 如果是内部节点 (children 列表不为空)
        else {
            for (const auto& child : node.children) {
                query(child, query_env, result);
            }
        }
        // --- 修正结束 ---
    }

public:
    // *** 关键修改：将 max_per_node 默认值从 10 改为 9 ***
    // 这将强制包含10个项目的数据集进行分裂，使H-MBR真正“分层”。
    HierarchicalMBR(size_t depth = 3, size_t max_per_node = 9) 
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






// #include <geos/geom/Envelope.h>
// #include <vector>
// #include <algorithm>

// // 分层MBR节点
// struct HierarchicalMBRNode {
//     geos::geom::Envelope mbr;  // 当前层级的边界矩形
//     std::vector<HierarchicalMBRNode> children;  // 子层级
//     std::vector<geos::geom::Geometry*> geoms;  // 当前层级直接包含的几何对象
// };

// // 分层MBR管理器
// class HierarchicalMBR {
// private:
//     HierarchicalMBRNode root;
//     size_t max_depth;  // 最大分层深度
//     size_t max_geoms_per_node;  // 每个节点最多包含的几何对象数
//     // 递归构建分层结构
//     void build(HierarchicalMBRNode& node, const std::vector<geos::geom::Geometry*>& geoms, size_t depth) {
//         // --- 修正开始 ---
//         // 增加安全检查，防止geoms为空
//         if (geoms.empty()) {
//             return;
//         }
        
//         // 计算当前节点的MBR
//         for (const auto& g : geoms) {
//             // 增加空指针检查
//             if (g) {
//                 node.mbr.expandToInclude(g->getEnvelopeInternal());
//             }
//         }

//         if (depth >= max_depth || geoms.size() <= max_geoms_per_node) {
//             node.geoms = geoms;
//             return;
//         }

//         // 按x坐标拆分（可替换为更优的空间划分策略）
//         // 修正：创建非const副本进行排序
//         std::vector<geos::geom::Geometry*> temp_geoms = geoms;  // 复制为非const
//         std::sort(temp_geoms.begin(), temp_geoms.end(), [](geos::geom::Geometry* a, geos::geom::Geometry* b) {
//             // 确保指针有效
//             if (!a || !a->getEnvelopeInternal()) return true;
//             if (!b || !b->getEnvelopeInternal()) return false;
//             return a->getEnvelopeInternal()->getMinX() < b->getEnvelopeInternal()->getMinX();
//         });

//         // 修正：必须使用排序后的 'temp_geoms' 来进行拆分
//         auto mid = temp_geoms.begin() + temp_geoms.size() / 2;
//         std::vector<geos::geom::Geometry*> left(temp_geoms.begin(), mid);
//         std::vector<geos::geom::Geometry*> right(mid, temp_geoms.end());
//         // --- 修正结束 ---

//         node.children.emplace_back();
//         build(node.children[0], left, depth + 1);
//         node.children.emplace_back();
//         build(node.children[1], right, depth + 1);
//     }

//     // 递归查询：收集与查询窗口相交的几何对象
//     void query(const HierarchicalMBRNode& node, const geos::geom::Envelope& query_env, 
//                std::vector<geos::geom::Geometry*>& result) const {
//         if (!node.mbr.intersects(query_env)) {
//             return;
//         }
//         // 收集当前节点直接包含的几何对象
//         for (const auto& g : node.geoms) {
//             result.push_back(g);
//         }
//         // 递归查询子节点
//         for (const auto& child : node.children) {
//             query(child, query_env, result);
//         }
//     }

// public:
//     HierarchicalMBR(size_t depth = 3, size_t max_per_node = 10) 
//         : max_depth(depth), max_geoms_per_node(max_per_node) {}

//     // 从几何对象列表构建分层MBR
//     void build(const std::vector<geos::geom::Geometry*>& geoms) {
//         build(root, geoms, 0);
//     }

//     // 查询与目标窗口相交的所有几何对象
//     std::vector<geos::geom::Geometry*> query(const geos::geom::Envelope& query_env) const {
//         std::vector<geos::geom::Geometry*> result;
//         query(root, query_env, result);
//         return result;
//     }
// };
// #include <geos/geom/Envelope.h>
// #include <vector>
// #include <algorithm>

// // 分层MBR节点
// struct HierarchicalMBRNode {
//     geos::geom::Envelope mbr;  // 当前层级的边界矩形
//     std::vector<HierarchicalMBRNode> children;  // 子层级
//     std::vector<geos::geom::Geometry*> geoms;  // 当前层级直接包含的几何对象
// };

// // 分层MBR管理器
// class HierarchicalMBR {
// private:
//     HierarchicalMBRNode root;
//     size_t max_depth;  // 最大分层深度
//     size_t max_geoms_per_node;  // 每个节点最多包含的几何对象数
//     // 递归构建分层结构
//     void build(HierarchicalMBRNode& node, const std::vector<geos::geom::Geometry*>& geoms, size_t depth) {
//         // --- 修正开始 ---
//         // 增加安全检查，防止geoms为空
//         if (geoms.empty()) {
//             return;
//         }
        
//         // 计算当前节点的MBR
//         for (const auto& g : geoms) {
//             // 增加空指针检查
//             if (g) {
//                 node.mbr.expandToInclude(g->getEnvelopeInternal());
//             }
//         }

//         if (depth >= max_depth || geoms.size() <= max_geoms_per_node) {
//             node.geoms = geoms;
//             return;
//         }

//         // 按x坐标拆分（可替换为更优的空间划分策略）
//         // 修正：创建非const副本进行排序
//         std::vector<geos::geom::Geometry*> temp_geoms = geoms;  // 复制为非const
//         std::sort(temp_geoms.begin(), temp_geoms.end(), [](geos::geom::Geometry* a, geos::geom::Geometry* b) {
//             // 确保指针有效
//             if (!a || !a->getEnvelopeInternal()) return true;
//             if (!b || !b->getEnvelopeInternal()) return false;
//             return a->getEnvelopeInternal()->getMinX() < b->getEnvelopeInternal()->getMinX();
//         });

//         // 修正：必须使用排序后的 'temp_geoms' 来进行拆分
//         auto mid = temp_geoms.begin() + temp_geoms.size() / 2;
//         std::vector<geos::geom::Geometry*> left(temp_geoms.begin(), mid);
//         std::vector<geos::geom::Geometry*> right(mid, temp_geoms.end());
//         // --- 修正结束 ---

//         node.children.emplace_back();
//         build(node.children[0], left, depth + 1);
//         node.children.emplace_back();
//         build(node.children[1], right, depth + 1);
//     }

//     // 递归查询：收集与查询窗口相交的几何对象
//     void query(const HierarchicalMBRNode& node, const geos::geom::Envelope& query_env, 
//                std::vector<geos::geom::Geometry*>& result) const {
//         if (!node.mbr.intersects(query_env)) {
//             return;
//         }
//         // 收集当前节点直接包含的几何对象
//         for (const auto& g : node.geoms) {
//             result.push_back(g);
//         }
//         // 递归查询子节点
//         for (const auto& child : node.children) {
//             query(child, query_env, result);
//         }
//     }

// public:
//     HierarchicalMBR(size_t depth = 3, size_t max_per_node = 10) 
//         : max_depth(depth), max_geoms_per_node(max_per_node) {}

//     // 从几何对象列表构建分层MBR
//     void build(const std::vector<geos::geom::Geometry*>& geoms) {
//         build(root, geoms, 0);
//     }

//     // 查询与目标窗口相交的所有几何对象
//     std::vector<geos::geom::Geometry*> query(const geos::geom::Envelope& query_env) const {
//         std::vector<geos::geom::Geometry*> result;
//         query(root, query_env, result);
//         return result;
//     }
// };



// #pragma once

// #include <geos/geom/Envelope.h>
// #include <geos/geom/Geometry.h>
// #include <vector>
// #include <algorithm>
// #include <iostream> 

// // 分层MBR节点
// struct HierarchicalMBRNode {
//     geos::geom::Envelope mbr;
//     std::vector<HierarchicalMBRNode> children;  // 内部节点的子节点
//     std::vector<geos::geom::Geometry*> geoms;   // 叶子节点包含的几何体
// };

// // 分层MBR管理器
// class HierarchicalMBR {
// private:
//     HierarchicalMBRNode root;
//     size_t max_depth;
//     size_t max_geoms_per_node;

//     void build(HierarchicalMBRNode& node, const std::vector<geos::geom::Geometry*>& geoms, size_t depth) {
//         if (geoms.empty()) {
//             return;
//         }

//         for (const auto& g : geoms) {
//             if (g && g->getEnvelopeInternal()) {
//                 node.mbr.expandToInclude(g->getEnvelopeInternal());
//             } else {
//                 std::cerr << "[H-MBR Build] 警告: 几何对象或其包络为空" << std::endl;
//             }
//         }

//         // 终止条件：达到最大深度，或几何体数量已足够少
//         if (depth >= max_depth || geoms.size() <= max_geoms_per_node) {
//             node.geoms = geoms; 
//             return;
//         }

//         // 成为内部节点 (分裂)
//         std::vector<geos::geom::Geometry*> temp_geoms = geoms;
//         std::sort(temp_geoms.begin(), temp_geoms.end(), [](geos::geom::Geometry* a, geos::geom::Geometry* b) {
//             if (!a || !a->getEnvelopeInternal()) return true;
//             if (!b || !b->getEnvelopeInternal()) return false;
//             return a->getEnvelopeInternal()->getMinX() < b->getEnvelopeInternal()->getMinX();
//         });

//         auto mid = temp_geoms.begin() + temp_geoms.size() / 2;
//         std::vector<geos::geom::Geometry*> left(temp_geoms.begin(), mid);
//         std::vector<geos::geom::Geometry*> right(mid, temp_geoms.end());

//         if (!left.empty()) {
//             node.children.emplace_back(); 
//             build(node.children.back(), left, depth + 1); 
//         }
//         if (!right.empty()) {
//             node.children.emplace_back();
//             build(node.children.back(), right, depth + 1);
//         }
//     }

//     void query(const HierarchicalMBRNode& node, const geos::geom::Envelope& query_env, 
//                std::vector<geos::geom::Geometry*>& result) const {
//         // 剪枝：如果查询窗口与当前节点的MBR不相交，则直接返回
//         if (!node.mbr.intersects(query_env)) {
//             return;
//         }

//         // --- 修正开始：区分叶子节点和内部节点 ---
//         // 如果是叶子节点 (geoms 列表不为空)
//         if (!node.geoms.empty()) {
//             // 遍历此叶子节点中的所有几何体
//             for (const auto& g : node.geoms) {
//                 // 关键检查：只有当几何体自身的MBR也与查询窗口相交时，才将其加入候选列表
//                 if (g && g->getEnvelopeInternal()->intersects(query_env)) {
//                     result.push_back(g);
//                 }
//             }
//         } 
//         // 如果是内部节点 (children 列表不为空)
//         else {
//             for (const auto& child : node.children) {
//                 query(child, query_env, result);
//             }
//         }
//         // --- 修正结束 ---
//     }

// public:
//     // *** 关键修改 1：将 max_per_node 默认值从 10 改为 9 ***
//     // 这将强制包含10个项目的数据集进行分裂，使H-MBR真正“分层”。
//     HierarchicalMBR(size_t depth = 3, size_t max_per_node = 9)
//         : max_depth(depth), max_geoms_per_node(max_per_node) {}

//     void build(const std::vector<geos::geom::Geometry*>& geoms) {
//         root = HierarchicalMBRNode();
//         build(root, geoms, 0);
//     }

//     std::vector<geos::geom::Geometry*> query(const geos::geom::Envelope& query_env) const {
//         std::vector<geos::geom::Geometry*> result;
//         query(root, query_env, result);
//         return result;
//     }
// };