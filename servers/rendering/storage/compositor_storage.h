/**************************************************************************/
/*  compositor_storage.h                                                  */
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

#include "core/templates/rid_owner.h"
#include "servers/rendering_server.h"

class RendererCompositorStorage {
private:
	static RendererCompositorStorage *singleton;
	int num_compositor_effects_with_motion_vectors = 0;

	// Compositor effect
	struct CompositorEffect {
		bool is_enabled = true;
		RS::CompositorEffectCallbackType callback_type;
		Callable callback;

		BitField<RS::CompositorEffectFlags> flags;
	};

	mutable RID_Owner<CompositorEffect, true> compositor_effects_owner;

	// Compositor
	struct Compositor {
		// Compositor effects
		Vector<RID> compositor_effects;
	};

	mutable RID_Owner<Compositor, true> compositor_owner;

public:
	static RendererCompositorStorage *get_singleton() { return singleton; }
	int get_num_compositor_effects_with_motion_vectors() const { return num_compositor_effects_with_motion_vectors; }

	RendererCompositorStorage();
	virtual ~RendererCompositorStorage();

	// Compositor effect
	RID compositor_effect_allocate();
	void compositor_effect_initialize(RID p_rid);
	void compositor_effect_free(RID p_rid);

	bool is_compositor_effect(RID p_effect) const {
		return compositor_effects_owner.owns(p_effect);
	}

	void compositor_effect_set_enabled(RID p_effect, bool p_enabled);
	bool compositor_effect_get_enabled(RID p_effect) const;

	void compositor_effect_set_callback(RID p_effect, RS::CompositorEffectCallbackType p_callback_type, const Callable &p_callback);
	RS::CompositorEffectCallbackType compositor_effect_get_callback_type(RID p_effect) const;
	Callable compositor_effect_get_callback(RID p_effect) const;

	void compositor_effect_set_flag(RID p_effect, RS::CompositorEffectFlags p_flag, bool p_set);
	bool compositor_effect_get_flag(RID p_effect, RS::CompositorEffectFlags p_flag) const;

	// Compositor
	RID compositor_allocate();
	void compositor_initialize(RID p_rid);
	void compositor_free(RID p_rid);

	bool is_compositor(RID p_compositor) const {
		return compositor_owner.owns(p_compositor);
	}

	void compositor_set_compositor_effects(RID p_compositor, const Vector<RID> &p_effects);
	Vector<RID> compositor_get_compositor_effects(RID p_compositor, RS::CompositorEffectCallbackType p_callback_type = RS::COMPOSITOR_EFFECT_CALLBACK_TYPE_ANY, bool p_enabled_only = true) const;
};
