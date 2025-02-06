/**************************************************************************/
/*  time.cpp                                                              */
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

#include "time.h"

#include "core/os/os.h"

#define UNIX_EPOCH_YEAR_AD 1970 // 1970
#define SECONDS_PER_DAY (24 * 60 * 60) // 86400
#define IS_LEAP_YEAR(year) (!((year) % 4) && (((year) % 100) || !((year) % 400)))
#define YEAR_SIZE(year) (IS_LEAP_YEAR(year) ? 366 : 365)

#define YEAR_KEY "year"
#define MONTH_KEY "month"
#define DAY_KEY "day"
#define WEEKDAY_KEY "weekday"
#define HOUR_KEY "hour"
#define MINUTE_KEY "minute"
#define SECOND_KEY "second"
#define DST_KEY "dst"

// Table of number of days in each month (for regular year and leap year).
static const uint8_t MONTH_DAYS_TABLE[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

#define UNIX_TIME_TO_HMS                                                     \
	uint8_t hour, minute, second;                                            \
	{                                                                        \
		/* The time of the day (in seconds since start of day). */           \
		uint32_t day_clock = Math::posmod(p_unix_time_val, SECONDS_PER_DAY); \
		/* On x86 these 4 lines can be optimized to only 2 divisions. */     \
		second = day_clock % 60;                                             \
		day_clock /= 60;                                                     \
		minute = day_clock % 60;                                             \
		hour = day_clock / 60;                                               \
	}

#define UNIX_TIME_TO_YMD                                                                    \
	int64_t year;                                                                           \
	Month month;                                                                            \
	uint8_t day;                                                                            \
	/* The day number since Unix epoch (0-index). Days before 1970 are negative. */         \
	int64_t day_number = Math::floor(p_unix_time_val / (double)SECONDS_PER_DAY);            \
	{                                                                                       \
		int64_t day_number_copy = day_number;                                               \
		year = UNIX_EPOCH_YEAR_AD;                                                          \
		uint8_t month_zero_index = 0;                                                       \
		while (day_number_copy >= YEAR_SIZE(year)) {                                        \
			day_number_copy -= YEAR_SIZE(year);                                             \
			year++;                                                                         \
		}                                                                                   \
		while (day_number_copy < 0) {                                                       \
			year--;                                                                         \
			day_number_copy += YEAR_SIZE(year);                                             \
		}                                                                                   \
		/* After the above, day_number now represents the day of the year (0-index). */     \
		while (day_number_copy >= MONTH_DAYS_TABLE[IS_LEAP_YEAR(year)][month_zero_index]) { \
			day_number_copy -= MONTH_DAYS_TABLE[IS_LEAP_YEAR(year)][month_zero_index];      \
			month_zero_index++;                                                             \
		}                                                                                   \
		/* After the above, day_number now represents the day of the month (0-index). */    \
		month = (Month)(month_zero_index + 1);                                              \
		day = day_number_copy + 1;                                                          \
	}

#define VALIDATE_YMDHMS(ret)                                                                                                                                              \
	ERR_FAIL_COND_V_MSG(month == 0, ret, "Invalid month value of: " + itos(month) + ", months are 1-indexed and cannot be 0. See the Time.Month enum for valid values."); \
	ERR_FAIL_COND_V_MSG(month < 0, ret, "Invalid month value of: " + itos(month) + ".");                                                                                  \
	ERR_FAIL_COND_V_MSG(month > 12, ret, "Invalid month value of: " + itos(month) + ". See the Time.Month enum for valid values.");                                       \
	ERR_FAIL_COND_V_MSG(hour > 23, ret, "Invalid hour value of: " + itos(hour) + ".");                                                                                    \
	ERR_FAIL_COND_V_MSG(hour < 0, ret, "Invalid hour value of: " + itos(hour) + ".");                                                                                     \
	ERR_FAIL_COND_V_MSG(minute > 59, ret, "Invalid minute value of: " + itos(minute) + ".");                                                                              \
	ERR_FAIL_COND_V_MSG(minute < 0, ret, "Invalid minute value of: " + itos(minute) + ".");                                                                               \
	ERR_FAIL_COND_V_MSG(second > 59, ret, "Invalid second value of: " + itos(second) + " (leap seconds are not supported).");                                             \
	ERR_FAIL_COND_V_MSG(second < 0, ret, "Invalid second value of: " + itos(second) + ".");                                                                               \
	ERR_FAIL_COND_V_MSG(day == 0, ret, "Invalid day value of: " + itos(day) + ", days are 1-indexed and cannot be 0.");                                                   \
	ERR_FAIL_COND_V_MSG(day < 0, ret, "Invalid day value of: " + itos(day) + ".");                                                                                        \
	/* Do this check after month is tested as valid. */                                                                                                                   \
	uint8_t days_in_this_month = MONTH_DAYS_TABLE[IS_LEAP_YEAR(year)][month - 1];                                                                                         \
	ERR_FAIL_COND_V_MSG(day > days_in_this_month, ret, "Invalid day value of: " + itos(day) + " which is larger than the maximum for this month, " + itos(days_in_this_month) + ".");

#define YMD_TO_DAY_NUMBER                                                           \
	/* The day number since Unix epoch (0-index). Days before 1970 are negative. */ \
	int64_t day_number = day - 1;                                                   \
	/* Add the days in the months to day_number. */                                 \
	for (int i = 0; i < month - 1; i++) {                                           \
		day_number += MONTH_DAYS_TABLE[IS_LEAP_YEAR(year)][i];                      \
	}                                                                               \
	/* Add the days in the years to day_number. */                                  \
	if (year >= UNIX_EPOCH_YEAR_AD) {                                               \
		for (int64_t iyear = UNIX_EPOCH_YEAR_AD; iyear < year; iyear++) {           \
			day_number += YEAR_SIZE(iyear);                                         \
		}                                                                           \
	} else {                                                                        \
		for (int64_t iyear = UNIX_EPOCH_YEAR_AD - 1; iyear >= year; iyear--) {      \
			day_number -= YEAR_SIZE(iyear);                                         \
		}                                                                           \
	}

#define PARSE_ISO8601_STRING(ret)                                                             \
	int64_t year = UNIX_EPOCH_YEAR_AD;                                                        \
	Month month = MONTH_JANUARY;                                                              \
	int day = 1;                                                                              \
	int hour = 0;                                                                             \
	int minute = 0;                                                                           \
	int second = 0;                                                                           \
	{                                                                                         \
		bool has_date = false, has_time = false;                                              \
		String date, time;                                                                    \
		if (p_datetime.find_char('T') > 0) {                                                  \
			has_date = has_time = true;                                                       \
			PackedStringArray array = p_datetime.split("T");                                  \
			ERR_FAIL_COND_V_MSG(array.size() < 2, ret, "Invalid ISO 8601 date/time string."); \
			date = array[0];                                                                  \
			time = array[1];                                                                  \
		} else if (p_datetime.find_char(' ') > 0) {                                           \
			has_date = has_time = true;                                                       \
			PackedStringArray array = p_datetime.split(" ");                                  \
			ERR_FAIL_COND_V_MSG(array.size() < 2, ret, "Invalid ISO 8601 date/time string."); \
			date = array[0];                                                                  \
			time = array[1];                                                                  \
		} else if (p_datetime.find_char('-', 1) > 0) {                                        \
			has_date = true;                                                                  \
			date = p_datetime;                                                                \
		} else if (p_datetime.find_char(':') > 0) {                                           \
			has_time = true;                                                                  \
			time = p_datetime;                                                                \
		}                                                                                     \
		/* Set the variables from the contents of the string. */                              \
		if (has_date) {                                                                       \
			PackedInt32Array array = date.split_ints("-", false);                             \
			ERR_FAIL_COND_V_MSG(array.size() < 3, ret, "Invalid ISO 8601 date string.");      \
			year = array[0];                                                                  \
			month = (Month)array[1];                                                          \
			day = array[2];                                                                   \
			/* Handle negative years. */                                                      \
			if (p_datetime.find_char('-') == 0) {                                             \
				year *= -1;                                                                   \
			}                                                                                 \
		}                                                                                     \
		if (has_time) {                                                                       \
			PackedInt32Array array = time.split_ints(":", false);                             \
			ERR_FAIL_COND_V_MSG(array.size() < 3, ret, "Invalid ISO 8601 time string.");      \
			hour = array[0];                                                                  \
			minute = array[1];                                                                \
			second = array[2];                                                                \
		}                                                                                     \
	}

#define EXTRACT_FROM_DICTIONARY                                                                   \
	/* Get all time values from the dictionary. If it doesn't exist, set the */                   \
	/* values to the default values for Unix epoch (1970-01-01 00:00:00). */                      \
	int64_t year = p_datetime.has(YEAR_KEY) ? int64_t(p_datetime[YEAR_KEY]) : UNIX_EPOCH_YEAR_AD; \
	Month month = Month((p_datetime.has(MONTH_KEY)) ? int(p_datetime[MONTH_KEY]) : 1);            \
	int day = p_datetime.has(DAY_KEY) ? int(p_datetime[DAY_KEY]) : 1;                             \
	int hour = p_datetime.has(HOUR_KEY) ? int(p_datetime[HOUR_KEY]) : 0;                          \
	int minute = p_datetime.has(MINUTE_KEY) ? int(p_datetime[MINUTE_KEY]) : 0;                    \
	int second = p_datetime.has(SECOND_KEY) ? int(p_datetime[SECOND_KEY]) : 0;

Time *Time::singleton = nullptr;

Time *Time::get_singleton() {
	return singleton;
}

Dictionary Time::get_datetime_dict_from_unix_time(int64_t p_unix_time_val) const {
	UNIX_TIME_TO_HMS
	UNIX_TIME_TO_YMD
	Dictionary datetime;
	datetime[YEAR_KEY] = year;
	datetime[MONTH_KEY] = (uint8_t)month;
	datetime[DAY_KEY] = day;
	// Unix epoch was a Thursday (day 0 aka 1970-01-01).
	datetime[WEEKDAY_KEY] = Math::posmod(day_number + WEEKDAY_THURSDAY, 7);
	datetime[HOUR_KEY] = hour;
	datetime[MINUTE_KEY] = minute;
	datetime[SECOND_KEY] = second;

	return datetime;
}

Dictionary Time::get_date_dict_from_unix_time(int64_t p_unix_time_val) const {
	UNIX_TIME_TO_YMD
	Dictionary datetime;
	datetime[YEAR_KEY] = year;
	datetime[MONTH_KEY] = (uint8_t)month;
	datetime[DAY_KEY] = day;
	// Unix epoch was a Thursday (day 0 aka 1970-01-01).
	datetime[WEEKDAY_KEY] = Math::posmod(day_number + WEEKDAY_THURSDAY, 7);

	return datetime;
}

Dictionary Time::get_time_dict_from_unix_time(int64_t p_unix_time_val) const {
	UNIX_TIME_TO_HMS
	Dictionary datetime;
	datetime[HOUR_KEY] = hour;
	datetime[MINUTE_KEY] = minute;
	datetime[SECOND_KEY] = second;

	return datetime;
}

String Time::get_datetime_string_from_unix_time(int64_t p_unix_time_val, bool p_use_space) const {
	UNIX_TIME_TO_HMS
	UNIX_TIME_TO_YMD
	// vformat only supports up to 6 arguments, so we need to split this up into 2 parts.
	String timestamp = vformat("%04d-%02d-%02d", year, (uint8_t)month, day);
	if (p_use_space) {
		timestamp = vformat("%s %02d:%02d:%02d", timestamp, hour, minute, second);
	} else {
		timestamp = vformat("%sT%02d:%02d:%02d", timestamp, hour, minute, second);
	}

	return timestamp;
}

String Time::get_date_string_from_unix_time(int64_t p_unix_time_val) const {
	UNIX_TIME_TO_YMD
	// Android is picky about the types passed to make Variant, so we need a cast.
	return vformat("%04d-%02d-%02d", year, (uint8_t)month, day);
}

String Time::get_time_string_from_unix_time(int64_t p_unix_time_val) const {
	UNIX_TIME_TO_HMS
	return vformat("%02d:%02d:%02d", hour, minute, second);
}

Dictionary Time::get_datetime_dict_from_datetime_string(const String &p_datetime, bool p_weekday) const {
	PARSE_ISO8601_STRING(Dictionary())
	Dictionary dict;
	dict[YEAR_KEY] = year;
	dict[MONTH_KEY] = (uint8_t)month;
	dict[DAY_KEY] = day;
	if (p_weekday) {
		YMD_TO_DAY_NUMBER
		// Unix epoch was a Thursday (day 0 aka 1970-01-01).
		dict[WEEKDAY_KEY] = Math::posmod(day_number + WEEKDAY_THURSDAY, 7);
	}
	dict[HOUR_KEY] = hour;
	dict[MINUTE_KEY] = minute;
	dict[SECOND_KEY] = second;

	return dict;
}

String Time::get_datetime_string_from_datetime_dict(const Dictionary &p_datetime, bool p_use_space) const {
	ERR_FAIL_COND_V_MSG(p_datetime.is_empty(), "", "Invalid datetime Dictionary: Dictionary is empty.");
	EXTRACT_FROM_DICTIONARY
	VALIDATE_YMDHMS("")
	// vformat only supports up to 6 arguments, so we need to split this up into 2 parts.
	String timestamp = vformat("%04d-%02d-%02d", year, (uint8_t)month, day);
	if (p_use_space) {
		timestamp = vformat("%s %02d:%02d:%02d", timestamp, hour, minute, second);
	} else {
		timestamp = vformat("%sT%02d:%02d:%02d", timestamp, hour, minute, second);
	}
	return timestamp;
}


Dictionary Time::get_date_dict_from_date_string(const String &p_date, bool p_weekday) const {
	PARSE_ISO8601_STRING(Dictionary())
	Dictionary dict;
	dict[YEAR_KEY] = year;
	dict[MONTH_KEY] = (uint8_t)month;
	dict[DAY_KEY] = day;
	if (p_weekday) {
		YMD_TO_DAY_NUMBER
		// Unix epoch was a Thursday (day 0 aka 1970-01-01).
		dict[WEEKDAY_KEY] = Math::posmod(day_number + WEEKDAY_THURSDAY, 7);
	}

	return dict;
}

int64_t Time::get_unix_time_from_datetime_dict(const Dictionary &p_datetime) const {
	ERR_FAIL_COND_V_MSG(p_datetime.is_empty(), 0, "Invalid datetime Dictionary: Dictionary is empty");
	EXTRACT_FROM_DICTIONARY
	VALIDATE_YMDHMS(0)
	YMD_TO_DAY_NUMBER
	return day_number * SECONDS_PER_DAY + hour * 3600 + minute * 60 + second;
}

int64_t Time::get_unix_time_from_datetime_string(const String &p_datetime) const {
	PARSE_ISO8601_STRING(-1)
	VALIDATE_YMDHMS(0)
	YMD_TO_DAY_NUMBER
	return day_number * SECONDS_PER_DAY + hour * 3600 + minute * 60 + second;
}

String Time::get_offset_string_from_offset_minutes(int64_t p_offset_minutes) const {
	String sign;
	if (p_offset_minutes < 0) {
		sign = "-";
		p_offset_minutes = -p_offset_minutes;
	} else {
		sign = "+";
	}
	// These two lines can be optimized to one instruction on x86 and others.
	// Note that % is acceptable here only because we ensure it's positive.
	int64_t offset_hours = p_offset_minutes / 60;
	int64_t offset_minutes = p_offset_minutes % 60;
	return vformat("%s%02d:%02d", sign, offset_hours, offset_minutes);
}

Dictionary Time::get_datetime_dict_from_system(bool p_utc) const {
	OS::DateTime dt = OS::get_singleton()->get_datetime(p_utc);
	Dictionary datetime;
	datetime[YEAR_KEY] = dt.year;
	datetime[MONTH_KEY] = (uint8_t)dt.month;
	datetime[DAY_KEY] = dt.day;
	datetime[WEEKDAY_KEY] = (uint8_t)dt.weekday;
	datetime[HOUR_KEY] = dt.hour;
	datetime[MINUTE_KEY] = dt.minute;
	datetime[SECOND_KEY] = dt.second;
	datetime[DST_KEY] = dt.dst;
	return datetime;
}

Dictionary Time::get_date_dict_from_system(bool p_utc) const {
	OS::DateTime dt = OS::get_singleton()->get_datetime(p_utc);
	Dictionary date_dictionary;
	date_dictionary[YEAR_KEY] = dt.year;
	date_dictionary[MONTH_KEY] = (uint8_t)dt.month;
	date_dictionary[DAY_KEY] = dt.day;
	date_dictionary[WEEKDAY_KEY] = (uint8_t)dt.weekday;
	return date_dictionary;
}

Dictionary Time::get_time_dict_from_system(bool p_utc) const {
	OS::DateTime dt = OS::get_singleton()->get_datetime(p_utc);
	Dictionary time_dictionary;
	time_dictionary[HOUR_KEY] = dt.hour;
	time_dictionary[MINUTE_KEY] = dt.minute;
	time_dictionary[SECOND_KEY] = dt.second;
	return time_dictionary;
}

String Time::get_datetime_string_from_system(bool p_utc, bool p_use_space) const {
	OS::DateTime dt = OS::get_singleton()->get_datetime(p_utc);
	// vformat only supports up to 6 arguments, so we need to split this up into 2 parts.
	String timestamp = vformat("%04d-%02d-%02d", dt.year, (uint8_t)dt.month, dt.day);
	if (p_use_space) {
		timestamp = vformat("%s %02d:%02d:%02d", timestamp, dt.hour, dt.minute, dt.second);
	} else {
		timestamp = vformat("%sT%02d:%02d:%02d", timestamp, dt.hour, dt.minute, dt.second);
	}

	return timestamp;
}

String Time::get_date_string_from_system(bool p_utc) const {
	OS::DateTime dt = OS::get_singleton()->get_datetime(p_utc);
	// Android is picky about the types passed to make Variant, so we need a cast.
	return vformat("%04d-%02d-%02d", dt.year, (uint8_t)dt.month, dt.day);
}

String Time::get_time_string_from_system(bool p_utc) const {
	OS::DateTime dt = OS::get_singleton()->get_datetime(p_utc);
	return vformat("%02d:%02d:%02d", dt.hour, dt.minute, dt.second);
}

Dictionary Time::get_time_zone_from_system() const {
	OS::TimeZoneInfo info = OS::get_singleton()->get_time_zone_info();
	Dictionary ret_timezone;
	ret_timezone["bias"] = info.bias;
	ret_timezone["name"] = info.name;
	return ret_timezone;
}

double Time::get_unix_time_from_system() const {
	return OS::get_singleton()->get_unix_time();
}

uint64_t Time::get_ticks_msec() const {
	return OS::get_singleton()->get_ticks_msec();
}

uint64_t Time::get_ticks_usec() const {
	return OS::get_singleton()->get_ticks_usec();
}

void Time::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_datetime_dict_from_unix_time", "unix_time_val"), &Time::get_datetime_dict_from_unix_time);
	ClassDB::bind_method(D_METHOD("get_date_dict_from_unix_time", "unix_time_val"), &Time::get_date_dict_from_unix_time);
	ClassDB::bind_method(D_METHOD("get_time_dict_from_unix_time", "unix_time_val"), &Time::get_time_dict_from_unix_time);
	ClassDB::bind_method(D_METHOD("get_datetime_string_from_unix_time", "unix_time_val", "use_space"), &Time::get_datetime_string_from_unix_time, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_date_string_from_unix_time", "unix_time_val"), &Time::get_date_string_from_unix_time);
	ClassDB::bind_method(D_METHOD("get_time_string_from_unix_time", "unix_time_val"), &Time::get_time_string_from_unix_time);
	ClassDB::bind_method(D_METHOD("get_datetime_dict_from_datetime_string", "datetime", "weekday"), &Time::get_datetime_dict_from_datetime_string);
	ClassDB::bind_method(D_METHOD("get_datetime_string_from_datetime_dict", "datetime", "use_space"), &Time::get_datetime_string_from_datetime_dict);
	ClassDB::bind_method(D_METHOD("get_date_dict_from_date_string", "date", "weekday"), &Time::get_date_dict_from_date_string);
	ClassDB::bind_method(D_METHOD("get_unix_time_from_datetime_dict", "datetime"), &Time::get_unix_time_from_datetime_dict);
	ClassDB::bind_method(D_METHOD("get_unix_time_from_datetime_string", "datetime"), &Time::get_unix_time_from_datetime_string);
	ClassDB::bind_method(D_METHOD("get_offset_string_from_offset_minutes", "offset_minutes"), &Time::get_offset_string_from_offset_minutes);

	ClassDB::bind_method(D_METHOD("get_datetime_dict_from_system", "utc"), &Time::get_datetime_dict_from_system, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_date_dict_from_system", "utc"), &Time::get_date_dict_from_system, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_time_dict_from_system", "utc"), &Time::get_time_dict_from_system, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_datetime_string_from_system", "utc", "use_space"), &Time::get_datetime_string_from_system, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_date_string_from_system", "utc"), &Time::get_date_string_from_system, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_time_string_from_system", "utc"), &Time::get_time_string_from_system, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_time_zone_from_system"), &Time::get_time_zone_from_system);
	ClassDB::bind_method(D_METHOD("get_unix_time_from_system"), &Time::get_unix_time_from_system);
	ClassDB::bind_method(D_METHOD("get_ticks_msec"), &Time::get_ticks_msec);
	ClassDB::bind_method(D_METHOD("get_ticks_usec"), &Time::get_ticks_usec);

	BIND_ENUM_CONSTANT(MONTH_JANUARY);
	BIND_ENUM_CONSTANT(MONTH_FEBRUARY);
	BIND_ENUM_CONSTANT(MONTH_MARCH);
	BIND_ENUM_CONSTANT(MONTH_APRIL);
	BIND_ENUM_CONSTANT(MONTH_MAY);
	BIND_ENUM_CONSTANT(MONTH_JUNE);
	BIND_ENUM_CONSTANT(MONTH_JULY);
	BIND_ENUM_CONSTANT(MONTH_AUGUST);
	BIND_ENUM_CONSTANT(MONTH_SEPTEMBER);
	BIND_ENUM_CONSTANT(MONTH_OCTOBER);
	BIND_ENUM_CONSTANT(MONTH_NOVEMBER);
	BIND_ENUM_CONSTANT(MONTH_DECEMBER);

	BIND_ENUM_CONSTANT(WEEKDAY_SUNDAY);
	BIND_ENUM_CONSTANT(WEEKDAY_MONDAY);
	BIND_ENUM_CONSTANT(WEEKDAY_TUESDAY);
	BIND_ENUM_CONSTANT(WEEKDAY_WEDNESDAY);
	BIND_ENUM_CONSTANT(WEEKDAY_THURSDAY);
	BIND_ENUM_CONSTANT(WEEKDAY_FRIDAY);
	BIND_ENUM_CONSTANT(WEEKDAY_SATURDAY);
}

Time::Time() {
	ERR_FAIL_COND_MSG(singleton, "Singleton for Time already exists.");
	singleton = this;
}

Time::~Time() {
	singleton = nullptr;
}
