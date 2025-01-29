/**************************************************************************/
/*  display_server_windows.cpp                                            */
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

#include "display_server_windows.h"

#include "drop_target_windows.h"
#include "os_windows.h"
#include "wgl_detect_version.h"

#include "core/config/project_settings.h"
#include "core/io/marshalls.h"
#include "core/version.h"
#include "drivers/png/png_driver_common.h"
#include "main/main.h"
#include "scene/resources/texture.h"

#if defined(VULKAN_ENABLED)
#include "rendering_context_driver_vulkan_windows.h"
#endif
#if defined(D3D12_ENABLED)
#include "drivers/d3d12/rendering_context_driver_d3d12.h"
#endif
#if defined(GLES3_ENABLED)
#include "drivers/gles3/rasterizer_gles3.h"
#endif

#include <avrt.h>
#include <dwmapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <shobjidl.h>
#include <wbemcli.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWCP_DEFAULT
#define DWMWCP_DEFAULT 0
#endif

#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif

#define WM_INDICATOR_CALLBACK_MESSAGE (WM_USER + 1)

#if defined(__GNUC__)
// Workaround GCC warning from -Wcast-function-type.
#define GetProcAddress (void *)GetProcAddress
#endif

static String format_error_message(DWORD id) {
	LPWSTR messageBuffer = nullptr;
	size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			nullptr, id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, nullptr);

	String msg = "Error " + itos(id) + ": " + String::utf16((const char16_t *)messageBuffer, size);

	LocalFree(messageBuffer);

	return msg;
}

static void track_mouse_leave_event(HWND hWnd) {
	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = hWnd;
	tme.dwHoverTime = HOVER_DEFAULT;
	TrackMouseEvent(&tme);
}

bool DisplayServerWindows::has_feature(Feature p_feature) const {
	switch (p_feature) {
#ifndef DISABLE_DEPRECATED
		case FEATURE_GLOBAL_MENU: {
			return (native_menu && native_menu->has_feature(NativeMenu::FEATURE_GLOBAL_MENU));
		} break;
#endif
		case FEATURE_SUBWINDOWS:
		case FEATURE_TOUCHSCREEN:
		case FEATURE_MOUSE:
		case FEATURE_MOUSE_WARP:
		case FEATURE_CLIPBOARD:
		case FEATURE_CURSOR_SHAPE:
		case FEATURE_CUSTOM_CURSOR_SHAPE:
		case FEATURE_IME:
		case FEATURE_WINDOW_TRANSPARENCY:
		case FEATURE_HIDPI:
		case FEATURE_ICON:
		case FEATURE_NATIVE_ICON:
		case FEATURE_NATIVE_DIALOG:
		case FEATURE_NATIVE_DIALOG_INPUT:
		case FEATURE_NATIVE_DIALOG_FILE:
		case FEATURE_NATIVE_DIALOG_FILE_EXTRA:
		//case FEATURE_NATIVE_DIALOG_FILE_MIME:
		case FEATURE_SWAP_BUFFERS:
		case FEATURE_KEEP_SCREEN_ON:
		case FEATURE_TEXT_TO_SPEECH:
		case FEATURE_SCREEN_CAPTURE:
		case FEATURE_STATUS_INDICATOR:
		case FEATURE_WINDOW_EMBEDDING:
		case FEATURE_WINDOW_DRAG:
		case FEATURE_SCREEN_EXCLUDE_FROM_CAPTURE:
			return true;
		case FEATURE_EMOJI_AND_SYMBOL_PICKER:
			return (os_ver.dwBuildNumber >= 17134); // Windows 10 Redstone 4 (1803)+ only.
		default:
			return false;
	}
}

String DisplayServerWindows::get_name() const {
	return "Windows";
}

void DisplayServerWindows::_set_mouse_mode_impl(MouseMode p_mode) {
	if (p_mode == MOUSE_MODE_HIDDEN || p_mode == MOUSE_MODE_CAPTURED || p_mode == MOUSE_MODE_CONFINED_HIDDEN) {
		// Hide cursor before moving.
		if (hCursor == nullptr) {
			hCursor = SetCursor(nullptr);
		} else {
			SetCursor(nullptr);
		}
	}

	if (windows.has(MAIN_WINDOW_ID) && (p_mode == MOUSE_MODE_CAPTURED || p_mode == MOUSE_MODE_CONFINED || p_mode == MOUSE_MODE_CONFINED_HIDDEN)) {
		// Mouse is grabbed (captured or confined).
		WindowID window_id = _get_focused_window_or_popup();
		if (!windows.has(window_id)) {
			window_id = MAIN_WINDOW_ID;
		}

		WindowData &wd = windows[window_id];

		RECT clipRect;
		GetClientRect(wd.hWnd, &clipRect);
		ClientToScreen(wd.hWnd, (POINT *)&clipRect.left);
		ClientToScreen(wd.hWnd, (POINT *)&clipRect.right);
		ClipCursor(&clipRect);
		if (p_mode == MOUSE_MODE_CAPTURED) {
			center = window_get_size() / 2;
			POINT pos = { (int)center.x, (int)center.y };
			ClientToScreen(wd.hWnd, &pos);
			SetCursorPos(pos.x, pos.y);
			SetCapture(wd.hWnd);

			_register_raw_input_devices(window_id);
		}
	} else {
		// Mouse is free to move around (not captured or confined).
		ReleaseCapture();
		ClipCursor(nullptr);

		_register_raw_input_devices(INVALID_WINDOW_ID);
	}

	if (p_mode == MOUSE_MODE_VISIBLE || p_mode == MOUSE_MODE_CONFINED) {
		// Show cursor.
		CursorShape c = cursor_shape;
		cursor_shape = CURSOR_MAX;
		cursor_set_shape(c);
	}
}

DisplayServer::WindowID DisplayServerWindows::_get_focused_window_or_popup() const {
	const List<WindowID>::Element *E = popup_list.back();
	if (E) {
		return E->get();
	}

	return last_focused_window;
}

void DisplayServerWindows::_register_raw_input_devices(WindowID p_target_window) {
	use_raw_input = true;

	RAWINPUTDEVICE rid[2] = {};
	rid[0].usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
	rid[0].usUsage = 0x02; // HID_USAGE_GENERIC_MOUSE
	rid[0].dwFlags = 0;

	rid[1].usUsagePage = 0x01; // HID_USAGE_PAGE_GENERIC
	rid[1].usUsage = 0x06; // HID_USAGE_GENERIC_KEYBOARD
	rid[1].dwFlags = 0;

	if (p_target_window != INVALID_WINDOW_ID && windows.has(p_target_window)) {
		// Follow the defined window
		rid[0].hwndTarget = windows[p_target_window].hWnd;
		rid[1].hwndTarget = windows[p_target_window].hWnd;
	} else {
		// Follow the keyboard focus
		rid[0].hwndTarget = nullptr;
		rid[1].hwndTarget = nullptr;
	}

	if (RegisterRawInputDevices(rid, 2, sizeof(rid[0])) == FALSE) {
		// Registration failed.
		use_raw_input = false;
	}
}

bool DisplayServerWindows::tts_is_speaking() const {
	ERR_FAIL_NULL_V_MSG(tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return tts->is_speaking();
}

bool DisplayServerWindows::tts_is_paused() const {
	ERR_FAIL_NULL_V_MSG(tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return tts->is_paused();
}

TypedArray<Dictionary> DisplayServerWindows::tts_get_voices() const {
	ERR_FAIL_NULL_V_MSG(tts, TypedArray<Dictionary>(), "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	return tts->get_voices();
}

void DisplayServerWindows::tts_speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->speak(p_text, p_voice, p_volume, p_pitch, p_rate, p_utterance_id, p_interrupt);
}

void DisplayServerWindows::tts_pause() {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->pause();
}

void DisplayServerWindows::tts_resume() {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->resume();
}

void DisplayServerWindows::tts_stop() {
	ERR_FAIL_NULL_MSG(tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	tts->stop();
}

Error DisplayServerWindows::file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) {
	return _file_dialog_with_options_show(p_title, p_current_directory, String(), p_filename, p_show_hidden, p_mode, p_filters, TypedArray<Dictionary>(), p_callback, false);
}

Error DisplayServerWindows::file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) {
	return _file_dialog_with_options_show(p_title, p_current_directory, p_root, p_filename, p_show_hidden, p_mode, p_filters, p_options, p_callback, true);
}

// Silence warning due to a COM API weirdness.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

class FileDialogEventHandler : public IFileDialogEvents, public IFileDialogControlEvents {
	LONG ref_count = 1;
	int ctl_id = 1;

	HashMap<int, String> ctls;
	Dictionary selected;
	String root;

public:
	// IUnknown methods
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) {
		static const QITAB qit[] = {
#ifdef __MINGW32__
			{ &__uuidof(IFileDialogEvents), static_cast<decltype(qit[0].dwOffset)>(OFFSETOFCLASS(IFileDialogEvents, FileDialogEventHandler)) },
			{ &__uuidof(IFileDialogControlEvents), static_cast<decltype(qit[0].dwOffset)>(OFFSETOFCLASS(IFileDialogControlEvents, FileDialogEventHandler)) },
#else
			QITABENT(FileDialogEventHandler, IFileDialogEvents),
			QITABENT(FileDialogEventHandler, IFileDialogControlEvents),
#endif
			{ nullptr, 0 },
		};
		return QISearch(this, qit, riid, ppv);
	}

	ULONG STDMETHODCALLTYPE AddRef() {
		return InterlockedIncrement(&ref_count);
	}

	ULONG STDMETHODCALLTYPE Release() {
		long ref = InterlockedDecrement(&ref_count);
		if (!ref) {
			delete this;
		}
		return ref;
	}

	// IFileDialogEvents methods
	HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog *) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog *) { return S_OK; }

	HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog *p_pfd, IShellItem *p_item) {
		if (root.is_empty()) {
			return S_OK;
		}

		LPWSTR lpw_path = nullptr;
		p_item->GetDisplayName(SIGDN_FILESYSPATH, &lpw_path);
		if (!lpw_path) {
			return S_FALSE;
		}
		String path = String::utf16((const char16_t *)lpw_path).replace("\\", "/").trim_prefix(R"(\\?\)").simplify_path();
		if (!path.begins_with(root.simplify_path())) {
			return S_FALSE;
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnHelp(IFileDialog *) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog *) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog *, IShellItem *, FDE_SHAREVIOLATION_RESPONSE *) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog *pfd) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog *, IShellItem *, FDE_OVERWRITE_RESPONSE *) { return S_OK; }

	// IFileDialogControlEvents methods
	HRESULT STDMETHODCALLTYPE OnItemSelected(IFileDialogCustomize *p_pfdc, DWORD p_ctl_id, DWORD p_item_idx) {
		if (ctls.has(p_ctl_id)) {
			selected[ctls[p_ctl_id]] = (int)p_item_idx;
		}
		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE OnButtonClicked(IFileDialogCustomize *, DWORD) { return S_OK; }
	HRESULT STDMETHODCALLTYPE OnCheckButtonToggled(IFileDialogCustomize *p_pfdc, DWORD p_ctl_id, BOOL p_checked) {
		if (ctls.has(p_ctl_id)) {
			selected[ctls[p_ctl_id]] = (bool)p_checked;
		}
		return S_OK;
	}
	HRESULT STDMETHODCALLTYPE OnControlActivating(IFileDialogCustomize *, DWORD) { return S_OK; }

	Dictionary get_selected() {
		return selected;
	}

	void set_root(const String &p_root) {
		root = p_root;
	}

	void add_option(IFileDialogCustomize *p_pfdc, const String &p_name, const Vector<String> &p_options, int p_default) {
		int gid = ctl_id++;
		int cid = ctl_id++;

		if (p_options.size() == 0) {
			// Add check box.
			p_pfdc->StartVisualGroup(gid, L"");
			p_pfdc->AddCheckButton(cid, (LPCWSTR)p_name.utf16().get_data(), p_default);
			p_pfdc->SetControlState(cid, CDCS_VISIBLE | CDCS_ENABLED);
			p_pfdc->EndVisualGroup();
			selected[p_name] = (bool)p_default;
		} else {
			// Add combo box.
			p_pfdc->StartVisualGroup(gid, (LPCWSTR)p_name.utf16().get_data());
			p_pfdc->AddComboBox(cid);
			p_pfdc->SetControlState(cid, CDCS_VISIBLE | CDCS_ENABLED);
			for (int i = 0; i < p_options.size(); i++) {
				p_pfdc->AddControlItem(cid, i, (LPCWSTR)p_options[i].utf16().get_data());
			}
			p_pfdc->SetSelectedControlItem(cid, p_default);
			p_pfdc->EndVisualGroup();
			selected[p_name] = p_default;
		}
		ctls[cid] = p_name;
	}

	virtual ~FileDialogEventHandler() {}
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

LRESULT CALLBACK WndProcFileDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	DisplayServerWindows *ds_win = static_cast<DisplayServerWindows *>(DisplayServer::get_singleton());
	if (ds_win) {
		return ds_win->WndProcFileDialog(hWnd, uMsg, wParam, lParam);
	} else {
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}
}

LRESULT DisplayServerWindows::WndProcFileDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	MutexLock lock(file_dialog_mutex);
	if (file_dialog_wnd.has(hWnd)) {
		if (file_dialog_wnd[hWnd]->close_requested.is_set()) {
			IPropertyStore *prop_store;
			HRESULT hr = SHGetPropertyStoreForWindow(hWnd, IID_IPropertyStore, (void **)&prop_store);
			if (hr == S_OK) {
				PROPVARIANT val;
				PropVariantInit(&val);
				prop_store->SetValue(PKEY_AppUserModel_ID, val);
				prop_store->Release();
			}
			DestroyWindow(hWnd);
			file_dialog_wnd.erase(hWnd);
		}
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void DisplayServerWindows::_thread_fd_monitor(void *p_ud) {
	DisplayServerWindows *ds = static_cast<DisplayServerWindows *>(get_singleton());
	FileDialogData *fd = (FileDialogData *)p_ud;

	if (fd->mode < 0 && fd->mode >= DisplayServer::FILE_DIALOG_MODE_SAVE_MAX) {
		fd->finished.set();
		return;
	}
	CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

	int64_t x = fd->wrect.position.x;
	int64_t y = fd->wrect.position.y;
	int64_t w = fd->wrect.size.x;
	int64_t h = fd->wrect.size.y;

	WNDCLASSW wc = {};
	wc.lpfnWndProc = (WNDPROC)::WndProcFileDialog;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = L"Engine File Dialog";
	RegisterClassW(&wc);

	HWND hwnd_dialog = CreateWindowExW(WS_EX_APPWINDOW, L"Engine File Dialog", L"", WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
	if (hwnd_dialog) {
		{
			MutexLock lock(ds->file_dialog_mutex);
			ds->file_dialog_wnd[hwnd_dialog] = fd;
		}

		HICON mainwindow_icon = (HICON)SendMessage(fd->hwnd_owner, WM_GETICON, ICON_SMALL, 0);
		if (mainwindow_icon) {
			SendMessage(hwnd_dialog, WM_SETICON, ICON_SMALL, (LPARAM)mainwindow_icon);
		}
		mainwindow_icon = (HICON)SendMessage(fd->hwnd_owner, WM_GETICON, ICON_BIG, 0);
		if (mainwindow_icon) {
			SendMessage(hwnd_dialog, WM_SETICON, ICON_BIG, (LPARAM)mainwindow_icon);
		}
		IPropertyStore *prop_store;
		HRESULT hr = SHGetPropertyStoreForWindow(hwnd_dialog, IID_IPropertyStore, (void **)&prop_store);
		if (hr == S_OK) {
			PROPVARIANT val;
			InitPropVariantFromString((PCWSTR)fd->appid.utf16().get_data(), &val);
			prop_store->SetValue(PKEY_AppUserModel_ID, val);
			prop_store->Release();
		}
	}

	SetCurrentProcessExplicitAppUserModelID((PCWSTR)fd->appid.utf16().get_data());

	Vector<Char16String> filter_names;
	Vector<Char16String> filter_exts;
	for (const String &E : fd->filters) {
		Vector<String> tokens = E.split(";");
		if (tokens.size() >= 1) {
			String flt = tokens[0].strip_edges();
			int filter_slice_count = flt.get_slice_count(",");
			Vector<String> exts;
			for (int j = 0; j < filter_slice_count; j++) {
				String str = (flt.get_slice(",", j).strip_edges());
				if (!str.is_empty()) {
					exts.push_back(str);
				}
			}
			if (!exts.is_empty()) {
				String str = String(";").join(exts);
				filter_exts.push_back(str.utf16());
				if (tokens.size() == 2) {
					filter_names.push_back(tokens[1].strip_edges().utf16());
				} else {
					filter_names.push_back(str.utf16());
				}
			}
		}
	}
	if (filter_names.is_empty()) {
		filter_exts.push_back(String("*.*").utf16());
		filter_names.push_back((RTR("All Files") + " (*.*)").utf16());
	}

	Vector<COMDLG_FILTERSPEC> filters;
	for (int i = 0; i < filter_names.size(); i++) {
		filters.push_back({ (LPCWSTR)filter_names[i].ptr(), (LPCWSTR)filter_exts[i].ptr() });
	}

	HRESULT hr = S_OK;
	IFileDialog *pfd = nullptr;
	if (fd->mode == DisplayServer::FILE_DIALOG_MODE_SAVE_FILE) {
		hr = CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileSaveDialog, (void **)&pfd);
	} else {
		hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_IFileOpenDialog, (void **)&pfd);
	}
	if (SUCCEEDED(hr)) {
		IFileDialogEvents *pfde = nullptr;
		FileDialogEventHandler *event_handler = new FileDialogEventHandler();
		hr = event_handler->QueryInterface(IID_PPV_ARGS(&pfde));

		DWORD cookie = 0;
		hr = pfd->Advise(pfde, &cookie);

		IFileDialogCustomize *pfdc = nullptr;
		hr = pfd->QueryInterface(IID_PPV_ARGS(&pfdc));

		for (int i = 0; i < fd->options.size(); i++) {
			const Dictionary &item = fd->options[i];
			if (!item.has("name") || !item.has("values") || !item.has("default")) {
				continue;
			}
			event_handler->add_option(pfdc, item["name"], item["values"], item["default"]);
		}
		event_handler->set_root(fd->root);

		pfdc->Release();

		DWORD flags;
		pfd->GetOptions(&flags);
		if (fd->mode == DisplayServer::FILE_DIALOG_MODE_OPEN_FILES) {
			flags |= FOS_ALLOWMULTISELECT;
		}
		if (fd->mode == DisplayServer::FILE_DIALOG_MODE_OPEN_DIR) {
			flags |= FOS_PICKFOLDERS;
		}
		if (fd->show_hidden) {
			flags |= FOS_FORCESHOWHIDDEN;
		}
		pfd->SetOptions(flags | FOS_FORCEFILESYSTEM);
		pfd->SetTitle((LPCWSTR)fd->title.utf16().get_data());

		String dir = ProjectSettings::get_singleton()->globalize_path(fd->current_directory);
		if (dir == ".") {
			dir = OS::get_singleton()->get_executable_path().get_base_dir();
		}
		if (dir.is_relative_path() || dir == ".") {
			Char16String current_dir_name;
			size_t str_len = GetCurrentDirectoryW(0, nullptr);
			current_dir_name.resize(str_len + 1);
			GetCurrentDirectoryW(current_dir_name.size(), (LPWSTR)current_dir_name.ptrw());
			if (dir == ".") {
				dir = String::utf16((const char16_t *)current_dir_name.get_data()).trim_prefix(R"(\\?\)").replace("\\", "/");
			} else {
				dir = String::utf16((const char16_t *)current_dir_name.get_data()).trim_prefix(R"(\\?\)").replace("\\", "/").path_join(dir);
			}
		}
		dir = dir.simplify_path();
		dir = dir.trim_prefix(R"(\\?\)").replace("/", "\\");

		IShellItem *shellitem = nullptr;
		hr = SHCreateItemFromParsingName((LPCWSTR)dir.utf16().ptr(), nullptr, IID_IShellItem, (void **)&shellitem);
		if (SUCCEEDED(hr)) {
			pfd->SetDefaultFolder(shellitem);
			pfd->SetFolder(shellitem);
		}

		pfd->SetFileName((LPCWSTR)fd->filename.utf16().get_data());
		pfd->SetFileTypes(filters.size(), filters.ptr());
		pfd->SetFileTypeIndex(0);

		hr = pfd->Show(hwnd_dialog);
		pfd->Unadvise(cookie);

		Dictionary options = event_handler->get_selected();

		pfde->Release();
		event_handler->Release();

		UINT index = 0;
		pfd->GetFileTypeIndex(&index);
		if (index > 0) {
			index = index - 1;
		}

		if (SUCCEEDED(hr)) {
			Vector<String> file_names;

			if (fd->mode == DisplayServer::FILE_DIALOG_MODE_OPEN_FILES) {
				IShellItemArray *results;
				hr = static_cast<IFileOpenDialog *>(pfd)->GetResults(&results);
				if (SUCCEEDED(hr)) {
					DWORD count = 0;
					results->GetCount(&count);
					for (DWORD i = 0; i < count; i++) {
						IShellItem *result;
						results->GetItemAt(i, &result);

						PWSTR file_path = nullptr;
						hr = result->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
						if (SUCCEEDED(hr)) {
							file_names.push_back(String::utf16((const char16_t *)file_path).replace("\\", "/").trim_prefix(R"(\\?\)"));
							CoTaskMemFree(file_path);
						}
						result->Release();
					}
					results->Release();
				}
			} else {
				IShellItem *result;
				hr = pfd->GetResult(&result);
				if (SUCCEEDED(hr)) {
					PWSTR file_path = nullptr;
					hr = result->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
					if (SUCCEEDED(hr)) {
						file_names.push_back(String::utf16((const char16_t *)file_path).replace("\\", "/").trim_prefix(R"(\\?\)"));
						CoTaskMemFree(file_path);
					}
					result->Release();
				}
			}
			if (fd->callback.is_valid()) {
				MutexLock lock(ds->file_dialog_mutex);
				FileDialogCallback cb;
				cb.callback = fd->callback;
				cb.status = true;
				cb.files = file_names;
				cb.index = index;
				cb.options = options;
				cb.opt_in_cb = fd->options_in_cb;
				ds->pending_cbs.push_back(cb);
			}
		} else {
			if (fd->callback.is_valid()) {
				MutexLock lock(ds->file_dialog_mutex);
				FileDialogCallback cb;
				cb.callback = fd->callback;
				cb.status = false;
				cb.files = Vector<String>();
				cb.index = index;
				cb.options = options;
				cb.opt_in_cb = fd->options_in_cb;
				ds->pending_cbs.push_back(cb);
			}
		}
		pfd->Release();
	} else {
		if (fd->callback.is_valid()) {
			MutexLock lock(ds->file_dialog_mutex);
			FileDialogCallback cb;
			cb.callback = fd->callback;
			cb.status = false;
			cb.files = Vector<String>();
			cb.index = 0;
			cb.options = Dictionary();
			cb.opt_in_cb = fd->options_in_cb;
			ds->pending_cbs.push_back(cb);
		}
	}
	{
		MutexLock lock(ds->file_dialog_mutex);
		if (hwnd_dialog && ds->file_dialog_wnd.has(hwnd_dialog)) {
			IPropertyStore *prop_store;
			hr = SHGetPropertyStoreForWindow(hwnd_dialog, IID_IPropertyStore, (void **)&prop_store);
			if (hr == S_OK) {
				PROPVARIANT val;
				PropVariantInit(&val);
				prop_store->SetValue(PKEY_AppUserModel_ID, val);
				prop_store->Release();
			}
			DestroyWindow(hwnd_dialog);
			ds->file_dialog_wnd.erase(hwnd_dialog);
		}
	}
	UnregisterClassW(L"Engine File Dialog", GetModuleHandle(nullptr));
	CoUninitialize();

	fd->finished.set();

	if (fd->window_id != INVALID_WINDOW_ID) {
		callable_mp(DisplayServer::get_singleton(), &DisplayServer::window_move_to_foreground).call_deferred(fd->window_id);
	}
}

Error DisplayServerWindows::_file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback, bool p_options_in_cb) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_INDEX_V(int(p_mode), FILE_DIALOG_MODE_SAVE_MAX, FAILED);

	WindowID window_id = _get_focused_window_or_popup();
	if (!windows.has(window_id)) {
		window_id = MAIN_WINDOW_ID;
	}
	String appname;
	if (Engine::get_singleton()->is_editor_hint()) {
		appname = "Godot.GodotEditor." + String(VERSION_BRANCH);
	} else {
		String name = GLOBAL_GET("application/config/name");
		String version = GLOBAL_GET("application/config/version");
		if (version.is_empty()) {
			version = "0";
		}
		String clean_app_name = name.to_pascal_case();
		for (int i = 0; i < clean_app_name.length(); i++) {
			if (!is_ascii_alphanumeric_char(clean_app_name[i]) && clean_app_name[i] != '_' && clean_app_name[i] != '.') {
				clean_app_name[i] = '_';
			}
		}
		clean_app_name = clean_app_name.substr(0, 120 - version.length()).trim_suffix(".");
		appname = "Godot." + clean_app_name + "." + version;
	}

	FileDialogData *fd = memnew(FileDialogData);
	if (window_id != INVALID_WINDOW_ID) {
		fd->hwnd_owner = windows[window_id].hWnd;
		RECT crect;
		GetWindowRect(fd->hwnd_owner, &crect);
		fd->wrect = Rect2i(crect.left, crect.top, crect.right - crect.left, crect.bottom - crect.top);
	} else {
		fd->hwnd_owner = nullptr;
		fd->wrect = Rect2i(CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT);
	}
	fd->appid = appname;
	fd->title = p_title;
	fd->current_directory = p_current_directory;
	fd->root = p_root;
	fd->filename = p_filename;
	fd->show_hidden = p_show_hidden;
	fd->mode = p_mode;
	fd->window_id = window_id;
	fd->filters = p_filters;
	fd->options = p_options;
	fd->callback = p_callback;
	fd->options_in_cb = p_options_in_cb;
	fd->finished.clear();
	fd->close_requested.clear();

	fd->listener_thread.start(DisplayServerWindows::_thread_fd_monitor, fd);

	file_dialogs.push_back(fd);

	return OK;
}

void DisplayServerWindows::process_file_dialog_callbacks() {
	MutexLock lock(file_dialog_mutex);
	while (!pending_cbs.is_empty()) {
		FileDialogCallback cb = pending_cbs.front()->get();
		pending_cbs.pop_front();

		if (cb.opt_in_cb) {
			Variant ret;
			Callable::CallError ce;
			const Variant *args[4] = { &cb.status, &cb.files, &cb.index, &cb.options };

			cb.callback.callp(args, 4, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Failed to execute file dialog callback: %s.", Variant::get_callable_error_text(cb.callback, args, 4, ce)));
			}
		} else {
			Variant ret;
			Callable::CallError ce;
			const Variant *args[3] = { &cb.status, &cb.files, &cb.index };

			cb.callback.callp(args, 3, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Failed to execute file dialog callback: %s.", Variant::get_callable_error_text(cb.callback, args, 3, ce)));
			}
		}
	}
}

void DisplayServerWindows::beep() const {
	MessageBeep(MB_OK);
}

void DisplayServerWindows::_mouse_update_mode() {
	_THREAD_SAFE_METHOD_

	MouseMode wanted_mouse_mode = mouse_mode_override_enabled
			? mouse_mode_override
			: mouse_mode_base;

	if (mouse_mode == wanted_mouse_mode) {
		// Already in the same mode; do nothing.
		return;
	}

	mouse_mode = wanted_mouse_mode;

	_set_mouse_mode_impl(wanted_mouse_mode);
}

void DisplayServerWindows::mouse_set_mode(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MouseMode::MOUSE_MODE_MAX);
	if (p_mode == mouse_mode_base) {
		return;
	}
	mouse_mode_base = p_mode;
	_mouse_update_mode();
}

DisplayServer::MouseMode DisplayServerWindows::mouse_get_mode() const {
	return mouse_mode;
}

void DisplayServerWindows::mouse_set_mode_override(MouseMode p_mode) {
	ERR_FAIL_INDEX(p_mode, MouseMode::MOUSE_MODE_MAX);
	if (p_mode == mouse_mode_override) {
		return;
	}
	mouse_mode_override = p_mode;
	_mouse_update_mode();
}

DisplayServer::MouseMode DisplayServerWindows::mouse_get_mode_override() const {
	return mouse_mode_override;
}

void DisplayServerWindows::mouse_set_mode_override_enabled(bool p_override_enabled) {
	if (p_override_enabled == mouse_mode_override_enabled) {
		return;
	}
	mouse_mode_override_enabled = p_override_enabled;
	_mouse_update_mode();
}

bool DisplayServerWindows::mouse_is_mode_override_enabled() const {
	return mouse_mode_override_enabled;
}

void DisplayServerWindows::warp_mouse(const Point2i &p_position) {
	_THREAD_SAFE_METHOD_

	WindowID window_id = _get_focused_window_or_popup();

	if (!windows.has(window_id)) {
		return; // No focused window?
	}

	if (mouse_mode == MOUSE_MODE_CAPTURED) {
		old_x = p_position.x;
		old_y = p_position.y;
	} else {
		POINT p;
		p.x = p_position.x;
		p.y = p_position.y;
		ClientToScreen(windows[window_id].hWnd, &p);

		SetCursorPos(p.x, p.y);
	}
}

Point2i DisplayServerWindows::mouse_get_position() const {
	POINT p;
	GetCursorPos(&p);
	return Point2i(p.x, p.y) - _get_screens_origin();
}

BitField<MouseButtonMask> DisplayServerWindows::mouse_get_button_state() const {
	BitField<MouseButtonMask> last_button_state = 0;

	if (GetKeyState(VK_LBUTTON) & (1 << 15)) {
		last_button_state.set_flag(MouseButtonMask::LEFT);
	}
	if (GetKeyState(VK_RBUTTON) & (1 << 15)) {
		last_button_state.set_flag(MouseButtonMask::RIGHT);
	}
	if (GetKeyState(VK_MBUTTON) & (1 << 15)) {
		last_button_state.set_flag(MouseButtonMask::MIDDLE);
	}
	if (GetKeyState(VK_XBUTTON1) & (1 << 15)) {
		last_button_state.set_flag(MouseButtonMask::MB_XBUTTON1);
	}
	if (GetKeyState(VK_XBUTTON2) & (1 << 15)) {
		last_button_state.set_flag(MouseButtonMask::MB_XBUTTON2);
	}

	return last_button_state;
}

void DisplayServerWindows::clipboard_set(const String &p_text) {
	_THREAD_SAFE_METHOD_

	if (!windows.has(MAIN_WINDOW_ID)) {
		return;
	}

	// Convert LF line endings to CRLF in clipboard content.
	// Otherwise, line endings won't be visible when pasted in other software.
	String text = p_text.replace("\r\n", "\n").replace("\n", "\r\n"); // Avoid \r\r\n.

	if (!OpenClipboard(windows[MAIN_WINDOW_ID].hWnd)) {
		ERR_FAIL_MSG("Unable to open clipboard.");
	}
	EmptyClipboard();

	Char16String utf16 = text.utf16();
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (utf16.length() + 1) * sizeof(WCHAR));
	ERR_FAIL_NULL_MSG(mem, "Unable to allocate memory for clipboard contents.");

	LPWSTR lptstrCopy = (LPWSTR)GlobalLock(mem);
	memcpy(lptstrCopy, utf16.get_data(), (utf16.length() + 1) * sizeof(WCHAR));
	GlobalUnlock(mem);

	SetClipboardData(CF_UNICODETEXT, mem);

	// Set the CF_TEXT version (not needed?).
	CharString utf8 = text.utf8();
	mem = GlobalAlloc(GMEM_MOVEABLE, utf8.length() + 1);
	ERR_FAIL_NULL_MSG(mem, "Unable to allocate memory for clipboard contents.");

	LPTSTR ptr = (LPTSTR)GlobalLock(mem);
	memcpy(ptr, utf8.get_data(), utf8.length());
	ptr[utf8.length()] = 0;
	GlobalUnlock(mem);

	SetClipboardData(CF_TEXT, mem);

	CloseClipboard();
}

String DisplayServerWindows::clipboard_get() const {
	_THREAD_SAFE_METHOD_

	if (!windows.has(MAIN_WINDOW_ID)) {
		return String();
	}

	String ret;
	if (!OpenClipboard(windows[MAIN_WINDOW_ID].hWnd)) {
		ERR_FAIL_V_MSG("", "Unable to open clipboard.");
	}

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		HGLOBAL mem = GetClipboardData(CF_UNICODETEXT);
		if (mem != nullptr) {
			LPWSTR ptr = (LPWSTR)GlobalLock(mem);
			if (ptr != nullptr) {
				ret = String::utf16((const char16_t *)ptr);
				GlobalUnlock(mem);
			}
		}

	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		HGLOBAL mem = GetClipboardData(CF_UNICODETEXT);
		if (mem != nullptr) {
			LPTSTR ptr = (LPTSTR)GlobalLock(mem);
			if (ptr != nullptr) {
				ret.parse_utf8((const char *)ptr);
				GlobalUnlock(mem);
			}
		}
	}

	CloseClipboard();

	return ret;
}

Ref<Image> DisplayServerWindows::clipboard_get_image() const {
	Ref<Image> image;
	if (!windows.has(last_focused_window)) {
		return image; // No focused window?
	}
	if (!OpenClipboard(windows[last_focused_window].hWnd)) {
		ERR_FAIL_V_MSG(image, "Unable to open clipboard.");
	}
	UINT png_format = RegisterClipboardFormatA("PNG");
	if (png_format && IsClipboardFormatAvailable(png_format)) {
		HANDLE png_handle = GetClipboardData(png_format);
		if (png_handle) {
			size_t png_size = GlobalSize(png_handle);
			uint8_t *png_data = (uint8_t *)GlobalLock(png_handle);
			image.instantiate();

			PNGDriverCommon::png_to_image(png_data, png_size, false, image);

			GlobalUnlock(png_handle);
		}
	} else if (IsClipboardFormatAvailable(CF_DIB)) {
		HGLOBAL mem = GetClipboardData(CF_DIB);
		if (mem != nullptr) {
			BITMAPINFO *ptr = static_cast<BITMAPINFO *>(GlobalLock(mem));

			if (ptr != nullptr) {
				BITMAPINFOHEADER *info = &ptr->bmiHeader;
				void *dib_bits = (void *)(ptr->bmiColors);

				// Draw DIB image to temporary DC surface and read it back as BGRA8.
				HDC dc = GetDC(nullptr);
				if (dc) {
					HDC hdc = CreateCompatibleDC(dc);
					if (hdc) {
						HBITMAP hbm = CreateCompatibleBitmap(dc, info->biWidth, abs(info->biHeight));
						if (hbm) {
							SelectObject(hdc, hbm);
							SetDIBitsToDevice(hdc, 0, 0, info->biWidth, abs(info->biHeight), 0, 0, 0, abs(info->biHeight), dib_bits, ptr, DIB_RGB_COLORS);

							BITMAPINFO bmp_info = {};
							bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
							bmp_info.bmiHeader.biWidth = info->biWidth;
							bmp_info.bmiHeader.biHeight = -abs(info->biHeight);
							bmp_info.bmiHeader.biPlanes = 1;
							bmp_info.bmiHeader.biBitCount = 32;
							bmp_info.bmiHeader.biCompression = BI_RGB;

							Vector<uint8_t> img_data;
							img_data.resize(info->biWidth * abs(info->biHeight) * 4);
							GetDIBits(hdc, hbm, 0, abs(info->biHeight), img_data.ptrw(), &bmp_info, DIB_RGB_COLORS);

							uint8_t *wr = (uint8_t *)img_data.ptrw();
							for (int i = 0; i < info->biWidth * abs(info->biHeight); i++) {
								SWAP(wr[i * 4 + 0], wr[i * 4 + 2]); // Swap B and R.
								if (info->biBitCount != 32) {
									wr[i * 4 + 3] = 255; // Set A to solid if it's not in the source image.
								}
							}
							image = Image::create_from_data(info->biWidth, abs(info->biHeight), false, Image::Format::FORMAT_RGBA8, img_data);

							DeleteObject(hbm);
						}
						DeleteDC(hdc);
					}
					ReleaseDC(nullptr, dc);
				}
				GlobalUnlock(mem);
			}
		}
	}
	CloseClipboard();

	return image;
}

bool DisplayServerWindows::clipboard_has() const {
	return (IsClipboardFormatAvailable(CF_TEXT) ||
			IsClipboardFormatAvailable(CF_UNICODETEXT) ||
			IsClipboardFormatAvailable(CF_OEMTEXT));
}

bool DisplayServerWindows::clipboard_has_image() const {
	UINT png_format = RegisterClipboardFormatA("PNG");
	return ((png_format && IsClipboardFormatAvailable(png_format)) || IsClipboardFormatAvailable(CF_DIB));
}

typedef struct {
	int count;
	int screen;
	HMONITOR monitor;
} EnumScreenData;

static BOOL CALLBACK _MonitorEnumProcPrim(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumScreenData *data = (EnumScreenData *)dwData;
	if ((lprcMonitor->left == 0) && (lprcMonitor->top == 0)) {
		data->screen = data->count;
		return FALSE;
	}

	data->count++;
	return TRUE;
}

static BOOL CALLBACK _MonitorEnumProcScreen(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumScreenData *data = (EnumScreenData *)dwData;
	if (data->monitor == hMonitor) {
		data->screen = data->count;
	}

	data->count++;
	return TRUE;
}

static BOOL CALLBACK _MonitorEnumProcCount(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	int *data = (int *)dwData;
	(*data)++;
	return TRUE;
}

int DisplayServerWindows::get_screen_count() const {
	_THREAD_SAFE_METHOD_

	int data = 0;
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcCount, (LPARAM)&data);
	return data;
}

int DisplayServerWindows::get_primary_screen() const {
	EnumScreenData data = { 0, 0, nullptr };
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcPrim, (LPARAM)&data);
	return data.screen;
}

int DisplayServerWindows::get_keyboard_focus_screen() const {
	HWND hwnd = GetForegroundWindow();
	if (hwnd) {
		EnumScreenData data = { 0, 0, MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST) };
		EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcScreen, (LPARAM)&data);
		return data.screen;
	} else {
		return get_primary_screen();
	}
}

typedef struct {
	int count;
	int screen;
	Point2 pos;
} EnumPosData;

static BOOL CALLBACK _MonitorEnumProcPos(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumPosData *data = (EnumPosData *)dwData;
	if (data->count == data->screen) {
		data->pos.x = lprcMonitor->left;
		data->pos.y = lprcMonitor->top;
	}

	data->count++;
	return TRUE;
}

static BOOL CALLBACK _MonitorEnumProcOrigin(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumPosData *data = (EnumPosData *)dwData;
	data->pos = data->pos.min(Point2(lprcMonitor->left, lprcMonitor->top));

	return TRUE;
}

Point2i DisplayServerWindows::_get_screens_origin() const {
	_THREAD_SAFE_METHOD_

	EnumPosData data = { 0, 0, Point2() };
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcOrigin, (LPARAM)&data);
	return data.pos;
}

Point2i DisplayServerWindows::screen_get_position(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	EnumPosData data = { 0, p_screen, Point2() };
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcPos, (LPARAM)&data);
	return data.pos - _get_screens_origin();
}

typedef struct {
	int count;
	int screen;
	Size2 size;
} EnumSizeData;

typedef struct {
	int count;
	int screen;
	Rect2i rect;
} EnumRectData;

typedef struct {
	Vector<DISPLAYCONFIG_PATH_INFO> paths;
	Vector<DISPLAYCONFIG_MODE_INFO> modes;
	int count;
	int screen;
	float rate;
} EnumRefreshRateData;

static BOOL CALLBACK _MonitorEnumProcSize(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumSizeData *data = (EnumSizeData *)dwData;
	if (data->count == data->screen) {
		data->size.x = lprcMonitor->right - lprcMonitor->left;
		data->size.y = lprcMonitor->bottom - lprcMonitor->top;
	}

	data->count++;
	return TRUE;
}

Size2i DisplayServerWindows::screen_get_size(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	EnumSizeData data = { 0, p_screen, Size2() };
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcSize, (LPARAM)&data);
	return data.size;
}

static BOOL CALLBACK _MonitorEnumProcUsableSize(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumRectData *data = (EnumRectData *)dwData;
	if (data->count == data->screen) {
		MONITORINFO minfo;
		memset(&minfo, 0, sizeof(MONITORINFO));
		minfo.cbSize = sizeof(MONITORINFO);
		GetMonitorInfoA(hMonitor, &minfo);

		data->rect.position.x = minfo.rcWork.left;
		data->rect.position.y = minfo.rcWork.top;
		data->rect.size.x = minfo.rcWork.right - minfo.rcWork.left;
		data->rect.size.y = minfo.rcWork.bottom - minfo.rcWork.top;
	}

	data->count++;
	return TRUE;
}

static BOOL CALLBACK _MonitorEnumProcRefreshRate(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumRefreshRateData *data = (EnumRefreshRateData *)dwData;
	if (data->count == data->screen) {
		MONITORINFOEXW minfo;
		memset(&minfo, 0, sizeof(minfo));
		minfo.cbSize = sizeof(minfo);
		GetMonitorInfoW(hMonitor, &minfo);

		bool found = false;
		for (const DISPLAYCONFIG_PATH_INFO &path : data->paths) {
			DISPLAYCONFIG_SOURCE_DEVICE_NAME source_name;
			memset(&source_name, 0, sizeof(source_name));
			source_name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
			source_name.header.size = sizeof(source_name);
			source_name.header.adapterId = path.sourceInfo.adapterId;
			source_name.header.id = path.sourceInfo.id;
			if (DisplayConfigGetDeviceInfo(&source_name.header) == ERROR_SUCCESS) {
				if (wcscmp(minfo.szDevice, source_name.viewGdiDeviceName) == 0 && path.targetInfo.refreshRate.Numerator != 0 && path.targetInfo.refreshRate.Denominator != 0) {
					data->rate = (double)path.targetInfo.refreshRate.Numerator / (double)path.targetInfo.refreshRate.Denominator;
					found = true;
					break;
				}
			}
		}
		if (!found) {
			DEVMODEW dm;
			memset(&dm, 0, sizeof(dm));
			dm.dmSize = sizeof(dm);
			EnumDisplaySettingsW(minfo.szDevice, ENUM_CURRENT_SETTINGS, &dm);

			data->rate = dm.dmDisplayFrequency;
		}
	}

	data->count++;
	return TRUE;
}

Rect2i DisplayServerWindows::screen_get_usable_rect(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	EnumRectData data = { 0, p_screen, Rect2i() };
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcUsableSize, (LPARAM)&data);
	data.rect.position -= _get_screens_origin();
	return data.rect;
}

typedef struct {
	int count;
	int screen;
	int dpi;
} EnumDpiData;

enum _MonitorDpiType {
	MDT_Effective_DPI = 0,
	MDT_Angular_DPI = 1,
	MDT_Raw_DPI = 2,
	MDT_Default = MDT_Effective_DPI
};

static int QueryDpiForMonitor(HMONITOR hmon, _MonitorDpiType dpiType = MDT_Default) {
	int dpiX = 96, dpiY = 96;

	static HMODULE Shcore = nullptr;
	typedef HRESULT(WINAPI * GetDPIForMonitor_t)(HMONITOR hmonitor, _MonitorDpiType dpiType, UINT * dpiX, UINT * dpiY);
	static GetDPIForMonitor_t getDPIForMonitor = nullptr;

	if (Shcore == nullptr) {
		Shcore = LoadLibraryW(L"Shcore.dll");
		getDPIForMonitor = Shcore ? (GetDPIForMonitor_t)GetProcAddress(Shcore, "GetDpiForMonitor") : nullptr;

		if ((Shcore == nullptr) || (getDPIForMonitor == nullptr)) {
			if (Shcore) {
				FreeLibrary(Shcore);
			}
			Shcore = (HMODULE)INVALID_HANDLE_VALUE;
		}
	}

	UINT x = 0, y = 0;
	if (hmon && (Shcore != (HMODULE)INVALID_HANDLE_VALUE)) {
		HRESULT hr = getDPIForMonitor(hmon, dpiType /*MDT_Effective_DPI*/, &x, &y);
		if (SUCCEEDED(hr) && (x > 0) && (y > 0)) {
			dpiX = (int)x;
			dpiY = (int)y;
		}
	} else {
		static int overallX = 0, overallY = 0;
		if (overallX <= 0 || overallY <= 0) {
			HDC hdc = GetDC(nullptr);
			if (hdc) {
				overallX = GetDeviceCaps(hdc, LOGPIXELSX);
				overallY = GetDeviceCaps(hdc, LOGPIXELSY);
				ReleaseDC(nullptr, hdc);
			}
		}
		if (overallX > 0 && overallY > 0) {
			dpiX = overallX;
			dpiY = overallY;
		}
	}

	return (dpiX + dpiY) / 2;
}

static BOOL CALLBACK _MonitorEnumProcDpi(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumDpiData *data = (EnumDpiData *)dwData;
	if (data->count == data->screen) {
		data->dpi = QueryDpiForMonitor(hMonitor);
	}

	data->count++;
	return TRUE;
}

int DisplayServerWindows::screen_get_dpi(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	EnumDpiData data = { 0, p_screen, 72 };
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcDpi, (LPARAM)&data);
	return data.dpi;
}

Color DisplayServerWindows::screen_get_pixel(const Point2i &p_position) const {
	Point2i pos = p_position + _get_screens_origin();

	POINT p;
	p.x = pos.x;
	p.y = pos.y;
	if (win81p_LogicalToPhysicalPointForPerMonitorDPI) {
		win81p_LogicalToPhysicalPointForPerMonitorDPI(nullptr, &p);
	}
	HDC dc = GetDC(nullptr);
	if (dc) {
		COLORREF col = GetPixel(dc, p.x, p.y);
		if (col != CLR_INVALID) {
			ReleaseDC(nullptr, dc);
			return Color(float(col & 0x000000FF) / 255.0f, float((col & 0x0000FF00) >> 8) / 255.0f, float((col & 0x00FF0000) >> 16) / 255.0f, 1.0f);
		}
		ReleaseDC(nullptr, dc);
	}

	return Color();
}

Ref<Image> DisplayServerWindows::screen_get_image(int p_screen) const {
	ERR_FAIL_INDEX_V(p_screen, get_screen_count(), Ref<Image>());

	switch (p_screen) {
		case SCREEN_PRIMARY: {
			p_screen = get_primary_screen();
		} break;
		case SCREEN_OF_MAIN_WINDOW: {
			p_screen = window_get_current_screen(MAIN_WINDOW_ID);
		} break;
		default:
			break;
	}

	Point2i pos = screen_get_position(p_screen) + _get_screens_origin();
	Size2i size = screen_get_size(p_screen);

	POINT p1;
	p1.x = pos.x;
	p1.y = pos.y;

	POINT p2;
	p2.x = pos.x + size.x;
	p2.y = pos.y + size.y;
	if (win81p_LogicalToPhysicalPointForPerMonitorDPI) {
		win81p_LogicalToPhysicalPointForPerMonitorDPI(nullptr, &p1);
		win81p_LogicalToPhysicalPointForPerMonitorDPI(nullptr, &p2);
	}

	Ref<Image> img;
	HDC dc = GetDC(nullptr);
	if (dc) {
		HDC hdc = CreateCompatibleDC(dc);
		int width = p2.x - p1.x;
		int height = p2.y - p1.y;
		if (hdc) {
			HBITMAP hbm = CreateCompatibleBitmap(dc, width, height);
			if (hbm) {
				SelectObject(hdc, hbm);
				BitBlt(hdc, 0, 0, width, height, dc, p1.x, p1.y, SRCCOPY);

				BITMAPINFO bmp_info = {};
				bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
				bmp_info.bmiHeader.biWidth = width;
				bmp_info.bmiHeader.biHeight = -height;
				bmp_info.bmiHeader.biPlanes = 1;
				bmp_info.bmiHeader.biBitCount = 32;
				bmp_info.bmiHeader.biCompression = BI_RGB;

				Vector<uint8_t> img_data;
				img_data.resize(width * height * 4);
				GetDIBits(hdc, hbm, 0, height, img_data.ptrw(), &bmp_info, DIB_RGB_COLORS);

				uint8_t *wr = (uint8_t *)img_data.ptrw();
				for (int i = 0; i < width * height; i++) {
					SWAP(wr[i * 4 + 0], wr[i * 4 + 2]); // Swap B and R.
				}
				img = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, img_data);

				DeleteObject(hbm);
			}
			DeleteDC(hdc);
		}
		ReleaseDC(nullptr, dc);
	}

	return img;
}

Ref<Image> DisplayServerWindows::screen_get_image_rect(const Rect2i &p_rect) const {
	Point2i pos = p_rect.position + _get_screens_origin();
	Size2i size = p_rect.size;

	POINT p1;
	p1.x = pos.x;
	p1.y = pos.y;

	POINT p2;
	p2.x = pos.x + size.x;
	p2.y = pos.y + size.y;
	if (win81p_LogicalToPhysicalPointForPerMonitorDPI) {
		win81p_LogicalToPhysicalPointForPerMonitorDPI(0, &p1);
		win81p_LogicalToPhysicalPointForPerMonitorDPI(0, &p2);
	}

	Ref<Image> img;
	HDC dc = GetDC(0);
	if (dc) {
		HDC hdc = CreateCompatibleDC(dc);
		int width = p2.x - p1.x;
		int height = p2.y - p1.y;
		if (hdc) {
			HBITMAP hbm = CreateCompatibleBitmap(dc, width, height);
			if (hbm) {
				SelectObject(hdc, hbm);
				BitBlt(hdc, 0, 0, width, height, dc, p1.x, p1.y, SRCCOPY);

				BITMAPINFO bmp_info = {};
				bmp_info.bmiHeader.biSize = sizeof(bmp_info.bmiHeader);
				bmp_info.bmiHeader.biWidth = width;
				bmp_info.bmiHeader.biHeight = -height;
				bmp_info.bmiHeader.biPlanes = 1;
				bmp_info.bmiHeader.biBitCount = 32;
				bmp_info.bmiHeader.biCompression = BI_RGB;

				Vector<uint8_t> img_data;
				img_data.resize(width * height * 4);
				GetDIBits(hdc, hbm, 0, height, img_data.ptrw(), &bmp_info, DIB_RGB_COLORS);

				uint8_t *wr = (uint8_t *)img_data.ptrw();
				for (int i = 0; i < width * height; i++) {
					SWAP(wr[i * 4 + 0], wr[i * 4 + 2]); // Swap B and R.
				}
				img = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, img_data);

				DeleteObject(hbm);
			}
			DeleteDC(hdc);
		}
		ReleaseDC(NULL, dc);
	}

	return img;
}

float DisplayServerWindows::screen_get_refresh_rate(int p_screen) const {
	_THREAD_SAFE_METHOD_

	p_screen = _get_screen_index(p_screen);
	EnumRefreshRateData data = { Vector<DISPLAYCONFIG_PATH_INFO>(), Vector<DISPLAYCONFIG_MODE_INFO>(), 0, p_screen, SCREEN_REFRESH_RATE_FALLBACK };

	uint32_t path_count = 0;
	uint32_t mode_count = 0;
	if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) == ERROR_SUCCESS) {
		data.paths.resize(path_count);
		data.modes.resize(mode_count);
		if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, data.paths.ptrw(), &mode_count, data.modes.ptrw(), nullptr) != ERROR_SUCCESS) {
			data.paths.clear();
			data.modes.clear();
		}
	}

	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcRefreshRate, (LPARAM)&data);
	return data.rate;
}

void DisplayServerWindows::screen_set_keep_on(bool p_enable) {
	if (keep_screen_on == p_enable) {
		return;
	}

	if (p_enable) {
		const String reason = "Godot Engine running with display/window/energy_saving/keep_screen_on = true";
		Char16String reason_utf16 = reason.utf16();
		REASON_CONTEXT context;
		context.Version = POWER_REQUEST_CONTEXT_VERSION;
		context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
		context.Reason.SimpleReasonString = (LPWSTR)(reason_utf16.ptrw());
		power_request = PowerCreateRequest(&context);
		if (power_request == INVALID_HANDLE_VALUE) {
			print_error("Failed to enable screen_keep_on.");
			return;
		}
		if (PowerSetRequest(power_request, POWER_REQUEST_TYPE::PowerRequestSystemRequired) == 0) {
			print_error("Failed to request system sleep override.");
			return;
		}
		if (PowerSetRequest(power_request, POWER_REQUEST_TYPE::PowerRequestDisplayRequired) == 0) {
			print_error("Failed to request display timeout override.");
			return;
		}
	} else {
		PowerClearRequest(power_request, POWER_REQUEST_TYPE::PowerRequestSystemRequired);
		PowerClearRequest(power_request, POWER_REQUEST_TYPE::PowerRequestDisplayRequired);
		CloseHandle(power_request);
		power_request = nullptr;
	}

	keep_screen_on = p_enable;
}

bool DisplayServerWindows::screen_is_kept_on() const {
	return keep_screen_on;
}

Vector<DisplayServer::WindowID> DisplayServerWindows::get_window_list() const {
	_THREAD_SAFE_METHOD_

	Vector<DisplayServer::WindowID> ret;
	for (const KeyValue<WindowID, WindowData> &E : windows) {
		ret.push_back(E.key);
	}
	return ret;
}

DisplayServer::WindowID DisplayServerWindows::get_window_at_screen_position(const Point2i &p_position) const {
	Point2i offset = _get_screens_origin();
	POINT p;
	p.x = p_position.x + offset.x;
	p.y = p_position.y + offset.y;
	HWND hwnd = WindowFromPoint(p);
	for (const KeyValue<WindowID, WindowData> &E : windows) {
		if (E.value.hWnd == hwnd) {
			return E.key;
		}
	}

	return INVALID_WINDOW_ID;
}

DisplayServer::WindowID DisplayServerWindows::create_sub_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect, bool p_exclusive, WindowID p_transient_parent) {
	_THREAD_SAFE_METHOD_

	WindowID window_id = _create_window(p_mode, p_vsync_mode, p_flags, p_rect, p_exclusive, p_transient_parent, NULL);
	ERR_FAIL_COND_V_MSG(window_id == INVALID_WINDOW_ID, INVALID_WINDOW_ID, "Failed to create sub window.");

	WindowData &wd = windows[window_id];

	if (p_flags & WINDOW_FLAG_RESIZE_DISABLED_BIT) {
		wd.resizable = false;
	}
	if (p_flags & WINDOW_FLAG_BORDERLESS_BIT) {
		wd.borderless = true;
	}
	if (p_flags & WINDOW_FLAG_ALWAYS_ON_TOP_BIT && p_mode != WINDOW_MODE_FULLSCREEN && p_mode != WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
		wd.always_on_top = true;
	}
	if (p_flags & WINDOW_FLAG_SHARP_CORNERS_BIT) {
		wd.sharp_corners = true;
	}
	if (p_flags & WINDOW_FLAG_NO_FOCUS_BIT) {
		wd.no_focus = true;
	}
	if (p_flags & WINDOW_FLAG_MOUSE_PASSTHROUGH_BIT) {
		wd.mpass = true;
	}
	if (p_flags & WINDOW_FLAG_EXCLUDE_FROM_CAPTURE_BIT) {
		wd.hide_from_capture = true;
		if (os_ver.dwBuildNumber >= 19041) {
			SetWindowDisplayAffinity(wd.hWnd, WDA_EXCLUDEFROMCAPTURE);
		} else {
			SetWindowDisplayAffinity(wd.hWnd, WDA_MONITOR);
		}
	}
	if (p_flags & WINDOW_FLAG_POPUP_BIT) {
		wd.is_popup = true;
	}
	if (p_flags & WINDOW_FLAG_TRANSPARENT_BIT) {
		if (OS::get_singleton()->is_layered_allowed()) {
			DWM_BLURBEHIND bb;
			ZeroMemory(&bb, sizeof(bb));
			HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
			bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
			bb.hRgnBlur = hRgn;
			bb.fEnable = TRUE;
			DwmEnableBlurBehindWindow(wd.hWnd, &bb);
		}

		wd.layered_window = true;
	}

	// Inherit icons from MAIN_WINDOW for all sub windows.
	HICON mainwindow_icon = (HICON)SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_GETICON, ICON_SMALL, 0);
	if (mainwindow_icon) {
		SendMessage(windows[window_id].hWnd, WM_SETICON, ICON_SMALL, (LPARAM)mainwindow_icon);
	}
	mainwindow_icon = (HICON)SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_GETICON, ICON_BIG, 0);
	if (mainwindow_icon) {
		SendMessage(windows[window_id].hWnd, WM_SETICON, ICON_BIG, (LPARAM)mainwindow_icon);
	}
#ifdef RD_ENABLED
	if (rendering_device) {
		rendering_device->screen_create(window_id);
	}
#endif
	wd.initialized = true;
	return window_id;
}

bool DisplayServerWindows::_is_always_on_top_recursive(WindowID p_window) const {
	ERR_FAIL_COND_V(!windows.has(p_window), false);

	const WindowData &wd = windows[p_window];
	if (wd.always_on_top) {
		return true;
	}

	if (wd.transient_parent != INVALID_WINDOW_ID) {
		return _is_always_on_top_recursive(wd.transient_parent);
	}

	return false;
}

void DisplayServerWindows::show_window(WindowID p_id) {
	ERR_FAIL_COND(!windows.has(p_id));

	WindowData &wd = windows[p_id];
	popup_open(p_id);

	if (p_id != MAIN_WINDOW_ID) {
		_update_window_style(p_id);
	}

	if (wd.maximized) {
		ShowWindow(wd.hWnd, SW_SHOWMAXIMIZED);
		SetForegroundWindow(wd.hWnd); // Slightly higher priority.
		SetFocus(wd.hWnd); // Set keyboard focus.
	} else if (wd.minimized) {
		ShowWindow(wd.hWnd, SW_SHOWMINIMIZED);
	} else if (wd.no_focus) {
		// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow
		ShowWindow(wd.hWnd, SW_SHOWNA);
	} else if (wd.is_popup) {
		ShowWindow(wd.hWnd, SW_SHOWNA);
		SetFocus(wd.hWnd); // Set keyboard focus.
	} else {
		ShowWindow(wd.hWnd, SW_SHOW);
		SetForegroundWindow(wd.hWnd); // Slightly higher priority.
		SetFocus(wd.hWnd); // Set keyboard focus.
	}
	if (_is_always_on_top_recursive(p_id)) {
		SetWindowPos(wd.hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | ((wd.no_focus || wd.is_popup) ? SWP_NOACTIVATE : 0));
	}
}

void DisplayServerWindows::delete_sub_window(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	ERR_FAIL_COND_MSG(p_window == MAIN_WINDOW_ID, "Main window cannot be deleted.");

	popup_close(p_window);

	WindowData &wd = windows[p_window];

	IPropertyStore *prop_store;
	HRESULT hr = SHGetPropertyStoreForWindow(wd.hWnd, IID_IPropertyStore, (void **)&prop_store);
	if (hr == S_OK) {
		PROPVARIANT val;
		PropVariantInit(&val);
		prop_store->SetValue(PKEY_AppUserModel_ID, val);
		prop_store->Release();
	}

	while (wd.transient_children.size()) {
		window_set_transient(*wd.transient_children.begin(), INVALID_WINDOW_ID);
	}

	if (wd.transient_parent != INVALID_WINDOW_ID) {
		window_set_transient(p_window, INVALID_WINDOW_ID);
	}

#ifdef RD_ENABLED
	if (rendering_device) {
		rendering_device->screen_free(p_window);
	}

	if (rendering_context) {
		rendering_context->window_destroy(p_window);
	}
#endif
#ifdef GLES3_ENABLED
	if (gl_manager_angle) {
		gl_manager_angle->window_destroy(p_window);
	}
	if (gl_manager_native) {
		gl_manager_native->window_destroy(p_window);
	}
#endif

	if ((tablet_get_current_driver() == "wintab") && wintab_available && wd.wtctx) {
		wintab_WTClose(wd.wtctx);
		wd.wtctx = nullptr;
	}

	if (wd.drop_target != nullptr) {
		RevokeDragDrop(wd.hWnd);
		wd.drop_target->Release();
	}

	DestroyWindow(wd.hWnd);
	windows.erase(p_window);

	if (last_focused_window == p_window) {
		last_focused_window = INVALID_WINDOW_ID;
	}
}

void DisplayServerWindows::gl_window_make_current(DisplayServer::WindowID p_window_id) {
#if defined(GLES3_ENABLED)
	if (gl_manager_angle) {
		gl_manager_angle->window_make_current(p_window_id);
	}
	if (gl_manager_native) {
		gl_manager_native->window_make_current(p_window_id);
	}
#endif
}

int64_t DisplayServerWindows::window_get_native_handle(HandleType p_handle_type, WindowID p_window) const {
	ERR_FAIL_COND_V(!windows.has(p_window), 0);
	switch (p_handle_type) {
		case DISPLAY_HANDLE: {
			return 0; // Not supported.
		}
		case WINDOW_HANDLE: {
			return (int64_t)windows[p_window].hWnd;
		}
#if defined(GLES3_ENABLED)
		case WINDOW_VIEW: {
			if (gl_manager_native) {
				return (int64_t)gl_manager_native->get_hdc(p_window);
			} else {
				return (int64_t)GetDC(windows[p_window].hWnd);
			}
		}
		case OPENGL_CONTEXT: {
			if (gl_manager_native) {
				return (int64_t)gl_manager_native->get_hglrc(p_window);
			}
			if (gl_manager_angle) {
				return (int64_t)gl_manager_angle->get_context(p_window);
			}
			return 0;
		}
		case EGL_DISPLAY: {
			if (gl_manager_angle) {
				return (int64_t)gl_manager_angle->get_display(p_window);
			}
			return 0;
		}
		case EGL_CONFIG: {
			if (gl_manager_angle) {
				return (int64_t)gl_manager_angle->get_config(p_window);
			}
			return 0;
		}
#endif
		default: {
			return 0;
		}
	}
}

void DisplayServerWindows::window_attach_instance_id(ObjectID p_instance, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].instance_id = p_instance;
}

ObjectID DisplayServerWindows::window_get_attached_instance_id(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), ObjectID());
	return windows[p_window].instance_id;
}

void DisplayServerWindows::window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].rect_changed_callback = p_callable;
}

void DisplayServerWindows::window_set_window_event_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].event_callback = p_callable;
}

void DisplayServerWindows::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].input_event_callback = p_callable;
}

void DisplayServerWindows::window_set_input_text_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].input_text_callback = p_callable;
}

void DisplayServerWindows::window_set_drop_files_callback(const Callable &p_callable, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &window_data = windows[p_window];

	window_data.drop_files_callback = p_callable;

	if (window_data.drop_target == nullptr) {
		window_data.drop_target = memnew(DropTargetWindows(&window_data));
		ERR_FAIL_COND(RegisterDragDrop(window_data.hWnd, window_data.drop_target) != S_OK);
	}
}

void DisplayServerWindows::window_set_title(const String &p_title, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	SetWindowTextW(windows[p_window].hWnd, (LPCWSTR)(p_title.utf16().get_data()));
}

Size2i DisplayServerWindows::window_get_title_size(const String &p_title, WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	Size2i size;
	ERR_FAIL_COND_V(!windows.has(p_window), size);

	const WindowData &wd = windows[p_window];
	if (wd.fullscreen || wd.minimized || wd.borderless) {
		return size;
	}

	HDC hdc = GetDCEx(wd.hWnd, nullptr, DCX_WINDOW);
	if (hdc) {
		Char16String s = p_title.utf16();
		SIZE text_size;
		if (GetTextExtentPoint32W(hdc, (LPCWSTR)(s.get_data()), s.length(), &text_size)) {
			size.x = text_size.cx;
			size.y = text_size.cy;
		}

		ReleaseDC(wd.hWnd, hdc);
	}
	RECT rect;
	if (DwmGetWindowAttribute(wd.hWnd, DWMWA_CAPTION_BUTTON_BOUNDS, &rect, sizeof(RECT)) == S_OK) {
		if (rect.right - rect.left > 0) {
			ClientToScreen(wd.hWnd, (POINT *)&rect.left);
			ClientToScreen(wd.hWnd, (POINT *)&rect.right);

			if (win81p_PhysicalToLogicalPointForPerMonitorDPI) {
				win81p_PhysicalToLogicalPointForPerMonitorDPI(nullptr, (POINT *)&rect.left);
				win81p_PhysicalToLogicalPointForPerMonitorDPI(nullptr, (POINT *)&rect.right);
			}

			size.x += (rect.right - rect.left);
			size.y = MAX(size.y, rect.bottom - rect.top);
		}
	}
	if (icon.is_valid()) {
		size.x += 32;
	} else {
		size.x += 16;
	}
	return size;
}

void DisplayServerWindows::window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	windows[p_window].mpath = p_region;
	_update_window_mouse_passthrough(p_window);
}

void DisplayServerWindows::_update_window_mouse_passthrough(WindowID p_window) {
	ERR_FAIL_COND(!windows.has(p_window));
	if (windows[p_window].mpass || windows[p_window].mpath.size() == 0) {
		SetWindowRgn(windows[p_window].hWnd, nullptr, FALSE);
	} else {
		POINT *points = (POINT *)memalloc(sizeof(POINT) * windows[p_window].mpath.size());
		for (int i = 0; i < windows[p_window].mpath.size(); i++) {
			if (windows[p_window].borderless) {
				points[i].x = windows[p_window].mpath[i].x;
				points[i].y = windows[p_window].mpath[i].y;
			} else {
				points[i].x = windows[p_window].mpath[i].x + GetSystemMetrics(SM_CXSIZEFRAME);
				points[i].y = windows[p_window].mpath[i].y + GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CYCAPTION);
			}
		}

		HRGN region = CreatePolygonRgn(points, windows[p_window].mpath.size(), ALTERNATE);
		SetWindowRgn(windows[p_window].hWnd, region, FALSE);
		memfree(points);
	}
}

int DisplayServerWindows::window_get_current_screen(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), -1);

	EnumScreenData data = { 0, 0, MonitorFromWindow(windows[p_window].hWnd, MONITOR_DEFAULTTONEAREST) };
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcScreen, (LPARAM)&data);
	return data.screen;
}

void DisplayServerWindows::window_set_current_screen(int p_screen, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	ERR_FAIL_INDEX(p_screen, get_screen_count());

	if (window_get_current_screen(p_window) == p_screen) {
		return;
	}
	const WindowData &wd = windows[p_window];
	ERR_FAIL_COND_MSG(wd.parent_hwnd, "Embedded window can't be moved to another screen.");
	if (wd.fullscreen) {
		Point2 pos = screen_get_position(p_screen) + _get_screens_origin();
		Size2 size = screen_get_size(p_screen);

		MoveWindow(wd.hWnd, pos.x, pos.y, size.width, size.height, TRUE);
	} else if (wd.maximized) {
		Point2 pos = screen_get_position(p_screen) + _get_screens_origin();
		Size2 size = screen_get_size(p_screen);

		ShowWindow(wd.hWnd, SW_RESTORE);
		MoveWindow(wd.hWnd, pos.x, pos.y, size.width, size.height, TRUE);
		ShowWindow(wd.hWnd, SW_MAXIMIZE);
	} else {
		Rect2i srect = screen_get_usable_rect(p_screen);
		Point2i wpos = window_get_position(p_window) - screen_get_position(window_get_current_screen(p_window));
		Size2i wsize = window_get_size(p_window);
		wpos += srect.position;

		wpos = wpos.clamp(srect.position, srect.position + srect.size - wsize / 3);
		window_set_position(wpos, p_window);
	}
}

Point2i DisplayServerWindows::window_get_position(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Point2i());
	const WindowData &wd = windows[p_window];

	if (wd.minimized) {
		return wd.last_pos;
	}

	POINT point;
	point.x = 0;
	point.y = 0;

	ClientToScreen(wd.hWnd, &point);

	return Point2i(point.x, point.y) - _get_screens_origin();
}

Point2i DisplayServerWindows::window_get_position_with_decorations(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Point2i());
	const WindowData &wd = windows[p_window];

	if (wd.minimized) {
		return wd.last_pos;
	}

	RECT r;
	if (GetWindowRect(wd.hWnd, &r)) {
		return Point2i(r.left, r.top) - _get_screens_origin();
	}

	return Point2i();
}

void DisplayServerWindows::_update_real_mouse_position(WindowID p_window) {
	ERR_FAIL_COND(!windows.has(p_window));

	POINT mouse_pos;
	if (GetCursorPos(&mouse_pos) && ScreenToClient(windows[p_window].hWnd, &mouse_pos)) {
		if (mouse_pos.x > 0 && mouse_pos.y > 0 && mouse_pos.x <= windows[p_window].width && mouse_pos.y <= windows[p_window].height) {
			old_x = mouse_pos.x;
			old_y = mouse_pos.y;
			old_invalid = false;
			Input::get_singleton()->set_mouse_position(Point2i(mouse_pos.x, mouse_pos.y));
		}
	}
}

void DisplayServerWindows::window_set_position(const Point2i &p_position, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	ERR_FAIL_COND_MSG(wd.parent_hwnd, "Embedded window can't be moved.");

	if (wd.fullscreen || wd.maximized) {
		return;
	}

	Point2i offset = _get_screens_origin();

	RECT rc;
	rc.left = p_position.x + offset.x;
	rc.right = p_position.x + wd.width + offset.x;
	rc.bottom = p_position.y + wd.height + offset.y;
	rc.top = p_position.y + offset.y;

	const DWORD style = GetWindowLongPtr(wd.hWnd, GWL_STYLE);
	const DWORD exStyle = GetWindowLongPtr(wd.hWnd, GWL_EXSTYLE);

	AdjustWindowRectEx(&rc, style, false, exStyle);
	MoveWindow(wd.hWnd, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, TRUE);

	wd.last_pos = p_position;
	_update_real_mouse_position(p_window);
}

void DisplayServerWindows::window_set_exclusive(WindowID p_window, bool p_exclusive) {
	_THREAD_SAFE_METHOD_
	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	if (wd.exclusive != p_exclusive) {
		wd.exclusive = p_exclusive;
		if (wd.transient_parent != INVALID_WINDOW_ID) {
			if (wd.exclusive) {
				WindowData &wd_parent = windows[wd.transient_parent];
				SetWindowLongPtr(wd.hWnd, GWLP_HWNDPARENT, (LONG_PTR)wd_parent.hWnd);
			} else {
				SetWindowLongPtr(wd.hWnd, GWLP_HWNDPARENT, (LONG_PTR) nullptr);
			}
		}
	}
}

void DisplayServerWindows::window_set_transient(WindowID p_window, WindowID p_parent) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(p_window == p_parent);
	ERR_FAIL_COND(!windows.has(p_window));

	WindowData &wd_window = windows[p_window];

	ERR_FAIL_COND(wd_window.transient_parent == p_parent);
	ERR_FAIL_COND_MSG(wd_window.always_on_top, "Windows with the 'on top' can't become transient.");

	if (p_parent == INVALID_WINDOW_ID) {
		// Remove transient.

		ERR_FAIL_COND(wd_window.transient_parent == INVALID_WINDOW_ID);
		ERR_FAIL_COND(!windows.has(wd_window.transient_parent));

		WindowData &wd_parent = windows[wd_window.transient_parent];

		wd_window.transient_parent = INVALID_WINDOW_ID;
		wd_parent.transient_children.erase(p_window);

		if (wd_window.exclusive) {
			SetWindowLongPtr(wd_window.hWnd, GWLP_HWNDPARENT, (LONG_PTR) nullptr);
		}
	} else {
		ERR_FAIL_COND(!windows.has(p_parent));
		ERR_FAIL_COND_MSG(wd_window.transient_parent != INVALID_WINDOW_ID, "Window already has a transient parent");
		WindowData &wd_parent = windows[p_parent];

		wd_window.transient_parent = p_parent;
		wd_parent.transient_children.insert(p_window);

		if (wd_window.exclusive) {
			SetWindowLongPtr(wd_window.hWnd, GWLP_HWNDPARENT, (LONG_PTR)wd_parent.hWnd);
		}
	}
}

void DisplayServerWindows::window_set_max_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	ERR_FAIL_COND_MSG(wd.parent_hwnd, "Embedded windows can't have a maximum size.");

	if ((p_size != Size2()) && ((p_size.x < wd.min_size.x) || (p_size.y < wd.min_size.y))) {
		ERR_PRINT("Maximum window size can't be smaller than minimum window size!");
		return;
	}
	wd.max_size = p_size;
}

Size2i DisplayServerWindows::window_get_max_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];
	return wd.max_size;
}

void DisplayServerWindows::window_set_min_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	ERR_FAIL_COND_MSG(wd.parent_hwnd, "Embedded windows can't have a minimum size.");

	if ((p_size != Size2()) && (wd.max_size != Size2()) && ((p_size.x > wd.max_size.x) || (p_size.y > wd.max_size.y))) {
		ERR_PRINT("Minimum window size can't be larger than maximum window size!");
		return;
	}
	wd.min_size = p_size;
}

Size2i DisplayServerWindows::window_get_min_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];
	return wd.min_size;
}

void DisplayServerWindows::window_set_size(const Size2i p_size, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	ERR_FAIL_COND_MSG(wd.parent_hwnd, "Embedded window can't be resized.");

	if (wd.fullscreen || wd.maximized) {
		return;
	}

	int w = p_size.width;
	int h = p_size.height;
	RECT rect;
	GetWindowRect(wd.hWnd, &rect);

	if (!wd.borderless) {
		RECT crect;
		GetClientRect(wd.hWnd, &crect);

		w += (rect.right - rect.left) - (crect.right - crect.left);
		h += (rect.bottom - rect.top) - (crect.bottom - crect.top);
	}

	MoveWindow(wd.hWnd, rect.left, rect.top, w, h, TRUE);
}

Size2i DisplayServerWindows::window_get_size(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];

	// GetClientRect() returns a zero rect for a minimized window, so we need to get the size in another way.
	if (wd.minimized) {
		return Size2(wd.width, wd.height);
	}

	RECT r;
	if (GetClientRect(wd.hWnd, &r)) { // Retrieves area inside of window border, including decoration.
		return Size2(r.right - r.left, r.bottom - r.top);
	}
	return Size2();
}

Size2i DisplayServerWindows::window_get_size_with_decorations(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Size2i());
	const WindowData &wd = windows[p_window];

	RECT r;
	if (GetWindowRect(wd.hWnd, &r)) { // Retrieves area inside of window border, including decoration.
		return Size2(r.right - r.left, r.bottom - r.top);
	}
	return Size2();
}

void DisplayServerWindows::_get_window_style(bool p_main_window, bool p_initialized, bool p_fullscreen, bool p_multiwindow_fs, bool p_borderless, bool p_resizable, bool p_minimized, bool p_maximized, bool p_maximized_fs, bool p_no_activate_focus, bool p_embed_child, DWORD &r_style, DWORD &r_style_ex) {
	// Windows docs for window styles:
	// https://docs.microsoft.com/en-us/windows/win32/winmsg/window-styles
	// https://docs.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles

	r_style = 0;
	r_style_ex = WS_EX_WINDOWEDGE;
	if (p_main_window) {
		// When embedded, we don't want the window to have WS_EX_APPWINDOW because it will
		// show the embedded process in the taskbar and Alt-Tab.
		if (!p_embed_child) {
			r_style_ex |= WS_EX_APPWINDOW;
		}
		if (p_initialized) {
			r_style |= WS_VISIBLE;
		}
	}

	if (p_embed_child) {
		r_style |= WS_POPUP;
	} else if (p_fullscreen || p_borderless) {
		r_style |= WS_POPUP; // p_borderless was WS_EX_TOOLWINDOW in the past.
		if (p_minimized) {
			r_style |= WS_MINIMIZE;
		} else if (p_maximized) {
			r_style |= WS_MAXIMIZE;
		}
		if (!p_fullscreen) {
			r_style |= WS_SYSMENU | WS_MINIMIZEBOX;

			if (p_resizable) {
				r_style |= WS_MAXIMIZEBOX;
			}
		}
		if ((p_fullscreen && p_multiwindow_fs) || p_maximized_fs) {
			r_style |= WS_BORDER; // Allows child windows to be displayed on top of full screen.
		}
	} else {
		if (p_resizable) {
			if (p_minimized) {
				r_style = WS_OVERLAPPEDWINDOW | WS_MINIMIZE;
			} else if (p_maximized) {
				r_style = WS_OVERLAPPEDWINDOW | WS_MAXIMIZE;
			} else {
				r_style = WS_OVERLAPPEDWINDOW;
			}
		} else {
			if (p_minimized) {
				r_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MINIMIZE;
			} else {
				r_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
			}
		}
	}

	if (p_no_activate_focus && !p_embed_child) {
		r_style_ex |= WS_EX_TOPMOST | WS_EX_NOACTIVATE;
	}

	if (!p_borderless && !p_no_activate_focus && p_initialized) {
		r_style |= WS_VISIBLE;
	}

	r_style |= WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	r_style_ex |= WS_EX_ACCEPTFILES;

	if (OS::get_singleton()->get_current_rendering_driver_name() == "d3d12") {
		r_style_ex |= WS_EX_NOREDIRECTIONBITMAP;
	}
}

void DisplayServerWindows::_update_window_style(WindowID p_window, bool p_repaint) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	DWORD style = 0;
	DWORD style_ex = 0;

	_get_window_style(p_window == MAIN_WINDOW_ID, wd.initialized, wd.fullscreen, wd.multiwindow_fs, wd.borderless, wd.resizable, wd.minimized, wd.maximized, wd.maximized_fs, wd.no_focus || wd.is_popup, wd.parent_hwnd, style, style_ex);

	SetWindowLongPtr(wd.hWnd, GWL_STYLE, style);
	SetWindowLongPtr(wd.hWnd, GWL_EXSTYLE, style_ex);

	if (icon.is_valid()) {
		set_icon(icon);
	}

	SetWindowPos(wd.hWnd, _is_always_on_top_recursive(p_window) ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | ((wd.no_focus || wd.is_popup) ? SWP_NOACTIVATE : 0));

	if (p_repaint) {
		RECT rect;
		GetWindowRect(wd.hWnd, &rect);

		MoveWindow(wd.hWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}
}

void DisplayServerWindows::window_set_mode(WindowMode p_mode, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	ERR_FAIL_COND_MSG(p_mode != WINDOW_MODE_WINDOWED && wd.parent_hwnd, "Embedded window only supports Windowed mode.");

	bool was_fullscreen = wd.fullscreen;
	wd.was_fullscreen_pre_min = false;

	if (wd.fullscreen && p_mode != WINDOW_MODE_FULLSCREEN && p_mode != WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
		RECT rect;

		wd.fullscreen = false;
		wd.multiwindow_fs = false;

		// Restore previous maximized state.
		wd.maximized = wd.was_maximized_pre_fs;

		_update_window_style(p_window, false);

		// Restore window rect after exiting fullscreen.
		if (wd.pre_fs_valid) {
			rect = wd.pre_fs_rect;
		} else {
			rect.left = 0;
			rect.right = wd.width;
			rect.top = 0;
			rect.bottom = wd.height;
		}

		ShowWindow(wd.hWnd, SW_RESTORE);
		MoveWindow(wd.hWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);

		if (restore_mouse_trails > 1) {
			SystemParametersInfoA(SPI_SETMOUSETRAILS, restore_mouse_trails, nullptr, 0);
			restore_mouse_trails = 0;
		}
	}

	if (p_mode == WINDOW_MODE_WINDOWED) {
		ShowWindow(wd.hWnd, SW_NORMAL);
		wd.maximized = false;
		wd.minimized = false;
	}

	if (p_mode == WINDOW_MODE_MAXIMIZED) {
		ShowWindow(wd.hWnd, SW_MAXIMIZE);
		wd.maximized = true;
		wd.minimized = false;
	}

	if (p_mode == WINDOW_MODE_MINIMIZED) {
		ShowWindow(wd.hWnd, SW_MINIMIZE);
		wd.maximized = false;
		wd.minimized = true;
		wd.was_fullscreen_pre_min = was_fullscreen;
	}

	if (p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
		wd.multiwindow_fs = false;
		_update_window_style(p_window, false);
	} else {
		wd.multiwindow_fs = true;
		_update_window_style(p_window, false);
	}

	if ((p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) && !wd.fullscreen) {
		if (wd.minimized || wd.maximized) {
			ShowWindow(wd.hWnd, SW_RESTORE);
		}

		// Save previous maximized stare.
		wd.was_maximized_pre_fs = wd.maximized;

		if (!was_fullscreen) {
			// Save non-fullscreen rect before entering fullscreen.
			GetWindowRect(wd.hWnd, &wd.pre_fs_rect);
			wd.pre_fs_valid = true;
		}

		int cs = window_get_current_screen(p_window);
		Point2 pos = screen_get_position(cs) + _get_screens_origin();
		Size2 size = screen_get_size(cs);

		wd.fullscreen = true;
		wd.maximized = false;
		wd.minimized = false;

		_update_window_style(p_window, false);

		MoveWindow(wd.hWnd, pos.x, pos.y, size.width, size.height, TRUE);

		// If the user has mouse trails enabled in windows, then sometimes the cursor disappears in fullscreen mode.
		// Save number of trails so we can restore when exiting, then turn off mouse trails
		SystemParametersInfoA(SPI_GETMOUSETRAILS, 0, &restore_mouse_trails, 0);
		if (restore_mouse_trails > 1) {
			SystemParametersInfoA(SPI_SETMOUSETRAILS, 0, nullptr, 0);
		}
	}
}

DisplayServer::WindowMode DisplayServerWindows::window_get_mode(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), WINDOW_MODE_WINDOWED);
	const WindowData &wd = windows[p_window];

	if (wd.fullscreen) {
		if (wd.multiwindow_fs) {
			return WINDOW_MODE_FULLSCREEN;
		} else {
			return WINDOW_MODE_EXCLUSIVE_FULLSCREEN;
		}
	} else if (wd.minimized) {
		return WINDOW_MODE_MINIMIZED;
	} else if (wd.maximized) {
		return WINDOW_MODE_MAXIMIZED;
	} else {
		return WINDOW_MODE_WINDOWED;
	}
}

bool DisplayServerWindows::window_is_maximize_allowed(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), false);

	// FIXME: Implement this, or confirm that it should always be true.

	return true;
}

void DisplayServerWindows::window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	switch (p_flag) {
		case WINDOW_FLAG_RESIZE_DISABLED: {
			ERR_FAIL_COND_MSG(p_enabled && wd.parent_hwnd, "Embedded window resize can't be disabled.");
			wd.resizable = !p_enabled;
			_update_window_style(p_window);
		} break;
		case WINDOW_FLAG_BORDERLESS: {
			wd.borderless = p_enabled;
			_update_window_style(p_window);
			_update_window_mouse_passthrough(p_window);
			ShowWindow(wd.hWnd, (wd.no_focus || wd.is_popup) ? SW_SHOWNOACTIVATE : SW_SHOW); // Show the window.
		} break;
		case WINDOW_FLAG_ALWAYS_ON_TOP: {
			ERR_FAIL_COND_MSG(wd.transient_parent != INVALID_WINDOW_ID && p_enabled, "Transient windows can't become on top.");
			ERR_FAIL_COND_MSG(p_enabled && wd.parent_hwnd, "Embedded window can't become on top.");
			wd.always_on_top = p_enabled;
			_update_window_style(p_window);
		} break;
		case WINDOW_FLAG_SHARP_CORNERS: {
			wd.sharp_corners = p_enabled;
			DWORD value = wd.sharp_corners ? DWMWCP_DONOTROUND : DWMWCP_DEFAULT;
			::DwmSetWindowAttribute(wd.hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &value, sizeof(value));
			_update_window_style(p_window);
		} break;
		case WINDOW_FLAG_TRANSPARENT: {
			if (p_enabled) {
				// Enable per-pixel alpha.
				if (OS::get_singleton()->is_layered_allowed()) {
					DWM_BLURBEHIND bb;
					ZeroMemory(&bb, sizeof(bb));
					HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
					bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
					bb.hRgnBlur = hRgn;
					bb.fEnable = TRUE;
					DwmEnableBlurBehindWindow(wd.hWnd, &bb);
				}
				wd.layered_window = true;
			} else {
				// Disable per-pixel alpha.
				wd.layered_window = false;
				if (OS::get_singleton()->is_layered_allowed()) {
					DWM_BLURBEHIND bb;
					ZeroMemory(&bb, sizeof(bb));
					HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
					bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
					bb.hRgnBlur = hRgn;
					bb.fEnable = FALSE;
					DwmEnableBlurBehindWindow(wd.hWnd, &bb);
				}
			}
		} break;
		case WINDOW_FLAG_NO_FOCUS: {
			wd.no_focus = p_enabled;
			_update_window_style(p_window);
		} break;
		case WINDOW_FLAG_MOUSE_PASSTHROUGH: {
			wd.mpass = p_enabled;
			_update_window_mouse_passthrough(p_window);
		} break;
		case WINDOW_FLAG_EXCLUDE_FROM_CAPTURE: {
			wd.hide_from_capture = p_enabled;
			if (p_enabled) {
				if (os_ver.dwBuildNumber >= 19041) {
					SetWindowDisplayAffinity(wd.hWnd, WDA_EXCLUDEFROMCAPTURE);
				} else {
					SetWindowDisplayAffinity(wd.hWnd, WDA_MONITOR);
				}
			} else {
				SetWindowDisplayAffinity(wd.hWnd, WDA_NONE);
			}
		} break;
		case WINDOW_FLAG_POPUP: {
			ERR_FAIL_COND_MSG(p_window == MAIN_WINDOW_ID, "Main window can't be popup.");
			ERR_FAIL_COND_MSG(IsWindowVisible(wd.hWnd) && (wd.is_popup != p_enabled), "Popup flag can't changed while window is opened.");
			ERR_FAIL_COND_MSG(p_enabled && wd.parent_hwnd, "Embedded window can't be popup.");
			wd.is_popup = p_enabled;
		} break;
		default:
			break;
	}
}

bool DisplayServerWindows::window_get_flag(WindowFlags p_flag, WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), false);
	const WindowData &wd = windows[p_window];
	switch (p_flag) {
		case WINDOW_FLAG_RESIZE_DISABLED: {
			return !wd.resizable;
		} break;
		case WINDOW_FLAG_BORDERLESS: {
			return wd.borderless;
		} break;
		case WINDOW_FLAG_ALWAYS_ON_TOP: {
			return wd.always_on_top;
		} break;
		case WINDOW_FLAG_SHARP_CORNERS: {
			return wd.sharp_corners;
		} break;
		case WINDOW_FLAG_TRANSPARENT: {
			return wd.layered_window;
		} break;
		case WINDOW_FLAG_NO_FOCUS: {
			return wd.no_focus;
		} break;
		case WINDOW_FLAG_MOUSE_PASSTHROUGH: {
			return wd.mpass;
		} break;
		case WINDOW_FLAG_EXCLUDE_FROM_CAPTURE: {
			return wd.hide_from_capture;
		} break;
		case WINDOW_FLAG_POPUP: {
			return wd.is_popup;
		} break;
		default:
			break;
	}

	return false;
}

void DisplayServerWindows::window_request_attention(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	const WindowData &wd = windows[p_window];

	FLASHWINFO info;
	info.cbSize = sizeof(FLASHWINFO);
	info.hwnd = wd.hWnd;
	info.dwFlags = FLASHW_ALL;
	info.dwTimeout = 0;
	info.uCount = 2;
	FlashWindowEx(&info);
}

void DisplayServerWindows::window_move_to_foreground(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if (!wd.no_focus && !wd.is_popup) {
		SetForegroundWindow(wd.hWnd);
	}
}

bool DisplayServerWindows::window_is_focused(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), false);
	const WindowData &wd = windows[p_window];

	return wd.window_focused;
}

DisplayServerWindows::WindowID DisplayServerWindows::get_focused_window() const {
	return last_focused_window;
}

bool DisplayServerWindows::window_can_draw(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), false);
	const WindowData &wd = windows[p_window];
	return !wd.minimized;
}

bool DisplayServerWindows::can_any_window_draw() const {
	_THREAD_SAFE_METHOD_

	for (const KeyValue<WindowID, WindowData> &E : windows) {
		if (!E.value.minimized) {
			return true;
		}
	}

	return false;
}

Vector2i DisplayServerWindows::ime_get_selection() const {
	_THREAD_SAFE_METHOD_

	DisplayServer::WindowID window_id = _get_focused_window_or_popup();
	const WindowData &wd = windows[window_id];
	if (!wd.ime_active) {
		return Vector2i();
	}

	int cursor = ImmGetCompositionStringW(wd.im_himc, GCS_CURSORPOS, nullptr, 0);

	int32_t length = ImmGetCompositionStringW(wd.im_himc, GCS_COMPSTR, nullptr, 0);
	wchar_t *string = reinterpret_cast<wchar_t *>(memalloc(length));
	ImmGetCompositionStringW(wd.im_himc, GCS_COMPSTR, string, length);

	int32_t utf32_cursor = 0;
	for (int32_t i = 0; i < length / int32_t(sizeof(wchar_t)); i++) {
		if ((string[i] & 0xfffffc00) == 0xd800) {
			i++;
		}
		if (i < cursor) {
			utf32_cursor++;
		} else {
			break;
		}
	}

	memdelete(string);

	return Vector2i(utf32_cursor, 0);
}

String DisplayServerWindows::ime_get_text() const {
	_THREAD_SAFE_METHOD_

	DisplayServer::WindowID window_id = _get_focused_window_or_popup();
	const WindowData &wd = windows[window_id];
	if (!wd.ime_active) {
		return String();
	}

	String ret;
	int32_t length = ImmGetCompositionStringW(wd.im_himc, GCS_COMPSTR, nullptr, 0);
	wchar_t *string = reinterpret_cast<wchar_t *>(memalloc(length));
	ImmGetCompositionStringW(wd.im_himc, GCS_COMPSTR, string, length);
	ret.parse_utf16((char16_t *)string, length / sizeof(wchar_t));

	memdelete(string);

	return ret;
}

void DisplayServerWindows::window_set_ime_active(const bool p_active, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if (p_active) {
		wd.ime_active = true;
		ImmAssociateContext(wd.hWnd, wd.im_himc);
		CreateCaret(wd.hWnd, nullptr, 1, 1);
		window_set_ime_position(wd.im_position, p_window);
	} else {
		ImmAssociateContext(wd.hWnd, (HIMC) nullptr);
		DestroyCaret();
		wd.ime_active = false;
	}
}

void DisplayServerWindows::window_set_ime_position(const Point2i &p_pos, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	wd.im_position = p_pos;

	HIMC himc = ImmGetContext(wd.hWnd);
	if (himc == (HIMC) nullptr) {
		return;
	}

	COMPOSITIONFORM cps;
	cps.dwStyle = CFS_POINT;
	cps.ptCurrentPos.x = wd.im_position.x;
	cps.ptCurrentPos.y = wd.im_position.y;
	ImmSetCompositionWindow(himc, &cps);
	ImmReleaseContext(wd.hWnd, himc);
}

void DisplayServerWindows::cursor_set_shape(CursorShape p_shape) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	if (cursor_shape == p_shape) {
		return;
	}

	if (mouse_mode != MOUSE_MODE_VISIBLE && mouse_mode != MOUSE_MODE_CONFINED) {
		cursor_shape = p_shape;
		return;
	}

	static const LPCTSTR win_cursors[CURSOR_MAX] = {
		IDC_ARROW,
		IDC_IBEAM,
		IDC_HAND, // Finger.
		IDC_CROSS,
		IDC_WAIT,
		IDC_APPSTARTING,
		IDC_SIZEALL,
		IDC_ARROW,
		IDC_NO,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_SIZENESW,
		IDC_SIZENWSE,
		IDC_SIZEALL,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_HELP
	};

	if (cursors_cache.has(p_shape)) {
		SetCursor(cursors[p_shape]);
	} else {
		SetCursor(LoadCursor(hInstance, win_cursors[p_shape]));
	}

	cursor_shape = p_shape;
}

DisplayServer::CursorShape DisplayServerWindows::cursor_get_shape() const {
	return cursor_shape;
}

void DisplayServerWindows::cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	if (p_cursor.is_valid()) {
		RBMap<CursorShape, Vector<Variant>>::Element *cursor_c = cursors_cache.find(p_shape);

		if (cursor_c) {
			if (cursor_c->get()[0] == p_cursor && cursor_c->get()[1] == p_hotspot) {
				cursor_set_shape(p_shape);
				return;
			}

			cursors_cache.erase(p_shape);
		}

		Ref<Image> image = _get_cursor_image_from_resource(p_cursor, p_hotspot);
		ERR_FAIL_COND(image.is_null());
		Vector2i texture_size = image->get_size();

		UINT image_size = texture_size.width * texture_size.height;

		// Create the BITMAP with alpha channel.
		COLORREF *buffer = nullptr;

		BITMAPV5HEADER bi;
		ZeroMemory(&bi, sizeof(bi));
		bi.bV5Size = sizeof(bi);
		bi.bV5Width = texture_size.width;
		bi.bV5Height = -texture_size.height;
		bi.bV5Planes = 1;
		bi.bV5BitCount = 32;
		bi.bV5Compression = BI_BITFIELDS;
		bi.bV5RedMask = 0x00ff0000;
		bi.bV5GreenMask = 0x0000ff00;
		bi.bV5BlueMask = 0x000000ff;
		bi.bV5AlphaMask = 0xff000000;

		HDC dc = GetDC(nullptr);
		HBITMAP bitmap = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS, reinterpret_cast<void **>(&buffer), nullptr, 0);
		HBITMAP mask = CreateBitmap(texture_size.width, texture_size.height, 1, 1, nullptr);

		bool fully_transparent = true;
		for (UINT index = 0; index < image_size; index++) {
			int row_index = floor(index / texture_size.width);
			int column_index = index % int(texture_size.width);

			const Color &c = image->get_pixel(column_index, row_index);
			fully_transparent = fully_transparent && (c.a == 0.f);

			*(buffer + index) = c.to_argb32();
		}

		// Finally, create the icon.
		if (cursors[p_shape]) {
			DestroyIcon(cursors[p_shape]);
		}

		if (fully_transparent) {
			cursors[p_shape] = nullptr;
		} else {
			ICONINFO iconinfo;
			iconinfo.fIcon = FALSE;
			iconinfo.xHotspot = p_hotspot.x;
			iconinfo.yHotspot = p_hotspot.y;
			iconinfo.hbmMask = mask;
			iconinfo.hbmColor = bitmap;
			cursors[p_shape] = CreateIconIndirect(&iconinfo);
		}

		Vector<Variant> params;
		params.push_back(p_cursor);
		params.push_back(p_hotspot);
		cursors_cache.insert(p_shape, params);

		if (p_shape == cursor_shape) {
			if (mouse_mode == MOUSE_MODE_VISIBLE || mouse_mode == MOUSE_MODE_CONFINED) {
				SetCursor(cursors[p_shape]);
			}
		}

		DeleteObject(mask);
		DeleteObject(bitmap);
		ReleaseDC(nullptr, dc);
	} else {
		// Reset to default system cursor.
		if (cursors[p_shape]) {
			DestroyIcon(cursors[p_shape]);
		}
		cursors[p_shape] = nullptr;

		cursors_cache.erase(p_shape);

		CursorShape c = cursor_shape;
		cursor_shape = CURSOR_MAX;
		cursor_set_shape(c);
	}
}

bool DisplayServerWindows::get_swap_cancel_ok() {
	return true;
}

void DisplayServerWindows::enable_for_stealing_focus(OS::ProcessID pid) {
	_THREAD_SAFE_METHOD_

	AllowSetForegroundWindow(pid);
}

struct WindowEnumData {
	DWORD process_id;
	HWND parent_hWnd;
	HWND hWnd;
};

static BOOL CALLBACK _enum_proc_find_window_from_process_id_callback(HWND hWnd, LPARAM lParam) {
	WindowEnumData &ed = *(WindowEnumData *)lParam;
	DWORD process_id = 0x0;

	GetWindowThreadProcessId(hWnd, &process_id);
	if (ed.process_id == process_id) {
		if (GetParent(hWnd) != ed.parent_hWnd) {
			return TRUE;
		}

		// Found it.
		ed.hWnd = hWnd;
		SetLastError(ERROR_SUCCESS);
		return FALSE;
	}
	// Continue enumeration.
	return TRUE;
}

HWND DisplayServerWindows::_find_window_from_process_id(OS::ProcessID p_pid, HWND p_current_hwnd) {
	DWORD pid = p_pid;
	WindowEnumData ed = { pid, p_current_hwnd, NULL };

	// First, check our own child, maybe it's already embedded.
	if (!EnumChildWindows(p_current_hwnd, _enum_proc_find_window_from_process_id_callback, (LPARAM)&ed) && (GetLastError() == ERROR_SUCCESS)) {
		if (ed.hWnd) {
			return ed.hWnd;
		}
	}

	// Then check all the opened windows on the computer.
	if (!EnumWindows(_enum_proc_find_window_from_process_id_callback, (LPARAM)&ed) && (GetLastError() == ERROR_SUCCESS)) {
		return ed.hWnd;
	}

	return NULL;
}

Error DisplayServerWindows::embed_process(WindowID p_window, OS::ProcessID p_pid, const Rect2i &p_rect, bool p_visible, bool p_grab_focus) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), FAILED);

	const WindowData &wd = windows[p_window];

	EmbeddedProcessData *ep = nullptr;
	if (embedded_processes.has(p_pid)) {
		ep = embedded_processes.get(p_pid);
	} else {
		// New process, trying to find the window.
		HWND handle_to_embed = _find_window_from_process_id(p_pid, wd.hWnd);
		if (!handle_to_embed) {
			return ERR_DOES_NOT_EXIST;
		}

		const DWORD style = GetWindowLongPtr(handle_to_embed, GWL_STYLE);

		ep = memnew(EmbeddedProcessData);
		ep->window_handle = handle_to_embed;
		ep->parent_window_handle = wd.hWnd;
		ep->is_visible = (style & WS_VISIBLE) == WS_VISIBLE;

		embedded_processes.insert(p_pid, ep);
	}

	if (p_rect.size.x <= 100 || p_rect.size.y <= 100) {
		p_visible = false;
	}

	// In Godot, the window position is offset by the screen's origin coordinates.
	// We need to adjust for this when a screen is positioned in the negative space
	// (e.g., a screen to the left of the main screen).
	const Rect2i adjusted_rect = Rect2i(p_rect.position + _get_screens_origin(), p_rect.size);

	SetWindowPos(ep->window_handle, nullptr, adjusted_rect.position.x, adjusted_rect.position.y, adjusted_rect.size.x, adjusted_rect.size.y, SWP_NOZORDER | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);

	if (ep->is_visible != p_visible) {
		if (p_visible) {
			ShowWindow(ep->window_handle, SW_SHOWNA);
		} else {
			ShowWindow(ep->window_handle, SW_HIDE);
		}
		ep->is_visible = p_visible;
	}

	if (p_grab_focus) {
		SetFocus(ep->window_handle);
	}

	return OK;
}

Error DisplayServerWindows::remove_embedded_process(OS::ProcessID p_pid) {
	_THREAD_SAFE_METHOD_

	if (!embedded_processes.has(p_pid)) {
		return ERR_DOES_NOT_EXIST;
	}

	EmbeddedProcessData *ep = embedded_processes.get(p_pid);

	// Send a close message to gracefully close the process.
	PostMessage(ep->window_handle, WM_CLOSE, 0, 0);

	// This is a workaround to ensure the parent window correctly regains focus after the
	// embedded window is closed. When the embedded window is closed while it has focus,
	// the parent window (the editor) does not become active. It appears focused but is not truly activated.
	// Opening a new window and closing it forces Windows to set the focus and activation correctly.
	DWORD style = WS_POPUP | WS_VISIBLE;
	DWORD style_ex = WS_EX_TOPMOST;

	WNDCLASSW wcTemp = {};
	wcTemp.lpfnWndProc = DefWindowProcW;
	wcTemp.hInstance = GetModuleHandle(nullptr);
	wcTemp.lpszClassName = L"Engine temp window";
	RegisterClassW(&wcTemp);

	HWND hWnd = CreateWindowExW(
			style_ex,
			L"Engine temp window", L"",
			style,
			0,
			0,
			1,
			1,
			ep->parent_window_handle,
			nullptr,
			GetModuleHandle(nullptr),
			nullptr);

	SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

	DestroyWindow(hWnd);
	UnregisterClassW(L"Engine temp window", GetModuleHandle(nullptr));

	SetForegroundWindow(ep->parent_window_handle);

	embedded_processes.erase(p_pid);
	memdelete(ep);

	return OK;
}

OS::ProcessID DisplayServerWindows::get_focused_process_id() {
	HWND hwnd = GetForegroundWindow();
	if (!hwnd) {
		return 0;
	}

	// Get the process ID of the window.
	DWORD processID;
	GetWindowThreadProcessId(hwnd, &processID);

	return processID;
}

static HRESULT CALLBACK win32_task_dialog_callback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) {
	if (msg == TDN_CREATED) {
		// To match the input text dialog.
		SendMessageW(hwnd, WM_SETICON, ICON_BIG, 0);
		SendMessageW(hwnd, WM_SETICON, ICON_SMALL, 0);
	}

	return 0;
}

Error DisplayServerWindows::dialog_show(String p_title, String p_description, Vector<String> p_buttons, const Callable &p_callback) {
	_THREAD_SAFE_METHOD_

	TASKDIALOGCONFIG config;
	ZeroMemory(&config, sizeof(TASKDIALOGCONFIG));
	config.cbSize = sizeof(TASKDIALOGCONFIG);

	Char16String title = p_title.utf16();
	Char16String message = p_description.utf16();
	LocalVector<Char16String> buttons;
	for (String s : p_buttons) {
		buttons.push_back(s.utf16());
	}

	WindowID window_id = _get_focused_window_or_popup();
	if (!windows.has(window_id)) {
		window_id = MAIN_WINDOW_ID;
	}

	config.pszWindowTitle = (LPCWSTR)(title.get_data());
	config.pszContent = (LPCWSTR)(message.get_data());
	config.hwndParent = windows[window_id].hWnd;

	const int button_count = buttons.size();
	config.cButtons = button_count;

	// No dynamic stack array size :(
	TASKDIALOG_BUTTON *tbuttons = button_count != 0 ? (TASKDIALOG_BUTTON *)alloca(sizeof(TASKDIALOG_BUTTON) * button_count) : nullptr;
	if (tbuttons) {
		for (int i = 0; i < button_count; i++) {
			tbuttons[i].nButtonID = i + 100;
			tbuttons[i].pszButtonText = (LPCWSTR)(buttons[i].get_data());
		}
	}
	config.pButtons = tbuttons;
	config.pfCallback = win32_task_dialog_callback;

	Error result = FAILED;
	HMODULE comctl = LoadLibraryW(L"comctl32.dll");
	if (comctl) {
		typedef HRESULT(WINAPI * TaskDialogIndirectPtr)(const TASKDIALOGCONFIG *pTaskConfig, int *pnButton, int *pnRadioButton, BOOL *pfVerificationFlagChecked);

		TaskDialogIndirectPtr task_dialog_indirect = (TaskDialogIndirectPtr)GetProcAddress(comctl, "TaskDialogIndirect");
		int button_pressed;

		if (task_dialog_indirect && SUCCEEDED(task_dialog_indirect(&config, &button_pressed, nullptr, nullptr))) {
			if (p_callback.is_valid()) {
				Variant button = button_pressed - 100;
				const Variant *args[1] = { &button };
				Variant ret;
				Callable::CallError ce;
				p_callback.callp(args, 1, ret, ce);
				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT(vformat("Failed to execute dialog callback: %s.", Variant::get_callable_error_text(p_callback, args, 1, ce)));
				}
			}

			result = OK;
		}
		FreeLibrary(comctl);
	} else {
		ERR_PRINT("Unable to create native dialog.");
	}

	return result;
}

struct Win32InputTextDialogInit {
	const char16_t *title;
	const char16_t *description;
	const char16_t *partial;
	const Callable &callback;
};

static int scale_with_dpi(int p_pos, int p_dpi) {
	return IsProcessDPIAware() ? (p_pos * p_dpi / 96) : p_pos;
}

static INT_PTR input_text_dialog_init(HWND hWnd, UINT code, WPARAM wParam, LPARAM lParam) {
	Win32InputTextDialogInit init = *(Win32InputTextDialogInit *)lParam;
	SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)&init.callback); // Set dialog callback.

	SetWindowTextW(hWnd, (LPCWSTR)init.title);

	const int dpi = DisplayServerWindows::get_singleton()->screen_get_dpi();

	const int margin = scale_with_dpi(7, dpi);
	const SIZE dlg_size = { scale_with_dpi(300, dpi), scale_with_dpi(50, dpi) };

	int str_len = lstrlenW((LPCWSTR)init.description);
	SIZE str_size = { dlg_size.cx, 0 };
	if (str_len > 0) {
		HDC hdc = GetDC(nullptr);
		RECT trect = { margin, margin, margin + dlg_size.cx, margin + dlg_size.cy };
		SelectObject(hdc, (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0));

		// `+ margin` adds some space between the static text and the edit field.
		// Don't scale this with DPI because DPI is already handled by DrawText.
		str_size.cy = DrawTextW(hdc, (LPCWSTR)init.description, str_len, &trect, DT_LEFT | DT_WORDBREAK | DT_CALCRECT) + margin;

		ReleaseDC(nullptr, hdc);
	}

	RECT crect, wrect;
	GetClientRect(hWnd, &crect);
	GetWindowRect(hWnd, &wrect);
	int sw = GetSystemMetrics(SM_CXSCREEN);
	int sh = GetSystemMetrics(SM_CYSCREEN);
	int new_width = dlg_size.cx + margin * 2 + wrect.right - wrect.left - crect.right;
	int new_height = dlg_size.cy + margin * 2 + wrect.bottom - wrect.top - crect.bottom + str_size.cy;

	MoveWindow(hWnd, (sw - new_width) / 2, (sh - new_height) / 2, new_width, new_height, true);

	HWND ok_button = GetDlgItem(hWnd, 1);
	MoveWindow(ok_button,
			dlg_size.cx + margin - scale_with_dpi(65, dpi),
			dlg_size.cy + str_size.cy + margin - scale_with_dpi(20, dpi),
			scale_with_dpi(65, dpi), scale_with_dpi(20, dpi), true);

	HWND description = GetDlgItem(hWnd, 3);
	MoveWindow(description, margin, margin, dlg_size.cx, str_size.cy, true);
	SetWindowTextW(description, (LPCWSTR)init.description);

	HWND text_edit = GetDlgItem(hWnd, 2);
	MoveWindow(text_edit, margin, str_size.cy + margin, dlg_size.cx, scale_with_dpi(20, dpi), true);
	SetWindowTextW(text_edit, (LPCWSTR)init.partial);

	return TRUE;
}

static INT_PTR input_text_dialog_cmd_proc(HWND hWnd, UINT code, WPARAM wParam, LPARAM lParam) {
	if (LOWORD(wParam) == 1) {
		HWND text_edit = GetDlgItem(hWnd, 2);
		ERR_FAIL_NULL_V(text_edit, false);

		Char16String text;
		text.resize(GetWindowTextLengthW(text_edit) + 1);
		GetWindowTextW(text_edit, (LPWSTR)text.get_data(), text.size());

		const Callable *callback = (const Callable *)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
		if (callback && callback->is_valid()) {
			Variant v_result = String((const wchar_t *)text.get_data());
			Variant ret;
			Callable::CallError ce;
			const Variant *args[1] = { &v_result };

			callback->callp(args, 1, ret, ce);
			if (ce.error != Callable::CallError::CALL_OK) {
				ERR_PRINT(vformat("Failed to execute input dialog callback: %s.", Variant::get_callable_error_text(*callback, args, 1, ce)));
			}
		}

		return EndDialog(hWnd, 0);
	}

	return false;
}

static INT_PTR CALLBACK input_text_dialog_proc(HWND hWnd, UINT code, WPARAM wParam, LPARAM lParam) {
	switch (code) {
		case WM_INITDIALOG:
			return input_text_dialog_init(hWnd, code, wParam, lParam);

		case WM_COMMAND:
			return input_text_dialog_cmd_proc(hWnd, code, wParam, lParam);

		default:
			return FALSE;
	}
}

Error DisplayServerWindows::dialog_input_text(String p_title, String p_description, String p_partial, const Callable &p_callback) {
#pragma pack(push, 1)

	// NOTE: Use default/placeholder coordinates here. Windows uses its own coordinate system
	//       specifically for dialogs which relies on font sizes instead of pixels.
	const struct {
		WORD dlgVer; // must be 1
		WORD signature; // must be 0xFFFF
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		WORD cDlgItems;
		short x;
		short y;
		short cx;
		short cy;
		WCHAR menu[1]; // must be 0
		WCHAR windowClass[7]; // must be "#32770" -- the default window class for dialogs
		WCHAR title[1]; // must be 0
		WORD pointsize;
		WORD weight;
		BYTE italic;
		BYTE charset;
		WCHAR font[13]; // must be "MS Shell Dlg"
	} template_base = {
		1, 0xFFFF, 0, 0,
		DS_SYSMODAL | DS_SETFONT | DS_MODALFRAME | DS_3DLOOK | DS_FIXEDSYS | DS_CENTER | WS_POPUP | WS_CAPTION,
		3, 0, 0, 20, 20, L"", L"#32770", L"", 8, FW_NORMAL, 0, DEFAULT_CHARSET, L"MS Shell Dlg"
	};

	const struct {
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		short x;
		short y;
		short cx;
		short cy;
		DWORD id;
		WCHAR windowClass[7]; // must be "Button"
		WCHAR title[3]; // must be "OK"
		WORD extraCount;
	} ok_button = {
		0, 0, WS_VISIBLE | BS_DEFPUSHBUTTON, 0, 0, 50, 14, 1, WC_BUTTONW, L"OK", 0
	};
	const struct {
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		short x;
		short y;
		short cx;
		short cy;
		DWORD id;
		WCHAR windowClass[5]; // must be "Edit"
		WCHAR title[1]; // must be 0
		WORD extraCount;
	} text_field = {
		0, 0, WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 250, 14, 2, WC_EDITW, L"", 0
	};
	const struct {
		DWORD helpID;
		DWORD exStyle;
		DWORD style;
		short x;
		short y;
		short cx;
		short cy;
		DWORD id;
		WCHAR windowClass[7]; // must be "Static"
		WCHAR title[1]; // must be 0
		WORD extraCount;
	} static_text = {
		0, 0, WS_VISIBLE, 0, 0, 250, 14, 3, WC_STATICW, L"", 0
	};

#pragma pack(pop)

	// Dialog template
	const size_t data_size = sizeof(template_base) + (sizeof(template_base) % 4) +
			sizeof(ok_button) + (sizeof(ok_button) % 4) +
			sizeof(text_field) + (sizeof(text_field) % 4) +
			sizeof(static_text) + (sizeof(static_text) % 4);

	void *data_template = memalloc(data_size);
	ERR_FAIL_NULL_V_MSG(data_template, FAILED, "Unable to allocate memory for the dialog template.");
	ZeroMemory(data_template, data_size);

	char *current_block = (char *)data_template;
	CopyMemory(current_block, &template_base, sizeof(template_base));
	current_block += sizeof(template_base) + (sizeof(template_base) % 4);
	CopyMemory(current_block, &ok_button, sizeof(ok_button));
	current_block += sizeof(ok_button) + (sizeof(ok_button) % 4);
	CopyMemory(current_block, &text_field, sizeof(text_field));
	current_block += sizeof(text_field) + (sizeof(text_field) % 4);
	CopyMemory(current_block, &static_text, sizeof(static_text));

	Char16String title16 = p_title.utf16();
	Char16String description16 = p_description.utf16();
	Char16String partial16 = p_partial.utf16();

	Win32InputTextDialogInit init = {
		title16.get_data(), description16.get_data(), partial16.get_data(), p_callback
	};

	// No modal dialogs for specific windows? Assume main window here.
	INT_PTR ret = DialogBoxIndirectParamW(hInstance, (LPDLGTEMPLATEW)data_template, nullptr, (DLGPROC)input_text_dialog_proc, (LPARAM)(&init));

	Error result = ret != -1 ? OK : FAILED;
	memfree(data_template);

	if (result == FAILED) {
		ERR_PRINT("Unable to create native dialog.");
	}
	return result;
}

int DisplayServerWindows::keyboard_get_layout_count() const {
	return GetKeyboardLayoutList(0, nullptr);
}

int DisplayServerWindows::keyboard_get_current_layout() const {
	HKL cur_layout = GetKeyboardLayout(0);

	int layout_count = GetKeyboardLayoutList(0, nullptr);
	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);

	for (int i = 0; i < layout_count; i++) {
		if (cur_layout == layouts[i]) {
			memfree(layouts);
			return i;
		}
	}
	memfree(layouts);
	return -1;
}

void DisplayServerWindows::keyboard_set_current_layout(int p_index) {
	int layout_count = GetKeyboardLayoutList(0, nullptr);

	ERR_FAIL_INDEX(p_index, layout_count);

	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);
	ActivateKeyboardLayout(layouts[p_index], KLF_SETFORPROCESS);
	memfree(layouts);
}

String DisplayServerWindows::keyboard_get_layout_language(int p_index) const {
	int layout_count = GetKeyboardLayoutList(0, nullptr);

	ERR_FAIL_INDEX_V(p_index, layout_count, "");

	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);

	WCHAR buf[LOCALE_NAME_MAX_LENGTH];
	memset(buf, 0, LOCALE_NAME_MAX_LENGTH * sizeof(WCHAR));
	LCIDToLocaleName(MAKELCID(LOWORD(layouts[p_index]), SORT_DEFAULT), buf, LOCALE_NAME_MAX_LENGTH, 0);

	memfree(layouts);

	return String::utf16((const char16_t *)buf).substr(0, 2);
}

Key DisplayServerWindows::keyboard_get_keycode_from_physical(Key p_keycode) const {
	Key modifiers = p_keycode & KeyModifierMask::MODIFIER_MASK;
	Key keycode_no_mod = (Key)(p_keycode & KeyModifierMask::CODE_MASK);

	if (keycode_no_mod == Key::PRINT ||
			keycode_no_mod == Key::KP_ADD ||
			keycode_no_mod == Key::KP_5 ||
			(keycode_no_mod >= Key::KEY_0 && keycode_no_mod <= Key::KEY_9)) {
		return p_keycode;
	}

	unsigned int scancode = KeyMappingWindows::get_scancode(keycode_no_mod);
	if (scancode == 0) {
		return p_keycode;
	}

	HKL current_layout = GetKeyboardLayout(0);
	UINT vk = MapVirtualKeyEx(scancode, MAPVK_VSC_TO_VK, current_layout);
	if (vk == 0) {
		return p_keycode;
	}

	UINT char_code = MapVirtualKeyEx(vk, MAPVK_VK_TO_CHAR, current_layout) & 0x7FFF;
	// Unlike a similar Linux/BSD check which matches full Latin-1 range,
	// we limit these to ASCII to fix some layouts, including Arabic ones
	if (char_code >= 32 && char_code <= 127) {
		// Godot uses 'braces' instead of 'brackets'
		if (char_code == (unsigned int)Key::BRACKETLEFT || char_code == (unsigned int)Key::BRACKETRIGHT) {
			char_code += 32;
		}
		return (Key)(char_code | (unsigned int)modifiers);
	}

	return (Key)(KeyMappingWindows::get_keysym(vk) | modifiers);
}

Key DisplayServerWindows::keyboard_get_label_from_physical(Key p_keycode) const {
	Key modifiers = p_keycode & KeyModifierMask::MODIFIER_MASK;
	Key keycode_no_mod = (Key)(p_keycode & KeyModifierMask::CODE_MASK);

	if (keycode_no_mod == Key::PRINT ||
			keycode_no_mod == Key::KP_ADD ||
			keycode_no_mod == Key::KP_5 ||
			(keycode_no_mod >= Key::KEY_0 && keycode_no_mod <= Key::KEY_9)) {
		return p_keycode;
	}

	unsigned int scancode = KeyMappingWindows::get_scancode(keycode_no_mod);
	if (scancode == 0) {
		return p_keycode;
	}

	Key keycode = KeyMappingWindows::get_keysym(MapVirtualKey(scancode, MAPVK_VSC_TO_VK));

	HKL current_layout = GetKeyboardLayout(0);
	static BYTE keyboard_state[256];
	memset(keyboard_state, 0, 256);
	wchar_t chars[256] = {};
	UINT extended_code = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
	if (ToUnicodeEx(extended_code, scancode, keyboard_state, chars, 255, 4, current_layout) > 0) {
		String keysym = String::utf16((char16_t *)chars, 255);
		if (!keysym.is_empty()) {
			return fix_key_label(keysym[0], keycode) | modifiers;
		}
	}
	return p_keycode;
}

void DisplayServerWindows::show_emoji_and_symbol_picker() const {
	// Send Win + Period shortcut, there's no non-WinRT public API.

	INPUT input[4] = {};
	input[0].type = INPUT_KEYBOARD; // Win down.
	input[0].ki.wVk = VK_LWIN;

	input[1].type = INPUT_KEYBOARD; // Period down.
	input[1].ki.wVk = VK_OEM_PERIOD;

	input[2].type = INPUT_KEYBOARD; // Win up.
	input[2].ki.wVk = VK_LWIN;
	input[2].ki.dwFlags = KEYEVENTF_KEYUP;

	input[3].type = INPUT_KEYBOARD; // Period up.
	input[3].ki.wVk = VK_OEM_PERIOD;
	input[3].ki.dwFlags = KEYEVENTF_KEYUP;

	SendInput(4, input, sizeof(INPUT));
}

String DisplayServerWindows::_get_keyboard_layout_display_name(const String &p_klid) const {
	String ret;
	HKEY key;
	if (RegOpenKeyW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts", &key) != ERROR_SUCCESS) {
		return String();
	}

	WCHAR buffer[MAX_PATH] = {};
	DWORD buffer_size = MAX_PATH;
	if (RegGetValueW(key, (LPCWSTR)p_klid.utf16().get_data(), L"Layout Display Name", RRF_RT_REG_SZ, nullptr, buffer, &buffer_size) == ERROR_SUCCESS) {
		if (load_indirect_string) {
			if (load_indirect_string(buffer, buffer, buffer_size, nullptr) == S_OK) {
				ret = String::utf16((const char16_t *)buffer, buffer_size);
			}
		}
	} else {
		if (RegGetValueW(key, (LPCWSTR)p_klid.utf16().get_data(), L"Layout Text", RRF_RT_REG_SZ, nullptr, buffer, &buffer_size) == ERROR_SUCCESS) {
			ret = String::utf16((const char16_t *)buffer, buffer_size);
		}
	}

	RegCloseKey(key);
	return ret;
}

String DisplayServerWindows::_get_klid(HKL p_hkl) const {
	String ret;

	WORD device = HIWORD(p_hkl);
	if ((device & 0xf000) == 0xf000) {
		WORD layout_id = device & 0x0fff;

		HKEY key;
		if (RegOpenKeyW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts", &key) != ERROR_SUCCESS) {
			return String();
		}

		DWORD index = 0;
		wchar_t klid_buffer[KL_NAMELENGTH];
		DWORD klid_buffer_size = KL_NAMELENGTH;
		while (RegEnumKeyExW(key, index, klid_buffer, &klid_buffer_size, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {
			wchar_t layout_id_buf[MAX_PATH] = {};
			DWORD layout_id_size = MAX_PATH;
			if (RegGetValueW(key, klid_buffer, L"Layout Id", RRF_RT_REG_SZ, nullptr, layout_id_buf, &layout_id_size) == ERROR_SUCCESS) {
				if (layout_id == String::utf16((char16_t *)layout_id_buf, layout_id_size).hex_to_int()) {
					ret = String::utf16((const char16_t *)klid_buffer, klid_buffer_size).lpad(8, "0");
					break;
				}
			}
			klid_buffer_size = KL_NAMELENGTH;
			++index;
		}

		RegCloseKey(key);
	} else {
		if (device == 0) {
			device = LOWORD(p_hkl);
		}
		ret = (String::num_uint64((uint64_t)device, 16, false)).lpad(8, "0");
	}

	return ret;
}

String DisplayServerWindows::keyboard_get_layout_name(int p_index) const {
	int layout_count = GetKeyboardLayoutList(0, nullptr);

	ERR_FAIL_INDEX_V(p_index, layout_count, "");

	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);

	String ret = _get_keyboard_layout_display_name(_get_klid(layouts[p_index])); // Try reading full name from Windows registry, fallback to locale name if failed (e.g. on Wine).
	if (ret.is_empty()) {
		WCHAR buf[LOCALE_NAME_MAX_LENGTH];
		memset(buf, 0, LOCALE_NAME_MAX_LENGTH * sizeof(WCHAR));
		LCIDToLocaleName(MAKELCID(LOWORD(layouts[p_index]), SORT_DEFAULT), buf, LOCALE_NAME_MAX_LENGTH, 0);

		WCHAR name[1024];
		memset(name, 0, 1024 * sizeof(WCHAR));
		GetLocaleInfoEx(buf, LOCALE_SLOCALIZEDDISPLAYNAME, (LPWSTR)&name, 1024);

		ret = String::utf16((const char16_t *)name);
	}
	memfree(layouts);

	return ret;
}

void DisplayServerWindows::process_events() {
	ERR_FAIL_COND(!Thread::is_main_thread());

	if (!drop_events) {
		joypad->process_joypads();
	}

	_THREAD_SAFE_LOCK_
	MSG msg = {};
	while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}
	_THREAD_SAFE_UNLOCK_

	if (tts) {
		tts->process_events();
	}

	if (!drop_events) {
		_process_key_events();
		Input::get_singleton()->flush_buffered_events();
	}

	LocalVector<List<FileDialogData *>::Element *> to_remove;
	for (List<FileDialogData *>::Element *E = file_dialogs.front(); E; E = E->next()) {
		FileDialogData *fd = E->get();
		if (fd->finished.is_set()) {
			if (fd->listener_thread.is_started()) {
				fd->listener_thread.wait_to_finish();
			}
			to_remove.push_back(E);
		}
	}
	for (List<FileDialogData *>::Element *E : to_remove) {
		memdelete(E->get());
		E->erase();
	}
	process_file_dialog_callbacks();
}

void DisplayServerWindows::force_process_and_drop_events() {
	ERR_FAIL_COND(!Thread::is_main_thread());

	drop_events = true;
	process_events();
	drop_events = false;
}

void DisplayServerWindows::release_rendering_thread() {
#if defined(GLES3_ENABLED)
	if (gl_manager_angle) {
		gl_manager_angle->release_current();
	}
	if (gl_manager_native) {
		gl_manager_native->release_current();
	}
#endif
}

void DisplayServerWindows::swap_buffers() {
#if defined(GLES3_ENABLED)
	if (gl_manager_angle) {
		gl_manager_angle->swap_buffers();
	}
	if (gl_manager_native) {
		gl_manager_native->swap_buffers();
	}
#endif
}

void DisplayServerWindows::set_native_icon(const String &p_filename) {
	_THREAD_SAFE_METHOD_

	Ref<FileAccess> f = FileAccess::open(p_filename, FileAccess::READ);
	ERR_FAIL_COND_MSG(f.is_null(), "Cannot open file with icon '" + p_filename + "'.");

	ICONDIR *icon_dir = (ICONDIR *)memalloc(sizeof(ICONDIR));
	int pos = 0;

	icon_dir->idReserved = f->get_32();
	pos += sizeof(WORD);
	f->seek(pos);

	icon_dir->idType = f->get_32();
	pos += sizeof(WORD);
	f->seek(pos);

	ERR_FAIL_COND_MSG(icon_dir->idType != 1, "Invalid icon file format!");

	icon_dir->idCount = f->get_32();
	pos += sizeof(WORD);
	f->seek(pos);

	icon_dir = (ICONDIR *)memrealloc(icon_dir, sizeof(ICONDIR) - sizeof(ICONDIRENTRY) + icon_dir->idCount * sizeof(ICONDIRENTRY));
	f->get_buffer((uint8_t *)&icon_dir->idEntries[0], icon_dir->idCount * sizeof(ICONDIRENTRY));

	int small_icon_index = -1; // Select 16x16 with largest color count.
	int small_icon_cc = 0;
	int big_icon_index = -1; // Select largest.
	int big_icon_width = 16;
	int big_icon_cc = 0;

	for (int i = 0; i < icon_dir->idCount; i++) {
		int colors = (icon_dir->idEntries[i].bColorCount == 0) ? 32768 : icon_dir->idEntries[i].bColorCount;
		int width = (icon_dir->idEntries[i].bWidth == 0) ? 256 : icon_dir->idEntries[i].bWidth;
		if (width == 16) {
			if (colors >= small_icon_cc) {
				small_icon_index = i;
				small_icon_cc = colors;
			}
		}
		if (width >= big_icon_width) {
			if (colors >= big_icon_cc) {
				big_icon_index = i;
				big_icon_width = width;
				big_icon_cc = colors;
			}
		}
	}

	ERR_FAIL_COND_MSG(big_icon_index == -1, "No valid icons found!");

	if (small_icon_index == -1) {
		WARN_PRINT("No small icon found, reusing " + itos(big_icon_width) + "x" + itos(big_icon_width) + " @" + itos(big_icon_cc) + " icon!");
		small_icon_index = big_icon_index;
		small_icon_cc = big_icon_cc;
	}

	// Read the big icon.
	DWORD bytecount_big = icon_dir->idEntries[big_icon_index].dwBytesInRes;
	Vector<uint8_t> data_big;
	data_big.resize(bytecount_big);
	pos = icon_dir->idEntries[big_icon_index].dwImageOffset;
	f->seek(pos);
	f->get_buffer((uint8_t *)&data_big.write[0], bytecount_big);
	HICON icon_big = CreateIconFromResource((PBYTE)&data_big.write[0], bytecount_big, TRUE, 0x00030000);
	ERR_FAIL_NULL_MSG(icon_big, "Could not create " + itos(big_icon_width) + "x" + itos(big_icon_width) + " @" + itos(big_icon_cc) + " icon, error: " + format_error_message(GetLastError()) + ".");

	// Read the small icon.
	DWORD bytecount_small = icon_dir->idEntries[small_icon_index].dwBytesInRes;
	Vector<uint8_t> data_small;
	data_small.resize(bytecount_small);
	pos = icon_dir->idEntries[small_icon_index].dwImageOffset;
	f->seek(pos);
	f->get_buffer((uint8_t *)&data_small.write[0], bytecount_small);
	HICON icon_small = CreateIconFromResource((PBYTE)&data_small.write[0], bytecount_small, TRUE, 0x00030000);
	ERR_FAIL_NULL_MSG(icon_small, "Could not create 16x16 @" + itos(small_icon_cc) + " icon, error: " + format_error_message(GetLastError()) + ".");

	// Online tradition says to be sure last error is cleared and set the small icon first.
	int err = 0;
	SetLastError(err);

	SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_SETICON, ICON_SMALL, (LPARAM)icon_small);
	err = GetLastError();
	ERR_FAIL_COND_MSG(err, "Error setting ICON_SMALL: " + format_error_message(err) + ".");

	SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_SETICON, ICON_BIG, (LPARAM)icon_big);
	err = GetLastError();
	ERR_FAIL_COND_MSG(err, "Error setting ICON_BIG: " + format_error_message(err) + ".");

	memdelete(icon_dir);
}

void DisplayServerWindows::set_icon(const Ref<Image> &p_icon) {
	_THREAD_SAFE_METHOD_

	if (p_icon.is_valid()) {
		ERR_FAIL_COND(p_icon->get_width() <= 0 || p_icon->get_height() <= 0);

		Ref<Image> img = p_icon;
		if (img != icon) {
			img = img->duplicate();
			img->convert(Image::FORMAT_RGBA8);
		}

		int w = img->get_width();
		int h = img->get_height();

		// Create temporary bitmap buffer.
		int icon_len = 40 + h * w * 4;
		Vector<BYTE> v;
		v.resize(icon_len);
		BYTE *icon_bmp = v.ptrw();

		encode_uint32(40, &icon_bmp[0]);
		encode_uint32(w, &icon_bmp[4]);
		encode_uint32(h * 2, &icon_bmp[8]);
		encode_uint16(1, &icon_bmp[12]);
		encode_uint16(32, &icon_bmp[14]);
		encode_uint32(BI_RGB, &icon_bmp[16]);
		encode_uint32(w * h * 4, &icon_bmp[20]);
		encode_uint32(0, &icon_bmp[24]);
		encode_uint32(0, &icon_bmp[28]);
		encode_uint32(0, &icon_bmp[32]);
		encode_uint32(0, &icon_bmp[36]);

		uint8_t *wr = &icon_bmp[40];
		const uint8_t *r = img->get_data().ptr();

		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				const uint8_t *rpx = &r[((h - i - 1) * w + j) * 4];
				uint8_t *wpx = &wr[(i * w + j) * 4];
				wpx[0] = rpx[2];
				wpx[1] = rpx[1];
				wpx[2] = rpx[0];
				wpx[3] = rpx[3];
			}
		}

		HICON hicon = CreateIconFromResource(icon_bmp, icon_len, TRUE, 0x00030000);
		ERR_FAIL_NULL(hicon);

		icon = img;

		// Set the icon for the window.
		SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);

		// Set the icon in the task manager (should we do this?).
		SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
	} else {
		icon = Ref<Image>();
		SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_SETICON, ICON_SMALL, 0);
		SendMessage(windows[MAIN_WINDOW_ID].hWnd, WM_SETICON, ICON_BIG, 0);
	}
}

DisplayServer::IndicatorID DisplayServerWindows::create_status_indicator(const Ref<Texture2D> &p_icon, const String &p_tooltip, const Callable &p_callback) {
	HICON hicon = nullptr;
	if (p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
		Ref<Image> img = p_icon->get_image();
		img = img->duplicate();
		if (img->is_compressed()) {
			img->decompress();
		}
		img->convert(Image::FORMAT_RGBA8);

		int w = img->get_width();
		int h = img->get_height();

		// Create temporary bitmap buffer.
		int icon_len = 40 + h * w * 4;
		Vector<BYTE> v;
		v.resize(icon_len);
		BYTE *icon_bmp = v.ptrw();

		encode_uint32(40, &icon_bmp[0]);
		encode_uint32(w, &icon_bmp[4]);
		encode_uint32(h * 2, &icon_bmp[8]);
		encode_uint16(1, &icon_bmp[12]);
		encode_uint16(32, &icon_bmp[14]);
		encode_uint32(BI_RGB, &icon_bmp[16]);
		encode_uint32(w * h * 4, &icon_bmp[20]);
		encode_uint32(0, &icon_bmp[24]);
		encode_uint32(0, &icon_bmp[28]);
		encode_uint32(0, &icon_bmp[32]);
		encode_uint32(0, &icon_bmp[36]);

		uint8_t *wr = &icon_bmp[40];
		const uint8_t *r = img->get_data().ptr();

		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				const uint8_t *rpx = &r[((h - i - 1) * w + j) * 4];
				uint8_t *wpx = &wr[(i * w + j) * 4];
				wpx[0] = rpx[2];
				wpx[1] = rpx[1];
				wpx[2] = rpx[0];
				wpx[3] = rpx[3];
			}
		}

		hicon = CreateIconFromResource(icon_bmp, icon_len, TRUE, 0x00030000);
	}

	IndicatorData idat;
	idat.callback = p_callback;

	NOTIFYICONDATAW ndat;
	ZeroMemory(&ndat, sizeof(NOTIFYICONDATAW));
	ndat.cbSize = sizeof(NOTIFYICONDATAW);
	ndat.hWnd = windows[MAIN_WINDOW_ID].hWnd;
	ndat.uID = indicator_id_counter;
	ndat.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	ndat.uCallbackMessage = WM_INDICATOR_CALLBACK_MESSAGE;
	ndat.hIcon = hicon;
	memcpy(ndat.szTip, p_tooltip.utf16().get_data(), MIN(p_tooltip.utf16().length(), 127) * sizeof(WCHAR));
	ndat.uVersion = NOTIFYICON_VERSION;

	Shell_NotifyIconW(NIM_ADD, &ndat);
	Shell_NotifyIconW(NIM_SETVERSION, &ndat);

	IndicatorID iid = indicator_id_counter++;
	indicators[iid] = idat;

	return iid;
}

void DisplayServerWindows::status_indicator_set_icon(IndicatorID p_id, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND(!indicators.has(p_id));

	HICON hicon = nullptr;
	if (p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
		Ref<Image> img = p_icon->get_image();
		img = img->duplicate();
		if (img->is_compressed()) {
			img->decompress();
		}
		img->convert(Image::FORMAT_RGBA8);

		int w = img->get_width();
		int h = img->get_height();

		// Create temporary bitmap buffer.
		int icon_len = 40 + h * w * 4;
		Vector<BYTE> v;
		v.resize(icon_len);
		BYTE *icon_bmp = v.ptrw();

		encode_uint32(40, &icon_bmp[0]);
		encode_uint32(w, &icon_bmp[4]);
		encode_uint32(h * 2, &icon_bmp[8]);
		encode_uint16(1, &icon_bmp[12]);
		encode_uint16(32, &icon_bmp[14]);
		encode_uint32(BI_RGB, &icon_bmp[16]);
		encode_uint32(w * h * 4, &icon_bmp[20]);
		encode_uint32(0, &icon_bmp[24]);
		encode_uint32(0, &icon_bmp[28]);
		encode_uint32(0, &icon_bmp[32]);
		encode_uint32(0, &icon_bmp[36]);

		uint8_t *wr = &icon_bmp[40];
		const uint8_t *r = img->get_data().ptr();

		for (int i = 0; i < h; i++) {
			for (int j = 0; j < w; j++) {
				const uint8_t *rpx = &r[((h - i - 1) * w + j) * 4];
				uint8_t *wpx = &wr[(i * w + j) * 4];
				wpx[0] = rpx[2];
				wpx[1] = rpx[1];
				wpx[2] = rpx[0];
				wpx[3] = rpx[3];
			}
		}

		hicon = CreateIconFromResource(icon_bmp, icon_len, TRUE, 0x00030000);
	}

	NOTIFYICONDATAW ndat;
	ZeroMemory(&ndat, sizeof(NOTIFYICONDATAW));
	ndat.cbSize = sizeof(NOTIFYICONDATAW);
	ndat.hWnd = windows[MAIN_WINDOW_ID].hWnd;
	ndat.uID = p_id;
	ndat.uFlags = NIF_ICON;
	ndat.hIcon = hicon;
	ndat.uVersion = NOTIFYICON_VERSION;

	Shell_NotifyIconW(NIM_MODIFY, &ndat);
}

void DisplayServerWindows::status_indicator_set_tooltip(IndicatorID p_id, const String &p_tooltip) {
	ERR_FAIL_COND(!indicators.has(p_id));

	NOTIFYICONDATAW ndat;
	ZeroMemory(&ndat, sizeof(NOTIFYICONDATAW));
	ndat.cbSize = sizeof(NOTIFYICONDATAW);
	ndat.hWnd = windows[MAIN_WINDOW_ID].hWnd;
	ndat.uID = p_id;
	ndat.uFlags = NIF_TIP;
	memcpy(ndat.szTip, p_tooltip.utf16().get_data(), MIN(p_tooltip.utf16().length(), 127) * sizeof(WCHAR));
	ndat.uVersion = NOTIFYICON_VERSION;

	Shell_NotifyIconW(NIM_MODIFY, &ndat);
}

void DisplayServerWindows::status_indicator_set_menu(IndicatorID p_id, const RID &p_menu_rid) {
	ERR_FAIL_COND(!indicators.has(p_id));

	indicators[p_id].menu_rid = p_menu_rid;
}

void DisplayServerWindows::status_indicator_set_callback(IndicatorID p_id, const Callable &p_callback) {
	ERR_FAIL_COND(!indicators.has(p_id));

	indicators[p_id].callback = p_callback;
}

Rect2 DisplayServerWindows::status_indicator_get_rect(IndicatorID p_id) const {
	ERR_FAIL_COND_V(!indicators.has(p_id), Rect2());

	NOTIFYICONIDENTIFIER nid;
	ZeroMemory(&nid, sizeof(NOTIFYICONIDENTIFIER));
	nid.cbSize = sizeof(NOTIFYICONIDENTIFIER);
	nid.hWnd = windows[MAIN_WINDOW_ID].hWnd;
	nid.uID = p_id;
	nid.guidItem = GUID_NULL;

	RECT rect;
	if (Shell_NotifyIconGetRect(&nid, &rect) != S_OK) {
		return Rect2();
	}
	Rect2 ind_rect = Rect2(Point2(rect.left, rect.top) - _get_screens_origin(), Size2(rect.right - rect.left, rect.bottom - rect.top));
	for (int i = 0; i < get_screen_count(); i++) {
		Rect2 screen_rect = Rect2(screen_get_position(i), screen_get_size(i));
		if (screen_rect.encloses(ind_rect)) {
			return ind_rect;
		}
	}
	return Rect2();
}

void DisplayServerWindows::delete_status_indicator(IndicatorID p_id) {
	ERR_FAIL_COND(!indicators.has(p_id));

	NOTIFYICONDATAW ndat;
	ZeroMemory(&ndat, sizeof(NOTIFYICONDATAW));
	ndat.cbSize = sizeof(NOTIFYICONDATAW);
	ndat.hWnd = windows[MAIN_WINDOW_ID].hWnd;
	ndat.uID = p_id;
	ndat.uVersion = NOTIFYICON_VERSION;

	Shell_NotifyIconW(NIM_DELETE, &ndat);
	indicators.erase(p_id);
}

void DisplayServerWindows::window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, WindowID p_window) {
	_THREAD_SAFE_METHOD_
#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_vsync_mode(p_window, p_vsync_mode);
	}
#endif

#if defined(GLES3_ENABLED)
	if (gl_manager_native) {
		gl_manager_native->set_use_vsync(p_window, p_vsync_mode != DisplayServer::VSYNC_DISABLED);
	}
	if (gl_manager_angle) {
		gl_manager_angle->set_use_vsync(p_vsync_mode != DisplayServer::VSYNC_DISABLED);
	}
#endif
}

DisplayServer::VSyncMode DisplayServerWindows::window_get_vsync_mode(WindowID p_window) const {
	_THREAD_SAFE_METHOD_
#if defined(RD_ENABLED)
	if (rendering_context) {
		return rendering_context->window_get_vsync_mode(p_window);
	}
#endif

#if defined(GLES3_ENABLED)
	if (gl_manager_native) {
		return gl_manager_native->is_using_vsync(p_window) ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED;
	}
	if (gl_manager_angle) {
		return gl_manager_angle->is_using_vsync() ? DisplayServer::VSYNC_ENABLED : DisplayServer::VSYNC_DISABLED;
	}
#endif
	return DisplayServer::VSYNC_ENABLED;
}

void DisplayServerWindows::window_start_drag(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if (wd.parent_hwnd) {
		return; // Embedded window.
	}

	ReleaseCapture();

	POINT coords;
	GetCursorPos(&coords);
	ScreenToClient(wd.hWnd, &coords);

	SendMessage(wd.hWnd, WM_SYSCOMMAND, SC_MOVE | HTCAPTION, MAKELPARAM(coords.x, coords.y));
}

void DisplayServerWindows::window_start_resize(WindowResizeEdge p_edge, WindowID p_window) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_INDEX(int(p_edge), WINDOW_EDGE_MAX);
	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];

	if (wd.parent_hwnd) {
		return; // Embedded window.
	}

	ReleaseCapture();

	POINT coords;
	GetCursorPos(&coords);
	ScreenToClient(wd.hWnd, &coords);

	DWORD op = 0;
	switch (p_edge) {
		case DisplayServer::WINDOW_EDGE_TOP_LEFT: {
			op = WMSZ_TOPLEFT;
		} break;
		case DisplayServer::WINDOW_EDGE_TOP: {
			op = WMSZ_TOP;
		} break;
		case DisplayServer::WINDOW_EDGE_TOP_RIGHT: {
			op = WMSZ_TOPRIGHT;
		} break;
		case DisplayServer::WINDOW_EDGE_LEFT: {
			op = WMSZ_LEFT;
		} break;
		case DisplayServer::WINDOW_EDGE_RIGHT: {
			op = WMSZ_RIGHT;
		} break;
		case DisplayServer::WINDOW_EDGE_BOTTOM_LEFT: {
			op = WMSZ_BOTTOMLEFT;
		} break;
		case DisplayServer::WINDOW_EDGE_BOTTOM: {
			op = WMSZ_BOTTOM;
		} break;
		case DisplayServer::WINDOW_EDGE_BOTTOM_RIGHT: {
			op = WMSZ_BOTTOMRIGHT;
		} break;
		default:
			break;
	}

	SendMessage(wd.hWnd, WM_SYSCOMMAND, SC_SIZE | op, MAKELPARAM(coords.x, coords.y));
}

void DisplayServerWindows::set_context(Context p_context) {
}

bool DisplayServerWindows::is_window_transparency_available() const {
	BOOL dwm_enabled = true;
	if (DwmIsCompositionEnabled(&dwm_enabled) == S_OK) { // Note: Always enabled on Windows 8+, this check can be removed after Windows 7 support is dropped.
		if (!dwm_enabled) {
			return false;
		}
	}
#if defined(RD_ENABLED)
	if (rendering_device && !rendering_device->is_composite_alpha_supported()) {
		return false;
	}
#endif
	return OS::get_singleton()->is_layered_allowed();
}

#define MI_WP_SIGNATURE 0xFF515700
#define SIGNATURE_MASK 0xFFFFFF00
// Keeping the name suggested by Microsoft, but this macro really answers:
// Is this mouse event emulated from touch or pen input?
#define IsPenEvent(dw) (((dw) & SIGNATURE_MASK) == MI_WP_SIGNATURE)
// This one tells whether the event comes from touchscreen (and not from pen).
#define IsTouchEvent(dw) (IsPenEvent(dw) && ((dw) & 0x80))

void DisplayServerWindows::_touch_event(WindowID p_window, bool p_pressed, float p_x, float p_y, int idx) {
	if (touch_state.has(idx) == p_pressed) {
		return;
	}

	if (p_pressed) {
		touch_state.insert(idx, Vector2(p_x, p_y));
	} else {
		touch_state.erase(idx);
	}

	Ref<InputEventScreenTouch> event;
	event.instantiate();
	event->set_index(idx);
	event->set_window_id(p_window);
	event->set_pressed(p_pressed);
	event->set_position(Vector2(p_x, p_y));

	Input::get_singleton()->parse_input_event(event);
}

void DisplayServerWindows::_drag_event(WindowID p_window, float p_x, float p_y, int idx) {
	RBMap<int, Vector2>::Element *curr = touch_state.find(idx);
	if (!curr) {
		return;
	}

	if (curr->get() == Vector2(p_x, p_y)) {
		return;
	}

	Ref<InputEventScreenDrag> event;
	event.instantiate();
	event->set_window_id(p_window);
	event->set_index(idx);
	event->set_position(Vector2(p_x, p_y));
	event->set_relative(Vector2(p_x, p_y) - curr->get());
	event->set_relative_screen_position(event->get_relative());

	Input::get_singleton()->parse_input_event(event);

	curr->get() = Vector2(p_x, p_y);
}

void DisplayServerWindows::_send_window_event(const WindowData &wd, WindowEvent p_event) {
	if (wd.event_callback.is_valid()) {
		Variant event = int(p_event);
		wd.event_callback.call(event);
	}
}

void DisplayServerWindows::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	static_cast<DisplayServerWindows *>(get_singleton())->_dispatch_input_event(p_event);
}

void DisplayServerWindows::_dispatch_input_event(const Ref<InputEvent> &p_event) {
	if (in_dispatch_input_event) {
		return;
	}
	in_dispatch_input_event = true;

	{
		List<WindowID>::Element *E = popup_list.back();
		if (E && Object::cast_to<InputEventKey>(*p_event)) {
			// Redirect keyboard input to active popup.
			if (windows.has(E->get())) {
				Callable callable = windows[E->get()].input_event_callback;
				if (callable.is_valid()) {
					callable.call(p_event);
				}
			}
			in_dispatch_input_event = false;
			return;
		}
	}

	Ref<InputEventFromWindow> event_from_window = p_event;
	if (event_from_window.is_valid() && event_from_window->get_window_id() != INVALID_WINDOW_ID) {
		// Send to a single window.
		if (windows.has(event_from_window->get_window_id())) {
			Callable callable = windows[event_from_window->get_window_id()].input_event_callback;
			if (callable.is_valid()) {
				callable.call(p_event);
			}
		}
	} else {
		// Send to all windows.
		for (const KeyValue<WindowID, WindowData> &E : windows) {
			const Callable callable = E.value.input_event_callback;
			if (callable.is_valid()) {
				callable.call(p_event);
			}
		}
	}

	in_dispatch_input_event = false;
}

LRESULT CALLBACK MouseProc(int code, WPARAM wParam, LPARAM lParam) {
	DisplayServerWindows *ds_win = static_cast<DisplayServerWindows *>(DisplayServer::get_singleton());
	if (ds_win) {
		return ds_win->MouseProc(code, wParam, lParam);
	} else {
		return ::CallNextHookEx(nullptr, code, wParam, lParam);
	}
}

DisplayServer::WindowID DisplayServerWindows::window_get_active_popup() const {
	const List<WindowID>::Element *E = popup_list.back();
	if (E) {
		return E->get();
	} else {
		return INVALID_WINDOW_ID;
	}
}

void DisplayServerWindows::window_set_popup_safe_rect(WindowID p_window, const Rect2i &p_rect) {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND(!windows.has(p_window));
	WindowData &wd = windows[p_window];
	wd.parent_safe_rect = p_rect;
}

Rect2i DisplayServerWindows::window_get_popup_safe_rect(WindowID p_window) const {
	_THREAD_SAFE_METHOD_

	ERR_FAIL_COND_V(!windows.has(p_window), Rect2i());
	const WindowData &wd = windows[p_window];
	return wd.parent_safe_rect;
}

void DisplayServerWindows::popup_open(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	bool has_popup_ancestor = false;
	WindowID transient_root = p_window;
	while (true) {
		WindowID parent = windows[transient_root].transient_parent;
		if (parent == INVALID_WINDOW_ID) {
			break;
		} else {
			transient_root = parent;
			if (windows[parent].is_popup) {
				has_popup_ancestor = true;
				break;
			}
		}
	}

	// Detect tooltips and other similar popups that shouldn't block input to their parent.
	bool ignores_input = window_get_flag(WINDOW_FLAG_NO_FOCUS, p_window) && window_get_flag(WINDOW_FLAG_MOUSE_PASSTHROUGH, p_window);

	WindowData &wd = windows[p_window];
	if (wd.is_popup || (has_popup_ancestor && !ignores_input)) {
		// Find current popup parent, or root popup if new window is not transient.
		List<WindowID>::Element *C = nullptr;
		List<WindowID>::Element *E = popup_list.back();
		while (E) {
			if (wd.transient_parent != E->get() || wd.transient_parent == INVALID_WINDOW_ID) {
				C = E;
				E = E->prev();
			} else {
				break;
			}
		}
		if (C) {
			_send_window_event(windows[C->get()], DisplayServerWindows::WINDOW_EVENT_CLOSE_REQUEST);
		}

		time_since_popup = OS::get_singleton()->get_ticks_msec();
		popup_list.push_back(p_window);
	}
}

void DisplayServerWindows::popup_close(WindowID p_window) {
	_THREAD_SAFE_METHOD_

	List<WindowID>::Element *E = popup_list.find(p_window);
	while (E) {
		List<WindowID>::Element *F = E->next();
		WindowID win_id = E->get();
		popup_list.erase(E);

		if (win_id != p_window) {
			// Only request close on related windows, not this window.  We are already processing it.
			_send_window_event(windows[win_id], DisplayServerWindows::WINDOW_EVENT_CLOSE_REQUEST);
		}
		E = F;
	}
}

BitField<DisplayServerWindows::WinKeyModifierMask> DisplayServerWindows::_get_mods() const {
	BitField<WinKeyModifierMask> mask;
	static unsigned char keyboard_state[256];
	if (GetKeyboardState((PBYTE)&keyboard_state)) {
		if ((keyboard_state[VK_LSHIFT] & 0x80) || (keyboard_state[VK_RSHIFT] & 0x80)) {
			mask.set_flag(WinKeyModifierMask::SHIFT);
		}
		if ((keyboard_state[VK_LCONTROL] & 0x80) || (keyboard_state[VK_RCONTROL] & 0x80)) {
			mask.set_flag(WinKeyModifierMask::CTRL);
		}
		if ((keyboard_state[VK_LMENU] & 0x80) || (keyboard_state[VK_RMENU] & 0x80)) {
			mask.set_flag(WinKeyModifierMask::ALT);
		}
		if ((keyboard_state[VK_RMENU] & 0x80)) {
			mask.set_flag(WinKeyModifierMask::ALT_GR);
		}
		if ((keyboard_state[VK_LWIN] & 0x80) || (keyboard_state[VK_RWIN] & 0x80)) {
			mask.set_flag(WinKeyModifierMask::META);
		}
	}

	return mask;
}

LRESULT DisplayServerWindows::MouseProc(int code, WPARAM wParam, LPARAM lParam) {
	_THREAD_SAFE_METHOD_

	uint64_t delta = OS::get_singleton()->get_ticks_msec() - time_since_popup;
	if (delta > 250) {
		switch (wParam) {
			case WM_NCLBUTTONDOWN:
			case WM_NCRBUTTONDOWN:
			case WM_NCMBUTTONDOWN:
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN: {
				MOUSEHOOKSTRUCT *ms = (MOUSEHOOKSTRUCT *)lParam;
				Point2i pos = Point2i(ms->pt.x, ms->pt.y) - _get_screens_origin();
				List<WindowID>::Element *C = nullptr;
				List<WindowID>::Element *E = popup_list.back();
				// Find top popup to close.
				while (E) {
					// Popup window area.
					Rect2i win_rect = Rect2i(window_get_position_with_decorations(E->get()), window_get_size_with_decorations(E->get()));
					// Area of the parent window, which responsible for opening sub-menu.
					Rect2i safe_rect = window_get_popup_safe_rect(E->get());
					if (win_rect.has_point(pos)) {
						break;
					} else if (safe_rect != Rect2i() && safe_rect.has_point(pos)) {
						break;
					} else {
						C = E;
						E = E->prev();
					}
				}
				if (C) {
					_send_window_event(windows[C->get()], DisplayServerWindows::WINDOW_EVENT_CLOSE_REQUEST);
					return 1;
				}
			} break;
		}
	}
	return ::CallNextHookEx(mouse_monitor, code, wParam, lParam);
}

// Handle a single window message received while CreateWindowEx is still on the stack and our data
// structures are not fully initialized.
LRESULT DisplayServerWindows::_handle_early_window_message(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_GETMINMAXINFO: {
			// We receive this during CreateWindowEx and we haven't initialized the window
			// struct, so let Windows figure out the maximized size.
			// Silently forward to user/default.
		} break;
		case WM_NCCREATE: {
			// We tunnel an unowned pointer to our window context (WindowData) through the
			// first possible message (WM_NCCREATE) to fix up our window context collection.
			CREATESTRUCTW *pCreate = (CREATESTRUCTW *)lParam;
			WindowData *pWindowData = reinterpret_cast<WindowData *>(pCreate->lpCreateParams);

			// Fix this up so we can recognize the remaining messages.
			pWindowData->hWnd = hWnd;
		} break;
		default: {
			// Additional messages during window creation should happen after we fixed
			// up the data structures on WM_NCCREATE, but this might change in the future,
			// so report an error here and then we can implement them.
			ERR_PRINT_ONCE(vformat("Unexpected window message 0x%x received for window we cannot recognize in our collection; sequence error.", uMsg));
		} break;
	}

	if (user_proc) {
		return CallWindowProcW(user_proc, hWnd, uMsg, wParam, lParam);
	}
	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

// The window procedure for our window class "Engine", used to handle processing of window-related system messages/events.
// See: https://docs.microsoft.com/en-us/windows/win32/winmsg/window-procedures
LRESULT DisplayServerWindows::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (drop_events) {
		if (user_proc) {
			return CallWindowProcW(user_proc, hWnd, uMsg, wParam, lParam);
		} else {
			return DefWindowProcW(hWnd, uMsg, wParam, lParam);
		}
	}

	WindowID window_id = INVALID_WINDOW_ID;
	bool window_created = false;

	// Check whether window exists
	// FIXME this is O(n), where n is the set of currently open windows and subwindows
	// we should have a secondary map from HWND to WindowID or even WindowData* alias, if we want to eliminate all the map lookups below
	for (const KeyValue<WindowID, WindowData> &E : windows) {
		if (E.value.hWnd == hWnd) {
			window_id = E.key;
			window_created = true;
			break;
		}
	}

	// WARNING: We get called with events before the window is registered in our collection
	// specifically, even the call to CreateWindowEx already calls here while still on the stack,
	// so there is no way to store the window handle in our collection before we get here.
	if (!window_created) {
		// don't let code below operate on incompletely initialized window objects or missing window_id
		return _handle_early_window_message(hWnd, uMsg, wParam, lParam);
	}

	// Process window messages.
	switch (uMsg) {
		case WM_MENUCOMMAND: {
			native_menu->_menu_activate(HMENU(lParam), (int)wParam);
		} break;
		case WM_CREATE: {
			{
				DWORD value = windows[window_id].sharp_corners ? DWMWCP_DONOTROUND : DWMWCP_DEFAULT;
				::DwmSetWindowAttribute(windows[window_id].hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &value, sizeof(value));
			}
			if (is_dark_mode_supported() && dark_title_available) {
				BOOL value = is_dark_mode();

				::DwmSetWindowAttribute(windows[window_id].hWnd, use_legacy_dark_mode_before_20H1 ? DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 : DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
				SendMessageW(windows[window_id].hWnd, WM_PAINT, 0, 0);
			}
		} break;
		case WM_NCPAINT: {
			if (RenderingServer::get_singleton() && (windows[window_id].borderless || (windows[window_id].fullscreen && windows[window_id].multiwindow_fs))) {
				Color color = RenderingServer::get_singleton()->get_default_clear_color();
				HDC hdc = GetWindowDC(hWnd);
				if (hdc) {
					HPEN pen = CreatePen(PS_SOLID, 1, RGB(color.r * 255.f, color.g * 255.f, color.b * 255.f));
					if (pen) {
						HGDIOBJ prev_pen = SelectObject(hdc, pen);
						HGDIOBJ prev_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

						RECT rc;
						GetWindowRect(hWnd, &rc);
						OffsetRect(&rc, -rc.left, -rc.top);
						Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

						SelectObject(hdc, prev_pen);
						SelectObject(hdc, prev_brush);
						DeleteObject(pen);
					}
					ReleaseDC(hWnd, hdc);
				}
				return 0;
			}
		} break;
		case WM_NCHITTEST: {
			if (windows[window_id].mpass) {
				return HTTRANSPARENT;
			}
		} break;
		case WM_MOUSEACTIVATE: {
			if (windows[window_id].no_focus || windows[window_id].is_popup) {
				return MA_NOACTIVATE; // Do not activate, but process mouse messages.
			}
			// When embedded, the window is a child of the parent and is not activated
			// by default because it lacks native controls.
			if (windows[window_id].parent_hwnd) {
				SetFocus(windows[window_id].hWnd);
				return MA_ACTIVATE;
			}
		} break;
		case WM_ACTIVATEAPP: {
			bool new_app_focused = (bool)wParam;
			if (new_app_focused == app_focused) {
				break;
			}
			app_focused = new_app_focused;
			if (OS::get_singleton()->get_main_loop()) {
				OS::get_singleton()->get_main_loop()->notification(app_focused ? MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN : MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT);
			}
		} break;
		case WM_ACTIVATE: {
			// Activation can happen just after the window has been created, even before the callbacks are set.
			// Therefore, it's safer to defer the delivery of the event.
			// It's important to set an nIDEvent different from the SetTimer for move_timer_id because
			// if the same nIDEvent is passed, the timer is replaced and the same timer_id is returned.
			// The problem with the timer is that the window cannot be resized or the buttons cannot be used correctly
			// if the window is not activated first. This happens because the code in the activation process runs
			// after the mouse click is handled. To address this, the timer is now used only when the window is created.
			windows[window_id].activate_state = GET_WM_ACTIVATE_STATE(wParam, lParam);
			if (windows[window_id].first_activation_done) {
				_process_activate_event(window_id);
			} else {
				windows[window_id].activate_timer_id = SetTimer(windows[window_id].hWnd, DisplayServerWindows::TIMER_ID_WINDOW_ACTIVATION, USER_TIMER_MINIMUM, (TIMERPROC) nullptr);
			}
			return 0;
		} break;
		case WM_GETMINMAXINFO: {
			if (windows[window_id].resizable && !windows[window_id].fullscreen) {
				// Size of window decorations.
				Size2 decor = window_get_size_with_decorations(window_id) - window_get_size(window_id);

				MINMAXINFO *min_max_info = (MINMAXINFO *)lParam;
				if (windows[window_id].min_size != Size2()) {
					min_max_info->ptMinTrackSize.x = windows[window_id].min_size.x + decor.x;
					min_max_info->ptMinTrackSize.y = windows[window_id].min_size.y + decor.y;
				}
				if (windows[window_id].max_size != Size2()) {
					min_max_info->ptMaxTrackSize.x = windows[window_id].max_size.x + decor.x;
					min_max_info->ptMaxTrackSize.y = windows[window_id].max_size.y + decor.y;
				}
				if (windows[window_id].borderless) {
					Rect2i screen_rect = screen_get_usable_rect(window_get_current_screen(window_id));

					// Set the size of (borderless) maximized mode to exclude taskbar (or any other panel) if present.
					min_max_info->ptMaxPosition.x = screen_rect.position.x;
					min_max_info->ptMaxPosition.y = screen_rect.position.y;
					min_max_info->ptMaxSize.x = screen_rect.size.x;
					min_max_info->ptMaxSize.y = screen_rect.size.y;
				}
				return 0;
			}
		} break;
		case WM_ERASEBKGND: {
			Color early_color;
			if (!_get_window_early_clear_override(early_color)) {
				break;
			}
			bool must_recreate_brush = !window_bkg_brush || window_bkg_brush_color != early_color.to_argb32();
			if (must_recreate_brush) {
				if (window_bkg_brush) {
					DeleteObject(window_bkg_brush);
				}
				window_bkg_brush = CreateSolidBrush(RGB(early_color.get_r8(), early_color.get_g8(), early_color.get_b8()));
			}
			HDC hdc = (HDC)wParam;
			RECT rect = {};
			if (GetUpdateRect(hWnd, &rect, true)) {
				FillRect(hdc, &rect, window_bkg_brush);
			}
			return 1;
		} break;
		case WM_PAINT: {
			Main::force_redraw();
		} break;
		case WM_SETTINGCHANGE:
		case WM_SYSCOLORCHANGE: {
			if (lParam && CompareStringOrdinal(reinterpret_cast<LPCWCH>(lParam), -1, L"ImmersiveColorSet", -1, true) == CSTR_EQUAL) {
				if (is_dark_mode_supported() && dark_title_available) {
					BOOL value = is_dark_mode();
					::DwmSetWindowAttribute(windows[window_id].hWnd, use_legacy_dark_mode_before_20H1 ? DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 : DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
				}
			}
			if (system_theme_changed.is_valid()) {
				Variant ret;
				Callable::CallError ce;
				system_theme_changed.callp(nullptr, 0, ret, ce);
				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT(vformat("Failed to execute system theme changed callback: %s.", Variant::get_callable_error_text(system_theme_changed, nullptr, 0, ce)));
				}
			}
		} break;
		case WM_THEMECHANGED: {
			if (is_dark_mode_supported() && dark_title_available) {
				BOOL value = is_dark_mode();
				::DwmSetWindowAttribute(windows[window_id].hWnd, use_legacy_dark_mode_before_20H1 ? DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 : DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
			}
		} break;
		case WM_SYSCOMMAND: // Intercept system commands.
		{
			switch (wParam) // Check system calls.
			{
				case SC_SCREENSAVE: // Screensaver trying to start?
				case SC_MONITORPOWER: // Monitor trying to enter powersave?
					return 0; // Prevent from happening.
				case SC_KEYMENU:
					Engine *engine = Engine::get_singleton();
					if (((lParam >> 16) <= 0) && !engine->is_project_manager_hint() && !engine->is_editor_hint() && !GLOBAL_GET("application/run/enable_alt_space_menu")) {
						return 0;
					}
					if (!_get_mods().has_flag(WinKeyModifierMask::ALT) || !(GetAsyncKeyState(VK_SPACE) & (1 << 15))) {
						return 0;
					}
					SendMessage(windows[window_id].hWnd, WM_SYSKEYUP, VK_SPACE, 0);
					SendMessage(windows[window_id].hWnd, WM_SYSKEYUP, VK_MENU, 0);
			}
		} break;
		case WM_INDICATOR_CALLBACK_MESSAGE: {
			if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONDOWN || lParam == WM_MBUTTONDOWN || lParam == WM_XBUTTONDOWN) {
				IndicatorID iid = (IndicatorID)wParam;
				MouseButton mb = MouseButton::LEFT;
				if (lParam == WM_RBUTTONDOWN) {
					mb = MouseButton::RIGHT;
				} else if (lParam == WM_MBUTTONDOWN) {
					mb = MouseButton::MIDDLE;
				} else if (lParam == WM_XBUTTONDOWN) {
					mb = MouseButton::MB_XBUTTON1;
				}
				if (indicators.has(iid)) {
					if (lParam == WM_RBUTTONDOWN && indicators[iid].menu_rid.is_valid() && native_menu->has_menu(indicators[iid].menu_rid)) {
						NOTIFYICONIDENTIFIER nid;
						ZeroMemory(&nid, sizeof(NOTIFYICONIDENTIFIER));
						nid.cbSize = sizeof(NOTIFYICONIDENTIFIER);
						nid.hWnd = windows[MAIN_WINDOW_ID].hWnd;
						nid.uID = iid;
						nid.guidItem = GUID_NULL;

						RECT rect;
						if (Shell_NotifyIconGetRect(&nid, &rect) == S_OK) {
							native_menu->popup(indicators[iid].menu_rid, Vector2i((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2));
						}
					} else if (indicators[iid].callback.is_valid()) {
						Variant v_button = mb;
						Variant v_pos = mouse_get_position();
						const Variant *v_args[2] = { &v_button, &v_pos };
						Variant ret;
						Callable::CallError ce;
						indicators[iid].callback.callp((const Variant **)&v_args, 2, ret, ce);
						if (ce.error != Callable::CallError::CALL_OK) {
							ERR_PRINT(vformat("Failed to execute status indicator callback: %s.", Variant::get_callable_error_text(indicators[iid].callback, v_args, 2, ce)));
						}
					}
				}
				return 0;
			}
		} break;
		case WM_CLOSE: {
			if (windows[window_id].activate_timer_id) {
				KillTimer(windows[window_id].hWnd, windows[window_id].activate_timer_id);
				windows[window_id].activate_timer_id = 0;
			}
			_send_window_event(windows[window_id], WINDOW_EVENT_CLOSE_REQUEST);
			return 0;
		}
		case WM_MOUSELEAVE: {
			if (window_mouseover_id == window_id) {
				old_invalid = true;
				window_mouseover_id = INVALID_WINDOW_ID;

				_send_window_event(windows[window_id], WINDOW_EVENT_MOUSE_EXIT);
			} else if (window_mouseover_id != INVALID_WINDOW_ID && windows.has(window_mouseover_id)) {
				// This is reached during drag and drop, after dropping in a different window.
				// Once-off notification, must call again.
				track_mouse_leave_event(windows[window_mouseover_id].hWnd);
			}

		} break;
		case WM_INPUT: {
			if (!use_raw_input) {
				break;
			}

			UINT dwSize;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
			LPBYTE lpb = new BYTE[dwSize];
			if (lpb == nullptr) {
				return 0;
			}

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize) {
				OutputDebugString(TEXT("GetRawInputData does not return correct size !\n"));
			}

			RAWINPUT *raw = (RAWINPUT *)lpb;

			const BitField<WinKeyModifierMask> &mods = _get_mods();
			if (raw->header.dwType == RIM_TYPEKEYBOARD) {
				if (raw->data.keyboard.VKey == VK_SHIFT) {
					// If multiple Shifts are held down at the same time,
					// Windows natively only sends a KEYUP for the last one to be released.
					if (raw->data.keyboard.Flags & RI_KEY_BREAK) {
						// Make sure to check the latest key state since
						// we're in the middle of the message queue.
						if (GetAsyncKeyState(VK_SHIFT) < 0) {
							// A Shift is released, but another Shift is still held
							ERR_BREAK(key_event_pos >= KEY_EVENT_BUFFER_SIZE);

							KeyEvent ke;
							ke.shift = false;
							ke.altgr = mods.has_flag(WinKeyModifierMask::ALT_GR);
							ke.alt = mods.has_flag(WinKeyModifierMask::ALT);
							ke.control = mods.has_flag(WinKeyModifierMask::CTRL);
							ke.meta = mods.has_flag(WinKeyModifierMask::META);
							ke.uMsg = WM_KEYUP;
							ke.window_id = window_id;

							ke.wParam = VK_SHIFT;
							// data.keyboard.MakeCode -> 0x2A - left shift, 0x36 - right shift.
							// Bit 30 -> key was previously down, bit 31 -> key is being released.
							ke.lParam = raw->data.keyboard.MakeCode << 16 | 1 << 30 | 1 << 31;
							key_event_buffer[key_event_pos++] = ke;
						}
					}
				}
			} else if (mouse_mode == MOUSE_MODE_CAPTURED && raw->header.dwType == RIM_TYPEMOUSE) {
				Ref<InputEventMouseMotion> mm;
				mm.instantiate();

				mm->set_window_id(window_id);
				mm->set_ctrl_pressed(mods.has_flag(WinKeyModifierMask::CTRL));
				mm->set_shift_pressed(mods.has_flag(WinKeyModifierMask::SHIFT));
				mm->set_alt_pressed(mods.has_flag(WinKeyModifierMask::ALT));
				mm->set_meta_pressed(mods.has_flag(WinKeyModifierMask::META));

				mm->set_pressure((raw->data.mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_DOWN) ? 1.0f : 0.0f);

				mm->set_button_mask(mouse_get_button_state());

				Point2i c(windows[window_id].width / 2, windows[window_id].height / 2);

				// Centering just so it works as before.
				POINT pos = { (int)c.x, (int)c.y };
				ClientToScreen(windows[window_id].hWnd, &pos);
				SetCursorPos(pos.x, pos.y);

				mm->set_position(c);
				mm->set_global_position(c);
				mm->set_velocity(Vector2(0, 0));
				mm->set_screen_velocity(Vector2(0, 0));

				if (raw->data.mouse.usFlags == MOUSE_MOVE_RELATIVE) {
					mm->set_relative(Vector2(raw->data.mouse.lLastX, raw->data.mouse.lLastY));

				} else if (raw->data.mouse.usFlags == MOUSE_MOVE_ABSOLUTE) {
					int nScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
					int nScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
					int nScreenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
					int nScreenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);

					Vector2 abs_pos(
							(double(raw->data.mouse.lLastX) - 65536.0 / (nScreenWidth)) * nScreenWidth / 65536.0 + nScreenLeft,
							(double(raw->data.mouse.lLastY) - 65536.0 / (nScreenHeight)) * nScreenHeight / 65536.0 + nScreenTop);

					POINT coords; // Client coords.
					coords.x = abs_pos.x;
					coords.y = abs_pos.y;

					ScreenToClient(hWnd, &coords);

					mm->set_relative(Vector2(coords.x - old_x, coords.y - old_y));
					old_x = coords.x;
					old_y = coords.y;
				}
				mm->set_relative_screen_position(mm->get_relative());

				if ((windows[window_id].window_focused || windows[window_id].is_popup) && mm->get_relative() != Vector2()) {
					Input::get_singleton()->parse_input_event(mm);
				}
			}
			delete[] lpb;
		} break;
		case WT_CSRCHANGE:
		case WT_PROXIMITY: {
			if ((tablet_get_current_driver() == "wintab") && wintab_available && windows[window_id].wtctx) {
				AXIS pressure;
				if (wintab_WTInfo(WTI_DEVICES + windows[window_id].wtlc.lcDevice, DVC_NPRESSURE, &pressure)) {
					windows[window_id].min_pressure = int(pressure.axMin);
					windows[window_id].max_pressure = int(pressure.axMax);
				}
				AXIS orientation[3];
				if (wintab_WTInfo(WTI_DEVICES + windows[window_id].wtlc.lcDevice, DVC_ORIENTATION, &orientation)) {
					windows[window_id].tilt_supported = orientation[0].axResolution && orientation[1].axResolution;
				}
				return 0;
			}
		} break;
		case WT_PACKET: {
			if ((tablet_get_current_driver() == "wintab") && wintab_available && windows[window_id].wtctx) {
				PACKET packet;
				if (wintab_WTPacket(windows[window_id].wtctx, wParam, &packet)) {
					POINT coords;
					GetCursorPos(&coords);
					ScreenToClient(windows[window_id].hWnd, &coords);

					windows[window_id].last_pressure_update = 0;

					float pressure = float(packet.pkNormalPressure - windows[window_id].min_pressure) / float(windows[window_id].max_pressure - windows[window_id].min_pressure);
					double azim = (packet.pkOrientation.orAzimuth / 10.0f) * (Math_PI / 180);
					double alt = Math::tan((Math::abs(packet.pkOrientation.orAltitude / 10.0f)) * (Math_PI / 180));
					bool inverted = packet.pkStatus & TPS_INVERT;

					Vector2 tilt = (windows[window_id].tilt_supported) ? Vector2(Math::atan(Math::sin(azim) / alt), Math::atan(Math::cos(azim) / alt)) : Vector2();

					// Nothing changed, ignore event.
					if (!old_invalid && coords.x == old_x && coords.y == old_y && windows[window_id].last_pressure == pressure && windows[window_id].last_tilt == tilt && windows[window_id].last_pen_inverted == inverted) {
						break;
					}

					windows[window_id].last_pressure = pressure;
					windows[window_id].last_tilt = tilt;
					windows[window_id].last_pen_inverted = inverted;

					// Don't calculate relative mouse movement if we don't have focus in CAPTURED mode.
					if (!windows[window_id].window_focused && mouse_mode == MOUSE_MODE_CAPTURED) {
						break;
					}

					const BitField<WinKeyModifierMask> &mods = _get_mods();
					Ref<InputEventMouseMotion> mm;
					mm.instantiate();
					mm->set_window_id(window_id);
					mm->set_ctrl_pressed(mods.has_flag(WinKeyModifierMask::CTRL));
					mm->set_shift_pressed(mods.has_flag(WinKeyModifierMask::SHIFT));
					mm->set_alt_pressed(mods.has_flag(WinKeyModifierMask::ALT));
					mm->set_meta_pressed(mods.has_flag(WinKeyModifierMask::META));

					mm->set_pressure(windows[window_id].last_pressure);
					mm->set_tilt(windows[window_id].last_tilt);
					mm->set_pen_inverted(windows[window_id].last_pen_inverted);

					mm->set_button_mask(mouse_get_button_state());

					mm->set_position(Vector2(coords.x, coords.y));
					mm->set_global_position(Vector2(coords.x, coords.y));

					if (mouse_mode == MOUSE_MODE_CAPTURED) {
						Point2i c(windows[window_id].width / 2, windows[window_id].height / 2);
						old_x = c.x;
						old_y = c.y;

						if (mm->get_position() == c) {
							center = c;
							return 0;
						}

						Point2i ncenter = mm->get_position();
						center = ncenter;
						POINT pos = { (int)c.x, (int)c.y };
						ClientToScreen(windows[window_id].hWnd, &pos);
						SetCursorPos(pos.x, pos.y);
					}

					mm->set_velocity(Input::get_singleton()->get_last_mouse_velocity());
					mm->set_screen_velocity(mm->get_velocity());

					if (old_invalid) {
						old_x = mm->get_position().x;
						old_y = mm->get_position().y;
						old_invalid = false;
					}

					mm->set_relative(Vector2(mm->get_position() - Vector2(old_x, old_y)));
					mm->set_relative_screen_position(mm->get_relative());
					old_x = mm->get_position().x;
					old_y = mm->get_position().y;

					if (windows[window_id].window_focused || window_get_active_popup() == window_id) {
						Input::get_singleton()->parse_input_event(mm);
					}
				}
				return 0;
			}
		} break;
		case WM_POINTERENTER: {
			if (mouse_mode == MOUSE_MODE_CAPTURED && use_raw_input) {
				break;
			}

			if ((tablet_get_current_driver() != "winink") || !winink_available) {
				break;
			}

			uint32_t pointer_id = LOWORD(wParam);
			POINTER_INPUT_TYPE pointer_type = PT_POINTER;
			if (!win8p_GetPointerType(pointer_id, &pointer_type)) {
				break;
			}

			if (pointer_type != PT_PEN) {
				break;
			}

			pointer_button[GET_POINTERID_WPARAM(wParam)] = MouseButton::NONE;
			windows[window_id].block_mm = true;
			return 0;
		} break;
		case WM_POINTERLEAVE: {
			pointer_button[GET_POINTERID_WPARAM(wParam)] = MouseButton::NONE;
			windows[window_id].block_mm = false;
			return 0;
		} break;
		case WM_POINTERDOWN:
		case WM_POINTERUP: {
			if (mouse_mode == MOUSE_MODE_CAPTURED && use_raw_input) {
				break;
			}

			if ((tablet_get_current_driver() != "winink") || !winink_available) {
				break;
			}

			uint32_t pointer_id = LOWORD(wParam);
			POINTER_INPUT_TYPE pointer_type = PT_POINTER;
			if (!win8p_GetPointerType(pointer_id, &pointer_type)) {
				break;
			}

			if (pointer_type != PT_PEN) {
				break;
			}

			Ref<InputEventMouseButton> mb;
			mb.instantiate();
			mb->set_window_id(window_id);

			BitField<MouseButtonMask> last_button_state = 0;
			if (IS_POINTER_FIRSTBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::LEFT);
				mb->set_button_index(MouseButton::LEFT);
			}
			if (IS_POINTER_SECONDBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::RIGHT);
				mb->set_button_index(MouseButton::RIGHT);
			}
			if (IS_POINTER_THIRDBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::MIDDLE);
				mb->set_button_index(MouseButton::MIDDLE);
			}
			if (IS_POINTER_FOURTHBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::MB_XBUTTON1);
				mb->set_button_index(MouseButton::MB_XBUTTON1);
			}
			if (IS_POINTER_FIFTHBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::MB_XBUTTON2);
				mb->set_button_index(MouseButton::MB_XBUTTON2);
			}
			mb->set_button_mask(last_button_state);

			const BitField<WinKeyModifierMask> &mods = _get_mods();
			mb->set_ctrl_pressed(mods.has_flag(WinKeyModifierMask::CTRL));
			mb->set_shift_pressed(mods.has_flag(WinKeyModifierMask::SHIFT));
			mb->set_alt_pressed(mods.has_flag(WinKeyModifierMask::ALT));
			mb->set_meta_pressed(mods.has_flag(WinKeyModifierMask::META));

			POINT coords; // Client coords.
			coords.x = GET_X_LPARAM(lParam);
			coords.y = GET_Y_LPARAM(lParam);

			// Note: Handle popup closing here, since mouse event is not emulated and hook will not be called.
			uint64_t delta = OS::get_singleton()->get_ticks_msec() - time_since_popup;
			if (delta > 250) {
				Point2i pos = Point2i(coords.x, coords.y) - _get_screens_origin();
				List<WindowID>::Element *C = nullptr;
				List<WindowID>::Element *E = popup_list.back();
				// Find top popup to close.
				while (E) {
					// Popup window area.
					Rect2i win_rect = Rect2i(window_get_position_with_decorations(E->get()), window_get_size_with_decorations(E->get()));
					// Area of the parent window, which responsible for opening sub-menu.
					Rect2i safe_rect = window_get_popup_safe_rect(E->get());
					if (win_rect.has_point(pos)) {
						break;
					} else if (safe_rect != Rect2i() && safe_rect.has_point(pos)) {
						break;
					} else {
						C = E;
						E = E->prev();
					}
				}
				if (C) {
					_send_window_event(windows[C->get()], DisplayServerWindows::WINDOW_EVENT_CLOSE_REQUEST);
				}
			}

			int64_t pen_id = GET_POINTERID_WPARAM(wParam);
			if (uMsg == WM_POINTERDOWN) {
				mb->set_pressed(true);
				if (pointer_down_time.has(pen_id) && (pointer_prev_button[pen_id] == mb->get_button_index()) && (ABS(coords.y - pointer_last_pos[pen_id].y) < GetSystemMetrics(SM_CYDOUBLECLK)) && GetMessageTime() - pointer_down_time[pen_id] < (LONG)GetDoubleClickTime()) {
					mb->set_double_click(true);
					pointer_down_time[pen_id] = 0;
				} else {
					pointer_down_time[pen_id] = GetMessageTime();
					pointer_prev_button[pen_id] = mb->get_button_index();
					pointer_last_pos[pen_id] = Vector2(coords.x, coords.y);
				}
				pointer_button[pen_id] = mb->get_button_index();
			} else {
				if (!pointer_button.has(pen_id)) {
					return 0;
				}
				mb->set_pressed(false);
				mb->set_button_index(pointer_button[pen_id]);
				pointer_button[pen_id] = MouseButton::NONE;
			}

			ScreenToClient(windows[window_id].hWnd, &coords);

			mb->set_position(Vector2(coords.x, coords.y));
			mb->set_global_position(Vector2(coords.x, coords.y));

			Input::get_singleton()->parse_input_event(mb);

			return 0;
		} break;
		case WM_POINTERUPDATE: {
			if (mouse_mode == MOUSE_MODE_CAPTURED && use_raw_input) {
				break;
			}

			if ((tablet_get_current_driver() != "winink") || !winink_available) {
				break;
			}

			uint32_t pointer_id = LOWORD(wParam);
			POINTER_INPUT_TYPE pointer_type = PT_POINTER;
			if (!win8p_GetPointerType(pointer_id, &pointer_type)) {
				break;
			}

			if (pointer_type != PT_PEN) {
				break;
			}

			POINTER_PEN_INFO pen_info;
			if (!win8p_GetPointerPenInfo(pointer_id, &pen_info)) {
				break;
			}

			if (Input::get_singleton()->is_emulating_mouse_from_touch()) {
				// Universal translation enabled; ignore OS translation.
				LPARAM extra = GetMessageExtraInfo();
				if (IsTouchEvent(extra)) {
					break;
				}
			}

			if (window_mouseover_id != window_id) {
				// Mouse enter.

				if (mouse_mode != MOUSE_MODE_CAPTURED) {
					if (window_mouseover_id != INVALID_WINDOW_ID && windows.has(window_mouseover_id)) {
						// Leave previous window.
						_send_window_event(windows[window_mouseover_id], WINDOW_EVENT_MOUSE_EXIT);
					}
					_send_window_event(windows[window_id], WINDOW_EVENT_MOUSE_ENTER);
				}

				CursorShape c = cursor_shape;
				cursor_shape = CURSOR_MAX;
				cursor_set_shape(c);
				window_mouseover_id = window_id;

				// Once-off notification, must call again.
				track_mouse_leave_event(hWnd);
			}

			// Don't calculate relative mouse movement if we don't have focus in CAPTURED mode.
			if (!windows[window_id].window_focused && mouse_mode == MOUSE_MODE_CAPTURED) {
				break;
			}

			Ref<InputEventMouseMotion> mm;
			mm.instantiate();

			mm->set_window_id(window_id);
			if (pen_info.penMask & PEN_MASK_PRESSURE) {
				mm->set_pressure((float)pen_info.pressure / 1024);
			} else {
				mm->set_pressure((HIWORD(wParam) & POINTER_MESSAGE_FLAG_FIRSTBUTTON) ? 1.0f : 0.0f);
			}
			if ((pen_info.penMask & PEN_MASK_TILT_X) && (pen_info.penMask & PEN_MASK_TILT_Y)) {
				mm->set_tilt(Vector2((float)pen_info.tiltX / 90, (float)pen_info.tiltY / 90));
			}
			mm->set_pen_inverted(pen_info.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER));

			const BitField<WinKeyModifierMask> &mods = _get_mods();
			mm->set_ctrl_pressed(mods.has_flag(WinKeyModifierMask::CTRL));
			mm->set_shift_pressed(mods.has_flag(WinKeyModifierMask::SHIFT));
			mm->set_alt_pressed(mods.has_flag(WinKeyModifierMask::ALT));
			mm->set_meta_pressed(mods.has_flag(WinKeyModifierMask::META));

			BitField<MouseButtonMask> last_button_state = 0;
			if (IS_POINTER_FIRSTBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::LEFT);
			}
			if (IS_POINTER_SECONDBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::RIGHT);
			}
			if (IS_POINTER_THIRDBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::MIDDLE);
			}
			if (IS_POINTER_FOURTHBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::MB_XBUTTON1);
			}
			if (IS_POINTER_FIFTHBUTTON_WPARAM(wParam)) {
				last_button_state.set_flag(MouseButtonMask::MB_XBUTTON2);
			}
			mm->set_button_mask(last_button_state);

			POINT coords; // Client coords.
			coords.x = GET_X_LPARAM(lParam);
			coords.y = GET_Y_LPARAM(lParam);

			ScreenToClient(windows[window_id].hWnd, &coords);

			mm->set_position(Vector2(coords.x, coords.y));
			mm->set_global_position(Vector2(coords.x, coords.y));

			if (mouse_mode == MOUSE_MODE_CAPTURED) {
				Point2i c(windows[window_id].width / 2, windows[window_id].height / 2);
				old_x = c.x;
				old_y = c.y;

				if (mm->get_position() == c) {
					center = c;
					return 0;
				}

				Point2i ncenter = mm->get_position();
				center = ncenter;
				POINT pos = { (int)c.x, (int)c.y };
				ClientToScreen(hWnd, &pos);
				SetCursorPos(pos.x, pos.y);
			}

			mm->set_velocity(Input::get_singleton()->get_last_mouse_velocity());
			mm->set_screen_velocity(mm->get_velocity());

			if (old_invalid) {
				old_x = mm->get_position().x;
				old_y = mm->get_position().y;
				old_invalid = false;
			}

			mm->set_relative(Vector2(mm->get_position() - Vector2(old_x, old_y)));
			mm->set_relative_screen_position(mm->get_relative());
			old_x = mm->get_position().x;
			old_y = mm->get_position().y;
			if (windows[window_id].window_focused || window_get_active_popup() == window_id) {
				Input::get_singleton()->parse_input_event(mm);
			}

			return 0; // Pointer event handled return 0 to avoid duplicate WM_MOUSEMOVE event.
		} break;
		case WM_MOUSEMOVE: {
			if (windows[window_id].block_mm) {
				break;
			}

			if (mouse_mode == MOUSE_MODE_CAPTURED && use_raw_input) {
				break;
			}

			if (Input::get_singleton()->is_emulating_mouse_from_touch()) {
				// Universal translation enabled; ignore OS translation.
				LPARAM extra = GetMessageExtraInfo();
				if (IsTouchEvent(extra)) {
					break;
				}
			}

			DisplayServer::WindowID over_id = get_window_at_screen_position(mouse_get_position());
			if (windows.has(over_id) && !Rect2(window_get_position(over_id), Point2(windows[over_id].width, windows[over_id].height)).has_point(mouse_get_position())) {
				// Don't consider the windowborder as part of the window.
				over_id = INVALID_WINDOW_ID;
			}
			if (window_mouseover_id != over_id) {
				// Mouse enter.

				if (mouse_mode != MOUSE_MODE_CAPTURED) {
					if (window_mouseover_id != INVALID_WINDOW_ID && windows.has(window_mouseover_id)) {
						// Leave previous window.
						_send_window_event(windows[window_mouseover_id], WINDOW_EVENT_MOUSE_EXIT);
					}

					if (over_id != INVALID_WINDOW_ID && windows.has(over_id)) {
						_send_window_event(windows[over_id], WINDOW_EVENT_MOUSE_ENTER);
					}
				}

				CursorShape c = cursor_shape;
				cursor_shape = CURSOR_MAX;
				cursor_set_shape(c);
				window_mouseover_id = over_id;

				// Once-off notification, must call again.
				track_mouse_leave_event(hWnd);
			}

			// Don't calculate relative mouse movement if we don't have focus in CAPTURED mode.
			if (!windows[window_id].window_focused && mouse_mode == MOUSE_MODE_CAPTURED) {
				break;
			}

			DisplayServer::WindowID receiving_window_id = window_id;
			if (!windows[window_id].no_focus) {
				receiving_window_id = _get_focused_window_or_popup();
				if (receiving_window_id == INVALID_WINDOW_ID) {
					receiving_window_id = window_id;
				}
			}

			const BitField<WinKeyModifierMask> &mods = _get_mods();
			Ref<InputEventMouseMotion> mm;
			mm.instantiate();
			mm->set_window_id(receiving_window_id);
			mm->set_ctrl_pressed(mods.has_flag(WinKeyModifierMask::CTRL));
			mm->set_shift_pressed(mods.has_flag(WinKeyModifierMask::SHIFT));
			mm->set_alt_pressed(mods.has_flag(WinKeyModifierMask::ALT));
			mm->set_meta_pressed(mods.has_flag(WinKeyModifierMask::META));

			if ((tablet_get_current_driver() == "wintab") && wintab_available && windows[window_id].wtctx) {
				// Note: WinTab sends both WT_PACKET and WM_xBUTTONDOWN/UP/MOUSEMOVE events, use mouse 1/0 pressure only when last_pressure was not updated recently.
				if (windows[window_id].last_pressure_update < 10) {
					windows[window_id].last_pressure_update++;
				} else {
					windows[window_id].last_tilt = Vector2();
					windows[window_id].last_pressure = (wParam & MK_LBUTTON) ? 1.0f : 0.0f;
					windows[window_id].last_pen_inverted = false;
				}
			} else {
				windows[window_id].last_tilt = Vector2();
				windows[window_id].last_pressure = (wParam & MK_LBUTTON) ? 1.0f : 0.0f;
				windows[window_id].last_pen_inverted = false;
			}

			mm->set_pressure(windows[window_id].last_pressure);
			mm->set_tilt(windows[window_id].last_tilt);
			mm->set_pen_inverted(windows[window_id].last_pen_inverted);

			mm->set_button_mask(mouse_get_button_state());

			mm->set_position(Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
			mm->set_global_position(Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));

			if (mouse_mode == MOUSE_MODE_CAPTURED) {
				Point2i c(windows[window_id].width / 2, windows[window_id].height / 2);
				old_x = c.x;
				old_y = c.y;

				if (mm->get_position() == c) {
					center = c;
					return 0;
				}

				Point2i ncenter = mm->get_position();
				center = ncenter;
				POINT pos = { (int)c.x, (int)c.y };
				ClientToScreen(windows[window_id].hWnd, &pos);
				SetCursorPos(pos.x, pos.y);
			}

			mm->set_velocity(Input::get_singleton()->get_last_mouse_velocity());
			mm->set_screen_velocity(mm->get_velocity());

			if (old_invalid) {
				old_x = mm->get_position().x;
				old_y = mm->get_position().y;
				old_invalid = false;
			}

			mm->set_relative(Vector2(mm->get_position() - Vector2(old_x, old_y)));
			mm->set_relative_screen_position(mm->get_relative());
			old_x = mm->get_position().x;
			old_y = mm->get_position().y;

			if (receiving_window_id != window_id) {
				// Adjust event position relative to window distance when event is sent to a different window.
				mm->set_position(mm->get_position() - window_get_position(receiving_window_id) + window_get_position(window_id));
				mm->set_global_position(mm->get_position());
			}

			Input::get_singleton()->parse_input_event(mm);

		} break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			if (Input::get_singleton()->is_emulating_mouse_from_touch()) {
				// Universal translation enabled; ignore OS translations for left button.
				LPARAM extra = GetMessageExtraInfo();
				if (IsTouchEvent(extra)) {
					break;
				}
			}
			[[fallthrough]];
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP: {
			Ref<InputEventMouseButton> mb;
			mb.instantiate();
			mb->set_window_id(window_id);

			switch (uMsg) {
				case WM_LBUTTONDOWN: {
					mb->set_pressed(true);
					mb->set_button_index(MouseButton::LEFT);
				} break;
				case WM_LBUTTONUP: {
					mb->set_pressed(false);
					mb->set_button_index(MouseButton::LEFT);
				} break;
				case WM_MBUTTONDOWN: {
					mb->set_pressed(true);
					mb->set_button_index(MouseButton::MIDDLE);
				} break;
				case WM_MBUTTONUP: {
					mb->set_pressed(false);
					mb->set_button_index(MouseButton::MIDDLE);
				} break;
				case WM_RBUTTONDOWN: {
					mb->set_pressed(true);
					mb->set_button_index(MouseButton::RIGHT);
				} break;
				case WM_RBUTTONUP: {
					mb->set_pressed(false);
					mb->set_button_index(MouseButton::RIGHT);
				} break;
				case WM_LBUTTONDBLCLK: {
					mb->set_pressed(true);
					mb->set_button_index(MouseButton::LEFT);
					mb->set_double_click(true);
				} break;
				case WM_RBUTTONDBLCLK: {
					mb->set_pressed(true);
					mb->set_button_index(MouseButton::RIGHT);
					mb->set_double_click(true);
				} break;
				case WM_MBUTTONDBLCLK: {
					mb->set_pressed(true);
					mb->set_button_index(MouseButton::MIDDLE);
					mb->set_double_click(true);
				} break;
				case WM_MOUSEWHEEL: {
					mb->set_pressed(true);
					int motion = (short)HIWORD(wParam);
					if (!motion) {
						return 0;
					}

					if (motion > 0) {
						mb->set_button_index(MouseButton::WHEEL_UP);
					} else {
						mb->set_button_index(MouseButton::WHEEL_DOWN);
					}
					mb->set_factor(fabs((double)motion / (double)WHEEL_DELTA));
				} break;
				case WM_MOUSEHWHEEL: {
					mb->set_pressed(true);
					int motion = (short)HIWORD(wParam);
					if (!motion) {
						return 0;
					}

					if (motion < 0) {
						mb->set_button_index(MouseButton::WHEEL_LEFT);
					} else {
						mb->set_button_index(MouseButton::WHEEL_RIGHT);
					}
					mb->set_factor(fabs((double)motion / (double)WHEEL_DELTA));
				} break;
				case WM_XBUTTONDOWN: {
					mb->set_pressed(true);
					if (HIWORD(wParam) == XBUTTON1) {
						mb->set_button_index(MouseButton::MB_XBUTTON1);
					} else {
						mb->set_button_index(MouseButton::MB_XBUTTON2);
					}
				} break;
				case WM_XBUTTONUP: {
					mb->set_pressed(false);
					if (HIWORD(wParam) == XBUTTON1) {
						mb->set_button_index(MouseButton::MB_XBUTTON1);
					} else {
						mb->set_button_index(MouseButton::MB_XBUTTON2);
					}
				} break;
				case WM_XBUTTONDBLCLK: {
					mb->set_pressed(true);
					if (HIWORD(wParam) == XBUTTON1) {
						mb->set_button_index(MouseButton::MB_XBUTTON1);
					} else {
						mb->set_button_index(MouseButton::MB_XBUTTON2);
					}
					mb->set_double_click(true);
				} break;
				default: {
					return 0;
				}
			}

			const BitField<WinKeyModifierMask> &mods = _get_mods();
			mb->set_ctrl_pressed(mods.has_flag(WinKeyModifierMask::CTRL));
			mb->set_shift_pressed(mods.has_flag(WinKeyModifierMask::SHIFT));
			mb->set_alt_pressed(mods.has_flag(WinKeyModifierMask::ALT));
			mb->set_meta_pressed(mods.has_flag(WinKeyModifierMask::META));

			if (mb->is_pressed() && mb->get_button_index() >= MouseButton::WHEEL_UP && mb->get_button_index() <= MouseButton::WHEEL_RIGHT) {
				MouseButtonMask mask = mouse_button_to_mask(mb->get_button_index());
				BitField<MouseButtonMask> scroll_mask = mouse_get_button_state();
				scroll_mask.set_flag(mask);
				mb->set_button_mask(scroll_mask);
			} else {
				mb->set_button_mask(mouse_get_button_state());
			}

			mb->set_position(Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));

			if (mouse_mode == MOUSE_MODE_CAPTURED && !use_raw_input) {
				mb->set_position(Vector2(old_x, old_y));
			}

			if (uMsg != WM_MOUSEWHEEL && uMsg != WM_MOUSEHWHEEL) {
				if (mb->is_pressed()) {
					if (++pressrc > 0 && mouse_mode != MOUSE_MODE_CAPTURED) {
						SetCapture(hWnd);
					}
				} else {
					if (--pressrc <= 0 || mouse_get_button_state().is_empty()) {
						if (mouse_mode != MOUSE_MODE_CAPTURED) {
							ReleaseCapture();
						}
						pressrc = 0;
					}
				}
			} else {
				// For reasons unknown to humanity, wheel comes in screen coordinates.
				POINT coords;
				coords.x = mb->get_position().x;
				coords.y = mb->get_position().y;

				ScreenToClient(hWnd, &coords);

				mb->set_position(Vector2(coords.x, coords.y));
			}

			mb->set_global_position(mb->get_position());

			Input::get_singleton()->parse_input_event(mb);
			if (mb->is_pressed() && mb->get_button_index() >= MouseButton::WHEEL_UP && mb->get_button_index() <= MouseButton::WHEEL_RIGHT) {
				// Send release for mouse wheel.
				Ref<InputEventMouseButton> mbd = mb->duplicate();
				mbd->set_window_id(window_id);
				mbd->set_button_mask(mouse_get_button_state());
				mbd->set_pressed(false);
				Input::get_singleton()->parse_input_event(mbd);
			}

			// Propagate the button up event to the window on which the button down
			// event was triggered. This is needed for drag & drop to work between windows,
			// because the engine expects events to keep being processed
			// on the same window dragging started.
			if (mb->is_pressed()) {
				last_mouse_button_down_window = window_id;
			} else if (last_mouse_button_down_window != INVALID_WINDOW_ID) {
				mb->set_window_id(last_mouse_button_down_window);
				last_mouse_button_down_window = INVALID_WINDOW_ID;
			}
		} break;

		case WM_WINDOWPOSCHANGED: {
			Rect2i window_client_rect;
			Rect2i window_rect;
			{
				RECT rect;
				GetClientRect(hWnd, &rect);
				ClientToScreen(hWnd, (POINT *)&rect.left);
				ClientToScreen(hWnd, (POINT *)&rect.right);
				window_client_rect = Rect2i(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
				window_client_rect.position -= _get_screens_origin();

				RECT wrect;
				GetWindowRect(hWnd, &wrect);
				window_rect = Rect2i(wrect.left, wrect.top, wrect.right - wrect.left, wrect.bottom - wrect.top);
				window_rect.position -= _get_screens_origin();
			}

			WINDOWPOS *window_pos_params = (WINDOWPOS *)lParam;
			WindowData &window = windows[window_id];

			bool rect_changed = false;
			if (!(window_pos_params->flags & SWP_NOSIZE) || window_pos_params->flags & SWP_FRAMECHANGED) {
				int screen_id = window_get_current_screen(window_id);
				Size2i screen_size = screen_get_size(screen_id);
				Point2i screen_position = screen_get_position(screen_id);

				window.maximized = false;
				window.minimized = false;
				window.fullscreen = false;

				if (IsIconic(hWnd)) {
					window.minimized = true;
				} else if (IsZoomed(hWnd)) {
					window.maximized = true;

					// If maximized_window_size == screen_size add 1px border to prevent switching to exclusive_fs.
					if (!window.maximized_fs && window.borderless && window_rect.position == screen_position && window_rect.size == screen_size) {
						// Window (borderless) was just maximized and the covers the entire screen.
						window.maximized_fs = true;
						_update_window_style(window_id, false);
					}
				} else if (window_rect.position == screen_position && window_rect.size == screen_size) {
					window.fullscreen = true;
				}

				if (window.maximized_fs && !window.maximized) {
					// Window (maximized and covering fullscreen) was just non-maximized.
					window.maximized_fs = false;
					_update_window_style(window_id, false);
				}

				if (!window.minimized) {
					window.width = window_client_rect.size.width;
					window.height = window_client_rect.size.height;

					rect_changed = true;
				}
#if defined(RD_ENABLED)
				if (window.create_completed && rendering_context && window.context_created) {
					// Note: Trigger resize event to update swapchains when window is minimized/restored, even if size is not changed.
					rendering_context->window_set_size(window_id, window.width, window.height);
				}
#endif
#if defined(GLES3_ENABLED)
				if (window.create_completed && gl_manager_native) {
					gl_manager_native->window_resize(window_id, window.width, window.height);
				}
				if (window.create_completed && gl_manager_angle) {
					gl_manager_angle->window_resize(window_id, window.width, window.height);
				}
#endif
			}

			if (!window.minimized && (!(window_pos_params->flags & SWP_NOMOVE) || window_pos_params->flags & SWP_FRAMECHANGED)) {
				window.last_pos = window_client_rect.position;
				rect_changed = true;
			}

			if (rect_changed) {
				if (window.rect_changed_callback.is_valid()) {
					window.rect_changed_callback.call(Rect2i(window.last_pos.x, window.last_pos.y, window.width, window.height));
				}

				// Update cursor clip region after window rect has changed.
				if (mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN) {
					RECT crect;
					GetClientRect(window.hWnd, &crect);
					ClientToScreen(window.hWnd, (POINT *)&crect.left);
					ClientToScreen(window.hWnd, (POINT *)&crect.right);
					ClipCursor(&crect);
				}

				if (!window.minimized && window.was_fullscreen_pre_min) {
					// Restore fullscreen mode if window was in fullscreen before it was minimized.
					int cs = window_get_current_screen(window_id);
					Point2 pos = screen_get_position(cs) + _get_screens_origin();
					Size2 size = screen_get_size(cs);

					window.was_fullscreen_pre_min = false;
					window.fullscreen = true;
					window.maximized = false;
					window.minimized = false;

					_update_window_style(window_id, false);

					MoveWindow(window.hWnd, pos.x, pos.y, size.width, size.height, TRUE);
				}
			} else {
				if (window.parent_hwnd) {
					// WM_WINDOWPOSCHANGED is sent when the parent changes.
					// If we are supposed to have a parent and now we don't, it's likely
					// because the parent was closed. We will close our window as well.
					// This prevents an embedded game from staying alive when the editor is closed or crashes.
					if (!GetParent(window.hWnd)) {
						SendMessage(window.hWnd, WM_CLOSE, 0, 0);
					}
				}
			}

			// Return here to prevent WM_MOVE and WM_SIZE from being sent
			// See: https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-windowposchanged#remarks
			return 0;
		} break;

		case WM_ENTERSIZEMOVE: {
			Input::get_singleton()->release_pressed_events();
			windows[window_id].move_timer_id = SetTimer(windows[window_id].hWnd, DisplayServerWindows::TIMER_ID_MOVE_REDRAW, USER_TIMER_MINIMUM, (TIMERPROC) nullptr);
		} break;
		case WM_EXITSIZEMOVE: {
			KillTimer(windows[window_id].hWnd, windows[window_id].move_timer_id);
			windows[window_id].move_timer_id = 0;
		} break;
		case WM_TIMER: {
			if (wParam == windows[window_id].move_timer_id) {
				_THREAD_SAFE_UNLOCK_
				_process_key_events();
				if (!Main::is_iterating()) {
					Main::iteration();
				}
				_THREAD_SAFE_LOCK_
			} else if (wParam == windows[window_id].activate_timer_id) {
				_process_activate_event(window_id);
				KillTimer(windows[window_id].hWnd, windows[window_id].activate_timer_id);
				windows[window_id].activate_timer_id = 0;
				windows[window_id].first_activation_done = true;
			}
		} break;
		case WM_SYSKEYUP:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN: {
			if (windows[window_id].ime_suppress_next_keyup && (uMsg == WM_KEYUP || uMsg == WM_SYSKEYUP)) {
				windows[window_id].ime_suppress_next_keyup = false;
				break;
			}
			if (windows[window_id].ime_in_progress) {
				break;
			}

			if (mouse_mode == MOUSE_MODE_CAPTURED) {
				// When SetCapture is used, ALT+F4 hotkey is ignored by Windows, so handle it ourselves
				if (wParam == VK_F4 && _get_mods().has_flag(WinKeyModifierMask::ALT) && (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)) {
					_send_window_event(windows[window_id], WINDOW_EVENT_CLOSE_REQUEST);
				}
			}
			[[fallthrough]];
		}
		case WM_CHAR: {
			ERR_BREAK(key_event_pos >= KEY_EVENT_BUFFER_SIZE);
			const BitField<WinKeyModifierMask> &mods = _get_mods();

			KeyEvent ke;
			ke.shift = mods.has_flag(WinKeyModifierMask::SHIFT);
			ke.alt = mods.has_flag(WinKeyModifierMask::ALT);
			ke.altgr = mods.has_flag(WinKeyModifierMask::ALT_GR);
			ke.control = mods.has_flag(WinKeyModifierMask::CTRL);
			ke.meta = mods.has_flag(WinKeyModifierMask::META);
			ke.uMsg = uMsg;
			ke.window_id = window_id;

			if (ke.uMsg == WM_SYSKEYDOWN) {
				ke.uMsg = WM_KEYDOWN;
			}
			if (ke.uMsg == WM_SYSKEYUP) {
				ke.uMsg = WM_KEYUP;
			}

			ke.wParam = wParam;
			ke.lParam = lParam;
			key_event_buffer[key_event_pos++] = ke;

		} break;
		case WM_IME_COMPOSITION: {
			CANDIDATEFORM cf;
			cf.dwIndex = 0;

			cf.dwStyle = CFS_CANDIDATEPOS;
			cf.ptCurrentPos.x = windows[window_id].im_position.x;
			cf.ptCurrentPos.y = windows[window_id].im_position.y;
			ImmSetCandidateWindow(windows[window_id].im_himc, &cf);

			cf.dwStyle = CFS_EXCLUDE;
			cf.rcArea.left = windows[window_id].im_position.x;
			cf.rcArea.right = windows[window_id].im_position.x;
			cf.rcArea.top = windows[window_id].im_position.y;
			cf.rcArea.bottom = windows[window_id].im_position.y;
			ImmSetCandidateWindow(windows[window_id].im_himc, &cf);

			if (windows[window_id].ime_active) {
				SetCaretPos(windows[window_id].im_position.x, windows[window_id].im_position.y);
				OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_IME_UPDATE);
			}
		} break;
		case WM_INPUTLANGCHANGEREQUEST: {
			// FIXME: Do something?
		} break;
		case WM_IME_STARTCOMPOSITION: {
			if (windows[window_id].ime_active) {
				windows[window_id].ime_in_progress = true;
				if (key_event_pos > 0) {
					key_event_pos--;
				}
			}
			return 0;
		} break;
		case WM_IME_ENDCOMPOSITION: {
			if (windows[window_id].ime_active) {
				windows[window_id].ime_in_progress = false;
				windows[window_id].ime_suppress_next_keyup = true;
			}
			return 0;
		} break;
		case WM_IME_NOTIFY: {
			return 0;
		} break;
		case WM_TOUCH: {
			BOOL bHandled = FALSE;
			UINT cInputs = LOWORD(wParam);
			PTOUCHINPUT pInputs = memnew_arr(TOUCHINPUT, cInputs);
			if (pInputs) {
				if (GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs, sizeof(TOUCHINPUT))) {
					for (UINT i = 0; i < cInputs; i++) {
						TOUCHINPUT ti = pInputs[i];
						POINT touch_pos = {
							TOUCH_COORD_TO_PIXEL(ti.x),
							TOUCH_COORD_TO_PIXEL(ti.y),
						};
						ScreenToClient(hWnd, &touch_pos);
						// Do something with each touch input entry.
						if (ti.dwFlags & TOUCHEVENTF_MOVE) {
							_drag_event(window_id, touch_pos.x, touch_pos.y, ti.dwID);
						} else if (ti.dwFlags & (TOUCHEVENTF_UP | TOUCHEVENTF_DOWN)) {
							_touch_event(window_id, ti.dwFlags & TOUCHEVENTF_DOWN, touch_pos.x, touch_pos.y, ti.dwID);
						}
					}
					bHandled = TRUE;
				} else {
					// TODO: Handle the error here.
				}
				memdelete_arr(pInputs);
			} else {
				// TODO: Handle the error here, probably out of memory.
			}
			if (bHandled) {
				CloseTouchInputHandle((HTOUCHINPUT)lParam);
				return 0;
			}

		} break;
		case WM_DEVICECHANGE: {
			joypad->probe_joypads();
		} break;
		case WM_DESTROY: {
			Input::get_singleton()->flush_buffered_events();
			if (window_mouseover_id == window_id) {
				window_mouseover_id = INVALID_WINDOW_ID;
				_send_window_event(windows[window_id], WINDOW_EVENT_MOUSE_EXIT);
			}
		} break;
		case WM_SETCURSOR: {
			if (LOWORD(lParam) == HTCLIENT) {
				if (windows[window_id].window_focused && (mouse_mode == MOUSE_MODE_HIDDEN || mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN)) {
					// Hide the cursor.
					if (hCursor == nullptr) {
						hCursor = SetCursor(nullptr);
					} else {
						SetCursor(nullptr);
					}
				} else {
					if (hCursor != nullptr) {
						CursorShape c = cursor_shape;
						cursor_shape = CURSOR_MAX;
						cursor_set_shape(c);
						hCursor = nullptr;
					}
				}
			}
		} break;
		default: {
			if (user_proc) {
				return CallWindowProcW(user_proc, hWnd, uMsg, wParam, lParam);
			}
		}
	}

	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	DisplayServerWindows *ds_win = static_cast<DisplayServerWindows *>(DisplayServer::get_singleton());
	if (ds_win) {
		return ds_win->WndProc(hWnd, uMsg, wParam, lParam);
	} else {
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
	}
}

void DisplayServerWindows::_process_activate_event(WindowID p_window_id) {
	WindowData &wd = windows[p_window_id];
	if (wd.activate_state == WA_ACTIVE || wd.activate_state == WA_CLICKACTIVE) {
		last_focused_window = p_window_id;
		_set_mouse_mode_impl(mouse_mode);
		if (!IsIconic(wd.hWnd)) {
			SetFocus(wd.hWnd);
		}
		wd.window_focused = true;
		_send_window_event(wd, WINDOW_EVENT_FOCUS_IN);
	} else { // WM_INACTIVE.
		Input::get_singleton()->release_pressed_events();
		track_mouse_leave_event(wd.hWnd);
		// Release capture unconditionally because it can be set due to dragging, in addition to captured mode.
		ReleaseCapture();
		wd.window_focused = false;
		_send_window_event(wd, WINDOW_EVENT_FOCUS_OUT);
	}

	if ((tablet_get_current_driver() == "wintab") && wintab_available && wd.wtctx) {
		wintab_WTEnable(wd.wtctx, wd.activate_state);
	}
}

void DisplayServerWindows::_process_key_events() {
	for (int i = 0; i < key_event_pos; i++) {
		KeyEvent &ke = key_event_buffer[i];
		switch (ke.uMsg) {
			case WM_CHAR: {
				// Extended keys should only be processed as WM_KEYDOWN message.
				if (!KeyMappingWindows::is_extended_key(ke.wParam) && ((i == 0 && ke.uMsg == WM_CHAR) || (i > 0 && key_event_buffer[i - 1].uMsg == WM_CHAR))) {
					static char32_t prev_wc = 0;
					char32_t unicode = ke.wParam;
					if ((unicode & 0xfffffc00) == 0xd800) {
						if (prev_wc != 0) {
							ERR_PRINT("invalid utf16 surrogate input");
						}
						prev_wc = unicode;
						break; // Skip surrogate.
					} else if ((unicode & 0xfffffc00) == 0xdc00) {
						if (prev_wc == 0) {
							ERR_PRINT("invalid utf16 surrogate input");
							break; // Skip invalid surrogate.
						}
						unicode = (prev_wc << 10UL) + unicode - ((0xd800 << 10UL) + 0xdc00 - 0x10000);
						prev_wc = 0;
					} else {
						prev_wc = 0;
					}
					Ref<InputEventKey> k;
					k.instantiate();

					Key keycode = KeyMappingWindows::get_keysym(MapVirtualKey((ke.lParam >> 16) & 0xFF, MAPVK_VSC_TO_VK));
					Key key_label = keycode;
					Key physical_keycode = KeyMappingWindows::get_scansym((ke.lParam >> 16) & 0xFF, ke.lParam & (1 << 24));

					static BYTE keyboard_state[256];
					memset(keyboard_state, 0, 256);
					wchar_t chars[256] = {};
					UINT extended_code = MapVirtualKey((ke.lParam >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX);
					if (!(ke.lParam & (1 << 24)) && ToUnicodeEx(extended_code, (ke.lParam >> 16) & 0xFF, keyboard_state, chars, 255, 4, GetKeyboardLayout(0)) > 0) {
						String keysym = String::utf16((char16_t *)chars, 255);
						if (!keysym.is_empty()) {
							char32_t unicode_value = keysym[0];
							// For printable ASCII characters (0x20-0x7E), override the original keycode with the character value.
							if (Key::SPACE <= (Key)unicode_value && (Key)unicode_value <= Key::ASCIITILDE) {
								keycode = fix_keycode(unicode_value, (Key)unicode_value);
							}
							key_label = fix_key_label(unicode_value, keycode);
						}
					}

					k->set_window_id(ke.window_id);
					if (keycode != Key::SHIFT) {
						k->set_shift_pressed(ke.shift);
					}
					if (keycode != Key::ALT) {
						k->set_alt_pressed(ke.alt);
					}
					if (keycode != Key::CTRL) {
						k->set_ctrl_pressed(ke.control);
					}
					if (keycode != Key::META) {
						k->set_meta_pressed(ke.meta);
					}
					k->set_pressed(true);
					k->set_keycode(keycode);
					k->set_physical_keycode(physical_keycode);
					k->set_key_label(key_label);
					k->set_unicode(fix_unicode(unicode));
					if (k->get_unicode() && ke.altgr && windows[ke.window_id].ime_active) {
						k->set_alt_pressed(false);
						k->set_ctrl_pressed(false);
					}

					Input::get_singleton()->parse_input_event(k);
				} else {
					// Do nothing.
				}
			} break;
			case WM_KEYUP:
			case WM_KEYDOWN: {
				Ref<InputEventKey> k;
				k.instantiate();

				k->set_window_id(ke.window_id);
				k->set_pressed(ke.uMsg == WM_KEYDOWN);

				Key keycode = KeyMappingWindows::get_keysym(ke.wParam);
				if ((ke.lParam & (1 << 24)) && (ke.wParam == VK_RETURN)) {
					// Special case for Numpad Enter key.
					keycode = Key::KP_ENTER;
				}
				Key key_label = keycode;
				Key physical_keycode = KeyMappingWindows::get_scansym((ke.lParam >> 16) & 0xFF, ke.lParam & (1 << 24));
				KeyLocation location = KeyMappingWindows::get_location((ke.lParam >> 16) & 0xFF, ke.lParam & (1 << 24));

				static BYTE keyboard_state[256];
				memset(keyboard_state, 0, 256);
				wchar_t chars[256] = {};
				UINT extended_code = MapVirtualKey((ke.lParam >> 16) & 0xFF, MAPVK_VSC_TO_VK_EX);
				if (!(ke.lParam & (1 << 24)) && ToUnicodeEx(extended_code, (ke.lParam >> 16) & 0xFF, keyboard_state, chars, 255, 4, GetKeyboardLayout(0)) > 0) {
					String keysym = String::utf16((char16_t *)chars, 255);
					if (!keysym.is_empty()) {
						char32_t unicode_value = keysym[0];
						// For printable ASCII characters (0x20-0x7E), override the original keycode with the character value.
						if (Key::SPACE <= (Key)unicode_value && (Key)unicode_value <= Key::ASCIITILDE) {
							keycode = fix_keycode(unicode_value, (Key)unicode_value);
						}
						key_label = fix_key_label(unicode_value, keycode);
					}
				}

				if (keycode != Key::SHIFT) {
					k->set_shift_pressed(ke.shift);
				}
				if (keycode != Key::ALT) {
					k->set_alt_pressed(ke.alt);
				}
				if (keycode != Key::CTRL) {
					k->set_ctrl_pressed(ke.control);
				}
				if (keycode != Key::META) {
					k->set_meta_pressed(ke.meta);
				}
				k->set_keycode(keycode);
				k->set_physical_keycode(physical_keycode);
				k->set_location(location);
				k->set_key_label(key_label);

				if (i + 1 < key_event_pos && key_event_buffer[i + 1].uMsg == WM_CHAR) {
					char32_t unicode = key_event_buffer[i + 1].wParam;
					static char32_t prev_wck = 0;
					if ((unicode & 0xfffffc00) == 0xd800) {
						if (prev_wck != 0) {
							ERR_PRINT("invalid utf16 surrogate input");
						}
						prev_wck = unicode;
						break; // Skip surrogate.
					} else if ((unicode & 0xfffffc00) == 0xdc00) {
						if (prev_wck == 0) {
							ERR_PRINT("invalid utf16 surrogate input");
							break; // Skip invalid surrogate.
						}
						unicode = (prev_wck << 10UL) + unicode - ((0xd800 << 10UL) + 0xdc00 - 0x10000);
						prev_wck = 0;
					} else {
						prev_wck = 0;
					}
					k->set_unicode(fix_unicode(unicode));
				}
				if (k->get_unicode() && ke.altgr && windows[ke.window_id].ime_active) {
					k->set_alt_pressed(false);
					k->set_ctrl_pressed(false);
				}

				k->set_echo((ke.uMsg == WM_KEYDOWN && (ke.lParam & (1 << 30))));

				Input::get_singleton()->parse_input_event(k);

			} break;
		}
	}

	key_event_pos = 0;
}

void DisplayServerWindows::_update_tablet_ctx(const String &p_old_driver, const String &p_new_driver) {
	for (KeyValue<WindowID, WindowData> &E : windows) {
		WindowData &wd = E.value;
		wd.block_mm = false;
		if ((p_old_driver == "wintab") && wintab_available && wd.wtctx) {
			wintab_WTEnable(wd.wtctx, false);
			wintab_WTClose(wd.wtctx);
			wd.wtctx = nullptr;
		}
		if ((p_new_driver == "wintab") && wintab_available) {
			wintab_WTInfo(WTI_DEFSYSCTX, 0, &wd.wtlc);
			wd.wtlc.lcOptions |= CXO_MESSAGES;
			wd.wtlc.lcPktData = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_ORIENTATION;
			wd.wtlc.lcMoveMask = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE;
			wd.wtlc.lcPktMode = 0;
			wd.wtlc.lcOutOrgX = 0;
			wd.wtlc.lcOutExtX = wd.wtlc.lcInExtX;
			wd.wtlc.lcOutOrgY = 0;
			wd.wtlc.lcOutExtY = -wd.wtlc.lcInExtY;
			wd.wtctx = wintab_WTOpen(wd.hWnd, &wd.wtlc, false);
			if (wd.wtctx) {
				wintab_WTEnable(wd.wtctx, true);
				AXIS pressure;
				if (wintab_WTInfo(WTI_DEVICES + wd.wtlc.lcDevice, DVC_NPRESSURE, &pressure)) {
					wd.min_pressure = int(pressure.axMin);
					wd.max_pressure = int(pressure.axMax);
				}
				AXIS orientation[3];
				if (wintab_WTInfo(WTI_DEVICES + wd.wtlc.lcDevice, DVC_ORIENTATION, &orientation)) {
					wd.tilt_supported = orientation[0].axResolution && orientation[1].axResolution;
				}
				wintab_WTEnable(wd.wtctx, true);
			} else {
				print_verbose("WinTab context creation failed.");
			}
		}
	}
}

DisplayServer::WindowID DisplayServerWindows::_create_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect, bool p_exclusive, WindowID p_transient_parent, HWND p_parent_hwnd) {
	DWORD dwExStyle;
	DWORD dwStyle;

	_get_window_style(window_id_counter == MAIN_WINDOW_ID, false, (p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN), p_mode != WINDOW_MODE_EXCLUSIVE_FULLSCREEN, p_flags & WINDOW_FLAG_BORDERLESS_BIT, !(p_flags & WINDOW_FLAG_RESIZE_DISABLED_BIT), p_mode == WINDOW_MODE_MINIMIZED, p_mode == WINDOW_MODE_MAXIMIZED, false, (p_flags & WINDOW_FLAG_NO_FOCUS_BIT) | (p_flags & WINDOW_FLAG_POPUP), p_parent_hwnd, dwStyle, dwExStyle);

	RECT WindowRect;

	WindowRect.left = p_rect.position.x;
	WindowRect.right = p_rect.position.x + p_rect.size.x;
	WindowRect.top = p_rect.position.y;
	WindowRect.bottom = p_rect.position.y + p_rect.size.y;

	int rq_screen = get_screen_from_rect(p_rect);
	if (rq_screen < 0) {
		rq_screen = get_primary_screen(); // Requested window rect is outside any screen bounds.
	}

	Point2i offset = _get_screens_origin();

	if (!p_parent_hwnd) {
		if (p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
			Rect2i screen_rect = Rect2i(screen_get_position(rq_screen), screen_get_size(rq_screen));

			WindowRect.left = screen_rect.position.x;
			WindowRect.right = screen_rect.position.x + screen_rect.size.x;
			WindowRect.top = screen_rect.position.y;
			WindowRect.bottom = screen_rect.position.y + screen_rect.size.y;
		} else {
			Rect2i srect = screen_get_usable_rect(rq_screen);
			Point2i wpos = p_rect.position;
			if (srect != Rect2i()) {
				wpos = wpos.clamp(srect.position, srect.position + srect.size - p_rect.size / 3);
			}

			WindowRect.left = wpos.x;
			WindowRect.right = wpos.x + p_rect.size.x;
			WindowRect.top = wpos.y;
			WindowRect.bottom = wpos.y + p_rect.size.y;
		}

		WindowRect.left += offset.x;
		WindowRect.right += offset.x;
		WindowRect.top += offset.y;
		WindowRect.bottom += offset.y;

		if (p_mode != WINDOW_MODE_FULLSCREEN && p_mode != WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
			AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);
		}
	}

	WindowID id = window_id_counter;
	{
		WindowData *wd_transient_parent = nullptr;
		HWND owner_hwnd = nullptr;
		if (p_parent_hwnd) {
			owner_hwnd = p_parent_hwnd;
		} else if (p_transient_parent != INVALID_WINDOW_ID) {
			if (!windows.has(p_transient_parent)) {
				ERR_PRINT("Condition \"!windows.has(p_transient_parent)\" is true.");
				p_transient_parent = INVALID_WINDOW_ID;
			} else {
				wd_transient_parent = &windows[p_transient_parent];
				if (p_exclusive) {
					owner_hwnd = wd_transient_parent->hWnd;
				}
			}
		}

		WindowData &wd = windows[id];

		wd.hWnd = CreateWindowExW(
				dwExStyle,
				L"Engine", L"",
				dwStyle,
				WindowRect.left,
				WindowRect.top,
				WindowRect.right - WindowRect.left,
				WindowRect.bottom - WindowRect.top,
				owner_hwnd,
				nullptr,
				hInstance,
				// tunnel the WindowData we need to handle creation message
				// lifetime is ensured because we are still on the stack when this is
				// processed in the window proc
				reinterpret_cast<void *>(&wd));
		if (!wd.hWnd) {
			MessageBoxW(nullptr, L"Window Creation Error.", L"ERROR", MB_OK | MB_ICONEXCLAMATION);
			windows.erase(id);
			ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Failed to create Windows OS window.");
		}

		wd.parent_hwnd = p_parent_hwnd;

		if (p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
			wd.fullscreen = true;
			if (p_mode == WINDOW_MODE_FULLSCREEN) {
				wd.multiwindow_fs = true;
			}
		}

		if (p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
			// Save initial non-fullscreen rect.
			Rect2i srect = screen_get_usable_rect(rq_screen);
			Point2i wpos = p_rect.position;
			if (srect != Rect2i()) {
				wpos = wpos.clamp(srect.position, srect.position + srect.size - p_rect.size / 3);
			}

			wd.pre_fs_rect.left = wpos.x + offset.x;
			wd.pre_fs_rect.right = wpos.x + p_rect.size.x + offset.x;
			wd.pre_fs_rect.top = wpos.y + offset.y;
			wd.pre_fs_rect.bottom = wpos.y + p_rect.size.y + offset.y;
			wd.pre_fs_valid = true;
		}

		wd.exclusive = p_exclusive;
		if (wd_transient_parent) {
			wd.transient_parent = p_transient_parent;
			wd_transient_parent->transient_children.insert(id);
		}

		wd.sharp_corners = p_flags & WINDOW_FLAG_SHARP_CORNERS_BIT;
		{
			DWORD value = wd.sharp_corners ? DWMWCP_DONOTROUND : DWMWCP_DEFAULT;
			::DwmSetWindowAttribute(wd.hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &value, sizeof(value));
		}

		if (is_dark_mode_supported() && dark_title_available) {
			BOOL value = is_dark_mode();
			::DwmSetWindowAttribute(wd.hWnd, use_legacy_dark_mode_before_20H1 ? DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 : DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
		}

		RECT real_client_rect;
		GetClientRect(wd.hWnd, &real_client_rect);

#ifdef RD_ENABLED
		if (rendering_context) {
			union {
#ifdef VULKAN_ENABLED
				RenderingContextDriverVulkanWindows::WindowPlatformData vulkan;
#endif
#ifdef D3D12_ENABLED
				RenderingContextDriverD3D12::WindowPlatformData d3d12;
#endif
			} wpd;
#ifdef VULKAN_ENABLED
			if (rendering_driver == "vulkan") {
				wpd.vulkan.window = wd.hWnd;
				wpd.vulkan.instance = hInstance;
			}
#endif
#ifdef D3D12_ENABLED
			if (rendering_driver == "d3d12") {
				wpd.d3d12.window = wd.hWnd;
			}
#endif
			if (rendering_context->window_create(id, &wpd) != OK) {
				ERR_PRINT(vformat("Failed to create %s window.", rendering_driver));
				memdelete(rendering_context);
				rendering_context = nullptr;
				windows.erase(id);
				return INVALID_WINDOW_ID;
			}

			rendering_context->window_set_size(id, real_client_rect.right - real_client_rect.left, real_client_rect.bottom - real_client_rect.top);
			rendering_context->window_set_vsync_mode(id, p_vsync_mode);
			wd.context_created = true;
		}
#endif

#ifdef GLES3_ENABLED
		if (gl_manager_native) {
			if (gl_manager_native->window_create(id, wd.hWnd, hInstance, real_client_rect.right - real_client_rect.left, real_client_rect.bottom - real_client_rect.top) != OK) {
				memdelete(gl_manager_native);
				gl_manager_native = nullptr;
				windows.erase(id);
				ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Failed to create an OpenGL window.");
			}
			window_set_vsync_mode(p_vsync_mode, id);
		}

		if (gl_manager_angle) {
			if (gl_manager_angle->window_create(id, nullptr, wd.hWnd, real_client_rect.right - real_client_rect.left, real_client_rect.bottom - real_client_rect.top) != OK) {
				memdelete(gl_manager_angle);
				gl_manager_angle = nullptr;
				windows.erase(id);
				ERR_FAIL_V_MSG(INVALID_WINDOW_ID, "Failed to create an OpenGL window.");
			}
			window_set_vsync_mode(p_vsync_mode, id);
		}
#endif

		RegisterTouchWindow(wd.hWnd, 0);
		DragAcceptFiles(wd.hWnd, true);

		if ((tablet_get_current_driver() == "wintab") && wintab_available) {
			wintab_WTInfo(WTI_DEFSYSCTX, 0, &wd.wtlc);
			wd.wtlc.lcOptions |= CXO_MESSAGES;
			wd.wtlc.lcPktData = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_ORIENTATION;
			wd.wtlc.lcMoveMask = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE;
			wd.wtlc.lcPktMode = 0;
			wd.wtlc.lcOutOrgX = 0;
			wd.wtlc.lcOutExtX = wd.wtlc.lcInExtX;
			wd.wtlc.lcOutOrgY = 0;
			wd.wtlc.lcOutExtY = -wd.wtlc.lcInExtY;
			wd.wtctx = wintab_WTOpen(wd.hWnd, &wd.wtlc, false);
			if (wd.wtctx) {
				wintab_WTEnable(wd.wtctx, true);
				AXIS pressure;
				if (wintab_WTInfo(WTI_DEVICES + wd.wtlc.lcDevice, DVC_NPRESSURE, &pressure)) {
					wd.min_pressure = int(pressure.axMin);
					wd.max_pressure = int(pressure.axMax);
				}
				AXIS orientation[3];
				if (wintab_WTInfo(WTI_DEVICES + wd.wtlc.lcDevice, DVC_ORIENTATION, &orientation)) {
					wd.tilt_supported = orientation[0].axResolution && orientation[1].axResolution;
				}
			} else {
				print_verbose("WinTab context creation failed.");
			}
		} else {
			wd.wtctx = nullptr;
		}

		if (p_mode == WINDOW_MODE_MAXIMIZED) {
			wd.maximized = true;
			wd.minimized = false;
		}

		if (p_mode == WINDOW_MODE_MINIMIZED) {
			wd.maximized = false;
			wd.minimized = true;
		}

		wd.last_pressure = 0;
		wd.last_pressure_update = 0;
		wd.last_tilt = Vector2();

		IPropertyStore *prop_store;
		HRESULT hr = SHGetPropertyStoreForWindow(wd.hWnd, IID_IPropertyStore, (void **)&prop_store);
		if (hr == S_OK) {
			PROPVARIANT val;
			String appname;
			if (Engine::get_singleton()->is_editor_hint()) {
				appname = "Godot.GodotEditor." + String(VERSION_FULL_CONFIG);
			} else {
				String name = GLOBAL_GET("application/config/name");
				String version = GLOBAL_GET("application/config/version");
				if (version.is_empty()) {
					version = "0";
				}
				String clean_app_name = name.to_pascal_case();
				for (int i = 0; i < clean_app_name.length(); i++) {
					if (!is_ascii_alphanumeric_char(clean_app_name[i]) && clean_app_name[i] != '_' && clean_app_name[i] != '.') {
						clean_app_name[i] = '_';
					}
				}
				clean_app_name = clean_app_name.substr(0, 120 - version.length()).trim_suffix(".");
				appname = "Godot." + clean_app_name + "." + version;
			}
			InitPropVariantFromString((PCWSTR)appname.utf16().get_data(), &val);
			prop_store->SetValue(PKEY_AppUserModel_ID, val);
			prop_store->Release();
		}

		// IME.
		wd.im_himc = ImmGetContext(wd.hWnd);
		ImmAssociateContext(wd.hWnd, (HIMC) nullptr);

		wd.im_position = Vector2();

		if (p_mode == WINDOW_MODE_FULLSCREEN || p_mode == WINDOW_MODE_EXCLUSIVE_FULLSCREEN || p_mode == WINDOW_MODE_MAXIMIZED) {
			RECT r;
			GetClientRect(wd.hWnd, &r);
			ClientToScreen(wd.hWnd, (POINT *)&r.left);
			ClientToScreen(wd.hWnd, (POINT *)&r.right);
			wd.last_pos = Point2i(r.left, r.top) - _get_screens_origin();
			wd.width = r.right - r.left;
			wd.height = r.bottom - r.top;
		} else {
			wd.last_pos = p_rect.position;
			wd.width = p_rect.size.width;
			wd.height = p_rect.size.height;
		}

		// Set size of maximized borderless window (by default it covers the entire screen).
		if (p_mode == WINDOW_MODE_MAXIMIZED && (p_flags & WINDOW_FLAG_BORDERLESS_BIT)) {
			Rect2i srect = screen_get_usable_rect(rq_screen);
			SetWindowPos(wd.hWnd, HWND_TOP, srect.position.x, srect.position.y, srect.size.width, srect.size.height, SWP_NOZORDER | SWP_NOACTIVATE);
		}

		wd.create_completed = true;
		window_id_counter++;
	}

	return id;
}

BitField<DisplayServerWindows::DriverID> DisplayServerWindows::tested_drivers = 0;

// WinTab API.
bool DisplayServerWindows::wintab_available = false;
WTOpenPtr DisplayServerWindows::wintab_WTOpen = nullptr;
WTClosePtr DisplayServerWindows::wintab_WTClose = nullptr;
WTInfoPtr DisplayServerWindows::wintab_WTInfo = nullptr;
WTPacketPtr DisplayServerWindows::wintab_WTPacket = nullptr;
WTEnablePtr DisplayServerWindows::wintab_WTEnable = nullptr;

// UXTheme API.
bool DisplayServerWindows::dark_title_available = false;
bool DisplayServerWindows::use_legacy_dark_mode_before_20H1 = false;
bool DisplayServerWindows::ux_theme_available = false;
ShouldAppsUseDarkModePtr DisplayServerWindows::ShouldAppsUseDarkMode = nullptr;
GetImmersiveColorFromColorSetExPtr DisplayServerWindows::GetImmersiveColorFromColorSetEx = nullptr;
GetImmersiveColorTypeFromNamePtr DisplayServerWindows::GetImmersiveColorTypeFromName = nullptr;
GetImmersiveUserColorSetPreferencePtr DisplayServerWindows::GetImmersiveUserColorSetPreference = nullptr;

// Windows Ink API.
bool DisplayServerWindows::winink_available = false;
GetPointerTypePtr DisplayServerWindows::win8p_GetPointerType = nullptr;
GetPointerPenInfoPtr DisplayServerWindows::win8p_GetPointerPenInfo = nullptr;
LogicalToPhysicalPointForPerMonitorDPIPtr DisplayServerWindows::win81p_LogicalToPhysicalPointForPerMonitorDPI = nullptr;
PhysicalToLogicalPointForPerMonitorDPIPtr DisplayServerWindows::win81p_PhysicalToLogicalPointForPerMonitorDPI = nullptr;

// Shell API,
SHLoadIndirectStringPtr DisplayServerWindows::load_indirect_string = nullptr;

Vector2i _get_device_ids(const String &p_device_name) {
	if (p_device_name.is_empty()) {
		return Vector2i();
	}

	REFCLSID clsid = CLSID_WbemLocator; // Unmarshaler CLSID
	REFIID uuid = IID_IWbemLocator; // Interface UUID
	IWbemLocator *wbemLocator = nullptr; // to get the services
	IWbemServices *wbemServices = nullptr; // to get the class
	IEnumWbemClassObject *iter = nullptr;
	IWbemClassObject *pnpSDriverObject[1]; // contains driver name, version, etc.

	HRESULT hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, uuid, (LPVOID *)&wbemLocator);
	if (hr != S_OK) {
		return Vector2i();
	}
	BSTR resource_name = SysAllocString(L"root\\CIMV2");
	hr = wbemLocator->ConnectServer(resource_name, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &wbemServices);
	SysFreeString(resource_name);

	SAFE_RELEASE(wbemLocator) // from now on, use `wbemServices`
	if (hr != S_OK) {
		SAFE_RELEASE(wbemServices)
		return Vector2i();
	}

	Vector2i ids;

	const String gpu_device_class_query = vformat("SELECT * FROM Win32_PnPSignedDriver WHERE DeviceName = \"%s\"", p_device_name);
	BSTR query = SysAllocString((const WCHAR *)gpu_device_class_query.utf16().get_data());
	BSTR query_lang = SysAllocString(L"WQL");
	hr = wbemServices->ExecQuery(query_lang, query, WBEM_FLAG_RETURN_IMMEDIATELY | WBEM_FLAG_FORWARD_ONLY, nullptr, &iter);
	SysFreeString(query_lang);
	SysFreeString(query);
	if (hr == S_OK) {
		ULONG resultCount;
		hr = iter->Next(5000, 1, pnpSDriverObject, &resultCount); // Get exactly 1. Wait max 5 seconds.

		if (hr == S_OK && resultCount > 0) {
			VARIANT did;
			VariantInit(&did);
			BSTR object_name = SysAllocString(L"DeviceID");
			hr = pnpSDriverObject[0]->Get(object_name, 0, &did, nullptr, nullptr);
			SysFreeString(object_name);
			if (hr == S_OK) {
				String device_id = String(V_BSTR(&did));
				ids.x = device_id.get_slice("&", 0).lstrip("PCI\\VEN_").hex_to_int();
				ids.y = device_id.get_slice("&", 1).lstrip("DEV_").hex_to_int();
			}

			for (ULONG i = 0; i < resultCount; i++) {
				SAFE_RELEASE(pnpSDriverObject[i])
			}
		}
	}

	SAFE_RELEASE(wbemServices)
	SAFE_RELEASE(iter)

	return ids;
}

bool DisplayServerWindows::is_dark_mode_supported() const {
	return ux_theme_available;
}

bool DisplayServerWindows::is_dark_mode() const {
	return ux_theme_available && ShouldAppsUseDarkMode();
}

Color DisplayServerWindows::get_accent_color() const {
	if (!ux_theme_available) {
		return Color(0, 0, 0, 0);
	}

	int argb = GetImmersiveColorFromColorSetEx((UINT)GetImmersiveUserColorSetPreference(false, false), GetImmersiveColorTypeFromName(L"ImmersiveSystemAccent"), false, 0);
	return Color((argb & 0xFF) / 255.f, ((argb & 0xFF00) >> 8) / 255.f, ((argb & 0xFF0000) >> 16) / 255.f, ((argb & 0xFF000000) >> 24) / 255.f);
}

Color DisplayServerWindows::get_base_color() const {
	if (!ux_theme_available) {
		return Color(0, 0, 0, 0);
	}

	int argb = GetImmersiveColorFromColorSetEx((UINT)GetImmersiveUserColorSetPreference(false, false), GetImmersiveColorTypeFromName(ShouldAppsUseDarkMode() ? L"ImmersiveDarkChromeMediumLow" : L"ImmersiveLightChromeMediumLow"), false, 0);
	return Color((argb & 0xFF) / 255.f, ((argb & 0xFF00) >> 8) / 255.f, ((argb & 0xFF0000) >> 16) / 255.f, ((argb & 0xFF000000) >> 24) / 255.f);
}

void DisplayServerWindows::set_system_theme_change_callback(const Callable &p_callable) {
	system_theme_changed = p_callable;
}

int DisplayServerWindows::tablet_get_driver_count() const {
	return tablet_drivers.size();
}

String DisplayServerWindows::tablet_get_driver_name(int p_driver) const {
	if (p_driver < 0 || p_driver >= tablet_drivers.size()) {
		return "";
	} else {
		return tablet_drivers[p_driver];
	}
}

String DisplayServerWindows::tablet_get_current_driver() const {
	return tablet_driver;
}

void DisplayServerWindows::tablet_set_current_driver(const String &p_driver) {
	if (tablet_get_driver_count() == 0) {
		return;
	}
	bool found = false;
	for (int i = 0; i < tablet_get_driver_count(); i++) {
		if (p_driver == tablet_get_driver_name(i)) {
			found = true;
		}
	}
	if (found) {
		_update_tablet_ctx(tablet_driver, p_driver);
		tablet_driver = p_driver;
	} else {
		ERR_PRINT("Unknown tablet driver " + p_driver + ".");
	}
}

DisplayServerWindows::DisplayServerWindows(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	KeyMappingWindows::initialize();

	tested_drivers.clear();

	drop_events = false;
	key_event_pos = 0;

	hInstance = static_cast<OS_Windows *>(OS::get_singleton())->get_hinstance();

	pressrc = 0;
	old_invalid = true;
	mouse_mode = MOUSE_MODE_VISIBLE;

	rendering_driver = p_rendering_driver;

	// Init TTS
	bool tts_enabled = GLOBAL_GET("audio/general/text_to_speech");
	if (tts_enabled) {
		tts = memnew(TTS_Windows);
	}
	native_menu = memnew(NativeMenuWindows);

	// Enforce default keep screen on value.
	screen_set_keep_on(GLOBAL_GET("display/window/energy_saving/keep_screen_on"));

	// Load Windows version info.
	ZeroMemory(&os_ver, sizeof(OSVERSIONINFOW));
	os_ver.dwOSVersionInfoSize = sizeof(OSVERSIONINFOW);

	HMODULE nt_lib = LoadLibraryW(L"ntdll.dll");
	if (nt_lib) {
		WineGetVersionPtr wine_get_version = (WineGetVersionPtr)GetProcAddress(nt_lib, "wine_get_version"); // Do not read Windows build number under Wine, it can be set to arbitrary value.
		if (!wine_get_version) {
			RtlGetVersionPtr RtlGetVersion = (RtlGetVersionPtr)GetProcAddress(nt_lib, "RtlGetVersion");
			if (RtlGetVersion) {
				RtlGetVersion(&os_ver);
			}
		}
		FreeLibrary(nt_lib);
	}

	// Load Shell API.
	HMODULE shellapi_lib = LoadLibraryW(L"shlwapi.dll");
	if (shellapi_lib) {
		load_indirect_string = (SHLoadIndirectStringPtr)GetProcAddress(shellapi_lib, "SHLoadIndirectString");
	}

	// Load UXTheme, available on Windows 10+ only.
	if (os_ver.dwBuildNumber >= 10240) {
		HMODULE ux_theme_lib = LoadLibraryW(L"uxtheme.dll");
		if (ux_theme_lib) {
			ShouldAppsUseDarkMode = (ShouldAppsUseDarkModePtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(132));
			GetImmersiveColorFromColorSetEx = (GetImmersiveColorFromColorSetExPtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(95));
			GetImmersiveColorTypeFromName = (GetImmersiveColorTypeFromNamePtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(96));
			GetImmersiveUserColorSetPreference = (GetImmersiveUserColorSetPreferencePtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(98));
			if (os_ver.dwBuildNumber >= 17763) { // Windows 10 Redstone 5 (1809)+ only.
				AllowDarkModeForAppPtr AllowDarkModeForApp = nullptr;
				SetPreferredAppModePtr SetPreferredAppMode = nullptr;
				FlushMenuThemesPtr FlushMenuThemes = nullptr;
				if (os_ver.dwBuildNumber < 18362) { // Windows 10 Redstone 5 (1809) and 19H1 (1903) only.
					AllowDarkModeForApp = (AllowDarkModeForAppPtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(135));
				} else { // Windows 10 19H2 (1909)+ only.
					SetPreferredAppMode = (SetPreferredAppModePtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(135));
					FlushMenuThemes = (FlushMenuThemesPtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(136));
				}
				RefreshImmersiveColorPolicyStatePtr RefreshImmersiveColorPolicyState = (RefreshImmersiveColorPolicyStatePtr)GetProcAddress(ux_theme_lib, MAKEINTRESOURCEA(104));
				if (ShouldAppsUseDarkMode) {
					bool dark_mode = ShouldAppsUseDarkMode();
					if (SetPreferredAppMode) {
						SetPreferredAppMode(dark_mode ? APPMODE_ALLOWDARK : APPMODE_DEFAULT);
					} else if (AllowDarkModeForApp) {
						AllowDarkModeForApp(dark_mode);
					}
					if (RefreshImmersiveColorPolicyState) {
						RefreshImmersiveColorPolicyState();
					}
					if (FlushMenuThemes) {
						FlushMenuThemes();
					}
				}
			}

			ux_theme_available = ShouldAppsUseDarkMode && GetImmersiveColorFromColorSetEx && GetImmersiveColorTypeFromName && GetImmersiveUserColorSetPreference;
			if (os_ver.dwBuildNumber >= 18363) {
				dark_title_available = true;
				if (os_ver.dwBuildNumber < 19041) {
					use_legacy_dark_mode_before_20H1 = true;
				}
			}
		}
	}

	// Note: Windows Ink API for pen input, available on Windows 8+ only.
	// Note: DPI conversion API, available on Windows 8.1+ only.
	HMODULE user32_lib = LoadLibraryW(L"user32.dll");
	if (user32_lib) {
		win8p_GetPointerType = (GetPointerTypePtr)GetProcAddress(user32_lib, "GetPointerType");
		win8p_GetPointerPenInfo = (GetPointerPenInfoPtr)GetProcAddress(user32_lib, "GetPointerPenInfo");
		win81p_LogicalToPhysicalPointForPerMonitorDPI = (LogicalToPhysicalPointForPerMonitorDPIPtr)GetProcAddress(user32_lib, "LogicalToPhysicalPointForPerMonitorDPI");
		win81p_PhysicalToLogicalPointForPerMonitorDPI = (PhysicalToLogicalPointForPerMonitorDPIPtr)GetProcAddress(user32_lib, "PhysicalToLogicalPointForPerMonitorDPI");

		winink_available = win8p_GetPointerType && win8p_GetPointerPenInfo;
	}

	if (winink_available) {
		tablet_drivers.push_back("winink");
	}

	// Note: Wacom WinTab driver API for pen input, for devices incompatible with Windows Ink.
	HMODULE wintab_lib = LoadLibraryW(L"wintab32.dll");
	if (wintab_lib) {
		wintab_WTOpen = (WTOpenPtr)GetProcAddress(wintab_lib, "WTOpenW");
		wintab_WTClose = (WTClosePtr)GetProcAddress(wintab_lib, "WTClose");
		wintab_WTInfo = (WTInfoPtr)GetProcAddress(wintab_lib, "WTInfoW");
		wintab_WTPacket = (WTPacketPtr)GetProcAddress(wintab_lib, "WTPacket");
		wintab_WTEnable = (WTEnablePtr)GetProcAddress(wintab_lib, "WTEnable");

		wintab_available = wintab_WTOpen && wintab_WTClose && wintab_WTInfo && wintab_WTPacket && wintab_WTEnable;
	}

	if (wintab_available) {
		tablet_drivers.push_back("wintab");
	}

	tablet_drivers.push_back("dummy");

	if (OS::get_singleton()->is_hidpi_allowed()) {
		HMODULE Shcore = LoadLibraryW(L"Shcore.dll");

		if (Shcore != nullptr) {
			typedef HRESULT(WINAPI * SetProcessDpiAwareness_t)(SHC_PROCESS_DPI_AWARENESS);

			SetProcessDpiAwareness_t SetProcessDpiAwareness = (SetProcessDpiAwareness_t)GetProcAddress(Shcore, "SetProcessDpiAwareness");

			if (SetProcessDpiAwareness) {
				SetProcessDpiAwareness(SHC_PROCESS_SYSTEM_DPI_AWARE);
			}
		}
	}

	HMODULE comctl32 = LoadLibraryW(L"comctl32.dll");
	if (comctl32) {
		typedef BOOL(WINAPI * InitCommonControlsExPtr)(_In_ const INITCOMMONCONTROLSEX *picce);
		InitCommonControlsExPtr init_common_controls_ex = (InitCommonControlsExPtr)GetProcAddress(comctl32, "InitCommonControlsEx");

		// Fails if the incorrect version was loaded. Probably not a big enough deal to print an error about.
		if (init_common_controls_ex) {
			INITCOMMONCONTROLSEX icc = {};
			icc.dwICC = ICC_STANDARD_CLASSES;
			icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
			if (!init_common_controls_ex(&icc)) {
				WARN_PRINT("Unable to initialize Windows common controls. Native dialogs may not work properly.");
			}
		}
		FreeLibrary(comctl32);
	}

	OleInitialize(nullptr);

	memset(&wc, 0, sizeof(WNDCLASSEXW));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc = (WNDPROC)::WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = hInstance ? hInstance : GetModuleHandle(nullptr);
	wc.hIcon = LoadIcon(nullptr, IDI_WINLOGO);
	wc.hCursor = nullptr;
	wc.hbrBackground = nullptr;
	wc.lpszMenuName = nullptr;
	wc.lpszClassName = L"Engine";

	if (!RegisterClassExW(&wc)) {
		r_error = ERR_UNAVAILABLE;
		return;
	}

	_register_raw_input_devices(INVALID_WINDOW_ID);

#if defined(RD_ENABLED)
#if defined(VULKAN_ENABLED)
	if (rendering_driver == "vulkan") {
		rendering_context = memnew(RenderingContextDriverVulkanWindows);
		tested_drivers.set_flag(DRIVER_ID_RD_VULKAN);
	}
#endif
#if defined(D3D12_ENABLED)
	if (rendering_driver == "d3d12") {
		rendering_context = memnew(RenderingContextDriverD3D12);
		tested_drivers.set_flag(DRIVER_ID_RD_D3D12);
	}
#endif

	if (rendering_context) {
		if (rendering_context->initialize() != OK) {
			bool failed = true;
#if defined(VULKAN_ENABLED)
			bool fallback_to_vulkan = GLOBAL_GET("rendering/rendering_device/fallback_to_vulkan");
			if (failed && fallback_to_vulkan && rendering_driver != "vulkan") {
				memdelete(rendering_context);
				rendering_context = memnew(RenderingContextDriverVulkanWindows);
				tested_drivers.set_flag(DRIVER_ID_RD_VULKAN);
				if (rendering_context->initialize() == OK) {
					WARN_PRINT("Your video card drivers seem not to support Direct3D 12, switching to Vulkan.");
					rendering_driver = "vulkan";
					OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
					failed = false;
				}
			}
#endif
#if defined(D3D12_ENABLED)
			bool fallback_to_d3d12 = GLOBAL_GET("rendering/rendering_device/fallback_to_d3d12");
			if (failed && fallback_to_d3d12 && rendering_driver != "d3d12") {
				memdelete(rendering_context);
				rendering_context = memnew(RenderingContextDriverD3D12);
				tested_drivers.set_flag(DRIVER_ID_RD_D3D12);
				if (rendering_context->initialize() == OK) {
					WARN_PRINT("Your video card drivers seem not to support Vulkan, switching to Direct3D 12.");
					rendering_driver = "d3d12";
					OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
					failed = false;
				}
			}
#endif
#if defined(GLES3_ENABLED)
			bool fallback_to_opengl3 = GLOBAL_GET("rendering/rendering_device/fallback_to_opengl3");
			if (failed && fallback_to_opengl3 && rendering_driver != "opengl3") {
				memdelete(rendering_context);
				rendering_context = nullptr;
				tested_drivers.set_flag(DRIVER_ID_COMPAT_OPENGL3);
				WARN_PRINT("Your video card drivers seem not to support Direct3D 12 or Vulkan, switching to OpenGL 3.");
				rendering_driver = "opengl3";
				OS::get_singleton()->set_current_rendering_method("gl_compatibility");
				OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
				failed = false;
			}
#endif
			if (failed) {
				memdelete(rendering_context);
				rendering_context = nullptr;
				r_error = ERR_UNAVAILABLE;
				return;
			}
		}
	}
#endif
// Init context and rendering device
#if defined(GLES3_ENABLED)

	bool fallback = GLOBAL_GET("rendering/gl_compatibility/fallback_to_angle");
	bool show_warning = true;

	if (rendering_driver == "opengl3") {
		// There's no native OpenGL drivers on Windows for ARM, always enable fallback.
#if defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
		fallback = true;
		show_warning = false;
#else
		typedef BOOL(WINAPI * IsWow64Process2Ptr)(HANDLE, USHORT *, USHORT *);

		IsWow64Process2Ptr IsWow64Process2 = (IsWow64Process2Ptr)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process2");
		if (IsWow64Process2) {
			USHORT process_arch = 0;
			USHORT machine_arch = 0;
			if (!IsWow64Process2(GetCurrentProcess(), &process_arch, &machine_arch)) {
				machine_arch = 0;
			}
			if (machine_arch == 0xAA64) {
				fallback = true;
				show_warning = false;
			}
		}
#endif
	}

	bool gl_supported = true;
	if (fallback && (rendering_driver == "opengl3")) {
		Dictionary gl_info = detect_wgl();

		bool force_angle = false;
		gl_supported = gl_info["version"].operator int() >= 30003;

		Vector2i device_id = _get_device_ids(gl_info["name"]);
		Array device_list = GLOBAL_GET("rendering/gl_compatibility/force_angle_on_devices");
		for (int i = 0; i < device_list.size(); i++) {
			const Dictionary &device = device_list[i];
			if (device.has("vendor") && device.has("name")) {
				const String &vendor = device["vendor"];
				const String &name = device["name"];
				if (device_id != Vector2i() && vendor.begins_with("0x") && name.begins_with("0x") && device_id.x == vendor.lstrip("0x").hex_to_int() && device_id.y == name.lstrip("0x").hex_to_int()) {
					// Check vendor/device IDs.
					force_angle = true;
					break;
				} else if (gl_info["vendor"].operator String().to_upper().contains(vendor.to_upper()) && (name == "*" || gl_info["name"].operator String().to_upper().contains(name.to_upper()))) {
					// Check vendor/device names.
					force_angle = true;
					break;
				}
			}
		}

		if (force_angle || (gl_info["version"].operator int() < 30003)) {
			tested_drivers.set_flag(DRIVER_ID_COMPAT_OPENGL3);
			if (show_warning) {
				if (gl_info["version"].operator int() < 30003) {
					WARN_PRINT("Your video card drivers seem not to support the required OpenGL 3.3 version, switching to ANGLE.");
				} else {
					WARN_PRINT("Your video card drivers are known to have low quality OpenGL 3.3 support, switching to ANGLE.");
				}
			}
			rendering_driver = "opengl3_angle";
			OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
		}
	}

	if (rendering_driver == "opengl3_angle") {
		gl_manager_angle = memnew(GLManagerANGLE_Windows);
		tested_drivers.set_flag(DRIVER_ID_COMPAT_ANGLE_D3D11);

		if (gl_manager_angle->initialize() != OK) {
			memdelete(gl_manager_angle);
			gl_manager_angle = nullptr;
			bool fallback_to_native = GLOBAL_GET("rendering/gl_compatibility/fallback_to_native");
			if (fallback_to_native && gl_supported) {
#ifdef EGL_STATIC
				WARN_PRINT("Your video card drivers seem not to support GLES3 / ANGLE, switching to native OpenGL.");
#else
				WARN_PRINT("Your video card drivers seem not to support GLES3 / ANGLE or ANGLE dynamic libraries (libEGL.dll and libGLESv2.dll) are missing, switching to native OpenGL.");
#endif
				rendering_driver = "opengl3";
			} else {
				r_error = ERR_UNAVAILABLE;
				ERR_FAIL_MSG("Could not initialize ANGLE OpenGL.");
			}
		}
	}
	if (rendering_driver == "opengl3") {
		gl_manager_native = memnew(GLManagerNative_Windows);
		tested_drivers.set_flag(DRIVER_ID_COMPAT_OPENGL3);

		if (gl_manager_native->initialize() != OK) {
			memdelete(gl_manager_native);
			gl_manager_native = nullptr;
			r_error = ERR_UNAVAILABLE;
			ERR_FAIL_MSG("Could not initialize native OpenGL.");
		}
	}

	if (rendering_driver == "opengl3") {
		RasterizerGLES3::make_current(true);
	}
	if (rendering_driver == "opengl3_angle") {
		RasterizerGLES3::make_current(false);
	}
#endif
	String appname;
	if (Engine::get_singleton()->is_editor_hint()) {
		appname = "Godot.GodotEditor." + String(VERSION_FULL_CONFIG);
	} else {
		String name = GLOBAL_GET("application/config/name");
		String version = GLOBAL_GET("application/config/version");
		if (version.is_empty()) {
			version = "0";
		}
		String clean_app_name = name.to_pascal_case();
		for (int i = 0; i < clean_app_name.length(); i++) {
			if (!is_ascii_alphanumeric_char(clean_app_name[i]) && clean_app_name[i] != '_' && clean_app_name[i] != '.') {
				clean_app_name[i] = '_';
			}
		}
		clean_app_name = clean_app_name.substr(0, 120 - version.length()).trim_suffix(".");
		appname = "Godot." + clean_app_name + "." + version;

#ifndef TOOLS_ENABLED
		// Set for exported projects only.
		HKEY key;
		if (RegOpenKeyW(HKEY_CURRENT_USER_LOCAL_SETTINGS, L"Software\\Microsoft\\Windows\\Shell\\MuiCache", &key) == ERROR_SUCCESS) {
			Char16String cs_name = name.utf16();
			String value_name = OS::get_singleton()->get_executable_path().replace("/", "\\") + ".FriendlyAppName";
			RegSetValueExW(key, (LPCWSTR)value_name.utf16().get_data(), 0, REG_SZ, (const BYTE *)cs_name.get_data(), cs_name.size() * sizeof(WCHAR));
			RegCloseKey(key);
		}
#endif
	}
	SetCurrentProcessExplicitAppUserModelID((PCWSTR)appname.utf16().get_data());

	mouse_monitor = SetWindowsHookEx(WH_MOUSE, ::MouseProc, nullptr, GetCurrentThreadId());

	Point2i window_position;
	if (p_position != nullptr) {
		window_position = *p_position;
	} else {
		if (p_screen == SCREEN_OF_MAIN_WINDOW) {
			p_screen = SCREEN_PRIMARY;
		}
		Rect2i scr_rect = screen_get_usable_rect(p_screen);
		window_position = scr_rect.position + (scr_rect.size - p_resolution) / 2;
	}

	HWND parent_hwnd = NULL;
	if (p_parent_window) {
		// Parented window.
		parent_hwnd = (HWND)p_parent_window;
	}

	WindowID main_window = _create_window(p_mode, p_vsync_mode, p_flags, Rect2i(window_position, p_resolution), false, INVALID_WINDOW_ID, parent_hwnd);
	if (main_window == INVALID_WINDOW_ID) {
		r_error = ERR_UNAVAILABLE;
		ERR_FAIL_MSG("Failed to create main window.");
	}

	joypad = new JoypadWindows(&windows[MAIN_WINDOW_ID].hWnd);

	for (int i = 0; i < WINDOW_FLAG_MAX; i++) {
		if (p_flags & (1 << i)) {
			window_set_flag(WindowFlags(i), true, main_window);
		}
	}

	windows[MAIN_WINDOW_ID].initialized = true;
	show_window(MAIN_WINDOW_ID);

#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_device = memnew(RenderingDevice);
		if (rendering_device->initialize(rendering_context, MAIN_WINDOW_ID) != OK) {
			memdelete(rendering_device);
			rendering_device = nullptr;
			memdelete(rendering_context);
			rendering_context = nullptr;
			r_error = ERR_UNAVAILABLE;
			return;
		}
		rendering_device->screen_create(MAIN_WINDOW_ID);

		RendererCompositorRD::make_current();
	}
#endif

	if (!Engine::get_singleton()->is_editor_hint() && !OS::get_singleton()->is_in_low_processor_usage_mode()) {
		// Increase priority for projects that are not in low-processor mode (typically games)
		// to reduce the risk of frame stuttering.
		// This is not done for the editor to prevent importers or resource bakers
		// from making the system unresponsive.
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
		DWORD index = 0;
		HANDLE handle = AvSetMmThreadCharacteristicsW(L"Games", &index);
		if (handle) {
			AvSetMmThreadPriority(handle, AVRT_PRIORITY_CRITICAL);
		}

		// This is needed to make sure that background work does not starve the main thread.
		// This is only setting the priority of this thread, not the whole process.
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	}

	cursor_shape = CURSOR_ARROW;

	_update_real_mouse_position(MAIN_WINDOW_ID);

	r_error = OK;

	static_cast<OS_Windows *>(OS::get_singleton())->set_main_window(windows[MAIN_WINDOW_ID].hWnd);
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);
}

Vector<String> DisplayServerWindows::get_rendering_drivers_func() {
	Vector<String> drivers;

#ifdef VULKAN_ENABLED
	drivers.push_back("vulkan");
#endif
#ifdef D3D12_ENABLED
	drivers.push_back("d3d12");
#endif
#ifdef GLES3_ENABLED
	drivers.push_back("opengl3");
	drivers.push_back("opengl3_angle");
#endif

	return drivers;
}

DisplayServer *DisplayServerWindows::create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	DisplayServer *ds = memnew(DisplayServerWindows(p_rendering_driver, p_mode, p_vsync_mode, p_flags, p_position, p_resolution, p_screen, p_context, p_parent_window, r_error));
	if (r_error != OK) {
		if (tested_drivers == 0) {
			OS::get_singleton()->alert("Failed to register the window class.", "Unable to initialize DisplayServer");
		} else if (tested_drivers.has_flag(DRIVER_ID_RD_VULKAN) || tested_drivers.has_flag(DRIVER_ID_RD_D3D12)) {
			Vector<String> drivers;
			if (tested_drivers.has_flag(DRIVER_ID_RD_VULKAN)) {
				drivers.push_back("Vulkan");
			}
			if (tested_drivers.has_flag(DRIVER_ID_RD_D3D12)) {
				drivers.push_back("Direct3D 12");
			}
			String executable_name = OS::get_singleton()->get_executable_path().get_file();
			OS::get_singleton()->alert(
					vformat("Your video card drivers seem not to support the required %s version.\n\n"
							"If possible, consider updating your video card drivers or using the OpenGL 3 driver.\n\n"
							"You can enable the OpenGL 3 driver by starting the engine from the\n"
							"command line with the command:\n\n    \"%s\" --rendering-driver opengl3\n\n"
							"If you have recently updated your video card drivers, try rebooting.",
							String(" or ").join(drivers),
							executable_name),
					"Unable to initialize video driver");
		} else {
			Vector<String> drivers;
			if (tested_drivers.has_flag(DRIVER_ID_COMPAT_OPENGL3)) {
				drivers.push_back("OpenGL 3.3");
			}
			if (tested_drivers.has_flag(DRIVER_ID_COMPAT_ANGLE_D3D11)) {
				drivers.push_back("Direct3D 11");
			}
			OS::get_singleton()->alert(
					vformat(
							"Your video card drivers seem not to support the required %s version.\n\n"
							"If possible, consider updating your video card drivers.\n\n"
							"If you have recently updated your video card drivers, try rebooting.",
							String(" or ").join(drivers)),
					"Unable to initialize video driver");
		}
	}
	return ds;
}

void DisplayServerWindows::register_windows_driver() {
	register_create_function("windows", create_func, get_rendering_drivers_func);
}

DisplayServerWindows::~DisplayServerWindows() {
	LocalVector<List<FileDialogData *>::Element *> to_remove;
	for (List<FileDialogData *>::Element *E = file_dialogs.front(); E; E = E->next()) {
		FileDialogData *fd = E->get();
		if (fd->listener_thread.is_started()) {
			fd->close_requested.set();
			fd->listener_thread.wait_to_finish();
		}
		to_remove.push_back(E);
	}
	for (List<FileDialogData *>::Element *E : to_remove) {
		memdelete(E->get());
		E->erase();
	}

	delete joypad;
	touch_state.clear();

	cursors_cache.clear();

	// Destroy all status indicators.
	for (HashMap<IndicatorID, IndicatorData>::Iterator E = indicators.begin(); E; ++E) {
		NOTIFYICONDATAW ndat;
		ZeroMemory(&ndat, sizeof(NOTIFYICONDATAW));
		ndat.cbSize = sizeof(NOTIFYICONDATAW);
		ndat.hWnd = windows[MAIN_WINDOW_ID].hWnd;
		ndat.uID = E->key;
		ndat.uVersion = NOTIFYICON_VERSION;

		Shell_NotifyIconW(NIM_DELETE, &ndat);
	}

	if (mouse_monitor) {
		UnhookWindowsHookEx(mouse_monitor);
	}

	if (user_proc) {
		SetWindowLongPtr(windows[MAIN_WINDOW_ID].hWnd, GWLP_WNDPROC, (LONG_PTR)user_proc);
	}

	// Close power request handle.
	screen_set_keep_on(false);

	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}

#ifdef GLES3_ENABLED
	// destroy windows .. NYI?
	// FIXME wglDeleteContext is never called
#endif

	if (windows.has(MAIN_WINDOW_ID)) {
#ifdef RD_ENABLED
		if (rendering_device) {
			rendering_device->screen_free(MAIN_WINDOW_ID);
		}

		if (rendering_context) {
			rendering_context->window_destroy(MAIN_WINDOW_ID);
		}
#endif
		if (wintab_available && windows[MAIN_WINDOW_ID].wtctx) {
			wintab_WTClose(windows[MAIN_WINDOW_ID].wtctx);
			windows[MAIN_WINDOW_ID].wtctx = nullptr;
		}

		if (windows[MAIN_WINDOW_ID].drop_target != nullptr) {
			RevokeDragDrop(windows[MAIN_WINDOW_ID].hWnd);
			windows[MAIN_WINDOW_ID].drop_target->Release();
		}

		DestroyWindow(windows[MAIN_WINDOW_ID].hWnd);
	}

#ifdef RD_ENABLED
	if (rendering_device) {
		memdelete(rendering_device);
		rendering_device = nullptr;
	}

	if (rendering_context) {
		memdelete(rendering_context);
		rendering_context = nullptr;
	}
#endif

	if (restore_mouse_trails > 1) {
		SystemParametersInfoA(SPI_SETMOUSETRAILS, restore_mouse_trails, nullptr, 0);
	}
#ifdef GLES3_ENABLED
	if (gl_manager_angle) {
		memdelete(gl_manager_angle);
		gl_manager_angle = nullptr;
	}
	if (gl_manager_native) {
		memdelete(gl_manager_native);
		gl_manager_native = nullptr;
	}
#endif
	if (tts) {
		memdelete(tts);
	}

	OleUninitialize();
}
