#include "rlnc.hh"
#include "galois.hh"
#include <cstdlib>
#include <cstring>
#include <algorithm>

RLNCCoder::RLNCCoder(int k, int m) : _k(k), _m(m) {}

void RLNCCoder::encode_symbol(char** data_ptrs, int block_size, char* output, uint8_t* coeffs_out) {
    Galois& gf = Galois::get_instance();

    // 1. Generate random coefficients
    // For systematic coding, this function might be called with specific coeffs, 
    // but here we assume this is for repair symbols.
    // Ensure at least one non-zero coefficient to avoid empty packets
    bool all_zero = true;
    while(all_zero) {
        for (int i = 0; i < _k; i++) {
            coeffs_out[i] = rand() % 256;
            if (coeffs_out[i] != 0) all_zero = false;
        }
    }

    // 2. Linear combination
    std::memset(output, 0, block_size);
    
    for (int i = 0; i < _k; i++) {
        uint8_t c = coeffs_out[i];
        if (c == 0) continue;

        uint8_t* src = (uint8_t*)data_ptrs[i];
        uint8_t* dst = (uint8_t*)output;

        for (int j = 0; j < block_size; j++) {
            dst[j] = gf.add(dst[j], gf.mul(src[j], c));
        }
    }
}

bool RLNCCoder::decode(const std::vector<char*>& inputs, const std::vector<std::vector<uint8_t>>& coeffs, int block_size, char** recovered_ptrs) {
    if (inputs.size() < (size_t)_k) return false;

    Galois& gf = Galois::get_instance();
    int rows = inputs.size();
    int cols = _k; // columns of the matrix (variables)

    // Build the augmented matrix [Coeffs | Data]
    // However, data is large. We perform row operations on Coeffs, and mirror them on Data.
    // Since we don't want to destroy inputs, we clone coeffs. 
    // Data is handled via pointers. We'll need to copy data to a workspace if we modify it.
    // Gaussian elimination modifies rows.
    
    // Structure to hold the matrix
    std::vector<std::vector<uint8_t>> matrix = coeffs; // Copy coefficients
    
    // We need a workspace for data because row operations modify it.
    // Ideally we would decode in-place if allowed, but `inputs` are const pointers potentially.
    // Let's allocate a temporary workspace for the data being processed.
    // But this is expensive (Memory Copy).
    // Optimization: Only copy the `k` rows we end up using for the pivot?
    // Let's try to use the first `k` rows if possible.
    
    // To keep it simple and correct:
    // 1. Allocate workspace for `k` rows of data.
    // 2. We need to select `k` linearly independent rows from `inputs`.
    
    // Better approach: Gaussian Elimination on the fly.
    // Use an array of `rows` index mapping.
    
    std::vector<std::vector<uint8_t>> tmp_data(rows); 
    // This is huge. Optimization: The caller usually expects `recovered_ptrs` to be filled.
    // We can assume `recovered_ptrs` points to valid buffers.
    
    // Let's modify the signature or assume we can copy `inputs` to `recovered_ptrs` initially?
    // No, `inputs` might be coded. `recovered_ptrs` needs decoded.
    
    // Standard Gaussian Elimination
    // We work on the matrix of coefficients.
    // Augment with Data? No, Data is `block_size`.
    
    // We need to store the data rows corresponding to `matrix` rows.
    // Since we are doing this in software, let's just copy the input data to std::vector<uint8_t> buffers
    // for the rows we are working on.
    
    // Wait, we only need `k` pivots.
    
    std::vector<std::vector<uint8_t>> working_data(rows);
    for(int i=0; i<rows; ++i) {
        working_data[i].resize(block_size);
        std::memcpy(working_data[i].data(), inputs[i], block_size);
    }
    
    int pivot_row = 0;
    for (int col = 0; col < cols && pivot_row < rows; col++) {
        // Find pivot
        int sel = -1;
        for (int i = pivot_row; i < rows; i++) {
            if (matrix[i][col] != 0) {
                sel = i;
                break;
            }
        }
        
        if (sel == -1) continue; // No pivot in this column
        
        // Swap rows
        std::swap(matrix[pivot_row], matrix[sel]);
        std::swap(working_data[pivot_row], working_data[sel]);
        
        // Normalize pivot row
        uint8_t pivot_val = matrix[pivot_row][col];
        uint8_t inv = gf.div(1, pivot_val);
        
        for (int j = col; j < cols; j++) {
            matrix[pivot_row][j] = gf.mul(matrix[pivot_row][j], inv);
        }
        
        // Apply to data
        for (int b = 0; b < block_size; b++) {
            working_data[pivot_row][b] = gf.mul(working_data[pivot_row][b], inv);
        }
        
        // Eliminate other rows
        for (int i = 0; i < rows; i++) {
            if (i != pivot_row && matrix[i][col] != 0) {
                uint8_t factor = matrix[i][col];
                for (int j = col; j < cols; j++) {
                    matrix[i][j] = gf.sub(matrix[i][j], gf.mul(matrix[pivot_row][j], factor));
                }
                // Apply to data
                for (int b = 0; b < block_size; b++) {
                    working_data[i][b] = gf.sub(working_data[i][b], gf.mul(working_data[pivot_row][b], factor));
                }
            }
        }
        
        pivot_row++;
    }
    
    // Check rank
    if (pivot_row < _k) return false;
    
    // Output
    for (int i = 0; i < _k; i++) {
        std::memcpy(recovered_ptrs[i], working_data[i].data(), block_size);
    }
    
    return true;
}
