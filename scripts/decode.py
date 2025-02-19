import numpy as np

# padding a 0 before each data vector
z = np.zeros((16, 1), dtype=int)
zp = np.hstack((z, np.eye(16, dtype=int)))

M11 = np.hstack((zp, np.zeros((16, 17), dtype=int)))
M12 = np.hstack((np.zeros((16, 17), dtype=int), zp))
M1 = np.vstack((M11, M12))
print(M1.shape)
# x = np.ones((1, 32))
# y = np.dot(x, M)
# print(y)

# encode

# define the cyclic shift matrix
C = np.zeros((17, 17), dtype=int)
for i in range(17):
    C[i, (i+1) % 17] = 1
# C = np.linalg.matrix_power(C, 15)
C8_C9 = np.linalg.matrix_power(C, 8) + np.linalg.matrix_power(C, 9)
# print(C8_C9)

M21 = np.hstack((C8_C9, np.zeros((17, 17), dtype=int)))
M22 = np.hstack((np.zeros((17, 17), dtype=int), C8_C9))
M2 = np.vstack((M21, M22))
print(M2.shape)
# print(M2)

o = np.ones((1, 16), dtype=int)
op = np.vstack((o, np.eye(16, dtype=int)))
M31 = np.hstack((op, np.zeros((17, 16), dtype=int)))
M32 = np.hstack((np.zeros((17, 16), dtype=int), op))
M3 = np.vstack((M31, M32))
print(M3.shape)

M = np.dot(M1, np.dot(M2, M3))
print(M.shape)
print(M)