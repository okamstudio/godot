/**************************************************************************/
/*  joypad_windows.cpp                                                    */
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

#include "joypad_windows.h"

#include <oleauto.h>
#include <wbemidl.h>

#if defined(__GNUC__)
// Workaround GCC warning from -Wcast-function-type.
#define GetProcAddress (void *)GetProcAddress
#endif

DWORD WINAPI _xinput_get_state(DWORD dwUserIndex, XINPUT_STATE *pState) {
	return ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI _xinput_set_state(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration) {
	return ERROR_DEVICE_NOT_CONNECTED;
}

MMRESULT WINAPI _winmm_get_joycaps(UINT uJoyID, LPJOYCAPSW pjc, UINT cbjc) {
	return MMSYSERR_NODRIVER;
}

JoypadWindows::JoypadWindows() {
}

JoypadWindows::JoypadWindows(HWND *hwnd) {
	input = Input::get_singleton();
	hWnd = hwnd;
	x_joypad_probe_count = 0;
	d_joypad_count = 0;
	dinput = nullptr;
	xinput_dll = nullptr;
	xinput_get_state = nullptr;
	xinput_set_state = nullptr;
	winmm_get_joycaps = nullptr;

	load_xinput();

	for (int i = 0; i < JOYPADS_MAX; i++) {
		attached_joypads[i] = false;
	}

	HRESULT result = DirectInput8Create(GetModuleHandle(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8, (void **)&dinput, nullptr);
	if (result == DI_OK) {
		probe_joypads();
	} else {
		ERR_PRINT("Couldn't initialize DirectInput. Error: " + itos(result));
		if (result == DIERR_OUTOFMEMORY) {
			ERR_PRINT("The Windows DirectInput subsystem could not allocate sufficient memory.");
			ERR_PRINT("Rebooting your PC may solve this issue.");
		}
		// Ensure dinput is still a nullptr.
		dinput = nullptr;
	}
}

JoypadWindows::~JoypadWindows() {
	close_d_joypad();
	if (dinput) {
		dinput->Release();
	}
	unload_winmm();
	unload_xinput();
}

bool JoypadWindows::is_d_joypad_known(const GUID &p_guid) {
	for (int i = 0; i < JOYPADS_MAX; i++) {
		if (d_joypads[i].guid == p_guid) {
			d_joypads[i].confirmed = true;
			return true;
		}
	}
	return false;
}

// adapted from SDL2, works a lot better than the MSDN version
bool JoypadWindows::is_xinput_joypad(const GUID *p_guid) {
	static GUID IID_ValveStreamingGamepad = { MAKELONG(0x28DE, 0x11FF), 0x28DE, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_X360WiredGamepad = { MAKELONG(0x045E, 0x02A1), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_X360WirelessGamepad = { MAKELONG(0x045E, 0x028E), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XSWirelessGamepad = { MAKELONG(0x045E, 0x0B13), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XEliteWirelessGamepad = { MAKELONG(0x045E, 0x0B05), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XOneWiredGamepad = { MAKELONG(0x045E, 0x02FF), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XOneWirelessGamepad = { MAKELONG(0x045E, 0x02DD), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XOneNewWirelessGamepad = { MAKELONG(0x045E, 0x02D1), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XOneSWirelessGamepad = { MAKELONG(0x045E, 0x02EA), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XOneSBluetoothGamepad = { MAKELONG(0x045E, 0x02E0), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XOneEliteWirelessGamepad = { MAKELONG(0x045E, 0x02E3), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };
	static GUID IID_XOneElite2WirelessGamepad = { MAKELONG(0x045E, 0x0B22), 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 } };

	if (memcmp(p_guid, &IID_ValveStreamingGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_X360WiredGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_X360WirelessGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XSWirelessGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XEliteWirelessGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XOneWiredGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XOneWirelessGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XOneNewWirelessGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XOneSWirelessGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XOneSBluetoothGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XOneEliteWirelessGamepad, sizeof(*p_guid)) == 0 ||
			memcmp(p_guid, &IID_XOneElite2WirelessGamepad, sizeof(*p_guid)) == 0) {
		return true;
	}

	PRAWINPUTDEVICELIST dev_list = nullptr;
	unsigned int dev_list_count = 0;

	if (GetRawInputDeviceList(nullptr, &dev_list_count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
		return false;
	}
	dev_list = (PRAWINPUTDEVICELIST)memalloc(sizeof(RAWINPUTDEVICELIST) * dev_list_count);
	ERR_FAIL_NULL_V_MSG(dev_list, false, "Out of memory.");

	if (GetRawInputDeviceList(dev_list, &dev_list_count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) {
		memfree(dev_list);
		return false;
	}
	for (unsigned int i = 0; i < dev_list_count; i++) {
		RID_DEVICE_INFO rdi;
		char dev_name[128];
		UINT rdiSize = sizeof(rdi);
		UINT nameSize = sizeof(dev_name);

		rdi.cbSize = rdiSize;
		if ((dev_list[i].dwType == RIM_TYPEHID) &&
				(GetRawInputDeviceInfoA(dev_list[i].hDevice, RIDI_DEVICEINFO, &rdi, &rdiSize) != (UINT)-1) &&
				(MAKELONG(rdi.hid.dwVendorId, rdi.hid.dwProductId) == (LONG)p_guid->Data1) &&
				(GetRawInputDeviceInfoA(dev_list[i].hDevice, RIDI_DEVICENAME, &dev_name, &nameSize) != (UINT)-1) &&
				(strstr(dev_name, "IG_") != nullptr)) {
			memfree(dev_list);
			return true;
		}
	}
	memfree(dev_list);
	return false;
}

void JoypadWindows::probe_xinput_joypad(const String &name) {
	if (x_joypad_probe_count >= XUSER_MAX_COUNT) {
		return;
	}
	int i = x_joypad_probe_count;
	x_joypad_probe_count++;

	ZeroMemory(&x_joypads[i].state, sizeof(XINPUT_STATE));

	DWORD dwResult = xinput_get_state(i, &x_joypads[i].state);
	if (dwResult == ERROR_SUCCESS) {
		int id = input->get_unused_joy_id();
		if (id != -1 && !x_joypads[i].attached) {
			x_joypads[i].attached = true;
			x_joypads[i].id = id;
			x_joypads[i].ff_timestamp = 0;
			x_joypads[i].ff_end_timestamp = 0;
			x_joypads[i].vibrating = false;
			attached_joypads[id] = true;
			Dictionary joypad_info;
			String joypad_name;

			joypad_info["xinput_index"] = (int)i;

			JOYCAPSW jc;
			memset(&jc, 0, sizeof(JOYCAPSW));
			MMRESULT jcResult = winmm_get_joycaps((UINT)id, &jc, sizeof(JOYCAPSW));
			if (jcResult == JOYERR_NOERROR) {
				joypad_info["vendor_id"] = itos(jc.wMid);
				joypad_info["product_id"] = itos(jc.wPid);
				if (!name.is_empty()) {
					joypad_name = name.trim_prefix("Controller (").trim_suffix(")");
				}
			}

			input->joy_connection_changed(id, true, joypad_name, "__XINPUT_DEVICE__", joypad_info);
		}
	} else if (x_joypads[i].attached) {
		x_joypads[i].attached = false;
		attached_joypads[x_joypads[i].id] = false;
		input->joy_connection_changed(x_joypads[i].id, false, "");
	}
}

bool JoypadWindows::setup_dinput_joypad(const DIDEVICEINSTANCE *instance) {
	ERR_FAIL_NULL_V_MSG(dinput, false, "DirectInput not initialized. Rebooting your PC may solve this issue.");
	HRESULT hr;
	int num = input->get_unused_joy_id();

	if (is_d_joypad_known(instance->guidInstance) || num == -1) {
		return false;
	}

	d_joypads[num] = dinput_gamepad();
	dinput_gamepad *joy = &d_joypads[num];

	const DWORD devtype = (instance->dwDevType & 0xFF);

	if ((devtype != DI8DEVTYPE_JOYSTICK) && (devtype != DI8DEVTYPE_GAMEPAD) && (devtype != DI8DEVTYPE_1STPERSON) && (devtype != DI8DEVTYPE_DRIVING)) {
		return false;
	}

	hr = dinput->CreateDevice(instance->guidInstance, &joy->di_joy, nullptr);

	if (FAILED(hr)) {
		return false;
	}

	const GUID &guid = instance->guidProduct;
	char uid[128];

	ERR_FAIL_COND_V_MSG(memcmp(&guid.Data4[2], "PIDVID", 6), false, "DirectInput device not recognized.");
	WORD type = BSWAP16(0x03);
	WORD vendor = BSWAP16(LOWORD(guid.Data1));
	WORD product = BSWAP16(HIWORD(guid.Data1));
	WORD version = 0;
	sprintf_s(uid, "%04x%04x%04x%04x%04x%04x%04x%04x", type, 0, vendor, 0, product, 0, version, 0);

	Dictionary joypad_info;
	joypad_info["vendor_id"] = itos(vendor);
	joypad_info["product_id"] = itos(product);

	id_to_change = num;
	slider_count = 0;

	joy->di_joy->SetDataFormat(&c_dfDIJoystick2);
	joy->di_joy->SetCooperativeLevel(*hWnd, DISCL_FOREGROUND);
	joy->di_joy->EnumObjects(objectsCallback, this, 0);
	joy->joy_axis.sort();

	joy->guid = instance->guidInstance;
	const String &name = String(instance->tszProductName).trim_prefix("Controller (").trim_suffix(")");
	input->joy_connection_changed(num, true, name, uid, joypad_info);
	joy->attached = true;
	joy->id = num;
	attached_joypads[num] = true;
	joy->confirmed = true;
	d_joypad_count++;
	return true;
}

void JoypadWindows::setup_d_joypad_object(const DIDEVICEOBJECTINSTANCE *ob, int p_joy_id) {
	if (ob->dwType & DIDFT_AXIS) {
		HRESULT res;
		DIPROPRANGE prop_range;
		DIPROPDWORD dilong;
		LONG ofs;
		if (ob->guidType == GUID_XAxis) {
			ofs = DIJOFS_X;
		} else if (ob->guidType == GUID_YAxis) {
			ofs = DIJOFS_Y;
		} else if (ob->guidType == GUID_ZAxis) {
			ofs = DIJOFS_Z;
		} else if (ob->guidType == GUID_RxAxis) {
			ofs = DIJOFS_RX;
		} else if (ob->guidType == GUID_RyAxis) {
			ofs = DIJOFS_RY;
		} else if (ob->guidType == GUID_RzAxis) {
			ofs = DIJOFS_RZ;
		} else if (ob->guidType == GUID_Slider) {
			if (slider_count < 2) {
				ofs = DIJOFS_SLIDER(slider_count);
				slider_count++;
			} else {
				return;
			}
		} else {
			return;
		}
		prop_range.diph.dwSize = sizeof(DIPROPRANGE);
		prop_range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		prop_range.diph.dwObj = ob->dwType;
		prop_range.diph.dwHow = DIPH_BYID;
		prop_range.lMin = -MAX_JOY_AXIS;
		prop_range.lMax = +MAX_JOY_AXIS;

		dinput_gamepad &joy = d_joypads[p_joy_id];

		res = IDirectInputDevice8_SetProperty(joy.di_joy, DIPROP_RANGE, &prop_range.diph);
		if (FAILED(res)) {
			return;
		}

		dilong.diph.dwSize = sizeof(dilong);
		dilong.diph.dwHeaderSize = sizeof(dilong.diph);
		dilong.diph.dwObj = ob->dwType;
		dilong.diph.dwHow = DIPH_BYID;
		dilong.dwData = 0;

		res = IDirectInputDevice8_SetProperty(joy.di_joy, DIPROP_DEADZONE, &dilong.diph);
		if (FAILED(res)) {
			return;
		}

		joy.joy_axis.push_back(ofs);
	}
}

BOOL CALLBACK JoypadWindows::enumCallback(const DIDEVICEINSTANCE *p_instance, void *p_context) {
	JoypadWindows *self = static_cast<JoypadWindows *>(p_context);
	if (self->is_xinput_joypad(&p_instance->guidProduct)) {
		self->probe_xinput_joypad(p_instance->tszProductName);
		return DIENUM_CONTINUE;
	}
	self->setup_dinput_joypad(p_instance);
	return DIENUM_CONTINUE;
}

BOOL CALLBACK JoypadWindows::objectsCallback(const DIDEVICEOBJECTINSTANCE *p_instance, void *p_context) {
	JoypadWindows *self = static_cast<JoypadWindows *>(p_context);
	self->setup_d_joypad_object(p_instance, self->id_to_change);

	return DIENUM_CONTINUE;
}

void JoypadWindows::close_d_joypad(int id) {
	if (id == -1) {
		for (int i = 0; i < JOYPADS_MAX; i++) {
			close_d_joypad(i);
		}
		return;
	}

	if (!d_joypads[id].attached) {
		return;
	}

	d_joypads[id].di_joy->Unacquire();
	d_joypads[id].di_joy->Release();
	d_joypads[id].attached = false;
	attached_joypads[d_joypads[id].id] = false;
	d_joypads[id].guid.Data1 = d_joypads[id].guid.Data2 = d_joypads[id].guid.Data3 = 0;
	input->joy_connection_changed(d_joypads[id].id, false, "");
	d_joypad_count--;
}

void JoypadWindows::probe_joypads() {
	ERR_FAIL_NULL_MSG(dinput, "DirectInput not initialized. Rebooting your PC may solve this issue.");

	for (int i = 0; i < d_joypad_count; i++) {
		d_joypads[i].confirmed = false; // Flag DirectInput devices for re-checking their availability.
	}

	x_joypad_probe_count = 0;
	// Probe _all attached_ joypad devices.
	dinput->EnumDevices(DI8DEVCLASS_GAMECTRL, enumCallback, this, DIEDFL_ATTACHEDONLY);

	for (int i = x_joypad_probe_count; i < XUSER_MAX_COUNT; i++) {
		// Handle disconnect of XInput devices.
		// And act as a fallback, just in case DirectInput could not find the device.
		probe_xinput_joypad();
	}

	for (int i = 0; i < d_joypad_count; i++) {
		if (!d_joypads[i].confirmed) {
			close_d_joypad(i); // Any DirectInput device not found during probing is considered as disconnected.
		}
	}
}

void JoypadWindows::process_joypads() {
	HRESULT hr;

	// Handle XInput joypads.
	for (int i = 0; i < XUSER_MAX_COUNT; i++) {
		xinput_gamepad &joy = x_joypads[i];
		if (!joy.attached) {
			continue;
		}
		ZeroMemory(&joy.state, sizeof(XINPUT_STATE));

		xinput_get_state(i, &joy.state);
		if (joy.state.dwPacketNumber != joy.last_packet) {
			int button_mask = XINPUT_GAMEPAD_DPAD_UP;
			for (int j = 0; j <= 16; j++) {
				input->joy_button(joy.id, (JoyButton)j, joy.state.Gamepad.wButtons & button_mask);
				button_mask = button_mask * 2;
			}

			input->joy_axis(joy.id, JoyAxis::LEFT_X, axis_correct(joy.state.Gamepad.sThumbLX, true));
			input->joy_axis(joy.id, JoyAxis::LEFT_Y, axis_correct(joy.state.Gamepad.sThumbLY, true, false, true));
			input->joy_axis(joy.id, JoyAxis::RIGHT_X, axis_correct(joy.state.Gamepad.sThumbRX, true));
			input->joy_axis(joy.id, JoyAxis::RIGHT_Y, axis_correct(joy.state.Gamepad.sThumbRY, true, false, true));
			input->joy_axis(joy.id, JoyAxis::TRIGGER_LEFT, axis_correct(joy.state.Gamepad.bLeftTrigger, true, true));
			input->joy_axis(joy.id, JoyAxis::TRIGGER_RIGHT, axis_correct(joy.state.Gamepad.bRightTrigger, true, true));
			joy.last_packet = joy.state.dwPacketNumber;
		}
		uint64_t timestamp = input->get_joy_vibration_timestamp(joy.id);
		if (timestamp > joy.ff_timestamp) {
			Vector2 strength = input->get_joy_vibration_strength(joy.id);
			float duration = input->get_joy_vibration_duration(joy.id);
			if (strength.x == 0 && strength.y == 0) {
				joypad_vibration_stop_xinput(i, timestamp);
			} else {
				joypad_vibration_start_xinput(i, strength.x, strength.y, duration, timestamp);
			}
		} else if (joy.vibrating && joy.ff_end_timestamp != 0) {
			uint64_t current_time = OS::get_singleton()->get_ticks_usec();
			if (current_time >= joy.ff_end_timestamp) {
				joypad_vibration_stop_xinput(i, current_time);
			}
		}
	}

	// Handle DirectIndput joypads.
	for (int i = 0; i < JOYPADS_MAX; i++) {
		dinput_gamepad *joy = &d_joypads[i];

		if (!joy->attached) {
			continue;
		}

		DIJOYSTATE2 js;
		hr = joy->di_joy->Poll();
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED) {
			IDirectInputDevice8_Acquire(joy->di_joy);
			joy->di_joy->Poll();
		}

		hr = joy->di_joy->GetDeviceState(sizeof(DIJOYSTATE2), &js);
		if (FAILED(hr)) {
			continue;
		}

		post_hat(joy->id, js.rgdwPOV[0]);

		for (int j = 0; j < 128; j++) {
			if (js.rgbButtons[j] & 0x80) {
				if (!joy->last_buttons[j]) {
					input->joy_button(joy->id, (JoyButton)j, true);
					joy->last_buttons[j] = true;
				}
			} else {
				if (joy->last_buttons[j]) {
					input->joy_button(joy->id, (JoyButton)j, false);
					joy->last_buttons[j] = false;
				}
			}
		}

		// on mingw, these constants are not constants
		int count = 8;
		const LONG axes[] = { DIJOFS_X, DIJOFS_Y, DIJOFS_Z, DIJOFS_RX, DIJOFS_RY, DIJOFS_RZ, (LONG)DIJOFS_SLIDER(0), (LONG)DIJOFS_SLIDER(1) };
		int values[] = { js.lX, js.lY, js.lZ, js.lRx, js.lRy, js.lRz, js.rglSlider[0], js.rglSlider[1] };

		for (uint32_t j = 0; j < joy->joy_axis.size(); j++) {
			for (int k = 0; k < count; k++) {
				if (joy->joy_axis[j] == axes[k]) {
					input->joy_axis(joy->id, (JoyAxis)j, axis_correct(values[k]));
					break;
				}
			}
		}
	}
	return;
}

void JoypadWindows::post_hat(int p_device, DWORD p_dpad) {
	BitField<HatMask> dpad_val;

	// Should be -1 when centered, but according to docs:
	// "Some drivers report the centered position of the POV indicator as 65,535. Determine whether the indicator is centered as follows:
	//  BOOL POVCentered = (LOWORD(dwPOV) == 0xFFFF);"
	// https://docs.microsoft.com/en-us/previous-versions/windows/desktop/ee416628(v%3Dvs.85)#remarks
	if (LOWORD(p_dpad) == 0xFFFF) {
		// Do nothing.
		// dpad_val.set_flag(HatMask::CENTER);
	}
	if (p_dpad == 0) {
		dpad_val.set_flag(HatMask::UP);

	} else if (p_dpad == 4500) {
		dpad_val.set_flag(HatMask::UP);
		dpad_val.set_flag(HatMask::RIGHT);

	} else if (p_dpad == 9000) {
		dpad_val.set_flag(HatMask::RIGHT);

	} else if (p_dpad == 13500) {
		dpad_val.set_flag(HatMask::RIGHT);
		dpad_val.set_flag(HatMask::DOWN);

	} else if (p_dpad == 18000) {
		dpad_val.set_flag(HatMask::DOWN);

	} else if (p_dpad == 22500) {
		dpad_val.set_flag(HatMask::DOWN);
		dpad_val.set_flag(HatMask::LEFT);

	} else if (p_dpad == 27000) {
		dpad_val.set_flag(HatMask::LEFT);

	} else if (p_dpad == 31500) {
		dpad_val.set_flag(HatMask::LEFT);
		dpad_val.set_flag(HatMask::UP);
	}
	input->joy_hat(p_device, dpad_val);
}

float JoypadWindows::axis_correct(int p_val, bool p_xinput, bool p_trigger, bool p_negate) const {
	if (Math::abs(p_val) < MIN_JOY_AXIS) {
		return p_trigger ? -1.0f : 0.0f;
	}
	if (!p_xinput) {
		return (float)p_val / MAX_JOY_AXIS;
	}
	if (p_trigger) {
		// Convert to a value between -1.0f and 1.0f.
		return 2.0f * p_val / MAX_TRIGGER - 1.0f;
	}
	float value;
	if (p_val < 0) {
		value = (float)p_val / MAX_JOY_AXIS;
	} else {
		value = (float)p_val / (MAX_JOY_AXIS - 1);
	}
	if (p_negate) {
		value = -value;
	}
	return value;
}

void JoypadWindows::joypad_vibration_start_xinput(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration, uint64_t p_timestamp) {
	xinput_gamepad &joy = x_joypads[p_device];
	if (joy.attached) {
		XINPUT_VIBRATION effect;
		effect.wLeftMotorSpeed = (65535 * p_strong_magnitude);
		effect.wRightMotorSpeed = (65535 * p_weak_magnitude);
		if (xinput_set_state(p_device, &effect) == ERROR_SUCCESS) {
			joy.ff_timestamp = p_timestamp;
			joy.ff_end_timestamp = p_duration == 0 ? 0 : p_timestamp + (uint64_t)(p_duration * 1000000.0);
			joy.vibrating = true;
		}
	}
}

void JoypadWindows::joypad_vibration_stop_xinput(int p_device, uint64_t p_timestamp) {
	xinput_gamepad &joy = x_joypads[p_device];
	if (joy.attached) {
		XINPUT_VIBRATION effect;
		effect.wLeftMotorSpeed = 0;
		effect.wRightMotorSpeed = 0;
		if (xinput_set_state(p_device, &effect) == ERROR_SUCCESS) {
			joy.ff_timestamp = p_timestamp;
			joy.vibrating = false;
		}
	}
}

void JoypadWindows::load_xinput() {
	xinput_get_state = &_xinput_get_state;
	xinput_set_state = &_xinput_set_state;
	winmm_get_joycaps = &_winmm_get_joycaps;
	bool legacy_xinput = false;

	xinput_dll = LoadLibrary("XInput1_4.dll");
	if (!xinput_dll) {
		xinput_dll = LoadLibrary("XInput1_3.dll");
		if (!xinput_dll) {
			xinput_dll = LoadLibrary("XInput9_1_0.dll");
			legacy_xinput = true;
		}
	}

	if (!xinput_dll) {
		print_verbose("Could not find XInput, using DirectInput only");
		return;
	}

	// (LPCSTR)100 is the magic number to get XInputGetStateEx, which also provides the state for the guide button
	LPCSTR get_state_func_name = legacy_xinput ? "XInputGetState" : (LPCSTR)100;
	XInputGetState_t func = (XInputGetState_t)GetProcAddress((HMODULE)xinput_dll, get_state_func_name);
	XInputSetState_t set_func = (XInputSetState_t)GetProcAddress((HMODULE)xinput_dll, "XInputSetState");
	if (!func || !set_func) {
		unload_xinput();
		return;
	}
	xinput_get_state = func;
	xinput_set_state = set_func;

	winmm_dll = LoadLibrary("Winmm.dll");
	if (winmm_dll) {
		joyGetDevCaps_t caps_func = (joyGetDevCaps_t)GetProcAddress((HMODULE)winmm_dll, "joyGetDevCapsW");
		if (caps_func) {
			winmm_get_joycaps = caps_func;
		} else {
			unload_winmm();
		}
	}
}

void JoypadWindows::unload_xinput() {
	if (xinput_dll) {
		FreeLibrary((HMODULE)xinput_dll);
	}
}

void JoypadWindows::unload_winmm() {
	if (winmm_dll) {
		FreeLibrary((HMODULE)winmm_dll);
	}
}
