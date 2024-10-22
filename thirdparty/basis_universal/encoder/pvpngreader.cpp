// pngreader.cpp - Public Domain - see unlicense at bottom of file.
//
// Notes: 
// This is ancient code from ~1995 ported to C++. It was originally written for a 
// DOS app with very limited memory. It's not as fast as it should be, but it works. 
// The low-level PNG reader class was written assuming the PNG file could not fit 
// entirely into memory, which dictated how it was written/structured.
// It has been modified to use either zlib or miniz.
// It supports all PNG color types/bit depths/interlacing, however 16-bit/component 
// images are converted to 8-bit.
// TRNS chunks are converted to alpha as needed.
// GAMA chunk is read, but not applied.

#include "../transcoder/basisu.h"

#define MINIZ_HEADER_FILE_ONLY
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES
#include "basisu_miniz.h"

#include "pvpngreader.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <assert.h>

#define PVPNG_IDAT_CRC_CHECKING (1)
#define PVPNG_ADLER32_CHECKING (1)

namespace pv_png
{

const uint32_t MIN_PNG_SIZE = 8 + 13 + 8 + 1 + 4 + 12;

template <typename S> inline S maximum(S a, S b) { return (a > b) ? a : b; }
template <typename S> inline S minimum(S a, S b) { return (a < b) ? a : b; }

template <typename T> inline void clear_obj(T& obj) { memset(&obj, 0, sizeof(obj)); }

#define MAX_SUPPORTED_RES (32768)
#define FALSE (0)
#define TRUE (1)
#define PNG_MAX_ALLOC_BLOCKS (16)

enum
{
	PNG_DECERROR = -3,
	PNG_ALLDONE = -5,
	PNG_READPASTEOF = -11,
	PNG_UNKNOWNTYPE = -16,
	PNG_FILEREADERROR = -17,
	PNG_NOTENOUGHMEM = -108,
	PNG_BAD_CHUNK_CRC32 = -13000,
	PNG_NO_IHDR = -13001,
	PNG_BAD_WIDTH = -13002,
	PNG_BAD_HEIGHT = -13003,
	PNG_UNS_COMPRESSION = -13004,
	PNG_UNS_FILTER = -13005,
	PNG_UNS_ILACE = -13006,
	PNG_UNS_COLOR_TYPE = -13007,
	PNG_BAD_BIT_DEPTH = -13008,
	PNG_BAD_CHUNK_SIZE = -13009,
	PNG_UNS_CRITICAL_CHUNK = -13010,
	PNG_BAD_TRNS_CHUNK = -13011,
	PNG_BAD_PLTE_CHUNK = -13012,
	PNG_UNS_RESOLUTION = -13013,
	PNG_INVALID_DATA_STREAM = -13014,
	PNG_MISSING_PALETTE = -13015,
	PNG_UNS_PREDICTOR = -13016,
	PNG_INCOMPLETE_IMAGE = -13017,
	PNG_TOO_MUCH_DATA = -13018
};

#define PNG_COLOR_TYPE_PAL_MASK        (1)
#define PNG_COLOR_TYPE_COL_MASK        (2)
#define PNG_COLOR_TYPE_ALP_MASK        (4)

#define PNG_INFLATE_SRC_BUF_SIZE       (4096)

struct ihdr_struct
{
	uint32_t m_width;
	uint32_t m_height;
	uint8_t m_bit_depth;
	uint8_t m_color_type;
	uint8_t m_comp_type;
	uint8_t m_filter_type;
	uint8_t m_ilace_type;
};

class png_file
{
public:
	png_file() { }
	virtual ~png_file() { }

	virtual bool resize(uint64_t new_size) = 0;
	virtual uint64_t get_size() = 0;
	virtual uint64_t tell() = 0;
	virtual bool seek(uint64_t ofs) = 0;
	virtual size_t write(const void* pBuf, size_t len) = 0;
	virtual size_t read(void* pBuf, size_t len) = 0;
};

class png_memory_file : public png_file
{
public:
	std::vector<uint8_t> m_buf;
	uint64_t m_ofs;
	
	png_memory_file() : 
		png_file(),
		m_ofs(0)
	{ 
	}
	
	virtual ~png_memory_file()
	{ 
	}

	std::vector<uint8_t>& get_buf() { return m_buf; }
	const std::vector<uint8_t>& get_buf() const { return m_buf; }
	 
	void init()
	{
		m_ofs = 0;
		m_buf.resize(0);
	}

	virtual bool resize(uint64_t new_size)
	{
		if ((sizeof(size_t) == sizeof(uint32_t)) && (new_size >= 0x7FFFFFFF))
			return false;

		m_buf.resize((size_t)new_size);
		m_ofs = m_buf.size();

		return true;
	}

	virtual uint64_t get_size()
	{
		return m_buf.size();
	}

	virtual uint64_t tell()
	{
		return m_ofs;
	}

	virtual bool seek(uint64_t ofs)
	{
		m_ofs = ofs;
		return true;
	}

	virtual size_t write(const void* pBuf, size_t len)
	{
		uint64_t new_size = m_ofs + len;
		if (new_size > m_buf.size())
		{
			if ((sizeof(size_t) == sizeof(uint32_t)) && (new_size > 0x7FFFFFFFUL))
				return 0;
			m_buf.resize((size_t)new_size);
		}

		memcpy(&m_buf[(size_t)m_ofs], pBuf, len);
		m_ofs += len;

		return len;
	}

	virtual size_t read(void* pBuf, size_t len)
	{
		if (m_ofs >= m_buf.size())
			return 0;

		uint64_t max_bytes = minimum<uint64_t>(len, m_buf.size() - m_ofs);
		memcpy(pBuf, &m_buf[(size_t)m_ofs], (size_t)max_bytes);

		m_ofs += max_bytes;

		return (size_t)max_bytes;
	}
};

class png_readonly_memory_file : public png_file
{
public:
	const uint8_t* m_pBuf;
	size_t m_buf_size;
	uint64_t m_ofs;

	png_readonly_memory_file() :
		png_file(),
		m_pBuf(nullptr),
		m_buf_size(0),
		m_ofs(0)
	{
	}

	virtual ~png_readonly_memory_file()
	{
	}

	void init(const void *pBuf, size_t buf_size)
	{
		m_pBuf = static_cast<const uint8_t*>(pBuf);
		m_buf_size = buf_size;
		m_ofs = 0;
	}

	virtual bool resize(uint64_t new_size)
	{
		(void)new_size;
		assert(0);
		return false;
	}

	virtual uint64_t get_size()
	{
		return m_buf_size;
	}

	virtual uint64_t tell()
	{
		return m_ofs;
	}

	virtual bool seek(uint64_t ofs)
	{
		m_ofs = ofs;
		return true;
	}

	virtual size_t write(const void* pBuf, size_t len)
	{
		(void)pBuf;
		(void)len;
		assert(0);
		return 0;
	}

	virtual size_t read(void* pBuf, size_t len)
	{
		if (m_ofs >= m_buf_size)
			return 0;

		uint64_t max_bytes = minimum<uint64_t>(len, m_buf_size - m_ofs);
		memcpy(pBuf, &m_pBuf[(size_t)m_ofs], (size_t)max_bytes);

		m_ofs += max_bytes;

		return (size_t)max_bytes;
	}
};

#ifdef _MSC_VER
#define ftell64 _ftelli64
#define fseek64 _fseeki64
#else
#define ftell64 ftello
#define fseek64 fseeko
#endif

class png_cfile : public png_file
{
public:
	FILE* m_pFile;
	
	png_cfile() : 
		png_file(),
		m_pFile(nullptr)
	{
	}

	virtual ~png_cfile()
	{
		close();
	}

	bool init(const char *pFilename, const char *pMode)
	{
		close();
		
		m_pFile = nullptr;
		
#ifdef _MSC_VER
		fopen_s(&m_pFile, pFilename, pMode);
#else
		m_pFile = fopen(pFilename, pMode);
#endif

		return m_pFile != nullptr;
	}

	bool close()
	{
		bool status = true;
		if (m_pFile)
		{
			if (fclose(m_pFile) == EOF)
				status = false;
			m_pFile = nullptr;
		}
		return status;
	}

	virtual bool resize(uint64_t new_size)
	{
		if (new_size)
		{
			if (!seek(new_size - 1))
				return false;

			int v = 0;
			if (write(&v, 1) != 1)
				return false;
		}
		else
		{
			if (!seek(0))
				return false;
		}

		return true;
	}

	virtual uint64_t get_size()
	{
		int64_t cur_ofs = ftell64(m_pFile);
		if (cur_ofs < 0)
			return 0;
		
		if (fseek64(m_pFile, 0, SEEK_END) != 0)
			return 0;
		
		const int64_t cur_size = ftell64(m_pFile);
		if (cur_size < 0)
			return 0;

		if (fseek64(m_pFile, cur_ofs, SEEK_SET) != 0)
			return 0;
		
		return cur_size;
	}

	virtual uint64_t tell()
	{
		int64_t cur_ofs = ftell64(m_pFile);
		if (cur_ofs < 0)
			return 0;

		return cur_ofs;
	}

	virtual bool seek(uint64_t ofs)
	{
		return fseek64(m_pFile, ofs, SEEK_SET) == 0;
	}

	virtual size_t write(const void* pBuf, size_t len)
	{
		return (size_t)fwrite(pBuf, 1, len, m_pFile);
	}

	virtual size_t read(void* pBuf, size_t len)
	{
		return (size_t)fread(pBuf, 1, len, m_pFile);
	}
};

// This low-level helper class handles the actual decoding of PNG files.
class png_decoder
{
public:
	png_decoder();
	~png_decoder();

	// Scans the PNG file, but doesn't decode the IDAT data. 
	// Returns 0 on success, or an error code.
	// If the returned status is non-zero, or m_img_supported_flag==FALSE the image either the image is corrupted/not PNG or is unsupported in some way.
	int png_scan(png_file *pFile);

	// Decodes a single scanline of PNG image data.
	// Returns a pointer to the scanline's pixel data and its size in bytes. 
	// This data is only minimally processed from the internal PNG pixel data.
	// The caller must use the ihdr, trns_flag and values, and the palette to actually decode the pixel data.
	//
	// Possible returned pixel formats is somewhat complex due to the history of this code:
	// 8-bit RGBA, always 4 bytes/pixel - 24bpp PNG's are converted to 32bpp and TRNS processing is done automatically (8/16bpp RGB or RGBA PNG files)
	// 1/2/4/8-bit grayscale, 1 byte per pixel - must convert to [0,255] using the palette or some other means, must optionally use the TRNS chunk for alpha (1/2/4/8 Grayscale PNG files - not 16bpp though!)
	// 1/2/4/8-bit palettized, 1 byte per pixel - must convert to RGB using the 24bpp palette and optionally the TRNS chunk for alpha (1/2/4/8bpp palettized PNG files)
	// 8-bit grayscale with alpha, 2 bytes per pixel - TRNS processing will be done for you on 16bpp images (there's a special case here for 16bpp Grey files) (8/16bpp Gray-Alpha *or 16bpp Grayscale* PNG files)
	//
	// Returns 0 on success, a non-zero error code, or PNG_ALLDONE.
	int png_decode(void** ppImg_ptr, uint32_t* pImg_len);
	
	// Starts decoding. Returns 0 on success, otherwise an error code.
	int png_decode_start();
	
	// Deinitializes the decoder, freeing all allocations.
	void png_decode_end();

	png_file* m_pFile;
		
	// Image's 24bpp palette - 3 bytes per entry
	uint8_t m_plte_flag;
	uint8_t m_img_pal[768];
		
	int m_img_supported_flag;
		
	ihdr_struct m_ihdr;

	uint8_t m_chunk_flag;
	uint32_t m_chunk_size;
	uint32_t m_chunk_left;
	uint32_t m_chunk_crc32;
	uint8_t m_chunk_name[4];

	uint8_t m_end_of_idat_chunks;

	void* m_pMalloc_blocks[PNG_MAX_ALLOC_BLOCKS];

	uint32_t m_dec_bytes_per_pixel; // bytes per pixel decoded from the PNG file (minimum 1 for 1/2/4 bpp), factors in the PNG 8/16 bit/component bit depth, may be up to 8 bytes (2*4)
	uint32_t m_dst_bytes_per_pixel; // bytes per pixel returned to the caller (1-4), always has alpha if the PNG has alpha, 16-bit components always converted to 8-bits/component

	uint32_t m_dec_bytes_per_line;	// bytes per line decoded from the PNG file (before 1/2/4 expansion), +1 for the filter byte
	uint32_t m_src_bytes_per_line;	// decoded PNG bytes per line, before 1/2/4 bpp expansion, not counting the filter byte, updated during adam7 deinterlacing
	uint32_t m_dst_bytes_per_line;	// bytes per line returned to the caller (1-4 times width)

	int (*m_pProcess_func)(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi);

	uint8_t* m_pPre_line_buf;
	uint8_t* m_pCur_line_buf;
	uint8_t* m_pPro_line_buf;

	uint8_t m_bkgd_flag;
	uint32_t  m_bkgd_value[3];

	uint8_t m_gama_flag;
	uint32_t m_gama_value;
			
	uint8_t m_trns_flag;
	uint32_t m_trns_value[256];

	buminiz::mz_stream m_inflator;

	uint8_t inflate_src_buf[PNG_INFLATE_SRC_BUF_SIZE];

	uint32_t m_inflate_src_buf_ofs;
	uint32_t m_inflate_src_buf_size;
	uint32_t m_inflate_dst_buf_ofs;

	int m_inflate_eof_flag;
		
	uint8_t m_gamma_table[256];

	int m_pass_x_size;
	int m_pass_y_left;

	int m_adam7_pass_num;
	int m_adam7_pass_y;
	int m_adam7_pass_size_x[7];
	int m_adam7_pass_size_y[7];

	std::vector<uint8_t> m_adam7_image_buf;
		
	int m_adam7_decoded_flag;
	
	bool m_scanned_flag;
		
	int m_terminate_status;
		
#define TEMP_BUF_SIZE (384)
	uint8_t m_temp_buf[TEMP_BUF_SIZE * 4];
			
	void clear();
	void uninitialize();
	int terminate(int status);
	void* png_malloc(uint32_t i);
	void* png_calloc(uint32_t i);
	int block_read(void* buf, uint32_t len);
	int64_t block_read_dword();
	int fetch_next_chunk_data(uint8_t* buf, int bytes);
	int fetch_next_chunk_byte();
	int fetch_next_chunk_word();
	int64_t fetch_next_chunk_dword();
	int fetch_next_chunk_init();
	int unchunk_data(uint8_t* buf, uint32_t bytes, uint32_t* ptr_bytes_read);
	inline void adam7_write_pixel_8(int x, int y, int c);
	inline void adam7_write_pixel_16(int x, int y, int r, int g);
	inline void adam7_write_pixel_24(int x, int y, int r, int g, int b);
	inline void adam7_write_pixel_32(int x, int y, int r, int g, int b, int a);
	void unpredict_sub(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp);
	void unpredict_up(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp);
	void unpredict_average(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp);
	inline uint8_t paeth_predictor(int a, int b, int c);
	void unpredict_paeth(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp);
	int adam7_pass_size(int size, int start, int step);
	int decompress_line(uint32_t* bytes_decoded);
	int find_iend_chunk();
	void calc_gamma_table();
	void create_grey_palette();
	int read_signature();
	int read_ihdr_chunk();
	int read_bkgd_chunk();
	int read_gama_chunk();
	int read_trns_chunk();
	int read_plte_chunk();
	int find_idat_chunk();
};

void png_decoder::uninitialize()
{
	m_pFile = nullptr;
				
	for (int i = 0; i < PNG_MAX_ALLOC_BLOCKS; i++)
	{
		free(m_pMalloc_blocks[i]);
		m_pMalloc_blocks[i] = nullptr;
	}

	mz_inflateEnd(&m_inflator);
}

int png_decoder::terminate(int status)
{
	if (m_terminate_status == 0)
		m_terminate_status = status;

	uninitialize();
	return status;
}

void* png_decoder::png_malloc(uint32_t len)
{
	if (!len)
		len++;

	void* p = malloc(len);

	if (!p)
		return nullptr;

	int j;
	for (j = 0; j < PNG_MAX_ALLOC_BLOCKS; j++)
		if (!m_pMalloc_blocks[j])
			break;

	if (j == PNG_MAX_ALLOC_BLOCKS)
		return nullptr;

	m_pMalloc_blocks[j] = p;

	return p;
}

void* png_decoder::png_calloc(uint32_t len)
{
	void* p = png_malloc(len);
	if (!p)
		return nullptr;

	if (p)
		memset(p, 0, len);

	return p;
}

int png_decoder::block_read(void* buf, uint32_t len)
{
	size_t bytes_read = m_pFile->read(buf, len);
	if (bytes_read != len)
		return terminate(PNG_READPASTEOF);
	return 0;
}

int64_t png_decoder::block_read_dword()
{
	uint8_t buf[4];

	int status = block_read(buf, 4);
	if (status != 0)
		return status;

	uint32_t v = buf[3] + ((uint32_t)buf[2] << 8) + ((uint32_t)buf[1] << 16) + ((uint32_t)buf[0] << 24);
	return (int64_t)v;
}

int png_decoder::fetch_next_chunk_data(uint8_t* buf, int bytes)
{
	if (!m_chunk_flag)
		return 0;

	bytes = minimum<int>(bytes, m_chunk_left);

	int status = block_read(buf, bytes);
	if (status != 0)
		return status;
				
#if PVPNG_IDAT_CRC_CHECKING
	bool check_crc32 = true;
#else
	const bool is_idat = (m_chunk_name[0] == 'I') && (m_chunk_name[1] == 'D') && (m_chunk_name[2] == 'A') && (m_chunk_name[3] == 'T');
	bool check_crc32 = !is_idat;
#endif

	if (check_crc32)
		m_chunk_crc32 = buminiz::mz_crc32(m_chunk_crc32, buf, bytes);

	if ((m_chunk_left -= bytes) == 0)
	{
		int64_t res = block_read_dword();
		if (res < 0)
			return (int)res;

		if (check_crc32)
		{
			if (m_chunk_crc32 != (uint32_t)res)
				return terminate(PNG_BAD_CHUNK_CRC32);
		}

		m_chunk_flag = FALSE;
	}

	return bytes;
}

int png_decoder::fetch_next_chunk_byte()
{
	uint8_t buf[1];

	int status = fetch_next_chunk_data(buf, 1);
	if (status < 0)
		return status;

	if (status != 1)
		return terminate(PNG_BAD_CHUNK_SIZE);

	return buf[0];
}

int png_decoder::fetch_next_chunk_word()
{
	uint8_t buf[2];

	int status = fetch_next_chunk_data(buf, 2);
	if (status < 0)
		return status;

	if (status != 2)
		return terminate(PNG_BAD_CHUNK_SIZE);

	return buf[1] + ((uint32_t)buf[0] << 8);
}

int64_t png_decoder::fetch_next_chunk_dword()
{
	uint8_t buf[4];

	int status = fetch_next_chunk_data(buf, 4);
	if (status < 0)
		return status;

	if (status != 4)
		terminate(PNG_BAD_CHUNK_SIZE);

	uint32_t v = buf[3] + ((uint32_t)buf[2] << 8) + ((uint32_t)buf[1] << 16) + ((uint32_t)buf[0] << 24);
	return (int64_t)v;
}

int png_decoder::fetch_next_chunk_init()
{
	while (m_chunk_flag)
	{
		int status = fetch_next_chunk_data(m_temp_buf, TEMP_BUF_SIZE * 4);
		if (status != 0)
			return status;
	}
	
	int64_t n = block_read_dword();
	if (n < 0)
		return (int)n;

	m_chunk_size = (uint32_t)n;

	m_chunk_flag = TRUE;
	m_chunk_left = m_chunk_size + 4;
	m_chunk_crc32 = 0;

	int status = fetch_next_chunk_data(m_chunk_name, 4);
	if (status < 0)
		return status;

	return 0;
}

int png_decoder::unchunk_data(uint8_t* buf, uint32_t bytes, uint32_t* ptr_bytes_read)
{
	uint32_t bytes_read = 0;

	if ((!bytes) || (m_end_of_idat_chunks))
	{
		*ptr_bytes_read = 0;
		return TRUE;
	}

	while (bytes_read != bytes)
	{
		if (!m_chunk_flag)
		{
			int res = fetch_next_chunk_init();
			if (res < 0)
				return res;

			if ((m_chunk_name[0] != 'I') ||
				(m_chunk_name[1] != 'D') ||
				(m_chunk_name[2] != 'A') ||
				(m_chunk_name[3] != 'T'))
			{
				*ptr_bytes_read = bytes_read;
				m_end_of_idat_chunks = TRUE;
				return TRUE;
			}
		}

		int res = fetch_next_chunk_data(buf + bytes_read, bytes - bytes_read);
		if (res < 0)
			return res;

		bytes_read += (uint32_t)res;
	}

	*ptr_bytes_read = bytes_read;

	return FALSE;
}

inline void png_decoder::adam7_write_pixel_8(int x, int y, int c)
{
	m_adam7_image_buf[x + y * m_dst_bytes_per_line] = (uint8_t)c;
}

inline void png_decoder::adam7_write_pixel_16(int x, int y, int r, int g)
{
	uint32_t ofs = x * 2 + y * m_dst_bytes_per_line;
	m_adam7_image_buf[ofs + 0] = (uint8_t)r;
	m_adam7_image_buf[ofs + 1] = (uint8_t)g;
}

inline void png_decoder::adam7_write_pixel_24(int x, int y, int r, int g, int b)
{
	uint32_t ofs = x * 3 + y * m_dst_bytes_per_line;
	m_adam7_image_buf[ofs + 0] = (uint8_t)r;
	m_adam7_image_buf[ofs + 1] = (uint8_t)g;
	m_adam7_image_buf[ofs + 2] = (uint8_t)b;
}

inline void png_decoder::adam7_write_pixel_32(int x, int y, int r, int g, int b, int a)
{
	uint32_t ofs = x * 4 + y * m_dst_bytes_per_line;
	m_adam7_image_buf[ofs + 0] = (uint8_t)r;
	m_adam7_image_buf[ofs + 1] = (uint8_t)g;
	m_adam7_image_buf[ofs + 2] = (uint8_t)b;
	m_adam7_image_buf[ofs + 3] = (uint8_t)a;
}

static void PixelDePack2(void* src, void* dst, int numbytes)
{
	uint8_t* src8 = (uint8_t*)src;
	uint8_t* dst8 = (uint8_t*)dst;

	while (numbytes)
	{
		uint8_t v = *src8++;
		
		for (uint32_t i = 0; i < 8; i++)
			dst8[7 - i] = (v >> i) & 1;

		dst8 += 8;
		numbytes--;
	}
}

static void PixelDePack16(void* src, void* dst, int numbytes)
{
	uint8_t* src8 = (uint8_t*)src;
	uint8_t* dst8 = (uint8_t*)dst;

	while (numbytes)
	{
		uint8_t v = *src8++;

		dst8[0] = (uint8_t)v >> 4;
		dst8[1] = (uint8_t)v & 0xF;
		dst8 += 2;

		numbytes--;
	}
}

static int unpack_grey_1(uint8_t* src, uint8_t* dst, int pixels, png_decoder *pwi)
{
	(void)pwi;
	PixelDePack2(src, dst, pixels >> 3);

	dst += (pixels & 0xFFF8);

	if ((pixels & 7) != 0)
	{
		uint8_t c = src[pixels >> 3];

		pixels &= 7;

		while (pixels--)
		{
			*dst++ = ((c & 128) >> 7);

			c <<= 1;
		}
	}

	return TRUE;
}

static int unpack_grey_2(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)pwi;
	int i = pixels;
	uint8_t c;

	while (i >= 4)
	{
		c = *src++;

		*dst++ = (c >> 6);
		*dst++ = (c >> 4) & 3;
		*dst++ = (c >> 2) & 3;
		*dst++ = (c) & 3;

		i -= 4;
	}

	if (i)
	{
		c = *src;

		while (i--)
		{
			*dst++ = (c >> 6);

			c <<= 2;
		}
	}

	return TRUE;
}

static int unpack_grey_4(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)pwi;

	PixelDePack16(src, dst, pixels >> 1);

	if (pixels & 1)
		dst[pixels & 0xFFFE] = (src[pixels >> 1] >> 4);

	return TRUE;
}

static int unpack_grey_8(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)src;
	(void)dst;
	(void)pixels;
	(void)pwi;
	return FALSE;
}

static int unpack_grey_16(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)pwi;
	while (pixels--)
	{
		*dst++ = *src++;

		src++;
	}

	return TRUE;
}

static int unpack_grey_16_2(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	if (pwi->m_trns_flag)
	{
		while (pixels--)
		{
			uint32_t v = (src[0] << 8) + src[1];
			src += 2;

			*dst++ = (uint8_t)(v >> 8);
			*dst++ = (v == pwi->m_trns_value[0]) ? 0 : 255;
		}
	}
	else
	{
		while (pixels--)
		{
			*dst++ = *src++;
			*dst++ = 0xFF;

			src++;
		}
	}

	return TRUE;
}

static int unpack_true_8(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	if (pwi->m_trns_flag)
	{
		const uint32_t tr = pwi->m_trns_value[0];
		const uint32_t tg = pwi->m_trns_value[1];
		const uint32_t tb = pwi->m_trns_value[2];

		for (int i = 0; i < pixels; i++)
		{
			uint8_t r = src[i * 3 + 0];
			uint8_t g = src[i * 3 + 1];
			uint8_t b = src[i * 3 + 2];
			
			dst[i * 4 + 0] = r;
			dst[i * 4 + 1] = g;
			dst[i * 4 + 2] = b;
			dst[i * 4 + 3] = ((r == tr) && (g == tg) && (b == tb)) ? 0 : 255;
		}
	}
	else
	{
		for (int i = 0; i < pixels; i++)
		{
			dst[i * 4 + 0] = src[i * 3 + 0];
			dst[i * 4 + 1] = src[i * 3 + 1];
			dst[i * 4 + 2] = src[i * 3 + 2];
			dst[i * 4 + 3] = 255;
		}
	}

	return TRUE;
}

static int unpack_true_16(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	if (pwi->m_trns_flag)
	{
		const uint32_t tr = pwi->m_trns_value[0];
		const uint32_t tg = pwi->m_trns_value[1];
		const uint32_t tb = pwi->m_trns_value[2];

		for (int i = 0; i < pixels; i++)
		{
			uint32_t r = (src[i * 6 + 0] << 8) + src[i * 6 + 1];
			uint32_t g = (src[i * 6 + 2] << 8) + src[i * 6 + 3];
			uint32_t b = (src[i * 6 + 4] << 8) + src[i * 6 + 5];

			dst[i * 4 + 0] = (uint8_t)(r >> 8);
			dst[i * 4 + 1] = (uint8_t)(g >> 8);
			dst[i * 4 + 2] = (uint8_t)(b >> 8);
			dst[i * 4 + 3] = ((r == tr) && (g == tg) && (b == tb)) ? 0 : 255;
		}
	}
	else
	{
		while (pixels--)
		{
			dst[0] = src[0];
			dst[1] = src[2];
			dst[2] = src[4];
			dst[3] = 255;
			
			dst += 4;
			src += 6;
		}
	}

	return TRUE;
}

static int unpack_grey_alpha_8(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)pwi;
	while (pixels--)
	{
		dst[0] = src[0];
		dst[1] = src[1];
		dst += 2;
		src += 2;
	}

	return TRUE;
}

static int unpack_grey_alpha_16(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)pwi;
	while (pixels--)
	{
		dst[0] = src[0];
		dst[1] = src[2];
		dst += 2;
		src += 4;
	}

	return TRUE;
}

static int unpack_true_alpha_8(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)src;
	(void)dst;
	(void)pixels;
	(void)pwi;
	return FALSE;
}

static int unpack_true_alpha_16(uint8_t* src, uint8_t* dst, int pixels, png_decoder* pwi)
{
	(void)pwi;
	while (pixels--)
	{
		dst[0] = src[0];
		dst[1] = src[2];
		dst[2] = src[4];
		dst[3] = src[6];
		dst += 4;
		src += 8;
	}

	return TRUE;
}

void png_decoder::unpredict_sub(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp)
{
	(void)lst;
	if (bytes == (uint32_t)bpp)
		return;

	cur += bpp;
	bytes -= bpp;

	while (bytes--)
	{
		*cur += *(cur - bpp);
		cur++;
	}
}

void png_decoder::unpredict_up(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp)
{
	(void)bpp;
	while (bytes--)
		*cur++ += *lst++;
}

void png_decoder::unpredict_average(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp)
{
	int i;

	for (i = 0; i < bpp; i++)
		*cur++ += (*lst++ >> 1);

	if (bytes == (uint32_t)bpp)
		return;

	bytes -= bpp;

	while (bytes--)
	{
		*cur += ((*lst++ + *(cur - bpp)) >> 1);
		cur++;
	}
}

inline uint8_t png_decoder::paeth_predictor(int a, int b, int c)
{
	int p, pa, pb, pc;

	/* a = left, b = above, c = upper left */

	p = a + b - c;

	pa = abs(p - a);
	pb = abs(p - b);
	pc = abs(p - c);

	if ((pa <= pb) && (pa <= pc))
		return (uint8_t)a;
	else if (pb <= pc)
		return (uint8_t)b;
	else
		return (uint8_t)c;
}

void png_decoder::unpredict_paeth(uint8_t* lst, uint8_t* cur, uint32_t bytes, int bpp)
{
	int i;

	for (i = 0; i < bpp; i++)
		*cur++ += paeth_predictor(0, *lst++, 0);

	if (bytes == (uint32_t)bpp)
		return;

	bytes -= bpp;

	while (bytes--)
	{
		int p, a, b, c, pa, pb, pc;

		a = *(cur - bpp);
		b = *lst;
		c = *(lst - bpp);

		p = a + b - c;

		pa = abs(p - a);
		pb = abs(p - b);
		pc = abs(p - c);

		if ((pa <= pb) && (pa <= pc))
			*cur++ += (uint8_t)a;
		else if (pb <= pc)
			*cur++ += (uint8_t)b;
		else
			*cur++ += (uint8_t)c;

		lst++;
	}
}

int png_decoder::adam7_pass_size(int size, int start, int step)
{
	if (size > start)
		return 1 + ((size - 1) - start) / step;
	else
		return 0;
}

// TRUE if no more data, negative on error, FALSE if OK
int png_decoder::decompress_line(uint32_t* bytes_decoded)
{
	int status;
	uint32_t temp, src_bytes_left, dst_bytes_left;

	m_inflate_dst_buf_ofs = 0;

	for (; ; )
	{
		if (m_inflate_src_buf_ofs == PNG_INFLATE_SRC_BUF_SIZE)
		{
			int res = unchunk_data(inflate_src_buf, PNG_INFLATE_SRC_BUF_SIZE, &temp);
			if (res < 0)
				return res;
			m_inflate_eof_flag = res;

			m_inflate_src_buf_size = temp;

			m_inflate_src_buf_ofs = 0;
		}

		for (; ; )
		{
			src_bytes_left = m_inflate_src_buf_size - m_inflate_src_buf_ofs;
			dst_bytes_left = m_dec_bytes_per_line - m_inflate_dst_buf_ofs;

			m_inflator.next_in = inflate_src_buf + m_inflate_src_buf_ofs;
			m_inflator.avail_in = src_bytes_left;
			
			m_inflator.next_out = m_pCur_line_buf + m_inflate_dst_buf_ofs;
			m_inflator.avail_out = dst_bytes_left;
						
			status = buminiz::mz_inflate2(&m_inflator, buminiz::MZ_NO_FLUSH, PVPNG_ADLER32_CHECKING);

			const uint32_t src_bytes_consumed = src_bytes_left - m_inflator.avail_in;
			const uint32_t dst_bytes_written = dst_bytes_left - m_inflator.avail_out;
			
			m_inflate_src_buf_ofs += src_bytes_consumed;
			m_inflate_dst_buf_ofs += dst_bytes_written;

			if (status != buminiz::MZ_OK)
			{
				if (status != buminiz::MZ_STREAM_END)
					return terminate(PNG_INVALID_DATA_STREAM);

				if (bytes_decoded)
					*bytes_decoded = m_inflate_dst_buf_ofs;

				return TRUE;
			}

			if (m_inflate_dst_buf_ofs == m_dec_bytes_per_line)
			{
				if (bytes_decoded)
					*bytes_decoded = m_inflate_dst_buf_ofs;

				return FALSE;
			}

			if ((m_inflate_src_buf_ofs == m_inflate_src_buf_size) &&
				(m_inflate_eof_flag == FALSE))
				break;
		}
	}
}

int png_decoder::find_iend_chunk()
{
	uint32_t dummy;

	while (!m_end_of_idat_chunks)
	{
		int res = unchunk_data(m_temp_buf, TEMP_BUF_SIZE * 4, &dummy);
		if (res < 0)
			return res;
	}

	for (; ; )
	{
		if ((m_chunk_name[0] == 'I') &&
			(m_chunk_name[1] == 'E') &&
			(m_chunk_name[2] == 'N') &&
			(m_chunk_name[3] == 'D'))
			break;

		int res = fetch_next_chunk_init();
		if (res < 0)
			return res;
	}

	return 0;
}

int png_decoder::png_decode(void** ppImg_ptr, uint32_t* pImg_len)
{
	int status;
	uint8_t* decoded_line;
	uint32_t bytes_decoded;

	if (m_adam7_decoded_flag)
	{
		if (m_pass_y_left == 0)
			return PNG_ALLDONE;
				
		*ppImg_ptr = &m_adam7_image_buf[(m_ihdr.m_height - m_pass_y_left) * m_dst_bytes_per_line];
		*pImg_len = m_dst_bytes_per_line;
				
		m_pass_y_left--;

		return 0;
	}

	if (m_pass_y_left == 0)
	{
		if (m_ihdr.m_ilace_type == 0)
		{
			status = find_iend_chunk();
			if (status < 0)
				return status;

			return PNG_ALLDONE;
		}

		for (; ; )
		{
			if (++m_adam7_pass_num == 7)
			{
				status = find_iend_chunk();
				if (status < 0)
					return status;
								
				return PNG_ALLDONE;
			}

			if (((m_pass_y_left = m_adam7_pass_size_y[m_adam7_pass_num]) != 0) &&
				((m_pass_x_size = m_adam7_pass_size_x[m_adam7_pass_num]) != 0))
				break;
		}

		switch (m_adam7_pass_num)
		{
		case 0:
		case 1:
		case 3:
		case 5:
			m_adam7_pass_y = 0;
			break;
		case 2:
			m_adam7_pass_y = 4;
			break;
		case 4:
			m_adam7_pass_y = 2;
			break;
		case 6:
			m_adam7_pass_y = 1;
			break;
		}

		switch (m_ihdr.m_color_type)
		{
		case PNG_COLOR_TYPE_GREYSCALE:
		case PNG_COLOR_TYPE_PALETTIZED:
		{
			m_src_bytes_per_line = (((uint32_t)m_pass_x_size * m_ihdr.m_bit_depth) + 7) / 8;
			break;
		}
		case PNG_COLOR_TYPE_TRUECOLOR:
		{
			m_src_bytes_per_line = ((uint32_t)m_pass_x_size * m_dec_bytes_per_pixel);
			break;
		}
		case PNG_COLOR_TYPE_GREYSCALE_ALPHA:
		{
			m_src_bytes_per_line = ((uint32_t)m_pass_x_size * m_dec_bytes_per_pixel);
			break;
		}
		case PNG_COLOR_TYPE_TRUECOLOR_ALPHA:
		{
			m_src_bytes_per_line = ((uint32_t)m_pass_x_size * m_dec_bytes_per_pixel);
			break;
		}
		}

		m_dec_bytes_per_line = m_src_bytes_per_line + 1;

		memset(m_pPre_line_buf, 0, m_src_bytes_per_line);
	}

	int res = decompress_line(&bytes_decoded);
	if (res < 0)
		return terminate(res);

	if (res)
	{
		if (m_ihdr.m_ilace_type == 0)
		{
			if (m_pass_y_left != 1)
				return terminate(PNG_INCOMPLETE_IMAGE);
		}
		else
		{
			if ((m_pass_y_left != 1) && (m_adam7_pass_num != 6))
				return terminate(PNG_INCOMPLETE_IMAGE);
		}
	}

	if (bytes_decoded != m_dec_bytes_per_line)
		return terminate(PNG_INCOMPLETE_IMAGE);

	decoded_line = &m_pCur_line_buf[1];

	switch (m_pCur_line_buf[0])
	{
	case 0:
		break;
	case 1:
	{
		unpredict_sub(m_pPre_line_buf, m_pCur_line_buf + 1, m_src_bytes_per_line, m_dec_bytes_per_pixel);
		break;
	}
	case 2:
	{
		unpredict_up(m_pPre_line_buf, m_pCur_line_buf + 1, m_src_bytes_per_line, m_dec_bytes_per_pixel);
		break;
	}
	case 3:
	{
		unpredict_average(m_pPre_line_buf, m_pCur_line_buf + 1, m_src_bytes_per_line, m_dec_bytes_per_pixel);
		break;
	}
	case 4:
	{
		unpredict_paeth(m_pPre_line_buf, m_pCur_line_buf + 1, m_src_bytes_per_line, m_dec_bytes_per_pixel);
		break;
	}
	default:
		return terminate(PNG_UNS_PREDICTOR);
	}

	memmove(m_pPre_line_buf, m_pCur_line_buf + 1, m_src_bytes_per_line);

	if (m_pProcess_func)
	{
		if ((*m_pProcess_func)(m_pCur_line_buf + 1, m_pPro_line_buf, m_pass_x_size, this))
			decoded_line = m_pPro_line_buf;
	}
		
	if (m_ihdr.m_ilace_type == 0)
	{
		*ppImg_ptr = decoded_line;
		*pImg_len = m_dst_bytes_per_line;

		if (--m_pass_y_left == 0)
		{
			res = decompress_line(&bytes_decoded);
			if (res < 0)
				return terminate(res);

			if (res == FALSE)
				return terminate(PNG_TOO_MUCH_DATA);

			if (bytes_decoded)
				return terminate(PNG_TOO_MUCH_DATA);
		}
	}
	else
	{
		int i, x_ofs = 0, y_ofs = 0, x_stp = 0;
		uint8_t* p = decoded_line;

		switch (m_adam7_pass_num)
		{
		case 0: { x_ofs = 0; x_stp = 8; break; }
		case 1: { x_ofs = 4; x_stp = 8; break; }
		case 2: { x_ofs = 0; x_stp = 4; break; }
		case 3: { x_ofs = 2; x_stp = 4; break; }
		case 4: { x_ofs = 0; x_stp = 2; break; }
		case 5: { x_ofs = 1; x_stp = 2; break; }
		case 6: { x_ofs = 0; x_stp = 1; break; }
		}

		y_ofs = m_adam7_pass_y;

		assert(x_ofs < (int)m_ihdr.m_width);
		assert(y_ofs < (int)m_ihdr.m_height);

		if (m_dst_bytes_per_pixel == 1)
		{
			for (i = m_pass_x_size; i > 0; i--, x_ofs += x_stp)
				adam7_write_pixel_8(x_ofs, y_ofs, *p++);
		}
		else if (m_dst_bytes_per_pixel == 2)
		{
			for (i = m_pass_x_size; i > 0; i--, x_ofs += x_stp, p += 2)
				adam7_write_pixel_16(x_ofs, y_ofs, p[0], p[1]);
		}
		else if (m_dst_bytes_per_pixel == 3)
		{
			for (i = m_pass_x_size; i > 0; i--, x_ofs += x_stp, p += 3)
				adam7_write_pixel_24(x_ofs, y_ofs, p[0], p[1], p[2]);
		}
		else if (m_dst_bytes_per_pixel == 4)
		{
			for (i = m_pass_x_size; i > 0; i--, x_ofs += x_stp, p += 4)
				adam7_write_pixel_32(x_ofs, y_ofs, p[0], p[1], p[2], p[3]);
		}
		else
		{
			assert(0);
		}

		switch (m_adam7_pass_num)
		{
		case 0:
		case 1:
		case 2: { m_adam7_pass_y += 8; break; }
		case 3:
		case 4: { m_adam7_pass_y += 4; break; }
		case 5:
		case 6: { m_adam7_pass_y += 2; break; }
		}

		if ((--m_pass_y_left == 0) && (m_adam7_pass_num == 6))
		{
			res = decompress_line(&bytes_decoded);
			if (res < 0)
				return terminate(res);

			if (res == FALSE)
				return terminate(PNG_TOO_MUCH_DATA);

			if (bytes_decoded)
				return terminate(PNG_TOO_MUCH_DATA);
		}
	}

	return 0;
}

void png_decoder::png_decode_end()
{
	uninitialize();
}

int png_decoder::png_decode_start()
{
	int status;
	
	if (m_img_supported_flag != TRUE)
		return terminate(m_img_supported_flag);
		
	switch (m_ihdr.m_color_type)
	{
	case PNG_COLOR_TYPE_GREYSCALE:
	{
		if (m_ihdr.m_bit_depth == 16)
		{
			// This is a special case. We can't pass back 8-bit samples and let the caller decide on transparency because the PNG is 16-bits. 
			// So we expand to 8-bit Gray-Alpha and handle transparency during decoding.
			// We don't do this with all grayscale cases because that would require more code to deal with 1/2/4bpp expansion.
			m_dec_bytes_per_pixel = (m_ihdr.m_bit_depth + 7) / 8;
			m_dst_bytes_per_pixel = 2;

			m_src_bytes_per_line = (((uint32_t)m_ihdr.m_width * m_ihdr.m_bit_depth) + 7) / 8;
			m_dst_bytes_per_line = 2 * m_ihdr.m_width;

			m_pProcess_func = unpack_grey_16_2;
		}
		else
		{
			m_dec_bytes_per_pixel = (m_ihdr.m_bit_depth + 7) / 8;
			m_dst_bytes_per_pixel = 1;

			m_src_bytes_per_line = (((uint32_t)m_ihdr.m_width * m_ihdr.m_bit_depth) + 7) / 8;
			m_dst_bytes_per_line = m_ihdr.m_width;

			if (m_ihdr.m_bit_depth == 1)
				m_pProcess_func = unpack_grey_1;
			else if (m_ihdr.m_bit_depth == 2)
				m_pProcess_func = unpack_grey_2;
			else if (m_ihdr.m_bit_depth == 4)
				m_pProcess_func = unpack_grey_4;
			else 
				m_pProcess_func = unpack_grey_8;
		}

		break;
	}
	case PNG_COLOR_TYPE_PALETTIZED:
	{
		m_dec_bytes_per_pixel = (m_ihdr.m_bit_depth + 7) / 8;
		m_dst_bytes_per_pixel = 1;

		m_src_bytes_per_line = (((uint32_t)m_ihdr.m_width * m_ihdr.m_bit_depth) + 7) / 8;
		m_dst_bytes_per_line = m_ihdr.m_width;

		if (m_ihdr.m_bit_depth == 1)
			m_pProcess_func = unpack_grey_1;
		else if (m_ihdr.m_bit_depth == 2)
			m_pProcess_func = unpack_grey_2;
		else if (m_ihdr.m_bit_depth == 4)
			m_pProcess_func = unpack_grey_4;
		else if (m_ihdr.m_bit_depth == 8)
			m_pProcess_func = unpack_grey_8;
		else if (m_ihdr.m_bit_depth == 16)
			m_pProcess_func = unpack_grey_16;

		break;
	}
	case PNG_COLOR_TYPE_TRUECOLOR:
	{
		// We always pass back alpha with transparency handling.
		m_dec_bytes_per_pixel = 3 * (m_ihdr.m_bit_depth / 8);
		m_dst_bytes_per_pixel = 4;

		m_src_bytes_per_line = ((uint32_t)m_ihdr.m_width * m_dec_bytes_per_pixel);
		m_dst_bytes_per_line = 4 * m_ihdr.m_width;

		if (m_ihdr.m_bit_depth == 8)
			m_pProcess_func = unpack_true_8;
		else if (m_ihdr.m_bit_depth == 16)
			m_pProcess_func = unpack_true_16;

		break;
	}
	case PNG_COLOR_TYPE_GREYSCALE_ALPHA:
	{
		m_dec_bytes_per_pixel = 2 * (m_ihdr.m_bit_depth / 8);
		m_dst_bytes_per_pixel = 2;

		m_src_bytes_per_line = ((uint32_t)m_ihdr.m_width * m_dec_bytes_per_pixel);
		m_dst_bytes_per_line = m_ihdr.m_width * 2;

		if (m_ihdr.m_bit_depth == 8)
			m_pProcess_func = unpack_grey_alpha_8;
		else if (m_ihdr.m_bit_depth == 16)
			m_pProcess_func = unpack_grey_alpha_16;

		break;
	}
	case PNG_COLOR_TYPE_TRUECOLOR_ALPHA:
	{
		m_dec_bytes_per_pixel = 4 * (m_ihdr.m_bit_depth / 8);
		m_dst_bytes_per_pixel = 4;

		m_src_bytes_per_line = ((uint32_t)m_ihdr.m_width * m_dec_bytes_per_pixel);
		m_dst_bytes_per_line = 4 * m_ihdr.m_width;

		if (m_ihdr.m_bit_depth == 8)
			m_pProcess_func = unpack_true_alpha_8;
		else
			m_pProcess_func = unpack_true_alpha_16;

		break;
	}
	}

	m_dec_bytes_per_line = m_src_bytes_per_line + 1;

	m_pPre_line_buf = (uint8_t*)png_calloc(m_src_bytes_per_line);
	m_pCur_line_buf = (uint8_t*)png_calloc(m_dec_bytes_per_line);
	m_pPro_line_buf = (uint8_t*)png_calloc(m_dst_bytes_per_line);

	if (!m_pPre_line_buf || !m_pCur_line_buf || !m_pPro_line_buf)
		return terminate(PNG_NOTENOUGHMEM);

	m_inflate_src_buf_ofs = PNG_INFLATE_SRC_BUF_SIZE;

	int res = mz_inflateInit(&m_inflator);
	if (res != 0)
		return terminate(PNG_DECERROR);

	if (m_ihdr.m_ilace_type == 1)
	{
		//int i;
		//uint32_t total_lines, lines_processed;

		m_adam7_pass_size_x[0] = adam7_pass_size(m_ihdr.m_width, 0, 8);
		m_adam7_pass_size_x[1] = adam7_pass_size(m_ihdr.m_width, 4, 8);
		m_adam7_pass_size_x[2] = adam7_pass_size(m_ihdr.m_width, 0, 4);
		m_adam7_pass_size_x[3] = adam7_pass_size(m_ihdr.m_width, 2, 4);
		m_adam7_pass_size_x[4] = adam7_pass_size(m_ihdr.m_width, 0, 2);
		m_adam7_pass_size_x[5] = adam7_pass_size(m_ihdr.m_width, 1, 2);
		m_adam7_pass_size_x[6] = adam7_pass_size(m_ihdr.m_width, 0, 1);

		m_adam7_pass_size_y[0] = adam7_pass_size(m_ihdr.m_height, 0, 8);
		m_adam7_pass_size_y[1] = adam7_pass_size(m_ihdr.m_height, 0, 8);
		m_adam7_pass_size_y[2] = adam7_pass_size(m_ihdr.m_height, 4, 8);
		m_adam7_pass_size_y[3] = adam7_pass_size(m_ihdr.m_height, 0, 4);
		m_adam7_pass_size_y[4] = adam7_pass_size(m_ihdr.m_height, 2, 4);
		m_adam7_pass_size_y[5] = adam7_pass_size(m_ihdr.m_height, 0, 2);
		m_adam7_pass_size_y[6] = adam7_pass_size(m_ihdr.m_height, 1, 2);
				
		m_adam7_image_buf.resize(m_dst_bytes_per_line * m_ihdr.m_height);
								
		m_adam7_pass_num = -1;

		m_pass_y_left = 0;

#if 0
		total_lines = lines_processed = 0;

		for (i = 0; i < 7; i++)
			total_lines += m_adam7_pass_size_y[i];
#endif

		for (; ; )
		{
			void* dummy_ptr = nullptr;
			uint32_t dummy_len = 0;

			status = png_decode(&dummy_ptr, &dummy_len);

			if (status)
			{
				if (status == PNG_ALLDONE)
					break;
				else
				{
					uninitialize();

					return status;
				}
			}

			//lines_processed++;
		}

		m_adam7_decoded_flag = TRUE;
		m_pass_y_left = m_ihdr.m_height;
	}
	else
	{
		m_pass_x_size = m_ihdr.m_width;
		m_pass_y_left = m_ihdr.m_height;
	}
		
	return 0;
}

void png_decoder::calc_gamma_table()
{
	if (m_gama_value == 45000)
	{
		for (int i = 0; i < 256; i++)
			m_gamma_table[i] = (uint8_t)i;
		return;
	}

	float gamma = (float)(m_gama_value) / 100000.0f;

	gamma = 1.0f / (gamma * 2.2f);

	for (int i = 0; i < 256; i++)
	{
		float temp = powf((float)(i) / 255.0f, gamma) * 255.0f;

		int j = (int)(temp + .5f);

		if (j < 0)
			j = 0;
		else if (j > 255)
			j = 255;

		m_gamma_table[i] = (uint8_t)j;
	}
}

void png_decoder::create_grey_palette()
{
	int i, j;
	uint8_t* p = m_img_pal;

	const int img_colors = minimum(256, 1 << m_ihdr.m_bit_depth);
	for (i = 0; i < img_colors; i++)
	{
		j = ((uint32_t)255 * (uint32_t)i) / (img_colors - 1);

		*p++ = (uint8_t)j;
		*p++ = (uint8_t)j;
		*p++ = (uint8_t)j;
	}
}

int png_decoder::read_signature()
{
	if (m_pFile->read(m_temp_buf, 8) != 8)
		return terminate(PNG_UNKNOWNTYPE);

	if ((m_temp_buf[0] != 137) ||
		(m_temp_buf[1] != 80) ||
		(m_temp_buf[2] != 78) ||
		(m_temp_buf[3] != 71) ||
		(m_temp_buf[4] != 13) ||
		(m_temp_buf[5] != 10) ||
		(m_temp_buf[6] != 26) ||
		(m_temp_buf[7] != 10))
	{
		return terminate(PNG_UNKNOWNTYPE);
	}

	return 0;
}

int png_decoder::read_ihdr_chunk()
{
	int res = fetch_next_chunk_init();
	if (res < 0)
		return res;

	if ((m_chunk_name[0] != 'I') || (m_chunk_name[1] != 'H') || (m_chunk_name[2] != 'D') || (m_chunk_name[3] != 'R') || (m_chunk_size != 13))
		return terminate(PNG_NO_IHDR);

	int64_t v64 = fetch_next_chunk_dword();
	if (v64 < 0)
		return (int)v64;
	m_ihdr.m_width = (uint32_t)v64;

	v64 = fetch_next_chunk_dword();
	if (v64 < 0)
		return (int)v64;
	m_ihdr.m_height = (uint32_t)v64;

	if ((m_ihdr.m_width == 0) || (m_ihdr.m_width > MAX_SUPPORTED_RES))
		return terminate(PNG_BAD_WIDTH);

	if ((m_ihdr.m_height == 0) || (m_ihdr.m_height > MAX_SUPPORTED_RES))
		return terminate(PNG_BAD_HEIGHT);

	int v = fetch_next_chunk_byte(); 
	if (v < 0) 
		return v;
	m_ihdr.m_bit_depth = (uint8_t)v;

	v = fetch_next_chunk_byte(); 
	if (v < 0) 
		return v;
	m_ihdr.m_color_type = (uint8_t)v;

	v = fetch_next_chunk_byte();
	if (v < 0)
		return v;
	m_ihdr.m_comp_type = (uint8_t)v;

	v = fetch_next_chunk_byte();
	if (v < 0)
		return v;
	m_ihdr.m_filter_type = (uint8_t)v;

	v = fetch_next_chunk_byte();
	if (v < 0)
		return v;
	m_ihdr.m_ilace_type = (uint8_t)v;

	if (m_ihdr.m_comp_type != 0)
		m_img_supported_flag = PNG_UNS_COMPRESSION;

	if (m_ihdr.m_filter_type != 0)
		m_img_supported_flag = PNG_UNS_FILTER;

	if (m_ihdr.m_ilace_type > 1)
		m_img_supported_flag = PNG_UNS_ILACE;
		
	switch (m_ihdr.m_color_type)
	{
	case PNG_COLOR_TYPE_GREYSCALE:
	{
		switch (m_ihdr.m_bit_depth)
		{
		case 1:
		case 2:
		case 4:
		case 8:
		case 16:
		{
			break;
		}
		default:
			return terminate(PNG_BAD_BIT_DEPTH);
		}

		break;
	}
	case PNG_COLOR_TYPE_PALETTIZED:
	{
		switch (m_ihdr.m_bit_depth)
		{
		case 1:
		case 2:
		case 4:
		case 8:
		{
			break;
		}
		default:
			return terminate(PNG_BAD_BIT_DEPTH);
		}

		break;
	}
	case PNG_COLOR_TYPE_TRUECOLOR:
	case PNG_COLOR_TYPE_GREYSCALE_ALPHA:
	case PNG_COLOR_TYPE_TRUECOLOR_ALPHA:
	{
		switch (m_ihdr.m_bit_depth)
		{
		case 8:
		case 16:
		{
			break;
		}
		default:
			return terminate(PNG_BAD_BIT_DEPTH);
		}

		break;
	}
	default:
		return terminate(PNG_UNS_COLOR_TYPE);
	}

	return 0;
}

int png_decoder::read_bkgd_chunk()
{
	m_bkgd_flag = TRUE;

	if (m_ihdr.m_color_type == PNG_COLOR_TYPE_PALETTIZED)
	{
		int v = fetch_next_chunk_byte();
		if (v < 0)
			return v;
		m_bkgd_value[0] = v;
	}
	else if ((m_ihdr.m_color_type == PNG_COLOR_TYPE_GREYSCALE) || (m_ihdr.m_color_type == PNG_COLOR_TYPE_GREYSCALE_ALPHA))
	{
		int v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_bkgd_value[0] = v;
	}
	else if ((m_ihdr.m_color_type == PNG_COLOR_TYPE_TRUECOLOR) || (m_ihdr.m_color_type == PNG_COLOR_TYPE_TRUECOLOR_ALPHA))
	{
		int v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_bkgd_value[0] = v;

		v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_bkgd_value[1] = v;
		
		v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_bkgd_value[2] = v;
	}

	return 0;
}

int png_decoder::read_gama_chunk()
{
	m_gama_flag = TRUE;

	int64_t v = fetch_next_chunk_dword();
	if (v < 0)
		return (int)v;

	m_gama_value = (uint32_t)v;
	
	return 0;
}

int png_decoder::read_trns_chunk()
{
	int i;

	m_trns_flag = TRUE;

	if (m_ihdr.m_color_type == PNG_COLOR_TYPE_PALETTIZED)
	{
		for (i = 0; i < 256; i++)
			m_trns_value[i] = 255;

		const uint32_t img_colors = 1 << m_ihdr.m_bit_depth;
		if (m_chunk_size > (uint32_t)img_colors)
			return terminate(PNG_BAD_TRNS_CHUNK);

		for (i = 0; i < (int)m_chunk_size; i++)
		{
			int v = fetch_next_chunk_byte();
			if (v < 0)
				return v;
			m_trns_value[i] = v;
		}
	}
	else if (m_ihdr.m_color_type == PNG_COLOR_TYPE_GREYSCALE)
	{
		int v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_trns_value[0] = v;
	}
	else if (m_ihdr.m_color_type == PNG_COLOR_TYPE_TRUECOLOR)
	{
		int v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_trns_value[0] = v;
		
		v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_trns_value[1] = v;
		
		v = fetch_next_chunk_word();
		if (v < 0)
			return v;
		m_trns_value[2] = v;
	}
	else
	{
		return terminate(PNG_BAD_TRNS_CHUNK);
	}
	return 0;
}

int png_decoder::read_plte_chunk()
{
	int i, j;
	uint8_t* p;

	if (m_plte_flag)
		return terminate(PNG_BAD_PLTE_CHUNK);

	m_plte_flag = TRUE;

	memset(m_img_pal, 0, 768);

	if (m_chunk_size % 3)
		return terminate(PNG_BAD_PLTE_CHUNK);

	j = m_chunk_size / 3;

	const int img_colors = minimum(256, 1 << m_ihdr.m_bit_depth);
	if (j > img_colors)
		return terminate(PNG_BAD_PLTE_CHUNK);

	if ((m_ihdr.m_color_type == PNG_COLOR_TYPE_GREYSCALE) ||
		(m_ihdr.m_color_type == PNG_COLOR_TYPE_GREYSCALE_ALPHA))
		return terminate(PNG_BAD_PLTE_CHUNK);

	p = m_img_pal;

	for (i = 0; i < j; i++)
	{
		int v = fetch_next_chunk_byte();
		if (v < 0)
			return v;
		*p++ = (uint8_t)v;
		
		v = fetch_next_chunk_byte();
		if (v < 0)
			return v;
		*p++ = (uint8_t)v;

		v = fetch_next_chunk_byte();
		if (v < 0)
			return v;
		*p++ = (uint8_t)v;
	}
	
	return 0;
}

int png_decoder::find_idat_chunk()
{
	for (; ; )
	{
		int res = fetch_next_chunk_init();
		if (res < 0)
			return res;

		if (m_chunk_name[0] & 32)  /* ancillary? */
		{
			if ((m_chunk_name[0] == 'b') && (m_chunk_name[1] == 'K') && (m_chunk_name[2] == 'G') && (m_chunk_name[3] == 'D'))
			{
				res = read_bkgd_chunk();
				if (res < 0)
					return res;
			}
			else if ((m_chunk_name[0] == 'g') && (m_chunk_name[1] == 'A') && (m_chunk_name[2] == 'M') && (m_chunk_name[3] == 'A'))
			{
				res = read_gama_chunk();
				if (res < 0)
					return res;
			}
			else if ((m_chunk_name[0] == 't') && (m_chunk_name[1] == 'R') && (m_chunk_name[2] == 'N') && (m_chunk_name[3] == 'S'))
			{
				res = read_trns_chunk();
				if (res < 0)
					return res;
			}
		}
		else
		{
			if ((m_chunk_name[0] == 'P') && (m_chunk_name[1] == 'L') && (m_chunk_name[2] == 'T') && (m_chunk_name[3] == 'E'))
			{
				res = read_plte_chunk();
				if (res < 0)
					return res;
			}
			else if ((m_chunk_name[0] == 'I') && (m_chunk_name[1] == 'D') && (m_chunk_name[2] == 'A') && (m_chunk_name[3] == 'T'))
			{
				break;
			}
			else
			{
				m_img_supported_flag = PNG_UNS_CRITICAL_CHUNK;
			}
		}
	}

	return 0;
}

png_decoder::png_decoder()
{
	clear();
}

png_decoder::~png_decoder()
{
	uninitialize();
}

void png_decoder::clear()
{
	clear_obj(m_pMalloc_blocks);

	m_pFile = nullptr;

	clear_obj(m_img_pal);

	m_img_supported_flag = FALSE;

	m_adam7_image_buf.clear();

	clear_obj(m_ihdr);

	m_chunk_flag = FALSE;
	m_chunk_size = 0;
	m_chunk_left = 0;
	m_chunk_crc32 = 0;
	clear_obj(m_chunk_name);

	m_end_of_idat_chunks = 0;

	m_dec_bytes_per_pixel = 0;
	m_dst_bytes_per_pixel = 0;

	m_dec_bytes_per_line = 0;
	m_src_bytes_per_line = 0;
	m_dst_bytes_per_line = 0;

	m_pProcess_func = nullptr;

	m_pPre_line_buf = nullptr;
	m_pCur_line_buf = nullptr;
	m_pPro_line_buf = nullptr;

	m_bkgd_flag = FALSE;
	clear_obj(m_bkgd_value);

	m_gama_flag = FALSE;
	m_gama_value = 0;

	m_plte_flag = FALSE;

	m_trns_flag = FALSE;
	clear_obj(m_trns_value);

	clear_obj(m_inflator);

	m_inflate_src_buf_ofs = 0;
	m_inflate_src_buf_size = 0;
	m_inflate_dst_buf_ofs = 0;

	m_inflate_eof_flag = FALSE;
		
	clear_obj(m_trns_value);

	m_pass_x_size = 0;
	m_pass_y_left = 0;

	m_adam7_pass_num = 0;
	m_adam7_pass_y = 0;
	clear_obj(m_adam7_pass_size_x);
	clear_obj(m_adam7_pass_size_y);
		
	m_adam7_decoded_flag = FALSE;
	
	m_scanned_flag = false;
				
	m_terminate_status = 0;
}

int png_decoder::png_scan(png_file *pFile)
{
	m_pFile = pFile;
		
	m_img_supported_flag = TRUE;
	m_terminate_status = 0;

	int res = read_signature();
	if (res != 0)
		return res;

	res = read_ihdr_chunk();
	if (res != 0)
		return res;
		
	res = find_idat_chunk();
	if (res != 0)
		return res;
				
	if (m_gama_flag)
		calc_gamma_table();

	if (m_ihdr.m_color_type == PNG_COLOR_TYPE_PALETTIZED)
	{
		if (!m_plte_flag)
			return terminate(PNG_MISSING_PALETTE);
	}
	else if ((m_ihdr.m_color_type == PNG_COLOR_TYPE_GREYSCALE) || (m_ihdr.m_color_type == PNG_COLOR_TYPE_GREYSCALE_ALPHA))
	{
		create_grey_palette();
	}

	m_scanned_flag = true;

	return 0;
}

static inline uint8_t get_709_luma(uint32_t r, uint32_t g, uint32_t b)
{ 
	return (uint8_t)((13938U * r + 46869U * g + 4729U * b + 32768U) >> 16U);
}

bool get_png_info(const void* pImage_buf, size_t buf_size, png_info &info)
{
	memset(&info, 0, sizeof(info));
		
	if ((!pImage_buf) || (buf_size < MIN_PNG_SIZE))
		return false;

	png_readonly_memory_file mf;
	mf.init(pImage_buf, buf_size);

	png_decoder dec;

	int status = dec.png_scan(&mf);
	if ((status != 0) || (dec.m_img_supported_flag != TRUE))
		return false;

	info.m_width = dec.m_ihdr.m_width;
	info.m_height = dec.m_ihdr.m_height;
	info.m_bit_depth = dec.m_ihdr.m_bit_depth;
	info.m_color_type = dec.m_ihdr.m_color_type;
	info.m_has_gamma = dec.m_gama_flag != 0;
	info.m_gamma_value = dec.m_gama_value;
	info.m_has_trns = dec.m_trns_flag != 0;

	switch (dec.m_ihdr.m_color_type)
	{
	case PNG_COLOR_TYPE_GREYSCALE:
		info.m_num_chans = dec.m_trns_flag ? 2 : 1;
		break;
	case PNG_COLOR_TYPE_GREYSCALE_ALPHA:
		info.m_num_chans = 2;
		break;
	case PNG_COLOR_TYPE_PALETTIZED:
	case PNG_COLOR_TYPE_TRUECOLOR:
		info.m_num_chans = dec.m_trns_flag ? 4 : 3;
		break;
	case PNG_COLOR_TYPE_TRUECOLOR_ALPHA:
		info.m_num_chans = 4;
		break;
	default:
		assert(0);
		break;
	}

	return true;
}

void* load_png(const void* pImage_buf, size_t buf_size, uint32_t desired_chans, uint32_t& width, uint32_t& height, uint32_t& num_chans)
{
	width = 0;
	height = 0;
	num_chans = 0;
		
	if ((!pImage_buf) || (buf_size < MIN_PNG_SIZE))
	{
		assert(0);
		return nullptr;
	}

	if (desired_chans > 4)
	{
		assert(0);
		return nullptr;
	}

	png_readonly_memory_file mf;
	mf.init(pImage_buf, buf_size);

	png_decoder dec;
		
	int status = dec.png_scan(&mf);
	if ((status != 0) || (dec.m_img_supported_flag != TRUE))
		return nullptr;

	uint32_t colortype = dec.m_ihdr.m_color_type;
	switch (colortype)
	{
	case PNG_COLOR_TYPE_GREYSCALE:
		num_chans = dec.m_trns_flag ? 2 : 1;
		break;
	case PNG_COLOR_TYPE_GREYSCALE_ALPHA:
		num_chans = 2;
		break;
	case PNG_COLOR_TYPE_PALETTIZED:
	case PNG_COLOR_TYPE_TRUECOLOR:
		num_chans = dec.m_trns_flag ? 4 : 3;
		break;
	case PNG_COLOR_TYPE_TRUECOLOR_ALPHA:
		num_chans = 4;
		break;
	default:
		assert(0);
		break;
	}

	if (!desired_chans)
		desired_chans = num_chans;

#if 0
	printf("lode_png: %ux%u bitdepth: %u colortype: %u trns: %u ilace: %u\n",
		dec.m_ihdr.m_width,
		dec.m_ihdr.m_height,
		dec.m_ihdr.m_bit_depth,
		dec.m_ihdr.m_color_type,
		dec.m_trns_flag,
		dec.m_ihdr.m_ilace_type);
#endif

	width = dec.m_ihdr.m_width;
	height = dec.m_ihdr.m_height;
	uint32_t bitdepth = dec.m_ihdr.m_bit_depth;
	uint32_t pitch = width * desired_chans;

	uint64_t total_size = (uint64_t)pitch * height;
	if (total_size > 0x7FFFFFFFULL)
		return nullptr;
	
	uint8_t* pBuf = (uint8_t*)malloc((size_t)total_size);
	if (!pBuf)
		return nullptr;
	
	if (dec.png_decode_start() != 0)
	{
		free(pBuf);
		return nullptr;
	}

	uint8_t* pDst = pBuf;

	for (uint32_t y = 0; y < height; y++, pDst += pitch)
	{
		uint8_t* pLine;
		uint32_t line_bytes;
		if (dec.png_decode((void**)&pLine, &line_bytes) != 0)
		{
			free(pBuf);
			return nullptr;
		}

		// This conversion matrix handles converting RGB->Luma, converting grayscale samples to 8-bit samples, converting palettized images, and PNG transparency.
		switch (colortype)
		{
		case PNG_COLOR_TYPE_GREYSCALE:
		{
			uint32_t trans_value = dec.m_trns_value[0];

			switch (desired_chans)
			{
			case 1:
				if (bitdepth == 16)
				{
					assert(line_bytes == width * 2);

					for (uint32_t i = 0; i < width; i++)
						pDst[i] = dec.m_img_pal[pLine[i * 2 + 0] * 3];
				}
				else if (bitdepth == 8)
				{
					assert(line_bytes == width);
					memcpy(pDst, pLine, pitch);
				}
				else
				{
					assert(line_bytes == width);
					for (uint32_t i = 0; i < width; i++)
						pDst[i] = dec.m_img_pal[pLine[i] * 3];
				}
				break;
			case 2:
				if (bitdepth == 16)
				{
					assert(line_bytes == width * 2);
					for (uint32_t i = 0; i < width; i++)
					{
						pDst[i * 2 + 0] = dec.m_img_pal[pLine[i * 2 + 0] * 3];
						pDst[i * 2 + 1] = pLine[i * 2 + 1];
					}
				}
				else if (dec.m_trns_flag)
				{
					assert(line_bytes == width);
					for (uint32_t i = 0; i < width; i++)
					{
						pDst[i * 2 + 0] = dec.m_img_pal[pLine[i] * 3];
						pDst[i * 2 + 1] = (pLine[i] == trans_value) ? 0 : 255;
					}
				}
				else
				{
					assert(line_bytes == width);
					for (uint32_t i = 0; i < width; i++)
					{
						pDst[i * 2 + 0] = dec.m_img_pal[pLine[i] * 3];
						pDst[i * 2 + 1] = 255;
					}
				}
				break;
			case 3:
				if (bitdepth == 16)
				{
					assert(line_bytes == width * 2);
					for (uint32_t i = 0; i < width; i++)
					{
						uint8_t c = dec.m_img_pal[pLine[i * 2 + 0] * 3];
						pDst[i * 3 + 0] = c;
						pDst[i * 3 + 1] = c;
						pDst[i * 3 + 2] = c;
					}
				}
				else
				{
					assert(line_bytes == width);
					for (uint32_t i = 0; i < width; i++)
					{
						uint8_t c = dec.m_img_pal[pLine[i] * 3];
						pDst[i * 3 + 0] = c;
						pDst[i * 3 + 1] = c;
						pDst[i * 3 + 2] = c;
					}
				}
				break;
			case 4:
				if (bitdepth == 16)
				{
					assert(line_bytes == width * 2);
					for (uint32_t i = 0; i < width; i++)
					{
						uint8_t c = dec.m_img_pal[pLine[i * 2 + 0] * 3];
						pDst[i * 4 + 0] = c;
						pDst[i * 4 + 1] = c;
						pDst[i * 4 + 2] = c;
						pDst[i * 4 + 3] = pLine[i * 2 + 1];
					}
				}
				else if (dec.m_trns_flag)
				{
					assert(line_bytes == width);
					for (uint32_t i = 0; i < width; i++)
					{
						uint8_t c = dec.m_img_pal[pLine[i] * 3];
						pDst[i * 4 + 0] = c;
						pDst[i * 4 + 1] = c;
						pDst[i * 4 + 2] = c;
						pDst[i * 4 + 3] = (pLine[i] == trans_value) ? 0 : 255;
					}
				}
				else
				{
					assert(line_bytes == width);
					for (uint32_t i = 0; i < width; i++)
					{
						uint8_t c = dec.m_img_pal[pLine[i] * 3];
						pDst[i * 4 + 0] = c;
						pDst[i * 4 + 1] = c;
						pDst[i * 4 + 2] = c;
						pDst[i * 4 + 3] = 255;
					}
				}
				break;
			}

			break;
		}
		case PNG_COLOR_TYPE_GREYSCALE_ALPHA:
		{
			assert(line_bytes == width * 2);

			switch (desired_chans)
			{
			case 1:
				for (uint32_t i = 0; i < width; i++)
					pDst[i] = dec.m_img_pal[pLine[i * 2 + 0] * 3];
				break;
			case 2:
				assert(line_bytes == pitch);
				if (bitdepth >= 8)
					memcpy(pDst, pLine, pitch);
				else
				{
					for (uint32_t i = 0; i < width; i++)
					{
						pDst[i * 2 + 0] = dec.m_img_pal[pLine[i * 2 + 0] * 3];
						pDst[i * 2 + 1] = pLine[i * 2 + 1];
					}
				}
				break;
			case 3:
				for (uint32_t i = 0; i < width; i++)
				{
					uint8_t c = dec.m_img_pal[pLine[i * 2 + 0] * 3];
					pDst[i * 3 + 0] = c;
					pDst[i * 3 + 1] = c;
					pDst[i * 3 + 2] = c;
				}
				break;
			case 4:
				for (uint32_t i = 0; i < width; i++)
				{
					uint8_t c = dec.m_img_pal[pLine[i * 2 + 0] * 3];
					pDst[i * 4 + 0] = c;
					pDst[i * 4 + 1] = c;
					pDst[i * 4 + 2] = c;
					pDst[i * 4 + 3] = pLine[i * 2 + 1];
				}
				break;
			}

			break;
		}
		case PNG_COLOR_TYPE_PALETTIZED:
		{
			assert(line_bytes == width);

			switch (desired_chans)
			{
			case 1:
				for (uint32_t i = 0; i < width; i++)
				{
					const uint8_t* p = &dec.m_img_pal[pLine[i] * 3];
					pDst[i] = get_709_luma(p[0], p[1], p[2]);
				}
				break;
			case 2:
				if (dec.m_trns_flag)
				{
					for (uint32_t i = 0; i < width; i++)
					{
						const uint8_t* p = &dec.m_img_pal[pLine[i] * 3];
						pDst[i * 2 + 0] = get_709_luma(p[0], p[1], p[2]);
						pDst[i * 2 + 1] = (uint8_t)dec.m_trns_value[pLine[i]];
					}
				}
				else
				{
					for (uint32_t i = 0; i < width; i++)
					{
						const uint8_t* p = &dec.m_img_pal[pLine[i] * 3];
						pDst[i * 2 + 0] = get_709_luma(p[0], p[1], p[2]);
						pDst[i * 2 + 1] = 255;
					}
				}
				break;
			case 3:
				for (uint32_t i = 0; i < width; i++)
				{
					const uint8_t* p = &dec.m_img_pal[pLine[i] * 3];
					pDst[i * 3 + 0] = p[0];
					pDst[i * 3 + 1] = p[1];
					pDst[i * 3 + 2] = p[2];
				}
				break;
			case 4:
				if (dec.m_trns_flag)
				{
					for (uint32_t i = 0; i < width; i++)
					{
						const uint8_t* p = &dec.m_img_pal[pLine[i] * 3];
						pDst[i * 4 + 0] = p[0];
						pDst[i * 4 + 1] = p[1];
						pDst[i * 4 + 2] = p[2];
						pDst[i * 4 + 3] = (uint8_t)dec.m_trns_value[pLine[i]];
					}
				}
				else
				{
					for (uint32_t i = 0; i < width; i++)
					{
						const uint8_t* p = &dec.m_img_pal[pLine[i] * 3];
						pDst[i * 4 + 0] = p[0];
						pDst[i * 4 + 1] = p[1];
						pDst[i * 4 + 2] = p[2];
						pDst[i * 4 + 3] = 255;
					}
				}
				break;
			}

			break;
		}
		case PNG_COLOR_TYPE_TRUECOLOR:
		case PNG_COLOR_TYPE_TRUECOLOR_ALPHA:
		{
			assert(line_bytes == width * 4);

			switch (desired_chans)
			{
			case 1:
				for (uint32_t i = 0; i < width; i++)
				{
					const uint8_t* p = &pLine[i * 4];
					pDst[i] = get_709_luma(p[0], p[1], p[2]);
				}
				break;
			case 2:
				for (uint32_t i = 0; i < width; i++)
				{
					const uint8_t* p = &pLine[i * 4];
					pDst[i * 2 + 0] = get_709_luma(p[0], p[1], p[2]);
					pDst[i * 2 + 1] = p[3];
				}
				break;
			case 3:
				for (uint32_t i = 0; i < width; i++)
				{
					const uint8_t* p = &pLine[i * 4];
					pDst[i * 3 + 0] = p[0];
					pDst[i * 3 + 1] = p[1];
					pDst[i * 3 + 2] = p[2];
				}
				break;
			case 4:
				memcpy(pDst, pLine, pitch);
				break;
			}

			break;
		}
		default:
			assert(0);
			break;
		}

	} // y

	return pBuf;
}

} // namespace pv_png

/*
	This is free and unencumbered software released into the public domain.

	Anyone is free to copy, modify, publish, use, compile, sell, or
	distribute this software, either in source code form or as a compiled
	binary, for any purpose, commercial or non-commercial, and by any
	means.

	In jurisdictions that recognize copyright laws, the author or authors
	of this software dedicate any and all copyright interest in the
	software to the public domain. We make this dedication for the benefit
	of the public at large and to the detriment of our heirs and
	successors. We intend this dedication to be an overt act of
	relinquishment in perpetuity of all present and future rights to this
	software under copyright law.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
	OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.

	For more information, please refer to <http://unlicense.org/>

	Richard Geldreich, Jr.
	1/20/2022
*/
