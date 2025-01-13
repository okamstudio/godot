/**************************************************************************/
/*  project_list.cpp                                                      */
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

#include "project_list.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/os/time.h"
#include "core/version.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/project_manager.h"
#include "editor/project_manager/project_tag.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/image_texture.h"

void ProjectListItemControl::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			if (icon_needs_reload) {
				// The project icon may not be loaded by the time the control is displayed,
				// so use a loading placeholder.
				project_icon->set_texture(get_editor_theme_icon(SNAME("ProjectIconLoading")));
			}

			project_title->begin_bulk_theme_override();
			project_title->add_theme_font_override(SceneStringName(font), get_theme_font(SNAME("title"), EditorStringName(EditorFonts)));
			project_title->add_theme_font_size_override(SceneStringName(font_size), get_theme_font_size(SNAME("title_size"), EditorStringName(EditorFonts)));
			project_title->add_theme_color_override(SceneStringName(font_color), get_theme_color(SceneStringName(font_color), SNAME("Tree")));
			project_title->end_bulk_theme_override();

			project_path->add_theme_color_override(SceneStringName(font_color), get_theme_color(SceneStringName(font_color), SNAME("Tree")));
			project_unsupported_features->set_texture(get_editor_theme_icon(SNAME("NodeWarning")));

			favorite_button->set_texture_normal(get_editor_theme_icon(SNAME("Favorites")));
			if (project_is_missing) {
				explore_button->set_button_icon(get_editor_theme_icon(SNAME("FileBroken")));
			} else {
				explore_button->set_button_icon(get_editor_theme_icon(SNAME("Load")));
			}
		} break;

		case NOTIFICATION_MOUSE_ENTER: {
			is_hovering = true;
			queue_redraw();
		} break;

		case NOTIFICATION_MOUSE_EXIT: {
			is_hovering = false;
			queue_redraw();
		} break;

		case NOTIFICATION_DRAW: {
			if (is_selected) {
				draw_style_box(get_theme_stylebox(SNAME("selected"), SNAME("Tree")), Rect2(Point2(), get_size()));
			}
			if (is_hovering) {
				draw_style_box(get_theme_stylebox(SNAME("hovered"), SNAME("Tree")), Rect2(Point2(), get_size()));
			}

			draw_line(Point2(0, get_size().y + 1), Point2(get_size().x, get_size().y + 1), get_theme_color(SNAME("guide_color"), SNAME("Tree")));
		} break;
	}
}

void ProjectListItemControl::_favorite_button_pressed() {
	emit_signal(SNAME("favorite_pressed"));
}

void ProjectListItemControl::_explore_button_pressed() {
	emit_signal(SNAME("explore_pressed"));
}

void ProjectListItemControl::set_project_title(const String &p_title) {
	project_title->set_text(p_title);
}

void ProjectListItemControl::set_project_path(const String &p_path) {
	project_path->set_text(p_path);
}

void ProjectListItemControl::set_tags(const PackedStringArray &p_tags, ProjectList *p_parent_list) {
	for (const String &tag : p_tags) {
		ProjectTag *tag_control = memnew(ProjectTag(tag));
		tag_container->add_child(tag_control);
		tag_control->connect_button_to(callable_mp(p_parent_list, &ProjectList::add_search_tag).bind(tag));
	}
}

void ProjectListItemControl::set_project_icon(const Ref<Texture2D> &p_icon) {
	icon_needs_reload = false;

	// The default project icon is 128×128 to look crisp on hiDPI displays,
	// but we want the actual displayed size to be 64×64 on loDPI displays.
	project_icon->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	project_icon->set_custom_minimum_size(Size2(64, 64) * EDSCALE);
	project_icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);

	project_icon->set_texture(p_icon);
}

void ProjectListItemControl::set_last_edited_info(const String &p_info) {
	last_edited_info->set_text(p_info);
}

void ProjectListItemControl::set_project_version(const String &p_info) {
	project_version->set_text(p_info);
}

void ProjectListItemControl::set_unsupported_features(PackedStringArray p_features) {
	if (p_features.size() > 0) {
		String tooltip_text = "";
		for (int i = 0; i < p_features.size(); i++) {
			if (ProjectList::project_feature_looks_like_version(p_features[i])) {
				PackedStringArray project_version_split = p_features[i].split(".");
				int project_version_major = 0, project_version_minor = 0;
				if (project_version_split.size() >= 2) {
					project_version_major = project_version_split[0].to_int();
					project_version_minor = project_version_split[1].to_int();
				}
				if (VERSION_MAJOR != project_version_major || VERSION_MINOR <= project_version_minor) {
					// Don't show a warning if the project was last edited in a previous minor version.
					tooltip_text += TTR("This project was last edited in a different Godot version: ") + p_features[i] + "\n";
				}
				p_features.remove_at(i);
				i--;
			}
		}
		if (p_features.size() > 0) {
			String unsupported_features_str = String(", ").join(p_features);
			tooltip_text += TTR("This project uses features unsupported by the current build:") + "\n" + unsupported_features_str;
		}
		if (tooltip_text.is_empty()) {
			return;
		}
		project_version->set_tooltip_text(tooltip_text);
		project_unsupported_features->set_tooltip_text(tooltip_text);
		project_unsupported_features->show();
	} else {
		project_unsupported_features->hide();
	}
}

bool ProjectListItemControl::should_load_project_icon() const {
	return icon_needs_reload;
}

void ProjectListItemControl::set_selected(bool p_selected) {
	is_selected = p_selected;
	queue_redraw();
}

void ProjectListItemControl::set_is_favorite(bool p_favorite) {
	favorite_button->set_modulate(p_favorite ? Color(1, 1, 1, 1) : Color(1, 1, 1, 0.2));
}

void ProjectListItemControl::set_is_missing(bool p_missing) {
	if (project_is_missing == p_missing) {
		return;
	}
	project_is_missing = p_missing;

	if (project_is_missing) {
		project_icon->set_modulate(Color(1, 1, 1, 0.5));

		explore_button->set_button_icon(get_editor_theme_icon(SNAME("FileBroken")));
		explore_button->set_tooltip_text(TTR("Error: Project is missing on the filesystem."));
	} else {
		project_icon->set_modulate(Color(1, 1, 1, 1.0));

		explore_button->set_button_icon(get_editor_theme_icon(SNAME("Load")));
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
		explore_button->set_tooltip_text(TTR("Show in File Manager"));
#else
		// Opening the system file manager is not supported on the Android and web editors.
		explore_button->hide();
#endif
	}
}

void ProjectListItemControl::set_is_grayed(bool p_grayed) {
	if (p_grayed) {
		main_vbox->set_modulate(Color(1, 1, 1, 0.5));
		// Don't make the icon less prominent if the parent is already grayed out.
		explore_button->set_modulate(Color(1, 1, 1, 1.0));
	} else {
		main_vbox->set_modulate(Color(1, 1, 1, 1.0));
		explore_button->set_modulate(Color(1, 1, 1, 0.5));
	}
}

void ProjectListItemControl::_bind_methods() {
	ADD_SIGNAL(MethodInfo("favorite_pressed"));
	ADD_SIGNAL(MethodInfo("explore_pressed"));
}

ProjectListItemControl::ProjectListItemControl() {
	set_focus_mode(FocusMode::FOCUS_ALL);

	VBoxContainer *favorite_box = memnew(VBoxContainer);
	favorite_box->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	add_child(favorite_box);

	favorite_button = memnew(TextureButton);
	favorite_button->set_name("FavoriteButton");
	// This makes the project's "hover" style display correctly when hovering the favorite icon.
	favorite_button->set_mouse_filter(MOUSE_FILTER_PASS);
	favorite_box->add_child(favorite_button);
	favorite_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectListItemControl::_favorite_button_pressed));

	project_icon = memnew(TextureRect);
	project_icon->set_name("ProjectIcon");
	project_icon->set_v_size_flags(SIZE_SHRINK_CENTER);
	add_child(project_icon);

	main_vbox = memnew(VBoxContainer);
	main_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(main_vbox);

	Control *ec = memnew(Control);
	ec->set_custom_minimum_size(Size2(0, 1));
	ec->set_mouse_filter(MOUSE_FILTER_PASS);
	main_vbox->add_child(ec);

	// Top half, title, tags and unsupported features labels.
	{
		HBoxContainer *title_hb = memnew(HBoxContainer);
		main_vbox->add_child(title_hb);

		project_title = memnew(Label);
		project_title->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
		project_title->set_name("ProjectName");
		project_title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		project_title->set_clip_text(true);
		title_hb->add_child(project_title);

		tag_container = memnew(HBoxContainer);
		title_hb->add_child(tag_container);

		Control *spacer = memnew(Control);
		spacer->set_custom_minimum_size(Size2(10, 10));
		title_hb->add_child(spacer);
	}

	// Bottom half, containing the path and view folder button.
	{
		HBoxContainer *path_hb = memnew(HBoxContainer);
		path_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		main_vbox->add_child(path_hb);

		explore_button = memnew(Button);
		explore_button->set_name("ExploreButton");
		explore_button->set_flat(true);
		path_hb->add_child(explore_button);
		explore_button->connect(SceneStringName(pressed), callable_mp(this, &ProjectListItemControl::_explore_button_pressed));

		project_path = memnew(Label);
		project_path->set_name("ProjectPath");
		project_path->set_structured_text_bidi_override(TextServer::STRUCTURED_TEXT_FILE);
		project_path->set_clip_text(true);
		project_path->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		project_path->set_modulate(Color(1, 1, 1, 0.5));
		path_hb->add_child(project_path);

		project_unsupported_features = memnew(TextureRect);
		project_unsupported_features->set_name("ProjectUnsupportedFeatures");
		project_unsupported_features->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		path_hb->add_child(project_unsupported_features);
		project_unsupported_features->hide();

		project_version = memnew(Label);
		project_version->set_name("ProjectVersion");
		project_version->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		path_hb->add_child(project_version);

		last_edited_info = memnew(Label);
		last_edited_info->set_name("LastEditedInfo");
		last_edited_info->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		last_edited_info->set_tooltip_text(TTR("Last edited timestamp"));
		last_edited_info->set_modulate(Color(1, 1, 1, 0.5));
		path_hb->add_child(last_edited_info);

		Control *spacer = memnew(Control);
		spacer->set_custom_minimum_size(Size2(10, 10));
		path_hb->add_child(spacer);
	}
}

struct ProjectListComparator {
	ProjectList::FilterOption order_option = ProjectList::FilterOption::EDIT_DATE;

	// operator<
	_FORCE_INLINE_ bool operator()(const ProjectList::Item &a, const ProjectList::Item &b) const {
		if (a.favorite && !b.favorite) {
			return true;
		}
		if (b.favorite && !a.favorite) {
			return false;
		}
		switch (order_option) {
			case ProjectList::PATH:
				return a.path < b.path;
			case ProjectList::EDIT_DATE:
				return a.last_edited > b.last_edited;
			case ProjectList::TAGS:
				return a.tag_sort_string < b.tag_sort_string;
			default:
				return a.project_name < b.project_name;
		}
	}
};

const char *ProjectList::SIGNAL_LIST_CHANGED = "list_changed";
const char *ProjectList::SIGNAL_SELECTION_CHANGED = "selection_changed";
const char *ProjectList::SIGNAL_PROJECT_ASK_OPEN = "project_ask_open";

// Helpers.

bool ProjectList::project_feature_looks_like_version(const String &p_feature) {
	return p_feature.contains_char('.') && p_feature.substr(0, 3).is_numeric();
}

// Notifications.

void ProjectList::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_PROCESS: {
			// Load icons as a coroutine to speed up launch when you have hundreds of projects.
			if (_icon_load_index < _projects.size()) {
				Item &item = _projects.write[_icon_load_index];
				if (item.control->should_load_project_icon()) {
					_load_project_icon(_icon_load_index);
				}
				_icon_load_index++;

				// Scan directories in thread to avoid blocking the window.
			} else if (scan_data && scan_data->scan_in_progress.is_set()) {
				// Wait for the thread.
			} else {
				set_process(false);
				if (scan_data) {
					_scan_finished();
				}
			}
		} break;
	}
}

// Projects scan.

void ProjectList::_scan_thread(void *p_scan_data) {
	ScanData *scan_data = static_cast<ScanData *>(p_scan_data);

	for (const String &base_path : scan_data->paths_to_scan) {
		print_verbose(vformat("Scanning for projects in \"%s\".", base_path));
		_scan_folder_recursive(base_path, &scan_data->found_projects, scan_data->scan_in_progress);

		if (!scan_data->scan_in_progress.is_set()) {
			print_verbose("Scan aborted.");
			break;
		}
	}
	print_verbose(vformat("Found %d project(s).", scan_data->found_projects.size()));
	scan_data->scan_in_progress.clear();
}

void ProjectList::_scan_finished() {
	if (scan_data->scan_in_progress.is_set()) {
		// Abort scanning.
		scan_data->scan_in_progress.clear();
	}

	scan_data->thread->wait_to_finish();
	memdelete(scan_data->thread);
	if (scan_progress) {
		scan_progress->hide();
	}

	for (const String &E : scan_data->found_projects) {
		add_project(E, false);
	}
	memdelete(scan_data);
	scan_data = nullptr;

	save_config();

	if (ProjectManager::get_singleton()->is_initialized()) {
		update_project_list();
	}
}

// Initialization & loading.

void ProjectList::_migrate_config() {
	// Proposal #1637 moved the project list from editor settings to a separate config file
	// If the new config file doesn't exist, populate it from EditorSettings
	if (FileAccess::exists(_config_path)) {
		return;
	}

	List<PropertyInfo> properties;
	EditorSettings::get_singleton()->get_property_list(&properties);

	for (const PropertyInfo &E : properties) {
		// This is actually something like "projects/C:::Documents::Godot::Projects::MyGame"
		String property_key = E.name;
		if (!property_key.begins_with("projects/")) {
			continue;
		}

		String path = EDITOR_GET(property_key);
		print_line("Migrating legacy project '" + path + "'.");

		String favoriteKey = "favorite_projects/" + property_key.get_slice("/", 1);
		bool favorite = EditorSettings::get_singleton()->has_setting(favoriteKey);
		add_project(path, favorite);
		if (favorite) {
			EditorSettings::get_singleton()->erase(favoriteKey);
		}
		EditorSettings::get_singleton()->erase(property_key);
	}

	save_config();
}

void ProjectList::save_config() {
	_config.save(_config_path);
}

// Load project data from p_property_key and return it in a ProjectList::Item.
// p_favorite is passed directly into the Item.
ProjectList::Item ProjectList::load_project_data(const String &p_path, bool p_favorite) {
	String conf = p_path.path_join("project.godot");
	bool grayed = false;
	bool missing = false;

	Ref<ConfigFile> cf = memnew(ConfigFile);
	Error cf_err = cf->load(conf);

	int config_version = 0;
	String project_name = TTR("Unnamed Project");
	if (cf_err == OK) {
		String cf_project_name = cf->get_value("application", "config/name", "");
		if (!cf_project_name.is_empty()) {
			project_name = cf_project_name.xml_unescape();
		}
		config_version = (int)cf->get_value("", "config_version", 0);
	}

	if (config_version > ProjectSettings::CONFIG_VERSION) {
		// Comes from an incompatible (more recent) Godot version, gray it out.
		grayed = true;
	}

	const String description = cf->get_value("application", "config/description", "");
	const PackedStringArray tags = cf->get_value("application", "config/tags", PackedStringArray());
	const String icon = cf->get_value("application", "config/icon", "");
	const String main_scene = cf->get_value("application", "run/main_scene", "");

	PackedStringArray project_features = cf->get_value("application", "config/features", PackedStringArray());
	PackedStringArray unsupported_features = ProjectSettings::get_unsupported_features(project_features);

	String project_version = "?";
	for (int i = 0; i < project_features.size(); i++) {
		if (ProjectList::project_feature_looks_like_version(project_features[i])) {
			project_version = project_features[i];
			break;
		}
	}

	if (config_version < ProjectSettings::CONFIG_VERSION) {
		// Previous versions may not have unsupported features.
		if (config_version == 4) {
			unsupported_features.push_back("3.x");
			project_version = "3.x";
		} else {
			unsupported_features.push_back("Unknown version");
		}
	}

	uint64_t last_edited = 0;
	if (cf_err == OK) {
		// The modification date marks the date the project was last edited.
		// This is because the `project.godot` file will always be modified
		// when editing a project (but not when running it).
		last_edited = FileAccess::get_modified_time(conf);

		String fscache = p_path.path_join(".fscache");
		if (FileAccess::exists(fscache)) {
			uint64_t cache_modified = FileAccess::get_modified_time(fscache);
			if (cache_modified > last_edited) {
				last_edited = cache_modified;
			}
		}
	} else {
		grayed = true;
		missing = true;
		print_line("Project is missing: " + conf);
	}

	for (const String &tag : tags) {
		ProjectManager::get_singleton()->add_new_tag(tag);
	}

	return Item(project_name, description, project_version, tags, p_path, icon, main_scene, unsupported_features, last_edited, p_favorite, grayed, missing, config_version);
}

void ProjectList::_update_icons_async() {
	_icon_load_index = 0;
	set_process(true);
}

void ProjectList::_load_project_icon(int p_index) {
	Item &item = _projects.write[p_index];

	Ref<Texture2D> default_icon = get_editor_theme_icon(SNAME("DefaultProjectIcon"));
	Ref<Texture2D> icon;
	if (!item.icon.is_empty()) {
		Ref<Image> img;
		img.instantiate();
		Error err = img->load(item.icon.replace_first("res://", item.path + "/"));
		if (err == OK) {
			img->resize(default_icon->get_width(), default_icon->get_height(), Image::INTERPOLATE_LANCZOS);
			icon = ImageTexture::create_from_image(img);
		}
	}
	if (icon.is_null()) {
		icon = default_icon;
	}

	item.control->set_project_icon(icon);
}

// Project list updates.

void ProjectList::update_project_list() {
	// This is a full, hard reload of the list. Don't call this unless really required, it's expensive.
	// If you have 150 projects, it may read through 150 files on your disk at once + load 150 icons.
	// FIXME: Does it really have to be a full, hard reload? Runtime updates should be made much cheaper.

	if (ProjectManager::get_singleton()->is_initialized()) {
		// Clear whole list
		for (int i = 0; i < _projects.size(); ++i) {
			Item &project = _projects.write[i];
			CRASH_COND(project.control == nullptr);
			memdelete(project.control); // Why not queue_free()?
		}

		_projects.clear();
		_last_clicked = "";
		_selected_project_paths.clear();

		load_project_list();
	}

	// Create controls
	for (int i = 0; i < _projects.size(); ++i) {
		_create_project_item_control(i);
	}

	sort_projects();
	_update_icons_async();
	update_dock_menu();

	set_v_scroll(0);
	emit_signal(SNAME(SIGNAL_LIST_CHANGED));
}

void ProjectList::sort_projects() {
	SortArray<Item, ProjectListComparator> sorter;
	sorter.compare.order_option = _order_option;
	sorter.sort(_projects.ptrw(), _projects.size());

	String search_term;
	PackedStringArray tags;

	if (!_search_term.is_empty()) {
		PackedStringArray search_parts = _search_term.split(" ");
		if (search_parts.size() > 1 || search_parts[0].begins_with("tag:")) {
			PackedStringArray remaining;
			for (const String &part : search_parts) {
				if (part.begins_with("tag:")) {
					tags.push_back(part.get_slice(":", 1));
				} else {
					remaining.append(part);
				}
			}
			search_term = String(" ").join(remaining); // Search term without tags.
		} else {
			search_term = _search_term;
		}
	}

	for (int i = 0; i < _projects.size(); ++i) {
		Item &item = _projects.write[i];

		bool item_visible = true;
		if (!_search_term.is_empty()) {
			String search_path;
			if (search_term.contains_char('/')) {
				// Search path will match the whole path
				search_path = item.path;
			} else {
				// Search path will only match the last path component to make searching more strict
				search_path = item.path.get_file();
			}

			bool missing_tags = false;
			for (const String &tag : tags) {
				if (!item.tags.has(tag)) {
					missing_tags = true;
					break;
				}
			}

			// When searching, display projects whose name or path contain the search term and whose tags match the searched tags.
			item_visible = !missing_tags && (search_term.is_empty() || item.project_name.containsn(search_term) || search_path.containsn(search_term));
		}

		item.control->set_visible(item_visible);
	}

	for (int i = 0; i < _projects.size(); ++i) {
		Item &item = _projects.write[i];
		item.control->get_parent()->move_child(item.control, i);
	}

	// Rewind the coroutine because order of projects changed
	_update_icons_async();
	update_dock_menu();
}

int ProjectList::get_project_count() const {
	return _projects.size();
}

void ProjectList::find_projects(const String &p_path) {
	PackedStringArray paths = { p_path };
	find_projects_multiple(paths);
}

void ProjectList::find_projects_multiple(const PackedStringArray &p_paths) {
	if (!scan_progress && is_inside_tree()) {
		scan_progress = memnew(AcceptDialog);
		scan_progress->set_title(TTR("Scanning"));
		scan_progress->set_ok_button_text(TTR("Cancel"));

		VBoxContainer *vb = memnew(VBoxContainer);
		scan_progress->add_child(vb);

		Label *label = memnew(Label);
		label->set_text(TTR("Scanning for projects..."));
		vb->add_child(label);

		ProgressBar *progress = memnew(ProgressBar);
		progress->set_indeterminate(true);
		vb->add_child(progress);

		add_child(scan_progress);
		scan_progress->connect(SceneStringName(confirmed), callable_mp(this, &ProjectList::_scan_finished));
		scan_progress->connect("canceled", callable_mp(this, &ProjectList::_scan_finished));
	}

	scan_data = memnew(ScanData);
	scan_data->paths_to_scan = p_paths;
	scan_data->scan_in_progress.set();

	scan_data->thread = memnew(Thread);
	scan_data->thread->start(_scan_thread, scan_data);

	if (scan_progress) {
		scan_progress->reset_size();
		scan_progress->popup_centered();
	}
	set_process(true);
}

void ProjectList::load_project_list() {
	List<String> sections;
	_config.load(_config_path);
	_config.get_sections(&sections);

	for (const String &path : sections) {
		bool favorite = _config.get_value(path, "favorite", false);
		_projects.push_back(load_project_data(path, favorite));
	}
}

void ProjectList::_scan_folder_recursive(const String &p_path, List<String> *r_projects, const SafeFlag &p_scan_active) {
	if (!p_scan_active.is_set()) {
		return;
	}

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	Error error = da->change_dir(p_path);
	ERR_FAIL_COND_MSG(error != OK, vformat("Failed to open the path \"%s\" for scanning (code %d).", p_path, error));

	da->list_dir_begin();
	String n = da->get_next();
	while (!n.is_empty()) {
		if (!p_scan_active.is_set()) {
			return;
		}

		if (da->current_is_dir() && n[0] != '.') {
			_scan_folder_recursive(da->get_current_dir().path_join(n), r_projects, p_scan_active);
		} else if (n == "project.godot") {
			r_projects->push_back(da->get_current_dir());
		}
		n = da->get_next();
	}
	da->list_dir_end();
}

// Project list items.

void ProjectList::add_project(const String &dir_path, bool favorite) {
	if (!_config.has_section(dir_path)) {
		_config.set_value(dir_path, "favorite", favorite);
	}
}

void ProjectList::set_project_version(const String &p_project_path, int p_version) {
	for (ProjectList::Item &E : _projects) {
		if (E.path == p_project_path) {
			E.version = p_version;
			break;
		}
	}
}

int ProjectList::refresh_project(const String &dir_path) {
	// Reloads information about a specific project.
	// If it wasn't loaded and should be in the list, it is added (i.e new project).
	// If it isn't in the list anymore, it is removed.
	// If it is in the list but doesn't exist anymore, it is marked as missing.

	bool should_be_in_list = _config.has_section(dir_path);
	bool is_favourite = _config.get_value(dir_path, "favorite", false);

	bool was_selected = _selected_project_paths.has(dir_path);

	// Remove item in any case
	for (int i = 0; i < _projects.size(); ++i) {
		const Item &existing_item = _projects[i];
		if (existing_item.path == dir_path) {
			_remove_project(i, false);
			break;
		}
	}

	int index = -1;
	if (should_be_in_list) {
		// Recreate it with updated info

		Item item = load_project_data(dir_path, is_favourite);

		_projects.push_back(item);
		_create_project_item_control(_projects.size() - 1);

		sort_projects();

		for (int i = 0; i < _projects.size(); ++i) {
			if (_projects[i].path == dir_path) {
				if (was_selected) {
					select_project(i);
					ensure_project_visible(i);
				}
				_load_project_icon(i);

				index = i;
				break;
			}
		}
	}

	return index;
}

void ProjectList::ensure_project_visible(int p_index) {
	const Item &item = _projects[p_index];
	ensure_control_visible(item.control);
}

void ProjectList::_create_project_item_control(int p_index) {
	// Will be added last in the list, so make sure indexes match
	ERR_FAIL_COND(p_index != project_list_vbox->get_child_count());

	Item &item = _projects.write[p_index];
	ERR_FAIL_COND(item.control != nullptr); // Already created

	ProjectListItemControl *hb = memnew(ProjectListItemControl);
	hb->add_theme_constant_override("separation", 10 * EDSCALE);

	hb->set_project_title(!item.missing ? item.project_name : TTR("Missing Project"));
	hb->set_project_path(item.path);
	hb->set_tooltip_text(item.description);
	hb->set_tags(item.tags, this);
	hb->set_unsupported_features(item.unsupported_features.duplicate());
	hb->set_project_version(item.project_version);
	hb->set_last_edited_info(!item.missing ? Time::get_singleton()->get_datetime_string_from_unix_time(item.last_edited, true) : TTR("Missing Date"));

	hb->set_is_favorite(item.favorite);
	hb->set_is_missing(item.missing);
	hb->set_is_grayed(item.grayed);

	hb->connect(SceneStringName(gui_input), callable_mp(this, &ProjectList::_list_item_input).bind(hb));
	hb->connect("favorite_pressed", callable_mp(this, &ProjectList::_on_favorite_pressed).bind(hb));

#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	hb->connect("explore_pressed", callable_mp(this, &ProjectList::_on_explore_pressed).bind(item.path));
#endif

	project_list_vbox->add_child(hb);
	item.control = hb;
}

void ProjectList::_toggle_project(int p_index) {
	// This methods adds to the selection or removes from the
	// selection.
	Item &item = _projects.write[p_index];

	if (_selected_project_paths.has(item.path)) {
		_deselect_project_nocheck(p_index);
	} else {
		_select_project_nocheck(p_index);
	}
}

void ProjectList::_remove_project(int p_index, bool p_update_config) {
	const Item item = _projects[p_index]; // Take a copy

	_selected_project_paths.erase(item.path);

	if (_last_clicked == item.path) {
		_last_clicked = "";
	}

	memdelete(item.control);
	_projects.remove_at(p_index);

	if (p_update_config) {
		_config.erase_section(item.path);
		// Not actually saving the file, in case you are doing more changes to settings
	}

	update_dock_menu();
}

void ProjectList::_list_item_input(const Ref<InputEvent> &p_ev, Node *p_hb) {
	Ref<InputEventMouseButton> mb = p_ev;
	int clicked_index = p_hb->get_index();
	const Item &clicked_project = _projects[clicked_index];

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_shift_pressed() && _selected_project_paths.size() > 0 && !_last_clicked.is_empty() && clicked_project.path != _last_clicked) {
			int anchor_index = -1;
			for (int i = 0; i < _projects.size(); ++i) {
				const Item &p = _projects[i];
				if (p.path == _last_clicked) {
					anchor_index = p.control->get_index();
					break;
				}
			}
			CRASH_COND(anchor_index == -1);
			_select_project_range(anchor_index, clicked_index);

		} else if (mb->is_command_or_control_pressed()) {
			_toggle_project(clicked_index);

		} else {
			_last_clicked = clicked_project.path;
			select_project(clicked_index);
		}

		emit_signal(SNAME(SIGNAL_SELECTION_CHANGED));

		// Do not allow opening a project more than once using a single project manager instance.
		// Opening the same project in several editor instances at once can lead to various issues.
		if (!mb->is_command_or_control_pressed() && mb->is_double_click() && !project_opening_initiated) {
			emit_signal(SNAME(SIGNAL_PROJECT_ASK_OPEN));
		}
	}
}

void ProjectList::_on_favorite_pressed(Node *p_hb) {
	ProjectListItemControl *control = Object::cast_to<ProjectListItemControl>(p_hb);

	int index = control->get_index();
	Item item = _projects.write[index]; // Take copy

	item.favorite = !item.favorite;

	_config.set_value(item.path, "favorite", item.favorite);
	save_config();

	_projects.write[index] = item;

	control->set_is_favorite(item.favorite);

	sort_projects();

	if (item.favorite) {
		for (int i = 0; i < _projects.size(); ++i) {
			if (_projects[i].path == item.path) {
				ensure_project_visible(i);
				break;
			}
		}
	}

	update_dock_menu();
}

void ProjectList::_on_explore_pressed(const String &p_path) {
	OS::get_singleton()->shell_show_in_file_manager(p_path, true);
}

// Project list selection.

void ProjectList::_clear_project_selection() {
	Vector<Item> previous_selected_items = get_selected_projects();
	_selected_project_paths.clear();

	for (int i = 0; i < previous_selected_items.size(); ++i) {
		previous_selected_items[i].control->set_selected(false);
	}
}

void ProjectList::_select_project_nocheck(int p_index) {
	Item &item = _projects.write[p_index];
	_selected_project_paths.insert(item.path);
	item.control->set_selected(true);
}

void ProjectList::_deselect_project_nocheck(int p_index) {
	Item &item = _projects.write[p_index];
	_selected_project_paths.erase(item.path);
	item.control->set_selected(false);
}

inline void _sort_project_range(int &a, int &b) {
	if (a > b) {
		int temp = a;
		a = b;
		b = temp;
	}
}

void ProjectList::_select_project_range(int p_begin, int p_end) {
	_clear_project_selection();

	_sort_project_range(p_begin, p_end);
	for (int i = p_begin; i <= p_end; ++i) {
		_select_project_nocheck(i);
	}
}

void ProjectList::select_project(int p_index) {
	// This method keeps only one project selected.
	_clear_project_selection();
	_select_project_nocheck(p_index);
}

void ProjectList::select_first_visible_project() {
	_clear_project_selection();

	for (int i = 0; i < _projects.size(); i++) {
		if (_projects[i].control->is_visible()) {
			_select_project_nocheck(i);
			break;
		}
	}
}

Vector<ProjectList::Item> ProjectList::get_selected_projects() const {
	Vector<Item> items;
	if (_selected_project_paths.size() == 0) {
		return items;
	}
	items.resize(_selected_project_paths.size());
	int j = 0;
	for (int i = 0; i < _projects.size(); ++i) {
		const Item &item = _projects[i];
		if (_selected_project_paths.has(item.path)) {
			items.write[j++] = item;
		}
	}
	ERR_FAIL_COND_V(j != items.size(), items);
	return items;
}

const HashSet<String> &ProjectList::get_selected_project_keys() const {
	// Faster if that's all you need
	return _selected_project_paths;
}

int ProjectList::get_single_selected_index() const {
	if (_selected_project_paths.size() == 0) {
		// Default selection
		return 0;
	}
	String key;
	if (_selected_project_paths.size() == 1) {
		// Only one selected
		key = *_selected_project_paths.begin();
	} else {
		// Multiple selected, consider the last clicked one as "main"
		key = _last_clicked;
	}
	for (int i = 0; i < _projects.size(); ++i) {
		if (_projects[i].path == key) {
			return i;
		}
	}
	return 0;
}

void ProjectList::erase_selected_projects(bool p_delete_project_contents) {
	if (_selected_project_paths.size() == 0) {
		return;
	}

	for (int i = 0; i < _projects.size(); ++i) {
		Item &item = _projects.write[i];
		if (_selected_project_paths.has(item.path) && item.control->is_visible()) {
			_config.erase_section(item.path);

			// Comment out for now until we have a better warning system to
			// ensure users delete their project only.
			//if (p_delete_project_contents) {
			//	OS::get_singleton()->move_to_trash(item.path);
			//}

			memdelete(item.control);
			_projects.remove_at(i);
			--i;
		}
	}

	save_config();
	_selected_project_paths.clear();
	_last_clicked = "";

	update_dock_menu();
}

// Missing projects.

bool ProjectList::is_any_project_missing() const {
	for (int i = 0; i < _projects.size(); ++i) {
		if (_projects[i].missing) {
			return true;
		}
	}
	return false;
}

void ProjectList::erase_missing_projects() {
	if (_projects.is_empty()) {
		return;
	}

	int deleted_count = 0;
	int remaining_count = 0;

	for (int i = 0; i < _projects.size(); ++i) {
		const Item &item = _projects[i];

		if (item.missing) {
			_remove_project(i, true);
			--i;
			++deleted_count;

		} else {
			++remaining_count;
		}
	}

	print_line("Removed " + itos(deleted_count) + " projects from the list, remaining " + itos(remaining_count) + " projects");
	save_config();
}

// Project list sorting and filtering.

void ProjectList::set_search_term(String p_search_term) {
	_search_term = p_search_term;
}

void ProjectList::add_search_tag(const String &p_tag) {
	const String tag_string = "tag:" + p_tag;

	int exists = _search_term.find(tag_string);
	if (exists > -1) {
		_search_term = _search_term.erase(exists, tag_string.length() + 1);
	} else if (_search_term.is_empty() || _search_term.ends_with(" ")) {
		_search_term += tag_string;
	} else {
		_search_term += " " + tag_string;
	}
	ProjectManager::get_singleton()->get_search_box()->set_text(_search_term);

	sort_projects();
}

void ProjectList::set_order_option(int p_option) {
	FilterOption selected = (FilterOption)p_option;
	EditorSettings::get_singleton()->set("project_manager/sorting_order", p_option);
	EditorSettings::get_singleton()->save();
	_order_option = selected;

	sort_projects();
}

// Global menu integration.

void ProjectList::update_dock_menu() {
	if (!NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_GLOBAL_MENU)) {
		return;
	}
	RID dock_rid = NativeMenu::get_singleton()->get_system_menu(NativeMenu::DOCK_MENU_ID);
	NativeMenu::get_singleton()->clear(dock_rid);

	int favs_added = 0;
	int total_added = 0;
	for (int i = 0; i < _projects.size(); ++i) {
		if (!_projects[i].grayed && !_projects[i].missing) {
			if (_projects[i].favorite) {
				favs_added++;
			} else {
				if (favs_added != 0) {
					NativeMenu::get_singleton()->add_separator(dock_rid);
				}
				favs_added = 0;
			}
			NativeMenu::get_singleton()->add_item(dock_rid, _projects[i].project_name + " ( " + _projects[i].path + " )", callable_mp(this, &ProjectList::_global_menu_open_project), Callable(), i);
			total_added++;
		}
	}
	if (total_added != 0) {
		NativeMenu::get_singleton()->add_separator(dock_rid);
	}
	NativeMenu::get_singleton()->add_item(dock_rid, TTR("New Window"), callable_mp(this, &ProjectList::_global_menu_new_window));
}

void ProjectList::_global_menu_new_window(const Variant &p_tag) {
	List<String> args;
	args.push_back("-p");
	OS::get_singleton()->create_instance(args);
}

void ProjectList::_global_menu_open_project(const Variant &p_tag) {
	int idx = (int)p_tag;

	if (idx >= 0 && idx < _projects.size()) {
		String conf = _projects[idx].path.path_join("project.godot");
		List<String> args;
		args.push_back(conf);
		OS::get_singleton()->create_instance(args);
	}
}

// Object methods.

void ProjectList::_bind_methods() {
	ADD_SIGNAL(MethodInfo(SIGNAL_LIST_CHANGED));
	ADD_SIGNAL(MethodInfo(SIGNAL_SELECTION_CHANGED));
	ADD_SIGNAL(MethodInfo(SIGNAL_PROJECT_ASK_OPEN));
}

ProjectList::ProjectList() {
	project_list_vbox = memnew(VBoxContainer);
	project_list_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	add_child(project_list_vbox);

	_config_path = EditorPaths::get_singleton()->get_data_dir().path_join("projects.cfg");
	_migrate_config();
}
