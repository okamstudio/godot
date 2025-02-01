/**************************************************************************/
/*  steam_tracker.h                                                       */
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

#pragma once

#if defined(STEAMAPI_ENABLED)

#include "core/os/os.h"

// SteamTracker is used to load SteamAPI dynamic library and initialize
// the interface, this notifies Steam that Godot editor is running and
// allow tracking of the usage time of child instances of the engine
// (e.g., opened projects).
//
// Currently, SteamAPI is not used by the engine in any way, and is not
// exposed to the scripting APIs.

enum SteamAPIInitResult {
	SteamAPIInitResult_OK = 0,
	SteamAPIInitResult_FailedGeneric = 1,
	SteamAPIInitResult_NoSteamClient = 2,
	SteamAPIInitResult_VersionMismatch = 3,
};

// https://partner.steamgames.com/doc/api/steam_api#SteamAPI_Init
typedef bool (*SteamAPI_InitFunction)();
typedef SteamAPIInitResult (*SteamAPI_InitFlatFunction)(char *r_err_msg);

// https://partner.steamgames.com/doc/api/steam_api#SteamAPI_Shutdown
typedef void (*SteamAPI_ShutdownFunction)();

class SteamTracker {
	void *steam_library_handle = nullptr;
	SteamAPI_InitFunction steam_init_function = nullptr;
	SteamAPI_InitFlatFunction steam_init_flat_function = nullptr;
	SteamAPI_ShutdownFunction steam_shutdown_function = nullptr;
	bool steam_initialized = false;

public:
	SteamTracker();
	~SteamTracker();
};

#endif // STEAMAPI_ENABLED
