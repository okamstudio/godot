/**************************************************************************/
/*  editor_vcs_interface.cpp                                              */
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

#include "editor_vcs_interface.h"

#include "editor_node.h"

EditorVCSInterface *EditorVCSInterface::singleton = nullptr;

void EditorVCSInterface::popup_error(const String &p_msg) {
	// TRANSLATORS: %s refers to the name of a version control system (e.g. "Git").
	EditorNode::get_singleton()->show_warning(p_msg.strip_edges(), vformat(TTR("%s Error"), get_vcs_name()));
}

bool EditorVCSInterface::initialize(const String &p_project_path) {
	bool result = false;
	GDVIRTUAL_CALL(_initialize, p_project_path, result);
	return result;
}

void EditorVCSInterface::set_credentials(const String &p_username, const String &p_password, const String &p_ssh_public_key, const String &p_ssh_private_key, const String &p_ssh_passphrase) {
	GDVIRTUAL_CALL(_set_credentials, p_username, p_password, p_ssh_public_key, p_ssh_private_key, p_ssh_passphrase);
}

List<String> EditorVCSInterface::get_remotes() {
	TypedArray<String> result;
	if (!GDVIRTUAL_CALL(_get_remotes, result)) {
		return {};
	}

	List<String> remotes;
	for (int i = 0; i < result.size(); i++) {
		remotes.push_back(result[i]);
	}
	return remotes;
}

List<EditorVCSInterface::StatusFile> EditorVCSInterface::get_modified_files_data() {
	TypedArray<Dictionary> result;
	if (!GDVIRTUAL_CALL(_get_modified_files_data, result)) {
		return {};
	}

	List<EditorVCSInterface::StatusFile> status_files;
	for (int i = 0; i < result.size(); i++) {
		status_files.push_back(_convert_status_file(result[i]));
	}
	return status_files;
}

void EditorVCSInterface::stage_file(const String &p_file_path) {
	GDVIRTUAL_CALL(_stage_file, p_file_path);
}

void EditorVCSInterface::unstage_file(const String &p_file_path) {
	GDVIRTUAL_CALL(_unstage_file, p_file_path);
}

void EditorVCSInterface::discard_file(const String &p_file_path) {
	GDVIRTUAL_CALL(_discard_file, p_file_path);
}

void EditorVCSInterface::commit(const String &p_msg) {
	GDVIRTUAL_CALL(_commit, p_msg);
}

List<EditorVCSInterface::DiffFile> EditorVCSInterface::get_diff(const String &p_identifier, TreeArea p_area) {
	TypedArray<Dictionary> result;
	if (!GDVIRTUAL_CALL(_get_diff, p_identifier, int(p_area), result)) {
		return {};
	}

	List<DiffFile> diff_files;
	for (int i = 0; i < result.size(); i++) {
		diff_files.push_back(_convert_diff_file(result[i]));
	}
	return diff_files;
}

List<EditorVCSInterface::Commit> EditorVCSInterface::get_previous_commits(int p_max_commits) {
	TypedArray<Dictionary> result;
	if (!GDVIRTUAL_CALL(_get_previous_commits, p_max_commits, result)) {
		return {};
	}

	List<EditorVCSInterface::Commit> commits;
	for (int i = 0; i < result.size(); i++) {
		commits.push_back(_convert_commit(result[i]));
	}
	return commits;
}

List<String> EditorVCSInterface::get_branch_list() {
	TypedArray<String> result;
	if (!GDVIRTUAL_CALL(_get_branch_list, result)) {
		return {};
	}

	List<String> branch_list;
	for (int i = 0; i < result.size(); i++) {
		branch_list.push_back(result[i]);
	}
	return branch_list;
}

void EditorVCSInterface::create_branch(const String &p_branch_name) {
	GDVIRTUAL_CALL(_create_branch, p_branch_name);
}

void EditorVCSInterface::create_remote(const String &p_remote_name, const String &p_remote_url) {
	GDVIRTUAL_CALL(_create_remote, p_remote_name, p_remote_url);
}

void EditorVCSInterface::remove_branch(const String &p_branch_name) {
	GDVIRTUAL_CALL(_remove_branch, p_branch_name);
}

void EditorVCSInterface::remove_remote(const String &p_remote_name) {
	GDVIRTUAL_CALL(_remove_remote, p_remote_name);
}

String EditorVCSInterface::get_current_branch_name() {
	String result;
	GDVIRTUAL_CALL(_get_current_branch_name, result);
	return result;
}

bool EditorVCSInterface::checkout_branch(const String &p_branch_name) {
	bool result = false;
	GDVIRTUAL_CALL(_checkout_branch, p_branch_name, result);
	return result;
}

void EditorVCSInterface::pull(const String &p_remote) {
	GDVIRTUAL_CALL(_pull, p_remote);
}

void EditorVCSInterface::push(const String &p_remote, bool p_force) {
	GDVIRTUAL_CALL(_push, p_remote, p_force);
}

void EditorVCSInterface::fetch(const String &p_remote) {
	GDVIRTUAL_CALL(_fetch, p_remote);
}

List<EditorVCSInterface::DiffHunk> EditorVCSInterface::get_line_diff(const String &p_file_path, const String &p_text) {
	TypedArray<Dictionary> result;
	if (!GDVIRTUAL_CALL(_get_line_diff, p_file_path, p_text, result)) {
		return {};
	}

	List<DiffHunk> diff_hunks;
	for (int i = 0; i < result.size(); i++) {
		diff_hunks.push_back(_convert_diff_hunk(result[i]));
	}
	return diff_hunks;
}

bool EditorVCSInterface::shut_down() {
	bool result = false;
	GDVIRTUAL_CALL(_shut_down, result);
	return result;
}

String EditorVCSInterface::get_vcs_name() {
	String result;
	GDVIRTUAL_CALL(_get_vcs_name, result);
	return result;
}

Dictionary EditorVCSInterface::create_diff_line(int p_new_line_no, int p_old_line_no, const String &p_content, const String &p_status) {
	Dictionary diff_line;
	diff_line["new_line_no"] = p_new_line_no;
	diff_line["old_line_no"] = p_old_line_no;
	diff_line["content"] = p_content;
	diff_line["status"] = p_status;

	return diff_line;
}

Dictionary EditorVCSInterface::create_diff_hunk(int p_old_start, int p_new_start, int p_old_lines, int p_new_lines) {
	Dictionary diff_hunk;
	diff_hunk["new_lines"] = p_new_lines;
	diff_hunk["old_lines"] = p_old_lines;
	diff_hunk["new_start"] = p_new_start;
	diff_hunk["old_start"] = p_old_start;
	diff_hunk["diff_lines"] = TypedArray<Dictionary>();
	return diff_hunk;
}

Dictionary EditorVCSInterface::add_line_diffs_into_diff_hunk(Dictionary p_diff_hunk, TypedArray<Dictionary> p_line_diffs) {
	p_diff_hunk["diff_lines"] = p_line_diffs;
	return p_diff_hunk;
}

Dictionary EditorVCSInterface::create_diff_file(const String &p_new_file, const String &p_old_file) {
	Dictionary file_diff;
	file_diff["new_file"] = p_new_file;
	file_diff["old_file"] = p_old_file;
	file_diff["diff_hunks"] = TypedArray<Dictionary>();
	return file_diff;
}

Dictionary EditorVCSInterface::create_commit(const String &p_msg, const String &p_author, const String &p_id, int64_t p_unix_timestamp, int64_t p_offset_minutes) {
	Dictionary commit_info;
	commit_info["message"] = p_msg;
	commit_info["author"] = p_author;
	commit_info["unix_timestamp"] = p_unix_timestamp;
	commit_info["offset_minutes"] = p_offset_minutes;
	commit_info["id"] = p_id;
	return commit_info;
}

Dictionary EditorVCSInterface::add_diff_hunks_into_diff_file(Dictionary p_diff_file, TypedArray<Dictionary> p_diff_hunks) {
	p_diff_file["diff_hunks"] = p_diff_hunks;
	return p_diff_file;
}

Dictionary EditorVCSInterface::create_status_file(const String &p_file_path, ChangeType p_change, TreeArea p_area) {
	Dictionary sf;
	sf["file_path"] = p_file_path;
	sf["change_type"] = p_change;
	sf["area"] = p_area;
	return sf;
}

EditorVCSInterface::DiffLine EditorVCSInterface::_convert_diff_line(const Dictionary &p_diff_line) {
	DiffLine d;
	d.new_line_no = p_diff_line["new_line_no"];
	d.old_line_no = p_diff_line["old_line_no"];
	d.content = p_diff_line["content"];
	d.status = p_diff_line["status"];
	return d;
}

EditorVCSInterface::DiffHunk EditorVCSInterface::_convert_diff_hunk(const Dictionary &p_diff_hunk) {
	DiffHunk dh;
	dh.new_lines = p_diff_hunk["new_lines"];
	dh.old_lines = p_diff_hunk["old_lines"];
	dh.new_start = p_diff_hunk["new_start"];
	dh.old_start = p_diff_hunk["old_start"];
	TypedArray<Dictionary> diff_lines = p_diff_hunk["diff_lines"];
	for (int i = 0; i < diff_lines.size(); i++) {
		DiffLine dl = _convert_diff_line(diff_lines[i]);
		dh.diff_lines.push_back(dl);
	}
	return dh;
}

EditorVCSInterface::DiffFile EditorVCSInterface::_convert_diff_file(const Dictionary &p_diff_file) {
	DiffFile df;
	df.new_file = p_diff_file["new_file"];
	df.old_file = p_diff_file["old_file"];
	TypedArray<Dictionary> diff_hunks = p_diff_file["diff_hunks"];
	for (int i = 0; i < diff_hunks.size(); i++) {
		DiffHunk dh = _convert_diff_hunk(diff_hunks[i]);
		df.diff_hunks.push_back(dh);
	}
	return df;
}

EditorVCSInterface::Commit EditorVCSInterface::_convert_commit(const Dictionary &p_commit) {
	EditorVCSInterface::Commit c;
	c.msg = p_commit["message"];
	c.author = p_commit["author"];
	c.unix_timestamp = p_commit["unix_timestamp"];
	c.offset_minutes = p_commit["offset_minutes"];
	c.id = p_commit["id"];
	return c;
}

EditorVCSInterface::StatusFile EditorVCSInterface::_convert_status_file(const Dictionary &p_status_file) {
	StatusFile sf;
	sf.file_path = p_status_file["file_path"];
	sf.change_type = (ChangeType)(int)p_status_file["change_type"];
	sf.area = (TreeArea)(int)p_status_file["area"];
	return sf;
}

void EditorVCSInterface::_bind_methods() {
	// Proxy end points that implement the VCS specific operations that the editor demands.
	GDVIRTUAL_BIND(_initialize, "project_path");
	GDVIRTUAL_BIND(_set_credentials, "username", "password", "ssh_public_key_path", "ssh_private_key_path", "ssh_passphrase");
	GDVIRTUAL_BIND(_get_modified_files_data);
	GDVIRTUAL_BIND(_stage_file, "file_path");
	GDVIRTUAL_BIND(_unstage_file, "file_path");
	GDVIRTUAL_BIND(_discard_file, "file_path");
	GDVIRTUAL_BIND(_commit, "msg");
	GDVIRTUAL_BIND(_get_diff, "identifier", "area");
	GDVIRTUAL_BIND(_shut_down);
	GDVIRTUAL_BIND(_get_vcs_name);
	GDVIRTUAL_BIND(_get_previous_commits, "max_commits");
	GDVIRTUAL_BIND(_get_branch_list);
	GDVIRTUAL_BIND(_get_remotes);
	GDVIRTUAL_BIND(_create_branch, "branch_name");
	GDVIRTUAL_BIND(_remove_branch, "branch_name");
	GDVIRTUAL_BIND(_create_remote, "remote_name", "remote_url");
	GDVIRTUAL_BIND(_remove_remote, "remote_name");
	GDVIRTUAL_BIND(_get_current_branch_name);
	GDVIRTUAL_BIND(_checkout_branch, "branch_name");
	GDVIRTUAL_BIND(_pull, "remote");
	GDVIRTUAL_BIND(_push, "remote", "force");
	GDVIRTUAL_BIND(_fetch, "remote");
	GDVIRTUAL_BIND(_get_line_diff, "file_path", "text");

	ClassDB::bind_method(D_METHOD("create_diff_line", "new_line_no", "old_line_no", "content", "status"), &EditorVCSInterface::create_diff_line);
	ClassDB::bind_method(D_METHOD("create_diff_hunk", "old_start", "new_start", "old_lines", "new_lines"), &EditorVCSInterface::create_diff_hunk);
	ClassDB::bind_method(D_METHOD("create_diff_file", "new_file", "old_file"), &EditorVCSInterface::create_diff_file);
	ClassDB::bind_method(D_METHOD("create_commit", "msg", "author", "id", "unix_timestamp", "offset_minutes"), &EditorVCSInterface::create_commit);
	ClassDB::bind_method(D_METHOD("create_status_file", "file_path", "change_type", "area"), &EditorVCSInterface::create_status_file);
	ClassDB::bind_method(D_METHOD("add_diff_hunks_into_diff_file", "diff_file", "diff_hunks"), &EditorVCSInterface::add_diff_hunks_into_diff_file);
	ClassDB::bind_method(D_METHOD("add_line_diffs_into_diff_hunk", "diff_hunk", "line_diffs"), &EditorVCSInterface::add_line_diffs_into_diff_hunk);
	ClassDB::bind_method(D_METHOD("popup_error", "msg"), &EditorVCSInterface::popup_error);

	BIND_ENUM_CONSTANT(CHANGE_TYPE_NEW);
	BIND_ENUM_CONSTANT(CHANGE_TYPE_MODIFIED);
	BIND_ENUM_CONSTANT(CHANGE_TYPE_RENAMED);
	BIND_ENUM_CONSTANT(CHANGE_TYPE_DELETED);
	BIND_ENUM_CONSTANT(CHANGE_TYPE_TYPECHANGE);
	BIND_ENUM_CONSTANT(CHANGE_TYPE_UNMERGED);

	BIND_ENUM_CONSTANT(TREE_AREA_COMMIT);
	BIND_ENUM_CONSTANT(TREE_AREA_STAGED);
	BIND_ENUM_CONSTANT(TREE_AREA_UNSTAGED);
}

EditorVCSInterface *EditorVCSInterface::get_singleton() {
	return singleton;
}

void EditorVCSInterface::set_singleton(EditorVCSInterface *p_singleton) {
	singleton = p_singleton;
}

void EditorVCSInterface::create_vcs_metadata_files(VCSMetadata p_vcs_metadata_type, String &p_dir) {
	if (p_vcs_metadata_type == VCSMetadata::GIT) {
		Ref<FileAccess> f = FileAccess::open(p_dir.path_join(".gitignore"), FileAccess::WRITE);
		if (f.is_null()) {
			ERR_FAIL_MSG("Couldn't create .gitignore in project path.");
		} else {
			f->store_line("# Godot 4+ specific ignores");
			f->store_line(".godot/");
			f->store_line("*.translation");
			f->store_line("/android/");
		}
		f = FileAccess::open(p_dir.path_join(".gitattributes"), FileAccess::WRITE);
		if (f.is_null()) {
			ERR_FAIL_MSG("Couldn't create .gitattributes in project path.");
		} else {
			f->store_line("# Normalize EOL for all files that Git considers text files.");
			f->store_line("* text=auto eol=lf");
		}
	}
}
