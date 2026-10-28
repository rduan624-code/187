CC ?= cc
CFLAGS ?= -std=c99 -O1 -g -Wall -Wextra -Werror -pedantic
SANITIZERS ?= -fsanitize=address,undefined -fno-omit-frame-pointer
CORE = src/icd_upgrade.c src/upgrade_app.c
HEADERS = $(wildcard src/*upgrade*.h)
.PHONY: test clean
test: build/test_upgrade build/test_backend build/test_dispatch
	ASAN_OPTIONS=detect_leaks=0 ./build/test_upgrade
	ASAN_OPTIONS=detect_leaks=0 ./build/test_backend
	ASAN_OPTIONS=detect_leaks=0 ./build/test_dispatch
build/test_upgrade: $(CORE) tests/test_upgrade.c $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(SANITIZERS) -Isrc $(CORE) tests/test_upgrade.c -o $@
clean:
	rm -rf build

BACKENDS = src/upgrade_board.c
BSP_INCLUDE ?= ../sdk-review/bsp/RFAM_187_XVC_bsp/microblaze_0/include
.PHONY: check-bsp
build/test_backend: $(BACKENDS) tests/test_backend.c $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(SANITIZERS) -Isrc $(BACKENDS) tests/test_backend.c -o $@
# Host syntax/API check against supplied BSP headers; NOT a MicroBlaze binary build.
# UUID below is a compilation fixture, not a confirmed board configuration.
check-bsp:
	$(CC) -std=c99 -fsyntax-only -D__MICROBLAZE__ -DUP_BOARD_ENABLED=1 -DUP_BOARD_UUID=0x360A -Isrc -I$(BSP_INCLUDE) src/upgrade_udp_sdk.c $(BACKENDS) src/main_sdk.c src/platform.c src/platform_mb.c
	$(CC) -std=c99 -fsyntax-only -D__MICROBLAZE__ -Isrc -I$(BSP_INCLUDE) src/upgrade_udp_sdk.c src/main_sdk.c src/platform.c src/platform_mb.c

build/test_dispatch: src/upgrade_board.c tests/test_dispatch.c $(HEADERS)
	mkdir -p build
	$(CC) $(CFLAGS) $(SANITIZERS) -Isrc src/upgrade_board.c tests/test_dispatch.c -o $@
