/**************************************************************************/
/*  native_menu_macos.mm                                                  */
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

#import "native_menu_macos.h"

#import "display_server_macos.h"
#import "godot_menu_item.h"
#import "key_mapping_macos.h"

#include "scene/resources/image_texture.h"

void NativeMenuMacOS::_register_system_menus(NSMenu *p_main_menu, NSMenu *p_application_menu, NSMenu *p_window_menu, NSMenu *p_help_menu, NSMenu *p_dock_menu) {
	{
		MenuData *md = memnew(MenuData);
		md->menu = p_main_menu;
		md->is_system = true;
		main_menu = menus.make_rid(md);
		main_menu_ns = p_main_menu;
		menu_lookup[md->menu] = main_menu;
	}
	{
		MenuData *md = memnew(MenuData);
		md->menu = p_application_menu;
		md->is_system = true;
		application_menu = menus.make_rid(md);
		application_menu_ns = p_application_menu;
		menu_lookup[md->menu] = application_menu;
	}
	{
		MenuData *md = memnew(MenuData);
		md->menu = p_window_menu;
		md->is_system = true;
		window_menu = menus.make_rid(md);
		window_menu_ns = p_window_menu;
		menu_lookup[md->menu] = window_menu;
	}
	{
		MenuData *md = memnew(MenuData);
		md->menu = p_help_menu;
		md->is_system = true;
		help_menu = menus.make_rid(md);
		help_menu_ns = p_help_menu;
		menu_lookup[md->menu] = help_menu;
	}
	{
		MenuData *md = memnew(MenuData);
		md->menu = p_dock_menu;
		md->is_system = true;
		dock_menu = menus.make_rid(md);
		dock_menu_ns = p_dock_menu;
		menu_lookup[md->menu] = dock_menu;
	}
}

NSMenu *NativeMenuMacOS::_get_dock_menu() {
	MenuData *md = menus.get_or_null(dock_menu);
	if (md) {
		return md->menu;
	}
	return nullptr;
}

void NativeMenuMacOS::_menu_open(NSMenu *p_menu) {
	if (menu_lookup.has(p_menu)) {
		MenuData *md = menus.get_or_null(menu_lookup[p_menu]);
		if (md) {
			// Note: Set "is_open" flag, but do not call callback, menu items can't be modified during this call and "_menu_need_update" will be called right before it.
			md->is_open = true;
		}
	}
}

void NativeMenuMacOS::_menu_need_update(NSMenu *p_menu) {
	if (menu_lookup.has(p_menu)) {
		MenuData *md = menus.get_or_null(menu_lookup[p_menu]);
		if (md) {
			// Note: "is_open" flag is set by "_menu_open", this method is always called before menu is shown, but might be called for the other reasons as well.
			if (md->open_cb.is_valid()) {
				Variant ret;
				Callable::CallError ce;

				// Callback is called directly, since it's expected to modify menu items before it's shown.
				md->open_cb.callp(nullptr, 0, ret, ce);
				if (ce.error != Callable::CallError::CALL_OK) {
					ERR_PRINT(vformat("Failed to execute menu open callback: %s.", Variant::get_callable_error_text(md->open_cb, nullptr, 0, ce)));
				}
			}
		}
	}
}

void NativeMenuMacOS::_menu_close(NSMenu *p_menu) {
	if (menu_lookup.has(p_menu)) {
		MenuData *md = menus.get_or_null(menu_lookup[p_menu]);
		if (md) {
			md->is_open = false;

			// Callback called deferred, since it should not modify menu items during "_menu_close" call.
			callable_mp(this, &NativeMenuMacOS::_menu_close_cb).call_deferred(menu_lookup[p_menu]);
		}
	}
}

void NativeMenuMacOS::_menu_close_cb(const RID &p_rid) {
	MenuData *md = menus.get_or_null(p_rid);
	if (md->close_cb.is_valid()) {
		Variant ret;
		Callable::CallError ce;

		md->close_cb.callp(nullptr, 0, ret, ce);
		if (ce.error != Callable::CallError::CALL_OK) {
			ERR_PRINT(vformat("Failed to execute menu close callback: %s.", Variant::get_callable_error_text(md->close_cb, nullptr, 0, ce)));
		}
	}
}

bool NativeMenuMacOS::_is_menu_opened(NSMenu *p_menu) const {
	if (menu_lookup.has(p_menu)) {
		const MenuData *md = menus.get_or_null(menu_lookup[p_menu]);
		if (md && md->is_open) {
			return true;
		}
	}
	for (NSInteger i = (p_menu == [NSApp mainMenu]) ? 1 : 0; i < [p_menu numberOfItems]; i++) {
		const NSMenuItem *menu_item = [p_menu itemAtIndex:i];
		if ([menu_item submenu]) {
			if (_is_menu_opened([menu_item submenu])) {
				return true;
			}
		}
	}
	return false;
}

int NativeMenuMacOS::_get_system_menu_start(const NSMenu *p_menu) const {
	if (p_menu == [NSApp mainMenu]) { // Skip Apple menu.
		return 1;
	}
	if (p_menu == application_menu_ns || p_menu == window_menu_ns || p_menu == help_menu_ns) {
		int count = [p_menu numberOfItems];
		for (int i = 0; i < count; i++) {
			NSMenuItem *menu_item = [p_menu itemAtIndex:i];
			if (menu_item.tag == MENU_TAG_START) {
				return i + 1;
			}
		}
	}
	return 0;
}

int NativeMenuMacOS::_get_system_menu_count(const NSMenu *p_menu) const {
	if (p_menu == [NSApp mainMenu]) { // Skip Apple, Window and Help menu.
		return [p_menu numberOfItems] - 3;
	}
	if (p_menu == application_menu_ns || p_menu == window_menu_ns || p_menu == help_menu_ns) {
		int start = 0;
		int count = [p_menu numberOfItems];
		for (int i = 0; i < count; i++) {
			NSMenuItem *menu_item = [p_menu itemAtIndex:i];
			if (menu_item.tag == MENU_TAG_START) {
				start = i + 1;
			}
			if (menu_item.tag == MENU_TAG_END) {
				return i - start;
			}
		}
	}
	return [p_menu numberOfItems];
}

bool NativeMenuMacOS::has_feature(Feature p_feature) const {
	switch (p_feature) {
		case FEATURE_GLOBAL_MENU:
		case FEATURE_POPUP_MENU:
		case FEATURE_OPEN_CLOSE_CALLBACK:
		case FEATURE_HOVER_CALLBACK:
		case FEATURE_KEY_CALLBACK:
			return true;
		default:
			return false;
	}
}

bool NativeMenuMacOS::has_system_menu(SystemMenus p_menu_id) const {
	switch (p_menu_id) {
		case MAIN_MENU_ID:
		case APPLICATION_MENU_ID:
		case WINDOW_MENU_ID:
		case HELP_MENU_ID:
		case DOCK_MENU_ID:
			return true;
		default:
			return false;
	}
}

RID NativeMenuMacOS::get_system_menu(SystemMenus p_menu_id) const {
	switch (p_menu_id) {
		case MAIN_MENU_ID:
			return main_menu;
		case APPLICATION_MENU_ID:
			return application_menu;
		case WINDOW_MENU_ID:
			return window_menu;
		case HELP_MENU_ID:
			return help_menu;
		case DOCK_MENU_ID:
			return dock_menu;
		default:
			return RID();
	}
}

RID NativeMenuMacOS::create_menu() {
	MenuData *md = memnew(MenuData);
	md->menu = [[NSMenu alloc] initWithTitle:@""];
	[md->menu setAutoenablesItems:NO];
	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds) {
		ds->set_menu_delegate(md->menu);
	}
	RID rid = menus.make_rid(md);
	menu_lookup[md->menu] = rid;
	return rid;
}

bool NativeMenuMacOS::has_menu(const RID &p_rid) const {
	return menus.owns(p_rid);
}

void NativeMenuMacOS::free_menu(const RID &p_rid) {
	MenuData *md = menus.get_or_null(p_rid);
	if (md && !md->is_system) {
		clear(p_rid);
		menus.free(p_rid);
		menu_lookup.erase(md->menu);
		md->menu = nullptr;
		memdelete(md);
	}
}

NSMenu *NativeMenuMacOS::get_native_menu_handle(const RID &p_rid) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, nullptr);

	return md->menu;
}

Size2 NativeMenuMacOS::get_size(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Size2());

	return Size2(md->menu.size.width, md->menu.size.height) * DisplayServer::get_singleton()->screen_get_max_scale();
}

void NativeMenuMacOS::popup(const RID &p_rid, const Vector2i &p_position) {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
	if (ds) {
		Point2i position = p_position;
		// macOS native y-coordinate relative to _get_screens_origin() is negative,
		// Godot passes a positive value.
		position.y *= -1;
		position += ds->_get_screens_origin();
		position /= ds->screen_get_max_scale();

		[md->menu popUpMenuPositioningItem:nil atLocation:NSMakePoint(position.x, position.y - 5) inView:nil]; // Menu vertical position doesn't include rounded corners, add `5` display pixels to better align it with Godot buttons.
	}
}

void NativeMenuMacOS::set_interface_direction(const RID &p_rid, bool p_is_rtl) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	md->menu.userInterfaceLayoutDirection = p_is_rtl ? NSUserInterfaceLayoutDirectionRightToLeft : NSUserInterfaceLayoutDirectionLeftToRight;
}

void NativeMenuMacOS::set_popup_open_callback(const RID &p_rid, const Callable &p_callback) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	md->open_cb = p_callback;
}

Callable NativeMenuMacOS::get_popup_open_callback(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Callable());

	return md->open_cb;
}

void NativeMenuMacOS::set_popup_close_callback(const RID &p_rid, const Callable &p_callback) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	md->close_cb = p_callback;
}

Callable NativeMenuMacOS::get_popup_close_callback(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Callable());

	return md->close_cb;
}

void NativeMenuMacOS::set_minimum_width(const RID &p_rid, float p_width) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	md->menu.minimumWidth = p_width / DisplayServer::get_singleton()->screen_get_max_scale();
}

float NativeMenuMacOS::get_minimum_width(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, 0.0);

	return md->menu.minimumWidth * DisplayServer::get_singleton()->screen_get_max_scale();
}

bool NativeMenuMacOS::is_opened(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);

	return md->is_open;
}

int NativeMenuMacOS::add_submenu_item(const RID &p_rid, const String &p_label, const RID &p_submenu_rid, const Variant &p_tag, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	MenuData *md_sub = menus.get_or_null(p_submenu_rid);
	ERR_FAIL_NULL_V(md, -1);
	ERR_FAIL_NULL_V(md_sub, -1);
	ERR_FAIL_COND_V_MSG(md->menu == md_sub->menu, -1, "Can't set submenu to self!");
	ERR_FAIL_COND_V_MSG([md_sub->menu supermenu], -1, "Can't set submenu to menu that is already a submenu of some other menu!");

	NSMenuItem *menu_item;
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	if (p_index < 0) {
		p_index = item_start + item_count;
	} else {
		p_index += item_start;
		p_index = CLAMP(p_index, item_start, item_start + item_count);
	}
	menu_item = [md->menu insertItemWithTitle:[NSString stringWithUTF8String:p_label.utf8().get_data()] action:nil keyEquivalent:@"" atIndex:p_index];

	GodotMenuItem *obj = [[GodotMenuItem alloc] init];
	obj->meta = p_tag;
	[menu_item setRepresentedObject:obj];

	[md_sub->menu setTitle:[NSString stringWithUTF8String:p_label.utf8().get_data()]];
	[md->menu setSubmenu:md_sub->menu forItem:menu_item];

	return p_index - item_start;
}

NSMenuItem *NativeMenuMacOS::_menu_add_item(NSMenu *p_menu, const String &p_label, Key p_accel, int p_index, int *r_out) {
	if (p_menu) {
		String keycode = KeyMappingMacOS::keycode_get_native_string(p_accel & KeyModifierMask::CODE_MASK);
		NSMenuItem *menu_item;
		int item_start = _get_system_menu_start(p_menu);
		int item_count = _get_system_menu_count(p_menu);
		if (p_index < 0) {
			p_index = item_start + item_count;
		} else {
			p_index += item_start;
			p_index = CLAMP(p_index, item_start, item_start + item_count);
		}
		menu_item = [p_menu insertItemWithTitle:[NSString stringWithUTF8String:p_label.utf8().get_data()] action:@selector(globalMenuCallback:) keyEquivalent:[NSString stringWithUTF8String:keycode.utf8().get_data()] atIndex:p_index];
		*r_out = p_index - item_start;
		return menu_item;
	}
	return nullptr;
}

int NativeMenuMacOS::add_item(const RID &p_rid, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int out = -1;
	NSMenuItem *menu_item = _menu_add_item(md->menu, p_label, p_accel, p_index, &out);
	if (menu_item) {
		GodotMenuItem *obj = [[GodotMenuItem alloc] init];
		obj->callback = p_callback;
		obj->key_callback = p_key_callback;
		obj->meta = p_tag;
		[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_accel)];
		[menu_item setRepresentedObject:obj];
	}
	return out;
}

int NativeMenuMacOS::add_check_item(const RID &p_rid, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int out = -1;
	NSMenuItem *menu_item = _menu_add_item(md->menu, p_label, p_accel, p_index, &out);
	if (menu_item) {
		GodotMenuItem *obj = [[GodotMenuItem alloc] init];
		obj->callback = p_callback;
		obj->key_callback = p_key_callback;
		obj->meta = p_tag;
		obj->checkable_type = CHECKABLE_TYPE_CHECK_BOX;
		[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_accel)];
		[menu_item setRepresentedObject:obj];
	}
	return out;
}

int NativeMenuMacOS::add_icon_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int out = -1;
	NSMenuItem *menu_item = _menu_add_item(md->menu, p_label, p_accel, p_index, &out);
	if (menu_item) {
		GodotMenuItem *obj = [[GodotMenuItem alloc] init];
		obj->callback = p_callback;
		obj->key_callback = p_key_callback;
		obj->meta = p_tag;
		DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
		if (ds && p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
			obj->img = p_icon->get_image();
			obj->img = obj->img->duplicate();
			if (obj->img->is_compressed()) {
				obj->img->decompress();
			}
			NSImage *image = ds->_convert_to_nsimg(obj->img);
			[image setSize:NSMakeSize(16, 16)];
			[menu_item setImage:image];
		}
		[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_accel)];
		[menu_item setRepresentedObject:obj];
	}
	return out;
}

int NativeMenuMacOS::add_icon_check_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int out = -1;
	NSMenuItem *menu_item = _menu_add_item(md->menu, p_label, p_accel, p_index, &out);
	if (menu_item) {
		GodotMenuItem *obj = [[GodotMenuItem alloc] init];
		obj->callback = p_callback;
		obj->key_callback = p_key_callback;
		obj->meta = p_tag;
		obj->checkable_type = CHECKABLE_TYPE_CHECK_BOX;
		DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
		if (ds && p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
			obj->img = p_icon->get_image();
			obj->img = obj->img->duplicate();
			if (obj->img->is_compressed()) {
				obj->img->decompress();
			}
			NSImage *image = ds->_convert_to_nsimg(obj->img);
			[image setSize:NSMakeSize(16, 16)];
			[menu_item setImage:image];
		}
		[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_accel)];
		[menu_item setRepresentedObject:obj];
	}
	return out;
}

int NativeMenuMacOS::add_radio_check_item(const RID &p_rid, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int out = -1;
	NSMenuItem *menu_item = _menu_add_item(md->menu, p_label, p_accel, p_index, &out);
	if (menu_item) {
		GodotMenuItem *obj = [[GodotMenuItem alloc] init];
		obj->callback = p_callback;
		obj->key_callback = p_key_callback;
		obj->meta = p_tag;
		obj->checkable_type = CHECKABLE_TYPE_RADIO_BUTTON;
		[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_accel)];
		[menu_item setRepresentedObject:obj];
	}
	return out;
}

int NativeMenuMacOS::add_icon_radio_check_item(const RID &p_rid, const Ref<Texture2D> &p_icon, const String &p_label, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int out = -1;
	NSMenuItem *menu_item = _menu_add_item(md->menu, p_label, p_accel, p_index, &out);
	if (menu_item) {
		GodotMenuItem *obj = [[GodotMenuItem alloc] init];
		obj->callback = p_callback;
		obj->key_callback = p_key_callback;
		obj->meta = p_tag;
		obj->checkable_type = CHECKABLE_TYPE_RADIO_BUTTON;
		DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
		if (ds && p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
			obj->img = p_icon->get_image();
			obj->img = obj->img->duplicate();
			if (obj->img->is_compressed()) {
				obj->img->decompress();
			}
			NSImage *image = ds->_convert_to_nsimg(obj->img);
			[image setSize:NSMakeSize(16, 16)];
			[menu_item setImage:image];
		}
		[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_accel)];
		[menu_item setRepresentedObject:obj];
	}
	return out;
}

int NativeMenuMacOS::add_multistate_item(const RID &p_rid, const String &p_label, int p_max_states, int p_default_state, const Callable &p_callback, const Callable &p_key_callback, const Variant &p_tag, Key p_accel, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int out = -1;
	NSMenuItem *menu_item = _menu_add_item(md->menu, p_label, p_accel, p_index, &out);
	if (menu_item) {
		GodotMenuItem *obj = [[GodotMenuItem alloc] init];
		obj->callback = p_callback;
		obj->key_callback = p_key_callback;
		obj->meta = p_tag;
		obj->max_states = p_max_states;
		obj->state = p_default_state;
		[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_accel)];
		[menu_item setRepresentedObject:obj];
	}
	return out;
}

int NativeMenuMacOS::add_separator(const RID &p_rid, int p_index) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	if (md->menu == [NSApp mainMenu]) { // Do not add separators into main menu.
		return -1;
	}
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	if (p_index < 0) {
		p_index = item_start + item_count;
	} else {
		p_index += item_start;
		p_index = CLAMP(p_index, item_start, item_start + item_count);
	}
	[md->menu insertItem:[NSMenuItem separatorItem] atIndex:p_index];
	return p_index - item_start;
}

int NativeMenuMacOS::find_item_index_with_text(const RID &p_rid, const String &p_text) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int item_start = _get_system_menu_start(md->menu);
	int index = [md->menu indexOfItemWithTitle:[NSString stringWithUTF8String:p_text.utf8().get_data()]];
	if (index >= 0) {
		return index - item_start;
	}
	return -1;
}

int NativeMenuMacOS::find_item_index_with_tag(const RID &p_rid, const Variant &p_tag) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, -1);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	for (NSInteger i = item_start; i < item_start + item_count; i++) {
		const NSMenuItem *menu_item = [md->menu itemAtIndex:i];
		if (menu_item) {
			const GodotMenuItem *obj = [menu_item representedObject];
			if (obj && obj->meta == p_tag) {
				return i - item_start;
			}
		}
	}
	return -1;
}

bool NativeMenuMacOS::is_item_checked(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, false);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		const GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->checked;
		}
	}
	return false;
}

bool NativeMenuMacOS::is_item_checkable(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, false);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->checkable_type == CHECKABLE_TYPE_CHECK_BOX;
		}
	}
	return false;
}

bool NativeMenuMacOS::is_item_radio_checkable(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, false);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->checkable_type == CHECKABLE_TYPE_RADIO_BUTTON;
		}
	}
	return false;
}

Callable NativeMenuMacOS::get_item_callback(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Callable());

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Callable());

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, Callable());
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->callback;
		}
	}
	return Callable();
}

Callable NativeMenuMacOS::get_item_key_callback(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Callable());

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Callable());

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, Callable());
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->key_callback;
		}
	}
	return Callable();
}

Variant NativeMenuMacOS::get_item_tag(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Variant());

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Variant());

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, Variant());
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->meta;
		}
	}
	return Variant();
}

String NativeMenuMacOS::get_item_text(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, String());

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, String());

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, String());
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		return String::utf8([[menu_item title] UTF8String]);
	}
	return String();
}

RID NativeMenuMacOS::get_item_submenu(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, RID());

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, RID());

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, RID());
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		NSMenu *sub_menu = [menu_item submenu];
		if (sub_menu && menu_lookup.has(sub_menu)) {
			return menu_lookup[sub_menu];
		}
	}
	return RID();
}

Key NativeMenuMacOS::get_item_accelerator(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Key::NONE);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Key::NONE);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, Key::NONE);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		String ret = String::utf8([[menu_item keyEquivalent] UTF8String]);
		Key keycode = find_keycode(ret);
		NSUInteger mask = [menu_item keyEquivalentModifierMask];
		if (mask & NSEventModifierFlagControl) {
			keycode |= KeyModifierMask::CTRL;
		}
		if (mask & NSEventModifierFlagOption) {
			keycode |= KeyModifierMask::ALT;
		}
		if (mask & NSEventModifierFlagShift) {
			keycode |= KeyModifierMask::SHIFT;
		}
		if (mask & NSEventModifierFlagCommand) {
			keycode |= KeyModifierMask::META;
		}
		if (mask & NSEventModifierFlagNumericPad) {
			keycode |= KeyModifierMask::KPAD;
		}
		return keycode;
	}
	return Key::NONE;
}

bool NativeMenuMacOS::is_item_disabled(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, false);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		return ![menu_item isEnabled];
	}
	return false;
}

bool NativeMenuMacOS::is_item_hidden(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, false);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, false);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		return [menu_item isHidden];
	}
	return false;
}

String NativeMenuMacOS::get_item_tooltip(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, String());

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, String());

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, String());
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		return String::utf8([[menu_item toolTip] UTF8String]);
	}
	return String();
}

int NativeMenuMacOS::get_item_state(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, 0);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, 0);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, 0);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->state;
		}
	}
	return 0;
}

int NativeMenuMacOS::get_item_max_states(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, 0);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, 0);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, 0);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			return obj->max_states;
		}
	}
	return 0;
}

Ref<Texture2D> NativeMenuMacOS::get_item_icon(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, Ref<Texture2D>());

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, Ref<Texture2D>());

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, Ref<Texture2D>());
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			if (obj->img.is_valid()) {
				return ImageTexture::create_from_image(obj->img);
			}
		}
	}
	return Ref<Texture2D>();
}

int NativeMenuMacOS::get_item_indentation_level(const RID &p_rid, int p_idx) const {
	ERR_FAIL_COND_V(p_idx < 0, 0);

	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, 0);

	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND_V(p_idx >= item_start + item_count, 0);
	const NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		return [menu_item indentationLevel];
	}
	return 0;
}

void NativeMenuMacOS::set_item_checked(const RID &p_rid, int p_idx, bool p_checked) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		if (obj) {
			obj->checked = p_checked;
			if (p_checked) {
				[menu_item setState:NSControlStateValueOn];
			} else {
				[menu_item setState:NSControlStateValueOff];
			}
		}
	}
}

void NativeMenuMacOS::set_item_checkable(const RID &p_rid, int p_idx, bool p_checkable) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->checkable_type = (p_checkable) ? CHECKABLE_TYPE_CHECK_BOX : CHECKABLE_TYPE_NONE;
	}
}

void NativeMenuMacOS::set_item_radio_checkable(const RID &p_rid, int p_idx, bool p_checkable) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->checkable_type = (p_checkable) ? CHECKABLE_TYPE_RADIO_BUTTON : CHECKABLE_TYPE_NONE;
	}
}

void NativeMenuMacOS::set_item_callback(const RID &p_rid, int p_idx, const Callable &p_callback) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->callback = p_callback;
	}
}

void NativeMenuMacOS::set_item_key_callback(const RID &p_rid, int p_idx, const Callable &p_key_callback) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->key_callback = p_key_callback;
	}
}

void NativeMenuMacOS::set_item_hover_callbacks(const RID &p_rid, int p_idx, const Callable &p_callback) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->hover_callback = p_callback;
	}
}

void NativeMenuMacOS::set_item_tag(const RID &p_rid, int p_idx, const Variant &p_tag) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->meta = p_tag;
	}
}

void NativeMenuMacOS::set_item_text(const RID &p_rid, int p_idx, const String &p_text) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		[menu_item setTitle:[NSString stringWithUTF8String:p_text.utf8().get_data()]];
		NSMenu *sub_menu = [menu_item submenu];
		if (sub_menu) {
			[sub_menu setTitle:[NSString stringWithUTF8String:p_text.utf8().get_data()]];
		}
	}
}

void NativeMenuMacOS::set_item_submenu(const RID &p_rid, int p_idx, const RID &p_submenu_rid) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);

	if (p_submenu_rid.is_valid()) {
		MenuData *md_sub = menus.get_or_null(p_submenu_rid);
		ERR_FAIL_NULL(md_sub);
		ERR_FAIL_COND_MSG(md->menu == md_sub->menu, "Can't set submenu to self!");
		ERR_FAIL_COND_MSG([md_sub->menu supermenu], "Can't set submenu to menu that is already a submenu of some other menu!");

		int item_start = _get_system_menu_start(md->menu);
		int item_count = _get_system_menu_count(md->menu);
		p_idx += item_start;
		ERR_FAIL_COND(p_idx >= item_start + item_count);
		NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
		if (menu_item) {
			[md->menu setSubmenu:md_sub->menu forItem:menu_item];
		}
	} else {
		int item_start = _get_system_menu_start(md->menu);
		int item_count = _get_system_menu_count(md->menu);
		p_idx += item_start;
		ERR_FAIL_COND(p_idx >= item_start + item_count);
		NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
		if (menu_item) {
			if ([menu_item submenu] && _is_menu_opened([menu_item submenu])) {
				ERR_PRINT("Can't remove open menu!");
				return;
			}
			[md->menu setSubmenu:nil forItem:menu_item];
		}
	}
}

void NativeMenuMacOS::set_item_accelerator(const RID &p_rid, int p_idx, Key p_keycode) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		if (p_keycode == Key::NONE) {
			[menu_item setKeyEquivalent:@""];
		} else {
			[menu_item setKeyEquivalentModifierMask:KeyMappingMacOS::keycode_get_native_mask(p_keycode)];
			String keycode = KeyMappingMacOS::keycode_get_native_string(p_keycode & KeyModifierMask::CODE_MASK);
			[menu_item setKeyEquivalent:[NSString stringWithUTF8String:keycode.utf8().get_data()]];
		}
	}
}

void NativeMenuMacOS::set_item_disabled(const RID &p_rid, int p_idx, bool p_disabled) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		[menu_item setEnabled:(!p_disabled)];
	}
}

void NativeMenuMacOS::set_item_hidden(const RID &p_rid, int p_idx, bool p_hidden) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		[menu_item setHidden:p_hidden];
	}
}

void NativeMenuMacOS::set_item_tooltip(const RID &p_rid, int p_idx, const String &p_tooltip) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		[menu_item setToolTip:[NSString stringWithUTF8String:p_tooltip.utf8().get_data()]];
	}
}

void NativeMenuMacOS::set_item_state(const RID &p_rid, int p_idx, int p_state) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->state = p_state;
	}
}

void NativeMenuMacOS::set_item_max_states(const RID &p_rid, int p_idx, int p_max_states) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		obj->max_states = p_max_states;
	}
}

void NativeMenuMacOS::set_item_icon(const RID &p_rid, int p_idx, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		GodotMenuItem *obj = [menu_item representedObject];
		ERR_FAIL_NULL(obj);
		DisplayServerMacOS *ds = (DisplayServerMacOS *)DisplayServer::get_singleton();
		if (ds && p_icon.is_valid() && p_icon->get_width() > 0 && p_icon->get_height() > 0 && p_icon->get_image().is_valid()) {
			obj->img = p_icon->get_image();
			obj->img = obj->img->duplicate();
			if (obj->img->is_compressed()) {
				obj->img->decompress();
			}
			NSImage *image = ds->_convert_to_nsimg(obj->img);
			[image setSize:NSMakeSize(16, 16)];
			[menu_item setImage:image];
		} else {
			obj->img = Ref<Image>();
			[menu_item setImage:nil];
		}
	}
}

void NativeMenuMacOS::set_item_indentation_level(const RID &p_rid, int p_idx, int p_level) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if (menu_item) {
		[menu_item setIndentationLevel:p_level];
	}
}

int NativeMenuMacOS::get_item_count(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, 0);

	return _get_system_menu_count(md->menu);
}

bool NativeMenuMacOS::is_system_menu(const RID &p_rid) const {
	const MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL_V(md, false);

	return md->is_system;
}

void NativeMenuMacOS::remove_item(const RID &p_rid, int p_idx) {
	ERR_FAIL_COND(p_idx < 0);

	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	int item_start = _get_system_menu_start(md->menu);
	int item_count = _get_system_menu_count(md->menu);
	p_idx += item_start;
	ERR_FAIL_COND(p_idx >= item_start + item_count);
	NSMenuItem *menu_item = [md->menu itemAtIndex:p_idx];
	if ([menu_item submenu] && _is_menu_opened([menu_item submenu])) {
		ERR_PRINT("Can't remove open menu!");
		return;
	}
	[md->menu removeItemAtIndex:p_idx];
}

void NativeMenuMacOS::clear(const RID &p_rid) {
	MenuData *md = menus.get_or_null(p_rid);
	ERR_FAIL_NULL(md);
	ERR_FAIL_COND_MSG(_is_menu_opened(md->menu), "Can't remove open menu!");

	if (p_rid == application_menu || p_rid == window_menu || p_rid == help_menu) {
		int start = _get_system_menu_start(md->menu);
		int count = _get_system_menu_count(md->menu);
		for (int i = start + count - 1; i >= start; i--) {
			[md->menu removeItemAtIndex:i];
		}
	} else {
		[md->menu removeAllItems];
	}

	if (p_rid == main_menu) {
		// Restore Apple, Window and Help menu.
		MenuData *md_app = menus.get_or_null(application_menu);
		if (md_app) {
			NSMenuItem *menu_item = [md->menu addItemWithTitle:@"" action:nil keyEquivalent:@""];
			[md->menu setSubmenu:md_app->menu forItem:menu_item];
		}
		MenuData *md_win = menus.get_or_null(window_menu);
		if (md_win) {
			NSMenuItem *menu_item = [md->menu addItemWithTitle:@"Window" action:nil keyEquivalent:@""];
			[md->menu setSubmenu:md_win->menu forItem:menu_item];
		}
		MenuData *md_hlp = menus.get_or_null(help_menu);
		if (md_hlp) {
			NSMenuItem *menu_item = [md->menu addItemWithTitle:@"Help" action:nil keyEquivalent:@""];
			[md->menu setSubmenu:md_hlp->menu forItem:menu_item];
		}
	}
}

NativeMenuMacOS::NativeMenuMacOS() {}

NativeMenuMacOS::~NativeMenuMacOS() {
	if (main_menu.is_valid()) {
		MenuData *md = menus.get_or_null(main_menu);
		if (md) {
			clear(main_menu);
			menus.free(main_menu);
			menu_lookup.erase(md->menu);
			md->menu = nullptr;
			main_menu_ns = nullptr;
			memdelete(md);
		}
	}
	if (application_menu.is_valid()) {
		MenuData *md = menus.get_or_null(application_menu);
		if (md) {
			clear(application_menu);
			menus.free(application_menu);
			menu_lookup.erase(md->menu);
			md->menu = nullptr;
			application_menu_ns = nullptr;
			memdelete(md);
		}
	}
	if (window_menu.is_valid()) {
		MenuData *md = menus.get_or_null(window_menu);
		if (md) {
			clear(window_menu);
			menus.free(window_menu);
			menu_lookup.erase(md->menu);
			md->menu = nullptr;
			window_menu_ns = nullptr;
			memdelete(md);
		}
	}
	if (help_menu.is_valid()) {
		MenuData *md = menus.get_or_null(help_menu);
		if (md) {
			clear(help_menu);
			menus.free(help_menu);
			menu_lookup.erase(md->menu);
			md->menu = nullptr;
			help_menu_ns = nullptr;
			memdelete(md);
		}
	}
	if (dock_menu.is_valid()) {
		MenuData *md = menus.get_or_null(dock_menu);
		if (md) {
			clear(dock_menu);
			menus.free(dock_menu);
			menu_lookup.erase(md->menu);
			md->menu = nullptr;
			dock_menu_ns = nullptr;
			memdelete(md);
		}
	}
}
