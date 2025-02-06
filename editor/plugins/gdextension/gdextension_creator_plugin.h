/**************************************************************************/
/*  gdextension_creator_plugin.h                                          */
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

#ifndef GDEXTENSION_CREATOR_PLUGIN_H
#define GDEXTENSION_CREATOR_PLUGIN_H

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"

class GDExtensionCreatorPlugin : public RefCounted {
	GDCLASS(GDExtensionCreatorPlugin, RefCounted);

protected:
	enum MessageType {
		MSG_OK,
		MSG_WARNING,
		MSG_ERROR,
		MSG_INFO,
	};

public:
	virtual void create_gdextension(const String &p_path, const String &p_base_name, const String &p_library_name, int p_variation_index, bool p_compile) = 0;
	virtual void setup_creator() = 0;
	virtual PackedStringArray get_language_variations() const = 0;
	virtual Dictionary get_validation_messages(const String &p_path, const String &p_base_name, const String &p_library_name, int p_variation_index, bool p_compile) = 0;
};

#endif // GDEXTENSION_CREATOR_PLUGIN_H
