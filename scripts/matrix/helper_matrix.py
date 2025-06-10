"""
Generate the helper matrix for cyclic shift conversion
"""

import numpy as np

class HelperMatrix:
    def __init__(self, m, L):
        """
        m: number of input symbols
        L: the converted bit length of a symbol
        """
        self.m, self.L = m, L
        self.zp = np.kron(np.eye(self.m, dtype=int), self._h_zero_eye())
        self.op = np.kron(np.eye(self.m, dtype=int), self._v_one_eye())
    
    def _h_zero_eye(self):
        """
        Generate the matrix [0, I] for padding 0 before each data vector
        """
        z = np.zeros((self.L - 1, 1), dtype=int)
        return np.hstack((z, np.eye(self.L - 1, dtype=int)))
    
    def _v_one_eye(self):
        """
        Generate the matrix [1; I]
        """
        o = np.ones((1, self.L - 1), dtype=int)
        return np.vstack((o, np.eye(self.L - 1, dtype=int)))

if __name__ == "__main__":
    # Example usage
    hm = HelperMatrix(2, 3)
    print(hm.zp)
    print(hm.op)