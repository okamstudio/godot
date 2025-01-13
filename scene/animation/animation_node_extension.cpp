/**************************************************************************/
/*  animation_node_extension.cpp                                          */
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

#include "animation_node_extension.h"

AnimationNode::NodeTimeInfo AnimationNodeExtension::_process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only) {
	PackedFloat32Array r_ret;

	GDVIRTUAL_CALL(
			_process,
			_playback_info_to_array(p_playback_info),
			p_test_only,
			r_ret);

	return _array_to_node_time_info(r_ret);
}

bool AnimationNodeExtension::is_looping(const PackedFloat32Array &p_node_info) {
	return _array_to_node_time_info(p_node_info).is_looping();
}

double AnimationNodeExtension::get_remaining_time(const PackedFloat32Array &p_node_info, bool p_break_loop) {
	return _array_to_node_time_info(p_node_info).get_remain(p_break_loop);
}

void AnimationNodeExtension::_bind_methods() {
	ClassDB::bind_static_method("AnimationNodeExtension", D_METHOD("is_looping", "node_info"), &AnimationNodeExtension::is_looping);
	ClassDB::bind_static_method("AnimationNodeExtension", D_METHOD("get_remaining_time", "node_info", "break_loop"), &AnimationNodeExtension::get_remaining_time);
	GDVIRTUAL_BIND(_process, "playback_info", "test_only");
}

AnimationNode::NodeTimeInfo AnimationNodeExtension::_array_to_node_time_info(const PackedFloat32Array &p_node_info) {
	ERR_FAIL_COND_V_MSG(p_node_info.size() != 6, AnimationNode::NodeTimeInfo(), "Invalid node info.");
	AnimationNode::NodeTimeInfo ret_val;
	ret_val.length = p_node_info[0];
	ret_val.position = p_node_info[1];
	ret_val.delta = p_node_info[2];
	ret_val.loop_mode = static_cast<Animation::LoopMode>(p_node_info[3]);
	ret_val.will_end = p_node_info[4] > 0.0;
	ret_val.is_infinity = p_node_info[5] > 0.0;
	return ret_val;
}

PackedFloat64Array AnimationNodeExtension::_playback_info_to_array(const AnimationMixer::PlaybackInfo &p_playback_info) {
	PackedFloat64Array playback_info_array;
	playback_info_array.push_back(p_playback_info.time);
	playback_info_array.push_back(p_playback_info.delta);
	playback_info_array.push_back(p_playback_info.start);
	playback_info_array.push_back(p_playback_info.end);
	playback_info_array.push_back(p_playback_info.seeked);
	playback_info_array.push_back(p_playback_info.is_external_seeking);
	playback_info_array.push_back(p_playback_info.looped_flag);
	playback_info_array.push_back(p_playback_info.weight);

	return playback_info_array;
}
