CC:=clang
CFLAGS:=-std=gnu11 -O0 -g3 -Weverything -Werror -Wno-declaration-after-statement -Wno-alloca -Wno-padded -Wno-unsafe-buffer-usage -Wno-gnu-statement-expression -Wno-vla -Wno-shadow -Wno-disabled-macro-expansion

BUILD_DIR:=$(CURDIR)/build

CFLAGS += -MMD -MP
-include $(BUILD_DIR)/*.d

.PHONY: all clean client async_server

all: client async_server

clean:
	-rm -rf $(BUILD_DIR)

$(BUILD_DIR):
	mkdir $@

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

client: $(BUILD_DIR)/client.o
async_server: $(BUILD_DIR)/async_server.o
