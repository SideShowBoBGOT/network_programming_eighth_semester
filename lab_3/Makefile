CC:=clang
CFLAGS:=-std=gnu11 -O0 -g3 -Weverything -Werror -Wno-unused-parameter -Wno-declaration-after-statement -Wno-alloca -Wno-padded -Wno-unsafe-buffer-usage -Wno-gnu-statement-expression -Wno-vla -Wno-shadow
BUILD_DIR:=$(CURDIR)/build
CLIENT_BINARY:=$(BUILD_DIR)/client.o
ITERATICE_SERVER_BINARY:=$(BUILD_DIR)/iterative_server.o


.PHONY: all

all: client iterative_server

$(BUILD_DIR):
	mkdir $@

$(CLIENT_BINARY): client.c protocol.h | $(BUILD_DIR)
	$(CC) $< -o $@ $(CFLAGS)

$(ITERATICE_SERVER_BINARY): iterative_server.c iterative_server_utils.h protocol.h | $(BUILD_DIR)
	$(CC) $< -o $@ $(CFLAGS)

.PHONY: client
client: $(CLIENT_BINARY)

.PHONY: iterative_server
iterative_server: $(ITERATICE_SERVER_BINARY)

.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR)
