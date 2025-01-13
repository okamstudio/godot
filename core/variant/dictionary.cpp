/**************************************************************************/
/*  dictionary.cpp                                                        */
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

#include "dictionary.h"

#include "core/templates/hash_map.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/container_type_validate.h"
#include "core/variant/variant.h"
// required in this order by VariantInternal, do not remove this comment.
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/type_info.h"
#include "core/variant/variant_internal.h"

struct DictionaryPrivate {
	SafeRefCount refcount;
	Variant *read_only = nullptr; // If enabled, a pointer is used to a temporary value that is used to return read-only values.
	HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator> variant_map;
	ContainerTypeValidate typed_key;
	ContainerTypeValidate typed_value;
	Variant *typed_fallback = nullptr; // Allows a typed dictionary to return dummy values when attempting an invalid access.
};

void Dictionary::get_key_list(List<Variant> *p_keys) const {
	if (_p->variant_map.is_empty()) {
		return;
	}

	for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
		p_keys->push_back(E.key);
	}
}

Variant Dictionary::get_key_at_index(int p_index) const {
	int index = 0;
	for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
		if (index == p_index) {
			return E.key;
		}
		index++;
	}

	return Variant();
}

Variant Dictionary::get_value_at_index(int p_index) const {
	int index = 0;
	for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
		if (index == p_index) {
			return E.value;
		}
		index++;
	}

	return Variant();
}

// WARNING: This operator does not validate the value type. For scripting/extensions this is
// done in `variant_setget.cpp`. Consider using `set()` if the data might be invalid.
Variant &Dictionary::operator[](const Variant &p_key) {
	Variant key = p_key;
	if (unlikely(!_p->typed_key.validate(key, "use `operator[]`"))) {
		if (unlikely(!_p->typed_fallback)) {
			_p->typed_fallback = memnew(Variant);
		}
		VariantInternal::initialize(_p->typed_fallback, _p->typed_value.type);
		return *_p->typed_fallback;
	} else if (unlikely(_p->read_only)) {
		if (likely(_p->variant_map.has(key))) {
			*_p->read_only = _p->variant_map[key];
		} else {
			VariantInternal::initialize(_p->read_only, _p->typed_value.type);
		}
		return *_p->read_only;
	} else {
		if (unlikely(!_p->variant_map.has(key))) {
			VariantInternal::initialize(&_p->variant_map[key], _p->typed_value.type);
		}
		return _p->variant_map[key];
	}
}

const Variant &Dictionary::operator[](const Variant &p_key) const {
	Variant key = p_key;
	if (unlikely(!_p->typed_key.validate(key, "use `operator[]`"))) {
		if (unlikely(!_p->typed_fallback)) {
			_p->typed_fallback = memnew(Variant);
		}
		VariantInternal::initialize(_p->typed_fallback, _p->typed_value.type);
		return *_p->typed_fallback;
	} else {
		// Will not insert key, so no initialization is necessary.
		return _p->variant_map[key];
	}
}

const Variant *Dictionary::getptr(const Variant &p_key) const {
	Variant key = p_key;
	if (unlikely(!_p->typed_key.validate(key, "getptr"))) {
		return nullptr;
	}
	HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator>::ConstIterator E(_p->variant_map.find(key));
	if (!E) {
		return nullptr;
	}
	return &E->value;
}

// WARNING: This method does not validate the value type.
Variant *Dictionary::getptr(const Variant &p_key) {
	Variant key = p_key;
	if (unlikely(!_p->typed_key.validate(key, "getptr"))) {
		return nullptr;
	}
	HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator>::Iterator E(_p->variant_map.find(key));
	if (!E) {
		return nullptr;
	}
	if (unlikely(_p->read_only != nullptr)) {
		*_p->read_only = E->value;
		return _p->read_only;
	} else {
		return &E->value;
	}
}

Variant Dictionary::get_valid(const Variant &p_key) const {
	Variant key = p_key;
	ERR_FAIL_COND_V(!_p->typed_key.validate(key, "get_valid"), Variant());
	HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator>::ConstIterator E(_p->variant_map.find(key));

	if (!E) {
		return Variant();
	}
	return E->value;
}

Variant Dictionary::get(const Variant &p_key, const Variant &p_default) const {
	Variant key = p_key;
	ERR_FAIL_COND_V(!_p->typed_key.validate(key, "get"), p_default);
	const Variant *result = getptr(key);
	if (!result) {
		return p_default;
	}

	return *result;
}

Variant Dictionary::get_or_add(const Variant &p_key, const Variant &p_default) {
	Variant key = p_key;
	ERR_FAIL_COND_V(!_p->typed_key.validate(key, "get"), p_default);
	const Variant *result = getptr(key);
	if (!result) {
		Variant value = p_default;
		ERR_FAIL_COND_V(!_p->typed_value.validate(value, "add"), value);
		operator[](key) = value;
		return value;
	}
	return *result;
}

bool Dictionary::set(const Variant &p_key, const Variant &p_value) {
	ERR_FAIL_COND_V_MSG(_p->read_only, false, "Dictionary is in read-only state.");
	Variant key = p_key;
	ERR_FAIL_COND_V(!_p->typed_key.validate(key, "set"), false);
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed_value.validate(value, "set"), false);
	_p->variant_map[key] = value;
	return true;
}

int Dictionary::size() const {
	return _p->variant_map.size();
}

bool Dictionary::is_empty() const {
	return !_p->variant_map.size();
}

bool Dictionary::has(const Variant &p_key) const {
	Variant key = p_key;
	ERR_FAIL_COND_V(!_p->typed_key.validate(key, "use 'has'"), false);
	return _p->variant_map.has(p_key);
}

bool Dictionary::has_all(const Array &p_keys) const {
	for (int i = 0; i < p_keys.size(); i++) {
		Variant key = p_keys[i];
		ERR_FAIL_COND_V(!_p->typed_key.validate(key, "use 'has_all'"), false);
		if (!has(key)) {
			return false;
		}
	}
	return true;
}

Variant Dictionary::find_key(const Variant &p_value) const {
	Variant value = p_value;
	ERR_FAIL_COND_V(!_p->typed_value.validate(value, "find_key"), Variant());
	for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
		if (E.value == value) {
			return E.key;
		}
	}
	return Variant();
}

bool Dictionary::erase(const Variant &p_key) {
	Variant key = p_key;
	ERR_FAIL_COND_V(!_p->typed_key.validate(key, "erase"), false);
	ERR_FAIL_COND_V_MSG(_p->read_only, false, "Dictionary is in read-only state.");
	return _p->variant_map.erase(key);
}

bool Dictionary::operator==(const Dictionary &p_dictionary) const {
	return recursive_equal(p_dictionary, 0);
}

bool Dictionary::operator!=(const Dictionary &p_dictionary) const {
	return !recursive_equal(p_dictionary, 0);
}

bool Dictionary::recursive_equal(const Dictionary &p_dictionary, int recursion_count) const {
	// Cheap checks
	if (_p == p_dictionary._p) {
		return true;
	}
	if (_p->variant_map.size() != p_dictionary._p->variant_map.size()) {
		return false;
	}

	// Heavy O(n) check
	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return true;
	}
	recursion_count++;
	for (const KeyValue<Variant, Variant> &this_E : _p->variant_map) {
		HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator>::ConstIterator other_E(p_dictionary._p->variant_map.find(this_E.key));
		if (!other_E || !this_E.value.hash_compare(other_E->value, recursion_count, false)) {
			return false;
		}
	}
	return true;
}

void Dictionary::_ref(const Dictionary &p_from) const {
	//make a copy first (thread safe)
	if (!p_from._p->refcount.ref()) {
		return; // couldn't copy
	}

	//if this is the same, unreference the other one
	if (p_from._p == _p) {
		_p->refcount.unref();
		return;
	}
	if (_p) {
		_unref();
	}
	_p = p_from._p;
}

void Dictionary::clear() {
	ERR_FAIL_COND_MSG(_p->read_only, "Dictionary is in read-only state.");
	_p->variant_map.clear();
}

void Dictionary::sort() {
	ERR_FAIL_COND_MSG(_p->read_only, "Dictionary is in read-only state.");
	_p->variant_map.sort();
}

void Dictionary::merge(const Dictionary &p_dictionary, bool p_overwrite) {
	ERR_FAIL_COND_MSG(_p->read_only, "Dictionary is in read-only state.");
	for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
		Variant key = E.key;
		Variant value = E.value;
		ERR_FAIL_COND(!_p->typed_key.validate(key, "merge"));
		ERR_FAIL_COND(!_p->typed_value.validate(value, "merge"));
		if (p_overwrite || !has(key)) {
			operator[](key) = value;
		}
	}
}

Dictionary Dictionary::merged(const Dictionary &p_dictionary, bool p_overwrite) const {
	Dictionary ret = duplicate();
	ret.merge(p_dictionary, p_overwrite);
	return ret;
}

void Dictionary::_unref() const {
	ERR_FAIL_NULL(_p);
	if (_p->refcount.unref()) {
		if (_p->read_only) {
			memdelete(_p->read_only);
		}
		if (_p->typed_fallback) {
			memdelete(_p->typed_fallback);
		}
		memdelete(_p);
	}
	_p = nullptr;
}

uint32_t Dictionary::hash() const {
	return recursive_hash(0);
}

uint32_t Dictionary::recursive_hash(int recursion_count) const {
	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return 0;
	}

	uint32_t h = hash_murmur3_one_32(Variant::DICTIONARY);

	recursion_count++;
	for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
		h = hash_murmur3_one_32(E.key.recursive_hash(recursion_count), h);
		h = hash_murmur3_one_32(E.value.recursive_hash(recursion_count), h);
	}

	return hash_fmix32(h);
}

Array Dictionary::keys() const {
	Array varr;
	if (is_typed_key()) {
		varr.set_typed(get_typed_key_builtin(), get_typed_key_class_name(), get_typed_key_script());
	}
	if (_p->variant_map.is_empty()) {
		return varr;
	}

	varr.resize(size());

	int i = 0;
	for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
		varr[i] = E.key;
		i++;
	}

	return varr;
}

Array Dictionary::values() const {
	Array varr;
	if (is_typed_value()) {
		varr.set_typed(get_typed_value_builtin(), get_typed_value_class_name(), get_typed_value_script());
	}
	if (_p->variant_map.is_empty()) {
		return varr;
	}

	varr.resize(size());

	int i = 0;
	for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
		varr[i] = E.value;
		i++;
	}

	return varr;
}

void Dictionary::assign(const Dictionary &p_dictionary) {
	const ContainerTypeValidate &typed_key = _p->typed_key;
	const ContainerTypeValidate &typed_key_source = p_dictionary._p->typed_key;

	const ContainerTypeValidate &typed_value = _p->typed_value;
	const ContainerTypeValidate &typed_value_source = p_dictionary._p->typed_value;

	if ((typed_key == typed_key_source || typed_key.type == Variant::NIL || (typed_key_source.type == Variant::OBJECT && typed_key.can_reference(typed_key_source))) &&
			(typed_value == typed_value_source || typed_value.type == Variant::NIL || (typed_value_source.type == Variant::OBJECT && typed_value.can_reference(typed_value_source)))) {
		// From same to same or,
		// from anything to variants or,
		// from subclasses to base classes.
		_p->variant_map = p_dictionary._p->variant_map;
		return;
	}

	int size = p_dictionary._p->variant_map.size();
	HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator> variant_map = HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator>(size);

	Vector<Variant> key_array;
	key_array.resize(size);
	Variant *key_data = key_array.ptrw();

	Vector<Variant> value_array;
	value_array.resize(size);
	Variant *value_data = value_array.ptrw();

	if (typed_key == typed_key_source || typed_key.type == Variant::NIL || (typed_key_source.type == Variant::OBJECT && typed_key.can_reference(typed_key_source))) {
		// From same to same or,
		// from anything to variants or,
		// from subclasses to base classes.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *key = &E.key;
			key_data[i++] = *key;
		}
	} else if ((typed_key_source.type == Variant::NIL && typed_key.type == Variant::OBJECT) || (typed_key_source.type == Variant::OBJECT && typed_key_source.can_reference(typed_key))) {
		// From variants to objects or,
		// from base classes to subclasses.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *key = &E.key;
			if (key->get_type() != Variant::NIL && (key->get_type() != Variant::OBJECT || !typed_key.validate_object(*key, "assign"))) {
				ERR_FAIL_MSG(vformat(R"(Unable to convert key from "%s" to "%s".)", Variant::get_type_name(key->get_type()), Variant::get_type_name(typed_key.type)));
			}
			key_data[i++] = *key;
		}
	} else if (typed_key.type == Variant::OBJECT || typed_key_source.type == Variant::OBJECT) {
		ERR_FAIL_MSG(vformat(R"(Cannot assign contents of "Dictionary[%s, %s]" to "Dictionary[%s, %s]".)", Variant::get_type_name(typed_key_source.type), Variant::get_type_name(typed_value_source.type),
				Variant::get_type_name(typed_key.type), Variant::get_type_name(typed_value.type)));
	} else if (typed_key_source.type == Variant::NIL && typed_key.type != Variant::OBJECT) {
		// From variants to primitives.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *key = &E.key;
			if (key->get_type() == typed_key.type) {
				key_data[i++] = *key;
				continue;
			}
			if (!Variant::can_convert_strict(key->get_type(), typed_key.type)) {
				ERR_FAIL_MSG(vformat(R"(Unable to convert key from "%s" to "%s".)", Variant::get_type_name(key->get_type()), Variant::get_type_name(typed_key.type)));
			}
			Callable::CallError ce;
			Variant::construct(typed_key.type, key_data[i++], &key, 1, ce);
			ERR_FAIL_COND_MSG(ce.error, vformat(R"(Unable to convert key from "%s" to "%s".)", Variant::get_type_name(key->get_type()), Variant::get_type_name(typed_key.type)));
		}
	} else if (Variant::can_convert_strict(typed_key_source.type, typed_key.type)) {
		// From primitives to different convertible primitives.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *key = &E.key;
			Callable::CallError ce;
			Variant::construct(typed_key.type, key_data[i++], &key, 1, ce);
			ERR_FAIL_COND_MSG(ce.error, vformat(R"(Unable to convert key from "%s" to "%s".)", Variant::get_type_name(key->get_type()), Variant::get_type_name(typed_key.type)));
		}
	} else {
		ERR_FAIL_MSG(vformat(R"(Cannot assign contents of "Dictionary[%s, %s]" to "Dictionary[%s, %s].)", Variant::get_type_name(typed_key_source.type), Variant::get_type_name(typed_value_source.type),
				Variant::get_type_name(typed_key.type), Variant::get_type_name(typed_value.type)));
	}

	if (typed_value == typed_value_source || typed_value.type == Variant::NIL || (typed_value_source.type == Variant::OBJECT && typed_value.can_reference(typed_value_source))) {
		// From same to same or,
		// from anything to variants or,
		// from subclasses to base classes.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *value = &E.value;
			value_data[i++] = *value;
		}
	} else if (((typed_value_source.type == Variant::NIL && typed_value.type == Variant::OBJECT) || (typed_value_source.type == Variant::OBJECT && typed_value_source.can_reference(typed_value)))) {
		// From variants to objects or,
		// from base classes to subclasses.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *value = &E.value;
			if (value->get_type() != Variant::NIL && (value->get_type() != Variant::OBJECT || !typed_value.validate_object(*value, "assign"))) {
				ERR_FAIL_MSG(vformat(R"(Unable to convert value at key "%s" from "%s" to "%s".)", key_data[i], Variant::get_type_name(value->get_type()), Variant::get_type_name(typed_value.type)));
			}
			value_data[i++] = *value;
		}
	} else if (typed_value.type == Variant::OBJECT || typed_value_source.type == Variant::OBJECT) {
		ERR_FAIL_MSG(vformat(R"(Cannot assign contents of "Dictionary[%s, %s]" to "Dictionary[%s, %s]".)", Variant::get_type_name(typed_key_source.type), Variant::get_type_name(typed_value_source.type),
				Variant::get_type_name(typed_key.type), Variant::get_type_name(typed_value.type)));
	} else if (typed_value_source.type == Variant::NIL && typed_value.type != Variant::OBJECT) {
		// From variants to primitives.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *value = &E.value;
			if (value->get_type() == typed_value.type) {
				value_data[i++] = *value;
				continue;
			}
			if (!Variant::can_convert_strict(value->get_type(), typed_value.type)) {
				ERR_FAIL_MSG(vformat(R"(Unable to convert value at key "%s" from "%s" to "%s".)", key_data[i], Variant::get_type_name(value->get_type()), Variant::get_type_name(typed_value.type)));
			}
			Callable::CallError ce;
			Variant::construct(typed_value.type, value_data[i++], &value, 1, ce);
			ERR_FAIL_COND_MSG(ce.error, vformat(R"(Unable to convert value at key "%s" from "%s" to "%s".)", key_data[i - 1], Variant::get_type_name(value->get_type()), Variant::get_type_name(typed_value.type)));
		}
	} else if (Variant::can_convert_strict(typed_value_source.type, typed_value.type)) {
		// From primitives to different convertible primitives.
		int i = 0;
		for (const KeyValue<Variant, Variant> &E : p_dictionary._p->variant_map) {
			const Variant *value = &E.value;
			Callable::CallError ce;
			Variant::construct(typed_value.type, value_data[i++], &value, 1, ce);
			ERR_FAIL_COND_MSG(ce.error, vformat(R"(Unable to convert value at key "%s" from "%s" to "%s".)", key_data[i - 1], Variant::get_type_name(value->get_type()), Variant::get_type_name(typed_value.type)));
		}
	} else {
		ERR_FAIL_MSG(vformat(R"(Cannot assign contents of "Dictionary[%s, %s]" to "Dictionary[%s, %s].)", Variant::get_type_name(typed_key_source.type), Variant::get_type_name(typed_value_source.type),
				Variant::get_type_name(typed_key.type), Variant::get_type_name(typed_value.type)));
	}

	for (int i = 0; i < size; i++) {
		variant_map.insert(key_data[i], value_data[i]);
	}

	_p->variant_map = variant_map;
}

const Variant *Dictionary::next(const Variant *p_key) const {
	if (p_key == nullptr) {
		// caller wants to get the first element
		if (_p->variant_map.begin()) {
			return &_p->variant_map.begin()->key;
		}
		return nullptr;
	}
	Variant key = *p_key;
	ERR_FAIL_COND_V(!_p->typed_key.validate(key, "next"), nullptr);
	HashMap<Variant, Variant, VariantHasher, StringLikeVariantComparator>::Iterator E = _p->variant_map.find(key);

	if (!E) {
		return nullptr;
	}

	++E;

	if (E) {
		return &E->key;
	}

	return nullptr;
}

Dictionary Dictionary::duplicate(bool p_deep) const {
	return recursive_duplicate(p_deep, 0);
}

void Dictionary::make_read_only() {
	if (_p->read_only == nullptr) {
		_p->read_only = memnew(Variant);
	}
}
bool Dictionary::is_read_only() const {
	return _p->read_only != nullptr;
}

Dictionary Dictionary::recursive_duplicate(bool p_deep, int recursion_count) const {
	Dictionary n;
	n._p->typed_key = _p->typed_key;
	n._p->typed_value = _p->typed_value;

	if (recursion_count > MAX_RECURSION) {
		ERR_PRINT("Max recursion reached");
		return n;
	}

	if (p_deep) {
		recursion_count++;
		for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
			n[E.key.recursive_duplicate(true, recursion_count)] = E.value.recursive_duplicate(true, recursion_count);
		}
	} else {
		for (const KeyValue<Variant, Variant> &E : _p->variant_map) {
			n[E.key] = E.value;
		}
	}

	return n;
}

void Dictionary::set_typed(const ContainerType &p_key_type, const ContainerType &p_value_type) {
	set_typed(p_key_type.builtin_type, p_key_type.class_name, p_key_type.script, p_value_type.builtin_type, p_value_type.class_name, p_key_type.script);
}

void Dictionary::set_typed(uint32_t p_key_type, const StringName &p_key_class_name, const Variant &p_key_script, uint32_t p_value_type, const StringName &p_value_class_name, const Variant &p_value_script) {
	ERR_FAIL_COND_MSG(_p->read_only, "Dictionary is in read-only state.");
	ERR_FAIL_COND_MSG(_p->variant_map.size() > 0, "Type can only be set when dictionary is empty.");
	ERR_FAIL_COND_MSG(_p->refcount.get() > 1, "Type can only be set when dictionary has no more than one user.");
	ERR_FAIL_COND_MSG(_p->typed_key.type != Variant::NIL || _p->typed_value.type != Variant::NIL, "Type can only be set once.");
	ERR_FAIL_COND_MSG((p_key_class_name != StringName() && p_key_type != Variant::OBJECT) || (p_value_class_name != StringName() && p_value_type != Variant::OBJECT), "Class names can only be set for type OBJECT.");
	Ref<Script> key_script = p_key_script;
	ERR_FAIL_COND_MSG(key_script.is_valid() && p_key_class_name == StringName(), "Script class can only be set together with base class name.");
	Ref<Script> value_script = p_value_script;
	ERR_FAIL_COND_MSG(value_script.is_valid() && p_value_class_name == StringName(), "Script class can only be set together with base class name.");

	_p->typed_key.type = Variant::Type(p_key_type);
	_p->typed_key.class_name = p_key_class_name;
	_p->typed_key.script = key_script;
	_p->typed_key.where = "TypedDictionary.Key";

	_p->typed_value.type = Variant::Type(p_value_type);
	_p->typed_value.class_name = p_value_class_name;
	_p->typed_value.script = value_script;
	_p->typed_value.where = "TypedDictionary.Value";
}

bool Dictionary::is_typed() const {
	return is_typed_key() || is_typed_value();
}

bool Dictionary::is_typed_key() const {
	return _p->typed_key.type != Variant::NIL;
}

bool Dictionary::is_typed_value() const {
	return _p->typed_value.type != Variant::NIL;
}

bool Dictionary::is_same_typed(const Dictionary &p_other) const {
	return is_same_typed_key(p_other) && is_same_typed_value(p_other);
}

bool Dictionary::is_same_typed_key(const Dictionary &p_other) const {
	return _p->typed_key == p_other._p->typed_key;
}

bool Dictionary::is_same_typed_value(const Dictionary &p_other) const {
	return _p->typed_value == p_other._p->typed_value;
}

ContainerType Dictionary::get_key_type() const {
	ContainerType type;
	type.builtin_type = _p->typed_key.type;
	type.class_name = _p->typed_key.class_name;
	type.script = _p->typed_key.script;
	return type;
}

ContainerType Dictionary::get_value_type() const {
	ContainerType type;
	type.builtin_type = _p->typed_value.type;
	type.class_name = _p->typed_value.class_name;
	type.script = _p->typed_value.script;
	return type;
}

uint32_t Dictionary::get_typed_key_builtin() const {
	return _p->typed_key.type;
}

uint32_t Dictionary::get_typed_value_builtin() const {
	return _p->typed_value.type;
}

StringName Dictionary::get_typed_key_class_name() const {
	return _p->typed_key.class_name;
}

StringName Dictionary::get_typed_value_class_name() const {
	return _p->typed_value.class_name;
}

Variant Dictionary::get_typed_key_script() const {
	return _p->typed_key.script;
}

Variant Dictionary::get_typed_value_script() const {
	return _p->typed_value.script;
}

void Dictionary::operator=(const Dictionary &p_dictionary) {
	if (this == &p_dictionary) {
		return;
	}
	_ref(p_dictionary);
}

const void *Dictionary::id() const {
	return _p;
}

Dictionary::Dictionary(const Dictionary &p_base, uint32_t p_key_type, const StringName &p_key_class_name, const Variant &p_key_script, uint32_t p_value_type, const StringName &p_value_class_name, const Variant &p_value_script) {
	_p = memnew(DictionaryPrivate);
	_p->refcount.init();
	set_typed(p_key_type, p_key_class_name, p_key_script, p_value_type, p_value_class_name, p_value_script);
	assign(p_base);
}

Dictionary::Dictionary(const Dictionary &p_from) {
	_p = nullptr;
	_ref(p_from);
}

Dictionary::Dictionary() {
	_p = memnew(DictionaryPrivate);
	_p->refcount.init();
}

Dictionary::Dictionary(std::initializer_list<KeyValue<Variant, Variant>> p_init) {
	_p = memnew(DictionaryPrivate);
	_p->refcount.init();

	for (const KeyValue<Variant, Variant> &E : p_init) {
		operator[](E.key) = E.value;
	}
}

Dictionary::~Dictionary() {
	_unref();
}
