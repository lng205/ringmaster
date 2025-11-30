/**
 * 实验A：FEC丢包恢复能力验证
 * 验证RS码在不同丢包数量下的恢复能力
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <cstring>
#include <optional>
#include <iomanip>

#include "intra_frame.hh"

using namespace std;

vector<uint8_t> generate_data(size_t size) {
    vector<uint8_t> data(size);
    for (size_t i = 0; i < size; i++) {
        data[i] = i % 256;
    }
    return data;
}

int main() {
    const size_t FRAME_SIZE = 10 * 1024;  // 10KB
    const int MAX_PAYLOAD = 1200;
    const int RUNS = 100;
    
    vector<float> redundancies = {0.25f, 0.5f, 0.75f};
    
    // CSV输出
    ofstream csv("exp_a_results.csv");
    csv << "redundancy,k,m,lost,success_rate" << endl;
    
    cout << "实验A: FEC丢包恢复能力验证" << endl;
    cout << "帧大小: " << FRAME_SIZE << " bytes" << endl;
    cout << string(50, '=') << endl;
    
    for (float redundancy : redundancies) {
        // 获取k, m参数
        auto test_data = generate_data(FRAME_SIZE);
        IntraFrameFEC fec_test(MAX_PAYLOAD, redundancy);
        auto encoded_test = fec_test.encode(0, test_data.data(), test_data.size());
        int k = fec_test.info.k;
        int m = fec_test.info.m;
        int total = k + m;
        
        cout << "\n冗余率: " << (int)(redundancy*100) << "% (k=" << k << ", m=" << m << ")" << endl;
        cout << "丢包数\t成功率" << endl;
        
        // 测试不同丢包数量
        for (int lost = 0; lost <= m + 2 && lost <= total; lost++) {
            int successes = 0;
            
            for (int run = 0; run < RUNS; run++) {
                auto data = generate_data(FRAME_SIZE);
                IntraFrameFEC fec(MAX_PAYLOAD, redundancy);
                auto encoded = fec.encode(run, data.data(), data.size());
                
                // 随机选择丢包位置
                vector<int> indices(encoded.size());
                for (size_t i = 0; i < indices.size(); i++) indices[i] = i;
                
                random_device rd;
                mt19937 gen(rd());
                for (int i = indices.size() - 1; i > 0; i--) {
                    uniform_int_distribution<int> dist(0, i);
                    swap(indices[i], indices[dist(gen)]);
                }
                
                // 模拟丢包
                vector<optional<FECDatagram>> datagrams;
                for (auto& dg : encoded) datagrams.push_back(dg);
                for (int i = 0; i < lost; i++) {
                    datagrams[indices[i]] = nullopt;
                }
                
                // 尝试解码
                try {
                    auto decoded = fec.decode(datagrams);
                    if (decoded.size() == data.size() &&
                        memcmp(decoded.data(), data.data(), data.size()) == 0) {
                        successes++;
                    }
                } catch (...) {}
            }
            
            double rate = 100.0 * successes / RUNS;
            cout << lost << "\t" << fixed << setprecision(0) << rate << "%" << endl;
            csv << redundancy << "," << k << "," << m << "," << lost << "," << rate << endl;
        }
    }
    
    cout << "\n结果已保存到 exp_a_results.csv" << endl;
    return 0;
}
