CC := clang
CFLAGS := -std=c17 -Wall -Wextra -Weverything -pedantic -march=native
CFLAGS += $(shell pkgconf --cflags liblz4)
LDFLAGS := $(shell pkgconf --libs liblz4)

main: main.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $@
