/**************************************************************************/
/*  saveload_api.h                                                        */
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

#ifndef SAVELOAD_API_H
#define SAVELOAD_API_H

#include "core/object/class_db.h"

class SaveloadAPI : public Object {
	GDCLASS(SaveloadAPI, Object);

	static SaveloadAPI *singleton;

protected:
	static void _bind_methods();

public:
	static SaveloadAPI *get_singleton();

	virtual Error track(Object *p_object) = 0;
	virtual Error untrack(Object *p_object) = 0;

	virtual Variant serialize(const Variant &p_configuration_data = Variant()) = 0;
	virtual Error deserialize(const Variant &p_serialized_state, const Variant &p_configuration_data = Variant()) = 0;

	virtual Error save(const String &p_path, const Variant &p_configuration_data = Variant()) = 0;
	virtual Error load(const String &p_path, const Variant &p_configuration_data = Variant()) = 0;

	SaveloadAPI() { singleton = this; }
	virtual ~SaveloadAPI() { singleton = nullptr; }
};

//class SaveloadAPIExtension : public SaveloadAPI {
//	GDCLASS(SaveloadAPIExtension, SaveloadAPI);
//
//protected:
//	static void _bind_methods();
//
//public:
//	virtual Error object_configuration_add(Object *p_object, Variant p_config) override;
//	virtual Error object_configuration_remove(Object *p_object, Variant p_config) override;
//
//	// Extensions
//	GDVIRTUAL2R(Error, _object_configuration_add, Object *, Variant);
//	GDVIRTUAL2R(Error, _object_configuration_remove, Object *, Variant);
//};

#endif // SAVELOAD_API_H
