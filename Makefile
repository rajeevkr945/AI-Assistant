CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
LDFLAGS = -lnotcurses -lnotcurses-core
TARGET = subash-bin
SRCS = src/main.c src/models.c src/ipc.c src/ui.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
