#ifndef __PROCESSDXT1_HPP__
#define __PROCESSDXT1_HPP__

#include <stddef.h>
#include <stdint.h>

void CompressBc1( const uint32_t* src, uint64_t* dst, uint32_t blocks, size_t width );
void CompressBc1Dither( const uint32_t* src, uint64_t* dst, uint32_t blocks, size_t width );
void CompressBc3( const uint32_t* src, uint64_t* dst, uint32_t blocks, size_t width );

void CompressBc4( const uint32_t* src, uint64_t* dst, uint32_t blocks, size_t width );
void CompressBc5( const uint32_t* src, uint64_t* dst, uint32_t blocks, size_t width );

#endif
