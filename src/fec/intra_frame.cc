#include <cstring>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstdlib>

#include "intra_frame.hh"

#define SIZE_ALIGN 16

template <typename T>
T int_div_ceil(T a, T b) {
    return (a + b - 1) / b;
}

std::ostream& operator<<(std::ostream& os, FECType& fec_type) {
    switch (fec_type) {
        case FECType::DATA:
            os << "DATA";
            break;
        case FECType::REPAIR:
            os << "REPAIR";
            break;
    }
    return os;
}

IntraFrameFEC::IntraFrameFEC(int max_payload, float redundancy) :
_max_payload(max_payload), _redundancy(redundancy) {}

void IntraFrameFEC::_calc_fec_params(size_t size) {
    // We need to find k such that:
    // packet_data_size = ceil(size / k)
    // packet_total_size = packet_data_size + k (coeffs)
    // packet_total_size <= _max_payload
    
    // Initial guess: assume 0 overhead
    int k = static_cast<int>(int_div_ceil<size_t>(size, _max_payload));
    if (k == 0) k = 1;

    while (true) {
        size_t packet_data_size = int_div_ceil<size_t>(size, k);
        // align data size
        packet_data_size = int_div_ceil<size_t>(packet_data_size, SIZE_ALIGN) * SIZE_ALIGN;
        
        if (packet_data_size + k <= (size_t)_max_payload) {
             // Found a valid k
             info.k = k;
             info.size = packet_data_size; // This is the data size per packet
             break;
        }
        k++;
        // Safety break
        if (k > 500) {
            // Should not happen for reasonable frame sizes and MTU
            break;
        }
    }

    // Use probabilistic rounding to allow fractional redundancy
    double exact_m = info.k * _redundancy;
    info.m = static_cast<int>(exact_m);
    // Probabilistically add one more based on fractional part
    double frac = exact_m - info.m;
    if (frac > 0 && (rand() / (double)RAND_MAX) < frac) {
        info.m++;
    }
    info.w = 8;
}

vector<FECDatagram> IntraFrameFEC::encode(uint32_t frame_id, uint8_t* data, size_t size) {
    _calc_fec_params(size);
    
    // Buffer size needs to hold: Coeffs (k) + Data (info.size)
    size_t payload_len = info.k + info.size;

    vector<char*> data_buf = _get_buf(_data_buf, info.k, payload_len);
    vector<char*> coding_buf = _get_buf(_coding_buf, info.m, payload_len);
    
    // We also need a temporary buffer to hold just the DATA pointers for RLNC encoding
    // RLNC encoder expects pointers to pure data.
    // Wait, we can point into the payload (after coeffs).
    vector<char*> source_data_ptrs(info.k);

    // 1. Prepare Systematic Packets
    for (int i = 0; i < info.k; i++) {
        char* pkt = data_buf[i];
        
        // a. Set Coefficients (Identity Matrix)
        memset(pkt, 0, info.k);
        pkt[i] = 1;
        
        // b. Copy Data
        char* data_ptr = pkt + info.k;
        source_data_ptrs[i] = data_ptr; // Store for RLNC
        
        if (i < info.k - 1) {
            memcpy(data_ptr, data + i * info.size, info.size);
        } else {
            // Last packet
            int last_pkt_len = size - (info.k - 1) * info.size;
            memcpy(data_ptr, data + i * info.size, last_pkt_len);
            memset(data_ptr + last_pkt_len, 0, info.size - last_pkt_len);
        }
    }

    // 2. Prepare Coded (Repair) Packets
    RLNCCoder coder(info.k, info.m);
    for (int j = 0; j < info.m; j++) {
        char* pkt = coding_buf[j];
        uint8_t* coeffs = (uint8_t*)pkt;
        char* data_out = pkt + info.k;
        
        coder.encode_symbol(source_data_ptrs.data(), info.size, data_out, coeffs);
    }

    // 3. Create Datagrams
    // Note: We send the full payload (Coeffs + Data).
    // The decoder will need to know where to split. It knows 'k' from the header.
    // Wait, the header contains 'k'. So decoder knows 'k' bytes are coeffs.
    
    // Calculate padding for the last DATA packet to report correctly?
    // The 'padding' field in FECDatagram was used to strip 0s from the reconstructed frame.
    int last_pkt_len = size - (info.k - 1) * info.size;
    int padding = info.size - last_pkt_len;

    vector<FECDatagram> datagrams;
    datagrams.reserve(info.k + info.m);

    // Systematic
    for (int i = 0; i < info.k; i++) {
        // For the last packet, we might want to trim the payload if we were not using fixed size?
        // But here we use fixed size blocks. The 'padding' field handles the logic.
        // We send the full block.
        // Wait, current logic sends full block?
        // Old logic: "datagrams.emplace_back(..., string_view(data_buf[info.k - 1], last_pkt_len));" 
        // -> It sent SHORT packet for the last one?
        // If we send short packet, we must ensure Coeffs are still correct?
        // RLNC works on fixed block size. If we send partial data, we can't easily multiply.
        // Simpler to send padded full packet.
        // BUT, if we want to save bandwidth on the last packet?
        // If we truncate the last packet, the receiver needs to know it's truncated and pad it back before matrix ops.
        // Let's stick to full packets for simplicity in this RLNC implementation, 
        // OR respect the 'padding' logic.
        // If I change payload size, `FECDatagram` might be weird.
        // Let's send full packets. The overhead is small (padding).
        
        // Actually, let's look at old code:
        // `string_view(data_buf[info.k - 1], last_pkt_len)`
        // It sent truncated data. The decoder: `memset(..., 0, padding)`
        // This implies the transport handles variable length.
        
        // For RLNC, if we truncate the systematic packet, it's fine.
        // But for Coded packets, they are combinations of FULL packets.
        // So Coded packets must be full size.
        // Systematic packets can be short.
        // BUT, my `data_buf` layout is `Coeffs | Data`.
        // If I truncate, I must keep `Coeffs`.
        // So `string_view` should be `Coeffs` + `Truncated Data`.
        
        size_t this_data_len = (i == info.k - 1) ? last_pkt_len : info.size;
        datagrams.emplace_back(frame_id, FECType::DATA, i, info.k,
            padding, string_view(data_buf[i], info.k + this_data_len));
    }

    // Repair
    for (int j = 0; j < info.m; j++) {
        datagrams.emplace_back(frame_id, FECType::REPAIR, j, info.k,
            padding, string_view(coding_buf[j], info.k + info.size));
    }

    return datagrams;
}



vector<uint8_t> IntraFrameFEC::decode(const vector<optional<FECDatagram>>& datagrams) {
    // 1. Recover Info
    CodingInfo info;
    size_t padding = 0;
    bool found_info = false;
    
    for (auto& dgram : datagrams) {
        if (dgram) {
            info.k = dgram->frag_cnt;
            padding = dgram->padding;
            
            // Re-calculate packet size
            // payload = k + data_len
            // If data_len is truncated (last packet), we need to derive standard size.
            // But we might receive a Repair packet first which is full size.
            // Or a Data packet which is full size (unless it's the last one).
            
            if (dgram->fec_type == FECType::REPAIR) {
                 info.size = dgram->payload.size() - info.k;
            } else {
                // DATA
                 if (dgram->frag_id == info.k - 1) {
                     // This is the last one, it might be short.
                     // But we know 'padding'.
                     // size = payload_len - k + padding
                     info.size = (dgram->payload.size() - info.k) + padding;
                 } else {
                     info.size = dgram->payload.size() - info.k;
                 }
            }
            found_info = true;
            break;
        }
    }
    
    if (!found_info) return {};

    // 2. Collect Valid Packets
    vector<char*> inputs;
    vector<vector<uint8_t>> coeffs;
    
    // We need to store copies of data because we need to pad them if short
    // and `datagrams` is const.
    // Also we need to align them for RLNC.
    
    // Allocate buffer for decoding
    // We reuse _data_buf/coding_buf to store the INPUTS for the decoder?
    // No, `decode` produces `frame_buf`.
    // The decoder needs `inputs`.
    // Let's use `_coding_buf` as scratch space for inputs? 
    // `_coding_buf` has size `m`. We might need `k` inputs.
    // Let's just use a local vector of vectors for simplicity and safety.
    
    // Optimization: If we have all systematic packets, we don't need to run Gaussian Elimination!
    // We can just assemble.
    
    int systematic_count = 0;
    for (auto& dgram : datagrams) {
        if (dgram && dgram->fec_type == FECType::DATA) {
            systematic_count++;
        }
    }
    
    // Buffer for reconstructed frame
    // We allocate aligned size to safely allow RLNC decoder to write full blocks
    size_t aligned_frame_size = info.k * info.size;
    frame_size = aligned_frame_size - padding;
    vector<uint8_t> frame_buf(aligned_frame_size);
    
    // Pointers for RLNC output
    vector<char*> recovered_ptrs(info.k);
    // We can point directly into `frame_buf`!
    for(int i=0; i<info.k; ++i) {
        recovered_ptrs[i] = (char*)frame_buf.data() + i * info.size;
    }

    if (systematic_count == info.k) {
        // Fast path: All data packets received.
        for (auto& dgram : datagrams) {
            if (dgram && dgram->fec_type == FECType::DATA) {
                 size_t len = dgram->payload.size() - info.k; // skip coeffs
                 const char* data_ptr = dgram->payload.data() + info.k;
                 // Safety check
                 if (len > info.size) len = info.size;
                 memcpy(recovered_ptrs[dgram->frag_id], data_ptr, len);
            }
        }
        frame_buf.resize(frame_size);
        return frame_buf;
    }

    // Slow path: Need RLNC decoding
    vector<vector<uint8_t>> input_data_store; 
    
    for (auto& dgram : datagrams) {
        if (!dgram) continue;
        
        // Extract Coeffs
        if (dgram->payload.size() < info.k) continue; // Invalid
        
        vector<uint8_t> c(info.k);
        memcpy(c.data(), dgram->payload.data(), info.k);
        coeffs.push_back(c);
        
        // Extract Data
        const char* src_data = dgram->payload.data() + info.k;
        size_t src_len = dgram->payload.size() - info.k;
        
        // We need a full block for RLNC
        vector<uint8_t> block(info.size, 0);
        if (src_len > info.size) src_len = info.size;
        memcpy(block.data(), src_data, src_len); // Pads with 0 if short
        input_data_store.push_back(std::move(block));
        
        // Stop if we have k
        if (input_data_store.size() == info.k) break;
    }
    
    if (input_data_store.size() < info.k) {
        // Not enough packets
        return {}; 
    }
    
    // Prepare inputs pointers
    for(auto& v : input_data_store) {
        inputs.push_back((char*)v.data());
    }
    
    RLNCCoder coder(info.k, 0);
    if (coder.decode(inputs, coeffs, info.size, recovered_ptrs.data())) {
        frame_buf.resize(frame_size);
        return frame_buf;
    } else {
        return {}; // Failed (rank deficient)
    }
}

vector<char*> IntraFrameFEC::_get_buf(vector<string>& buf, size_t k, size_t size) {
    if (buf.size() < k || (buf.size() > 0 && buf[0].size() < size)) {
            buf.clear(); // Clear to force resize if size grew (or handle better)
            buf.resize(k, string(size, 0));
    }
    
    // Ensure all are size
    for(auto& s : buf) {
        if(s.size() < size) s.resize(size, 0);
    }

    vector<char*> ptrs;
    ptrs.reserve(k);
    for (int i = 0; i < k; ++i) {
        ptrs.push_back(buf[i].data());
    }
    return ptrs;
}
