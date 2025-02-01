/**************************************************************************/
/*  editor_preview_plugins.h                                              */
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

#include "editor/editor_resource_preview.h"

class ScriptLanguage;

void post_process_preview(Ref<Image> p_image);

class EditorTexturePreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorTexturePreviewPlugin, EditorResourcePreviewGenerator);

public:
	virtual bool handles(const String &p_type) const override;
	virtual bool generate_small_preview_automatically() const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;

	EditorTexturePreviewPlugin();
};

class EditorImagePreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorImagePreviewPlugin, EditorResourcePreviewGenerator);

public:
	virtual bool handles(const String &p_type) const override;
	virtual bool generate_small_preview_automatically() const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;

	EditorImagePreviewPlugin();
};

class EditorBitmapPreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorBitmapPreviewPlugin, EditorResourcePreviewGenerator);

public:
	virtual bool handles(const String &p_type) const override;
	virtual bool generate_small_preview_automatically() const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;

	EditorBitmapPreviewPlugin();
};

class EditorPackedScenePreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorPackedScenePreviewPlugin, EditorResourcePreviewGenerator);

public:
	virtual bool handles(const String &p_type) const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;
	virtual Ref<Texture2D> generate_from_path(const String &p_path, const Size2 &p_size, Dictionary &p_metadata) const override;

	EditorPackedScenePreviewPlugin();
};

class EditorMaterialPreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorMaterialPreviewPlugin, EditorResourcePreviewGenerator);

	RID scenario;
	RID sphere;
	RID sphere_instance;
	RID viewport;
	RID viewport_texture;
	RID light;
	RID light_instance;
	RID light2;
	RID light_instance2;
	RID camera;
	RID camera_attributes;
	mutable DrawRequester draw_requester;

public:
	virtual bool handles(const String &p_type) const override;
	virtual bool generate_small_preview_automatically() const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;
	virtual void abort() override;

	EditorMaterialPreviewPlugin();
	~EditorMaterialPreviewPlugin();
};

class EditorScriptPreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorScriptPreviewPlugin, EditorResourcePreviewGenerator);

	Ref<Texture2D> _generate_from_source_code(const ScriptLanguage *p_language, const String &p_source_code, const Size2 &p_size, Dictionary &p_metadata) const;

public:
	virtual bool handles(const String &p_type) const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;
	virtual Ref<Texture2D> generate_from_path(const String &p_path, const Size2 &p_size, Dictionary &p_metadata) const override;

	EditorScriptPreviewPlugin();
};

class EditorAudioStreamPreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorAudioStreamPreviewPlugin, EditorResourcePreviewGenerator);

public:
	virtual bool handles(const String &p_type) const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;

	EditorAudioStreamPreviewPlugin();
};

class EditorMeshPreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorMeshPreviewPlugin, EditorResourcePreviewGenerator);

	RID scenario;
	RID mesh_instance;
	RID viewport;
	RID viewport_texture;
	RID light;
	RID light_instance;
	RID light2;
	RID light_instance2;
	RID camera;
	RID camera_attributes;
	mutable DrawRequester draw_requester;

public:
	virtual bool handles(const String &p_type) const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;
	virtual void abort() override;

	EditorMeshPreviewPlugin();
	~EditorMeshPreviewPlugin();
};

class EditorFontPreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorFontPreviewPlugin, EditorResourcePreviewGenerator);

	RID viewport;
	RID viewport_texture;
	RID canvas;
	RID canvas_item;
	mutable DrawRequester draw_requester;

public:
	virtual bool handles(const String &p_type) const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;
	virtual Ref<Texture2D> generate_from_path(const String &p_path, const Size2 &p_size, Dictionary &p_metadata) const override;
	virtual void abort() override;

	EditorFontPreviewPlugin();
	~EditorFontPreviewPlugin();
};

class EditorGradientPreviewPlugin : public EditorResourcePreviewGenerator {
	GDCLASS(EditorGradientPreviewPlugin, EditorResourcePreviewGenerator);

public:
	virtual bool handles(const String &p_type) const override;
	virtual bool generate_small_preview_automatically() const override;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const override;

	EditorGradientPreviewPlugin();
};
