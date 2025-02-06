/**************************************************************************/
/*  video_stream_theora.cpp                                               */
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

#include "video_stream_theora.h"

#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "scene/resources/image_texture.h"

#include "thirdparty/misc/yuv2rgb.h"

int VideoStreamPlaybackTheora::buffer_data() {
	char *buffer = ogg_sync_buffer(&oy, 4096);

	uint64_t bytes = file->get_buffer((uint8_t *)buffer, 4096);
	ogg_sync_wrote(&oy, bytes);
	return bytes;
}

int VideoStreamPlaybackTheora::queue_page(ogg_page *page) {
	ogg_stream_pagein(&to, page);
	if (to.e_o_s) {
		theora_eos = true;
	}
	if (has_audio) {
		ogg_stream_pagein(&vo, page);
		if (vo.e_o_s) {
			vorbis_eos = true;
		}
	}
	return 0;
}

int VideoStreamPlaybackTheora::read_page(ogg_page *page) {
	int ret = 0;

	while (ret <= 0) {
		ret = ogg_sync_pageout(&oy, page);
		if (ret <= 0) {
			int bytes = buffer_data();
			if (bytes == 0) {
				return 0;
			}
		}
	}

	return ret;
}

double VideoStreamPlaybackTheora::get_page_time(ogg_page *page) {
	uint64_t granulepos = ogg_page_granulepos(page);
	int page_serialno = ogg_page_serialno(page);
	double page_time = -1;

	if (page_serialno == to.serialno) {
		page_time = th_granule_time(td, granulepos);
	}
	if (has_audio && page_serialno == vo.serialno) {
		page_time = vorbis_granule_time(&vd, granulepos);
	}

	return page_time;
}

// Read one buffer worth of pages and feed them to the streams.
int VideoStreamPlaybackTheora::feed_pages() {
	int pages = 0;
	ogg_page og;

	while (pages == 0) {
		while (ogg_sync_pageout(&oy, &og) > 0) {
			queue_page(&og);
			pages++;
		}
		if (pages == 0) {
			int bytes = buffer_data();
			if (bytes == 0) {
				break;
			}
		}
	}

	return pages;
}

// Find position in file from where requested granulepos video and audio pages can be read.
// Since not all pages have granulepos, we might get the previous one instead.
// Some videos duplicate granulepos instead of leaving it blank, use only the first unique one.
// Return -1 if no pages are found or granulepos is past the ones requested.
int64_t VideoStreamPlaybackTheora::seek_page(int64_t &video_granulepos, int64_t &audio_granulepos) {
	int64_t seek_pos = file->get_position();
	int64_t last_video_page_start = -1;
	int64_t last_audio_page_start = -1;
	bool have_video_page = false;
	bool have_audio_page = false;
	uint64_t last_page_seek_pos = seek_pos;
	int64_t last_video_granulepos = 0;
	int64_t start_video_granulepos = 0;
	int64_t start_audio_granulepos = 0;

	ogg_sync_reset(&oy);

	while (!have_video_page || (has_audio && !have_audio_page)) {
		ogg_page page;
		uint64_t last_seek_pos = file->get_position() - oy.fill + oy.returned;
		int ret = read_page(&page);
		if (ret <= 0) {
			// When the end of file is reached, use the last seen pages if any.
			if (last_video_page_start >= 0 && (!has_audio || last_audio_page_start >= 0)) {
				have_video_page = true;
				have_audio_page = true;
			} else {
				return -1;
			}
			break;
		}
		int64_t cur_granulepos = ogg_page_granulepos(&page);
		if (cur_granulepos >= 0) {
			int page_serialno = ogg_page_serialno(&page);
			if (!have_video_page && page_serialno == to.serialno) {
				if (cur_granulepos >= video_granulepos) {
					if (last_video_page_start >= 0) {
						have_video_page = true;
					} else {
						return -1;
					}
				} else if (cur_granulepos != last_video_granulepos) {
					last_video_page_start = last_page_seek_pos;
					start_video_granulepos = cur_granulepos;
					if (video_granulepos == INT64_MAX) {
						return last_video_page_start;
					}
				}
				last_video_granulepos = cur_granulepos;
			}
			if ((has_audio && !have_audio_page) && page_serialno == vo.serialno) {
				if (cur_granulepos >= audio_granulepos) {
					if (last_audio_page_start >= 0) {
						have_audio_page = true;
					} else {
						return -1;
					}
				} else {
					last_audio_page_start = last_page_seek_pos;
					start_audio_granulepos = cur_granulepos;
					if (audio_granulepos == INT64_MAX) {
						return last_audio_page_start;
					}
				}
			}
			last_page_seek_pos = last_seek_pos;
		}
	}

	if (seek_pos <= (int64_t)stream_data_offset) {
		seek_pos = stream_data_offset;
	} else {
		if (has_audio) {
			seek_pos = MIN(last_video_page_start, last_audio_page_start);
		} else {
			seek_pos = last_video_page_start;
		}
	}

	video_granulepos = start_video_granulepos;
	audio_granulepos = start_audio_granulepos;

	return seek_pos;
}

void VideoStreamPlaybackTheora::video_write(th_ycbcr_buffer yuv) {
	int pitch = 4;
	frame_data.resize(size.x * size.y * pitch);
	{
		uint8_t *w = frame_data.ptrw();
		char *dst = (char *)w;

		if (px_fmt == TH_PF_444) {
			yuv444_2_rgb8888((uint8_t *)dst, (uint8_t *)yuv[0].data, (uint8_t *)yuv[1].data, (uint8_t *)yuv[2].data, size.x, size.y, yuv[0].stride, yuv[1].stride, size.x << 2);

		} else if (px_fmt == TH_PF_422) {
			yuv422_2_rgb8888((uint8_t *)dst, (uint8_t *)yuv[0].data, (uint8_t *)yuv[1].data, (uint8_t *)yuv[2].data, size.x, size.y, yuv[0].stride, yuv[1].stride, size.x << 2);

		} else if (px_fmt == TH_PF_420) {
			yuv420_2_rgb8888((uint8_t *)dst, (uint8_t *)yuv[0].data, (uint8_t *)yuv[1].data, (uint8_t *)yuv[2].data, size.x, size.y, yuv[0].stride, yuv[1].stride, size.x << 2);
		}

		format = Image::FORMAT_RGBA8;
	}

	Ref<Image> img = memnew(Image(size.x, size.y, false, Image::FORMAT_RGBA8, frame_data)); //zero copy image creation
	if (region.size.x != size.x || region.size.y != size.y) {
		img = img->get_region(region);
	}

	texture->update(img); //zero copy send to rendering server
}

void VideoStreamPlaybackTheora::clear() {
	if (!file.is_null()) {
		file.unref();
	}
	if (has_audio) {
		vorbis_block_clear(&vb);
		vorbis_dsp_clear(&vd);
		vorbis_comment_clear(&vc);
		vorbis_info_clear(&vi);
		ogg_stream_clear(&vo);
	}
	if (has_video) {
		th_decode_free(td);
		th_comment_clear(&tc);
		th_info_clear(&ti);
		ogg_stream_clear(&to);
		ogg_sync_clear(&oy);
	}

	playing = false;
}

void VideoStreamPlaybackTheora::find_streams(th_setup_info *&ts) {
	ogg_stream_state test;
	ogg_packet op;
	ogg_page og;
	int stateflag = 0;
	int audio_track_skip = audio_track;

	/* Only interested in Vorbis/Theora streams */
	while (!stateflag) {
		int ret = buffer_data();
		if (!ret) {
			break;
		}
		while (ogg_sync_pageout(&oy, &og) > 0) {
			/* is this a mandated initial header? If not, stop parsing */
			if (!ogg_page_bos(&og)) {
				/* don't leak the page; get it into the appropriate stream */
				queue_page(&og);
				stateflag = 1;
				break;
			}

			ogg_stream_init(&test, ogg_page_serialno(&og));
			ogg_stream_pagein(&test, &og);
			ogg_stream_packetout(&test, &op);

			/* identify the codec: try theora */
			if (!has_video && th_decode_headerin(&ti, &tc, &ts, &op) >= 0) {
				/* it is theora */
				memcpy(&to, &test, sizeof(test));
				has_video = true;
			} else if (!has_audio && vorbis_synthesis_headerin(&vi, &vc, &op) >= 0) {
				/* it is vorbis */
				if (audio_track_skip) {
					vorbis_info_clear(&vi);
					vorbis_comment_clear(&vc);
					ogg_stream_clear(&test);
					vorbis_info_init(&vi);
					vorbis_comment_init(&vc);
					audio_track_skip--;
				} else {
					memcpy(&vo, &test, sizeof(test));
					has_audio = true;
				}
			} else {
				/* whatever it is, we don't care about it */
				ogg_stream_clear(&test);
			}
		}
	}
}

void VideoStreamPlaybackTheora::read_headers(th_setup_info *&ts) {
	ogg_packet op;
	int theora_header_packets = 1;
	int vorbis_header_packets = 1;

	/* we're expecting more header packets. */
	while (theora_header_packets < 3 || (has_audio && vorbis_header_packets < 3)) {
		/* look for further theora headers */
		// The API says there can be more than three but only three are mandatory.
		while (theora_header_packets < 3 && ogg_stream_packetout(&to, &op) > 0) {
			if (th_decode_headerin(&ti, &tc, &ts, &op) > 0) {
				theora_header_packets++;
			}
		}

		/* look for more vorbis header packets */
		while (has_audio && vorbis_header_packets < 3 && ogg_stream_packetout(&vo, &op) > 0) {
			if (!vorbis_synthesis_headerin(&vi, &vc, &op)) {
				vorbis_header_packets++;
			}
		}

		/* The header pages/packets will arrive before anything else we
		   care about, or the stream is not obeying spec */
		if (theora_header_packets < 3 || (has_audio && vorbis_header_packets < 3)) {
			ogg_page page;
			if (read_page(&page)) {
				queue_page(&page);
			} else {
				fprintf(stderr, "End of file while searching for codec headers.\n");
				break;
			}
		}
	}

	has_video = theora_header_packets == 3;
	has_audio = vorbis_header_packets == 3;
}

void VideoStreamPlaybackTheora::set_file(const String &p_file) {
	ERR_FAIL_COND(playing);
	th_setup_info *ts = nullptr;

	file_name = p_file;
	file = FileAccess::open(p_file, FileAccess::READ);
	ERR_FAIL_COND_MSG(file.is_null(), "Cannot open file '" + p_file + "'.");

	ogg_sync_init(&oy);

	/* init supporting Vorbis structures needed in header parsing */
	vorbis_info_init(&vi);
	vorbis_comment_init(&vc);

	/* init supporting Theora structures needed in header parsing */
	th_comment_init(&tc);
	th_info_init(&ti);

	/* Zero stream state structs so they can be checked later. */
	memset(&to, 0, sizeof(to));
	memset(&vo, 0, sizeof(vo));

	has_video = false;
	has_audio = false;
	theora_eos = false;
	vorbis_eos = false;
	playing = false;

	/* Ogg file open; parse the headers */
	find_streams(ts);
	read_headers(ts);

	if (!has_audio) {
		vorbis_comment_clear(&vc);
		vorbis_info_clear(&vi);
		if (!ogg_stream_check(&vo)) {
			ogg_stream_clear(&vo);
		}
	}

	// One video stream is mandatory.
	if (!has_video) {
		th_setup_free(ts);
		th_comment_clear(&tc);
		th_info_clear(&ti);
		if (!ogg_stream_check(&to)) {
			ogg_stream_clear(&to);
		}
		return;
	}

	/* And now we have it all. Initialize decoders. */
	td = th_decode_alloc(&ti, ts);
	th_setup_free(ts);
	px_fmt = ti.pixel_fmt;
	switch (ti.pixel_fmt) {
		case TH_PF_420:
			//printf(" 4:2:0 video\n");
			break;
		case TH_PF_422:
			//printf(" 4:2:2 video\n");
			break;
		case TH_PF_444:
			//printf(" 4:4:4 video\n");
			break;
		case TH_PF_RSVD:
		default:
			printf(" video\n  (UNKNOWN Chroma sampling!)\n");
			break;
	}
	th_decode_ctl(td, TH_DECCTL_GET_PPLEVEL_MAX, &pp_level_max, sizeof(pp_level_max));
	pp_level = 0;
	th_decode_ctl(td, TH_DECCTL_SET_PPLEVEL, &pp_level, sizeof(pp_level));
	pp_inc = 0;

	size.x = ti.frame_width;
	size.y = ti.frame_height;
	region.position.x = ti.pic_x;
	region.position.y = ti.pic_y;
	region.size.x = ti.pic_width;
	region.size.y = ti.pic_height;

	Ref<Image> img = Image::create_empty(region.size.x, region.size.y, false, Image::FORMAT_RGBA8);
	texture->set_image(img);

	frame_duration = (double)ti.fps_denominator / ti.fps_numerator;

	if (has_audio) {
		vorbis_synthesis_init(&vd, &vi);
		vorbis_block_init(&vd, &vb);
	}

	stream_data_offset = file->get_position() - oy.fill + oy.returned;
	stream_data_size = file->get_length() - stream_data_offset;

	// Sync to last page to find video length.
	int64_t seek_pos = MAX(stream_data_offset, (int64_t)file->get_length() - 64 * 1024);
	int64_t video_granulepos = INT64_MAX;
	int64_t audio_granulepos = INT64_MAX;
	file->seek(seek_pos);
	seek_pos = seek_page(video_granulepos, audio_granulepos);
	file->seek(seek_pos);
	ogg_sync_reset(&oy);

	stream_length = 0;
	ogg_page page;
	while (read_page(&page) > 0) {
		// Use MAX because, even though pages are ordered, page time can be -1
		// for pages without full frames. Streams could be truncated too.
		stream_length = MAX(stream_length, get_page_time(&page));
	}

	seek(0);
}

double VideoStreamPlaybackTheora::get_time() const {
	// FIXME: AudioServer output latency was fixed in af9bb0e, previously it used to
	// systematically return 0. Now that it gives a proper latency, it broke this
	// code where the delay compensation likely never really worked.
	return time - /* AudioServer::get_singleton()->get_output_latency() - */ delay_compensation;
}

Ref<Texture2D> VideoStreamPlaybackTheora::get_texture() const {
	return texture;
}

void VideoStreamPlaybackTheora::update(double p_delta) {
	if (file.is_null()) {
		return;
	}

	if (!playing || paused) {
		return;
	}

	time += p_delta;

	double comp_time = get_time();
	bool audio_ready = false;

	// Read data until we fill the audio buffer and get a new video frame.
	while ((!audio_ready && !audio_done) || (!video_ready && !video_done)) {
		ogg_packet op;

		while (!audio_ready && !audio_done) {
			float **pcm;
			int ret = vorbis_synthesis_pcmout(&vd, &pcm);
			if (ret > 0) {
				const int AUXBUF_LEN = 4096;
				int to_read = ret;
				float aux_buffer[AUXBUF_LEN];
				while (to_read) {
					int m = MIN(AUXBUF_LEN / vi.channels, to_read);
					int count = 0;
					for (int j = 0; j < m; j++) {
						for (int i = 0; i < vi.channels; i++) {
							aux_buffer[count++] = pcm[i][j];
						}
					}
					int mixed = mix_callback(mix_udata, aux_buffer, m);
					to_read -= mixed;
					if (mixed != m) { //could mix no more
						audio_ready = true;
						break;
					}
				}
				vorbis_synthesis_read(&vd, ret - to_read);
			} else {
				/* no pending audio; is there a pending packet to decode? */
				if (ogg_stream_packetout(&vo, &op) > 0) {
					if (vorbis_synthesis(&vb, &op) == 0) { /* test for success! */
						vorbis_synthesis_blockin(&vd, &vb);
					}
				} else { /* we need more data; break out to suck in another page */
					audio_done = vorbis_eos;
					break;
				}
			}
		}

		while (!video_ready && !video_done) {
			if (ogg_stream_packetout(&to, &op) > 0) {
				if (pp_inc) {
					pp_level += pp_inc;
					th_decode_ctl(td, TH_DECCTL_SET_PPLEVEL, &pp_level, sizeof(pp_level));
					pp_inc = 0;
				}
				if (op.granulepos >= 0) {
					th_decode_ctl(td, TH_DECCTL_SET_GRANPOS, &op.granulepos, sizeof(op.granulepos));
				}
				int64_t videobuf_granulepos;
				int ret = th_decode_packetin(td, &op, &videobuf_granulepos);
				if (ret == 0 || ret == TH_DUPFRAME) {
					next_frame_time = th_granule_time(td, videobuf_granulepos);
					if (next_frame_time > comp_time) {
						dup_frame = (ret == TH_DUPFRAME);
						video_ready = true;
					} else {
						/*If we are too slow, reduce the pp level.*/
						pp_inc = pp_level > 0 ? -1 : 0;
					}
				}
			} else { /* we need more data; break out to suck in another page */
				video_done = theora_eos;
				break;
			}
		}

		if (!video_ready || !audio_ready) {
			int ret = buffer_data();
			ogg_page og;
			if (ret > 0) {
				while (ogg_sync_pageout(&oy, &og) > 0) {
					queue_page(&og);
				}
			} else {
				vorbis_eos = true;
				theora_eos = true;
				break;
			}
		}

		double tdiff = next_frame_time - comp_time;
		/*If we have lots of extra time, increase the post-processing level.*/
		if (tdiff > frame_duration * 0.25) {
			pp_inc = pp_level < pp_level_max && pp_level < pp_level_requested ? 1 : 0;
		} else if (tdiff < frame_duration * 0.05) {
			pp_inc = pp_level > 0 ? -1 : 0;
		}
	}

	if (!video_ready && video_done && audio_done) {
		stop();
		return;
	}

	// Wait for the last frame to end before rendering the next one.
	if (video_ready && comp_time >= current_frame_time) {
		if (!dup_frame) {
			th_ycbcr_buffer yuv;
			th_decode_ycbcr_out(td, yuv);
			video_write(yuv);
		}
		dup_frame = false;
		video_ready = false;
		current_frame_time = next_frame_time;
	}
}

void VideoStreamPlaybackTheora::play() {
	if (playing) {
		return;
	}

	playing = true;
	delay_compensation = GLOBAL_GET("audio/video/video_delay_compensation_ms");
	delay_compensation /= 1000.0;
}

void VideoStreamPlaybackTheora::stop() {
	playing = false;
	seek(0);
}

bool VideoStreamPlaybackTheora::is_playing() const {
	return playing;
}

void VideoStreamPlaybackTheora::set_paused(bool p_paused) {
	paused = p_paused;
}

bool VideoStreamPlaybackTheora::is_paused() const {
	return paused;
}

double VideoStreamPlaybackTheora::get_length() const {
	return stream_length;
}

double VideoStreamPlaybackTheora::get_playback_position() const {
	return get_time();
}

void VideoStreamPlaybackTheora::seek(double p_time) {
	if (file.is_null()) {
		return;
	}

	time = p_time;
	video_ready = false;
	next_frame_time = 0;
	current_frame_time = 0;
	dup_frame = false;
	video_done = false;
	audio_done = !has_audio;
	theora_eos = false;
	vorbis_eos = false;

	ogg_stream_reset(&to);
	if (has_audio) {
		ogg_stream_reset(&vo);
		vorbis_synthesis_restart(&vd);
	}

	// Make a guess where in the file we should start reading and scan from there.
	// We base the guess on the mean bitrate of the file. It would be theoretically faster to use the bisect method but
	// in practice there's a lot of linear scanning to do to find the right pages.
	// We want to catch the previous keyframe to the seek time. Since we only know the max GOP, we use that.
	int64_t video_frame = (int64_t)(p_time / frame_duration);
	int64_t video_granulepos = MAX(0LL, video_frame - (1LL << ti.keyframe_granule_shift)) << ti.keyframe_granule_shift;
	int64_t audio_granulepos = 0;
	int64_t rewind_size = 2 * ((1LL << (ti.keyframe_granule_shift))) * frame_duration * stream_data_size / stream_length;
	int64_t seek_pos = (video_granulepos >> ti.keyframe_granule_shift) * frame_duration * stream_data_size / stream_length + stream_data_offset - rewind_size;

	if (has_audio) {
		audio_granulepos = video_frame * frame_duration * vi.rate;
	}

	// Align to 4096 blocks.
	seek_pos -= seek_pos % 4096;
	rewind_size -= rewind_size % 4096;

	// Find file position to start decoding.
	// When successful, video_granulepos and audio_granulepos will be the granules where decoding should start.
	while (seek_pos > stream_data_offset) {
		file->seek(seek_pos);
		int64_t ret = seek_page(video_granulepos, audio_granulepos);
		if (ret == -1) {
			seek_pos -= rewind_size;
		} else {
			seek_pos = ret;
			break;
		}
	}

	if (seek_pos <= stream_data_offset) {
		seek_pos = stream_data_offset;
	}

	file->seek(seek_pos);
	ogg_sync_reset(&oy);

	// Start decoding until we reach the requested seek time.
	int64_t granulepos = 1;
	th_decode_ctl(td, TH_DECCTL_SET_GRANPOS, &granulepos, sizeof(granulepos));

	// Set post-processing to lowest to be faster
	pp_level = 0;
	th_decode_ctl(td, TH_DECCTL_SET_PPLEVEL, &pp_level, sizeof(pp_level));

	double last_audio_time = 0;
	double last_video_time = 0;
	bool first_frame_decoded = false;
	bool start_audio = false;
	bool start_video = false;
	while ((has_audio && last_audio_time < p_time) || (last_video_time < p_time)) {
		ogg_packet op;
		feed_pages();
		while (has_audio && last_audio_time < p_time && ogg_stream_packetout(&vo, &op) > 0) {
			if (op.granulepos >= 0) {
				last_audio_time = vorbis_granule_time(&vd, op.granulepos);
				if (op.granulepos >= audio_granulepos) {
					start_audio = true;
				}
			}
			if (start_audio) {
				if (last_audio_time == 0) {
					vorbis_synthesis_trackonly(&vb, &op);
				} else if (last_audio_time < p_time) {
					if (vorbis_synthesis(&vb, &op) == 0) { /* test for success! */
						float **pcm;
						vorbis_synthesis_blockin(&vd, &vb);
						int ret;
						do { /* Consume everything */
							ret = vorbis_synthesis_pcmout(&vd, &pcm);
							int diff = ceil((p_time - last_audio_time) * vi.rate);
							int read = MIN(ret, diff);
							vorbis_synthesis_read(&vd, read);
							last_audio_time += (double)read / vi.rate;
						} while (ret > 0 && last_audio_time < p_time);
					}
				}
			}
		}
		while (last_video_time < p_time && ogg_stream_packetout(&to, &op) > 0) {
			if (op.granulepos >= 0) {
				if (op.granulepos >= video_granulepos) {
					start_video = true;
				}
				th_decode_ctl(td, TH_DECCTL_SET_GRANPOS, &op.granulepos, sizeof(op.granulepos));
				first_frame_decoded = true;
			}
			if (start_video) {
				int64_t videobuf_granulepos;
				int ret = th_decode_packetin(td, &op, &videobuf_granulepos);
				if (ret == 0 || ret == TH_DUPFRAME) {
					last_video_time = th_granule_time(td, videobuf_granulepos);
				}
			}
		}
	}

	if (first_frame_decoded) {
		// Draw the current frame.
		th_ycbcr_buffer yuv;
		th_decode_ycbcr_out(td, yuv);
		video_write(yuv);
	}

	pp_level = pp_level_requested;
	th_decode_ctl(td, TH_DECCTL_SET_PPLEVEL, &pp_level, sizeof(pp_level));
	pp_inc = 0;
}

int VideoStreamPlaybackTheora::get_channels() const {
	return vi.channels;
}

void VideoStreamPlaybackTheora::set_audio_track(int p_idx) {
	audio_track = p_idx;
}

int VideoStreamPlaybackTheora::get_mix_rate() const {
	return vi.rate;
}

void VideoStreamPlaybackTheora::set_pp_level(int p_pp_level) {
	pp_level_requested = p_pp_level;
}

VideoStreamPlaybackTheora::VideoStreamPlaybackTheora() {
	texture.instantiate();
}

VideoStreamPlaybackTheora::~VideoStreamPlaybackTheora() {
	clear();
}

void VideoStreamTheora::_bind_methods() {}

Ref<Resource> ResourceFormatLoaderTheora::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		if (r_error) {
			*r_error = ERR_CANT_OPEN;
		}
		return Ref<Resource>();
	}

	VideoStreamTheora *stream = memnew(VideoStreamTheora);
	stream->set_file(p_path);

	Ref<VideoStreamTheora> ogv_stream = Ref<VideoStreamTheora>(stream);

	if (r_error) {
		*r_error = OK;
	}

	return ogv_stream;
}

void ResourceFormatLoaderTheora::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("ogv");
}

bool ResourceFormatLoaderTheora::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "VideoStream");
}

String ResourceFormatLoaderTheora::get_resource_type(const String &p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == "ogv") {
		return "VideoStreamTheora";
	}
	return "";
}
