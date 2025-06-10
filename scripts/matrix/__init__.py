from .vandermonde import Van
from .cyc_matrix import CyclicMatrix

class Matrix:
    def __init__(self, m, k, order):
        self.m = m
        self.k = k
        self.order = order
        self.vandermonde = Van(m, k, order)


    def get_encode_matrix(self):
        matrix = self.vandermonde.M
        cyclic = CyclicMatrix(self.order)
        cyclic_matrix = cyclic.filp_bits(matrix)
        return cyclic_matrix