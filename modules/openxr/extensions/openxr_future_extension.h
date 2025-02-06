/**************************************************************************/
/*  openxr_future_extension.h                                             */
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

#ifndef OPENXR_FUTURE_EXTENSION_H
#define OPENXR_FUTURE_EXTENSION_H

/*
	The OpenXR future extension forms the basis of OpenXRs ability to
	execute logic asynchronously.

	Asynchronous functions will return a future object which can be
	polled each frame to determine if the asynchronous function has
	been completed.

	If so the future can be used to obtain final return values.
	The API call for this is often part of the extension that utilizes
	the future.

	We will be using Godot Callables to drive responses on futures.
*/

#include "../util.h"
#include "core/templates/hash_map.h"
#include "core/variant/callable.h"
#include "core/variant/variant.h"
#include "openxr_extension_wrapper.h"

#include <openxr/openxr.h>

class OpenXRFutureExtension : public OpenXRExtensionWrapper {
	GDCLASS(OpenXRFutureExtension, OpenXRExtensionWrapper);

protected:
	static void _bind_methods();

public:
	static OpenXRFutureExtension *get_singleton();

	OpenXRFutureExtension();
	virtual ~OpenXRFutureExtension() override;

	virtual HashMap<String, bool *> get_requested_extensions() override;

	virtual void on_instance_created(const XrInstance p_instance) override;
	virtual void on_instance_destroyed() override;
	virtual void on_session_destroyed() override;

	virtual void on_process() override;

	bool get_active() const;

	void register_future(uint64_t p_future, Callable p_callable);
	void cancel_future(uint64_t p_future);

private:
	static OpenXRFutureExtension *singleton;

	bool future_ext = false;

	HashMap<uint64_t, Callable> futures;

	// OpenXR API call wrappers

	// Futures
	EXT_PROTO_XRRESULT_FUNC3(xrPollFutureEXT, (XrInstance), instance, (const XrFuturePollInfoEXT *), poll_info, (XrFuturePollResultEXT *), poll_result);
	EXT_PROTO_XRRESULT_FUNC2(xrCancelFutureEXT, (XrInstance), instance, (const XrFutureCancelInfoEXT *), cancel_info);
};

#endif // OPENXR_FUTURE_EXTENSION_H
