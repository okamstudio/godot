/**************************************************************************/
/*  variant_utility.h                                                     */
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

#include "variant.h"

struct VariantUtilityFunctions {
	// Math
	static double sin(double arg);
	static double cos(double arg);
	static double tan(double arg);
	static double sinh(double arg);
	static double cosh(double arg);
	static double tanh(double arg);
	static double asin(double arg);
	static double acos(double arg);
	static double atan(double arg);
	static double atan2(double y, double x);
	static double asinh(double arg);
	static double acosh(double arg);
	static double atanh(double arg);
	static double sqrt(double x);
	static double fmod(double b, double r);
	static double fposmod(double b, double r);
	static int64_t posmod(int64_t b, int64_t r);
	static Variant floor(const Variant &x, Callable::CallError &r_error);
	static double floorf(double x);
	static int64_t floori(double x);
	static Variant ceil(const Variant &x, Callable::CallError &r_error);
	static double ceilf(double x);
	static int64_t ceili(double x);
	static Variant round(const Variant &x, Callable::CallError &r_error);
	static double roundf(double x);
	static int64_t roundi(double x);
	static Variant abs(const Variant &x, Callable::CallError &r_error);
	static double absf(double x);
	static int64_t absi(int64_t x);
	static Variant sign(const Variant &x, Callable::CallError &r_error);
	static double signf(double x);
	static int64_t signi(int64_t x);
	static double pow(double x, double y);
	static double log(double x);
	static double exp(double x);
	static bool is_nan(double x);
	static bool is_inf(double x);
	static bool is_equal_approx(double x, double y);
	static bool is_zero_approx(double x);
	static bool is_finite(double x);
	static double ease(float x, float curve);
	static int step_decimals(float step);
	static Variant snapped(const Variant &x, const Variant &step, Callable::CallError &r_error);
	static double snappedf(double x, double step);
	static int64_t snappedi(double x, int64_t step);
	static Variant lerp(const Variant &from, const Variant &to, double weight, Callable::CallError &r_error);
	static double lerpf(double from, double to, double weight);
	static double cubic_interpolate(double from, double to, double pre, double post, double weight);
	static double cubic_interpolate_angle(double from, double to, double pre, double post, double weight);
	static double cubic_interpolate_in_time(double from, double to, double pre, double post, double weight,
			double to_t, double pre_t, double post_t);
	static double cubic_interpolate_angle_in_time(double from, double to, double pre, double post, double weight,
			double to_t, double pre_t, double post_t);
	static double bezier_interpolate(double p_start, double p_control_1, double p_control_2, double p_end, double p_t);
	static double bezier_derivative(double p_start, double p_control_1, double p_control_2, double p_end, double p_t);
	static double angle_difference(double from, double to);
	static double lerp_angle(double from, double to, double weight);
	static double inverse_lerp(double from, double to, double weight);
	static double remap(double value, double istart, double istop, double ostart, double ostop);
	static double smoothstep(double from, double to, double val);
	static double move_toward(double from, double to, double delta);
	static double rotate_toward(double from, double to, double delta);
	static double deg_to_rad(double angle_deg);
	static double rad_to_deg(double angle_rad);
	static double linear_to_db(double linear);
	static double db_to_linear(double db);
	static Variant wrap(const Variant &p_x, const Variant &p_min, const Variant &p_max, Callable::CallError &r_error);
	static int64_t wrapi(int64_t value, int64_t min, int64_t max);
	static double wrapf(double value, double min, double max);
	static double pingpong(double value, double length);
	static Variant max(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	static double maxf(double x, double y);
	static int64_t maxi(int64_t x, int64_t y);
	static Variant min(const Variant **p_args, int p_argcount, Callable::CallError &r_error);
	static double minf(double x, double y);
	static int64_t mini(int64_t x, int64_t y);
	static Variant clamp(const Variant &x, const Variant &min, const Variant &max, Callable::CallError &r_error);
	static double clampf(double x, double min, double max);
	static int64_t clampi(int64_t x, int64_t min, int64_t max);
	static int64_t nearest_po2(int64_t x);
	// Random
	static void randomize();
	static int64_t randi();
	static double randf();
	static double randfn(double mean, double deviation);
	static int64_t randi_range(int64_t from, int64_t to);
	static double randf_range(double from, double to);
	static void seed(int64_t s);
	static PackedInt64Array rand_from_seed(int64_t seed);
	// Utility
	static Variant weakref(const Variant &obj, Callable::CallError &r_error);
	static int64_t _typeof(const Variant &obj);
	static Variant type_convert(const Variant &p_variant, const Variant::Type p_type);
	static String str(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static String error_string(Error error);
	static String type_string(Variant::Type p_type);
	static void print(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void print_rich(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void _print_verbose(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void printerr(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void printt(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void prints(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void printraw(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void push_error(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static void push_warning(const Variant **p_args, int p_arg_count, Callable::CallError &r_error);
	static String var_to_str(const Variant &p_var);
	static Variant str_to_var(const String &p_var);
	static PackedByteArray var_to_bytes(const Variant &p_var);
	static PackedByteArray var_to_bytes_with_objects(const Variant &p_var);
	static Variant bytes_to_var(const PackedByteArray &p_arr);
	static Variant bytes_to_var_with_objects(const PackedByteArray &p_arr);
	static int64_t hash(const Variant &p_arr);
	static Object *instance_from_id(int64_t p_id);
	static bool is_instance_id_valid(int64_t p_id);
	static bool is_instance_valid(const Variant &p_instance);
	static uint64_t rid_allocate_id();
	static RID rid_from_int64(uint64_t p_base);
	static bool is_same(const Variant &p_a, const Variant &p_b);
};
