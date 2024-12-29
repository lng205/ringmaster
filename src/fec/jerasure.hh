// C++ Wrapper for libjerasure
#include <vector>
#include <fec_datagram.hh>

extern "C" {
#include <jerasure.h>
}

class Jerasure {
public:
    Jerasure();
    FECDatagram encode(int k, int m, int size, std::vector<std::string>&& data);
};