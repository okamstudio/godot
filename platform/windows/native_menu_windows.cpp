/**************************************************************************/
/*  native_menu_windows.cpp                                               */
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

#include "native_menu_windows.h"

#include "display_server_windows.h"

#include "scene/resources/image_texture.h"

HBITMAP NativeMenuWindows::_make_bitmap(const Ref<Image> &p_img) const {
	p_img->convert(Image::FORMAT_RGBA8);

	Vector2i texture_size = p_img->get_size();
	UINT image_size = texture_size.width * texture_size.height;

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
	for (UINT index = 0; index < image_size; index++) {
		int row_index = floor(index / texture_size.width);
		int column_index = (index % int(texture_size.width));
		const Color &c = p_img->get_pixel(column_index, row_index);
		*(buffer + index) = c.to_argb32();
	}
	ReleaseDC(nullptr, dc);

	return bitmap;
}

void NativeMenuWindows::_menu_activate(HMENU p_menu, int p_index) const {
	if (menu_lookup.has(p_menu)) {
		MenuData *md = menus.get_or_null(menu_lookup[p_menu]);
		if (md) {
			int count = GetMenuItemCount(md->menu);
			if (p_index >= 0 && p_index < count) {
				MENUITEMINFOW item;
				ZeroMemory(&item, sizeof(item));
				item.cbSize = sizeof(item);
				item.fMask = MIIM_STATE | MIIM_DATA;
				if (GetMenuItemInfoW(md->menu, p_index, true, &item)) {
					MenuItemData *item_data = (MenuItemData *)item.dwItemData;
					if (item_data) {
						if (item_data->callback.is_valid()) {
							Variant ret;
							Callable::CallError ce;
							const Variant *args[1] = { &item_data->meta };

							item_data->callback.callp(args, 1, ret, ce);
							if (ce.error != Callable::CallError::CALL_OK) {
								ERR_PRINT(vformat("Failed to execute menu callback: %s.", Variant::get_callable_error_text(item_data->callback, args, 1, ce)));
							}
						}
					}
				}
			}
		}
	}
}

bool NativeMenuWindows::has_feature(Feature p_feature) const {
	switch (p_feature) {
		// case FEATURE_GLOBAL_MENU:
		// case FEATURE_OPEN_CLOSE_CALLBACK:
		// case FEATURE_HOVER_CALLBACK:
		// case FEATURE_KEY_CALLBACK:
		case FEATURE_POPUP_MENU:
			return true;
		default:
			return false;
	}
}

bool NativeMenuWindows::has_system_menu(SystemMenus p_menu_id) const {
	return false;
}

RID NativeMenuWindows::get_system_menu(SystemMenus p_menu_id) const {
	return RID();
}

RID NativeMenuWindows::create_menu() {
	MenuData *md = memnew(MenuData);
	md->menu = CreatePopupMenu();

	MENUINFO menu_info;
	ZeroMemory(&menu_info, sizeof(menu_info));
	menu_info.cbSize = sizeof(menu_info);
	menu_info.fMask = MIM_STYLE;
	menu_info.dwStyle = MNS_NOTIFYBYPOS;
	SetMenuInfo(md->menu, &menu_info);

	RID rid = menus.make_rid(md);
	menu_lookup[md->menu] = rid;
	return rid;
}

bool NativeMenuWindows::has_menu(const RID &p_rid) const {
	return menus.owns(p_rid);
}

void NativeMenuWindows::free_menu(const RID &p_rid) {
	MenuData *md = menus.get_or_null(p_rid);
	if (md) {
		clear(p_rid);
		DestroyMenu(md->menu);
		menus.free(p_rid);
		menu_lookup.erase(md->menu);
		memdelete(md);
	}
}

Size2 NativeMenuWindows::get_size(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Size2());

	Size2 size;
	int count = GetMenuItemCount(md->menu);
	for (int i = 0; i < count; i++) {
		RECT rect;
		if (GetMenuItemRect(nullptr, md->menu, i, &rect)) {
			size.x = MAX(size.x, rect.right - rect.left);
			size.y += rect.bottom - rect.top;
		}
	}
	return size;
}

void NativeMenuWindows::popup(const RID &p_rid, const Vector2i &p_position) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	HWND hwnd = (HWND)DisplayServer::get_singleton()->window_get_native_handle(DisplayServer::WINDOW_HANDLE, DisplayServer::MAIN_WINDOW_ID);
	UINT flags = TPM_HORIZONTAL | TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON | TPM_VERPOSANIMATION;
	if (md->is_rtl) {
		flags |= TPM_LAYOUTRTL;
	}
	SetForegroundWindow(hwnd);
	TrackPopupMenuEx(md->menu, flags, p_position.x, p_position.y, hwnd, nullptr);

	if (md->close_cb.is_valid()) {
		Variant ret;
		Callable::CallError ce;
		md->close_cb.callp(nullptr, 0, ret, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_PRINT(vformat("Failed to execute popup close callback: %s.", Variant::get_callable_error_text(md->close_cb, nullptr, 0, ce)));
		}
	}

	PostMessage(hwnd, WM_NULL, 0, 0);
}

void NativeMenuWindows::set_interface_direction(const RID &p_rid, bool p_is_rtl) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	if (md->is_rtl == p_is_rtl) {
		return;
	}
	md->is_rtl = p_is_rtl;
}

void NativeMenuWindows::set_popup_open_callback(const RID &p_rid, const Callable &p_callback) {
	// Not supported.
}

Callable NativeMenuWindows::get_popup_open_callback(const RID &p_rid) const {
	// Not supported.
	return Callable();
}

void NativeMenuWindows::set_popup_close_callback(const RID &p_rid, const Callable &p_callback) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	md->close_cb = p_callback;
}

Callable NativeMenuWindows::get_popup_close_callback(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Callable());

	return md->close_cb;
}

void NativeMenuWindows::set_minimum_width(const RID &p_rid, float p_width) {
	// Not supported.
}

float NativeMenuWindows::get_minimum_width(const RID &p_rid) const {
	// Not supported.
	return 0.f;
}

bool NativeMenuWindows::is_opened(const RID &p_rid) const {
	// Not supported.
	return false;
}

int NativeMenuWindows::add_submenu_item(const RID &p_rid, const String &p_label, const RID &p_submenu_rid, const Variant &p_tag, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	MenuData *md_sub = menus.get_or_null(p_submenu_rid);
	ERR_FAIL_NULL_V(md, -1);
	ERR_FAIL_NULL_V(md_sub, -1);
	ERR_FAIL_COND_V_MSG(md->menu == md_sub->menu, -1, "Can't set submenu to self!");

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_NONE;
	item_data->max_states = 0;
	item_data->state = 0;

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA | MIIM_SUBMENU;
	item.fType = MFT_STRING;
	item.hSubMenu = md_sub->menu;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_item(const RID &p_rid, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->callback = p_callback;
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_NONE;
	item_data->max_states = 0;
	item_data->state = 0;

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA;
	item.fType = MFT_STRING;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_check_item(const RID &p_rid, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->callback = p_callback;
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_CHECK_BOX;
	item_data->max_states = 0;
	item_data->state = 0;

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA;
	item.fType = MFT_STRING;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_icon_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->callback = p_callback;
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_NONE;
	item_data->max_states = 0;
	item_data->state = 0;
	if (p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
		item_data->img = p_icon->get_image();
		item_data->img = item_data->img->duplicate();
		if (item_data->img->is_compressed()) {
			item_data->img->decompress();
		}
		item_data->bmp = _make_bitmap(item_data->img);
	}

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA | MIIM_BITMAP;
	item.fType = MFT_STRING;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();
	item.hbmpItem = item_data->bmp;

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_icon_check_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->callback = p_callback;
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_CHECK_BOX;
	item_data->max_states = 0;
	item_data->state = 0;
	if (p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
		item_data->img = p_icon->get_image();
		item_data->img = item_data->img->duplicate();
		if (item_data->img->is_compressed()) {
			item_data->img->decompress();
		}
		item_data->bmp = _make_bitmap(item_data->img);
	}

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA | MIIM_BITMAP;
	item.fType = MFT_STRING;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();
	item.hbmpItem = item_data->bmp;

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_radio_check_item(const RID &p_rid, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->callback = p_callback;
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_RADIO_BUTTON;
	item_data->max_states = 0;
	item_data->state = 0;

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA;
	item.fType = MFT_STRING | MFT_RADIOCHECK;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_icon_radio_check_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->callback = p_callback;
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_RADIO_BUTTON;
	item_data->max_states = 0;
	item_data->state = 0;
	if (p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
		item_data->img = p_icon->get_image();
		item_data->img = item_data->img->duplicate();
		if (item_data->img->is_compressed()) {
			item_data->img->decompress();
		}
		item_data->bmp = _make_bitmap(item_data->img);
	}

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA | MIIM_BITMAP;
	item.fType = MFT_STRING | MFT_RADIOCHECK;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();
	item.hbmpItem = item_data->bmp;

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_multistate_item(const RID &p_rid, const String &p_label, int p_max_states, int p_default_state, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->callback = p_callback;
	item_data->meta = p_tag;
	item_data->checkable_type = CHECKABLE_TYPE_NONE;
	item_data->max_states = p_max_states;
	item_data->state = p_default_state;

	Char16String label = p_label.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA;
	item.fType = MFT_STRING;
	item.dwItemData = (ULONG_PTR)item_data;
	item.dwTypeData = (LPWSTR)label.get_data();

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::add_separator(const RID &p_rid, int p_index) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (p_index == -1) {
		p_index = GetMenuItemCount(md->menu);
	} else {
		p_index = CLAMP(p_index, 0, GetMenuItemCount(md->menu));
	}

	MenuItemData *item_data = memnew(MenuItemData);
	item_data->checkable_type = CHECKABLE_TYPE_NONE;
	item_data->max_states = 0;
	item_data->state = 0;

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_DATA;
	item.fType = MFT_SEPARATOR;
	item.dwItemData = (ULONG_PTR)item_data;

	if (!InsertMenuItemW(md->menu, p_index, true, &item)) {
		memdelete(item_data);
		return -1;
	}
	return p_index;
}

int NativeMenuWindows::find_item_index_with_text(const RID &p_rid, const String &p_text) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	MENUITEMINFOW item;
	int count = GetMenuItemCount(md->menu);
	for (int i = 0; i < count; i++) {
		ZeroMemory(&item, sizeof(item));
		item.cbSize = sizeof(item);
		item.fMask = MIIM_STRING;
		item.dwTypeData = nullptr;
		if (GetMenuItemInfoW(md->menu, i, true, &item)) {
			item.cch++;
			Char16String str;
			str.resize(item.cch);
			item.dwTypeData = (LPWSTR)str.ptrw();
			if (GetMenuItemInfoW(md->menu, i, true, &item)) {
				if (String::utf16((const char16_t *)str.get_data()) == p_text) {
					return i;
				}
			}
		}
	}
	return -1;
}

int NativeMenuWindows::find_item_index_with_tag(const RID &p_rid, const Variant &p_tag) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	MENUITEMINFOW item;
	int count = GetMenuItemCount(md->menu);
	for (int i = 0; i < count; i++) {
		ZeroMemory(&item, sizeof(item));
		item.cbSize = sizeof(item);
		item.fMask = MIIM_DATA;
		if (GetMenuItemInfoW(md->menu, i, true, &item)) {
			MenuItemData *item_data = (MenuItemData *)item.dwItemData;
			if (item_data) {
				if (item_data->meta == p_tag) {
					return i;
				}
			}
		}
	}
	return -1;
}

bool NativeMenuWindows::is_item_checked(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, false);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_STATE | MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return item_data->checked;
		}
	}
	return false;
}

bool NativeMenuWindows::is_item_checkable(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, false);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return item_data->checkable_type == CHECKABLE_TYPE_CHECK_BOX;
		}
	}
	return false;
}

bool NativeMenuWindows::is_item_radio_checkable(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, false);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return item_data->checkable_type == CHECKABLE_TYPE_RADIO_BUTTON;
		}
	}
	return false;
}

Callable NativeMenuWindows::get_item_callback(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Callable());
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Callable());
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, Callable());

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return item_data->callback;
		}
	}
	return Callable();
}

Callable NativeMenuWindows::get_item_key_callback(const RID &p_rid, int p_idx) const {
	// Not supported.
	return Callable();
}

Variant NativeMenuWindows::get_item_tag(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Variant());
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Variant());
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, Variant());

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return item_data->meta;
		}
	}
	return Variant();
}

String NativeMenuWindows::get_item_text(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, String());
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, String());
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, String());

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_STRING;
	item.dwTypeData = nullptr;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		item.cch++;
		Char16String str;
		str.resize(item.cch);
		item.dwTypeData = (LPWSTR)str.ptrw();
		if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
			return String::utf16((const char16_t *)str.get_data());
		}
	}
	return String();
}

RID NativeMenuWindows::get_item_submenu(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, RID());
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, RID());
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, RID());

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_SUBMENU;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		if (menu_lookup.has(item.hSubMenu)) {
			return menu_lookup[item.hSubMenu];
		}
	}
	return RID();
}

Key NativeMenuWindows::get_item_accelerator(const RID &p_rid, int p_idx) const {
	// Not supported.
	return Key::NONE;
}

bool NativeMenuWindows::is_item_disabled(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, false);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_STATE;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		return (item.fState & MFS_DISABLED) == MFS_DISABLED;
	}
	return false;
}

bool NativeMenuWindows::is_item_hidden(const RID &p_rid, int p_idx) const {
	// Not supported.
	return false;
}

String NativeMenuWindows::get_item_tooltip(const RID &p_rid, int p_idx) const {
	// Not supported.
	return String();
}

int NativeMenuWindows::get_item_state(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, -1);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, -1);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return item_data->state;
		}
	}
	return -1;
}

int NativeMenuWindows::get_item_max_states(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, -1);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, -1);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return item_data->max_states;
		}
	}
	return -1;
}

Ref<Texture2D> NativeMenuWindows::get_item_icon(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Ref<Texture2D>());
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Ref<Texture2D>());
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND_V(p_idx >= count, Ref<Texture2D>());

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			return ImageTexture::create_from_image(item_data->img);
		}
	}
	return Ref<Texture2D>();
}

int NativeMenuWindows::get_item_indentation_level(const RID &p_rid, int p_idx) const {
	// Not supported.
	return 0;
}

void NativeMenuWindows::set_item_checked(const RID &p_rid, int p_idx, bool p_checked) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_STATE | MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			item_data->checked = p_checked;
			if (p_checked) {
				item.fState |= MFS_CHECKED;
			} else {
				item.fState &= ~MFS_CHECKED;
			}
		}
		SetMenuItemInfoW(md->menu, p_idx, true, &item);
	}
}

void NativeMenuWindows::set_item_checkable(const RID &p_rid, int p_idx, bool p_checkable) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			item.fType &= ~MFT_RADIOCHECK;
			item_data->checkable_type = (p_checkable) ? CHECKABLE_TYPE_CHECK_BOX : CHECKABLE_TYPE_NONE;
			SetMenuItemInfoW(md->menu, p_idx, true, &item);
		}
	}
}

void NativeMenuWindows::set_item_radio_checkable(const RID &p_rid, int p_idx, bool p_checkable) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			if (p_checkable) {
				item.fType |= MFT_RADIOCHECK;
				item_data->checkable_type = CHECKABLE_TYPE_CHECK_BOX;
			} else {
				item.fType &= ~MFT_RADIOCHECK;
				item_data->checkable_type = CHECKABLE_TYPE_NONE;
			}
			SetMenuItemInfoW(md->menu, p_idx, true, &item);
		}
	}
}

void NativeMenuWindows::set_item_callback(const RID &p_rid, int p_idx, const Callable &p_callback) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			item_data->callback = p_callback;
		}
	}
}

void NativeMenuWindows::set_item_key_callback(const RID &p_rid, int p_idx, const Callable &p_key_callback) {
	// Not supported.
}

void NativeMenuWindows::set_item_hover_callbacks(const RID &p_rid, int p_idx, const Callable &p_callback) {
	// Not supported.
}

void NativeMenuWindows::set_item_tag(const RID &p_rid, int p_idx, const Variant &p_tag) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			item_data->meta = p_tag;
		}
	}
}

void NativeMenuWindows::set_item_text(const RID &p_rid, int p_idx, const String &p_text) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	Char16String label = p_text.utf16();
	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		item.dwTypeData = (LPWSTR)label.get_data();
		SetMenuItemInfoW(md->menu, p_idx, true, &item);
	}
}

void NativeMenuWindows::set_item_submenu(const RID &p_rid, int p_idx, const RID &p_submenu_rid) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MenuData *md_sub = menus.get_or_null(p_submenu_rid);
	ERR_FAIL_COND_MSG(md->menu == md_sub->menu, "Can't set submenu to self!");

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_SUBMENU;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		if (p_submenu_rid.is_valid()) {
			item.hSubMenu = md_sub->menu;
		} else {
			item.hSubMenu = nullptr;
		}
		SetMenuItemInfoW(md->menu, p_idx, true, &item);
	}
}

void NativeMenuWindows::set_item_accelerator(const RID &p_rid, int p_idx, Key p_keycode) {
	// Not supported.
}

void NativeMenuWindows::set_item_disabled(const RID &p_rid, int p_idx, bool p_disabled) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_STATE;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		if (p_disabled) {
			item.fState |= MFS_DISABLED;
		} else {
			item.fState &= ~MFS_DISABLED;
		}
		SetMenuItemInfoW(md->menu, p_idx, true, &item);
	}
}

void NativeMenuWindows::set_item_hidden(const RID &p_rid, int p_idx, bool p_hidden) {
	// Not supported.
}

void NativeMenuWindows::set_item_tooltip(const RID &p_rid, int p_idx, const String &p_tooltip) {
	// Not supported.
}

void NativeMenuWindows::set_item_state(const RID &p_rid, int p_idx, int p_state) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			item_data->state = p_state;
		}
	}
}

void NativeMenuWindows::set_item_max_states(const RID &p_rid, int p_idx, int p_max_states) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			item_data->max_states = p_max_states;
		}
	}
}

void NativeMenuWindows::set_item_icon(const RID &p_rid, int p_idx, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA | MIIM_BITMAP;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			if (item_data->bmp) {
				DeleteObject(item_data->bmp);
			}
			if (p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
				item_data->img = p_icon->get_image();
				item_data->img = item_data->img->duplicate();
				if (item_data->img->is_compressed()) {
					item_data->img->decompress();
				}
				item_data->bmp = _make_bitmap(item_data->img);
			} else {
				item_data->img = Ref<Image>();
				item_data->bmp = nullptr;
			}
			item.hbmpItem = item_data->bmp;
			SetMenuItemInfoW(md->menu, p_idx, true, &item);
		}
	}
}

void NativeMenuWindows::set_item_indentation_level(const RID &p_rid, int p_idx, int p_level) {
	// Not supported.
}

int NativeMenuWindows::get_item_count(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, 0);

	return GetMenuItemCount(md->menu);
}

bool NativeMenuWindows::is_system_menu(const RID &p_rid) const {
	return false;
}

void NativeMenuWindows::remove_item(const RID &p_rid, int p_idx) {
	ERR_FAIL_COND(p_idx < 0);
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int count = GetMenuItemCount(md->menu);
	ERR_FAIL_COND(p_idx >= count);

	MENUITEMINFOW item;
	ZeroMemory(&item, sizeof(item));
	item.cbSize = sizeof(item);
	item.fMask = MIIM_DATA;
	if (GetMenuItemInfoW(md->menu, p_idx, true, &item)) {
		MenuItemData *item_data = (MenuItemData *)item.dwItemData;
		if (item_data) {
			if (item_data->bmp) {
				DeleteObject(item_data->bmp);
			}
			memdelete(item_data);
		}
	}
	RemoveMenu(md->menu, p_idx, MF_BYPOSITION);
}

void NativeMenuWindows::clear(const RID &p_rid) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	MENUITEMINFOW item;
	int count = GetMenuItemCount(md->menu);
	for (int i = 0; i < count; i++) {
		ZeroMemory(&item, sizeof(item));
		item.cbSize = sizeof(item);
		item.fMask = MIIM_DATA;
		if (GetMenuItemInfoW(md->menu, 0, true, &item)) {
			MenuItemData *item_data = (MenuItemData *)item.dwItemData;
			if (item_data) {
				if (item_data->bmp) {
					DeleteObject(item_data->bmp);
				}
				memdelete(item_data);
			}
		}
		RemoveMenu(md->menu, 0, MF_BYPOSITION);
	}
}

NativeMenuWindows::NativeMenuWindows() {}

NativeMenuWindows::~NativeMenuWindows() {}
