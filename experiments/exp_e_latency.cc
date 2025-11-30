// 实验E: 端到端延迟分解
// 分析FEC在总延迟中的占比（模拟视频编解码延迟）

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <random>
#include <cstring>
#include <iomanip>
#include "jerasure.hh"

using namespace std;
using namespace std::chrono;

const int MTU = 1200;
const int RUNS = 100;

// 模拟视频编码延迟（基于帧大小估算）
double simulate_video_encode(int frame_size) {
    // 典型VP9编码：约 10-50ms for 1080p
    // 简化模型：0.1ms/KB
    return frame_size / 1024.0 * 100;  // 返回 us
}

// 模拟视频解码延迟
double simulate_video_decode(int frame_size) {
    // 解码通常比编码快 2-5x
    return frame_size / 1024.0 * 30;  // 返回 us
}

int main() {
    // 测试不同分辨率场景
    vector<pair<string, int>> scenarios = {
        {"720p", 50 * 1024},
        {"1080p", 100 * 1024},
        {"4K", 500 * 1024}
    };
    
    cout << "实验E: 端到端延迟分解\n";
    cout << "======================\n\n";
    
    ofstream csv("exp_e_results.csv");
    csv << "scenario,frame_size,vp9_encode_us,fec_encode_us,fec_decode_us,vp9_decode_us,total_us,fec_ratio\n";
    
    random_device rd;
    mt19937 gen(rd());
    
    for (auto& [name, frame_size] : scenarios) {
        int k = (frame_size + MTU - 1) / MTU;
        int m = k * 50 / 100;  // 50%冗余
        
        // 调整packet_size避免k+m>256
        int packet_size = MTU;
        while (k + m > 250) {
            packet_size *= 2;
            k = (frame_size + packet_size - 1) / packet_size;
            m = k * 50 / 100;
        }
        
        CodingInfo info{(uint16_t)k, (uint16_t)m, 8, (size_t)packet_size};
        Jerasure fec(info);
        
        // 分配内存
        vector<char*> data(k), coding(m);
        for (int i = 0; i < k; i++) data[i] = new char[packet_size];
        for (int i = 0; i < m; i++) coding[i] = new char[packet_size];
        
        // 填充数据
        for (int i = 0; i < k; i++) {
            for (int j = 0; j < packet_size; j++) {
                data[i][j] = gen() & 0xFF;
            }
        }
        
        // 测量FEC编码
        double total_fec_encode = 0;
        for (int r = 0; r < RUNS; r++) {
            auto t1 = high_resolution_clock::now();
            fec.encode(data.data(), coding.data());
            auto t2 = high_resolution_clock::now();
            total_fec_encode += duration_cast<nanoseconds>(t2 - t1).count() / 1000.0;
        }
        double fec_encode_us = total_fec_encode / RUNS;
        
        // 测量FEC解码（有丢包）
        double total_fec_decode = 0;
        for (int r = 0; r < RUNS; r++) {
            // 先重新编码，确保coding数据正确
            fec.encode(data.data(), coding.data());
            
            // 保存原始数据
            int lost = max(1, m / 2);
            vector<vector<char>> backup(lost);
            for (int i = 0; i < lost; i++) {
                backup[i].assign(data[i], data[i] + packet_size);
                memset(data[i], 0, packet_size);  // 模拟丢失
            }
            
            vector<int> erasures;
            for (int i = 0; i < lost; i++) erasures.push_back(i);
            erasures.push_back(-1);
            
            auto t1 = high_resolution_clock::now();
            fec.decode(data.data(), coding.data(), erasures.data());
            auto t2 = high_resolution_clock::now();
            total_fec_decode += duration_cast<nanoseconds>(t2 - t1).count() / 1000.0;
            
            // 恢复数据供下次使用
            for (int i = 0; i < lost; i++) {
                memcpy(data[i], backup[i].data(), packet_size);
            }
        }
        double fec_decode_us = total_fec_decode / RUNS;
        
        // 模拟视频编解码
        double vp9_encode_us = simulate_video_encode(frame_size);
        double vp9_decode_us = simulate_video_decode(frame_size);
        
        double total_us = vp9_encode_us + fec_encode_us + fec_decode_us + vp9_decode_us;
        double fec_ratio = (fec_encode_us + fec_decode_us) / total_us * 100;
        
        cout << name << " (" << frame_size/1024 << "KB):\n";
        cout << "  VP9编码: " << fixed << setprecision(0) << vp9_encode_us << " us\n";
        cout << "  FEC编码: " << fec_encode_us << " us\n";
        cout << "  FEC解码: " << fec_decode_us << " us\n";
        cout << "  VP9解码: " << vp9_decode_us << " us\n";
        cout << "  总延迟:  " << total_us << " us (" << total_us/1000 << " ms)\n";
        cout << "  FEC占比: " << setprecision(1) << fec_ratio << "%\n\n";
        
        csv << name << "," << frame_size << ","
            << vp9_encode_us << "," << fec_encode_us << ","
            << fec_decode_us << "," << vp9_decode_us << ","
            << total_us << "," << fec_ratio << "\n";
        
        for (int i = 0; i < k; i++) delete[] data[i];
        for (int i = 0; i < m; i++) delete[] coding[i];
    }
    
    csv.close();
    cout << "结果已保存到 exp_e_results.csv\n";
    return 0;
}
