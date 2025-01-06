/**************************************************************************/
/*  godot_main_macos.mm                                                   */
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

#import "os_macos.h"

#include "main/main.h"

#include <string.h>
#include <unistd.h>

#if defined(SANITIZERS_ENABLED)
#include <sys/resource.h>
#endif

int main(int argc, char **argv) {
#if defined(VULKAN_ENABLED)
	setenv("MVK_CONFIG_FULL_IMAGE_VIEW_SWIZZLE", "1", 1); // MoltenVK - enable full component swizzling support.
	setenv("MVK_CONFIG_SWAPCHAIN_MIN_MAG_FILTER_USE_NEAREST", "0", 1); // MoltenVK - use linear surface scaling. TODO: remove when full DPI scaling is implemented.
#endif

#if defined(SANITIZERS_ENABLED)
	// Note: Set stack size to be at least 30 MB (vs 8 MB default) to avoid overflow, address sanitizer can increase stack usage up to 3 times.
	struct rlimit stack_lim = { 0x1E00000, 0x1E00000 };
	setrlimit(RLIMIT_STACK, &stack_lim);
#endif

	int first_arg = 1;
	const char *dbg_arg = "-NSDocumentRevisionsDebugMode";
	for (int i = 0; i < argc; i++) {
		if (strcmp(dbg_arg, argv[i]) == 0) {
			first_arg = i + 2;
		}
	}

	OS_MacOS os;
	Error err;

	// We must override main when testing is enabled.
	TEST_MAIN_OVERRIDE

	@autoreleasepool {
		err = Main::setup(argv[0], argc - first_arg, &argv[first_arg]);
	}

	if (err != OK) {
		if (err == ERR_HELP) { // Returned by --help and --version, so success.
			return EXIT_SUCCESS;
		}
		return EXIT_FAILURE;
	}

	int ret;
	@autoreleasepool {
		ret = Main::start();
	}
	if (ret == EXIT_SUCCESS) {
		os.run();
	} else {
		os.set_exit_code(EXIT_FAILURE);
	}

	@autoreleasepool {
		Main::cleanup();
	}

	return os.get_exit_code();
}
