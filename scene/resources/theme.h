/**************************************************************************/
/*  theme.h                                                               */
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

#include "core/io/resource.h"
#include "scene/resources/font.h"
#include "scene/resources/style_box.h"
#include "scene/resources/texture.h"

class Theme : public Resource {
	GDCLASS(Theme, Resource);
	RES_BASE_EXTENSION("theme");

#ifdef TOOLS_ENABLED
	friend class ThemeItemImportTree;
	friend class ThemeItemEditorDialog;
	friend class ThemeTypeEditor;
#endif

public:
	using ThemeIconMap = HashMap<StringName, Ref<Texture2D>>;
	using ThemeStyleMap = HashMap<StringName, Ref<StyleBox>>;
	using ThemeFontMap = HashMap<StringName, Ref<Font>>;
	using ThemeFontSizeMap = HashMap<StringName, int>;
	using ThemeColorMap = HashMap<StringName, Color>;
	using ThemeConstantMap = HashMap<StringName, int>;

	enum DataType {
		DATA_TYPE_COLOR,
		DATA_TYPE_CONSTANT,
		DATA_TYPE_FONT,
		DATA_TYPE_FONT_SIZE,
		DATA_TYPE_ICON,
		DATA_TYPE_STYLEBOX,
		DATA_TYPE_MAX
	};

private:
	bool no_change_propagation = false;

	void _emit_theme_changed(bool p_notify_list_changed = false);

	Vector<String> _get_icon_list(const String &p_theme_type) const;
	Vector<String> _get_icon_type_list() const;
	Vector<String> _get_stylebox_list(const String &p_theme_type) const;
	Vector<String> _get_stylebox_type_list() const;
	Vector<String> _get_font_list(const String &p_theme_type) const;
	Vector<String> _get_font_type_list() const;
	Vector<String> _get_font_size_list(const String &p_theme_type) const;
	Vector<String> _get_font_size_type_list() const;
	Vector<String> _get_color_list(const String &p_theme_type) const;
	Vector<String> _get_color_type_list() const;
	Vector<String> _get_constant_list(const String &p_theme_type) const;
	Vector<String> _get_constant_type_list() const;

	Vector<String> _get_theme_item_list(DataType p_data_type, const String &p_theme_type) const;
	Vector<String> _get_theme_item_type_list(DataType p_data_type) const;

	Vector<String> _get_type_variation_list(const StringName &p_theme_type) const;
	Vector<String> _get_type_list() const;

protected:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	// Default values configurable for each individual theme.
	float default_base_scale = 0.0;
	Ref<Font> default_font;
	int default_font_size = -1;

	HashMap<StringName, ThemeIconMap> icon_map;
	HashMap<StringName, ThemeStyleMap> style_map;
	HashMap<StringName, ThemeFontMap> font_map;
	HashMap<StringName, ThemeFontSizeMap> font_size_map;
	HashMap<StringName, ThemeColorMap> color_map;
	HashMap<StringName, ThemeConstantMap> constant_map;
	HashMap<StringName, StringName> variation_map;
	HashMap<StringName, List<StringName>> variation_base_map;

	static void _bind_methods();

	void _freeze_change_propagation();
	void _unfreeze_and_propagate_changes();

	virtual void reset_state() override;

public:
	static bool is_valid_type_name(const String &p_name);
	static bool is_valid_item_name(const String &p_name);

	void set_default_base_scale(float p_base_scale);
	float get_default_base_scale() const;
	bool has_default_base_scale() const;

	void set_default_font(const Ref<Font> &p_default_font);
	Ref<Font> get_default_font() const;
	bool has_default_font() const;

	void set_default_font_size(int p_font_size);
	int get_default_font_size() const;
	bool has_default_font_size() const;

	void set_icon(const StringName &p_name, const StringName &p_theme_type, const Ref<Texture2D> &p_icon);
	virtual Ref<Texture2D> get_icon(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_icon(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_icon_nocheck(const StringName &p_name, const StringName &p_theme_type) const;
	void rename_icon(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type);
	void clear_icon(const StringName &p_name, const StringName &p_theme_type);
	void get_icon_list(const StringName &p_theme_type, List<StringName> *p_list) const;
	void add_icon_type(const StringName &p_theme_type);
	void remove_icon_type(const StringName &p_theme_type);
	void get_icon_type_list(List<StringName> *p_list) const;

	void set_stylebox(const StringName &p_name, const StringName &p_theme_type, const Ref<StyleBox> &p_style);
	virtual Ref<StyleBox> get_stylebox(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_stylebox(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_stylebox_nocheck(const StringName &p_name, const StringName &p_theme_type) const;
	void rename_stylebox(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type);
	void clear_stylebox(const StringName &p_name, const StringName &p_theme_type);
	void get_stylebox_list(const StringName &p_theme_type, List<StringName> *p_list) const;
	void add_stylebox_type(const StringName &p_theme_type);
	void remove_stylebox_type(const StringName &p_theme_type);
	void get_stylebox_type_list(List<StringName> *p_list) const;

	void set_font(const StringName &p_name, const StringName &p_theme_type, const Ref<Font> &p_font);
	virtual Ref<Font> get_font(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_font(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_font_no_default(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_font_nocheck(const StringName &p_name, const StringName &p_theme_type) const;
	void rename_font(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type);
	void clear_font(const StringName &p_name, const StringName &p_theme_type);
	void get_font_list(const StringName &p_theme_type, List<StringName> *p_list) const;
	void add_font_type(const StringName &p_theme_type);
	void remove_font_type(const StringName &p_theme_type);
	void get_font_type_list(List<StringName> *p_list) const;

	void set_font_size(const StringName &p_name, const StringName &p_theme_type, int p_font_size);
	virtual int get_font_size(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_font_size(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_font_size_no_default(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_font_size_nocheck(const StringName &p_name, const StringName &p_theme_type) const;
	void rename_font_size(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type);
	void clear_font_size(const StringName &p_name, const StringName &p_theme_type);
	void get_font_size_list(const StringName &p_theme_type, List<StringName> *p_list) const;
	void add_font_size_type(const StringName &p_theme_type);
	void remove_font_size_type(const StringName &p_theme_type);
	void get_font_size_type_list(List<StringName> *p_list) const;

	void set_color(const StringName &p_name, const StringName &p_theme_type, const Color &p_color);
	virtual Color get_color(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_color(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_color_nocheck(const StringName &p_name, const StringName &p_theme_type) const;
	void rename_color(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type);
	void clear_color(const StringName &p_name, const StringName &p_theme_type);
	void get_color_list(const StringName &p_theme_type, List<StringName> *p_list) const;
	void add_color_type(const StringName &p_theme_type);
	void remove_color_type(const StringName &p_theme_type);
	void get_color_type_list(List<StringName> *p_list) const;

	void set_constant(const StringName &p_name, const StringName &p_theme_type, int p_constant);
	virtual int get_constant(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_constant(const StringName &p_name, const StringName &p_theme_type) const;
	bool has_constant_nocheck(const StringName &p_name, const StringName &p_theme_type) const;
	void rename_constant(const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type);
	void clear_constant(const StringName &p_name, const StringName &p_theme_type);
	void get_constant_list(const StringName &p_theme_type, List<StringName> *p_list) const;
	void add_constant_type(const StringName &p_theme_type);
	void remove_constant_type(const StringName &p_theme_type);
	void get_constant_type_list(List<StringName> *p_list) const;

	void set_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type, const Variant &p_value);
	Variant get_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const;
	bool has_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const;
	bool has_theme_item_nocheck(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type) const;
	void rename_theme_item(DataType p_data_type, const StringName &p_old_name, const StringName &p_name, const StringName &p_theme_type);
	void clear_theme_item(DataType p_data_type, const StringName &p_name, const StringName &p_theme_type);
	void get_theme_item_list(DataType p_data_type, const StringName &p_theme_type, List<StringName> *p_list) const;
	void add_theme_item_type(DataType p_data_type, const StringName &p_theme_type);
	void remove_theme_item_type(DataType p_data_type, const StringName &p_theme_type);
	void get_theme_item_type_list(DataType p_data_type, List<StringName> *p_list) const;

	void set_type_variation(const StringName &p_theme_type, const StringName &p_base_type);
	bool is_type_variation(const StringName &p_theme_type, const StringName &p_base_type) const;
	void clear_type_variation(const StringName &p_theme_type);
	StringName get_type_variation_base(const StringName &p_theme_type) const;
	void get_type_variation_list(const StringName &p_base_type, List<StringName> *p_list) const;

	void add_type(const StringName &p_theme_type);
	void remove_type(const StringName &p_theme_type);
	void get_type_list(List<StringName> *p_list) const;
	void get_type_dependencies(const StringName &p_base_type, const StringName &p_type_variant, Vector<StringName> &r_result);

	void merge_with(const Ref<Theme> &p_other);
	void clear();

	Theme();
	~Theme();
};

VARIANT_ENUM_CAST(Theme::DataType);
