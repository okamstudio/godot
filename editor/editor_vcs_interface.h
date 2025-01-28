/**************************************************************************/
/*  editor_vcs_interface.h                                                */
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

#ifndef EDITOR_VCS_INTERFACE_H
#define EDITOR_VCS_INTERFACE_H

#include "core/object/gdvirtual.gen.inc"
#include "core/string/ustring.h"
#include "core/variant/typed_array.h"

class EditorVCSInterface : public Object {
	GDCLASS(EditorVCSInterface, Object)

public:
	enum ChangeType {
		CHANGE_TYPE_NEW = 0,
		CHANGE_TYPE_MODIFIED = 1,
		CHANGE_TYPE_RENAMED = 2,
		CHANGE_TYPE_DELETED = 3,
		CHANGE_TYPE_TYPECHANGE = 4,
		CHANGE_TYPE_UNMERGED = 5
	};

	enum TreeArea {
		TREE_AREA_COMMIT = 0,
		TREE_AREA_STAGED = 1,
		TREE_AREA_UNSTAGED = 2
	};

	struct DiffLine {
		int new_line_no;
		int old_line_no;
		String content;
		String status;

		String old_text;
		String new_text;
	};

	struct DiffHunk {
		int new_start;
		int old_start;
		int new_lines;
		int old_lines;
		List<DiffLine> diff_lines;
	};

	struct DiffFile {
		String new_file;
		String old_file;
		List<DiffHunk> diff_hunks;
	};

	struct Commit {
		String author;
		String msg;
		String id;
		int64_t unix_timestamp;
		int64_t offset_minutes;
	};

	struct StatusFile {
		TreeArea area;
		ChangeType change_type;
		String file_path;
	};

protected:
	static EditorVCSInterface *singleton;

	static void _bind_methods();

	DiffLine _convert_diff_line(const Dictionary &p_diff_line);
	DiffHunk _convert_diff_hunk(const Dictionary &p_diff_hunk);
	DiffFile _convert_diff_file(const Dictionary &p_diff_file);
	Commit _convert_commit(const Dictionary &p_commit);
	StatusFile _convert_status_file(const Dictionary &p_status_file);

	// Proxy endpoints for extensions to implement
	GDVIRTUAL1R_REQUIRED(bool, _initialize, String);
	GDVIRTUAL5_REQUIRED(_set_credentials, String, String, String, String, String);
	GDVIRTUAL0R_REQUIRED(TypedArray<Dictionary>, _get_modified_files_data);
	GDVIRTUAL1_REQUIRED(_stage_file, String);
	GDVIRTUAL1_REQUIRED(_unstage_file, String);
	GDVIRTUAL1_REQUIRED(_discard_file, String);
	GDVIRTUAL1_REQUIRED(_commit, String);
	GDVIRTUAL2R_REQUIRED(TypedArray<Dictionary>, _get_diff, String, int);
	GDVIRTUAL0R_REQUIRED(bool, _shut_down);
	GDVIRTUAL0R_REQUIRED(String, _get_vcs_name);
	GDVIRTUAL1R_REQUIRED(TypedArray<Dictionary>, _get_previous_commits, int);
	GDVIRTUAL0R_REQUIRED(TypedArray<String>, _get_branch_list);
	GDVIRTUAL0R_REQUIRED(TypedArray<String>, _get_remotes);
	GDVIRTUAL1_REQUIRED(_create_branch, String);
	GDVIRTUAL1_REQUIRED(_remove_branch, String);
	GDVIRTUAL2_REQUIRED(_create_remote, String, String);
	GDVIRTUAL1_REQUIRED(_remove_remote, String);
	GDVIRTUAL0R_REQUIRED(String, _get_current_branch_name);
	GDVIRTUAL1R_REQUIRED(bool, _checkout_branch, String);
	GDVIRTUAL1_REQUIRED(_pull, String);
	GDVIRTUAL2_REQUIRED(_push, String, bool);
	GDVIRTUAL1_REQUIRED(_fetch, String);
	GDVIRTUAL2R_REQUIRED(TypedArray<Dictionary>, _get_line_diff, String, String);

public:
	static EditorVCSInterface *get_singleton();
	static void set_singleton(EditorVCSInterface *p_singleton);

	enum class VCSMetadata {
		NONE,
		GIT,
	};
	static void create_vcs_metadata_files(VCSMetadata p_vcs_metadata_type, String &p_dir);

	// Proxies to the editor for use
	bool initialize(const String &p_project_path);
	void set_credentials(const String &p_username, const String &p_password, const String &p_ssh_public_key_path, const String &p_ssh_private_key_path, const String &p_ssh_passphrase);
	List<StatusFile> get_modified_files_data();
	void stage_file(const String &p_file_path);
	void unstage_file(const String &p_file_path);
	void discard_file(const String &p_file_path);
	void commit(const String &p_msg);
	List<DiffFile> get_diff(const String &p_identifier, TreeArea p_area);
	bool shut_down();
	String get_vcs_name();
	List<Commit> get_previous_commits(int p_max_commits);
	List<String> get_branch_list();
	List<String> get_remotes();
	void create_branch(const String &p_branch_name);
	void remove_branch(const String &p_branch_name);
	void create_remote(const String &p_remote_name, const String &p_remote_url);
	void remove_remote(const String &p_remote_name);
	String get_current_branch_name();
	bool checkout_branch(const String &p_branch_name);
	void pull(const String &p_remote);
	void push(const String &p_remote, bool p_force);
	void fetch(const String &p_remote);
	List<DiffHunk> get_line_diff(const String &p_file_path, const String &p_text);

	// Helper functions to create and convert Dictionary into data structures
	Dictionary create_diff_line(int p_new_line_no, int p_old_line_no, const String &p_content, const String &p_status);
	Dictionary create_diff_hunk(int p_old_start, int p_new_start, int p_old_lines, int p_new_lines);
	Dictionary create_diff_file(const String &p_new_file, const String &p_old_file);
	Dictionary create_commit(const String &p_msg, const String &p_author, const String &p_id, int64_t p_unix_timestamp, int64_t p_offset_minutes);
	Dictionary create_status_file(const String &p_file_path, ChangeType p_change, TreeArea p_area);
	Dictionary add_line_diffs_into_diff_hunk(Dictionary p_diff_hunk, TypedArray<Dictionary> p_line_diffs);
	Dictionary add_diff_hunks_into_diff_file(Dictionary p_diff_file, TypedArray<Dictionary> p_diff_hunks);

	void popup_error(const String &p_msg);
};

VARIANT_ENUM_CAST(EditorVCSInterface::ChangeType);
VARIANT_ENUM_CAST(EditorVCSInterface::TreeArea);

#endif // EDITOR_VCS_INTERFACE_H
