#include "jerasure.hh"

class IntraFrameFEC {
public:
    IntraFrameFEC(int max_payload);
    ~IntraFrameFEC();
    void encode(uint8_t* data, size_t size);
    vector<string_view> serialize();
    CodingInfo info;
private:
    Jerasure _calc_fec_params(size_t size);
    void _check_buf(char** buf, size_t& buf_k, size_t& buf_size, size_t k, size_t size);
    int _max_payload;
    size_t _data_buf_k = 0, _data_buf_size = 0, _coding_buf_k = 0, _coding_buf_size = 0;
    char** _data_buf = nullptr, **_coding_buf = nullptr;
};