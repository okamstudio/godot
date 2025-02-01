/**************************************************************************/
/*  jni_singleton.h                                                       */
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

#include "java_class_wrapper.h"

#include "core/config/engine.h"
#include "core/variant/variant.h"

class JNISingleton : public Object {
	GDCLASS(JNISingleton, Object);

	struct MethodData {
		Variant::Type ret_type;
		Vector<Variant::Type> argtypes;
	};

	RBMap<StringName, MethodData> method_map;
	Ref<JavaObject> wrapped_object;

public:
	virtual Variant callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) override {
		if (wrapped_object.is_valid()) {
			RBMap<StringName, MethodData>::Element *E = method_map.find(p_method);

			// Check the method we're looking for is in the JNISingleton map and that
			// the arguments match.
			bool call_error = !E || E->get().argtypes.size() != p_argcount;
			if (!call_error) {
				for (int i = 0; i < p_argcount; i++) {
					if (!Variant::can_convert(p_args[i]->get_type(), E->get().argtypes[i])) {
						call_error = true;
						break;
					}
				}
			}

			if (!call_error) {
				return wrapped_object->callp(p_method, p_args, p_argcount, r_error);
			}
		}

		return Object::callp(p_method, p_args, p_argcount, r_error);
	}

	Ref<JavaObject> get_wrapped_object() const {
		return wrapped_object;
	}

	void add_method(const StringName &p_name, const Vector<Variant::Type> &p_args, Variant::Type p_ret_type) {
		MethodData md;
		md.argtypes = p_args;
		md.ret_type = p_ret_type;
		method_map[p_name] = md;
	}

	void add_signal(const StringName &p_name, const Vector<Variant::Type> &p_args) {
		MethodInfo mi;
		mi.name = p_name;
		for (int i = 0; i < p_args.size(); i++) {
			mi.arguments.push_back(PropertyInfo(p_args[i], "arg" + itos(i + 1)));
		}
		ADD_SIGNAL(mi);
	}

	JNISingleton() {}

	JNISingleton(const Ref<JavaObject> &p_wrapped_object) {
		wrapped_object = p_wrapped_object;
	}

	~JNISingleton() {
		method_map.clear();
		wrapped_object.unref();
	}
};
