CC:=clang
CFLAGS:=-std=gnu11 -O0 -g3 -Weverything -Werror -Wno-gnu-label-as-value -Wno-unused-parameter -Wno-declaration-after-statement -Wno-alloca -Wno-padded -Wno-unsafe-buffer-usage -Wno-gnu-statement-expression -Wno-vla -Wno-shadow
BUILD_DIR:=$(CURDIR)/build
CLIENT_BINARY:=


.PHONY: all

all: client iterative_server

$(BUILD_DIR):
	mkdir $@

$(BUILD_DIR)/client.o: client.c protocol.h | $(BUILD_DIR)
	$(CC) $< -o $@ $(CFLAGS)

iterative_server_utils.h: protocol.h

$(BUILD_DIR)/iterative_server.o: iterative_server.c iterative_server_utils.h | $(BUILD_DIR)
	$(CC) $< -o $@ $(CFLAGS)

parallel_server.c: iterative_server_utils.h

$(BUILD_DIR)/parallel_server.o: parallel_server.c | $(BUILD_DIR)
	$(CC) $< -o $@ $(CFLAGS)

.PHONY: client
client: $(BUILD_DIR)/client.o

.PHONY: iterative_server
iterative_server: $(BUILD_DIR)/iterative_server.o

.PHONY: parallel_server
parallel_server: $(BUILD_DIR)/parallel_server.o

.PHONY: clean
clean:
	-rm -rf $(BUILD_DIR)
