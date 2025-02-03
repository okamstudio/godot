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

#include "core/image.h"
#include "core/io/file_access_compressed.h"
#include "core/io/marshalls.h"
#include "core/os/dir_access.h"
#include "core/project_settings.h"
#include "core/version.h"

//#define print_bl(m_what) print_line(m_what)
#define print_bl(m_what) (void)(m_what)

enum {

	//numbering must be different from variant, in case new variant types are added (variant must be always contiguous for jumptable optimization)
	VARIANT_NIL = 1,
	VARIANT_BOOL = 2,
	VARIANT_INT = 3,
	VARIANT_REAL = 4,
	VARIANT_STRING = 5,
	VARIANT_VECTOR2 = 10,
	VARIANT_RECT2 = 11,
	VARIANT_VECTOR3 = 12,
	VARIANT_PLANE = 13,
	VARIANT_QUAT = 14,
	VARIANT_AABB = 15,
	VARIANT_MATRIX3 = 16,
	VARIANT_TRANSFORM = 17,
	VARIANT_MATRIX32 = 18,
	VARIANT_COLOR = 20,
	VARIANT_NODE_PATH = 22,
	VARIANT_RID = 23,
	VARIANT_OBJECT = 24,
	VARIANT_INPUT_EVENT = 25,
	VARIANT_DICTIONARY = 26,
	VARIANT_ARRAY = 30,
	VARIANT_RAW_ARRAY = 31,
	VARIANT_INT_ARRAY = 32,
	VARIANT_REAL_ARRAY = 33,
	VARIANT_STRING_ARRAY = 34,
	VARIANT_VECTOR3_ARRAY = 35,
	VARIANT_COLOR_ARRAY = 36,
	VARIANT_VECTOR2_ARRAY = 37,
	VARIANT_INT64 = 40,
	VARIANT_DOUBLE = 41,
#ifndef DISABLE_DEPRECATED
	VARIANT_IMAGE = 21, // - no longer variant type
	IMAGE_ENCODING_EMPTY = 0,
	IMAGE_ENCODING_RAW = 1,
	IMAGE_ENCODING_LOSSLESS = 2,
	IMAGE_ENCODING_LOSSY = 3,
#endif
	OBJECT_EMPTY = 0,
	OBJECT_EXTERNAL_RESOURCE = 1,
	OBJECT_INTERNAL_RESOURCE = 2,
	OBJECT_EXTERNAL_RESOURCE_INDEX = 3,
	//version 2: added 64 bits support for float and int
	//version 3: changed nodepath encoding
	FORMAT_VERSION = 3,
	FORMAT_VERSION_CAN_RENAME_DEPS = 1,
	FORMAT_VERSION_NO_NODEPATH_PROPERTY = 3,

};

void ResourceInteractiveLoaderBinary::_advance_padding(uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

StringName ResourceInteractiveLoaderBinary::_get_string() {
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
		s.parse_utf8(&str_buf[0]);
		return s;
	}

	return string_map[id];
}

Error ResourceInteractiveLoaderBinary::parse_variant(Variant &r_v) {
	uint32_t type = f->get_32();
	print_bl("find property of type: " + itos(type));

	switch (type) {
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
		case VARIANT_REAL: {
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
		case VARIANT_RECT2: {
			Rect2 v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_VECTOR3: {
			Vector3 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
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
		case VARIANT_QUAT: {
			Quat v;
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
		case VARIANT_MATRIX32: {
			Transform2D v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			r_v = v;

		} break;
		case VARIANT_MATRIX3: {
			Basis v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[0].z = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[1].z = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			v.elements[2].z = f->get_real();
			r_v = v;

		} break;
		case VARIANT_TRANSFORM: {
			Transform v;
			v.basis.elements[0].x = f->get_real();
			v.basis.elements[0].y = f->get_real();
			v.basis.elements[0].z = f->get_real();
			v.basis.elements[1].x = f->get_real();
			v.basis.elements[1].y = f->get_real();
			v.basis.elements[1].z = f->get_real();
			v.basis.elements[2].x = f->get_real();
			v.basis.elements[2].y = f->get_real();
			v.basis.elements[2].z = f->get_real();
			v.origin.x = f->get_real();
			v.origin.y = f->get_real();
			v.origin.z = f->get_real();
			r_v = v;
		} break;
		case VARIANT_COLOR: {
			Color v;
			v.r = f->get_real();
			v.g = f->get_real();
			v.b = f->get_real();
			v.a = f->get_real();
			r_v = v;

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
					String path = res_path + "::" + itos(index);
					RES res;
					if (internal_resources_cache.has(index)) {
						res = internal_resources_cache[index];
					} else {
						res = ResourceLoader::load(path, "", no_subresource_cache);
						internal_resources_cache[index] = res;
					}
					if (res.is_null()) {
						WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
					}
					r_v = res;

				} break;
				case OBJECT_EXTERNAL_RESOURCE: {
					//old file format, still around for compatibility

					String exttype = get_unicode_string();
					String path = get_unicode_string();

					if (path.find("://") == -1 && path.is_rel_path()) {
						// path is relative to file being loaded, so convert to a resource path
						path = ProjectSettings::get_singleton()->localize_path(res_path.get_base_dir().plus_file(path));
					}

					if (remaps.find(path)) {
						path = remaps[path];
					}

					RES res = ResourceLoader::load(path, exttype, false);

					if (res.is_null()) {
						WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
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
						String exttype = external_resources[erindex].type;
						String path = external_resources[erindex].path;

						if (path.find("://") == -1 && path.is_rel_path()) {
							// path is relative to file being loaded, so convert to a resource path
							path = ProjectSettings::get_singleton()->localize_path(res_path.get_base_dir().plus_file(path));
						}

						RES res;

						for (List<RES>::Element *E = resource_cache.front(); E; E = E->next()) {
							if (E->get()->get_path() == path) {
								res = E->get();
								break;
							}
						}

						if (res.is_null()) {
							res = ResourceLoader::load(path, exttype, no_subresource_cache);
						}

						if (res.is_null()) {
							WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
						}
						r_v = res;
					}

				} break;
				default: {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				} break;
			}

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
		case VARIANT_RAW_ARRAY: {
			uint32_t len = f->get_32();

			PoolVector<uint8_t> array;
			array.resize(len);
			PoolVector<uint8_t>::Write w = array.write();
			f->get_buffer(w.ptr(), len);
			_advance_padding(len);
			w.release();
			r_v = array;

		} break;
		case VARIANT_INT_ARRAY: {
			uint32_t len = f->get_32();

			PoolVector<int> array;
			array.resize(len);
			PoolVector<int>::Write w = array.write();
			f->get_buffer((uint8_t *)w.ptr(), len * 4);
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif
			w.release();
			r_v = array;
		} break;
		case VARIANT_REAL_ARRAY: {
			uint32_t len = f->get_32();

			PoolVector<real_t> array;
			array.resize(len);
			PoolVector<real_t>::Write w = array.write();
			f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			w.release();
			r_v = array;
		} break;
		case VARIANT_STRING_ARRAY: {
			uint32_t len = f->get_32();
			PoolVector<String> array;
			array.resize(len);
			PoolVector<String>::Write w = array.write();
			for (uint32_t i = 0; i < len; i++) {
				w[i] = get_unicode_string();
			}
			w.release();
			r_v = array;

		} break;
		case VARIANT_VECTOR2_ARRAY: {
			uint32_t len = f->get_32();

			PoolVector<Vector2> array;
			array.resize(len);
			PoolVector<Vector2>::Write w = array.write();
			if (sizeof(Vector2) == 8) {
				f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t) * 2);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 2; i++) {
						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Vector2 size is NOT 8!");
			}
			w.release();
			r_v = array;

		} break;
		case VARIANT_VECTOR3_ARRAY: {
			uint32_t len = f->get_32();

			PoolVector<Vector3> array;
			array.resize(len);
			PoolVector<Vector3>::Write w = array.write();
			if (sizeof(Vector3) == 12) {
				f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t) * 3);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 3; i++) {
						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Vector3 size is NOT 12!");
			}
			w.release();
			r_v = array;

		} break;
		case VARIANT_COLOR_ARRAY: {
			uint32_t len = f->get_32();

			PoolVector<Color> array;
			array.resize(len);
			PoolVector<Color>::Write w = array.write();
			if (sizeof(Color) == 16) {
				f->get_buffer((uint8_t *)w.ptr(), len * sizeof(real_t) * 4);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 4; i++) {
						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Color size is NOT 16!");
			}
			w.release();
			r_v = array;
		} break;
#ifndef DISABLE_DEPRECATED
		case VARIANT_IMAGE: {
			uint32_t encoding = f->get_32();
			if (encoding == IMAGE_ENCODING_EMPTY) {
				r_v = Ref<Image>();
				break;
			} else if (encoding == IMAGE_ENCODING_RAW) {
				uint32_t width = f->get_32();
				uint32_t height = f->get_32();
				uint32_t mipmaps = f->get_32();
				uint32_t format = f->get_32();
				const uint32_t format_version_shift = 24;
				const uint32_t format_version_mask = format_version_shift - 1;

				uint32_t format_version = format >> format_version_shift;

				const uint32_t current_version = 0;
				if (format_version > current_version) {
					ERR_PRINT("Format version for encoded binary image is too new.");
					return ERR_PARSE_ERROR;
				}

				Image::Format fmt = Image::Format(format & format_version_mask); //if format changes, we can add a compatibility bit on top

				uint32_t datalen = f->get_32();

				PoolVector<uint8_t> imgdata;
				imgdata.resize(datalen);
				PoolVector<uint8_t>::Write w = imgdata.write();
				f->get_buffer(w.ptr(), datalen);
				_advance_padding(datalen);
				w.release();

				Ref<Image> image;
				image.instance();
				image->create(width, height, mipmaps, fmt, imgdata);
				r_v = image;

			} else {
				//compressed
				PoolVector<uint8_t> data;
				data.resize(f->get_32());
				PoolVector<uint8_t>::Write w = data.write();
				f->get_buffer(w.ptr(), data.size());
				w.release();

				Ref<Image> image;

				if (encoding == IMAGE_ENCODING_LOSSY && Image::webp_unpacker) {
					image = Image::webp_unpacker(data); // IMAGE_ENCODING_LOSSY always meant WebP
				} else if (encoding == IMAGE_ENCODING_LOSSLESS && Image::png_unpacker) {
					image = Image::png_unpacker(data); // IMAGE_ENCODING_LOSSLESS always meant png
				}
				_advance_padding(data.size());

				r_v = image;
			}

		} break;
#endif
		default: {
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		} break;
	}

	return OK; //never reach anyway
}

void ResourceInteractiveLoaderBinary::set_local_path(const String &p_local_path) {
	res_path = p_local_path;
}

Ref<Resource> ResourceInteractiveLoaderBinary::get_resource() {
	return resource;
}
Error ResourceInteractiveLoaderBinary::poll() {
	if (error != OK) {
		return error;
	}

	int s = stage;

	if (s < external_resources.size()) {
		String path = external_resources[s].path;

		if (remaps.has(path)) {
			path = remaps[path];
		}
		RES res = ResourceLoader::load(path, external_resources[s].type, false);
		if (res.is_null()) {
			if (!ResourceLoader::get_abort_on_missing_resources()) {
				ResourceLoader::notify_dependency_error(local_path, path, external_resources[s].type);
			} else {
				error = ERR_FILE_MISSING_DEPENDENCIES;
				ERR_FAIL_V_MSG(error, "Can't load dependency: " + path + ".");
			}

		} else {
			resource_cache.push_back(res);
		}

		stage++;
		return error;
	}

	s -= external_resources.size();

	if (s >= internal_resources.size()) {
		error = ERR_BUG;
		ERR_FAIL_COND_V(s >= internal_resources.size(), error);
	}

	bool main = s == (internal_resources.size() - 1);

	//maybe it is loaded already
	String path;
	int subindex = 0;

	if (!main) {
		path = internal_resources[s].path;
		if (path.begins_with("local://")) {
			path = path.replace_first("local://", "");
			subindex = path.to_int();
			path = res_path + "::" + path;
		}

		if (!no_subresource_cache && ResourceCache::has(path)) {
			//already loaded, don't do anything
			stage++;
			error = OK;
			return error;
		}
	} else {
		if (!ResourceCache::has(res_path)) {
			path = res_path;
		}
	}

	uint64_t offset = internal_resources[s].offset;

	f->seek(offset);

	String t = get_unicode_string();

	Object *obj = ClassDB::instance(t);
	if (!obj) {
		error = ERR_FILE_CORRUPT;
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, local_path + ":Resource of unrecognized type in file: " + t + ".");
	}

	Resource *r = Object::cast_to<Resource>(obj);
	if (!r) {
		String obj_class = obj->get_class();
		error = ERR_FILE_CORRUPT;
		memdelete(obj); //bye
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, local_path + ":Resource type in resource field not a resource, type is: " + obj_class + ".");
	}

	RES res = RES(r);

	if (!no_subresource_cache) {
		r->set_path(path);
	}
	r->set_subindex(subindex);

	int pc = f->get_32();

	//set properties

	for (int i = 0; i < pc; i++) {
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

		res->set(name, value);
	}
#ifdef TOOLS_ENABLED
	res->set_edited(false);
#endif
	stage++;

	internal_resources_cache[subindex] = res;
	resource_cache.push_back(res);

	if (main) {
		f->close();
		resource = res;
		resource->set_as_translation_remapped(translation_remapped);
		error = ERR_FILE_EOF;

	} else {
		error = OK;
	}

	return OK;
}
int ResourceInteractiveLoaderBinary::get_stage() const {
	return stage;
}
int ResourceInteractiveLoaderBinary::get_stage_count() const {
	return external_resources.size() + internal_resources.size();
}

void ResourceInteractiveLoaderBinary::set_translation_remapped(bool p_remapped) {
	translation_remapped = p_remapped;
}

static void save_ustring(FileAccess *f, const String &p_string) {
	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

static String get_ustring(FileAccess *f) {
	int len = f->get_32();
	Vector<char> str_buf;
	str_buf.resize(len);
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

String ResourceInteractiveLoaderBinary::get_unicode_string() {
	int len = f->get_32();
	if (len > str_buf.size()) {
		str_buf.resize(len);
	}
	if (len == 0) {
		return String();
	}
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

void ResourceInteractiveLoaderBinary::get_dependencies(FileAccess *p_f, List<String> *p_dependencies, bool p_add_types) {
	open(p_f);
	if (error) {
		return;
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String dep = external_resources[i].path;

		if (p_add_types && external_resources[i].type != String()) {
			dep += "::" + external_resources[i].type;
		}

		p_dependencies->push_back(dep);
	}
}

void ResourceInteractiveLoaderBinary::open(FileAccess *p_f) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		error = fac->open_after_magic(f);
		if (error != OK) {
			memdelete(fac);
			f->close();
			ERR_FAIL_MSG("Failed to open binary resource file: " + local_path + ".");
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f->close();
		ERR_FAIL_MSG("Unrecognized binary resource file: " + local_path + ".");
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian

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
		f->close();
		ERR_FAIL_MSG(vformat("File '%s' can't be loaded, as it uses a format version (%d) or engine version (%d.%d) which are not supported by your engine version (%s).",
				local_path, ver_format, ver_major, ver_minor, VERSION_BRANCH));
	}

	type = get_unicode_string();

	print_bl("type: " + type);

	importmd_ofs = f->get_64();
	for (int i = 0; i < 14; i++) {
		f->get_32(); //skip a few reserved fields
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
		f->close();
		ERR_FAIL_MSG("Premature end of file (EOF): " + local_path + ".");
	}
}

String ResourceInteractiveLoaderBinary::recognize(FileAccess *p_f) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		error = fac->open_after_magic(f);
		if (error != OK) {
			memdelete(fac);
			f->close();
			return "";
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f->close();
		return "";
	}

	bool big_endian = f->get_32();
	f->get_32(); // use_real64

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian

	uint32_t ver_major = f->get_32();
	f->get_32(); // ver_minor
	uint32_t ver_format = f->get_32();

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		f->close();
		return "";
	}

	String type = get_unicode_string();

	return type;
}

ResourceInteractiveLoaderBinary::ResourceInteractiveLoaderBinary() :
		translation_remapped(false),
		ver_format(0),
		f(nullptr),
		importmd_ofs(0),
		error(OK),
		stage(0) {
}

ResourceInteractiveLoaderBinary::~ResourceInteractiveLoaderBinary() {
	if (f) {
		memdelete(f);
	}
}

Ref<ResourceInteractiveLoader> ResourceFormatLoaderBinary::load_interactive(const String &p_path, const String &p_original_path, Error *r_error, bool p_no_subresource_cache) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}

	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<ResourceInteractiveLoader>(), "Cannot open file '" + p_path + "'.");

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->set_no_subresource_cache(p_no_subresource_cache);
	String path = p_original_path != "" ? p_original_path : p_path;
	ria->local_path = ProjectSettings::get_singleton()->localize_path(path);
	ria->res_path = ria->local_path;
	//ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	ria->open(f);

	return ria;
}

void ResourceFormatLoaderBinary::get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const {
	if (p_type == "") {
		get_recognized_extensions(p_extensions);
		return;
	}

	List<String> extensions;
	ClassDB::get_extensions_for_type(p_type, &extensions);

	extensions.sort();

	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		String ext = E->get().to_lower();
		p_extensions->push_back(ext);
	}
}
void ResourceFormatLoaderBinary::get_recognized_extensions(List<String> *p_extensions) const {
	List<String> extensions;
	ClassDB::get_resource_base_extensions(&extensions);
	extensions.sort();

	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		String ext = E->get().to_lower();
		p_extensions->push_back(ext);
	}
}

bool ResourceFormatLoaderBinary::handles_type(const String &p_type) const {
	return true; //handles all
}

void ResourceFormatLoaderBinary::get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types) {
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_MSG(!f, "Cannot open file '" + p_path + "'.");

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	ria->res_path = ria->local_path;
	//ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	ria->get_dependencies(f, p_dependencies, p_add_types);
}

Error ResourceFormatLoaderBinary::rename_dependencies(const String &p_path, const Map<String, String> &p_map) {
	//Error error=OK;

	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(!f, ERR_CANT_OPEN, "Cannot open file '" + p_path + "'.");

	FileAccess *fw = nullptr; //=FileAccess::open(p_path+".depren");

	String local_path = p_path.get_base_dir();

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		Error err = fac->open_after_magic(f);
		if (err != OK) {
			memdelete(fac);
			memdelete(f);
			ERR_FAIL_V_MSG(err, "Cannot open file '" + p_path + "'.");
		}
		f = fac;

		FileAccessCompressed *facw = memnew(FileAccessCompressed);
		facw->configure("RSCC");
		err = facw->_open(p_path + ".depren", FileAccess::WRITE);
		if (err) {
			memdelete(fac);
			memdelete(facw);
			ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Cannot create file '" + p_path + ".depren'.");
		}

		fw = facw;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		memdelete(f);
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unrecognized binary resource file '" + local_path + "'.");
	} else {
		fw = FileAccess::open(p_path + ".depren", FileAccess::WRITE);
		if (!fw) {
			memdelete(f);
		}
		ERR_FAIL_COND_V_MSG(!fw, ERR_CANT_CREATE, "Cannot create file '" + p_path + ".depren'.");

		uint8_t magic[4] = { 'R', 'S', 'R', 'C' };
		fw->store_buffer(magic, 4);
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian
#ifdef BIG_ENDIAN_ENABLED
	fw->store_32(!big_endian);
#else
	fw->store_32(big_endian);
#endif
	fw->set_endian_swap(big_endian != 0);
	fw->store_32(use_real64); //use real64

	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_format = f->get_32();

	if (ver_format < FORMAT_VERSION_CAN_RENAME_DEPS) {
		memdelete(f);
		memdelete(fw);
		DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		da->remove(p_path + ".depren");
		memdelete(da);
		//use the old approach

		WARN_PRINT("This file is old, so it can't refactor dependencies, opening and resaving '" + p_path + "'.");

		Error err;
		f = FileAccess::open(p_path, FileAccess::READ, &err);

		ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_CANT_OPEN, "Cannot open file '" + p_path + "'.");

		Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
		ria->local_path = ProjectSettings::get_singleton()->localize_path(p_path);
		ria->res_path = ria->local_path;
		ria->remaps = p_map;
		//ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
		ria->open(f);

		err = ria->poll();

		while (err == OK) {
			err = ria->poll();
		}

		ERR_FAIL_COND_V(err != ERR_FILE_EOF, ERR_FILE_CORRUPT);
		RES res = ria->get_resource();
		ERR_FAIL_COND_V(!res.is_valid(), ERR_FILE_CORRUPT);

		return ResourceFormatSaverBinary::singleton->save(p_path, res);
	}

	if (ver_format > FORMAT_VERSION || ver_major > VERSION_MAJOR) {
		memdelete(f);
		memdelete(fw);
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

	for (int i = 0; i < 14; i++) {
		fw->store_32(0);
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

		bool relative = false;
		if (!path.begins_with("res://")) {
			path = local_path.plus_file(path).simplify_path();
			relative = true;
		}

		if (p_map.has(path)) {
			String np = p_map[path];
			path = np;
		}

		if (relative) {
			//restore relative
			path = local_path.path_to_file(path);
		}

		save_ustring(fw, type);
		save_ustring(fw, path);
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

	bool all_ok = fw->get_error() == OK;

	fw->seek(md_ofs);
	fw->store_64(importmd_ofs + size_diff);

	memdelete(f);
	memdelete(fw);

	if (!all_ok) {
		return ERR_CANT_CREATE;
	}

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	da->remove(p_path);
	da->rename(p_path + ".depren", p_path);
	memdelete(da);
	return OK;
}

String ResourceFormatLoaderBinary::get_resource_type(const String &p_path) const {
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	if (!f) {
		return ""; //could not rwead
	}

	Ref<ResourceInteractiveLoaderBinary> ria = memnew(ResourceInteractiveLoaderBinary);
	ria->local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	ria->res_path = ria->local_path;
	//ria->set_local_path( Globals::get_singleton()->localize_path(p_path) );
	String r = ria->recognize(f);
	return r;
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////

void ResourceFormatSaverBinaryInstance::_pad_buffer(FileAccess *f, int p_bytes) {
	int extra = 4 - (p_bytes % 4);
	if (extra < 4) {
		for (int i = 0; i < extra; i++) {
			f->store_8(0); //pad to 32
		}
	}
}

void ResourceFormatSaverBinaryInstance::_write_variant(const Variant &p_property, const PropertyInfo &p_hint) {
	write_variant(f, p_property, resource_set, external_resources, string_map, p_hint);
}

void ResourceFormatSaverBinaryInstance::write_variant(FileAccess *f, const Variant &p_property, Set<RES> &resource_set, Map<RES, int> &external_resources, Map<StringName, int> &string_map, const PropertyInfo &p_hint) {
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
				f->store_64(val);

			} else {
				f->store_32(VARIANT_INT);
				f->store_32(int32_t(p_property));
			}

		} break;
		case Variant::REAL: {
			double d = p_property;
			float fl = d;
			if (double(fl) != d) {
				f->store_32(VARIANT_DOUBLE);
				f->store_double(d);
			} else {
				f->store_32(VARIANT_REAL);
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
		case Variant::RECT2: {
			f->store_32(VARIANT_RECT2);
			Rect2 val = p_property;
			f->store_real(val.position.x);
			f->store_real(val.position.y);
			f->store_real(val.size.x);
			f->store_real(val.size.y);

		} break;
		case Variant::VECTOR3: {
			f->store_32(VARIANT_VECTOR3);
			Vector3 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);

		} break;
		case Variant::PLANE: {
			f->store_32(VARIANT_PLANE);
			Plane val = p_property;
			f->store_real(val.normal.x);
			f->store_real(val.normal.y);
			f->store_real(val.normal.z);
			f->store_real(val.d);

		} break;
		case Variant::QUAT: {
			f->store_32(VARIANT_QUAT);
			Quat val = p_property;
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
			f->store_32(VARIANT_MATRIX32);
			Transform2D val = p_property;
			f->store_real(val.elements[0].x);
			f->store_real(val.elements[0].y);
			f->store_real(val.elements[1].x);
			f->store_real(val.elements[1].y);
			f->store_real(val.elements[2].x);
			f->store_real(val.elements[2].y);

		} break;
		case Variant::BASIS: {
			f->store_32(VARIANT_MATRIX3);
			Basis val = p_property;
			f->store_real(val.elements[0].x);
			f->store_real(val.elements[0].y);
			f->store_real(val.elements[0].z);
			f->store_real(val.elements[1].x);
			f->store_real(val.elements[1].y);
			f->store_real(val.elements[1].z);
			f->store_real(val.elements[2].x);
			f->store_real(val.elements[2].y);
			f->store_real(val.elements[2].z);

		} break;
		case Variant::TRANSFORM: {
			f->store_32(VARIANT_TRANSFORM);
			Transform val = p_property;
			f->store_real(val.basis.elements[0].x);
			f->store_real(val.basis.elements[0].y);
			f->store_real(val.basis.elements[0].z);
			f->store_real(val.basis.elements[1].x);
			f->store_real(val.basis.elements[1].y);
			f->store_real(val.basis.elements[1].z);
			f->store_real(val.basis.elements[2].x);
			f->store_real(val.basis.elements[2].y);
			f->store_real(val.basis.elements[2].z);
			f->store_real(val.origin.x);
			f->store_real(val.origin.y);
			f->store_real(val.origin.z);

		} break;
		case Variant::COLOR: {
			f->store_32(VARIANT_COLOR);
			Color val = p_property;
			f->store_real(val.r);
			f->store_real(val.g);
			f->store_real(val.b);
			f->store_real(val.a);

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
					f->store_32(string_map[np.get_name(i)]);
				} else {
					save_unicode_string(f, np.get_name(i), true);
				}
			}
			for (int i = 0; i < np.get_subname_count(); i++) {
				if (string_map.has(np.get_subname(i))) {
					f->store_32(string_map[np.get_subname(i)]);
				} else {
					save_unicode_string(f, np.get_subname(i), true);
				}
			}

		} break;
		case Variant::_RID: {
			f->store_32(VARIANT_RID);
			WARN_PRINT("Can't save RIDs.");
			RID val = p_property;
			f->store_32(val.get_id());
		} break;
		case Variant::OBJECT: {
			f->store_32(VARIANT_OBJECT);
			RES res = p_property;
			if (res.is_null()) {
				f->store_32(OBJECT_EMPTY);
				return; // don't save it
			}

			if (res->get_path().length() && res->get_path().find("::") == -1) {
				f->store_32(OBJECT_EXTERNAL_RESOURCE_INDEX);
				f->store_32(external_resources[res]);
			} else {
				if (!resource_set.has(res)) {
					f->store_32(OBJECT_EMPTY);
					ERR_FAIL_MSG("Resource was not pre cached for the resource section, most likely due to circular reference.");
				}

				f->store_32(OBJECT_INTERNAL_RESOURCE);
				f->store_32(res->get_subindex());
				//internal resource
			}

		} break;
		case Variant::DICTIONARY: {
			f->store_32(VARIANT_DICTIONARY);
			Dictionary d = p_property;
			f->store_32(uint32_t(d.size()));

			List<Variant> keys;
			d.get_key_list(&keys);

			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				/*
				if (!_check_type(dict[E->get()]))
					continue;
				*/

				write_variant(f, E->get(), resource_set, external_resources, string_map);
				write_variant(f, d[E->get()], resource_set, external_resources, string_map);
			}

		} break;
		case Variant::ARRAY: {
			f->store_32(VARIANT_ARRAY);
			Array a = p_property;
			f->store_32(uint32_t(a.size()));
			for (int i = 0; i < a.size(); i++) {
				write_variant(f, a[i], resource_set, external_resources, string_map);
			}

		} break;
		case Variant::POOL_BYTE_ARRAY: {
			f->store_32(VARIANT_RAW_ARRAY);
			PoolVector<uint8_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			PoolVector<uint8_t>::Read r = arr.read();
			f->store_buffer(r.ptr(), len);
			_pad_buffer(f, len);

		} break;
		case Variant::POOL_INT_ARRAY: {
			f->store_32(VARIANT_INT_ARRAY);
			PoolVector<int> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			PoolVector<int>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_32(r[i]);
			}

		} break;
		case Variant::POOL_REAL_ARRAY: {
			f->store_32(VARIANT_REAL_ARRAY);
			PoolVector<real_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			PoolVector<real_t>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i]);
			}

		} break;
		case Variant::POOL_STRING_ARRAY: {
			f->store_32(VARIANT_STRING_ARRAY);
			PoolVector<String> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			PoolVector<String>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				save_unicode_string(f, r[i]);
			}

		} break;
		case Variant::POOL_VECTOR3_ARRAY: {
			f->store_32(VARIANT_VECTOR3_ARRAY);
			PoolVector<Vector3> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			PoolVector<Vector3>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
				f->store_real(r[i].z);
			}

		} break;
		case Variant::POOL_VECTOR2_ARRAY: {
			f->store_32(VARIANT_VECTOR2_ARRAY);
			PoolVector<Vector2> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			PoolVector<Vector2>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].x);
				f->store_real(r[i].y);
			}

		} break;
		case Variant::POOL_COLOR_ARRAY: {
			f->store_32(VARIANT_COLOR_ARRAY);
			PoolVector<Color> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			PoolVector<Color>::Read r = arr.read();
			for (int i = 0; i < len; i++) {
				f->store_real(r[i].r);
				f->store_real(r[i].g);
				f->store_real(r[i].b);
				f->store_real(r[i].a);
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
			RES res = p_variant.operator RefPtr();

			if (res.is_null() || external_resources.has(res)) {
				return;
			}

			if (!p_main && (!bundle_resources) && res->get_path().length() && res->get_path().find("::") == -1) {
				if (res->get_path() == path) {
					ERR_PRINT("Circular reference to resource being saved found: '" + local_path + "' will be null next time it's loaded.");
					return;
				}
				int idx = external_resources.size();
				external_resources[res] = idx;
				return;
			}

			if (resource_set.has(res)) {
				return;
			}

			List<PropertyInfo> property_list;

			res->get_property_list(&property_list);

			for (List<PropertyInfo>::Element *E = property_list.front(); E; E = E->next()) {
				if (E->get().usage & PROPERTY_USAGE_STORAGE) {
					Variant value = res->get(E->get().name);
					if (E->get().usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = res;
						npk.property = E->get().name;
						non_persistent_map[npk] = value;

						RES sres = value;
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

			resource_set.insert(res);
			saved_resources.push_back(res);

		} break;

		case Variant::ARRAY: {
			Array varray = p_variant;
			int len = varray.size();
			for (int i = 0; i < len; i++) {
				const Variant &v = varray.get(i);
				_find_resources(v);
			}

		} break;

		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			List<Variant> keys;
			d.get_key_list(&keys);
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				_find_resources(E->get());
				Variant v = d[E->get()];
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

void ResourceFormatSaverBinaryInstance::save_unicode_string(FileAccess *f, const String &p_string, bool p_bit_on_len) {
	CharString utf8 = p_string.utf8();
	if (p_bit_on_len) {
		f->store_32((utf8.length() + 1) | 0x80000000);
	} else {
		f->store_32(utf8.length() + 1);
	}
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
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

Error ResourceFormatSaverBinaryInstance::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {
	Error err;
	if (p_flags & ResourceSaver::FLAG_COMPRESS) {
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		fac->configure("RSCC");
		f = fac;
		err = fac->_open(p_path, FileAccess::WRITE);
		if (err) {
			memdelete(f);
		}

	} else {
		f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot create file '" + p_path + "'.");

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
		f->set_endian_swap(true);
	} else {
		f->store_32(0);
	}

	f->store_32(0); //64 bits file, false for now
	f->store_32(VERSION_MAJOR);
	f->store_32(VERSION_MINOR);
	f->store_32(FORMAT_VERSION);

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		f->close();
		memdelete(f);
		return ERR_CANT_CREATE;
	}

	save_unicode_string(f, p_resource->get_class());
	f->store_64(0); //offset to import metadata
	for (int i = 0; i < 14; i++) {
		f->store_32(0); // reserved
	}

	List<ResourceData> resources;

	{
		for (List<RES>::Element *E = saved_resources.front(); E; E = E->next()) {
			ResourceData &rd = resources.push_back(ResourceData())->get();
			rd.type = E->get()->get_class();

			List<PropertyInfo> property_list;
			E->get()->get_property_list(&property_list);

			for (List<PropertyInfo>::Element *F = property_list.front(); F; F = F->next()) {
				if (skip_editor && F->get().name.begins_with("__editor")) {
					continue;
				}
				if ((F->get().usage & PROPERTY_USAGE_STORAGE)) {
					Property p;
					p.name_idx = get_string_index(F->get().name);

					if (F->get().usage & PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT) {
						NonPersistentKey npk;
						npk.base = E->get();
						npk.property = F->get().name;
						if (non_persistent_map.has(npk)) {
							p.value = non_persistent_map[npk];
						}
					} else {
						p.value = E->get()->get(F->get().name);
					}

					Variant default_value = ClassDB::class_get_default_property_value(E->get()->get_class(), F->get().name);

					if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, p.value, default_value))) {
						continue;
					}

					p.pi = F->get();

					rd.properties.push_back(p);
				}
			}
		}
	}

	f->store_32(strings.size()); //string table size
	for (int i = 0; i < strings.size(); i++) {
		save_unicode_string(f, strings[i]);
	}

	// save external resource table
	f->store_32(external_resources.size()); //amount of external resources
	Vector<RES> save_order;
	save_order.resize(external_resources.size());

	for (Map<RES, int>::Element *E = external_resources.front(); E; E = E->next()) {
		save_order.write[E->get()] = E->key();
	}

	for (int i = 0; i < save_order.size(); i++) {
		save_unicode_string(f, save_order[i]->get_save_class());
		String path = save_order[i]->get_path();
		path = relative_paths ? local_path.path_to_file(path) : path;
		save_unicode_string(f, path);
	}
	// save internal resource table
	f->store_32(saved_resources.size()); //amount of internal resources
	Vector<uint64_t> ofs_pos;
	Set<int> used_indices;

	for (List<RES>::Element *E = saved_resources.front(); E; E = E->next()) {
		RES r = E->get();
		if (r->get_path() == "" || r->get_path().find("::") != -1) {
			if (r->get_subindex() != 0) {
				if (used_indices.has(r->get_subindex())) {
					r->set_subindex(0); //repeated
				} else {
					used_indices.insert(r->get_subindex());
				}
			}
		}
	}

	for (List<RES>::Element *E = saved_resources.front(); E; E = E->next()) {
		RES r = E->get();
		if (r->get_path() == "" || r->get_path().find("::") != -1) {
			if (r->get_subindex() == 0) {
				int new_subindex = 1;
				if (used_indices.size()) {
					new_subindex = used_indices.back()->get() + 1;
				}

				r->set_subindex(new_subindex);
				used_indices.insert(new_subindex);
			}

			save_unicode_string(f, "local://" + itos(r->get_subindex()));
			if (takeover_paths) {
				r->set_path(p_path + "::" + itos(r->get_subindex()), true);
			}
#ifdef TOOLS_ENABLED
			r->set_edited(false);
#endif
		} else {
			save_unicode_string(f, r->get_path()); //actual external
		}
		ofs_pos.push_back(f->get_position());
		f->store_64(0); //offset in 64 bits
	}

	Vector<uint64_t> ofs_table;

	//now actually save the resources
	for (List<ResourceData>::Element *E = resources.front(); E; E = E->next()) {
		ResourceData &rd = E->get();

		ofs_table.push_back(f->get_position());
		save_unicode_string(f, rd.type);
		f->store_32(rd.properties.size());

		for (List<Property>::Element *F = rd.properties.front(); F; F = F->next()) {
			Property &p = F->get();
			f->store_32(p.name_idx);
			_write_variant(p.value, F->get().pi);
		}
	}

	for (int i = 0; i < ofs_table.size(); i++) {
		f->seek(ofs_pos[i]);
		f->store_64(ofs_table[i]);
	}

	f->seek_end();

	f->store_buffer((const uint8_t *)"RSRC", 4); //magic at end

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		f->close();
		memdelete(f);
		return ERR_CANT_CREATE;
	}

	f->close();
	memdelete(f);

	return OK;
}

Error ResourceFormatSaverBinary::save(const String &p_path, const RES &p_resource, uint32_t p_flags) {
	String local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	ResourceFormatSaverBinaryInstance saver;
	return saver.save(local_path, p_resource, p_flags);
}

bool ResourceFormatSaverBinary::recognize(const RES &p_resource) const {
	return true; //all recognized
}

void ResourceFormatSaverBinary::get_recognized_extensions(const RES &p_resource, List<String> *p_extensions) const {
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
