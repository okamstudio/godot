/**************************************************************************/
/*  editor_resource_preview.h                                             */
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

#ifndef EDITOR_RESOURCE_PREVIEW_H
#define EDITOR_RESOURCE_PREVIEW_H

#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "core/templates/safe_refcount.h"
#include "scene/main/node.h"

class ImageTexture;
class Texture2D;

class EditorResourcePreviewGenerator : public RefCounted {
	GDCLASS(EditorResourcePreviewGenerator, RefCounted);

protected:
	static void _bind_methods();

	GDVIRTUAL1RC(bool, _handles, String)
	GDVIRTUAL3RC(Ref<Texture2D>, _generate, Ref<Resource>, Vector2i, Dictionary)
	GDVIRTUAL3RC(Ref<Texture2D>, _generate_from_path, String, Vector2i, Dictionary)
	GDVIRTUAL0RC(bool, _generate_small_preview_automatically)
	GDVIRTUAL0RC(bool, _can_generate_small_preview)

	class DrawRequester : public Object {
		Semaphore semaphore;

		Variant _post_semaphore();

	public:
		void request_and_wait(RID p_viewport);
		void abort();
	};

public:
	virtual bool handles(const String &p_type) const;
	virtual Ref<Texture2D> generate(const Ref<Resource> &p_from, const Size2 &p_size, Dictionary &p_metadata) const;
	virtual Ref<Texture2D> generate_from_path(const String &p_path, const Size2 &p_size, Dictionary &p_metadata) const;
	virtual void abort() {}

	virtual bool generate_small_preview_automatically() const;
	virtual bool can_generate_small_preview() const;

	EditorResourcePreviewGenerator();
};

class EditorResourcePreview : public Node {
	GDCLASS(EditorResourcePreview, Node);

	inline static constexpr int CURRENT_METADATA_VERSION = 1; // Increment this number to invalidate all previews.
	inline static EditorResourcePreview *singleton = nullptr;

	struct QueueItem {
		Ref<Resource> resource;
		String path;
		ObjectID id;
		StringName function;
		Variant userdata;
	};

	List<QueueItem> queue;

	Mutex preview_mutex;
	Semaphore preview_sem;
	Thread thread;
	SafeFlag exiting;
	SafeFlag exited;

	struct Item {
		Ref<Texture2D> preview;
		Ref<Texture2D> small_preview;
		Dictionary preview_metadata;
		uint32_t last_hash = 0;
		uint64_t modified_time = 0;
	};

	HashMap<String, Item> cache;

	void _preview_ready(const String &p_path, int p_hash, const Ref<Texture2D> &p_texture, const Ref<Texture2D> &p_small_texture, ObjectID id, const StringName &p_func, const Variant &p_ud, const Dictionary &p_metadata);
	void _generate_preview(Ref<ImageTexture> &r_texture, Ref<ImageTexture> &r_small_texture, const QueueItem &p_item, const String &cache_base, Dictionary &p_metadata);

	int small_thumbnail_size = -1;

	static void _thread_func(void *ud);
	void _thread(); // For rendering drivers supporting async texture creation.
	static void _idle_callback(); // For other rendering drivers (i.e., OpenGL).
	void _iterate();

	void _write_preview_cache(Ref<FileAccess> p_file, int p_thumbnail_size, bool p_has_small_texture, uint64_t p_modified_time, const String &p_hash, const Dictionary &p_metadata);
	void _read_preview_cache(Ref<FileAccess> p_file, int *r_thumbnail_size, bool *r_has_small_texture, uint64_t *r_modified_time, String *r_hash, Dictionary *r_metadata, bool *r_outdated);

	Vector<Ref<EditorResourcePreviewGenerator>> preview_generators;

	void _update_thumbnail_sizes();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static EditorResourcePreview *get_singleton();

	struct PreviewItem {
		Ref<Texture2D> preview;
		Ref<Texture2D> small_preview;
	};

	// p_receiver_func callback has signature (String p_path, Ref<Texture2D> p_preview, Ref<Texture2D> p_preview_small, Variant p_userdata)
	// p_preview will be null if there was an error
	void queue_resource_preview(const String &p_path, Object *p_receiver, const StringName &p_receiver_func, const Variant &p_userdata);
	void queue_edited_resource_preview(const Ref<Resource> &p_res, Object *p_receiver, const StringName &p_receiver_func, const Variant &p_userdata);
	const Dictionary get_preview_metadata(const String &p_path) const;

	PreviewItem get_resource_preview_if_available(const String &p_path);

	void add_preview_generator(const Ref<EditorResourcePreviewGenerator> &p_generator);
	void remove_preview_generator(const Ref<EditorResourcePreviewGenerator> &p_generator);
	void check_for_invalidation(const String &p_path);

	void start();
	void stop();
	bool is_threaded() const;

	EditorResourcePreview();
	~EditorResourcePreview();
};

#endif // EDITOR_RESOURCE_PREVIEW_H
