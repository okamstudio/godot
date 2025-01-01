/**************************************************************************/
/*  jni_utils.cpp                                                         */
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

#include "jni_utils.h"

#include "api/java_class_wrapper.h"

jobject callable_to_jcallable(JNIEnv *p_env, const Variant &p_callable) {
	ERR_FAIL_NULL_V(p_env, nullptr);
	if (p_callable.get_type() != Variant::CALLABLE) {
		return nullptr;
	}

	Variant *callable_jcopy = memnew(Variant(p_callable));

	jclass bclass = p_env->FindClass("org/godotengine/godot/variant/Callable");
	jmethodID ctor = p_env->GetMethodID(bclass, "<init>", "(J)V");
	jobject jcallable = p_env->NewObject(bclass, ctor, reinterpret_cast<int64_t>(callable_jcopy));
	p_env->DeleteLocalRef(bclass);

	return jcallable;
}

Callable jcallable_to_callable(JNIEnv *p_env, jobject p_jcallable_obj) {
	ERR_FAIL_NULL_V(p_env, Callable());

	const Variant *callable_variant = nullptr;
	jclass callable_class = p_env->FindClass("org/godotengine/godot/variant/Callable");
	if (callable_class && p_env->IsInstanceOf(p_jcallable_obj, callable_class)) {
		jmethodID get_native_pointer = p_env->GetMethodID(callable_class, "getNativePointer", "()J");
		jlong native_callable = p_env->CallLongMethod(p_jcallable_obj, get_native_pointer);

		callable_variant = reinterpret_cast<const Variant *>(native_callable);
	}

	p_env->DeleteLocalRef(callable_class);

	ERR_FAIL_NULL_V(callable_variant, Callable());
	return *callable_variant;
}

String charsequence_to_string(JNIEnv *p_env, jobject p_charsequence) {
	ERR_FAIL_NULL_V(p_env, String());

	String result;
	jclass bclass = p_env->FindClass("java/lang/CharSequence");
	if (bclass && p_env->IsInstanceOf(p_charsequence, bclass)) {
		jmethodID to_string = p_env->GetMethodID(bclass, "toString", "()Ljava/lang/String;");
		jstring obj_string = (jstring)p_env->CallObjectMethod(p_charsequence, to_string);

		result = jstring_to_string(obj_string, p_env);
		p_env->DeleteLocalRef(obj_string);
	}

	p_env->DeleteLocalRef(bclass);
	return result;
}

jvalret _variant_to_jvalue(JNIEnv *env, Variant::Type p_type, const Variant *p_arg, bool force_jobject) {
	jvalret v;

	switch (p_type) {
		case Variant::BOOL: {
			if (force_jobject) {
				jclass bclass = env->FindClass("java/lang/Boolean");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(Z)V");
				jvalue val;
				val.z = (bool)(*p_arg);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				v.val.l = obj;
				v.obj = obj;
				env->DeleteLocalRef(bclass);
			} else {
				v.val.z = *p_arg;
			}
		} break;
		case Variant::INT: {
			if (force_jobject) {
				jclass bclass = env->FindClass("java/lang/Integer");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(I)V");
				jvalue val;
				val.i = (int)(*p_arg);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				v.val.l = obj;
				v.obj = obj;
				env->DeleteLocalRef(bclass);

			} else {
				v.val.i = *p_arg;
			}
		} break;
		case Variant::FLOAT: {
			if (force_jobject) {
				jclass bclass = env->FindClass("java/lang/Double");
				jmethodID ctor = env->GetMethodID(bclass, "<init>", "(D)V");
				jvalue val;
				val.d = (double)(*p_arg);
				jobject obj = env->NewObjectA(bclass, ctor, &val);
				v.val.l = obj;
				v.obj = obj;
				env->DeleteLocalRef(bclass);

			} else {
				v.val.f = *p_arg;
			}
		} break;
		case Variant::STRING: {
			String s = *p_arg;
			jstring jStr = env->NewStringUTF(s.utf8().get_data());
			v.val.l = jStr;
			v.obj = jStr;
		} break;
		case Variant::PACKED_STRING_ARRAY: {
			Vector<String> sarray = *p_arg;
			jobjectArray arr = env->NewObjectArray(sarray.size(), env->FindClass("java/lang/String"), env->NewStringUTF(""));

			for (int j = 0; j < sarray.size(); j++) {
				jstring str = env->NewStringUTF(sarray[j].utf8().get_data());
				env->SetObjectArrayElement(arr, j, str);
				env->DeleteLocalRef(str);
			}
			v.val.l = arr;
			v.obj = arr;

		} break;

		case Variant::CALLABLE: {
			jobject jcallable = callable_to_jcallable(env, *p_arg);
			v.val.l = jcallable;
			v.obj = jcallable;
		} break;

		case Variant::DICTIONARY: {
			Dictionary dict = *p_arg;
			jclass dclass = env->FindClass("org/godotengine/godot/Dictionary");
			jmethodID ctor = env->GetMethodID(dclass, "<init>", "()V");
			jobject jdict = env->NewObject(dclass, ctor);

			Array keys = dict.keys();

			jobjectArray jkeys = env->NewObjectArray(keys.size(), env->FindClass("java/lang/String"), env->NewStringUTF(""));
			for (int j = 0; j < keys.size(); j++) {
				jstring str = env->NewStringUTF(String(keys[j]).utf8().get_data());
				env->SetObjectArrayElement(jkeys, j, str);
				env->DeleteLocalRef(str);
			}

			jmethodID set_keys = env->GetMethodID(dclass, "set_keys", "([Ljava/lang/String;)V");
			jvalue val;
			val.l = jkeys;
			env->CallVoidMethodA(jdict, set_keys, &val);
			env->DeleteLocalRef(jkeys);

			jobjectArray jvalues = env->NewObjectArray(keys.size(), env->FindClass("java/lang/Object"), nullptr);

			for (int j = 0; j < keys.size(); j++) {
				Variant var = dict[keys[j]];
				jvalret valret = _variant_to_jvalue(env, var.get_type(), &var, true);
				env->SetObjectArrayElement(jvalues, j, valret.val.l);
				if (valret.obj) {
					env->DeleteLocalRef(valret.obj);
				}
			}

			jmethodID set_values = env->GetMethodID(dclass, "set_values", "([Ljava/lang/Object;)V");
			val.l = jvalues;
			env->CallVoidMethodA(jdict, set_values, &val);
			env->DeleteLocalRef(jvalues);
			env->DeleteLocalRef(dclass);

			v.val.l = jdict;
			v.obj = jdict;
		} break;

		case Variant::PACKED_INT32_ARRAY: {
			Vector<int> array = *p_arg;
			jintArray arr = env->NewIntArray(array.size());
			const int *r = array.ptr();
			env->SetIntArrayRegion(arr, 0, array.size(), r);
			v.val.l = arr;
			v.obj = arr;

		} break;
		case Variant::PACKED_INT64_ARRAY: {
			Vector<int64_t> array = *p_arg;
			jlongArray arr = env->NewLongArray(array.size());
			const int64_t *r = array.ptr();
			env->SetLongArrayRegion(arr, 0, array.size(), r);
			v.val.l = arr;
			v.obj = arr;

		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			Vector<uint8_t> array = *p_arg;
			jbyteArray arr = env->NewByteArray(array.size());
			const uint8_t *r = array.ptr();
			env->SetByteArrayRegion(arr, 0, array.size(), reinterpret_cast<const signed char *>(r));
			v.val.l = arr;
			v.obj = arr;

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			Vector<float> array = *p_arg;
			jfloatArray arr = env->NewFloatArray(array.size());
			const float *r = array.ptr();
			env->SetFloatArrayRegion(arr, 0, array.size(), r);
			v.val.l = arr;
			v.obj = arr;

		} break;
		case Variant::PACKED_FLOAT64_ARRAY: {
			Vector<double> array = *p_arg;
			jdoubleArray arr = env->NewDoubleArray(array.size());
			const double *r = array.ptr();
			env->SetDoubleArrayRegion(arr, 0, array.size(), r);
			v.val.l = arr;
			v.obj = arr;

		} break;
		case Variant::OBJECT: {
			Ref<JavaObject> generic_object = *p_arg;
			if (generic_object.is_valid()) {
				jobject obj = env->NewLocalRef(generic_object->get_instance());
				v.val.l = obj;
				v.obj = obj;
			} else {
				v.val.i = 0;
			}
		} break;

		default: {
			v.val.i = 0;
		} break;
	}
	return v;
}

String _get_class_name(JNIEnv *env, jclass cls, bool *array) {
	jclass cclass = env->FindClass("java/lang/Class");
	jmethodID getName = env->GetMethodID(cclass, "getName", "()Ljava/lang/String;");
	jstring clsName = (jstring)env->CallObjectMethod(cls, getName);

	if (array) {
		jmethodID isArray = env->GetMethodID(cclass, "isArray", "()Z");
		jboolean isarr = env->CallBooleanMethod(cls, isArray);
		(*array) = isarr != 0;
	}
	String name = jstring_to_string(clsName, env);
	env->DeleteLocalRef(clsName);

	return name;
}

Variant _jobject_to_variant(JNIEnv *env, jobject obj) {
	if (obj == nullptr) {
		return Variant();
	}

	jclass c = env->GetObjectClass(obj);
	bool array;
	String name = _get_class_name(env, c, &array);

	if (name == "java.lang.String") {
		return jstring_to_string((jstring)obj, env);
	}

	if (name == "java.lang.CharSequence") {
		return charsequence_to_string(env, obj);
	}

	if (name == "[Ljava.lang.String;") {
		jobjectArray arr = (jobjectArray)obj;
		int stringCount = env->GetArrayLength(arr);
		Vector<String> sarr;

		for (int i = 0; i < stringCount; i++) {
			jstring string = (jstring)env->GetObjectArrayElement(arr, i);
			sarr.push_back(jstring_to_string(string, env));
			env->DeleteLocalRef(string);
		}

		return sarr;
	}

	if (name == "[Ljava.lang.CharSequence;") {
		jobjectArray arr = (jobjectArray)obj;
		int stringCount = env->GetArrayLength(arr);
		Vector<String> sarr;

		for (int i = 0; i < stringCount; i++) {
			jobject charsequence = env->GetObjectArrayElement(arr, i);
			sarr.push_back(charsequence_to_string(env, charsequence));
			env->DeleteLocalRef(charsequence);
		}

		return sarr;
	}

	if (name == "java.lang.Boolean") {
		jmethodID boolValue = env->GetMethodID(c, "booleanValue", "()Z");
		bool ret = env->CallBooleanMethod(obj, boolValue);
		return ret;
	}

	if (name == "java.lang.Integer" || name == "java.lang.Long") {
		jclass nclass = env->FindClass("java/lang/Number");
		jmethodID longValue = env->GetMethodID(nclass, "longValue", "()J");
		jlong ret = env->CallLongMethod(obj, longValue);
		return ret;
	}

	if (name == "[I") {
		jintArray arr = (jintArray)obj;
		int fCount = env->GetArrayLength(arr);
		Vector<int> sarr;
		sarr.resize(fCount);

		int *w = sarr.ptrw();
		env->GetIntArrayRegion(arr, 0, fCount, w);
		return sarr;
	}

	if (name == "[J") {
		jlongArray arr = (jlongArray)obj;
		int fCount = env->GetArrayLength(arr);
		Vector<int64_t> sarr;
		sarr.resize(fCount);

		int64_t *w = sarr.ptrw();
		env->GetLongArrayRegion(arr, 0, fCount, w);
		return sarr;
	}

	if (name == "[B") {
		jbyteArray arr = (jbyteArray)obj;
		int fCount = env->GetArrayLength(arr);
		Vector<uint8_t> sarr;
		sarr.resize(fCount);

		uint8_t *w = sarr.ptrw();
		env->GetByteArrayRegion(arr, 0, fCount, reinterpret_cast<signed char *>(w));
		return sarr;
	}

	if (name == "java.lang.Float" || name == "java.lang.Double") {
		jclass nclass = env->FindClass("java/lang/Number");
		jmethodID doubleValue = env->GetMethodID(nclass, "doubleValue", "()D");
		double ret = env->CallDoubleMethod(obj, doubleValue);
		return ret;
	}

	if (name == "[D") {
		jdoubleArray arr = (jdoubleArray)obj;
		int fCount = env->GetArrayLength(arr);
		PackedFloat64Array packed_array;
		packed_array.resize(fCount);

		double *w = packed_array.ptrw();

		for (int i = 0; i < fCount; i++) {
			double n;
			env->GetDoubleArrayRegion(arr, i, 1, &n);
			w[i] = n;
		}
		return packed_array;
	}

	if (name == "[F") {
		jfloatArray arr = (jfloatArray)obj;
		int fCount = env->GetArrayLength(arr);
		PackedFloat32Array packed_array;
		packed_array.resize(fCount);

		float *w = packed_array.ptrw();

		for (int i = 0; i < fCount; i++) {
			float n;
			env->GetFloatArrayRegion(arr, i, 1, &n);
			w[i] = n;
		}
		return packed_array;
	}

	if (name == "[Ljava.lang.Object;") {
		jobjectArray arr = (jobjectArray)obj;
		int objCount = env->GetArrayLength(arr);
		Array varr;

		for (int i = 0; i < objCount; i++) {
			jobject jobj = env->GetObjectArrayElement(arr, i);
			Variant v = _jobject_to_variant(env, jobj);
			varr.push_back(v);
			env->DeleteLocalRef(jobj);
		}

		return varr;
	}

	if (name == "java.util.HashMap" || name == "org.godotengine.godot.Dictionary") {
		Dictionary ret;
		jclass oclass = c;
		jmethodID get_keys = env->GetMethodID(oclass, "get_keys", "()[Ljava/lang/String;");
		jobjectArray arr = (jobjectArray)env->CallObjectMethod(obj, get_keys);

		PackedStringArray keys = _jobject_to_variant(env, arr);
		env->DeleteLocalRef(arr);

		jmethodID get_values = env->GetMethodID(oclass, "get_values", "()[Ljava/lang/Object;");
		arr = (jobjectArray)env->CallObjectMethod(obj, get_values);

		Array vals = _jobject_to_variant(env, arr);
		env->DeleteLocalRef(arr);

		for (int i = 0; i < keys.size(); i++) {
			ret[keys[i]] = vals[i];
		}

		return ret;
	}

	if (name == "org.godotengine.godot.variant.Callable") {
		return jcallable_to_callable(env, obj);
	}

	Ref<JavaObject> generic_object(memnew(JavaObject(JavaClassWrapper::get_singleton()->wrap(name), obj)));

	env->DeleteLocalRef(c);

	return generic_object;
}

Variant::Type get_jni_type(const String &p_type) {
	static struct {
		const char *name;
		Variant::Type type;
	} _type_to_vtype[] = {
		{ "void", Variant::NIL },
		{ "boolean", Variant::BOOL },
		{ "int", Variant::INT },
		{ "long", Variant::INT },
		{ "float", Variant::FLOAT },
		{ "double", Variant::FLOAT },
		{ "java.lang.String", Variant::STRING },
		{ "java.lang.CharSequence", Variant::STRING },
		{ "[I", Variant::PACKED_INT32_ARRAY },
		{ "[J", Variant::PACKED_INT64_ARRAY },
		{ "[B", Variant::PACKED_BYTE_ARRAY },
		{ "[F", Variant::PACKED_FLOAT32_ARRAY },
		{ "[D", Variant::PACKED_FLOAT64_ARRAY },
		{ "[Ljava.lang.String;", Variant::PACKED_STRING_ARRAY },
		{ "[Ljava.lang.CharSequence;", Variant::PACKED_STRING_ARRAY },
		{ "org.godotengine.godot.Dictionary", Variant::DICTIONARY },
		{ "org.godotengine.godot.variant.Callable", Variant::CALLABLE },
		{ nullptr, Variant::NIL }
	};

	int idx = 0;

	while (_type_to_vtype[idx].name) {
		if (p_type == _type_to_vtype[idx].name) {
			return _type_to_vtype[idx].type;
		}

		idx++;
	}

	return Variant::OBJECT;
}
