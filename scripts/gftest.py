import numpy as np
import galois

# GF(2**16)
GF = galois.GF(2**16)

print(GF.properties)

a = GF('x')
f5 = GF([[1, 1],
         [0, a**2]])

d5 = np.linalg.inv(f5)

# poly format 
d5_p = [[GF._print_poly(x) for x in row] for row in d5]
print(d5_p)

get_order = lambda x: int(GF([x]).log()[0]) if x != 0 else None
d5_o = [[get_order(x) for x in row] for row in d5]
print(d5_o)

f6 = GF([[1, 1],
         [a, a**2]])
d6 = np.linalg.inv(f6)

d6_p = [[GF._print_poly(x) for x in row] for row in d6]
print(d6_p)

d6_o = [[get_order(x) for x in row] for row in d6]
print(d6_o)