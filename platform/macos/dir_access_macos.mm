/**************************************************************************/
/*  dir_access_macos.mm                                                   */
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

#include "dir_access_macos.h"

#include "core/config/project_settings.h"

#if defined(UNIX_ENABLED)

#include <errno.h>

#import <AppKit/NSWorkspace.h>
#import <Foundation/Foundation.h>

String DirAccessMacOS::fix_unicode_name(const char *p_name) const {
	String fname;
	if (p_name != nullptr) {
		NSString *nsstr = [[NSString stringWithUTF8String:p_name] precomposedStringWithCanonicalMapping];
		fname.parse_utf8([nsstr UTF8String]);
	}

	return fname;
}

int DirAccessMacOS::get_drive_count() {
	NSArray *res_keys = [NSArray arrayWithObjects:NSURLVolumeURLKey, NSURLIsSystemImmutableKey, nil];
	NSArray *vols = [[NSFileManager defaultManager] mountedVolumeURLsIncludingResourceValuesForKeys:res_keys options:NSVolumeEnumerationSkipHiddenVolumes];

	return [vols count];
}

String DirAccessMacOS::get_drive(int p_drive) {
	NSArray *res_keys = [NSArray arrayWithObjects:NSURLVolumeURLKey, NSURLIsSystemImmutableKey, nil];
	NSArray *vols = [[NSFileManager defaultManager] mountedVolumeURLsIncludingResourceValuesForKeys:res_keys options:NSVolumeEnumerationSkipHiddenVolumes];
	int count = [vols count];

	ERR_FAIL_INDEX_V(p_drive, count, "");

	String volname;
	NSString *path = [vols[p_drive] path];

	volname.parse_utf8([path UTF8String]);

	return volname;
}

bool DirAccessMacOS::is_hidden(const String &p_name) {
	String f = get_current_dir().path_join(p_name);
	NSURL *url = [NSURL fileURLWithPath:@(f.utf8().get_data())];
	NSNumber *hidden = nil;
	if (![url getResourceValue:&hidden forKey:NSURLIsHiddenKey error:nil]) {
		return DirAccessUnix::is_hidden(p_name);
	}
	return [hidden boolValue];
}

bool DirAccessMacOS::is_case_sensitive(const String &p_path) const {
	String f = p_path;
	if (!f.is_absolute_path()) {
		f = get_current_dir().path_join(f);
	}
	f = fix_path(f);

	NSURL *url = [NSURL fileURLWithPath:@(f.utf8().get_data())];
	NSNumber *cs = nil;
	if (![url getResourceValue:&cs forKey:NSURLVolumeSupportsCaseSensitiveNamesKey error:nil]) {
		return false;
	}
	return [cs boolValue];
}

bool DirAccessMacOS::is_bundle(const String &p_file) const {
	String f = p_file;
	if (!f.is_absolute_path()) {
		f = get_current_dir().path_join(f);
	}
	f = fix_path(f);

	return [[NSWorkspace sharedWorkspace] isFilePackageAtPath:[NSString stringWithUTF8String:f.utf8().get_data()]];
}

#endif // UNIX_ENABLED
