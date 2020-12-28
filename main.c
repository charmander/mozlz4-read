#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lz4.h>

#define BUF_CAPACITY_INITIAL ((size_t)4096)
#define READ_SIZE ((size_t)4096)

static uint8_t MOZLZ4_HEADER[8] = "mozLz40";

enum {
	MOZLZ4_SIZE_HEADER_SIZE = 4,
	MOZLZ4_LZ4_OFFSET = sizeof MOZLZ4_HEADER + MOZLZ4_SIZE_HEADER_SIZE,
};

struct buf {
	uint8_t* contents;
	size_t length;
	size_t capacity;
};

__attribute__ ((nonnull, warn_unused_result))
static bool buf_new(struct buf* const buf) {
	uint8_t* const new_contents = malloc(BUF_CAPACITY_INITIAL);

	if (new_contents == NULL) {
		return false;
	}

	buf->contents = new_contents;
	buf->length = 0;
	buf->capacity = BUF_CAPACITY_INITIAL;
	return true;
}

__attribute__ ((nonnull, warn_unused_result))
static bool buf_ensure_capacity(struct buf* const buf, size_t const capacity) {
	size_t new_capacity = buf->capacity;

	if (new_capacity >= capacity) {
		return true;
	}

	while (new_capacity < capacity) {
		if (new_capacity / 2 > SIZE_MAX - new_capacity) {
			return false;
		}

		new_capacity += new_capacity / 2;
	}

	uint8_t* const new_contents = realloc(buf->contents, new_capacity);

	if (new_contents == NULL) {
		return false;
	}

	buf->contents = new_contents;
	buf->capacity = new_capacity;

	return true;
}

static inline void buf_free(struct buf const buf) {
	free(buf.contents);
}

int main(void) {
	struct buf buf;

	if (!buf_new(&buf)) {
		fputs("Failed to allocate new buffer\n", stderr);
		return EXIT_FAILURE;
	}

	for (;;) {
		if (
			buf.length > SIZE_MAX - READ_SIZE
			|| !buf_ensure_capacity(&buf, buf.length + READ_SIZE)
		) {
			fprintf(stderr, "Failed to resize buffer to %zu + %zu bytes\n", buf.length, READ_SIZE);
			goto free_fail;
		}

		size_t const r = fread(&buf.contents[buf.length], sizeof(uint8_t), READ_SIZE, stdin);
		buf.length += r;

		if (r == READ_SIZE) {
			continue;
		}

		if (feof(stdin)) {
			break;
		}

		perror("Failed to read from stdin");
		goto free_fail;
	}

	if (buf.length < MOZLZ4_LZ4_OFFSET) {
		fprintf(stderr, "Input of %zu bytes is too small to contain size header\n", buf.length);
		goto free_fail;
	}

	if (memcmp(buf.contents, MOZLZ4_HEADER, sizeof MOZLZ4_HEADER) != 0) {
		fputs("Missing or invalid mozLz4 header\n", stderr);
		goto free_fail;
	}

	int header_size;

	{
		uint_least32_t const header_size_u =
			  (uint_least32_t)buf.contents[sizeof MOZLZ4_HEADER + 0]
			| (uint_least32_t)buf.contents[sizeof MOZLZ4_HEADER + 1] << 8
			| (uint_least32_t)buf.contents[sizeof MOZLZ4_HEADER + 2] << 16
			| (uint_least32_t)buf.contents[sizeof MOZLZ4_HEADER + 3] << 24;

		if (header_size_u > (uint_least32_t)INT_MAX) {
			fprintf(stderr, "Declared decompressed size of %" PRIuLEAST32 " bytes is too large\n", header_size_u);
			goto free_fail;
		}

		header_size = (int)header_size_u;
	}

	uint8_t* const decompressed = malloc((size_t)header_size);

	if (decompressed == NULL) {
		buf_free(buf);
		fprintf(stderr, "Failed to allocate buffer of size %d for decompressed data\n", header_size);
		return EXIT_FAILURE;
	}

	if (buf.length - MOZLZ4_LZ4_OFFSET > INT_MAX) {
		fprintf(stderr, "Input of %zu bytes is too large\n", buf.length);
		goto free_fail;
	}

	int const decompressed_count = LZ4_decompress_safe(
		(char const*)&buf.contents[MOZLZ4_LZ4_OFFSET],
		(char*)decompressed,
		(int)(buf.length - MOZLZ4_LZ4_OFFSET),
		header_size
	);

	buf_free(buf);

	if (decompressed_count < 0) {
		fprintf(stderr, "Decompression failed: error %d\n", -decompressed_count);
		return EXIT_FAILURE;
	}

	if (decompressed_count != header_size) {
		fprintf(stderr, "Warning: header declared capacity %d bytes, but decompression produced %d bytes\n", header_size, decompressed_count);
		return EXIT_FAILURE;
	}

	if (fwrite(decompressed, sizeof(uint8_t), (size_t)decompressed_count, stdout) != (size_t)decompressed_count) {
		perror("Failed to write output");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

free_fail:
	buf_free(buf);
	return EXIT_FAILURE;
}
