#include <cstring>
#include <iostream>
#include <cmath>

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

vector<FECDatagram> IntraFrameFEC::encode(uint32_t frame_id, uint8_t* data, size_t size) {
    Jerasure jerasure = _calc_fec_params(size);
    info = jerasure.get_info();

    vector<char*> data_buf = _get_buf(_data_buf, info.k, info.size);
    vector<char*> coding_buf = _get_buf(_coding_buf, info.m, info.size);

    for (int i = 0; i < info.k - 1; i++) {
        memcpy(data_buf[i], data + i * info.size, info.size);
    }

    // pad the last data packet
    int last_pkt_len = size - (info.k - 1) * info.size;
    int padding = info.size - last_pkt_len;
    memcpy(data_buf[info.k - 1], data + (info.k - 1) * info.size, last_pkt_len);
    memset(data_buf[info.k - 1] + last_pkt_len, 0, padding);

    jerasure.encode(data_buf.data(), coding_buf.data());

    // create datagrams
    vector<FECDatagram> datagrams;
    for (int i = 0; i < info.k - 1; i++) {
        datagrams.emplace_back(frame_id, FECType::DATA, i, info.k, info.m,
        padding, string_view(data_buf[i], info.size));
    }
    // unpad
    datagrams.emplace_back(frame_id, FECType::DATA, info.k - 1, info.k, info.m,
    padding, string_view(data_buf[info.k - 1], last_pkt_len));

    for (int j = 0; j < info.m; j++) {
        datagrams.emplace_back(frame_id, FECType::REPAIR, j, info.k, info.m,
        padding, string_view(coding_buf[j], info.size));
    }

    return datagrams;
}

vector<uint8_t> IntraFrameFEC::decode(const vector<optional<FECDatagram>>& datagrams) {
    CodingInfo info;
    size_t padding;
    for (auto& datagram : datagrams) {
        // extract the FEC parameters from the first datagram
        if (datagram) {
            uint16_t k = datagram->frag_cnt;
            uint16_t m = datagram->repair_cnt;
            padding = datagram->padding;
            bool is_padded = (
                datagram->fec_type == FECType::DATA &&
                datagram->frag_id == k - 1
            );
            int payload_size = datagram->payload.size();
            size_t size = is_padded ? payload_size + padding : payload_size;
            info = {k, m, 8, size};
            break;
        }
    }
    Jerasure jerasure = Jerasure(info);

    vector<char*> data_buf = _get_buf(_data_buf, info.k, info.size);
    vector<char*> coding_buf = _get_buf(_coding_buf, info.m, info.size);

    vector<int> erasures;
    for (int i = 0; i < datagrams.size(); i++) {
        if (!datagrams[i]) {
            erasures.push_back(i);
            continue;
        }
        if (datagrams[i]->fec_type == FECType::DATA) {
            memcpy(data_buf[datagrams[i]->frag_id], datagrams[i]->payload.data(), datagrams[i]->payload.size());
            if (datagrams[i]->frag_id == info.k - 1) {
                memset(data_buf[datagrams[i]->frag_id] + datagrams[i]->payload.size(), 0, padding);
            }
        } else {
            memcpy(coding_buf[datagrams[i]->frag_id], datagrams[i]->payload.data(), datagrams[i]->payload.size());
        }
    }
    erasures.push_back(-1);

    jerasure.decode(data_buf.data(), coding_buf.data(), erasures.data());

    frame_size = info.k * info.size - padding;
    vector<uint8_t> frame_buf(frame_size);
    for (int i = 0; i < info.k - 1; i++) {
        memcpy(frame_buf.data() + i * info.size, data_buf[i], info.size);
    }

    size_t last_pkt_len = frame_size - (info.k - 1) * info.size;
    memcpy(frame_buf.data() + (info.k - 1) * info.size, data_buf[info.k - 1], last_pkt_len);

    return frame_buf;
}

vector<char*> IntraFrameFEC::_get_buf(vector<string>& buf, size_t k, size_t size) {
    if (buf.empty() || buf.size() < k || buf[0].size() < size) {
            buf.resize(2 * k, string(2 * size, 0));
    }

    vector<char*> ptrs;
    for (auto& str : buf) {
        ptrs.push_back(str.data());
    }
    return ptrs;
}

Jerasure IntraFrameFEC::_calc_fec_params(size_t size) {
    uint8_t k = static_cast<int>(int_div_ceil<size_t>(size, _max_payload));
    size_t packet_raw_size = int_div_ceil<size_t>(size, k);
    size_t packet_size = int_div_ceil<size_t>(packet_raw_size, SIZE_ALIGN) * SIZE_ALIGN;

    uint8_t m = ceil(k * _redundancy);

    return Jerasure({k, m, 8, packet_size});
};