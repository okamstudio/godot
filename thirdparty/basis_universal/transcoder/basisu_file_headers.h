// basis_file_headers.h
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#pragma once
#include "basisu_transcoder_internal.h"

namespace basist
{
	// Slice desc header flags
	enum basis_slice_desc_flags
	{
		cSliceDescFlagsHasAlpha = 1,
		
		// Video only: Frame doesn't refer to previous frame (no usage of conditional replenishment pred symbols)
		// Currently the first frame is always an I-Frame, all subsequent frames are P-Frames. This will eventually be changed to periodic I-Frames.
		cSliceDescFlagsFrameIsIFrame = 2			
	};

#pragma pack(push)
#pragma pack(1)
	struct basis_slice_desc
	{
		basisu::packed_uint<3> m_image_index;  // The index of the source image provided to the encoder (will always appear in order from first to last, first image index is 0, no skipping allowed)
		basisu::packed_uint<1> m_level_index;	// The mipmap level index (mipmaps will always appear from largest to smallest)
		basisu::packed_uint<1> m_flags;			// enum basis_slice_desc_flags

		basisu::packed_uint<2> m_orig_width;	// The original image width (may not be a multiple of 4 pixels)
		basisu::packed_uint<2> m_orig_height;  // The original image height (may not be a multiple of 4 pixels)

		basisu::packed_uint<2> m_num_blocks_x;	// The slice's block X dimensions. Each block is 4x4 pixels. The slice's pixel resolution may or may not be a power of 2.
		basisu::packed_uint<2> m_num_blocks_y;	// The slice's block Y dimensions. 

		basisu::packed_uint<4> m_file_ofs;		// Offset from the start of the file to the start of the slice's data
		basisu::packed_uint<4> m_file_size;		// The size of the compressed slice data in bytes

		basisu::packed_uint<2> m_slice_data_crc16; // The CRC16 of the compressed slice data, for extra-paranoid use cases
	};

	// File header files
	enum basis_header_flags
	{
		// Always set for ETC1S files. Not set for UASTC files.
		cBASISHeaderFlagETC1S = 1,					 
		
		// Set if the texture had to be Y flipped before encoding. The actual interpretation of this (is Y up or down?) is up to the user.
		cBASISHeaderFlagYFlipped = 2,				 
		
		// Set if any slices contain alpha (for ETC1S, if the odd slices contain alpha data)
		cBASISHeaderFlagHasAlphaSlices = 4,		 
		
		// For ETC1S files, this will be true if the file utilizes a codebook from another .basis file. 
		cBASISHeaderFlagUsesGlobalCodebook = 8, 
		
		// Set if the texture data is sRGB, otherwise it's linear. 
		// In reality, we have no idea if the texture data is actually linear or sRGB. This is the m_perceptual parameter passed to the compressor.
		cBASISHeaderFlagSRGB = 16,					 
	};

	// The image type field attempts to describe how to interpret the image data in a Basis file.
	// The encoder library doesn't really do anything special or different with these texture types, this is mostly here for the benefit of the user. 
	// We do make sure the various constraints are followed (2DArray/cubemap/videoframes/volume implies that each image has the same resolution and # of mipmap levels, etc., cubemap implies that the # of image slices is a multiple of 6)
	enum basis_texture_type
	{
		cBASISTexType2D = 0,					// An arbitrary array of 2D RGB or RGBA images with optional mipmaps, array size = # images, each image may have a different resolution and # of mipmap levels
		cBASISTexType2DArray = 1,			// An array of 2D RGB or RGBA images with optional mipmaps, array size = # images, each image has the same resolution and mipmap levels
		cBASISTexTypeCubemapArray = 2,	// an array of cubemap levels, total # of images must be divisable by 6, in X+, X-, Y+, Y-, Z+, Z- order, with optional mipmaps
		cBASISTexTypeVideoFrames = 3,		// An array of 2D video frames, with optional mipmaps, # frames = # images, each image has the same resolution and # of mipmap levels
		cBASISTexTypeVolume = 4,			// A 3D texture with optional mipmaps, Z dimension = # images, each image has the same resolution and # of mipmap levels

		cBASISTexTypeTotal
	};

	enum
	{
		cBASISMaxUSPerFrame = 0xFFFFFF
	};

	enum class basis_tex_format
	{
		cETC1S = 0,
		cUASTC4x4 = 1,
		cUASTC_HDR_4x4 = 2
	};

	struct basis_file_header
	{
		enum
		{
			cBASISSigValue = ('B' << 8) | 's',
			cBASISFirstVersion = 0x10
		};

		basisu::packed_uint<2>      m_sig;				// 2 byte file signature
		basisu::packed_uint<2>      m_ver;				// Baseline file version
		basisu::packed_uint<2>      m_header_size;	// Header size in bytes, sizeof(basis_file_header)
		basisu::packed_uint<2>      m_header_crc16;	// CRC16 of the remaining header data

		basisu::packed_uint<4>      m_data_size;		// The total size of all data after the header
		basisu::packed_uint<2>      m_data_crc16;		// The CRC16 of all data after the header

		basisu::packed_uint<3>      m_total_slices;	// The total # of compressed slices (1 slice per image, or 2 for alpha .basis files)

		basisu::packed_uint<3>      m_total_images;	// The total # of images
				
		basisu::packed_uint<1>      m_tex_format;		// enum basis_tex_format
		basisu::packed_uint<2>      m_flags;			// enum basist::header_flags
		basisu::packed_uint<1>      m_tex_type;		// enum basist::basis_texture_type
		basisu::packed_uint<3>      m_us_per_frame;	// Framerate of video, in microseconds per frame

		basisu::packed_uint<4>      m_reserved;		// For future use
		basisu::packed_uint<4>      m_userdata0;		// For client use
		basisu::packed_uint<4>      m_userdata1;		// For client use

		basisu::packed_uint<2>      m_total_endpoints;			// The number of endpoints in the endpoint codebook 
		basisu::packed_uint<4>      m_endpoint_cb_file_ofs;	// The compressed endpoint codebook's file offset relative to the start of the file
		basisu::packed_uint<3>      m_endpoint_cb_file_size;	// The compressed endpoint codebook's size in bytes

		basisu::packed_uint<2>      m_total_selectors;			// The number of selectors in the endpoint codebook 
		basisu::packed_uint<4>      m_selector_cb_file_ofs;	// The compressed selectors codebook's file offset relative to the start of the file
		basisu::packed_uint<3>      m_selector_cb_file_size;	// The compressed selector codebook's size in bytes

		basisu::packed_uint<4>      m_tables_file_ofs;			// The file offset of the compressed Huffman codelength tables, for decompressing slices
		basisu::packed_uint<4>      m_tables_file_size;			// The file size in bytes of the compressed huffman codelength tables

		basisu::packed_uint<4>      m_slice_desc_file_ofs;		// The file offset to the slice description array, usually follows the header
		
		basisu::packed_uint<4>      m_extended_file_ofs;		// The file offset of the "extended" header and compressed data, for future use
		basisu::packed_uint<4>      m_extended_file_size;		// The file size in bytes of the "extended" header and compressed data, for future use
	};
#pragma pack (pop)

} // namespace basist
