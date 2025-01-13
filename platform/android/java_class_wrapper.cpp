/**************************************************************************/
/*  java_class_wrapper.cpp                                                */
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

#include "api/java_class_wrapper.h"

#include "jni_utils.h"
#include "thread_jandroid.h"

bool JavaClass::_call_method(JavaObject *p_instance, const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error, Variant &ret) {
	HashMap<StringName, List<MethodInfo>>::Iterator M = methods.find(p_method);
	if (!M) {
		return false;
	}

	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, false);

	MethodInfo *method = nullptr;
	for (MethodInfo &E : M->value) {
		if (!p_instance && !E._static && !E._constructor) {
			r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
			continue;
		}

		int pc = E.param_types.size();
		if (p_argcount < pc) {
			r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
			r_error.expected = pc;
			continue;
		}
		if (p_argcount > pc) {
			r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
			r_error.expected = pc;
			continue;
		}
		uint32_t *ptypes = E.param_types.ptrw();
		bool valid = true;

		for (int i = 0; i < pc; i++) {
			Variant::Type arg_expected = Variant::NIL;
			switch (ptypes[i]) {
				case ARG_TYPE_VOID: {
					//bug?
				} break;
				case ARG_TYPE_BOOLEAN: {
					if (p_args[i]->get_type() != Variant::BOOL) {
						arg_expected = Variant::BOOL;
					}
				} break;
				case ARG_NUMBER_CLASS_BIT | ARG_TYPE_BYTE:
				case ARG_NUMBER_CLASS_BIT | ARG_TYPE_CHAR:
				case ARG_NUMBER_CLASS_BIT | ARG_TYPE_SHORT:
				case ARG_NUMBER_CLASS_BIT | ARG_TYPE_INT:
				case ARG_NUMBER_CLASS_BIT | ARG_TYPE_LONG:
				case ARG_TYPE_BYTE:
				case ARG_TYPE_CHAR:
				case ARG_TYPE_SHORT:
				case ARG_TYPE_INT:
				case ARG_TYPE_LONG: {
					if (!p_args[i]->is_num()) {
						arg_expected = Variant::INT;
					}
				} break;
				case ARG_NUMBER_CLASS_BIT | ARG_TYPE_FLOAT:
				case ARG_NUMBER_CLASS_BIT | ARG_TYPE_DOUBLE:
				case ARG_TYPE_FLOAT:
				case ARG_TYPE_DOUBLE: {
					if (!p_args[i]->is_num()) {
						arg_expected = Variant::FLOAT;
					}
				} break;
				case ARG_TYPE_STRING:
				case ARG_TYPE_CHARSEQUENCE: {
					if (!p_args[i]->is_string()) {
						arg_expected = Variant::STRING;
					}
				} break;
				case ARG_TYPE_CALLABLE: {
					if (p_args[i]->get_type() != Variant::CALLABLE) {
						arg_expected = Variant::CALLABLE;
					}
				} break;
				case ARG_TYPE_CLASS: {
					if (p_args[i]->get_type() != Variant::OBJECT && p_args[i]->get_type() != Variant::NIL) {
						arg_expected = Variant::OBJECT;
					} else {
						Ref<RefCounted> ref = *p_args[i];
						if (ref.is_valid()) {
							if (Object::cast_to<JavaObject>(ref.ptr())) {
								Ref<JavaObject> jo = ref;
								//could be faster
								String cn = E.param_sigs[i].operator String();
								if (cn.begins_with("L") && cn.ends_with(";")) {
									cn = cn.substr(1, cn.length() - 2);
								}
								jclass c = env->FindClass(cn.utf8().get_data());
								if (!c || !env->IsInstanceOf(jo->instance, c)) {
									arg_expected = Variant::OBJECT;
								} else {
									//ok
								}
							} else {
								arg_expected = Variant::OBJECT;
							}
						}
					}
				} break;
				default: {
					if (p_args[i]->get_type() != Variant::ARRAY) {
						arg_expected = Variant::ARRAY;
					}
				} break;
			}

			if (arg_expected != Variant::NIL) {
				r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
				r_error.argument = i;
				r_error.expected = arg_expected;
				valid = false;
				break;
			}
		}
		if (!valid) {
			continue;
		}

		method = &E;
		break;
	}

	if (!method) {
		return true; //no version convinces
	}

	r_error.error = Callable::CallError::CALL_OK;

	jvalue *argv = nullptr;

	if (method->param_types.size()) {
		argv = (jvalue *)alloca(sizeof(jvalue) * method->param_types.size());
	}

	List<jobject> to_free;
	for (int i = 0; i < method->param_types.size(); i++) {
		switch (method->param_types[i]) {
			case ARG_TYPE_VOID: {
				//can't happen
				argv[i].l = nullptr; //I hope this works
			} break;

			case ARG_TYPE_BOOLEAN: {
				argv[i].z = *p_args[i];
			} break;
			case ARG_TYPE_BYTE: {
				argv[i].b = *p_args[i];
			} break;
			case ARG_TYPE_CHAR: {
				argv[i].c = *p_args[i];
			} break;
			case ARG_TYPE_SHORT: {
				argv[i].s = *p_args[i];
			} break;
			case ARG_TYPE_INT: {
				argv[i].i = *p_args[i];
			} break;
			case ARG_TYPE_LONG: {
				argv[i].j = (int64_t)*p_args[i];
			} break;
			case ARG_TYPE_FLOAT: {
				argv[i].f = *p_args[i];
			} break;
			case ARG_TYPE_DOUBLE: {
				argv[i].d = *p_args[i];
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_BOOLEAN: {
				jclass bclass = env->FindClass("java/lang/Boolean");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(Z)V");
				jvalue val;
				val.z = (bool)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_BYTE: {
				jclass bclass = env->FindClass("java/lang/Byte");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(B)V");
				jvalue val;
				val.b = (int)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_CHAR: {
				jclass bclass = env->FindClass("java/lang/Character");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(C)V");
				jvalue val;
				val.c = (int)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_SHORT: {
				jclass bclass = env->FindClass("java/lang/Short");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(S)V");
				jvalue val;
				val.s = (int)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_INT: {
				jclass bclass = env->FindClass("java/lang/Integer");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(I)V");
				jvalue val;
				val.i = (int)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_LONG: {
				jclass bclass = env->FindClass("java/lang/Long");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(J)V");
				jvalue val;
				val.j = (int64_t)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_FLOAT: {
				jclass bclass = env->FindClass("java/lang/Float");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(F)V");
				jvalue val;
				val.f = (float)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_NUMBER_CLASS_BIT | ARG_TYPE_DOUBLE: {
				jclass bclass = env->FindClass("java/lang/Double");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(D)V");
				jvalue val;
				val.d = (double)(*p_args[i]);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				argv[i].l = obj;
				to_free.push_back(obj);
			} break;
			case ARG_TYPE_STRING:
			case ARG_TYPE_CHARSEQUENCE: {
				String s = *p_args[i];
				jstring jStr = env->NewStringUTF(s.utf8().get_data());
				argv[i].l = jStr;
				to_free.push_back(jStr);
			} break;
			case ARG_TYPE_CALLABLE: {
				jobject jcallable = callable_to_jcallable(env, *p_args[i]);
				argv[i].l = jcallable;
				to_free.push_back(jcallable);
			} break;
			case ARG_TYPE_CLASS: {
				Ref<JavaObject> jo = *p_args[i];
				if (jo.is_valid()) {
					argv[i].l = jo->instance;
				} else {
					argv[i].l = nullptr; //I hope this works
				}

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_BOOLEAN: {
				Array arr = *p_args[i];
				jbooleanArray a = env->NewBooleanArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jboolean val = arr[j];
					env->SetBooleanArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_BYTE: {
				Array arr = *p_args[i];
				jbyteArray a = env->NewByteArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jbyte val = arr[j];
					env->SetByteArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_CHAR: {
				Array arr = *p_args[i];
				jcharArray a = env->NewCharArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jchar val = arr[j];
					env->SetCharArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_SHORT: {
				Array arr = *p_args[i];
				jshortArray a = env->NewShortArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jshort val = arr[j];
					env->SetShortArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_INT: {
				Array arr = *p_args[i];
				jintArray a = env->NewIntArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jint val = arr[j];
					env->SetIntArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);
			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_LONG: {
				Array arr = *p_args[i];
				jlongArray a = env->NewLongArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jlong val = (int64_t)arr[j];
					env->SetLongArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_FLOAT: {
				Array arr = *p_args[i];
				jfloatArray a = env->NewFloatArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jfloat val = arr[j];
					env->SetFloatArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_DOUBLE: {
				Array arr = *p_args[i];
				jdoubleArray a = env->NewDoubleArray(arr.size());
				for (int j = 0; j < arr.size(); j++) {
					jdouble val = arr[j];
					env->SetDoubleArrayRegion(a, j, 1, &val);
				}
				argv[i].l = a;
				to_free.push_back(a);

			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_STRING:
			case ARG_ARRAY_BIT | ARG_TYPE_CHARSEQUENCE: {
				Array arr = *p_args[i];
				jobjectArray a = env->NewObjectArray(arr.size(), env->FindClass("java/lang/String"), nullptr);
				for (int j = 0; j < arr.size(); j++) {
					String s = arr[j];
					jstring jStr = env->NewStringUTF(s.utf8().get_data());
					env->SetObjectArrayElement(a, j, jStr);
					to_free.push_back(jStr);
				}

				argv[i].l = a;
				to_free.push_back(a);
			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_CALLABLE: {
				Array arr = *p_args[i];
				jobjectArray jarr = env->NewObjectArray(arr.size(), env->FindClass("org/godotengine/godot/variant/Callable"), nullptr);
				for (int j = 0; j < arr.size(); j++) {
					Variant callable = arr[j];
					jobject jcallable = callable_to_jcallable(env, callable);
					env->SetObjectArrayElement(jarr, j, jcallable);
					to_free.push_back(jcallable);
				}

				argv[i].l = jarr;
				to_free.push_back(jarr);
			} break;
			case ARG_ARRAY_BIT | ARG_TYPE_CLASS: {
				argv[i].l = nullptr;
			} break;
		}
	}

	r_error.error = Callable::CallError::CALL_OK;
	bool success = true;

	switch (method->return_type) {
		case ARG_TYPE_VOID: {
			if (method->_static) {
				env->CallStaticVoidMethodA(_class, method->method, argv);
			} else {
				env->CallVoidMethodA(p_instance->instance, method->method, argv);
			}
			ret = Variant();

		} break;
		case ARG_TYPE_BOOLEAN: {
			if (method->_static) {
				ret = env->CallStaticBooleanMethodA(_class, method->method, argv);
			} else {
				ret = env->CallBooleanMethodA(p_instance->instance, method->method, argv);
			}
		} break;
		case ARG_TYPE_BYTE: {
			if (method->_static) {
				ret = env->CallStaticByteMethodA(_class, method->method, argv);
			} else {
				ret = env->CallByteMethodA(p_instance->instance, method->method, argv);
			}
		} break;
		case ARG_TYPE_CHAR: {
			if (method->_static) {
				ret = env->CallStaticCharMethodA(_class, method->method, argv);
			} else {
				ret = env->CallCharMethodA(p_instance->instance, method->method, argv);
			}
		} break;
		case ARG_TYPE_SHORT: {
			if (method->_static) {
				ret = env->CallStaticShortMethodA(_class, method->method, argv);
			} else {
				ret = env->CallShortMethodA(p_instance->instance, method->method, argv);
			}

		} break;
		case ARG_TYPE_INT: {
			if (method->_static) {
				ret = env->CallStaticIntMethodA(_class, method->method, argv);
			} else {
				ret = env->CallIntMethodA(p_instance->instance, method->method, argv);
			}

		} break;
		case ARG_TYPE_LONG: {
			if (method->_static) {
				ret = (int64_t)env->CallStaticLongMethodA(_class, method->method, argv);
			} else {
				ret = (int64_t)env->CallLongMethodA(p_instance->instance, method->method, argv);
			}

		} break;
		case ARG_TYPE_FLOAT: {
			if (method->_static) {
				ret = env->CallStaticFloatMethodA(_class, method->method, argv);
			} else {
				ret = env->CallFloatMethodA(p_instance->instance, method->method, argv);
			}

		} break;
		case ARG_TYPE_DOUBLE: {
			if (method->_static) {
				ret = env->CallStaticDoubleMethodA(_class, method->method, argv);
			} else {
				ret = env->CallDoubleMethodA(p_instance->instance, method->method, argv);
			}

		} break;
		default: {
			jobject obj;
			if (method->_constructor) {
				obj = env->NewObjectA(_class, method->method, argv);
			} else if (method->_static) {
				obj = env->CallStaticObjectMethodA(_class, method->method, argv);
			} else {
				obj = env->CallObjectMethodA(p_instance->instance, method->method, argv);
			}

			if (!obj) {
				ret = Variant();
			} else {
				if (!_convert_object_to_variant(env, obj, ret, method->return_type)) {
					ret = Variant();
					r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
					success = false;
				}
				env->DeleteLocalRef(obj);
			}

		} break;
	}

	for (jobject &E : to_free) {
		env->DeleteLocalRef(E);
	}

	return success;
}

bool JavaClass::_get(const StringName &p_name, Variant &r_ret) const {
	if (constant_map.has(p_name)) {
		r_ret = constant_map[p_name];
		return true;
	}

	return false;
}

Variant JavaClass::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	Variant ret;

	String method = (p_method == java_constructor_name) ? "<init>" : p_method;
	bool found = _call_method(nullptr, method, p_args, p_argcount, r_error, ret);
	if (found) {
		return ret;
	}

	return RefCounted::callp(p_method, p_args, p_argcount, r_error);
}

String JavaClass::get_java_class_name() const {
	return java_class_name;
}

TypedArray<Dictionary> JavaClass::get_java_method_list() const {
	TypedArray<Dictionary> method_list;

	for (const KeyValue<StringName, List<MethodInfo>> &item : methods) {
		for (const MethodInfo &mi : item.value) {
			Dictionary method;

			method["name"] = mi._constructor ? java_constructor_name : String(item.key);
			method["id"] = (uint64_t)mi.method;
			method["default_args"] = Array();
			method["flags"] = METHOD_FLAGS_DEFAULT & (mi._static || mi._constructor ? METHOD_FLAG_STATIC : METHOD_FLAG_NORMAL);

			{
				Array a;

				for (uint32_t argtype : mi.param_types) {
					Dictionary d;

					Variant::Type t = Variant::NIL;
					float likelihood = 0.0;
					_convert_to_variant_type(argtype, t, likelihood);
					d["type"] = t;
					if (t == Variant::OBJECT) {
						d["hint"] = PROPERTY_HINT_RESOURCE_TYPE;
						d["hint_string"] = "JavaObject";
					} else {
						d["hint"] = 0;
						d["hint_string"] = "";
					}

					a.push_back(d);
				}

				method["args"] = a;
			}

			{
				Dictionary d;

				if (mi._constructor) {
					d["type"] = Variant::OBJECT;
					d["hint"] = PROPERTY_HINT_RESOURCE_TYPE;
					d["hint_string"] = "JavaObject";
				} else {
					Variant::Type t = Variant::NIL;
					float likelihood = 0.0;
					_convert_to_variant_type(mi.return_type, t, likelihood);
					d["type"] = t;
					if (t == Variant::OBJECT) {
						d["hint"] = PROPERTY_HINT_RESOURCE_TYPE;
						d["hint_string"] = "JavaObject";
					} else {
						d["hint"] = 0;
						d["hint_string"] = "";
					}
				}

				method["return_type"] = d;
			}

			method_list.push_back(method);
		}
	}

	return method_list;
}

Ref<JavaClass> JavaClass::get_java_parent_class() const {
	ERR_FAIL_NULL_V(_class, Ref<JavaClass>());

	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, Ref<JavaClass>());

	jclass superclass = (jclass)env->CallObjectMethod(_class, JavaClassWrapper::singleton->Class_getSuperclass);
	if (!superclass) {
		return Ref<JavaClass>();
	}

	Ref<JavaClass> ret = JavaClassWrapper::singleton->wrap_jclass(superclass);
	env->DeleteLocalRef(superclass);
	return ret;
}

String JavaClass::to_string() {
	return "<JavaClass:" + java_class_name + ">";
}

JavaClass::JavaClass() {
}

JavaClass::~JavaClass() {
	if (_class) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);

		env->DeleteGlobalRef(_class);
	}
}

/////////////////////

Variant JavaObject::callp(const StringName &p_method, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (instance) {
		Ref<JavaClass> c = base_class;
		while (c.is_valid()) {
			Variant ret;
			bool found = c->_call_method(this, p_method, p_args, p_argcount, r_error, ret);
			if (found) {
				return ret;
			}
			c = c->get_java_parent_class();
		}
	}

	return RefCounted::callp(p_method, p_args, p_argcount, r_error);
}

Ref<JavaClass> JavaObject::get_java_class() const {
	return base_class;
}

String JavaObject::to_string() {
	if (base_class.is_valid() && instance) {
		return "<JavaObject:" + base_class->java_class_name + " \"" + (String)call("toString") + "\">";
	}
	return RefCounted::to_string();
}

JavaObject::JavaObject() {
}

JavaObject::JavaObject(const Ref<JavaClass> &p_base, jobject p_instance) {
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);

	base_class = p_base;
	instance = env->NewGlobalRef(p_instance);
}

JavaObject::~JavaObject() {
	if (instance) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);

		env->DeleteGlobalRef(instance);
	}
}

////////////////////

bool JavaClassWrapper::_get_type_sig(JNIEnv *env, jobject obj, uint32_t &sig, String &strsig) {
	jstring name2 = (jstring)env->CallObjectMethod(obj, Class_getName);
	String str_type = jstring_to_string(name2, env);
	env->DeleteLocalRef(name2);
	uint32_t t = 0;

	if (str_type.begins_with("[")) {
		t = JavaClass::ARG_ARRAY_BIT;
		strsig = "[";
		str_type = str_type.substr(1, str_type.length() - 1);
		if (str_type.begins_with("[")) {
			print_line("Nested arrays not supported for type: " + str_type);
			return false;
		}
		if (str_type.begins_with("L")) {
			str_type = str_type.substr(1, str_type.length() - 2); //ok it's a class
		}
	}

	if (str_type == "void" || str_type == "V") {
		t |= JavaClass::ARG_TYPE_VOID;
		strsig += "V";
	} else if (str_type == "boolean" || str_type == "Z") {
		t |= JavaClass::ARG_TYPE_BOOLEAN;
		strsig += "Z";
	} else if (str_type == "byte" || str_type == "B") {
		t |= JavaClass::ARG_TYPE_BYTE;
		strsig += "B";
	} else if (str_type == "char" || str_type == "C") {
		t |= JavaClass::ARG_TYPE_CHAR;
		strsig += "C";
	} else if (str_type == "short" || str_type == "S") {
		t |= JavaClass::ARG_TYPE_SHORT;
		strsig += "S";
	} else if (str_type == "int" || str_type == "I") {
		t |= JavaClass::ARG_TYPE_INT;
		strsig += "I";
	} else if (str_type == "long" || str_type == "J") {
		t |= JavaClass::ARG_TYPE_LONG;
		strsig += "J";
	} else if (str_type == "float" || str_type == "F") {
		t |= JavaClass::ARG_TYPE_FLOAT;
		strsig += "F";
	} else if (str_type == "double" || str_type == "D") {
		t |= JavaClass::ARG_TYPE_DOUBLE;
		strsig += "D";
	} else if (str_type == "java.lang.String") {
		t |= JavaClass::ARG_TYPE_STRING;
		strsig += "Ljava/lang/String;";
	} else if (str_type == "java.lang.CharSequence") {
		t |= JavaClass::ARG_TYPE_CHARSEQUENCE;
		strsig += "Ljava/lang/CharSequence;";
	} else if (str_type == "org.godotengine.godot.variant.Callable") {
		t |= JavaClass::ARG_TYPE_CALLABLE;
		strsig += "Lorg/godotengine/godot/variant/Callable;";
	} else if (str_type == "java.lang.Boolean") {
		t |= JavaClass::ARG_TYPE_BOOLEAN | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Boolean;";
	} else if (str_type == "java.lang.Byte") {
		t |= JavaClass::ARG_TYPE_BYTE | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Byte;";
	} else if (str_type == "java.lang.Character") {
		t |= JavaClass::ARG_TYPE_CHAR | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Character;";
	} else if (str_type == "java.lang.Short") {
		t |= JavaClass::ARG_TYPE_SHORT | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Short;";
	} else if (str_type == "java.lang.Integer") {
		t |= JavaClass::ARG_TYPE_INT | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Integer;";
	} else if (str_type == "java.lang.Long") {
		t |= JavaClass::ARG_TYPE_LONG | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Long;";
	} else if (str_type == "java.lang.Float") {
		t |= JavaClass::ARG_TYPE_FLOAT | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Float;";
	} else if (str_type == "java.lang.Double") {
		t |= JavaClass::ARG_TYPE_DOUBLE | JavaClass::ARG_NUMBER_CLASS_BIT;
		strsig += "Ljava/lang/Double;";
	} else {
		//a class likely
		strsig += "L" + str_type.replace(".", "/") + ";";
		t |= JavaClass::ARG_TYPE_CLASS;
	}

	sig = t;

	return true;
}

bool JavaClass::_convert_object_to_variant(JNIEnv *env, jobject obj, Variant &var, uint32_t p_sig) {
	if (!obj) {
		var = Variant(); //seems null is just null...
		return true;
	}

	switch (p_sig) {
		case ARG_TYPE_VOID: {
			return Variant();
		} break;
		case ARG_TYPE_BOOLEAN | ARG_NUMBER_CLASS_BIT: {
			var = env->CallBooleanMethod(obj, JavaClassWrapper::singleton->Boolean_booleanValue);
			return true;
		} break;
		case ARG_TYPE_BYTE | ARG_NUMBER_CLASS_BIT: {
			var = env->CallByteMethod(obj, JavaClassWrapper::singleton->Byte_byteValue);
			return true;

		} break;
		case ARG_TYPE_CHAR | ARG_NUMBER_CLASS_BIT: {
			var = env->CallCharMethod(obj, JavaClassWrapper::singleton->Character_characterValue);
			return true;

		} break;
		case ARG_TYPE_SHORT | ARG_NUMBER_CLASS_BIT: {
			var = env->CallShortMethod(obj, JavaClassWrapper::singleton->Short_shortValue);
			return true;

		} break;
		case ARG_TYPE_INT | ARG_NUMBER_CLASS_BIT: {
			var = env->CallIntMethod(obj, JavaClassWrapper::singleton->Integer_integerValue);
			return true;

		} break;
		case ARG_TYPE_LONG | ARG_NUMBER_CLASS_BIT: {
			var = (int64_t)env->CallLongMethod(obj, JavaClassWrapper::singleton->Long_longValue);
			return true;

		} break;
		case ARG_TYPE_FLOAT | ARG_NUMBER_CLASS_BIT: {
			var = env->CallFloatMethod(obj, JavaClassWrapper::singleton->Float_floatValue);
			return true;

		} break;
		case ARG_TYPE_DOUBLE | ARG_NUMBER_CLASS_BIT: {
			var = env->CallDoubleMethod(obj, JavaClassWrapper::singleton->Double_doubleValue);
			return true;
		} break;
		case ARG_TYPE_STRING: {
			var = jstring_to_string((jstring)obj, env);
			return true;
		} break;
		case ARG_TYPE_CHARSEQUENCE: {
			var = charsequence_to_string(env, obj);
			return true;
		} break;
		case ARG_TYPE_CALLABLE: {
			var = jcallable_to_callable(env, obj);
			return true;
		} break;
		case ARG_TYPE_CLASS: {
			jclass java_class = env->GetObjectClass(obj);
			Ref<JavaClass> java_class_wrapped = JavaClassWrapper::singleton->wrap_jclass(java_class);
			env->DeleteLocalRef(java_class);

			if (java_class_wrapped.is_valid()) {
				Ref<JavaObject> ret = Ref<JavaObject>(memnew(JavaObject(java_class_wrapped, obj)));
				var = ret;
				return true;
			}

			return false;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_VOID: {
			var = Array(); // ?
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_BOOLEAN: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jboolean val;
				env->GetBooleanArrayRegion((jbooleanArray)arr, 0, 1, &val);
				ret.push_back(val);
			}

			var = ret;
			return true;

		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_BYTE: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jbyte val;
				env->GetByteArrayRegion((jbyteArray)arr, 0, 1, &val);
				ret.push_back(val);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_CHAR: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jchar val;
				env->GetCharArrayRegion((jcharArray)arr, 0, 1, &val);
				ret.push_back(val);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_SHORT: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jshort val;
				env->GetShortArrayRegion((jshortArray)arr, 0, 1, &val);
				ret.push_back(val);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_INT: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jint val;
				env->GetIntArrayRegion((jintArray)arr, 0, 1, &val);
				ret.push_back(val);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_LONG: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jlong val;
				env->GetLongArrayRegion((jlongArray)arr, 0, 1, &val);
				ret.push_back((int64_t)val);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_FLOAT: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jfloat val;
				env->GetFloatArrayRegion((jfloatArray)arr, 0, 1, &val);
				ret.push_back(val);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_DOUBLE: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jdouble val;
				env->GetDoubleArrayRegion((jdoubleArray)arr, 0, 1, &val);
				ret.push_back(val);
			}

			var = ret;
			return true;
		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_BOOLEAN: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					bool val = env->CallBooleanMethod(o, JavaClassWrapper::singleton->Boolean_booleanValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;

		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_BYTE: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					int val = env->CallByteMethod(o, JavaClassWrapper::singleton->Byte_byteValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_CHAR: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					int val = env->CallCharMethod(o, JavaClassWrapper::singleton->Character_characterValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_SHORT: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					int val = env->CallShortMethod(o, JavaClassWrapper::singleton->Short_shortValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_INT: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					int val = env->CallIntMethod(o, JavaClassWrapper::singleton->Integer_integerValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_LONG: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					int64_t val = env->CallLongMethod(o, JavaClassWrapper::singleton->Long_longValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_FLOAT: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					float val = env->CallFloatMethod(o, JavaClassWrapper::singleton->Float_floatValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_NUMBER_CLASS_BIT | ARG_ARRAY_BIT | ARG_TYPE_DOUBLE: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					double val = env->CallDoubleMethod(o, JavaClassWrapper::singleton->Double_doubleValue);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;

		case ARG_ARRAY_BIT | ARG_TYPE_STRING: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					String val = jstring_to_string((jstring)o, env);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_CHARSEQUENCE: {
			Array ret;
			jobjectArray arr = (jobjectArray)obj;

			int count = env->GetArrayLength(arr);

			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(arr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					String val = charsequence_to_string(env, o);
					ret.push_back(val);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_CALLABLE: {
			Array ret;
			jobjectArray jarr = (jobjectArray)obj;
			int count = env->GetArrayLength(jarr);
			for (int i = 0; i < count; i++) {
				jobject o = env->GetObjectArrayElement(jarr, i);
				if (!o) {
					ret.push_back(Variant());
				} else {
					Callable callable = jcallable_to_callable(env, o);
					ret.push_back(callable);
				}
				env->DeleteLocalRef(o);
			}

			var = ret;
			return true;
		} break;
		case ARG_ARRAY_BIT | ARG_TYPE_CLASS: {
		} break;
	}

	return false;
}

Ref<JavaClass> JavaClassWrapper::_wrap(const String &p_class, bool p_allow_private_methods_access) {
	String class_name_dots = p_class.replace("/", ".");
	if (class_cache.has(class_name_dots)) {
		return class_cache[class_name_dots];
	}

	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, Ref<JavaClass>());

	jclass bclass = env->FindClass(class_name_dots.replace(".", "/").utf8().get_data());
	ERR_FAIL_NULL_V(bclass, Ref<JavaClass>());

	jobjectArray constructors = (jobjectArray)env->CallObjectMethod(bclass, Class_getDeclaredConstructors);
	ERR_FAIL_NULL_V(constructors, Ref<JavaClass>());

	jobjectArray methods = (jobjectArray)env->CallObjectMethod(bclass, Class_getDeclaredMethods);
	ERR_FAIL_NULL_V(methods, Ref<JavaClass>());

	Ref<JavaClass> java_class = memnew(JavaClass);
	java_class->java_class_name = class_name_dots;
	Vector<String> class_name_parts = class_name_dots.split(".");
	java_class->java_constructor_name = class_name_parts[class_name_parts.size() - 1];
	java_class->_class = (jclass)env->NewGlobalRef(bclass);
	class_cache[class_name_dots] = java_class;

	LocalVector<jobject> methods_and_constructors;
	int constructor_count = env->GetArrayLength(constructors);
	int method_count = env->GetArrayLength(methods);
	methods_and_constructors.resize(method_count + constructor_count);
	for (int i = 0; i < constructor_count; i++) {
		methods_and_constructors[i] = env->GetObjectArrayElement(constructors, i);
	}
	for (int i = 0; i < method_count; i++) {
		methods_and_constructors[constructor_count + i] = env->GetObjectArrayElement(methods, i);
	}

	for (int i = 0; i < (int)methods_and_constructors.size(); i++) {
		jobject obj = methods_and_constructors[i];
		ERR_CONTINUE(!obj);

		bool is_constructor = i < constructor_count;

		String str_method;
		if (is_constructor) {
			str_method = "<init>";
		} else {
			jstring name = (jstring)env->CallObjectMethod(obj, Method_getName);
			str_method = jstring_to_string(name, env);
			env->DeleteLocalRef(name);
		}

		Vector<String> params;

		jint mods = env->CallIntMethod(obj, is_constructor ? Constructor_getModifiers : Method_getModifiers);

		if (!(mods & 0x0001) && (is_constructor || !p_allow_private_methods_access)) {
			env->DeleteLocalRef(obj);
			continue; //not public bye
		}

		jobjectArray param_types = (jobjectArray)env->CallObjectMethod(obj, is_constructor ? Constructor_getParameterTypes : Method_getParameterTypes);
		int count = env->GetArrayLength(param_types);

		if (!java_class->methods.has(str_method)) {
			java_class->methods[str_method] = List<JavaClass::MethodInfo>();
		}

		JavaClass::MethodInfo mi;
		mi._static = (mods & 0x8) != 0;
		mi._constructor = is_constructor;
		bool valid = true;
		String signature = "(";

		for (int j = 0; j < count; j++) {
			jobject obj2 = env->GetObjectArrayElement(param_types, j);
			String strsig;
			uint32_t sig = 0;
			if (!_get_type_sig(env, obj2, sig, strsig)) {
				valid = false;
				env->DeleteLocalRef(obj2);
				break;
			}
			signature += strsig;
			mi.param_types.push_back(sig);
			mi.param_sigs.push_back(strsig);
			env->DeleteLocalRef(obj2);
		}

		if (!valid) {
			print_line("Method can't be bound (unsupported arguments): " + class_name_dots + "::" + str_method);
			env->DeleteLocalRef(obj);
			env->DeleteLocalRef(param_types);
			continue;
		}

		signature += ")";

		if (is_constructor) {
			signature += "V";
			mi.return_type = JavaClass::ARG_TYPE_CLASS;
		} else {
			jobject return_type = (jobject)env->CallObjectMethod(obj, Method_getReturnType);

			String strsig;
			uint32_t sig = 0;
			if (!_get_type_sig(env, return_type, sig, strsig)) {
				print_line("Method can't be bound (unsupported return type): " + class_name_dots + "::" + str_method);
				env->DeleteLocalRef(obj);
				env->DeleteLocalRef(param_types);
				env->DeleteLocalRef(return_type);
				continue;
			}

			signature += strsig;
			mi.return_type = sig;

			env->DeleteLocalRef(return_type);
		}

		bool discard = false;

		for (List<JavaClass::MethodInfo>::Element *E = java_class->methods[str_method].front(); E; E = E->next()) {
			float new_likeliness = 0;
			float existing_likeliness = 0;

			if (E->get().param_types.size() != mi.param_types.size()) {
				continue;
			}
			bool this_valid = true;
			for (int j = 0; j < E->get().param_types.size(); j++) {
				Variant::Type _new;
				float new_l;
				Variant::Type existing;
				float existing_l;
				JavaClass::_convert_to_variant_type(E->get().param_types[j], existing, existing_l);
				JavaClass::_convert_to_variant_type(mi.param_types[j], _new, new_l);
				if (_new != existing) {
					this_valid = false;
					break;
				}
				new_likeliness += new_l;
				existing_likeliness = existing_l;
			}

			if (!this_valid) {
				continue;
			}

			if (new_likeliness > existing_likeliness) {
				java_class->methods[str_method].erase(E);
				break;
			} else {
				discard = true;
			}
		}

		if (!discard) {
			if (mi._static) {
				mi.method = env->GetStaticMethodID(bclass, str_method.utf8().get_data(), signature.utf8().get_data());
			} else {
				mi.method = env->GetMethodID(bclass, str_method.utf8().get_data(), signature.utf8().get_data());
			}

			ERR_CONTINUE(!mi.method);

			java_class->methods[str_method].push_back(mi);
		}

		env->DeleteLocalRef(obj);
		env->DeleteLocalRef(param_types);
	}

	env->DeleteLocalRef(constructors);
	env->DeleteLocalRef(methods);

	jobjectArray fields = (jobjectArray)env->CallObjectMethod(bclass, Class_getFields);

	int count = env->GetArrayLength(fields);

	for (int i = 0; i < count; i++) {
		jobject obj = env->GetObjectArrayElement(fields, i);
		ERR_CONTINUE(!obj);

		jstring name = (jstring)env->CallObjectMethod(obj, Field_getName);
		String str_field = jstring_to_string(name, env);
		env->DeleteLocalRef(name);
		int mods = env->CallIntMethod(obj, Field_getModifiers);
		if ((mods & 0x8) && (mods & 0x10) && (mods & 0x1)) { //static final public!

			jobject objc = env->CallObjectMethod(obj, Field_get, nullptr);
			if (objc) {
				uint32_t sig;
				String strsig;
				jclass cl = env->GetObjectClass(objc);
				if (JavaClassWrapper::_get_type_sig(env, cl, sig, strsig)) {
					if ((sig & JavaClass::ARG_TYPE_MASK) <= JavaClass::ARG_TYPE_STRING) {
						Variant value;
						if (JavaClass::_convert_object_to_variant(env, objc, value, sig)) {
							java_class->constant_map[str_field] = value;
						}
					}
				}

				env->DeleteLocalRef(cl);
			}

			env->DeleteLocalRef(objc);
		}
		env->DeleteLocalRef(obj);
	}

	env->DeleteLocalRef(fields);

	return java_class;
}

Ref<JavaClass> JavaClassWrapper::wrap_jclass(jclass p_class, bool p_allow_private_methods_access) {
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, Ref<JavaClass>());

	jstring class_name = (jstring)env->CallObjectMethod(p_class, Class_getName);
	String class_name_string = jstring_to_string(class_name, env);
	env->DeleteLocalRef(class_name);

	return _wrap(class_name_string, p_allow_private_methods_access);
}

JavaClassWrapper *JavaClassWrapper::singleton = nullptr;

JavaClassWrapper::JavaClassWrapper() {
	singleton = this;

	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);

	jclass bclass = env->FindClass("java/lang/Class");
	Class_getDeclaredConstructors = env->GetMethodID(bclass, "getDeclaredConstructors", "()[Ljava/lang/reflect/Constructor;");
	Class_getDeclaredMethods = env->GetMethodID(bclass, "getDeclaredMethods", "()[Ljava/lang/reflect/Method;");
	Class_getFields = env->GetMethodID(bclass, "getFields", "()[Ljava/lang/reflect/Field;");
	Class_getName = env->GetMethodID(bclass, "getName", "()Ljava/lang/String;");
	Class_getSuperclass = env->GetMethodID(bclass, "getSuperclass", "()Ljava/lang/Class;");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/reflect/Constructor");
	Constructor_getParameterTypes = env->GetMethodID(bclass, "getParameterTypes", "()[Ljava/lang/Class;");
	Constructor_getModifiers = env->GetMethodID(bclass, "getModifiers", "()I");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/reflect/Method");
	Method_getParameterTypes = env->GetMethodID(bclass, "getParameterTypes", "()[Ljava/lang/Class;");
	Method_getReturnType = env->GetMethodID(bclass, "getReturnType", "()Ljava/lang/Class;");
	Method_getName = env->GetMethodID(bclass, "getName", "()Ljava/lang/String;");
	Method_getModifiers = env->GetMethodID(bclass, "getModifiers", "()I");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/reflect/Field");
	Field_getName = env->GetMethodID(bclass, "getName", "()Ljava/lang/String;");
	Field_getModifiers = env->GetMethodID(bclass, "getModifiers", "()I");
	Field_get = env->GetMethodID(bclass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Boolean");
	Boolean_booleanValue = env->GetMethodID(bclass, "booleanValue", "()Z");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Byte");
	Byte_byteValue = env->GetMethodID(bclass, "byteValue", "()B");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Character");
	Character_characterValue = env->GetMethodID(bclass, "charValue", "()C");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Short");
	Short_shortValue = env->GetMethodID(bclass, "shortValue", "()S");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Integer");
	Integer_integerValue = env->GetMethodID(bclass, "intValue", "()I");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Long");
	Long_longValue = env->GetMethodID(bclass, "longValue", "()J");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Float");
	Float_floatValue = env->GetMethodID(bclass, "floatValue", "()F");
	env->DeleteLocalRef(bclass);

	bclass = env->FindClass("java/lang/Double");
	Double_doubleValue = env->GetMethodID(bclass, "doubleValue", "()D");
	env->DeleteLocalRef(bclass);
}
