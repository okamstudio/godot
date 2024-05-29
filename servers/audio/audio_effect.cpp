/**************************************************************************/
/*  audio_effect.cpp                                                      */
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

#include "audio_effect.h"

void AudioEffectInstance::process(const AudioFrame *p_src_frames, AudioFrame *p_dst_frames, int p_frame_count) {
	GDVIRTUAL_CALL(_process, p_src_frames, p_dst_frames, p_frame_count);
}
bool AudioEffectInstance::process_silence() const {
	bool ret = false;
	GDVIRTUAL_CALL(_process_silence, ret);
	return ret;
}
void AudioEffectInstance::set_current_channel(int p_channel) {
	GDVIRTUAL_CALL(_set_current_channel, p_channel);
}

void AudioEffectInstance::_bind_methods() {
	GDVIRTUAL_BIND(_process, "src_buffer", "dst_buffer", "frame_count");
	GDVIRTUAL_BIND(_process_silence);
	GDVIRTUAL_BIND(_set_current_channel, "channel");
}

////

Ref<AudioEffectInstance> AudioEffect::instantiate() {
	Ref<AudioEffectInstance> ret;
	GDVIRTUAL_CALL(_instantiate, ret);
	return ret;
}
void AudioEffect::_bind_methods() {
	GDVIRTUAL_BIND(_instantiate);
}

AudioEffect::AudioEffect() {
}
