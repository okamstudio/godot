/**************************************************************************/
/*  asset_library_editor_plugin.h                                         */
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

#include "editor/editor_asset_installer.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/box_container.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/link_button.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/http_request.h"

class EditorFileDialog;
class MenuButton;

class EditorAssetLibraryItem : public PanelContainer {
	GDCLASS(EditorAssetLibraryItem, PanelContainer);

	TextureButton *icon = nullptr;
	LinkButton *title = nullptr;
	LinkButton *category = nullptr;
	LinkButton *author = nullptr;
	Label *price = nullptr;

	String title_text;
	int asset_id = 0;
	int category_id = 0;
	int author_id = 0;

	void _asset_clicked();
	void _category_clicked();
	void _author_clicked();

	void set_image(int p_type, int p_index, const Ref<Texture2D> &p_image);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void configure(const String &p_title, int p_asset_id, const String &p_category, int p_category_id, const String &p_author, int p_author_id, const String &p_cost);

	void clamp_width(int p_max_width);

	EditorAssetLibraryItem(bool p_clickable = false);
};

class EditorAssetLibraryItemDescription : public ConfirmationDialog {
	GDCLASS(EditorAssetLibraryItemDescription, ConfirmationDialog);

	EditorAssetLibraryItem *item = nullptr;
	RichTextLabel *description = nullptr;
	VBoxContainer *previews_vbox = nullptr;
	ScrollContainer *previews = nullptr;
	HBoxContainer *preview_hb = nullptr;
	PanelContainer *previews_bg = nullptr;

	struct Preview {
		int id = 0;
		bool is_video = false;
		String video_link;
		Button *button = nullptr;
		Ref<Texture2D> image;
	};

	Vector<Preview> preview_images;
	TextureRect *preview = nullptr;

	void set_image(int p_type, int p_index, const Ref<Texture2D> &p_image);

	int asset_id = 0;
	String download_url;
	String title;
	String sha256;
	Ref<Texture2D> icon;

	void _link_click(const String &p_url);
	void _preview_click(int p_id);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void configure(const String &p_title, int p_asset_id, const String &p_category, int p_category_id, const String &p_author, int p_author_id, const String &p_cost, int p_version, const String &p_version_string, const String &p_description, const String &p_download_url, const String &p_browse_url, const String &p_sha256_hash);
	void add_preview(int p_id, bool p_video, const String &p_url);

	String get_title() { return title; }
	Ref<Texture2D> get_preview_icon() { return icon; }
	String get_download_url() { return download_url; }
	int get_asset_id() { return asset_id; }
	String get_sha256() { return sha256; }
	EditorAssetLibraryItemDescription();
};

class EditorAssetLibraryItemDownload : public MarginContainer {
	GDCLASS(EditorAssetLibraryItemDownload, MarginContainer);

	PanelContainer *panel = nullptr;
	TextureRect *icon = nullptr;
	Label *title = nullptr;
	ProgressBar *progress = nullptr;
	Button *install_button = nullptr;
	Button *retry_button = nullptr;
	TextureButton *dismiss_button = nullptr;

	AcceptDialog *download_error = nullptr;
	HTTPRequest *download = nullptr;
	String host;
	String sha256;
	Label *status = nullptr;

	int prev_status;

	int asset_id = 0;

	bool external_install;

	EditorAssetInstaller *asset_installer = nullptr;

	void _close();
	void _make_request();
	void _http_download_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_external_install(bool p_enable) { external_install = p_enable; }
	int get_asset_id() { return asset_id; }
	void configure(const String &p_title, int p_asset_id, const Ref<Texture2D> &p_preview, const String &p_download_url, const String &p_sha256_hash);

	bool can_install() const;
	void install();

	EditorAssetLibraryItemDownload();
};

class EditorAssetLibrary : public PanelContainer {
	GDCLASS(EditorAssetLibrary, PanelContainer);

	String host;

	EditorFileDialog *asset_open = nullptr;
	EditorAssetInstaller *asset_installer = nullptr;

	void _asset_open();
	void _asset_file_selected(const String &p_file);
	void _update_repository_options();

	PanelContainer *library_scroll_bg = nullptr;
	ScrollContainer *library_scroll = nullptr;
	VBoxContainer *library_vb = nullptr;
	VBoxContainer *library_message_box = nullptr;
	Label *library_message = nullptr;
	Button *library_message_button = nullptr;
	Callable library_message_action;

	void _set_library_message(const String &p_message);
	void _set_library_message_with_action(const String &p_message, const String &p_action_text, const Callable &p_action);

	LineEdit *filter = nullptr;
	Timer *filter_debounce_timer = nullptr;
	OptionButton *categories = nullptr;
	OptionButton *repository = nullptr;
	OptionButton *sort = nullptr;
	HBoxContainer *error_hb = nullptr;
	TextureRect *error_tr = nullptr;
	Label *error_label = nullptr;
	MenuButton *support = nullptr;

	HBoxContainer *contents = nullptr;

	HBoxContainer *asset_top_page = nullptr;
	GridContainer *asset_items = nullptr;
	HBoxContainer *asset_bottom_page = nullptr;

	HTTPRequest *request = nullptr;

	bool templates_only = false;
	bool initial_loading = true;
	bool loading_blocked = false;

	void _force_online_mode();

	enum Support {
		SUPPORT_FEATURED,
		SUPPORT_COMMUNITY,
		SUPPORT_TESTING,
		SUPPORT_MAX
	};

	enum SortOrder {
		SORT_UPDATED,
		SORT_UPDATED_REVERSE,
		SORT_NAME,
		SORT_NAME_REVERSE,
		SORT_COST,
		SORT_COST_REVERSE,
		SORT_MAX
	};

	static const char *sort_key[SORT_MAX];
	static const char *sort_text[SORT_MAX];
	static const char *support_key[SUPPORT_MAX];
	static const char *support_text[SUPPORT_MAX];

	///MainListing

	enum ImageType {
		IMAGE_QUEUE_ICON,
		IMAGE_QUEUE_THUMBNAIL,
		IMAGE_QUEUE_SCREENSHOT,

	};

	struct ImageQueue {
		bool active = false;
		int queue_id = 0;
		ImageType image_type = ImageType::IMAGE_QUEUE_ICON;
		int image_index = 0;
		String image_url;
		HTTPRequest *request = nullptr;
		ObjectID target;
		int asset_id = -1;
	};

	int last_queue_id;
	HashMap<int, ImageQueue> image_queue;

	void _image_update(bool p_use_cache, bool p_final, const PackedByteArray &p_data, int p_queue_id);
	void _image_request_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data, int p_queue_id);
	void _request_image(ObjectID p_for, int p_asset_id, String p_image_url, ImageType p_type, int p_image_index);
	void _update_image_queue();

	HBoxContainer *_make_pages(int p_page, int p_page_count, int p_page_len, int p_total_items, int p_current_items);

	//
	EditorAssetLibraryItemDescription *description = nullptr;
	//

	enum RequestType {
		REQUESTING_NONE,
		REQUESTING_CONFIG,
		REQUESTING_SEARCH,
		REQUESTING_ASSET,
	};

	RequestType requesting;
	Dictionary category_map;

	ScrollContainer *downloads_scroll = nullptr;
	HBoxContainer *downloads_hb = nullptr;

	void _install_asset();

	void _select_author(const String &p_author);
	void _select_category(int p_id);
	void _select_asset(int p_id);

	void _manage_plugins();

	void _search(int p_page = 0);
	void _rerun_search(int p_ignore);
	void _search_text_changed(const String &p_text = "");
	void _search_text_submitted(const String &p_text = "");
	void _api_request(const String &p_request, RequestType p_request_type, const String &p_arguments = "");
	void _http_request_completed(int p_status, int p_code, const PackedStringArray &headers, const PackedByteArray &p_data);
	void _filter_debounce_timer_timeout();
	void _request_current_config();
	EditorAssetLibraryItemDownload *_get_asset_in_progress(int p_asset_id) const;

	void _repository_changed(int p_repository_id);
	void _support_toggled(int p_support);

	void _install_external_asset(String p_zip_path, String p_title);

	int asset_items_column_width = 0;

	void _update_asset_items_columns();

	friend class EditorAssetLibraryItemDescription;
	friend class EditorAssetLibraryItem;

protected:
	static void _bind_methods();
	void _notification(int p_what);
	virtual void shortcut_input(const Ref<InputEvent> &p_event) override;

public:
	void disable_community_support();

	EditorAssetLibrary(bool p_templates_only = false);
};

class AssetLibraryEditorPlugin : public EditorPlugin {
	GDCLASS(AssetLibraryEditorPlugin, EditorPlugin);

	EditorAssetLibrary *addon_library = nullptr;

public:
	static bool is_available();

	virtual String get_plugin_name() const override { return "AssetLib"; }
	bool has_main_screen() const override { return true; }
	virtual void edit(Object *p_object) override {}
	virtual bool handles(Object *p_object) const override { return false; }
	virtual void make_visible(bool p_visible) override;
	//virtual bool get_remove_list(List<Node*> *p_list) { return canvas_item_editor->get_remove_list(p_list); }
	//virtual Dictionary get_state() const;
	//virtual void set_state(const Dictionary& p_state);

	AssetLibraryEditorPlugin();
	~AssetLibraryEditorPlugin();
};
