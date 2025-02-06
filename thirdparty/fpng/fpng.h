// fpng.h - unlicense (see end of fpng.cpp)
#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <vector>

#ifndef FPNG_TRAIN_HUFFMAN_TABLES
	// Set to 1 when using the -t (training) option in fpng_test to generate new opaque/alpha Huffman tables for the single pass encoder.
	#define FPNG_TRAIN_HUFFMAN_TABLES (0)
#endif

namespace fpng
{
	// ---- Library initialization - call once to identify if the processor supports SSE.
	// Otherwise you'll only get scalar fallbacks.
	void fpng_init();

	// ---- Useful Utilities

	// Returns true if the CPU supports SSE 4.1, and SSE support wasn't disabled by setting FPNG_NO_SSE=1.
	// fpng_init() must have been called first, or it'll assert and return false.
	bool fpng_cpu_supports_sse41();

	// Fast CRC-32 SSE4.1+pclmul or a scalar fallback (slice by 4)
	const uint32_t FPNG_CRC32_INIT = 0;
	uint32_t fpng_crc32(const void* pData, size_t size, uint32_t prev_crc32 = FPNG_CRC32_INIT);

	// Fast Adler32 SSE4.1 Adler-32 with a scalar fallback.
	const uint32_t FPNG_ADLER32_INIT = 1;
	uint32_t fpng_adler32(const void* pData, size_t size, uint32_t adler = FPNG_ADLER32_INIT);

	// ---- Compression
	enum
	{
		// Enables computing custom Huffman tables for each file, instead of using the custom global tables. 
		// Results in roughly 6% smaller files on average, but compression is around 40% slower.
		FPNG_ENCODE_SLOWER = 1, 
		
		// Only use raw Deflate blocks (no compression at all). Intended for testing.
		FPNG_FORCE_UNCOMPRESSED = 2,
	};

	// Fast PNG encoding. The resulting file can be decoded either using a standard PNG decoder or by the fpng_decode_memory() function below.
	// pImage: pointer to RGB or RGBA image pixels, R first in memory, B/A last.
	// w/h - image dimensions. Image's row pitch in bytes must is w*num_chans.
	// num_chans must be 3 or 4. 
	bool fpng_encode_image_to_memory(const void* pImage, uint32_t w, uint32_t h, uint32_t num_chans, std::vector<uint8_t>& out_buf, uint32_t flags = 0);

#ifndef FPNG_NO_STDIO
	// Fast PNG encoding to the specified file.
	bool fpng_encode_image_to_file(const char* pFilename, const void* pImage, uint32_t w, uint32_t h, uint32_t num_chans, uint32_t flags = 0);
#endif

	// ---- Decompression
		
	enum
	{
		FPNG_DECODE_SUCCESS = 0,				// file is a valid PNG file and written by FPNG and the decode succeeded
		
		FPNG_DECODE_NOT_FPNG,					// file is a valid PNG file, but it wasn't written by FPNG so you should try decoding it with a general purpose PNG decoder

		FPNG_DECODE_INVALID_ARG,				// invalid function parameter

		FPNG_DECODE_FAILED_NOT_PNG,				// file cannot be a PNG file
		FPNG_DECODE_FAILED_HEADER_CRC32,		// a chunk CRC32 check failed, file is likely corrupted or not PNG
		FPNG_DECODE_FAILED_INVALID_DIMENSIONS,  // invalid image dimensions in IHDR chunk (0 or too large)
		FPNG_DECODE_FAILED_DIMENSIONS_TOO_LARGE, // decoding the file fully into memory would likely require too much memory (only on 32bpp builds)
		FPNG_DECODE_FAILED_CHUNK_PARSING,		// failed while parsing the chunk headers, or file is corrupted
		FPNG_DECODE_FAILED_INVALID_IDAT,		// IDAT data length is too small and cannot be valid, file is either corrupted or it's a bug

		// fpng_decode_file() specific errors
		FPNG_DECODE_FILE_OPEN_FAILED,
		FPNG_DECODE_FILE_TOO_LARGE,
		FPNG_DECODE_FILE_READ_FAILED,
		FPNG_DECODE_FILE_SEEK_FAILED
	};

	// Fast PNG decoding of files ONLY created by fpng_encode_image_to_memory() or fpng_encode_image_to_file().
	// If fpng_get_info() or fpng_decode_memory() returns FPNG_DECODE_NOT_FPNG, you should decode the PNG by falling back to a general purpose decoder.
	//
	// fpng_get_info() parses the PNG header and iterates through all chunks to determine if it's a file written by FPNG, but does not decompress the actual image data so it's relatively fast.
	// 
	// pImage, image_size: Pointer to PNG image data and its size
	// width, height: output image's dimensions
	// channels_in_file: will be 3 or 4
	// 
	// Returns FPNG_DECODE_SUCCESS on success, otherwise one of the failure codes above.
	// If FPNG_DECODE_NOT_FPNG is returned, you must decompress the file with a general purpose PNG decoder.
	// If another error occurs, the file is likely corrupted or invalid, but you can still try to decompress the file with another decoder (which will likely fail).
	int fpng_get_info(const void* pImage, uint32_t image_size, uint32_t& width, uint32_t& height, uint32_t& channels_in_file);

	// fpng_decode_memory() decompresses 24/32bpp PNG files ONLY encoded by this module.
	// If the image was written by FPNG, it will decompress the image data, otherwise it will return FPNG_DECODE_NOT_FPNG in which case you should fall back to a general purpose PNG decoder (lodepng, stb_image, libpng, etc.)
	//
	// pImage, image_size: Pointer to PNG image data and its size
	// out: Output 24/32bpp image buffer
	// width, height: output image's dimensions
	// channels_in_file: will be 3 or 4
	// desired_channels: must be 3 or 4 
	// 
	// If the image is 24bpp and 32bpp is requested, the alpha values will be set to 0xFF. 
	// If the image is 32bpp and 24bpp is requested, the alpha values will be discarded.
	// 
	// Returns FPNG_DECODE_SUCCESS on success, otherwise one of the failure codes above.
	// If FPNG_DECODE_NOT_FPNG is returned, you must decompress the file with a general purpose PNG decoder.
	// If another error occurs, the file is likely corrupted or invalid, but you can still try to decompress the file with another decoder (which will likely fail).
	int fpng_decode_memory(const void* pImage, uint32_t image_size, std::vector<uint8_t>& out, uint32_t& width, uint32_t& height, uint32_t& channels_in_file, uint32_t desired_channels);

#ifndef FPNG_NO_STDIO
	int fpng_decode_file(const char* pFilename, std::vector<uint8_t>& out, uint32_t& width, uint32_t& height, uint32_t& channels_in_file, uint32_t desired_channels);
#endif

	// ---- Internal API used for Huffman table training purposes

#if FPNG_TRAIN_HUFFMAN_TABLES
	const uint32_t HUFF_COUNTS_SIZE = 288;
	extern uint64_t g_huff_counts[HUFF_COUNTS_SIZE];
	bool create_dynamic_block_prefix(uint64_t* pFreq, uint32_t num_chans, std::vector<uint8_t>& prefix, uint64_t& bit_buf, int& bit_buf_size, uint32_t *pCodes, uint8_t *pCodesizes);
#endif

} // namespace fpng
