/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 fnTech3 contributors
 */
#include "../code/qcommon/q_shared.h"
#include "../code/qcommon/unzip.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int allocations;
static int allocationBudget = -1;

void *S_Malloc(int bytes) {
	void *result;
	if (allocationBudget == 0) return NULL;
	if (allocationBudget > 0) --allocationBudget;
	result = calloc(1, bytes);
	if (result) ++allocations;
	return result;
}
void *Z_Malloc(int bytes) { return S_Malloc(bytes); }
void Z_Free(void *ptr) { if (ptr) { --allocations; free(ptr); } }
FILE *Sys_FOpen(const char *name, const char *mode) { return fopen(name, mode); }

static void Put16(FILE *file, unsigned value) {
	fputc(value & 255, file); fputc((value >> 8) & 255, file);
}
static void Put32(FILE *file, unsigned long value) {
	Put16(file, (unsigned)value); Put16(file, (unsigned)(value >> 16));
}

static void WriteArchive(const char *path, const byte *data, unsigned length, int method) {
	z_stream stream = {0};
	byte packed[200000];
	unsigned packedLength = length;
	unsigned crc = (unsigned)crc32(0, data, length);
	FILE *file;
	long directory, end;
	if (method == Z_DEFLATED) {
		assert(deflateInit2(&stream, 6, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) == Z_OK);
		stream.next_in = (Bytef *)data; stream.avail_in = length;
		stream.next_out = packed; stream.avail_out = sizeof(packed);
		assert(deflate(&stream, Z_FINISH) == Z_STREAM_END);
		packedLength = (unsigned)stream.total_out;
		assert(deflateEnd(&stream) == Z_OK);
	} else {
		memcpy(packed, data, length);
	}
	file = fopen(path, "wb"); assert(file);
	Put32(file, 0x04034b50); Put16(file, 20); Put16(file, 0); Put16(file, method);
	Put32(file, 0); Put32(file, crc); Put32(file, packedLength); Put32(file, length);
	Put16(file, 9); Put16(file, 0); fwrite("asset.bin", 1, 9, file);
	fwrite(packed, 1, packedLength, file);
	directory = ftell(file);
	Put32(file, 0x02014b50); Put16(file, 20); Put16(file, 20); Put16(file, 0); Put16(file, method);
	Put32(file, 0); Put32(file, crc); Put32(file, packedLength); Put32(file, length);
	Put16(file, 9); Put16(file, 0); Put16(file, 0); Put16(file, 0); Put16(file, 0);
	Put32(file, 0); Put32(file, 0); fwrite("asset.bin", 1, 9, file);
	end = ftell(file);
	Put32(file, 0x06054b50); Put16(file, 0); Put16(file, 0); Put16(file, 1); Put16(file, 1);
	Put32(file, end - directory); Put32(file, directory); Put16(file, 0);
	assert(fclose(file) == 0);
}

int main(int argc, char **argv) {
	byte data[163841], result[163842], packed[200000];
	unsigned i, state = 0xaff23119u;
	int method;
	assert(argc == 2);
	for (i = 0; i < sizeof(data); ++i) {
		state ^= state << 13; state ^= state >> 17; state ^= state << 5;
		data[i] = (byte)state;
	}
	for (method = 0; method <= Z_DEFLATED; method += Z_DEFLATED) {
		unzFile archive;
		unz_file_info info;
		int baseline, budget, total = 0, count;
		WriteArchive(argv[1], data, sizeof(data), method);
		archive = unzOpen(argv[1]); assert(archive);
		/* fnTech3 deliberately defers directory parsing until first selection. */
		assert(unzGoToFirstFile(archive) == UNZ_OK);
		assert(unzLocateFile(archive, "ASSET.BIN", 2) == UNZ_OK);
		assert(unzGetCurrentFileInfo(archive, &info, NULL, 0, NULL, 0, NULL, 0) == UNZ_OK);
		assert(info.uncompressed_size == sizeof(data));
		assert(info.crc == crc32(0, data, sizeof(data)));
		baseline = allocations;
		for (budget = 0; budget < (method ? 3 : 2); ++budget) {
			allocationBudget = budget;
			assert(unzOpenCurrentFile(archive) != UNZ_OK);
			assert(allocations == baseline);
			assert(unzReadCurrentFile(archive, result, 1) == UNZ_PARAMERROR);
		}
		allocationBudget = -1;
		assert(unzOpenCurrentFile(archive) == UNZ_OK);
		while ((count = unzReadCurrentFile(archive, result + total,
			(unsigned)(sizeof(data) - total > 317 ? 317 : sizeof(data) - total))) > 0) total += count;
		assert(count == 0 && total == sizeof(data));
		assert(!memcmp(result, data, sizeof(data)));
		assert(unztell(archive) == sizeof(data));
		assert(unzCloseCurrentFile(archive) == UNZ_OK && allocations == baseline);
		assert(unzClose(archive) == UNZ_OK && allocations == 0);
	}
	{
		uLongf packedLength = sizeof(packed);
		unsigned long outputLength;
		assert(compress2(packed, &packedLength, data, sizeof(data), 6) == Z_OK);
		outputLength = sizeof(result);
		assert(QZ_Uncompress(result, &outputLength, packed, packedLength) == Z_OK);
		assert(outputLength == sizeof(data) && !memcmp(result, data, sizeof(data)));
		outputLength = sizeof(data) - 1;
		assert(QZ_Uncompress(result, &outputLength, packed, packedLength) != Z_OK);
		for (i = 1; i <= 8; ++i) {
			outputLength = sizeof(result);
			assert(QZ_Uncompress(result, &outputLength, packed, packedLength - i) != Z_OK);
		}
		packed[packedLength - 1] ^= 1;
		outputLength = sizeof(result);
		assert(QZ_Uncompress(result, &outputLength, packed, packedLength) == Z_DATA_ERROR);
		assert(QZ_Uncompress(NULL, &outputLength, packed, packedLength) == Z_STREAM_ERROR);
		assert(allocations == 0);
	}
	assert(remove(argv[1]) == 0);
	return 0;
}
