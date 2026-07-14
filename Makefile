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

# Install the binary to ~/.local/bin so the shell function in ~/.bashrc can find it.
# The shell function itself only needs to be added once (already in ~/.bashrc).
install: $(TARGET)
	cp $(TARGET) ~/.local/bin/subash-bin
	chmod +x ~/.local/bin/subash-bin
	@echo "Installed: ~/.local/bin/subash-bin"
	@echo "Run 'source ~/.bashrc' or open a new terminal to use 'subash'."

.PHONY: all clean install
