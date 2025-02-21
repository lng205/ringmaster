from vandermonde import Van
from cyc_matrix import CyclicMatrix
from helper_matrix import HelperMatrix

import numpy as np
import galois

np.set_printoptions(threshold=np.inf, linewidth=np.inf)

GF = galois.GF(2)

L = 3

def test():
    m = Van(2, 4, L - 1)
    c = CyclicMatrix(L)

    # u3, u4, t6
    packets = [2, 3]

    encode = c.convert_matrix(m.M)
    print("The encoding matrix:")
    print(encode)
    print()

    e_alpha = m.M[:, packets]
    e = c.convert_matrix(e_alpha)

    d_alpha = m.invert(packets)
    d = c.convert_matrix(d_alpha)

    h = HelperMatrix(2, L)
    decode = GF(d) @ GF(h.op)
    print("The decoding matrix:")
    print(decode)
    print()

    res = GF(h.zp) @ GF(e) @ decode

    print("Zero padding + encoding + decoding:")
    print(res)

if __name__ == "__main__":
    test()