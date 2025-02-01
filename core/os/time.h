/**************************************************************************/
/*  time.h                                                                */
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

#include "core/object/class_db.h"
#include "time_enums.h"

// This Time class conforms with as many of the ISO 8601 standards as possible.
// * As per ISO 8601:2004 4.3.2.1, all dates follow the Proleptic Gregorian
//   calendar. As such, the day before 1582-10-15 is 1582-10-14, not 1582-10-04.
//   See: https://en.wikipedia.org/wiki/Proleptic_Gregorian_calendar
// * As per ISO 8601:2004 3.4.2 and 4.1.2.4, the year before 1 AD (aka 1 BC)
//   is number "0", with the year before that (2 BC) being "-1", etc.
// Conversion methods assume "the same timezone", and do not handle DST.
// Leap seconds are not handled, they must be done manually if desired.
// Suffixes such as "Z" are not handled, you need to strip them away manually.

class Time : public Object {
	GDCLASS(Time, Object);
	static void _bind_methods();
	static Time *singleton;

public:
	static Time *get_singleton();

	// Methods that convert times.
	Dictionary get_datetime_dict_from_unix_time(int64_t p_unix_time_val) const;
	Dictionary get_date_dict_from_unix_time(int64_t p_unix_time_val) const;
	Dictionary get_time_dict_from_unix_time(int64_t p_unix_time_val) const;
	String get_datetime_string_from_unix_time(int64_t p_unix_time_val, bool p_use_space = false) const;
	String get_date_string_from_unix_time(int64_t p_unix_time_val) const;
	String get_time_string_from_unix_time(int64_t p_unix_time_val) const;
	Dictionary get_datetime_dict_from_datetime_string(const String &p_datetime, bool p_weekday = true) const;
	String get_datetime_string_from_datetime_dict(const Dictionary &p_datetime, bool p_use_space = false) const;
	int64_t get_unix_time_from_datetime_dict(const Dictionary &p_datetime) const;
	int64_t get_unix_time_from_datetime_string(const String &p_datetime) const;
	String get_offset_string_from_offset_minutes(int64_t p_offset_minutes) const;

	// Methods that get information from OS.
	Dictionary get_datetime_dict_from_system(bool p_utc = false) const;
	Dictionary get_date_dict_from_system(bool p_utc = false) const;
	Dictionary get_time_dict_from_system(bool p_utc = false) const;
	String get_datetime_string_from_system(bool p_utc = false, bool p_use_space = false) const;
	String get_date_string_from_system(bool p_utc = false) const;
	String get_time_string_from_system(bool p_utc = false) const;
	Dictionary get_time_zone_from_system() const;
	double get_unix_time_from_system() const;
	uint64_t get_ticks_msec() const;
	uint64_t get_ticks_usec() const;

	Time();
	virtual ~Time();
};

VARIANT_ENUM_CAST(Month);
VARIANT_ENUM_CAST(Weekday);
