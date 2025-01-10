#include "jerasure.hh"

struct FECDatagram {
    uint32_t frame_id;
    uint16_t frag_id;
    uint16_t frag_cnt;
    uint16_t repair_id;
    uint16_t repair_cnt;
    string_view payload;
};


class IntraFrameFEC {
public:
    ~IntraFrameFEC();
    vector<FECDatagram> encode(uint32_t frame_id, uint8_t* data, size_t size, int max_payload);
    CodingInfo info;
private:
    Jerasure _calc_fec_params(size_t size, int max_payload);
    void _check_buf(char** buf, size_t& buf_k, size_t& buf_size, size_t k, size_t size);
    size_t _data_buf_k {}, _data_buf_size {}, _coding_buf_k {}, _coding_buf_size {};
    char** _data_buf = nullptr, **_coding_buf = nullptr;
};