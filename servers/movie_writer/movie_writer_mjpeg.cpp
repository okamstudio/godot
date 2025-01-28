/**************************************************************************/
/*  movie_writer_mjpeg.cpp                                                */
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

#include "movie_writer_mjpeg.h"
#include "core/config/project_settings.h"

uint32_t MovieWriterMJPEG::get_audio_mix_rate() const {
	return mix_rate;
}
AudioServer::SpeakerMode MovieWriterMJPEG::get_audio_speaker_mode() const {
	return speaker_mode;
}

bool MovieWriterMJPEG::handles_file(const String &p_path) const {
	return p_path.get_extension().to_lower() == "avi";
}

void MovieWriterMJPEG::get_supported_extensions(List<String> *r_extensions) const {
	r_extensions->push_back("avi");
}

Error MovieWriterMJPEG::write_begin(const Size2i &p_movie_size, uint32_t p_fps, const String &p_base_path) {
	// Quick & Dirty MJPEG Code based on - https://docs.microsoft.com/en-us/windows/win32/directshow/avi-riff-file-reference

	base_path = p_base_path.get_basename();
	if (base_path.is_relative_path()) {
		base_path = "res://" + base_path;
	}

	base_path += ".avi";

	f = FileAccess::open(base_path, FileAccess::WRITE_READ);

	fps = p_fps;

	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_OPEN);

	f->store_buffer((const uint8_t *)"RIFF", 4);
	f->store_32(0); // Total length (update later)
	f->store_buffer((const uint8_t *)"AVI ", 4);
	f->store_buffer((const uint8_t *)"LIST", 4);
	f->store_32(300); // 4 + 4 + 4 + 56 + 4 + 4 + 132 + 4 + 4 + 84
	f->store_buffer((const uint8_t *)"hdrl", 4);
	f->store_buffer((const uint8_t *)"avih", 4);
	f->store_32(56);

	f->store_32(1000000 / p_fps); // Microsecs per frame.
	f->store_32(7000); // Max bytes per second
	f->store_32(0); // Padding Granularity
	f->store_32(16);
	total_frames_ofs = f->get_position();
	f->store_32(0); // Total frames (update later)
	f->store_32(0); // Initial frames
	f->store_32(1); // Streams
	f->store_32(0); // Suggested buffer size
	f->store_32(p_movie_size.width); // Movie Width
	f->store_32(p_movie_size.height); // Movie Height
	for (uint32_t i = 0; i < 4; i++) {
		f->store_32(0); // Reserved.
	}
	f->store_buffer((const uint8_t *)"LIST", 4);
	f->store_32(132); // 4 + 4 + 4 + 48 + 4 + 4 + 40 + 4 + 4 + 16
	f->store_buffer((const uint8_t *)"strl", 4);
	f->store_buffer((const uint8_t *)"strh", 4);
	f->store_32(48);
	f->store_buffer((const uint8_t *)"vids", 4);
	f->store_buffer((const uint8_t *)"MJPG", 4);
	f->store_32(0); // Flags
	f->store_16(0); // Priority
	f->store_16(0); // Language
	f->store_32(0); // Initial Frames
	f->store_32(1); // Scale
	f->store_32(p_fps); // FPS
	f->store_32(0); // Start
	total_frames_ofs2 = f->get_position();
	f->store_32(0); // Number of frames (to be updated later)
	f->store_32(0); // Suggested Buffer Size
	f->store_32(0); // Quality
	f->store_32(0); // Sample Size

	f->store_buffer((const uint8_t *)"strf", 4);
	f->store_32(40); // Size.
	f->store_32(40); // Size.

	f->store_32(p_movie_size.width); // Width
	f->store_32(p_movie_size.height); // Width
	f->store_16(1); // Planes
	f->store_16(24); // Bitcount
	f->store_buffer((const uint8_t *)"MJPG", 4); // Compression

	f->store_32(((p_movie_size.width * 24 / 8 + 3) & 0xFFFFFFFC) * p_movie_size.height); // SizeImage
	f->store_32(0); // XPelsXMeter
	f->store_32(0); // YPelsXMeter
	f->store_32(0); // ClrUsed
	f->store_32(0); // ClrImportant

	f->store_buffer((const uint8_t *)"LIST", 4);
	f->store_32(16);

	f->store_buffer((const uint8_t *)"odml", 4);
	f->store_buffer((const uint8_t *)"dmlh", 4);
	f->store_32(4); // sizes

	total_frames_ofs3 = f->get_position();
	f->store_32(0); // Number of frames (to be updated later)

	// Audio //

	const uint32_t bit_depth = 32;
	uint32_t channels = 2;
	switch (speaker_mode) {
		case AudioServer::SPEAKER_MODE_STEREO:
			channels = 2;
			break;
		case AudioServer::SPEAKER_SURROUND_31:
			channels = 4;
			break;
		case AudioServer::SPEAKER_SURROUND_51:
			channels = 6;
			break;
		case AudioServer::SPEAKER_SURROUND_71:
			channels = 8;
			break;
	}
	uint32_t blockalign = bit_depth / 8 * channels;

	f->store_buffer((const uint8_t *)"LIST", 4);
	f->store_32(84); // 4 + 4 + 4 + 48 + 4 + 4 + 16
	f->store_buffer((const uint8_t *)"strl", 4);
	f->store_buffer((const uint8_t *)"strh", 4);
	f->store_32(48);
	f->store_buffer((const uint8_t *)"auds", 4);
	f->store_32(0); // Handler
	f->store_32(0); // Flags
	f->store_16(0); // Priority
	f->store_16(0); // Language
	f->store_32(0); // Initial Frames
	f->store_32(blockalign); // Scale
	f->store_32(mix_rate * blockalign); // mix rate
	f->store_32(0); // Start
	total_audio_frames_ofs4 = f->get_position();
	f->store_32(0); // Number of frames (to be updated later)
	f->store_32(12288); // Suggested Buffer Size
	f->store_32(0xFFFFFFFF); // Quality
	f->store_32(blockalign); // Block Align to 32 bits

	audio_block_size = (mix_rate / fps) * blockalign;

	f->store_buffer((const uint8_t *)"strf", 4);
	f->store_32(16); // Standard format, no extra fields
	f->store_16(1); // Compression code, standard PCM
	f->store_16(channels);
	f->store_32(mix_rate); // Samples (frames) / Sec
	f->store_32(mix_rate * blockalign); // Bytes / sec
	f->store_16(blockalign); // Bytes / sec
	f->store_16(bit_depth); // Bytes / sec

	f->store_buffer((const uint8_t *)"LIST", 4);
	movi_data_ofs = f->get_position();
	f->store_32(0); // Number of frames (to be updated later)
	f->store_buffer((const uint8_t *)"movi", 4);

	return OK;
}

Error MovieWriterMJPEG::write_frame(const Ref<Image> &p_image, const int32_t *p_audio_data) {
	ERR_FAIL_COND_V(f.is_null(), ERR_UNCONFIGURED);

	Vector<uint8_t> jpg_buffer = p_image->save_jpg_to_buffer(quality);
	uint32_t s = jpg_buffer.size();

	f->store_buffer((const uint8_t *)"00db", 4); // Stream 0, Video
	f->store_32(jpg_buffer.size()); // sizes
	f->store_buffer(jpg_buffer.ptr(), jpg_buffer.size());
	if (jpg_buffer.size() & 1) {
		f->store_8(0);
		s++;
	}
	jpg_frame_sizes.push_back(s);

	f->store_buffer((const uint8_t *)"01wb", 4); // Stream 1, Audio.
	f->store_32(audio_block_size);
	f->store_buffer((const uint8_t *)p_audio_data, audio_block_size);

	frame_count++;

	return OK;
}

void MovieWriterMJPEG::write_end() {
	if (f.is_valid()) {
		// Finalize the file (frame indices)
		f->store_buffer((const uint8_t *)"idx1", 4);
		f->store_32(8 * 4 * frame_count);
		uint32_t ofs = 4;
		uint32_t all_data_size = 0;
		for (uint32_t i = 0; i < frame_count; i++) {
			f->store_buffer((const uint8_t *)"00db", 4);
			f->store_32(16); // AVI_KEYFRAME
			f->store_32(ofs);
			f->store_32(jpg_frame_sizes[i]);

			ofs += jpg_frame_sizes[i] + 8;

			f->store_buffer((const uint8_t *)"01wb", 4);
			f->store_32(16); // AVI_KEYFRAME
			f->store_32(ofs);
			f->store_32(audio_block_size);

			ofs += audio_block_size + 8;
			all_data_size += jpg_frame_sizes[i] + audio_block_size;
		}

		uint32_t file_size = f->get_position();
		f->seek(4);
		f->store_32(file_size - 78);
		f->seek(total_frames_ofs);
		f->store_32(frame_count);
		f->seek(total_frames_ofs2);
		f->store_32(frame_count);
		f->seek(total_frames_ofs3);
		f->store_32(frame_count);
		f->seek(total_audio_frames_ofs4);
		f->store_32(frame_count * mix_rate / fps);
		f->seek(movi_data_ofs);
		f->store_32(all_data_size + 4 + 16 * frame_count);

		f.unref();
	}
}

MovieWriterMJPEG::MovieWriterMJPEG() {
	mix_rate = GLOBAL_GET("editor/movie_writer/mix_rate");
	speaker_mode = AudioServer::SpeakerMode(int(GLOBAL_GET("editor/movie_writer/speaker_mode")));
	quality = GLOBAL_GET("editor/movie_writer/mjpeg_quality");
}
