"""
Converting a linear solution of a network code to a (L-1, L) cyclic shift solution
Using cyclic shift operations for encoding and use bitwise XOR for decoding
Export the converted operations to a file for further use
"""

import numpy as np

class CyclicMatrix:
    def __init__(self, L):
        self.L = L
        self.C = self._clc(L)
    
    def _clc(self, L):
        # Generate the cyclic matrix C_L in GF(2)
        C = np.zeros((L, L), dtype=int)
        for i in range(L):
            C[i, (i+1) % L] = 1
        return C

    def filp_bits(self, M: np.ndarray) -> np.ndarray:
        """
        Flip bits if the bits in the element is greater than half
        """
        for i in range(M.shape[0]):
            for j in range(M.shape[1]):
                if M[i, j].bit_count() > (self.L - 1) / 2:
                    M[i, j] = M[i, j] ^ (2**(self.L) - 1)
        return M

    def convert(self, m: int) -> np.ndarray:
        assert 0 <= m < 2**(self.L - 1)
        if m.bit_count() > (self.L - 1) / 2:
            m = m ^ (2**(self.L) - 1)
        
        assert m.bit_count() <= (self.L - 1) / 2

        res = np.zeros((self.L, self.L), dtype=int)
        i = 0
        while m > 0:
            if m % 2 == 1:
                res = np.bitwise_xor(res, np.linalg.matrix_power(self.C, i))
            m = m // 2
            i += 1
        return res

    def convert_matrix(self, M: np.ndarray) -> np.ndarray:
        # Convert each element to a cyclic shift matrix
        rows = [np.hstack([self.convert(x) for x in row]) for row in M]
        return np.vstack(rows)

if __name__ == "__main__":
    # Example usage
    L = 3
    cm = CyclicMatrix(L)
    C = cm.convert(3)
    print(C)
