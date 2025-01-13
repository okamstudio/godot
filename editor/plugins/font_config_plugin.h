/**************************************************************************/
/*  font_config_plugin.h                                                  */
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

#ifndef FONT_CONFIG_PLUGIN_H
#define FONT_CONFIG_PLUGIN_H

#include "editor/editor_properties.h"
#include "editor/editor_properties_array_dict.h"
#include "editor/plugins/editor_plugin.h"

/*************************************************************************/

class EditorPropertyFontMetaObject : public RefCounted {
	GDCLASS(EditorPropertyFontMetaObject, RefCounted);

	Dictionary dict;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;

public:
	void set_dict(const Dictionary &p_dict);
	Dictionary get_dict();

	EditorPropertyFontMetaObject() {}
};

/*************************************************************************/

class EditorPropertyFontOTObject : public RefCounted {
	GDCLASS(EditorPropertyFontOTObject, RefCounted);

	Dictionary dict;
	Dictionary defaults_dict;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	bool _property_can_revert(const StringName &p_name) const;
	bool _property_get_revert(const StringName &p_name, Variant &r_property) const;

public:
	void set_dict(const Dictionary &p_dict);
	Dictionary get_dict();

	void set_defaults(const Dictionary &p_dict);
	Dictionary get_defaults();

	EditorPropertyFontOTObject() {}
};

/*************************************************************************/

class EditorPropertyFontMetaOverride : public EditorProperty {
	GDCLASS(EditorPropertyFontMetaOverride, EditorProperty);

	Ref<EditorPropertyFontMetaObject> object;

	MarginContainer *container = nullptr;
	VBoxContainer *property_vbox = nullptr;

	Button *button_add = nullptr;
	Button *edit = nullptr;
	PopupMenu *menu = nullptr;
	EditorLocaleDialog *locale_select = nullptr;

	Vector<String> script_codes;

	bool script_editor = false;
	bool updating = false;
	int page_length = 20;
	int page_index = 0;
	EditorPaginator *paginator = nullptr;

protected:
	void _notification(int p_what);
	static void _bind_methods() {}

	void _edit_pressed();
	void _page_changed(int p_page);
	void _property_changed(const String &p_property, const Variant &p_value, const String &p_name = "", bool p_changing = false);
	void _remove(Object *p_button, const String &p_key);
	void _add_menu();
	void _add_script(int p_option);
	void _add_lang(const String &p_locale);
	void _object_id_selected(const StringName &p_property, ObjectID p_id);

public:
	virtual void update_property() override;

	EditorPropertyFontMetaOverride(bool p_script);
};

/*************************************************************************/

class EditorPropertyOTVariation : public EditorProperty {
	GDCLASS(EditorPropertyOTVariation, EditorProperty);

	Ref<EditorPropertyFontOTObject> object;

	MarginContainer *container = nullptr;
	VBoxContainer *property_vbox = nullptr;

	Button *edit = nullptr;

	bool updating = false;
	int page_length = 20;
	int page_index = 0;
	EditorPaginator *paginator = nullptr;

protected:
	static void _bind_methods() {}

	void _edit_pressed();
	void _page_changed(int p_page);
	void _property_changed(const String &p_property, const Variant &p_value, const String &p_name = "", bool p_changing = false);
	void _object_id_selected(const StringName &p_property, ObjectID p_id);

public:
	virtual void update_property() override;

	EditorPropertyOTVariation();
};

/*************************************************************************/

class EditorPropertyOTFeatures : public EditorProperty {
	GDCLASS(EditorPropertyOTFeatures, EditorProperty);

	enum FeatureGroups {
		FGRP_STYLISTIC_SET,
		FGRP_CHARACTER_VARIANT,
		FGRP_CAPITLS,
		FGRP_LIGATURES,
		FGRP_ALTERNATES,
		FGRP_EAL,
		FGRP_EAW,
		FGRP_NUMAL,
		FGRP_CUSTOM,
		FGRP_MAX,
	};

	Ref<EditorPropertyFontOTObject> object;

	MarginContainer *container = nullptr;
	VBoxContainer *property_vbox = nullptr;

	Button *button_add = nullptr;
	Button *edit = nullptr;
	PopupMenu *menu = nullptr;
	PopupMenu *menu_sub[FGRP_MAX];
	String group_names[FGRP_MAX];

	bool updating = false;
	int page_length = 20;
	int page_index = 0;
	EditorPaginator *paginator = nullptr;

protected:
	void _notification(int p_what);
	static void _bind_methods() {}

	void _edit_pressed();
	void _page_changed(int p_page);
	void _property_changed(const String &p_property, const Variant &p_value, const String &p_name = "", bool p_changing = false);
	void _remove(Object *p_button, int p_key);
	void _add_menu();
	void _add_feature(int p_option);
	void _object_id_selected(const StringName &p_property, ObjectID p_id);

public:
	virtual void update_property() override;

	EditorPropertyOTFeatures();
};

/*************************************************************************/

class EditorInspectorPluginFontVariation : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginFontVariation, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

/*************************************************************************/

class FontPreview : public Control {
	GDCLASS(FontPreview, Control);

protected:
	void _notification(int p_what);
	static void _bind_methods();

	Ref<Font> prev_font;

	void _preview_changed();

public:
	virtual Size2 get_minimum_size() const override;

	void set_data(const Ref<Font> &p_f);

	FontPreview();
};

/*************************************************************************/

class EditorInspectorPluginFontPreview : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginFontPreview, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual void parse_begin(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

/*************************************************************************/

class EditorPropertyFontNamesArray : public EditorPropertyArray {
	GDCLASS(EditorPropertyFontNamesArray, EditorPropertyArray);

	PopupMenu *menu = nullptr;

protected:
	virtual void _add_element() override;

	void _add_font(int p_option);
	static void _bind_methods() {}

public:
	EditorPropertyFontNamesArray();
};

/*************************************************************************/

class EditorInspectorPluginSystemFont : public EditorInspectorPlugin {
	GDCLASS(EditorInspectorPluginSystemFont, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override;
};

/*************************************************************************/

class FontEditorPlugin : public EditorPlugin {
	GDCLASS(FontEditorPlugin, EditorPlugin);

public:
	FontEditorPlugin();

	virtual String get_plugin_name() const override { return "Font"; }
};

#endif // FONT_CONFIG_PLUGIN_H
