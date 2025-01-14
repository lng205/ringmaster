#include <optional>

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
    uint16_t repair_cnt;
    uint16_t padding;
    string_view payload;
};

class IntraFrameFEC {
public:
    IntraFrameFEC() {};
    IntraFrameFEC(int max_payload, float redundancy);
    vector<FECDatagram> encode(uint32_t frame_id, uint8_t* data, size_t size);
    vector<uint8_t> decode(const vector<std::optional<FECDatagram>>& datagrams);
    CodingInfo info;
    size_t frame_size {};

    // setters
    void set_max_payload(int max_payload) { _max_payload = max_payload; }
    void set_redundancy(float redundancy) { _redundancy = redundancy; }
private:
    Jerasure _calc_fec_params(size_t size);
    vector<char*> _get_buf(vector<string>& buf, size_t k, size_t size);
    vector<string> _data_buf;
    vector<string> _coding_buf;
    int _max_payload;
    float _redundancy;
    vector<uint8_t> _frame_buf {};
};