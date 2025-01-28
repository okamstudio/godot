/**************************************************************************/
/*  main.cpp                                                              */
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

#include "main.h"

#include "core/config/project_settings.h"
#include "core/core_globals.h"
#include "core/crypto/crypto.h"
#include "core/debugger/engine_debugger.h"
#include "core/extension/extension_api_dump.h"
#include "core/extension/gdextension_interface_dump.gen.h"
#include "core/extension/gdextension_manager.h"
#include "core/input/input.h"
#include "core/input/input_map.h"
#include "core/io/dir_access.h"
#include "core/io/file_access_pack.h"
#include "core/io/file_access_zip.h"
#include "core/io/image.h"
#include "core/io/image_loader.h"
#include "core/io/ip.h"
#include "core/io/resource_loader.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/register_core_types.h"
#include "core/string/translation_server.h"
#include "core/version.h"
#include "drivers/register_driver_types.h"
#include "main/app_icon.gen.h"
#include "main/main_timer_sync.h"
#include "main/performance.h"
#include "main/splash.gen.h"
#include "modules/register_module_types.h"
#include "platform/register_platform_apis.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/property_list_helper.h"
#include "scene/register_scene_types.h"
#include "scene/resources/packed_scene.h"
#include "scene/theme/theme_db.h"
#include "servers/audio/audio_driver_dummy.h"
#include "servers/audio_server.h"
#include "servers/camera_server.h"
#include "servers/display_server.h"
#include "servers/movie_writer/movie_writer.h"
#include "servers/movie_writer/movie_writer_mjpeg.h"
#include "servers/navigation_server_3d.h"
#include "servers/navigation_server_3d_dummy.h"
#include "servers/register_server_types.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/text/text_server_dummy.h"
#include "servers/text_server.h"

// 2D
#include "servers/navigation_server_2d.h"
#include "servers/navigation_server_2d_dummy.h"
#include "servers/physics_server_2d.h"
#include "servers/physics_server_2d_dummy.h"

#ifndef _3D_DISABLED
#include "servers/physics_server_3d.h"
#include "servers/physics_server_3d_dummy.h"
#include "servers/xr_server.h"
#endif // _3D_DISABLED

#ifdef TESTS_ENABLED
#include "tests/test_main.h"
#endif

#ifdef TOOLS_ENABLED
#include "editor/debugger/debug_adapter/debug_adapter_server.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/doc_data_class_path.gen.h"
#include "editor/doc_tools.h"
#include "editor/editor_file_system.h"
#include "editor/editor_help.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/editor_settings.h"
#include "editor/editor_translation.h"
#include "editor/progress_dialog.h"
#include "editor/project_manager.h"
#include "editor/register_editor_types.h"

#if defined(TOOLS_ENABLED) && !defined(NO_EDITOR_SPLASH)
#include "main/splash_editor.gen.h"
#endif

#ifndef DISABLE_DEPRECATED
#include "editor/project_converter_3_to_4.h"
#endif // DISABLE_DEPRECATED
#endif // TOOLS_ENABLED

#if defined(STEAMAPI_ENABLED)
#include "main/steam_tracker.h"
#endif

#include "modules/modules_enabled.gen.h" // For mono.

#if defined(MODULE_MONO_ENABLED) && defined(TOOLS_ENABLED)
#include "modules/mono/editor/bindings_generator.h"
#endif

#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/gdscript.h"
#if defined(TOOLS_ENABLED) && !defined(GDSCRIPT_NO_LSP)
#include "modules/gdscript/language_server/gdscript_language_server.h"
#endif // TOOLS_ENABLED && !GDSCRIPT_NO_LSP
#endif // MODULE_GDSCRIPT_ENABLED

/* Static members */

// Singletons

// Initialized in setup()
static Engine *engine = nullptr;
static ProjectSettings *globals = nullptr;
static Input *input = nullptr;
static InputMap *input_map = nullptr;
static TranslationServer *translation_server = nullptr;
static Performance *performance = nullptr;
static PackedData *packed_data = nullptr;
#ifdef MINIZIP_ENABLED
static ZipArchive *zip_packed_data = nullptr;
#endif
static MessageQueue *message_queue = nullptr;

#if defined(STEAMAPI_ENABLED)
static SteamTracker *steam_tracker = nullptr;
#endif

// Initialized in setup2()
static AudioServer *audio_server = nullptr;
static CameraServer *camera_server = nullptr;
static DisplayServer *display_server = nullptr;
static RenderingServer *rendering_server = nullptr;
static TextServerManager *tsman = nullptr;
static ThemeDB *theme_db = nullptr;
static PhysicsServer2DManager *physics_server_2d_manager = nullptr;
static PhysicsServer2D *physics_server_2d = nullptr;
#ifndef _3D_DISABLED
static PhysicsServer3DManager *physics_server_3d_manager = nullptr;
static PhysicsServer3D *physics_server_3d = nullptr;
static XRServer *xr_server = nullptr;
#endif // _3D_DISABLED
// We error out if setup2() doesn't turn this true
static bool _start_success = false;

// Drivers

String display_driver = "";
String tablet_driver = "";
String text_driver = "";
String rendering_driver = "";
String rendering_method = "";
static int text_driver_idx = -1;
static int audio_driver_idx = -1;

// Engine config/tools

static bool single_window = false;
static bool editor = false;
static bool project_manager = false;
static bool cmdline_tool = false;
static String locale;
static String log_file;
static bool show_help = false;
static uint64_t quit_after = 0;
static OS::ProcessID editor_pid = 0;
#ifdef TOOLS_ENABLED
static bool found_project = false;
static bool recovery_mode = false;
static bool auto_build_solutions = false;
static String debug_server_uri;
static bool wait_for_import = false;
static bool restore_editor_window_layout = true;
#ifndef DISABLE_DEPRECATED
static int converter_max_kb_file = 4 * 1024; // 4MB
static int converter_max_line_length = 100000;
#endif // DISABLE_DEPRECATED

HashMap<Main::CLIScope, Vector<String>> forwardable_cli_arguments;
#endif
static bool single_threaded_scene = false;

// Display

static DisplayServer::WindowMode window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
static DisplayServer::ScreenOrientation window_orientation = DisplayServer::SCREEN_LANDSCAPE;
static DisplayServer::VSyncMode window_vsync_mode = DisplayServer::VSYNC_ENABLED;
static uint32_t window_flags = 0;
static Size2i window_size = Size2i(1152, 648);

static int init_screen = DisplayServer::SCREEN_PRIMARY;
static bool init_fullscreen = false;
static bool init_maximized = false;
static bool init_windowed = false;
static bool init_always_on_top = false;
static bool init_use_custom_pos = false;
static bool init_use_custom_screen = false;
static Vector2 init_custom_pos;
static int64_t init_embed_parent_window_id = 0;
static bool use_custom_res = true;
static bool force_res = false;

// Debug

static bool use_debug_profiler = false;
#ifdef DEBUG_ENABLED
static bool debug_collisions = false;
static bool debug_paths = false;
static bool debug_navigation = false;
static bool debug_avoidance = false;
static bool debug_canvas_item_redraw = false;
#endif
static int max_fps = -1;
static int frame_delay = 0;
static int audio_output_latency = 0;
static bool disable_render_loop = false;
static int fixed_fps = -1;
static MovieWriter *movie_writer = nullptr;
static bool disable_vsync = false;
static bool print_fps = false;
#ifdef TOOLS_ENABLED
static bool editor_pseudolocalization = false;
static bool dump_gdextension_interface = false;
static bool dump_extension_api = false;
static bool include_docs_in_extension_api_dump = false;
static bool validate_extension_api = false;
static String validate_extension_api_file;
#endif
bool profile_gpu = false;

// Constants.

static const String NULL_DISPLAY_DRIVER("headless");
static const String NULL_AUDIO_DRIVER("Dummy");

// The length of the longest column in the command-line help we should align to
// (excluding the 2-space left and right margins).
// Currently, this is `--export-release <preset> <path>`.
static const int OPTION_COLUMN_LENGTH = 32;

/* Helper methods */

bool Main::is_cmdline_tool() {
	return cmdline_tool;
}

#ifdef TOOLS_ENABLED
const Vector<String> &Main::get_forwardable_cli_arguments(Main::CLIScope p_scope) {
	return forwardable_cli_arguments[p_scope];
}
#endif

static String unescape_cmdline(const String &p_str) {
	return p_str.replace("%20", " ");
}

static String get_full_version_string() {
	String hash = String(VERSION_HASH);
	if (!hash.is_empty()) {
		hash = "." + hash.left(9);
	}
	return String(VERSION_FULL_BUILD) + hash;
}

#if defined(TOOLS_ENABLED) && defined(MODULE_GDSCRIPT_ENABLED)
static Vector<String> get_files_with_extension(const String &p_root, const String &p_extension) {
	Vector<String> paths;

	Ref<DirAccess> dir = DirAccess::open(p_root);
	if (dir.is_valid()) {
		dir->list_dir_begin();
		String fn = dir->get_next();
		while (!fn.is_empty()) {
			if (!dir->current_is_hidden() && fn != "." && fn != "..") {
				if (dir->current_is_dir()) {
					paths.append_array(get_files_with_extension(p_root.path_join(fn), p_extension));
				} else if (fn.get_extension() == p_extension) {
					paths.append(p_root.path_join(fn));
				}
			}
			fn = dir->get_next();
		}
		dir->list_dir_end();
	}

	return paths;
}
#endif

// FIXME: Could maybe be moved to have less code in main.cpp.
void initialize_physics() {
#ifndef _3D_DISABLED
	/// 3D Physics Server
	physics_server_3d = PhysicsServer3DManager::get_singleton()->new_server(
			GLOBAL_GET(PhysicsServer3DManager::setting_property_name));
	if (!physics_server_3d) {
		// Physics server not found, Use the default physics
		physics_server_3d = PhysicsServer3DManager::get_singleton()->new_default_server();
	}

	// Fall back to dummy if no default server has been registered.
	if (!physics_server_3d) {
		WARN_PRINT(vformat("Falling back to dummy PhysicsServer3D; 3D physics functionality will be disabled. If this is intended, set the %s project setting to Dummy.", PhysicsServer3DManager::setting_property_name));
		physics_server_3d = memnew(PhysicsServer3DDummy);
	}

	// Should be impossible, but make sure it's not null.
	ERR_FAIL_NULL_MSG(physics_server_3d, "Failed to initialize PhysicsServer3D.");
	physics_server_3d->init();
#endif // _3D_DISABLED

	// 2D Physics server
	physics_server_2d = PhysicsServer2DManager::get_singleton()->new_server(
			GLOBAL_GET(PhysicsServer2DManager::get_singleton()->setting_property_name));
	if (!physics_server_2d) {
		// Physics server not found, Use the default physics
		physics_server_2d = PhysicsServer2DManager::get_singleton()->new_default_server();
	}

	// Fall back to dummy if no default server has been registered.
	if (!physics_server_2d) {
		WARN_PRINT(vformat("Falling back to dummy PhysicsServer2D; 2D physics functionality will be disabled. If this is intended, set the %s project setting to Dummy.", PhysicsServer2DManager::setting_property_name));
		physics_server_2d = memnew(PhysicsServer2DDummy);
	}

	// Should be impossible, but make sure it's not null.
	ERR_FAIL_NULL_MSG(physics_server_2d, "Failed to initialize PhysicsServer2D.");
	physics_server_2d->init();
}

void finalize_physics() {
#ifndef _3D_DISABLED
	physics_server_3d->finish();
	memdelete(physics_server_3d);
#endif // _3D_DISABLED

	physics_server_2d->finish();
	memdelete(physics_server_2d);
}

void finalize_display() {
	rendering_server->finish();
	memdelete(rendering_server);

	memdelete(display_server);
}

void initialize_theme_db() {
	theme_db = memnew(ThemeDB);
}

void finalize_theme_db() {
	memdelete(theme_db);
	theme_db = nullptr;
}

//#define DEBUG_INIT
#ifdef DEBUG_INIT
#define MAIN_PRINT(m_txt) print_line(m_txt)
#else
#define MAIN_PRINT(m_txt)
#endif

void Main::print_header(bool p_rich) {
	if (VERSION_TIMESTAMP > 0) {
		// Version timestamp available.
		if (p_rich) {
			Engine::get_singleton()->print_header_rich("\u001b[38;5;39m" + String(VERSION_NAME) + "\u001b[0m v" + get_full_version_string() + " (" + Time::get_singleton()->get_datetime_string_from_unix_time(VERSION_TIMESTAMP, true) + " UTC) - \u001b[4m" + String(VERSION_WEBSITE));
		} else {
			Engine::get_singleton()->print_header(String(VERSION_NAME) + " v" + get_full_version_string() + " (" + Time::get_singleton()->get_datetime_string_from_unix_time(VERSION_TIMESTAMP, true) + " UTC) - " + String(VERSION_WEBSITE));
		}
	} else {
		if (p_rich) {
			Engine::get_singleton()->print_header_rich("\u001b[38;5;39m" + String(VERSION_NAME) + "\u001b[0m v" + get_full_version_string() + " - \u001b[4m" + String(VERSION_WEBSITE));
		} else {
			Engine::get_singleton()->print_header(String(VERSION_NAME) + " v" + get_full_version_string() + " - " + String(VERSION_WEBSITE));
		}
	}
}

/**
 * Prints a copyright notice in the command-line help with colored text. A newline is
 * automatically added at the end.
 */
void Main::print_help_copyright(const char *p_notice) {
	OS::get_singleton()->print("\u001b[90m%s\u001b[0m\n", p_notice);
}

/**
 * Prints a title in the command-line help with colored text. A newline is
 * automatically added at beginning and at the end.
 */
void Main::print_help_title(const char *p_title) {
	OS::get_singleton()->print("\n\u001b[1;93m%s:\u001b[0m\n", p_title);
}

/**
 * Returns the option string with required and optional arguments colored separately from the rest of the option.
 * This color replacement must be done *after* calling `rpad()` for the length padding to be done correctly.
 */
String Main::format_help_option(const char *p_option) {
	return (String(p_option)
					.rpad(OPTION_COLUMN_LENGTH)
					.replace("[", "\u001b[96m[")
					.replace("]", "]\u001b[0m")
					.replace("<", "\u001b[95m<")
					.replace(">", ">\u001b[0m"));
}

/**
 * Prints an option in the command-line help with colored text. No newline is
 * added at the end. `p_availability` denotes which build types the argument is
 * available in. Support in release export templates implies support in debug
 * export templates and editor. Support in debug export templates implies
 * support in editor.
 */
void Main::print_help_option(const char *p_option, const char *p_description, CLIOptionAvailability p_availability) {
	const bool option_empty = (p_option && !p_option[0]);
	if (!option_empty) {
		const char *availability_badge = "";
		switch (p_availability) {
			case CLI_OPTION_AVAILABILITY_EDITOR:
				availability_badge = "\u001b[1;91mE";
				break;
			case CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG:
				availability_badge = "\u001b[1;94mD";
				break;
			case CLI_OPTION_AVAILABILITY_TEMPLATE_RELEASE:
				availability_badge = "\u001b[1;92mR";
				break;
			case CLI_OPTION_AVAILABILITY_HIDDEN:
				// Use for multiline option names (but not when the option name is empty).
				availability_badge = " ";
				break;
		}
		OS::get_singleton()->print(
				"  \u001b[92m%s  %s\u001b[0m  %s",
				format_help_option(p_option).utf8().ptr(),
				availability_badge,
				p_description);
	} else {
		// Make continuation lines for descriptions faint if the option name is empty.
		OS::get_singleton()->print(
				"  \u001b[92m%s   \u001b[0m  \u001b[90m%s",
				format_help_option(p_option).utf8().ptr(),
				p_description);
	}
}

void Main::print_help(const char *p_binary) {
	print_header(true);
	print_help_copyright("Free and open source software under the terms of the MIT license.");
	print_help_copyright("(c) 2014-present Godot Engine contributors. (c) 2007-present Juan Linietsky, Ariel Manzur.");

	print_help_title("Usage");
	OS::get_singleton()->print("  %s \u001b[96m[options] [path to scene or \"project.godot\" file]\u001b[0m\n", p_binary);

#if defined(TOOLS_ENABLED)
	print_help_title("Option legend (this build = editor)");
#elif defined(DEBUG_ENABLED)
	print_help_title("Option legend (this build = debug export template)");
#else
	print_help_title("Option legend (this build = release export template)");
#endif

	OS::get_singleton()->print("  \u001b[1;92mR\u001b[0m  Available in editor builds, debug export templates and release export templates.\n");
#ifdef DEBUG_ENABLED
	OS::get_singleton()->print("  \u001b[1;94mD\u001b[0m  Available in editor builds and debug export templates only.\n");
#endif
#ifdef TOOLS_ENABLED
	OS::get_singleton()->print("  \u001b[1;91mE\u001b[0m  Only available in editor builds.\n");
#endif

	print_help_title("General options");
	print_help_option("-h, --help", "Display this help message.\n");
	print_help_option("--version", "Display the version string.\n");
	print_help_option("-v, --verbose", "Use verbose stdout mode.\n");
	print_help_option("--quiet", "Quiet mode, silences stdout messages. Errors are still displayed.\n");
	print_help_option("--no-header", "Do not print engine version and rendering method header on startup.\n");

	print_help_title("Run options");
	print_help_option("--, ++", "Separator for user-provided arguments. Following arguments are not used by the engine, but can be read from `OS.get_cmdline_user_args()`.\n");
#ifdef TOOLS_ENABLED
	print_help_option("-e, --editor", "Start the editor instead of running the scene.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("-p, --project-manager", "Start the project manager, even if a project is auto-detected.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--recovery-mode", "Start the editor in recovery mode, which disables features that can typically cause startup crashes, such as tool scripts, editor plugins, GDExtension addons, and others.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--debug-server <uri>", "Start the editor debug server (<protocol>://<host/IP>[:port], e.g. tcp://127.0.0.1:6007)\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--dap-port <port>", "Use the specified port for the GDScript Debugger Adaptor protocol. Recommended port range [1024, 49151].\n", CLI_OPTION_AVAILABILITY_EDITOR);
#if defined(MODULE_GDSCRIPT_ENABLED) && !defined(GDSCRIPT_NO_LSP)
	print_help_option("--lsp-port <port>", "Use the specified port for the GDScript language server protocol. Recommended port range [1024, 49151].\n", CLI_OPTION_AVAILABILITY_EDITOR);
#endif // MODULE_GDSCRIPT_ENABLED && !GDSCRIPT_NO_LSP
#endif
	print_help_option("--quit", "Quit after the first iteration.\n");
	print_help_option("--quit-after <int>", "Quit after the given number of iterations. Set to 0 to disable.\n");
	print_help_option("-l, --language <locale>", "Use a specific locale (<locale> being a two-letter code).\n");
	print_help_option("--path <directory>", "Path to a project (<directory> must contain a \"project.godot\" file).\n");
	print_help_option("-u, --upwards", "Scan folders upwards for project.godot file.\n");
	print_help_option("--main-pack <file>", "Path to a pack (.pck) file to load.\n");
#ifdef DISABLE_DEPRECATED
	print_help_option("--render-thread <mode>", "Render thread mode (\"safe\", \"separate\").\n");
#else
	print_help_option("--render-thread <mode>", "Render thread mode (\"unsafe\" [deprecated], \"safe\", \"separate\").\n");
#endif
	print_help_option("--remote-fs <address>", "Remote filesystem (<host/IP>[:<port>] address).\n");
	print_help_option("--remote-fs-password <password>", "Password for remote filesystem.\n");

	print_help_option("--audio-driver <driver>", "Audio driver [");
	for (int i = 0; i < AudioDriverManager::get_driver_count(); i++) {
		if (i > 0) {
			OS::get_singleton()->print(", ");
		}
		OS::get_singleton()->print("\"%s\"", AudioDriverManager::get_driver(i)->get_name());
	}
	OS::get_singleton()->print("].\n");

	print_help_option("--display-driver <driver>", "Display driver (and rendering driver) [");
	for (int i = 0; i < DisplayServer::get_create_function_count(); i++) {
		if (i > 0) {
			OS::get_singleton()->print(", ");
		}
		OS::get_singleton()->print("\"%s\" (", DisplayServer::get_create_function_name(i));
		Vector<String> rd = DisplayServer::get_create_function_rendering_drivers(i);
		for (int j = 0; j < rd.size(); j++) {
			if (j > 0) {
				OS::get_singleton()->print(", ");
			}
			OS::get_singleton()->print("\"%s\"", rd[j].utf8().get_data());
		}
		OS::get_singleton()->print(")");
	}
	OS::get_singleton()->print("].\n");
	print_help_option("--audio-output-latency <ms>", "Override audio output latency in milliseconds (default is 15 ms).\n");
	print_help_option("", "Lower values make sound playback more reactive but increase CPU usage, and may result in audio cracking if the CPU can't keep up.\n");

	print_help_option("--rendering-method <renderer>", "Renderer name. Requires driver support.\n");
	print_help_option("--rendering-driver <driver>", "Rendering driver (depends on display driver).\n");
	print_help_option("--gpu-index <device_index>", "Use a specific GPU (run with --verbose to get a list of available devices).\n");
	print_help_option("--text-driver <driver>", "Text driver (used for font rendering, bidirectional support and shaping).\n");
	print_help_option("--tablet-driver <driver>", "Pen tablet input driver.\n");
	print_help_option("--headless", "Enable headless mode (--display-driver headless --audio-driver Dummy). Useful for servers and with --script.\n");
	print_help_option("--log-file <file>", "Write output/error log to the specified path instead of the default location defined by the project.\n");
	print_help_option("", "<file> path should be absolute or relative to the project directory.\n");
	print_help_option("--write-movie <file>", "Write a video to the specified path (usually with .avi or .png extension).\n");
	print_help_option("", "--fixed-fps is forced when enabled, but it can be used to change movie FPS.\n");
	print_help_option("", "--disable-vsync can speed up movie writing but makes interaction more difficult.\n");
	print_help_option("", "--quit-after can be used to specify the number of frames to write.\n");

	print_help_title("Display options");
	print_help_option("-f, --fullscreen", "Request fullscreen mode.\n");
	print_help_option("-m, --maximized", "Request a maximized window.\n");
	print_help_option("-w, --windowed", "Request windowed mode.\n");
	print_help_option("-t, --always-on-top", "Request an always-on-top window.\n");
	print_help_option("--resolution <W>x<H>", "Request window resolution.\n");
	print_help_option("--position <X>,<Y>", "Request window position.\n");
	print_help_option("--screen <N>", "Request window screen.\n");
	print_help_option("--single-window", "Use a single window (no separate subwindows).\n");
#ifndef _3D_DISABLED
	print_help_option("--xr-mode <mode>", "Select XR (Extended Reality) mode [\"default\", \"off\", \"on\"].\n");
#endif
	print_help_option("--wid <window_id>", "Request parented to window.\n");

	print_help_title("Debug options");
	print_help_option("-d, --debug", "Debug (local stdout debugger).\n");
	print_help_option("-b, --breakpoints", "Breakpoint list as source::line comma-separated pairs, no spaces (use %%20 instead).\n");
	print_help_option("--profiling", "Enable profiling in the script debugger.\n");
	print_help_option("--gpu-profile", "Show a GPU profile of the tasks that took the most time during frame rendering.\n");
	print_help_option("--gpu-validation", "Enable graphics API validation layers for debugging.\n");
#ifdef DEBUG_ENABLED
	print_help_option("--gpu-abort", "Abort on graphics API usage errors (usually validation layer errors). May help see the problem if your system freezes.\n", CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG);
#endif
	print_help_option("--generate-spirv-debug-info", "Generate SPIR-V debug information. This allows source-level shader debugging with RenderDoc.\n");
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
	print_help_option("--extra-gpu-memory-tracking", "Enables additional memory tracking (see class reference for `RenderingDevice.get_driver_and_device_memory_report()` and linked methods). Currently only implemented for Vulkan. Enabling this feature may cause crashes on some systems due to buggy drivers or bugs in the Vulkan Loader. See https://github.com/godotengine/godot/issues/95967\n");
	print_help_option("--accurate-breadcrumbs", "Force barriers between breadcrumbs. Useful for narrowing down a command causing GPU resets. Currently only implemented for Vulkan.\n");
#endif
	print_help_option("--remote-debug <uri>", "Remote debug (<protocol>://<host/IP>[:<port>], e.g. tcp://127.0.0.1:6007).\n");
	print_help_option("--single-threaded-scene", "Force scene tree to run in single-threaded mode. Sub-thread groups are disabled and run on the main thread.\n");
#if defined(DEBUG_ENABLED)
	print_help_option("--debug-collisions", "Show collision shapes when running the scene.\n", CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG);
	print_help_option("--debug-paths", "Show path lines when running the scene.\n", CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG);
	print_help_option("--debug-navigation", "Show navigation polygons when running the scene.\n", CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG);
	print_help_option("--debug-avoidance", "Show navigation avoidance debug visuals when running the scene.\n", CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG);
	print_help_option("--debug-stringnames", "Print all StringName allocations to stdout when the engine quits.\n", CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG);
	print_help_option("--debug-canvas-item-redraw", "Display a rectangle each time a canvas item requests a redraw (useful to troubleshoot low processor mode).\n", CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG);

#endif
	print_help_option("--max-fps <fps>", "Set a maximum number of frames per second rendered (can be used to limit power usage). A value of 0 results in unlimited framerate.\n");
	print_help_option("--frame-delay <ms>", "Simulate high CPU load (delay each frame by <ms> milliseconds). Do not use as a FPS limiter; use --max-fps instead.\n");
	print_help_option("--time-scale <scale>", "Force time scale (higher values are faster, 1.0 is normal speed).\n");
	print_help_option("--disable-vsync", "Forces disabling of vertical synchronization, even if enabled in the project settings. Does not override driver-level V-Sync enforcement.\n");
	print_help_option("--disable-render-loop", "Disable render loop so rendering only occurs when called explicitly from script.\n");
	print_help_option("--disable-crash-handler", "Disable crash handler when supported by the platform code.\n");
	print_help_option("--fixed-fps <fps>", "Force a fixed number of frames per second. This setting disables real-time synchronization.\n");
	print_help_option("--delta-smoothing <enable>", "Enable or disable frame delta smoothing [\"enable\", \"disable\"].\n");
	print_help_option("--print-fps", "Print the frames per second to the stdout.\n");
#ifdef TOOLS_ENABLED
	print_help_option("--editor-pseudolocalization", "Enable pseudolocalization for the editor and the project manager.\n", CLI_OPTION_AVAILABILITY_EDITOR);
#endif

	print_help_title("Standalone tools");
	print_help_option("-s, --script <script>", "Run a script.\n");
	print_help_option("--main-loop <main_loop_name>", "Run a MainLoop specified by its global class name.\n");
	print_help_option("--check-only", "Only parse for errors and quit (use with --script).\n");
#ifdef TOOLS_ENABLED
	print_help_option("--import", "Starts the editor, waits for any resources to be imported, and then quits.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--export-release <preset> <path>", "Export the project in release mode using the given preset and output path. The preset name should match one defined in \"export_presets.cfg\".\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("", "<path> should be absolute or relative to the project directory, and include the filename for the binary (e.g. \"builds/game.exe\").\n");
	print_help_option("", "The target directory must exist.\n");
	print_help_option("--export-debug <preset> <path>", "Export the project in debug mode using the given preset and output path. See --export-release description for other considerations.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--export-pack <preset> <path>", "Export the project data only using the given preset and output path. The <path> extension determines whether it will be in PCK or ZIP format.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--export-patch <preset> <path>", "Export pack with changed files only. See --export-pack description for other considerations.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--patches <paths>", "List of patches to use with --export-patch. The list is comma-separated.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--install-android-build-template", "Install the Android build template. Used in conjunction with --export-release or --export-debug.\n", CLI_OPTION_AVAILABILITY_EDITOR);
#ifndef DISABLE_DEPRECATED
	// Commands are long; split the description to a second line.
	print_help_option("--convert-3to4 ", "\n", CLI_OPTION_AVAILABILITY_HIDDEN);
	print_help_option("  [max_file_kb] [max_line_size]", "Converts project from Godot 3.x to Godot 4.x.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--validate-conversion-3to4 ", "\n", CLI_OPTION_AVAILABILITY_HIDDEN);
	print_help_option("  [max_file_kb] [max_line_size]", "Shows what elements will be renamed when converting project from Godot 3.x to Godot 4.x.\n", CLI_OPTION_AVAILABILITY_EDITOR);
#endif // DISABLE_DEPRECATED
	print_help_option("--doctool [path]", "Dump the engine API reference to the given <path> (defaults to current directory) in XML format, merging if existing files are found.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--no-docbase", "Disallow dumping the base types (used with --doctool).\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--gdextension-docs", "Rather than dumping the engine API, generate API reference from all the GDExtensions loaded in the current project (used with --doctool).\n", CLI_OPTION_AVAILABILITY_EDITOR);
#ifdef MODULE_GDSCRIPT_ENABLED
	print_help_option("--gdscript-docs <path>", "Rather than dumping the engine API, generate API reference from the inline documentation in the GDScript files found in <path> (used with --doctool).\n", CLI_OPTION_AVAILABILITY_EDITOR);
#endif
	print_help_option("--build-solutions", "Build the scripting solutions (e.g. for C# projects). Implies --editor and requires a valid project to edit.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--dump-gdextension-interface", "Generate a GDExtension header file \"gdextension_interface.h\" in the current folder. This file is the base file required to implement a GDExtension.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--dump-extension-api", "Generate a JSON dump of the Godot API for GDExtension bindings named \"extension_api.json\" in the current folder.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--dump-extension-api-with-docs", "Generate JSON dump of the Godot API like the previous option, but including documentation.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--validate-extension-api <path>", "Validate an extension API file dumped (with one of the two previous options) from a previous version of the engine to ensure API compatibility.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("", "If incompatibilities or errors are detected, the exit code will be non-zero.\n");
	print_help_option("--benchmark", "Benchmark the run time and print it to console.\n", CLI_OPTION_AVAILABILITY_EDITOR);
	print_help_option("--benchmark-file <path>", "Benchmark the run time and save it to a given file in JSON format. The path should be absolute.\n", CLI_OPTION_AVAILABILITY_EDITOR);
#ifdef TESTS_ENABLED
	print_help_option("--test [--help]", "Run unit tests. Use --test --help for more information.\n", CLI_OPTION_AVAILABILITY_EDITOR);
#endif
#endif
	OS::get_singleton()->print("\n");
}

#ifdef TESTS_ENABLED
// The order is the same as in `Main::setup()`, only core and some editor types
// are initialized here. This also combines `Main::setup2()` initialization.
Error Main::test_setup() {
	Thread::make_main_thread();
	set_current_thread_safe_for_nodes(true);

	OS::get_singleton()->initialize();

	engine = memnew(Engine);

	register_core_types();
	register_core_driver_types();

	packed_data = memnew(PackedData);

	globals = memnew(ProjectSettings);

	register_core_settings(); // Here globals are present.

	translation_server = memnew(TranslationServer);
	tsman = memnew(TextServerManager);

	if (tsman) {
		Ref<TextServerDummy> ts;
		ts.instantiate();
		tsman->add_interface(ts);
	}

#ifndef _3D_DISABLED
	physics_server_3d_manager = memnew(PhysicsServer3DManager);
#endif // _3D_DISABLED
	physics_server_2d_manager = memnew(PhysicsServer2DManager);

	// From `Main::setup2()`.
	register_early_core_singletons();
	initialize_modules(MODULE_INITIALIZATION_LEVEL_CORE);
	register_core_extensions();

	register_core_singletons();

	/** INITIALIZE SERVERS **/
	register_server_types();
#ifndef _3D_DISABLED
	XRServer::set_xr_mode(XRServer::XRMODE_OFF); // Skip in tests.
#endif // _3D_DISABLED
	initialize_modules(MODULE_INITIALIZATION_LEVEL_SERVERS);
	GDExtensionManager::get_singleton()->initialize_extensions(GDExtension::INITIALIZATION_LEVEL_SERVERS);

	translation_server->setup(); //register translations, load them, etc.
	if (!locale.is_empty()) {
		translation_server->set_locale(locale);
	}
	translation_server->load_translations();
	ResourceLoader::load_translation_remaps(); //load remaps for resources

	ResourceLoader::load_path_remaps();

	// Initialize ThemeDB early so that scene types can register their theme items.
	// Default theme will be initialized later, after modules and ScriptServer are ready.
	initialize_theme_db();

	NavigationServer3DManager::initialize_server(); // 3D server first because 2D depends on it.
	NavigationServer2DManager::initialize_server();

	register_scene_types();
	register_driver_types();

	register_scene_singletons();

	initialize_modules(MODULE_INITIALIZATION_LEVEL_SCENE);
	GDExtensionManager::get_singleton()->initialize_extensions(GDExtension::INITIALIZATION_LEVEL_SCENE);

#ifdef TOOLS_ENABLED
	ClassDB::set_current_api(ClassDB::API_EDITOR);
	register_editor_types();

	initialize_modules(MODULE_INITIALIZATION_LEVEL_EDITOR);
	GDExtensionManager::get_singleton()->initialize_extensions(GDExtension::INITIALIZATION_LEVEL_EDITOR);

	ClassDB::set_current_api(ClassDB::API_CORE);
#endif
	register_platform_apis();

	// Theme needs modules to be initialized so that sub-resources can be loaded.
	theme_db->initialize_theme_noproject();

	ERR_FAIL_COND_V(TextServerManager::get_singleton()->get_interface_count() == 0, ERR_CANT_CREATE);

	/* Use one with the most features available. */
	int max_features = 0;
	for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
		uint32_t features = TextServerManager::get_singleton()->get_interface(i)->get_features();
		int feature_number = 0;
		while (features) {
			feature_number += features & 1;
			features >>= 1;
		}
		if (feature_number >= max_features) {
			max_features = feature_number;
			text_driver_idx = i;
		}
	}
	if (text_driver_idx >= 0) {
		Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(text_driver_idx);
		TextServerManager::get_singleton()->set_primary_interface(ts);
		if (ts->has_feature(TextServer::FEATURE_USE_SUPPORT_DATA)) {
			ts->load_support_data("res://" + ts->get_support_data_filename());
		}
	} else {
		ERR_FAIL_V_MSG(ERR_CANT_CREATE, "TextServer: Unable to create TextServer interface.");
	}

	ClassDB::set_current_api(ClassDB::API_NONE);

	_start_success = true;

	return OK;
}

// The order is the same as in `Main::cleanup()`.
void Main::test_cleanup() {
	ERR_FAIL_COND(!_start_success);

	for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
		TextServerManager::get_singleton()->get_interface(i)->cleanup();
	}

	ResourceLoader::remove_custom_loaders();
	ResourceSaver::remove_custom_savers();
	PropertyListHelper::clear_base_helpers();

#ifdef TOOLS_ENABLED
	GDExtensionManager::get_singleton()->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_EDITOR);
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_EDITOR);
	unregister_editor_types();
#endif

	GDExtensionManager::get_singleton()->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_SCENE);
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SCENE);

	unregister_platform_apis();
	unregister_driver_types();
	unregister_scene_types();

	finalize_theme_db();

	NavigationServer2DManager::finalize_server(); // 2D goes first as it uses the 3D server behind the scene.
	NavigationServer3DManager::finalize_server();

	GDExtensionManager::get_singleton()->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_SERVERS);
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SERVERS);
	unregister_server_types();

	EngineDebugger::deinitialize();
	OS::get_singleton()->finalize();

	if (packed_data) {
		memdelete(packed_data);
	}
	if (translation_server) {
		memdelete(translation_server);
	}
	if (tsman) {
		memdelete(tsman);
	}
#ifndef _3D_DISABLED
	if (physics_server_3d_manager) {
		memdelete(physics_server_3d_manager);
	}
#endif // _3D_DISABLED
	if (physics_server_2d_manager) {
		memdelete(physics_server_2d_manager);
	}
	if (globals) {
		memdelete(globals);
	}

	unregister_core_driver_types();
	unregister_core_extensions();
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_CORE);

	if (engine) {
		memdelete(engine);
	}

	unregister_core_types();

	OS::get_singleton()->finalize_core();
}
#endif

int Main::test_entrypoint(int argc, char *argv[], bool &tests_need_run) {
	for (int x = 0; x < argc; x++) {
		if ((strncmp(argv[x], "--test", 6) == 0) && (strlen(argv[x]) == 6)) {
			tests_need_run = true;
#ifdef TESTS_ENABLED
			// TODO: need to come up with different test contexts.
			// Not every test requires high-level functionality like `ClassDB`.
			test_setup();
			int status = test_main(argc, argv);
			test_cleanup();
			return status;
#else
			ERR_PRINT(
					"`--test` was specified on the command line, but this Godot binary was compiled without support for unit tests. Aborting.\n"
					"To be able to run unit tests, use the `tests=yes` SCons option when compiling Godot.\n");
			return EXIT_FAILURE;
#endif
		}
	}
	tests_need_run = false;
	return EXIT_SUCCESS;
}

/* Engine initialization
 *
 * Consists of several methods that are called by each platform's specific main(argc, argv).
 * To fully understand engine init, one should therefore start from the platform's main and
 * see how it calls into the Main class' methods.
 *
 * The initialization is typically done in 3 steps (with the setup2 step triggered either
 * automatically by setup, or manually in the platform's main).
 *
 * - setup(execpath, argc, argv, p_second_phase) is the main entry point for all platforms,
 *   responsible for the initialization of all low level singletons and core types, and parsing
 *   command line arguments to configure things accordingly.
 *   If p_second_phase is true, it will chain into setup2() (default behavior). This is
 *   disabled on some platforms (Android, iOS) which trigger the second step in their own time.
 *
 * - setup2(p_main_tid_override) registers high level servers and singletons, displays the
 *   boot splash, then registers higher level types (scene, editor, etc.).
 *
 * - start() is the last step and that's where command line tools can run, or the main loop
 *   can be created eventually and the project settings put into action. That's also where
 *   the editor node is created, if relevant.
 *   start() does it own argument parsing for a subset of the command line arguments described
 *   in help, it's a bit messy and should be globalized with the setup() parsing somehow.
 */

Error Main::setup(const char *execpath, int argc, char *argv[], bool p_second_phase) {
	Thread::make_main_thread();
	set_current_thread_safe_for_nodes(true);

	OS::get_singleton()->initialize();

	// Benchmark tracking must be done after `OS::get_singleton()->initialize()` as on some
	// platforms, it's used to set up the time utilities.
	OS::get_singleton()->benchmark_begin_measure("Startup", "Main::Setup");

	engine = memnew(Engine);

	MAIN_PRINT("Main: Initialize CORE");

	register_core_types();
	register_core_driver_types();

	MAIN_PRINT("Main: Initialize Globals");

	input_map = memnew(InputMap);
	globals = memnew(ProjectSettings);

	register_core_settings(); //here globals are present

	translation_server = memnew(TranslationServer);
	performance = memnew(Performance);
	GDREGISTER_CLASS(Performance);
	engine->add_singleton(Engine::Singleton("Performance", performance));

	// Only flush stdout in debug builds by default, as spamming `print()` will
	// decrease performance if this is enabled.
	GLOBAL_DEF_RST("application/run/flush_stdout_on_print", false);
	GLOBAL_DEF_RST("application/run/flush_stdout_on_print.debug", true);

	MAIN_PRINT("Main: Parse CMDLine");

	/* argument parsing and main creation */
	List<String> args;
	List<String> main_args;
	List<String> user_args;
	bool adding_user_args = false;
	List<String> platform_args = OS::get_singleton()->get_cmdline_platform_args();

	// Add command line arguments.
	for (int i = 0; i < argc; i++) {
		args.push_back(String::utf8(argv[i]));
	}

	// Add arguments received from macOS LaunchService (URL schemas, file associations).
	for (const String &arg : platform_args) {
		args.push_back(arg);
	}

	List<String>::Element *I = args.front();

	while (I) {
		I->get() = unescape_cmdline(I->get().strip_edges());
		I = I->next();
	}

	String audio_driver = "";
	String project_path = ".";
	bool upwards = false;
	String debug_uri = "";
	bool skip_breakpoints = false;
	String main_pack;
	bool quiet_stdout = false;
	int separate_thread_render = -1; // Tri-state: -1 = not set, 0 = false, 1 = true.

	String remotefs;
	String remotefs_pass;

	Vector<String> breakpoints;
	bool delta_smoothing_override = false;

	String default_renderer = "";
	String default_renderer_mobile = "";
	String renderer_hints = "";

	packed_data = PackedData::get_singleton();
	if (!packed_data) {
		packed_data = memnew(PackedData);
	}

#ifdef MINIZIP_ENABLED

	//XXX: always get_singleton() == 0x0
	zip_packed_data = ZipArchive::get_singleton();
	//TODO: remove this temporary fix
	if (!zip_packed_data) {
		zip_packed_data = memnew(ZipArchive);
	}

	packed_data->add_pack_source(zip_packed_data);
#endif

	// Exit error code used in the `goto error` conditions.
	// It's returned as the program exit code. ERR_HELP is special cased and handled as success (0).
	Error exit_err = ERR_INVALID_PARAMETER;

	I = args.front();
	while (I) {
		List<String>::Element *N = I->next();

		const String &arg = I->get();

#ifdef MACOS_ENABLED
		// Ignore the process serial number argument passed by macOS Gatekeeper.
		// Otherwise, Godot would try to open a non-existent project on the first start and abort.
		if (arg.begins_with("-psn_")) {
			I = N;
			continue;
		}
#endif

#ifdef TOOLS_ENABLED
		if (arg == "--debug" ||
				arg == "--verbose" ||
				arg == "--disable-crash-handler") {
			forwardable_cli_arguments[CLI_SCOPE_TOOL].push_back(arg);
			forwardable_cli_arguments[CLI_SCOPE_PROJECT].push_back(arg);
		}
		if (arg == "--single-window") {
			forwardable_cli_arguments[CLI_SCOPE_TOOL].push_back(arg);
		}
		if (arg == "--audio-driver" ||
				arg == "--display-driver" ||
				arg == "--rendering-method" ||
				arg == "--rendering-driver" ||
				arg == "--xr-mode") {
			if (N) {
				forwardable_cli_arguments[CLI_SCOPE_TOOL].push_back(arg);
				forwardable_cli_arguments[CLI_SCOPE_TOOL].push_back(N->get());
			}
		}
		// If gpu is specified, both editor and debug instances started from editor will inherit.
		if (arg == "--gpu-index") {
			if (N) {
				const String &next_arg = N->get();
				forwardable_cli_arguments[CLI_SCOPE_TOOL].push_back(arg);
				forwardable_cli_arguments[CLI_SCOPE_TOOL].push_back(next_arg);
				forwardable_cli_arguments[CLI_SCOPE_PROJECT].push_back(arg);
				forwardable_cli_arguments[CLI_SCOPE_PROJECT].push_back(next_arg);
			}
		}
#endif

		if (adding_user_args) {
			user_args.push_back(arg);
		} else if (arg == "-h" || arg == "--help" || arg == "/?") { // display help

			show_help = true;
			exit_err = ERR_HELP; // Hack to force an early exit in `main()` with a success code.
			goto error;

		} else if (arg == "--version") {
			print_line(get_full_version_string());
			exit_err = ERR_HELP; // Hack to force an early exit in `main()` with a success code.
			goto error;

		} else if (arg == "-v" || arg == "--verbose") { // verbose output

			OS::get_singleton()->_verbose_stdout = true;
		} else if (arg == "-q" || arg == "--quiet") { // quieter output

			quiet_stdout = true;

		} else if (arg == "--no-header") {
			Engine::get_singleton()->_print_header = false;

		} else if (arg == "--audio-driver") { // audio driver

			if (N) {
				audio_driver = N->get();

				bool found = false;
				for (int i = 0; i < AudioDriverManager::get_driver_count(); i++) {
					if (audio_driver == AudioDriverManager::get_driver(i)->get_name()) {
						found = true;
					}
				}

				if (!found) {
					OS::get_singleton()->print("Unknown audio driver '%s', aborting.\nValid options are ",
							audio_driver.utf8().get_data());

					for (int i = 0; i < AudioDriverManager::get_driver_count(); i++) {
						if (i == AudioDriverManager::get_driver_count() - 1) {
							OS::get_singleton()->print(" and ");
						} else if (i != 0) {
							OS::get_singleton()->print(", ");
						}

						OS::get_singleton()->print("'%s'", AudioDriverManager::get_driver(i)->get_name());
					}

					OS::get_singleton()->print(".\n");

					goto error;
				}

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing audio driver argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--audio-output-latency") {
			if (N) {
				audio_output_latency = N->get().to_int();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing audio output latency argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--text-driver") {
			if (N) {
				text_driver = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing text driver argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--display-driver") { // force video driver

			if (N) {
				display_driver = N->get();

				bool found = false;
				for (int i = 0; i < DisplayServer::get_create_function_count(); i++) {
					if (display_driver == DisplayServer::get_create_function_name(i)) {
						found = true;
					}
				}

				if (!found) {
					OS::get_singleton()->print("Unknown display driver '%s', aborting.\nValid options are ",
							display_driver.utf8().get_data());

					for (int i = 0; i < DisplayServer::get_create_function_count(); i++) {
						if (i == DisplayServer::get_create_function_count() - 1) {
							OS::get_singleton()->print(" and ");
						} else if (i != 0) {
							OS::get_singleton()->print(", ");
						}

						OS::get_singleton()->print("'%s'", DisplayServer::get_create_function_name(i));
					}

					OS::get_singleton()->print(".\n");

					goto error;
				}

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing display driver argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--rendering-method") {
			if (N) {
				rendering_method = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing renderer name argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--rendering-driver") {
			if (N) {
				rendering_driver = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing rendering driver argument, aborting.\n");
				goto error;
			}
		} else if (arg == "-f" || arg == "--fullscreen") { // force fullscreen
			init_fullscreen = true;
			window_mode = DisplayServer::WINDOW_MODE_FULLSCREEN;
		} else if (arg == "-m" || arg == "--maximized") { // force maximized window
			init_maximized = true;
			window_mode = DisplayServer::WINDOW_MODE_MAXIMIZED;
		} else if (arg == "-w" || arg == "--windowed") { // force windowed window

			init_windowed = true;
		} else if (arg == "--gpu-index") {
			if (N) {
				Engine::singleton->gpu_idx = N->get().to_int();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing GPU index argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--gpu-validation") {
			Engine::singleton->use_validation_layers = true;
#ifdef DEBUG_ENABLED
		} else if (arg == "--gpu-abort") {
			Engine::singleton->abort_on_gpu_errors = true;
#endif
		} else if (arg == "--generate-spirv-debug-info") {
			Engine::singleton->generate_spirv_debug_info = true;
#if defined(DEBUG_ENABLED) || defined(DEV_ENABLED)
		} else if (arg == "--extra-gpu-memory-tracking") {
			Engine::singleton->extra_gpu_memory_tracking = true;
		} else if (arg == "--accurate-breadcrumbs") {
			Engine::singleton->accurate_breadcrumbs = true;
#endif
		} else if (arg == "--tablet-driver") {
			if (N) {
				tablet_driver = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing tablet driver argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--delta-smoothing") {
			if (N) {
				String string = N->get();
				bool recognized = false;
				if (string == "enable") {
					OS::get_singleton()->set_delta_smoothing(true);
					delta_smoothing_override = true;
					recognized = true;
				}
				if (string == "disable") {
					OS::get_singleton()->set_delta_smoothing(false);
					delta_smoothing_override = false;
					recognized = true;
				}
				if (!recognized) {
					OS::get_singleton()->print("Delta-smoothing argument not recognized, aborting.\n");
					goto error;
				}
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing delta-smoothing argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--single-window") { // force single window

			single_window = true;
		} else if (arg == "-t" || arg == "--always-on-top") { // force always-on-top window

			init_always_on_top = true;
		} else if (arg == "--resolution") { // force resolution

			if (N) {
				String vm = N->get();

				if (!vm.contains_char('x')) { // invalid parameter format

					OS::get_singleton()->print("Invalid resolution '%s', it should be e.g. '1280x720'.\n",
							vm.utf8().get_data());
					goto error;
				}

				int w = vm.get_slice("x", 0).to_int();
				int h = vm.get_slice("x", 1).to_int();

				if (w <= 0 || h <= 0) {
					OS::get_singleton()->print("Invalid resolution '%s', width and height must be above 0.\n",
							vm.utf8().get_data());
					goto error;
				}

				window_size.width = w;
				window_size.height = h;
				force_res = true;

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing resolution argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--screen") { // set window screen

			if (N) {
				init_screen = N->get().to_int();
				init_use_custom_screen = true;

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing screen argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--position") { // set window position

			if (N) {
				String vm = N->get();

				if (!vm.contains_char(',')) { // invalid parameter format

					OS::get_singleton()->print("Invalid position '%s', it should be e.g. '80,128'.\n",
							vm.utf8().get_data());
					goto error;
				}

				int x = vm.get_slice(",", 0).to_int();
				int y = vm.get_slice(",", 1).to_int();

				init_custom_pos = Point2(x, y);
				init_use_custom_pos = true;

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing position argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--headless") { // enable headless mode (no audio, no rendering).

			audio_driver = NULL_AUDIO_DRIVER;
			display_driver = NULL_DISPLAY_DRIVER;

		} else if (arg == "--log-file") { // write to log file

			if (N) {
				log_file = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing log file path argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--profiling") { // enable profiling

			use_debug_profiler = true;

		} else if (arg == "-l" || arg == "--language") { // language

			if (N) {
				locale = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing language argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--remote-fs") { // remote filesystem

			if (N) {
				remotefs = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing remote filesystem address, aborting.\n");
				goto error;
			}
		} else if (arg == "--remote-fs-password") { // remote filesystem password

			if (N) {
				remotefs_pass = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing remote filesystem password, aborting.\n");
				goto error;
			}
		} else if (arg == "--render-thread") { // render thread mode

			if (N) {
				if (N->get() == "safe") {
					separate_thread_render = 0;
#ifndef DISABLE_DEPRECATED
				} else if (N->get() == "unsafe") {
					OS::get_singleton()->print("The --render-thread unsafe option is unsupported in Godot 4 and will be removed.\n");
					separate_thread_render = 0;
#endif
				} else if (N->get() == "separate") {
					separate_thread_render = 1;
				} else {
					OS::get_singleton()->print("Unknown render thread mode, aborting.\n");
#ifdef DISABLE_DEPRECATED
					OS::get_singleton()->print("Valid options are 'safe' and 'separate'.\n");
#else
					OS::get_singleton()->print("Valid options are 'unsafe', 'safe' and 'separate'.\n");
#endif
					goto error;
				}

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing render thread mode argument, aborting.\n");
				goto error;
			}
#ifdef TOOLS_ENABLED
		} else if (arg == "-e" || arg == "--editor") { // starts editor

			editor = true;
		} else if (arg == "-p" || arg == "--project-manager") { // starts project manager
			project_manager = true;
		} else if (arg == "--recovery-mode") { // Enables recovery mode.
			recovery_mode = true;
		} else if (arg == "--debug-server") {
			if (N) {
				debug_server_uri = N->get();
				if (!debug_server_uri.contains("://")) { // wrong address
					OS::get_singleton()->print("Invalid debug server uri. It should be of the form <protocol>://<bind_address>:<port>.\n");
					goto error;
				}
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing remote debug server uri, aborting.\n");
				goto error;
			}
		} else if (arg == "--single-threaded-scene") {
			single_threaded_scene = true;
		} else if (arg == "--build-solutions") { // Build the scripting solution such C#

			auto_build_solutions = true;
			editor = true;
			cmdline_tool = true;
		} else if (arg == "--dump-gdextension-interface") {
			// Register as an editor instance to use low-end fallback if relevant.
			editor = true;
			cmdline_tool = true;
			dump_gdextension_interface = true;
			print_line("Dumping GDExtension interface header file");
			// Hack. Not needed but otherwise we end up detecting that this should
			// run the project instead of a cmdline tool.
			// Needs full refactoring to fix properly.
			main_args.push_back(arg);
		} else if (arg == "--dump-extension-api") {
			// Register as an editor instance to use low-end fallback if relevant.
			editor = true;
			cmdline_tool = true;
			dump_extension_api = true;
			print_line("Dumping Extension API");
			// Hack. Not needed but otherwise we end up detecting that this should
			// run the project instead of a cmdline tool.
			// Needs full refactoring to fix properly.
			main_args.push_back(arg);
		} else if (arg == "--dump-extension-api-with-docs") {
			// Register as an editor instance to use low-end fallback if relevant.
			editor = true;
			cmdline_tool = true;
			dump_extension_api = true;
			include_docs_in_extension_api_dump = true;
			print_line("Dumping Extension API including documentation");
			// Hack. Not needed but otherwise we end up detecting that this should
			// run the project instead of a cmdline tool.
			// Needs full refactoring to fix properly.
			main_args.push_back(arg);
		} else if (arg == "--validate-extension-api") {
			// Register as an editor instance to use low-end fallback if relevant.
			editor = true;
			cmdline_tool = true;
			validate_extension_api = true;
			// Hack. Not needed but otherwise we end up detecting that this should
			// run the project instead of a cmdline tool.
			// Needs full refactoring to fix properly.
			main_args.push_back(arg);

			if (N) {
				validate_extension_api_file = N->get();

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing file to load argument after --validate-extension-api, aborting.");
				goto error;
			}
		} else if (arg == "--import") {
			editor = true;
			cmdline_tool = true;
			wait_for_import = true;
			quit_after = 1;
		} else if (arg == "--export-release" || arg == "--export-debug" ||
				arg == "--export-pack" || arg == "--export-patch") { // Export project
			// Actually handling is done in start().
			editor = true;
			cmdline_tool = true;
			wait_for_import = true;
			main_args.push_back(arg);
		} else if (arg == "--patches") {
			if (N) {
				// Actually handling is done in start().
				main_args.push_back(arg);
				main_args.push_back(N->get());

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing comma-separated list of patches after --patches, aborting.\n");
				goto error;
			}
#ifndef DISABLE_DEPRECATED
		} else if (arg == "--export") { // For users used to 3.x syntax.
			OS::get_singleton()->print("The Godot 3 --export option was changed to more explicit --export-release / --export-debug / --export-pack options.\nSee the --help output for details.\n");
			goto error;
		} else if (arg == "--convert-3to4") {
			// Actually handling is done in start().
			cmdline_tool = true;
			main_args.push_back(arg);

			if (N && !N->get().begins_with("-")) {
				if (itos(N->get().to_int()) == N->get()) {
					converter_max_kb_file = N->get().to_int();
				}
				if (N->next() && !N->next()->get().begins_with("-")) {
					if (itos(N->next()->get().to_int()) == N->next()->get()) {
						converter_max_line_length = N->next()->get().to_int();
					}
				}
			}
		} else if (arg == "--validate-conversion-3to4") {
			// Actually handling is done in start().
			cmdline_tool = true;
			main_args.push_back(arg);

			if (N && !N->get().begins_with("-")) {
				if (itos(N->get().to_int()) == N->get()) {
					converter_max_kb_file = N->get().to_int();
				}
				if (N->next() && !N->next()->get().begins_with("-")) {
					if (itos(N->next()->get().to_int()) == N->next()->get()) {
						converter_max_line_length = N->next()->get().to_int();
					}
				}
			}
#endif // DISABLE_DEPRECATED
		} else if (arg == "--doctool") {
			// Actually handling is done in start().
			cmdline_tool = true;

			// `--doctool` implies `--headless` to avoid spawning an unnecessary window
			// and speed up class reference generation.
			audio_driver = NULL_AUDIO_DRIVER;
			display_driver = NULL_DISPLAY_DRIVER;
			main_args.push_back(arg);
#ifdef MODULE_GDSCRIPT_ENABLED
		} else if (arg == "--gdscript-docs") {
			if (N) {
				project_path = N->get();
				// Will be handled in start()
				main_args.push_back(arg);
				main_args.push_back(N->get());
				N = N->next();
				// GDScript docgen requires Autoloads, but loading those also creates a main loop.
				// This forces main loop to quit without adding more GDScript-specific exceptions to setup.
				quit_after = 1;
			} else {
				OS::get_singleton()->print("Missing relative or absolute path to project for --gdscript-docs, aborting.\n");
				goto error;
			}
#endif // MODULE_GDSCRIPT_ENABLED
#endif // TOOLS_ENABLED
		} else if (arg == "--path") { // set path of project to start or edit

			if (N) {
				String p = N->get();
				if (OS::get_singleton()->set_cwd(p) != OK) {
					OS::get_singleton()->print("Invalid project path specified: \"%s\", aborting.\n", p.utf8().get_data());
					goto error;
				}
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing relative or absolute path, aborting.\n");
				goto error;
			}
		} else if (arg == "-u" || arg == "--upwards") { // scan folders upwards
			upwards = true;
		} else if (arg == "--quit") { // Auto quit at the end of the first main loop iteration
			quit_after = 1;
		} else if (arg == "--quit-after") { // Quit after the given number of iterations
			if (N) {
				quit_after = N->get().to_int();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing number of iterations, aborting.\n");
				goto error;
			}
		} else if (arg.ends_with("project.godot")) {
			String path;
			String file = arg;
			int sep = MAX(file.rfind_char('/'), file.rfind_char('\\'));
			if (sep == -1) {
				path = ".";
			} else {
				path = file.substr(0, sep);
			}
			if (OS::get_singleton()->set_cwd(path) == OK) {
				// path already specified, don't override
			} else {
				project_path = path;
			}
#ifdef TOOLS_ENABLED
			editor = true;
#endif
		} else if (arg == "-b" || arg == "--breakpoints") { // add breakpoints

			if (N) {
				String bplist = N->get();
				breakpoints = bplist.split(",");
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing list of breakpoints, aborting.\n");
				goto error;
			}

		} else if (arg == "--max-fps") { // set maximum rendered FPS

			if (N) {
				max_fps = N->get().to_int();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing maximum FPS argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--frame-delay") { // force frame delay

			if (N) {
				frame_delay = N->get().to_int();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing frame delay argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--time-scale") { // force time scale

			if (N) {
				Engine::get_singleton()->set_time_scale(N->get().to_float());
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing time scale argument, aborting.\n");
				goto error;
			}

		} else if (arg == "--main-pack") {
			if (N) {
				main_pack = N->get();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing path to main pack file, aborting.\n");
				goto error;
			}

		} else if (arg == "-d" || arg == "--debug") {
			debug_uri = "local://";
			OS::get_singleton()->_debug_stdout = true;
#if defined(DEBUG_ENABLED)
		} else if (arg == "--debug-collisions") {
			debug_collisions = true;
		} else if (arg == "--debug-paths") {
			debug_paths = true;
		} else if (arg == "--debug-navigation") {
			debug_navigation = true;
		} else if (arg == "--debug-avoidance") {
			debug_avoidance = true;
		} else if (arg == "--debug-canvas-item-redraw") {
			debug_canvas_item_redraw = true;
		} else if (arg == "--debug-stringnames") {
			StringName::set_debug_stringnames(true);
#endif
		} else if (arg == "--remote-debug") {
			if (N) {
				debug_uri = N->get();
				if (!debug_uri.contains("://")) { // wrong address
					OS::get_singleton()->print(
							"Invalid debug host address, it should be of the form <protocol>://<host/IP>:<port>.\n");
					goto error;
				}
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing remote debug host address, aborting.\n");
				goto error;
			}
		} else if (arg == "--editor-pid") { // not exposed to user
			if (N) {
				editor_pid = N->get().to_int();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing editor PID argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--disable-render-loop") {
			disable_render_loop = true;
		} else if (arg == "--fixed-fps") {
			if (N) {
				fixed_fps = N->get().to_int();
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing fixed-fps argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--write-movie") {
			if (N) {
				Engine::get_singleton()->set_write_movie_path(N->get());
				N = N->next();
				if (fixed_fps == -1) {
					fixed_fps = 60;
				}
				OS::get_singleton()->_writing_movie = true;
			} else {
				OS::get_singleton()->print("Missing write-movie argument, aborting.\n");
				goto error;
			}
		} else if (arg == "--disable-vsync") {
			disable_vsync = true;
		} else if (arg == "--print-fps") {
			print_fps = true;
#ifdef TOOLS_ENABLED
		} else if (arg == "--editor-pseudolocalization") {
			editor_pseudolocalization = true;
#endif // TOOLS_ENABLED
		} else if (arg == "--profile-gpu") {
			profile_gpu = true;
		} else if (arg == "--disable-crash-handler") {
			OS::get_singleton()->disable_crash_handler();
		} else if (arg == "--skip-breakpoints") {
			skip_breakpoints = true;
#ifndef _3D_DISABLED
		} else if (arg == "--xr-mode") {
			if (N) {
				String xr_mode = N->get().to_lower();
				N = N->next();
				if (xr_mode == "default") {
					XRServer::set_xr_mode(XRServer::XRMODE_DEFAULT);
				} else if (xr_mode == "off") {
					XRServer::set_xr_mode(XRServer::XRMODE_OFF);
				} else if (xr_mode == "on") {
					XRServer::set_xr_mode(XRServer::XRMODE_ON);
				} else {
					OS::get_singleton()->print("Unknown --xr-mode argument \"%s\", aborting.\n", xr_mode.ascii().get_data());
					goto error;
				}
			} else {
				OS::get_singleton()->print("Missing --xr-mode argument, aborting.\n");
				goto error;
			}
#endif // _3D_DISABLED
		} else if (arg == "--benchmark") {
			OS::get_singleton()->set_use_benchmark(true);
		} else if (arg == "--benchmark-file") {
			if (N) {
				OS::get_singleton()->set_use_benchmark(true);
				String benchmark_file = N->get();
				OS::get_singleton()->set_benchmark_file(benchmark_file);
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing <path> argument for --benchmark-file <path>.\n");
				goto error;
			}
#if defined(TOOLS_ENABLED) && defined(MODULE_GDSCRIPT_ENABLED) && !defined(GDSCRIPT_NO_LSP)
		} else if (arg == "--lsp-port") {
			if (N) {
				int port_override = N->get().to_int();
				if (port_override < 0 || port_override > 65535) {
					OS::get_singleton()->print("<port> argument for --lsp-port <port> must be between 0 and 65535.\n");
					goto error;
				}
				GDScriptLanguageServer::port_override = port_override;
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing <port> argument for --lsp-port <port>.\n");
				goto error;
			}
#endif // TOOLS_ENABLED && MODULE_GDSCRIPT_ENABLED && !GDSCRIPT_NO_LSP
#if defined(TOOLS_ENABLED)
		} else if (arg == "--dap-port") {
			if (N) {
				int port_override = N->get().to_int();
				if (port_override < 0 || port_override > 65535) {
					OS::get_singleton()->print("<port> argument for --dap-port <port> must be between 0 and 65535.\n");
					goto error;
				}
				DebugAdapterServer::port_override = port_override;
				N = N->next();
			} else {
				OS::get_singleton()->print("Missing <port> argument for --dap-port <port>.\n");
				goto error;
			}
#endif // TOOLS_ENABLED
		} else if (arg == "--wid") {
			if (N) {
				init_embed_parent_window_id = N->get().to_int();
				if (init_embed_parent_window_id == 0) {
					OS::get_singleton()->print("<window_id> argument for --wid <window_id> must be different then 0.\n");
					goto error;
				}

				OS::get_singleton()->_embedded_in_editor = true;
				Engine::get_singleton()->set_embedded_in_editor(true);

				N = N->next();
			} else {
				OS::get_singleton()->print("Missing <window_id> argument for --wid <window_id>.\n");
				goto error;
			}

		} else if (arg == "--" || arg == "++") {
			adding_user_args = true;
		} else {
			main_args.push_back(arg);
		}

		I = N;
	}

#ifdef TOOLS_ENABLED
	if (editor && project_manager) {
		OS::get_singleton()->print(
				"Error: Command line arguments implied opening both editor and project manager, which is not possible. Aborting.\n");
		goto error;
	}
#endif

	// Network file system needs to be configured before globals, since globals are based on the
	// 'project.godot' file which will only be available through the network if this is enabled
	if (!remotefs.is_empty()) {
		int port;
		if (remotefs.contains_char(':')) {
			port = remotefs.get_slicec(':', 1).to_int();
			remotefs = remotefs.get_slicec(':', 0);
		} else {
			port = 6010;
		}
		Error err = OS::get_singleton()->setup_remote_filesystem(remotefs, port, remotefs_pass, project_path);

		if (err) {
			OS::get_singleton()->printerr("Could not connect to remotefs: %s:%i.\n", remotefs.utf8().get_data(), port);
			goto error;
		}
	}

	OS::get_singleton()->_in_editor = editor;
	if (globals->setup(project_path, main_pack, upwards, editor) == OK) {
#ifdef TOOLS_ENABLED
		found_project = true;
#endif
	} else {
#ifdef TOOLS_ENABLED
		editor = false;
#else
		const String error_msg = "Error: Couldn't load project data at path \"" + project_path + "\". Is the .pck file missing?\nIf you've renamed the executable, the associated .pck file should also be renamed to match the executable's name (without the extension).\n";
		OS::get_singleton()->print("%s", error_msg.utf8().get_data());
		OS::get_singleton()->alert(error_msg);

		goto error;
#endif
	}

	// Initialize WorkerThreadPool.
	{
#ifdef THREADS_ENABLED
		if (editor || project_manager) {
			WorkerThreadPool::get_singleton()->init(-1, 0.75);
		} else {
			int worker_threads = GLOBAL_GET("threading/worker_pool/max_threads");
			float low_priority_ratio = GLOBAL_GET("threading/worker_pool/low_priority_thread_ratio");
			WorkerThreadPool::get_singleton()->init(worker_threads, low_priority_ratio);
		}
#else
		WorkerThreadPool::get_singleton()->init(0, 0);
#endif
	}

#ifdef TOOLS_ENABLED
	if (editor) {
		Engine::get_singleton()->set_editor_hint(true);
		Engine::get_singleton()->set_extension_reloading_enabled(true);

		// Create initialization lock file to detect crashes during startup.
		OS::get_singleton()->create_lock_file();

		main_args.push_back("--editor");
		if (!init_windowed && !init_fullscreen) {
			init_maximized = true;
			window_mode = DisplayServer::WINDOW_MODE_MAXIMIZED;
		}
	}

	if (!project_manager && !editor) {
		// If we didn't find a project, we fall back to the project manager.
		project_manager = !found_project && !cmdline_tool;
	}

	if (project_manager) {
		Engine::get_singleton()->set_project_manager_hint(true);
	}

	if (recovery_mode) {
		if (project_manager || !editor) {
			OS::get_singleton()->print("Error: Recovery mode can only be used in the editor. Aborting.\n");
			goto error;
		}

		Engine::get_singleton()->set_recovery_mode_hint(true);
	}
#endif

	OS::get_singleton()->set_cmdline(execpath, main_args, user_args);

	Engine::get_singleton()->set_physics_ticks_per_second(GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "physics/common/physics_ticks_per_second", PROPERTY_HINT_RANGE, "1,1000,1"), 60));
	Engine::get_singleton()->set_max_physics_steps_per_frame(GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "physics/common/max_physics_steps_per_frame", PROPERTY_HINT_RANGE, "1,100,1"), 8));
	Engine::get_singleton()->set_physics_jitter_fix(GLOBAL_DEF(PropertyInfo(Variant::FLOAT, "physics/common/physics_jitter_fix", PROPERTY_HINT_RANGE, "0,2,0.001,or_greater"), 0.5));
	Engine::get_singleton()->set_max_fps(GLOBAL_DEF(PropertyInfo(Variant::INT, "application/run/max_fps", PROPERTY_HINT_RANGE, "0,1000,1"), 0));
	if (max_fps >= 0) {
		Engine::get_singleton()->set_max_fps(max_fps);
	}

	// Initialize user data dir.
	OS::get_singleton()->ensure_user_data_dir();

	OS::get_singleton()->set_low_processor_usage_mode(GLOBAL_DEF("application/run/low_processor_mode", false));
	OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(
			GLOBAL_DEF(PropertyInfo(Variant::INT, "application/run/low_processor_mode_sleep_usec", PROPERTY_HINT_RANGE, "0,33200,1,or_greater"), 6900)); // Roughly 144 FPS

	GLOBAL_DEF("application/run/delta_smoothing", true);
	if (!delta_smoothing_override) {
		OS::get_singleton()->set_delta_smoothing(GLOBAL_GET("application/run/delta_smoothing"));
	}

	GLOBAL_DEF("debug/settings/stdout/print_fps", false);
	GLOBAL_DEF("debug/settings/stdout/print_gpu_profile", false);
	GLOBAL_DEF("debug/settings/stdout/verbose_stdout", false);
	GLOBAL_DEF("debug/settings/physics_interpolation/enable_warnings", true);
	if (!OS::get_singleton()->_verbose_stdout) { // Not manually overridden.
		OS::get_singleton()->_verbose_stdout = GLOBAL_GET("debug/settings/stdout/verbose_stdout");
	}

	register_early_core_singletons();
	initialize_modules(MODULE_INITIALIZATION_LEVEL_CORE);
	register_core_extensions(); // core extensions must be registered after globals setup and before display

	ResourceUID::get_singleton()->load_from_cache(true); // load UUIDs from cache.

	if (ProjectSettings::get_singleton()->has_custom_feature("dedicated_server")) {
		audio_driver = NULL_AUDIO_DRIVER;
		display_driver = NULL_DISPLAY_DRIVER;
	}

	GLOBAL_DEF(PropertyInfo(Variant::INT, "network/limits/debugger/max_chars_per_second", PROPERTY_HINT_RANGE, "256,4096,1,or_greater"), 32768);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "network/limits/debugger/max_queued_messages", PROPERTY_HINT_RANGE, "128,8192,1,or_greater"), 2048);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "network/limits/debugger/max_errors_per_second", PROPERTY_HINT_RANGE, "1,200,1,or_greater"), 400);
	GLOBAL_DEF(PropertyInfo(Variant::INT, "network/limits/debugger/max_warnings_per_second", PROPERTY_HINT_RANGE, "1,200,1,or_greater"), 400);

	EngineDebugger::initialize(debug_uri, skip_breakpoints, breakpoints, []() {
		if (editor_pid) {
			DisplayServer::get_singleton()->enable_for_stealing_focus(editor_pid);
		}
	});

#ifdef TOOLS_ENABLED
	if (editor) {
		packed_data->set_disabled(true);
	}
#endif

	GLOBAL_DEF("debug/file_logging/enable_file_logging", false);
	// Only file logging by default on desktop platforms as logs can't be
	// accessed easily on mobile/Web platforms (if at all).
	// This also prevents logs from being created for the editor instance, as feature tags
	// are disabled while in the editor (even if they should logically apply).
	GLOBAL_DEF("debug/file_logging/enable_file_logging.pc", true);
	GLOBAL_DEF("debug/file_logging/log_path", "user://logs/godot.log");
	GLOBAL_DEF(PropertyInfo(Variant::INT, "debug/file_logging/max_log_files", PROPERTY_HINT_RANGE, "0,20,1,or_greater"), 5);

	// If `--log-file` is used to override the log path, allow creating logs for the project manager or editor
	// and even if file logging is disabled in the Project Settings.
	// `--log-file` can be used with any path (including absolute paths outside the project folder),
	// so check for filesystem access if it's used.
	if (FileAccess::get_create_func(!log_file.is_empty() ? FileAccess::ACCESS_FILESYSTEM : FileAccess::ACCESS_USERDATA) &&
			(!log_file.is_empty() || (!project_manager && !editor && GLOBAL_GET("debug/file_logging/enable_file_logging")))) {
		// Don't create logs for the project manager as they would be written to
		// the current working directory, which is inconvenient.
		String base_path;
		int max_files;
		if (!log_file.is_empty()) {
			base_path = log_file;
			// Ensure log file name respects the specified override by disabling log rotation.
			max_files = 1;
		} else {
			base_path = GLOBAL_GET("debug/file_logging/log_path");
			max_files = GLOBAL_GET("debug/file_logging/max_log_files");
		}
		OS::get_singleton()->add_logger(memnew(RotatedFileLogger(base_path, max_files)));
	}

	if (main_args.size() == 0 && String(GLOBAL_GET("application/run/main_scene")) == "") {
#ifdef TOOLS_ENABLED
		if (!editor && !project_manager) {
#endif
			const String error_msg = "Error: Can't run project: no main scene defined in the project.\n";
			OS::get_singleton()->print("%s", error_msg.utf8().get_data());
			OS::get_singleton()->alert(error_msg);
			goto error;
#ifdef TOOLS_ENABLED
		}
#endif
	}

	if (editor || project_manager) {
		Engine::get_singleton()->set_editor_hint(true);
		use_custom_res = false;
		input_map->load_default(); //keys for editor
	} else {
		input_map->load_from_project_settings(); //keys for game
	}

	if (bool(GLOBAL_GET("application/run/disable_stdout"))) {
		quiet_stdout = true;
	}
	if (bool(GLOBAL_GET("application/run/disable_stderr"))) {
		CoreGlobals::print_error_enabled = false;
	}
	if (!bool(GLOBAL_GET("application/run/print_header"))) {
		// --no-header option for project settings.
		Engine::get_singleton()->_print_header = false;
	}

	if (quiet_stdout) {
		CoreGlobals::print_line_enabled = false;
	}

	Logger::set_flush_stdout_on_print(GLOBAL_GET("application/run/flush_stdout_on_print"));

	{
		String driver_hints = "";
		String driver_hints_with_d3d12 = "";
		String driver_hints_with_metal = "";

		{
			Vector<String> driver_hints_arr;
#ifdef VULKAN_ENABLED
			driver_hints_arr.push_back("vulkan");
#endif
			driver_hints = String(",").join(driver_hints_arr);

#ifdef D3D12_ENABLED
			driver_hints_arr.push_back("d3d12");
#endif
			driver_hints_with_d3d12 = String(",").join(driver_hints_arr);

#ifdef METAL_ENABLED
			// Make metal the preferred and default driver.
			driver_hints_arr.insert(0, "metal");
#endif
			driver_hints_with_metal = String(",").join(driver_hints_arr);
		}

		String default_driver = driver_hints.get_slice(",", 0);
		String default_driver_with_d3d12 = driver_hints_with_d3d12.get_slice(",", 0);
		String default_driver_with_metal = driver_hints_with_metal.get_slice(",", 0);

		// For now everything defaults to vulkan when available. This can change in future updates.
		GLOBAL_DEF_RST_NOVAL("rendering/rendering_device/driver", default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/rendering_device/driver.windows", PROPERTY_HINT_ENUM, driver_hints_with_d3d12), default_driver_with_d3d12);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/rendering_device/driver.linuxbsd", PROPERTY_HINT_ENUM, driver_hints), default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/rendering_device/driver.android", PROPERTY_HINT_ENUM, driver_hints), default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/rendering_device/driver.ios", PROPERTY_HINT_ENUM, driver_hints_with_metal), default_driver_with_metal);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/rendering_device/driver.macos", PROPERTY_HINT_ENUM, driver_hints_with_metal), default_driver_with_metal);

		GLOBAL_DEF_RST("rendering/rendering_device/fallback_to_vulkan", true);
		GLOBAL_DEF_RST("rendering/rendering_device/fallback_to_d3d12", true);
		GLOBAL_DEF_RST("rendering/rendering_device/fallback_to_opengl3", true);
	}

	{
		String driver_hints = "";
		String driver_hints_angle = "";
		String driver_hints_egl = "";
#ifdef GLES3_ENABLED
		driver_hints = "opengl3";
		driver_hints_angle = "opengl3,opengl3_angle"; // macOS, Windows.
		driver_hints_egl = "opengl3,opengl3_es"; // Linux.
#endif

		String default_driver = driver_hints.get_slice(",", 0);

		GLOBAL_DEF_RST_NOVAL("rendering/gl_compatibility/driver", default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/gl_compatibility/driver.windows", PROPERTY_HINT_ENUM, driver_hints_angle), default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/gl_compatibility/driver.linuxbsd", PROPERTY_HINT_ENUM, driver_hints_egl), default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/gl_compatibility/driver.web", PROPERTY_HINT_ENUM, driver_hints), default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/gl_compatibility/driver.android", PROPERTY_HINT_ENUM, driver_hints), default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/gl_compatibility/driver.ios", PROPERTY_HINT_ENUM, driver_hints), default_driver);
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "rendering/gl_compatibility/driver.macos", PROPERTY_HINT_ENUM, driver_hints_angle), default_driver);

		GLOBAL_DEF_RST("rendering/gl_compatibility/nvidia_disable_threaded_optimization", true);
		GLOBAL_DEF_RST("rendering/gl_compatibility/fallback_to_angle", true);
		GLOBAL_DEF_RST("rendering/gl_compatibility/fallback_to_native", true);
		GLOBAL_DEF_RST("rendering/gl_compatibility/fallback_to_gles", true);

		Array device_blocklist;

#define BLOCK_DEVICE(m_vendor, m_name)      \
	{                                       \
		Dictionary device;                  \
		device["vendor"] = m_vendor;        \
		device["name"] = m_name;            \
		device_blocklist.push_back(device); \
	}

		// AMD GPUs.
		BLOCK_DEVICE("ATI", "Radeon 9"); // ATI Radeon 9000 Series
		BLOCK_DEVICE("ATI", "Radeon X"); // ATI Radeon X500-X2000 Series
		BLOCK_DEVICE("ATI", "Radeon HD 2"); // AMD/ATI (Mobility) Radeon HD 2xxx Series
		BLOCK_DEVICE("ATI", "Radeon HD 3"); // AMD/ATI (Mobility) Radeon HD 3xxx Series
		BLOCK_DEVICE("ATI", "Radeon HD 4"); // AMD/ATI (Mobility) Radeon HD 4xxx Series
		BLOCK_DEVICE("ATI", "Radeon HD 5"); // AMD/ATI (Mobility) Radeon HD 5xxx Series
		BLOCK_DEVICE("ATI", "Radeon HD 6"); // AMD/ATI (Mobility) Radeon HD 6xxx Series
		BLOCK_DEVICE("ATI", "Radeon HD 7"); // AMD/ATI (Mobility) Radeon HD 7xxx Series
		BLOCK_DEVICE("ATI", "Radeon HD 8"); // AMD/ATI (Mobility) Radeon HD 8xxx Series
		BLOCK_DEVICE("ATI", "Radeon(TM) R2 Graphics"); // APUs
		BLOCK_DEVICE("ATI", "Radeon(TM) R3 Graphics");
		BLOCK_DEVICE("ATI", "Radeon(TM) R4 Graphics");
		BLOCK_DEVICE("ATI", "Radeon(TM) R5 Graphics");
		BLOCK_DEVICE("ATI", "Radeon(TM) R6 Graphics");
		BLOCK_DEVICE("ATI", "Radeon(TM) R7 Graphics");
		BLOCK_DEVICE("AMD", "Radeon(TM) R7 Graphics");
		BLOCK_DEVICE("AMD", "Radeon(TM) R8 Graphics");
		BLOCK_DEVICE("ATI", "Radeon R5 Graphics");
		BLOCK_DEVICE("ATI", "Radeon R6 Graphics");
		BLOCK_DEVICE("ATI", "Radeon R7 Graphics");
		BLOCK_DEVICE("AMD", "Radeon R7 Graphics");
		BLOCK_DEVICE("AMD", "Radeon R8 Graphics");
		BLOCK_DEVICE("ATI", "Radeon R5 2"); // Rx 2xx Series
		BLOCK_DEVICE("ATI", "Radeon R7 2");
		BLOCK_DEVICE("ATI", "Radeon R9 2");
		BLOCK_DEVICE("ATI", "Radeon R5 M2"); // Rx M2xx Series
		BLOCK_DEVICE("ATI", "Radeon R7 M2");
		BLOCK_DEVICE("ATI", "Radeon R9 M2");
		BLOCK_DEVICE("ATI", "Radeon (TM) R9 Fury");
		BLOCK_DEVICE("ATI", "Radeon (TM) R5 3"); // Rx 3xx Series
		BLOCK_DEVICE("AMD", "Radeon (TM) R5 3");
		BLOCK_DEVICE("ATI", "Radeon (TM) R7 3");
		BLOCK_DEVICE("AMD", "Radeon (TM) R7 3");
		BLOCK_DEVICE("ATI", "Radeon (TM) R9 3");
		BLOCK_DEVICE("AMD", "Radeon (TM) R9 3");
		BLOCK_DEVICE("ATI", "Radeon (TM) R5 M3"); // Rx M3xx Series
		BLOCK_DEVICE("AMD", "Radeon (TM) R5 M3");
		BLOCK_DEVICE("ATI", "Radeon (TM) R7 M3");
		BLOCK_DEVICE("AMD", "Radeon (TM) R7 M3");
		BLOCK_DEVICE("ATI", "Radeon (TM) R9 M3");
		BLOCK_DEVICE("AMD", "Radeon (TM) R9 M3");

		// Intel GPUs.
		BLOCK_DEVICE("0x8086", "0x0042"); // HD Graphics, Gen5, Clarkdale
		BLOCK_DEVICE("0x8086", "0x0046"); // HD Graphics, Gen5, Arrandale
		BLOCK_DEVICE("0x8086", "0x010A"); // HD Graphics, Gen6, Sandy Bridge
		BLOCK_DEVICE("Intel", "Intel HD Graphics 2000");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 2000");
		BLOCK_DEVICE("0x8086", "0x0102"); // HD Graphics 2000, Gen6, Sandy Bridge
		BLOCK_DEVICE("0x8086", "0x0116"); // HD Graphics 3000, Gen6, Sandy Bridge
		BLOCK_DEVICE("Intel", "Intel HD Graphics 3000");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 3000");
		BLOCK_DEVICE("0x8086", "0x0126"); // HD Graphics 3000, Gen6, Sandy Bridge
		BLOCK_DEVICE("Intel", "Intel HD Graphics P3000");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics P3000");
		BLOCK_DEVICE("0x8086", "0x0112"); // HD Graphics P3000, Gen6, Sandy Bridge
		BLOCK_DEVICE("0x8086", "0x0122");
		BLOCK_DEVICE("0x8086", "0x015A"); // HD Graphics, Gen7, Ivy Bridge
		BLOCK_DEVICE("Intel", "Intel HD Graphics 2500");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 2500");
		BLOCK_DEVICE("0x8086", "0x0152"); // HD Graphics 2500, Gen7, Ivy Bridge
		BLOCK_DEVICE("Intel", "Intel HD Graphics 4000");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 4000");
		BLOCK_DEVICE("0x8086", "0x0162"); // HD Graphics 4000, Gen7, Ivy Bridge
		BLOCK_DEVICE("0x8086", "0x0166");
		BLOCK_DEVICE("Intel", "Intel HD Graphics P4000");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics P4000");
		BLOCK_DEVICE("0x8086", "0x016A"); // HD Graphics P4000, Gen7, Ivy Bridge
		BLOCK_DEVICE("Intel", "Intel(R) Vallyview Graphics");
		BLOCK_DEVICE("0x8086", "0x0F30"); // Intel(R) Vallyview Graphics, Gen7, Vallyview
		BLOCK_DEVICE("0x8086", "0x0F31");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 4200");
		BLOCK_DEVICE("0x8086", "0x0A1E"); // Intel(R) HD Graphics 4200, Gen7.5, Haswell
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 4400");
		BLOCK_DEVICE("0x8086", "0x0A16"); // Intel(R) HD Graphics 4400, Gen7.5, Haswell
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 4600");
		BLOCK_DEVICE("0x8086", "0x0412"); // Intel(R) HD Graphics 4600, Gen7.5, Haswell
		BLOCK_DEVICE("0x8086", "0x0416");
		BLOCK_DEVICE("0x8086", "0x0426");
		BLOCK_DEVICE("0x8086", "0x0D12");
		BLOCK_DEVICE("0x8086", "0x0D16");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics P4600/P4700");
		BLOCK_DEVICE("0x8086", "0x041A"); // Intel(R) HD Graphics P4600/P4700, Gen7.5, Haswell
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 5000");
		BLOCK_DEVICE("0x8086", "0x0422"); // Intel(R) HD Graphics 5000, Gen7.5, Haswell
		BLOCK_DEVICE("0x8086", "0x042A");
		BLOCK_DEVICE("0x8086", "0x0A26");
		BLOCK_DEVICE("Intel", "Intel(R) Iris(TM) Graphics 5100");
		BLOCK_DEVICE("0x8086", "0x0A22"); // Intel(R) Iris(TM) Graphics 5100, Gen7.5, Haswell
		BLOCK_DEVICE("0x8086", "0x0A2A");
		BLOCK_DEVICE("0x8086", "0x0A2B");
		BLOCK_DEVICE("0x8086", "0x0A2E");
		BLOCK_DEVICE("Intel", "Intel(R) Iris(TM) Pro Graphics 5200");
		BLOCK_DEVICE("0x8086", "0x0D22"); // Intel(R) Iris(TM) Pro Graphics 5200, Gen7.5, Haswell
		BLOCK_DEVICE("0x8086", "0x0D26");
		BLOCK_DEVICE("0x8086", "0x0D2A");
		BLOCK_DEVICE("0x8086", "0x0D2B");
		BLOCK_DEVICE("0x8086", "0x0D2E");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 400");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 405");
		BLOCK_DEVICE("0x8086", "0x22B0"); // Intel(R) HD Graphics, Gen8, Cherryview Braswell
		BLOCK_DEVICE("0x8086", "0x22B1");
		BLOCK_DEVICE("0x8086", "0x22B2");
		BLOCK_DEVICE("0x8086", "0x22B3");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 5300");
		BLOCK_DEVICE("0x8086", "0x161E"); // Intel(R) HD Graphics 5300, Gen8, Broadwell
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 5500");
		BLOCK_DEVICE("0x8086", "0x1616"); // Intel(R) HD Graphics 5500, Gen8, Broadwell
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 5600");
		BLOCK_DEVICE("0x8086", "0x1612"); // Intel(R) HD Graphics 5600, Gen8, Broadwell
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 6000");
		BLOCK_DEVICE("0x8086", "0x1626"); // Intel(R) HD Graphics 6000, Gen8, Broadwell
		BLOCK_DEVICE("Intel", "Intel(R) Iris(TM) Graphics 6100");
		BLOCK_DEVICE("0x8086", "0x162B"); // Intel(R) Iris(TM) Graphics 6100, Gen8, Broadwell
		BLOCK_DEVICE("Intel", "Intel(R) Iris(TM) Pro Graphics 6200");
		BLOCK_DEVICE("0x8086", "0x1622"); // Intel(R) Iris(TM) Pro Graphics 6200, Gen8, Broadwell
		BLOCK_DEVICE("Intel", "Intel(R) Iris(TM) Pro Graphics P6300");
		BLOCK_DEVICE("0x8086", "0x162A"); // Intel(R) Iris(TM) Pro Graphics P6300, Gen8, Broadwell
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 500");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 505");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 510");
		BLOCK_DEVICE("0x8086", "0x1902"); // Intel(R) HD Graphics 510, Gen9, Skylake
		BLOCK_DEVICE("0x8086", "0x1906");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 520");
		BLOCK_DEVICE("0x8086", "0x1916"); // Intel(R) HD Graphics 520, Gen9, Skylake
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 530");
		BLOCK_DEVICE("0x8086", "0x1912"); // Intel(R) HD Graphics 530, Gen9, Skylake
		BLOCK_DEVICE("0x8086", "0x191B");
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics P530");
		BLOCK_DEVICE("0x8086", "0x191D"); // Intel(R) HD Graphics P530, Gen9, Skylake
		BLOCK_DEVICE("Intel", "Intel(R) HD Graphics 515");
		BLOCK_DEVICE("0x8086", "0x191E"); // Intel(R) HD Graphics 515, Gen9, Skylake
		BLOCK_DEVICE("Intel", "Intel(R) Iris Graphics 540");
		BLOCK_DEVICE("0x8086", "0x1926"); // Intel(R) Iris Graphics 540, Gen9, Skylake
		BLOCK_DEVICE("0x8086", "0x1927");
		BLOCK_DEVICE("Intel", "Intel(R) Iris Pro Graphics 580");
		BLOCK_DEVICE("0x8086", "0x193B"); // Intel(R) Iris Pro Graphics 580, Gen9, Skylake
		BLOCK_DEVICE("Intel", "Intel(R) Iris Pro Graphics P580");
		BLOCK_DEVICE("0x8086", "0x193D"); // Intel(R) Iris Pro Graphics P580, Gen9, Skylake

#undef BLOCK_DEVICE

		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::ARRAY, "rendering/gl_compatibility/force_angle_on_devices", PROPERTY_HINT_ARRAY_TYPE, vformat("%s/%s:%s", Variant::DICTIONARY, PROPERTY_HINT_NONE, String())), device_blocklist);
	}

	// Start with RenderingDevice-based backends.
#ifdef RD_ENABLED
	renderer_hints = "forward_plus,mobile";
	default_renderer_mobile = "mobile";
#endif

	// And Compatibility next, or first if Vulkan is disabled.
#ifdef GLES3_ENABLED
	if (!renderer_hints.is_empty()) {
		renderer_hints += ",";
	}
	renderer_hints += "gl_compatibility";
	if (default_renderer_mobile.is_empty()) {
		default_renderer_mobile = "gl_compatibility";
	}
	// Default to Compatibility when using the project manager.
	if (rendering_driver.is_empty() && rendering_method.is_empty() && project_manager) {
		rendering_driver = "opengl3";
		rendering_method = "gl_compatibility";
		default_renderer_mobile = "gl_compatibility";
	}
#endif
	if (renderer_hints.is_empty()) {
		ERR_PRINT("No renderers available.");
	}

	if (!rendering_method.is_empty()) {
		if (rendering_method != "forward_plus" &&
				rendering_method != "mobile" &&
				rendering_method != "gl_compatibility") {
			OS::get_singleton()->print("Unknown rendering method '%s', aborting.\nValid options are ",
					rendering_method.utf8().get_data());

			const Vector<String> rendering_method_hints = renderer_hints.split(",");
			for (int i = 0; i < rendering_method_hints.size(); i++) {
				if (i == rendering_method_hints.size() - 1) {
					OS::get_singleton()->print(" and ");
				} else if (i != 0) {
					OS::get_singleton()->print(", ");
				}
				OS::get_singleton()->print("'%s'", rendering_method_hints[i].utf8().get_data());
			}

			OS::get_singleton()->print(".\n");
			goto error;
		}
	}

	if (!rendering_driver.is_empty()) {
		// As the rendering drivers available may depend on the display driver and renderer
		// selected, we can't do an exhaustive check here, but we can look through all
		// the options in all the display drivers for a match.

		bool found = false;
		for (int i = 0; i < DisplayServer::get_create_function_count(); i++) {
			Vector<String> r_drivers = DisplayServer::get_create_function_rendering_drivers(i);

			for (int d = 0; d < r_drivers.size(); d++) {
				if (rendering_driver == r_drivers[d]) {
					found = true;
					break;
				}
			}
		}

		if (!found) {
			OS::get_singleton()->print("Unknown rendering driver '%s', aborting.\nValid options are ",
					rendering_driver.utf8().get_data());

			// Deduplicate driver entries, as a rendering driver may be supported by several display servers.
			Vector<String> unique_rendering_drivers;
			for (int i = 0; i < DisplayServer::get_create_function_count(); i++) {
				Vector<String> r_drivers = DisplayServer::get_create_function_rendering_drivers(i);

				for (int d = 0; d < r_drivers.size(); d++) {
					if (!unique_rendering_drivers.has(r_drivers[d])) {
						unique_rendering_drivers.append(r_drivers[d]);
					}
				}
			}

			for (int i = 0; i < unique_rendering_drivers.size(); i++) {
				if (i == unique_rendering_drivers.size() - 1) {
					OS::get_singleton()->print(" and ");
				} else if (i != 0) {
					OS::get_singleton()->print(", ");
				}
				OS::get_singleton()->print("'%s'", unique_rendering_drivers[i].utf8().get_data());
			}

			OS::get_singleton()->print(".\n");

			goto error;
		}

		// Set a default renderer if none selected. Try to choose one that matches the driver.
		if (rendering_method.is_empty()) {
			if (rendering_driver == "opengl3" || rendering_driver == "opengl3_angle" || rendering_driver == "opengl3_es") {
				rendering_method = "gl_compatibility";
			} else {
				rendering_method = "forward_plus";
			}
		}

		// Now validate whether the selected driver matches with the renderer.
		bool valid_combination = false;
		Vector<String> available_drivers;
		if (rendering_method == "forward_plus" || rendering_method == "mobile") {
#ifdef VULKAN_ENABLED
			available_drivers.push_back("vulkan");
#endif
#ifdef D3D12_ENABLED
			available_drivers.push_back("d3d12");
#endif
#ifdef METAL_ENABLED
			available_drivers.push_back("metal");
#endif
		}
#ifdef GLES3_ENABLED
		if (rendering_method == "gl_compatibility") {
			available_drivers.push_back("opengl3");
			available_drivers.push_back("opengl3_angle");
			available_drivers.push_back("opengl3_es");
		}
#endif
		if (available_drivers.is_empty()) {
			OS::get_singleton()->print("Unknown renderer name '%s', aborting.\n", rendering_method.utf8().get_data());
			goto error;
		}

		for (int i = 0; i < available_drivers.size(); i++) {
			if (rendering_driver == available_drivers[i]) {
				valid_combination = true;
				break;
			}
		}

		if (!valid_combination) {
			OS::get_singleton()->print("Invalid renderer/driver combination '%s' and '%s', aborting. %s only supports the following drivers ", rendering_method.utf8().get_data(), rendering_driver.utf8().get_data(), rendering_method.utf8().get_data());

			for (int d = 0; d < available_drivers.size(); d++) {
				OS::get_singleton()->print("'%s', ", available_drivers[d].utf8().get_data());
			}

			OS::get_singleton()->print(".\n");

			goto error;
		}
	}

	default_renderer = renderer_hints.get_slice(",", 0);
	GLOBAL_DEF_RST_BASIC(PropertyInfo(Variant::STRING, "rendering/renderer/rendering_method", PROPERTY_HINT_ENUM, renderer_hints), default_renderer);
	GLOBAL_DEF_RST_BASIC("rendering/renderer/rendering_method.mobile", default_renderer_mobile);
	GLOBAL_DEF_RST_BASIC("rendering/renderer/rendering_method.web", "gl_compatibility"); // This is a bit of a hack until we have WebGPU support.

	// Default to ProjectSettings default if nothing set on the command line.
	if (rendering_method.is_empty()) {
		rendering_method = GLOBAL_GET("rendering/renderer/rendering_method");
	}

	if (rendering_driver.is_empty()) {
		if (rendering_method == "gl_compatibility") {
			rendering_driver = GLOBAL_GET("rendering/gl_compatibility/driver");
		} else {
			rendering_driver = GLOBAL_GET("rendering/rendering_device/driver");
		}
	}

	// always convert to lower case for consistency in the code
	rendering_driver = rendering_driver.to_lower();

	OS::get_singleton()->set_current_rendering_driver_name(rendering_driver);
	OS::get_singleton()->set_current_rendering_method(rendering_method);

#ifdef TOOLS_ENABLED
	if (!force_res && project_manager) {
		// Ensure splash screen size matches the project manager window size
		// (see `editor/project_manager.cpp` for defaults).
		window_size.width = ProjectManager::DEFAULT_WINDOW_WIDTH;
		window_size.height = ProjectManager::DEFAULT_WINDOW_HEIGHT;
	}
#endif

	if (use_custom_res) {
		if (!force_res) {
			window_size.width = GLOBAL_GET("display/window/size/viewport_width");
			window_size.height = GLOBAL_GET("display/window/size/viewport_height");

			if (globals->has_setting("display/window/size/window_width_override") &&
					globals->has_setting("display/window/size/window_height_override")) {
				int desired_width = GLOBAL_GET("display/window/size/window_width_override");
				if (desired_width > 0) {
					window_size.width = desired_width;
				}
				int desired_height = GLOBAL_GET("display/window/size/window_height_override");
				if (desired_height > 0) {
					window_size.height = desired_height;
				}
			}
		}

		if (!bool(GLOBAL_GET("display/window/size/resizable"))) {
			window_flags |= DisplayServer::WINDOW_FLAG_RESIZE_DISABLED_BIT;
		}
		if (bool(GLOBAL_GET("display/window/size/borderless"))) {
			window_flags |= DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
		}
		if (bool(GLOBAL_GET("display/window/size/always_on_top"))) {
			window_flags |= DisplayServer::WINDOW_FLAG_ALWAYS_ON_TOP_BIT;
		}
		if (bool(GLOBAL_GET("display/window/size/transparent"))) {
			window_flags |= DisplayServer::WINDOW_FLAG_TRANSPARENT_BIT;
		}
		if (bool(GLOBAL_GET("display/window/size/extend_to_title"))) {
			window_flags |= DisplayServer::WINDOW_FLAG_EXTEND_TO_TITLE_BIT;
		}
		if (bool(GLOBAL_GET("display/window/size/no_focus"))) {
			window_flags |= DisplayServer::WINDOW_FLAG_NO_FOCUS_BIT;
		}
		if (bool(GLOBAL_GET("display/window/size/sharp_corners"))) {
			window_flags |= DisplayServer::WINDOW_FLAG_SHARP_CORNERS_BIT;
		}
		window_mode = (DisplayServer::WindowMode)(GLOBAL_GET("display/window/size/mode").operator int());
		int initial_position_type = GLOBAL_GET("display/window/size/initial_position_type").operator int();
		if (initial_position_type == 0) { // Absolute.
			if (!init_use_custom_pos) {
				init_custom_pos = GLOBAL_GET("display/window/size/initial_position").operator Vector2i();
				init_use_custom_pos = true;
			}
		} else if (initial_position_type == 1) { // Center of Primary Screen.
			if (!init_use_custom_screen) {
				init_screen = DisplayServer::SCREEN_PRIMARY;
				init_use_custom_screen = true;
			}
		} else if (initial_position_type == 2) { // Center of Other Screen.
			if (!init_use_custom_screen) {
				init_screen = GLOBAL_GET("display/window/size/initial_screen").operator int();
				init_use_custom_screen = true;
			}
		} else if (initial_position_type == 3) { // Center of Screen With Mouse Pointer.
			if (!init_use_custom_screen) {
				init_screen = DisplayServer::SCREEN_WITH_MOUSE_FOCUS;
				init_use_custom_screen = true;
			}
		} else if (initial_position_type == 4) { // Center of Screen With Keyboard Focus.
			if (!init_use_custom_screen) {
				init_screen = DisplayServer::SCREEN_WITH_KEYBOARD_FOCUS;
				init_use_custom_screen = true;
			}
		}
	}

	GLOBAL_DEF_BASIC("internationalization/locale/include_text_server_data", false);

	OS::get_singleton()->_allow_hidpi = GLOBAL_DEF("display/window/dpi/allow_hidpi", true);
	OS::get_singleton()->_allow_layered = GLOBAL_DEF_RST("display/window/per_pixel_transparency/allowed", false);

#ifdef TOOLS_ENABLED
	if (editor || project_manager) {
		// The editor and project manager always detect and use hiDPI if needed.
		OS::get_singleton()->_allow_hidpi = true;
		// Disable Vulkan overlays in editor, they cause various issues.
		OS::get_singleton()->set_environment("DISABLE_MANGOHUD", "1"); // GH-57403.
		OS::get_singleton()->set_environment("DISABLE_RTSS_LAYER", "1"); // GH-57937.
		OS::get_singleton()->set_environment("DISABLE_VKBASALT", "1");
		OS::get_singleton()->set_environment("DISABLE_VK_LAYER_reshade_1", "1"); // GH-70849.
		OS::get_singleton()->set_environment("VK_LAYER_bandicam_helper_DEBUG_1", "1"); // GH-101480.
		OS::get_singleton()->set_environment("DISABLE_VK_LAYER_bandicam_helper_1", "1"); // GH-101480.
	} else {
		// Re-allow using Vulkan overlays, disabled while using the editor.
		OS::get_singleton()->unset_environment("DISABLE_MANGOHUD");
		OS::get_singleton()->unset_environment("DISABLE_RTSS_LAYER");
		OS::get_singleton()->unset_environment("DISABLE_VKBASALT");
		OS::get_singleton()->unset_environment("DISABLE_VK_LAYER_reshade_1");
		OS::get_singleton()->unset_environment("VK_LAYER_bandicam_helper_DEBUG_1");
		OS::get_singleton()->unset_environment("DISABLE_VK_LAYER_bandicam_helper_1");
	}
#endif

	if (separate_thread_render == -1) {
		separate_thread_render = (int)GLOBAL_DEF("rendering/driver/threads/thread_model", OS::RENDER_THREAD_SAFE) == OS::RENDER_SEPARATE_THREAD;
	}

	if (editor || project_manager) {
		// Editor and project manager cannot run with rendering in a separate thread (they will crash on startup).
		separate_thread_render = 0;
	}
#if !defined(THREADS_ENABLED)
	separate_thread_render = 0;
#endif
	OS::get_singleton()->_separate_thread_render = separate_thread_render;

	/* Determine audio and video drivers */

	// Display driver, e.g. X11, Wayland.
	// Make sure that headless is the last one, which it is assumed to be by design.
	DEV_ASSERT(NULL_DISPLAY_DRIVER == DisplayServer::get_create_function_name(DisplayServer::get_create_function_count() - 1));

	GLOBAL_DEF_NOVAL("display/display_server/driver", "default");
	GLOBAL_DEF_NOVAL(PropertyInfo(Variant::STRING, "display/display_server/driver.windows", PROPERTY_HINT_ENUM_SUGGESTION, "default,windows,headless"), "default");
	GLOBAL_DEF_NOVAL(PropertyInfo(Variant::STRING, "display/display_server/driver.linuxbsd", PROPERTY_HINT_ENUM_SUGGESTION, "default,x11,wayland,headless"), "default");
	GLOBAL_DEF_NOVAL(PropertyInfo(Variant::STRING, "display/display_server/driver.android", PROPERTY_HINT_ENUM_SUGGESTION, "default,android,headless"), "default");
	GLOBAL_DEF_NOVAL(PropertyInfo(Variant::STRING, "display/display_server/driver.ios", PROPERTY_HINT_ENUM_SUGGESTION, "default,iOS,headless"), "default");
	GLOBAL_DEF_NOVAL(PropertyInfo(Variant::STRING, "display/display_server/driver.macos", PROPERTY_HINT_ENUM_SUGGESTION, "default,macos,headless"), "default");

	GLOBAL_DEF_RST_NOVAL("audio/driver/driver", AudioDriverManager::get_driver(0)->get_name());
	if (audio_driver.is_empty()) { // Specified in project.godot.
		if (project_manager) {
			// The project manager doesn't need to play sound (TTS audio output is not emitted by Godot, but by the system itself).
			// Disable audio output so it doesn't appear in the list of applications outputting sound in the OS.
			// On macOS, this also prevents the project manager from inhibiting suspend.
			audio_driver = "Dummy";
		} else {
			audio_driver = GLOBAL_GET("audio/driver/driver");
		}
	}

	// Make sure that dummy is the last one, which it is assumed to be by design.
	DEV_ASSERT(NULL_AUDIO_DRIVER == AudioDriverManager::get_driver(AudioDriverManager::get_driver_count() - 1)->get_name());
	for (int i = 0; i < AudioDriverManager::get_driver_count(); i++) {
		if (audio_driver == AudioDriverManager::get_driver(i)->get_name()) {
			audio_driver_idx = i;
			break;
		}
	}

	if (audio_driver_idx < 0) {
		// If the requested driver wasn't found, pick the first entry.
		// If all else failed it would be the dummy driver (no sound).
		audio_driver_idx = 0;
	}

	if (Engine::get_singleton()->get_write_movie_path() != String()) {
		// Always use dummy driver for audio driver (which is last), also in no threaded mode.
		audio_driver_idx = AudioDriverManager::get_driver_count() - 1;
		AudioDriverDummy::get_dummy_singleton()->set_use_threads(false);
	}

	{
		window_orientation = DisplayServer::ScreenOrientation(int(GLOBAL_DEF_BASIC("display/window/handheld/orientation", DisplayServer::ScreenOrientation::SCREEN_LANDSCAPE)));
	}
	{
		window_vsync_mode = DisplayServer::VSyncMode(int(GLOBAL_DEF_BASIC("display/window/vsync/vsync_mode", DisplayServer::VSyncMode::VSYNC_ENABLED)));
		if (disable_vsync) {
			window_vsync_mode = DisplayServer::VSyncMode::VSYNC_DISABLED;
		}
	}

	GLOBAL_DEF_RST(PropertyInfo(Variant::INT, "audio/driver/output_latency", PROPERTY_HINT_RANGE, "1,100,1"), 15);
	// Use a safer default output_latency for web to avoid audio cracking on low-end devices, especially mobile.
	GLOBAL_DEF_RST("audio/driver/output_latency.web", 50);

	Engine::get_singleton()->set_audio_output_latency(GLOBAL_GET("audio/driver/output_latency"));

#if defined(MACOS_ENABLED) || defined(IOS_ENABLED)
	OS::get_singleton()->set_environment("MVK_CONFIG_LOG_LEVEL", OS::get_singleton()->_verbose_stdout ? "3" : "1"); // 1 = Errors only, 3 = Info
#endif

	if (frame_delay == 0) {
		frame_delay = GLOBAL_DEF(PropertyInfo(Variant::INT, "application/run/frame_delay_msec", PROPERTY_HINT_RANGE, "0,100,1,or_greater"), 0);
		if (Engine::get_singleton()->is_editor_hint()) {
			frame_delay = 0;
		}
	}

	if (audio_output_latency >= 1) {
		Engine::get_singleton()->set_audio_output_latency(audio_output_latency);
	}

	GLOBAL_DEF("display/window/ios/allow_high_refresh_rate", true);
	GLOBAL_DEF("display/window/ios/hide_home_indicator", true);
	GLOBAL_DEF("display/window/ios/hide_status_bar", true);
	GLOBAL_DEF("display/window/ios/suppress_ui_gesture", true);

#ifndef _3D_DISABLED
	// XR project settings.
	GLOBAL_DEF_RST_BASIC("xr/openxr/enabled", false);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "xr/openxr/default_action_map", PROPERTY_HINT_FILE, "*.tres"), "res://openxr_action_map.tres");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "xr/openxr/form_factor", PROPERTY_HINT_ENUM, "Head Mounted,Handheld"), "0");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "xr/openxr/view_configuration", PROPERTY_HINT_ENUM, "Mono,Stereo"), "1"); // "Mono,Stereo,Quad,Observer"
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "xr/openxr/reference_space", PROPERTY_HINT_ENUM, "Local,Stage,Local Floor"), "1");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "xr/openxr/environment_blend_mode", PROPERTY_HINT_ENUM, "Opaque,Additive,Alpha"), "0");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "xr/openxr/foveation_level", PROPERTY_HINT_ENUM, "Off,Low,Medium,High"), "0");
	GLOBAL_DEF_BASIC("xr/openxr/foveation_dynamic", false);

	GLOBAL_DEF_BASIC("xr/openxr/submit_depth_buffer", false);
	GLOBAL_DEF_BASIC("xr/openxr/startup_alert", true);

	// OpenXR project extensions settings.
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "xr/openxr/extensions/debug_utils", PROPERTY_HINT_ENUM, "Disabled,Error,Warning,Info,Verbose"), "0");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "xr/openxr/extensions/debug_message_types", PROPERTY_HINT_FLAGS, "General,Validation,Performance,Conformance"), "15");
	GLOBAL_DEF_BASIC("xr/openxr/extensions/hand_tracking", false);
	GLOBAL_DEF_BASIC("xr/openxr/extensions/hand_tracking_unobstructed_data_source", false); // XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT
	GLOBAL_DEF_BASIC("xr/openxr/extensions/hand_tracking_controller_data_source", false); // XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT
	GLOBAL_DEF_RST_BASIC("xr/openxr/extensions/hand_interaction_profile", false);
	GLOBAL_DEF_RST_BASIC("xr/openxr/extensions/eye_gaze_interaction", false);

	// OpenXR Binding modifier settings
	GLOBAL_DEF_BASIC("xr/openxr/binding_modifiers/analog_threshold", false);
	GLOBAL_DEF_RST_BASIC("xr/openxr/binding_modifiers/dpad_binding", false);

#ifdef TOOLS_ENABLED
	// Disabled for now, using XR inside of the editor we'll be working on during the coming months.

	// editor settings (it seems we're too early in the process when setting up rendering, to access editor settings...)
	// EDITOR_DEF_RST("xr/openxr/in_editor", false);
	// GLOBAL_DEF("xr/openxr/in_editor", false);
#endif // TOOLS_ENABLED
#endif // _3D_DISABLED

	Engine::get_singleton()->set_frame_delay(frame_delay);

	message_queue = memnew(MessageQueue);

	Thread::release_main_thread(); // If setup2() is called from another thread, that one will become main thread, so preventively release this one.
	set_current_thread_safe_for_nodes(false);

#if defined(STEAMAPI_ENABLED)
	if (editor || project_manager) {
		steam_tracker = memnew(SteamTracker);
	}
#endif

	OS::get_singleton()->benchmark_end_measure("Startup", "Main::Setup");

	if (p_second_phase) {
		exit_err = setup2();
		if (exit_err != OK) {
			goto error;
		}
	}

	return OK;

error:

	text_driver = "";
	display_driver = "";
	audio_driver = "";
	tablet_driver = "";
	Engine::get_singleton()->set_write_movie_path(String());
	project_path = "";

	args.clear();
	main_args.clear();

	if (show_help) {
		print_help(execpath);
	}

	if (editor) {
		OS::get_singleton()->remove_lock_file();
	}

	EngineDebugger::deinitialize();

	if (performance) {
		memdelete(performance);
	}
	if (input_map) {
		memdelete(input_map);
	}
	if (translation_server) {
		memdelete(translation_server);
	}
	if (globals) {
		memdelete(globals);
	}
	if (packed_data) {
		memdelete(packed_data);
	}

	unregister_core_driver_types();
	unregister_core_extensions();

	if (engine) {
		memdelete(engine);
	}

	unregister_core_types();

	OS::get_singleton()->_cmdline.clear();
	OS::get_singleton()->_user_args.clear();

	if (message_queue) {
		memdelete(message_queue);
	}

	OS::get_singleton()->benchmark_end_measure("Startup", "Main::Setup");

#if defined(STEAMAPI_ENABLED)
	if (steam_tracker) {
		memdelete(steam_tracker);
	}
#endif

	OS::get_singleton()->finalize_core();
	locale = String();

	return exit_err;
}

Error _parse_resource_dummy(void *p_data, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or String (ext-resource ID)";
		return ERR_PARSE_ERROR;
	}

	r_res.unref();

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error Main::setup2(bool p_show_boot_logo) {
	OS::get_singleton()->benchmark_begin_measure("Startup", "Main::Setup2");

	Thread::make_main_thread(); // Make whatever thread call this the main thread.
	set_current_thread_safe_for_nodes(true);

	// Don't use rich formatting to prevent ANSI escape codes from being written to log files.
	print_header(false);

#ifdef TOOLS_ENABLED
	if (editor || project_manager || cmdline_tool) {
		OS::get_singleton()->benchmark_begin_measure("Startup", "Initialize Early Settings");

		EditorPaths::create();

		// Editor setting class is not available, load config directly.
		if (!init_use_custom_screen && (editor || project_manager) && EditorPaths::get_singleton()->are_paths_valid()) {
			ERR_FAIL_COND_V(!DirAccess::dir_exists_absolute(EditorPaths::get_singleton()->get_config_dir()), FAILED);

			String config_file_path = EditorSettings::get_existing_settings_path();
			if (FileAccess::exists(config_file_path)) {
				Error err;
				Ref<FileAccess> f = FileAccess::open(config_file_path, FileAccess::READ, &err);
				if (f.is_valid()) {
					VariantParser::StreamFile stream;
					stream.f = f;

					String assign;
					Variant value;
					VariantParser::Tag next_tag;

					int lines = 0;
					String error_text;

					VariantParser::ResourceParser rp_new;
					rp_new.ext_func = _parse_resource_dummy;
					rp_new.sub_func = _parse_resource_dummy;

					bool screen_found = false;
					String screen_property;

					bool prefer_wayland_found = false;
					bool prefer_wayland = false;

					if (editor) {
						screen_property = "interface/editor/editor_screen";
					} else if (project_manager) {
						screen_property = "interface/editor/project_manager_screen";
					} else {
						// Skip.
						screen_found = true;
					}

					if (!display_driver.is_empty()) {
						// Skip.
						prefer_wayland_found = true;
					}

					while (!screen_found || !prefer_wayland_found) {
						assign = Variant();
						next_tag.fields.clear();
						next_tag.name = String();

						err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp_new, true);
						if (err == ERR_FILE_EOF) {
							break;
						}

						if (err == OK && !assign.is_empty()) {
							if (!screen_found && assign == screen_property) {
								init_screen = value;
								screen_found = true;

								if (editor) {
									restore_editor_window_layout = value.operator int() == EditorSettings::InitialScreen::INITIAL_SCREEN_AUTO;
								}
							}

							if (!prefer_wayland_found && assign == "run/platforms/linuxbsd/prefer_wayland") {
								prefer_wayland = value;
								prefer_wayland_found = true;
							}
						}
					}

					if (display_driver.is_empty()) {
						if (prefer_wayland) {
							display_driver = "wayland";
						} else {
							display_driver = "default";
						}
					}
				}
			}
		}

		if (found_project && EditorPaths::get_singleton()->is_self_contained()) {
			if (ProjectSettings::get_singleton()->get_resource_path() == OS::get_singleton()->get_executable_path().get_base_dir()) {
				ERR_PRINT("You are trying to run a self-contained editor at the same location as a project. This is not allowed, since editor files will mix with project files.");
				OS::get_singleton()->set_exit_code(EXIT_FAILURE);
				return FAILED;
			}
		}

		bool has_command_line_window_override = init_use_custom_pos || init_use_custom_screen || init_windowed;
		if (editor && !has_command_line_window_override && restore_editor_window_layout) {
			Ref<ConfigFile> config;
			config.instantiate();
			// Load and amend existing config if it exists.
			Error err = config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
			if (err == OK) {
				init_screen = config->get_value("EditorWindow", "screen", init_screen);
				String mode = config->get_value("EditorWindow", "mode", "maximized");
				window_size = config->get_value("EditorWindow", "size", window_size);
				if (mode == "windowed") {
					window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
					init_windowed = true;
				} else if (mode == "fullscreen") {
					window_mode = DisplayServer::WINDOW_MODE_FULLSCREEN;
					init_fullscreen = true;
				} else {
					window_mode = DisplayServer::WINDOW_MODE_MAXIMIZED;
					init_maximized = true;
				}

				if (init_windowed) {
					init_use_custom_pos = true;
					init_custom_pos = config->get_value("EditorWindow", "position", Vector2i(0, 0));
				}
			}
		}

		if (init_screen == EditorSettings::InitialScreen::INITIAL_SCREEN_AUTO) {
			init_screen = DisplayServer::SCREEN_PRIMARY;
		}

		OS::get_singleton()->benchmark_end_measure("Startup", "Initialize Early Settings");
	}
#endif

	OS::get_singleton()->benchmark_begin_measure("Startup", "Servers");

	tsman = memnew(TextServerManager);
	if (tsman) {
		Ref<TextServerDummy> ts;
		ts.instantiate();
		tsman->add_interface(ts);
	}

#ifndef _3D_DISABLED
	physics_server_3d_manager = memnew(PhysicsServer3DManager);
#endif // _3D_DISABLED
	physics_server_2d_manager = memnew(PhysicsServer2DManager);

	register_server_types();
	{
		OS::get_singleton()->benchmark_begin_measure("Servers", "Modules and Extensions");

		initialize_modules(MODULE_INITIALIZATION_LEVEL_SERVERS);
		GDExtensionManager::get_singleton()->initialize_extensions(GDExtension::INITIALIZATION_LEVEL_SERVERS);

		OS::get_singleton()->benchmark_end_measure("Servers", "Modules and Extensions");
	}

	/* Initialize Input */

	{
		OS::get_singleton()->benchmark_begin_measure("Servers", "Input");

		input = memnew(Input);
		OS::get_singleton()->initialize_joypads();

		OS::get_singleton()->benchmark_end_measure("Servers", "Input");
	}

	/* Initialize Display Server */

	{
		OS::get_singleton()->benchmark_begin_measure("Servers", "Display");

		if (display_driver.is_empty()) {
			display_driver = GLOBAL_GET("display/display_server/driver");
		}

		int display_driver_idx = -1;

		if (display_driver.is_empty() || display_driver == "default") {
			display_driver_idx = 0;
		} else {
			for (int i = 0; i < DisplayServer::get_create_function_count(); i++) {
				String name = DisplayServer::get_create_function_name(i);
				if (display_driver == name) {
					display_driver_idx = i;
					break;
				}
			}

			if (display_driver_idx < 0) {
				// If the requested driver wasn't found, pick the first entry.
				// If all else failed it would be the headless server.
				display_driver_idx = 0;
			}
		}

		Vector2i *window_position = nullptr;
		Vector2i position = init_custom_pos;
		if (init_use_custom_pos) {
			window_position = &position;
		}

		Color boot_bg_color = GLOBAL_DEF_BASIC("application/boot_splash/bg_color", boot_splash_bg_color);
		DisplayServer::set_early_window_clear_color_override(true, boot_bg_color);

		DisplayServer::Context context;
		if (editor) {
			context = DisplayServer::CONTEXT_EDITOR;
		} else if (project_manager) {
			context = DisplayServer::CONTEXT_PROJECTMAN;
		} else {
			context = DisplayServer::CONTEXT_ENGINE;
		}

		if (init_embed_parent_window_id) {
			// Reset flags and other settings to be sure it's borderless and windowed. The position and size should have been initialized correctly
			// from --position and --resolution parameters.
			window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
			window_flags = DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
		}

		// rendering_driver now held in static global String in main and initialized in setup()
		Error err;
		display_server = DisplayServer::create(display_driver_idx, rendering_driver, window_mode, window_vsync_mode, window_flags, window_position, window_size, init_screen, context, init_embed_parent_window_id, err);
		if (err != OK || display_server == nullptr) {
			String last_name = DisplayServer::get_create_function_name(display_driver_idx);

			// We can't use this display server, try other ones as fallback.
			// Skip headless (always last registered) because that's not what users
			// would expect if they didn't request it explicitly.
			for (int i = 0; i < DisplayServer::get_create_function_count() - 1; i++) {
				if (i == display_driver_idx) {
					continue; // Don't try the same twice.
				}
				String name = DisplayServer::get_create_function_name(i);
				WARN_PRINT(vformat("Display driver %s failed, falling back to %s.", last_name, name));

				display_server = DisplayServer::create(i, rendering_driver, window_mode, window_vsync_mode, window_flags, window_position, window_size, init_screen, context, init_embed_parent_window_id, err);
				if (err == OK && display_server != nullptr) {
					break;
				}
			}
		}

		if (err != OK || display_server == nullptr) {
			ERR_PRINT("Unable to create DisplayServer, all display drivers failed.\nUse \"--headless\" command line argument to run the engine in headless mode if this is desired (e.g. for continuous integration).");

			if (display_server) {
				memdelete(display_server);
			}

			GDExtensionManager::get_singleton()->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_SERVERS);
			uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SERVERS);
			unregister_server_types();

			if (input) {
				memdelete(input);
			}
			if (tsman) {
				memdelete(tsman);
			}
#ifndef _3D_DISABLED
			if (physics_server_3d_manager) {
				memdelete(physics_server_3d_manager);
			}
#endif // _3D_DISABLED
			if (physics_server_2d_manager) {
				memdelete(physics_server_2d_manager);
			}

			return err;
		}

		if (display_server->has_feature(DisplayServer::FEATURE_ORIENTATION)) {
			display_server->screen_set_orientation(window_orientation);
		}

		OS::get_singleton()->benchmark_end_measure("Servers", "Display");
	}

	// Max FPS needs to be set after the DisplayServer is created.
	RenderingDevice *rd = RenderingDevice::get_singleton();
	if (rd) {
		rd->_set_max_fps(engine->get_max_fps());
	}

#ifdef TOOLS_ENABLED
	// If the editor is running in windowed mode, ensure the window rect fits
	// the screen in case screen count or position has changed.
	if (editor && init_windowed) {
		// We still need to check we are actually in windowed mode, because
		// certain platform might only support one fullscreen window.
		if (DisplayServer::get_singleton()->window_get_mode() == DisplayServer::WINDOW_MODE_WINDOWED) {
			Vector2i current_size = DisplayServer::get_singleton()->window_get_size();
			Vector2i current_pos = DisplayServer::get_singleton()->window_get_position();
			int screen = DisplayServer::get_singleton()->window_get_current_screen();
			Rect2i screen_rect = DisplayServer::get_singleton()->screen_get_usable_rect(screen);

			Vector2i adjusted_end = screen_rect.get_end().min(current_pos + current_size);
			Vector2i adjusted_pos = screen_rect.get_position().max(adjusted_end - current_size);
			Vector2i adjusted_size = DisplayServer::get_singleton()->window_get_min_size().max(adjusted_end - adjusted_pos);

			if (current_pos != adjusted_end || current_size != adjusted_size) {
				DisplayServer::get_singleton()->window_set_position(adjusted_pos);
				DisplayServer::get_singleton()->window_set_size(adjusted_size);
			}
		}
	}
#endif

	if (GLOBAL_GET("debug/settings/stdout/print_fps") || print_fps) {
		// Print requested V-Sync mode at startup to diagnose the printed FPS not going above the monitor refresh rate.
		switch (window_vsync_mode) {
			case DisplayServer::VSyncMode::VSYNC_DISABLED:
				print_line("Requested V-Sync mode: Disabled");
				break;
			case DisplayServer::VSyncMode::VSYNC_ENABLED:
				print_line("Requested V-Sync mode: Enabled - FPS will likely be capped to the monitor refresh rate.");
				break;
			case DisplayServer::VSyncMode::VSYNC_ADAPTIVE:
				print_line("Requested V-Sync mode: Adaptive");
				break;
			case DisplayServer::VSyncMode::VSYNC_MAILBOX:
				print_line("Requested V-Sync mode: Mailbox");
				break;
		}
	}

	if (OS::get_singleton()->_separate_thread_render) {
		WARN_PRINT("The separate rendering thread feature is experimental. Feel free to try it since it will eventually become a stable feature.\n"
				   "However, bear in mind that at the moment it can lead to project crashes or instability.\n"
				   "So, unless you want to test the engine, set the \"rendering/driver/threads/thread_model\" project setting to 'Safe'.");
	}

	/* Initialize Pen Tablet Driver */

	{
		OS::get_singleton()->benchmark_begin_measure("Servers", "Tablet Driver");

		GLOBAL_DEF_RST_NOVAL("input_devices/pen_tablet/driver", "");
		GLOBAL_DEF_RST_NOVAL(PropertyInfo(Variant::STRING, "input_devices/pen_tablet/driver.windows", PROPERTY_HINT_ENUM, "winink,wintab,dummy"), "");

		if (tablet_driver.is_empty()) { // specified in project.godot
			tablet_driver = GLOBAL_GET("input_devices/pen_tablet/driver");
			if (tablet_driver.is_empty()) {
				tablet_driver = DisplayServer::get_singleton()->tablet_get_driver_name(0);
			}
		}

		for (int i = 0; i < DisplayServer::get_singleton()->tablet_get_driver_count(); i++) {
			if (tablet_driver == DisplayServer::get_singleton()->tablet_get_driver_name(i)) {
				DisplayServer::get_singleton()->tablet_set_current_driver(DisplayServer::get_singleton()->tablet_get_driver_name(i));
				break;
			}
		}

		if (DisplayServer::get_singleton()->tablet_get_current_driver().is_empty()) {
			DisplayServer::get_singleton()->tablet_set_current_driver(DisplayServer::get_singleton()->tablet_get_driver_name(0));
		}

		print_verbose("Using \"" + tablet_driver + "\" pen tablet driver...");

		OS::get_singleton()->benchmark_end_measure("Servers", "Tablet Driver");
	}

	/* Initialize Rendering Server */

	{
		OS::get_singleton()->benchmark_begin_measure("Servers", "Rendering");

		rendering_server = memnew(RenderingServerDefault(OS::get_singleton()->is_separate_thread_rendering_enabled()));

		rendering_server->init();
		//rendering_server->call_set_use_vsync(OS::get_singleton()->_use_vsync);
		rendering_server->set_render_loop_enabled(!disable_render_loop);

		if (profile_gpu || (!editor && bool(GLOBAL_GET("debug/settings/stdout/print_gpu_profile")))) {
			rendering_server->set_print_gpu_profile(true);
		}

		if (Engine::get_singleton()->get_write_movie_path() != String()) {
			movie_writer = MovieWriter::find_writer_for_file(Engine::get_singleton()->get_write_movie_path());
			if (movie_writer == nullptr) {
				ERR_PRINT("Can't find movie writer for file type, aborting: " + Engine::get_singleton()->get_write_movie_path());
				Engine::get_singleton()->set_write_movie_path(String());
			}
		}

		OS::get_singleton()->benchmark_end_measure("Servers", "Rendering");
	}

#ifdef UNIX_ENABLED
	// Print warning after initializing the renderer but before initializing audio.
	if (OS::get_singleton()->get_environment("USER") == "root" && !OS::get_singleton()->has_environment("GODOT_SILENCE_ROOT_WARNING")) {
		WARN_PRINT("Started the engine as `root`/superuser. This is a security risk, and subsystems like audio may not work correctly.\nSet the environment variable `GODOT_SILENCE_ROOT_WARNING` to 1 to silence this warning.");
	}
#endif

	/* Initialize Audio Driver */

	{
		OS::get_singleton()->benchmark_begin_measure("Servers", "Audio");

		AudioDriverManager::initialize(audio_driver_idx);

		// Right moment to create and initialize the audio server.
		audio_server = memnew(AudioServer);
		audio_server->init();

		OS::get_singleton()->benchmark_end_measure("Servers", "Audio");
	}

#ifndef _3D_DISABLED
	/* Initialize XR Server */

	{
		OS::get_singleton()->benchmark_begin_measure("Servers", "XR");

		xr_server = memnew(XRServer);

		OS::get_singleton()->benchmark_end_measure("Servers", "XR");
	}
#endif // _3D_DISABLED

	OS::get_singleton()->benchmark_end_measure("Startup", "Servers");

#ifndef WEB_ENABLED
	// Add a blank line for readability.
	Engine::get_singleton()->print_header("");
#endif // WEB_ENABLED

	register_core_singletons();

	/* Initialize the main window and boot screen */

	{
		OS::get_singleton()->benchmark_begin_measure("Startup", "Setup Window and Boot");

		MAIN_PRINT("Main: Setup Logo");

		if (!init_embed_parent_window_id) {
			if (init_windowed) {
				//do none..
			} else if (init_maximized) {
				DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_MAXIMIZED);
			} else if (init_fullscreen) {
				DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_FULLSCREEN);
			}
			if (init_always_on_top) {
				DisplayServer::get_singleton()->window_set_flag(DisplayServer::WINDOW_FLAG_ALWAYS_ON_TOP, true);
			}
		}

		Color clear = GLOBAL_DEF_BASIC("rendering/environment/defaults/default_clear_color", Color(0.3, 0.3, 0.3));
		RenderingServer::get_singleton()->set_default_clear_color(clear);

		if (p_show_boot_logo) {
			setup_boot_logo();
		}

		MAIN_PRINT("Main: Clear Color");

		DisplayServer::set_early_window_clear_color_override(false);

		GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "application/config/icon", PROPERTY_HINT_FILE, "*.png,*.bmp,*.hdr,*.jpg,*.jpeg,*.svg,*.tga,*.exr,*.webp"), String());
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "application/config/macos_native_icon", PROPERTY_HINT_FILE, "*.icns"), String());
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "application/config/windows_native_icon", PROPERTY_HINT_FILE, "*.ico"), String());

		MAIN_PRINT("Main: Touch Input");

		Input *id = Input::get_singleton();
		if (id) {
			bool agile_input_event_flushing = GLOBAL_DEF("input_devices/buffering/agile_event_flushing", false);
			id->set_agile_input_event_flushing(agile_input_event_flushing);

			if (bool(GLOBAL_DEF_BASIC("input_devices/pointing/emulate_touch_from_mouse", false)) &&
					!(editor || project_manager)) {
				if (!DisplayServer::get_singleton()->is_touchscreen_available()) {
					//only if no touchscreen ui hint, set emulation
					id->set_emulate_touch_from_mouse(true);
				}
			}

			id->set_emulate_mouse_from_touch(bool(GLOBAL_DEF_BASIC("input_devices/pointing/emulate_mouse_from_touch", true)));

			if (editor) {
				id->set_emulate_mouse_from_touch(true);
			}
		}

		OS::get_singleton()->benchmark_end_measure("Startup", "Setup Window and Boot");
	}

	MAIN_PRINT("Main: Load Translations and Remaps");

	/* Setup translations and remaps */

	{
		OS::get_singleton()->benchmark_begin_measure("Startup", "Translations and Remaps");

		translation_server->setup(); //register translations, load them, etc.
		if (!locale.is_empty()) {
			translation_server->set_locale(locale);
		}
		translation_server->load_translations();
		ResourceLoader::load_translation_remaps(); //load remaps for resources

		ResourceLoader::load_path_remaps();

		OS::get_singleton()->benchmark_end_measure("Startup", "Translations and Remaps");
	}

	MAIN_PRINT("Main: Load TextServer");

	/* Setup Text Server */

	{
		OS::get_singleton()->benchmark_begin_measure("Startup", "Text Server");

		/* Enum text drivers */
		GLOBAL_DEF_RST("internationalization/rendering/text_driver", "");
		String text_driver_options;
		for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
			const String driver_name = TextServerManager::get_singleton()->get_interface(i)->get_name();
			if (driver_name == "Dummy") {
				// Dummy text driver cannot draw any text, making the editor unusable if selected.
				continue;
			}
			if (!text_driver_options.is_empty() && !text_driver_options.contains_char(',')) {
				// Not the first option; add a comma before it as a separator for the property hint.
				text_driver_options += ",";
			}
			text_driver_options += driver_name;
		}
		ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, "internationalization/rendering/text_driver", PROPERTY_HINT_ENUM, text_driver_options));

		/* Determine text driver */
		if (text_driver.is_empty()) {
			text_driver = GLOBAL_GET("internationalization/rendering/text_driver");
		}

		if (!text_driver.is_empty()) {
			/* Load user selected text server. */
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				if (TextServerManager::get_singleton()->get_interface(i)->get_name() == text_driver) {
					text_driver_idx = i;
					break;
				}
			}
		}

		if (text_driver_idx < 0) {
			/* If not selected, use one with the most features available. */
			int max_features = 0;
			for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
				uint32_t features = TextServerManager::get_singleton()->get_interface(i)->get_features();
				int feature_number = 0;
				while (features) {
					feature_number += features & 1;
					features >>= 1;
				}
				if (feature_number >= max_features) {
					max_features = feature_number;
					text_driver_idx = i;
				}
			}
		}
		if (text_driver_idx >= 0) {
			Ref<TextServer> ts = TextServerManager::get_singleton()->get_interface(text_driver_idx);
			TextServerManager::get_singleton()->set_primary_interface(ts);
			if (ts->has_feature(TextServer::FEATURE_USE_SUPPORT_DATA)) {
				ts->load_support_data("res://" + ts->get_support_data_filename());
			}
		} else {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, "TextServer: Unable to create TextServer interface.");
		}

		OS::get_singleton()->benchmark_end_measure("Startup", "Text Server");
	}

	MAIN_PRINT("Main: Load Scene Types");

	OS::get_singleton()->benchmark_begin_measure("Startup", "Scene");

	// Initialize ThemeDB early so that scene types can register their theme items.
	// Default theme will be initialized later, after modules and ScriptServer are ready.
	initialize_theme_db();

	MAIN_PRINT("Main: Load Navigation");

	NavigationServer3DManager::initialize_server(); // 3D server first because 2D depends on it.
	NavigationServer2DManager::initialize_server();

	register_scene_types();
	register_driver_types();

	register_scene_singletons();

	{
		OS::get_singleton()->benchmark_begin_measure("Scene", "Modules and Extensions");

		initialize_modules(MODULE_INITIALIZATION_LEVEL_SCENE);
		GDExtensionManager::get_singleton()->initialize_extensions(GDExtension::INITIALIZATION_LEVEL_SCENE);

		OS::get_singleton()->benchmark_end_measure("Scene", "Modules and Extensions");
	}

	PackedStringArray extensions;
	extensions.push_back("gd");
	if (ClassDB::class_exists("CSharpScript")) {
		extensions.push_back("cs");
	}
	extensions.push_back("gdshader");
	GLOBAL_DEF_NOVAL(PropertyInfo(Variant::PACKED_STRING_ARRAY, "editor/script/search_in_file_extensions"), extensions); // Note: should be defined after Scene level modules init to see .NET.

	OS::get_singleton()->benchmark_end_measure("Startup", "Scene");

#ifdef TOOLS_ENABLED
	ClassDB::set_current_api(ClassDB::API_EDITOR);
	register_editor_types();

	{
		OS::get_singleton()->benchmark_begin_measure("Editor", "Modules and Extensions");

		initialize_modules(MODULE_INITIALIZATION_LEVEL_EDITOR);
		GDExtensionManager::get_singleton()->initialize_extensions(GDExtension::INITIALIZATION_LEVEL_EDITOR);

		OS::get_singleton()->benchmark_end_measure("Editor", "Modules and Extensions");
	}

	ClassDB::set_current_api(ClassDB::API_CORE);

#endif

	MAIN_PRINT("Main: Load Platforms");

	OS::get_singleton()->benchmark_begin_measure("Startup", "Platforms");

	register_platform_apis();

	OS::get_singleton()->benchmark_end_measure("Startup", "Platforms");

	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "display/mouse_cursor/custom_image", PROPERTY_HINT_FILE, "*.png,*.bmp,*.hdr,*.jpg,*.jpeg,*.svg,*.tga,*.exr,*.webp"), String());
	GLOBAL_DEF_BASIC("display/mouse_cursor/custom_image_hotspot", Vector2());
	GLOBAL_DEF_BASIC("display/mouse_cursor/tooltip_position_offset", Point2(10, 10));

	if (String(GLOBAL_GET("display/mouse_cursor/custom_image")) != String()) {
		Ref<Texture2D> cursor = ResourceLoader::load(
				GLOBAL_GET("display/mouse_cursor/custom_image"));
		if (cursor.is_valid()) {
			Vector2 hotspot = GLOBAL_GET("display/mouse_cursor/custom_image_hotspot");
			Input::get_singleton()->set_custom_mouse_cursor(cursor, Input::CURSOR_ARROW, hotspot);
		}
	}

	OS::get_singleton()->benchmark_begin_measure("Startup", "Finalize Setup");

	camera_server = CameraServer::create();

	MAIN_PRINT("Main: Load Physics");

	initialize_physics();

	register_server_singletons();

	// This loads global classes, so it must happen before custom loaders and savers are registered
	ScriptServer::init_languages();

	theme_db->initialize_theme();
	audio_server->load_default_bus_layout();

#if defined(MODULE_MONO_ENABLED) && defined(TOOLS_ENABLED)
	// Hacky to have it here, but we don't have good facility yet to let modules
	// register command line options to call at the right time. This needs to happen
	// after init'ing the ScriptServer, but also after init'ing the ThemeDB,
	// for the C# docs generation in the bindings.
	List<String> cmdline_args = OS::get_singleton()->get_cmdline_args();
	BindingsGenerator::handle_cmdline_args(cmdline_args);
#endif

	if (use_debug_profiler && EngineDebugger::is_active()) {
		// Start the "scripts" profiler, used in local debugging.
		// We could add more, and make the CLI arg require a comma-separated list of profilers.
		EngineDebugger::get_singleton()->profiler_enable("scripts", true);
	}

	if (!project_manager) {
		// If not running the project manager, and now that the engine is
		// able to load resources, load the global shader variables.
		// If running on editor, don't load the textures because the editor
		// may want to import them first. Editor will reload those later.
		rendering_server->global_shader_parameters_load_settings(!editor);
	}

	OS::get_singleton()->benchmark_end_measure("Startup", "Finalize Setup");

	_start_success = true;

	ClassDB::set_current_api(ClassDB::API_NONE); //no more APIs are registered at this point

	print_verbose("CORE API HASH: " + uitos(ClassDB::get_api_hash(ClassDB::API_CORE)));
	print_verbose("EDITOR API HASH: " + uitos(ClassDB::get_api_hash(ClassDB::API_EDITOR)));
	MAIN_PRINT("Main: Done");

	OS::get_singleton()->benchmark_end_measure("Startup", "Main::Setup2");

	return OK;
}

void Main::setup_boot_logo() {
	MAIN_PRINT("Main: Load Boot Image");

#if !defined(TOOLS_ENABLED) && defined(WEB_ENABLED)
	bool show_logo = false;
#else
	bool show_logo = true;
#endif

	if (show_logo) { //boot logo!
		const bool boot_logo_image = GLOBAL_DEF_BASIC("application/boot_splash/show_image", true);
		const String boot_logo_path = ResourceUID::ensure_path(GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "application/boot_splash/image", PROPERTY_HINT_FILE, "*.png"), String())).strip_edges();
		const bool boot_logo_scale = GLOBAL_DEF_BASIC("application/boot_splash/fullsize", true);
		const bool boot_logo_filter = GLOBAL_DEF_BASIC("application/boot_splash/use_filter", true);

		Ref<Image> boot_logo;

		if (boot_logo_image) {
			if (!boot_logo_path.is_empty()) {
				boot_logo.instantiate();
				Error load_err = ImageLoader::load_image(boot_logo_path, boot_logo);
				if (load_err) {
					String msg = (boot_logo_path.ends_with(".png") ? "" : "The only supported format is PNG.");
					ERR_PRINT("Non-existing or invalid boot splash at '" + boot_logo_path + +"'. " + msg + " Loading default splash.");
				}
			}
		} else {
			// Create a 1×1 transparent image. This will effectively hide the splash image.
			boot_logo.instantiate();
			boot_logo->initialize_data(1, 1, false, Image::FORMAT_RGBA8);
			boot_logo->set_pixel(0, 0, Color(0, 0, 0, 0));
		}

		Color boot_bg_color = GLOBAL_GET("application/boot_splash/bg_color");

#if defined(TOOLS_ENABLED) && !defined(NO_EDITOR_SPLASH)
		boot_bg_color = GLOBAL_DEF_BASIC("application/boot_splash/bg_color", (editor || project_manager) ? boot_splash_editor_bg_color : boot_splash_bg_color);
#endif
		if (boot_logo.is_valid()) {
			RenderingServer::get_singleton()->set_boot_image(boot_logo, boot_bg_color, boot_logo_scale, boot_logo_filter);

		} else {
#ifndef NO_DEFAULT_BOOT_LOGO
			MAIN_PRINT("Main: Create bootsplash");
#if defined(TOOLS_ENABLED) && !defined(NO_EDITOR_SPLASH)
			Ref<Image> splash = (editor || project_manager) ? memnew(Image(boot_splash_editor_png)) : memnew(Image(boot_splash_png));
#else
			Ref<Image> splash = memnew(Image(boot_splash_png));
#endif

			MAIN_PRINT("Main: ClearColor");
			RenderingServer::get_singleton()->set_default_clear_color(boot_bg_color);
			MAIN_PRINT("Main: Image");
			RenderingServer::get_singleton()->set_boot_image(splash, boot_bg_color, false);
#endif
		}

#if defined(TOOLS_ENABLED) && defined(MACOS_ENABLED)
		if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_ICON) && OS::get_singleton()->get_bundle_icon_path().is_empty()) {
			Ref<Image> icon = memnew(Image(app_icon_png));
			DisplayServer::get_singleton()->set_icon(icon);
		}
#endif
	}
	RenderingServer::get_singleton()->set_default_clear_color(
			GLOBAL_GET("rendering/environment/defaults/default_clear_color"));
}

String Main::get_rendering_driver_name() {
	return rendering_driver;
}

// everything the main loop needs to know about frame timings
static MainTimerSync main_timer_sync;

// Return value should be EXIT_SUCCESS if we start successfully
// and should move on to `OS::run`, and EXIT_FAILURE otherwise for
// an early exit with that error code.
int Main::start() {
	OS::get_singleton()->benchmark_begin_measure("Startup", "Main::Start");

	ERR_FAIL_COND_V(!_start_success, false);

	bool has_icon = false;
	String positional_arg;
	String game_path;
	String script;
	String main_loop_type;
	bool check_only = false;

#ifdef TOOLS_ENABLED
	String doc_tool_path;
	bool doc_tool_implicit_cwd = false;
	BitField<DocTools::GenerateFlags> gen_flags;
	String _export_preset;
	Vector<String> patches;
	bool export_debug = false;
	bool export_pack_only = false;
	bool install_android_build_template = false;
	bool export_patch = false;
#ifdef MODULE_GDSCRIPT_ENABLED
	String gdscript_docs_path;
#endif
#ifndef DISABLE_DEPRECATED
	bool converting_project = false;
	bool validating_converting_project = false;
#endif // DISABLE_DEPRECATED
#endif // TOOLS_ENABLED

	main_timer_sync.init(OS::get_singleton()->get_ticks_usec());
	List<String> args = OS::get_singleton()->get_cmdline_args();

	for (List<String>::Element *E = args.front(); E; E = E->next()) {
		// First check parameters that do not have an argument to the right.

		// Doctest Unit Testing Handler
		// Designed to override and pass arguments to the unit test handler.
		if (E->get() == "--check-only") {
			check_only = true;
#ifdef TOOLS_ENABLED
		} else if (E->get() == "--no-docbase") {
			gen_flags.set_flag(DocTools::GENERATE_FLAG_SKIP_BASIC_TYPES);
		} else if (E->get() == "--gdextension-docs") {
			gen_flags.set_flag(DocTools::GENERATE_FLAG_SKIP_BASIC_TYPES);
			gen_flags.set_flag(DocTools::GENERATE_FLAG_EXTENSION_CLASSES_ONLY);
#ifndef DISABLE_DEPRECATED
		} else if (E->get() == "--convert-3to4") {
			converting_project = true;
		} else if (E->get() == "--validate-conversion-3to4") {
			validating_converting_project = true;
#endif // DISABLE_DEPRECATED
		} else if (E->get() == "-e" || E->get() == "--editor") {
			editor = true;
		} else if (E->get() == "-p" || E->get() == "--project-manager") {
			project_manager = true;
		} else if (E->get() == "--recovery-mode") {
			recovery_mode = true;
		} else if (E->get() == "--install-android-build-template") {
			install_android_build_template = true;
#endif // TOOLS_ENABLED
		} else if (E->get().length() && E->get()[0] != '-' && positional_arg.is_empty()) {
			positional_arg = E->get();

			if (E->get().ends_with(".scn") ||
					E->get().ends_with(".tscn") ||
					E->get().ends_with(".escn") ||
					E->get().ends_with(".res") ||
					E->get().ends_with(".tres")) {
				// Only consider the positional argument to be a scene path if it ends with
				// a file extension associated with Godot scenes. This makes it possible
				// for projects to parse command-line arguments for custom CLI arguments
				// or other file extensions without trouble. This can be used to implement
				// "drag-and-drop onto executable" logic, which can prove helpful
				// for non-game applications.
				game_path = E->get();
			}
		}
		// Then parameters that have an argument to the right.
		else if (E->next()) {
			bool parsed_pair = true;
			if (E->get() == "-s" || E->get() == "--script") {
				script = E->next()->get();
			} else if (E->get() == "--main-loop") {
				main_loop_type = E->next()->get();
#ifdef TOOLS_ENABLED
			} else if (E->get() == "--doctool") {
				doc_tool_path = E->next()->get();
				if (doc_tool_path.begins_with("-")) {
					// Assuming other command line arg, so default to cwd.
					doc_tool_path = ".";
					doc_tool_implicit_cwd = true;
					parsed_pair = false;
				}
#ifdef MODULE_GDSCRIPT_ENABLED
			} else if (E->get() == "--gdscript-docs") {
				gdscript_docs_path = E->next()->get();
#endif
			} else if (E->get() == "--export-release") {
				ERR_FAIL_COND_V_MSG(!editor && !found_project, EXIT_FAILURE, "Please provide a valid project path when exporting, aborting.");
				editor = true; //needs editor
				_export_preset = E->next()->get();
			} else if (E->get() == "--export-debug") {
				ERR_FAIL_COND_V_MSG(!editor && !found_project, EXIT_FAILURE, "Please provide a valid project path when exporting, aborting.");
				editor = true; //needs editor
				_export_preset = E->next()->get();
				export_debug = true;
			} else if (E->get() == "--export-pack") {
				ERR_FAIL_COND_V_MSG(!editor && !found_project, EXIT_FAILURE, "Please provide a valid project path when exporting, aborting.");
				editor = true;
				_export_preset = E->next()->get();
				export_pack_only = true;
			} else if (E->get() == "--export-patch") {
				ERR_FAIL_COND_V_MSG(!editor && !found_project, EXIT_FAILURE, "Please provide a valid project path when exporting, aborting.");
				editor = true;
				_export_preset = E->next()->get();
				export_pack_only = true;
				export_patch = true;
			} else if (E->get() == "--patches") {
				patches = E->next()->get().split(",", false);
#endif
			} else {
				// The parameter does not match anything known, don't skip the next argument
				parsed_pair = false;
			}
			if (parsed_pair) {
				E = E->next();
			}
		} else if (E->get().begins_with("--export-")) {
			ERR_FAIL_V_MSG(EXIT_FAILURE, "Missing export preset name, aborting.");
		}
#ifdef TOOLS_ENABLED
		// Handle case where no path is given to --doctool.
		else if (E->get() == "--doctool") {
			doc_tool_path = ".";
			doc_tool_implicit_cwd = true;
		}
#endif
	}

	uint64_t minimum_time_msec = GLOBAL_DEF(PropertyInfo(Variant::INT, "application/boot_splash/minimum_display_time", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:ms"), 0);
	if (Engine::get_singleton()->is_editor_hint()) {
		minimum_time_msec = 0;
	}

#ifdef TOOLS_ENABLED
#ifdef MODULE_GDSCRIPT_ENABLED
	if (!doc_tool_path.is_empty() && gdscript_docs_path.is_empty()) {
#else
	if (!doc_tool_path.is_empty()) {
#endif
		// Needed to instance editor-only classes for their default values
		Engine::get_singleton()->set_editor_hint(true);

		// Translate the class reference only when `-l LOCALE` parameter is given.
		if (!locale.is_empty() && locale != "en") {
			load_doc_translations(locale);
		}

		{
			Ref<DirAccess> da = DirAccess::open(doc_tool_path);
			ERR_FAIL_COND_V_MSG(da.is_null(), EXIT_FAILURE, "Argument supplied to --doctool must be a valid directory path.");
			// Ensure that doctool is running in the root dir, but only if
			// user did not manually specify a path as argument.
			if (doc_tool_implicit_cwd) {
				ERR_FAIL_COND_V_MSG(!da->dir_exists("doc"), EXIT_FAILURE, "--doctool must be run from the Godot repository's root folder, or specify a path that points there.");
			}
		}

#ifndef MODULE_MONO_ENABLED
		// Hack to define .NET-specific project settings even on non-.NET builds,
		// so that we don't lose their descriptions and default values in DocTools.
		// Default values should be synced with mono_gd/gd_mono.cpp.
		GLOBAL_DEF("dotnet/project/assembly_name", "");
		GLOBAL_DEF("dotnet/project/solution_directory", "");
		GLOBAL_DEF(PropertyInfo(Variant::INT, "dotnet/project/assembly_reload_attempts", PROPERTY_HINT_RANGE, "1,16,1,or_greater"), 3);
#endif

		Error err;
		DocTools doc;
		doc.generate(gen_flags);

		DocTools docsrc;
		HashMap<String, String> doc_data_classes;
		HashSet<String> checked_paths;
		print_line("Loading docs...");

		const bool gdextension_docs = gen_flags.has_flag(DocTools::GENERATE_FLAG_EXTENSION_CLASSES_ONLY);

		if (!gdextension_docs) {
			for (int i = 0; i < _doc_data_class_path_count; i++) {
				// Custom modules are always located by absolute path.
				String path = _doc_data_class_paths[i].path;
				if (path.is_relative_path()) {
					path = doc_tool_path.path_join(path);
				}
				String name = _doc_data_class_paths[i].name;
				doc_data_classes[name] = path;
				if (!checked_paths.has(path)) {
					checked_paths.insert(path);

					// Create the module documentation directory if it doesn't exist
					Ref<DirAccess> da = DirAccess::create_for_path(path);
					err = da->make_dir_recursive(path);
					ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error: Can't create directory: " + path + ": " + itos(err));

					print_line("Loading docs from: " + path);
					err = docsrc.load_classes(path);
					ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error loading docs from: " + path + ": " + itos(err));
				}
			}
		}

		// For GDExtension docs, use a path that is compatible with Godot modules.
		String index_path = gdextension_docs ? doc_tool_path.path_join("doc_classes") : doc_tool_path.path_join("doc/classes");
		// Create the main documentation directory if it doesn't exist
		Ref<DirAccess> da = DirAccess::create_for_path(index_path);
		err = da->make_dir_recursive(index_path);
		ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error: Can't create index directory: " + index_path + ": " + itos(err));

		print_line("Loading classes from: " + index_path);
		err = docsrc.load_classes(index_path);
		ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error loading classes from: " + index_path + ": " + itos(err));
		checked_paths.insert(index_path);

		print_line("Merging docs...");
		doc.merge_from(docsrc);

		for (const String &E : checked_paths) {
			print_line("Erasing old docs at: " + E);
			err = DocTools::erase_classes(E);
			ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error erasing old docs at: " + E + ": " + itos(err));
		}

		print_line("Generating new docs...");
		err = doc.save_classes(index_path, doc_data_classes, !gdextension_docs);
		ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error saving new docs:" + itos(err));

		print_line("Deleting docs cache...");
		if (FileAccess::exists(EditorHelp::get_cache_full_path())) {
			DirAccess::remove_file_or_error(EditorHelp::get_cache_full_path());
		}

		return EXIT_SUCCESS;
	}

	// GDExtension API and interface.
	{
		if (dump_gdextension_interface) {
			GDExtensionInterfaceDump::generate_gdextension_interface_file("gdextension_interface.h");
		}

		if (dump_extension_api) {
			Engine::get_singleton()->set_editor_hint(true); // "extension_api.json" should always contains editor singletons.
			GDExtensionAPIDump::generate_extension_json_file("extension_api.json", include_docs_in_extension_api_dump);
		}

		if (dump_gdextension_interface || dump_extension_api) {
			return EXIT_SUCCESS;
		}

		if (validate_extension_api) {
			Engine::get_singleton()->set_editor_hint(true); // "extension_api.json" should always contains editor singletons.
			bool valid = GDExtensionAPIDump::validate_extension_json_file(validate_extension_api_file) == OK;
			return valid ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

#ifndef DISABLE_DEPRECATED
	if (converting_project) {
		int ret = ProjectConverter3To4(converter_max_kb_file, converter_max_line_length).convert();
		return ret ? EXIT_SUCCESS : EXIT_FAILURE;
	}
	if (validating_converting_project) {
		bool ret = ProjectConverter3To4(converter_max_kb_file, converter_max_line_length).validate_conversion();
		return ret ? EXIT_SUCCESS : EXIT_FAILURE;
	}
#endif // DISABLE_DEPRECATED

#endif // TOOLS_ENABLED

	if (script.is_empty() && game_path.is_empty()) {
		game_path = ResourceUID::ensure_path(GLOBAL_GET("application/run/main_scene"));
	}

#ifdef TOOLS_ENABLED
	if (!editor && !project_manager && !cmdline_tool && script.is_empty() && game_path.is_empty()) {
		// If we end up here, it means we didn't manage to detect what we want to run.
		// Let's throw an error gently. The code leading to this is pretty brittle so
		// this might end up triggered by valid usage, in which case we'll have to
		// fine-tune further.
		OS::get_singleton()->alert("Couldn't detect whether to run the editor, the project manager or a specific project. Aborting.");
		ERR_FAIL_V_MSG(EXIT_FAILURE, "Couldn't detect whether to run the editor, the project manager or a specific project. Aborting.");
	}
#endif

	MainLoop *main_loop = nullptr;
	if (editor) {
		main_loop = memnew(SceneTree);
	}
	if (main_loop_type.is_empty()) {
		main_loop_type = GLOBAL_GET("application/run/main_loop_type");
	}

	if (!script.is_empty()) {
		Ref<Script> script_res = ResourceLoader::load(script);
		ERR_FAIL_COND_V_MSG(script_res.is_null(), EXIT_FAILURE, "Can't load script: " + script);

		if (check_only) {
			return script_res->is_valid() ? EXIT_SUCCESS : EXIT_FAILURE;
		}

		if (script_res->can_instantiate()) {
			StringName instance_type = script_res->get_instance_base_type();
			Object *obj = ClassDB::instantiate(instance_type);
			MainLoop *script_loop = Object::cast_to<MainLoop>(obj);
			if (!script_loop) {
				if (obj) {
					memdelete(obj);
				}
				OS::get_singleton()->alert(vformat("Can't load the script \"%s\" as it doesn't inherit from SceneTree or MainLoop.", script));
				ERR_FAIL_V_MSG(EXIT_FAILURE, vformat("Can't load the script \"%s\" as it doesn't inherit from SceneTree or MainLoop.", script));
			}

			script_loop->set_script(script_res);
			main_loop = script_loop;
		} else {
			return EXIT_FAILURE;
		}
	} else { // Not based on script path.
		if (!editor && !ClassDB::class_exists(main_loop_type) && ScriptServer::is_global_class(main_loop_type)) {
			String script_path = ScriptServer::get_global_class_path(main_loop_type);
			Ref<Script> script_res = ResourceLoader::load(script_path);
			if (script_res.is_null()) {
				OS::get_singleton()->alert("Error: Could not load MainLoop script type: " + main_loop_type);
				ERR_FAIL_V_MSG(EXIT_FAILURE, vformat("Could not load global class %s.", main_loop_type));
			}
			StringName script_base = script_res->get_instance_base_type();
			Object *obj = ClassDB::instantiate(script_base);
			MainLoop *script_loop = Object::cast_to<MainLoop>(obj);
			if (!script_loop) {
				if (obj) {
					memdelete(obj);
				}
				OS::get_singleton()->alert("Error: Invalid MainLoop script base type: " + script_base);
				ERR_FAIL_V_MSG(EXIT_FAILURE, vformat("The global class %s does not inherit from SceneTree or MainLoop.", main_loop_type));
			}
			script_loop->set_script(script_res);
			main_loop = script_loop;
		}
	}

	if (!main_loop && main_loop_type.is_empty()) {
		main_loop_type = "SceneTree";
	}

	if (!main_loop) {
		if (!ClassDB::class_exists(main_loop_type)) {
			OS::get_singleton()->alert("Error: MainLoop type doesn't exist: " + main_loop_type);
			return EXIT_FAILURE;
		} else {
			Object *ml = ClassDB::instantiate(main_loop_type);
			ERR_FAIL_NULL_V_MSG(ml, EXIT_FAILURE, "Can't instance MainLoop type.");

			main_loop = Object::cast_to<MainLoop>(ml);
			if (!main_loop) {
				memdelete(ml);
				ERR_FAIL_V_MSG(EXIT_FAILURE, "Invalid MainLoop type.");
			}
		}
	}

	OS::get_singleton()->set_main_loop(main_loop);

	SceneTree *sml = Object::cast_to<SceneTree>(main_loop);
	if (sml) {
#ifdef DEBUG_ENABLED
		if (debug_collisions) {
			sml->set_debug_collisions_hint(true);
		}
		if (debug_paths) {
			sml->set_debug_paths_hint(true);
		}
		if (debug_navigation) {
			sml->set_debug_navigation_hint(true);
			NavigationServer3D::get_singleton()->set_debug_navigation_enabled(true);
		}
		if (debug_avoidance) {
			NavigationServer3D::get_singleton()->set_debug_avoidance_enabled(true);
		}
		if (debug_navigation || debug_avoidance) {
			NavigationServer3D::get_singleton()->set_active(true);
			NavigationServer3D::get_singleton()->set_debug_enabled(true);
		}
		if (debug_canvas_item_redraw) {
			RenderingServer::get_singleton()->canvas_item_set_debug_redraw(true);
		}
#endif

		if (single_threaded_scene) {
			sml->set_disable_node_threading(true);
		}

		bool embed_subwindows = GLOBAL_GET("display/window/subwindows/embed_subwindows");

		if (single_window || (!project_manager && !editor && embed_subwindows) || !DisplayServer::get_singleton()->has_feature(DisplayServer::Feature::FEATURE_SUBWINDOWS)) {
			sml->get_root()->set_embedding_subwindows(true);
		}

		ResourceLoader::add_custom_loaders();
		ResourceSaver::add_custom_savers();

		if (!project_manager && !editor) { // game
			if (!game_path.is_empty() || !script.is_empty()) {
				//autoload
				OS::get_singleton()->benchmark_begin_measure("Startup", "Load Autoloads");
				HashMap<StringName, ProjectSettings::AutoloadInfo> autoloads = ProjectSettings::get_singleton()->get_autoload_list();

				//first pass, add the constants so they exist before any script is loaded
				for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
					const ProjectSettings::AutoloadInfo &info = E.value;

					if (info.is_singleton) {
						for (int i = 0; i < ScriptServer::get_language_count(); i++) {
							ScriptServer::get_language(i)->add_global_constant(info.name, Variant());
						}
					}
				}

				//second pass, load into global constants
				List<Node *> to_add;
				for (const KeyValue<StringName, ProjectSettings::AutoloadInfo> &E : autoloads) {
					const ProjectSettings::AutoloadInfo &info = E.value;

					Node *n = nullptr;
					if (ResourceLoader::get_resource_type(info.path) == "PackedScene") {
						// Cache the scene reference before loading it (for cyclic references)
						Ref<PackedScene> scn;
						scn.instantiate();
						scn->set_path(info.path);
						scn->reload_from_file();
						ERR_CONTINUE_MSG(scn.is_null(), vformat("Failed to instantiate an autoload, can't load from path: %s.", info.path));

						if (scn.is_valid()) {
							n = scn->instantiate();
						}
					} else {
						Ref<Resource> res = ResourceLoader::load(info.path);
						ERR_CONTINUE_MSG(res.is_null(), vformat("Failed to instantiate an autoload, can't load from path: %s.", info.path));

						Ref<Script> script_res = res;
						if (script_res.is_valid()) {
							StringName ibt = script_res->get_instance_base_type();
							bool valid_type = ClassDB::is_parent_class(ibt, "Node");
							ERR_CONTINUE_MSG(!valid_type, vformat("Failed to instantiate an autoload, script '%s' does not inherit from 'Node'.", info.path));

							Object *obj = ClassDB::instantiate(ibt);
							ERR_CONTINUE_MSG(!obj, vformat("Failed to instantiate an autoload, cannot instantiate '%s'.", ibt));

							n = Object::cast_to<Node>(obj);
							n->set_script(script_res);
						}
					}

					ERR_CONTINUE_MSG(!n, vformat("Failed to instantiate an autoload, path is not pointing to a scene or a script: %s.", info.path));
					n->set_name(info.name);

					//defer so references are all valid on _ready()
					to_add.push_back(n);

					if (info.is_singleton) {
						for (int i = 0; i < ScriptServer::get_language_count(); i++) {
							ScriptServer::get_language(i)->add_global_constant(info.name, n);
						}
					}
				}

				for (Node *E : to_add) {
					sml->get_root()->add_child(E);
				}
				OS::get_singleton()->benchmark_end_measure("Startup", "Load Autoloads");
			}
		}

#ifdef TOOLS_ENABLED
#ifdef MODULE_GDSCRIPT_ENABLED
		if (!doc_tool_path.is_empty() && !gdscript_docs_path.is_empty()) {
			DocTools docs;
			Error err;

			Vector<String> paths = get_files_with_extension(gdscript_docs_path, "gd");
			ERR_FAIL_COND_V_MSG(paths.is_empty(), EXIT_FAILURE, "Couldn't find any GDScript files under the given directory: " + gdscript_docs_path);

			for (const String &path : paths) {
				Ref<GDScript> gdscript = ResourceLoader::load(path);
				for (const DocData::ClassDoc &class_doc : gdscript->get_documentation()) {
					docs.add_doc(class_doc);
				}
			}

			if (doc_tool_implicit_cwd) {
				doc_tool_path = "./docs";
			}

			Ref<DirAccess> da = DirAccess::create_for_path(doc_tool_path);
			err = da->make_dir_recursive(doc_tool_path);
			ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error: Can't create GDScript docs directory: " + doc_tool_path + ": " + itos(err));

			HashMap<String, String> doc_data_classes;
			err = docs.save_classes(doc_tool_path, doc_data_classes, false);
			ERR_FAIL_COND_V_MSG(err != OK, EXIT_FAILURE, "Error saving GDScript docs:" + itos(err));

			return EXIT_SUCCESS;
		}
#endif // MODULE_GDSCRIPT_ENABLED

		EditorNode *editor_node = nullptr;
		if (editor) {
			OS::get_singleton()->benchmark_begin_measure("Startup", "Editor");

			if (editor_pseudolocalization) {
				translation_server->get_editor_domain()->set_pseudolocalization_enabled(true);
			}

			editor_node = memnew(EditorNode);
			sml->get_root()->add_child(editor_node);

			if (!_export_preset.is_empty()) {
				editor_node->export_preset(_export_preset, positional_arg, export_debug, export_pack_only, install_android_build_template, export_patch, patches);
				game_path = ""; // Do not load anything.
			}

			OS::get_singleton()->benchmark_end_measure("Startup", "Editor");
		}
#endif
		sml->set_auto_accept_quit(GLOBAL_GET("application/config/auto_accept_quit"));
		sml->set_quit_on_go_back(GLOBAL_GET("application/config/quit_on_go_back"));

		if (!editor && !project_manager) {
			//standard helpers that can be changed from main config

			String stretch_mode = GLOBAL_GET("display/window/stretch/mode");
			String stretch_aspect = GLOBAL_GET("display/window/stretch/aspect");
			Size2i stretch_size = Size2i(GLOBAL_GET("display/window/size/viewport_width"),
					GLOBAL_GET("display/window/size/viewport_height"));
			real_t stretch_scale = GLOBAL_GET("display/window/stretch/scale");
			String stretch_scale_mode = GLOBAL_GET("display/window/stretch/scale_mode");

			Window::ContentScaleMode cs_sm = Window::CONTENT_SCALE_MODE_DISABLED;
			if (stretch_mode == "canvas_items") {
				cs_sm = Window::CONTENT_SCALE_MODE_CANVAS_ITEMS;
			} else if (stretch_mode == "viewport") {
				cs_sm = Window::CONTENT_SCALE_MODE_VIEWPORT;
			}

			Window::ContentScaleAspect cs_aspect = Window::CONTENT_SCALE_ASPECT_IGNORE;
			if (stretch_aspect == "keep") {
				cs_aspect = Window::CONTENT_SCALE_ASPECT_KEEP;
			} else if (stretch_aspect == "keep_width") {
				cs_aspect = Window::CONTENT_SCALE_ASPECT_KEEP_WIDTH;
			} else if (stretch_aspect == "keep_height") {
				cs_aspect = Window::CONTENT_SCALE_ASPECT_KEEP_HEIGHT;
			} else if (stretch_aspect == "expand") {
				cs_aspect = Window::CONTENT_SCALE_ASPECT_EXPAND;
			}

			Window::ContentScaleStretch cs_stretch = Window::CONTENT_SCALE_STRETCH_FRACTIONAL;
			if (stretch_scale_mode == "integer") {
				cs_stretch = Window::CONTENT_SCALE_STRETCH_INTEGER;
			}

			sml->get_root()->set_content_scale_mode(cs_sm);
			sml->get_root()->set_content_scale_aspect(cs_aspect);
			sml->get_root()->set_content_scale_stretch(cs_stretch);
			sml->get_root()->set_content_scale_size(stretch_size);
			sml->get_root()->set_content_scale_factor(stretch_scale);

			sml->set_auto_accept_quit(GLOBAL_GET("application/config/auto_accept_quit"));
			sml->set_quit_on_go_back(GLOBAL_GET("application/config/quit_on_go_back"));
			String appname = GLOBAL_GET("application/config/name");
			appname = TranslationServer::get_singleton()->translate(appname);
#ifdef DEBUG_ENABLED
			// Append a suffix to the window title to denote that the project is running
			// from a debug build (including the editor). Since this results in lower performance,
			// this should be clearly presented to the user.
			DisplayServer::get_singleton()->window_set_title(vformat("%s (DEBUG)", appname));
#else
			DisplayServer::get_singleton()->window_set_title(appname);
#endif

			bool snap_controls = GLOBAL_GET("gui/common/snap_controls_to_pixels");
			sml->get_root()->set_snap_controls_to_pixels(snap_controls);

			bool font_oversampling = GLOBAL_GET("gui/fonts/dynamic_fonts/use_oversampling");
			sml->get_root()->set_use_font_oversampling(font_oversampling);

			int texture_filter = GLOBAL_GET("rendering/textures/canvas_textures/default_texture_filter");
			int texture_repeat = GLOBAL_GET("rendering/textures/canvas_textures/default_texture_repeat");
			sml->get_root()->set_default_canvas_item_texture_filter(
					Viewport::DefaultCanvasItemTextureFilter(texture_filter));
			sml->get_root()->set_default_canvas_item_texture_repeat(
					Viewport::DefaultCanvasItemTextureRepeat(texture_repeat));
		}

#ifdef TOOLS_ENABLED
		if (editor) {
			bool editor_embed_subwindows = EditorSettings::get_singleton()->get_setting(
					"interface/editor/single_window_mode");

			if (editor_embed_subwindows) {
				sml->get_root()->set_embedding_subwindows(true);
			}
			restore_editor_window_layout = EditorSettings::get_singleton()->get_setting("interface/editor/editor_screen").operator int() == EditorSettings::InitialScreen::INITIAL_SCREEN_AUTO;
		}
#endif

		String local_game_path;
		if (!game_path.is_empty() && !project_manager) {
			local_game_path = game_path.replace("\\", "/");

			if (!local_game_path.begins_with("res://")) {
				bool absolute =
						(local_game_path.size() > 1) && (local_game_path[0] == '/' || local_game_path[1] == ':');

				if (!absolute) {
					if (ProjectSettings::get_singleton()->is_using_datapack()) {
						local_game_path = "res://" + local_game_path;

					} else {
						int sep = local_game_path.rfind_char('/');

						if (sep == -1) {
							Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
							ERR_FAIL_COND_V(da.is_null(), EXIT_FAILURE);

							local_game_path = da->get_current_dir().path_join(local_game_path);
						} else {
							Ref<DirAccess> da = DirAccess::open(local_game_path.substr(0, sep));
							if (da.is_valid()) {
								local_game_path = da->get_current_dir().path_join(
										local_game_path.substr(sep + 1, local_game_path.length()));
							}
						}
					}
				}
			}

			local_game_path = ProjectSettings::get_singleton()->localize_path(local_game_path);

#ifdef TOOLS_ENABLED
			if (editor) {
				if (!recovery_mode && (game_path != String(GLOBAL_GET("application/run/main_scene")) || !editor_node->has_scenes_in_session())) {
					Error serr = editor_node->load_scene(local_game_path);
					if (serr != OK) {
						ERR_PRINT("Failed to load scene");
					}
				}
				if (!debug_server_uri.is_empty()) {
					EditorDebuggerNode::get_singleton()->start(debug_server_uri);
					EditorDebuggerNode::get_singleton()->set_keep_open(true);
				}
			}
#endif
		}

		if (!project_manager && !editor) { // game

			OS::get_singleton()->benchmark_begin_measure("Startup", "Load Game");

			// Load SSL Certificates from Project Settings (or builtin).
			Crypto::load_default_certificates(GLOBAL_GET("network/tls/certificate_bundle_override"));

			if (!game_path.is_empty()) {
				Node *scene = nullptr;
				Ref<PackedScene> scenedata = ResourceLoader::load(local_game_path);
				if (scenedata.is_valid()) {
					scene = scenedata->instantiate();
				}

				ERR_FAIL_NULL_V_MSG(scene, EXIT_FAILURE, "Failed loading scene: " + local_game_path + ".");
				sml->add_current_scene(scene);

#ifdef MACOS_ENABLED
				String mac_icon_path = GLOBAL_GET("application/config/macos_native_icon");
				if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_ICON) && !mac_icon_path.is_empty()) {
					DisplayServer::get_singleton()->set_native_icon(mac_icon_path);
					has_icon = true;
				}
#endif

#ifdef WINDOWS_ENABLED
				String win_icon_path = GLOBAL_GET("application/config/windows_native_icon");
				if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_NATIVE_ICON) && !win_icon_path.is_empty()) {
					DisplayServer::get_singleton()->set_native_icon(win_icon_path);
					has_icon = true;
				}
#endif

				String icon_path = GLOBAL_GET("application/config/icon");
				if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_ICON) && !icon_path.is_empty() && !has_icon) {
					Ref<Image> icon;
					icon.instantiate();
					if (ImageLoader::load_image(icon_path, icon) == OK) {
						DisplayServer::get_singleton()->set_icon(icon);
						has_icon = true;
					}
				}
			}

			OS::get_singleton()->benchmark_end_measure("Startup", "Load Game");
		}

#ifdef TOOLS_ENABLED
		if (project_manager) {
			OS::get_singleton()->benchmark_begin_measure("Startup", "Project Manager");
			Engine::get_singleton()->set_editor_hint(true);

			if (editor_pseudolocalization) {
				translation_server->get_editor_domain()->set_pseudolocalization_enabled(true);
			}

			ProjectManager *pmanager = memnew(ProjectManager(force_res || use_custom_res));
			ProgressDialog *progress_dialog = memnew(ProgressDialog);
			pmanager->add_child(progress_dialog);

			sml->get_root()->add_child(pmanager);
			OS::get_singleton()->benchmark_end_measure("Startup", "Project Manager");
		}

		if (project_manager || editor) {
			// Load SSL Certificates from Editor Settings (or builtin)
			Crypto::load_default_certificates(
					EditorSettings::get_singleton()->get_setting("network/tls/editor_tls_certificates").operator String());
		}

		if (recovery_mode) {
			Engine::get_singleton()->set_recovery_mode_hint(true);
		}
#endif
	}

	if (DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_ICON) && !has_icon && OS::get_singleton()->get_bundle_icon_path().is_empty()) {
		Ref<Image> icon = memnew(Image(app_icon_png));
		DisplayServer::get_singleton()->set_icon(icon);
	}

	if (movie_writer) {
		movie_writer->begin(DisplayServer::get_singleton()->window_get_size(), fixed_fps, Engine::get_singleton()->get_write_movie_path());
	}

	if (minimum_time_msec) {
		uint64_t minimum_time = 1000 * minimum_time_msec;
		uint64_t elapsed_time = OS::get_singleton()->get_ticks_usec();
		if (elapsed_time < minimum_time) {
			OS::get_singleton()->delay_usec(minimum_time - elapsed_time);
		}
	}

	OS::get_singleton()->benchmark_end_measure("Startup", "Main::Start");
	OS::get_singleton()->benchmark_dump();

	return EXIT_SUCCESS;
}

/* Main iteration
 *
 * This is the iteration of the engine's game loop, advancing the state of physics,
 * rendering and audio.
 * It's called directly by the platform's OS::run method, where the loop is created
 * and monitored.
 *
 * The OS implementation can impact its draw step with the Main::force_redraw() method.
 */

uint64_t Main::last_ticks = 0;
uint32_t Main::frames = 0;
uint32_t Main::hide_print_fps_attempts = 3;
uint32_t Main::frame = 0;
bool Main::force_redraw_requested = false;
int Main::iterating = 0;

bool Main::is_iterating() {
	return iterating > 0;
}

// For performance metrics.
static uint64_t physics_process_max = 0;
static uint64_t process_max = 0;
static uint64_t navigation_process_max = 0;

// Return false means iterating further, returning true means `OS::run`
// will terminate the program. In case of failure, the OS exit code needs
// to be set explicitly here (defaults to EXIT_SUCCESS).
bool Main::iteration() {
	iterating++;

	const uint64_t ticks = OS::get_singleton()->get_ticks_usec();
	Engine::get_singleton()->_frame_ticks = ticks;
	main_timer_sync.set_cpu_ticks_usec(ticks);
	main_timer_sync.set_fixed_fps(fixed_fps);

	const uint64_t ticks_elapsed = ticks - last_ticks;

	const int physics_ticks_per_second = Engine::get_singleton()->get_physics_ticks_per_second();
	const double physics_step = 1.0 / physics_ticks_per_second;

	const double time_scale = Engine::get_singleton()->get_time_scale();

	MainFrameTime advance = main_timer_sync.advance(physics_step, physics_ticks_per_second);
	double process_step = advance.process_step;
	double scaled_step = process_step * time_scale;

	Engine::get_singleton()->_process_step = process_step;
	Engine::get_singleton()->_physics_interpolation_fraction = advance.interpolation_fraction;

	uint64_t physics_process_ticks = 0;
	uint64_t process_ticks = 0;
	uint64_t navigation_process_ticks = 0;

	frame += ticks_elapsed;

	last_ticks = ticks;

	const int max_physics_steps = Engine::get_singleton()->get_max_physics_steps_per_frame();
	if (fixed_fps == -1 && advance.physics_steps > max_physics_steps) {
		process_step -= (advance.physics_steps - max_physics_steps) * physics_step;
		advance.physics_steps = max_physics_steps;
	}

	bool exit = false;

	// process all our active interfaces
#ifndef _3D_DISABLED
	XRServer::get_singleton()->_process();
#endif // _3D_DISABLED

	NavigationServer2D::get_singleton()->sync();
	NavigationServer3D::get_singleton()->sync();

	for (int iters = 0; iters < advance.physics_steps; ++iters) {
		if (Input::get_singleton()->is_agile_input_event_flushing()) {
			Input::get_singleton()->flush_buffered_events();
		}

		Engine::get_singleton()->_in_physics = true;
		Engine::get_singleton()->_physics_frames++;

		uint64_t physics_begin = OS::get_singleton()->get_ticks_usec();

		// Prepare the fixed timestep interpolated nodes BEFORE they are updated
		// by the physics server, otherwise the current and previous transforms
		// may be the same, and no interpolation takes place.
		OS::get_singleton()->get_main_loop()->iteration_prepare();

#ifndef _3D_DISABLED
		PhysicsServer3D::get_singleton()->sync();
		PhysicsServer3D::get_singleton()->flush_queries();
#endif // _3D_DISABLED

		PhysicsServer2D::get_singleton()->sync();
		PhysicsServer2D::get_singleton()->flush_queries();

		if (OS::get_singleton()->get_main_loop()->physics_process(physics_step * time_scale)) {
#ifndef _3D_DISABLED
			PhysicsServer3D::get_singleton()->end_sync();
#endif // _3D_DISABLED
			PhysicsServer2D::get_singleton()->end_sync();

			Engine::get_singleton()->_in_physics = false;
			exit = true;
			break;
		}

		uint64_t navigation_begin = OS::get_singleton()->get_ticks_usec();

		NavigationServer3D::get_singleton()->process(physics_step * time_scale);

		navigation_process_ticks = MAX(navigation_process_ticks, OS::get_singleton()->get_ticks_usec() - navigation_begin); // keep the largest one for reference
		navigation_process_max = MAX(OS::get_singleton()->get_ticks_usec() - navigation_begin, navigation_process_max);

		message_queue->flush();

#ifndef _3D_DISABLED
		PhysicsServer3D::get_singleton()->end_sync();
		PhysicsServer3D::get_singleton()->step(physics_step * time_scale);
#endif // _3D_DISABLED

		PhysicsServer2D::get_singleton()->end_sync();
		PhysicsServer2D::get_singleton()->step(physics_step * time_scale);

		message_queue->flush();

		OS::get_singleton()->get_main_loop()->iteration_end();

		physics_process_ticks = MAX(physics_process_ticks, OS::get_singleton()->get_ticks_usec() - physics_begin); // keep the largest one for reference
		physics_process_max = MAX(OS::get_singleton()->get_ticks_usec() - physics_begin, physics_process_max);

		Engine::get_singleton()->_in_physics = false;
	}

	if (Input::get_singleton()->is_agile_input_event_flushing()) {
		Input::get_singleton()->flush_buffered_events();
	}

	uint64_t process_begin = OS::get_singleton()->get_ticks_usec();

	if (OS::get_singleton()->get_main_loop()->process(process_step * time_scale)) {
		exit = true;
	}
	message_queue->flush();

	RenderingServer::get_singleton()->sync(); //sync if still drawing from previous frames.

	const bool has_pending_resources_for_processing = RD::get_singleton() && RD::get_singleton()->has_pending_resources_for_processing();
	bool wants_present = (DisplayServer::get_singleton()->can_any_window_draw() ||
								 DisplayServer::get_singleton()->has_additional_outputs()) &&
			RenderingServer::get_singleton()->is_render_loop_enabled();

	if (wants_present || has_pending_resources_for_processing) {
		wants_present |= force_redraw_requested;
		if ((!force_redraw_requested) && OS::get_singleton()->is_in_low_processor_usage_mode()) {
			if (RenderingServer::get_singleton()->has_changed()) {
				RenderingServer::get_singleton()->draw(wants_present, scaled_step); // flush visual commands
				Engine::get_singleton()->increment_frames_drawn();
			}
		} else {
			RenderingServer::get_singleton()->draw(wants_present, scaled_step); // flush visual commands
			Engine::get_singleton()->increment_frames_drawn();
			force_redraw_requested = false;
		}
	}

	process_ticks = OS::get_singleton()->get_ticks_usec() - process_begin;
	process_max = MAX(process_ticks, process_max);
	uint64_t frame_time = OS::get_singleton()->get_ticks_usec() - ticks;

	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptServer::get_language(i)->frame();
	}

	AudioServer::get_singleton()->update();

	if (EngineDebugger::is_active()) {
		EngineDebugger::get_singleton()->iteration(frame_time, process_ticks, physics_process_ticks, physics_step);
	}

	frames++;
	Engine::get_singleton()->_process_frames++;

	if (frame > 1000000) {
		// Wait a few seconds before printing FPS, as FPS reporting just after the engine has started is inaccurate.
		if (hide_print_fps_attempts == 0) {
			if (editor || project_manager) {
				if (print_fps) {
					print_line(vformat("Editor FPS: %d (%s mspf)", frames, rtos(1000.0 / frames).pad_decimals(2)));
				}
			} else if (print_fps || GLOBAL_GET("debug/settings/stdout/print_fps")) {
				print_line(vformat("Project FPS: %d (%s mspf)", frames, rtos(1000.0 / frames).pad_decimals(2)));
			}
		} else {
			hide_print_fps_attempts--;
		}

		Engine::get_singleton()->_fps = frames;
		performance->set_process_time(USEC_TO_SEC(process_max));
		performance->set_physics_process_time(USEC_TO_SEC(physics_process_max));
		performance->set_navigation_process_time(USEC_TO_SEC(navigation_process_max));
		process_max = 0;
		physics_process_max = 0;
		navigation_process_max = 0;

		frame %= 1000000;
		frames = 0;
	}

	iterating--;

	if (movie_writer) {
		movie_writer->add_frame();
	}

#ifdef TOOLS_ENABLED
	bool quit_after_timeout = false;
#endif
	if ((quit_after > 0) && (Engine::get_singleton()->_process_frames >= quit_after)) {
#ifdef TOOLS_ENABLED
		quit_after_timeout = true;
#endif
		exit = true;
	}

#ifdef TOOLS_ENABLED
	if (wait_for_import && EditorFileSystem::get_singleton() && EditorFileSystem::get_singleton()->doing_first_scan()) {
		exit = false;
	}
#endif

	if (fixed_fps != -1) {
		return exit;
	}

	OS::get_singleton()->add_frame_delay(DisplayServer::get_singleton()->window_can_draw());

#ifdef TOOLS_ENABLED
	if (auto_build_solutions) {
		auto_build_solutions = false;
		// Only relevant when running the editor.
		if (!editor) {
			OS::get_singleton()->set_exit_code(EXIT_FAILURE);
			ERR_FAIL_V_MSG(true,
					"Command line option --build-solutions was passed, but no project is being edited. Aborting.");
		}
		if (!EditorNode::get_singleton()->call_build()) {
			OS::get_singleton()->set_exit_code(EXIT_FAILURE);
			ERR_FAIL_V_MSG(true,
					"Command line option --build-solutions was passed, but the build callback failed. Aborting.");
		}
	}
#endif

#ifdef TOOLS_ENABLED
	if (exit && quit_after_timeout && EditorNode::get_singleton()) {
		EditorNode::get_singleton()->unload_editor_addons();
	}
#endif

	return exit;
}

void Main::force_redraw() {
	force_redraw_requested = true;
}

/* Engine deinitialization
 *
 * Responsible for freeing all the memory allocated by previous setup steps,
 * so that the engine closes cleanly without leaking memory or crashing.
 * The order matters as some of those steps are linked with each other.
 */
void Main::cleanup(bool p_force) {
	OS::get_singleton()->benchmark_begin_measure("Shutdown", "Main::Cleanup");
	if (!p_force) {
		ERR_FAIL_COND(!_start_success);
	}

#ifdef DEBUG_ENABLED
	if (input) {
		input->flush_frame_parsed_events();
	}
#endif

	for (int i = 0; i < TextServerManager::get_singleton()->get_interface_count(); i++) {
		TextServerManager::get_singleton()->get_interface(i)->cleanup();
	}

	if (movie_writer) {
		movie_writer->end();
	}

	ResourceLoader::clear_thread_load_tasks();

	ResourceLoader::remove_custom_loaders();
	ResourceSaver::remove_custom_savers();
	PropertyListHelper::clear_base_helpers();

	// Flush before uninitializing the scene, but delete the MessageQueue as late as possible.
	message_queue->flush();

	OS::get_singleton()->delete_main_loop();

	OS::get_singleton()->_cmdline.clear();
	OS::get_singleton()->_user_args.clear();
	OS::get_singleton()->_execpath = "";
	OS::get_singleton()->_local_clipboard = "";

	ResourceLoader::clear_translation_remaps();
	ResourceLoader::clear_path_remaps();

	WorkerThreadPool::get_singleton()->exit_languages_threads();

	ScriptServer::finish_languages();

	// Sync pending commands that may have been queued from a different thread during ScriptServer finalization
	RenderingServer::get_singleton()->sync();

	//clear global shader variables before scene and other graphics stuff are deinitialized.
	rendering_server->global_shader_parameters_clear();

#ifndef _3D_DISABLED
	if (xr_server) {
		// Now that we're unregistering properly in plugins we need to keep access to xr_server for a little longer
		// We do however unset our primary interface
		xr_server->set_primary_interface(Ref<XRInterface>());
	}
#endif // _3D_DISABLED

#ifdef TOOLS_ENABLED
	GDExtensionManager::get_singleton()->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_EDITOR);
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_EDITOR);
	unregister_editor_types();

#endif

	ImageLoader::cleanup();

	GDExtensionManager::get_singleton()->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_SCENE);
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SCENE);

	unregister_platform_apis();
	unregister_driver_types();
	unregister_scene_types();

	finalize_theme_db();

	// Before deinitializing server extensions, finalize servers which may be loaded as extensions.
	NavigationServer2DManager::finalize_server(); // 2D goes first as it uses the 3D server behind the scene.
	NavigationServer3DManager::finalize_server();
	finalize_physics();

	GDExtensionManager::get_singleton()->deinitialize_extensions(GDExtension::INITIALIZATION_LEVEL_SERVERS);
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_SERVERS);
	unregister_server_types();

	EngineDebugger::deinitialize();

#ifndef _3D_DISABLED
	if (xr_server) {
		memdelete(xr_server);
	}
#endif // _3D_DISABLED

	if (audio_server) {
		audio_server->finish();
		memdelete(audio_server);
	}

	if (camera_server) {
		memdelete(camera_server);
	}

	OS::get_singleton()->finalize();

	finalize_display();

	if (input) {
		memdelete(input);
	}

	if (packed_data) {
		memdelete(packed_data);
	}
	if (performance) {
		memdelete(performance);
	}
	if (input_map) {
		memdelete(input_map);
	}
	if (translation_server) {
		memdelete(translation_server);
	}
	if (tsman) {
		memdelete(tsman);
	}
#ifndef _3D_DISABLED
	if (physics_server_3d_manager) {
		memdelete(physics_server_3d_manager);
	}
#endif // _3D_DISABLED
	if (physics_server_2d_manager) {
		memdelete(physics_server_2d_manager);
	}
	if (globals) {
		memdelete(globals);
	}

	if (OS::get_singleton()->is_restart_on_exit_set()) {
		//attempt to restart with arguments
		List<String> args = OS::get_singleton()->get_restart_on_exit_arguments();
		OS::get_singleton()->create_instance(args);
		OS::get_singleton()->set_restart_on_exit(false, List<String>()); //clear list (uses memory)
	}

	// Now should be safe to delete MessageQueue (famous last words).
	message_queue->flush();
	memdelete(message_queue);

#if defined(STEAMAPI_ENABLED)
	if (steam_tracker) {
		memdelete(steam_tracker);
	}
#endif

	unregister_core_driver_types();
	unregister_core_extensions();
	uninitialize_modules(MODULE_INITIALIZATION_LEVEL_CORE);

	if (engine) {
		memdelete(engine);
	}

	unregister_core_types();

	OS::get_singleton()->benchmark_end_measure("Shutdown", "Main::Cleanup");
	OS::get_singleton()->benchmark_dump();

	OS::get_singleton()->finalize_core();
}
