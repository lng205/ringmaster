#include "galois.hh"
#include <iostream>

Galois& Galois::get_instance() {
    static Galois instance;
    return instance;
}

Galois::Galois() {
    // Initialize GF(2^8) with primitive polynomial x^8 + x^4 + x^3 + x^2 + 1 (0x11D)
    // This is the Rijndael polynomial.
    
    int i, j;
    // Log and Exp tables for generation
    uint8_t gf_exp[512];
    int gf_log[256];
    int x = 1;
    for (i = 0; i < 255; i++) {
        gf_exp[i] = x;
        gf_log[x] = i;
        x <<= 1;
        if (x & 0x100) x ^= 0x11D;
    }
    for (i = 255; i < 512; i++) gf_exp[i] = gf_exp[i - 255];
    gf_log[0] = 0; // Special case

    // Populate Mul/Div tables
    for (i = 0; i < 256; i++) {
        for (j = 0; j < 256; j++) {
            if (i == 0 || j == 0) {
                _mul_table[i][j] = 0;
            } else {
                int log_res = gf_log[i] + gf_log[j];
                _mul_table[i][j] = gf_exp[log_res % 255]; // Actually log_res can be > 255, but we use extended exp table or mod.
                // Since gf_exp is size 512, we can just use gf_exp[log_res] if log_res < 510.
                // max log sum is 254+254 = 508.
                _mul_table[i][j] = gf_exp[gf_log[i] + gf_log[j]];
            }
        }
    }

    for (i = 0; i < 256; i++) {
        for (j = 0; j < 256; j++) {
            if (i == 0) {
                _div_table[i][j] = 0;
            } else if (j == 0) {
                _div_table[i][j] = 0; // Division by zero! Handling as 0 or error? keeping 0 for now.
            } else {
                int log_diff = gf_log[i] - gf_log[j];
                if (log_diff < 0) log_diff += 255;
                _div_table[i][j] = gf_exp[log_diff];
            }
        }
    }
}

uint8_t Galois::add(uint8_t a, uint8_t b) {
    return a ^ b;
}

uint8_t Galois::sub(uint8_t a, uint8_t b) {
    return a ^ b;
}

uint8_t Galois::mul(uint8_t a, uint8_t b) {
    return _mul_table[a][b];
}

uint8_t Galois::div(uint8_t a, uint8_t b) {
    return _div_table[a][b];
}
