// 实验D: RS编解码性能测试（模拟大帧）
// 测试不同帧大小下的FEC编解码时间和吞吐量

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <random>
#include <cstring>
#include "jerasure.hh"

using namespace std;
using namespace std::chrono;

const int MTU = 1200;
const int RUNS = 50;  // 每个配置测试次数

struct Result {
    int frame_size;
    int redundancy_pct;
    int k, m;
    double encode_us;
    double decode_no_loss_us;
    double decode_with_loss_us;
    double encode_throughput_mbps;
    double decode_throughput_mbps;
};

int main() {
    // 测试帧大小：10KB到1MB，覆盖CIF到4K
    vector<int> frame_sizes = {
        10 * 1024,      // 10KB - CIF编码帧
        50 * 1024,      // 50KB - 720p编码帧
        100 * 1024,     // 100KB - 1080p编码帧
        300 * 1024,     // 300KB - 2K编码帧
        500 * 1024,     // 500KB - 4K I帧
        1024 * 1024     // 1MB - 4K高质量帧
    };
    vector<int> redundancy_pcts = {25, 50};
    
    vector<Result> results;
    random_device rd;
    mt19937 gen(rd());
    
    cout << "实验D: RS编解码性能测试\n";
    cout << "========================\n\n";
    
    for (int frame_size : frame_sizes) {
        for (int redundancy_pct : redundancy_pcts) {
            // w=8时，k+m <= 256，需要动态调整packet_size
            int packet_size = MTU;
            int k = (frame_size + packet_size - 1) / packet_size;
            int m = k * redundancy_pct / 100;
            if (m < 1) m = 1;
            
            // 如果k+m超过256，增大packet_size
            while (k + m > 250) {
                packet_size *= 2;
                k = (frame_size + packet_size - 1) / packet_size;
                m = k * redundancy_pct / 100;
                if (m < 1) m = 1;
            }
            
            cout << "测试: 帧大小=" << frame_size/1024 << "KB, "
                 << "冗余=" << redundancy_pct << "%, "
                 << "k=" << k << ", m=" << m 
                 << ", pkt=" << packet_size << "B" << endl;
            
            CodingInfo info{(uint16_t)k, (uint16_t)m, 8, (size_t)packet_size};
            Jerasure fec(info);
            
            // 分配内存
            vector<char*> data(k), coding(m);
            for (int i = 0; i < k; i++) {
                data[i] = new char[packet_size];
            }
            for (int i = 0; i < m; i++) {
                coding[i] = new char[packet_size];
            }
            
            double total_encode_us = 0;
            double total_decode_no_loss_us = 0;
            double total_decode_with_loss_us = 0;
            
            for (int run = 0; run < RUNS; run++) {
                // 填充随机数据
                for (int i = 0; i < k; i++) {
                    for (int j = 0; j < packet_size; j++) {
                        data[i][j] = gen() & 0xFF;
                    }
                }
                
                // 保存原始数据副本
                vector<vector<char>> original(k);
                for (int i = 0; i < k; i++) {
                    original[i].assign(data[i], data[i] + packet_size);
                }
                
                // 测试编码时间
                auto t1 = high_resolution_clock::now();
                fec.encode(data.data(), coding.data());
                auto t2 = high_resolution_clock::now();
                total_encode_us += duration_cast<microseconds>(t2 - t1).count();
                
                // 测试无丢包解码时间
                vector<int> no_erasures = {-1};
                auto t3 = high_resolution_clock::now();
                fec.decode(data.data(), coding.data(), no_erasures.data());
                auto t4 = high_resolution_clock::now();
                total_decode_no_loss_us += duration_cast<microseconds>(t4 - t3).count();
                
                // 测试有丢包解码时间（丢失m/2个数据包）
                int lost = max(1, m / 2);
                vector<int> erasures;
                for (int i = 0; i < lost; i++) {
                    erasures.push_back(i);
                    memset(data[i], 0, packet_size);  // 模拟丢失
                }
                erasures.push_back(-1);
                
                auto t5 = high_resolution_clock::now();
                fec.decode(data.data(), coding.data(), erasures.data());
                auto t6 = high_resolution_clock::now();
                total_decode_with_loss_us += duration_cast<microseconds>(t6 - t5).count();
                
                // 恢复数据
                for (int i = 0; i < lost; i++) {
                    memcpy(data[i], original[i].data(), packet_size);
                }
            }
            
            // 计算平均值
            double avg_encode = total_encode_us / RUNS;
            double avg_decode_no_loss = total_decode_no_loss_us / RUNS;
            double avg_decode_with_loss = total_decode_with_loss_us / RUNS;
            
            // 计算吞吐量 (MB/s)
            double data_size_mb = (double)frame_size / (1024 * 1024);
            double encode_tp = data_size_mb / (avg_encode / 1e6);
            double decode_tp = data_size_mb / (avg_decode_with_loss / 1e6);
            
            results.push_back({
                frame_size, redundancy_pct, k, m,
                avg_encode, avg_decode_no_loss, avg_decode_with_loss,
                encode_tp, decode_tp
            });
            
            cout << "  编码: " << avg_encode << "us, "
                 << "解码(无丢包): " << avg_decode_no_loss << "us, "
                 << "解码(有丢包): " << avg_decode_with_loss << "us\n";
            cout << "  吞吐量: 编码=" << encode_tp << "MB/s, "
                 << "解码=" << decode_tp << "MB/s\n\n";
            
            // 清理
            for (int i = 0; i < k; i++) delete[] data[i];
            for (int i = 0; i < m; i++) delete[] coding[i];
        }
    }
    
    // 输出CSV
    ofstream csv("exp_d_results.csv");
    csv << "frame_size,redundancy_pct,k,m,encode_us,decode_no_loss_us,decode_with_loss_us,encode_mbps,decode_mbps\n";
    for (const auto& r : results) {
        csv << r.frame_size << "," << r.redundancy_pct << ","
            << r.k << "," << r.m << ","
            << r.encode_us << "," << r.decode_no_loss_us << "," << r.decode_with_loss_us << ","
            << r.encode_throughput_mbps << "," << r.decode_throughput_mbps << "\n";
    }
    csv.close();
    
    cout << "结果已保存到 exp_d_results.csv\n";
    return 0;
}
