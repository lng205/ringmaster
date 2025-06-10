"""
Generate and invert the systematic Vandermonde matrix in GF(2**k)
"""

import numpy as np
import galois

class Van:
    def __init__(self, m, k, order):
        """
        m: number of input symbols
        k: number of output symbols
        order: the order of the field
        """
        assert m < k
        assert m - k <= order # the field should be large enough

        self.m, self.k, self.order = m, k, order
        self.GF = galois.GF(2**order, irreducible_poly=[1]*(order + 1))
        self.a = self.GF('x')
        self.M = self._vander(m, k - m)

    def _vander(self, rows, cols):
        vander = np.zeros((rows, cols), dtype=int)
        for i in range(rows):
            for j in range(cols):
                vander[i, j] = self.a**(i*(j+1))

        # Concatenate the Vandermonde matrix with the identity matrix
        M = np.eye(rows, dtype=int)
        M = np.concatenate((M, vander), axis=1) # the ref example uses 1 - I
        return M

    def invert(self, cols: list) -> np.ndarray:
        """
        cols: the list of columns to invert
        """
        assert len(cols) == self.m
        assert all([0 <= x < self.k for x in cols])

        enc = self.M[:, cols]
        rev = np.linalg.inv(self.GF(enc))
        # convert from GF to int
        return np.array(rev, dtype=int)

if __name__ == "__main__":
    gen = Van(2, 4, 16)
    print(gen.M)
    print(gen.invert([1, 3]))