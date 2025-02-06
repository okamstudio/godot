/**************************************************************************/
/*  test_resource.h                                                       */
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

#ifndef TEST_RESOURCE_H
#define TEST_RESOURCE_H

#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "scene/main/node.h"

#include "thirdparty/doctest/doctest.h"

#include "tests/test_macros.h"

#include <functional>

namespace TestResource {

enum TestDuplicateMode {
	TEST_MODE_RESOURCE_DUPLICATE_SHALLOW,
	TEST_MODE_RESOURCE_DUPLICATE_DEEP,
	TEST_MODE_RESOURCE_DUPLICATE_DEEP_WITH_MODE,
	TEST_MODE_RESOURCE_DUPLICATE_FOR_LOCAL_SCENE,
	TEST_MODE_VARIANT_DUPLICATE_SHALLOW,
	TEST_MODE_VARIANT_DUPLICATE_DEEP,
	TEST_MODE_VARIANT_DUPLICATE_DEEP_WITH_MODE,
};

struct DuplicateGuineaPigData : public Object {
	const Variant SENTINEL_1 = "A";
	const Variant SENTINEL_2 = 645;
	const Variant SENTINEL_3 = StringName("X");
	const Variant SENTINEL_4 = true;

	Ref<Resource> SUBRES_1 = memnew(Resource);
	Ref<Resource> SUBRES_2 = memnew(Resource);
	Ref<Resource> SUBRES_3 = memnew(Resource);
	Ref<Resource> SUBRES_SL_1 = memnew(Resource);
	Ref<Resource> SUBRES_SL_2 = memnew(Resource);
	Ref<Resource> SUBRES_SL_3 = memnew(Resource);

	Variant obj; // Variant helps with lifetime so duplicates pointing to the same don't try to double-free it.
	Array arr;
	Dictionary dict;
	Variant packed; // A PackedByteArray, but using Variant to be able to tell if the array is shared or not.
	Ref<Resource> subres;
	Ref<Resource> subres_sl;

	void set_defaults() {
		SUBRES_1->set_name("juan");
		SUBRES_2->set_name("you");
		SUBRES_3->set_name("tree");
		SUBRES_SL_1->set_name("maybe_scene_local");
		SUBRES_SL_2->set_name("perhaps_local_to_scene");
		SUBRES_SL_3->set_name("sometimes_locality_scenial");

		// To try some cases of internal and external.
		SUBRES_1->set_path_cache("");
		SUBRES_2->set_path_cache("local://hehe");
		SUBRES_3->set_path_cache("res://some.tscn::1");
		DEV_ASSERT(SUBRES_1->is_built_in());
		DEV_ASSERT(SUBRES_2->is_built_in());
		DEV_ASSERT(SUBRES_3->is_built_in());
		SUBRES_SL_1->set_path_cache("res://thing.scn");
		SUBRES_SL_2->set_path_cache("C:/not/really/possible/but/still/external");
		SUBRES_SL_3->set_path_cache("/this/neither");
		DEV_ASSERT(!SUBRES_SL_1->is_built_in());
		DEV_ASSERT(!SUBRES_SL_2->is_built_in());
		DEV_ASSERT(!SUBRES_SL_3->is_built_in());

		obj = memnew(Object);

		// Construct enough cases to test deep recursion involving resources;
		// we mix some primitive values with recurses nested in different ways,
		// acting as array values and dictionary keys and values, some of those
		// being marked as scene-local when for subcases where scene-local is relevant.

		arr.push_back(SENTINEL_1);
		arr.push_back(SUBRES_1);
		arr.push_back(SUBRES_SL_1);
		{
			Dictionary d;
			d[SENTINEL_2] = SENTINEL_3;
			d[SENTINEL_4] = SUBRES_2;
			d[SUBRES_3] = SUBRES_SL_2;
			d[SUBRES_SL_3] = SUBRES_1;
			arr.push_back(d);
		}

		dict[SENTINEL_4] = SENTINEL_1;
		dict[SENTINEL_2] = SUBRES_2;
		dict[SUBRES_3] = SUBRES_SL_1;
		dict[SUBRES_SL_2] = SUBRES_1;
		{
			Array a;
			a.push_back(SENTINEL_3);
			a.push_back(SUBRES_2);
			a.push_back(SUBRES_SL_3);
			dict[SENTINEL_4] = a;
		}

		packed = PackedByteArray{ 0xaa, 0xbb, 0xcc };

		subres = SUBRES_1;
		subres_sl = SUBRES_SL_1;
	}

	void verify_empty() const {
		CHECK(obj.get_type() == Variant::NIL);
		CHECK(arr.size() == 0);
		CHECK(dict.size() == 0);
		CHECK(packed.get_type() == Variant::NIL);
		CHECK(subres.is_null());
	}

	void verify_duplication(const DuplicateGuineaPigData *p_orig, uint32_t p_property_usage, TestDuplicateMode p_test_mode, ResourceDeepDuplicateMode p_deep_mode) const {
		if (!(p_property_usage & PROPERTY_USAGE_STORAGE)) {
			verify_empty();
			return;
		}

		// To see if each resource involved is copied once at most,
		// and then the reference to the duplicate reused.
		HashMap<Resource *, Resource *> duplicates;

		auto _verify_resource = [&](const Ref<Resource> &p_dupe_res, const Ref<Resource> &p_orig_res, bool p_is_property = false) {
			bool expect_true_copy = (p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_DEEP && p_orig_res->is_built_in()) ||
					(p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_DEEP_WITH_MODE && p_deep_mode == RESOURCE_DEEP_DUPLICATE_INTERNAL && p_orig_res->is_built_in()) ||
					(p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_DEEP_WITH_MODE && p_deep_mode == RESOURCE_DEEP_DUPLICATE_ALL) ||
					(p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_FOR_LOCAL_SCENE && p_orig_res->is_local_to_scene()) ||
					(p_test_mode == TEST_MODE_VARIANT_DUPLICATE_DEEP_WITH_MODE && p_deep_mode == RESOURCE_DEEP_DUPLICATE_INTERNAL && p_orig_res->is_built_in()) ||
					(p_test_mode == TEST_MODE_VARIANT_DUPLICATE_DEEP_WITH_MODE && p_deep_mode == RESOURCE_DEEP_DUPLICATE_ALL);

			if (expect_true_copy) {
				if (p_deep_mode == RESOURCE_DEEP_DUPLICATE_NONE) {
					expect_true_copy = false;
				} else if (p_deep_mode == RESOURCE_DEEP_DUPLICATE_INTERNAL) {
					expect_true_copy = p_orig_res->is_built_in();
				}
			}

			if (p_is_property) {
				if ((p_property_usage & PROPERTY_USAGE_ALWAYS_DUPLICATE)) {
					expect_true_copy = true;
				} else if ((p_property_usage & PROPERTY_USAGE_NEVER_DUPLICATE)) {
					expect_true_copy = false;
				}
			}

			if (expect_true_copy) {
				CHECK(p_dupe_res != p_orig_res);
				CHECK(p_dupe_res->get_name() == p_orig_res->get_name());
				if (duplicates.has(p_orig_res.ptr())) {
					CHECK(duplicates[p_orig_res.ptr()] == p_dupe_res.ptr());
				} else {
					duplicates[p_orig_res.ptr()] = p_dupe_res.ptr();
				}
			} else {
				CHECK(p_dupe_res == p_orig_res);
			}
		};

		std::function<void(const Variant &p_a, const Variant &p_b)> _verify_deep_copied_variants = [&](const Variant &p_a, const Variant &p_b) {
			CHECK(p_a.get_type() == p_b.get_type());
			const Ref<Resource> &res_a = p_a;
			const Ref<Resource> &res_b = p_b;
			if (res_a.is_valid()) {
				_verify_resource(res_a, res_b);
			} else if (p_a.get_type() == Variant::ARRAY) {
				const Array &arr_a = p_a;
				const Array &arr_b = p_b;
				CHECK(!arr_a.is_same_instance(arr_b));
				CHECK(arr_a.size() == arr_b.size());
				for (int i = 0; i < arr_a.size(); i++) {
					_verify_deep_copied_variants(arr_a[i], arr_b[i]);
				}
			} else if (p_a.get_type() == Variant::DICTIONARY) {
				const Dictionary &dict_a = p_a;
				const Dictionary &dict_b = p_b;
				CHECK(!dict_a.is_same_instance(dict_b));
				CHECK(dict_a.size() == dict_b.size());
				for (int i = 0; i < dict_a.size(); i++) {
					_verify_deep_copied_variants(dict_a.get_key_at_index(i), dict_b.get_key_at_index(i));
					_verify_deep_copied_variants(dict_a.get_value_at_index(i), dict_b.get_value_at_index(i));
				}
			} else {
				CHECK(p_a == p_b);
			}
		};

		CHECK(this != p_orig);

		CHECK((Object *)obj == (Object *)p_orig->obj);

		bool expect_true_copy = p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_DEEP ||
				p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_DEEP_WITH_MODE ||
				p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_FOR_LOCAL_SCENE ||
				p_test_mode == TEST_MODE_VARIANT_DUPLICATE_DEEP ||
				p_test_mode == TEST_MODE_VARIANT_DUPLICATE_DEEP_WITH_MODE;
		if (expect_true_copy) {
			_verify_deep_copied_variants(arr, p_orig->arr);
			_verify_deep_copied_variants(dict, p_orig->dict);
			CHECK(!packed.identity_compare(p_orig->packed));
		} else {
			CHECK(arr.is_same_instance(p_orig->arr));
			CHECK(dict.is_same_instance(p_orig->dict));
			CHECK(packed.identity_compare(p_orig->packed));
		}

		_verify_resource(subres, p_orig->subres, true);
		_verify_resource(subres_sl, p_orig->subres_sl, true);
	}

	void enable_scene_local_subresources() {
		SUBRES_SL_1->set_local_to_scene(true);
		SUBRES_SL_2->set_local_to_scene(true);
		SUBRES_SL_3->set_local_to_scene(true);
	}

	virtual ~DuplicateGuineaPigData() {
		Object *obj_ptr = obj.get_validated_object();
		if (obj_ptr) {
			memdelete(obj_ptr);
		}
	}
};

#define DEFINE_DUPLICATE_GUINEA_PIG(m_class_name, m_property_usage)                                                                                 \
	class m_class_name : public Resource {                                                                                                          \
		GDCLASS(m_class_name, Resource)                                                                                                             \
                                                                                                                                                    \
		DuplicateGuineaPigData data;                                                                                                                \
                                                                                                                                                    \
	public:                                                                                                                                         \
		void set_obj(Object *p_obj) {                                                                                                               \
			data.obj = p_obj;                                                                                                                       \
		}                                                                                                                                           \
		Object *get_obj() const {                                                                                                                   \
			return data.obj;                                                                                                                        \
		}                                                                                                                                           \
                                                                                                                                                    \
		void set_arr(const Array &p_arr) {                                                                                                          \
			data.arr = p_arr;                                                                                                                       \
		}                                                                                                                                           \
		Array get_arr() const {                                                                                                                     \
			return data.arr;                                                                                                                        \
		}                                                                                                                                           \
                                                                                                                                                    \
		void set_dict(const Dictionary &p_dict) {                                                                                                   \
			data.dict = p_dict;                                                                                                                     \
		}                                                                                                                                           \
		Dictionary get_dict() const {                                                                                                               \
			return data.dict;                                                                                                                       \
		}                                                                                                                                           \
                                                                                                                                                    \
		void set_packed(const Variant &p_packed) {                                                                                                  \
			data.packed = p_packed;                                                                                                                 \
		}                                                                                                                                           \
		Variant get_packed() const {                                                                                                                \
			return data.packed;                                                                                                                     \
		}                                                                                                                                           \
                                                                                                                                                    \
		void set_subres(const Ref<Resource> &p_subres) {                                                                                            \
			data.subres = p_subres;                                                                                                                 \
		}                                                                                                                                           \
		Ref<Resource> get_subres() const {                                                                                                          \
			return data.subres;                                                                                                                     \
		}                                                                                                                                           \
                                                                                                                                                    \
		void set_subres_sl(const Ref<Resource> &p_subres) {                                                                                         \
			data.subres_sl = p_subres;                                                                                                              \
		}                                                                                                                                           \
		Ref<Resource> get_subres_sl() const {                                                                                                       \
			return data.subres_sl;                                                                                                                  \
		}                                                                                                                                           \
                                                                                                                                                    \
		void set_defaults() {                                                                                                                       \
			data.set_defaults();                                                                                                                    \
		}                                                                                                                                           \
                                                                                                                                                    \
		Object *get_data() {                                                                                                                        \
			return &data;                                                                                                                           \
		}                                                                                                                                           \
                                                                                                                                                    \
		void verify_duplication(const Ref<Resource> &p_orig, int p_test_mode, int p_deep_mode) const {                                              \
			const DuplicateGuineaPigData *orig_data = Object::cast_to<DuplicateGuineaPigData>(p_orig->call("get_data"));                            \
			data.verify_duplication(orig_data, m_property_usage, (TestDuplicateMode)p_test_mode, (ResourceDeepDuplicateMode)p_deep_mode);           \
		}                                                                                                                                           \
                                                                                                                                                    \
	protected:                                                                                                                                      \
		static void _bind_methods() {                                                                                                               \
			ClassDB::bind_method(D_METHOD("set_obj", "obj"), &m_class_name::set_obj);                                                               \
			ClassDB::bind_method(D_METHOD("get_obj"), &m_class_name::get_obj);                                                                      \
                                                                                                                                                    \
			ClassDB::bind_method(D_METHOD("set_arr", "arr"), &m_class_name::set_arr);                                                               \
			ClassDB::bind_method(D_METHOD("get_arr"), &m_class_name::get_arr);                                                                      \
                                                                                                                                                    \
			ClassDB::bind_method(D_METHOD("set_dict", "dict"), &m_class_name::set_dict);                                                            \
			ClassDB::bind_method(D_METHOD("get_dict"), &m_class_name::get_dict);                                                                    \
                                                                                                                                                    \
			ClassDB::bind_method(D_METHOD("set_packed", "packed"), &m_class_name::set_packed);                                                      \
			ClassDB::bind_method(D_METHOD("get_packed"), &m_class_name::get_packed);                                                                \
                                                                                                                                                    \
			ClassDB::bind_method(D_METHOD("set_subres", "subres"), &m_class_name::set_subres);                                                      \
			ClassDB::bind_method(D_METHOD("get_subres"), &m_class_name::get_subres);                                                                \
                                                                                                                                                    \
			ClassDB::bind_method(D_METHOD("set_subres_sl", "subres"), &m_class_name::set_subres_sl);                                                \
			ClassDB::bind_method(D_METHOD("get_subres_sl"), &m_class_name::get_subres_sl);                                                          \
                                                                                                                                                    \
			ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "obj", PROPERTY_HINT_NONE, "", m_property_usage), "set_obj", "get_obj");                     \
			ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "arr", PROPERTY_HINT_NONE, "", m_property_usage), "set_arr", "get_arr");                      \
			ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "dict", PROPERTY_HINT_NONE, "", m_property_usage), "set_dict", "get_dict");              \
			ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "packed", PROPERTY_HINT_NONE, "", m_property_usage), "set_packed", "get_packed"); \
			ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "subres", PROPERTY_HINT_NONE, "", m_property_usage), "set_subres", "get_subres");            \
			ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "subres_sl", PROPERTY_HINT_NONE, "", m_property_usage), "set_subres_sl", "get_subres_sl");   \
                                                                                                                                                    \
			ClassDB::bind_method(D_METHOD("set_defaults"), &m_class_name::set_defaults);                                                            \
			ClassDB::bind_method(D_METHOD("get_data"), &m_class_name::get_data);                                                                    \
			ClassDB::bind_method(D_METHOD("verify_duplication", "orig", "test_mode", "deep_mode"), &m_class_name::verify_duplication);              \
		}                                                                                                                                           \
                                                                                                                                                    \
	public:                                                                                                                                         \
		static m_class_name *register_and_instantiate() {                                                                                           \
			static bool registered = false;                                                                                                         \
			if (!registered) {                                                                                                                      \
				GDREGISTER_INTERNAL_CLASS(m_class_name);                                                                                            \
				registered = true;                                                                                                                  \
			}                                                                                                                                       \
			return memnew(m_class_name);                                                                                                            \
		}                                                                                                                                           \
	};

DEFINE_DUPLICATE_GUINEA_PIG(DuplicateGuineaPig_None, PROPERTY_USAGE_NONE)
DEFINE_DUPLICATE_GUINEA_PIG(DuplicateGuineaPig_Always, PROPERTY_USAGE_ALWAYS_DUPLICATE)
DEFINE_DUPLICATE_GUINEA_PIG(DuplicateGuineaPig_Storage, PROPERTY_USAGE_STORAGE)
DEFINE_DUPLICATE_GUINEA_PIG(DuplicateGuineaPig_Storage_Always, (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_ALWAYS_DUPLICATE))
DEFINE_DUPLICATE_GUINEA_PIG(DuplicateGuineaPig_Storage_Never, (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_NEVER_DUPLICATE))

TEST_CASE("[Resource] Duplication") {
	auto _run_test = [](
							 TestDuplicateMode p_test_mode,
							 ResourceDeepDuplicateMode p_deep_mode,
							 Ref<Resource> (*p_duplicate_fn)(const Ref<Resource> &)) -> void {
		LocalVector<Ref<Resource>> resources = {
			DuplicateGuineaPig_None::register_and_instantiate(),
			DuplicateGuineaPig_Always::register_and_instantiate(),
			DuplicateGuineaPig_Storage::register_and_instantiate(),
			DuplicateGuineaPig_Storage_Always::register_and_instantiate(),
			DuplicateGuineaPig_Storage_Never::register_and_instantiate(),
		};

		for (const Ref<Resource> &orig : resources) {
			INFO(std::string(String(orig->get_class_name()).utf8().get_data()));

			orig->call("set_defaults");

			const Ref<Resource> &dupe = p_duplicate_fn(orig);
			if ((p_test_mode == TEST_MODE_RESOURCE_DUPLICATE_DEEP_WITH_MODE || p_test_mode == TEST_MODE_VARIANT_DUPLICATE_DEEP_WITH_MODE) && p_deep_mode == RESOURCE_DEEP_DUPLICATE_MAX) {
				CHECK(dupe.is_null());
			} else {
				dupe->call("verify_duplication", orig, p_test_mode, p_deep_mode);
			}
		}
	};

	SUBCASE("Resource::duplicate(), shallow") {
		_run_test(
				TEST_MODE_RESOURCE_DUPLICATE_SHALLOW,
				RESOURCE_DEEP_DUPLICATE_MAX,
				[](const Ref<Resource> &p_res) -> Ref<Resource> {
					return p_res->duplicate(false);
				});
	}

	SUBCASE("Resource::duplicate(), deep") {
		_run_test(
				TEST_MODE_RESOURCE_DUPLICATE_DEEP,
				RESOURCE_DEEP_DUPLICATE_MAX,
				[](const Ref<Resource> &p_res) -> Ref<Resource> {
					return p_res->duplicate(true);
				});
	}

	SUBCASE("Resource::duplicate_deep()") {
		static int deep_mode = 0;
		for (deep_mode = 0; deep_mode <= RESOURCE_DEEP_DUPLICATE_MAX; deep_mode++) {
			_run_test(
					TEST_MODE_RESOURCE_DUPLICATE_DEEP_WITH_MODE,
					(ResourceDeepDuplicateMode)deep_mode,
					[](const Ref<Resource> &p_res) -> Ref<Resource> {
						return p_res->duplicate_deep((ResourceDeepDuplicateMode)deep_mode);
					});
		}
	}

	SUBCASE("Resource::duplicate_for_local_scene()") {
		static int mark_main_as_local = 0;
		static int mark_some_subs_as_local = 0;
		for (mark_main_as_local = 0; mark_main_as_local < 2; ++mark_main_as_local) { // Whether main is local-to-scene shouldn't matter.
			for (mark_some_subs_as_local = 0; mark_some_subs_as_local < 2; ++mark_some_subs_as_local) {
				_run_test(
						TEST_MODE_RESOURCE_DUPLICATE_FOR_LOCAL_SCENE,
						RESOURCE_DEEP_DUPLICATE_MAX,
						[](const Ref<Resource> &p_res) -> Ref<Resource> {
							if (mark_main_as_local) {
								p_res->set_local_to_scene(true);
							}
							if (mark_some_subs_as_local) {
								Object::cast_to<DuplicateGuineaPigData>(p_res->call("get_data"))->enable_scene_local_subresources();
							}
							HashMap<Ref<Resource>, Ref<Resource>> remap_cache;
							Node fake_scene;
							return p_res->duplicate_for_local_scene(&fake_scene, remap_cache);
						});
			}
		}
	}

	SUBCASE("Variant::duplicate(), shallow") {
		_run_test(
				TEST_MODE_VARIANT_DUPLICATE_SHALLOW,
				RESOURCE_DEEP_DUPLICATE_MAX,
				[](const Ref<Resource> &p_res) -> Ref<Resource> {
					return Variant(p_res).duplicate(false);
				});
	}

	SUBCASE("Variant::duplicate(), deep") {
		_run_test(
				TEST_MODE_VARIANT_DUPLICATE_DEEP,
				RESOURCE_DEEP_DUPLICATE_MAX,
				[](const Ref<Resource> &p_res) -> Ref<Resource> {
					return Variant(p_res).duplicate(true);
				});
	}

	SUBCASE("Variant::duplicate_deep()") {
		static int deep_mode = 0;
		for (deep_mode = 0; deep_mode <= RESOURCE_DEEP_DUPLICATE_MAX; deep_mode++) {
			_run_test(
					TEST_MODE_VARIANT_DUPLICATE_DEEP_WITH_MODE,
					(ResourceDeepDuplicateMode)deep_mode,
					[](const Ref<Resource> &p_res) -> Ref<Resource> {
						return Variant(p_res).duplicate_deep((ResourceDeepDuplicateMode)deep_mode);
					});
		}
	}

	SUBCASE("Via Variant, resource not being the root") {
		// Variant controls the deep copy, recursing until resources are found, and then
		// it's Resource who controls the deep copy from it onwards.
		// Therefore, we have to test if Variant is able to track unique duplicates across
		// multiple times Resource takes over.
		// Since the other test cases already prove Resource's mechanism to have at most
		// one duplicate per resource involved, the test for Variant is simple.

		Ref<Resource> res;
		res.instantiate();
		res->set_name("risi");
		Array a;
		a.push_back(res);
		{
			Dictionary d;
			d[res] = res;
			a.push_back(d);
		}

		Array dupe_a;
		Ref<Resource> dupe_res;

		SUBCASE("Variant::duplicate(), shallow") {
			dupe_a = Variant(a).duplicate(false);
			// Ensure it's referencing the original.
			dupe_res = dupe_a[0];
			CHECK(dupe_res == res);
		}
		SUBCASE("Variant::duplicate(), deep") {
			dupe_a = Variant(a).duplicate(true);
			// Ensure it's referencing the original.
			dupe_res = dupe_a[0];
			CHECK(dupe_res == res);
		}
		SUBCASE("Variant::duplicate_deep(), no resources") {
			dupe_a = Variant(a).duplicate_deep(RESOURCE_DEEP_DUPLICATE_NONE);
			// Ensure it's referencing the original.
			dupe_res = dupe_a[0];
			CHECK(dupe_res == res);
		}
		SUBCASE("Variant::duplicate_deep(), with resources") {
			dupe_a = Variant(a).duplicate_deep(RESOURCE_DEEP_DUPLICATE_ALL);
			// Ensure it's a copy.
			dupe_res = dupe_a[0];
			CHECK(dupe_res != res);
			CHECK(dupe_res->get_name() == "risi");

			// Ensure the map is already gone so we get new instances.
			Array dupe_a_2 = Variant(a).duplicate_deep(RESOURCE_DEEP_DUPLICATE_ALL);
			CHECK(dupe_a_2[0] != dupe_a[0]);
		}

		// Ensure all the usages are of the same resource.
		CHECK(((Dictionary)dupe_a[1]).get_key_at_index(0) == dupe_res);
		CHECK(((Dictionary)dupe_a[1]).get_value_at_index(0) == dupe_res);
	}
}

TEST_CASE("[Resource] Saving and loading") {
	Ref<Resource> resource = memnew(Resource);
	resource->set_name("Hello world");
	resource->set_meta("ExampleMetadata", Vector2i(40, 80));
	resource->set_meta("string", "The\nstring\nwith\nunnecessary\nline\n\t\\\nbreaks");
	Ref<Resource> child_resource = memnew(Resource);
	child_resource->set_name("I'm a child resource");
	resource->set_meta("other_resource", child_resource);
	const String save_path_binary = TestUtils::get_temp_path("resource.res");
	const String save_path_text = TestUtils::get_temp_path("resource.tres");
	ResourceSaver::save(resource, save_path_binary);
	ResourceSaver::save(resource, save_path_text);

	const Ref<Resource> &loaded_resource_binary = ResourceLoader::load(save_path_binary);
	CHECK_MESSAGE(
			loaded_resource_binary->get_name() == "Hello world",
			"The loaded resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_binary->get_meta("ExampleMetadata") == Vector2i(40, 80),
			"The loaded resource metadata should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_binary->get_meta("string") == "The\nstring\nwith\nunnecessary\nline\n\t\\\nbreaks",
			"The loaded resource metadata should be equal to the expected value.");
	const Ref<Resource> &loaded_child_resource_binary = loaded_resource_binary->get_meta("other_resource");
	CHECK_MESSAGE(
			loaded_child_resource_binary->get_name() == "I'm a child resource",
			"The loaded child resource name should be equal to the expected value.");

	const Ref<Resource> &loaded_resource_text = ResourceLoader::load(save_path_text);
	CHECK_MESSAGE(
			loaded_resource_text->get_name() == "Hello world",
			"The loaded resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_text->get_meta("ExampleMetadata") == Vector2i(40, 80),
			"The loaded resource metadata should be equal to the expected value.");
	CHECK_MESSAGE(
			loaded_resource_text->get_meta("string") == "The\nstring\nwith\nunnecessary\nline\n\t\\\nbreaks",
			"The loaded resource metadata should be equal to the expected value.");
	const Ref<Resource> &loaded_child_resource_text = loaded_resource_text->get_meta("other_resource");
	CHECK_MESSAGE(
			loaded_child_resource_text->get_name() == "I'm a child resource",
			"The loaded child resource name should be equal to the expected value.");
}

TEST_CASE("[Resource] Breaking circular references on save") {
	Ref<Resource> resource_a = memnew(Resource);
	resource_a->set_name("A");
	Ref<Resource> resource_b = memnew(Resource);
	resource_b->set_name("B");
	Ref<Resource> resource_c = memnew(Resource);
	resource_c->set_name("C");
	resource_a->set_meta("next", resource_b);
	resource_b->set_meta("next", resource_c);
	resource_c->set_meta("next", resource_b);

	const String save_path_binary = TestUtils::get_temp_path("resource.res");
	const String save_path_text = TestUtils::get_temp_path("resource.tres");
	ResourceSaver::save(resource_a, save_path_binary);
	// Suppress expected errors caused by the resources above being uncached.
	ERR_PRINT_OFF;
	ResourceSaver::save(resource_a, save_path_text);

	const Ref<Resource> &loaded_resource_a_binary = ResourceLoader::load(save_path_binary);
	ERR_PRINT_ON;
	CHECK_MESSAGE(
			loaded_resource_a_binary->get_name() == "A",
			"The loaded resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_b_binary = loaded_resource_a_binary->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_b_binary->get_name() == "B",
			"The loaded child resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_c_binary = loaded_resource_b_binary->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_c_binary->get_name() == "C",
			"The loaded child resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			!loaded_resource_c_binary->has_meta("next"),
			"The loaded child resource circular reference should be NULL.");

	const Ref<Resource> &loaded_resource_a_text = ResourceLoader::load(save_path_text);
	CHECK_MESSAGE(
			loaded_resource_a_text->get_name() == "A",
			"The loaded resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_b_text = loaded_resource_a_text->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_b_text->get_name() == "B",
			"The loaded child resource name should be equal to the expected value.");
	const Ref<Resource> &loaded_resource_c_text = loaded_resource_b_text->get_meta("next");
	CHECK_MESSAGE(
			loaded_resource_c_text->get_name() == "C",
			"The loaded child resource name should be equal to the expected value.");
	CHECK_MESSAGE(
			!loaded_resource_c_text->has_meta("next"),
			"The loaded child resource circular reference should be NULL.");

	// Break circular reference to avoid memory leak
	resource_c->remove_meta("next");
}
} // namespace TestResource

#endif // TEST_RESOURCE_H
