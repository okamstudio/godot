/**************************************************************************/
/*  godot_share.mm                                                        */
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

#include "godot_share.h"

#include <TargetConditionals.h>

#import <StoreKit/StoreKit.h>

GodotShare *GodotShare::singleton = nullptr;

GodotShare::GodotShare() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;
}

GodotShare::~GodotShare() {
	singleton = nullptr;
}

void GodotShare::share(const String &p_text, const String &p_subject, const String &p_title, const String &p_path) {
#if TARGET_OS_IPHONE
	UIViewController *root_controller = [[UIApplication sharedApplication] delegate].window.rootViewController;

	NSString *ns_text = [NSString stringWithCString:p_text.utf8().get_data() encoding:NSUTF8StringEncoding];
	NSMutableArray *share_items = [@[ ns_text ] mutableCopy];
	if (p_path.is_empty() == false) {
		NSString *image_path = [NSString stringWithCString:p_path.utf8().get_data() encoding:NSUTF8StringEncoding];
		UIImage *ui_image = [UIImage imageWithContentsOfFile:image_path];
		[share_items addObject:ui_image];
	}

	UIActivityViewController *avc = [[UIActivityViewController alloc] initWithActivityItems:share_items applicationActivities:nil];
	if (p_subject.is_empty() == false) {
		NSString *ns_subject = [NSString stringWithCString:p_subject.utf8().get_data() encoding:NSUTF8StringEncoding];
		[avc setValue:ns_subject forKey:@"subject"];
	}
	//if iPad
	if (UI_USER_INTERFACE_IDIOM() != UIUserInterfaceIdiomPhone) {
		// Change Rect to position Popover
		avc.modalPresentationStyle = UIModalPresentationPopover;
		avc.popoverPresentationController.sourceView = root_controller.view;
		avc.popoverPresentationController.sourceRect = CGRectMake(CGRectGetMidX(root_controller.view.bounds), CGRectGetMidY(root_controller.view.bounds), 0, 0);
		avc.popoverPresentationController.permittedArrowDirections = UIPopoverArrowDirection(0);
	}
	[root_controller presentViewController:avc animated:YES completion:nil];
#else
#endif
}

void GodotShare::rate() {
#if TARGET_OS_IPHONE
	/* SKStoreReviewController replaced byAppStore.requestReview(in: scene)
	if (@available(iOS 14.0, *)) {
		UIViewController *root_controller = [[UIApplication sharedApplication] delegate].window.rootViewController;
		[SKStoreReviewController requestReviewInScene:root_controller.view.window.windowScene];
	} else if (@available(iOS 10.3, *)) {
		[SKStoreReviewController requestReview];
	}
	*/
#elif TARGET_OS_OSX
	/* linker error
	   https://developer.apple.com/documentation/storekit/skstorereviewcontroller
	if (@available(macos 10.14, *)) {
		[SKStoreReviewController requestReview];
	}
	*/
	NSString *appId = @"id6446126962"; // Your app Id from the Itunes Connect portal

	NSURL *url = [NSURL URLWithString:[NSString stringWithFormat:@"https://apps.apple.com/app/%@?action=write-review", appId]];
	if (url) {
		[[NSWorkspace sharedWorkspace] openURL:url];
	}
#endif
}

void GodotShare::_bind_methods() {
	ClassDB::bind_method(D_METHOD("share", "text", "subject", "title", "path"), &GodotShare::share);
	ClassDB::bind_method(D_METHOD("rate"), &GodotShare::rate);
	ADD_SIGNAL(MethodInfo("rate_success"));
	ADD_SIGNAL(MethodInfo("rate_failed", PropertyInfo(Variant::STRING, "message")));
}
