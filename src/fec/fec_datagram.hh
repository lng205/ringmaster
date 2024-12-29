#include <memory>

class FECDatagram{
public:
    FECDatagram(uint32_t group, uint8_t group_id, uint8_t group_size, uint8_t seed)
        : _group(group), _group_id(group_id), _group_size(group_size), _seed(seed) {}
private:
    uint32_t _group {};
    uint8_t _group_id {};
    uint8_t _group_size {};
    uint8_t _seed {};
};