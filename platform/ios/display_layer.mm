/**************************************************************************/
/*  display_layer.mm                                                      */
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

#import "display_layer.h"

#import "display_server_ios.h"
#import "os_ios.h"

#include "core/config/project_settings.h"
#include "core/os/keyboard.h"
#include "main/main.h"
#include "servers/audio_server.h"

#import <AudioToolbox/AudioServices.h>
#import <GameController/GameController.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>

@implementation GodotMetalLayer

- (void)initializeDisplayLayer {
#if defined(TARGET_OS_SIMULATOR) && TARGET_OS_SIMULATOR
	if (@available(iOS 13, *)) {
		// Simulator supports Metal since iOS 13
	} else {
		NSLog(@"iOS Simulator prior to iOS 13 does not support Metal rendering.");
	}
#endif
}

- (void)layoutDisplayLayer {
}

- (void)startRenderDisplayLayer {
}

- (void)stopRenderDisplayLayer {
}

@end
#if !defined(OPENGL_DISABLED)
@implementation GodotOpenGLLayer {
	// The pixel dimensions of the backbuffer
	GLint backingWidth;
	GLint backingHeight;

	EAGLContext *context;
	GLuint viewRenderbuffer, viewFramebuffer;
	GLuint depthRenderbuffer;
}

- (void)initializeDisplayLayer {
	// Configure it so that it is opaque, does not retain the contents of the backbuffer when displayed, and uses RGBA8888 color.
	self.opaque = YES;
	self.drawableProperties = [NSDictionary
			dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:FALSE],
			kEAGLDrawablePropertyRetainedBacking,
			kEAGLColorFormatRGBA8,
			kEAGLDrawablePropertyColorFormat,
			nil];

	// Create GL ES 3 context
	if (GLOBAL_GET("rendering/renderer/rendering_method") == "gl_compatibility") {
		context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES3];
		NSLog(@"Setting up an OpenGL ES 3.0 context.");
		if (!context) {
			NSLog(@"Failed to create OpenGL ES 3.0 context!");
			return;
		}
	}

	if (![EAGLContext setCurrentContext:context]) {
		NSLog(@"Failed to set EAGLContext!");
		return;
	}
	if (![self createFramebuffer]) {
		NSLog(@"Failed to create frame buffer!");
		return;
	}
}

- (void)layoutDisplayLayer {
	[EAGLContext setCurrentContext:context];
	[self destroyFramebuffer];
	[self createFramebuffer];
}

- (void)startRenderDisplayLayer {
	[EAGLContext setCurrentContext:context];

	glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
}

- (void)stopRenderDisplayLayer {
	glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
	[context presentRenderbuffer:GL_RENDERBUFFER_OES];

#ifdef DEBUG_ENABLED
	GLenum err = glGetError();
	if (err) {
		NSLog(@"DrawView: %x error", err);
	}
#endif
}

- (void)dealloc {
	if ([EAGLContext currentContext] == context) {
		[EAGLContext setCurrentContext:nil];
	}

	if (context) {
		context = nil;
	}
}

- (BOOL)createFramebuffer {
	glGenFramebuffersOES(1, &viewFramebuffer);
	glGenRenderbuffersOES(1, &viewRenderbuffer);

	glBindFramebufferOES(GL_FRAMEBUFFER_OES, viewFramebuffer);
	glBindRenderbufferOES(GL_RENDERBUFFER_OES, viewRenderbuffer);
	// This call associates the storage for the current render buffer with the EAGLDrawable (our CAself)
	// allowing us to draw into a buffer that will later be rendered to screen wherever the layer is (which corresponds with our view).
	[context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(id<EAGLDrawable>)self];
	glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, viewRenderbuffer);

	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &backingWidth);
	glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &backingHeight);

	// For this sample, we also need a depth buffer, so we'll create and attach one via another renderbuffer.
	glGenRenderbuffersOES(1, &depthRenderbuffer);
	glBindRenderbufferOES(GL_RENDERBUFFER_OES, depthRenderbuffer);
	glRenderbufferStorageOES(GL_RENDERBUFFER_OES, GL_DEPTH_COMPONENT16_OES, backingWidth, backingHeight);
	glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_DEPTH_ATTACHMENT_OES, GL_RENDERBUFFER_OES, depthRenderbuffer);

	if (glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES) != GL_FRAMEBUFFER_COMPLETE_OES) {
		NSLog(@"failed to make complete framebuffer object %x", glCheckFramebufferStatusOES(GL_FRAMEBUFFER_OES));
		return NO;
	}

	GLES3::TextureStorage::system_fbo = viewFramebuffer;

	return YES;
}

// Clean up any buffers we have allocated.
- (void)destroyFramebuffer {
	GLES3::TextureStorage::system_fbo = 0;

	glDeleteFramebuffersOES(1, &viewFramebuffer);
	viewFramebuffer = 0;
	glDeleteRenderbuffersOES(1, &viewRenderbuffer);
	viewRenderbuffer = 0;

	if (depthRenderbuffer) {
		glDeleteRenderbuffersOES(1, &depthRenderbuffer);
		depthRenderbuffer = 0;
	}
}

@end
#endif // !defined(OPENGL_DISABLED)