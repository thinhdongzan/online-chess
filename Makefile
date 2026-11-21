# =====================================================================
#  IT4062 - Thuc hanh Lap trinh mang
#  Project: Ung dung choi co vua truc tuyen (Online Chess)
#
#  Targets:
#    make            : Bien dich server va client vao thu muc bin/
#    make server     : Bien dich rieng server
#    make client     : Bien dich rieng client
#    make test       : Bien dich va chay toan bo Unit Test (Unity)
#    make coverage   : Chay Unit Test voi --coverage va thong ke gcov
#    make load-test  : Bien dich cong cu kiem thu chiu tai
#    make sanitize   : Bien dich voi AddressSanitizer (-fsanitize=address)
#    make clean      : Don dep toan bo tep bien dich va coverage
# =====================================================================

CC        ?= gcc
CFLAGS    ?= -std=gnu11 -Wall -Wextra -g
INCLUDES   = -Isrc -Itests/unity
LDLIBS    ?= -lsqlite3 -lm

BIN_DIR    = bin
BUILD_DIR  = build

COMMON_SRCS = $(wildcard src/common/*.c)
CHESS_SRCS  = $(wildcard src/chess/*.c)
SERVER_MODS = $(filter-out src/server/main.c, $(wildcard src/server/*.c))
CLIENT_MODS = $(filter-out src/client/main.c, $(wildcard src/client/*.c))

LIB_SRCS    = $(COMMON_SRCS) $(CHESS_SRCS) $(SERVER_MODS) $(CLIENT_MODS)
SERVER_SRCS = src/server/main.c $(COMMON_SRCS) $(CHESS_SRCS) $(SERVER_MODS)
CLIENT_SRCS = src/client/main.c $(COMMON_SRCS) $(CLIENT_MODS)

UNITY_SRC   = tests/unity/unity.c
TEST_SRCS   = $(sort $(wildcard tests/unit/test_*.c))
TEST_BINS   = $(patsubst tests/unit/%.c,$(BIN_DIR)/%,$(TEST_SRCS))

LOAD_SRC    = tests/load/load_test.c

.PHONY: all server client test coverage load-test sanitize clean

all: server client

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

server: $(BIN_DIR)/server

$(BIN_DIR)/server: $(SERVER_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(SERVER_SRCS) -o $@ $(LDLIBS)

client: $(BIN_DIR)/client

$(BIN_DIR)/client: $(CLIENT_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $(CLIENT_SRCS) -o $@ -lm

$(BIN_DIR)/test_%: tests/unit/test_%.c $(UNITY_SRC) $(LIB_SRCS) | $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) $< $(UNITY_SRC) $(LIB_SRCS) -o $@ $(LDLIBS)

test: $(TEST_BINS)
	@if [ -z "$(TEST_BINS)" ]; then \
		echo "No unit tests found in tests/unit/test_*.c"; \
	else \
		for t in $(TEST_BINS); do \
			echo "=== Running $$t ==="; \
			./$$t || exit 1; \
		done; \
	fi

coverage: CFLAGS += --coverage -O0
coverage: clean $(TEST_BINS)
	@if [ -z "$(TEST_BINS)" ]; then \
		echo "No unit tests found in tests/unit/test_*.c"; \
	else \
		for t in $(TEST_BINS); do \
			echo "=== Running $$t (with coverage) ==="; \
			./$$t || exit 1; \
		done; \
		gcov -b $(LIB_SRCS); \
	fi

load-test: $(BIN_DIR)
	@if [ -f "$(LOAD_SRC)" ]; then \
		$(CC) $(CFLAGS) $(INCLUDES) $(LOAD_SRC) $(COMMON_SRCS) -o $(BIN_DIR)/load_test -lm; \
	else \
		echo "Load test source $(LOAD_SRC) not implemented yet."; \
	fi

sanitize: CFLAGS += -fsanitize=address -fno-omit-frame-pointer
sanitize: clean all

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)
	rm -f *.gcno *.gcda *.gcov coverage.info
	rm -rf coverage_html
