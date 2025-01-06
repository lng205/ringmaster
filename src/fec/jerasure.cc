#include "jerasure.hh"

string Jerasure::encode(const string& data, int size) {
    string coding = string((k + m) * size, 0);
    char* data_blocks = const_cast<char*>(data.data());
    char* coding_blocks = coding.data();
    jerasure_matrix_encode(k, m, w, matrix, &data_blocks, &coding_blocks, size);
    return coding;
}