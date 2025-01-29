/**************************************************************************/
/*  godot_open_save_delegate.h                                            */
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

#ifndef GODOT_OPEN_SAVE_DELEGATE_H
#define GODOT_OPEN_SAVE_DELEGATE_H

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/templates/hash_map.h"
#include "core/variant/typed_array.h"
#include "core/variant/variant.h"

@interface GodotOpenSaveDelegate : NSObject <NSOpenSavePanelDelegate> {
	NSSavePanel *dialog;
	NSMutableArray *allowed_types;

	HashMap<int, String> ctr_ids;
	Dictionary options;
	int cur_index;
	int ctr_id;

	String root;
}

- (void)makeAccessoryView:(NSSavePanel *)p_panel filters:(const Vector<String> &)p_filters options:(const TypedArray<Dictionary> &)p_options;
- (void)setFileTypes:(NSMutableArray *)p_allowed_types;
- (void)popupOptionAction:(id)p_sender;
- (void)popupCheckAction:(id)p_sender;
- (void)popupFileAction:(id)p_sender;
- (int)getIndex;
- (Dictionary)getSelection;
- (int)setDefaultInt:(const String &)p_name value:(int)p_value;
- (int)setDefaultBool:(const String &)p_name value:(bool)p_value;
- (void)setRootPath:(const String &)p_root_path;

@end

#endif // GODOT_OPEN_SAVE_DELEGATE_H
