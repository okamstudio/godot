/**************************************************************************/
/*  api.cpp                                                               */
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

#include "api.h"

#include "java_class_wrapper.h"
#include "jni_singleton.h"

#include "core/config/engine.h"

#if !defined(ANDROID_ENABLED)
static JavaClassWrapper *java_class_wrapper = nullptr;
#endif

void register_android_api() {
#if !defined(ANDROID_ENABLED)
	// On Android platforms, the `java_class_wrapper` instantiation occurs in
	// `platform/android/java_godot_lib_jni.cpp#Java_org_godotengine_godot_GodotLib_setup`
	java_class_wrapper = memnew(JavaClassWrapper);
#endif
	GDREGISTER_CLASS(JNISingleton);
	GDREGISTER_CLASS(JavaClass);
	GDREGISTER_CLASS(JavaObject);
	GDREGISTER_CLASS(JavaClassWrapper);
	Engine::get_singleton()->add_singleton(Engine::Singleton("JavaClassWrapper", JavaClassWrapper::get_singleton()));
}

void unregister_android_api() {
#if !defined(ANDROID_ENABLED)
	memdelete(java_class_wrapper);
#endif
}

void JavaClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_java_class_name"), &JavaClass::get_java_class_name);
	ClassDB::bind_method(D_METHOD("get_java_method_list"), &JavaClass::get_java_method_list);
	ClassDB::bind_method(D_METHOD("get_java_parent_class"), &JavaClass::get_java_parent_class);
}

void JavaObject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_java_class"), &JavaObject::get_java_class);
}

void JavaClassWrapper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("wrap", "name"), &JavaClassWrapper::wrap);
}

#if !defined(ANDROID_ENABLED)

Variant JavaClass::callp(const StringName &, const Variant **, int, Callable::CallError &) {
	return Variant();
}

String JavaClass::get_java_class_name() const {
	return "";
}

TypedArray<Dictionary> JavaClass::get_java_method_list() const {
	return TypedArray<Dictionary>();
}

Ref<JavaClass> JavaClass::get_java_parent_class() const {
	return Ref<JavaClass>();
}

JavaClass::JavaClass() {
}

JavaClass::~JavaClass() {
}

Variant JavaObject::callp(const StringName &, const Variant **, int, Callable::CallError &) {
	return Variant();
}

Ref<JavaClass> JavaObject::get_java_class() const {
	return Ref<JavaClass>();
}

JavaClassWrapper *JavaClassWrapper::singleton = nullptr;

Ref<JavaClass> JavaClassWrapper::_wrap(const String &, bool) {
	return Ref<JavaClass>();
}

JavaClassWrapper::JavaClassWrapper() {
	singleton = this;
}

#endif
