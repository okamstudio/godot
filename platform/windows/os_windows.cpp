/**************************************************************************/
/*  os_windows.cpp                                                        */
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

#include "os_windows.h"

#include "core/io/marshalls.h"
#include "core/math/geometry.h"
#include "core/version_generated.gen.h"
#include "drivers/gles2/rasterizer_gles2.h"
#include "drivers/gles3/rasterizer_gles3.h"
#include "drivers/unix/net_socket_posix.h"
#include "drivers/windows/dir_access_windows.h"
#include "drivers/windows/file_access_windows.h"
#include "joypad_windows.h"
#include "lang_table.h"
#include "main/main.h"
#include "servers/audio_server.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"
#include "windows_terminal_logger.h"

#include <avrt.h>
#include <direct.h>
#include <knownfolders.h>
#include <process.h>
#include <regstr.h>
#include <shlobj.h>

static const WORD MAX_CONSOLE_LINES = 1500;

extern "C" {
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

// Workaround mingw-w64 < 4.0 bug
#ifndef WM_TOUCH
#define WM_TOUCH 576
#endif

#if defined(__GNUC__)
// Workaround GCC warning from -Wcast-function-type.
#define GetProcAddress (void *)GetProcAddress
#endif

typedef struct {
	int count;
	int screen;
	HMONITOR monitor;
} EnumScreenData;

typedef struct {
	int count;
	int screen;
	Size2 size;
} EnumSizeData;

typedef struct {
	int count;
	int screen;
	Point2 pos;
} EnumPosData;

static BOOL CALLBACK _MonitorEnumProcScreen(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumScreenData *data = (EnumScreenData *)dwData;
	if (data->monitor == hMonitor) {
		data->screen = data->count;
	}

	data->count++;
	return TRUE;
}

static BOOL CALLBACK _MonitorEnumProcSize(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumSizeData *data = (EnumSizeData *)dwData;
	if (data->count == data->screen) {
		data->size.x = lprcMonitor->right - lprcMonitor->left;
		data->size.y = lprcMonitor->bottom - lprcMonitor->top;
	}

	data->count++;
	return TRUE;
}

static String format_error_message(DWORD id) {
	LPWSTR messageBuffer = NULL;
	size_t size = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

	String msg = "Error " + itos(id) + ": " + String(messageBuffer, size);

	LocalFree(messageBuffer);

	return msg;
}

extern HINSTANCE godot_hinstance;

void RedirectStream(const char *p_file_name, const char *p_mode, FILE *p_cpp_stream, const DWORD p_std_handle) {
	const HANDLE h_existing = GetStdHandle(p_std_handle);
	if (h_existing != INVALID_HANDLE_VALUE) { // Redirect only if attached console have a valid handle.
		const HANDLE h_cpp = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(p_cpp_stream)));
		if (h_cpp == INVALID_HANDLE_VALUE) { // Redirect only if it's not already redirected to the pipe or file.
			FILE *fp = p_cpp_stream;
			freopen_s(&fp, p_file_name, p_mode, p_cpp_stream); // Redirect stream.
			setvbuf(p_cpp_stream, nullptr, _IONBF, 0); // Disable stream buffering.
		}
	}
}

void RedirectIOToConsole() {
	if (AttachConsole(ATTACH_PARENT_PROCESS)) {
		RedirectStream("CONIN$", "r", stdin, STD_INPUT_HANDLE);
		RedirectStream("CONOUT$", "w", stdout, STD_OUTPUT_HANDLE);
		RedirectStream("CONOUT$", "w", stderr, STD_ERROR_HANDLE);

		printf("\n"); // Make sure our output is starting from the new line.
	}
}

BOOL WINAPI HandlerRoutine(_In_ DWORD dwCtrlType) {
	if (ScriptDebugger::get_singleton() == NULL)
		return FALSE;

	switch (dwCtrlType) {
		case CTRL_C_EVENT:
			ScriptDebugger::get_singleton()->set_depth(-1);
			ScriptDebugger::get_singleton()->set_lines_left(1);
			return TRUE;
		default:
			return FALSE;
	}
}

// WinTab API
bool OS_Windows::wintab_available = false;
WTOpenPtr OS_Windows::wintab_WTOpen = nullptr;
WTClosePtr OS_Windows::wintab_WTClose = nullptr;
WTInfoPtr OS_Windows::wintab_WTInfo = nullptr;
WTPacketPtr OS_Windows::wintab_WTPacket = nullptr;
WTEnablePtr OS_Windows::wintab_WTEnable = nullptr;

// Windows Ink API
bool OS_Windows::winink_available = false;
GetPointerTypePtr OS_Windows::win8p_GetPointerType = NULL;
GetPointerPenInfoPtr OS_Windows::win8p_GetPointerPenInfo = NULL;

void OS_Windows::initialize_debugging() {
	SetConsoleCtrlHandler(HandlerRoutine, TRUE);
}

void OS_Windows::initialize_core() {
	crash_handler.initialize();

	last_button_state = 0;
	restore_mouse_trails = 0;

#ifndef WINDOWS_SUBSYSTEM_CONSOLE
	RedirectIOToConsole();
#endif

	maximized = false;
	minimized = false;
	borderless = false;

	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_RESOURCES);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_USERDATA);
	FileAccess::make_default<FileAccessWindows>(FileAccess::ACCESS_FILESYSTEM);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_RESOURCES);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_USERDATA);
	DirAccess::make_default<DirAccessWindows>(DirAccess::ACCESS_FILESYSTEM);

	NetSocketPosix::make_default();

	// We need to know how often the clock is updated
	QueryPerformanceFrequency((LARGE_INTEGER *)&ticks_per_second);
	QueryPerformanceCounter((LARGE_INTEGER *)&ticks_start);

	// set minimum resolution for periodic timers, otherwise Sleep(n) may wait at least as
	//  long as the windows scheduler resolution (~16-30ms) even for calls like Sleep(1)
	timeBeginPeriod(1);

	process_map = memnew((Map<ProcessID, ProcessInfo>));

	// Add current Godot PID to the list of known PIDs
	ProcessInfo current_pi = {};
	PROCESS_INFORMATION current_pi_pi = {};
	current_pi.pi = current_pi_pi;
	current_pi.pi.hProcess = GetCurrentProcess();
	process_map->insert(GetCurrentProcessId(), current_pi);

	IP_Unix::make_default();

	cursor_shape = CURSOR_ARROW;
}

bool OS_Windows::can_draw() const {
	return !minimized;
};

#define MI_WP_SIGNATURE 0xFF515700
#define SIGNATURE_MASK 0xFFFFFF00
// Keeping the name suggested by Microsoft, but this macro really answers:
// Is this mouse event emulated from touch or pen input?
#define IsPenEvent(dw) (((dw)&SIGNATURE_MASK) == MI_WP_SIGNATURE)
// This one tells whether the event comes from touchscreen (and not from pen)
#define IsTouchEvent(dw) (IsPenEvent(dw) && ((dw)&0x80))

void OS_Windows::_touch_event(bool p_pressed, float p_x, float p_y, int idx) {
	// Defensive
	if (touch_state.has(idx) == p_pressed)
		return;

	if (p_pressed) {
		touch_state.insert(idx, Vector2(p_x, p_y));
	} else {
		touch_state.erase(idx);
	}

	Ref<InputEventScreenTouch> event;
	event.instance();
	event->set_index(idx);
	event->set_pressed(p_pressed);
	event->set_position(Vector2(p_x, p_y));

	if (main_loop) {
		input->parse_input_event(event);
	}
};

bool OS_Windows::tts_is_speaking() const {
	ERR_FAIL_COND_V_MSG(!tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	ERR_FAIL_COND_V(!tts, false);
	return tts->is_speaking();
}

bool OS_Windows::tts_is_paused() const {
	ERR_FAIL_COND_V_MSG(!tts, false, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	ERR_FAIL_COND_V(!tts, false);
	return tts->is_paused();
}

Array OS_Windows::tts_get_voices() const {
	ERR_FAIL_COND_V_MSG(!tts, Array(), "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	ERR_FAIL_COND_V(!tts, Array());
	return tts->get_voices();
}

void OS_Windows::tts_speak(const String &p_text, const String &p_voice, int p_volume, float p_pitch, float p_rate, int p_utterance_id, bool p_interrupt) {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	ERR_FAIL_COND(!tts);
	tts->speak(p_text, p_voice, p_volume, p_pitch, p_rate, p_utterance_id, p_interrupt);
}

void OS_Windows::tts_pause() {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	ERR_FAIL_COND(!tts);
	tts->pause();
}

void OS_Windows::tts_resume() {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	ERR_FAIL_COND(!tts);
	tts->resume();
}

void OS_Windows::tts_stop() {
	ERR_FAIL_COND_MSG(!tts, "Enable the \"audio/general/text_to_speech\" project setting to use text-to-speech.");
	ERR_FAIL_COND(!tts);
	tts->stop();
}

void OS_Windows::_drag_event(float p_x, float p_y, int idx) {
	Map<int, Vector2>::Element *curr = touch_state.find(idx);
	// Defensive
	if (!curr)
		return;

	if (curr->get() == Vector2(p_x, p_y))
		return;

	Ref<InputEventScreenDrag> event;
	event.instance();
	event->set_index(idx);
	event->set_position(Vector2(p_x, p_y));
	event->set_relative(Vector2(p_x, p_y) - curr->get());

	if (main_loop)
		input->parse_input_event(event);

	curr->get() = Vector2(p_x, p_y);
};

LRESULT OS_Windows::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (drop_events) {
		if (user_proc) {
			return CallWindowProcW(user_proc, hWnd, uMsg, wParam, lParam);
		} else {
			return DefWindowProcW(hWnd, uMsg, wParam, lParam);
		}
	};

	switch (uMsg) // Check For Windows Messages
	{
		case WM_SETFOCUS: {
			window_has_focus = true;

			// Restore mouse mode
			_set_mouse_mode_impl(mouse_mode);

			break;
		}
		case WM_KILLFOCUS: {
			window_has_focus = false;

			// Release capture unconditionally because it can be set due to dragging, in addition to captured mode
			ReleaseCapture();

			// Release every touch to avoid sticky points
			for (Map<int, Vector2>::Element *E = touch_state.front(); E; E = E->next()) {
				_touch_event(false, E->get().x, E->get().y, E->key());
			}
			touch_state.clear();

			break;
		}
		case WM_ACTIVATE: // Watch For Window Activate Message
		{
			minimized = HIWORD(wParam) != 0;
			if (!main_loop) {
				return 0;
			};
			if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE) {
				main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_IN);
				window_focused = true;
				alt_mem = false;
				control_mem = false;
				shift_mem = false;
				gr_mem = false;
			} else { // WM_INACTIVE
				input->release_pressed_events();
				main_loop->notification(MainLoop::NOTIFICATION_WM_FOCUS_OUT);
				window_focused = false;
				alt_mem = false;
			};

			if ((get_current_tablet_driver() == "wintab") && wintab_available && wtctx) {
				wintab_WTEnable(wtctx, GET_WM_ACTIVATE_STATE(wParam, lParam));
			}

			return 0; // Return  To The Message Loop
		}
		case WM_GETMINMAXINFO: {
			if (video_mode.resizable && !video_mode.fullscreen) {
				Size2 decor = get_real_window_size() - get_window_size(); // Size of window decorations
				MINMAXINFO *min_max_info = (MINMAXINFO *)lParam;
				if (min_size != Size2()) {
					min_max_info->ptMinTrackSize.x = min_size.x + decor.x;
					min_max_info->ptMinTrackSize.y = min_size.y + decor.y;
				}
				if (max_size != Size2()) {
					min_max_info->ptMaxTrackSize.x = max_size.x + decor.x;
					min_max_info->ptMaxTrackSize.y = max_size.y + decor.y;
				}
				return 0;
			} else {
				break;
			}
		}
		case WM_PAINT:

			Main::force_redraw();
			break;

		case WM_SYSCOMMAND: // Intercept System Commands
		{
			switch (wParam) // Check System Calls
			{
				case SC_SCREENSAVE: // Screensaver Trying To Start?
				case SC_MONITORPOWER: // Monitor Trying To Enter Powersave?
					return 0; // Prevent From Happening
				case SC_KEYMENU:
					if ((lParam >> 16) <= 0)
						return 0;
			}
			break; // Exit
		}

		case WM_CLOSE: // Did We Receive A Close Message?
		{
			if (main_loop)
				main_loop->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
			//force_quit=true;
			return 0; // Jump Back
		}
		case WM_MOUSELEAVE: {
			old_invalid = true;
			outside = true;
			if (main_loop && mouse_mode != MOUSE_MODE_CAPTURED)
				main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_EXIT);

		} break;
		case WM_INPUT: {
			if (mouse_mode != MOUSE_MODE_CAPTURED || !use_raw_input) {
				break;
			}

			UINT dwSize;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
			LPBYTE lpb = new BYTE[dwSize];
			if (lpb == NULL) {
				return 0;
			}

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
				OutputDebugString(TEXT("GetRawInputData does not return correct size !\n"));

			RAWINPUT *raw = (RAWINPUT *)lpb;

			if (raw->header.dwType == RIM_TYPEMOUSE) {
				Ref<InputEventMouseMotion> mm;
				mm.instance();

				mm->set_control(control_mem);
				mm->set_shift(shift_mem);
				mm->set_alt(alt_mem);

				mm->set_pressure((raw->data.mouse.ulButtons & RI_MOUSE_LEFT_BUTTON_DOWN) ? 1.0f : 0.0f);

				mm->set_button_mask(last_button_state);

				Point2i c(video_mode.width / 2, video_mode.height / 2);

				// centering just so it works as before
				POINT pos = { (int)c.x, (int)c.y };
				ClientToScreen(hWnd, &pos);
				SetCursorPos(pos.x, pos.y);

				mm->set_position(c);
				mm->set_global_position(c);
				mm->set_speed(Vector2(0, 0));

				if (raw->data.mouse.usFlags == MOUSE_MOVE_RELATIVE) {
					mm->set_relative(Vector2(raw->data.mouse.lLastX, raw->data.mouse.lLastY));

				} else if (raw->data.mouse.usFlags == MOUSE_MOVE_ABSOLUTE) {
					int nScreenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
					int nScreenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
					int nScreenLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
					int nScreenTop = GetSystemMetrics(SM_YVIRTUALSCREEN);

					Vector2 abs_pos(
							(double(raw->data.mouse.lLastX) - 65536.0 / (nScreenWidth)) * nScreenWidth / 65536.0 + nScreenLeft,
							(double(raw->data.mouse.lLastY) - 65536.0 / (nScreenHeight)) * nScreenHeight / 65536.0 + nScreenTop);

					POINT coords; //client coords
					coords.x = abs_pos.x;
					coords.y = abs_pos.y;

					ScreenToClient(hWnd, &coords);

					mm->set_relative(Vector2(coords.x - old_x, coords.y - old_y));
					old_x = coords.x;
					old_y = coords.y;

					/*Input.mi.dx = (int)((((double)(pos.x)-nScreenLeft) * 65536) / nScreenWidth + 65536 / (nScreenWidth));
					Input.mi.dy = (int)((((double)(pos.y)-nScreenTop) * 65536) / nScreenHeight + 65536 / (nScreenHeight));
					*/
				}

				if (window_has_focus && main_loop && mm->get_relative() != Vector2())
					input->parse_input_event(mm);
			}
			delete[] lpb;
		} break;
		case WT_CSRCHANGE:
		case WT_PROXIMITY: {
			if ((get_current_tablet_driver() == "wintab") && wintab_available && wtctx) {
				AXIS pressure;
				if (wintab_WTInfo(WTI_DEVICES + wtlc.lcDevice, DVC_NPRESSURE, &pressure)) {
					min_pressure = int(pressure.axMin);
					max_pressure = int(pressure.axMax);
				}
				AXIS orientation[3];
				if (wintab_WTInfo(WTI_DEVICES + wtlc.lcDevice, DVC_ORIENTATION, &orientation)) {
					tilt_supported = orientation[0].axResolution && orientation[1].axResolution;
				}
				return 0;
			}
		} break;
		case WT_PACKET: {
			if ((get_current_tablet_driver() == "wintab") && wintab_available && wtctx) {
				PACKET packet;
				if (wintab_WTPacket(wtctx, wParam, &packet)) {
					float pressure = float(packet.pkNormalPressure - min_pressure) / float(max_pressure - min_pressure);
					last_pressure = pressure;
					last_pressure_update = 0;

					double azim = (packet.pkOrientation.orAzimuth / 10.0f) * (Math_PI / 180);
					double alt = Math::tan((Math::abs(packet.pkOrientation.orAltitude / 10.0f)) * (Math_PI / 180));

					if (tilt_supported) {
						last_tilt = Vector2(Math::atan(Math::sin(azim) / alt), Math::atan(Math::cos(azim) / alt));
					} else {
						last_tilt = Vector2();
					}

					last_pen_inverted = packet.pkStatus & TPS_INVERT;

					POINT coords;
					GetCursorPos(&coords);
					ScreenToClient(hWnd, &coords);

					// Don't calculate relative mouse movement if we don't have focus in CAPTURED mode.
					if (!window_has_focus && mouse_mode == MOUSE_MODE_CAPTURED) {
						break;
					}

					Ref<InputEventMouseMotion> mm;
					mm.instance();
					mm->set_control(GetKeyState(VK_CONTROL) < 0);
					mm->set_shift(GetKeyState(VK_SHIFT) < 0);
					mm->set_alt(alt_mem);

					mm->set_pen_inverted(last_pen_inverted);
					mm->set_pressure(last_pressure);
					mm->set_tilt(last_tilt);

					mm->set_button_mask(last_button_state);

					mm->set_position(Vector2(coords.x, coords.y));
					mm->set_global_position(Vector2(coords.x, coords.y));

					if (mouse_mode == MOUSE_MODE_CAPTURED) {
						Point2i c(video_mode.width / 2, video_mode.height / 2);
						old_x = c.x;
						old_y = c.y;

						if (mm->get_position() == c) {
							center = c;
							return 0;
						}

						Point2i ncenter = mm->get_position();
						center = ncenter;
						POINT pos = { (int)c.x, (int)c.y };
						ClientToScreen(hWnd, &pos);
						SetCursorPos(pos.x, pos.y);
					}

					mm->set_speed(input->get_last_mouse_speed());

					if (old_invalid) {
						old_x = mm->get_position().x;
						old_y = mm->get_position().y;
						old_invalid = false;
					}

					mm->set_relative(Vector2(mm->get_position() - Vector2(old_x, old_y)));
					old_x = mm->get_position().x;
					old_y = mm->get_position().y;
					if (window_has_focus && main_loop)
						input->parse_input_event(mm);
				}
				return 0;
			}
		} break;
		case WM_POINTERENTER: {
			if (mouse_mode == MOUSE_MODE_CAPTURED && use_raw_input) {
				break;
			}

			if ((get_current_tablet_driver() != "winink") || !winink_available) {
				break;
			}

			uint32_t pointer_id = LOWORD(wParam);
			POINTER_INPUT_TYPE pointer_type = PT_POINTER;
			if (!win8p_GetPointerType(pointer_id, &pointer_type)) {
				break;
			}

			if (pointer_type != PT_PEN) {
				break;
			}

			block_mm = true;
			return 0;
		} break;
		case WM_POINTERLEAVE: {
			block_mm = false;
			return 0;
		} break;
		case WM_POINTERUPDATE: {
			if (mouse_mode == MOUSE_MODE_CAPTURED && use_raw_input) {
				break;
			}

			if ((get_current_tablet_driver() != "winink") || !winink_available) {
				break;
			}

			uint32_t pointer_id = LOWORD(wParam);
			POINTER_INPUT_TYPE pointer_type = PT_POINTER;
			if (!win8p_GetPointerType(pointer_id, &pointer_type)) {
				break;
			}

			if (pointer_type != PT_PEN) {
				break;
			}

			POINTER_PEN_INFO pen_info;
			if (!win8p_GetPointerPenInfo(pointer_id, &pen_info)) {
				break;
			}

			if (input->is_emulating_mouse_from_touch()) {
				// Universal translation enabled; ignore OS translation
				LPARAM extra = GetMessageExtraInfo();
				if (IsTouchEvent(extra)) {
					break;
				}
			}

			if (outside) {
				//mouse enter

				if (main_loop && mouse_mode != MOUSE_MODE_CAPTURED)
					main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_ENTER);

				CursorShape c = cursor_shape;
				cursor_shape = CURSOR_MAX;
				set_cursor_shape(c);
				outside = false;

				//Once-Off notification, must call again....
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				TrackMouseEvent(&tme);
			}

			// Don't calculate relative mouse movement if we don't have focus in CAPTURED mode.
			if (!window_has_focus && mouse_mode == MOUSE_MODE_CAPTURED) {
				break;
			}

			Ref<InputEventMouseMotion> mm;
			mm.instance();

			mm->set_pen_inverted(pen_info.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER));

			if (pen_info.penMask & PEN_MASK_PRESSURE) {
				mm->set_pressure((float)pen_info.pressure / 1024);
			} else {
				mm->set_pressure((HIWORD(wParam) & POINTER_MESSAGE_FLAG_FIRSTBUTTON) ? 1.0f : 0.0f);
			}
			if ((pen_info.penMask & PEN_MASK_TILT_X) && (pen_info.penMask & PEN_MASK_TILT_Y)) {
				mm->set_tilt(Vector2((float)pen_info.tiltX / 90, (float)pen_info.tiltY / 90));
			}

			mm->set_control(GetKeyState(VK_CONTROL) < 0);
			mm->set_shift(GetKeyState(VK_SHIFT) < 0);
			mm->set_alt(alt_mem);

			mm->set_button_mask(last_button_state);

			POINT coords; //client coords
			coords.x = GET_X_LPARAM(lParam);
			coords.y = GET_Y_LPARAM(lParam);

			ScreenToClient(hWnd, &coords);

			mm->set_position(Vector2(coords.x, coords.y));
			mm->set_global_position(Vector2(coords.x, coords.y));

			if (mouse_mode == MOUSE_MODE_CAPTURED) {
				Point2i c(video_mode.width / 2, video_mode.height / 2);
				old_x = c.x;
				old_y = c.y;

				if (mm->get_position() == c) {
					center = c;
					return 0;
				}

				Point2i ncenter = mm->get_position();
				center = ncenter;
				POINT pos = { (int)c.x, (int)c.y };
				ClientToScreen(hWnd, &pos);
				SetCursorPos(pos.x, pos.y);
			}

			mm->set_speed(input->get_last_mouse_speed());

			if (old_invalid) {
				old_x = mm->get_position().x;
				old_y = mm->get_position().y;
				old_invalid = false;
			}

			mm->set_relative(Vector2(mm->get_position() - Vector2(old_x, old_y)));
			old_x = mm->get_position().x;
			old_y = mm->get_position().y;
			if (window_has_focus && main_loop)
				input->parse_input_event(mm);
			return 0;
		} break;
		case WM_MOUSEMOVE: {
			if (block_mm) {
				break;
			}

			if (mouse_mode == MOUSE_MODE_CAPTURED && use_raw_input) {
				break;
			}

			if (input->is_emulating_mouse_from_touch()) {
				// Universal translation enabled; ignore OS translation
				LPARAM extra = GetMessageExtraInfo();
				if (IsTouchEvent(extra)) {
					break;
				}
			}

			if (outside) {
				//mouse enter

				if (main_loop && mouse_mode != MOUSE_MODE_CAPTURED)
					main_loop->notification(MainLoop::NOTIFICATION_WM_MOUSE_ENTER);

				CursorShape c = cursor_shape;
				cursor_shape = CURSOR_MAX;
				set_cursor_shape(c);
				outside = false;

				//Once-Off notification, must call again....
				TRACKMOUSEEVENT tme;
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;
				tme.dwHoverTime = HOVER_DEFAULT;
				TrackMouseEvent(&tme);
			}

			// Don't calculate relative mouse movement if we don't have focus in CAPTURED mode.
			if (!window_has_focus && mouse_mode == MOUSE_MODE_CAPTURED) {
				break;
			}

			Ref<InputEventMouseMotion> mm;
			mm.instance();

			mm->set_control((wParam & MK_CONTROL) != 0);
			mm->set_shift((wParam & MK_SHIFT) != 0);
			mm->set_alt(alt_mem);

			if ((get_current_tablet_driver() == "wintab") && wintab_available && wtctx) {
				// Note: WinTab sends both WT_PACKET and WM_xBUTTONDOWN/UP/MOUSEMOVE events, use mouse 1/0 pressure only when last_pressure was not update recently.
				if (last_pressure_update < 10) {
					last_pressure_update++;
				} else {
					last_tilt = Vector2();
					last_pressure = (wParam & MK_LBUTTON) ? 1.0f : 0.0f;
					last_pen_inverted = false;
				}
			} else {
				last_tilt = Vector2();
				last_pressure = (wParam & MK_LBUTTON) ? 1.0f : 0.0f;
				last_pen_inverted = false;
			}

			mm->set_pressure(last_pressure);
			mm->set_tilt(last_tilt);
			mm->set_pen_inverted(last_pen_inverted);

			mm->set_button_mask(last_button_state);

			mm->set_position(Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
			mm->set_global_position(Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));

			if (mouse_mode == MOUSE_MODE_CAPTURED) {
				Point2i c(video_mode.width / 2, video_mode.height / 2);
				old_x = c.x;
				old_y = c.y;

				if (mm->get_position() == c) {
					center = c;
					return 0;
				}

				Point2i ncenter = mm->get_position();
				center = ncenter;
				POINT pos = { (int)c.x, (int)c.y };
				ClientToScreen(hWnd, &pos);
				SetCursorPos(pos.x, pos.y);
			}

			mm->set_speed(input->get_last_mouse_speed());

			if (old_invalid) {
				old_x = mm->get_position().x;
				old_y = mm->get_position().y;
				old_invalid = false;
			}

			mm->set_relative(Vector2(mm->get_position() - Vector2(old_x, old_y)));
			old_x = mm->get_position().x;
			old_y = mm->get_position().y;
			if (window_has_focus && main_loop)
				input->parse_input_event(mm);

		} break;
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
			if (input->is_emulating_mouse_from_touch()) {
				// Universal translation enabled; ignore OS translations for left button
				LPARAM extra = GetMessageExtraInfo();
				if (IsTouchEvent(extra)) {
					break;
				}
			}
			FALLTHROUGH;
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
		case WM_LBUTTONDBLCLK:
		case WM_MBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
		case WM_XBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP: {
			Ref<InputEventMouseButton> mb;
			mb.instance();

			switch (uMsg) {
				case WM_LBUTTONDOWN: {
					mb->set_pressed(true);
					mb->set_button_index(1);
				} break;
				case WM_LBUTTONUP: {
					mb->set_pressed(false);
					mb->set_button_index(1);
				} break;
				case WM_MBUTTONDOWN: {
					mb->set_pressed(true);
					mb->set_button_index(3);
				} break;
				case WM_MBUTTONUP: {
					mb->set_pressed(false);
					mb->set_button_index(3);
				} break;
				case WM_RBUTTONDOWN: {
					mb->set_pressed(true);
					mb->set_button_index(2);
				} break;
				case WM_RBUTTONUP: {
					mb->set_pressed(false);
					mb->set_button_index(2);
				} break;
				case WM_LBUTTONDBLCLK: {
					mb->set_pressed(true);
					mb->set_button_index(1);
					mb->set_doubleclick(true);
				} break;
				case WM_RBUTTONDBLCLK: {
					mb->set_pressed(true);
					mb->set_button_index(2);
					mb->set_doubleclick(true);
				} break;
				case WM_MBUTTONDBLCLK: {
					mb->set_pressed(true);
					mb->set_button_index(3);
					mb->set_doubleclick(true);
				} break;
				case WM_MOUSEWHEEL: {
					mb->set_pressed(true);
					int motion = (short)HIWORD(wParam);
					if (!motion)
						return 0;

					if (motion > 0) {
						mb->set_button_index(BUTTON_WHEEL_UP);
					} else {
						mb->set_button_index(BUTTON_WHEEL_DOWN);
					}
					mb->set_factor(fabs((double)motion / (double)WHEEL_DELTA));
				} break;
				case WM_MOUSEHWHEEL: {
					mb->set_pressed(true);
					int motion = (short)HIWORD(wParam);
					if (!motion)
						return 0;

					if (motion < 0) {
						mb->set_button_index(BUTTON_WHEEL_LEFT);
					} else {
						mb->set_button_index(BUTTON_WHEEL_RIGHT);
					}
					mb->set_factor(fabs((double)motion / (double)WHEEL_DELTA));
				} break;
				case WM_XBUTTONDOWN: {
					mb->set_pressed(true);
					if (HIWORD(wParam) == XBUTTON1)
						mb->set_button_index(BUTTON_XBUTTON1);
					else
						mb->set_button_index(BUTTON_XBUTTON2);
				} break;
				case WM_XBUTTONUP: {
					mb->set_pressed(false);
					if (HIWORD(wParam) == XBUTTON1)
						mb->set_button_index(BUTTON_XBUTTON1);
					else
						mb->set_button_index(BUTTON_XBUTTON2);
				} break;
				case WM_XBUTTONDBLCLK: {
					mb->set_pressed(true);
					if (HIWORD(wParam) == XBUTTON1)
						mb->set_button_index(BUTTON_XBUTTON1);
					else
						mb->set_button_index(BUTTON_XBUTTON2);
					mb->set_doubleclick(true);
				} break;
				default: {
					return 0;
				}
			}

			mb->set_control((wParam & MK_CONTROL) != 0);
			mb->set_shift((wParam & MK_SHIFT) != 0);
			mb->set_alt(alt_mem);
			//mb->get_alt()=(wParam&MK_MENU)!=0;
			if (mb->is_pressed())
				last_button_state |= (1 << (mb->get_button_index() - 1));
			else
				last_button_state &= ~(1 << (mb->get_button_index() - 1));
			mb->set_button_mask(last_button_state);

			mb->set_position(Vector2(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));

			if (mouse_mode == MOUSE_MODE_CAPTURED && !use_raw_input) {
				mb->set_position(Vector2(old_x, old_y));
			}

			if (uMsg != WM_MOUSEWHEEL && uMsg != WM_MOUSEHWHEEL) {
				if (mb->is_pressed()) {
					if (++pressrc > 0 && mouse_mode != MOUSE_MODE_CAPTURED)
						SetCapture(hWnd);
				} else {
					if (--pressrc <= 0) {
						if (mouse_mode != MOUSE_MODE_CAPTURED) {
							ReleaseCapture();
						}
						pressrc = 0;
					}
				}
			} else {
				// for reasons unknown to mankind, wheel comes in screen coordinates
				POINT coords;
				coords.x = mb->get_position().x;
				coords.y = mb->get_position().y;

				ScreenToClient(hWnd, &coords);

				mb->set_position(Vector2(coords.x, coords.y));
			}

			mb->set_global_position(mb->get_position());

			if (main_loop) {
				input->parse_input_event(mb);
				if (mb->is_pressed() && mb->get_button_index() > 3 && mb->get_button_index() < 8) {
					//send release for mouse wheel
					Ref<InputEventMouseButton> mbd = mb->duplicate();
					last_button_state &= ~(1 << (mbd->get_button_index() - 1));
					mbd->set_button_mask(last_button_state);
					mbd->set_pressed(false);
					input->parse_input_event(mbd);
				}
			}
		} break;

		case WM_MOVE: {
			if (!IsIconic(hWnd)) {
				int x = LOWORD(lParam);
				int y = HIWORD(lParam);
				last_pos = Point2(x, y);
			}
		} break;

		case WM_SIZE: {
			// Ignore size when a SIZE_MINIMIZED event is triggered
			if (wParam != SIZE_MINIMIZED) {
				int window_w = LOWORD(lParam);
				int window_h = HIWORD(lParam);
				if (window_w > 0 && window_h > 0 && !preserve_window_size) {
					video_mode.width = window_w;
					video_mode.height = window_h;
				} else {
					preserve_window_size = false;
					set_window_size(Size2(video_mode.width, video_mode.height));
				}
			}

			if (wParam == SIZE_MAXIMIZED) {
				maximized = true;
				minimized = false;
			} else if (wParam == SIZE_MINIMIZED) {
				maximized = false;
				minimized = true;
			} else if (wParam == SIZE_RESTORED) {
				maximized = false;
				minimized = false;
			}
			//return 0;								// Jump Back
		} break;

		case WM_ENTERSIZEMOVE: {
			input->release_pressed_events();
			move_timer_id = SetTimer(hWnd, 1, USER_TIMER_MINIMUM, (TIMERPROC)NULL);
		} break;
		case WM_EXITSIZEMOVE: {
			KillTimer(hWnd, move_timer_id);
		} break;
		case WM_TIMER: {
			if (wParam == move_timer_id) {
				process_key_events();
				if (!Main::is_iterating()) {
					Main::iteration();
				}
			}
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYUP:
		case WM_KEYDOWN: {
			if (wParam == VK_SHIFT)
				shift_mem = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN);
			if (wParam == VK_CONTROL)
				control_mem = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN);
			if (wParam == VK_MENU) {
				alt_mem = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN);
				if (lParam & (1 << 24))
					gr_mem = alt_mem;
			}

			if (mouse_mode == MOUSE_MODE_CAPTURED) {
				// When SetCapture is used, ALT+F4 hotkey is ignored by Windows, so handle it ourselves
				if (wParam == VK_F4 && alt_mem && (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN)) {
					if (main_loop)
						main_loop->notification(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
				}
			}
			/*
			if (wParam==VK_WIN) TODO wtf is this?
				meta_mem=uMsg==WM_KEYDOWN;
			*/
			FALLTHROUGH;
		}
		case WM_CHAR: {
			ERR_BREAK(key_event_pos >= KEY_EVENT_BUFFER_SIZE);

			// Make sure we don't include modifiers for the modifier key itself.
			KeyEvent ke;
			ke.shift = (wParam != VK_SHIFT) ? shift_mem : false;
			ke.alt = (!(wParam == VK_MENU && (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN))) ? alt_mem : false;
			ke.control = (wParam != VK_CONTROL) ? control_mem : false;
			ke.meta = meta_mem;
			ke.uMsg = uMsg;

			if (ke.uMsg == WM_SYSKEYDOWN)
				ke.uMsg = WM_KEYDOWN;
			if (ke.uMsg == WM_SYSKEYUP)
				ke.uMsg = WM_KEYUP;

			ke.wParam = wParam;
			ke.lParam = lParam;
			key_event_buffer[key_event_pos++] = ke;

		} break;
		case WM_INPUTLANGCHANGEREQUEST: {
			// FIXME: Do something?
		} break;

		case WM_TOUCH: {
			BOOL bHandled = FALSE;
			UINT cInputs = LOWORD(wParam);
			PTOUCHINPUT pInputs = memnew_arr(TOUCHINPUT, cInputs);
			if (pInputs) {
				if (GetTouchInputInfo((HTOUCHINPUT)lParam, cInputs, pInputs, sizeof(TOUCHINPUT))) {
					for (UINT i = 0; i < cInputs; i++) {
						TOUCHINPUT ti = pInputs[i];
						POINT touch_pos = {
							TOUCH_COORD_TO_PIXEL(ti.x),
							TOUCH_COORD_TO_PIXEL(ti.y),
						};
						ScreenToClient(hWnd, &touch_pos);
						//do something with each touch input entry
						if (ti.dwFlags & TOUCHEVENTF_MOVE) {
							_drag_event(touch_pos.x, touch_pos.y, ti.dwID);
						} else if (ti.dwFlags & (TOUCHEVENTF_UP | TOUCHEVENTF_DOWN)) {
							_touch_event(ti.dwFlags & TOUCHEVENTF_DOWN, touch_pos.x, touch_pos.y, ti.dwID);
						};
					}
					bHandled = TRUE;
				} else {
					/* handle the error here */
				}
				memdelete_arr(pInputs);
			} else {
				/* handle the error here, probably out of memory */
			}
			if (bHandled) {
				CloseTouchInputHandle((HTOUCHINPUT)lParam);
				return 0;
			};

		} break;

		case WM_DEVICECHANGE: {
			joypad->probe_joypads();
			CameraServer *camera_server = CameraServer::get_singleton();
			if (camera_server) {
				camera_server->update_feeds();
			}
		} break;
		case WM_SETCURSOR: {
			if (LOWORD(lParam) == HTCLIENT) {
				if (window_has_focus && (mouse_mode == MOUSE_MODE_HIDDEN || mouse_mode == MOUSE_MODE_CAPTURED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN)) {
					//Hide the cursor
					if (hCursor == NULL) {
						hCursor = SetCursor(NULL);
					} else {
						SetCursor(NULL);
					}
				} else {
					if (hCursor != NULL) {
						CursorShape c = cursor_shape;
						cursor_shape = CURSOR_MAX;
						set_cursor_shape(c);
						hCursor = NULL;
					}
				}
			}

		} break;
		case WM_DROPFILES: {
			HDROP hDropInfo = (HDROP)wParam;
			const int buffsize = 4096;
			wchar_t buf[buffsize];

			int fcount = DragQueryFileW(hDropInfo, 0xFFFFFFFF, NULL, 0);

			Vector<String> files;

			for (int i = 0; i < fcount; i++) {
				DragQueryFileW(hDropInfo, i, buf, buffsize);
				String file = buf;
				files.push_back(file);
			}

			if (files.size() && main_loop) {
				main_loop->drop_files(files, 0);
			}

		} break;

		default: {
			if (user_proc) {
				return CallWindowProcW(user_proc, hWnd, uMsg, wParam, lParam);
			};
		};
	}

	return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	OS_Windows *os_win = static_cast<OS_Windows *>(OS::get_singleton());
	if (os_win)
		return os_win->WndProc(hWnd, uMsg, wParam, lParam);
	else
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void OS_Windows::process_key_events() {
	for (int i = 0; i < key_event_pos; i++) {
		KeyEvent &ke = key_event_buffer[i];
		switch (ke.uMsg) {
			case WM_CHAR: {
				// extended keys should only be processed as WM_KEYDOWN message.
				if (!KeyMappingWindows::is_extended_key(ke.wParam) && ((i == 0 && ke.uMsg == WM_CHAR) || (i > 0 && key_event_buffer[i - 1].uMsg == WM_CHAR))) {
					Ref<InputEventKey> k;
					k.instance();

					k->set_shift(ke.shift);
					k->set_alt(ke.alt);
					k->set_control(ke.control);
					k->set_metakey(ke.meta);
					k->set_pressed(true);
					k->set_scancode(KeyMappingWindows::get_keysym(MapVirtualKey((ke.lParam >> 16) & 0xFF, MAPVK_VSC_TO_VK)));
					k->set_physical_scancode(KeyMappingWindows::get_scansym((ke.lParam >> 16) & 0xFF, ke.lParam & (1 << 24)));
					k->set_unicode(ke.wParam);
					if (k->get_unicode() && gr_mem) {
						k->set_alt(false);
						k->set_control(false);
					}

					if (k->get_unicode() < 32)
						k->set_unicode(0);

					input->parse_input_event(k);
				}

				//do nothing
			} break;
			case WM_KEYUP:
			case WM_KEYDOWN: {
				Ref<InputEventKey> k;
				k.instance();

				k->set_shift(ke.shift);
				k->set_alt(ke.alt);
				k->set_control(ke.control);
				k->set_metakey(ke.meta);

				k->set_pressed(ke.uMsg == WM_KEYDOWN);

				if ((ke.lParam & (1 << 24)) && (ke.wParam == VK_RETURN)) {
					// Special case for Numpad Enter key
					k->set_scancode(KEY_KP_ENTER);
				} else {
					k->set_scancode(KeyMappingWindows::get_keysym(ke.wParam));
				}

				k->set_physical_scancode(KeyMappingWindows::get_scansym((ke.lParam >> 16) & 0xFF, ke.lParam & (1 << 24)));

				if (i + 1 < key_event_pos && key_event_buffer[i + 1].uMsg == WM_CHAR) {
					k->set_unicode(key_event_buffer[i + 1].wParam);
				}
				if (k->get_unicode() && gr_mem) {
					k->set_alt(false);
					k->set_control(false);
				}

				if (k->get_unicode() < 32)
					k->set_unicode(0);

				k->set_echo((ke.uMsg == WM_KEYDOWN && (ke.lParam & (1 << 30))));

				input->parse_input_event(k);

			} break;
		}
	}

	key_event_pos = 0;
}

enum _MonitorDpiType {
	MDT_Effective_DPI = 0,
	MDT_Angular_DPI = 1,
	MDT_Raw_DPI = 2,
	MDT_Default = MDT_Effective_DPI
};

static int QueryDpiForMonitor(HMONITOR hmon, _MonitorDpiType dpiType = MDT_Default) {
	int dpiX = 96, dpiY = 96;

	static HMODULE Shcore = NULL;
	typedef HRESULT(WINAPI * GetDPIForMonitor_t)(HMONITOR hmonitor, _MonitorDpiType dpiType, UINT * dpiX, UINT * dpiY);
	static GetDPIForMonitor_t getDPIForMonitor = NULL;

	if (Shcore == NULL) {
		Shcore = LoadLibraryW(L"Shcore.dll");
		getDPIForMonitor = Shcore ? (GetDPIForMonitor_t)GetProcAddress(Shcore, "GetDpiForMonitor") : NULL;

		if ((Shcore == NULL) || (getDPIForMonitor == NULL)) {
			if (Shcore)
				FreeLibrary(Shcore);
			Shcore = (HMODULE)INVALID_HANDLE_VALUE;
		}
	}

	UINT x = 0, y = 0;
	HRESULT hr = E_FAIL;
	if (hmon && (Shcore != (HMODULE)INVALID_HANDLE_VALUE)) {
		hr = getDPIForMonitor(hmon, dpiType /*MDT_Effective_DPI*/, &x, &y);
		if (SUCCEEDED(hr) && (x > 0) && (y > 0)) {
			dpiX = (int)x;
			dpiY = (int)y;
		}
	} else {
		static int overallX = 0, overallY = 0;
		if (overallX <= 0 || overallY <= 0) {
			HDC hdc = GetDC(NULL);
			if (hdc) {
				overallX = GetDeviceCaps(hdc, LOGPIXELSX);
				overallY = GetDeviceCaps(hdc, LOGPIXELSY);
				ReleaseDC(NULL, hdc);
			}
		}
		if (overallX > 0 && overallY > 0) {
			dpiX = overallX;
			dpiY = overallY;
		}
	}

	return (dpiX + dpiY) / 2;
}

typedef enum _SHC_PROCESS_DPI_AWARENESS {
	SHC_PROCESS_DPI_UNAWARE = 0,
	SHC_PROCESS_SYSTEM_DPI_AWARE = 1,
	SHC_PROCESS_PER_MONITOR_DPI_AWARE = 2
} SHC_PROCESS_DPI_AWARENESS;

int OS_Windows::get_current_video_driver() const {
	return video_driver_index;
}

Error OS_Windows::initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) {
	main_loop = NULL;
	outside = true;
	window_has_focus = true;
	WNDCLASSEXW wc;

	if (is_hidpi_allowed()) {
		HMODULE Shcore = LoadLibraryW(L"Shcore.dll");

		if (Shcore != NULL) {
			typedef HRESULT(WINAPI * SetProcessDpiAwareness_t)(SHC_PROCESS_DPI_AWARENESS);

			SetProcessDpiAwareness_t SetProcessDpiAwareness = (SetProcessDpiAwareness_t)GetProcAddress(Shcore, "SetProcessDpiAwareness");

			if (SetProcessDpiAwareness) {
				SetProcessDpiAwareness(SHC_PROCESS_SYSTEM_DPI_AWARE);
			}
		}
	}

	video_mode = p_desired;
	//printf("**************** desired %s, mode %s\n", p_desired.fullscreen?"true":"false", video_mode.fullscreen?"true":"false");
	RECT WindowRect;

	WindowRect.left = 0;
	WindowRect.right = video_mode.width;
	WindowRect.top = 0;
	WindowRect.bottom = video_mode.height;

	memset(&wc, 0, sizeof(WNDCLASSEXW));
	wc.cbSize = sizeof(WNDCLASSEXW);
	wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
	wc.lpfnWndProc = (WNDPROC)::WndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	//wc.hInstance = hInstance;
	wc.hInstance = godot_hinstance ? godot_hinstance : GetModuleHandle(NULL);
	wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = NULL;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = L"Engine";

	if (!RegisterClassExW(&wc)) {
		MessageBox(NULL, "Failed To Register The Window Class.", "ERROR", MB_OK | MB_ICONEXCLAMATION);
		return ERR_UNAVAILABLE;
	}

	// Init TTS
	bool tts_enabled = GLOBAL_GET("audio/general/text_to_speech");
	if (tts_enabled) {
		tts = memnew(TTS_Windows);
	}

	use_raw_input = true;

	RAWINPUTDEVICE Rid[1];

	Rid[0].usUsagePage = 0x01;
	Rid[0].usUsage = 0x02;
	Rid[0].dwFlags = 0;
	Rid[0].hwndTarget = 0;

	if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == FALSE) {
		//registration failed.
		use_raw_input = false;
	}

	pre_fs_valid = true;
	if (video_mode.fullscreen) {
		/* this returns DPI unaware size, commenting
		DEVMODE current;
		memset(&current, 0, sizeof(current));
		EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &current);

		WindowRect.right = current.dmPelsWidth;
		WindowRect.bottom = current.dmPelsHeight;

		*/

		// Get the primary monitor without providing hwnd
		// Solution from https://devblogs.microsoft.com/oldnewthing/20070809-00/?p=25643
		const POINT ptZero = { 0, 0 };
		EnumScreenData primary_data = { 0, 0, MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY) };
		EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcScreen, (LPARAM)&primary_data);

		EnumSizeData data = { 0, primary_data.screen, Size2() };
		EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcSize, (LPARAM)&data);

		WindowRect.right = data.size.width;
		WindowRect.bottom = data.size.height;

		/*  DEVMODE dmScreenSettings;
		memset(&dmScreenSettings,0,sizeof(dmScreenSettings));
		dmScreenSettings.dmSize=sizeof(dmScreenSettings);
		dmScreenSettings.dmPelsWidth	= video_mode.width;
		dmScreenSettings.dmPelsHeight	= video_mode.height;
		dmScreenSettings.dmBitsPerPel	= current.dmBitsPerPel;
		dmScreenSettings.dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;

		LONG err = ChangeDisplaySettings(&dmScreenSettings,CDS_FULLSCREEN);
		if (err!=DISP_CHANGE_SUCCESSFUL) {

			video_mode.fullscreen=false;
		}*/
		pre_fs_valid = false;

		// If the user has mouse trails enabled in windows, then sometimes the cursor disappears in fullscreen mode.
		// Save number of trails so we can restore when exiting, then turn off mouse trails
		SystemParametersInfoA(SPI_GETMOUSETRAILS, 0, &restore_mouse_trails, 0);
		if (restore_mouse_trails > 1) {
			SystemParametersInfoA(SPI_SETMOUSETRAILS, 0, 0, 0);
		}
	}

	DWORD dwExStyle;
	DWORD dwStyle;

	if (video_mode.fullscreen || video_mode.borderless_window) {
		dwExStyle = WS_EX_APPWINDOW;
		dwStyle = WS_POPUP;

	} else {
		dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
		dwStyle = WS_OVERLAPPEDWINDOW;
		if (!video_mode.resizable) {
			dwStyle &= ~WS_THICKFRAME;
			dwStyle &= ~WS_MAXIMIZEBOX;
		}
	}

	AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);

	char *windowid;
#ifdef MINGW_ENABLED
	windowid = getenv("GODOT_WINDOWID");
#else
	size_t len;
	_dupenv_s(&windowid, &len, "GODOT_WINDOWID");
#endif

	if (windowid) {
// strtoull on mingw
#ifdef MINGW_ENABLED
		hWnd = (HWND)strtoull(windowid, NULL, 0);
#else
		hWnd = (HWND)_strtoui64(windowid, NULL, 0);
#endif
		free(windowid);
		SetLastError(0);
		user_proc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)(WNDPROC)::WndProc);
		DWORD le = GetLastError();
		if (user_proc == 0 && le != 0) {
			printf("Error setting WNDPROC: %li\n", le);
		};
		GetWindowLongPtr(hWnd, GWLP_WNDPROC);

		RECT rect;
		if (!GetClientRect(hWnd, &rect)) {
			MessageBoxW(NULL, L"Window Creation Error.", L"ERROR", MB_OK | MB_ICONEXCLAMATION);
			return ERR_UNAVAILABLE;
		};
		video_mode.width = rect.right;
		video_mode.height = rect.bottom;
		video_mode.fullscreen = false;
	} else {
		hWnd = CreateWindowExW(
				dwExStyle,
				L"Engine", L"",
				dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
				(GetSystemMetrics(SM_CXSCREEN) - WindowRect.right) / 2,
				(GetSystemMetrics(SM_CYSCREEN) - WindowRect.bottom) / 2,
				WindowRect.right - WindowRect.left,
				WindowRect.bottom - WindowRect.top,
				NULL, NULL, hInstance, NULL);
		if (!hWnd) {
			MessageBoxW(NULL, L"Window Creation Error.", L"ERROR", MB_OK | MB_ICONEXCLAMATION);
			return ERR_UNAVAILABLE;
		}
	};

	if (video_mode.always_on_top) {
		SetWindowPos(hWnd, video_mode.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	if ((get_current_tablet_driver() == "wintab") && wintab_available) {
		wintab_WTInfo(WTI_DEFSYSCTX, 0, &wtlc);
		wtlc.lcOptions |= CXO_MESSAGES;
		wtlc.lcPktData = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_ORIENTATION;
		wtlc.lcMoveMask = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE;
		wtlc.lcPktMode = 0;
		wtlc.lcOutOrgX = 0;
		wtlc.lcOutExtX = wtlc.lcInExtX;
		wtlc.lcOutOrgY = 0;
		wtlc.lcOutExtY = -wtlc.lcInExtY;
		wtctx = wintab_WTOpen(hWnd, &wtlc, false);
		if (wtctx) {
			wintab_WTEnable(wtctx, true);
			AXIS pressure;
			if (wintab_WTInfo(WTI_DEVICES + wtlc.lcDevice, DVC_NPRESSURE, &pressure)) {
				min_pressure = int(pressure.axMin);
				max_pressure = int(pressure.axMax);
			}
			AXIS orientation[3];
			if (wintab_WTInfo(WTI_DEVICES + wtlc.lcDevice, DVC_ORIENTATION, &orientation)) {
				tilt_supported = orientation[0].axResolution && orientation[1].axResolution;
			}
		} else {
			print_verbose("WinTab context creation failed.");
		}
	} else {
		wtctx = 0;
	}

	last_pressure = 0;
	last_pressure_update = 0;
	last_tilt = Vector2();
	last_pen_inverted = false;

#if defined(OPENGL_ENABLED)

	bool gles3_context = true;
	if (p_video_driver == VIDEO_DRIVER_GLES2) {
		gles3_context = false;
	}

	bool editor = Engine::get_singleton()->is_editor_hint();
	bool gl_initialization_error = false;

	gl_context = NULL;
	while (!gl_context) {
		gl_context = memnew(ContextGL_Windows(hWnd, gles3_context));

		if (gl_context->initialize() != OK) {
			memdelete(gl_context);
			gl_context = NULL;

			if (GLOBAL_GET("rendering/quality/driver/fallback_to_gles2") || editor) {
				if (p_video_driver == VIDEO_DRIVER_GLES2) {
					gl_initialization_error = true;
					break;
				}

				p_video_driver = VIDEO_DRIVER_GLES2;
				gles3_context = false;
			} else {
				gl_initialization_error = true;
				break;
			}
		}
	}

	while (true) {
		if (gles3_context) {
			if (RasterizerGLES3::is_viable() == OK) {
				RasterizerGLES3::register_config();
				RasterizerGLES3::make_current();
				break;
			} else {
				if (GLOBAL_GET("rendering/quality/driver/fallback_to_gles2") || editor) {
					p_video_driver = VIDEO_DRIVER_GLES2;
					gles3_context = false;
					continue;
				} else {
					gl_initialization_error = true;
					break;
				}
			}
		} else {
			if (RasterizerGLES2::is_viable() == OK) {
				RasterizerGLES2::register_config();
				RasterizerGLES2::make_current();
				break;
			} else {
				gl_initialization_error = true;
				break;
			}
		}
	}

	if (gl_initialization_error) {
		OS::get_singleton()->alert("Your video card driver does not support any of the supported OpenGL versions.\n"
								   "Please update your drivers or if you have a very old or integrated GPU, upgrade it.",
				"Unable to initialize Video driver");
		return ERR_UNAVAILABLE;
	}

	video_driver_index = p_video_driver;

	gl_context->set_use_vsync(video_mode.use_vsync);
	set_vsync_via_compositor(video_mode.vsync_via_compositor);
#endif

	visual_server = memnew(VisualServerRaster);
	if (get_render_thread_mode() != RENDER_THREAD_UNSAFE) {
		visual_server = memnew(VisualServerWrapMT(visual_server, get_render_thread_mode() == RENDER_SEPARATE_THREAD));
	}

	visual_server->init();

	input = memnew(InputDefault);
	joypad = memnew(JoypadWindows(input, &hWnd));

	power_manager = memnew(PowerWindows);

	AudioDriverManager::initialize(p_audio_driver);

	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = TME_LEAVE;
	tme.hwndTrack = hWnd;
	tme.dwHoverTime = HOVER_DEFAULT;
	TrackMouseEvent(&tme);

	RegisterTouchWindow(hWnd, 0);

	DragAcceptFiles(hWnd, true);

	move_timer_id = 1;

	if (!is_no_window_mode_enabled()) {
		ShowWindow(hWnd, SW_SHOW); // Show The Window
		SetForegroundWindow(hWnd); // Slightly Higher Priority
		SetFocus(hWnd); // Sets Keyboard Focus To
	}

	if (p_desired.layered) {
		set_window_per_pixel_transparency_enabled(true);
	}

	// IME
	im_himc = ImmGetContext(hWnd);
	ImmReleaseContext(hWnd, im_himc);

	im_position = Vector2();

	set_ime_active(false);

	if (!Engine::get_singleton()->is_editor_hint() && !OS::get_singleton()->is_in_low_processor_usage_mode()) {
		// Increase priority for projects that are not in low-processor mode (typically games)
		// to reduce the risk of frame stuttering.
		// This is not done for the editor to prevent importers or resource bakers
		// from making the system unresponsive.
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
		DWORD index = 0;
		HANDLE handle = AvSetMmThreadCharacteristics("Games", &index);
		if (handle)
			AvSetMmThreadPriority(handle, AVRT_PRIORITY_CRITICAL);

		// This is needed to make sure that background work does not starve the main thread.
		// This is only setting priority of this thread, not the whole process.
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
	}

	update_real_mouse_position();

	return OK;
}

bool OS_Windows::is_offscreen_gl_available() const {
#if defined(OPENGL_ENABLED)
	return gl_context->is_offscreen_available();
#else
	return false;
#endif
}

void OS_Windows::set_offscreen_gl_current(bool p_current) {
#if defined(OPENGL_ENABLED)
	if (p_current) {
		return gl_context->make_offscreen_current();
	} else {
		return gl_context->release_offscreen_current();
	}
#endif
}

void OS_Windows::set_clipboard(const String &p_text) {
	// Convert LF line endings to CRLF in clipboard content
	// Otherwise, line endings won't be visible when pasted in other software
	String text = p_text.replace("\r\n", "\n").replace("\n", "\r\n"); // avoid \r\r\n

	if (!OpenClipboard(hWnd)) {
		ERR_FAIL_MSG("Unable to open clipboard.");
	}
	EmptyClipboard();

	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (text.length() + 1) * sizeof(CharType));
	ERR_FAIL_COND_MSG(mem == NULL, "Unable to allocate memory for clipboard contents.");

	LPWSTR lptstrCopy = (LPWSTR)GlobalLock(mem);
	memcpy(lptstrCopy, text.c_str(), (text.length() + 1) * sizeof(CharType));
	GlobalUnlock(mem);

	SetClipboardData(CF_UNICODETEXT, mem);

	// set the CF_TEXT version (not needed?)
	CharString utf8 = text.utf8();
	mem = GlobalAlloc(GMEM_MOVEABLE, utf8.length() + 1);
	ERR_FAIL_COND_MSG(mem == NULL, "Unable to allocate memory for clipboard contents.");

	LPTSTR ptr = (LPTSTR)GlobalLock(mem);
	memcpy(ptr, utf8.get_data(), utf8.length());
	ptr[utf8.length()] = 0;
	GlobalUnlock(mem);

	SetClipboardData(CF_TEXT, mem);

	CloseClipboard();
};

String OS_Windows::get_clipboard() const {
	String ret;
	if (!OpenClipboard(hWnd)) {
		ERR_FAIL_V_MSG("", "Unable to open clipboard.");
	};

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		HGLOBAL mem = GetClipboardData(CF_UNICODETEXT);
		if (mem != NULL) {
			LPWSTR ptr = (LPWSTR)GlobalLock(mem);
			if (ptr != NULL) {
				ret = String((CharType *)ptr);
				GlobalUnlock(mem);
			};
		};

	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		HGLOBAL mem = GetClipboardData(CF_UNICODETEXT);
		if (mem != NULL) {
			LPTSTR ptr = (LPTSTR)GlobalLock(mem);
			if (ptr != NULL) {
				ret.parse_utf8((const char *)ptr);
				GlobalUnlock(mem);
			};
		};
	};

	CloseClipboard();

	return ret;
};

void OS_Windows::delete_main_loop() {
	if (main_loop)
		memdelete(main_loop);
	main_loop = NULL;
}

void OS_Windows::set_main_loop(MainLoop *p_main_loop) {
	input->set_main_loop(p_main_loop);
	main_loop = p_main_loop;
}

void OS_Windows::finalize() {
#ifdef WINMIDI_ENABLED
	driver_midi.close();
#endif

	if (main_loop)
		memdelete(main_loop);

	main_loop = NULL;

	memdelete(joypad);
	memdelete(input);
	touch_state.clear();

	icon.unref();
	cursors_cache.clear();
	visual_server->finish();
	memdelete(visual_server);
#ifdef OPENGL_ENABLED
	if (gl_context)
		memdelete(gl_context);
#endif

	if (user_proc) {
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)user_proc);
	};

	if (restore_mouse_trails > 1) {
		SystemParametersInfoA(SPI_SETMOUSETRAILS, restore_mouse_trails, 0, 0);
	}

	if (tts) {
		memdelete(tts);
	}
	CoUninitialize();
}

void OS_Windows::finalize_core() {
	timeEndPeriod(1);

	memdelete(process_map);
	NetSocketPosix::cleanup();
}

void OS_Windows::alert(const String &p_alert, const String &p_title) {
	if (is_no_window_mode_enabled()) {
		print_line("ALERT: " + p_title + ": " + p_alert);
		return;
	}

	MessageBoxW(NULL, p_alert.c_str(), p_title.c_str(), MB_OK | MB_ICONEXCLAMATION | MB_TASKMODAL);
}

void OS_Windows::set_mouse_mode(MouseMode p_mode) {
	if (mouse_mode == p_mode)
		return;

	mouse_mode = p_mode;

	_set_mouse_mode_impl(p_mode);
}

void OS_Windows::_set_mouse_mode_impl(MouseMode p_mode) {
	if (p_mode == MOUSE_MODE_CAPTURED || p_mode == MOUSE_MODE_CONFINED || p_mode == MOUSE_MODE_CONFINED_HIDDEN) {
		// Mouse is grabbed (captured or confined).
		RECT clipRect;
		GetClientRect(hWnd, &clipRect);
		ClientToScreen(hWnd, (POINT *)&clipRect.left);
		ClientToScreen(hWnd, (POINT *)&clipRect.right);
		ClipCursor(&clipRect);
		if (p_mode == MOUSE_MODE_CAPTURED) {
			center = Point2i(video_mode.width / 2, video_mode.height / 2);
			POINT pos = { (int)center.x, (int)center.y };
			ClientToScreen(hWnd, &pos);
			SetCursorPos(pos.x, pos.y);
			SetCapture(hWnd);
		}
	} else {
		// Mouse is free to move around (not captured or confined).
		ReleaseCapture();
		ClipCursor(NULL);
	}

	if (p_mode == MOUSE_MODE_CAPTURED || p_mode == MOUSE_MODE_HIDDEN || p_mode == MOUSE_MODE_CONFINED_HIDDEN) {
		if (hCursor == NULL) {
			hCursor = SetCursor(NULL);
		} else {
			SetCursor(NULL);
		}
	} else {
		CursorShape c = cursor_shape;
		cursor_shape = CURSOR_MAX;
		set_cursor_shape(c);
	}
}
OS_Windows::MouseMode OS_Windows::get_mouse_mode() const {
	return mouse_mode;
}

void OS_Windows::warp_mouse_position(const Point2 &p_to) {
	if (mouse_mode == MOUSE_MODE_CAPTURED) {
		old_x = p_to.x;
		old_y = p_to.y;
	} else {
		POINT p;
		p.x = p_to.x;
		p.y = p_to.y;
		ClientToScreen(hWnd, &p);

		SetCursorPos(p.x, p.y);
	}
}

Point2 OS_Windows::get_mouse_position() const {
	return Point2(old_x, old_y);
}

void OS_Windows::update_real_mouse_position() {
	POINT mouse_pos;
	if (GetCursorPos(&mouse_pos) && ScreenToClient(hWnd, &mouse_pos)) {
		if (mouse_pos.x > 0 && mouse_pos.y > 0 && mouse_pos.x <= video_mode.width && mouse_pos.y <= video_mode.height) {
			old_x = mouse_pos.x;
			old_y = mouse_pos.y;
			old_invalid = false;
			input->set_mouse_position(Point2i(mouse_pos.x, mouse_pos.y));
		}
	}
}

int OS_Windows::get_mouse_button_state() const {
	return last_button_state;
}

void OS_Windows::set_window_title(const String &p_title) {
	SetWindowTextW(hWnd, p_title.c_str());
}

void OS_Windows::set_window_mouse_passthrough(const PoolVector2Array &p_region) {
	mpath.clear();
	for (int i = 0; i < p_region.size(); i++) {
		mpath.push_back(p_region[i]);
	}
	_update_window_mouse_passthrough();
}

void OS_Windows::_update_window_mouse_passthrough() {
	if (mpath.size() == 0) {
		SetWindowRgn(hWnd, NULL, TRUE);
	} else {
		POINT *points = (POINT *)memalloc(sizeof(POINT) * mpath.size());
		for (int i = 0; i < mpath.size(); i++) {
			if (video_mode.borderless_window) {
				points[i].x = mpath[i].x;
				points[i].y = mpath[i].y;
			} else {
				points[i].x = mpath[i].x + GetSystemMetrics(SM_CXSIZEFRAME);
				points[i].y = mpath[i].y + GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CYCAPTION);
			}
		}

		HRGN region = CreatePolygonRgn(points, mpath.size(), ALTERNATE);
		SetWindowRgn(hWnd, region, TRUE);
		DeleteObject(region);
		memfree(points);
	}
}

void OS_Windows::set_video_mode(const VideoMode &p_video_mode, int p_screen) {
}

OS::VideoMode OS_Windows::get_video_mode(int p_screen) const {
	return video_mode;
}
void OS_Windows::get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen) const {
}

static BOOL CALLBACK _MonitorEnumProcCount(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	int *data = (int *)dwData;
	(*data)++;
	return TRUE;
}

int OS_Windows::get_screen_count() const {
	int data = 0;
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcCount, (LPARAM)&data);
	return data;
}

int OS_Windows::get_current_screen() const {
	EnumScreenData data = { 0, 0, MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST) };
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcScreen, (LPARAM)&data);
	return data.screen;
}

void OS_Windows::set_current_screen(int p_screen) {
	if (video_mode.fullscreen) {
		int cs = get_current_screen();
		if (cs == p_screen) {
			return;
		}
		Point2 pos = get_screen_position(p_screen);
		Size2 size = get_screen_size(p_screen);

		MoveWindow(hWnd, pos.x, pos.y, size.width, size.height, TRUE);
	} else {
		Vector2 ofs = get_window_position() - get_screen_position(get_current_screen());
		set_window_position(ofs + get_screen_position(p_screen));
	}
}

static BOOL CALLBACK _MonitorEnumProcPos(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumPosData *data = (EnumPosData *)dwData;
	if (data->count == data->screen) {
		data->pos.x = lprcMonitor->left;
		data->pos.y = lprcMonitor->top;
	}

	data->count++;
	return TRUE;
}

Point2 OS_Windows::get_screen_position(int p_screen) const {
	EnumPosData data = { 0, p_screen == -1 ? get_current_screen() : p_screen, Point2() };
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcPos, (LPARAM)&data);
	return data.pos;
}

Size2 OS_Windows::get_screen_size(int p_screen) const {
	EnumSizeData data = { 0, p_screen == -1 ? get_current_screen() : p_screen, Size2() };
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcSize, (LPARAM)&data);
	return data.size;
}

typedef struct {
	int count;
	int screen;
	int dpi;
} EnumDpiData;

typedef struct {
	int count;
	int screen;
	float rate;
} EnumRefreshRateData;

static BOOL CALLBACK _MonitorEnumProcDpi(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumDpiData *data = (EnumDpiData *)dwData;
	if (data->count == data->screen) {
		data->dpi = QueryDpiForMonitor(hMonitor);
	}

	data->count++;
	return TRUE;
}

static BOOL CALLBACK _MonitorEnumProcRefreshRate(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
	EnumRefreshRateData *data = (EnumRefreshRateData *)dwData;
	if (data->count == data->screen) {
		MONITORINFOEXW minfo;
		memset(&minfo, 0, sizeof(minfo));
		minfo.cbSize = sizeof(minfo);
		GetMonitorInfoW(hMonitor, &minfo);

		DEVMODEW dm;
		memset(&dm, 0, sizeof(dm));
		dm.dmSize = sizeof(dm);
		EnumDisplaySettingsW(minfo.szDevice, ENUM_CURRENT_SETTINGS, &dm);

		data->rate = dm.dmDisplayFrequency;
	}

	data->count++;
	return TRUE;
}

int OS_Windows::get_screen_dpi(int p_screen) const {
	EnumDpiData data = {
		0,
		p_screen == -1 ? get_current_screen() : p_screen,
		72,
	};
	EnumDisplayMonitors(NULL, NULL, _MonitorEnumProcDpi, (LPARAM)&data);
	return data.dpi;
}

float OS_Windows::get_screen_refresh_rate(int p_screen) const {
	EnumRefreshRateData data = {
		0,
		p_screen == -1 ? get_current_screen() : p_screen,
		OS::get_singleton()->SCREEN_REFRESH_RATE_FALLBACK,
	};
	EnumDisplayMonitors(nullptr, nullptr, _MonitorEnumProcRefreshRate, (LPARAM)&data);
	return data.rate;
}

Point2 OS_Windows::get_window_position() const {
	if (minimized) {
		return last_pos;
	}

	RECT r;
	GetWindowRect(hWnd, &r);
	return Point2(r.left, r.top);
}

void OS_Windows::set_window_position(const Point2 &p_position) {
	if (video_mode.fullscreen)
		return;
	RECT r;
	GetWindowRect(hWnd, &r);
	MoveWindow(hWnd, p_position.x, p_position.y, r.right - r.left, r.bottom - r.top, TRUE);

	// Don't let the mouse leave the window when moved
	if (mouse_mode == MOUSE_MODE_CONFINED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN) {
		RECT rect;
		GetClientRect(hWnd, &rect);
		ClientToScreen(hWnd, (POINT *)&rect.left);
		ClientToScreen(hWnd, (POINT *)&rect.right);
		ClipCursor(&rect);
	}

	last_pos = p_position;
	update_real_mouse_position();
}

Size2 OS_Windows::get_window_size() const {
	if (minimized) {
		return Size2(video_mode.width, video_mode.height);
	}

	RECT r;
	if (GetClientRect(hWnd, &r)) { // Only area inside of window border
		return Size2(r.right - r.left, r.bottom - r.top);
	}
	return Size2();
}

Size2 OS_Windows::get_max_window_size() const {
	return max_size;
}

Size2 OS_Windows::get_min_window_size() const {
	return min_size;
}

void OS_Windows::set_min_window_size(const Size2 p_size) {
	if ((p_size != Size2()) && (max_size != Size2()) && ((p_size.x > max_size.x) || (p_size.y > max_size.y))) {
		ERR_PRINT("Minimum window size can't be larger than maximum window size!");
		return;
	}
	min_size = p_size;
}

void OS_Windows::set_max_window_size(const Size2 p_size) {
	if ((p_size != Size2()) && ((p_size.x < min_size.x) || (p_size.y < min_size.y))) {
		ERR_PRINT("Maximum window size can't be smaller than minimum window size!");
		return;
	}
	max_size = p_size;
}

Size2 OS_Windows::get_real_window_size() const {
	RECT r;
	if (GetWindowRect(hWnd, &r)) { // Includes area of the window border
		return Size2(r.right - r.left, r.bottom - r.top);
	}
	return Size2();
}

void OS_Windows::set_window_size(const Size2 p_size) {
	int w = p_size.width;
	int h = p_size.height;

	video_mode.width = w;
	video_mode.height = h;

	if (video_mode.fullscreen) {
		return;
	}

	RECT rect;
	GetWindowRect(hWnd, &rect);

	if (!video_mode.borderless_window) {
		RECT crect;
		GetClientRect(hWnd, &crect);

		w += (rect.right - rect.left) - (crect.right - crect.left);
		h += (rect.bottom - rect.top) - (crect.bottom - crect.top);
	}

	MoveWindow(hWnd, rect.left, rect.top, w, h, TRUE);

	// Don't let the mouse leave the window when resizing to a smaller resolution
	if (mouse_mode == MOUSE_MODE_CONFINED || mouse_mode == MOUSE_MODE_CONFINED_HIDDEN) {
		RECT crect;
		GetClientRect(hWnd, &crect);
		ClientToScreen(hWnd, (POINT *)&crect.left);
		ClientToScreen(hWnd, (POINT *)&crect.right);
		ClipCursor(&crect);
	}
}
void OS_Windows::set_window_fullscreen(bool p_enabled) {
	if (video_mode.fullscreen == p_enabled)
		return;

	if (layered_window)
		set_window_per_pixel_transparency_enabled(false);

	if (p_enabled) {
		was_maximized = maximized;

		if (pre_fs_valid) {
			GetWindowRect(hWnd, &pre_fs_rect);
		}

		int cs = get_current_screen();
		Point2 pos = get_screen_position(cs);
		Size2 size = get_screen_size(cs);

		video_mode.fullscreen = true;

		_update_window_style(false);

		MoveWindow(hWnd, pos.x, pos.y, size.width, size.height, TRUE);

		SystemParametersInfoA(SPI_GETMOUSETRAILS, 0, &restore_mouse_trails, 0);
		if (restore_mouse_trails > 1) {
			SystemParametersInfoA(SPI_SETMOUSETRAILS, 0, 0, 0);
		}
	} else {
		RECT rect;

		video_mode.fullscreen = false;

		if (pre_fs_valid) {
			rect = pre_fs_rect;
		} else {
			rect.left = 0;
			rect.right = video_mode.width;
			rect.top = 0;
			rect.bottom = video_mode.height;
		}

		_update_window_style(false, was_maximized);

		MoveWindow(hWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);

		pre_fs_valid = true;

		if (restore_mouse_trails > 1) {
			SystemParametersInfoA(SPI_SETMOUSETRAILS, restore_mouse_trails, 0, 0);
		}
	}
}
bool OS_Windows::is_window_fullscreen() const {
	return video_mode.fullscreen;
}
void OS_Windows::set_window_resizable(bool p_enabled) {
	if (video_mode.resizable == p_enabled)
		return;

	video_mode.resizable = p_enabled;

	_update_window_style();
}
bool OS_Windows::is_window_resizable() const {
	return video_mode.resizable;
}
void OS_Windows::set_window_minimized(bool p_enabled) {
	if (is_no_window_mode_enabled()) {
		return;
	}

	if (p_enabled) {
		maximized = false;
		minimized = true;
		ShowWindow(hWnd, SW_MINIMIZE);
	} else {
		ShowWindow(hWnd, SW_RESTORE);
		maximized = false;
		minimized = false;
	}
}
bool OS_Windows::is_window_minimized() const {
	return minimized;
}
void OS_Windows::set_window_maximized(bool p_enabled) {
	if (is_no_window_mode_enabled()) {
		return;
	}

	if (p_enabled) {
		maximized = true;
		minimized = false;
		ShowWindow(hWnd, SW_MAXIMIZE);
	} else {
		ShowWindow(hWnd, SW_RESTORE);
		maximized = false;
		minimized = false;
	}
}
bool OS_Windows::is_window_maximized() const {
	return maximized;
}

void OS_Windows::set_window_always_on_top(bool p_enabled) {
	if (video_mode.always_on_top == p_enabled)
		return;

	video_mode.always_on_top = p_enabled;

	_update_window_style();
}

bool OS_Windows::is_window_always_on_top() const {
	return video_mode.always_on_top;
}

bool OS_Windows::is_window_focused() const {
	return window_focused;
}

bool OS_Windows::get_window_per_pixel_transparency_enabled() const {
	if (!is_layered_allowed())
		return false;
	return layered_window;
}

void OS_Windows::set_window_per_pixel_transparency_enabled(bool p_enabled) {
	if (!is_layered_allowed())
		return;
	if (layered_window != p_enabled) {
		if (p_enabled) {
			//enable per-pixel alpha

			DWM_BLURBEHIND bb = { 0 };
			HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
			bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
			bb.hRgnBlur = hRgn;
			bb.fEnable = TRUE;
			DwmEnableBlurBehindWindow(hWnd, &bb);

			layered_window = true;
		} else {
			//disable per-pixel alpha
			layered_window = false;

			DWM_BLURBEHIND bb = { 0 };
			HRGN hRgn = CreateRectRgn(0, 0, -1, -1);
			bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
			bb.hRgnBlur = hRgn;
			bb.fEnable = FALSE;
			DwmEnableBlurBehindWindow(hWnd, &bb);
		}
	}
}

void OS_Windows::set_borderless_window(bool p_borderless) {
	if (video_mode.borderless_window == p_borderless)
		return;

	video_mode.borderless_window = p_borderless;

	preserve_window_size = true;
	_update_window_style();
	_update_window_mouse_passthrough();
}

bool OS_Windows::get_borderless_window() {
	return video_mode.borderless_window;
}

void OS_Windows::_update_window_style(bool p_repaint, bool p_maximized) {
	if (video_mode.fullscreen || video_mode.borderless_window) {
		SetWindowLongPtr(hWnd, GWL_STYLE, WS_SYSMENU | WS_POPUP | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE);
	} else {
		if (video_mode.resizable) {
			if (p_maximized) {
				SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_MAXIMIZE);
			} else {
				SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
			}
		} else {
			SetWindowLongPtr(hWnd, GWL_STYLE, WS_CAPTION | WS_MINIMIZEBOX | WS_POPUPWINDOW | WS_VISIBLE);
		}
	}

	if (icon.is_valid()) {
		set_icon(icon);
	}

	SetWindowPos(hWnd, video_mode.always_on_top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);

	if (p_repaint) {
		RECT rect;
		GetWindowRect(hWnd, &rect);
		MoveWindow(hWnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}
}

Error OS_Windows::open_dynamic_library(const String p_path, void *&p_library_handle, bool p_also_set_library_path) {
	String path = p_path.replace("/", "\\");

	if (!FileAccess::exists(path)) {
		//this code exists so gdnative can load .dll files from within the executable path
		path = get_executable_path().get_base_dir().plus_file(p_path.get_file());
	}

	typedef DLL_DIRECTORY_COOKIE(WINAPI * PAddDllDirectory)(PCWSTR);
	typedef BOOL(WINAPI * PRemoveDllDirectory)(DLL_DIRECTORY_COOKIE);

	PAddDllDirectory add_dll_directory = (PAddDllDirectory)GetProcAddress(GetModuleHandle("kernel32.dll"), "AddDllDirectory");
	PRemoveDllDirectory remove_dll_directory = (PRemoveDllDirectory)GetProcAddress(GetModuleHandle("kernel32.dll"), "RemoveDllDirectory");

	bool has_dll_directory_api = ((add_dll_directory != NULL) && (remove_dll_directory != NULL));
	DLL_DIRECTORY_COOKIE cookie = NULL;

	if (p_also_set_library_path && has_dll_directory_api) {
		cookie = add_dll_directory(path.get_base_dir().c_str());
	}

	p_library_handle = (void *)LoadLibraryExW(path.c_str(), NULL, (p_also_set_library_path && has_dll_directory_api) ? LOAD_LIBRARY_SEARCH_DEFAULT_DIRS : 0);
	ERR_FAIL_COND_V_MSG(!p_library_handle, ERR_CANT_OPEN, "Can't open dynamic library: " + p_path + ", error: " + format_error_message(GetLastError()) + ".");

	if (cookie) {
		remove_dll_directory(cookie);
	}

	return OK;
}

Error OS_Windows::close_dynamic_library(void *p_library_handle) {
	if (!FreeLibrary((HMODULE)p_library_handle)) {
		return FAILED;
	}
	return OK;
}

Error OS_Windows::get_dynamic_library_symbol_handle(void *p_library_handle, const String p_name, void *&p_symbol_handle, bool p_optional) {
	p_symbol_handle = (void *)GetProcAddress((HMODULE)p_library_handle, p_name.utf8().get_data());
	if (!p_symbol_handle) {
		if (!p_optional) {
			ERR_FAIL_V_MSG(ERR_CANT_RESOLVE, "Can't resolve symbol " + p_name + ", error: " + String::num(GetLastError()) + ".");
		} else {
			return ERR_CANT_RESOLVE;
		}
	}
	return OK;
}

void OS_Windows::request_attention() {
	FLASHWINFO info;
	info.cbSize = sizeof(FLASHWINFO);
	info.hwnd = hWnd;
	info.dwFlags = FLASHW_TRAY;
	info.dwTimeout = 0;
	info.uCount = 2;
	FlashWindowEx(&info);
}

void *OS_Windows::get_native_handle(int p_handle_type) {
	switch (p_handle_type) {
		case APPLICATION_HANDLE:
			return hInstance;
		case DISPLAY_HANDLE:
			return NULL; // Do we have a value to return here?
		case WINDOW_HANDLE:
			return hWnd;
		case WINDOW_VIEW:
			return gl_context->get_hdc();
		case OPENGL_CONTEXT:
			return gl_context->get_hglrc();
		default:
			return NULL;
	}
}

String OS_Windows::get_name() const {
	return "Windows";
}

OS::Date OS_Windows::get_date(bool utc) const {
	SYSTEMTIME systemtime;
	if (utc)
		GetSystemTime(&systemtime);
	else
		GetLocalTime(&systemtime);

	// Get DST information from Windows, but only if utc is false.
	TIME_ZONE_INFORMATION info;
	bool daylight = false;
	if (!utc && GetTimeZoneInformation(&info) == TIME_ZONE_ID_DAYLIGHT) {
		daylight = true;
	}

	Date date;
	date.day = systemtime.wDay;
	date.month = Month(systemtime.wMonth);
	date.weekday = Weekday(systemtime.wDayOfWeek);
	date.year = systemtime.wYear;
	date.dst = daylight;
	return date;
}
OS::Time OS_Windows::get_time(bool utc) const {
	SYSTEMTIME systemtime;
	if (utc)
		GetSystemTime(&systemtime);
	else
		GetLocalTime(&systemtime);

	Time time;
	time.hour = systemtime.wHour;
	time.min = systemtime.wMinute;
	time.sec = systemtime.wSecond;
	return time;
}

OS::TimeZoneInfo OS_Windows::get_time_zone_info() const {
	TIME_ZONE_INFORMATION info;
	bool daylight = false;
	if (GetTimeZoneInformation(&info) == TIME_ZONE_ID_DAYLIGHT)
		daylight = true;

	// Daylight Bias needs to be added to the bias if DST is in effect, or else it will not properly update.
	TimeZoneInfo ret;
	if (daylight) {
		ret.name = info.DaylightName;
		ret.bias = info.Bias + info.DaylightBias;
	} else {
		ret.name = info.StandardName;
		ret.bias = info.Bias + info.StandardBias;
	}

	// Bias value returned by GetTimeZoneInformation is inverted of what we expect
	// For example, on GMT-3 GetTimeZoneInformation return a Bias of 180, so invert the value to get -180
	ret.bias = -ret.bias;
	return ret;
}

uint64_t OS_Windows::get_unix_time() const {
	FILETIME ft;
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);

	SYSTEMTIME ep;
	ep.wYear = 1970;
	ep.wMonth = 1;
	ep.wDayOfWeek = 4;
	ep.wDay = 1;
	ep.wHour = 0;
	ep.wMinute = 0;
	ep.wSecond = 0;
	ep.wMilliseconds = 0;
	FILETIME fep;
	SystemTimeToFileTime(&ep, &fep);

	// Type punning through unions (rather than pointer cast) as per:
	// https://docs.microsoft.com/en-us/windows/desktop/api/minwinbase/ns-minwinbase-filetime#remarks
	ULARGE_INTEGER ft_punning;
	ft_punning.LowPart = ft.dwLowDateTime;
	ft_punning.HighPart = ft.dwHighDateTime;

	ULARGE_INTEGER fep_punning;
	fep_punning.LowPart = fep.dwLowDateTime;
	fep_punning.HighPart = fep.dwHighDateTime;

	return (ft_punning.QuadPart - fep_punning.QuadPart) / 10000000;
};

uint64_t OS_Windows::get_system_time_secs() const {
	return get_system_time_msecs() / 1000;
}

uint64_t OS_Windows::get_system_time_msecs() const {
	const uint64_t WINDOWS_TICK = 10000;
	const uint64_t MSEC_TO_UNIX_EPOCH = 11644473600000LL;

	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	uint64_t ret;
	ret = ft.dwHighDateTime;
	ret <<= 32;
	ret |= ft.dwLowDateTime;

	return (uint64_t)(ret / WINDOWS_TICK - MSEC_TO_UNIX_EPOCH);
}

double OS_Windows::get_subsecond_unix_time() const {
	// 1 Windows tick is 100ns
	const uint64_t WINDOWS_TICKS_PER_SECOND = 10000000;
	const uint64_t TICKS_TO_UNIX_EPOCH = 116444736000000000LL;

	SYSTEMTIME st;
	GetSystemTime(&st);
	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);
	uint64_t ticks_time;
	ticks_time = ft.dwHighDateTime;
	ticks_time <<= 32;
	ticks_time |= ft.dwLowDateTime;

	return (double)(ticks_time - TICKS_TO_UNIX_EPOCH) / WINDOWS_TICKS_PER_SECOND;
}

void OS_Windows::delay_usec(uint32_t p_usec) const {
	if (p_usec < 1000)
		Sleep(1);
	else
		Sleep(p_usec / 1000);
}
uint64_t OS_Windows::get_ticks_usec() const {
	uint64_t ticks;

	// This is the number of clock ticks since start
	QueryPerformanceCounter((LARGE_INTEGER *)&ticks);
	// Subtract the ticks at game start to get
	// the ticks since the game started
	ticks -= ticks_start;

	// Divide by frequency to get the time in seconds
	// original calculation shown below is subject to overflow
	// with high ticks_per_second and a number of days since the last reboot.
	// time = ticks * 1000000L / ticks_per_second;

	// we can prevent this by either using 128 bit math
	// or separating into a calculation for seconds, and the fraction
	uint64_t seconds = ticks / ticks_per_second;

	// compiler will optimize these two into one divide
	uint64_t leftover = ticks % ticks_per_second;

	// remainder
	uint64_t time = (leftover * 1000000L) / ticks_per_second;

	// seconds
	time += seconds * 1000000L;

	return time;
}

void OS_Windows::process_events() {
	MSG msg;

	if (!drop_events) {
		joypad->process_joypads();
	}

	while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	if (!drop_events) {
		process_key_events();
		input->flush_buffered_events();
	}
}

void OS_Windows::set_cursor_shape(CursorShape p_shape) {
	ERR_FAIL_INDEX(p_shape, CURSOR_MAX);

	if (cursor_shape == p_shape)
		return;

	if (mouse_mode != MOUSE_MODE_VISIBLE && mouse_mode != MOUSE_MODE_CONFINED) {
		cursor_shape = p_shape;
		return;
	}

	static const LPCTSTR win_cursors[CURSOR_MAX] = {
		IDC_ARROW,
		IDC_IBEAM,
		IDC_HAND, //finger
		IDC_CROSS,
		IDC_WAIT,
		IDC_APPSTARTING,
		IDC_ARROW,
		IDC_ARROW,
		IDC_NO,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_SIZENESW,
		IDC_SIZENWSE,
		IDC_SIZEALL,
		IDC_SIZENS,
		IDC_SIZEWE,
		IDC_HELP
	};

	if (cursors[p_shape] != NULL) {
		SetCursor(cursors[p_shape]);
	} else {
		SetCursor(LoadCursor(hInstance, win_cursors[p_shape]));
	}

	cursor_shape = p_shape;
}

OS::CursorShape OS_Windows::get_cursor_shape() const {
	return cursor_shape;
}

void OS_Windows::set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape, const Vector2 &p_hotspot) {
	if (p_cursor.is_valid()) {
		Map<CursorShape, Vector<Variant>>::Element *cursor_c = cursors_cache.find(p_shape);

		if (cursor_c) {
			if (cursor_c->get()[0] == p_cursor && cursor_c->get()[1] == p_hotspot) {
				set_cursor_shape(p_shape);
				return;
			}

			cursors_cache.erase(p_shape);
		}

		Ref<Texture> texture = p_cursor;
		Ref<AtlasTexture> atlas_texture = p_cursor;
		Ref<Image> image;
		Size2 texture_size;
		Rect2 atlas_rect;

		if (texture.is_valid()) {
			image = texture->get_data();
		}

		if (!image.is_valid() && atlas_texture.is_valid()) {
			texture = atlas_texture->get_atlas();

			atlas_rect.size.width = texture->get_width();
			atlas_rect.size.height = texture->get_height();
			atlas_rect.position.x = atlas_texture->get_region().position.x;
			atlas_rect.position.y = atlas_texture->get_region().position.y;

			texture_size.width = atlas_texture->get_region().size.x;
			texture_size.height = atlas_texture->get_region().size.y;
		} else if (image.is_valid()) {
			texture_size.width = texture->get_width();
			texture_size.height = texture->get_height();
		}

		ERR_FAIL_COND(!texture.is_valid());
		ERR_FAIL_COND(p_hotspot.x < 0 || p_hotspot.y < 0);
		ERR_FAIL_COND(texture_size.width > 256 || texture_size.height > 256);
		ERR_FAIL_COND(p_hotspot.x > texture_size.width || p_hotspot.y > texture_size.height);

		image = texture->get_data();

		ERR_FAIL_COND(!image.is_valid());

		UINT image_size = texture_size.width * texture_size.height;

		// Create the BITMAP with alpha channel
		COLORREF *buffer = (COLORREF *)memalloc(sizeof(COLORREF) * image_size);

		image->lock();
		for (UINT index = 0; index < image_size; index++) {
			int row_index = floor(index / texture_size.width) + atlas_rect.position.y;
			int column_index = (index % int(texture_size.width)) + atlas_rect.position.x;

			if (atlas_texture.is_valid()) {
				column_index = MIN(column_index, atlas_rect.size.width - 1);
				row_index = MIN(row_index, atlas_rect.size.height - 1);
			}

			*(buffer + index) = image->get_pixel(column_index, row_index).to_argb32();
		}
		image->unlock();

		// Using 4 channels, so 4 * 8 bits
		HBITMAP bitmap = CreateBitmap(texture_size.width, texture_size.height, 1, 4 * 8, buffer);
		COLORREF clrTransparent = -1;

		// Create the AND and XOR masks for the bitmap
		HBITMAP hAndMask = NULL;
		HBITMAP hXorMask = NULL;

		GetMaskBitmaps(bitmap, clrTransparent, hAndMask, hXorMask);

		if (NULL == hAndMask || NULL == hXorMask) {
			memfree(buffer);
			DeleteObject(bitmap);
			return;
		}

		// Finally, create the icon
		ICONINFO iconinfo;
		iconinfo.fIcon = FALSE;
		iconinfo.xHotspot = p_hotspot.x;
		iconinfo.yHotspot = p_hotspot.y;
		iconinfo.hbmMask = hAndMask;
		iconinfo.hbmColor = hXorMask;

		if (cursors[p_shape])
			DestroyIcon(cursors[p_shape]);

		cursors[p_shape] = CreateIconIndirect(&iconinfo);

		Vector<Variant> params;
		params.push_back(p_cursor);
		params.push_back(p_hotspot);
		cursors_cache.insert(p_shape, params);

		if (p_shape == cursor_shape) {
			if (mouse_mode == MOUSE_MODE_VISIBLE || mouse_mode == MOUSE_MODE_CONFINED) {
				SetCursor(cursors[p_shape]);
			}
		}

		if (hAndMask != NULL) {
			DeleteObject(hAndMask);
		}

		if (hXorMask != NULL) {
			DeleteObject(hXorMask);
		}

		memfree(buffer);
		DeleteObject(bitmap);
	} else {
		// Reset to default system cursor
		if (cursors[p_shape]) {
			DestroyIcon(cursors[p_shape]);
			cursors[p_shape] = NULL;
		}

		CursorShape c = cursor_shape;
		cursor_shape = CURSOR_MAX;
		set_cursor_shape(c);

		cursors_cache.erase(p_shape);
	}
}

void OS_Windows::GetMaskBitmaps(HBITMAP hSourceBitmap, COLORREF clrTransparent, OUT HBITMAP &hAndMaskBitmap, OUT HBITMAP &hXorMaskBitmap) {
	// Get the system display DC
	HDC hDC = GetDC(NULL);

	// Create helper DC
	HDC hMainDC = CreateCompatibleDC(hDC);
	HDC hAndMaskDC = CreateCompatibleDC(hDC);
	HDC hXorMaskDC = CreateCompatibleDC(hDC);

	// Get the dimensions of the source bitmap
	BITMAP bm;
	GetObject(hSourceBitmap, sizeof(BITMAP), &bm);

	// Create the mask bitmaps
	hAndMaskBitmap = CreateCompatibleBitmap(hDC, bm.bmWidth, bm.bmHeight); // color
	hXorMaskBitmap = CreateCompatibleBitmap(hDC, bm.bmWidth, bm.bmHeight); // color

	// Release the system display DC
	ReleaseDC(NULL, hDC);

	// Select the bitmaps to helper DC
	HBITMAP hOldMainBitmap = (HBITMAP)SelectObject(hMainDC, hSourceBitmap);
	HBITMAP hOldAndMaskBitmap = (HBITMAP)SelectObject(hAndMaskDC, hAndMaskBitmap);
	HBITMAP hOldXorMaskBitmap = (HBITMAP)SelectObject(hXorMaskDC, hXorMaskBitmap);

	// Assign the monochrome AND mask bitmap pixels so that a pixels of the source bitmap
	// with 'clrTransparent' will be white pixels of the monochrome bitmap
	SetBkColor(hMainDC, clrTransparent);
	BitBlt(hAndMaskDC, 0, 0, bm.bmWidth, bm.bmHeight, hMainDC, 0, 0, SRCCOPY);

	// Assign the color XOR mask bitmap pixels so that a pixels of the source bitmap
	// with 'clrTransparent' will be black and rest the pixels same as corresponding
	// pixels of the source bitmap
	SetBkColor(hXorMaskDC, RGB(0, 0, 0));
	SetTextColor(hXorMaskDC, RGB(255, 255, 255));
	BitBlt(hXorMaskDC, 0, 0, bm.bmWidth, bm.bmHeight, hAndMaskDC, 0, 0, SRCCOPY);
	BitBlt(hXorMaskDC, 0, 0, bm.bmWidth, bm.bmHeight, hMainDC, 0, 0, SRCAND);

	// Deselect bitmaps from the helper DC
	SelectObject(hMainDC, hOldMainBitmap);
	SelectObject(hAndMaskDC, hOldAndMaskBitmap);
	SelectObject(hXorMaskDC, hOldXorMaskBitmap);

	// Delete the helper DC
	DeleteDC(hXorMaskDC);
	DeleteDC(hAndMaskDC);
	DeleteDC(hMainDC);
}

String OS_Windows::_quote_command_line_argument(const String &p_text) const {
	for (int i = 0; i < p_text.size(); i++) {
		CharType c = p_text[i];
		if (c == ' ' || c == '&' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '^' || c == '=' || c == ';' || c == '!' || c == '\'' || c == '+' || c == ',' || c == '`' || c == '~') {
			return "\"" + p_text + "\"";
		}
	}
	return p_text;
}

static void _append_to_pipe(char *p_bytes, int p_size, String *r_pipe, Mutex *p_pipe_mutex) {
	// Try to convert from default ANSI code page to Unicode.
	LocalVector<wchar_t> wchars;
	int total_wchars = MultiByteToWideChar(CP_ACP, 0, p_bytes, p_size, nullptr, 0);
	if (total_wchars > 0) {
		wchars.resize(total_wchars);
		if (MultiByteToWideChar(CP_ACP, 0, p_bytes, p_size, wchars.ptr(), total_wchars) == 0) {
			wchars.clear();
		}
	}

	if (p_pipe_mutex) {
		p_pipe_mutex->lock();
	}
	if (wchars.empty()) {
		// Let's hope it's compatible with UTF-8.
		(*r_pipe) += String::utf8(p_bytes, p_size);
	} else {
		(*r_pipe) += String(wchars.ptr(), total_wchars);
	}
	if (p_pipe_mutex) {
		p_pipe_mutex->unlock();
	}
}

Error OS_Windows::execute(const String &p_path, const List<String> &p_arguments, bool p_blocking, ProcessID *r_child_id, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	String path = p_path.replace("/", "\\");

	String cmdline = _quote_command_line_argument(path);
	const List<String>::Element *I = p_arguments.front();
	while (I) {
		cmdline += " " + _quote_command_line_argument(I->get());
		I = I->next();
	}

	ProcessInfo pi;
	ZeroMemory(&pi.si, sizeof(pi.si));
	pi.si.cb = sizeof(pi.si);
	ZeroMemory(&pi.pi, sizeof(pi.pi));
	LPSTARTUPINFOW si_w = (LPSTARTUPINFOW)&pi.si;

	Vector<CharType> modstr; // Windows wants to change this no idea why.
	modstr.resize(cmdline.size());
	for (int i = 0; i < cmdline.size(); i++) {
		modstr.write[i] = cmdline[i];
	}

	bool inherit_handles = false;
	HANDLE pipe[2] = { NULL, NULL };
	if (p_blocking && r_pipe) {
		// Create pipe for StdOut and StdErr.
		SECURITY_ATTRIBUTES sa;
		sa.nLength = sizeof(SECURITY_ATTRIBUTES);
		sa.bInheritHandle = true;
		sa.lpSecurityDescriptor = NULL;

		ERR_FAIL_COND_V(!CreatePipe(&pipe[0], &pipe[1], &sa, 0), ERR_CANT_FORK);
		ERR_FAIL_COND_V(!SetHandleInformation(pipe[0], HANDLE_FLAG_INHERIT, 0), ERR_CANT_FORK); // Read handle is for host process only and should not be inherited.

		pi.si.dwFlags |= STARTF_USESTDHANDLES;
		pi.si.hStdOutput = pipe[1];
		if (read_stderr) {
			pi.si.hStdError = pipe[1];
		}
		inherit_handles = true;
	}
	DWORD creaton_flags = NORMAL_PRIORITY_CLASS;
	if (p_open_console) {
		creaton_flags |= CREATE_NEW_CONSOLE;
	} else {
		creaton_flags |= CREATE_NO_WINDOW;
	}

	int ret = CreateProcessW(NULL, modstr.ptrw(), NULL, NULL, inherit_handles, creaton_flags, NULL, NULL, si_w, &pi.pi);
	if (!ret && r_pipe) {
		CloseHandle(pipe[0]); // Cleanup pipe handles.
		CloseHandle(pipe[1]);
	}
	ERR_FAIL_COND_V_MSG(ret == 0, ERR_CANT_FORK, "Could not create child process: " + String(modstr.ptr()));

	if (p_blocking) {
		if (r_pipe) {
			CloseHandle(pipe[1]); // Close pipe write handle (only child process is writing).

			LocalVector<char> bytes;
			int bytes_in_buffer = 0;

			const int CHUNK_SIZE = 4096;
			DWORD read = 0;
			for (;;) { // Read StdOut and StdErr from pipe.
				bytes.resize(bytes_in_buffer + CHUNK_SIZE);
				const bool success = ReadFile(pipe[0], bytes.ptr() + bytes_in_buffer, CHUNK_SIZE, &read, NULL);
				if (!success || read == 0) {
					break;
				}
				// Assume that all possible encodings are ASCII-compatible.
				// Break at newline to allow receiving long output in portions.
				int newline_index = -1;
				for (int i = read - 1; i >= 0; i--) {
					if (bytes[bytes_in_buffer + i] == '\n') {
						newline_index = i;
						break;
					}
				}

				if (newline_index == -1) {
					bytes_in_buffer += read;
					continue;
				}

				const int bytes_to_convert = bytes_in_buffer + (newline_index + 1);
				_append_to_pipe(bytes.ptr(), bytes_to_convert, r_pipe, p_pipe_mutex);

				bytes_in_buffer = read - (newline_index + 1);
				memmove(bytes.ptr(), bytes.ptr() + bytes_to_convert, bytes_in_buffer);
			}

			if (bytes_in_buffer > 0) {
				_append_to_pipe(bytes.ptr(), bytes_in_buffer, r_pipe, p_pipe_mutex);
			}

			CloseHandle(pipe[0]); // Close pipe read handle.
		} else {
			WaitForSingleObject(pi.pi.hProcess, INFINITE);
		}

		if (r_exitcode) {
			DWORD ret2;
			GetExitCodeProcess(pi.pi.hProcess, &ret2);
			*r_exitcode = ret2;
		}

		CloseHandle(pi.pi.hProcess);
		CloseHandle(pi.pi.hThread);
	} else {
		ProcessID pid = pi.pi.dwProcessId;
		if (r_child_id) {
			*r_child_id = pid;
		}
		process_map->insert(pid, pi);
	}
	return OK;
}

Error OS_Windows::kill(const ProcessID &p_pid) {
	ERR_FAIL_COND_V(!process_map->has(p_pid), FAILED);

	const PROCESS_INFORMATION pi = (*process_map)[p_pid].pi;
	process_map->erase(p_pid);

	const int ret = TerminateProcess(pi.hProcess, 0);

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	return ret != 0 ? OK : FAILED;
};

int OS_Windows::get_process_id() const {
	return _getpid();
}

bool OS_Windows::is_process_running(const ProcessID &p_pid) const {
	if (!process_map->has(p_pid)) {
		return false;
	}

	const PROCESS_INFORMATION &pi = (*process_map)[p_pid].pi;

	DWORD dw_exit_code = 0;
	if (!GetExitCodeProcess(pi.hProcess, &dw_exit_code)) {
		return false;
	}

	if (dw_exit_code != STILL_ACTIVE) {
		return false;
	}

	return true;
}

Error OS_Windows::set_cwd(const String &p_cwd) {
	if (_wchdir(p_cwd.c_str()) != 0)
		return ERR_CANT_OPEN;

	return OK;
}

String OS_Windows::get_executable_path() const {
	wchar_t bufname[4096];
	GetModuleFileNameW(NULL, bufname, 4096);
	String s = bufname;
	return s.replace("\\", "/");
}

void OS_Windows::set_native_icon(const String &p_filename) {
	FileAccess *f = FileAccess::open(p_filename, FileAccess::READ);
	ERR_FAIL_COND_MSG(!f, "Cannot open file with icon '" + p_filename + "'.");

	ICONDIR *icon_dir = (ICONDIR *)memalloc(sizeof(ICONDIR));
	int pos = 0;

	icon_dir->idReserved = f->get_32();
	pos += sizeof(WORD);
	f->seek(pos);

	icon_dir->idType = f->get_32();
	pos += sizeof(WORD);
	f->seek(pos);

	ERR_FAIL_COND_MSG(icon_dir->idType != 1, "Invalid icon file format!");

	icon_dir->idCount = f->get_32();
	pos += sizeof(WORD);
	f->seek(pos);

	icon_dir = (ICONDIR *)memrealloc(icon_dir, sizeof(ICONDIR) + (icon_dir->idCount * sizeof(ICONDIRENTRY)));
	f->get_buffer((uint8_t *)&icon_dir->idEntries[0], icon_dir->idCount * sizeof(ICONDIRENTRY));

	int small_icon_index = -1; // Select 16x16 with largest color count
	int small_icon_cc = 0;
	int big_icon_index = -1; // Select largest
	int big_icon_width = 16;
	int big_icon_cc = 0;

	for (int i = 0; i < icon_dir->idCount; i++) {
		int colors = (icon_dir->idEntries[i].bColorCount == 0) ? 32768 : icon_dir->idEntries[i].bColorCount;
		int width = (icon_dir->idEntries[i].bWidth == 0) ? 256 : icon_dir->idEntries[i].bWidth;
		if (width == 16) {
			if (colors >= small_icon_cc) {
				small_icon_index = i;
				small_icon_cc = colors;
			}
		}
		if (width >= big_icon_width) {
			if (colors >= big_icon_cc) {
				big_icon_index = i;
				big_icon_width = width;
				big_icon_cc = colors;
			}
		}
	}

	ERR_FAIL_COND_MSG(big_icon_index == -1, "No valid icons found!");

	if (small_icon_index == -1) {
		WARN_PRINT("No small icon found, reusing " + itos(big_icon_width) + "x" + itos(big_icon_width) + " @" + itos(big_icon_cc) + " icon!");
		small_icon_index = big_icon_index;
		small_icon_cc = big_icon_cc;
	}

	// Read the big icon
	DWORD bytecount_big = icon_dir->idEntries[big_icon_index].dwBytesInRes;
	Vector<uint8_t> data_big;
	data_big.resize(bytecount_big);
	pos = icon_dir->idEntries[big_icon_index].dwImageOffset;
	f->seek(pos);
	f->get_buffer((uint8_t *)&data_big.write[0], bytecount_big);
	HICON icon_big = CreateIconFromResource((PBYTE)&data_big.write[0], bytecount_big, TRUE, 0x00030000);
	ERR_FAIL_COND_MSG(!icon_big, "Could not create " + itos(big_icon_width) + "x" + itos(big_icon_width) + " @" + itos(big_icon_cc) + " icon, error: " + format_error_message(GetLastError()) + ".");

	// Read the small icon
	DWORD bytecount_small = icon_dir->idEntries[small_icon_index].dwBytesInRes;
	Vector<uint8_t> data_small;
	data_small.resize(bytecount_small);
	pos = icon_dir->idEntries[small_icon_index].dwImageOffset;
	f->seek(pos);
	f->get_buffer((uint8_t *)&data_small.write[0], bytecount_small);
	HICON icon_small = CreateIconFromResource((PBYTE)&data_small.write[0], bytecount_small, TRUE, 0x00030000);
	ERR_FAIL_COND_MSG(!icon_small, "Could not create 16x16 @" + itos(small_icon_cc) + " icon, error: " + format_error_message(GetLastError()) + ".");

	// Online tradition says to be sure last error is cleared and set the small icon first
	int err = 0;
	SetLastError(err);

	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)icon_small);
	err = GetLastError();
	ERR_FAIL_COND_MSG(err, "Error setting ICON_SMALL: " + format_error_message(err) + ".");

	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)icon_big);
	err = GetLastError();
	ERR_FAIL_COND_MSG(err, "Error setting ICON_BIG: " + format_error_message(err) + ".");

	memdelete(f);
	memdelete(icon_dir);
}

void OS_Windows::set_icon(const Ref<Image> &p_icon) {
	ERR_FAIL_COND(!p_icon.is_valid());
	if (icon != p_icon) {
		icon = p_icon->duplicate();
		if (icon->get_format() != Image::FORMAT_RGBA8) {
			icon->convert(Image::FORMAT_RGBA8);
		}
	}
	int w = icon->get_width();
	int h = icon->get_height();

	/* Create temporary bitmap buffer */
	int icon_len = 40 + h * w * 4;
	Vector<BYTE> v;
	v.resize(icon_len);
	BYTE *icon_bmp = v.ptrw();

	encode_uint32(40, &icon_bmp[0]);
	encode_uint32(w, &icon_bmp[4]);
	encode_uint32(h * 2, &icon_bmp[8]);
	encode_uint16(1, &icon_bmp[12]);
	encode_uint16(32, &icon_bmp[14]);
	encode_uint32(BI_RGB, &icon_bmp[16]);
	encode_uint32(w * h * 4, &icon_bmp[20]);
	encode_uint32(0, &icon_bmp[24]);
	encode_uint32(0, &icon_bmp[28]);
	encode_uint32(0, &icon_bmp[32]);
	encode_uint32(0, &icon_bmp[36]);

	uint8_t *wr = &icon_bmp[40];
	PoolVector<uint8_t>::Read r = icon->get_data().read();

	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			const uint8_t *rpx = &r[((h - i - 1) * w + j) * 4];
			uint8_t *wpx = &wr[(i * w + j) * 4];
			wpx[0] = rpx[2];
			wpx[1] = rpx[1];
			wpx[2] = rpx[0];
			wpx[3] = rpx[3];
		}
	}

	HICON hicon = CreateIconFromResource(icon_bmp, icon_len, TRUE, 0x00030000);

	/* Set the icon for the window */
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hicon);

	/* Set the icon in the task manager (should we do this?) */
	SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hicon);
}

bool OS_Windows::has_environment(const String &p_var) const {
#ifdef MINGW_ENABLED
	return _wgetenv(p_var.c_str()) != NULL;
#else
	wchar_t *env;
	size_t len;
	_wdupenv_s(&env, &len, p_var.c_str());
	const bool has_env = env != NULL;
	free(env);
	return has_env;
#endif
};

String OS_Windows::get_environment(const String &p_var) const {
	wchar_t wval[0x7Fff]; // MSDN says 32767 char is the maximum
	int wlen = GetEnvironmentVariableW(p_var.c_str(), wval, 0x7Fff);
	if (wlen > 0) {
		return wval;
	}
	return "";
}

bool OS_Windows::set_environment(const String &p_var, const String &p_value) const {
	return (bool)SetEnvironmentVariableW(p_var.c_str(), p_value.c_str());
}

String OS_Windows::get_stdin_string() {
	char buff[1024];
	return fgets(buff, 1024, stdin);
}

void OS_Windows::enable_for_stealing_focus(ProcessID pid) {
	AllowSetForegroundWindow(pid);
}

void OS_Windows::move_window_to_foreground() {
	SetForegroundWindow(hWnd);
}

Error OS_Windows::shell_open(String p_uri) {
	INT_PTR ret = (INT_PTR)ShellExecuteW(nullptr, nullptr, p_uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
	if (ret > 32) {
		return OK;
	} else {
		switch (ret) {
			case ERROR_FILE_NOT_FOUND:
			case SE_ERR_DLLNOTFOUND:
				return ERR_FILE_NOT_FOUND;
			case ERROR_PATH_NOT_FOUND:
				return ERR_FILE_BAD_PATH;
			case ERROR_BAD_FORMAT:
				return ERR_FILE_CORRUPT;
			case SE_ERR_ACCESSDENIED:
				return ERR_UNAUTHORIZED;
			case 0:
			case SE_ERR_OOM:
				return ERR_OUT_OF_MEMORY;
			default:
				return FAILED;
		}
	}
}

String OS_Windows::get_locale() const {
	const _WinLocale *wl = &_win_locales[0];

	LANGID langid = GetUserDefaultUILanguage();
	String neutral;
	int lang = PRIMARYLANGID(langid);
	int sublang = SUBLANGID(langid);

	while (wl->locale) {
		if (wl->main_lang == lang && wl->sublang == SUBLANG_NEUTRAL)
			neutral = wl->locale;

		if (lang == wl->main_lang && sublang == wl->sublang)
			return String(wl->locale).replace("-", "_");

		wl++;
	}

	if (neutral != "")
		return String(neutral).replace("-", "_");

	return "en";
}

// We need this because GetSystemInfo() is unreliable on WOW64
// see https://msdn.microsoft.com/en-us/library/windows/desktop/ms724381(v=vs.85).aspx
// Taken from MSDN
typedef BOOL(WINAPI *LPFN_ISWOW64PROCESS)(HANDLE, PBOOL);
LPFN_ISWOW64PROCESS fnIsWow64Process;

BOOL is_wow64() {
	BOOL wow64 = FALSE;

	fnIsWow64Process = (LPFN_ISWOW64PROCESS)GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process");

	if (fnIsWow64Process) {
		if (!fnIsWow64Process(GetCurrentProcess(), &wow64)) {
			wow64 = FALSE;
		}
	}

	return wow64;
}

String OS_Windows::get_processor_name() const {
	const String id = "Hardware\\Description\\System\\CentralProcessor\\0";

	HKEY hkey;
	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, (LPCWSTR)(id.c_str()), 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS) {
		ERR_FAIL_V_MSG("", String("Couldn't get the CPU model name. Returning an empty string."));
	}

	WCHAR buffer[256];
	DWORD buffer_len = 256;
	DWORD vtype = REG_SZ;
	if (RegQueryValueExW(hkey, L"ProcessorNameString", NULL, &vtype, (LPBYTE)buffer, &buffer_len) == ERROR_SUCCESS) {
		RegCloseKey(hkey);
		return String((const wchar_t *)buffer, buffer_len).strip_edges();
	} else {
		RegCloseKey(hkey);
		ERR_FAIL_V_MSG("", String("Couldn't get the CPU model name. Returning an empty string."));
	}
}

OS::LatinKeyboardVariant OS_Windows::get_latin_keyboard_variant() const {
	unsigned long azerty[] = {
		0x00020401, // Arabic (102) AZERTY
		0x0001080c, // Belgian (Comma)
		0x0000080c, // Belgian French
		0x0000040c, // French
		0 // <--- STOP MARK
	};
	unsigned long qwertz[] = {
		0x0000041a, // Croation
		0x00000405, // Czech
		0x00000407, // German
		0x00010407, // German (IBM)
		0x0000040e, // Hungarian
		0x0000046e, // Luxembourgish
		0x00010415, // Polish (214)
		0x00000418, // Romanian (Legacy)
		0x0000081a, // Serbian (Latin)
		0x0000041b, // Slovak
		0x00000424, // Slovenian
		0x0001042e, // Sorbian Extended
		0x0002042e, // Sorbian Standard
		0x0000042e, // Sorbian Standard (Legacy)
		0x0000100c, // Swiss French
		0x00000807, // Swiss German
		0 // <--- STOP MARK
	};
	unsigned long dvorak[] = {
		0x00010409, // US-Dvorak
		0x00030409, // US-Dvorak for left hand
		0x00040409, // US-Dvorak for right hand
		0 // <--- STOP MARK
	};

	char name[KL_NAMELENGTH + 1];
	name[0] = 0;
	GetKeyboardLayoutNameA(name);

	unsigned long hex = strtoul(name, NULL, 16);

	int i = 0;
	while (azerty[i] != 0) {
		if (azerty[i] == hex)
			return LATIN_KEYBOARD_AZERTY;
		i++;
	}

	i = 0;
	while (qwertz[i] != 0) {
		if (qwertz[i] == hex)
			return LATIN_KEYBOARD_QWERTZ;
		i++;
	}

	i = 0;
	while (dvorak[i] != 0) {
		if (dvorak[i] == hex)
			return LATIN_KEYBOARD_DVORAK;
		i++;
	}

	return LATIN_KEYBOARD_QWERTY;
}

int OS_Windows::keyboard_get_layout_count() const {
	return GetKeyboardLayoutList(0, NULL);
}

int OS_Windows::keyboard_get_current_layout() const {
	HKL cur_layout = GetKeyboardLayout(0);

	int layout_count = GetKeyboardLayoutList(0, NULL);
	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);

	for (int i = 0; i < layout_count; i++) {
		if (cur_layout == layouts[i]) {
			memfree(layouts);
			return i;
		}
	}
	memfree(layouts);
	return -1;
}

void OS_Windows::keyboard_set_current_layout(int p_index) {
	int layout_count = GetKeyboardLayoutList(0, NULL);

	ERR_FAIL_INDEX(p_index, layout_count);

	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);
	ActivateKeyboardLayout(layouts[p_index], KLF_SETFORPROCESS);
	memfree(layouts);
}

String OS_Windows::keyboard_get_layout_language(int p_index) const {
	int layout_count = GetKeyboardLayoutList(0, NULL);

	ERR_FAIL_INDEX_V(p_index, layout_count, "");

	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);

	wchar_t buf[LOCALE_NAME_MAX_LENGTH];
	memset(buf, 0, LOCALE_NAME_MAX_LENGTH * sizeof(wchar_t));
	LCIDToLocaleName(MAKELCID(LOWORD(layouts[p_index]), SORT_DEFAULT), buf, LOCALE_NAME_MAX_LENGTH, 0);

	memfree(layouts);

	return String(buf).substr(0, 2);
}

uint32_t OS_Windows::keyboard_get_scancode_from_physical(uint32_t p_scancode) const {
	unsigned int modifiers = p_scancode & KEY_MODIFIER_MASK;
	uint32_t scancode_no_mod = (uint32_t)(p_scancode & KEY_CODE_MASK);

	if (scancode_no_mod == KEY_PRINT ||
			scancode_no_mod == KEY_KP_ADD ||
			scancode_no_mod == KEY_KP_5 ||
			(scancode_no_mod >= KEY_0 && scancode_no_mod <= KEY_9)) {
		return p_scancode;
	}

	unsigned int scancode = KeyMappingWindows::get_scancode(scancode_no_mod);
	if (scancode == 0) {
		return p_scancode;
	}

	HKL current_layout = GetKeyboardLayout(0);
	UINT vk = MapVirtualKeyEx(scancode, MAPVK_VSC_TO_VK, current_layout);
	if (vk == 0) {
		return p_scancode;
	}

	UINT char_code = MapVirtualKeyEx(vk, MAPVK_VK_TO_CHAR, current_layout) & 0x7FFF;
	// Unlike a similar Linux/BSD check which matches full Latin-1 range,
	// we limit these to ASCII to fix some layouts, including Arabic ones
	if (char_code >= 32 && char_code <= 127) {
		// Godot uses 'braces' instead of 'brackets'
		if (char_code == KEY_BRACKETLEFT || char_code == KEY_BRACKETRIGHT) {
			char_code += 32;
		}
		return (uint32_t)(char_code | modifiers);
	}

	return (uint32_t)(KeyMappingWindows::get_keysym(vk) | modifiers);
}

String _get_full_layout_name_from_registry(HKL p_layout) {
	String id = "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\" + String::num_int64((int64_t)p_layout, 16, false).lpad(8, "0");
	String ret;

	HKEY hkey;
	wchar_t layout_text[1024];
	memset(layout_text, 0, 1024 * sizeof(wchar_t));

	if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, (LPCWSTR)id.c_str(), 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS) {
		return ret;
	}

	DWORD buffer = 1024;
	DWORD vtype = REG_SZ;
	if (RegQueryValueExW(hkey, L"Layout Text", NULL, &vtype, (LPBYTE)layout_text, &buffer) == ERROR_SUCCESS) {
		ret = String(layout_text);
	}
	RegCloseKey(hkey);
	return ret;
}

String OS_Windows::keyboard_get_layout_name(int p_index) const {
	int layout_count = GetKeyboardLayoutList(0, NULL);

	ERR_FAIL_INDEX_V(p_index, layout_count, "");

	HKL *layouts = (HKL *)memalloc(layout_count * sizeof(HKL));
	GetKeyboardLayoutList(layout_count, layouts);

	String ret = _get_full_layout_name_from_registry(layouts[p_index]); // Try reading full name from Windows registry, fallback to locale name if failed (e.g. on Wine).
	if (ret == String()) {
		wchar_t buf[LOCALE_NAME_MAX_LENGTH];
		memset(buf, 0, LOCALE_NAME_MAX_LENGTH * sizeof(wchar_t));
		LCIDToLocaleName(MAKELCID(LOWORD(layouts[p_index]), SORT_DEFAULT), buf, LOCALE_NAME_MAX_LENGTH, 0);

		wchar_t name[1024];
		memset(name, 0, 1024 * sizeof(wchar_t));
		GetLocaleInfoEx(buf, LOCALE_SLOCALIZEDDISPLAYNAME, (LPWSTR)&name, 1024);

		ret = String(name);
	}
	memfree(layouts);

	return ret;
}

void OS_Windows::release_rendering_thread() {
	gl_context->release_current();
}

void OS_Windows::make_rendering_thread() {
	gl_context->make_current();
}

void OS_Windows::swap_buffers() {
	gl_context->swap_buffers();
}

void OS_Windows::force_process_input() {
	process_events(); // get rid of pending events
}

void OS_Windows::run() {
	if (!main_loop)
		return;

	main_loop->init();

	while (!force_quit) {
		process_events(); // get rid of pending events
		if (Main::iteration())
			break;
	};

	main_loop->finish();
}

MainLoop *OS_Windows::get_main_loop() const {
	return main_loop;
}

uint64_t OS_Windows::get_embedded_pck_offset() const {
	FileAccessRef f = FileAccess::open(get_executable_path(), FileAccess::READ);
	if (!f) {
		return 0;
	}

	// Process header.
	{
		f->seek(0x3c);
		uint32_t pe_pos = f->get_32();

		f->seek(pe_pos);
		uint32_t magic = f->get_32();
		if (magic != 0x00004550) {
			return 0;
		}
	}

	int num_sections;
	{
		int64_t header_pos = f->get_position();

		f->seek(header_pos + 2);
		num_sections = f->get_16();
		f->seek(header_pos + 16);
		uint16_t opt_header_size = f->get_16();

		// Skip rest of header + optional header to go to the section headers.
		f->seek(f->get_position() + 2 + opt_header_size);
	}
	int64_t section_table_pos = f->get_position();

	// Search for the "pck" section.
	int64_t off = 0;
	for (int i = 0; i < num_sections; ++i) {
		int64_t section_header_pos = section_table_pos + i * 40;
		f->seek(section_header_pos);

		uint8_t section_name[9];
		f->get_buffer(section_name, 8);
		section_name[8] = '\0';

		if (strcmp((char *)section_name, "pck") == 0) {
			f->seek(section_header_pos + 20);
			off = f->get_32();
			break;
		}
	}

	return off;
}

String OS_Windows::get_config_path() const {
	if (has_environment("APPDATA")) {
		return get_environment("APPDATA").replace("\\", "/");
	}
	return ".";
}

String OS_Windows::get_data_path() const {
	return get_config_path();
}

String OS_Windows::get_cache_path() const {
	static String cache_path_cache;
	if (cache_path_cache == String()) {
		if (has_environment("LOCALAPPDATA")) {
			cache_path_cache = get_environment("LOCALAPPDATA").replace("\\", "/");
		}
		if (cache_path_cache == String() && has_environment("TEMP")) {
			cache_path_cache = get_environment("TEMP").replace("\\", "/");
		}
		if (cache_path_cache == String()) {
			cache_path_cache = get_config_path();
		}
	}
	return cache_path_cache;
}

// Get properly capitalized engine name for system paths
String OS_Windows::get_godot_dir_name() const {
	return String(VERSION_SHORT_NAME).capitalize();
}

String OS_Windows::get_system_dir(SystemDir p_dir, bool p_shared_storage) const {
	KNOWNFOLDERID id;

	switch (p_dir) {
		case SYSTEM_DIR_DESKTOP: {
			id = FOLDERID_Desktop;
		} break;
		case SYSTEM_DIR_DCIM: {
			id = FOLDERID_Pictures;
		} break;
		case SYSTEM_DIR_DOCUMENTS: {
			id = FOLDERID_Documents;
		} break;
		case SYSTEM_DIR_DOWNLOADS: {
			id = FOLDERID_Downloads;
		} break;
		case SYSTEM_DIR_MOVIES: {
			id = FOLDERID_Videos;
		} break;
		case SYSTEM_DIR_MUSIC: {
			id = FOLDERID_Music;
		} break;
		case SYSTEM_DIR_PICTURES: {
			id = FOLDERID_Pictures;
		} break;
		case SYSTEM_DIR_RINGTONES: {
			id = FOLDERID_Music;
		} break;
	}

	PWSTR szPath;
	HRESULT res = SHGetKnownFolderPath(id, 0, NULL, &szPath);
	ERR_FAIL_COND_V(res != S_OK, String());
	String path = String(szPath).replace("\\", "/");
	CoTaskMemFree(szPath);
	return path;
}

String OS_Windows::get_user_data_dir() const {
	String appname = get_safe_dir_name(ProjectSettings::get_singleton()->get("application/config/name"));
	if (appname != "") {
		bool use_custom_dir = ProjectSettings::get_singleton()->get("application/config/use_custom_user_dir");
		if (use_custom_dir) {
			String custom_dir = get_safe_dir_name(ProjectSettings::get_singleton()->get("application/config/custom_user_dir_name"), true);
			if (custom_dir == "") {
				custom_dir = appname;
			}
			return get_data_path().plus_file(custom_dir).replace("\\", "/");
		} else {
			return get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file(appname).replace("\\", "/");
		}
	}

	return get_data_path().plus_file(get_godot_dir_name()).plus_file("app_userdata").plus_file("[unnamed project]");
}

String OS_Windows::get_unique_id() const {
	HW_PROFILE_INFO HwProfInfo;
	ERR_FAIL_COND_V(!GetCurrentHwProfile(&HwProfInfo), "");
	return String(HwProfInfo.szHwProfileGuid);
}

void OS_Windows::set_ime_active(const bool p_active) {
	if (p_active) {
		ImmAssociateContext(hWnd, im_himc);

		set_ime_position(im_position);
	} else {
		ImmAssociateContext(hWnd, (HIMC)0);
	}
}

void OS_Windows::set_ime_position(const Point2 &p_pos) {
	im_position = p_pos;

	HIMC himc = ImmGetContext(hWnd);
	if (himc == (HIMC)0)
		return;

	COMPOSITIONFORM cps;
	cps.dwStyle = CFS_FORCE_POSITION;
	cps.ptCurrentPos.x = im_position.x;
	cps.ptCurrentPos.y = im_position.y;
	ImmSetCompositionWindow(himc, &cps);
	ImmReleaseContext(hWnd, himc);
}

bool OS_Windows::is_joy_known(int p_device) {
	return input->is_joy_mapped(p_device);
}

String OS_Windows::get_joy_guid(int p_device) const {
	return input->get_joy_guid_remapped(p_device);
}

void OS_Windows::_set_use_vsync(bool p_enable) {
	if (gl_context)
		gl_context->set_use_vsync(p_enable);
}
/*
bool OS_Windows::is_vsync_enabled() const {

	if (gl_context)
		return gl_context->is_using_vsync();

	return true;
}*/

OS::PowerState OS_Windows::get_power_state() {
	return power_manager->get_power_state();
}

int OS_Windows::get_power_seconds_left() {
	return power_manager->get_power_seconds_left();
}

int OS_Windows::get_power_percent_left() {
	return power_manager->get_power_percent_left();
}

bool OS_Windows::_check_internal_feature_support(const String &p_feature) {
	return p_feature == "pc";
}

void OS_Windows::disable_crash_handler() {
	crash_handler.disable();
}

bool OS_Windows::is_disable_crash_handler() const {
	return crash_handler.is_disabled();
}

void OS_Windows::process_and_drop_events() {
	drop_events = true;
	process_events();
	drop_events = false;
}

Error OS_Windows::move_to_trash(const String &p_path) {
	SHFILEOPSTRUCTW sf;
	WCHAR *from = new WCHAR[p_path.length() + 2];
	wcscpy_s(from, p_path.length() + 1, p_path.c_str());
	from[p_path.length() + 1] = 0;

	sf.hwnd = hWnd;
	sf.wFunc = FO_DELETE;
	sf.pFrom = from;
	sf.pTo = NULL;
	sf.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION;
	sf.fAnyOperationsAborted = FALSE;
	sf.hNameMappings = NULL;
	sf.lpszProgressTitle = NULL;

	int ret = SHFileOperationW(&sf);
	delete[] from;

	if (ret) {
		ERR_PRINT("SHFileOperation error: " + itos(ret));
		return FAILED;
	}

	return OK;
}

int OS_Windows::get_tablet_driver_count() const {
	return tablet_drivers.size();
}

String OS_Windows::get_tablet_driver_name(int p_driver) const {
	if (p_driver < 0 || p_driver >= tablet_drivers.size()) {
		return "";
	} else {
		return tablet_drivers[p_driver];
	}
}

String OS_Windows::get_current_tablet_driver() const {
	return tablet_driver;
}

void OS_Windows::set_current_tablet_driver(const String &p_driver) {
	if (get_tablet_driver_count() == 0) {
		return;
	}
	bool found = false;
	for (int i = 0; i < get_tablet_driver_count(); i++) {
		if (p_driver == get_tablet_driver_name(i)) {
			found = true;
		}
	}
	if (found) {
		if (hWnd) {
			block_mm = false;
			if ((tablet_driver == "wintab") && wintab_available && wtctx) {
				wintab_WTEnable(wtctx, false);
				wintab_WTClose(wtctx);
				wtctx = 0;
			}
			if ((p_driver == "wintab") && wintab_available) {
				wintab_WTInfo(WTI_DEFSYSCTX, 0, &wtlc);
				wtlc.lcOptions |= CXO_MESSAGES;
				wtlc.lcPktData = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE | PK_ORIENTATION;
				wtlc.lcMoveMask = PK_STATUS | PK_NORMAL_PRESSURE | PK_TANGENT_PRESSURE;
				wtlc.lcPktMode = 0;
				wtlc.lcOutOrgX = 0;
				wtlc.lcOutExtX = wtlc.lcInExtX;
				wtlc.lcOutOrgY = 0;
				wtlc.lcOutExtY = -wtlc.lcInExtY;
				wtctx = wintab_WTOpen(hWnd, &wtlc, false);
				if (wtctx) {
					wintab_WTEnable(wtctx, true);
					AXIS pressure;
					if (wintab_WTInfo(WTI_DEVICES + wtlc.lcDevice, DVC_NPRESSURE, &pressure)) {
						min_pressure = int(pressure.axMin);
						max_pressure = int(pressure.axMax);
					}
					AXIS orientation[3];
					if (wintab_WTInfo(WTI_DEVICES + wtlc.lcDevice, DVC_ORIENTATION, &orientation)) {
						tilt_supported = orientation[0].axResolution && orientation[1].axResolution;
					}
					wintab_WTEnable(wtctx, true);
				} else {
					print_verbose("WinTab context creation failed.");
				}
			}
		}
		tablet_driver = p_driver;
	} else {
		ERR_PRINT("Unknown tablet driver " + p_driver + ".");
	}
};

OS_Windows::OS_Windows(HINSTANCE _hInstance) {
	drop_events = false;
	key_event_pos = 0;
	layered_window = false;
	force_quit = false;
	alt_mem = false;
	gr_mem = false;
	shift_mem = false;
	control_mem = false;
	meta_mem = false;
	minimized = false;
	was_maximized = false;
	window_focused = true;

	//Note: Wacom WinTab driver API for pen input, for devices incompatible with Windows Ink.
	HMODULE wintab_lib = LoadLibraryW(L"wintab32.dll");
	if (wintab_lib) {
		wintab_WTOpen = (WTOpenPtr)GetProcAddress(wintab_lib, "WTOpenW");
		wintab_WTClose = (WTClosePtr)GetProcAddress(wintab_lib, "WTClose");
		wintab_WTInfo = (WTInfoPtr)GetProcAddress(wintab_lib, "WTInfoW");
		wintab_WTPacket = (WTPacketPtr)GetProcAddress(wintab_lib, "WTPacket");
		wintab_WTEnable = (WTEnablePtr)GetProcAddress(wintab_lib, "WTEnable");

		wintab_available = wintab_WTOpen && wintab_WTClose && wintab_WTInfo && wintab_WTPacket && wintab_WTEnable;
	}

	if (wintab_available) {
		tablet_drivers.push_back("wintab");
	}

	//Note: Windows Ink API for pen input, available on Windows 8+ only.
	HMODULE user32_lib = LoadLibraryW(L"user32.dll");
	if (user32_lib) {
		win8p_GetPointerType = (GetPointerTypePtr)GetProcAddress(user32_lib, "GetPointerType");
		win8p_GetPointerPenInfo = (GetPointerPenInfoPtr)GetProcAddress(user32_lib, "GetPointerPenInfo");

		winink_available = win8p_GetPointerType && win8p_GetPointerPenInfo;
	}

	if (winink_available) {
		tablet_drivers.push_back("winink");
	}

	hInstance = _hInstance;
	pressrc = 0;
	old_invalid = true;
	mouse_mode = MOUSE_MODE_VISIBLE;
#ifdef STDOUT_FILE
	stdo = fopen("stdout.txt", "wb");
#endif
	user_proc = NULL;

#ifdef WASAPI_ENABLED
	AudioDriverManager::add_driver(&driver_wasapi);
#endif
#ifdef XAUDIO2_ENABLED
	AudioDriverManager::add_driver(&driver_xaudio2);
#endif

	// Enable ANSI escape code support on Windows 10 v1607 (Anniversary Update) and later.
	// This lets the engine and projects use ANSI escape codes to color text just like on macOS and Linux.
	//
	// NOTE: The engine does not use ANSI escape codes to color error/warning messages; it uses Windows API calls instead.
	// Therefore, error/warning messages are still colored on Windows versions older than 10.
	HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD outMode = ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	if (!SetConsoleMode(stdoutHandle, outMode)) {
		// Windows 8.1 or below, or Windows 10 prior to Anniversary Update.
		print_verbose("Can't set the ENABLE_VIRTUAL_TERMINAL_PROCESSING Windows console mode.");
	}

	Vector<Logger *> loggers;
	loggers.push_back(memnew(WindowsTerminalLogger));
	_set_logger(memnew(CompositeLogger(loggers)));
}

OS_Windows::~OS_Windows() {
	if (wintab_available && wtctx) {
		wintab_WTClose(wtctx);
		wtctx = 0;
	}
#ifdef STDOUT_FILE
	fclose(stdo);
#endif
}
