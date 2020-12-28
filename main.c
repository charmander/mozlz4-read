#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <lz4.h>

static uint8_t MOZLZ4_HEADER[8] = "mozLz40";

enum {
	MOZLZ4_SIZE_HEADER_SIZE = 4,
	MOZLZ4_LZ4_OFFSET = sizeof MOZLZ4_HEADER + MOZLZ4_SIZE_HEADER_SIZE,
};

static void show_usage(void) {
	fputs("Usage: mozlz4-read <file>\n", stderr);
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		show_usage();
		return EXIT_FAILURE;
	}

	int fd = open(argv[1], O_RDONLY);

	if (fd == -1) {
		perror("Failed to open file");
		return EXIT_FAILURE;
	}

	size_t mozlz4_size;

	{
		struct stat stat;

		if (fstat(fd, &stat) != 0) {
			perror("Failed to stat file");
			return EXIT_FAILURE;
		}

		mozlz4_size = (size_t)stat.st_size;
	}

	uint8_t* const mozlz4_file = mmap(NULL, mozlz4_size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (mozlz4_file == MAP_FAILED) {
		perror("Failed to map file");
		return EXIT_FAILURE;
	}

	if (posix_madvise(mozlz4_file, mozlz4_size, POSIX_MADV_SEQUENTIAL) != 0) {
		perror("Warning: madvise failed");
	}

	if (mozlz4_size < MOZLZ4_LZ4_OFFSET) {
		fprintf(stderr, "Input of %zu bytes is too small to contain size header\n", mozlz4_size);
		return EXIT_FAILURE;
	}

	if (memcmp(mozlz4_file, MOZLZ4_HEADER, sizeof MOZLZ4_HEADER) != 0) {
		fputs("Missing or invalid mozLz4 header\n", stderr);
		return EXIT_FAILURE;
	}

	int header_size;

	{
		uint_least32_t const header_size_u =
			  (uint_least32_t)mozlz4_file[sizeof MOZLZ4_HEADER + 0]
			| (uint_least32_t)mozlz4_file[sizeof MOZLZ4_HEADER + 1] << 8
			| (uint_least32_t)mozlz4_file[sizeof MOZLZ4_HEADER + 2] << 16
			| (uint_least32_t)mozlz4_file[sizeof MOZLZ4_HEADER + 3] << 24;

		if (header_size_u > (uint_least32_t)INT_MAX) {
			fprintf(stderr, "Declared decompressed size of %" PRIuLEAST32 " bytes is too large\n", header_size_u);
			return EXIT_FAILURE;
		}

		header_size = (int)header_size_u;
	}

	uint8_t* const decompressed = malloc((size_t)header_size);

	if (decompressed == NULL) {
		fprintf(stderr, "Failed to allocate buffer of size %d for decompressed data\n", header_size);
		return EXIT_FAILURE;
	}

	if (mozlz4_size - MOZLZ4_LZ4_OFFSET > INT_MAX) {
		fprintf(stderr, "Input of %zu bytes is too large\n", mozlz4_size);
		return EXIT_FAILURE;
	}

	int const decompressed_count = LZ4_decompress_safe(
		(char const*)&mozlz4_file[MOZLZ4_LZ4_OFFSET],
		(char*)decompressed,
		(int)(mozlz4_size - MOZLZ4_LZ4_OFFSET),
		header_size
	);

	if (munmap(mozlz4_file, mozlz4_size) != 0) {
		perror("Failed to unmap file");
		return EXIT_FAILURE;
	}

	if (close(fd) != 0) {
		perror("Failed to close file");
		return EXIT_FAILURE;
	}

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
}
