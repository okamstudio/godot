/**************************************************************************/
/*  editor_resource_tooltip_plugins.cpp                                   */
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

#include "editor_resource_tooltip_plugins.h"

#include "editor/editor_resource_preview.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_rect.h"

void EditorResourceTooltipPlugin::_thumbnail_ready(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata) {
	ObjectID trid = p_udata;
	TextureRect *tr = Object::cast_to<TextureRect>(ObjectDB::get_instance(trid));

	if (!tr) {
		return;
	}

	tr->set_texture(p_preview);
}

void EditorResourceTooltipPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_thumbnail_ready"), &EditorResourceTooltipPlugin::_thumbnail_ready);
	ClassDB::bind_method(D_METHOD("request_thumbnail", "path", "control"), &EditorResourceTooltipPlugin::request_thumbnail);

	GDVIRTUAL_BIND(_handles, "type");
	GDVIRTUAL_BIND(_make_tooltip_for_path, "path", "metadata", "base");
}

VBoxContainer *EditorResourceTooltipPlugin::make_default_tooltip(const String &p_resource_path) {
	VBoxContainer *vb = memnew(VBoxContainer);
	vb->add_theme_constant_override("separation", -4 * EDSCALE);
	{
		Label *label = memnew(Label(p_resource_path.get_file()));
		vb->add_child(label);
	}

	{
		Ref<FileAccess> f = FileAccess::open(p_resource_path, FileAccess::READ);
		Label *label = memnew(Label(vformat(TTR("Size: %s"), String::humanize_size(f->get_length()))));
		vb->add_child(label);
	}

	if (ResourceLoader::exists(p_resource_path)) {
		String type = ResourceLoader::get_resource_type(p_resource_path);
		Label *label = memnew(Label(vformat(TTR("Type: %s"), type)));
		vb->add_child(label);
	}
	return vb;
}

void EditorResourceTooltipPlugin::request_thumbnail(const String &p_path, TextureRect *p_for_control) const {
	ERR_FAIL_NULL(p_for_control);
	EditorResourcePreview::get_singleton()->queue_resource_preview(p_path, const_cast<EditorResourceTooltipPlugin *>(this), "_thumbnail_ready", p_for_control->get_instance_id());
}

bool EditorResourceTooltipPlugin::handles(const String &p_resource_type) const {
	bool ret = false;
	GDVIRTUAL_CALL(_handles, p_resource_type, ret);
	return ret;
}

Control *EditorResourceTooltipPlugin::make_tooltip_for_path(const String &p_resource_path, const Dictionary &p_metadata, Control *p_base) const {
	Control *ret = nullptr;
	GDVIRTUAL_CALL(_make_tooltip_for_path, p_resource_path, p_metadata, p_base, ret);
	return ret;
}

// EditorTextureTooltipPlugin

bool EditorTextureTooltipPlugin::handles(const String &p_resource_type) const {
	return ClassDB::is_parent_class(p_resource_type, "Texture2D") || ClassDB::is_parent_class(p_resource_type, "Image");
}

Control *EditorTextureTooltipPlugin::make_tooltip_for_path(const String &p_resource_path, const Dictionary &p_metadata, Control *p_base) const {
	HBoxContainer *hb = memnew(HBoxContainer);
	VBoxContainer *vb = Object::cast_to<VBoxContainer>(p_base);
	DEV_ASSERT(vb);
	vb->set_alignment(BoxContainer::ALIGNMENT_CENTER);

	Vector2 dimensions = p_metadata.get("dimensions", Vector2());
	Label *label = memnew(Label(vformat(TTR(U"Dimensions: %d × %d"), dimensions.x, dimensions.y)));
	vb->add_child(label);

	TextureRect *tr = memnew(TextureRect);
	tr->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
	hb->add_child(tr);
	request_thumbnail(p_resource_path, tr);

	hb->add_child(vb);
	return hb;
}

// EditorAudioStreamTooltipPlugin

bool EditorAudioStreamTooltipPlugin::handles(const String &p_resource_type) const {
	return ClassDB::is_parent_class(p_resource_type, "AudioStream");
}

Control *EditorAudioStreamTooltipPlugin::make_tooltip_for_path(const String &p_resource_path, const Dictionary &p_metadata, Control *p_base) const {
	VBoxContainer *vb = Object::cast_to<VBoxContainer>(p_base);
	DEV_ASSERT(vb);

	double length = p_metadata.get("length", 0.0);
	if (length >= 60.0) {
		vb->add_child(memnew(Label(vformat(TTR("Length: %0dm %0ds"), int(length / 60.0), int(Math::fmod(length, 60))))));
	} else if (length >= 1.0) {
		vb->add_child(memnew(Label(vformat(TTR("Length: %0.1fs"), length))));
	} else {
		vb->add_child(memnew(Label(vformat(TTR("Length: %0.3fs"), length))));
	}

	TextureRect *tr = memnew(TextureRect);
	vb->add_child(tr);
	request_thumbnail(p_resource_path, tr);

	return vb;
}
