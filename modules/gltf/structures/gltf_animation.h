/**************************************************************************/
/*  gltf_animation.h                                                      */
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

#ifndef GLTF_ANIMATION_H
#define GLTF_ANIMATION_H

#include "scene/resources/animation.h"

class GLTFAnimation : public Resource {
	GDCLASS(GLTFAnimation, Resource);

protected:
	static void _bind_methods();

public:
	enum Interpolation {
		INTERP_LINEAR,
		INTERP_STEP,
		INTERP_CATMULLROMSPLINE,
		INTERP_CUBIC_SPLINE,
	};

	template <typename T>
	struct Channel {
		Interpolation interpolation = INTERP_LINEAR;
		Vector<double> times;
		Vector<T> values;
	};

	struct NodeTrack {
		Channel<Vector3> position_track;
		Channel<Quaternion> rotation_track;
		Channel<Vector3> scale_track;
		Vector<Channel<real_t>> weight_tracks;
	};

	String original_name;
	bool loop = false;
	HashMap<int, NodeTrack> node_tracks;
	HashMap<String, Channel<Variant>> pointer_tracks;
	Dictionary additional_data;

public:
	static Interpolation godot_to_gltf_interpolation(const Ref<Animation> &p_godot_animation, int32_t p_godot_anim_track_index);
	static Animation::InterpolationType gltf_to_godot_interpolation(Interpolation p_gltf_interpolation);

	String get_original_name();
	void set_original_name(String p_name);

	bool get_loop() const;
	void set_loop(bool p_val);

	HashMap<int, GLTFAnimation::NodeTrack> &get_node_tracks();
	HashMap<String, GLTFAnimation::Channel<Variant>> &get_pointer_tracks();
	bool is_empty_of_tracks() const;

	Variant get_additional_data(const StringName &p_extension_name);
	void set_additional_data(const StringName &p_extension_name, Variant p_additional_data);

	GLTFAnimation();
};

#endif // GLTF_ANIMATION_H
