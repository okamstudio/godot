/**************************************************************************/
/*  display_server_windows.h                                              */
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

#ifndef DISPLAY_SERVER_WINDOWS_H
#define DISPLAY_SERVER_WINDOWS_H

#include "crash_handler_windows.h"
#include "joypad_windows.h"
#include "key_mapping_windows.h"
#include "tts_windows.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/io/image.h"
#include "core/os/os.h"
#include "drivers/wasapi/audio_driver_wasapi.h"
#include "drivers/winmidi/midi_driver_winmidi.h"
#include "servers/audio_server.h"
#include "servers/display_server.h"
#include "servers/rendering/renderer_compositor.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering_server.h"

#ifdef XAUDIO2_ENABLED
#include "drivers/xaudio2/audio_driver_xaudio2.h"
#endif

#if defined(RD_ENABLED)
#include "servers/rendering/rendering_device.h"
#endif

#if defined(GLES3_ENABLED)
#include "gl_manager_windows_angle.h"
#include "gl_manager_windows_native.h"
#endif // GLES3_ENABLED

#include "native_menu_windows.h"

#include <io.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

// WinTab API
#define WT_PACKET 0x7FF0
#define WT_PROXIMITY 0x7FF5
#define WT_INFOCHANGE 0x7FF6
#define WT_CSRCHANGE 0x7FF7

#define WTI_DEFSYSCTX 4
#define WTI_DEVICES 100
#define DVC_NPRESSURE 15
#define DVC_TPRESSURE 16
#define DVC_ORIENTATION 17
#define DVC_ROTATION 18

#define CXO_MESSAGES 0x0004
#define PK_STATUS 0x0002
#define PK_NORMAL_PRESSURE 0x0400
#define PK_TANGENT_PRESSURE 0x0800
#define PK_ORIENTATION 0x1000

#define TPS_INVERT 0x0010 /* 1.1 */

typedef struct tagLOGCONTEXTW {
	WCHAR lcName[40];
	UINT lcOptions;
	UINT lcStatus;
	UINT lcLocks;
	UINT lcMsgBase;
	UINT lcDevice;
	UINT lcPktRate;
	DWORD lcPktData;
	DWORD lcPktMode;
	DWORD lcMoveMask;
	DWORD lcBtnDnMask;
	DWORD lcBtnUpMask;
	LONG lcInOrgX;
	LONG lcInOrgY;
	LONG lcInOrgZ;
	LONG lcInExtX;
	LONG lcInExtY;
	LONG lcInExtZ;
	LONG lcOutOrgX;
	LONG lcOutOrgY;
	LONG lcOutOrgZ;
	LONG lcOutExtX;
	LONG lcOutExtY;
	LONG lcOutExtZ;
	DWORD lcSensX;
	DWORD lcSensY;
	DWORD lcSensZ;
	BOOL lcSysMode;
	int lcSysOrgX;
	int lcSysOrgY;
	int lcSysExtX;
	int lcSysExtY;
	DWORD lcSysSensX;
	DWORD lcSysSensY;
} LOGCONTEXTW;

typedef struct tagAXIS {
	LONG axMin;
	LONG axMax;
	UINT axUnits;
	DWORD axResolution;
} AXIS;

typedef struct tagORIENTATION {
	int orAzimuth;
	int orAltitude;
	int orTwist;
} ORIENTATION;

typedef struct tagPACKET {
	int pkStatus;
	int pkNormalPressure;
	int pkTangentPressure;
	ORIENTATION pkOrientation;
} PACKET;

typedef HANDLE(WINAPI *WTOpenPtr)(HWND p_window, LOGCONTEXTW *p_ctx, BOOL p_enable);
typedef BOOL(WINAPI *WTClosePtr)(HANDLE p_ctx);
typedef UINT(WINAPI *WTInfoPtr)(UINT p_category, UINT p_index, LPVOID p_output);
typedef BOOL(WINAPI *WTPacketPtr)(HANDLE p_ctx, UINT p_param, LPVOID p_packets);
typedef BOOL(WINAPI *WTEnablePtr)(HANDLE p_ctx, BOOL p_enable);

enum PreferredAppMode {
	APPMODE_DEFAULT = 0,
	APPMODE_ALLOWDARK = 1,
	APPMODE_FORCEDARK = 2,
	APPMODE_FORCELIGHT = 3,
	APPMODE_MAX = 4
};

typedef const char *(CDECL *WineGetVersionPtr)(void);
typedef bool(WINAPI *ShouldAppsUseDarkModePtr)();
typedef DWORD(WINAPI *GetImmersiveColorFromColorSetExPtr)(UINT dwImmersiveColorSet, UINT dwImmersiveColorType, bool bIgnoreHighContrast, UINT dwHighContrastCacheMode);
typedef int(WINAPI *GetImmersiveColorTypeFromNamePtr)(const WCHAR *name);
typedef int(WINAPI *GetImmersiveUserColorSetPreferencePtr)(bool bForceCheckRegistry, bool bSkipCheckOnFail);
typedef HRESULT(WINAPI *RtlGetVersionPtr)(OSVERSIONINFOEXW *lpVersionInformation);
typedef bool(WINAPI *AllowDarkModeForAppPtr)(bool darkMode);
typedef PreferredAppMode(WINAPI *SetPreferredAppModePtr)(PreferredAppMode appMode);
typedef void(WINAPI *RefreshImmersiveColorPolicyStatePtr)();
typedef void(WINAPI *FlushMenuThemesPtr)();

// Windows Ink API
#ifndef POINTER_STRUCTURES

#define POINTER_STRUCTURES

typedef DWORD POINTER_INPUT_TYPE;
typedef UINT32 POINTER_FLAGS;
typedef UINT32 PEN_FLAGS;
typedef UINT32 PEN_MASK;

#ifndef PEN_FLAG_INVERTED
#define PEN_FLAG_INVERTED 0x00000002
#endif

#ifndef PEN_FLAG_ERASER
#define PEN_FLAG_ERASER 0x00000004
#endif

#ifndef PEN_MASK_PRESSURE
#define PEN_MASK_PRESSURE 0x00000001
#endif

#ifndef PEN_MASK_TILT_X
#define PEN_MASK_TILT_X 0x00000004
#endif

#ifndef PEN_MASK_TILT_Y
#define PEN_MASK_TILT_Y 0x00000008
#endif

#ifndef POINTER_MESSAGE_FLAG_FIRSTBUTTON
#define POINTER_MESSAGE_FLAG_FIRSTBUTTON 0x00000010
#endif

#ifndef POINTER_MESSAGE_FLAG_SECONDBUTTON
#define POINTER_MESSAGE_FLAG_SECONDBUTTON 0x00000020
#endif

#ifndef POINTER_MESSAGE_FLAG_THIRDBUTTON
#define POINTER_MESSAGE_FLAG_THIRDBUTTON 0x00000040
#endif

#ifndef POINTER_MESSAGE_FLAG_FOURTHBUTTON
#define POINTER_MESSAGE_FLAG_FOURTHBUTTON 0x00000080
#endif

#ifndef POINTER_MESSAGE_FLAG_FIFTHBUTTON
#define POINTER_MESSAGE_FLAG_FIFTHBUTTON 0x00000100
#endif

#ifndef IS_POINTER_FLAG_SET_WPARAM
#define IS_POINTER_FLAG_SET_WPARAM(wParam, flag) (((DWORD)HIWORD(wParam) & (flag)) == (flag))
#endif

#ifndef IS_POINTER_FIRSTBUTTON_WPARAM
#define IS_POINTER_FIRSTBUTTON_WPARAM(wParam) IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FIRSTBUTTON)
#endif

#ifndef IS_POINTER_SECONDBUTTON_WPARAM
#define IS_POINTER_SECONDBUTTON_WPARAM(wParam) IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_SECONDBUTTON)
#endif

#ifndef IS_POINTER_THIRDBUTTON_WPARAM
#define IS_POINTER_THIRDBUTTON_WPARAM(wParam) IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_THIRDBUTTON)
#endif

#ifndef IS_POINTER_FOURTHBUTTON_WPARAM
#define IS_POINTER_FOURTHBUTTON_WPARAM(wParam) IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FOURTHBUTTON)
#endif

#ifndef IS_POINTER_FIFTHBUTTON_WPARAM
#define IS_POINTER_FIFTHBUTTON_WPARAM(wParam) IS_POINTER_FLAG_SET_WPARAM(wParam, POINTER_MESSAGE_FLAG_FIFTHBUTTON)
#endif

#ifndef GET_POINTERID_WPARAM
#define GET_POINTERID_WPARAM(wParam) (LOWORD(wParam))
#endif

#if WINVER < 0x0602
enum tagPOINTER_INPUT_TYPE {
	PT_POINTER = 0x00000001,
	PT_TOUCH = 0x00000002,
	PT_PEN = 0x00000003,
	PT_MOUSE = 0x00000004,
	PT_TOUCHPAD = 0x00000005
};

typedef enum tagPOINTER_BUTTON_CHANGE_TYPE {
	POINTER_CHANGE_NONE,
	POINTER_CHANGE_FIRSTBUTTON_DOWN,
	POINTER_CHANGE_FIRSTBUTTON_UP,
	POINTER_CHANGE_SECONDBUTTON_DOWN,
	POINTER_CHANGE_SECONDBUTTON_UP,
	POINTER_CHANGE_THIRDBUTTON_DOWN,
	POINTER_CHANGE_THIRDBUTTON_UP,
	POINTER_CHANGE_FOURTHBUTTON_DOWN,
	POINTER_CHANGE_FOURTHBUTTON_UP,
	POINTER_CHANGE_FIFTHBUTTON_DOWN,
	POINTER_CHANGE_FIFTHBUTTON_UP,
} POINTER_BUTTON_CHANGE_TYPE;

typedef struct tagPOINTER_INFO {
	POINTER_INPUT_TYPE pointerType;
	UINT32 pointerId;
	UINT32 frameId;
	POINTER_FLAGS pointerFlags;
	HANDLE sourceDevice;
	HWND hwndTarget;
	POINT ptPixelLocation;
	POINT ptHimetricLocation;
	POINT ptPixelLocationRaw;
	POINT ptHimetricLocationRaw;
	DWORD dwTime;
	UINT32 historyCount;
	INT32 InputData;
	DWORD dwKeyStates;
	UINT64 PerformanceCount;
	POINTER_BUTTON_CHANGE_TYPE ButtonChangeType;
} POINTER_INFO;

typedef struct tagPOINTER_PEN_INFO {
	POINTER_INFO pointerInfo;
	PEN_FLAGS penFlags;
	PEN_MASK penMask;
	UINT32 pressure;
	UINT32 rotation;
	INT32 tiltX;
	INT32 tiltY;
} POINTER_PEN_INFO;
#endif

#endif //POINTER_STRUCTURES

#ifndef WM_POINTERUPDATE
#define WM_POINTERUPDATE 0x0245
#endif

#ifndef WM_POINTERENTER
#define WM_POINTERENTER 0x0249
#endif

#ifndef WM_POINTERLEAVE
#define WM_POINTERLEAVE 0x024A
#endif

#ifndef WM_POINTERDOWN
#define WM_POINTERDOWN 0x0246
#endif

#ifndef WM_POINTERUP
#define WM_POINTERUP 0x0247
#endif

typedef BOOL(WINAPI *GetPointerTypePtr)(uint32_t p_id, POINTER_INPUT_TYPE *p_type);
typedef BOOL(WINAPI *GetPointerPenInfoPtr)(uint32_t p_id, POINTER_PEN_INFO *p_pen_info);
typedef BOOL(WINAPI *LogicalToPhysicalPointForPerMonitorDPIPtr)(HWND hwnd, LPPOINT lpPoint);
typedef BOOL(WINAPI *PhysicalToLogicalPointForPerMonitorDPIPtr)(HWND hwnd, LPPOINT lpPoint);
typedef HRESULT(WINAPI *SHLoadIndirectStringPtr)(PCWSTR pszSource, PWSTR pszOutBuf, UINT cchOutBuf, void **ppvReserved);

typedef struct {
	BYTE bWidth; // Width, in pixels, of the image
	BYTE bHeight; // Height, in pixels, of the image
	BYTE bColorCount; // Number of colors in image (0 if >=8bpp)
	BYTE bReserved; // Reserved ( must be 0)
	WORD wPlanes; // Color Planes
	WORD wBitCount; // Bits per pixel
	DWORD dwBytesInRes; // How many bytes in this resource?
	DWORD dwImageOffset; // Where in the file is this image?
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct {
	WORD idReserved; // Reserved (must be 0)
	WORD idType; // Resource Type (1 for icons)
	WORD idCount; // How many images?
	ICONDIRENTRY idEntries[1]; // An entry for each image (idCount of 'em)
} ICONDIR, *LPICONDIR;

typedef enum _SHC_PROCESS_DPI_AWARENESS {
	SHC_PROCESS_DPI_UNAWARE = 0,
	SHC_PROCESS_SYSTEM_DPI_AWARE = 1,
	SHC_PROCESS_PER_MONITOR_DPI_AWARE = 2,
} SHC_PROCESS_DPI_AWARENESS;

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

class DropTargetWindows;

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

class DisplayServerWindows : public DisplayServer {
	// No need to register with GDCLASS, it's platform-specific and nothing is added.

	friend class DropTargetWindows;

	_THREAD_SAFE_CLASS_

	// UXTheme API
	static bool dark_title_available;
	static bool use_legacy_dark_mode_before_20H1;
	static bool ux_theme_available;
	static ShouldAppsUseDarkModePtr ShouldAppsUseDarkMode;
	static GetImmersiveColorFromColorSetExPtr GetImmersiveColorFromColorSetEx;
	static GetImmersiveColorTypeFromNamePtr GetImmersiveColorTypeFromName;
	static GetImmersiveUserColorSetPreferencePtr GetImmersiveUserColorSetPreference;

	// WinTab API
	static bool wintab_available;
	static WTOpenPtr wintab_WTOpen;
	static WTClosePtr wintab_WTClose;
	static WTInfoPtr wintab_WTInfo;
	static WTPacketPtr wintab_WTPacket;
	static WTEnablePtr wintab_WTEnable;

	// Windows Ink API
	static bool winink_available;
	static GetPointerTypePtr win8p_GetPointerType;
	static GetPointerPenInfoPtr win8p_GetPointerPenInfo;

	// DPI conversion API
	static LogicalToPhysicalPointForPerMonitorDPIPtr win81p_LogicalToPhysicalPointForPerMonitorDPI;
	static PhysicalToLogicalPointForPerMonitorDPIPtr win81p_PhysicalToLogicalPointForPerMonitorDPI;

	// Shell API
	static SHLoadIndirectStringPtr load_indirect_string;

	void _update_tablet_ctx(const String &p_old_driver, const String &p_new_driver);
	String tablet_driver;
	Vector<String> tablet_drivers;

	enum DriverID {
		DRIVER_ID_COMPAT_OPENGL3 = 1 << 0,
		DRIVER_ID_COMPAT_ANGLE_D3D11 = 1 << 1,
		DRIVER_ID_RD_VULKAN = 1 << 2,
		DRIVER_ID_RD_D3D12 = 1 << 3,
	};
	static BitField<DriverID> tested_drivers;

	enum TimerID {
		TIMER_ID_MOVE_REDRAW = 1,
		TIMER_ID_WINDOW_ACTIVATION = 2,
	};

	OSVERSIONINFOEXW os_ver;

	enum {
		KEY_EVENT_BUFFER_SIZE = 512
	};

	struct KeyEvent {
		WindowID window_id;
		bool alt, shift, control, meta, altgr;
		UINT uMsg;
		WPARAM wParam;
		LPARAM lParam;
	};

	WindowID window_mouseover_id = INVALID_WINDOW_ID;

	KeyEvent key_event_buffer[KEY_EVENT_BUFFER_SIZE];
	int key_event_pos;

	bool old_invalid;
	int old_x, old_y;
	Point2i center;

#if defined(GLES3_ENABLED)
	GLManagerANGLE_Windows *gl_manager_angle = nullptr;
	GLManagerNative_Windows *gl_manager_native = nullptr;
#endif

#if defined(RD_ENABLED)
	RenderingContextDriver *rendering_context = nullptr;
	RenderingDevice *rendering_device = nullptr;
#endif

	RBMap<int, Vector2> touch_state;

	int pressrc;
	HINSTANCE hInstance; // Holds The Instance Of The Application
	String rendering_driver;
	bool app_focused = false;
	bool keep_screen_on = false;
	HANDLE power_request;

	TTS_Windows *tts = nullptr;
	NativeMenuWindows *native_menu = nullptr;

	struct WindowData {
		HWND hWnd;

		Vector<Vector2> mpath;

		bool create_completed = false;
		bool pre_fs_valid = false;
		RECT pre_fs_rect;
		bool maximized = false;
		bool maximized_fs = false;
		bool minimized = false;
		bool fullscreen = false;
		bool multiwindow_fs = false;
		bool borderless = false;
		bool resizable = true;
		bool window_focused = false;
		int activate_state = 0;
		bool was_maximized_pre_fs = false;
		bool was_fullscreen_pre_min = false;
		bool first_activation_done = false;
		bool was_maximized = false;
		bool always_on_top = false;
		bool no_focus = false;
		bool exclusive = false;
		bool context_created = false;
		bool mpass = false;
		bool sharp_corners = false;
		bool hide_from_capture = false;

		// Used to transfer data between events using timer.
		WPARAM saved_wparam;
		LPARAM saved_lparam;

		// Timers.
		uint32_t move_timer_id = 0U;
		uint32_t activate_timer_id = 0U;

		HANDLE wtctx;
		LOGCONTEXTW wtlc;
		int min_pressure;
		int max_pressure;
		bool tilt_supported;
		bool pen_inverted = false;
		bool block_mm = false;

		int last_pressure_update;
		float last_pressure;
		Vector2 last_tilt;
		bool last_pen_inverted = false;

		Size2 min_size;
		Size2 max_size;
		int width = 0, height = 0;

		Size2 window_rect;
		Point2 last_pos;

		ObjectID instance_id;

		// IME
		HIMC im_himc;
		Vector2 im_position;
		bool ime_active = false;
		bool ime_in_progress = false;
		bool ime_suppress_next_keyup = false;

		bool layered_window = false;

		Callable rect_changed_callback;
		Callable event_callback;
		Callable input_event_callback;
		Callable input_text_callback;
		Callable drop_files_callback;

		// OLE API
		DropTargetWindows *drop_target = nullptr;

		WindowID transient_parent = INVALID_WINDOW_ID;
		HashSet<WindowID> transient_children;

		bool is_popup = false;
		Rect2i parent_safe_rect;

		bool initialized = false;

		HWND parent_hwnd = 0;
	};

	JoypadWindows *joypad = nullptr;
	HHOOK mouse_monitor = nullptr;
	List<WindowID> popup_list;
	uint64_t time_since_popup = 0;
	Ref<Image> icon;

	WindowID _create_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect, bool p_exclusive, WindowID p_transient_parent, HWND p_parent_hwnd);
	WindowID window_id_counter = MAIN_WINDOW_ID;
	RBMap<WindowID, WindowData> windows;

	WindowID last_focused_window = INVALID_WINDOW_ID;
	WindowID last_mouse_button_down_window = INVALID_WINDOW_ID;
	HCURSOR hCursor;

	WNDPROC user_proc = nullptr;

	struct IndicatorData {
		RID menu_rid;
		Callable callback;
	};

	IndicatorID indicator_id_counter = 0;
	HashMap<IndicatorID, IndicatorData> indicators;

	struct FileDialogData {
		HWND hwnd_owner = 0;
		Rect2i wrect;
		String appid;
		String title;
		String current_directory;
		String root;
		String filename;
		bool show_hidden = false;
		DisplayServer::FileDialogMode mode = FileDialogMode::FILE_DIALOG_MODE_OPEN_ANY;
		Vector<String> filters;
		TypedArray<Dictionary> options;
		WindowID window_id = DisplayServer::INVALID_WINDOW_ID;
		Callable callback;
		bool options_in_cb = false;
		Thread listener_thread;
		SafeFlag close_requested;
		SafeFlag finished;
	};
	Mutex file_dialog_mutex;
	List<FileDialogData *> file_dialogs;
	HashMap<HWND, FileDialogData *> file_dialog_wnd;
	struct FileDialogCallback {
		Callable callback;
		Variant status;
		Variant files;
		Variant index;
		Variant options;
		bool opt_in_cb = false;
	};
	List<FileDialogCallback> pending_cbs;
	void process_file_dialog_callbacks();

	static void _thread_fd_monitor(void *p_ud);

	HashMap<int64_t, MouseButton> pointer_prev_button;
	HashMap<int64_t, MouseButton> pointer_button;
	HashMap<int64_t, LONG> pointer_down_time;
	HashMap<int64_t, Vector2> pointer_last_pos;

	void _send_window_event(const WindowData &wd, WindowEvent p_event);
	void _get_window_style(bool p_main_window, bool p_initialized, bool p_fullscreen, bool p_multiwindow_fs, bool p_borderless, bool p_resizable, bool p_minimized, bool p_maximized, bool p_maximized_fs, bool p_no_activate_focus, bool p_embed_child, DWORD &r_style, DWORD &r_style_ex);

	MouseMode mouse_mode;
	int restore_mouse_trails = 0;

	bool use_raw_input = false;
	bool drop_events = false;
	bool in_dispatch_input_event = false;

	WNDCLASSEXW wc;
	HBRUSH window_bkg_brush = nullptr;
	uint32_t window_bkg_brush_color = 0;

	HCURSOR cursors[CURSOR_MAX] = { nullptr };
	CursorShape cursor_shape = CursorShape::CURSOR_ARROW;
	RBMap<CursorShape, Vector<Variant>> cursors_cache;

	Callable system_theme_changed;

	void _drag_event(WindowID p_window, float p_x, float p_y, int idx);
	void _touch_event(WindowID p_window, bool p_pressed, float p_x, float p_y, int idx);

	bool _is_always_on_top_recursive(WindowID p_window) const;

	void _update_window_style(WindowID p_window, bool p_repaint = true);
	void _update_window_mouse_passthrough(WindowID p_window);

	void _update_real_mouse_position(WindowID p_window);

	void _set_mouse_mode_impl(MouseMode p_mode);
	WindowID _get_focused_window_or_popup() const;
	void _register_raw_input_devices(WindowID p_target_window);

	void _process_activate_event(WindowID p_window_id);
	void _process_key_events();

	static void _dispatch_input_events(const Ref<InputEvent> &p_event);
	void _dispatch_input_event(const Ref<InputEvent> &p_event);

	LRESULT _handle_early_window_message(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	Point2i _get_screens_origin() const;

	enum class WinKeyModifierMask {
		ALT_GR = (1 << 1),
		SHIFT = (1 << 2),
		ALT = (1 << 3),
		META = (1 << 4),
		CTRL = (1 << 5),
	};
	BitField<WinKeyModifierMask> _get_mods() const;

	Error _file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback, bool p_options_in_cb);

	String _get_keyboard_layout_display_name(const String &p_klid) const;
	String _get_klid(HKL p_hkl) const;

	struct EmbeddedProcessData {
		HWND window_handle = 0;
		HWND parent_window_handle = 0;
		bool is_visible = false;
	};
	HashMap<OS::ProcessID, EmbeddedProcessData *> embedded_processes;

	HWND _find_window_from_process_id(OS::ProcessID p_pid, HWND p_current_hwnd);

public:
	LRESULT WndProcFileDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT MouseProc(int code, WPARAM wParam, LPARAM lParam);

	void popup_open(WindowID p_window);
	void popup_close(WindowID p_window);

	virtual bool has_feature(Feature p_feature) const override;
	virtual String get_name() const override;

	virtual bool tts_is_speaking() const override;
	virtual bool tts_is_paused() const override;
	virtual TypedArray<Dictionary> tts_get_voices() const override;

	virtual void tts_speak(const String &p_text, const String &p_voice, int p_volume = 50, float p_pitch = 1.f, float p_rate = 1.f, int p_utterance_id = 0, bool p_interrupt = false) override;
	virtual void tts_pause() override;
	virtual void tts_resume() override;
	virtual void tts_stop() override;

	virtual bool is_dark_mode_supported() const override;
	virtual bool is_dark_mode() const override;
	virtual Color get_accent_color() const override;
	virtual Color get_base_color() const override;
	virtual void set_system_theme_change_callback(const Callable &p_callable) override;

	virtual Error file_dialog_show(const String &p_title, const String &p_current_directory, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const Callable &p_callback) override;
	virtual Error file_dialog_with_options_show(const String &p_title, const String &p_current_directory, const String &p_root, const String &p_filename, bool p_show_hidden, FileDialogMode p_mode, const Vector<String> &p_filters, const TypedArray<Dictionary> &p_options, const Callable &p_callback) override;

	virtual void beep() const override;

	virtual void mouse_set_mode(MouseMode p_mode) override;
	virtual MouseMode mouse_get_mode() const override;

	virtual void warp_mouse(const Point2i &p_position) override;
	virtual Point2i mouse_get_position() const override;
	virtual BitField<MouseButtonMask> mouse_get_button_state() const override;

	virtual void clipboard_set(const String &p_text) override;
	virtual String clipboard_get() const override;
	virtual Ref<Image> clipboard_get_image() const override;
	virtual bool clipboard_has() const override;
	virtual bool clipboard_has_image() const override;

	virtual int get_screen_count() const override;
	virtual int get_primary_screen() const override;
	virtual int get_keyboard_focus_screen() const override;
	virtual Point2i screen_get_position(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Size2i screen_get_size(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Rect2i screen_get_usable_rect(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual int screen_get_dpi(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual float screen_get_refresh_rate(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Color screen_get_pixel(const Point2i &p_position) const override;
	virtual Ref<Image> screen_get_image(int p_screen = SCREEN_OF_MAIN_WINDOW) const override;
	virtual Ref<Image> screen_get_image_rect(const Rect2i &p_rect) const override;

	virtual void screen_set_keep_on(bool p_enable) override; //disable screensaver
	virtual bool screen_is_kept_on() const override;

	virtual Vector<DisplayServer::WindowID> get_window_list() const override;

	virtual WindowID create_sub_window(WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Rect2i &p_rect = Rect2i(), bool p_exclusive = false, WindowID p_transient_parent = INVALID_WINDOW_ID) override;
	virtual void show_window(WindowID p_window) override;
	virtual void delete_sub_window(WindowID p_window) override;

	virtual WindowID window_get_active_popup() const override;
	virtual void window_set_popup_safe_rect(WindowID p_window, const Rect2i &p_rect) override;
	virtual Rect2i window_get_popup_safe_rect(WindowID p_window) const override;

	virtual int64_t window_get_native_handle(HandleType p_handle_type, WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual WindowID get_window_at_screen_position(const Point2i &p_position) const override;

	virtual void window_attach_instance_id(ObjectID p_instance, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual ObjectID window_get_attached_instance_id(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void gl_window_make_current(DisplayServer::WindowID p_window_id) override;

	virtual void window_set_rect_changed_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_window_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_event_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_input_text_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_drop_files_callback(const Callable &p_callable, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_title(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_title_size(const String &p_title, WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_mouse_passthrough(const Vector<Vector2> &p_region, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual int window_get_current_screen(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_current_screen(int p_screen, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual Point2i window_get_position(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual Point2i window_get_position_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual void window_set_position(const Point2i &p_position, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void window_set_transient(WindowID p_window, WindowID p_parent) override;
	virtual void window_set_exclusive(WindowID p_window, bool p_exclusive) override;

	virtual void window_set_max_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_max_size(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_min_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_min_size(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_size(const Size2i p_size, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual Size2i window_get_size(WindowID p_window = MAIN_WINDOW_ID) const override;
	virtual Size2i window_get_size_with_decorations(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_mode(WindowMode p_mode, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual WindowMode window_get_mode(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool window_is_maximize_allowed(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_set_flag(WindowFlags p_flag, bool p_enabled, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual bool window_get_flag(WindowFlags p_flag, WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual void window_request_attention(WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_move_to_foreground(WindowID p_window = MAIN_WINDOW_ID) override;
	virtual bool window_is_focused(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual WindowID get_focused_window() const override;

	virtual bool window_can_draw(WindowID p_window = MAIN_WINDOW_ID) const override;

	virtual bool can_any_window_draw() const override;

	virtual void window_set_ime_active(const bool p_active, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_set_ime_position(const Point2i &p_pos, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual Point2i ime_get_selection() const override;
	virtual String ime_get_text() const override;

	virtual void window_set_vsync_mode(DisplayServer::VSyncMode p_vsync_mode, WindowID p_window = MAIN_WINDOW_ID) override;
	virtual DisplayServer::VSyncMode window_get_vsync_mode(WindowID p_vsync_mode) const override;

	virtual void window_start_drag(WindowID p_window = MAIN_WINDOW_ID) override;
	virtual void window_start_resize(WindowResizeEdge p_edge, WindowID p_window = MAIN_WINDOW_ID) override;

	virtual void cursor_set_shape(CursorShape p_shape) override;
	virtual CursorShape cursor_get_shape() const override;
	virtual void cursor_set_custom_image(const Ref<Resource> &p_cursor, CursorShape p_shape = CURSOR_ARROW, const Vector2 &p_hotspot = Vector2()) override;

	virtual bool get_swap_cancel_ok() override;

	virtual void enable_for_stealing_focus(OS::ProcessID pid) override;
	virtual Error embed_process(WindowID p_window, OS::ProcessID p_pid, const Rect2i &p_rect, bool p_visible, bool p_grab_focus) override;
	virtual Error remove_embedded_process(OS::ProcessID p_pid) override;
	virtual OS::ProcessID get_focused_process_id() override;

	virtual Error dialog_show(String p_title, String p_description, Vector<String> p_buttons, const Callable &p_callback) override;
	virtual Error dialog_input_text(String p_title, String p_description, String p_partial, const Callable &p_callback) override;

	virtual int keyboard_get_layout_count() const override;
	virtual int keyboard_get_current_layout() const override;
	virtual void keyboard_set_current_layout(int p_index) override;
	virtual String keyboard_get_layout_language(int p_index) const override;
	virtual String keyboard_get_layout_name(int p_index) const override;
	virtual Key keyboard_get_keycode_from_physical(Key p_keycode) const override;
	virtual Key keyboard_get_label_from_physical(Key p_keycode) const override;
	virtual void show_emoji_and_symbol_picker() const override;

	virtual int tablet_get_driver_count() const override;
	virtual String tablet_get_driver_name(int p_driver) const override;
	virtual String tablet_get_current_driver() const override;
	virtual void tablet_set_current_driver(const String &p_driver) override;

	virtual void process_events() override;

	virtual void force_process_and_drop_events() override;

	virtual void release_rendering_thread() override;
	virtual void swap_buffers() override;

	virtual void set_native_icon(const String &p_filename) override;
	virtual void set_icon(const Ref<Image> &p_icon) override;

	virtual IndicatorID create_status_indicator(const Ref<Texture2D> &p_icon, const String &p_tooltip, const Callable &p_callback) override;
	virtual void status_indicator_set_icon(IndicatorID p_id, const Ref<Texture2D> &p_icon) override;
	virtual void status_indicator_set_tooltip(IndicatorID p_id, const String &p_tooltip) override;
	virtual void status_indicator_set_menu(IndicatorID p_id, const RID &p_rid) override;
	virtual void status_indicator_set_callback(IndicatorID p_id, const Callable &p_callback) override;
	virtual Rect2 status_indicator_get_rect(IndicatorID p_id) const override;
	virtual void delete_status_indicator(IndicatorID p_id) override;

	virtual void set_context(Context p_context) override;

	virtual bool is_window_transparency_available() const override;

	static DisplayServer *create_func(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	static Vector<String> get_rendering_drivers_func();
	static void register_windows_driver();

	DisplayServerWindows(const String &p_rendering_driver, WindowMode p_mode, VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error);
	~DisplayServerWindows();
};

#endif // DISPLAY_SERVER_WINDOWS_H
