#include "jerasure.hh"

enum class FECType : uint8_t {
  DATA = 0,
  REPAIR = 1
};

std::ostream& operator<<(std::ostream& os, FECType& fec_type);

struct FECDatagram {
    uint32_t frame_id;
    FECType fec_type;
    uint16_t frag_id;
    uint16_t frag_cnt;
    uint16_t padding;
    string_view payload;
};


class IntraFrameFEC {
public:
    ~IntraFrameFEC();
    vector<FECDatagram> encode(uint32_t frame_id, uint8_t* data, size_t size, int max_payload);
    CodingInfo info;
private:
    Jerasure _calc_fec_params(size_t size, int max_payload);
    void _check_buf(char**& buf, size_t& buf_k, size_t& buf_size, size_t k, size_t size);
    size_t _data_buf_k {};
    size_t _data_buf_size {};
    size_t _coding_buf_k {};
    size_t _coding_buf_size {};
    char** _data_buf = nullptr;
    char** _coding_buf = nullptr;
};