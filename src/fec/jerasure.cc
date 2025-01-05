#include "jerasure.hh"

Jerasure::Jerasure() {
}

vector<string> Jerasure::encode(int k, int m, int size, const vector<string>& data) {
    vector<string> coding(m);
    
    return jerasure_matrix_encode(k, m, 8, matrix, data, coding, size);
}