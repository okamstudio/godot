/**************************************************************************/
/*  shader_types.h                                                        */
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

#ifndef SHADER_TYPES_H
#define SHADER_TYPES_H

#include "servers/rendering_server.h"
#include "shader_language.h"

class ShaderTypes {
	struct Type {
		HashMap<StringName, ShaderLanguage::FunctionInfo> functions;
		Vector<ShaderLanguage::ModeInfo> modes;
	};

	HashMap<RS::ShaderMode, Type> shader_modes;

	static ShaderTypes *singleton;

	HashSet<String> shader_types;
	List<String> shader_types_list;

public:
	static ShaderTypes *get_singleton() { return singleton; }

	const HashMap<StringName, ShaderLanguage::FunctionInfo> &get_functions(RS::ShaderMode p_mode) const;
	const Vector<ShaderLanguage::ModeInfo> &get_modes(RS::ShaderMode p_mode) const;
	const HashSet<String> &get_types() const;
	const List<String> &get_types_list() const;

	ShaderTypes();
};

#endif // SHADER_TYPES_H
