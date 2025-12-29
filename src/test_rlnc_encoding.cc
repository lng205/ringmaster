/**
 * RLNC编码性能测试
 * 测试不同分辨率和码率下的编码开销
 */

#include <iostream>
#include <chrono>
#include <vector>
#include <cstring>
#include <random>
#include <iomanip>

#include "fec/rlnc.hh"
#include "fec/intra_frame.hh"

using namespace std;
using namespace chrono;

// 视频参数配置
struct VideoConfig {
    string name;
    int width;
    int height;
    int bitrate_kbps;   // 码率 kbps
    int fps;
};

// 计算平均每帧字节数
size_t estimate_avg_frame_size(int bitrate_kbps, int fps) {
    return (bitrate_kbps * 1000 / 8) / fps;
}

// 模拟真实视频帧大小波动
// I帧间隔30帧，I帧约为平均的4倍，P帧在0.3-1.5倍之间波动
class FrameSizeSimulator {
public:
    FrameSizeSimulator(size_t avg_size, int gop = 30) 
        : avg_size_(avg_size), gop_(gop), frame_idx_(0), gen_(random_device{}()) {}
    
    size_t next_frame_size() {
        size_t size;
        if (frame_idx_ % gop_ == 0) {
            // I帧: 平均的3-5倍
            uniform_real_distribution<> dis(3.0, 5.0);
            size = static_cast<size_t>(avg_size_ * dis(gen_));
        } else {
            // P帧: 平均的0.3-1.2倍 (因为I帧占用了额外带宽)
            uniform_real_distribution<> dis(0.3, 1.2);
            size = static_cast<size_t>(avg_size_ * dis(gen_));
        }
        frame_idx_++;
        return max(size, (size_t)100);  // 最小100字节
    }
    
    void reset() { frame_idx_ = 0; }
    
private:
    size_t avg_size_;
    int gop_;
    int frame_idx_;
    mt19937 gen_;
};

// 测试单个配置的RLNC编码性能（带帧大小波动）
void test_rlnc_encoding(const VideoConfig& config, float redundancy, int num_frames = 100) {
    size_t avg_frame_size = estimate_avg_frame_size(config.bitrate_kbps, config.fps);
    
    // 帧大小模拟器
    FrameSizeSimulator simulator(avg_frame_size);
    
    // 预分配最大可能的缓冲区 (I帧可能是平均的5倍)
    size_t max_frame_size = avg_frame_size * 6;
    vector<uint8_t> frame_data(max_frame_size);
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);
    for (auto& byte : frame_data) {
        byte = dis(gen);
    }
    
    // 创建FEC编码器
    IntraFrameFEC fec(1200, redundancy);
    
    // 预热
    for (int i = 0; i < 10; i++) {
        size_t fs = simulator.next_frame_size();
        fec.encode(i, frame_data.data(), fs);
    }
    simulator.reset();
    
    // 正式测试
    double total_encode_time_us = 0;
    double min_time = numeric_limits<double>::max();
    double max_time = 0;
    size_t min_frame_size = numeric_limits<size_t>::max();
    size_t max_frame_size_actual = 0;
    size_t total_frame_size = 0;
    
    for (int i = 0; i < num_frames; i++) {
        size_t frame_size = simulator.next_frame_size();
        total_frame_size += frame_size;
        min_frame_size = min(min_frame_size, frame_size);
        max_frame_size_actual = max(max_frame_size_actual, frame_size);
        
        auto start = high_resolution_clock::now();
        auto datagrams = fec.encode(i, frame_data.data(), frame_size);
        auto end = high_resolution_clock::now();
        
        double elapsed_us = duration<double, micro>(end - start).count();
        total_encode_time_us += elapsed_us;
        min_time = min(min_time, elapsed_us);
        max_time = max(max_time, elapsed_us);
    }
    
    double avg_time_us = total_encode_time_us / num_frames;
    double avg_time_ms = avg_time_us / 1000.0;
    double frame_budget_ms = 1000.0 / config.fps;
    double overhead_pct = (avg_time_ms / frame_budget_ms) * 100;
    size_t actual_avg_size = total_frame_size / num_frames;
    
    // 输出结果
    cout << "| " << setw(12) << config.name 
         << " | " << setw(4) << config.width << "x" << left << setw(4) << config.height << right
         << " | " << setw(5) << config.bitrate_kbps << " kbps"
         << " | " << setw(3) << config.fps << " fps"
         << " | " << setw(6) << (min_frame_size/1024) << "-" << setw(4) << (max_frame_size_actual/1024) << "KB"
         << " | " << fixed << setprecision(2) << setw(8) << avg_time_us << " us"
         << " | " << setw(8) << min_time << " us"
         << " | " << setw(8) << max_time << " us"
         << " | " << setw(6) << overhead_pct << "%"
         << " |" << endl;
}

// 测试纯RLNC符号编码性能
void test_raw_rlnc_symbol_encoding() {
    cout << "\n========== 纯RLNC符号编码性能测试 ==========\n" << endl;
    cout << "测试不同k值和block_size下的单个符号编码时间\n" << endl;
    
    vector<int> k_values = {2, 4, 8, 16, 32, 64};
    vector<int> block_sizes = {500, 1000, 1200, 1400, 2000, 4000};
    
    cout << "| " << setw(6) << "k"
         << " | " << setw(10) << "block_size"
         << " | " << setw(12) << "平均耗时"
         << " | " << setw(12) << "最小耗时"
         << " | " << setw(12) << "最大耗时"
         << " | " << setw(12) << "吞吐量"
         << " |" << endl;
    cout << string(80, '-') << endl;
    
    for (int k : k_values) {
        for (int block_size : block_sizes) {
            RLNCCoder coder(k, 1);
            
            // 准备数据
            vector<vector<char>> data_blocks(k, vector<char>(block_size));
            vector<char*> data_ptrs(k);
            random_device rd;
            mt19937 gen(rd());
            uniform_int_distribution<> dis(0, 255);
            
            for (int i = 0; i < k; i++) {
                for (int j = 0; j < block_size; j++) {
                    data_blocks[i][j] = dis(gen);
                }
                data_ptrs[i] = data_blocks[i].data();
            }
            
            vector<char> output(block_size);
            vector<uint8_t> coeffs(k);
            
            // 预热
            for (int i = 0; i < 100; i++) {
                coder.encode_symbol(data_ptrs.data(), block_size, output.data(), coeffs.data());
            }
            
            // 正式测试
            int num_iters = 1000;
            double total_time_us = 0;
            double min_time = numeric_limits<double>::max();
            double max_time = 0;
            
            for (int i = 0; i < num_iters; i++) {
                auto start = high_resolution_clock::now();
                coder.encode_symbol(data_ptrs.data(), block_size, output.data(), coeffs.data());
                auto end = high_resolution_clock::now();
                
                double elapsed_us = duration<double, micro>(end - start).count();
                total_time_us += elapsed_us;
                min_time = min(min_time, elapsed_us);
                max_time = max(max_time, elapsed_us);
            }
            
            double avg_time_us = total_time_us / num_iters;
            // 吞吐量: k * block_size bytes / avg_time_us => MB/s
            double throughput_mbps = (k * block_size) / avg_time_us; // bytes/us = MB/s
            
            cout << "| " << setw(6) << k
                 << " | " << setw(10) << block_size
                 << " | " << fixed << setprecision(2) << setw(10) << avg_time_us << " us"
                 << " | " << setw(10) << min_time << " us"
                 << " | " << setw(10) << max_time << " us"
                 << " | " << setw(8) << throughput_mbps << " MB/s"
                 << " |" << endl;
        }
    }
}

// 测试不同冗余率的影响
void test_redundancy_impact() {
    cout << "\n========== 冗余率对编码性能的影响 ==========\n" << endl;
    cout << "固定配置: 1080p, 5000kbps, 30fps (带帧大小波动)\n" << endl;
    
    VideoConfig config = {"1080p", 1920, 1080, 5000, 30};
    vector<float> redundancies = {0.0f, 0.1f, 0.2f, 0.3f, 0.5f, 1.0f};
    
    cout << "| " << setw(12) << "冗余率"
         << " | " << setw(12) << "平均耗时"
         << " | " << setw(12) << "最小耗时"
         << " | " << setw(12) << "最大耗时"
         << " |" << endl;
    cout << string(60, '-') << endl;
    
    for (float redundancy : redundancies) {
        size_t avg_frame_size = estimate_avg_frame_size(config.bitrate_kbps, config.fps);
        FrameSizeSimulator simulator(avg_frame_size);
        
        size_t max_possible_size = avg_frame_size * 6;
        vector<uint8_t> frame_data(max_possible_size);
        random_device rd;
        mt19937 gen(rd());
        uniform_int_distribution<> dis(0, 255);
        for (auto& byte : frame_data) {
            byte = dis(gen);
        }
        
        IntraFrameFEC fec(1200, redundancy);
        
        // 预热
        for (int i = 0; i < 10; i++) {
            size_t fs = simulator.next_frame_size();
            fec.encode(i, frame_data.data(), fs);
        }
        simulator.reset();
        
        // 测试
        int num_frames = 100;
        double total_time_us = 0;
        double min_time = numeric_limits<double>::max();
        double max_time = 0;
        
        for (int i = 0; i < num_frames; i++) {
            size_t frame_size = simulator.next_frame_size();
            
            auto start = high_resolution_clock::now();
            auto datagrams = fec.encode(i, frame_data.data(), frame_size);
            auto end = high_resolution_clock::now();
            
            double elapsed_us = duration<double, micro>(end - start).count();
            total_time_us += elapsed_us;
            min_time = min(min_time, elapsed_us);
            max_time = max(max_time, elapsed_us);
        }
        
        double avg_time_us = total_time_us / num_frames;
        
        cout << "| " << setw(10) << (redundancy * 100) << " %"
             << " | " << fixed << setprecision(2) << setw(10) << avg_time_us << " us"
             << " | " << setw(10) << min_time << " us"
             << " | " << setw(10) << max_time << " us"
             << " |" << endl;
    }
}

int main() {
    cout << "\n========== RLNC编码性能测试 ==========\n" << endl;
    cout << "测试参数: 100帧/配置, 冗余率20%, MTU=1200\n" << endl;
    
    // 定义测试配置
    vector<VideoConfig> configs = {
        // 低分辨率
        {"360p低码率", 640, 360, 300, 30},
        {"360p中码率", 640, 360, 500, 30},
        {"360p高码率", 640, 360, 1000, 30},
        
        // 480p
        {"480p低码率", 854, 480, 500, 30},
        {"480p中码率", 854, 480, 1000, 30},
        {"480p高码率", 854, 480, 2000, 30},
        
        // 720p
        {"720p低码率", 1280, 720, 1000, 30},
        {"720p中码率", 1280, 720, 2500, 30},
        {"720p高码率", 1280, 720, 5000, 30},
        
        // 1080p
        {"1080p低码率", 1920, 1080, 2000, 30},
        {"1080p中码率", 1920, 1080, 5000, 30},
        {"1080p高码率", 1920, 1080, 10000, 30},
        
        // 4K (限制码率避免k值过大)
        {"4K低码率", 3840, 2160, 8000, 30},
        {"4K中码率", 3840, 2160, 15000, 30},
        
        // 不同帧率测试 (1080p)
        {"1080p@24fps", 1920, 1080, 5000, 24},
        {"1080p@30fps", 1920, 1080, 5000, 30},
        {"1080p@60fps", 1920, 1080, 8000, 60},
    };
    
    // 输出表头
    cout << "| " << setw(12) << "配置"
         << " | " << setw(10) << "分辨率"
         << " | " << setw(11) << "码率"
         << " | " << setw(6) << "帧率"
         << " | " << setw(12) << "帧大小范围"
         << " | " << setw(11) << "平均耗时"
         << " | " << setw(11) << "最小耗时"
         << " | " << setw(11) << "最大耗时"
         << " | " << setw(7) << "开销%"
         << " |" << endl;
    cout << string(125, '-') << endl;
    
    // 运行测试
    for (const auto& config : configs) {
        test_rlnc_encoding(config, 0.2f);
    }
    
    cout << "\n说明:" << endl;
    cout << "  - 帧大小模拟: I帧(每30帧)为平均的3-5倍, P帧为平均的0.3-1.2倍" << endl;
    cout << "  - 平均帧大小 = 码率 / (8 * fps)" << endl;
    cout << "  - 开销% = 平均编码时间 / 帧预算时间 * 100" << endl;
    cout << "  - 帧预算 = 1000ms / fps (例如30fps时为33.3ms)" << endl;
    
    // 测试纯RLNC符号编码
    test_raw_rlnc_symbol_encoding();
    
    // 测试冗余率影响
    test_redundancy_impact();
    
    return 0;
}
