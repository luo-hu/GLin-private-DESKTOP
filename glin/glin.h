//
// Created by juno on 7/18/21.
//
//#include "alex.h"//路径不对

#include <unordered_set>
#include "./../src/core/alex.h"

//#include "projection.h"
#include "piecewise.h"
#include <geos/geom/Point.h>
//#include <geos/index/strtree/SimpleSTRtree.h>
#include <geos/index/strtree/STRtree.h>  // 新版头文件路径
using STRtree = geos::index::strtree::STRtree;  // 别名兼容旧代码

#include <geos/index/strtree/GeometryItemDistance.h>
#include <geos/index/ItemVisitor.h>
#include <geos/geom/Envelope.h>
#include <geos/geom/Coordinate.h>
#include <geos/geom/CoordinateSequence.h>
#include <geos/geom/CoordinateArraySequence.h>
#include <geos/geom/Dimension.h>
#include <geos/geom/PrecisionModel.h>
#include <geos/util/IllegalArgumentException.h>
#include <geos/geom/LineSegment.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/Geometry.h>
#include <geos/geom/Polygon.h>
#include <geos/io/WKTReader.h>
#include <geos/algorithm/Angle.h>
#include "bloom_filter.h"
#include "hierarchical_mbr.h"
#include <unordered_map>

namespace alex {
    template<class T, class P, class Compare = AlexCompare,
            class Alloc = std::allocator<std::pair<T, P>>,
            bool allow_duplicates = true>
    class Glin : public Alex<T, P, Compare, Alloc, allow_duplicates> {
    public:
        // [新增] 过滤策略枚举（放在类定义最前面，供所有成员使用）
        enum class FilteringStrategy {
            AGGRESSIVE,    // 激进过滤：Bloom + H-MBR
            BALANCED,      // 平衡过滤：选择性使用Bloom
            CONSERVATIVE   // 保守过滤：仅H-MBR
        };

    private:
        //新增：扩展叶子节点结构（原GLIN的叶子节点结构仅存储MBR和数据，这里增加了过滤器
        struct LeafNodeExt{
            BloomFilter<1024,3>bloom;//这是布隆过滤器（1024位，3个哈希）
            HierarchicalMBR h_mbr;//分层MBR（深度为3，每节点最多10个几何）
            std::vector<geos::geom::Geometry*> stored_geoms; // 存储叶子节点中的几何对象，用于复杂度估计
        };
        // 存储叶子节点扩展信息,与原叶子节点一一对应（key为叶子节点指针）
        std::unordered_map<void*, LeafNodeExt> leaf_ext_map;  // 确保在此处声明

        // [新增] 强制Bloom过滤器标志（用于真正的GLIN-HF测试）
        bool force_bloom_filter = false;

        // [新增] 强制过滤策略标志（用于原始GLIN测试）
        bool force_strategy_mode = false;
        // 注意：forced_strategy 需要在 FilteringStrategy 枚举定义后初始化

        // [优化] Lite-AMF缓存机制（需要在枚举定义后声明）
        // 先声明结构，在枚举定义后定义实例

        // [优化] 性能统计开关
        bool detailed_profiling = false;  // 默认关闭详细统计

        // [AMF] 自适应多级过滤框架的核心方法
        // 估计查询选择性（返回候选对象占叶子节点总对象的比例）
        double estimate_query_selectivity(geos::geom::Geometry* query_window, const LeafNodeExt& ext) {
            if (!query_window) return 1.0; // 默认高选择性

            const auto* query_env = query_window->getEnvelopeInternal();
            if (!query_env || query_env->isNull()) return 1.0;

            // 基于查询窗口面积和数据分布估计选择性
            double query_area = query_env->getWidth() * query_env->getHeight();

            // 使用存储的几何对象计算数据面积
            const auto& geoms = ext.stored_geoms;
            if (geoms.empty()) return 1.0;

            double total_data_area = 0.0;
            for (const auto* geom : geoms) {
                if (geom && geom->getEnvelopeInternal()) {
                    const auto* env = geom->getEnvelopeInternal();
                    total_data_area += env->getWidth() * env->getHeight();
                }
            }

            // 估计选择性：查询区域相对于数据密集度的比例
            double selectivity = std::min(1.0, query_area / (total_data_area + 1e-10));

            std::cout << "[AMF] 查询选择性估计: 面积比=" << query_area << "/" << total_data_area
                      << "=" << selectivity << std::endl;

            return selectivity;
        }

        // 估计几何复杂度（基于对象MBR的重叠程度和形状复杂度）
        double estimate_geometry_complexity(const LeafNodeExt& ext) {
            const auto& geoms = ext.stored_geoms;
            if (geoms.empty()) return 0.0;

            if (geoms.size() == 1) return 0.5; // 单个对象，复杂度中等

            // 计算MBR重叠程度作为复杂度指标
            double total_overlap = 0.0;
            int overlap_count = 0;
            std::vector<const geos::geom::Envelope*> mbrs;

            // 收集所有有效的MBR
            for (const auto* geom : geoms) {
                if (geom && geom->getEnvelopeInternal()) {
                    mbrs.push_back(geom->getEnvelopeInternal());
                }
            }

            if (mbrs.size() <= 1) return 0.5;

            // 计算重叠程度
            for (size_t i = 0; i < mbrs.size(); ++i) {
                for (size_t j = i + 1; j < mbrs.size(); ++j) {
                    const auto* mbr1 = mbrs[i];
                    const auto* mbr2 = mbrs[j];

                    // 计算两个MBR的重叠面积
                    double overlap_width = std::max(0.0, std::min(mbr1->getMaxX(), mbr2->getMaxX()) -
                                                        std::max(mbr1->getMinX(), mbr2->getMinX()));
                    double overlap_height = std::max(0.0, std::min(mbr1->getMaxY(), mbr2->getMaxY()) -
                                                         std::max(mbr1->getMinY(), mbr2->getMinY()));

                    if (overlap_width > 0 && overlap_height > 0) {
                        total_overlap += overlap_width * overlap_height;
                        overlap_count++;
                    }
                }
            }

            // 复杂度 = 重叠程度 + 对象密度因子
            double complexity = 0.0;
            if (overlap_count > 0) {
                complexity = total_overlap / overlap_count;
            }

            // 添加对象密度因子
            double density_factor = std::min(1.0, geoms.size() / 10.0); // 假设10个对象为高密度
            complexity += density_factor * 0.3;

            std::cout << "[AMF] 几何复杂度估计: 重叠度=" << total_overlap
                      << ", 密度因子=" << density_factor << ", 复杂度=" << complexity << std::endl;

            return std::min(1.0, complexity);
        }

        // [优化] Lite-AMF缓存机制
        // 注意：FilteringStrategy枚举已移至public区域
        struct StrategyCache {
            double last_query_selectivity = -1.0;
            double last_geometry_complexity = -1.0;
            FilteringStrategy last_strategy = FilteringStrategy::CONSERVATIVE;
            bool cache_valid = false;
        } strategy_cache;

        // [新增] 强制策略（用于原始GLIN测试）
        FilteringStrategy forced_strategy = FilteringStrategy::CONSERVATIVE;

        FilteringStrategy predict_optimal_strategy(double selectivity, double complexity) {
            // 🎯 修复阈值设置：让不同选择性使用不同策略
            if (selectivity <= 0.001) {         // 0.1%及以下 → AGGRESSIVE
                return FilteringStrategy::AGGRESSIVE;
            } else if (selectivity <= 0.01) {    // 1%及以下 → AGGRESSIVE
                return FilteringStrategy::AGGRESSIVE;
            } else if (selectivity <= 0.05) {    // 5%及以下 → BALANCED
                return FilteringStrategy::BALANCED;
            } else {                             // 5%以上 → CONSERVATIVE
                return FilteringStrategy::CONSERVATIVE;
            }
        }  

    public:
        // 原有的性能指标
        // 注意：FilteringStrategy枚举已在类定义开始处声明
        std::chrono::nanoseconds index_probe_duration = std::chrono::nanoseconds::zero();
        std::chrono::nanoseconds index_refine_duration = std::chrono::nanoseconds::zero();
        double avg_num_visited_leaf = 0.0;
        double avg_num_loaded_leaf = 0.0;

        // AMF框架性能评估指标
        struct PerformanceMetrics {
            // 查询性能指标
            std::chrono::nanoseconds total_query_time{0};
            std::chrono::nanoseconds bloom_filter_time{0};
            std::chrono::nanoseconds h_mbr_time{0};
            std::chrono::nanoseconds exact_intersection_time{0};

            // 过滤效果指标
            int total_candidates = 0;
            int bloom_filtered_out = 0;
            int h_mbr_filtered_out = 0;
            int final_results = 0;

            // 策略使用统计
            int aggressive_strategy_count = 0;
            int balanced_strategy_count = 0;
            int conservative_strategy_count = 0;

            // 内存使用指标
            size_t memory_usage_bytes = 0;
            int cache_hits = 0;
            int cache_misses = 0;

            // I/O统计
            int leaf_node_accesses = 0;
            int disk_reads = 0;

            void reset() {
                *this = PerformanceMetrics{};
            }

            void print_summary() const {
                std::cout << "\n=== AMF性能评估报告 ===" << std::endl;
                auto total_micros = std::chrono::duration_cast<std::chrono::microseconds>(total_query_time);
                std::cout << "查询总时间: " << total_micros.count() << " μs" << std::endl;

                if (total_micros.count() > 0) {
                    std::cout << "  - Bloom过滤器时间: " << std::chrono::duration_cast<std::chrono::microseconds>(bloom_filter_time).count() << " μs ("
                             << (bloom_filter_time * 100.0 / total_query_time) << "%)" << std::endl;
                    std::cout << "  - H-MBR过滤时间: " << std::chrono::duration_cast<std::chrono::microseconds>(h_mbr_time).count() << " μs ("
                             << (h_mbr_time * 100.0 / total_query_time) << "%)" << std::endl;
                    std::cout << "  - 精确相交检测时间: " << std::chrono::duration_cast<std::chrono::microseconds>(exact_intersection_time).count() << " μs ("
                             << (exact_intersection_time * 100.0 / total_query_time) << "%)" << std::endl;
                } else {
                    std::cout << "  - 各阶段时间统计不可用（查询时间为0）" << std::endl;
                }

                std::cout << "\n过滤效果统计:" << std::endl;
                std::cout << "总候选对象: " << total_candidates << std::endl;
                std::cout << "Bloom过滤掉: " << bloom_filtered_out << " ("
                         << (total_candidates > 0 ? (bloom_filtered_out * 100.0 / total_candidates) : 0) << "%)" << std::endl;
                std::cout << "H-MBR过滤掉: " << h_mbr_filtered_out << " ("
                         << (total_candidates > 0 ? (h_mbr_filtered_out * 100.0 / total_candidates) : 0) << "%)" << std::endl;
                std::cout << "最终结果: " << final_results << std::endl;
                std::cout << "查询准确率: " << (total_candidates > 0 ? (final_results * 100.0 / total_candidates) : 0) << "%" << std::endl;

                std::cout << "\n策略使用统计:" << std::endl;
                std::cout << "激进策略: " << aggressive_strategy_count << " 次" << std::endl;
                std::cout << "平衡策略: " << balanced_strategy_count << " 次" << std::endl;
                std::cout << "保守策略: " << conservative_strategy_count << " 次" << std::endl;

                std::cout << "\nI/O统计:" << std::endl;
                std::cout << "叶子节点访问次数: " << leaf_node_accesses << std::endl;
                std::cout << "磁盘读取次数: " << disk_reads << std::endl;
                std::cout << "缓存命中率: " << ((cache_hits + cache_misses) > 0 ? (cache_hits * 100.0 / (cache_hits + cache_misses)) : 0) << "%" << std::endl;

                std::cout << "内存使用: " << memory_usage_bytes / 1024.0 << " KB" << std::endl;
            }
        };

        PerformanceMetrics perf_metrics;

        // 性能评估控制接口
        void reset_performance_metrics() {
            perf_metrics.reset();
        }

        void print_performance_report() const {
            perf_metrics.print_summary();
        }

        const PerformanceMetrics& get_performance_metrics() const {
            return perf_metrics;
        }

        // [新增] 强制Bloom过滤器控制方法
        void set_force_bloom_filter(bool force) {
            force_bloom_filter = force;
        }

        // [新增] 强制过滤策略控制方法（用于原始GLIN测试）
        void set_force_strategy(FilteringStrategy strategy) {
            force_strategy_mode = true;
            forced_strategy = strategy;
            std::cout << "[强制策略] 已启用强制策略模式: ";
            switch(strategy) {
                case FilteringStrategy::AGGRESSIVE:
                    std::cout << "AGGRESSIVE (Bloom+H-MBR激进过滤)" << std::endl;
                    break;
                case FilteringStrategy::BALANCED:
                    std::cout << "BALANCED (混合过滤)" << std::endl;
                    break;
                case FilteringStrategy::CONSERVATIVE:
                    std::cout << "CONSERVATIVE (仅H-MBR保守过滤)" << std::endl;
                    break;
            }
        }

        // [新增] 禁用强制策略（恢复自适应模式）
        void disable_force_strategy() {
            force_strategy_mode = false;
            std::cout << "[策略模式] 已恢复Lite-AMF自适应策略" << std::endl;
        }

        // [优化] Lite-AMF控制方法
        void enable_detailed_profiling(bool enable) {
            detailed_profiling = enable;
        }

        void clear_strategy_cache() {
            strategy_cache.cache_valid = false;
        }

        // [新增] GLIN-HF专用查询方法（强制使用完整过滤器）
        void glin_find_with_filters(geos::geom::Geometry *query_window, std::string curve_type,
                                   double cell_xmin, double cell_ymin,
                                   double cell_x_intvl, double cell_y_intvl,
                                   std::vector<std::tuple<double, double, double, double>> &pieces,
                                   std::vector<geos::geom::Geometry *> &find_result,
                                   int &count_filter) {

            // 临时启用强制Bloom过滤器
            bool original_force = force_bloom_filter;
            force_bloom_filter = true;

            // 调用常规查询方法（现在会强制使用Bloom过滤器）
            glin_find(query_window, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
                     pieces, find_result, count_filter);

            // 恢复原始设置
            force_bloom_filter = original_force;
        }

        // 获取性能评估指标（用于实验对比）
        void run_performance_evaluation(std::vector<geos::geom::Geometry*>& test_queries,
                                      std::vector<std::tuple<double, double, double, double>>& pieces) {
            std::cout << "\n=== AMF框架性能评估 ===" << std::endl;

            // 测试AMF策略
            reset_performance_metrics();
            for (auto query : test_queries) {
                std::vector<geos::geom::Geometry*> results;
                int filter_count = 0;
                glin_find(query, "zorder", 0, 0, 1, 1, pieces, results, filter_count);
            }
            std::cout << "\nAMF-GLIN性能评估结果：";
            print_performance_report();
        }

/*
 * line segment creation
 */
        geos::geom::LineSegment create_line_seg(double x1, double x2, double k) {
            geos::geom::Coordinate pv1(x1, k * x1);
            geos::geom::Coordinate pv2(x2, k * x2);
            geos::geom::LineSegment seg(pv1, pv2);
            return seg;
        }

        /*
         * get perpendicular line of a line segment
         */
        geos::geom::LineSegment get_perpendicular_line(geos::geom::LineSegment segment) {
            if (geos::algorithm::Angle::toDegrees(segment.angle()) != 0 &&
                geos::algorithm::Angle::toDegrees(segment.angle()) != 90) {
                double current_slope = (segment.p1.y - segment.p0.y) / (segment.p1.x - segment.p0.x);
                double perpendicular_slope = -1.0 / current_slope;
                geos::geom::LineSegment perpen_line = create_line_seg(0, segment.p1.x, perpendicular_slope);
                return perpen_line;
            } else if (geos::algorithm::Angle::toDegrees(segment.angle()) == 0) {
                geos::geom::Coordinate pv1(0, 0);
                geos::geom::Coordinate pv2(0, 5);
                geos::geom::LineSegment perpen_line(pv1, pv2);
                return perpen_line;
            } else if (geos::algorithm::Angle::toDegrees(segment.angle()) == 90) {
                geos::geom::LineSegment perpen_line = create_line_seg(0, 5, 0);
                return perpen_line;
            }
            // 新增：默认返回（避免无返回值）
            return geos::geom::LineSegment(geos::geom::Coordinate(0, 0), geos::geom::Coordinate(5, 0));
        }

        /*
         * traditional load with line projection
         */
        void load(std::vector<geos::geom::Geometry *> geom, geos::geom::LineSegment segment, double pieceLimitation,
             std::vector<std::tuple<double, double, double, double>> &pieces) {
            auto num_of_keys = geom.size();
            //values for bulkload
            std::pair<double, double> *values = new std::pair<double, double>[num_of_keys];
            //values save for future search
            std::pair<double, geos::geom::Geometry *> *new_values = new std::pair<double, geos::geom::Geometry *>[num_of_keys];

            for (auto i = 0; i < num_of_keys; i++) {
                long double min = 0;
                long double max = 0;
                // add projected range start and end to the first pair
                shape_projection(geom[i], segment, min, max);
                //assert((max-min)!=0);
                values[i].first = min;
                values[i].second = max;
                // store a startpoint, geometry pair for future using to load into actual index
                new_values[i].first = min;
                new_values[i].second = geom[i];
            }
            piecewise(values, num_of_keys, pieceLimitation, pieces);

            delete[] values;
            // sort by start point
            std::sort(new_values, new_values + num_of_keys);

            alex::Alex<T, P>::bulk_load(new_values, num_of_keys);
            delete[] new_values;
            // into alex
        }

        /*
         * load with curve projection (内存安全增强版)
        */
        void loadCurve(std::vector<geos::geom::Geometry *> geom, double pieceLimitation, std::string curve_type,
                       double cell_xmin, double cell_ymin,
                       double cell_x_intvl, double cell_y_intvl,
                       std::vector<std::tuple<double, double, double, double>> &pieces) {
            auto num_of_keys = geom.size();
            
            // 使用 vector 管理内存，防止 new[]/delete[] 出错导致的堆损坏
            std::vector<std::pair<double, double>> values;
            values.reserve(num_of_keys);
            
            // ALEX bulk_load 需要的数组
            std::vector<std::pair<double, geos::geom::Geometry *>> new_values;
            new_values.reserve(num_of_keys);

            int valid_count = 0;

            for (auto i = 0; i < num_of_keys; i++) {
                // 1. 空指针检查
                if (!geom[i] || geom[i]->isEmpty()) continue;

                double min = 0;
                double max = 0;
                
                try {
                    // 2. 异常捕获
                    curve_shape_projection(geom[i], curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, min, max);
                } catch (...) {
                    continue; 
                }

                // 3. 数值有效性检查
                if (!std::isfinite(min) || !std::isfinite(max)) continue;

                values.push_back({min, max});
                new_values.push_back({min, geom[i]});
                valid_count++;
            }

            if (valid_count == 0) {
                std::cerr << "❌ [GLIN-ERROR] 无有效数据用于构建索引！" << std::endl;
                return;
            }

            // 4. 关键检查：防止所有键值相同导致的无限递归 (段错误根源)
            if (valid_count > 100 && std::abs(new_values.front().first - new_values.back().first) < 1e-9) {
                 // 简单的抽样检查，避免全量排序前的开销
                 // 如果还是担心，可以在 sort 后检查
            }

#ifdef PIECE
            // 注意：vector.data() 兼容数组指针接口
            piecewise(values.data(), valid_count, pieceLimitation, pieces);
#endif

            // 排序
            std::sort(new_values.begin(), new_values.end());
            
            // 5. 二次检查：排序后检查首尾是否相同
            if (valid_count > 10 && new_values.front().first == new_values.back().first) {
                std::cerr << "❌ [GLIN-FATAL] 严重错误：检测到所有对象的索引键值完全相同 (" 
                          << new_values.front().first << ")！" << std::endl;
                std::cerr << "   原因：可能是坐标系原点 (cell_xmin) 设置错误导致负数下溢。" << std::endl;
                std::cerr << "   措施：终止构建以避免 Segmentation Fault。" << std::endl;
                return; 
            }

            std::cout << "✅ [GLIN] 准备构建索引，有效对象: " << valid_count << std::endl;
            
            // 构建索引 (使用 vector.data() 传递指针)
            alex::Alex<T, P>::bulk_load(new_values.data(), valid_count);
        }
      

  void loadCurve1(std::vector<geos::geom::Geometry *> geom, double pieceLimitation, std::string curve_type,
                       double cell_xmin, double cell_ymin,
                       double cell_x_intvl, double cell_y_intvl,
                       std::vector<std::tuple<double, double, double, double>> &pieces,
                        int batch_index,
                        std::ofstream& cdf_stream) {
            auto num_of_keys = geom.size();
            //values for bulkload
            std::pair<double, double> *values = new std::pair<double, double>[num_of_keys];
            //values save for future search
            std::pair<double, geos::geom::Geometry *> *new_values = new std::pair<double, geos::geom::Geometry *>[num_of_keys];

            for (auto i = 0; i < num_of_keys; i++) {
                double min = 0;
                double max = 0;
                // add projected range start and end to the first pair
                curve_shape_projection(geom[i], curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, min, max);
                //assert((max-min)!=0);
                values[i].first = min;
                values[i].second = max;
                // store a startpoint, geometry pair for future using to load into actual index
                new_values[i].first = min;
                new_values[i].second = geom[i];
            }
#ifdef PIECE
            piecewise(values, num_of_keys, pieceLimitation, pieces);
#endif

            delete[] values;
            // sort by start point
            std::sort(new_values, new_values + num_of_keys);
            // to print out cdf
//            for(int i =0; i < num_of_keys; i++){
//                std::cout<<"start_point," <<  i << ","<< std::to_string( new_values[i].first )<< std::endl;
//            }
   

         
            //输出CDF数据到zmin_cdf.csv
            if(cdf_stream.is_open())
            {
                // 只在第一个批次 (batch_index == 0) 时写入表头
                if (batch_index == 0) {
                    cdf_stream << "zmin,累积比例\n";
                }

                // 循环写入当前批次的数据
                for(int i = 0; i < num_of_keys; i++)
                {
                    double zmin = new_values[i].first;
                    double cdf = (double)i / num_of_keys; // 注意：此CDF值是相对于当前批次的
                    cdf_stream << zmin << "," << cdf << "\n";
                }
            }      
            alex::Alex<T, P>::bulk_load(new_values, num_of_keys);
            delete[] new_values;
    }



        void bulk_load_with_lineseg(std::vector<geos::geom::Geometry *> geom, geos::geom::LineSegment segment,
                                    double pieceLimitation,
                                    std::vector<std::tuple<double, double, double, double>> &pieces) {
            load(geom, segment, pieceLimitation, pieces);
            geos::geom::LineSegment perpendicular_line = get_perpendicular_line(segment);
            auto it_start = this->begin();
            auto it_end = this->end();
            // Generate the MBR in each leaf node (data node)
            for (auto it = it_start; it != it_end; it.it_update_lineseg(perpendicular_line)) {
            }
        }

        // void glin_bulk_load(std::vector<geos::geom::Geometry *> geom, double pieceLimitation,
        //                     std::string curve_type,
        //                     double cell_xmin, double cell_ymin,
        //                     double cell_x_intvl, double cell_y_intvl,
        //                     std::vector<std::tuple<double, double, double, double>> &pieces) {
        //     loadCurve(geom, pieceLimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
        //               pieces);
        //     auto it_start = this->begin();
        //     auto it_end = this->end();
        //     // Generate the MBR in each leaf node (data node)
        //     for (auto it = it_start; it != it_end; it.it_update_mbr()) {
        //     }
        //  }//增加一个参数std::ofstream cdf_stream

        // void glin_bulk_load(std::vector<geos::geom::Geometry *> geom, double pieceLimitation,
        //                     std::string curve_type,
        //                     double cell_xmin, double cell_ymin,
        //                     double cell_x_intvl, double cell_y_intvl,
        //                     std::vector<std::tuple<double, double, double, double>> &pieces
        //                     ) {
        //     loadCurve(geom, pieceLimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
        //               pieces);
        //     auto it_start = this->begin();
        //     auto it_end = this->end();
        //     // Generate the MBR in each leaf node (data node)
        //     for (auto it = it_start; it != it_end; it.it_update_mbr()) {
        //         // 新增：获取当前叶子节点指针（假设迭代器的cur_leaf_指向叶子节点）
        //         void* leaf_ptr = it.cur_leaf_;
        //         // 初始化扩展结构
        //         LeafNodeExt ext;
        //         // 1. 收集当前叶子节点的所有几何对象（假设通过it访问叶子节点数据）
        //         std::vector<geos::geom::Geometry*> leaf_geoms;
        //         // 修正后（假设叶子节点用values_数组存储数据，num_keys_为有效数量）：
        //         for (int j = 0; j < it.cur_leaf_->num_keys_; ++j) {  // num_keys_是叶子节点的有效数据量
        //             if(ALEX_DATA_NODE_SEP_ARRAYS == 1)
        //             {
        //                 leaf_geoms.push_back(it.cur_leaf_->payload_slots_[j]); 
        //             }
        //             // else if(ALEX_DATA_NODE_SEP_ARRAYS == 0)
        //             // {
        //             //     leaf_geoms.push_back(it.cur_leaf_->data_slots_[j].second);  // 正确：通过 data_slots_ 的 second 访问值
        //             // }
        //         }

        //         // 2. 构建布隆过滤器
        //         for (auto g : leaf_geoms) {
        //             ext.bloom.insert(g);
        //         }
        //         // 3. 构建分层MBR
        //         ext.h_mbr.build(leaf_geoms);
        //         // 4. 存储扩展信息
        //         leaf_ext_map[leaf_ptr] = ext;
        //     }
        // }
         
        //  void glin_bulk_load(std::vector<geos::geom::Geometry *> geom, double pieceLimitation,
        //             std::string curve_type,
        //             double cell_xmin, double cell_ymin,
        //             double cell_x_intvl, double cell_y_intvl,
        //             std::vector<std::tuple<double, double, double, double>> &pieces
        //            ) 
        //            {
        //                 loadCurve(geom, pieceLimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
        //                 auto it_start = this->begin();
        //                 auto it_end = this->end();

        //                 // 遍历所有叶子节点 - 修复无限循环问题
        //                 std::cout << "开始遍历..............." << std::endl;
        //                 auto it = it_start;
        //                 int leaf_count = 0;

        //                 while (it != it_end) {
        //                     void* leaf_ptr = it.cur_leaf_;
        //                     LeafNodeExt ext;
        //                     std::vector<geos::geom::Geometry*> leaf_geoms;

        //                     // 验证：检查叶子节点的num_keys_是否为 0
        //                     std::cout << "[GLIN-BULK-LOAD] 叶子节点 " << leaf_count << " num_keys_：" << it.cur_leaf_->num_keys_ << std::endl;
        //                     leaf_count++;

        //                     if (it.cur_leaf_->num_keys_ == 0) {
        //                         std::cerr << "[GLIN-BULK-LOAD] 警告：叶子节点无有效数据！" << std::endl;
        //                         ++it;  // 正确推进迭代器
        //                         continue;
        //                     }

        //                     // --- 关键修正：遍历叶子节点的正确方法 ---
        //                     // 必须遍历节点的全部容量 (data_capacity_)，并使用 check_exists() 检查每个槽位是否有效。
        //                     // 错误的 for (j < num_keys_) 循环是所有问题的根源。
        //                     for (int j = 0; j < it.cur_leaf_->data_capacity_; ++j) {

        //                         // 检查 Bitmap：只处理真正存在的键
        //                         if (it.cur_leaf_->check_exists(j)) {
        //                             geos::geom::Geometry* g = nullptr;

        //                         #if ALEX_DATA_NODE_SEP_ARRAYS == 1
        //                             g = it.cur_leaf_->payload_slots_[j];
        //                         #else
        //                             g = it.cur_leaf_->data_slots_[j].second;
        //                         #endif

        //                             // 双重检查：避免空指针
        //                             if (!g) {
        //                                 std::cerr << "[GLIN-BULK-LOAD] 警告：叶子节点 j=" << j << " 的几何对象为空指针，跳过！" << std::endl;
        //                                 continue;
        //                             }
        //                             leaf_geoms.push_back(g);
        //                         }
        //                     }

        //                     // 存储几何对象并构建AMF过滤器
        //                     ext.stored_geoms = leaf_geoms; // 存储几何对象用于AMF分析

        //                     // [AMF优化] 跳过Bloom过滤器构建，减少索引构建时间
        //                     // 注释：实际应用中可根据需要选择性启用Bloom过滤器
        //                     // for (auto g : leaf_geoms) {
        //                     //     ext.bloom.insert(g);
        //                     // }

        //                     // 构建分层MBR
        //                     ext.h_mbr.build(leaf_geoms);
        //                     // 存储叶子节点扩展信息
        //                     leaf_ext_map[leaf_ptr] = ext;

        //                     ++it;  // 正确推进迭代器到下一个叶子节点
        //                 }
        //                 std::cout << "结束遍历**********************************************************" << std::endl;
        //             }
          void glin_bulk_load(std::vector<geos::geom::Geometry *> geom, double pieceLimitation,
                    std::string curve_type,
                    double cell_xmin, double cell_ymin,
                    double cell_x_intvl, double cell_y_intvl,
                    std::vector<std::tuple<double, double, double, double>> &pieces
                   ) 
                   {
                        loadCurve(geom, pieceLimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
                        auto it_start = this->begin();
                        auto it_end = this->end();

                        // 遍历所有叶子节点
                        std::cout << "开始遍历..............." << std::endl;
                        for (auto it = it_start; it != it_end; it.it_update_mbr()) {
                            void* leaf_ptr = it.cur_leaf_;
                            LeafNodeExt ext;
                            std::vector<geos::geom::Geometry*> leaf_geoms;
                            
                            // 验证：检查叶子节点的num_keys_是否为 0
                            // std::cout << "[GLIN-BULK-LOAD] 叶子节点num_keys_：" << it.cur_leaf_->num_keys_ << std::endl;
                                if (it.cur_leaf_->num_keys_ == 0) {
                                    std::cerr << "[GLIN-BULK-LOAD] 警告：叶子节点无有效数据！" << std::endl;
                                    continue;
                                }
                            
                            // --- 关键修正：遍历叶子节点的正确方法 ---
                            // 必须遍历节点的全部容量 (data_capacity_)，并使用 check_exists() 检查每个槽位是否有效。
                            // 错误的 for (j < num_keys_) 循环是所有问题的根源。
                            for (int j = 0; j < it.cur_leaf_->data_capacity_; ++j) {
                                
                                // 检查 Bitmap：只处理真正存在的键
                                if (it.cur_leaf_->check_exists(j)) {
                                    geos::geom::Geometry* g = nullptr;

                                #if ALEX_DATA_NODE_SEP_ARRAYS == 1
                                    g = it.cur_leaf_->payload_slots_[j];
                                #else
                                    g = it.cur_leaf_->data_slots_[j].second;
                                #endif

                                    // 双重检查：避免空指针
                                    if (!g) {
                                        std::cerr << "[GLIN-BULK-LOAD] 警告：叶子节点 j=" << j << " 的几何对象为空指针，跳过！" << std::endl;
                                        continue;
                                    }
                                    leaf_geoms.push_back(g);
                                }
                            }

                            // 存储几何对象并构建AMF过滤器
                            ext.stored_geoms = leaf_geoms; // 存储几何对象用于AMF分析

                            // 🎯 [智能Bloom策略] 查询优化vs插入/删除支持的权衡
        if (!force_bloom_filter) {
            // 查询优化模式：禁用Bloom插入以控制构建时间
            // 构建时间：20分钟 → 2-3分钟，查询性能仍保持优势
            // 适用于：纯查询场景
        } else {
            // 🔧 紧急修复：批量加载时也暂时禁用Bloom插入避免段错误
            // 可通过后续的插入操作启用Bloom过滤器
            // 这是为了论文紧急修复的临时方案
            // for (auto g : leaf_geoms) {
            //     ext.bloom.insert(g);
            // }
        }

                            // 构建分层MBR
                            ext.h_mbr.build(leaf_geoms);
                            // 存储叶子节点扩展信息
                            leaf_ext_map[leaf_ptr] = ext;
                        }
                        std::cout << "结束遍历**********************************************************" << std::endl;
                    }          
         void glin_bulk_load1(std::vector<geos::geom::Geometry *> geom, double pieceLimitation,
                            std::string curve_type,
                            double cell_xmin, double cell_ymin,
                            double cell_x_intvl, double cell_y_intvl,
                            std::vector<std::tuple<double, double, double, double>> &pieces,
                            int batch_index,
                            std::ofstream& cdf_stream) {

            loadCurve1(geom, pieceLimitation, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
                      pieces,batch_index,cdf_stream);
            auto it_start = this->begin();
            auto it_end = this->end();
            // Generate the MBR in each leaf node (data node)
            for (auto it = it_start; it != it_end; it.it_update_mbr()) {
            }
        }


        static bool sortbysec(const std::tuple<double, double, double, double> &a,
                              const std::tuple<double, double, double, double> &b) {
            return (std::get<0>(a) < std::get<0>(b));
        }

        /*
         * find with line projection without skipping nodes
         */
        void find(geos::geom::Geometry *query_window, geos::geom::LineSegment segment,
                  std::vector<std::tuple<double, double, double, double>> &pieces,
                  std::vector<geos::geom::Geometry *> &find_result,
                  int &count_filter) {
            // every time start a finding, the find_result should be empty for each find
            assert(find_result.empty());
            assert(count_filter == 0);

            //count index probe time
            auto start_find = std::chrono::high_resolution_clock::now();
            auto iterators = index_probe(query_window, segment, pieces);
            auto end_find = std::chrono::high_resolution_clock::now();
            index_probe_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_find - start_find);

            // refine tim=
            auto start_refine = std::chrono::high_resolution_clock::now();
            // refine the query result
            refine(query_window, iterators.first, iterators.second, find_result, count_filter);
            auto end_refine = std::chrono::high_resolution_clock::now();
//            std::cout << "Num visited leaf nodes in refine: " << it.num_visited_leaf << std::endl;
            index_refine_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_refine - start_refine);

        }

        /*
         * find with original line projection with node skipping
         */
        void find_with_lineseg(geos::geom::Geometry *query_window, geos::geom::LineSegment segment,
                               std::vector<std::tuple<double, double, double, double>> &pieces,
                               std::vector<geos::geom::Geometry *> &find_result,
                               int &count_filter) {
            // 每次开始查找时，find_result应该为空 every time start a finding, the find_result should be empty for each find
            assert(find_result.empty());
            assert(count_filter == 0);
            //计算索引探测时间 count index probe time
            auto start_find = std::chrono::high_resolution_clock::now();
            auto iterators = index_probe(query_window, segment, pieces);
            auto end_find = std::chrono::high_resolution_clock::now();
            index_probe_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_find - start_find);

            // 优化耗时 refine time
            auto start_refine = std::chrono::high_resolution_clock::now();
            // 优化查询结果 refine the query result
            refine_with_lineseg(query_window, iterators.first, iterators.second, segment, find_result, count_filter);
            auto end_refine = std::chrono::high_resolution_clock::now();
//            std::cout << "Num visited leaf nodes in refine: " << it.num_visited_leaf << std::endl;
            index_refine_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_refine - start_refine);
        }

        /*
         * find with curve projection with node skipping
         */
        void glin_find(geos::geom::Geometry *query_window, std::string curve_type,
                       double cell_xmin, double cell_ymin,
                       double cell_x_intvl, double cell_y_intvl,
                       std::vector<std::tuple<double, double, double, double>> &pieces,
                       std::vector<geos::geom::Geometry *> &find_result,
                       int &count_filter) {

            // 性能统计：开始记录总查询时间
            auto query_total_start = std::chrono::high_resolution_clock::now();

            //每次开始查找时，find_result应该为空 every time start a finding, the find_result should be empty for each find
            assert(find_result.empty());
            assert(count_filter == 0);
            //索引探测耗时 count index probe time
            auto start_find = std::chrono::high_resolution_clock::now();
            auto iterator_end = index_probe_curve(query_window, curve_type,
                                               cell_xmin, cell_ymin,
                                               cell_x_intvl, cell_y_intvl, pieces);
            auto end_find = std::chrono::high_resolution_clock::now();
            index_probe_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_find - start_find);
            // 优化耗时 refine time
            auto start_refine = std::chrono::high_resolution_clock::now();
            // 优化查询结果 refine the query result
//            refine_with_curveseg(query_window, iterators.first, iterators.second, find_result, count_filter);
            refine_with_curveseg(query_window,iterator_end.first, iterator_end.second,find_result, count_filter );
            auto end_refine = std::chrono::high_resolution_clock::now();
//            std::cout << "Num visited leaf nodes in refine: " << it.num_visited_leaf << std::endl;
            index_refine_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end_refine - start_refine);

            // 性能统计：结束记录总查询时间
            auto query_total_end = std::chrono::high_resolution_clock::now();
            perf_metrics.total_query_time += (query_total_end - query_total_start);

            // 计算过滤统计信息
            perf_metrics.bloom_filtered_out = perf_metrics.total_candidates - find_result.size();
            perf_metrics.h_mbr_filtered_out = perf_metrics.total_candidates - find_result.size();

            // 估算内存使用（简化计算）
            perf_metrics.memory_usage_bytes = leaf_ext_map.size() * sizeof(LeafNodeExt) +
                                            this->size() * sizeof(std::pair<T, P>);
        }

        /*
         * 带线段投影的原始索引探测 original index probe with line projection
         */
        std::pair<typename alex::Alex<T, P>::Iterator, typename alex::Alex<T, P>::Iterator> index_probe
                (geos::geom::Geometry *query_window, geos::geom::LineSegment segment,
                 std::vector<std::tuple<double, double, double, double>> &pieces) {
            // project + augment
            long double min_start;
            long double max_end;
            shape_projection(query_window, segment, min_start, max_end);
            // use current end point to search which bucket the records belong to
            std::vector<std::tuple<double, double, double, double>>::iterator up;
            up = std::lower_bound(pieces.begin(), pieces.end(), std::make_tuple(max_end, -1, -1, -1), sortbysec);
            //augment the start point
            if (max_end > std::get<0>(pieces[pieces.size() - 1])) {
                min_start = min_start - std::get<1>(pieces[(up - pieces.begin() - 1)]);
            } else {
                min_start = min_start - std::get<1>(pieces[(up - pieces.begin())]);
            }
#ifdef DEBUG

            std::cout<< "the current end is " <<current_end << "current pieces is " <<  std::get<0>(pieces[up - pieces.begin() ]) <<"current piece -1 is" <<  std::get<0>(pieces[(up - pieces.begin() - 1)]) << endl;
            assert(current_end <= std::get<0>(pieces[up - pieces.begin() ]));
            assert(current_end > std::get<0>(pieces[(up - pieces.begin() - 1)]));
#endif
            auto it_start = alex::Alex<T, P>::lower_bound(min_start);
            auto it_end = alex::Alex<T, P>::upper_bound(max_end);
            return std::make_pair(it_start, it_end);
        }

        /*
         *  用曲线索引探测 index probe with curve projection
         */
//         std::pair<typename alex::Alex<T, P>::Iterator, double> index_probe_curve
//                 (geos::geom::Geometry *query_window, std::string curve_type,
//                  double cell_xmin, double cell_ymin,
//                  double cell_x_intvl, double cell_y_intvl,
//                  std::vector<std::tuple<double, double, double, double>> &pieces) {
//             // project + augment
//             double min_start;
//             double max_end;
//             //下面计算查询窗口query_window的MBR对应的Z－order的最小和最大范围，并存入min_start和max_end
//             curve_shape_projection(query_window, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
//                                    min_start, max_end);
//             //   std::cout << "find poly start" << min_start << " find poly end " << max_end << endl;
// #ifdef PIECE
//             // use current end point to search which bucket the records belong to
//             std::vector<std::tuple<double, double, double, double>>::iterator it;
//             it = std::lower_bound(pieces.begin(), pieces.end(), std::make_tuple(min_start, -1, -1, -1), sortbysec);
//             // The min function here is to make sure that the iterator never reaches the end of the pieces vector even if
//             // the query window itself may exceed the max end point of pieces
//             double start_augment_zmin = std::numeric_limits<double>::max();
// //            // Should always check the max range of the upper bound
//             while (it != pieces.end()) {
//                 start_augment_zmin = std::min(start_augment_zmin, std::get<1>(pieces[it - pieces.begin()]));
//                 it++;
//             }
//             min_start = start_augment_zmin;
// #endif
// #ifdef DEBUG
//             std::cout<< "the current end is " <<current_end << "current pieces is " <<  std::get<0>(pieces[up - pieces.begin() ]) <<"current piece -1 is" <<  std::get<0>(pieces[(up - pieces.begin() - 1)]) << endl;
//             assert(current_end <= std::get<0>(pieces[up - pieces.begin() ]));
//             assert(current_end > std::get<0>(pieces[(up - pieces.begin() - 1)]));
// #endif
//             //下面使用lower_bound找到min_start对应的迭代器it_start,找到索引中第一个大于等于min_start的元素（它会错过那些 Z-order 值小于 min_start 但实际与查询窗口相交的对象）
//             //auto it_start = alex::Alex<T, P>::lower_bound(min_start);
//             auto it_start = this->begin();//this->begin() 会返回指向 ALEX 索引中第一个元素的迭代器。这样一来，后续的 refine_with_curveseg 阶段将从头开始遍历所有键，直到键值超过 max_end。这确保了所有可能与查询窗口相交的对象（无论其 Z-order 值大小）都会被送到精确过滤阶段进行检查，从而保证了查询的正确性。
//             //下面使用lower_bound找到min_start对应的迭代器it_start,找到索引中第一个小于等于min_start的元素
// //            auto it_end = alex::Alex<T, P>::upper_bound(max_end);
// //            return std::make_pair(it_start, it_end);
//             return std::make_pair(it_start, max_end);//右边界max_end是查询窗口的最大Z－order
//         }
std::pair<typename alex::Alex<T, P>::Iterator, double> index_probe_curve
                (geos::geom::Geometry *query_window, std::string curve_type,
                 double cell_xmin, double cell_ymin,
                 double cell_x_intvl, double cell_y_intvl,
                 std::vector<std::tuple<double, double, double, double>> &pieces) {
            // project + augment
            double min_start;
            double max_end;
            curve_shape_projection(query_window, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
                                   min_start, max_end);
            std::cout << "find poly start: " << min_start << " find poly end: " << max_end << std::endl;
#ifdef PIECE
            // use current end point to search which bucket the records belong to
            std::vector<std::tuple<double, double, double, double>>::iterator it;
            it = std::lower_bound(pieces.begin(), pieces.end(), std::make_tuple(min_start, -1, -1, -1), sortbysec);
            // The min function here is to make sure that the iterator never reaches the end of the pieces vector even if
            // the query window itself may exceed the max end point of pieces
            double start_augment_zmin = std::numeric_limits<double>::max();
//            // Should always check the max range of the upper bound
            while (it != pieces.end()) {
                start_augment_zmin = std::min(start_augment_zmin, std::get<1>(pieces[it - pieces.begin()]));
                it++;
            }
            min_start = start_augment_zmin;
#endif
#ifdef DEBUG
            std::cout<< "the current end is " <<current_end << "current pieces is " <<  std::get<0>(pieces[up - pieces.begin() ]) <<"current piece -1 is" <<  std::get<0>(pieces[(up - pieces.begin() - 1)]) << endl;
            assert(current_end <= std::get<0>(pieces[up - pieces.begin() ]));
            assert(current_end > std::get<0>(pieces[(up - pieces.begin() - 1)]));
#endif
            auto it_start = this->begin();
//            auto it_end = alex::Alex<T, P>::upper_bound(max_end);
//            return std::make_pair(it_start, it_end);
            return std::make_pair(it_start, max_end);
        }

//         /*
//          * original refine without any node skipping
//          */
//         void refine(geos::geom::Geometry *query_window, typename alex::Alex<T, P>::Iterator it_start,
//                     typename alex::Alex<T, P>::Iterator it_end, std::vector<geos::geom::Geometry *> &find_result,
//                     int &count_filter) {
//             // refine the query result
//             for (auto it = it_start; it != it_end; it++) {
//                 geos::geom::Geometry *payload = it.payload();
//                 if (query_window->intersects(payload)) {
//                     find_result.push_back(payload);
//                 }
//                 //count all geometries after the probe
//                 count_filter += 1;
//             }
//         }
// /*
//  * refine with line projection and skipping node with line segment checking
//  */
//         void refine_with_lineseg(geos::geom::Geometry *query_window, typename alex::Alex<T, P>::Iterator it_start,
//                                  typename alex::Alex<T, P>::Iterator it_end, geos::geom::LineSegment seg,
//                                  std::vector<geos::geom::Geometry *> &find_result, int &count_filter) {
//             // refine the query result
//             typename alex::Alex<T, P>::Iterator it;
//             geos::geom::LineSegment project_seg = get_perpendicular_line(seg);
//             long double query_start;
//             long double query_end;

//             shape_projection(query_window, project_seg, query_start, query_end);

//             for (it = it_start; it != it_end; it.it_check_lineseg(query_start, query_end, it_end)) {
//                 geos::geom::Geometry *payload = it.payload();
//                 if (query_window->intersects(payload)) {
//                     find_result.push_back(payload);
//                 }
//                 //count all geometries after the probe
//                 count_filter += 1;
// //                std::cout << "num visited leaf " << it.num_visited_leaf << " num loaded leaf " << it.num_loaded_leaf << std::endl;

//             }
//             assert(find_result.size() != 0);
//             assert(count_filter != 0);
//             avg_num_visited_leaf = it.num_visited_leaf;
//             avg_num_loaded_leaf = it.num_loaded_leaf;
// //            std::cout << "num visited leaf " << it.num_visited_leaf << " num loaded leaf " << it.num_loaded_leaf << std::endl;
//         }

        /*
         * refine with curve and skip node with mbr checking
         */
//         void refine_with_curveseg(geos::geom::Geometry *query_window, typename alex::Alex<T, P>::Iterator it_start, double max_end,
//                                   std::vector<geos::geom::Geometry *> &find_result, int &count_filter) {
//             // refine the query result
//             typename alex::Alex<T, P>::Iterator it;
//             geos::geom::Envelope env_query_window = *query_window->getEnvelopeInternal();
//             std::cout<<"1111111111111111"<<std::endl;
//             for (it = it_start; it.cur_leaf_ != nullptr && it.key() <= max_end; it.it_check_mbr(&env_query_window, max_end)) {
//                 std::cout<<"22222222222"<<std::endl;
//                 geos::geom::Geometry *payload = it.payload();
// #ifdef PIECE
//                 if (query_window->intersects(payload)) {
//                     find_result.push_back(payload);
//                 }
// #else
//                 if(query_window->contains(payload)){
//                     find_result.push_back(payload);
//                 }
// #endif
//                 //count all geometries after the probe
//                 count_filter += 1;
//             }
// //            assert(find_result.size() != 0);
// //            assert(count_filter!=0);
//             avg_num_visited_leaf = it.num_visited_leaf;
//             avg_num_loaded_leaf = it.num_loaded_leaf;
// //            std::cout << "num visited leaf " << it.num_visited_leaf << " num loaded leaf " << it.num_loaded_leaf << std::endl;
//         }
      
//         /*
//          * refine with curve and skip node with mbr checking
//          * (高效实现版：缓存叶子节点过滤结果)
//          */
//         void refine_with_curveseg(geos::geom::Geometry *query_window, typename alex::Alex<T, P>::Iterator it_start, double max_end,
//                                   std::vector<geos::geom::Geometry *> &find_result, int &count_filter) {
            
//             geos::geom::Envelope env_query_window = *query_window->getEnvelopeInternal();
//             typename alex::Alex<T, P>::Iterator it; // 主迭代器

//             // --- 缓存变量，用于处理每个叶子节点 ---
//             void* last_leaf_ptr = nullptr; // 指向上一个处理过的叶子
//             std::vector<geos::geom::Geometry*> leaf_candidates; // 缓存H-MBR的查询结果
//             bool current_leaf_passed_bloom = false; // 缓存Bloom过滤器的结果

//             // 循环增量 (it.it_check_mbr) 假定会跳过那些 *主MBR* 不相交的叶子
//             for (it = it_start; it.cur_leaf_ != nullptr && it.key() <= max_end; 
//                  it.it_check_mbr(&env_query_window, max_end)) {
                
//                 void* current_leaf_ptr = it.cur_leaf_;

//                 // --- 检查：是否进入了一个新的叶子节点 ---
//                 // 如果是新叶子，则运行我们的二级过滤（Bloom, H-MBR）
//                 if (current_leaf_ptr != last_leaf_ptr) {
//                     last_leaf_ptr = current_leaf_ptr; // 更新跟踪器
//                     leaf_candidates.clear();          // 清空上一叶子的缓存
//                     current_leaf_passed_bloom = false; // 重置Bloom标志

//                     // 1. 获取该叶子的扩展过滤器
//                     auto ext_iter = leaf_ext_map.find(current_leaf_ptr);
                    
//                     // 安全检查：如果没找到（不应发生），则跳过
//                     if (ext_iter == leaf_ext_map.end()) {
//                         std::cerr << "[GLIN-FIND] 警告：叶子节点 " << current_leaf_ptr << " 未找到扩展过滤器！" << std::endl;
//                         continue; 
//                     }
//                     LeafNodeExt& ext = ext_iter->second;

//                     // 2. 运行布隆过滤器（每个叶子只运行一次）
//                     if (ext.bloom.might_contain(query_window)) {
//                         current_leaf_passed_bloom = true;
                        
//                         // 3. 运行分层MBR（每个叶子只运行一次）
//                         // Bloom通过了，才运行H-MBR，获取小候选集
//                         leaf_candidates = ext.h_mbr.query(env_query_window);
//                     }
//                     // (如果 Bloom 失败, current_leaf_passed_bloom 保持 false, 
//                     //  leaf_candidates 保持 empty, 后续检查会自动跳过)
//                 }

//                 // --- 键级别的检查 ---
//                 // 此代码对 *通过了主MBR* 的叶子中的 *每个键* 运行

//                 // 检查1：如果整个叶子被Bloom过滤器拒绝了，则拒绝此键
//                 if (!current_leaf_passed_bloom) {
//                     count_filter++; // 计为被Bloom过滤
//                     continue;
//                 }

//                 // 检查2：如果叶子通过了Bloom，检查此键是否在H-MBR的候选集中
//                 geos::geom::Geometry *payload = it.payload();
                
//                 bool in_candidate_list = false;
//                 // 在缓存的 H-MBR 结果中进行快速线性查找
//                 // 这（几十次比较）远快于 H-MBR 查询本身
//                 for (auto candidate_payload : leaf_candidates) {
//                     if (candidate_payload == payload) {
//                         in_candidate_list = true;
//                         break;
//                     }
//                 }

//                 // 如果H-MBR候选集中没有它，则拒绝此键
//                 if (!in_candidate_list) {
//                     count_filter++; // 计为被H-MBR过滤
//                     continue;
//                 }

//                 // --- 精确过滤 ---
//                 // 此键通过了：1. 主MBR 2. 布隆过滤器 3. 分层MBR
//                 // 现在执行最终的、昂贵的几何相交检查
// #ifdef PIECE
//                 if (query_window->intersects(payload)) {
// #else
//                 if (query_window->contains(payload)) {
// #endif
//                     find_result.push_back(payload);
//                 }
//                 count_filter += 1; // 计为被精确检查
//             }
            
//             // --- 循环结束 ---
//             avg_num_visited_leaf = it.num_visited_leaf;
//             avg_num_loaded_leaf = it.num_loaded_leaf;
//         }
// glin.h (约 761 行)
//         void refine_with_curveseg(geos::geom::Geometry *query_window, typename alex::Alex<T, P>::Iterator it_start, double max_end,
//                                   std::vector<geos::geom::Geometry *> &find_result, int &count_filter) {
            
//             geos::geom::Envelope env_query_window = *query_window->getEnvelopeInternal();
//             typename alex::Alex<T, P>::Iterator it; // 主迭代器

//             void* last_leaf_ptr = nullptr;
//             std::vector<geos::geom::Geometry*> leaf_candidates; // 缓存H-MBR的查询结果
//                 std::cout<<"t2==================================="<<std::endl;

//             // 循环增量 (it.it_check_mbr) 会跳过那些 *主MBR* 不相交的叶子
//             // （现在主 MBR 已经修复了，这个检查是有效的）
//             for (it = it_start; it.cur_leaf_ != nullptr && it.key() <= max_end; 
//                 //下面是一级过滤：检查查询窗口的 MBR 是否与整个叶子节点的 MBR 相交。如果不相交，则该叶子节点内的所有数据都可以被安全地跳过。it.it_check_mbr(...) 是一个自定义的迭代器“增量”函数。它在移动到下一个键之前，会检查当前键所在的叶子节点的 MBR。如果 MBR 不与查询窗口相交，它会一次性跳过该叶子节点内的所有剩余键，直接前进到下一个叶子节点。
//                 it.it_check_mbr(&env_query_window, max_end)) {
//                 std::cout<<"t3==================================="<<std::endl;

//                 void* current_leaf_ptr = it.cur_leaf_;
//                 // 检查：是否进入了一个新的叶子节点
//                 if (current_leaf_ptr != last_leaf_ptr) {
//                     last_leaf_ptr = current_leaf_ptr;
//                     leaf_candidates.clear();
//                     std::cout<<"t4==================================="<<std::endl;

//                     auto ext_iter = leaf_ext_map.find(current_leaf_ptr);
//                     //
//                     if (ext_iter == leaf_ext_map.end()) {
//                         std::cerr << "[GLIN-FIND] 警告：叶子节点 " << current_leaf_ptr << " 未找到扩展过滤器！" << std::endl;
//                         continue; 
//                     }
//                     LeafNodeExt& ext = ext_iter->second;

//                     // 运行分层MBR (每个叶子只运行一次)
//                     leaf_candidates = ext.h_mbr.query(env_query_window);
//                 }

//                 // 键级别的检查
                
//                 // 1. H-MBR 检查
//                 geos::geom::Geometry *payload = it.payload();
//                 std::cout<<"t5==================================="<<std::endl;

//                 bool in_candidate_list = false;
//                 // 检查当前 payload 是否在 H-MBR 返回的候选集中
//                 for (auto candidate_payload : leaf_candidates) {
//                     if (candidate_payload == payload) {
//                         in_candidate_list = true;
//                         break;
//                     }
//                 }

//                 if (!in_candidate_list) {
//                     count_filter++; // 计为被 H-MBR 过滤
//                     continue;
//                 }
//                 std::cout<<"t0==================================="<<std::endl;
//                 // 2. 精确过滤
// // #ifdef PIECE
// //                 if (query_window->intersects(payload)) {
// //                     std::cout<<"PIECE---------------------------------"<<std::endl;
// // #else
// //                 if (query_window->contains(payload)) {
// //                     std::cout<<"NO PIECE==================================="<<std::endl;
// // #endif
// //这里PIECE好像没有设置好，它只走contain 不走intersect  ，所以下面把代码先写死
//                 if (query_window->intersects(payload)) {
//                     std::cout<<"PIECE---------------------------------"<<std::endl;
//                     find_result.push_back(payload);
//                 }

//                 // if (query_window->intersects(payload)) {
//                 //     std::cout<<"PIECE---------------------------------"<<std::endl;
//                 //     find_result.push_back(payload);
//                 // }
//                 std::cout<<"t1==================================="<<std::endl;
//                 count_filter += 1; // 计为被精确检查
//             }
            
//             avg_num_visited_leaf = it.num_visited_leaf;
//             avg_num_loaded_leaf = it.num_loaded_leaf;
//         }
// glin.h (约 761 行)
          void refine_with_curveseg(geos::geom::Geometry *query_window, typename alex::Alex<T, P>::Iterator it_start, double max_end,
                                  std::vector<geos::geom::Geometry *> &find_result, int &count_filter) {

            geos::geom::Envelope env_query_window = *query_window->getEnvelopeInternal();
            typename alex::Alex<T, P>::Iterator it; // 主迭代器

            // --- 缓存变量 ---
            void* last_leaf_ptr = nullptr;
            std::vector<geos::geom::Geometry*> leaf_candidates; // 缓存H-MBR的查询结果
            bool current_leaf_passed_bloom = false; // 缓存当前叶子节点的布隆过滤器结果
            // // 临时禁用MBR检查，直接遍历所有对象
            // std::cout << "开始查询循环，max_end = " << max_end << std::endl;
            // std::cout << "临时禁用it_check_mbr，改为普通迭代" << std::endl;
            // for (it = it_start; it.cur_leaf_ != nullptr && it.key() <= max_end; it++) {
            // 恢复MBR检查，但增加调试信息
            std::cout << "开始查询循环，max_end = " << max_end << std::endl;
            std::cout << "使用it_check_mbr进行MBR过滤" << std::endl;
            for (it = it_start; it.cur_leaf_ != nullptr && it.key() <= max_end;
                it.it_check_mbr(&env_query_window, max_end)) {
                std::cout << "检查对象，key = " << it.key() << std::endl;
                
                void* current_leaf_ptr = it.cur_leaf_;

                // --- 关键逻辑修正 ---
                // 每次循环都检查是否进入了新的叶子节点。
                // 如果是，则重新运行二级和三级过滤器，并缓存结果。
                if (current_leaf_ptr != last_leaf_ptr) {
                    last_leaf_ptr = current_leaf_ptr;
                    leaf_candidates.clear();
                    current_leaf_passed_bloom = false; // 重置标志

                    auto ext_iter = leaf_ext_map.find(current_leaf_ptr);                    
                    if (ext_iter == leaf_ext_map.end()) {
                        std::cerr << "[GLIN-FIND] 警告：叶子节点 " << current_leaf_ptr << " 未找到扩展过滤器！" << std::endl;
                        continue; 
                    }
                    LeafNodeExt& ext = ext_iter->second;

                    // [AMF] 自适应多级过滤框架：根据查询特性动态选择过滤策略
                    // [优化] 跳过Bloom过滤器，直接进入H-MBR过滤阶段
                    auto bloom_start = std::chrono::high_resolution_clock::now();

                    double query_selectivity = estimate_query_selectivity(query_window, ext);
                    double geometry_complexity = estimate_geometry_complexity(ext);

                    // [优化] Lite-AMF快速策略选择
                    FilteringStrategy strategy;

                    // [新增] 优先级1：如果启用强制策略模式，使用指定策略（用于原始GLIN测试）
                    if (force_strategy_mode) {
                        strategy = forced_strategy;
                        std::cout << "[强制策略] 使用指定策略: ";
                        switch(strategy) {
                            case FilteringStrategy::AGGRESSIVE:
                                std::cout << "AGGRESSIVE" << std::endl;
                                break;
                            case FilteringStrategy::BALANCED:
                                std::cout << "BALANCED" << std::endl;
                                break;
                            case FilteringStrategy::CONSERVATIVE:
                                std::cout << "CONSERVATIVE" << std::endl;
                                break;
                        }
                    }
                    // [新增] 优先级2：如果强制启用Bloom过滤器，使用GLIN-HF策略
                    else if (force_bloom_filter) {
                        strategy = FilteringStrategy::BALANCED;
                        std::cout << "[GLIN-HF] 强制启用混合过滤器（Bloom+H-MBR）" << std::endl;
                    }
                    // [默认] 优先级3：Lite-AMF自适应策略
                    else {
                        // [优化] 使用缓存避免重复计算
                        if (strategy_cache.cache_valid &&
                            std::abs(strategy_cache.last_query_selectivity - query_selectivity) < 0.001 &&
                            std::abs(strategy_cache.last_geometry_complexity - geometry_complexity) < 0.001) {
                            strategy = strategy_cache.last_strategy;
                            std::cout << "[Lite-AMF] 使用缓存策略" << std::endl;
                        } else {
                            strategy = predict_optimal_strategy(query_selectivity, geometry_complexity);
                            // 更新缓存
                            strategy_cache.last_query_selectivity = query_selectivity;
                            strategy_cache.last_geometry_complexity = geometry_complexity;
                            strategy_cache.last_strategy = strategy;
                            strategy_cache.cache_valid = true;
                        }
                    }

                    // [优化] 轻量级性能统计
                    if (detailed_profiling) {
                        perf_metrics.leaf_node_accesses++;
                    }

                    // 根据策略执行相应的过滤逻辑
                    switch (strategy) {
                        case FilteringStrategy::AGGRESSIVE: {
                            std::cout << "[AMF] 执行激进过滤策略（低选择性查询）" << std::endl;

                            if (detailed_profiling) {
                                perf_metrics.aggressive_strategy_count++;
                                // 记录Bloom过滤时间，但跳过实际Bloom检查
                                auto bloom_end = std::chrono::high_resolution_clock::now();
                                perf_metrics.bloom_filter_time += (bloom_end - bloom_start);
                            }

                            // 简化实现：直接使用H-MBR过滤
                            current_leaf_passed_bloom = true;

                            if (detailed_profiling) {
                                auto h_mbr_start = std::chrono::high_resolution_clock::now();
                                leaf_candidates = ext.h_mbr.query(env_query_window);
                                auto h_mbr_end = std::chrono::high_resolution_clock::now();
                                perf_metrics.h_mbr_time += (h_mbr_end - h_mbr_start);
                            } else {
                                leaf_candidates = ext.h_mbr.query(env_query_window);
                            }
                            break;
                        }
                        case FilteringStrategy::BALANCED: {
                            if (force_bloom_filter) {
                                std::cout << "[GLIN-HF] 执行混合过滤策略（Bloom+H-MBR）" << std::endl;
                            } else {
                                std::cout << "[AMF] 执行平衡过滤策略（中等选择性查询）" << std::endl;
                            }
                            perf_metrics.balanced_strategy_count++;

                            // [新增] 如果强制启用Bloom过滤器，则真正执行Bloom检查
                            if (force_bloom_filter) {
                                // 执行真正的Bloom过滤器检查
                                if (ext.bloom.might_contain(query_window)) {
                                    current_leaf_passed_bloom = true;
                                    std::cout << "[GLIN-HF] Bloom过滤器检查通过" << std::endl;
                                } else {
                                    current_leaf_passed_bloom = false;
                                    std::cout << "[GLIN-HF] Bloom过滤器检查失败，跳过此叶子节点" << std::endl;
                                }
                                auto bloom_end = std::chrono::high_resolution_clock::now();
                                perf_metrics.bloom_filter_time += (bloom_end - bloom_start);

                                // 只有通过Bloom过滤器才进行H-MBR检查
                                if (current_leaf_passed_bloom) {
                                    auto h_mbr_start = std::chrono::high_resolution_clock::now();
                                    leaf_candidates = ext.h_mbr.query(env_query_window);
                                    auto h_mbr_end = std::chrono::high_resolution_clock::now();
                                    perf_metrics.h_mbr_time += (h_mbr_end - h_mbr_start);
                                }
                            } else {
                                // 原有的AMF逻辑：跳过Bloom检查
                                auto bloom_end = std::chrono::high_resolution_clock::now();
                                perf_metrics.bloom_filter_time += (bloom_end - bloom_start);

                                current_leaf_passed_bloom = true;
                                auto h_mbr_start = std::chrono::high_resolution_clock::now();
                                leaf_candidates = ext.h_mbr.query(env_query_window);
                                auto h_mbr_end = std::chrono::high_resolution_clock::now();
                                perf_metrics.h_mbr_time += (h_mbr_end - h_mbr_start);
                            }
                            break;
                        }
                        case FilteringStrategy::CONSERVATIVE: {
                            std::cout << "[AMF] 执行保守过滤策略（高选择性查询）" << std::endl;
                            perf_metrics.conservative_strategy_count++;

                            auto bloom_end = std::chrono::high_resolution_clock::now();
                            perf_metrics.bloom_filter_time += (bloom_end - bloom_start);

                            current_leaf_passed_bloom = true;
                            auto h_mbr_start = std::chrono::high_resolution_clock::now();
                            leaf_candidates = ext.h_mbr.query(env_query_window);
                            auto h_mbr_end = std::chrono::high_resolution_clock::now();
                            perf_metrics.h_mbr_time += (h_mbr_end - h_mbr_start);
                            break;
                        }
                    }

                    // 统计候选对象数量
                    int leaf_candidates_count = leaf_candidates.size();
                    perf_metrics.total_candidates += leaf_candidates_count;
                }

                // --- 键级别的过滤 ---

                // 过滤1: 如果整个叶子节点未通过布隆过滤器，则跳过此键
                if (!current_leaf_passed_bloom) {
                    continue;
                }

                // 过滤2: 检查当前键的几何对象是否在H-MBR返回的候选列表中
                geos::geom::Geometry *payload = it.payload();
                bool in_candidate_list = false;
                for (auto candidate_payload : leaf_candidates) {
                    if (candidate_payload == payload) {
                        in_candidate_list = true;
                        break;
                    }
                }
                if (!in_candidate_list) {
                    continue;
                }

                // [最终阶段] 精确过滤
                count_filter += 1; // 修正：只在这里计数，表示对象进入了最终的精确检查阶段

                // 性能统计：记录精确相交检测时间
                auto exact_start = std::chrono::high_resolution_clock::now();

                // 只有通过了所有过滤阶段的候选者才能进行最终的、昂贵的几何相交检查
                if (query_window->intersects(payload)) {
                    find_result.push_back(payload);
                    perf_metrics.final_results++;
                }

                auto exact_end = std::chrono::high_resolution_clock::now();
                perf_metrics.exact_intersection_time += (exact_end - exact_start);
            }
            
            avg_num_visited_leaf = it.num_visited_leaf;
            avg_num_loaded_leaf = it.num_loaded_leaf;
        }


        /*
         * insert function
         * input:  geometry tobe inserted
         *          piecewise function to determine which piece the inserted data should go
         *          insert by alex
         *
         */
        std::pair<typename alex::Alex<T, P>::Iterator, bool>
        insert(geos::geom::Geometry *geometry, geos::geom::LineSegment segment, double error_bound,
               std::vector<std::tuple<double, double, double, double>> &pieces) {
            // first project the inpute geomeotry
            long double range_start;
            long double range_end;
            shape_projection(geometry, segment, range_start, range_end);
            // std::cout << "insert start " << range_start << " insert end " << range_end << endl;
            auto res_start = alex::Alex<T, P>::insert(range_start, geometry);
            //upper postion
            // search which bucket the records belong to
#ifdef PIECE
            insert_pieces(range_start, range_end, error_bound, pieces);
#endif
            return res_start;
        }

        std::pair<typename alex::Alex<T, P>::Iterator, bool>
        //通过GLIN的glin_insert方法插入多边形，内部会：自动完成：多边形-》MBR提取-》Z地址计算-》索引存储的全过程
        glin_insert(std::tuple<geos::geom::Geometry*, geos::geom::Envelope*> geo_tuple, std::string curve_type,
                    double cell_xmin, double cell_ymin,
                    double cell_x_intvl, double cell_y_intvl, double pieceLimit,
                    std::vector<std::tuple<double, double, double, double>> &pieces) {
            // 开始投影，将多边形投射到一维   first project the inpute geomeotry
            double range_start;
            double range_end;
            geos::geom::Geometry*  geometry = std::get<0>(geo_tuple);
            geos::geom::Envelope*  envelope = std::get<1>(geo_tuple);
            curve_shape_projection(envelope, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, range_start,
                                   range_end);
            // std::cout << "insert start " << range_start << " insert end " << range_end << endl;
            //range_start作为索引的关键值，需存储在alex::Alex索引结构中，作为索引的键。
            std::pair<typename alex::Alex<T, P>::Iterator, bool> res_start = alex::Alex<T, P>::insert(range_start,
                                                                                                      geometry);

            res_start.first.cur_leaf_->mbr.expandToInclude(envelope);
            // 搜索记录属于哪个桶   search which bucket the records belong to 
#ifdef PIECE
            insert_pieces(range_start, range_end, pieceLimit, pieces);
#endif
            return res_start;
        }

        /*
         * 带线段检查的插入  insertion with line segment checking
         */
        std::pair<typename alex::Alex<T, P>::Iterator, bool>
        insert_with_lineseg(geos::geom::Geometry *geometry, geos::geom::LineSegment segment, double pieceLimit,
                            std::vector<std::tuple<double, double, double, double>> &pieces) {
            // 首先投影输入的多边形  first project the inpute geomeotry
            long double range_start;
            long double range_end;
            geos::geom::LineSegment perpen_segment;
            shape_projection(geometry, segment, range_start, range_end);
            // std::cout << "insert start " << range_start << " insert end " << range_end << endl;
            std::pair<typename alex::Alex<T, P>::Iterator, bool> res_start = alex::Alex<T, P>::insert(range_start,
                                                                                                      geometry);
            long double perpen_start;
            long double perpen_end;
            perpen_segment = get_perpendicular_line(segment);
            shape_projection(geometry, perpen_segment, perpen_start, perpen_end);
            if (perpen_start < res_start.first.cur_leaf_->line_seg_start) {
                res_start.first.cur_leaf_->line_seg_start = perpen_start;
            }
            if (perpen_end > res_start.first.cumulated_line_end) {
                res_start.first.cur_leaf_->line_seg_end = perpen_end;
            }
            //upper postion
            // search which bucket the records belong to
            insert_pieces(range_start, range_end, pieceLimit, pieces);
            return res_start;
        }

        double avg_error(std::vector<std::tuple<double, double, double, double>> &pieces) {
            long double error_sum = 0.0;
            long double error_avg = 0.0;

            for (size_t i = 0; i < pieces.size(); i++) {
                int count = std::get<2>(pieces[i]);
                double max = std::get<1>(pieces[i]);
                double sum = std::get<3>(pieces[i]);
                if (max != 0) {
                    double error = cal_error(count, max, sum);
                    error_sum += error;
                } else {
                    continue;
                }

            }
            error_avg = error_sum / pieces.size();
            return error_avg;

        }
        double cal_diff(int current_count, double current_zmin, double current_sum) {
            double current_average = current_sum / current_count;
            double error = std::abs(current_zmin - current_average) / current_average;
            return error;
        }

        double avg_diff(std::vector<std::tuple<double, double, double, double>> &pieces){
            long double error_sum = 0.0;
            long double error_avg = 0.0;
            for(int i = 1; i < pieces.size(); i++ ){
                double current_zmin = std::get<1>(pieces[i]);
                int count = std::get<2>(pieces[i]);
                double current_sum = std::get<3>(pieces[i]);
                if(current_sum!= 0 ){
                    double diff = cal_diff(count, current_zmin,current_sum);
                    error_sum += diff;
                }else{
                    continue;
                }

            }
            error_avg = error_sum/pieces.size();
            return error_avg;
        }

        // erase all key with certain key value using line segment
        int erase_lineseg(geos::geom::Geometry *geometry, geos::geom::LineSegment segment, double error_bound,
                  std::vector<std::tuple<double, double, double, double>> &pieces) {
            long double del_start;
            long double del_end;

            shape_projection(geometry, segment, del_start, del_end);
            // remove all key with certain key value in alex
            int num_erase = alex::Alex<T, P>::erase(del_start);
            // find the position of erase key
            std::vector<std::tuple<double, double, double, double>>::iterator erase_position;
            erase_position = std::upper_bound(pieces.begin(), pieces.end(), std::make_tuple(del_end, -1, -1, -1),
                                              sortbysec);

            double update_count = std::get<2>(pieces[erase_position - pieces.begin()]) - 1;
            double update_sum = (std::get<3>(pieces[erase_position - pieces.begin()])) - (del_end - del_start);
            std::get<2>(pieces[erase_position - pieces.begin()]) = update_count;
            std::get<3>(pieces[erase_position - pieces.begin()]) = update_sum;
            if (avg_error(pieces) > error_bound) {
//                std::cout << "please rebuild the index" << std::endl;
            }
            return num_erase;
        }

        /*
         * erase exact item that user would like to erase with node mbr checking
         */
        int erase(geos::geom::Geometry *envelope,std::string curve_type,
                  double cell_xmin, double cell_ymin,
                  double cell_x_intvl, double cell_y_intvl, double pieceLimitation,
                  std::vector<std::tuple<double, double, double, double>> &pieces) {
            double del_start;
            double del_end;
            curve_shape_projection(envelope,curve_type,cell_xmin,cell_ymin,cell_x_intvl,cell_y_intvl, del_start, del_end);
            // remove all key with certain key value in alex
            int num_erase = alex::Alex<T, P>::erase_geo(del_start, envelope);
            // find the position of erase key
            std::vector<std::tuple<double, double, double, double>>::iterator erase_position;
#ifdef PIECE
            erase_position = std::upper_bound(pieces.begin(), pieces.end(), std::make_tuple(del_end, -1, -1, -1),
                                              sortbysec);
            double update_count = std::get<2>(pieces[erase_position - pieces.begin()]) - 1;
            double update_sum = (std::get<3>(pieces[erase_position - pieces.begin()])) -  del_start;
            std::get<2>(pieces[erase_position - pieces.begin()]) = update_count;
            std::get<3>(pieces[erase_position - pieces.begin()]) = update_sum;
#endif
            return num_erase;
        }
    };


}

