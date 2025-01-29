/**************************************************************************/
/*  godot_open_save_delegate.mm                                           */
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

#include "godot_open_save_delegate.h"

@implementation GodotOpenSaveDelegate

- (instancetype)init {
	self = [super init];
	if ((self = [super init])) {
		dialog = nullptr;
		cur_index = 0;
		ctr_id = 1;
		allowed_types = nullptr;
		root = String();
	}
	return self;
}

- (void)makeAccessoryView:(NSSavePanel *)p_panel filters:(const Vector<String> &)p_filters options:(const TypedArray<Dictionary> &)p_options {
	dialog = p_panel;

	NSMutableArray *constraints = [NSMutableArray array];

	NSView *base_view = [[NSView alloc] initWithFrame:NSZeroRect];
	base_view.translatesAutoresizingMaskIntoConstraints = NO;

	NSGridView *view = [NSGridView gridViewWithNumberOfColumns:2 rows:0];
	view.translatesAutoresizingMaskIntoConstraints = NO;
	view.columnSpacing = 10;
	view.rowSpacing = 10;
	view.rowAlignment = NSGridRowAlignmentLastBaseline;

	int option_count = 0;

	for (int i = 0; i < p_options.size(); i++) {
		const Dictionary &item = p_options[i];
		if (!item.has("name") || !item.has("values") || !item.has("default")) {
			continue;
		}
		const String &name = item["name"];
		const Vector<String> &values = item["values"];
		int default_idx = item["default"];

		NSTextField *label = [NSTextField labelWithString:[NSString stringWithUTF8String:name.utf8().get_data()]];
		if (@available(macOS 10.14, *)) {
			label.textColor = NSColor.secondaryLabelColor;
		}
		if (@available(macOS 11.10, *)) {
			label.font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
		}

		NSView *popup = nullptr;
		if (values.is_empty()) {
			NSButton *popup_check = [NSButton checkboxWithTitle:@"" target:self action:@selector(popupCheckAction:)];
			int tag = [self setDefaultBool:name value:(bool)default_idx];
			popup_check.state = (default_idx) ? NSControlStateValueOn : NSControlStateValueOff;
			popup_check.tag = tag;
			popup = popup_check;
		} else {
			NSPopUpButton *popup_list = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
			for (int i = 0; i < values.size(); i++) {
				[popup_list addItemWithTitle:[NSString stringWithUTF8String:values[i].utf8().get_data()]];
			}
			int tag = [self setDefaultInt:name value:default_idx];
			[popup_list selectItemAtIndex:default_idx];
			popup_list.tag = tag;
			popup_list.target = self;
			popup_list.action = @selector(popupOptionAction:);
			popup = popup_list;
		}

		[view addRowWithViews:[NSArray arrayWithObjects:label, popup, nil]];

		option_count++;
	}

	NSMutableArray *new_allowed_types = [[NSMutableArray alloc] init];
	bool has_type_popup = false;
	{
		NSTextField *label = [NSTextField labelWithString:[NSString stringWithUTF8String:RTR("Format").utf8().get_data()]];
		if (@available(macOS 10.14, *)) {
			label.textColor = NSColor.secondaryLabelColor;
		}
		if (@available(macOS 11.10, *)) {
			label.font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
		}

		if (p_filters.size() > 1) {
			NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
			for (int i = 0; i < p_filters.size(); i++) {
				Vector<String> tokens = p_filters[i].split(";");
				if (tokens.size() >= 1) {
					String flt = tokens[0].strip_edges();
					String mime = (tokens.size() >= 2) ? tokens[2].strip_edges() : String();
					int filter_slice_count = flt.get_slice_count(",");

					NSMutableArray *type_filters = [[NSMutableArray alloc] init];
					for (int j = 0; j < filter_slice_count; j++) {
						String str = (flt.get_slice(",", j).strip_edges());
						if (!str.is_empty()) {
							if (@available(macOS 11, *)) {
								UTType *ut = nullptr;
								if (str == "*.*") {
									ut = UTTypeData;
								} else {
									ut = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:str.replace("*.", "").strip_edges().utf8().get_data()]];
								}
								if (ut) {
									[type_filters addObject:ut];
								}
							} else {
								[type_filters addObject:[NSString stringWithUTF8String:str.replace("*.", "").strip_edges().utf8().get_data()]];
							}
						}
					}

					if (@available(macOS 11, *)) {
						filter_slice_count = mime.get_slice_count(",");
						for (int j = 0; j < filter_slice_count; j++) {
							String str = mime.get_slicec(',', j).strip_edges();
							if (!str.is_empty()) {
								UTType *ut = [UTType typeWithMIMEType:[NSString stringWithUTF8String:str.strip_edges().utf8().get_data()]];
								if (ut) {
									[type_filters addObject:ut];
								}
							}
						}
					}

					if ([type_filters count] > 0) {
						NSString *name_str = [NSString stringWithUTF8String:((tokens.size() == 1) ? tokens[0] : tokens[1].strip_edges()).utf8().get_data()];
						[new_allowed_types addObject:type_filters];
						[popup addItemWithTitle:name_str];
					}
				}
			}
			if (popup.numberOfItems > 1) {
				has_type_popup = true;
				popup.target = self;
				popup.action = @selector(popupFileAction:);

				[view addRowWithViews:[NSArray arrayWithObjects:label, popup, nil]];
			}
		} else if (p_filters.size() == 1) {
			Vector<String> tokens = p_filters[0].split(";");
			if (tokens.size() >= 1) {
				String flt = tokens[0].strip_edges();
				String mime = (tokens.size() >= 2) ? tokens[2] : String();
				int filter_slice_count = flt.get_slice_count(",");

				NSMutableArray *type_filters = [[NSMutableArray alloc] init];
				for (int j = 0; j < filter_slice_count; j++) {
					String str = (flt.get_slice(",", j).strip_edges());
					if (!str.is_empty()) {
						if (@available(macOS 11, *)) {
							UTType *ut = nullptr;
							if (str == "*.*") {
								ut = UTTypeData;
							} else {
								ut = [UTType typeWithFilenameExtension:[NSString stringWithUTF8String:str.replace("*.", "").strip_edges().utf8().get_data()]];
							}
							if (ut) {
								[type_filters addObject:ut];
							}
						} else {
							[type_filters addObject:[NSString stringWithUTF8String:str.replace("*.", "").strip_edges().utf8().get_data()]];
						}
					}
				}
				if (@available(macOS 11, *)) {
					filter_slice_count = mime.get_slice_count(",");
					for (int j = 0; j < filter_slice_count; j++) {
						String str = mime.get_slicec(',', j).strip_edges();
						if (!str.is_empty()) {
							UTType *ut = [UTType typeWithMIMEType:[NSString stringWithUTF8String:str.strip_edges().utf8().get_data()]];
							if (ut) {
								[type_filters addObject:ut];
							}
						}
					}
				}

				if ([type_filters count] > 0) {
					[new_allowed_types addObject:type_filters];
				}
			}
		}
		[self setFileTypes:new_allowed_types];
	}

	[base_view addSubview:view];
	[constraints addObject:[view.topAnchor constraintEqualToAnchor:base_view.topAnchor constant:10]];
	[constraints addObject:[base_view.bottomAnchor constraintEqualToAnchor:view.bottomAnchor constant:10]];
	[constraints addObject:[base_view.centerXAnchor constraintEqualToAnchor:view.centerXAnchor constant:10]];
	[NSLayoutConstraint activateConstraints:constraints];

	if (option_count > 0 || has_type_popup) {
		[p_panel setAccessoryView:base_view];
	}
	if ([new_allowed_types count] > 0) {
		NSMutableArray *type_filters = [new_allowed_types objectAtIndex:0];
		if (@available(macOS 11, *)) {
			if (type_filters && [type_filters count] == 1 && [type_filters objectAtIndex:0] == UTTypeData) {
				[p_panel setAllowedContentTypes:@[ UTTypeData ]];
				[p_panel setAllowsOtherFileTypes:true];
			} else {
				[p_panel setAllowsOtherFileTypes:false];
				[p_panel setAllowedContentTypes:type_filters];
			}
		} else {
			if (type_filters && [type_filters count] == 1 && [[type_filters objectAtIndex:0] isEqualToString:@"*"]) {
				[p_panel setAllowedFileTypes:nil];
				[p_panel setAllowsOtherFileTypes:true];
			} else {
				[p_panel setAllowsOtherFileTypes:false];
				[p_panel setAllowedFileTypes:type_filters];
			}
		}
	} else {
		if (@available(macOS 11, *)) {
			[p_panel setAllowedContentTypes:@[ UTTypeData ]];
		} else {
			[p_panel setAllowedFileTypes:nil];
		}
		[p_panel setAllowsOtherFileTypes:true];
	}
}

- (int)getIndex {
	return cur_index;
}

- (Dictionary)getSelection {
	return options;
}

- (int)setDefaultInt:(const String &)p_name value:(int)p_value {
	int cid = ctr_id++;
	options[p_name] = p_value;
	ctr_ids[cid] = p_name;

	return cid;
}

- (int)setDefaultBool:(const String &)p_name value:(bool)p_value {
	int cid = ctr_id++;
	options[p_name] = p_value;
	ctr_ids[cid] = p_name;

	return cid;
}

- (void)setFileTypes:(NSMutableArray *)p_allowed_types {
	allowed_types = p_allowed_types;
}

- (instancetype)initWithDialog:(NSSavePanel *)p_dialog {
	if ((self = [super init])) {
		dialog = p_dialog;
		cur_index = 0;
		ctr_id = 1;
		allowed_types = nullptr;
	}
	return self;
}

- (void)popupCheckAction:(id)p_sender {
	NSButton *btn = p_sender;
	if (btn && ctr_ids.has(btn.tag)) {
		options[ctr_ids[btn.tag]] = ([btn state] == NSControlStateValueOn);
	}
}

- (void)popupOptionAction:(id)p_sender {
	NSPopUpButton *btn = p_sender;
	if (btn && ctr_ids.has(btn.tag)) {
		options[ctr_ids[btn.tag]] = (int)[btn indexOfSelectedItem];
	}
}

- (void)popupFileAction:(id)p_sender {
	NSPopUpButton *btn = p_sender;
	if (btn) {
		NSUInteger index = [btn indexOfSelectedItem];
		if (allowed_types && index < [allowed_types count]) {
			NSMutableArray *type_filters = [allowed_types objectAtIndex:index];
			if (@available(macOS 11, *)) {
				if (type_filters && [type_filters count] == 1 && [type_filters objectAtIndex:0] == UTTypeData) {
					[dialog setAllowedContentTypes:@[ UTTypeData ]];
					[dialog setAllowsOtherFileTypes:true];
				} else {
					[dialog setAllowsOtherFileTypes:false];
					[dialog setAllowedContentTypes:type_filters];
				}
			} else {
				if (type_filters && [type_filters count] == 1 && [[type_filters objectAtIndex:0] isEqualToString:@"*"]) {
					[dialog setAllowedFileTypes:nil];
					[dialog setAllowsOtherFileTypes:true];
				} else {
					[dialog setAllowsOtherFileTypes:false];
					[dialog setAllowedFileTypes:type_filters];
				}
			}
			cur_index = index;
		} else {
			if (@available(macOS 11, *)) {
				[dialog setAllowedContentTypes:@[ UTTypeData ]];
			} else {
				[dialog setAllowedFileTypes:nil];
			}
			[dialog setAllowsOtherFileTypes:true];
			cur_index = -1;
		}
	}
}

- (void)setRootPath:(const String &)p_root_path {
	root = p_root_path;
}

- (BOOL)panel:(id)sender validateURL:(NSURL *)url error:(NSError *_Nullable *)outError {
	if (root.is_empty()) {
		return YES;
	}

	NSString *ns_path = url.URLByStandardizingPath.URLByResolvingSymlinksInPath.path;
	String path = String::utf8([ns_path UTF8String]).simplify_path();
	if (!path.begins_with(root.simplify_path())) {
		return NO;
	}

	return YES;
}

@end
