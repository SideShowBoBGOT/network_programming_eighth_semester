CC:=clang
CFLAGS:=-std=gnu11 -O0 -g3 -Weverything -Werror -Wno-declaration-after-statement -Wno-alloca -Wno-padded -Wno-unsafe-buffer-usage -Wno-gnu-statement-expression -Wno-vla -Wno-shadow -Wno-disabled-macro-expansion
BUILD_DIR:=$(CURDIR)/build

.PHONY: all clean client iterative_server

clean:
	-rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

client.c: protocol.h
client: $(BUILD_DIR)/client.o
iterative_server_utils.h: protocol.h
iterative_server.c: iterative_server_utils.h
iterative_server: $(BUILD_DIR)/iterative_server.o
parallel_server.c: iterative_server_utils.h
