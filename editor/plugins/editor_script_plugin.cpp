/**************************************************************************/
/*  editor_script_plugin.cpp                                              */
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

#include "editor_script_plugin.h"

#include "core/io/resource_loader.h"
#include "editor/editor_command_palette.h"
#include "editor/editor_interface.h"
#include "editor/editor_script.h"

Ref<EditorScript> create_instance(const StringName &p_name) {
	Ref<EditorScript> es;
	es.instantiate();
	Ref<Script> scr = ResourceLoader::load(ScriptServer::get_global_class_path(p_name), "Script", ResourceFormatLoader::CACHE_MODE_REUSE);
	if (scr.is_valid()) {
		es->set_script(scr);
	}
	return es;
}

void EditorScriptPlugin::run_command(const StringName &p_name) {
	create_instance(p_name)->run();
}

void EditorScriptPlugin::command_palette_about_to_popup() {
	for (const StringName &command : commands) {
		EditorInterface::get_singleton()->get_command_palette()->remove_command("editor_scripts/" + command);
	}
	commands.clear();
	ScriptServer::get_inheriters_list(SNAME("EditorScript"), &commands);
	for (const StringName &command : commands) {
		EditorInterface::get_singleton()->get_command_palette()->add_command(command, "editor_scripts/" + command, callable_mp(this, &EditorScriptPlugin::run_command), varray(command), nullptr);
	}
}

EditorScriptPlugin::EditorScriptPlugin() {
	EditorInterface::get_singleton()->get_command_palette()->connect("about_to_popup", callable_mp(this, &EditorScriptPlugin::command_palette_about_to_popup));
}
