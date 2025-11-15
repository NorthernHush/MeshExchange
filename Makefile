# Secure File Exchange System - Enhanced Makefile
# Supports: libevent, libsodium, ncurses, OpenSSL, MongoDB

CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -Iinclude
LDFLAGS =

# Dependencies
LIBSODIUM_CFLAGS = $(shell pkg-config --cflags libsodium 2>/dev/null || echo "")
LIBSODIUM_LDFLAGS = $(shell pkg-config --libs libsodium 2>/dev/null || echo "-lsodium")

LIBEVENT_CFLAGS = $(shell pkg-config --cflags libevent 2>/dev/null || echo "")
LIBEVENT_LDFLAGS = $(shell pkg-config --libs libevent_openssl 2>/dev/null || echo "-levent_openssl -levent")

NCURSES_CFLAGS = $(shell pkg-config --cflags ncurses 2>/dev/null || echo "")
NCURSES_LDFLAGS = $(shell pkg-config --libs ncurses panel 2>/dev/null || echo "-lncursesw -lpanelw")

OPENSSL_CFLAGS = $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
OPENSSL_LDFLAGS = $(shell pkg-config --libs openssl 2>/dev/null || echo "-lssl -lcrypto")

GLIB_CFLAGS = $(shell pkg-config --cflags glib-2.0 2>/dev/null || echo "")
GLIB_LDFLAGS = $(shell pkg-config --libs glib-2.0 2>/dev/null || echo "-lglib-2.0")

MONGOC_CFLAGS = $(shell pkg-config --cflags libmongoc-1.0 2>/dev/null || echo "")
MONGOC_LDFLAGS = $(shell pkg-config --libs libmongoc-1.0 2>/dev/null || echo "-lmongoc-1.0")

READLINE_CFLAGS = $(shell pkg-config --cflags readline 2>/dev/null || echo "")
READLINE_LDFLAGS = $(shell pkg-config --libs readline 2>/dev/null || echo "-lreadline")

# Combine flags
CFLAGS += $(LIBSODIUM_CFLAGS) $(LIBEVENT_CFLAGS) $(NCURSES_CFLAGS) $(OPENSSL_CFLAGS) $(GLIB_CFLAGS) $(MONGOC_CFLAGS) $(READLINE_CFLAGS)
LDFLAGS += $(LIBSODIUM_LDFLAGS) $(LIBEVENT_LDFLAGS) $(NCURSES_LDFLAGS) $(OPENSSL_LDFLAGS) $(GLIB_LDFLAGS) $(MONGOC_LDFLAGS) $(READLINE_LDFLAGS) -lpthread -lm

# Source files
CLIENT_SRC = src/client/client_new.c
SERVER_SRC = src/server/server_new.c src/db/mongo_ops_server.c src/server/admin_panel.c
CRYPTO_SRC = src/crypto/crypto_session.c
UTILS_SRC = src/utils/utils.c

# Object files
CLIENT_OBJ = $(CLIENT_SRC:.c=.o) $(CRYPTO_SRC:.c=.o) $(UTILS_SRC:.c=.o) blake3.o blake3_dispatch.o blake3_portable.o blake3_avx2.o blake3_sse2.o blake3_sse41.o
SERVER_OBJ = $(SERVER_SRC:.c=.o) $(CRYPTO_SRC:.c=.o) $(UTILS_SRC:.c=.o) blake3.o blake3_dispatch.o blake3_portable.o blake3_avx2.o blake3_sse2.o blake3_sse41.o

# Targets
all: client server

client: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o bin/client $^ $(LDFLAGS)

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o bin/server $^ $(LDFLAGS)

# Pattern rules
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
src/client/client_new.o: src/client/client_new.c
src/server/server_new.o: src/server/server_new.c
src/crypto/crypto_session.o: src/crypto/crypto_session.c
src/utils/utils.o: src/utils/utils.c

# Create directories
bin:
	mkdir -p bin

# Clean
clean:
	rm -f src/client/client_new.o src/crypto/crypto_session.o src/utils/utils.o src/server/server_new.o src/db/mongo_ops_server.o src/server/admin_panel.o bin/client bin/server

# Install dependencies (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libsodium-dev libevent-dev libncurses-dev libssl-dev libglib2.0-dev libmongoc-dev libreadline-dev pkg-config

# Install dependencies (CentOS/RHEL/Fedora)
install-deps-rpm:
	sudo yum install -y libsodium-devel libevent-devel ncurses-devel openssl-devel glib2-devel mongo-c-driver-devel readline-devel pkgconfig

# Install dependencies (macOS with Homebrew)
install-deps-brew:
	brew install libsodium libevent ncurses openssl glib mongo-c-driver readline pkg-config

# Test build
test-build: clean all
	@echo "Build test completed successfully"

# Run tests (placeholder)
test: test-build
	@echo "Running tests..."
	# Add actual test commands here

# Development targets
debug: CFLAGS += -g -DDEBUG
debug: clean all

release: CFLAGS += -O3 -DNDEBUG
release: clean all

# Static build (for deployment)
static: LDFLAGS += -static
static: clean all

# Cross-compilation example (ARM)
arm: CC = arm-linux-gnueabihf-gcc
arm: clean all

# Docker build
docker-build:
	docker build -t secure-file-exchange .

docker-run:
	docker run -it --rm secure-file-exchange

# Help
help:
	@echo "Secure File Exchange System - Build Targets:"
	@echo "  all           - Build client and server"
	@echo "  client        - Build client only"
	@echo "  server        - Build server only"
	@echo "  clean         - Clean build artifacts"
	@echo "  install-deps  - Install dependencies (Ubuntu/Debian)"
	@echo "  test-build    - Test build process"
	@echo "  debug         - Build with debug symbols"
	@echo "  release       - Build optimized release"
	@echo "  static        - Build static binaries"
	@echo "  docker-build  - Build Docker image"
	@echo "  docker-run    - Run in Docker container"
	@echo "  help          - Show this help"

.PHONY: all clean install-deps test-build debug release static docker-build docker-run help
