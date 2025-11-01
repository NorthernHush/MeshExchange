# obfusificator script make

CC=gcc
CFLAGS=-Wall -Wextra -O2 -std=c99
TARGET=obfuscator
SOURCES=obfus.c

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

clean:
	rm -f $(TARGET)

.PHONY: install clean