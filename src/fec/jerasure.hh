// C++ Wrapper for libjerasure
#include <vector>
#include <fec_datagram.hh>

extern "C" {
#include <jerasure.h>
}

using namespace std;

class Jerasure {
public:
    Jerasure();
    /*
    Parameters:
        k: number of data blocks
        m: number of parity blocks
        size: size of each block in bytes r(should be multiple of sizeof(long))
        data: vector of data blocks
    Returns:
        vector of encoded blocks
    */
    vector<string> encode(int k, int m, int size, const vector<string>& data);
};