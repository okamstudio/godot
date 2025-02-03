/**************************************************************************/
/*  resource.h                                                            */
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

#ifndef RESOURCE_H
#define RESOURCE_H

#include "core/class_db.h"
#include "core/object.h"
#include "core/ref_ptr.h"
#include "core/reference.h"
#include "core/safe_refcount.h"
#include "core/self_list.h"

#define RES_BASE_EXTENSION(m_ext)                                                                                   \
public:                                                                                                             \
	static void register_custom_data_to_otdb() { ClassDB::add_resource_base_extension(m_ext, get_class_static()); } \
	virtual String get_base_extension() const { return m_ext; }                                                     \
                                                                                                                    \
private:

class Resource : public Reference {
	GDCLASS(Resource, Reference);
	OBJ_CATEGORY("Resources");
	RES_BASE_EXTENSION("res");

	Set<ObjectID> owners;

	friend class ResBase;
	friend class ResourceCache;

	String name;
	String path_cache;
	int subindex;

#ifdef TOOLS_ENABLED
	uint64_t last_modified_time;
	uint64_t import_last_modified_time;
	String import_path;
#endif

	bool local_to_scene;
	friend class SceneState;
	Node *local_scene;

	SelfList<Resource> remapped_list;

protected:
	void emit_changed();

	void notify_change_to_owners();

	virtual void _resource_path_changed();
	static void _bind_methods();

	void _set_path(const String &p_path);
	void _take_over_path(const String &p_path);

public:
	static Node *(*_get_local_scene_func)(); //used by editor

	virtual bool editor_can_reload_from_file();
	virtual void reload_from_file();

	void register_owner(Object *p_owner);
	void unregister_owner(Object *p_owner);

	void set_name(const String &p_name);
	String get_name() const;

	virtual void set_path(const String &p_path, bool p_take_over = false);
	String get_path() const;

	void set_subindex(int p_sub_index);
	int get_subindex() const;

	virtual Ref<Resource> duplicate(bool p_subresources = false) const;
	Ref<Resource> duplicate_for_local_scene(Node *p_for_scene, Map<Ref<Resource>, Ref<Resource>> &remap_cache);
	void configure_for_local_scene(Node *p_for_scene, Map<Ref<Resource>, Ref<Resource>> &remap_cache);

	void set_local_to_scene(bool p_enable);
	bool is_local_to_scene() const;
	virtual void setup_local_to_scene();

	Node *get_local_scene() const;

#ifdef TOOLS_ENABLED

	uint32_t hash_edited_version() const;

	virtual void set_last_modified_time(uint64_t p_time) { last_modified_time = p_time; }
	uint64_t get_last_modified_time() const { return last_modified_time; }

	virtual void set_import_last_modified_time(uint64_t p_time) { import_last_modified_time = p_time; }
	uint64_t get_import_last_modified_time() const { return import_last_modified_time; }

	void set_import_path(const String &p_path) { import_path = p_path; }
	String get_import_path() const { return import_path; }

#endif

	void set_as_translation_remapped(bool p_remapped);
	bool is_translation_remapped() const;

	virtual RID get_rid() const; // some resources may offer conversion to RID

#ifdef TOOLS_ENABLED
	//helps keep IDs same number when loading/saving scenes. -1 clears ID and it Returns -1 when no id stored
	void set_id_for_path(const String &p_path, int p_id);
	int get_id_for_path(const String &p_path) const;
#endif

	Resource();
	~Resource();
};

typedef Ref<Resource> RES;

class ResourceCache {
	friend class Resource;
	friend class ResourceLoader; //need the lock
	static RWLock lock;
	static HashMap<String, Resource *> resources;
#ifdef TOOLS_ENABLED
	static HashMap<String, HashMap<String, int>> resource_path_cache; // each tscn has a set of resource paths and IDs
	static RWLock path_cache_lock;
#endif // TOOLS_ENABLED
	friend void unregister_core_types();
	static void clear();
	friend void register_core_types();

public:
	static void reload_externals();
	static bool has(const String &p_path);
	static Resource *get(const String &p_path);
	static void forget(const String &p_path); // Removes a resource from cache
	static void dump(const char *p_file = nullptr, bool p_short = false);
	static void get_cached_resources(List<Ref<Resource>> *p_resources);
	static int get_cached_resource_count();
};

#endif // RESOURCE_H
