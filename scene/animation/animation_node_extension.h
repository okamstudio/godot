/**************************************************************************/
/*  animation_node_extension.h                                            */
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

#ifndef ANIMATION_NODE_EXTENSION_H
#define ANIMATION_NODE_EXTENSION_H

#include "scene/animation/animation_tree.h"

class AnimationNodeExtension : public AnimationRootNode {
	GDCLASS(AnimationNodeExtension, AnimationRootNode);

public:
	virtual NodeTimeInfo _process(const AnimationMixer::PlaybackInfo p_playback_info, bool p_test_only = false) override;

	static bool is_looping(const PackedFloat32Array &p_node_info);
	static double get_remaining_time(const PackedFloat32Array &p_node_info, bool p_break_loop = false);

protected:
	static void _bind_methods();

	GDVIRTUAL2R_REQUIRED(PackedFloat32Array, _process, PackedFloat64Array, bool);

private:
	static AnimationNode::NodeTimeInfo _array_to_node_time_info(const PackedFloat32Array &p_array);
	static PackedFloat64Array _playback_info_to_array(const AnimationMixer::PlaybackInfo &p_playback_info);
};

#endif // ANIMATION_NODE_EXTENSION_H
