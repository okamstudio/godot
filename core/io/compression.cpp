/**************************************************************************/
/*  compression.cpp                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "compression.h"

#include "core/config/project_settings.h"
#include "core/io/zip_io.h"

#include "thirdparty/misc/fastlz.h"

#include <zlib.h>
#include <zstd.h>

#ifdef BROTLI_ENABLED
#include <brotli/decode.h>
#endif

int Compression::compress(uint8_t *p_dst, const uint8_t *p_src, int p_src_size, Mode p_mode) {
	switch (p_mode) {
		case MODE_BROTLI: {
			ERR_FAIL_V_MSG(-1, "Only brotli decompression is supported.");
		} break;
		case MODE_FASTLZ: {
			if (p_src_size < 16) {
				uint8_t src[16];
				memset(&src[p_src_size], 0, 16 - p_src_size);
				memcpy(src, p_src, p_src_size);
				return fastlz_compress(src, 16, p_dst);
			} else {
				return fastlz_compress(p_src, p_src_size, p_dst);
			}

		} break;
		case MODE_DEFLATE:
		case MODE_GZIP:
		case MODE_DEFLATE_RAW: {
			int window_bits;
			switch (p_mode) {
				case MODE_DEFLATE:
					window_bits = 15;
					break;
				case MODE_DEFLATE_RAW:
					window_bits = -15;
					break;
				default:
					window_bits = 31;
			}

			z_stream strm;
			strm.zalloc = zipio_alloc;
			strm.zfree = zipio_free;
			strm.opaque = Z_NULL;
			int level = p_mode == MODE_DEFLATE ? zlib_level : gzip_level;
			int err = deflateInit2(&strm, level, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);
			if (err != Z_OK) {
				return -1;
			}

			strm.avail_in = p_src_size;
			int aout = deflateBound(&strm, p_src_size);
			strm.avail_out = aout;
			strm.next_in = (Bytef *)p_src;
			strm.next_out = p_dst;
			deflate(&strm, Z_FINISH);
			aout = aout - strm.avail_out;
			deflateEnd(&strm);
			return aout;

		} break;
		case MODE_ZSTD: {
			ZSTD_CCtx *cctx = ZSTD_createCCtx();
			ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, zstd_level);
			if (zstd_long_distance_matching) {
				ZSTD_CCtx_setParameter(cctx, ZSTD_c_enableLongDistanceMatching, 1);
				ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, zstd_window_log_size);
			}
			int max_dst_size = get_max_compressed_buffer_size(p_src_size, MODE_ZSTD);
			int ret = ZSTD_compressCCtx(cctx, p_dst, max_dst_size, p_src, p_src_size, zstd_level);
			ZSTD_freeCCtx(cctx);
			return ret;
		} break;
	}

	ERR_FAIL_V(-1);
}

int Compression::get_max_compressed_buffer_size(int p_src_size, Mode p_mode) {
	switch (p_mode) {
		case MODE_BROTLI: {
			ERR_FAIL_V_MSG(-1, "Only brotli decompression is supported.");
		} break;
		case MODE_FASTLZ: {
			int ss = p_src_size + p_src_size * 6 / 100;
			if (ss < 66) {
				ss = 66;
			}
			return ss;

		} break;
		case MODE_DEFLATE:
		case MODE_GZIP:
		case MODE_DEFLATE_RAW: {
			int window_bits;
			switch (p_mode) {
				case MODE_DEFLATE:
					window_bits = 15;
					break;
				case MODE_DEFLATE_RAW:
					window_bits = -15;
					break;
				default:
					window_bits = 31;
			}

			z_stream strm;
			strm.zalloc = zipio_alloc;
			strm.zfree = zipio_free;
			strm.opaque = Z_NULL;
			int err = deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, window_bits, 8, Z_DEFAULT_STRATEGY);
			if (err != Z_OK) {
				return -1;
			}
			int aout = deflateBound(&strm, p_src_size);
			deflateEnd(&strm);
			return aout;
		} break;
		case MODE_ZSTD: {
			return ZSTD_compressBound(p_src_size);
		} break;
	}

	ERR_FAIL_V(-1);
}

int Compression::decompress(uint8_t *p_dst, int p_dst_max_size, const uint8_t *p_src, int p_src_size, Mode p_mode) {
	switch (p_mode) {
		case MODE_BROTLI: {
#ifdef BROTLI_ENABLED
			size_t ret_size = p_dst_max_size;
			BrotliDecoderResult res = BrotliDecoderDecompress(p_src_size, p_src, &ret_size, p_dst);
			ERR_FAIL_COND_V(res != BROTLI_DECODER_RESULT_SUCCESS, -1);
			return ret_size;
#else
			ERR_FAIL_V_MSG(-1, "Godot was compiled without brotli support.");
#endif
		} break;
		case MODE_FASTLZ: {
			int ret_size = 0;

			if (p_dst_max_size < 16) {
				uint8_t dst[16];
				fastlz_decompress(p_src, p_src_size, dst, 16);
				memcpy(p_dst, dst, p_dst_max_size);
				ret_size = p_dst_max_size;
			} else {
				ret_size = fastlz_decompress(p_src, p_src_size, p_dst, p_dst_max_size);
			}
			return ret_size;
		} break;
		case MODE_DEFLATE:
		case MODE_GZIP:
		case MODE_DEFLATE_RAW: {
			int window_bits;
			switch (p_mode) {
				case MODE_DEFLATE:
					window_bits = 15;
					break;
				case MODE_DEFLATE_RAW:
					window_bits = -15;
					break;
				default:
					window_bits = 31;
			}

			z_stream strm;
			strm.zalloc = zipio_alloc;
			strm.zfree = zipio_free;
			strm.opaque = Z_NULL;
			strm.avail_in = 0;
			strm.next_in = Z_NULL;
			int err = inflateInit2(&strm, window_bits);
			ERR_FAIL_COND_V(err != Z_OK, -1);

			strm.avail_in = p_src_size;
			strm.avail_out = p_dst_max_size;
			strm.next_in = (Bytef *)p_src;
			strm.next_out = p_dst;

			err = inflate(&strm, Z_FINISH);
			int total = strm.total_out;
			inflateEnd(&strm);
			ERR_FAIL_COND_V(err != Z_STREAM_END, -1);
			return total;
		} break;
		case MODE_ZSTD: {
			ZSTD_DCtx *dctx = ZSTD_createDCtx();
			if (zstd_long_distance_matching) {
				ZSTD_DCtx_setParameter(dctx, ZSTD_d_windowLogMax, zstd_window_log_size);
			}
			int ret = ZSTD_decompressDCtx(dctx, p_dst, p_dst_max_size, p_src, p_src_size);
			ZSTD_freeDCtx(dctx);
			return ret;
		} break;
	}

	ERR_FAIL_V(-1);
}

/**
	This will handle both Gzip and Deflate streams. It will automatically allocate the output buffer into the provided p_dst_vect Vector.
	This is required for compressed data whose final uncompressed size is unknown, as is the case for HTTP response bodies.
	This is much slower however than using Compression::decompress because it may result in multiple full copies of the output buffer.
*/
int Compression::decompress_dynamic(Vector<uint8_t> *p_dst_vect, int p_max_dst_size, const uint8_t *p_src, int p_src_size, Mode p_mode) {
	uint8_t *dst = nullptr;
	int out_mark = 0;

	ERR_FAIL_COND_V(p_src_size <= 0, Z_DATA_ERROR);

	if (p_mode == MODE_BROTLI) {
#ifdef BROTLI_ENABLED
		BrotliDecoderResult ret;
		BrotliDecoderState *state = BrotliDecoderCreateInstance(nullptr, nullptr, nullptr);
		ERR_FAIL_NULL_V(state, Z_DATA_ERROR);

		// Setup the stream inputs.
		const uint8_t *next_in = p_src;
		size_t avail_in = p_src_size;
		uint8_t *next_out = nullptr;
		size_t avail_out = 0;
		size_t total_out = 0;

		// Ensure the destination buffer is empty.
		p_dst_vect->clear();

		// Decompress until stream ends or end of file.
		do {
			// Add another chunk size to the output buffer.
			// This forces a copy of the whole buffer.
			p_dst_vect->resize(p_dst_vect->size() + gzip_chunk);
			// Get pointer to the actual output buffer.
			dst = p_dst_vect->ptrw();

			// Set the stream to the new output stream.
			// Since it was copied, we need to reset the stream to the new buffer.
			next_out = &(dst[out_mark]);
			avail_out += gzip_chunk;

			ret = BrotliDecoderDecompressStream(state, &avail_in, &next_in, &avail_out, &next_out, &total_out);
			if (ret == BROTLI_DECODER_RESULT_ERROR) {
				WARN_PRINT(BrotliDecoderErrorString(BrotliDecoderGetErrorCode(state)));
				BrotliDecoderDestroyInstance(state);
				p_dst_vect->clear();
				return Z_DATA_ERROR;
			}

			out_mark += gzip_chunk - avail_out;

			// Enforce max output size.
			if (p_max_dst_size > -1 && total_out > (uint64_t)p_max_dst_size) {
				BrotliDecoderDestroyInstance(state);
				p_dst_vect->clear();
				return Z_BUF_ERROR;
			}
		} while (ret != BROTLI_DECODER_RESULT_SUCCESS);

		// If all done successfully, resize the output if it's larger than the actual output.
		if ((unsigned long)p_dst_vect->size() > total_out) {
			p_dst_vect->resize(total_out);
		}

		// Clean up and return.
		BrotliDecoderDestroyInstance(state);
		return Z_OK;
#else
		ERR_FAIL_V_MSG(Z_ERRNO, "Godot was compiled without brotli support.");
#endif
	} else {
		// This function only supports GZip, Deflate and Deflate Raw.
		ERR_FAIL_COND_V(p_mode != MODE_DEFLATE && p_mode != MODE_GZIP && p_mode != MODE_DEFLATE_RAW,
				Z_ERRNO);

		int ret;
		z_stream strm;
		int window_bits;
		switch (p_mode) {
			case MODE_DEFLATE:
				window_bits = 15;
				break;
			case MODE_DEFLATE_RAW:
				window_bits = -15;
				break;
			default:
				window_bits = 31;
		}

		// Initialize the stream.
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		strm.avail_in = 0;
		strm.next_in = Z_NULL;

		int err = inflateInit2(&strm, window_bits);
		ERR_FAIL_COND_V(err != Z_OK, -1);

		// Setup the stream inputs.
		strm.next_in = (Bytef *)p_src;
		strm.avail_in = p_src_size;

		// Ensure the destination buffer is empty.
		p_dst_vect->clear();

		// Decompress until deflate stream ends or end of file.
		do {
			// Add another chunk size to the output buffer.
			// This forces a copy of the whole buffer.
			p_dst_vect->resize(p_dst_vect->size() + gzip_chunk);
			// Get pointer to the actual output buffer.
			dst = p_dst_vect->ptrw();

			// Set the stream to the new output stream.
			// Since it was copied, we need to reset the stream to the new buffer.
			strm.next_out = &(dst[out_mark]);
			strm.avail_out = gzip_chunk;

			// Run inflate() on input until output buffer is full and needs to be resized or input runs out.
			do {
				ret = inflate(&strm, Z_SYNC_FLUSH);

				switch (ret) {
					case Z_NEED_DICT:
						ret = Z_DATA_ERROR;
						[[fallthrough]];
					case Z_DATA_ERROR:
					case Z_MEM_ERROR:
					case Z_STREAM_ERROR:
					case Z_BUF_ERROR:
						if (strm.msg) {
							WARN_PRINT(strm.msg);
						}
						(void)inflateEnd(&strm);
						p_dst_vect->clear();
						return ret;
				}
			} while (strm.avail_out > 0 && strm.avail_in > 0);

			out_mark += gzip_chunk;

			// Enforce max output size.
			if (p_max_dst_size > -1 && strm.total_out > (uint64_t)p_max_dst_size) {
				(void)inflateEnd(&strm);
				p_dst_vect->clear();
				return Z_BUF_ERROR;
			}
		} while (ret != Z_STREAM_END);

		// If all done successfully, resize the output if it's larger than the actual output.
		if ((unsigned long)p_dst_vect->size() > strm.total_out) {
			p_dst_vect->resize(strm.total_out);
		}

		// Clean up and return.
		(void)inflateEnd(&strm);
		return Z_OK;
	}
}

int Compression::zlib_level = Z_DEFAULT_COMPRESSION;
int Compression::gzip_level = Z_DEFAULT_COMPRESSION;
int Compression::zstd_level = 3;
bool Compression::zstd_long_distance_matching = false;
int Compression::zstd_window_log_size = 27; // ZSTD_WINDOWLOG_LIMIT_DEFAULT
int Compression::gzip_chunk = 16384;
