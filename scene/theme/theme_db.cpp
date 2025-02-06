/**************************************************************************/
/*  theme_db.cpp                                                          */
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

#include "theme_db.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"
#include "scene/main/window.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"
#include "scene/resources/texture.h"
#include "scene/theme/default_theme.h"
#include "servers/text_server.h"

// Default engine theme creation and configuration.

void ThemeDB::initialize_theme() {
	// Default theme-related project settings.

	// Allow creating the default theme at a different scale to suit higher/lower base resolutions.
	float default_theme_scale = GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "gui/theme/default_theme_scale", PROPERTY_HINT_RANGE, "0.5,8,0.01", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED), 1.0);

	String project_theme_path = GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "gui/theme/custom", PROPERTY_HINT_FILE, "*.tres,*.res,*.theme", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED), "");
	String project_font_path = GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "gui/theme/custom_font", PROPERTY_HINT_FILE, "*.tres,*.res,*.otf,*.ttf,*.woff,*.woff2,*.fnt,*.font,*.pfb,*.pfm", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED), "");

	TextServer::FontAntialiasing font_antialiasing = (TextServer::FontAntialiasing)(int)GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "gui/theme/default_font_antialiasing", PROPERTY_HINT_ENUM, "None,Grayscale,LCD Subpixel", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED), 1);
	TextServer::Hinting font_hinting = (TextServer::Hinting)(int)GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "gui/theme/default_font_hinting", PROPERTY_HINT_ENUM, "None,Light,Normal", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED), TextServer::HINTING_LIGHT);
	TextServer::SubpixelPositioning font_subpixel_positioning = (TextServer::SubpixelPositioning)(int)GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "gui/theme/default_font_subpixel_positioning", PROPERTY_HINT_ENUM, "Disabled,Auto,One Half of a Pixel,One Quarter of a Pixel", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED), TextServer::SUBPIXEL_POSITIONING_AUTO);

	const bool font_msdf = GLOBAL_DEF_RST("gui/theme/default_font_multichannel_signed_distance_field", false);
	const bool font_generate_mipmaps = GLOBAL_DEF_RST("gui/theme/default_font_generate_mipmaps", false);

	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "gui/theme/lcd_subpixel_layout", PROPERTY_HINT_ENUM, "Disabled,Horizontal RGB,Horizontal BGR,Vertical RGB,Vertical BGR"), 1);
	ProjectSettings::get_singleton()->set_restart_if_changed("gui/theme/lcd_subpixel_layout", false);

	// Attempt to load custom project theme and font.

	if (!project_theme_path.is_empty()) {
		Ref<Theme> theme = ResourceLoader::load(project_theme_path);
		if (theme.is_valid()) {
			set_project_theme(theme);
		} else {
			ERR_PRINT("Error loading custom project theme '" + project_theme_path + "'");
		}
	}

	Ref<Font> project_font;
	if (!project_font_path.is_empty()) {
		project_font = ResourceLoader::load(project_font_path);
		if (project_font.is_valid()) {
			set_fallback_font(project_font);
		} else {
			ERR_PRINT("Error loading custom project font '" + project_font_path + "'");
		}
	}

	// Always generate the default theme to serve as a fallback for all required theme definitions.

	if (RenderingServer::get_singleton()) {
		make_default_theme(default_theme_scale, project_font, font_subpixel_positioning, font_hinting, font_antialiasing, font_msdf, font_generate_mipmaps);
	}

	_init_default_theme_context();
}

void ThemeDB::initialize_theme_noproject() {
	if (RenderingServer::get_singleton()) {
		make_default_theme(1.0, Ref<Font>());
	}

	_init_default_theme_context();
}

void ThemeDB::finalize_theme() {
	if (!RenderingServer::get_singleton()) {
		WARN_PRINT("Finalizing theme when there is no RenderingServer is an error; check the order of operations.");
	}

	_finalize_theme_contexts();
	default_theme.unref();

	fallback_font.unref();
	fallback_icon.unref();
	fallback_stylebox.unref();
}

// Global Theme resources.

void ThemeDB::set_default_theme(const Ref<Theme> &p_default) {
	default_theme = p_default;
}

Ref<Theme> ThemeDB::get_default_theme() {
	return default_theme;
}

void ThemeDB::set_project_theme(const Ref<Theme> &p_project_default) {
	project_theme = p_project_default;
}

Ref<Theme> ThemeDB::get_project_theme() {
	return project_theme;
}

// Universal fallback values for theme item types.

void ThemeDB::set_fallback_base_scale(float p_base_scale) {
	if (fallback_base_scale == p_base_scale) {
		return;
	}

	fallback_base_scale = p_base_scale;
	emit_signal(SNAME("fallback_changed"));
}

float ThemeDB::get_fallback_base_scale() {
	return fallback_base_scale;
}

void ThemeDB::set_fallback_font(const Ref<Font> &p_font) {
	if (fallback_font == p_font) {
		return;
	}

	fallback_font = p_font;
	emit_signal(SNAME("fallback_changed"));
}

Ref<Font> ThemeDB::get_fallback_font() {
	return fallback_font;
}

void ThemeDB::set_fallback_font_size(int p_font_size) {
	if (fallback_font_size == p_font_size) {
		return;
	}

	fallback_font_size = p_font_size;
	emit_signal(SNAME("fallback_changed"));
}

int ThemeDB::get_fallback_font_size() {
	return fallback_font_size;
}

void ThemeDB::set_fallback_icon(const Ref<Texture2D> &p_icon) {
	if (fallback_icon == p_icon) {
		return;
	}

	fallback_icon = p_icon;
	emit_signal(SNAME("fallback_changed"));
}

Ref<Texture2D> ThemeDB::get_fallback_icon() {
	return fallback_icon;
}

void ThemeDB::set_fallback_stylebox(const Ref<StyleBox> &p_stylebox) {
	if (fallback_stylebox == p_stylebox) {
		return;
	}

	fallback_stylebox = p_stylebox;
	emit_signal(SNAME("fallback_changed"));
}

Ref<StyleBox> ThemeDB::get_fallback_stylebox() {
	return fallback_stylebox;
}

void ThemeDB::get_native_type_dependencies(const StringName &p_base_type, Vector<StringName> &r_result) {
	if (p_base_type == StringName()) {
		return;
	}

	// TODO: It may make sense to stop at Control/Window, because their parent classes cannot be used in
	// a meaningful way.
	if (!ClassDB::get_inheritance_chain_nocheck(p_base_type, r_result)) {
		r_result.push_back(p_base_type);
	}
}

// Global theme contexts.

ThemeContext *ThemeDB::create_theme_context(Node *p_node, Vector<Ref<Theme>> &p_themes) {
	ERR_FAIL_COND_V(!p_node->is_inside_tree(), nullptr);
	ERR_FAIL_COND_V(theme_contexts.has(p_node), nullptr);
	ERR_FAIL_COND_V(p_themes.is_empty(), nullptr);

	ThemeContext *context = memnew(ThemeContext);
	context->node = p_node;
	context->parent = get_nearest_theme_context(p_node);
	context->set_themes(p_themes);

	theme_contexts[p_node] = context;
	_propagate_theme_context(p_node, context);

	p_node->connect(SceneStringName(tree_exited), callable_mp(this, &ThemeDB::destroy_theme_context).bind(p_node));

	return context;
}

void ThemeDB::destroy_theme_context(Node *p_node) {
	ERR_FAIL_COND(!theme_contexts.has(p_node));

	p_node->disconnect(SceneStringName(tree_exited), callable_mp(this, &ThemeDB::destroy_theme_context));

	ThemeContext *context = theme_contexts[p_node];

	theme_contexts.erase(p_node);
	_propagate_theme_context(p_node, context->parent);

	memdelete(context);
}

void ThemeDB::_propagate_theme_context(Node *p_from_node, ThemeContext *p_context) {
	Control *from_control = Object::cast_to<Control>(p_from_node);
	Window *from_window = from_control ? nullptr : Object::cast_to<Window>(p_from_node);

	if (from_control) {
		from_control->set_theme_context(p_context);
	} else if (from_window) {
		from_window->set_theme_context(p_context);
	}

	for (int i = 0; i < p_from_node->get_child_count(); i++) {
		Node *child_node = p_from_node->get_child(i);

		// If the child is the root of another global context, stop the propagation
		// in this branch.
		if (theme_contexts.has(child_node)) {
			theme_contexts[child_node]->parent = p_context;
			continue;
		}

		_propagate_theme_context(child_node, p_context);
	}
}

void ThemeDB::_init_default_theme_context() {
	default_theme_context = memnew(ThemeContext);

	Vector<Ref<Theme>> themes;

	// Only add the project theme to the default context when running projects.

#ifdef TOOLS_ENABLED
	if (!Engine::get_singleton()->is_editor_hint()) {
		themes.push_back(project_theme);
	}
#else
	themes.push_back(project_theme);
#endif

	themes.push_back(default_theme);
	default_theme_context->set_themes(themes);
}

void ThemeDB::_finalize_theme_contexts() {
	if (default_theme_context) {
		memdelete(default_theme_context);
		default_theme_context = nullptr;
	}
	while (theme_contexts.size()) {
		HashMap<Node *, ThemeContext *>::Iterator E = theme_contexts.begin();
		memdelete(E->value);
		theme_contexts.remove(E);
	}
}

ThemeContext *ThemeDB::get_theme_context(Node *p_node) const {
	if (!theme_contexts.has(p_node)) {
		return nullptr;
	}

	return theme_contexts[p_node];
}

ThemeContext *ThemeDB::get_default_theme_context() const {
	return default_theme_context;
}

ThemeContext *ThemeDB::get_nearest_theme_context(Node *p_for_node) const {
	ERR_FAIL_COND_V(!p_for_node->is_inside_tree(), nullptr);

	Node *parent_node = p_for_node->get_parent();
	while (parent_node) {
		if (theme_contexts.has(parent_node)) {
			return theme_contexts[parent_node];
		}

		parent_node = parent_node->get_parent();
	}

	return nullptr;
}

// Theme item binding.

void ThemeDB::bind_class_item(Theme::DataType p_data_type, const StringName &p_class_name, const StringName &p_prop_name, const StringName &p_item_name, ThemeItemSetter p_setter) {
	ERR_FAIL_COND_MSG(theme_item_binds[p_class_name].has(p_prop_name), vformat("Failed to bind theme item '%s' in class '%s': already bound", p_prop_name, p_class_name));

	ThemeItemBind bind;
	bind.data_type = p_data_type;
	bind.class_name = p_class_name;
	bind.item_name = p_item_name;
	bind.setter = p_setter;

	theme_item_binds[p_class_name][p_prop_name] = bind;
	theme_item_binds_list[p_class_name].push_back(bind);
}

void ThemeDB::bind_class_external_item(Theme::DataType p_data_type, const StringName &p_class_name, const StringName &p_prop_name, const StringName &p_item_name, const StringName &p_type_name, ThemeItemSetter p_setter) {
	ERR_FAIL_COND_MSG(theme_item_binds[p_class_name].has(p_prop_name), vformat("Failed to bind theme item '%s' in class '%s': already bound", p_prop_name, p_class_name));

	ThemeItemBind bind;
	bind.data_type = p_data_type;
	bind.class_name = p_class_name;
	bind.item_name = p_item_name;
	bind.type_name = p_type_name;
	bind.external = true;
	bind.setter = p_setter;

	theme_item_binds[p_class_name][p_prop_name] = bind;
	theme_item_binds_list[p_class_name].push_back(bind);
}

void ThemeDB::update_class_instance_items(Node *p_instance) {
	ERR_FAIL_NULL(p_instance);

	// Use the hierarchy to initialize all inherited theme caches. Setters carry the necessary
	// context and will set the values appropriately.
	StringName class_name = p_instance->get_class();
	while (class_name != StringName()) {
		HashMap<StringName, HashMap<StringName, ThemeItemBind>>::Iterator E = theme_item_binds.find(class_name);
		if (E) {
			for (const KeyValue<StringName, ThemeItemBind> &F : E->value) {
				F.value.setter(p_instance, F.value.item_name, F.value.type_name);
			}
		}

		class_name = ClassDB::get_parent_class_nocheck(class_name);
	}
}

void ThemeDB::get_class_items(const StringName &p_class_name, List<ThemeItemBind> *r_list, bool p_include_inherited, Theme::DataType p_filter_type) {
	List<StringName> class_hierarchy;
	StringName class_name = p_class_name;
	while (class_name != StringName()) {
		class_hierarchy.push_front(class_name); // Put parent classes in front.
		class_name = ClassDB::get_parent_class_nocheck(class_name);
	}

	HashSet<StringName> inherited_props;
	for (const StringName &theme_type : class_hierarchy) {
		HashMap<StringName, List<ThemeItemBind>>::Iterator E = theme_item_binds_list.find(theme_type);
		if (E) {
			for (const ThemeItemBind &F : E->value) {
				if (p_filter_type != Theme::DATA_TYPE_MAX && F.data_type != p_filter_type) {
					continue;
				}
				if (inherited_props.has(F.item_name)) {
					continue; // Skip inherited properties.
				}
				if (F.external || F.class_name != p_class_name) {
					inherited_props.insert(F.item_name);

					if (!p_include_inherited) {
						continue; // Track properties defined in parent classes, and skip them.
					}
				}

				r_list->push_back(F);
			}
		}
	}
}

void ThemeDB::_sort_theme_items() {
	for (KeyValue<StringName, List<ThemeDB::ThemeItemBind>> &E : theme_item_binds_list) {
		E.value.sort_custom<ThemeItemBind::SortByType>();
	}
}

void ThemeDB::_update_project_settings() {
	String project_theme_path = GLOBAL_GET("gui/theme/custom");
	String project_font_path = GLOBAL_GET("gui/theme/custom_font");

	Ref<Theme> new_theme;
	if (!project_theme_path.is_empty()) {
		new_theme = ResourceLoader::load(project_theme_path);
	}
	set_project_theme(new_theme);

	Ref<Font> new_font;
	if (!project_font_path.is_empty()) {
		new_font = ResourceLoader::load(project_font_path);
	}
	set_fallback_font(new_font);
}

// Object methods.

void ThemeDB::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_default_theme"), &ThemeDB::get_default_theme);
	ClassDB::bind_method(D_METHOD("get_project_theme"), &ThemeDB::get_project_theme);

	ClassDB::bind_method(D_METHOD("set_fallback_base_scale", "base_scale"), &ThemeDB::set_fallback_base_scale);
	ClassDB::bind_method(D_METHOD("get_fallback_base_scale"), &ThemeDB::get_fallback_base_scale);
	ClassDB::bind_method(D_METHOD("set_fallback_font", "font"), &ThemeDB::set_fallback_font);
	ClassDB::bind_method(D_METHOD("get_fallback_font"), &ThemeDB::get_fallback_font);
	ClassDB::bind_method(D_METHOD("set_fallback_font_size", "font_size"), &ThemeDB::set_fallback_font_size);
	ClassDB::bind_method(D_METHOD("get_fallback_font_size"), &ThemeDB::get_fallback_font_size);
	ClassDB::bind_method(D_METHOD("set_fallback_icon", "icon"), &ThemeDB::set_fallback_icon);
	ClassDB::bind_method(D_METHOD("get_fallback_icon"), &ThemeDB::get_fallback_icon);
	ClassDB::bind_method(D_METHOD("set_fallback_stylebox", "stylebox"), &ThemeDB::set_fallback_stylebox);
	ClassDB::bind_method(D_METHOD("get_fallback_stylebox"), &ThemeDB::get_fallback_stylebox);

	ADD_GROUP("Fallback values", "fallback_");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fallback_base_scale", PROPERTY_HINT_RANGE, "0.0,2.0,0.01,or_greater"), "set_fallback_base_scale", "get_fallback_base_scale");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_font", PROPERTY_HINT_RESOURCE_TYPE, "Font", PROPERTY_USAGE_NONE), "set_fallback_font", "get_fallback_font");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "fallback_font_size", PROPERTY_HINT_RANGE, "0,256,1,or_greater,suffix:px"), "set_fallback_font_size", "get_fallback_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_icon", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_NONE), "set_fallback_icon", "get_fallback_icon");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "fallback_stylebox", PROPERTY_HINT_RESOURCE_TYPE, "StyleBox", PROPERTY_USAGE_NONE), "set_fallback_stylebox", "get_fallback_stylebox");

	ADD_SIGNAL(MethodInfo("fallback_changed"));
}

// Memory management, reference, and initialization.

ThemeDB *ThemeDB::singleton = nullptr;

ThemeDB *ThemeDB::get_singleton() {
	return singleton;
}

ThemeDB::ThemeDB() {
	singleton = this;
	if (MessageQueue::get_singleton()) { // May not exist in tests etc.
		callable_mp(this, &ThemeDB::_sort_theme_items).call_deferred();
	}
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &ThemeDB::_update_project_settings));
}

ThemeDB::~ThemeDB() {
	// For technical reasons unit tests recreate and destroy the default
	// theme over and over again. Make sure that finalize_theme() also
	// frees any objects that can be recreated by initialize_theme*().

	_finalize_theme_contexts();

	default_theme.unref();
	project_theme.unref();

	fallback_font.unref();
	fallback_icon.unref();
	fallback_stylebox.unref();

	singleton = nullptr;
}

void ThemeContext::_emit_changed() {
	emit_signal(CoreStringName(changed));
}

void ThemeContext::set_themes(Vector<Ref<Theme>> &p_themes) {
	for (const Ref<Theme> &theme : themes) {
		theme->disconnect_changed(callable_mp(this, &ThemeContext::_emit_changed));
	}

	themes.clear();

	for (const Ref<Theme> &theme : p_themes) {
		if (theme.is_null()) {
			continue;
		}

		themes.push_back(theme);
		theme->connect_changed(callable_mp(this, &ThemeContext::_emit_changed));
	}

	_emit_changed();
}

const Vector<Ref<Theme>> ThemeContext::get_themes() const {
	return themes;
}

Ref<Theme> ThemeContext::get_fallback_theme() const {
	// We expect all contexts to be valid and non-empty, but just in case...
	if (themes.size() == 0) {
		return ThemeDB::get_singleton()->get_default_theme();
	}

	return themes[themes.size() - 1];
}

void ThemeContext::_bind_methods() {
	ADD_SIGNAL(MethodInfo("changed"));
}
