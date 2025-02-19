import numpy as np
import galois

# Verify the inverse of a matrix in GF(2^3)
GF = galois.GF(2**3)

print(GF.properties)

a = GF('x')
f5 = GF([[1, 1],
         [0, a**2]])

d5 = np.linalg.inv(f5)

# The inverse of f5 is [[1, a**5], [0, a**5]]
# REF: "NC BOOK" p. 70 - 71
d5_ = GF([[1, a**5],
          [0, a**5]])

print(d5)
print(d5_)
print(np.all(d5 == d5_))