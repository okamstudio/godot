/**************************************************************************/
/*  saveload_api.cpp                                                      */
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

#include "saveload_api.h"

SaveloadAPI *SaveloadAPI::singleton = NULL;

SaveloadAPI *SaveloadAPI::get_singleton() {
	return singleton;
}

void SaveloadAPI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("track", "object"), &SaveloadAPI::track);
	ClassDB::bind_method(D_METHOD("untrack", "object"), &SaveloadAPI::untrack);
	ClassDB::bind_method(D_METHOD("serialize", "configuration_data"), &SaveloadAPI::serialize, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("deserialize", "serialized_state", "configuration_data"), &SaveloadAPI::deserialize, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("save", "path", "configuration_data"), &SaveloadAPI::save, DEFVAL(Variant()));
	ClassDB::bind_method(D_METHOD("load", "path", "configuration_data"), &SaveloadAPI::load, DEFVAL(Variant()));
}

/// SaveloadAPIExtension

//Error SaveloadAPIExtension::object_configuration_add(Object *p_object, Variant p_config) {
//	Error err = ERR_UNAVAILABLE;
//	GDVIRTUAL_CALL(_object_configuration_add, p_object, p_config, err);
//	return err;
//}
//
//Error SaveloadAPIExtension::object_configuration_remove(Object *p_object, Variant p_config) {
//	Error err = ERR_UNAVAILABLE;
//	GDVIRTUAL_CALL(_object_configuration_remove, p_object, p_config, err);
//	return err;
//}
//
//void SaveloadAPIExtension::_bind_methods() {
//	GDVIRTUAL_BIND(_object_configuration_add, "object", "configuration");
//	GDVIRTUAL_BIND(_object_configuration_remove, "object", "configuration");
//}
