#pragma once

#include <vector>
#include <cstdint>

class Galois {
public:
    static Galois& get_instance();

    uint8_t add(uint8_t a, uint8_t b);
    uint8_t sub(uint8_t a, uint8_t b);
    uint8_t mul(uint8_t a, uint8_t b);
    uint8_t div(uint8_t a, uint8_t b);

private:
    Galois();
    
    uint8_t _mul_table[256][256];
    uint8_t _div_table[256][256];
    // We could use log/exp tables, but full lookup tables for mul/div are fast and only 64KB each.
    // 256*256 bytes = 65KB. 
};
