/**************************************************************************/
/*  dynamic_font_import_settings.h                                        */
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

#ifndef DYNAMIC_FONT_IMPORT_SETTINGS_H
#define DYNAMIC_FONT_IMPORT_SETTINGS_H

#include "core/io/resource_importer.h"

#include "scene/gui/dialogs.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/tree.h"
#include "scene/resources/font.h"

class DynamicFontImportSettingsDialog;

class DynamicFontImportSettingsData : public RefCounted {
	GDCLASS(DynamicFontImportSettingsData, RefCounted)
	friend class DynamicFontImportSettingsDialog;

	HashMap<StringName, Variant> settings;
	HashMap<StringName, Variant> defaults;
	List<ResourceImporter::ImportOption> options;
	DynamicFontImportSettingsDialog *owner = nullptr;

	HashSet<char32_t> selected_chars;
	HashSet<int32_t> selected_glyphs;

	Ref<FontFile> fd;

public:
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

	Ref<FontFile> get_font() const;
};

class EditorFileDialog;
class EditorInspector;
class EditorLocaleDialog;

class DynamicFontImportSettingsDialog : public ConfirmationDialog {
	GDCLASS(DynamicFontImportSettingsDialog, ConfirmationDialog)
	friend class DynamicFontImportSettingsData;

	enum ItemButton {
		BUTTON_ADD_VAR,
		BUTTON_REMOVE_VAR,
	};

	static DynamicFontImportSettingsDialog *singleton;

	String base_path;

	Ref<DynamicFontImportSettingsData> import_settings_data;
	List<ResourceImporter::ImportOption> options_variations;
	List<ResourceImporter::ImportOption> options_general;

	// Root layout
	Label *label_warn = nullptr;
	TabContainer *main_pages = nullptr;

	// Page 1 layout: Rendering Options
	Label *page1_description = nullptr;
	Label *font_name_label = nullptr;
	Label *font_preview_label = nullptr;
	EditorInspector *inspector_general = nullptr;

	void _main_prop_changed(const String &p_edited_property);

	// Page 2 layout: Preload Configurations
	Label *page2_description = nullptr;
	Label *label_vars = nullptr;
	Button *add_var = nullptr;
	Tree *vars_list = nullptr;
	TreeItem *vars_list_root = nullptr;
	EditorInspector *inspector_vars = nullptr;

	void _variation_add();
	void _variation_selected();
	void _variation_remove(Object *p_item, int p_column, int p_id, MouseButton p_button);
	void _variation_changed(const String &p_edited_property);
	void _variations_validate();

	TabContainer *preload_pages = nullptr;

	Label *label_glyphs = nullptr;
	void _glyph_clear();
	void _glyph_update_lbl();

	// Page 2.0 layout: Translations
	Label *page2_0_description = nullptr;
	Tree *locale_tree = nullptr;
	TreeItem *locale_root = nullptr;
	Button *btn_fill_locales = nullptr;

	void _locale_edited();
	void _process_locales();

	// Page 2.1 layout: Text to select glyphs
	Label *page2_1_description = nullptr;
	TextEdit *text_edit = nullptr;
	EditorInspector *inspector_text = nullptr;
	Button *btn_fill = nullptr;

	List<ResourceImporter::ImportOption> options_text;
	Ref<DynamicFontImportSettingsData> text_settings_data;

	void _change_text_opts();
	void _glyph_text_selected();

	// Page 2.2 layout: Character map
	Label *page2_2_description = nullptr;
	Tree *glyph_table = nullptr;
	Tree *glyph_tree = nullptr;
	TreeItem *glyph_root = nullptr;

	void _glyph_selected();
	void _range_edited();
	void _range_selected();
	void _edit_range(int32_t p_start, int32_t p_end);
	bool _char_update(int32_t p_char);
	void _range_update(int32_t p_start, int32_t p_end);

	// Common

	void _add_glyph_range_item(int32_t p_start, int32_t p_end, const String &p_name);

	Ref<FontFile> font_preview;
	Ref<FontFile> font_main;

	void _re_import();

	String _pad_zeros(const String &p_hex) const;

protected:
	void _notification(int p_what);

public:
	void open_settings(const String &p_path);
	static DynamicFontImportSettingsDialog *get_singleton();

	DynamicFontImportSettingsDialog();
};

#endif // DYNAMIC_FONT_IMPORT_SETTINGS_H
