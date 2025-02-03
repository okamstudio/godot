/**************************************************************************/
/*  video_stream_webm.cpp                                                 */
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

#include "video_stream_webm.h"

#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "servers/audio_server.h"

#include "thirdparty/misc/yuv2rgb.h"

// libsimplewebm
#include <OpusVorbisDecoder.hpp>
#include <VPXDecoder.hpp>

// libvpx
#include <vpx/vpx_image.h>

// libwebm
#include <mkvparser/mkvparser.h>

class MkvReader : public mkvparser::IMkvReader {
public:
	MkvReader(const String &p_file) {
		file = FileAccess::open(p_file, FileAccess::READ);

		ERR_FAIL_COND_MSG(!file, "Failed loading resource: '" + p_file + "'.");
	}
	~MkvReader() {
		if (file) {
			memdelete(file);
		}
	}

	virtual int Read(long long pos, long len, unsigned char *buf) {
		if (file) {
			if (file->get_position() != (uint64_t)pos) {
				file->seek(pos);
			}
			if (file->get_buffer(buf, len) == (uint64_t)len) {
				return 0;
			}
		}
		return -1;
	}

	virtual int Length(long long *total, long long *available) {
		if (file) {
			const uint64_t len = file->get_len();
			if (total) {
				*total = len;
			}
			if (available) {
				*available = len;
			}
			return 0;
		}
		return -1;
	}

private:
	FileAccess *file;
};

/**/

VideoStreamPlaybackWebm::VideoStreamPlaybackWebm() :
		audio_track(0),
		webm(nullptr),
		video(nullptr),
		audio(nullptr),
		video_frames(nullptr),
		audio_frame(nullptr),
		video_frames_pos(0),
		video_frames_capacity(0),
		num_decoded_samples(0),
		samples_offset(-1),
		mix_callback(nullptr),
		mix_udata(nullptr),
		playing(false),
		paused(false),
		delay_compensation(0.0),
		time(0.0),
		video_frame_delay(0.0),
		video_pos(0.0),
		texture(memnew(ImageTexture)),
		pcm(nullptr) {}
VideoStreamPlaybackWebm::~VideoStreamPlaybackWebm() {
	delete_pointers();
}

bool VideoStreamPlaybackWebm::open_file(const String &p_file) {
	file_name = p_file;
	webm = memnew(WebMDemuxer(new MkvReader(file_name), 0, audio_track));
	if (webm->isOpen()) {
		video = memnew(VPXDecoder(*webm, OS::get_singleton()->get_processor_count()));
		if (video->isOpen()) {
			audio = memnew(OpusVorbisDecoder(*webm));
			if (audio->isOpen()) {
				audio_frame = memnew(WebMFrame);
				pcm = (float *)memalloc(sizeof(float) * audio->getBufferSamples() * webm->getChannels());
			} else {
				memdelete(audio);
				audio = nullptr;
			}

			frame_data.resize((webm->getWidth() * webm->getHeight()) << 2);
			texture->create(webm->getWidth(), webm->getHeight(), Image::FORMAT_RGBA8, Texture::FLAG_FILTER | Texture::FLAG_VIDEO_SURFACE);

			return true;
		}
		memdelete(video);
		video = nullptr;
	}
	memdelete(webm);
	webm = nullptr;
	return false;
}

void VideoStreamPlaybackWebm::stop() {
	if (playing) {
		delete_pointers();

		pcm = nullptr;

		audio_frame = nullptr;
		video_frames = nullptr;

		video = nullptr;
		audio = nullptr;

		open_file(file_name); //Should not fail here...

		video_frames_capacity = video_frames_pos = 0;
		num_decoded_samples = 0;
		samples_offset = -1;
		video_frame_delay = video_pos = 0.0;
	}
	time = 0.0;
	playing = false;
}
void VideoStreamPlaybackWebm::play() {
	stop();

	delay_compensation = ProjectSettings::get_singleton()->get("audio/video_delay_compensation_ms");
	delay_compensation /= 1000.0;

	playing = true;
}

bool VideoStreamPlaybackWebm::is_playing() const {
	return playing;
}

void VideoStreamPlaybackWebm::set_paused(bool p_paused) {
	paused = p_paused;
}
bool VideoStreamPlaybackWebm::is_paused() const {
	return paused;
}

void VideoStreamPlaybackWebm::set_loop(bool p_enable) {
	//Empty
}
bool VideoStreamPlaybackWebm::has_loop() const {
	return false;
}

float VideoStreamPlaybackWebm::get_length() const {
	if (webm) {
		return webm->getLength();
	}
	return 0.0f;
}

float VideoStreamPlaybackWebm::get_playback_position() const {
	return video_pos;
}
void VideoStreamPlaybackWebm::seek(float p_time) {
	if (webm) {
		time = webm->seek(p_time);
		video_pos = time;
	}
}

void VideoStreamPlaybackWebm::set_audio_track(int p_idx) {
	audio_track = p_idx;
}

Ref<Texture> VideoStreamPlaybackWebm::get_texture() const {
	return texture;
}

void VideoStreamPlaybackWebm::update(float p_delta) {
	if ((!playing || paused) || !video) {
		return;
	}

	time += p_delta;

	if (time < video_pos) {
		return;
	}

	bool audio_buffer_full = false;

	if (samples_offset > -1) {
		//Mix remaining samples
		const int to_read = num_decoded_samples - samples_offset;
		const int mixed = mix_callback(mix_udata, pcm + samples_offset * webm->getChannels(), to_read);
		if (mixed != to_read) {
			samples_offset += mixed;
			audio_buffer_full = true;
		} else {
			samples_offset = -1;
		}
	}

	const bool hasAudio = (audio && mix_callback);
	while ((hasAudio && !audio_buffer_full && !has_enough_video_frames()) ||
			(!hasAudio && video_frames_pos == 0)) {
		if (hasAudio && !audio_buffer_full && audio_frame->isValid() &&
				audio->getPCMF(*audio_frame, pcm, num_decoded_samples) && num_decoded_samples > 0) {
			const int mixed = mix_callback(mix_udata, pcm, num_decoded_samples);

			if (mixed != num_decoded_samples) {
				samples_offset = mixed;
				audio_buffer_full = true;
			}
		}

		WebMFrame *video_frame;
		if (video_frames_pos >= video_frames_capacity) {
			WebMFrame **video_frames_new = (WebMFrame **)memrealloc(video_frames, ++video_frames_capacity * sizeof(void *));
			ERR_FAIL_COND(!video_frames_new); //Out of memory
			(video_frames = video_frames_new)[video_frames_capacity - 1] = memnew(WebMFrame);
		}
		video_frame = video_frames[video_frames_pos];

		if (!webm->readFrame(video_frame, audio_frame)) { //This will invalidate frames
			break; //Can't demux, EOS?
		}

		if (video_frame->isValid()) {
			++video_frames_pos;
		}
	};

	bool video_frame_done = false;
	while (video_frames_pos > 0 && !video_frame_done) {
		WebMFrame *video_frame = video_frames[0];

		// It seems VPXDecoder::decode has to be executed even though we might skip this frame
		if (video->decode(*video_frame)) {
			VPXDecoder::IMAGE_ERROR err;
			VPXDecoder::Image image;

			if (should_process(*video_frame)) {
				if ((err = video->getImage(image)) != VPXDecoder::NO_FRAME) {
					if (err == VPXDecoder::NO_ERROR && image.w == webm->getWidth() && image.h == webm->getHeight()) {
						PoolVector<uint8_t>::Write w = frame_data.write();
						bool converted = false;

						if (image.chromaShiftW == 0 && image.chromaShiftH == 0 && image.cs == VPX_CS_SRGB) {
							uint8_t *wp = w.ptr();
							unsigned char *rRow = image.planes[2];
							unsigned char *gRow = image.planes[0];
							unsigned char *bRow = image.planes[1];
							for (int i = 0; i < image.h; i++) {
								for (int j = 0; j < image.w; j++) {
									*wp++ = rRow[j];
									*wp++ = gRow[j];
									*wp++ = bRow[j];
									*wp++ = 255;
								}
								rRow += image.linesize[2];
								gRow += image.linesize[0];
								bRow += image.linesize[1];
							}
							converted = true;
						} else if (image.chromaShiftW == 1 && image.chromaShiftH == 1) {
							yuv420_2_rgb8888(w.ptr(), image.planes[0], image.planes[1], image.planes[2], image.w, image.h, image.linesize[0], image.linesize[1], image.w << 2);
							//libyuv::I420ToARGB(image.planes[0], image.linesize[0], image.planes[2], image.linesize[2], image.planes[1], image.linesize[1], w.ptr(), image.w << 2, image.w, image.h);
							converted = true;
						} else if (image.chromaShiftW == 1 && image.chromaShiftH == 0) {
							yuv422_2_rgb8888(w.ptr(), image.planes[0], image.planes[1], image.planes[2], image.w, image.h, image.linesize[0], image.linesize[1], image.w << 2);
							//libyuv::I422ToARGB(image.planes[0], image.linesize[0], image.planes[2], image.linesize[2], image.planes[1], image.linesize[1], w.ptr(), image.w << 2, image.w, image.h);
							converted = true;
						} else if (image.chromaShiftW == 0 && image.chromaShiftH == 0) {
							yuv444_2_rgb8888(w.ptr(), image.planes[0], image.planes[1], image.planes[2], image.w, image.h, image.linesize[0], image.linesize[1], image.w << 2);
							//libyuv::I444ToARGB(image.planes[0], image.linesize[0], image.planes[2], image.linesize[2], image.planes[1], image.linesize[1], w.ptr(), image.w << 2, image.w, image.h);
							converted = true;
						} else if (image.chromaShiftW == 2 && image.chromaShiftH == 0) {
							//libyuv::I411ToARGB(image.planes[0], image.linesize[0], image.planes[2], image.linesize[2] image.planes[1], image.linesize[1], w.ptr(), image.w << 2, image.w, image.h);
							//converted = true;
						}

						if (converted) {
							Ref<Image> img = memnew(Image(image.w, image.h, 0, Image::FORMAT_RGBA8, frame_data));
							texture->set_data(img); //Zero copy send to visual server
							video_frame_done = true;
						}
					}
				}
			}
		}

		video_pos = video_frame->time;
		memmove(video_frames, video_frames + 1, (--video_frames_pos) * sizeof(void *));
		video_frames[video_frames_pos] = video_frame;
	}

	if (video_frames_pos == 0 && webm->isEOS()) {
		stop();
	}
}

void VideoStreamPlaybackWebm::set_mix_callback(VideoStreamPlayback::AudioMixCallback p_callback, void *p_userdata) {
	mix_callback = p_callback;
	mix_udata = p_userdata;
}
int VideoStreamPlaybackWebm::get_channels() const {
	if (audio) {
		return webm->getChannels();
	}
	return 0;
}
int VideoStreamPlaybackWebm::get_mix_rate() const {
	if (audio) {
		return webm->getSampleRate();
	}
	return 0;
}

inline bool VideoStreamPlaybackWebm::has_enough_video_frames() const {
	if (video_frames_pos > 0) {
		// FIXME: AudioServer output latency was fixed in af9bb0e, previously it used to
		// systematically return 0. Now that it gives a proper latency, it broke this
		// code where the delay compensation likely never really worked.
		//const double audio_delay = AudioServer::get_singleton()->get_output_latency();
		const double video_time = video_frames[video_frames_pos - 1]->time;
		return video_time >= time + /* audio_delay + */ delay_compensation;
	}
	return false;
}

bool VideoStreamPlaybackWebm::should_process(WebMFrame &video_frame) {
	// FIXME: AudioServer output latency was fixed in af9bb0e, previously it used to
	// systematically return 0. Now that it gives a proper latency, it broke this
	// code where the delay compensation likely never really worked.
	//const double audio_delay = AudioServer::get_singleton()->get_output_latency();
	return video_frame.time >= time + /* audio_delay + */ delay_compensation;
}

void VideoStreamPlaybackWebm::delete_pointers() {
	if (pcm) {
		memfree(pcm);
	}

	if (audio_frame) {
		memdelete(audio_frame);
	}
	if (video_frames) {
		for (int i = 0; i < video_frames_capacity; ++i) {
			memdelete(video_frames[i]);
		}
		memfree(video_frames);
	}

	if (video) {
		memdelete(video);
	}
	if (audio) {
		memdelete(audio);
	}

	if (webm) {
		memdelete(webm);
	}
}

/**/

VideoStreamWebm::VideoStreamWebm() :
		audio_track(0) {}

Ref<VideoStreamPlayback> VideoStreamWebm::instance_playback() {
	Ref<VideoStreamPlaybackWebm> pb = memnew(VideoStreamPlaybackWebm);
	pb->set_audio_track(audio_track);
	if (pb->open_file(file)) {
		return pb;
	}
	return nullptr;
}

void VideoStreamWebm::set_file(const String &p_file) {
	file = p_file;
}
String VideoStreamWebm::get_file() {
	return file;
}

void VideoStreamWebm::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_file", "file"), &VideoStreamWebm::set_file);
	ClassDB::bind_method(D_METHOD("get_file"), &VideoStreamWebm::get_file);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "file", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NOEDITOR | PROPERTY_USAGE_INTERNAL), "set_file", "get_file");
}

void VideoStreamWebm::set_audio_track(int p_track) {
	audio_track = p_track;
}

////////////

RES ResourceFormatLoaderWebm::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_no_subresource_cache) {
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	if (!f) {
		if (r_error) {
			*r_error = ERR_CANT_OPEN;
		}
		return RES();
	}

	VideoStreamWebm *stream = memnew(VideoStreamWebm);
	stream->set_file(p_path);

	Ref<VideoStreamWebm> webm_stream = Ref<VideoStreamWebm>(stream);

	if (r_error) {
		*r_error = OK;
	}

	f->close();
	memdelete(f);
	return webm_stream;
}

void ResourceFormatLoaderWebm::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("webm");
}

bool ResourceFormatLoaderWebm::handles_type(const String &p_type) const {
	return ClassDB::is_parent_class(p_type, "VideoStream");
}

String ResourceFormatLoaderWebm::get_resource_type(const String &p_path) const {
	String el = p_path.get_extension().to_lower();
	if (el == "webm") {
		return "VideoStreamWebm";
	}
	return "";
}
