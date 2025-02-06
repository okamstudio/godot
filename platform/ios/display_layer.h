/**************************************************************************/
/*  display_layer.h                                                       */
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

#import "godot_vision_view.h"
#import <OpenGLES/EAGLDrawable.h>
#import <QuartzCore/QuartzCore.h>

@protocol DisplayLayer <NSObject>

- (void)startRenderDisplayLayer;
- (void)stopRenderDisplayLayer;
- (void)initializeDisplayLayer;
- (void)layoutDisplayLayer;

@end

#if defined (VISIONOS)
@interface GodotMetalLayer : GodotView <DisplayLayer>
@end
#else
// An ugly workaround for iOS simulator
#if defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
#if defined(__IPHONE_13_0)
API_AVAILABLE(ios(13.0))
@interface GodotMetalLayer : CAMetalLayer <DisplayLayer>
#else
@interface GodotMetalLayer : CALayer <DisplayLayer>
#endif
#else
@interface GodotMetalLayer : CAMetalLayer <DisplayLayer>
#endif
@end
#if defined(OPENGL_DISABLED)
#else
API_DEPRECATED("OpenGLES is deprecated", ios(2.0, 12.0))
@interface GodotOpenGLLayer : CAEAGLLayer <DisplayLayer>
@end
#endif
#endif