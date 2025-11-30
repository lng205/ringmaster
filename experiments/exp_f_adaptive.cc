/**
 * 实验F：自适应冗余率评估
 * 对比固定冗余率和自适应冗余率在变化丢包环境下的表现
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cstring>
#include <optional>
#include <iomanip>

#include "intra_frame.hh"
#include "adaptive_fec.hh"

using namespace std;

vector<uint8_t> generate_data(size_t size) {
    vector<uint8_t> data(size);
    for (size_t i = 0; i < size; i++) data[i] = i % 256;
    return data;
}

struct Result {
    float loss_rate;
    float redundancy;
    double recovery_rate;
    double bandwidth_overhead;
};

// 模拟一个帧的传输
Result simulate_frame(float loss_rate, float redundancy, int frame_size, int max_payload, mt19937& gen) {
    Result r;
    r.loss_rate = loss_rate;
    r.redundancy = redundancy;
    
    auto data = generate_data(frame_size);
    IntraFrameFEC fec(max_payload, redundancy);
    auto encoded = fec.encode(0, data.data(), data.size());
    
    int k = fec.info.k;
    int m = fec.info.m;
    int total = k + m;
    
    // 计算带宽开销
    size_t total_bytes = 0;
    for (auto& dg : encoded) total_bytes += dg.payload.size();
    r.bandwidth_overhead = (double)total_bytes / frame_size - 1.0;
    
    // 模拟丢包
    uniform_real_distribution<float> dist(0.0f, 1.0f);
    vector<optional<FECDatagram>> datagrams;
    for (auto& dg : encoded) {
        if (dist(gen) < loss_rate) {
            datagrams.push_back(nullopt);
        } else {
            datagrams.push_back(dg);
        }
    }
    
    // 尝试恢复
    try {
        auto decoded = fec.decode(datagrams);
        if (decoded.size() == data.size() &&
            memcmp(decoded.data(), data.data(), data.size()) == 0) {
            r.recovery_rate = 1.0;
        } else {
            r.recovery_rate = 0.0;
        }
    } catch (...) {
        r.recovery_rate = 0.0;
    }
    
    return r;
}

int main() {
    const int FRAME_SIZE = 10 * 1024;
    const int MAX_PAYLOAD = 1200;
    const int FRAMES_PER_CONFIG = 200;
    
    // 模拟变化的丢包率场景
    vector<float> loss_rates = {0.02f, 0.05f, 0.08f, 0.12f, 0.18f, 0.25f, 0.15f, 0.10f, 0.05f, 0.03f};
    
    random_device rd;
    mt19937 gen(rd());
    
    ofstream csv("exp_f_results.csv");
    csv << "scenario,loss_rate,method,redundancy,recovery_rate,bandwidth_overhead" << endl;
    
    cout << "实验F: 自适应冗余率评估" << endl;
    cout << string(60, '=') << endl;
    
    // 对每个丢包率场景测试
    for (size_t scenario = 0; scenario < loss_rates.size(); scenario++) {
        float loss_rate = loss_rates[scenario];
        
        cout << "\n场景 " << scenario + 1 << ": 丢包率 " << (int)(loss_rate * 100) << "%" << endl;
        
        // 方法1: 固定冗余率 50%
        double fixed_recovery = 0, fixed_overhead = 0;
        for (int i = 0; i < FRAMES_PER_CONFIG; i++) {
            auto r = simulate_frame(loss_rate, 0.5f, FRAME_SIZE, MAX_PAYLOAD, gen);
            fixed_recovery += r.recovery_rate;
            fixed_overhead += r.bandwidth_overhead;
        }
        fixed_recovery /= FRAMES_PER_CONFIG;
        fixed_overhead /= FRAMES_PER_CONFIG;
        
        csv << scenario << "," << loss_rate << ",fixed_50," << 0.5 << ","
            << fixed_recovery << "," << fixed_overhead << endl;
        
        // 方法2: 自适应冗余率
        float adaptive_redundancy = AdaptiveFEC::recommend(loss_rate);
        double adaptive_recovery = 0, adaptive_overhead = 0;
        for (int i = 0; i < FRAMES_PER_CONFIG; i++) {
            auto r = simulate_frame(loss_rate, adaptive_redundancy, FRAME_SIZE, MAX_PAYLOAD, gen);
            adaptive_recovery += r.recovery_rate;
            adaptive_overhead += r.bandwidth_overhead;
        }
        adaptive_recovery /= FRAMES_PER_CONFIG;
        adaptive_overhead /= FRAMES_PER_CONFIG;
        
        csv << scenario << "," << loss_rate << ",adaptive," << adaptive_redundancy << ","
            << adaptive_recovery << "," << adaptive_overhead << endl;
        
        cout << "  固定50%:  恢复率=" << fixed << setprecision(1) << fixed_recovery * 100 
             << "%, 开销=" << fixed_overhead * 100 << "%" << endl;
        cout << "  自适应" << (int)(adaptive_redundancy * 100) << "%: 恢复率=" 
             << adaptive_recovery * 100 << "%, 开销=" << adaptive_overhead * 100 << "%" << endl;
    }
    
    // 统计总体结果
    cout << "\n" << string(60, '=') << endl;
    cout << "结果已保存到 exp_f_results.csv" << endl;
    
    return 0;
}
