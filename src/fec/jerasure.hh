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
    Jerasure(int k, int m, int size);
    void encode(char** data, char** coding);
    void decode(char** data, char** coding, int* erasures);
private:
    int k, m, w, size;
    int* matrix = NULL;
};