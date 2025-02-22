from vandermonde import Van
from cyc_matrix import CyclicMatrix
from helper_matrix import HelperMatrix

import numpy as np
import galois

np.set_printoptions(threshold=np.inf, linewidth=np.inf)

GF = galois.GF(2)

def test(L):
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

def test_paper():
    # Example from the paper
    
    L = 5
    m = np.array([
        [1, 1, 1, 1],
        [0, 1, 2, 4]
    ])
    c = CyclicMatrix(L)
    h = HelperMatrix(2, L)
    
    t2 = [2, 3]

    e_alpha = m[:, t2]
    d_alpha = np.linalg.inv(galois.GF(2**4, irreducible_poly=[1]*5)(e_alpha))

    print(d_alpha)

if __name__ == "__main__":
    test(5)
    # test_paper()