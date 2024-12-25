CC = gcc
CFLAGS = -I/usr/include/jerasure/
LDFLAGS = -L/usr/lib/x86_64-linux-gnu/ -lJerasure
TARGET = jerasure

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LDFLAGS)