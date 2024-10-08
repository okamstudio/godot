/**************************************************************************/
/*  core_constants.cpp                                                    */
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

#include "core_constants.h"

#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"

struct _CoreConstant {
#ifdef DEBUG_METHODS_ENABLED
	bool ignore_value_in_docs = false;
	bool is_bitfield = false;
#endif
	StringName enum_name;
	const char *name = nullptr;
	int64_t value = 0;

	_CoreConstant() {}

#ifdef DEBUG_METHODS_ENABLED
	_CoreConstant(const StringName &p_enum_name, const char *p_name, int64_t p_value, bool p_ignore_value_in_docs = false, bool p_is_bitfield = false) :
			ignore_value_in_docs(p_ignore_value_in_docs),
			is_bitfield(p_is_bitfield),
			enum_name(p_enum_name),
			name(p_name),
			value(p_value) {
	}
#else
	_CoreConstant(const StringName &p_enum_name, const char *p_name, int64_t p_value) :
			enum_name(p_enum_name),
			name(p_name),
			value(p_value) {
	}
#endif
};

static Vector<_CoreConstant> _global_constants;
static HashMap<StringName, int> _global_constants_map;
static HashMap<StringName, Vector<_CoreConstant>> _global_enums;

#ifdef DEBUG_METHODS_ENABLED

#define BIND_CORE_CONSTANT(m_constant)                                                 \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_BITFIELD_FLAG(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = __constant_get_bitfield_name(m_constant, #m_constant);                \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant, false, true)); \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

// This just binds enum classes as if they were regular enum constants.
#define BIND_CORE_ENUM_CLASS_CONSTANT(m_enum, m_prefix, m_member)                                                  \
	{                                                                                                              \
		StringName enum_name = __constant_get_enum_name(m_enum::m_member, #m_prefix "_" #m_member);                \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG(m_enum, m_prefix, m_member)                                                               \
	{                                                                                                                           \
		StringName enum_name = __constant_get_bitfield_name(m_enum::m_member, #m_prefix "_" #m_member);                         \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member, false, true)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                                          \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);                            \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(m_enum, m_name, m_member)                               \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_enum::m_member, #m_name);                  \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member));   \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                               \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(m_enum, m_name, m_member)                                          \
	{                                                                                                           \
		StringName enum_name = __constant_get_bitfield_name(m_enum::m_member, #m_name);                         \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member, false, true)); \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                                          \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);            \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_NO_VAL(m_enum, m_prefix, m_member)                                                 \
	{                                                                                                                    \
		StringName enum_name = __constant_get_enum_name(m_enum::m_member, #m_prefix "_" #m_member);                      \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member, true)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                                   \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);                     \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM(m_custom_name, m_constant)                                    \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant));            \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_CONSTANT_NO_VAL(m_constant)                                                \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant, true)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT_NO_VAL(m_constant)                                                   \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant, true));        \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM_NO_VAL(m_custom_name, m_constant)                             \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant, true));      \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#else

#define BIND_CORE_CONSTANT(m_constant)                                                 \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_BITFIELD_FLAG(m_constant)                                                          \
	{                                                                                                \
		StringName enum_name = __constant_get_bitfield_name(m_constant, #m_constant);                \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

// This just binds enum classes as if they were regular enum constants.
#define BIND_CORE_ENUM_CLASS_CONSTANT(m_enum, m_prefix, m_member)                                                  \
	{                                                                                                              \
		StringName enum_name = __constant_get_enum_name(m_enum::m_member, #m_prefix "_" #m_member);                \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG(m_enum, m_prefix, m_member)                                                  \
	{                                                                                                              \
		StringName enum_name = __constant_get_bitfield_name(m_enum::m_member, #m_prefix "_" #m_member);            \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(m_enum, m_name, m_member)                               \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_enum::m_member, #m_name);                  \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member));   \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                               \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(m_enum, m_name, m_member)                               \
	{                                                                                                \
		StringName enum_name = __constant_get_bitfield_name(m_enum::m_member, #m_name);              \
		_global_constants.push_back(_CoreConstant(enum_name, #m_name, (int64_t)m_enum::m_member));   \
		_global_constants_map[#m_name] = _global_constants.size() - 1;                               \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_ENUM_CLASS_CONSTANT_NO_VAL(m_enum, m_prefix, m_member)                                           \
	{                                                                                                              \
		StringName enum_name = __constant_get_enum_name(m_enum::m_member, #m_prefix "_" #m_member);                \
		_global_constants.push_back(_CoreConstant(enum_name, #m_prefix "_" #m_member, (int64_t)m_enum::m_member)); \
		_global_constants_map[#m_prefix "_" #m_member] = _global_constants.size() - 1;                             \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]);               \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM(m_custom_name, m_constant)                                    \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant));            \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_CONSTANT_NO_VAL(m_constant)                                          \
	_global_constants.push_back(_CoreConstant(StringName(), #m_constant, m_constant)); \
	_global_constants_map[#m_constant] = _global_constants.size() - 1;

#define BIND_CORE_ENUM_CONSTANT_NO_VAL(m_constant)                                                   \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, #m_constant, m_constant));              \
		_global_constants_map[#m_constant] = _global_constants.size() - 1;                           \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#define BIND_CORE_ENUM_CONSTANT_CUSTOM_NO_VAL(m_custom_name, m_constant)                             \
	{                                                                                                \
		StringName enum_name = __constant_get_enum_name(m_constant, #m_constant);                    \
		_global_constants.push_back(_CoreConstant(enum_name, m_custom_name, m_constant));            \
		_global_constants_map[m_custom_name] = _global_constants.size() - 1;                         \
		_global_enums[enum_name].push_back((_global_constants.ptr())[_global_constants.size() - 1]); \
	}

#endif

void register_global_constants() {
	BIND_CORE_ENUM_CONSTANT(SIDE_LEFT);
	BIND_CORE_ENUM_CONSTANT(SIDE_TOP);
	BIND_CORE_ENUM_CONSTANT(SIDE_RIGHT);
	BIND_CORE_ENUM_CONSTANT(SIDE_BOTTOM);

	BIND_CORE_ENUM_CONSTANT(CORNER_TOP_LEFT);
	BIND_CORE_ENUM_CONSTANT(CORNER_TOP_RIGHT);
	BIND_CORE_ENUM_CONSTANT(CORNER_BOTTOM_RIGHT);
	BIND_CORE_ENUM_CONSTANT(CORNER_BOTTOM_LEFT);

	BIND_CORE_ENUM_CONSTANT(VERTICAL);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL);

	BIND_CORE_ENUM_CONSTANT(CLOCKWISE);
	BIND_CORE_ENUM_CONSTANT(COUNTERCLOCKWISE);

	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_LEFT);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_CENTER);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_RIGHT);
	BIND_CORE_ENUM_CONSTANT(HORIZONTAL_ALIGNMENT_FILL);

	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_TOP);
	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_CENTER);
	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_BOTTOM);
	BIND_CORE_ENUM_CONSTANT(VERTICAL_ALIGNMENT_FILL);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TOP_TO);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_CENTER_TO);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_BASELINE_TO);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_BOTTOM_TO);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_TOP);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_CENTER);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_BASELINE);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TO_BOTTOM);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TOP);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_CENTER);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_BOTTOM);

	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_IMAGE_MASK);
	BIND_CORE_ENUM_CONSTANT(INLINE_ALIGNMENT_TEXT_MASK);

	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, XYZ);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, XZY);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, YXZ);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, YZX);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, ZXY);
	BIND_CORE_ENUM_CLASS_CONSTANT(EulerOrder, EULER_ORDER, ZYX);

	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, NONE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SPECIAL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, ESCAPE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, TAB);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BACKTAB);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BACKSPACE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, ENTER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_ENTER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, INSERT);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_DELETE, KEY_DELETE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PAUSE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PRINT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SYSREQ);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, CLEAR);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, HOME);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, END);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, UP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, DOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PAGEUP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PAGEDOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SHIFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, CTRL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, META);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, ALT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, CAPSLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, NUMLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SCROLLLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F1);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F2);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F3);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F4);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F5);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F6);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F7);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F8);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F10);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F11);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F12);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F13);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F14);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F15);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F16);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F17);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F18);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F19);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F20);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F21);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F22);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F23);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F24);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F25);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F26);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F27);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F28);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F29);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F30);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F31);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F32);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F33);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F34);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F35);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_MULTIPLY);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_DIVIDE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_SUBTRACT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_PERIOD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_ADD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_0);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_1);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_2);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_3);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_4);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_5);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_6);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_7);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_8);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KP_9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, MENU);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, HYPER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, HELP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BACK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, FORWARD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, STOP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, REFRESH);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, VOLUMEDOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, VOLUMEMUTE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, VOLUMEUP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, MEDIAPLAY);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, MEDIASTOP);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, MEDIAPREVIOUS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, MEDIANEXT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, MEDIARECORD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, HOMEPAGE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, FAVORITES);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SEARCH);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, STANDBY);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, OPENURL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHMAIL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHMEDIA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH0);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH1);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH2);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH3);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH4);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH5);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH6);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH7);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH8);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCH9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHB);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHC);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LAUNCHF);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, GLOBE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, KEYBOARD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, JIS_EISU);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, JIS_KANA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, UNKNOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SPACE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, EXCLAM);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, QUOTEDBL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, NUMBERSIGN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, DOLLAR);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PERCENT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, AMPERSAND);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, APOSTROPHE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PARENLEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PARENRIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, ASTERISK);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PLUS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, COMMA);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, MINUS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, PERIOD);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SLASH);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_0, KEY_0);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_1, KEY_1);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_2, KEY_2);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_3, KEY_3);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_4, KEY_4);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_5, KEY_5);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_6, KEY_6);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_7, KEY_7);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_8, KEY_8);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(Key, KEY_9, KEY_9);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, COLON);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SEMICOLON);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, LESS);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, EQUAL);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, GREATER);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, QUESTION);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, AT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, A);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, B);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, C);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, D);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, E);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, F);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, G);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, H);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, I);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, J);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, K);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, L);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, M);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, N);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, O);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, P);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, Q);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, R);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, S);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, T);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, U);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, V);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, W);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, X);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, Z);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BRACKETLEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BACKSLASH);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BRACKETRIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, ASCIICIRCUM);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, UNDERSCORE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, QUOTELEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BRACELEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BAR);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, BRACERIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, ASCIITILDE);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, YEN);
	BIND_CORE_ENUM_CLASS_CONSTANT(Key, KEY, SECTION);

	BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(KeyModifierMask, KEY_CODE_MASK, CODE_MASK);
	BIND_CORE_BITFIELD_CLASS_FLAG_CUSTOM(KeyModifierMask, KEY_MODIFIER_MASK, MODIFIER_MASK);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, CMD_OR_CTRL);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, SHIFT);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, ALT);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, META);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, CTRL);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, KPAD);
	BIND_CORE_BITFIELD_CLASS_FLAG(KeyModifierMask, KEY_MASK, GROUP_SWITCH);

	BIND_CORE_ENUM_CLASS_CONSTANT(KeyLocation, KEY_LOCATION, UNSPECIFIED);
	BIND_CORE_ENUM_CLASS_CONSTANT(KeyLocation, KEY_LOCATION, LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(KeyLocation, KEY_LOCATION, RIGHT);

	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, NONE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, MIDDLE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, WHEEL_UP);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, WHEEL_DOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, WHEEL_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MouseButton, MOUSE_BUTTON, WHEEL_RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(MouseButton, MOUSE_BUTTON_XBUTTON1, MB_XBUTTON1);
	BIND_CORE_ENUM_CLASS_CONSTANT_CUSTOM(MouseButton, MOUSE_BUTTON_XBUTTON2, MB_XBUTTON2);

	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, LEFT);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, RIGHT);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MIDDLE);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MB_XBUTTON1);
	BIND_CORE_BITFIELD_CLASS_FLAG(MouseButtonMask, MOUSE_BUTTON_MASK, MB_XBUTTON2);

	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, INVALID);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, A);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, B);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, X);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, BACK);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, GUIDE);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, START);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, LEFT_STICK);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, RIGHT_STICK);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, LEFT_SHOULDER);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, RIGHT_SHOULDER);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, DPAD_UP);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, DPAD_DOWN);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, DPAD_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, DPAD_RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, MISC1);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, PADDLE1);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, PADDLE2);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, PADDLE3);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, PADDLE4);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, TOUCHPAD);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, SDL_MAX);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyButton, JOY_BUTTON, MAX);

	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, INVALID);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, LEFT_X);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, LEFT_Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, RIGHT_X);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, RIGHT_Y);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, TRIGGER_LEFT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, TRIGGER_RIGHT);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, SDL_MAX);
	BIND_CORE_ENUM_CLASS_CONSTANT(JoyAxis, JOY_AXIS, MAX);

	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, NONE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, NOTE_OFF);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, NOTE_ON);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, AFTERTOUCH);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, CONTROL_CHANGE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, PROGRAM_CHANGE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, CHANNEL_PRESSURE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, PITCH_BEND);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, SYSTEM_EXCLUSIVE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, QUARTER_FRAME);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, SONG_POSITION_POINTER);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, SONG_SELECT);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, TUNE_REQUEST);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, TIMING_CLOCK);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, START);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, CONTINUE);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, STOP);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, ACTIVE_SENSING);
	BIND_CORE_ENUM_CLASS_CONSTANT(MIDIMessage, MIDI_MESSAGE, SYSTEM_RESET);

	// error list

	BIND_CORE_ENUM_CONSTANT(OK); // (0)
	BIND_CORE_ENUM_CONSTANT(FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_UNAVAILABLE);
	BIND_CORE_ENUM_CONSTANT(ERR_UNCONFIGURED);
	BIND_CORE_ENUM_CONSTANT(ERR_UNAUTHORIZED);
	BIND_CORE_ENUM_CONSTANT(ERR_PARAMETER_RANGE_ERROR); // (5)
	BIND_CORE_ENUM_CONSTANT(ERR_OUT_OF_MEMORY);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_NOT_FOUND);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_BAD_DRIVE);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_BAD_PATH);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_NO_PERMISSION); // (10)
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_ALREADY_IN_USE);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CANT_OPEN);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CANT_WRITE);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CANT_READ);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_UNRECOGNIZED); // (15)
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_CORRUPT);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_MISSING_DEPENDENCIES);
	BIND_CORE_ENUM_CONSTANT(ERR_FILE_EOF);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_OPEN);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_CREATE); // (20)
	BIND_CORE_ENUM_CONSTANT(ERR_QUERY_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_ALREADY_IN_USE);
	BIND_CORE_ENUM_CONSTANT(ERR_LOCKED);
	BIND_CORE_ENUM_CONSTANT(ERR_TIMEOUT);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_CONNECT); // (25)
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_RESOLVE);
	BIND_CORE_ENUM_CONSTANT(ERR_CONNECTION_ERROR);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_ACQUIRE_RESOURCE);
	BIND_CORE_ENUM_CONSTANT(ERR_CANT_FORK);
	BIND_CORE_ENUM_CONSTANT(ERR_INVALID_DATA); // (30)
	BIND_CORE_ENUM_CONSTANT(ERR_INVALID_PARAMETER);
	BIND_CORE_ENUM_CONSTANT(ERR_ALREADY_EXISTS);
	BIND_CORE_ENUM_CONSTANT(ERR_DOES_NOT_EXIST);
	BIND_CORE_ENUM_CONSTANT(ERR_DATABASE_CANT_READ);
	BIND_CORE_ENUM_CONSTANT(ERR_DATABASE_CANT_WRITE); // (35)
	BIND_CORE_ENUM_CONSTANT(ERR_COMPILATION_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_METHOD_NOT_FOUND);
	BIND_CORE_ENUM_CONSTANT(ERR_LINK_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_SCRIPT_FAILED);
	BIND_CORE_ENUM_CONSTANT(ERR_CYCLIC_LINK); // (40)
	BIND_CORE_ENUM_CONSTANT(ERR_INVALID_DECLARATION);
	BIND_CORE_ENUM_CONSTANT(ERR_DUPLICATE_SYMBOL);
	BIND_CORE_ENUM_CONSTANT(ERR_PARSE_ERROR);
	BIND_CORE_ENUM_CONSTANT(ERR_BUSY);
	BIND_CORE_ENUM_CONSTANT(ERR_SKIP); // (45)
	BIND_CORE_ENUM_CONSTANT(ERR_HELP);
	BIND_CORE_ENUM_CONSTANT(ERR_BUG);
	BIND_CORE_ENUM_CONSTANT(ERR_PRINTER_ON_FIRE);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NONE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_RANGE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_ENUM);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_ENUM_SUGGESTION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_EXP_EASING);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LINK);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_FLAGS);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_2D_RENDER);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_2D_PHYSICS);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_2D_NAVIGATION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_3D_RENDER);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_3D_PHYSICS);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_3D_NAVIGATION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LAYERS_AVOIDANCE);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_DIR);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_GLOBAL_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_GLOBAL_DIR);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_RESOURCE_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_MULTILINE_TEXT);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_EXPRESSION);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_PLACEHOLDER_TEXT);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_COLOR_NO_ALPHA);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_OBJECT_ID);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_TYPE_STRING);

	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_OBJECT_TOO_BIG);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NODE_PATH_VALID_TYPES);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_SAVE_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_GLOBAL_SAVE_FILE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_INT_IS_OBJECTID);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_INT_IS_POINTER);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_ARRAY_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_DICTIONARY_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LOCALE_ID);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_LOCALIZABLE_STRING);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_NODE_TYPE);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_HIDE_QUATERNION_EDIT);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_PASSWORD);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_TOOL_BUTTON);
	BIND_CORE_ENUM_CONSTANT(PROPERTY_HINT_MAX);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NONE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_STORAGE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_EDITOR);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_INTERNAL);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CHECKABLE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CHECKED);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_GROUP);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CATEGORY);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SUBGROUP);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CLASS_IS_BITFIELD);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NO_INSTANCE_STATE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_RESTART_IF_CHANGED);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SCRIPT_VARIABLE);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_STORE_IF_NULL);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SCRIPT_DEFAULT_VALUE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_CLASS_IS_ENUM);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NIL_IS_VARIANT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_ARRAY);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_ALWAYS_DUPLICATE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NEVER_DUPLICATE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_HIGH_END_GFX);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NODE_PATH_FROM_SCENE_ROOT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_RESOURCE_NOT_PERSISTENT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_KEYING_INCREMENTS);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_DEFERRED_SET_RESOURCE);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_EDITOR_INSTANTIATE_OBJECT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_EDITOR_BASIC_SETTING);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_READ_ONLY);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_SECRET);

	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_DEFAULT);
	BIND_CORE_BITFIELD_FLAG(PROPERTY_USAGE_NO_EDITOR);

	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_NORMAL);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_EDITOR);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_CONST);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_VIRTUAL);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_VARARG);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_STATIC);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAG_OBJECT_CORE);
	BIND_CORE_BITFIELD_FLAG(METHOD_FLAGS_DEFAULT);

	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_NIL", Variant::NIL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_BOOL", Variant::BOOL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_INT", Variant::INT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_FLOAT", Variant::FLOAT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_STRING", Variant::STRING);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR2", Variant::VECTOR2);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR2I", Variant::VECTOR2I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_RECT2", Variant::RECT2);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_RECT2I", Variant::RECT2I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR3", Variant::VECTOR3);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR3I", Variant::VECTOR3I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_TRANSFORM2D", Variant::TRANSFORM2D);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR4", Variant::VECTOR4);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_VECTOR4I", Variant::VECTOR4I);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PLANE", Variant::PLANE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_QUATERNION", Variant::QUATERNION);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_AABB", Variant::AABB);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_BASIS", Variant::BASIS);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_TRANSFORM3D", Variant::TRANSFORM3D);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PROJECTION", Variant::PROJECTION);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_COLOR", Variant::COLOR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_STRING_NAME", Variant::STRING_NAME);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_NODE_PATH", Variant::NODE_PATH);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_RID", Variant::RID);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_OBJECT", Variant::OBJECT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_CALLABLE", Variant::CALLABLE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_SIGNAL", Variant::SIGNAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_DICTIONARY", Variant::DICTIONARY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_ARRAY", Variant::ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_BYTE_ARRAY", Variant::PACKED_BYTE_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_INT32_ARRAY", Variant::PACKED_INT32_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_INT64_ARRAY", Variant::PACKED_INT64_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_FLOAT32_ARRAY", Variant::PACKED_FLOAT32_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_FLOAT64_ARRAY", Variant::PACKED_FLOAT64_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_STRING_ARRAY", Variant::PACKED_STRING_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_VECTOR2_ARRAY", Variant::PACKED_VECTOR2_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_VECTOR3_ARRAY", Variant::PACKED_VECTOR3_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_COLOR_ARRAY", Variant::PACKED_COLOR_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_PACKED_VECTOR4_ARRAY", Variant::PACKED_VECTOR4_ARRAY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("TYPE_MAX", Variant::VARIANT_MAX);

	//comparison
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_EQUAL", Variant::OP_EQUAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_NOT_EQUAL", Variant::OP_NOT_EQUAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_LESS", Variant::OP_LESS);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_LESS_EQUAL", Variant::OP_LESS_EQUAL);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_GREATER", Variant::OP_GREATER);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_GREATER_EQUAL", Variant::OP_GREATER_EQUAL);
	//mathematic
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_ADD", Variant::OP_ADD);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_SUBTRACT", Variant::OP_SUBTRACT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_MULTIPLY", Variant::OP_MULTIPLY);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_DIVIDE", Variant::OP_DIVIDE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_NEGATE", Variant::OP_NEGATE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_POSITIVE", Variant::OP_POSITIVE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_MODULE", Variant::OP_MODULE);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_POWER", Variant::OP_POWER);
	//bitwise
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_SHIFT_LEFT", Variant::OP_SHIFT_LEFT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_SHIFT_RIGHT", Variant::OP_SHIFT_RIGHT);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_AND", Variant::OP_BIT_AND);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_OR", Variant::OP_BIT_OR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_XOR", Variant::OP_BIT_XOR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_BIT_NEGATE", Variant::OP_BIT_NEGATE);
	//logic
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_AND", Variant::OP_AND);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_OR", Variant::OP_OR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_XOR", Variant::OP_XOR);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_NOT", Variant::OP_NOT);
	//containment
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_IN", Variant::OP_IN);
	BIND_CORE_ENUM_CONSTANT_CUSTOM("OP_MAX", Variant::OP_MAX);
}

void unregister_global_constants() {
	_global_constants.clear();
	_global_constants_map.clear();
	_global_enums.clear();
}

int CoreConstants::get_global_constant_count() {
	return _global_constants.size();
}

StringName CoreConstants::get_global_constant_enum(int p_idx) {
	return _global_constants[p_idx].enum_name;
}

#ifdef DEBUG_METHODS_ENABLED
bool CoreConstants::is_global_constant_bitfield(int p_idx) {
	return _global_constants[p_idx].is_bitfield;
}

bool CoreConstants::get_ignore_value_in_docs(int p_idx) {
	return _global_constants[p_idx].ignore_value_in_docs;
}
#else
bool CoreConstants::is_global_constant_bitfield(int p_idx) {
	return false;
}

bool CoreConstants::get_ignore_value_in_docs(int p_idx) {
	return false;
}
#endif

const char *CoreConstants::get_global_constant_name(int p_idx) {
	return _global_constants[p_idx].name;
}

int64_t CoreConstants::get_global_constant_value(int p_idx) {
	return _global_constants[p_idx].value;
}

bool CoreConstants::is_global_constant(const StringName &p_name) {
	return _global_constants_map.has(p_name);
}

int CoreConstants::get_global_constant_index(const StringName &p_name) {
	ERR_FAIL_COND_V_MSG(!_global_constants_map.has(p_name), -1, "Trying to get index of non-existing constant.");
	return _global_constants_map[p_name];
}

bool CoreConstants::is_global_enum(const StringName &p_enum) {
	return _global_enums.has(p_enum);
}

void CoreConstants::get_enum_values(const StringName &p_enum, HashMap<StringName, int64_t> *p_values) {
	ERR_FAIL_NULL_MSG(p_values, "Trying to get enum values with null map.");
	ERR_FAIL_COND_MSG(!_global_enums.has(p_enum), "Trying to get values of non-existing enum.");
	for (const _CoreConstant &constant : _global_enums[p_enum]) {
		(*p_values)[constant.name] = constant.value;
	}
}
