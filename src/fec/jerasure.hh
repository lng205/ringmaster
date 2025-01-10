// C++ Wrapper for libjerasure
#include <vector>
#include <string>

extern "C" {
#include <jerasure.h>
#include <cauchy.h>
}

using namespace std;

struct CodingInfo {
    uint16_t k, m, w;
    size_t size;
};

class Jerasure {
public:
    Jerasure(CodingInfo info);
    ~Jerasure() {free(matrix);}
    void encode(char** data, char** coding);
    void decode(char** data, char** coding, int* erasures);
    CodingInfo get_info() {return {k, m, w, size};}
private:
    uint16_t k, m, w;
    size_t size;
    int* matrix = NULL;
};