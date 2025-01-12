#include <cstring>
#include <iostream>

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

IntraFrameFEC::~IntraFrameFEC() {
    if (_data_buf) {
        for (int i = 0; i < _data_buf_k; i++) {
            free(_data_buf[i]);
        }
        free(_data_buf);
    }
    if (_coding_buf) {
        for (int i = 0; i < _coding_buf_k; i++) {
            free(_coding_buf[i]);
        }
        free(_coding_buf);
    }
}

vector<FECDatagram> IntraFrameFEC::encode(uint32_t frame_id, uint8_t* data, size_t size, int max_payload) {
    Jerasure jerasure = _calc_fec_params(size, max_payload);
    info = jerasure.get_info();

    _check_buf(_data_buf, _data_buf_k, _data_buf_size, info.k, info.size);
    _check_buf(_coding_buf, _coding_buf_k, _coding_buf_size, info.m, info.size);

    for (int i = 0; i < info.k - 1; i++) {
        memcpy(_data_buf[i], data + i * info.size, info.size);
    }

    // pad the last data packet
    int last_pkt_len = size - (info.k - 1) * info.size;
    int padding = info.size - last_pkt_len;
    memcpy(_data_buf[info.k - 1], data + (info.k - 1) * info.size, last_pkt_len);
    memset(_data_buf[info.k - 1] + last_pkt_len, 0, padding);

    jerasure.encode(_data_buf, _coding_buf);

    // create datagrams
    vector<FECDatagram> datagrams;
    for (int i = 0; i < info.k - 1; i++) {
        datagrams.emplace_back(frame_id, FECType::DATA, i, info.k, padding, string_view(_data_buf[i], info.size));
    }
    // unpad
    datagrams.emplace_back(frame_id, FECType::DATA, info.k - 1, info.k, padding, string_view(_data_buf[info.k - 1], last_pkt_len));

    for (int j = 0; j < info.m; j++) {
        datagrams.emplace_back(frame_id, FECType::REPAIR, j, info.m, padding, string_view(_coding_buf[j], info.size));
    }

    return datagrams;
}

void IntraFrameFEC::_check_buf(char**& buf, size_t& buf_k, size_t& buf_size, size_t k, size_t size) {
    if (buf_k < k || buf_size < size) {
        // the buffer is not large enough
        if (buf) {
            for (int i = 0; i < buf_k; i++) {
                free(buf[i]);
            }
            free(buf);
        }
        // set the buffer size to twice the required size
        buf_k = buf_k < k ? 2 * k : buf_k;
        buf_size = buf_size < size ? 2 * size : buf_size;
        buf = (char**)malloc(buf_k * sizeof(char*));
        for (int i = 0; i < buf_k; i++) {
            buf[i] = (char*)malloc(buf_size);
        }
    }
}

Jerasure IntraFrameFEC::_calc_fec_params(size_t size, int max_payload) {
    uint8_t k = static_cast<int>(int_div_ceil<size_t>(size, max_payload));
    size_t packet_raw_size = int_div_ceil<size_t>(size, k);
    size_t packet_size = int_div_ceil<size_t>(packet_raw_size, SIZE_ALIGN) * SIZE_ALIGN;

    // 50% redundancy
    uint8_t m = k;

    return Jerasure({k, m, 8, packet_size});
};