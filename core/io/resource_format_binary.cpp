/**************************************************************************/
/*  resource_format_binary.cpp                                            */
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

#include "resource_format_binary.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access_compressed.h"
#include "core/io/missing_resource.h"
#include "core/object/script_language.h"
#include "core/version.h"

//#define print_bl(m_what) print_line(m_what)
#define print_bl(m_what) (void)(m_what)

enum {
	//numbering must be different from variant, in case new variant types are added (variant must be always contiguous for jumptable optimization)
	VARIANT_NIL = 1,
	VARIANT_BOOL = 2,
	VARIANT_INT = 3,
	VARIANT_FLOAT = 4,
	VARIANT_STRING = 5,
	VARIANT_VECTOR2 = 10,
	VARIANT_RECT2 = 11,
	VARIANT_VECTOR3 = 12,
	VARIANT_PLANE = 13,
	VARIANT_QUATERNION = 14,
	VARIANT_AABB = 15,
	VARIANT_BASIS = 16,
	VARIANT_TRANSFORM3D = 17,
	VARIANT_TRANSFORM2D = 18,
	VARIANT_COLOR = 20,
	VARIANT_NODE_PATH = 22,
	VARIANT_RID = 23,
	VARIANT_OBJECT = 24,
	VARIANT_INPUT_EVENT = 25,
	VARIANT_DICTIONARY = 26,
	VARIANT_ARRAY = 30,
	VARIANT_PACKED_BYTE_ARRAY = 31,
	VARIANT_PACKED_INT32_ARRAY = 32,
	VARIANT_PACKED_FLOAT32_ARRAY = 33,
	VARIANT_PACKED_STRING_ARRAY = 34,
	VARIANT_PACKED_VECTOR3_ARRAY = 35,
	VARIANT_PACKED_COLOR_ARRAY = 36,
	VARIANT_PACKED_VECTOR2_ARRAY = 37,
	VARIANT_INT64 = 40,
	VARIANT_DOUBLE = 41,
	VARIANT_CALLABLE = 42,
	VARIANT_SIGNAL = 43,
	VARIANT_STRING_NAME = 44,
	VARIANT_VECTOR2I = 45,
	VARIANT_RECT2I = 46,
	VARIANT_VECTOR3I = 47,
	VARIANT_PACKED_INT64_ARRAY = 48,
	VARIANT_PACKED_FLOAT64_ARRAY = 49,
	VARIANT_VECTOR4 = 50,
	VARIANT_VECTOR4I = 51,
	VARIANT_PROJECTION = 52,
	VARIANT_PACKED_VECTOR4_ARRAY = 53,
	OBJECT_EMPTY = 0,
	OBJECT_EXTERNAL_RESOURCE = 1,
	OBJECT_INTERNAL_RESOURCE = 2,
	OBJECT_EXTERNAL_RESOURCE_INDEX = 3,
	// Version 2: Added 64-bit support for float and int.
	// Version 3: Changed NodePath encoding.
	// Version 4: New string ID for ext/subresources, breaks forward compat.
	// Version 5: Ability to store script class in the header.
	// Version 6: Added PackedVector4Array Variant type.
	FORMAT_VERSION = 6,
	FORMAT_VERSION_CAN_RENAME_DEPS = 1,
	FORMAT_VERSION_NO_NODEPATH_PROPERTY = 3,
};

void ResourceLoaderBinary::_advance_padding(uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

static Error read_reals(real_t *dst, Ref<FileAccess> &f, size_t count) {
	if (f->real_is_double) {
		if constexpr (sizeof(real_t) == 8) {
			// Ideal case with double-precision
			f->get_buffer((uint8_t *)dst, count * sizeof(double));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *dst = (uint64_t *)dst;
				for (size_t i = 0; i < count; i++) {
					dst[i] = BSWAP64(dst[i]);
				}
			}
#endif
		} else if constexpr (sizeof(real_t) == 4) {
			// May be slower, but this is for compatibility. Eventually the data should be converted.
			for (size_t i = 0; i < count; ++i) {
				dst[i] = f->get_double();
			}
		} else {
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "real_t size is neither 4 nor 8!");
		}
	} else {
		if constexpr (sizeof(real_t) == 4) {
			// Ideal case with float-precision
			f->get_buffer((uint8_t *)dst, count * sizeof(float));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *dst = (uint32_t *)dst;
				for (size_t i = 0; i < count; i++) {
					dst[i] = BSWAP32(dst[i]);
				}
			}
#endif
		} else if constexpr (sizeof(real_t) == 8) {
			for (size_t i = 0; i < count; ++i) {
				dst[i] = f->get_float();
			}
		} else {
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "real_t size is neither 4 nor 8!");
		}
	}
	return OK;
}

StringName ResourceLoaderBinary::_get_string() {
	uint32_t id = f->get_32();
	if (id & 0x80000000) {
		uint32_t len = id & 0x7FFFFFFF;
		if ((int)len > str_buf.size()) {
			str_buf.resize(len);
		}
		if (len == 0) {
			return StringName();
		}
		f->get_buffer((uint8_t *)&str_buf[0], len);
		String s;
		s.parse_utf8(&str_buf[0], len);
		return s;
	}

	return string_map[id];
}

Error ResourceLoaderBinary::parse_variant(Variant &r_v) {
	uint32_t prop_type = f->get_32();
	print_bl("find property of type: " + itos(prop_type));

	switch (prop_type) {
		case VARIANT_NIL: {
			r_v = Variant();
		} break;
		case VARIANT_BOOL: {
			r_v = bool(f->get_32());
		} break;
		case VARIANT_INT: {
			r_v = int(f->get_32());
		} break;
		case VARIANT_INT64: {
			r_v = int64_t(f->get_64());
		} break;
		case VARIANT_FLOAT: {
			r_v = f->get_real();
		} break;
		case VARIANT_DOUBLE: {
			r_v = f->get_double();
		} break;
		case VARIANT_STRING: {
			r_v = get_unicode_string();
		} break;
		case VARIANT_VECTOR2: {
			Vector2 v;
			v.x = f->get_real();
			v.y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_VECTOR2I: {
			Vector2i v;
			v.x = f->get_32();
			v.y = f->get_32();
			r_v = v;

		} break;
		case VARIANT_RECT2: {
			Rect2 v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_RECT2I: {
			Rect2i v;
			v.position.x = f->get_32();
			v.position.y = f->get_32();
			v.size.x = f->get_32();
			v.size.y = f->get_32();
			r_v = v;

		} break;
		case VARIANT_VECTOR3: {
			Vector3 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			r_v = v;
		} break;
		case VARIANT_VECTOR3I: {
			Vector3i v;
			v.x = f->get_32();
			v.y = f->get_32();
			v.z = f->get_32();
			r_v = v;
		} break;
		case VARIANT_VECTOR4: {
			Vector4 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;
		} break;
		case VARIANT_VECTOR4I: {
			Vector4i v;
			v.x = f->get_32();
			v.y = f->get_32();
			v.z = f->get_32();
			v.w = f->get_32();
			r_v = v;
		} break;
		case VARIANT_PLANE: {
			Plane v;
			v.normal.x = f->get_real();
			v.normal.y = f->get_real();
			v.normal.z = f->get_real();
			v.d = f->get_real();
			r_v = v;
		} break;
		case VARIANT_QUATERNION: {
			Quaternion v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;

		} break;
		case VARIANT_AABB: {
			AABB v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.position.z = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			v.size.z = f->get_real();
			r_v = v;

		} break;
		case VARIANT_TRANSFORM2D: {
			Transform2D v;
			v.columns[0].x = f->get_real();
			v.columns[0].y = f->get_real();
			v.columns[1].x = f->get_real();
			v.columns[1].y = f->get_real();
			v.columns[2].x = f->get_real();
			v.columns[2].y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_BASIS: {
			Basis v;
			v.rows[0].x = f->get_real();
			v.rows[0].y = f->get_real();
			v.rows[0].z = f->get_real();
			v.rows[1].x = f->get_real();
			v.rows[1].y = f->get_real();
			v.rows[1].z = f->get_real();
			v.rows[2].x = f->get_real();
			v.rows[2].y = f->get_real();
			v.rows[2].z = f->get_real();
			r_v = v;

		} break;
		case VARIANT_TRANSFORM3D: {
			Transform3D v;
			v.basis.rows[0].x = f->get_real();
			v.basis.rows[0].y = f->get_real();
			v.basis.rows[0].z = f->get_real();
			v.basis.rows[1].x = f->get_real();
			v.basis.rows[1].y = f->get_real();
			v.basis.rows[1].z = f->get_real();
			v.basis.rows[2].x = f->get_real();
			v.basis.rows[2].y = f->get_real();
			v.basis.rows[2].z = f->get_real();
			v.origin.x = f->get_real();
			v.origin.y = f->get_real();
			v.origin.z = f->get_real();
			r_v = v;
		} break;
		case VARIANT_PROJECTION: {
			Projection v;
			v.columns[0].x = f->get_real();
			v.columns[0].y = f->get_real();
			v.columns[0].z = f->get_real();
			v.columns[0].w = f->get_real();
			v.columns[1].x = f->get_real();
			v.columns[1].y = f->get_real();
			v.columns[1].z = f->get_real();
			v.columns[1].w = f->get_real();
			v.columns[2].x = f->get_real();
			v.columns[2].y = f->get_real();
			v.columns[2].z = f->get_real();
			v.columns[2].w = f->get_real();
			v.columns[3].x = f->get_real();
			v.columns[3].y = f->get_real();
			v.columns[3].z = f->get_real();
			v.columns[3].w = f->get_real();
			r_v = v;
		} break;
		case VARIANT_COLOR: {
			Color v; // Colors should always be in single-precision.
			v.r = f->get_float();
			v.g = f->get_float();
			v.b = f->get_float();
			v.a = f->get_float();
			r_v = v;

		} break;
		case VARIANT_STRING_NAME: {
			r_v = StringName(get_unicode_string());
		} break;

		case VARIANT_NODE_PATH: {
			Vector<StringName> names;
			Vector<StringName> subnames;
			bool absolute;

			int name_count = f->get_16();
			uint32_t subname_count = f->get_16();
			absolute = subname_count & 0x8000;
			subname_count &= 0x7FFF;
			if (ver_format < FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				subname_count += 1; // has a property field, so we should count it as well
			}

			for (int i = 0; i < name_count; i++) {
				names.push_back(_get_string());
			}
			for (uint32_t i = 0; i < subname_count; i++) {
				subnames.push_back(_get_string());
			}

			NodePath np = NodePath(names, subnames, absolute);

			r_v = np;

		} break;
		case VARIANT_RID: {
			r_v = f->get_32();
		} break;
		case VARIANT_OBJECT: {
			uint32_t objtype = f->get_32();

			switch (objtype) {
				case OBJECT_EMPTY: {
					//do none

				} break;
				case OBJECT_INTERNAL_RESOURCE: {
					uint32_t index = f->get_32();
					String path;

					if (using_named_scene_ids) { // New format.
						ERR_FAIL_INDEX_V((int)index, internal_resources.size(), ERR_PARSE_ERROR);
						path = internal_resources[index].path;
					} else {
						path += res_path + "::" + itos(index);
					}

					//always use internal cache for loading internal resources
					if (!internal_index_cache.has(path)) {
						WARN_PRINT(vformat("Couldn't load resource (no cache): %s.", path));
						r_v = Variant();
					} else {
						r_v = internal_index_cache[path];
					}
				} break;
				case OBJECT_EXTERNAL_RESOURCE: {
					//old file format, still around for compatibility

					String exttype = get_unicode_string();
					String path = get_unicode_string();

					if (!path.contains("://") && path.is_relative_path()) {
						// path is relative to file being loaded, so convert to a resource path
						path = ProjectSettings::get_singleton()->localize_path(res_path.get_base_dir().path_join(path));
					}

					if (remaps.find(path)) {
						path = remaps[path];
					}

					Ref<Resource> res = ResourceLoader::load(path, exttype, cache_mode_for_external);

					if (res.is_null()) {
						WARN_PRINT(vformat("Couldn't load resource: %s.", path));
					}
					r_v = res;

				} break;
				case OBJECT_EXTERNAL_RESOURCE_INDEX: {
					//new file format, just refers to an index in the external list
					int erindex = f->get_32();

					if (erindex < 0 || erindex >= external_resources.size()) {
						WARN_PRINT("Broken external resource! (index out of size)");
						r_v = Variant();
					} else {
						Ref<ResourceLoader::LoadToken> &load_token = external_resources.write[erindex].load_token;
						if (load_token.is_valid()) { // If not valid, it's OK since then we know this load accepts broken dependencies.
							Error err;
							Ref<Resource> res = ResourceLoader::_load_complete(*load_token.ptr(), &err);
							if (res.is_null()) {
								if (!ResourceLoader::is_cleaning_tasks()) {
									if (!ResourceLoader::get_abort_on_missing_resources()) {
										ResourceLoader::notify_dependency_error(local_path, external_resources[erindex].path, external_resources[erindex].type);
									} else {
										error = ERR_FILE_MISSING_DEPENDENCIES;
										ERR_FAIL_V_MSG(error, vformat("Can't load dependency: '%s'.", external_resources[erindex].path));
									}
								}
							} else {
								r_v = res;
							}
						}
					}
				} break;
				default: {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				} break;
			}
		} break;
		case VARIANT_CALLABLE: {
			r_v = Callable();
		} break;
		case VARIANT_SIGNAL: {
			r_v = Signal();
		} break;

		case VARIANT_DICTIONARY: {
			uint32_t len = f->get_32();
			Dictionary d; //last bit means shared
			len &= 0x7FFFFFFF;
			for (uint32_t i = 0; i < len; i++) {
				Variant key;
				Error err = parse_variant(key);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				Variant value;
				err = parse_variant(value);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				d[key] = value;
			}
			r_v = d;
		} break;
		case VARIANT_ARRAY: {
			uint32_t len = f->get_32();
			Array a; //last bit means shared
			len &= 0x7FFFFFFF;
			a.resize(len);
			for (uint32_t i = 0; i < len; i++) {
				Variant val;
				Error err = parse_variant(val);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				a[i] = val;
			}
			r_v = a;

		} break;
		case VARIANT_PACKED_BYTE_ARRAY: {
			uint32_t len = f->get_32();

			Vector<uint8_t> array;
			array.resize(len);
			uint8_t *w = array.ptrw();
			f->get_buffer(w, len);
			_advance_padding(len);

			r_v = array;

		} break;
		case VARIANT_PACKED_INT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int32_t> array;
			array.resize(len);
			int32_t *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(int32_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_INT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int64_t> array;
			array.resize(len);
			int64_t *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(int64_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *ptr = (uint64_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP64(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_FLOAT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<float> array;
			array.resize(len);
			float *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(float));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_FLOAT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<double> array;
			array.resize(len);
			double *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(double));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *ptr = (uint64_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP64(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_STRING_ARRAY: {
			uint32_t len = f->get_32();
			Vector<String> array;
			array.resize(len);
			String *w = array.ptrw();
			for (uint32_t i = 0; i < len; i++) {
				w[i] = get_unicode_string();
			}

			r_v = array;

		} break;
		case VARIANT_PACKED_VECTOR2_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Vector2> array;
			array.resize(len);
			Vector2 *w = array.ptrw();
			static_assert(sizeof(Vector2) == 2 * sizeof(real_t));
			const Error err = read_reals(reinterpret_cast<real_t *>(w), f, len * 2);
			ERR_FAIL_COND_V(err != OK, err);

			r_v = array;

		} break;
		case VARIANT_PACKED_VECTOR3_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Vector3> array;
			array.resize(len);
			Vector3 *w = array.ptrw();
			static_assert(sizeof(Vector3) == 3 * sizeof(real_t));
			const Error err = read_reals(reinterpret_cast<real_t *>(w), f, len * 3);
			ERR_FAIL_COND_V(err != OK, err);

			r_v = array;

		} break;
		case VARIANT_PACKED_COLOR_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Color> array;
			array.resize(len);
			Color *w = array.ptrw();
			// Colors always use `float` even with double-precision support enabled
			static_assert(sizeof(Color) == 4 * sizeof(float));
			f->get_buffer((uint8_t *)w, len * sizeof(float) * 4);
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len * 4; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VARIANT_PACKED_VECTOR4_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Vector4> array;
			array.resize(len);
			Vector4 *w = array.ptrw();
			static_assert(sizeof(Vector4) == 4 * sizeof(real_t));
			const Error err = read_reals(reinterpret_cast<real_t *>(w), f, len * 4);
			ERR_FAIL_COND_V(err != OK, err);

			r_v = array;

		} break;
		default: {
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		} break;
	}

	return OK; //never reach anyway
}

Ref<Resource> ResourceLoaderBinary::get_resource() {
	return resource;
}

Error ResourceLoaderBinary::load() {
	if (error != OK) {
		return error;
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String path = external_resources[i].path;

		if (remaps.has(path)) {
			path = remaps[path];
		}

		if (!path.contains("://") && path.is_relative_path()) {
			// path is relative to file being loaded, so convert to a resource path
			path = ProjectSettings::get_singleton()->localize_path(path.get_base_dir().path_join(external_resources[i].path));
		}

		external_resources.write[i].path = path; //remap happens here, not on load because on load it can actually be used for filesystem dock resource remap
		external_resources.write[i].load_token = ResourceLoader::_load_start(path, external_resources[i].type, use_sub_threads ? ResourceLoader::LOAD_THREAD_DISTRIBUTE : ResourceLoader::LOAD_THREAD_FROM_CURRENT, cache_mode_for_external);
		if (external_resources[i].load_token.is_null()) {
			if (!ResourceLoader::get_abort_on_missing_resources()) {
				ResourceLoader::notify_dependency_error(local_path, path, external_resources[i].type);
			} else {
				error = ERR_FILE_MISSING_DEPENDENCIES;
				ERR_FAIL_V_MSG(error, vformat("Can't load dependency: '%s'.", path));
			}
		}
	}

	for (int i = 0; i < internal_resources.size(); i++) {
		bool main = i == (internal_resources.size() - 1);

		//maybe it is loaded already
		String path;
		String id;

		if (!main) {
			path = internal_resources[i].path;

			if (path.begins_with("local://")) {
				path = path.replace_first("local://", "");
				id = path;
				path = res_path + "::" + path;

				internal_resources.write[i].path = path; // Update path.
			}

			if (cache_mode == ResourceFormatLoader::CACHE_MODE_REUSE && ResourceCache::has(path)) {
				Ref<Resource> cached = ResourceCache::get_ref(path);
				if (cached.is_valid()) {
					//already loaded, don't do anything
					error = OK;
					internal_index_cache[path] = cached;
					continue;
				}
			}
		} else {
			if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE && !ResourceCache::has(res_path)) {
				path = res_path;
			}
		}

		uint64_t offset = internal_resources[i].offset;

		f->seek(offset);

		String t = get_unicode_string();

		Ref<Resource> res;
		Resource *r = nullptr;

		MissingResource *missing_resource = nullptr;

		if (main) {
			res = ResourceLoader::get_resource_ref_override(local_path);
			r = res.ptr();
		}
		if (!r) {
			if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && ResourceCache::has(path)) {
				//use the existing one
				Ref<Resource> cached = ResourceCache::get_ref(path);
				if (cached->get_class() == t) {
					cached->reset_state();
					res = cached;
				}
			}

			if (res.is_null()) {
				//did not replace

				Object *obj = ClassDB::instantiate(t);
				if (!obj) {
					if (ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
						//create a missing resource
						missing_resource = memnew(MissingResource);
						missing_resource->set_original_class(t);
						missing_resource->set_recording_properties(true);
						obj = missing_resource;
					} else {
						error = ERR_FILE_CORRUPT;
						ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, vformat("'%s': Resource of unrecognized type in file: '%s'.", local_path, t));
					}
				}

				r = Object::cast_to<Resource>(obj);
				if (!r) {
					String obj_class = obj->get_class();
					error = ERR_FILE_CORRUPT;
					memdelete(obj); //bye
					ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, vformat("'%s': Resource type in resource field not a resource, type is: %s.", local_path, obj_class));
				}

				res = Ref<Resource>(r);
			}
		}

		if (r) {
			if (!path.is_empty()) {
				if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
					r->set_path(path, cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE); // If got here because the resource with same path has different type, replace it.
				} else {
					r->set_path_cache(path);
				}
			}
			r->set_scene_unique_id(id);
		}

		if (!main) {
			internal_index_cache[path] = res;
		}

		int pc = f->get_32();

		//set properties

		Dictionary missing_resource_properties;

		for (int j = 0; j < pc; j++) {
			StringName name = _get_string();

			if (name == StringName()) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}

			Variant value;

			error = parse_variant(value);
			if (error) {
				return error;
			}

			bool set_valid = true;
			if (value.get_type() == Variant::OBJECT && missing_resource == nullptr && ResourceLoader::is_creating_missing_resources_if_class_unavailable_enabled()) {
				// If the property being set is a missing resource (and the parent is not),
				// then setting it will most likely not work.
				// Instead, save it as metadata.

				Ref<MissingResource> mr = value;
				if (mr.is_valid()) {
					missing_resource_properties[name] = mr;
					set_valid = false;
				}
			}

			if (value.get_type() == Variant::ARRAY) {
				Array set_array = value;
				bool is_get_valid = false;
				Variant get_value = res->get(name, &is_get_valid);
				if (is_get_valid && get_value.get_type() == Variant::ARRAY) {
					Array get_array = get_value;
					if (!set_array.is_same_typed(get_array)) {
						value = Array(set_array, get_array.get_typed_builtin(), get_array.get_typed_class_name(), get_array.get_typed_script());
					}
				}
			}

			if (value.get_type() == Variant::DICTIONARY) {
				Dictionary set_dict = value;
				bool is_get_valid = false;
				Variant get_value = res->get(name, &is_get_valid);
				if (is_get_valid && get_value.get_type() == Variant::DICTIONARY) {
					Dictionary get_dict = get_value;
					if (!set_dict.is_same_typed(get_dict)) {
						value = Dictionary(set_dict, get_dict.get_typed_key_builtin(), get_dict.get_typed_key_class_name(), get_dict.get_typed_key_script(),
								get_dict.get_typed_value_builtin(), get_dict.get_typed_value_class_name(), get_dict.get_typed_value_script());
					}
				}
			}

			if (set_valid) {
				res->set(name, value);
			}
		}

		if (missing_resource) {
			missing_resource->set_recording_properties(false);
		}

		if (!missing_resource_properties.is_empty()) {
			res->set_meta(META_MISSING_RESOURCES, missing_resource_properties);
		}

#ifdef TOOLS_ENABLED
		res->set_edited(false);
#endif

		if (progress) {
			*progress = (i + 1) / float(internal_resources.size());
		}

		resource_cache.push_back(res);

		if (main) {
			f.unref();
			resource = res;
			resource->set_as_translation_remapped(translation_remapped);
			error = OK;
			return OK;
		}
	}

	return ERR_FILE_EOF;
}

void ResourceLoaderBinary::set_translation_remapped(bool p_remapped) {
	translation_remapped = p_remapped;
}

static void save_ustring(Ref<FileAccess> f, const String &p_string) {
	CharString utf8 = p_string.utf8();
	f->store_32(uint32_t(utf8.length() + 1));
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

static String get_ustring(Ref<FileAccess> f) {
	int len = f->get_32();
	Vector<char> str_buf;
	str_buf.resize(len);
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0], len);
	return s;
}

String ResourceLoaderBinary::get_unicode_string() {
	int len = f->get_32();
	if (len > str_buf.size()) {
		str_buf.resize(len);
	}
	if (len == 0) {
		return String();
	}
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0], len);
	return s;
}

void ResourceLoaderBinary::get_classes_used(Ref<FileAccess> p_f, HashSet<StringName> *p_classes) {
	open(p_f, false, true);
	if (error) {
		return;
	}

	for (int i = 0; i < internal_resources.size(); i++) {
		p_f->seek(internal_resources[i].offset);
		String t = get_unicode_string();
		ERR_FAIL_COND(p_f->get_error() != OK);
		if (t != String()) {
			p_classes->insert(t);
		}
	}
}

void ResourceLoaderBinary::get_dependencies(Ref<FileAccess> p_f, List<String> *p_dependencies, bool p_add_types) {
	open(p_f, false, true);
	if (error) {
		return;
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String dep;
		String fallback_path;

		if (external_resources[i].uid != ResourceUID::INVALID_ID) {
			dep = ResourceUID::get_singleton()->id_to_text(external_resources[i].uid);
			fallback_path = external_resources[i].path; // Used by Dependency Editor, in case uid path fails.
		} else {
			dep = external_resources[i].path;
		}

		if (p_add_types && !external_resources[i].type.is_empty()) {
			dep += "::" + external_resources[i].type;
		}
		if (!fallback_path.is_empty()) {
			if (!p_add_types) {
				dep += "::"; // Ensure that path comes third, even if there is no type.
			}
			dep += "::" + fallback_path;
		}

		p_dependencies->push_back(dep);
	}
}

void ResourceLoaderBinary::open(Ref<FileAccess> p_f, bool p_no_resources, bool p_keep_uuid_paths) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		error = fac->open_after_magic(f);
		if (error != OK) {
			f.unref();
			ERR_FAIL_MSG(vformat("Failed to open binary resource file: '%s'.", local_path));
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f.unref();
		ERR_FAIL_MSG(vformat("Unrecognized binary resource file: '%s'.", local_path));
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	ver_format = f->get_32();

	print_bl("big endian: " + itos(big_endian));
#ifdef BIG_ENDIAN_ENABLED
	print_bl("endian swap: " + itos(!big_endian));
#else
	print_bl("endian swap: " + itos(big_endian));
#endif
	print_bl("real64: " + itos(use_real64));
	print_bl("major: " + itos(ver_major));
	print_bl("minor: " + itos(ver_minor));
	print_bl("format: " + itos(ver_format));

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		f.unref();
		ERR_FAIL_MSG(vformat("File '%s' can't be loaded, as it uses a format version (%d) or engine version (%d.%d) which are not supported by your engine version (%s).",
				local_path, ver_format, ver_major, ver_minor, VERSION_BRANCH));
	}

	type = get_unicode_string();

	print_bl("type: " + type);

	importmd_ofs = f->get_64();
	uint32_t flags = f->get_32();
	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_NAMED_SCENE_IDS) {
		using_named_scene_ids = true;
	}
	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS) {
		using_uids = true;
	}
	f->real_is_double = (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_REAL_T_IS_DOUBLE) != 0;

	if (using_uids) {
		uid = ResourceUID::ID(f->get_64());
	} else {
		f->get_64(); // skip over uid field
		uid = ResourceUID::INVALID_ID;
	}

	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		script_class = get_unicode_string();
	}

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		f->get_32(); //skip a few reserved fields
	}

	if (p_no_resources) {
		return;
	}

	uint32_t string_table_size = f->get_32();
	string_map.resize(string_table_size);
	for (uint32_t i = 0; i < string_table_size; i++) {
		StringName s = get_unicode_string();
		string_map.write[i] = s;
	}

	print_bl("strings: " + itos(string_table_size));

	uint32_t ext_resources_size = f->get_32();
	for (uint32_t i = 0; i < ext_resources_size; i++) {
		ExtResource er;
		er.type = get_unicode_string();
		er.path = get_unicode_string();
		if (using_uids) {
			er.uid = ResourceUID::ID(f->get_64());
			if (!p_keep_uuid_paths && er.uid != ResourceUID::INVALID_ID) {
				if (ResourceUID::get_singleton()->has_id(er.uid)) {
					// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
					er.path = ResourceUID::get_singleton()->get_id_path(er.uid);
				} else {
#ifdef TOOLS_ENABLED
					// Silence a warning that can happen during the initial filesystem scan due to cache being regenerated.
					if (ResourceLoader::get_resource_uid(res_path) != er.uid) {
						WARN_PRINT(vformat("'%s': In external resource #%d, invalid UID: '%s' - using text path instead: '%s'.", res_path, i, ResourceUID::get_singleton()->id_to_text(er.uid), er.path));
					}
#else
					WARN_PRINT(vformat("'%s': In external resource #%d, invalid UID: '%s' - using text path instead: '%s'.", res_path, i, ResourceUID::get_singleton()->id_to_text(er.uid), er.path));
#endif
				}
			}
		}

		external_resources.push_back(er);
	}

	print_bl("ext resources: " + itos(ext_resources_size));
	uint32_t int_resources_size = f->get_32();

	for (uint32_t i = 0; i < int_resources_size; i++) {
		IntResource ir;
		ir.path = get_unicode_string();
		ir.offset = f->get_64();
		internal_resources.push_back(ir);
	}

	print_bl("int resources: " + itos(int_resources_size));

	if (f->eof_reached()) {
		error = ERR_FILE_CORRUPT;
		f.unref();
		ERR_FAIL_MSG(vformat("Premature end of file (EOF): '%s'.", local_path));
	}
}

String ResourceLoaderBinary::recognize(Ref<FileAccess> p_f) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		error = fac->open_after_magic(f);
		if (error != OK) {
			f.unref();
			return "";
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f.unref();
		return "";
	}

	bool big_endian = f->get_32();
	f->get_32(); // use_real64

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	f->get_32(); // ver_minor
	uint32_t ver_fmt = f->get_32();

	if (ver_fmt > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		f.unref();
		return "";
	}

	return get_unicode_string();
}

String ResourceLoaderBinary::recognize_script_class(Ref<FileAccess> p_f) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		error = fac->open_after_magic(f);
		if (error != OK) {
			f.unref();
			return "";
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f.unref();
		return "";
	}

	bool big_endian = f->get_32();
	f->get_32(); // use_real64

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	f->get_32(); // ver_minor
	uint32_t ver_fmt = f->get_32();

	if (ver_fmt > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		f.unref();
		return "";
	}

	get_unicode_string(); // type

	f->get_64(); // Metadata offset
	uint32_t flags = f->get_32();
	f->get_64(); // UID

	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		return get_unicode_string();
	} else {
		return String();
	}
}

Ref<Resource> ResourceFormatLoaderBinary::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), vformat("Cannot open file '%s'.", p_path));

	ResourceLoaderBinary loader;
	switch (p_cache_mode) {
		case CACHE_MODE_IGNORE:
		case CACHE_MODE_REUSE:
		case CACHE_MODE_REPLACE:
			loader.cache_mode = p_cache_mode;
			loader.cache_mode_for_external = CACHE_MODE_REUSE;
			break;
		case CACHE_MODE_IGNORE_DEEP:
			loader.cache_mode = CACHE_MODE_IGNORE;
			loader.cache_mode_for_external = p_cache_mode;
			break;
		case CACHE_MODE_REPLACE_DEEP:
			loader.cache_mode = CACHE_MODE_REPLACE;
			loader.cache_mode_for_external = p_cache_mode;
			break;
	}
	loader.use_sub_threads = p_use_sub_threads;
	loader.progress = r_progress;
	String path = !p_original_path.is_empty() ? p_original_path : p_path;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(path);
	loader.res_path = loader.local_path;
	loader.open(f);

	err = loader.load();

	if (r_error) {
		*r_error = err;
	}

	if (err) {
		return Ref<Resource>();
	}
	return loader.resource;
}

void ResourceFormatLoaderBinary::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type.is_empty()) {
		get_recognized_extensions(p_extensions);
		return;
	}

	// res files not supported for GDExtension.
	if (p_type == "GDExtension") {
		return;
	}

	List<String> extensions;
	ClassDB::get_extensions_for_type(p_type, &extensions);

	extensions.sort();

	for (const String &E : extensions) {
		String ext = E.to_lower();
		p_extensions->push_back(ext);
	}
}

void ResourceFormatLoaderBinary::get_recognized_extensions(List<String> *p_extensions) const {
	List<String> extensions;
	ClassDB::get_resource_base_extensions(&extensions);
	extensions.sort();

	for (const String &E : extensions) {
		String ext = E.to_lower();
		p_extensions->push_back(ext);
	}
}

bool ResourceFormatLoaderBinary::handles_type(const String &p_type) const {
	return true; //handles all
}

void ResourceFormatLoaderBinary::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(f.is_null(), vformat("Cannot open file '%s'.", p_path));

	ResourceLoaderBinary loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	loader.get_dependencies(f, p_dependencies, p_add_types);
}

Error ResourceFormatLoaderBinary::rename_dependencies(const String &p_path, const HashMap<String, String> &p_map) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, vformat("Cannot open file '%s'.", p_path));

	Ref<FileAccess> fw;

	String local_path = p_path.get_base_dir();

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		Error err = fac->open_after_magic(f);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot open file '%s'.", p_path));
		f = fac;

		Ref<FileAccessCompressed> facw;
		facw.instantiate();
		facw->configure("RSCC");
		err = facw->open_internal(p_path + ".depren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, vformat("Cannot create file '%s.depren'.", p_path));

		fw = facw;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("Unrecognized binary resource file '%s'.", local_path));
	} else {
		fw = FileAccess::open(p_path + ".depren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(fw.is_null(), ERR_CANT_CREATE, vformat("Cannot create file '%s.depren'.", p_path));

		uint8_t magic[4] = { 'R', 'S', 'R', 'C' };
		fw->store_buffer(magic, 4);
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian
#ifdef BIG_ENDIAN_ENABLED
	fw->store_32(!big_endian);
#else
	fw->store_32(big_endian);
#endif
	fw->set_big_endian(big_endian != 0);
	fw->store_32(use_real64); //use real64

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	if (ver_format < FORMAT_VERSION_CAN_RENAME_DEPS) {
		fw.unref();

		{
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			da->remove(p_path + ".depren");
		}

		// Use the old approach.

		WARN_PRINT(vformat("This file is old, so it can't refactor dependencies, opening and resaving '%s'.", p_path));

		Error err;
		f = FileAccess::open(p_path, FileAccess::READ, &err);

		ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_CANT_OPEN, vformat("Cannot open file '%s'.", p_path));

		ResourceLoaderBinary loader;
		loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
		loader.res_path = loader.local_path;
		loader.remaps = p_map;
		loader.open(f);

		err = loader.load();

		ERR_FAIL_COND_V(err != ERR_FILE_EOF, ERR_FILE_CORRUPT);
		Ref<Resource> res = loader.get_resource();
		ERR_FAIL_COND_V(res.is_null(), ERR_FILE_CORRUPT);

		return ResourceFormatSaverBinary::singleton->save(res, p_path);
	}

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED,
				vformat("File '%s' can't be loaded, as it uses a format version (%d) or engine version (%d.%d) which are not supported by your engine version (%s).",
						local_path, ver_format, ver_major, ver_minor, VERSION_BRANCH));
	}

	// Since we're not actually converting the file contents, leave the version
	// numbers in the file untouched.
	fw->store_32(ver_major);
	fw->store_32(ver_minor);
	fw->store_32(ver_format);

	save_ustring(fw, get_ustring(f)); //type

	uint64_t md_ofs = f->get_position();
	uint64_t importmd_ofs = f->get_64();
	fw->store_64(0); //metadata offset

	uint32_t flags = f->get_32();
	bool using_uids = (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS);
	uint64_t uid_data = f->get_64();

	fw->store_32(flags);
	fw->store_64(uid_data);
	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		save_ustring(fw, get_ustring(f));
	}

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		fw->store_32(0); // reserved
		f->get_32();
	}

	//string table
	uint32_t string_table_size = f->get_32();

	fw->store_32(string_table_size);

	for (uint32_t i = 0; i < string_table_size; i++) {
		String s = get_ustring(f);
		save_ustring(fw, s);
	}

	//external resources
	uint32_t ext_resources_size = f->get_32();
	fw->store_32(ext_resources_size);
	for (uint32_t i = 0; i < ext_resources_size; i++) {
		String type = get_ustring(f);
		String path = get_ustring(f);

		if (using_uids) {
			ResourceUID::ID uid = f->get_64();
			if (uid != ResourceUID::INVALID_ID) {
				if (ResourceUID::get_singleton()->has_id(uid)) {
					// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
					path = ResourceUID::get_singleton()->get_id_path(uid);
				}
			}
		}

		bool relative = false;
		if (!path.begins_with("res://")) {
			path = local_path.path_join(path).simplify_path();
			relative = true;
		}

		if (p_map.has(path)) {
			String np = p_map[path];
			path = np;
		}

		String full_path = path;

		if (relative) {
			//restore relative
			path = local_path.path_to_file(path);
		}

		save_ustring(fw, type);
		save_ustring(fw, path);

		if (using_uids) {
			ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(full_path);
			fw->store_64(uint64_t(uid));
		}
	}

	int64_t size_diff = (int64_t)fw->get_position() - (int64_t)f->get_position();

	//internal resources
	uint32_t int_resources_size = f->get_32();
	fw->store_32(int_resources_size);

	for (uint32_t i = 0; i < int_resources_size; i++) {
		String path = get_ustring(f);
		uint64_t offset = f->get_64();
		save_ustring(fw, path);
		fw->store_64(offset + size_diff);
	}

	//rest of file
	uint8_t b = f->get_8();
	while (!f->eof_reached()) {
		fw->store_8(b);
		b = f->get_8();
	}
	f.unref();

	bool all_ok = fw->get_error() == OK;

	fw->seek(md_ofs);
	fw->store_64(importmd_ofs + size_diff);

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	fw.unref();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (da->exists(p_path + ".depren")) {
		da->remove(p_path);
		da->rename(p_path + ".depren", p_path);
	}
	return OK;
}

void ResourceFormatLoaderBinary::get_classes_used(const String &p_path, HashSet<StringName> *r_classes) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(f.is_null(), vformat("Cannot open file '%s'.", p_path));

	ResourceLoaderBinary loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	loader.get_classes_used(f, r_classes);
}

String ResourceFormatLoaderBinary::get_resource_type(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderBinary loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	String r = loader.recognize(f);
	return ClassDB::get_compatibility_remapped_class(r);
}

String ResourceFormatLoaderBinary::get_resource_script_class(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ""; //could not read
	}

	ResourceLoaderBinary loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	return loader.recognize_script_class(f);
}

ResourceUID::ID ResourceFormatLoaderBinary::get_resource_uid(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();
	if (!ClassDB::is_resource_extension(ext)) {
		return ResourceUID::INVALID_ID;
	}

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return ResourceUID::INVALID_ID; //could not read
	}

	ResourceLoaderBinary loader;
	loader.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	loader.res_path = loader.local_path;
	loader.open(f, true);
	if (loader.error != OK) {
		return ResourceUID::INVALID_ID; //could not read
	}
	return loader.uid;
}

bool ResourceFormatLoaderBinary::has_custom_uid_support() const {
	return true;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

void ResourceFormatSaverBinaryInstance::_pad_buffer(Ref<FileAccess> f, int p_bytes) {
	int extra = 4 - (p_bytes % 4);
	if (extra < 4) {
		for (int i = 0; i < extra; i++) {
			f->store_8(0); //pad to 32
		}
	}
}

void ResourceFormatSaverBinaryInstance::write_variant(Ref<FileAccess> f, const Variant &p_property, HashMap<Ref<Resource>, int> &resource_map, HashMap<Ref<Resource>, int> &external_resources, HashMap<StringName, int> &string_map, const PropertyInfo &p_hint) {
	switch (p_property.get_type()) {
		case Variant::NIL: {
			f->store_32(VARIANT_NIL);
			// don't store anything
		} break;
		case Variant::BOOL: {
			f->store_32(VARIANT_BOOL);
			bool val = p_property;
			f->store_32(val);
		} break;
		case Variant::INT: {
			int64_t val = p_property;
			if (val > 0x7FFFFFFF || val < -(int64_t)0x80000000) {
				f->store_32(VARIANT_INT64);
				f->store_64(uint64_t(val));

			} else {
				f->store_32(VARIANT_INT);
				f->store_32(uint32_t(p_property));
			}

		} break;
		case Variant::FLOAT: {
			double d = p_property;
			float fl = d;
			if (double(fl) != d) {
				f->store_32(VARIANT_DOUBLE);
				f->store_double(d);
			} else {
				f->store_32(VARIANT_FLOAT);
				f->store_real(fl);
			}

		} break;
		case Variant::STRING: {
			f->store_32(VARIANT_STRING);
			String val = p_property;
			save_unicode_string(f, val);

		} break;
		case Variant::VECTOR2: {
			f->store_32(VARIANT_VECTOR2);
			Vector2 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);

		} break;
		case Variant::VECTOR2I: {
			f->store_32(VARIANT_VECTOR2I);
			Vector2i val = p_property;
			f->store_32(uint32_t(val.x));
			f->store_32(uint32_t(val.y));

		} break;
		case Variant::RECT2: {
			f->store_32(VARIANT_RECT2);
			Rect2 val = p_property;
			f->store_real(val.position.x);
			f->store_real(val.position.y);
			f->store_real(val.size.x);
			f->store_real(val.size.y);

		} break;
		case Variant::RECT2I: {
			f->store_32(VARIANT_RECT2I);
			Rect2i val = p_property;
			f->store_32(uint32_t(val.position.x));
			f->store_32(uint32_t(val.position.y));
			f->store_32(uint32_t(val.size.x));
			f->store_32(uint32_t(val.size.y));

		} break;
		case Variant::VECTOR3: {
			f->store_32(VARIANT_VECTOR3);
			Vector3 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);

		} break;
		case Variant::VECTOR3I: {
			f->store_32(VARIANT_VECTOR3I);
			Vector3i val = p_property;
			f->store_32(uint32_t(val.x));
			f->store_32(uint32_t(val.y));
			f->store_32(uint32_t(val.z));

		} break;
		case Variant::VECTOR4: {
			f->store_32(VARIANT_VECTOR4);
			Vector4 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);
			f->store_real(val.w);

		} break;
		case Variant::VECTOR4I: {
			f->store_32(VARIANT_VECTOR4I);
			Vector4i val = p_property;
			f->store_32(uint32_t(val.x));
			f->store_32(uint32_t(val.y));
			f->store_32(uint32_t(val.z));
			f->store_32(uint32_t(val.w));

		} break;
		case Variant::PLANE: {
			f->store_32(VARIANT_PLANE);
			Plane val = p_property;
			f->store_real(val.normal.x);
			f->store_real(val.normal.y);
			f->store_real(val.normal.z);
			f->store_real(val.d);

		} break;
		case Variant::QUATERNION: {
			f->store_32(VARIANT_QUATERNION);
			Quaternion val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);
			f->store_real(val.w);

		} break;
		case Variant::AABB: {
			f->store_32(VARIANT_AABB);
			AABB val = p_property;
			f->store_real(val.position.x);
			f->store_real(val.position.y);
			f->store_real(val.position.z);
			f->store_real(val.size.x);
			f->store_real(val.size.y);
			f->store_real(val.size.z);

		} break;
		case Variant::TRANSFORM2D: {
			f->store_32(VARIANT_TRANSFORM2D);
			Transform2D val = p_property;
			f->store_real(val.columns[0].x);
			f->store_real(val.columns[0].y);
			f->store_real(val.columns[1].x);
			f->store_real(val.columns[1].y);
			f->store_real(val.columns[2].x);
			f->store_real(val.columns[2].y);

		} break;
		case Variant::BASIS: {
			f->store_32(VARIANT_BASIS);
			Basis val = p_property;
			f->store_real(val.rows[0].x);
			f->store_real(val.rows[0].y);
			f->store_real(val.rows[0].z);
			f->store_real(val.rows[1].x);
			f->store_real(val.rows[1].y);
			f->store_real(val.rows[1].z);
			f->store_real(val.rows[2].x);
			f->store_real(val.rows[2].y);
			f->store_real(val.rows[2].z);

		} break;
		case Variant::TRANSFORM3D: {
			f->store_32(VARIANT_TRANSFORM3D);
			Transform3D val = p_property;
			f->store_real(val.basis.rows[0].x);
			f->store_real(val.basis.rows[0].y);
			f->store_real(val.basis.rows[0].z);
			f->store_real(val.basis.rows[1].x);
			f->store_real(val.basis.rows[1].y);
			f->store_real(val.basis.rows[1].z);
			f->store_real(val.basis.rows[2].x);
			f->store_real(val.basis.rows[2].y);
			f->store_real(val.basis.rows[2].z);
			f->store_real(val.origin.x);
			f->store_real(val.origin.y);
			f->store_real(val.origin.z);

		} break;
		case Variant::PROJECTION: {
			f->store_32(VARIANT_PROJECTION);
			Projection val = p_property;
			f->store_real(val.columns[0].x);
			f->store_real(val.columns[0].y);
			f->store_real(val.columns[0].z);
			f->store_real(val.columns[0].w);
			f->store_real(val.columns[1].x);
			f->store_real(val.columns[1].y);
			f->store_real(val.columns[1].z);
			f->store_real(val.columns[1].w);
			f->store_real(val.columns[2].x);
			f->store_real(val.columns[2].y);
			f->store_real(val.columns[2].z);
			f->store_real(val.columns[2].w);
			f->store_real(val.columns[3].x);
			f->store_real(val.columns[3].y);
			f->store_real(val.columns[3].z);
			f->store_real(val.columns[3].w);

		} break;
		case Variant::COLOR: {
			f->store_32(VARIANT_COLOR);
			Color val = p_property;
			// Color are always floats
			f->store_float(val.r);
			f->store_float(val.g);
			f->store_float(val.b);
			f->store_float(val.a);

		} break;
		case Variant::STRING_NAME: {
			f->store_32(VARIANT_STRING_NAME);
			String val = p_property;
			save_unicode_string(f, val);

		} break;

		case Variant::NODE_PATH: {
			f->store_32(VARIANT_NODE_PATH);
			NodePath np = p_property;
			f->store_16(np.get_name_count());
			uint16_t snc = np.get_subname_count();
			if (np.is_absolute()) {
				snc |= 0x8000;
			}
			f->store_16(snc);
			for (int i = 0; i < np.get_name_count(); i++) {
				if (string_map.has(np.get_name(i))) {
					f->store_32(uint32_t(string_map[np.get_name(i)]));
				} else {
					save_unicode_string(f, np.get_name(i), true);
				}
			}
			for (int i = 0; i < np.get_subname_count(); i++) {
				if (string_map.has(np.get_subname(i))) {
					f->store_32(uint32_t(string_map[np.get_subname(i)]));
				} else {
					save_unicode_string(f, np.get_subname(i), true);
				}
			}

		} break;
		case Variant::RID: {
			f->store_32(VARIANT_RID);
			WARN_PRINT("Can't save RIDs.");
			RID val = p_property;
			f->store_32(uint32_t(val.get_id()));
		} break;
		case Variant::OBJECT: {
			f->store_32(VARIANT_OBJECT);
			Ref<Resource> res = p_property;
			if (res.is_null() || res->get_meta(SNAME("_skip_save_"), false)) {
				f->store_32(OBJECT_EMPTY);
				return; // Don't save it.
			}

			if (!res->is_built_in()) {
				f->store_32(OBJECT_EXTERNAL_RESOURCE_INDEX);
				f->store_32(uint32_t(external_resources[res]));
			} else {
				if (!resource_map.has(res)) {
					f->store_32(OBJECT_EMPTY);
					ERR_FAIL_MSG("Resource was not pre cached for the resource section, most likely due to circular reference.");
				}

				f->store_32(OBJECT_INTERNAL_RESOURCE);
				f->store_32(uint32_t(resource_map[res]));
				//internal resource
			}

		} break;
		case Variant::CALLABLE: {
			f->store_32(VARIANT_CALLABLE);
			WARN_PRINT("Can't save Callables.");
		} break;
		case Variant::SIGNAL: {
			f->store_32(VARIANT_SIGNAL);
			WARN_PRINT("Can't save Signals.");
		} break;

		case Variant::DICTIONARY: {
			f->store_32(VARIANT_DICTIONARY);
			Dictionary d = p_property;
			f->store_32(uint32_t(d.size()));

			List<Variant> keys;
			d.get_key_list(&keys);

			for (const Variant &E : keys) {
				write_variant(f, E, resource_map, external_resources, string_map);
				write_variant(f, d[E], resource_map, external_resources, string_map);
			}

		} break;
		case Variant::ARRAY: {
			f->store_32(VARIANT_ARRAY);
			Array a = p_property;
			f->store_32(uint32_t(a.size()));
			for (const Variant &var : a) {
				write_variant(f, var, resource_map, external_resources, string_map);
			}

		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			f->store_32(VARIANT_PACKED_BYTE_ARRAY);
			Vector<uint8_t> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const uint8_t *r = arr.ptr();
			f->store_buffer(r, len);
			_pad_buffer(f, len);

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			f->store_32(VARIANT_PACKED_INT32_ARRAY);
			Vector<int32_t> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const int32_t *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_32(uint32_t(r[i]));
			}

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			f->store_32(VARIANT_PACKED_INT64_ARRAY);
			Vector<int64_t> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const int64_t *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_64(uint64_t(r[i]));
			}

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			f->store_32(VARIANT_PACKED_FLOAT32_ARRAY);
			Vector<float> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const float *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_float(r[i]);
			}

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			f->store_32(VARIANT_PACKED_FLOAT64_ARRAY);
			Vector<double> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const double *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_double(r[i]);
			}

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			f->store_32(VARIANT_PACKED_STRING_ARRAY);
			Vector<String> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const String *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				save_unicode_string(f, r[i]);
			}
		} break;

		case Variant::PACKED_VECTOR2_ARRAY: {
			f->store_32(VARIANT_PACKED_VECTOR2_ARRAY);
			Vector<Vector2> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const Vector2 *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
			}
		} break;

		case Variant::PACKED_VECTOR3_ARRAY: {
			f->store_32(VARIANT_PACKED_VECTOR3_ARRAY);
			Vector<Vector3> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const Vector3 *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
				f->store_real(r[i].z);
			}
		} break;

		case Variant::PACKED_COLOR_ARRAY: {
			f->store_32(VARIANT_PACKED_COLOR_ARRAY);
			Vector<Color> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const Color *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_float(r[i].r);
				f->store_float(r[i].g);
				f->store_float(r[i].b);
				f->store_float(r[i].a);
			}

		} break;
		case Variant::PACKED_VECTOR4_ARRAY: {
			f->store_32(VARIANT_PACKED_VECTOR4_ARRAY);
			Vector<Vector4> arr = p_property;
			int len = arr.size();
			f->store_32(uint32_t(len));
			const Vector4 *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
				f->store_real(r[i].z);
				f->store_real(r[i].w);
			}

		} break;
		default: {
			ERR_FAIL_MSG("Invalid variant.");
		}
	}
}

void ResourceFormatSaverBinaryInstance::_find_resources(const Variant &p_variant, bool p_main) {
	switch (p_variant.get_type()) {
		case Variant::OBJECT: {
			Ref<Resource> res = p_variant;

			if (res.is_null() || external_resources.has(res) || res->get_meta(SNAME("_skip_save_"), false)) {
				return;
			}

			if (!p_main && (!bundle_resources) && !res->is_built_in()) {
				if (res->get_path() == path) {
					ERR_PRINT(vformat("Circular reference to resource being saved found: '%s' will be null next time it's loaded.", local_path));
					return;
				}
				int idx = external_resources.size();
				external_resources[res] = idx;
				return;
			}

			if (resource_set.has(res)) {
				return;
			}

			resource_set.insert(res);

			List<PropertyInfo> property_list;

			res->get_property_list(&property_list);

			for (const PropertyInfo &E : property_list) {
				if (E.usage & PROPERTY_USAGE_STORAGE) {
					Variant value = res->get(E.name);
					if (E.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = res;
						npk.property = E.name;
						non_persistent_map[npk] = value;

						Ref<Resource> sres = value;
						if (sres.is_valid()) {
							resource_set.insert(sres);
							saved_resources.push_back(sres);
						} else {
							_find_resources(value);
						}
					} else {
						_find_resources(value);
					}
				}
			}

			saved_resources.push_back(res);

		} break;

		case Variant::ARRAY: {
			Array varray = p_variant;
			_find_resources(varray.get_typed_script());
			for (const Variant &v : varray) {
				_find_resources(v);
			}

		} break;

		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			_find_resources(d.get_typed_key_script());
			_find_resources(d.get_typed_value_script());
			List<Variant> keys;
			d.get_key_list(&keys);
			for (const Variant &E : keys) {
				_find_resources(E);
				Variant v = d[E];
				_find_resources(v);
			}
		} break;
		case Variant::NODE_PATH: {
			//take the chance and save node path strings
			NodePath np = p_variant;
			for (int i = 0; i < np.get_name_count(); i++) {
				get_string_index(np.get_name(i));
			}
			for (int i = 0; i < np.get_subname_count(); i++) {
				get_string_index(np.get_subname(i));
			}

		} break;
		default: {
		}
	}
}

void ResourceFormatSaverBinaryInstance::save_unicode_string(Ref<FileAccess> p_f, const String &p_string, bool p_bit_on_len) {
	CharString utf8 = p_string.utf8();
	if (p_bit_on_len) {
		p_f->store_32(uint32_t((utf8.length() + 1) | 0x80000000));
	} else {
		p_f->store_32(uint32_t(utf8.length() + 1));
	}
	p_f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

int ResourceFormatSaverBinaryInstance::get_string_index(const String &p_string) {
	StringName s = p_string;
	if (string_map.has(s)) {
		return string_map[s];
	}

	string_map[s] = strings.size();
	strings.push_back(s);
	return strings.size() - 1;
}

static String _resource_get_class(Ref<Resource> p_resource) {
	Ref<MissingResource> missing_resource = p_resource;
	if (missing_resource.is_valid()) {
		return missing_resource->get_original_class();
	} else {
		return p_resource->get_class();
	}
}

Error ResourceFormatSaverBinaryInstance::save(const String &p_path, const Ref<Resource> &p_resource, uint32_t p_flags) {
	Resource::seed_scene_unique_id(p_path.hash());

	Error err;
	Ref<FileAccess> f;
	if (p_flags & ResourceSaver::FLAG_COMPRESS) {
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		fac->configure("RSCC");
		f = fac;
		err = fac->open_internal(p_path, FileAccess::WRITE);
	} else {
		f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot create file '%s'.", p_path));

	relative_paths = p_flags & ResourceSaver::FLAG_RELATIVE_PATHS;
	skip_editor = p_flags & ResourceSaver::FLAG_OMIT_EDITOR_PROPERTIES;
	bundle_resources = p_flags & ResourceSaver::FLAG_BUNDLE_RESOURCES;
	big_endian = p_flags & ResourceSaver::FLAG_SAVE_BIG_ENDIAN;
	takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	if (!p_path.begins_with("res://")) {
		takeover_paths = false;
	}

	local_path = p_path.get_base_dir();
	path = ProjectSettings::get_singleton()->localize_path(p_path);

	_find_resources(p_resource, true);

	if (!(p_flags & ResourceSaver::FLAG_COMPRESS)) {
		//save header compressed
		static const uint8_t header[4] = { 'R', 'S', 'R', 'C' };
		f->store_buffer(header, 4);
	}

	if (big_endian) {
		f->store_32(1);
		f->set_big_endian(true);
	} else {
		f->store_32(0);
	}

	f->store_32(0); //64 bits file, false for now
	f->store_32(VERSION_MAJOR);
	f->store_32(VERSION_MINOR);
	f->store_32(FORMAT_VERSION);

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	save_unicode_string(f, _resource_get_class(p_resource));
	f->store_64(0); //offset to import metadata

	String script_class;
	{
		uint32_t format_flags = FORMAT_FLAG_NAMED_SCENE_IDS | FORMAT_FLAG_UIDS;
#ifdef REAL_T_IS_DOUBLE
		format_flags |= FORMAT_FLAG_REAL_T_IS_DOUBLE;
#endif
		if (!p_resource->is_class("PackedScene")) {
			Ref<Script> s = p_resource->get_script();
			if (s.is_valid()) {
				script_class = s->get_global_name();
				if (!script_class.is_empty()) {
					format_flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS;
				}
			}
		}

		f->store_32(format_flags);
	}
	ResourceUID::ID uid = ResourceSaver::get_resource_id_for_path(p_path, true);
	f->store_64(uint64_t(uid));
	if (!script_class.is_empty()) {
		save_unicode_string(f, script_class);
	}

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		f->store_32(0); // reserved
	}

	List<ResourceData> resources;

	{
		for (const Ref<Resource> &E : saved_resources) {
			Dictionary missing_resource_properties = E->get_meta(META_MISSING_RESOURCES, Dictionary());

			ResourceData &rd = resources.push_back(ResourceData())->get();
			rd.type = _resource_get_class(E);

			List<PropertyInfo> property_list;
			E->get_property_list(&property_list);

			for (const PropertyInfo &F : property_list) {
				if (skip_editor && F.name.begins_with("__editor")) {
					continue;
				}
				if (F.name == META_PROPERTY_MISSING_RESOURCES) {
					continue;
				}

				if ((F.usage & PROPERTY_USAGE_STORAGE) || missing_resource_properties.has(F.name)) {
					Property p;
					p.name_idx = get_string_index(F.name);

					if (F.usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = E;
						npk.property = F.name;
						if (non_persistent_map.has(npk)) {
							p.value = non_persistent_map[npk];
						}
					} else {
						p.value = E->get(F.name);
					}

					if (F.type == Variant::OBJECT && missing_resource_properties.has(F.name)) {
						// Was this missing resource overridden? If so do not save the old value.
						Ref<Resource> res = p.value;
						if (res.is_null()) {
							p.value = missing_resource_properties[F.name];
						}
					}

					Variant default_value = ClassDB::class_get_default_property_value(E->get_class(), F.name);

					if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, p.value, default_value))) {
						continue;
					}

					p.pi = F;

					rd.properties.push_back(p);
				}
			}
		}
	}

	f->store_32(uint32_t(strings.size())); //string table size
	for (int i = 0; i < strings.size(); i++) {
		save_unicode_string(f, strings[i]);
	}

	// save external resource table
	f->store_32(external_resources.size()); //amount of external resources
	Vector<Ref<Resource>> save_order;
	save_order.resize(external_resources.size());

	for (const KeyValue<Ref<Resource>, int> &E : external_resources) {
		save_order.write[E.value] = E.key;
	}

	for (int i = 0; i < save_order.size(); i++) {
		save_unicode_string(f, save_order[i]->get_save_class());
		String res_path = save_order[i]->get_path();
		res_path = relative_paths ? local_path.path_to_file(res_path) : res_path;
		save_unicode_string(f, res_path);
		ResourceUID::ID ruid = ResourceSaver::get_resource_id_for_path(save_order[i]->get_path(), false);
		f->store_64(uint64_t(ruid));
	}
	// save internal resource table
	f->store_32(uint32_t(saved_resources.size())); //amount of internal resources
	Vector<uint64_t> ofs_pos;
	HashSet<String> used_unique_ids;

	for (Ref<Resource> &r : saved_resources) {
		if (r->is_built_in()) {
			if (!r->get_scene_unique_id().is_empty()) {
				if (used_unique_ids.has(r->get_scene_unique_id())) {
					r->set_scene_unique_id("");
				} else {
					used_unique_ids.insert(r->get_scene_unique_id());
				}
			}
		}
	}

	HashMap<Ref<Resource>, int> resource_map;
	int res_index = 0;
	for (Ref<Resource> &r : saved_resources) {
		if (r->is_built_in()) {
			if (r->get_scene_unique_id().is_empty()) {
				String new_id;

				while (true) {
					new_id = _resource_get_class(r) + "_" + Resource::generate_scene_unique_id();
					if (!used_unique_ids.has(new_id)) {
						break;
					}
				}

				r->set_scene_unique_id(new_id);
				used_unique_ids.insert(new_id);
			}

			save_unicode_string(f, "local://" + r->get_scene_unique_id());
			if (takeover_paths) {
				r->set_path(p_path + "::" + r->get_scene_unique_id(), true);
			}
#ifdef TOOLS_ENABLED
			r->set_edited(false);
#endif
		} else {
			save_unicode_string(f, r->get_path()); //actual external
		}
		ofs_pos.push_back(f->get_position());
		f->store_64(0); //offset in 64 bits
		resource_map[r] = res_index++;
	}

	Vector<uint64_t> ofs_table;

	//now actually save the resources
	for (const ResourceData &rd : resources) {
		ofs_table.push_back(f->get_position());
		save_unicode_string(f, rd.type);
		f->store_32(uint32_t(rd.properties.size()));

		for (const Property &p : rd.properties) {
			f->store_32(uint32_t(p.name_idx));
			write_variant(f, p.value, resource_map, external_resources, string_map, p.pi);
		}
	}

	for (int i = 0; i < ofs_table.size(); i++) {
		f->seek(ofs_pos[i]);
		f->store_64(ofs_table[i]);
	}

	f->seek_end();

	f->store_buffer((const uint8_t *)"RSRC", 4); //magic at end

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error ResourceFormatSaverBinaryInstance::set_uid(const String &p_path, ResourceUID::ID p_uid) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, vformat("Cannot open file '%s'.", p_path));

	Ref<FileAccess> fw;

	local_path = p_path.get_base_dir();

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		Error err = fac->open_after_magic(f);
		ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot open file '%s'.", p_path));
		f = fac;

		Ref<FileAccessCompressed> facw;
		facw.instantiate();
		facw->configure("RSCC");
		err = facw->open_internal(p_path + ".uidren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, vformat("Cannot create file '%s.uidren'.", p_path));

		fw = facw;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not a binary resource.
		return ERR_FILE_UNRECOGNIZED;
	} else {
		fw = FileAccess::open(p_path + ".uidren", FileAccess::WRITE);
		ERR_FAIL_COND_V_MSG(fw.is_null(), ERR_CANT_CREATE, vformat("Cannot create file '%s.uidren'.", p_path));

		uint8_t magich[4] = { 'R', 'S', 'R', 'C' };
		fw->store_buffer(magich, 4);
	}

	big_endian = f->get_32();
	bool use_real64 = f->get_32();
	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian
#ifdef BIG_ENDIAN_ENABLED
	fw->store_32(!big_endian);
#else
	fw->store_32(big_endian);
#endif
	fw->set_big_endian(big_endian != 0);
	fw->store_32(use_real64); //use real64

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	if (ver_format < FORMAT_VERSION_CAN_RENAME_DEPS) {
		fw.unref();

		{
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			da->remove(p_path + ".uidren");
		}

		// Use the old approach.

		WARN_PRINT(vformat("This file is old, so it does not support UIDs, opening and resaving '%s'.", p_path));
		return ERR_UNAVAILABLE;
	}

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED,
				vformat("File '%s' can't be loaded, as it uses a format version (%d) or engine version (%d.%d) which are not supported by your engine version (%s).",
						local_path, ver_format, ver_major, ver_minor, VERSION_BRANCH));
	}

	// Since we're not actually converting the file contents, leave the version
	// numbers in the file untouched.
	fw->store_32(ver_major);
	fw->store_32(ver_minor);
	fw->store_32(ver_format);

	save_ustring(fw, get_ustring(f)); //type

	fw->store_64(f->get_64()); //metadata offset

	uint32_t flags = f->get_32();
	flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS;
	f->get_64(); // Skip previous UID

	fw->store_32(flags);
	fw->store_64(uint64_t(p_uid));

	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		save_ustring(fw, get_ustring(f));
	}

	//rest of file
	uint8_t b = f->get_8();
	while (!f->eof_reached()) {
		fw->store_8(b);
		b = f->get_8();
	}

	f.unref();

	bool all_ok = fw->get_error() == OK;

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	fw.unref();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	da->remove(p_path);
	da->rename(p_path + ".uidren", p_path);
	return OK;
}

Error ResourceFormatSaverBinary::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	ResourceFormatSaverBinaryInstance saver;
	return saver.save(local_path, p_resource, p_flags);
}

Error ResourceFormatSaverBinary::set_uid(const String &p_path, ResourceUID::ID p_uid) {
	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	ResourceFormatSaverBinaryInstance saver;
	return saver.set_uid(local_path, p_uid);
}

bool ResourceFormatSaverBinary::recognize(const Ref<Resource> &p_resource) const {
	return true; //all recognized
}

void ResourceFormatSaverBinary::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	String base = p_resource->get_base_extension().to_lower();
	p_extensions->push_back(base);
	if (base != "res") {
		p_extensions->push_back("res");
	}
}

ResourceFormatSaverBinary *ResourceFormatSaverBinary::singleton = nullptr;

ResourceFormatSaverBinary::ResourceFormatSaverBinary() {
	singleton = this;
}
