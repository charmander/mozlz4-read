# defaults
CC = clang
CFLAGS = -std=c17 -march=native -mtune=native -O2 -Wl,-s,-z,now,-z,relro -pipe -fno-plt -D_FORTIFY_SOURCE=2
CFLAGS += -Wall -Wextra -Weverything -Wno-disabled-macro-expansion -pedantic

# program
CFLAGS += -D_POSIX_C_SOURCE=200112L

# liblz4
CFLAGS += $(shell pkgconf --cflags liblz4)
LDFLAGS += $(shell pkgconf --libs liblz4)

main: main.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
