#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>

class RLNCCoder {
public:
    RLNCCoder(int k, int m);

    // Generate a single random coded symbol
    // data_ptrs: array of k pointers to source data
    // block_size: size of each data block
    // output: buffer to write the coded block
    // coeffs_out: buffer to write the k coefficients used (must be size k)
    void encode_symbol(char** data_ptrs, int block_size, char* output, uint8_t* coeffs_out);

    // Decode to recover original source symbols
    // inputs: list of available packets (pointers to data)
    // coeffs: list of coefficient vectors corresponding to inputs (each vector size k)
    // block_size: size of data block
    // recovered_ptrs: array of k pointers to write recovered source data
    // returns true if successful (full rank), false otherwise
    bool decode(const std::vector<char*>& inputs, const std::vector<std::vector<uint8_t>>& coeffs, int block_size, char** recovered_ptrs);

    int k() const { return _k; }
    int m() const { return _m; }

private:
    int _k;
    int _m;
};
