#include "jerasure.hh"

Jerasure::Jerasure(int k, int m, int size) : k(k), m(m), w(8), size(size) {
    matrix = cauchy_original_coding_matrix(k, m, w);
}

void Jerasure::encode(char** data, char** coding) {
    jerasure_matrix_encode(k, m, w, matrix, data, coding, size);
}

void Jerasure::decode(char** data, char** coding, int* erasures) {
    // row_k_ones ref: manual page 10
    jerasure_matrix_decode(k, m, w, matrix, 0, erasures, data, coding, size);
}