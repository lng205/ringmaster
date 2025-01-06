// C++ Wrapper for libjerasure
#include <vector>
#include <string>

extern "C" {
#include <jerasure.h>
#include <cauchy.h>
}

using namespace std;

class Jerasure {
public:
    Jerasure(int k, int m) {set_code(k, m);}

    /*
    Parameters:
        data: vector of data blocks
        size: size of each block in bytes r(should be multiple of sizeof(long))
    Returns:
        vector of encoded blocks
    */
    string encode(const string& data, int size);

    void set_code(int k, int m) {
        this->k = k;
        this->m = m;
        matrix = cauchy_original_coding_matrix(k, m, w);
    }

private:
    int k, m, w = 8;
    int* matrix = NULL;
};