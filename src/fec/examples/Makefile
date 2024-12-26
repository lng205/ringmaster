CC = gcc
CFLAGS = -I/usr/include/jerasure/
LDFLAGS = -L/usr/lib/x86_64-linux-gnu/ -lJerasure -lgf_complete
TARGET = jerasure

$(TARGET): $(TARGET).c
	$(CC) $(CFLAGS) -o $(TARGET) $(TARGET).c $(LDFLAGS)

clean:
	rm $(TARGET)