/**************************************************************************/
/*  editor_resource_tooltip_plugins.h                                     */
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

#include "core/object/gdvirtual.gen.inc"
#include "core/object/ref_counted.h"
#include "scene/gui/control.h"

class Texture2D;
class TextureRect;
class VBoxContainer;

class EditorResourceTooltipPlugin : public RefCounted {
	GDCLASS(EditorResourceTooltipPlugin, RefCounted);

	void _thumbnail_ready(const String &p_path, const Ref<Texture2D> &p_preview, const Ref<Texture2D> &p_small_preview, const Variant &p_udata);

protected:
	static void _bind_methods();

	GDVIRTUAL1RC(bool, _handles, String)
	GDVIRTUAL3RC(Control *, _make_tooltip_for_path, String, Dictionary, Control *)

public:
	static VBoxContainer *make_default_tooltip(const String &p_resource_path);
	void request_thumbnail(const String &p_path, TextureRect *p_for_control) const;

	virtual bool handles(const String &p_resource_type) const;
	virtual Control *make_tooltip_for_path(const String &p_resource_path, const Dictionary &p_metadata, Control *p_base) const;
};

class EditorTextureTooltipPlugin : public EditorResourceTooltipPlugin {
	GDCLASS(EditorTextureTooltipPlugin, EditorResourceTooltipPlugin);

public:
	virtual bool handles(const String &p_resource_type) const override;
	virtual Control *make_tooltip_for_path(const String &p_resource_path, const Dictionary &p_metadata, Control *p_base) const override;
};

class EditorAudioStreamTooltipPlugin : public EditorResourceTooltipPlugin {
	GDCLASS(EditorAudioStreamTooltipPlugin, EditorResourceTooltipPlugin);

public:
	virtual bool handles(const String &p_resource_type) const override;
	virtual Control *make_tooltip_for_path(const String &p_resource_path, const Dictionary &p_metadata, Control *p_base) const override;
};
