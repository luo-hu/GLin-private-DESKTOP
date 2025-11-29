#include "./../glin/glin.h"
#include <chrono>
#include <iostream>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <memory>
#include <algorithm>
#include <cmath>

// 🌊 可扩展的AREAWATER大规模数据测试
class ScalableAREAWaterTest {
public:
    struct ThroughputPoint {
        long timestamp_ms;
        long records_processed;
        double throughput;
    };

    struct Config {
        size_t target_records;
        size_t bulk_load_ratio;
        size_t batch_size;
        bool enable_streaming;
        bool enable_memory_optimization;
        size_t memory_cleanup_interval;

        Config(size_t records = 1000000) :
            target_records(records),
            bulk_load_ratio(20),
            batch_size(50000),
            enable_streaming(true),
            enable_memory_optimization(true),
            memory_cleanup_interval(100000) {}
    };

private:
    // 流式加载AREAWATER数据
    static std::vector<geos::geom::Geometry*> loadAREAWATERStreaming(
        const std::string& filepath,
        const Config& config,
        std::function<void(const std::vector<geos::geom::Geometry*>&)> batch_callback = nullptr) {

        std::cout << "📦 流式加载AREAWATER数据集: " << filepath << std::endl;
        std::cout << "   目标记录数: " << config.target_records << std::endl;

        auto factory = geos::geom::GeometryFactory::create();
        std::vector<geos::geom::Geometry*> all_geoms;

        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "❌ 无法打开AREAWATER数据文件" << std::endl;
            return all_geoms;
        }

        std::string line;
        std::vector<geos::geom::Geometry*> batch;
        batch.reserve(config.batch_size);
        size_t total_loaded = 0;

        if (!std::getline(file, line)) return all_geoms;

        auto start_time = std::chrono::high_resolution_clock::now();

        while (std::getline(file, line) && total_loaded < config.target_records) {
            std::istringstream iss(line);
            std::string field;
            std::vector<std::string> fields;
            while (std::getline(iss, field, ',')) fields.push_back(field);

            if (fields.size() >= 10) {
                try {
                    if (!fields[2].empty() && !fields[3].empty()) {
                        double x = std::stod(fields[2]);
                        double y = std::stod(fields[3]);

                        auto coords = new geos::geom::CoordinateArraySequence();
                        coords->add(geos::geom::Coordinate(x, y));
                        coords->add(geos::geom::Coordinate(x + 0.001, y));
                        coords->add(geos::geom::Coordinate(x + 0.001, y + 0.001));
                        coords->add(geos::geom::Coordinate(x, y + 0.001));
                        coords->add(geos::geom::Coordinate(x, y));

                        auto ring = factory->createLinearRing(coords);
                        auto polygon = factory->createPolygon(ring, nullptr);
                        batch.push_back(polygon);
                        total_loaded++;

                        if (batch.size() >= config.batch_size) {
                            if (batch_callback) batch_callback(batch);
                            
                            if (config.enable_streaming) {
                                batch.clear(); 
                            } else {
                                all_geoms.insert(all_geoms.end(), batch.begin(), batch.end());
                                batch.clear();
                            }
                        }

                        if (total_loaded % 100000 == 0) {
                            auto current_time = std::chrono::high_resolution_clock::now();
                            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                                current_time - start_time).count();
                            std::cout << "    已加载 " << total_loaded << " 个 (" << elapsed << "s)\r" << std::flush;
                        }
                    }
                } catch (...) { continue; }
            }
        }
        std::cout << std::endl;

        if (!batch.empty()) {
            if (batch_callback) batch_callback(batch);
            if (!config.enable_streaming) all_geoms.insert(all_geoms.end(), batch.begin(), batch.end());
        }

        file.close();
        std::cout << "✅ 加载完成: " << total_loaded << " 个对象" << std::endl;
        return all_geoms;
    }

    static void printMemoryUsage(const std::string& stage) {
        std::ifstream status_file("/proc/self/status");
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                std::cout << "  💾 " << stage << " 内存: " << line.substr(6) << std::endl;
                break;
            }
        }
    }

    static void batchInsert(alex::Glin<double, geos::geom::Geometry*>& glin,
                            const std::vector<geos::geom::Geometry*>& geoms,
                            size_t start_idx, size_t count,
                            const std::string& curve_type,
                            double cell_xmin, double cell_ymin,
                            double cell_x_intvl, double cell_y_intvl,
                            std::vector<std::tuple<double, double, double, double>>& pieces,
                            std::vector<ThroughputPoint>& throughput_data,
                            std::chrono::high_resolution_clock::time_point start_time,
                            size_t& global_inserted_count) {

        const size_t end_idx = start_idx + count;
        auto last_measure_time = std::chrono::high_resolution_clock::now();

        for (size_t i = start_idx; i < end_idx; i++) {
            auto* geom = geoms[i];
            const geos::geom::Envelope* env_internal = geom->getEnvelopeInternal();
            // 🛠️ [修复] 使用 auto_ptr 或直接传递，防止内存泄漏
            geos::geom::Envelope* env = new geos::geom::Envelope(*env_internal);

            double pieceLimit = 1000000.0;
            glin.glin_insert(std::make_tuple(geom, env), curve_type,
                             cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl,
                             pieceLimit, pieces);
            
            // 🛠️ [修复] 必须删除 Envelope，否则百万次插入会泄漏 ~64MB 内存
            delete env;

            global_inserted_count++;

            if (global_inserted_count % 5000 == 0) {
                auto current_time = std::chrono::high_resolution_clock::now();
                auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - last_measure_time).count();
                if (duration_ms == 0) duration_ms = 1;

                double current_throughput = (5000.0 * 1000.0) / duration_ms;
                auto global_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    current_time - start_time).count();

                throughput_data.push_back({global_elapsed_ms, static_cast<long>(global_inserted_count), current_throughput});

                std::cout << "    插入进度: " << global_inserted_count
                          << " | 吞吐量: " << std::fixed << std::setprecision(0)
                          << current_throughput << " ops/s" << "\r" << std::flush;
                last_measure_time = current_time;
            }
        }
    }

    static void runStreamingTest(const std::string& filepath, const Config& config) {
        std::cout << "\n🔄 启动流式处理..." << std::endl;

        alex::Glin<double, geos::geom::Geometry*> glin;
        glin.set_force_bloom_filter(false);
        glin.set_force_strategy(alex::Glin<double, geos::geom::Geometry*>::FilteringStrategy::CONSERVATIVE);

        std::string curve_type = "z";
        std::vector<std::tuple<double, double, double, double>> pieces;

        // 1. 扫描坐标
        double min_x = 1e9, min_y = 1e9, max_x = -1e9, max_y = -1e9;
        std::cout << "📊 扫描数据范围..." << std::endl;
        std::ifstream scan_file(filepath);
        if (scan_file.is_open()) {
            std::string line;
            size_t scan_count = 0;
            std::getline(scan_file, line);
            while (std::getline(scan_file, line) && scan_count < config.target_records) {
                scan_count++;
                if (scan_count % 50 != 0) continue; // 采样
                std::istringstream iss(line);
                std::string f; std::vector<std::string> fs;
                while (std::getline(iss, f, ',')) fs.push_back(f);
                if (fs.size() >= 10 && !fs[2].empty() && !fs[3].empty()) {
                    try {
                        double x = std::stod(fs[2]); double y = std::stod(fs[3]);
                        min_x = std::min(min_x, x); min_y = std::min(min_y, y);
                        max_x = std::max(max_x, x); max_y = std::max(max_y, y);
                    } catch (...) {}
                }
            }
            scan_file.close();
        }
        std::cout << "   X[" << min_x << "," << max_x << "] Y[" << min_y << "," << max_y << "]" << std::endl;

        // 🛠️ [关键修复] 设置极高分辨率的网格
        // 0.00001 度约为 1 米。对于360度的范围，这需要 3.6e7 个网格，完全在 32位整数范围内。
        // 这样可以确保即使是非常靠近的对象也有不同的 Z-value，避免键值冲突。
        double cell_x_intvl = 0.00001; 
        double cell_y_intvl = 0.00001;
        double cell_xmin = min_x - 1.0;
        double cell_ymin = min_y - 1.0;

        std::cout << "   网格配置: res=" << cell_x_intvl << " (约1米精度)" << std::endl;

        std::vector<ThroughputPoint> all_throughput_data;
        auto overall_start_time = std::chrono::high_resolution_clock::now();

        size_t bulk_load_count = config.target_records * config.bulk_load_ratio / 100;
        std::vector<geos::geom::Geometry*> bulk_data;
        bulk_data.reserve(bulk_load_count);

        bool is_bulk_loading_phase = true;
        size_t global_inserted_count = 0;

        auto batch_callback = [&](const std::vector<geos::geom::Geometry*>& batch) {
            size_t current_batch_idx = 0;

            if (is_bulk_loading_phase) {
                size_t needed = bulk_load_count - bulk_data.size();
                size_t to_take = std::min(batch.size(), needed);
                bulk_data.insert(bulk_data.end(), batch.begin(), batch.begin() + to_take);
                current_batch_idx += to_take;

                if (bulk_data.size() >= bulk_load_count) {
                    std::cout << "📦 执行批量加载: " << bulk_data.size() << " 个对象" << std::endl;
                    auto bs = std::chrono::high_resolution_clock::now();
                    glin.glin_bulk_load(bulk_data, 1000000.0, curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces);
                    auto be = std::chrono::high_resolution_clock::now();
                    std::cout << "   耗时: " << std::chrono::duration_cast<std::chrono::milliseconds>(be - bs).count() << "ms" << std::endl;
                    
                    bulk_data.clear();
                    is_bulk_loading_phase = false;
                    printMemoryUsage("批量加载后");
                    overall_start_time = std::chrono::high_resolution_clock::now(); // 重置计时
                }
            }

            if (!is_bulk_loading_phase && current_batch_idx < batch.size()) {
                batchInsert(glin, batch, current_batch_idx, batch.size() - current_batch_idx, 
                            curve_type, cell_xmin, cell_ymin, cell_x_intvl, cell_y_intvl, pieces,
                            all_throughput_data, overall_start_time, global_inserted_count);
                
                if (global_inserted_count % config.memory_cleanup_interval == 0) printMemoryUsage("检查点");
            }
        };

        loadAREAWATERStreaming(filepath, config, batch_callback);

        // 保存数据
        std::ofstream file("scalable_areawater_performance.csv");
        if (file.is_open()) {
            file << "ms,count,ops_sec\n";
            for (const auto& p : all_throughput_data) file << p.timestamp_ms << "," << p.records_processed << "," << p.throughput << "\n";
            file.close();
            std::cout << "\n📊 数据已保存" << std::endl;
        }
    }

public:
    static void runScalableTest(Config config) {
        try {
            if (config.enable_streaming) runStreamingTest("/mnt/hgfs/sharedFolder/AREAWATER.csv", config);
        } catch (const std::exception& e) { std::cerr << e.what() << std::endl; }
    }
};

int main(int argc, char* argv[]) {
    ScalableAREAWaterTest::Config config;
    if (argc > 1) config.target_records = std::stoull(argv[1]);
    ScalableAREAWaterTest::runScalableTest(config);
    return 0;
}