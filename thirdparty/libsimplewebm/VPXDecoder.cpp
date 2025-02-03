/*
	MIT License

	Copyright (c) 2016 Błażej Szczygieł

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/

#include "VPXDecoder.hpp"

#include <vpx/vpx_decoder.h>
#include <vpx/vp8dx.h>

#include <stdlib.h>
#include <string.h>

VPXDecoder::VPXDecoder(const WebMDemuxer &demuxer, unsigned threads) :
	m_ctx(NULL),
	m_iter(NULL),
	m_delay(0),
	m_last_space(VPX_CS_UNKNOWN)
{
	if (threads > 8)
		threads = 8;
	else if (threads < 1)
		threads = 1;

	const vpx_codec_dec_cfg_t codecCfg = {
		threads,
		0,
		0
	};
	vpx_codec_iface_t *codecIface = NULL;

	switch (demuxer.getVideoCodec())
	{
		case WebMDemuxer::VIDEO_VP8:
			codecIface = vpx_codec_vp8_dx();
			break;
		case WebMDemuxer::VIDEO_VP9:
			codecIface = vpx_codec_vp9_dx();
			m_delay = threads - 1;
			break;
		default:
			return;
	}

	m_ctx = new vpx_codec_ctx_t;
	
	// disable multi thread for decoder to make some vp8 and vp9 video files work properly
	// refertence url https://github.com/dimiaa/WebM-Video-Player/blob/main/src/WebMVideoDecoder.cpp
	int codecFlags = 0;
	if (vpx_codec_dec_init(m_ctx, codecIface, &codecCfg, codecFlags))
	{
		delete m_ctx;
		m_ctx = NULL;
	}
}
VPXDecoder::~VPXDecoder()
{
	if (m_ctx)
	{
		vpx_codec_destroy(m_ctx);
		delete m_ctx;
	}
}

bool VPXDecoder::decode(const WebMFrame &frame)
{
	m_iter = NULL;
	return !vpx_codec_decode(m_ctx, frame.buffer, frame.bufferSize, NULL, 0);
}
VPXDecoder::IMAGE_ERROR VPXDecoder::getImage(Image &image)
{
	IMAGE_ERROR err = NO_FRAME;
	if (vpx_image_t *img = vpx_codec_get_frame(m_ctx, &m_iter))
	{
		// It seems to be a common problem that UNKNOWN comes up a lot, yet FFMPEG is somehow getting accurate colour-space information.
		// After checking FFMPEG code, *they're* getting colour-space information, so I'm assuming something like this is going on.
		// It appears to work, at least.
		if (img->cs != VPX_CS_UNKNOWN)
			m_last_space = img->cs;
		if ((img->fmt & VPX_IMG_FMT_PLANAR) && !(img->fmt & (VPX_IMG_FMT_HAS_ALPHA | VPX_IMG_FMT_HIGHBITDEPTH)))
		{
			if (img->stride[0] && img->stride[1] && img->stride[2])
			{
				const int uPlane = !!(img->fmt & VPX_IMG_FMT_UV_FLIP) + 1;
				const int vPlane =  !(img->fmt & VPX_IMG_FMT_UV_FLIP) + 1;

				image.w = img->d_w;
				image.h = img->d_h;
				image.cs = m_last_space;
				image.chromaShiftW = img->x_chroma_shift;
				image.chromaShiftH = img->y_chroma_shift;

				image.planes[0] = img->planes[0];
				image.planes[1] = img->planes[uPlane];
				image.planes[2] = img->planes[vPlane];

				image.linesize[0] = img->stride[0];
				image.linesize[1] = img->stride[uPlane];
				image.linesize[2] = img->stride[vPlane];

				err = NO_ERROR;
			}
		}
		else
		{
			err = UNSUPPORTED_FRAME;
		}
	}
	return err;
}

/**/

// -- GODOT begin --
#if 0
// -- GODOT end --

static inline int ceilRshift(int val, int shift)
{
	return (val + (1 << shift) - 1) >> shift;
}

int VPXDecoder::Image::getWidth(int plane) const
{
	if (!plane)
		return w;
	return ceilRshift(w, chromaShiftW);
}
int VPXDecoder::Image::getHeight(int plane) const
{
	if (!plane)
		return h;
	return ceilRshift(h, chromaShiftH);
}

// -- GODOT begin --
#endif
// -- GODOT end --

