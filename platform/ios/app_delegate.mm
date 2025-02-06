/**************************************************************************/
/*  app_delegate.mm                                                       */
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

#import "app_delegate.h"

#import "godot_view.h"
#import "godot_vision_view.h"
#import "os_ios.h"
#import "view_controller.h"
#import "vision_view_controller.h"

#include "core/config/project_settings.h"
#import "drivers/coreaudio/audio_driver_coreaudio.h"
#include "main/main.h"

#import <AVFoundation/AVFoundation.h>
#import <AudioToolbox/AudioServices.h>

#define kRenderingFrequency 60

extern int gargc;
extern char **gargv;

extern int ios_main(int, char **);
extern void ios_finish();

@implementation AppDelegate

enum {
	SESSION_CATEGORY_AMBIENT,
	SESSION_CATEGORY_MULTI_ROUTE,
	SESSION_CATEGORY_PLAY_AND_RECORD,
	SESSION_CATEGORY_PLAYBACK,
	SESSION_CATEGORY_RECORD,
	SESSION_CATEGORY_SOLO_AMBIENT
};

static ViewController *mainViewController = nil;
static GodotView *mainGodotView = nil;

+ (ViewController *)viewController {
	return mainViewController;
}

// + (GodotView *)godotView {
// 	return mainGodotView;
// }
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
	// TODO: might be required to make an early return, so app wouldn't crash because of timeout.
	// TODO: logo screen is not displayed while shaders are compiling
	// DummyViewController(Splash/LoadingViewController) -> setup -> GodotViewController
#if defined(VISIONOS)
	// CGRect windowBounds = CGRectMake(0, 0, VISIONOS_SCREEN_WIDTH, VISIONOS_SCREEN_HEIGHT);
	//Vision doesn't have the static main. So we don't get the parameters
	const char *arg0 = [[[NSBundle mainBundle] executablePath] UTF8String]; // Path to the running application
	const char *newArgv[] = { arg0 };
	gargc = sizeof(newArgv) / sizeof(newArgv[0]);
	gargv = (char **)newArgv;
	mainViewController = [ViewController alloc];
	[mainViewController loadView];
	mainGodotView = [mainViewController godotView];

#else
	CGRect windowBounds = [[UIScreen mainScreen] bounds];

	// Create a full-screen window
	self.window = [[UIWindow alloc] initWithFrame:windowBounds];
#endif


	int err = ios_main(gargc, gargv);

	if (err != 0) {
		// bail, things did not go very well for us, should probably output a message on screen with our error code...
		exit(0);
		return NO;
	}
#if !defined(VISIONOS)
	ViewController *viewController = [[ViewController alloc] init];
	viewController.godotView.useCADisplayLink = bool(GLOBAL_DEF("display.iOS/use_cadisplaylink", true)) ? YES : NO;
	viewController.godotView.renderingInterval = 1.0 / kRenderingFrequency;

	self.window.rootViewController = viewController;

	// Show the window
	[self.window makeKeyAndVisible];
	mainViewController = viewController;
#endif

	[[NSNotificationCenter defaultCenter]
			addObserver:self
			   selector:@selector(onAudioInterruption:)
				   name:AVAudioSessionInterruptionNotification
				 object:[AVAudioSession sharedInstance]];


	int sessionCategorySetting = GLOBAL_GET("audio/general/ios/session_category");

	// Initialize with default Ambient category.
	AVAudioSessionCategory category = AVAudioSessionCategoryAmbient;
	AVAudioSessionCategoryOptions options = 0;

	if (GLOBAL_GET("audio/general/ios/mix_with_others")) {
		options |= AVAudioSessionCategoryOptionMixWithOthers;
	}

	if (sessionCategorySetting == SESSION_CATEGORY_MULTI_ROUTE) {
		category = AVAudioSessionCategoryMultiRoute;
	} else if (sessionCategorySetting == SESSION_CATEGORY_PLAY_AND_RECORD) {
		category = AVAudioSessionCategoryPlayAndRecord;
		options |= AVAudioSessionCategoryOptionDefaultToSpeaker;
		options |= AVAudioSessionCategoryOptionAllowBluetoothA2DP;
		options |= AVAudioSessionCategoryOptionAllowAirPlay;
	} else if (sessionCategorySetting == SESSION_CATEGORY_PLAYBACK) {
		category = AVAudioSessionCategoryPlayback;
	} else if (sessionCategorySetting == SESSION_CATEGORY_RECORD) {
		category = AVAudioSessionCategoryRecord;
	} else if (sessionCategorySetting == SESSION_CATEGORY_SOLO_AMBIENT) {
		category = AVAudioSessionCategorySoloAmbient;
	}

	[[AVAudioSession sharedInstance] setCategory:category withOptions:options error:nil];

	return YES;
}

- (UISceneConfiguration *)application:(UIApplication *)application
		configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
									   options:(UISceneConnectionOptions *)options {
	UISceneConfiguration * config = [[UISceneConfiguration alloc] initWithName:nil sessionRole:connectingSceneSession.role];
	config.delegateClass = [self class];
	return config;
}

- (void)onAudioInterruption:(NSNotification *)notification {
	if ([notification.name isEqualToString:AVAudioSessionInterruptionNotification]) {
		if ([[notification.userInfo valueForKey:AVAudioSessionInterruptionTypeKey] isEqualToNumber:[NSNumber numberWithInt:AVAudioSessionInterruptionTypeBegan]]) {
			NSLog(@"Audio interruption began");
			OS_IOS::get_singleton()->on_focus_out();
		} else if ([[notification.userInfo valueForKey:AVAudioSessionInterruptionTypeKey] isEqualToNumber:[NSNumber numberWithInt:AVAudioSessionInterruptionTypeEnded]]) {
			NSLog(@"Audio interruption ended");
			OS_IOS::get_singleton()->on_focus_in();
		}
	}
}

- (void)applicationDidReceiveMemoryWarning:(UIApplication *)application {
	if (OS::get_singleton()->get_main_loop()) {
		OS::get_singleton()->get_main_loop()->notification(MainLoop::NOTIFICATION_OS_MEMORY_WARNING);
	}
}

- (void)applicationWillTerminate:(UIApplication *)application {
	ios_finish();
}

// When application goes to background (e.g. user switches to another app or presses Home),
// then applicationWillResignActive -> applicationDidEnterBackground are called.
// When user opens the inactive app again,
// applicationWillEnterForeground -> applicationDidBecomeActive are called.

// There are cases when applicationWillResignActive -> applicationDidBecomeActive
// sequence is called without the app going to background. For example, that happens
// if you open the app list without switching to another app or open/close the
// notification panel by swiping from the upper part of the screen.

- (void)applicationWillResignActive:(UIApplication *)application {
	OS_IOS::get_singleton()->on_focus_out();
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
	OS_IOS::get_singleton()->on_focus_in();
}
- (void)sceneDidBecomeActive:(UIScene *)scene {
	OS_IOS::get_singleton()->on_focus_in();
}
- (void)sceneWillResignActive:(UIScene *)scene {
	OS_IOS::get_singleton()->on_focus_out();
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
	OS_IOS::get_singleton()->on_enter_background();
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
	OS_IOS::get_singleton()->on_exit_background();
}

- (void)dealloc {
	self.window = nil;
}

@end
