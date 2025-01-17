/**************************************************************************/
/*  os_linuxbsd.h                                                         */
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

#ifndef OS_LINUXBSD_H
#define OS_LINUXBSD_H

#include "crash_handler_linuxbsd.h"
#include "joypad_linux.h"

#include "core/input/input.h"
#include "drivers/alsa/audio_driver_alsa.h"
#include "drivers/alsamidi/midi_driver_alsamidi.h"
#include "drivers/pulseaudio/audio_driver_pulseaudio.h"
#include "drivers/unix/os_unix.h"
#include "servers/audio_server.h"

#ifdef FONTCONFIG_ENABLED
#ifdef SOWRAP_ENABLED
#include "fontconfig-so_wrap.h"
#else
#include <fontconfig/fontconfig.h>
#endif
#endif
class JoypadSDL;
class OS_LinuxBSD : public OS_Unix {
	virtual void delete_main_loop() override;

#ifdef FONTCONFIG_ENABLED
	bool font_config_initialized = false;
	FcConfig *config = nullptr;
	FcObjectSet *object_set = nullptr;

	int _weight_to_fc(int p_weight) const;
	int _stretch_to_fc(int p_stretch) const;
#endif

#ifdef JOYDEV_ENABLED
	JoypadLinux *joypad = nullptr;
#endif

#ifdef SDL_ENABLED
	JoypadSDL *joypad_sdl = nullptr;
#endif

#ifdef ALSA_ENABLED
	AudioDriverALSA driver_alsa;
#endif

#ifdef ALSAMIDI_ENABLED
	MIDIDriverALSAMidi driver_alsamidi;
#endif

#ifdef PULSEAUDIO_ENABLED
	AudioDriverPulseAudio driver_pulseaudio;
#endif

	CrashHandler crash_handler;

	MainLoop *main_loop = nullptr;

	String get_systemd_os_release_info_value(const String &key) const;

	Vector<String> lspci_device_filter(Vector<String> vendor_device_id_mapping, String class_suffix, String check_column, String whitelist) const;
	Vector<String> lspci_get_device_value(Vector<String> vendor_device_id_mapping, String check_column, String blacklist) const;

	String system_dir_desktop_cache;

protected:
	virtual void initialize() override;
	virtual void finalize() override;

	virtual void initialize_joypads() override;

	virtual void set_main_loop(MainLoop *p_main_loop) override;

public:
	virtual String get_identifier() const override;
	virtual String get_name() const override;
	virtual String get_distribution_name() const override;
	virtual String get_version() const override;

	virtual Vector<String> get_video_adapter_driver_info() const override;

	virtual MainLoop *get_main_loop() const override;

	virtual uint64_t get_embedded_pck_offset() const override;

	virtual Vector<String> get_system_fonts() const override;
	virtual String get_system_font_path(const String &p_font_name, int p_weight = 400, int p_stretch = 100, bool p_italic = false) const override;
	virtual Vector<String> get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale = String(), const String &p_script = String(), int p_weight = 400, int p_stretch = 100, bool p_italic = false) const override;

	virtual String get_config_path() const override;
	virtual String get_data_path() const override;
	virtual String get_cache_path() const override;

	virtual String get_system_dir(SystemDir p_dir, bool p_shared_storage = true) const override;

	virtual Error shell_open(const String &p_uri) override;

	virtual String get_unique_id() const override;
	virtual String get_processor_name() const override;

	virtual bool is_sandboxed() const override;

	virtual void alert(const String &p_alert, const String &p_title = "ALERT!") override;

	virtual bool _check_internal_feature_support(const String &p_feature) override;

	void run();

	virtual void disable_crash_handler() override;
	virtual bool is_disable_crash_handler() const override;

	virtual Error move_to_trash(const String &p_path) override;

	virtual String get_system_ca_certificates() override;

	OS_LinuxBSD();
	~OS_LinuxBSD();
};

#endif // OS_LINUXBSD_H
