BUILD_DIR=$(CURDIR)/build
BINARY=$(BUILD_DIR)/main

.PHONY: all

all: $(BINARY)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BINARY): | $(BUILD_DIR)
	gcc main.c -Werror -Wall -Wextra -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)