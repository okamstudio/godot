/**************************************************************************/
/*  movie_writer_mjpeg.h                                                  */
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

#pragma once

#include "servers/movie_writer/movie_writer.h"

class MovieWriterMJPEG : public MovieWriter {
	GDCLASS(MovieWriterMJPEG, MovieWriter)

	uint32_t mix_rate = 48000;
	AudioServer::SpeakerMode speaker_mode = AudioServer::SPEAKER_MODE_STEREO;
	String base_path;
	uint32_t frame_count = 0;
	uint32_t fps = 0;
	float quality = 0.75;

	uint32_t audio_block_size = 0;

	Vector<uint32_t> jpg_frame_sizes;

	uint64_t total_frames_ofs = 0;
	uint64_t total_frames_ofs2 = 0;
	uint64_t total_frames_ofs3 = 0;
	uint64_t total_audio_frames_ofs4 = 0;
	uint64_t movi_data_ofs = 0;

	Ref<FileAccess> f;

protected:
	virtual uint32_t get_audio_mix_rate() const override;
	virtual AudioServer::SpeakerMode get_audio_speaker_mode() const override;
	virtual void get_supported_extensions(List<String> *r_extensions) const override;

	virtual Error write_begin(const Size2i &p_movie_size, uint32_t p_fps, const String &p_base_path) override;
	virtual Error write_frame(const Ref<Image> &p_image, const int32_t *p_audio_data) override;
	virtual void write_end() override;

	virtual bool handles_file(const String &p_path) const override;

public:
	MovieWriterMJPEG();
};
