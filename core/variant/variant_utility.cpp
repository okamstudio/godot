/**************************************************************************/
/*  variant_utility.cpp                                                   */
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

#include "variant_utility.h"

#include "core/io/marshalls.h"
#include "core/object/ref_counted.h"
#include "core/os/os.h"
#include "core/templates/oa_hash_map.h"
#include "core/templates/rid.h"
#include "core/templates/rid_owner.h"
#include "core/variant/binder_common.h"
#include "core/variant/variant_parser.h"

// Math
double VariantUtilityFunctions::sin(double arg) {
	return Math::sin(arg);
}

double VariantUtilityFunctions::cos(double arg) {
	return Math::cos(arg);
}

double VariantUtilityFunctions::tan(double arg) {
	return Math::tan(arg);
}

double VariantUtilityFunctions::sinh(double arg) {
	return Math::sinh(arg);
}

double VariantUtilityFunctions::cosh(double arg) {
	return Math::cosh(arg);
}

double VariantUtilityFunctions::tanh(double arg) {
	return Math::tanh(arg);
}

double VariantUtilityFunctions::asin(double arg) {
	return Math::asin(arg);
}

double VariantUtilityFunctions::acos(double arg) {
	return Math::acos(arg);
}

double VariantUtilityFunctions::atan(double arg) {
	return Math::atan(arg);
}

double VariantUtilityFunctions::atan2(double y, double x) {
	return Math::atan2(y, x);
}

double VariantUtilityFunctions::asinh(double arg) {
	return Math::asinh(arg);
}

double VariantUtilityFunctions::acosh(double arg) {
	return Math::acosh(arg);
}

double VariantUtilityFunctions::atanh(double arg) {
	return Math::atanh(arg);
}

double VariantUtilityFunctions::sqrt(double x) {
	return Math::sqrt(x);
}

double VariantUtilityFunctions::fmod(double b, double r) {
	return Math::fmod(b, r);
}

double VariantUtilityFunctions::fposmod(double b, double r) {
	return Math::fposmod(b, r);
}

int64_t VariantUtilityFunctions::posmod(int64_t b, int64_t r) {
	return Math::posmod(b, r);
}

Variant VariantUtilityFunctions::floor(const Variant &x, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return VariantInternalAccessor<int64_t>::get(&x);
		} break;
		case Variant::FLOAT: {
			return Math::floor(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).floor();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).floor();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).floor();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x);
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::floorf(double x) {
	return Math::floor(x);
}

int64_t VariantUtilityFunctions::floori(double x) {
	return int64_t(Math::floor(x));
}

Variant VariantUtilityFunctions::ceil(const Variant &x, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return VariantInternalAccessor<int64_t>::get(&x);
		} break;
		case Variant::FLOAT: {
			return Math::ceil(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).ceil();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).ceil();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).ceil();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x);
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::ceilf(double x) {
	return Math::ceil(x);
}

int64_t VariantUtilityFunctions::ceili(double x) {
	return int64_t(Math::ceil(x));
}

Variant VariantUtilityFunctions::round(const Variant &x, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return VariantInternalAccessor<int64_t>::get(&x);
		} break;
		case Variant::FLOAT: {
			return Math::round(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).round();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).round();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).round();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x);
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::roundf(double x) {
	return Math::round(x);
}

int64_t VariantUtilityFunctions::roundi(double x) {
	return int64_t(Math::round(x));
}

Variant VariantUtilityFunctions::abs(const Variant &x, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return ABS(VariantInternalAccessor<int64_t>::get(&x));
		} break;
		case Variant::FLOAT: {
			return Math::absd(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).abs();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x).abs();
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).abs();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x).abs();
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).abs();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x).abs();
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::absf(double x) {
	return Math::absd(x);
}

int64_t VariantUtilityFunctions::absi(int64_t x) {
	return ABS(x);
}

Variant VariantUtilityFunctions::sign(const Variant &x, Callable::CallError &r_error) {
	r_error.error = Callable::CallError::CALL_OK;
	switch (x.get_type()) {
		case Variant::INT: {
			return SIGN(VariantInternalAccessor<int64_t>::get(&x));
		} break;
		case Variant::FLOAT: {
			return SIGN(VariantInternalAccessor<double>::get(&x));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).sign();
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x).sign();
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).sign();
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x).sign();
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).sign();
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x).sign();
		} break;
		default: {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
		} break;
	}
}

double VariantUtilityFunctions::signf(double x) {
	return SIGN(x);
}

int64_t VariantUtilityFunctions::signi(int64_t x) {
	return SIGN(x);
}

double VariantUtilityFunctions::pow(double x, double y) {
	return Math::pow(x, y);
}

double VariantUtilityFunctions::log(double x) {
	return Math::log(x);
}

double VariantUtilityFunctions::exp(double x) {
	return Math::exp(x);
}

bool VariantUtilityFunctions::is_nan(double x) {
	return Math::is_nan(x);
}

bool VariantUtilityFunctions::is_inf(double x) {
	return Math::is_inf(x);
}

bool VariantUtilityFunctions::is_equal_approx(double x, double y) {
	return Math::is_equal_approx(x, y);
}

bool VariantUtilityFunctions::is_zero_approx(double x) {
	return Math::is_zero_approx(x);
}

bool VariantUtilityFunctions::is_finite(double x) {
	return Math::is_finite(x);
}

double VariantUtilityFunctions::ease(float x, float curve) {
	return Math::ease(x, curve);
}

int VariantUtilityFunctions::step_decimals(float step) {
	return Math::step_decimals(step);
}

Variant VariantUtilityFunctions::snapped(const Variant &x, const Variant &step, Callable::CallError &r_error) {
	switch (x.get_type()) {
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::VECTOR2:
		case Variant::VECTOR2I:
		case Variant::VECTOR3:
		case Variant::VECTOR3I:
		case Variant::VECTOR4:
		case Variant::VECTOR4I:
			break;
		default:
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "x" must be "int", "float", "Vector2", "Vector2i", "Vector3", "Vector3i", "Vector4", or "Vector4i".)";
	}

	if (x.get_type() != step.get_type()) {
		if (x.get_type() == Variant::INT || x.get_type() == Variant::FLOAT) {
			if (step.get_type() != Variant::INT && step.get_type() != Variant::FLOAT) {
				r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
				r_error.argument = 1;
				r_error.expected = Variant::NIL;
				return R"(Argument "step" must be "int" or "float".)";
			}
		} else {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 1;
			r_error.expected = x.get_type();
			return Variant();
		}
	}

	r_error.error = Callable::CallError::CALL_OK;
	switch (step.get_type()) {
		case Variant::INT: {
			return snappedi(x, VariantInternalAccessor<int64_t>::get(&step));
		} break;
		case Variant::FLOAT: {
			return snappedf(x, VariantInternalAccessor<double>::get(&step));
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&x).snapped(VariantInternalAccessor<Vector2>::get(&step));
		} break;
		case Variant::VECTOR2I: {
			return VariantInternalAccessor<Vector2i>::get(&x).snapped(VariantInternalAccessor<Vector2i>::get(&step));
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&x).snapped(VariantInternalAccessor<Vector3>::get(&step));
		} break;
		case Variant::VECTOR3I: {
			return VariantInternalAccessor<Vector3i>::get(&x).snapped(VariantInternalAccessor<Vector3i>::get(&step));
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&x).snapped(VariantInternalAccessor<Vector4>::get(&step));
		} break;
		case Variant::VECTOR4I: {
			return VariantInternalAccessor<Vector4i>::get(&x).snapped(VariantInternalAccessor<Vector4i>::get(&step));
		} break;
		default: {
			return Variant(); // Already handled.
		} break;
	}
}

double VariantUtilityFunctions::snappedf(double x, double step) {
	return Math::snapped(x, step);
}

int64_t VariantUtilityFunctions::snappedi(double x, int64_t step) {
	return Math::snapped(x, step);
}

Variant VariantUtilityFunctions::lerp(const Variant &from, const Variant &to, double weight, Callable::CallError &r_error) {
	switch (from.get_type()) {
		case Variant::INT:
		case Variant::FLOAT:
		case Variant::VECTOR2:
		case Variant::VECTOR3:
		case Variant::VECTOR4:
		case Variant::QUATERNION:
		case Variant::BASIS:
		case Variant::COLOR:
		case Variant::TRANSFORM2D:
		case Variant::TRANSFORM3D:
			break;
		default:
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = 0;
			r_error.expected = Variant::NIL;
			return R"(Argument "from" must be "int", "float", "Vector2", "Vector3", "Vector4", "Color", "Quaternion", "Basis", "Transform2D", or "Transform3D".)";
	}

	if (from.get_type() != to.get_type()) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = from.get_type();
		return Variant();
	}

	r_error.error = Callable::CallError::CALL_OK;
	switch (from.get_type()) {
		case Variant::INT: {
			return lerpf(VariantInternalAccessor<int64_t>::get(&from), to, weight);
		} break;
		case Variant::FLOAT: {
			return lerpf(VariantInternalAccessor<double>::get(&from), to, weight);
		} break;
		case Variant::VECTOR2: {
			return VariantInternalAccessor<Vector2>::get(&from).lerp(VariantInternalAccessor<Vector2>::get(&to), weight);
		} break;
		case Variant::VECTOR3: {
			return VariantInternalAccessor<Vector3>::get(&from).lerp(VariantInternalAccessor<Vector3>::get(&to), weight);
		} break;
		case Variant::VECTOR4: {
			return VariantInternalAccessor<Vector4>::get(&from).lerp(VariantInternalAccessor<Vector4>::get(&to), weight);
		} break;
		case Variant::QUATERNION: {
			return VariantInternalAccessor<Quaternion>::get(&from).slerp(VariantInternalAccessor<Quaternion>::get(&to), weight);
		} break;
		case Variant::BASIS: {
			return VariantInternalAccessor<Basis>::get(&from).slerp(VariantInternalAccessor<Basis>::get(&to), weight);
		} break;
		case Variant::TRANSFORM2D: {
			return VariantInternalAccessor<Transform2D>::get(&from).interpolate_with(VariantInternalAccessor<Transform2D>::get(&to), weight);
		} break;
		case Variant::TRANSFORM3D: {
			return VariantInternalAccessor<Transform3D>::get(&from).interpolate_with(VariantInternalAccessor<Transform3D>::get(&to), weight);
		} break;
		case Variant::COLOR: {
			return VariantInternalAccessor<Color>::get(&from).lerp(VariantInternalAccessor<Color>::get(&to), weight);
		} break;
		default: {
			return Variant(); // Already handled.
		} break;
	}
}

double VariantUtilityFunctions::lerpf(double from, double to, double weight) {
	return Math::lerp(from, to, weight);
}

double VariantUtilityFunctions::cubic_interpolate(double from, double to, double pre, double post, double weight) {
	return Math::cubic_interpolate(from, to, pre, post, weight);
}

double VariantUtilityFunctions::cubic_interpolate_angle(double from, double to, double pre, double post, double weight) {
	return Math::cubic_interpolate_angle(from, to, pre, post, weight);
}

double VariantUtilityFunctions::cubic_interpolate_in_time(double from, double to, double pre, double post, double weight,
		double to_t, double pre_t, double post_t) {
	return Math::cubic_interpolate_in_time(from, to, pre, post, weight, to_t, pre_t, post_t);
}

double VariantUtilityFunctions::cubic_interpolate_angle_in_time(double from, double to, double pre, double post, double weight,
		double to_t, double pre_t, double post_t) {
	return Math::cubic_interpolate_angle_in_time(from, to, pre, post, weight, to_t, pre_t, post_t);
}

double VariantUtilityFunctions::bezier_interpolate(double p_start, double p_control_1, double p_control_2, double p_end, double p_t) {
	return Math::bezier_interpolate(p_start, p_control_1, p_control_2, p_end, p_t);
}

double VariantUtilityFunctions::bezier_derivative(double p_start, double p_control_1, double p_control_2, double p_end, double p_t) {
	return Math::bezier_derivative(p_start, p_control_1, p_control_2, p_end, p_t);
}

double VariantUtilityFunctions::angle_difference(double from, double to) {
	return Math::angle_difference(from, to);
}

double VariantUtilityFunctions::lerp_angle(double from, double to, double weight) {
	return Math::lerp_angle(from, to, weight);
}

double VariantUtilityFunctions::inverse_lerp(double from, double to, double weight) {
	return Math::inverse_lerp(from, to, weight);
}

double VariantUtilityFunctions::remap(double value, double istart, double istop, double ostart, double ostop) {
	return Math::remap(value, istart, istop, ostart, ostop);
}

double VariantUtilityFunctions::smoothstep(double from, double to, double val) {
	return Math::smoothstep(from, to, val);
}

double VariantUtilityFunctions::move_toward(double from, double to, double delta) {
	return Math::move_toward(from, to, delta);
}

double VariantUtilityFunctions::rotate_toward(double from, double to, double delta) {
	return Math::rotate_toward(from, to, delta);
}

double VariantUtilityFunctions::deg_to_rad(double angle_deg) {
	return Math::deg_to_rad(angle_deg);
}

double VariantUtilityFunctions::rad_to_deg(double angle_rad) {
	return Math::rad_to_deg(angle_rad);
}

double VariantUtilityFunctions::linear_to_db(double linear) {
	return Math::linear_to_db(linear);
}

double VariantUtilityFunctions::db_to_linear(double db) {
	return Math::db_to_linear(db);
}

Variant VariantUtilityFunctions::wrap(const Variant &p_x, const Variant &p_min, const Variant &p_max, Callable::CallError &r_error) {
	Variant::Type x_type = p_x.get_type();
	if (x_type != Variant::INT && x_type != Variant::FLOAT) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::FLOAT;
		return Variant();
	}

	Variant::Type min_type = p_min.get_type();
	if (min_type != Variant::INT && min_type != Variant::FLOAT) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = x_type;
		return Variant();
	}

	Variant::Type max_type = p_max.get_type();
	if (max_type != Variant::INT && max_type != Variant::FLOAT) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 2;
		r_error.expected = x_type;
		return Variant();
	}

	Variant value;

	switch (x_type) {
		case Variant::INT: {
			if (x_type != min_type || x_type != max_type) {
				value = wrapf((double)p_x, (double)p_min, (double)p_max);
			} else {
				value = wrapi((int)p_x, (int)p_min, (int)p_max);
			}
		} break;
		case Variant::FLOAT: {
			value = wrapf((double)p_x, (double)p_min, (double)p_max);
		} break;
		default:
			break;
	}

	r_error.error = Callable::CallError::CALL_OK;
	return value;
}

int64_t VariantUtilityFunctions::wrapi(int64_t value, int64_t min, int64_t max) {
	return Math::wrapi(value, min, max);
}

double VariantUtilityFunctions::wrapf(double value, double min, double max) {
	return Math::wrapf(value, min, max);
}

double VariantUtilityFunctions::pingpong(double value, double length) {
	return Math::pingpong(value, length);
}

Variant VariantUtilityFunctions::max(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 2) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 2;
		return Variant();
	}
	Variant base = *p_args[0];
	Variant ret;

	for (int i = 0; i < p_argcount; i++) {
		Variant::Type arg_type = p_args[i]->get_type();
		if (arg_type != Variant::INT && arg_type != Variant::FLOAT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = Variant::FLOAT;
			return Variant();
		}
		if (i == 0) {
			continue;
		}
		bool valid;
		Variant::evaluate(Variant::OP_LESS, base, *p_args[i], ret, valid);
		if (!valid) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = base.get_type();
			return Variant();
		}
		if (ret.booleanize()) {
			base = *p_args[i];
		}
	}
	r_error.error = Callable::CallError::CALL_OK;
	return base;
}

double VariantUtilityFunctions::maxf(double x, double y) {
	return MAX(x, y);
}

int64_t VariantUtilityFunctions::maxi(int64_t x, int64_t y) {
	return MAX(x, y);
}

Variant VariantUtilityFunctions::min(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	if (p_argcount < 2) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 2;
		return Variant();
	}
	Variant base = *p_args[0];
	Variant ret;

	for (int i = 0; i < p_argcount; i++) {
		Variant::Type arg_type = p_args[i]->get_type();
		if (arg_type != Variant::INT && arg_type != Variant::FLOAT) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = Variant::FLOAT;
			return Variant();
		}
		if (i == 0) {
			continue;
		}
		bool valid;
		Variant::evaluate(Variant::OP_GREATER, base, *p_args[i], ret, valid);
		if (!valid) {
			r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
			r_error.argument = i;
			r_error.expected = base.get_type();
			return Variant();
		}
		if (ret.booleanize()) {
			base = *p_args[i];
		}
	}
	r_error.error = Callable::CallError::CALL_OK;
	return base;
}

double VariantUtilityFunctions::minf(double x, double y) {
	return MIN(x, y);
}

int64_t VariantUtilityFunctions::mini(int64_t x, int64_t y) {
	return MIN(x, y);
}

Variant VariantUtilityFunctions::clamp(const Variant &x, const Variant &min, const Variant &max, Callable::CallError &r_error) {
	Variant value = x;

	Variant ret;

	bool valid;
	Variant::evaluate(Variant::OP_LESS, value, min, ret, valid);
	if (!valid) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 1;
		r_error.expected = value.get_type();
		return Variant();
	}
	if (ret.booleanize()) {
		value = min;
	}
	Variant::evaluate(Variant::OP_GREATER, value, max, ret, valid);
	if (!valid) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 2;
		r_error.expected = value.get_type();
		return Variant();
	}
	if (ret.booleanize()) {
		value = max;
	}

	r_error.error = Callable::CallError::CALL_OK;

	return value;
}

double VariantUtilityFunctions::clampf(double x, double min, double max) {
	return CLAMP(x, min, max);
}

int64_t VariantUtilityFunctions::clampi(int64_t x, int64_t min, int64_t max) {
	return CLAMP(x, min, max);
}

int64_t VariantUtilityFunctions::nearest_po2(int64_t x) {
	return nearest_power_of_2_templated(uint64_t(x));
}

// Random

void VariantUtilityFunctions::randomize() {
	Math::randomize();
}

int64_t VariantUtilityFunctions::randi() {
	return Math::rand();
}

double VariantUtilityFunctions::randf() {
	return Math::randf();
}

double VariantUtilityFunctions::randfn(double mean, double deviation) {
	return Math::randfn(mean, deviation);
}

int64_t VariantUtilityFunctions::randi_range(int64_t from, int64_t to) {
	return Math::random((int32_t)from, (int32_t)to);
}

double VariantUtilityFunctions::randf_range(double from, double to) {
	return Math::random(from, to);
}

void VariantUtilityFunctions::seed(int64_t s) {
	return Math::seed(s);
}

PackedInt64Array VariantUtilityFunctions::rand_from_seed(int64_t seed) {
	uint64_t s = seed;
	PackedInt64Array arr;
	arr.resize(2);
	arr.write[0] = Math::rand_from_seed(&s);
	arr.write[1] = s;
	return arr;
}

// Utility

Variant VariantUtilityFunctions::weakref(const Variant &obj, Callable::CallError &r_error) {
	if (obj.get_type() == Variant::OBJECT) {
		r_error.error = Callable::CallError::CALL_OK;
		if (obj.is_ref_counted()) {
			Ref<WeakRef> wref = memnew(WeakRef);
			Ref<RefCounted> r = obj;
			if (r.is_valid()) {
				wref->set_ref(r);
			}
			return wref;
		} else {
			Ref<WeakRef> wref = memnew(WeakRef);
			Object *o = obj.get_validated_object();
			if (o) {
				wref->set_obj(o);
			}
			return wref;
		}
	} else if (obj.get_type() == Variant::NIL) {
		r_error.error = Callable::CallError::CALL_OK;
		Ref<WeakRef> wref = memnew(WeakRef);
		return wref;
	} else {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_ARGUMENT;
		r_error.argument = 0;
		r_error.expected = Variant::OBJECT;
		return Variant();
	}
}

int64_t VariantUtilityFunctions::_typeof(const Variant &obj) {
	return obj.get_type();
}

Variant VariantUtilityFunctions::type_convert(const Variant &p_variant, const Variant::Type p_type) {
	switch (p_type) {
		case Variant::Type::NIL:
			return Variant();
		case Variant::Type::BOOL:
			return p_variant.operator bool();
		case Variant::Type::INT:
			return p_variant.operator int64_t();
		case Variant::Type::FLOAT:
			return p_variant.operator double();
		case Variant::Type::STRING:
			return p_variant.operator String();
		case Variant::Type::VECTOR2:
			return p_variant.operator Vector2();
		case Variant::Type::VECTOR2I:
			return p_variant.operator Vector2i();
		case Variant::Type::RECT2:
			return p_variant.operator Rect2();
		case Variant::Type::RECT2I:
			return p_variant.operator Rect2i();
		case Variant::Type::VECTOR3:
			return p_variant.operator Vector3();
		case Variant::Type::VECTOR3I:
			return p_variant.operator Vector3i();
		case Variant::Type::TRANSFORM2D:
			return p_variant.operator Transform2D();
		case Variant::Type::VECTOR4:
			return p_variant.operator Vector4();
		case Variant::Type::VECTOR4I:
			return p_variant.operator Vector4i();
		case Variant::Type::PLANE:
			return p_variant.operator Plane();
		case Variant::Type::QUATERNION:
			return p_variant.operator Quaternion();
		case Variant::Type::AABB:
			return p_variant.operator ::AABB();
		case Variant::Type::BASIS:
			return p_variant.operator Basis();
		case Variant::Type::TRANSFORM3D:
			return p_variant.operator Transform3D();
		case Variant::Type::PROJECTION:
			return p_variant.operator Projection();
		case Variant::Type::COLOR:
			return p_variant.operator Color();
		case Variant::Type::STRING_NAME:
			return p_variant.operator StringName();
		case Variant::Type::NODE_PATH:
			return p_variant.operator NodePath();
		case Variant::Type::RID:
			return p_variant.operator ::RID();
		case Variant::Type::OBJECT:
			return p_variant.operator Object *();
		case Variant::Type::CALLABLE:
			return p_variant.operator Callable();
		case Variant::Type::SIGNAL:
			return p_variant.operator Signal();
		case Variant::Type::DICTIONARY:
			return p_variant.operator Dictionary();
		case Variant::Type::ARRAY:
			return p_variant.operator Array();
		case Variant::Type::PACKED_BYTE_ARRAY:
			return p_variant.operator PackedByteArray();
		case Variant::Type::PACKED_INT32_ARRAY:
			return p_variant.operator PackedInt32Array();
		case Variant::Type::PACKED_INT64_ARRAY:
			return p_variant.operator PackedInt64Array();
		case Variant::Type::PACKED_FLOAT32_ARRAY:
			return p_variant.operator PackedFloat32Array();
		case Variant::Type::PACKED_FLOAT64_ARRAY:
			return p_variant.operator PackedFloat64Array();
		case Variant::Type::PACKED_STRING_ARRAY:
			return p_variant.operator PackedStringArray();
		case Variant::Type::PACKED_VECTOR2_ARRAY:
			return p_variant.operator PackedVector2Array();
		case Variant::Type::PACKED_VECTOR3_ARRAY:
			return p_variant.operator PackedVector3Array();
		case Variant::Type::PACKED_COLOR_ARRAY:
			return p_variant.operator PackedColorArray();
		case Variant::Type::PACKED_VECTOR4_ARRAY:
			return p_variant.operator PackedVector4Array();
		case Variant::Type::VARIANT_MAX:
			ERR_PRINT("Invalid type argument to type_convert(), use the TYPE_* constants. Returning the unconverted Variant.");
	}
	return p_variant;
}

String VariantUtilityFunctions::str(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	if (p_arg_count < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
		return String();
	}
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();

		if (i == 0) {
			s = os;
		} else {
			s += os;
		}
	}

	r_error.error = Callable::CallError::CALL_OK;

	return s;
}

String VariantUtilityFunctions::error_string(Error error) {
	if (error < 0 || error >= ERR_MAX) {
		return String("(invalid error code)");
	}

	return String(error_names[error]);
}

String VariantUtilityFunctions::type_string(Variant::Type p_type) {
	ERR_FAIL_INDEX_V_MSG((int)p_type, (int)Variant::VARIANT_MAX, "<invalid type>", "Invalid type argument to type_string(), use the TYPE_* constants.");
	return Variant::get_type_name(p_type);
}

void VariantUtilityFunctions::print(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();

		if (i == 0) {
			s = os;
		} else {
			s += os;
		}
	}

	print_line(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::print_rich(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();

		if (i == 0) {
			s = os;
		} else {
			s += os;
		}
	}

	print_line_rich(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::_print_verbose(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	if (OS::get_singleton()->is_stdout_verbose()) {
		String s;
		for (int i = 0; i < p_arg_count; i++) {
			String os = p_args[i]->operator String();

			if (i == 0) {
				s = os;
			} else {
				s += os;
			}
		}

		// No need to use `print_verbose()` as this call already only happens
		// when verbose mode is enabled. This avoids performing string argument concatenation
		// when not needed.
		print_line(s);
	}

	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::printerr(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();

		if (i == 0) {
			s = os;
		} else {
			s += os;
		}
	}

	print_error(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::printt(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		if (i) {
			s += "\t";
		}
		s += p_args[i]->operator String();
	}

	print_line(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::prints(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		if (i) {
			s += " ";
		}
		s += p_args[i]->operator String();
	}

	print_line(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::printraw(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();

		if (i == 0) {
			s = os;
		} else {
			s += os;
		}
	}

	OS::get_singleton()->print("%s", s.utf8().get_data());
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::push_error(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	if (p_arg_count < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
	}
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();

		if (i == 0) {
			s = os;
		} else {
			s += os;
		}
	}

	ERR_PRINT(s);
	r_error.error = Callable::CallError::CALL_OK;
}

void VariantUtilityFunctions::push_warning(const Variant **p_args, int p_arg_count, Callable::CallError &r_error) {
	if (p_arg_count < 1) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = 1;
	}
	String s;
	for (int i = 0; i < p_arg_count; i++) {
		String os = p_args[i]->operator String();

		if (i == 0) {
			s = os;
		} else {
			s += os;
		}
	}

	WARN_PRINT(s);
	r_error.error = Callable::CallError::CALL_OK;
}

String VariantUtilityFunctions::var_to_str(const Variant &p_var) {
	String vars;
	VariantWriter::write_to_string(p_var, vars);
	return vars;
}

Variant VariantUtilityFunctions::str_to_var(const String &p_var) {
	VariantParser::StreamString ss;
	ss.s = p_var;

	String errs;
	int line;
	Variant ret;
	(void)VariantParser::parse(&ss, ret, errs, line);

	return ret;
}

PackedByteArray VariantUtilityFunctions::var_to_bytes(const Variant &p_var) {
	int len;
	Error err = encode_variant(p_var, nullptr, len, false);
	if (err != OK) {
		return PackedByteArray();
	}

	PackedByteArray barr;
	barr.resize(len);
	{
		uint8_t *w = barr.ptrw();
		err = encode_variant(p_var, w, len, false);
		if (err != OK) {
			return PackedByteArray();
		}
	}

	return barr;
}

PackedByteArray VariantUtilityFunctions::var_to_bytes_with_objects(const Variant &p_var) {
	int len;
	Error err = encode_variant(p_var, nullptr, len, true);
	if (err != OK) {
		return PackedByteArray();
	}

	PackedByteArray barr;
	barr.resize(len);
	{
		uint8_t *w = barr.ptrw();
		err = encode_variant(p_var, w, len, true);
		if (err != OK) {
			return PackedByteArray();
		}
	}

	return barr;
}

Variant VariantUtilityFunctions::bytes_to_var(const PackedByteArray &p_arr) {
	Variant ret;
	{
		const uint8_t *r = p_arr.ptr();
		Error err = decode_variant(ret, r, p_arr.size(), nullptr, false);
		if (err != OK) {
			return Variant();
		}
	}
	return ret;
}

Variant VariantUtilityFunctions::bytes_to_var_with_objects(const PackedByteArray &p_arr) {
	Variant ret;
	{
		const uint8_t *r = p_arr.ptr();
		Error err = decode_variant(ret, r, p_arr.size(), nullptr, true);
		if (err != OK) {
			return Variant();
		}
	}
	return ret;
}

int64_t VariantUtilityFunctions::hash(const Variant &p_arr) {
	return p_arr.hash();
}

Object *VariantUtilityFunctions::instance_from_id(int64_t p_id) {
	ObjectID id = ObjectID((uint64_t)p_id);
	Object *ret = ObjectDB::get_instance(id);
	return ret;
}

bool VariantUtilityFunctions::is_instance_id_valid(int64_t p_id) {
	return ObjectDB::get_instance(ObjectID((uint64_t)p_id)) != nullptr;
}

bool VariantUtilityFunctions::is_instance_valid(const Variant &p_instance) {
	if (p_instance.get_type() != Variant::OBJECT) {
		return false;
	}
	return p_instance.get_validated_object() != nullptr;
}

uint64_t VariantUtilityFunctions::rid_allocate_id() {
	return RID_AllocBase::_gen_id();
}

RID VariantUtilityFunctions::rid_from_int64(uint64_t p_base) {
	return RID::from_uint64(p_base);
}

bool VariantUtilityFunctions::is_same(const Variant &p_a, const Variant &p_b) {
	return p_a.identity_compare(p_b);
}

#ifdef DEBUG_METHODS_ENABLED
#define VCALLR *ret = p_func(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...)
#define VCALL p_func(VariantCasterAndValidate<P>::cast(p_args, Is, r_error)...)
#else
#define VCALLR *ret = p_func(VariantCaster<P>::cast(*p_args[Is])...)
#define VCALL p_func(VariantCaster<P>::cast(*p_args[Is])...)
#endif

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void call_helperpr(R (*p_func)(P...), Variant *ret, const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;
	VCALLR;
	(void)p_args; // avoid gcc warning
	(void)r_error;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void validated_call_helperpr(R (*p_func)(P...), Variant *ret, const Variant **p_args, IndexSequence<Is...>) {
	*ret = p_func(VariantCaster<P>::cast(*p_args[Is])...);
	(void)p_args;
}

template <typename R, typename... P, size_t... Is>
static _FORCE_INLINE_ void ptr_call_helperpr(R (*p_func)(P...), void *ret, const void **p_args, IndexSequence<Is...>) {
	PtrToArg<R>::encode(p_func(PtrToArg<P>::convert(p_args[Is])...), ret);
	(void)p_args;
}

template <typename R, typename... P>
static _FORCE_INLINE_ void call_helperr(R (*p_func)(P...), Variant *ret, const Variant **p_args, Callable::CallError &r_error) {
	call_helperpr(p_func, ret, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void validated_call_helperr(R (*p_func)(P...), Variant *ret, const Variant **p_args) {
	validated_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ void ptr_call_helperr(R (*p_func)(P...), void *ret, const void **p_args) {
	ptr_call_helperpr(p_func, ret, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename R, typename... P>
static _FORCE_INLINE_ int get_arg_count_helperr(R (*p_func)(P...)) {
	return sizeof...(P);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type get_arg_type_helperr(R (*p_func)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename R, typename... P>
static _FORCE_INLINE_ Variant::Type get_ret_type_helperr(R (*p_func)(P...)) {
	return GetTypeInfo<R>::VARIANT_TYPE;
}

// WITHOUT RET

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void call_helperp(void (*p_func)(P...), const Variant **p_args, Callable::CallError &r_error, IndexSequence<Is...>) {
	r_error.error = Callable::CallError::CALL_OK;
	VCALL;
	(void)p_args;
	(void)r_error;
}

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void validated_call_helperp(void (*p_func)(P...), const Variant **p_args, IndexSequence<Is...>) {
	p_func(VariantCaster<P>::cast(*p_args[Is])...);
	(void)p_args;
}

template <typename... P, size_t... Is>
static _FORCE_INLINE_ void ptr_call_helperp(void (*p_func)(P...), const void **p_args, IndexSequence<Is...>) {
	p_func(PtrToArg<P>::convert(p_args[Is])...);
	(void)p_args;
}

template <typename... P>
static _FORCE_INLINE_ void call_helper(void (*p_func)(P...), const Variant **p_args, Callable::CallError &r_error) {
	call_helperp(p_func, p_args, r_error, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ void validated_call_helper(void (*p_func)(P...), const Variant **p_args) {
	validated_call_helperp(p_func, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ void ptr_call_helper(void (*p_func)(P...), const void **p_args) {
	ptr_call_helperp(p_func, p_args, BuildIndexSequence<sizeof...(P)>{});
}

template <typename... P>
static _FORCE_INLINE_ int get_arg_count_helper(void (*p_func)(P...)) {
	return sizeof...(P);
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type get_arg_type_helper(void (*p_func)(P...), int p_arg) {
	return call_get_argument_type<P...>(p_arg);
}

template <typename... P>
static _FORCE_INLINE_ Variant::Type get_ret_type_helper(void (*p_func)(P...)) {
	return Variant::NIL;
}

#define FUNCBINDR(m_func, m_args, m_category)                                                                    \
	class Func_##m_func {                                                                                        \
	public:                                                                                                      \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) { \
			call_helperr(VariantUtilityFunctions::m_func, r_ret, p_args, r_error);                               \
		}                                                                                                        \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                     \
			validated_call_helperr(VariantUtilityFunctions::m_func, r_ret, p_args);                              \
		}                                                                                                        \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                    \
			ptr_call_helperr(VariantUtilityFunctions::m_func, ret, p_args);                                      \
		}                                                                                                        \
		static int get_argument_count() {                                                                        \
			return get_arg_count_helperr(VariantUtilityFunctions::m_func);                                       \
		}                                                                                                        \
		static Variant::Type get_argument_type(int p_arg) {                                                      \
			return get_arg_type_helperr(VariantUtilityFunctions::m_func, p_arg);                                 \
		}                                                                                                        \
		static Variant::Type get_return_type() {                                                                 \
			return get_ret_type_helperr(VariantUtilityFunctions::m_func);                                        \
		}                                                                                                        \
		static bool has_return_type() {                                                                          \
			return true;                                                                                         \
		}                                                                                                        \
		static bool is_vararg() {                                                                                \
			return false;                                                                                        \
		}                                                                                                        \
		static Variant::UtilityFunctionType get_type() {                                                         \
			return m_category;                                                                                   \
		}                                                                                                        \
	};                                                                                                           \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVR(m_func, m_args, m_category)                                                                          \
	class Func_##m_func {                                                                                               \
	public:                                                                                                             \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {        \
			r_error.error = Callable::CallError::CALL_OK;                                                               \
			*r_ret = VariantUtilityFunctions::m_func(*p_args[0], r_error);                                              \
		}                                                                                                               \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                            \
			Callable::CallError ce;                                                                                     \
			*r_ret = VariantUtilityFunctions::m_func(*p_args[0], ce);                                                   \
		}                                                                                                               \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                           \
			Callable::CallError ce;                                                                                     \
			PtrToArg<Variant>::encode(VariantUtilityFunctions::m_func(PtrToArg<Variant>::convert(p_args[0]), ce), ret); \
		}                                                                                                               \
		static int get_argument_count() {                                                                               \
			return 1;                                                                                                   \
		}                                                                                                               \
		static Variant::Type get_argument_type(int p_arg) {                                                             \
			return Variant::NIL;                                                                                        \
		}                                                                                                               \
		static Variant::Type get_return_type() {                                                                        \
			return Variant::NIL;                                                                                        \
		}                                                                                                               \
		static bool has_return_type() {                                                                                 \
			return true;                                                                                                \
		}                                                                                                               \
		static bool is_vararg() {                                                                                       \
			return false;                                                                                               \
		}                                                                                                               \
		static Variant::UtilityFunctionType get_type() {                                                                \
			return m_category;                                                                                          \
		}                                                                                                               \
	};                                                                                                                  \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVR2(m_func, m_args, m_category)                                                                                    \
	class Func_##m_func {                                                                                                          \
	public:                                                                                                                        \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {                   \
			r_error.error = Callable::CallError::CALL_OK;                                                                          \
			*r_ret = VariantUtilityFunctions::m_func(*p_args[0], *p_args[1], r_error);                                             \
		}                                                                                                                          \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                                       \
			Callable::CallError ce;                                                                                                \
			*r_ret = VariantUtilityFunctions::m_func(*p_args[0], *p_args[1], ce);                                                  \
		}                                                                                                                          \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                                      \
			Callable::CallError ce;                                                                                                \
			Variant r;                                                                                                             \
			r = VariantUtilityFunctions::m_func(PtrToArg<Variant>::convert(p_args[0]), PtrToArg<Variant>::convert(p_args[1]), ce); \
			PtrToArg<Variant>::encode(r, ret);                                                                                     \
		}                                                                                                                          \
		static int get_argument_count() {                                                                                          \
			return 2;                                                                                                              \
		}                                                                                                                          \
		static Variant::Type get_argument_type(int p_arg) {                                                                        \
			return Variant::NIL;                                                                                                   \
		}                                                                                                                          \
		static Variant::Type get_return_type() {                                                                                   \
			return Variant::NIL;                                                                                                   \
		}                                                                                                                          \
		static bool has_return_type() {                                                                                            \
			return true;                                                                                                           \
		}                                                                                                                          \
		static bool is_vararg() {                                                                                                  \
			return false;                                                                                                          \
		}                                                                                                                          \
		static Variant::UtilityFunctionType get_type() {                                                                           \
			return m_category;                                                                                                     \
		}                                                                                                                          \
	};                                                                                                                             \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVR3(m_func, m_args, m_category)                                                                                                                           \
	class Func_##m_func {                                                                                                                                                 \
	public:                                                                                                                                                               \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {                                                          \
			r_error.error = Callable::CallError::CALL_OK;                                                                                                                 \
			*r_ret = VariantUtilityFunctions::m_func(*p_args[0], *p_args[1], *p_args[2], r_error);                                                                        \
		}                                                                                                                                                                 \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                                                                              \
			Callable::CallError ce;                                                                                                                                       \
			*r_ret = VariantUtilityFunctions::m_func(*p_args[0], *p_args[1], *p_args[2], ce);                                                                             \
		}                                                                                                                                                                 \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                                                                             \
			Callable::CallError ce;                                                                                                                                       \
			Variant r;                                                                                                                                                    \
			r = VariantUtilityFunctions::m_func(PtrToArg<Variant>::convert(p_args[0]), PtrToArg<Variant>::convert(p_args[1]), PtrToArg<Variant>::convert(p_args[2]), ce); \
			PtrToArg<Variant>::encode(r, ret);                                                                                                                            \
		}                                                                                                                                                                 \
		static int get_argument_count() {                                                                                                                                 \
			return 3;                                                                                                                                                     \
		}                                                                                                                                                                 \
		static Variant::Type get_argument_type(int p_arg) {                                                                                                               \
			return Variant::NIL;                                                                                                                                          \
		}                                                                                                                                                                 \
		static Variant::Type get_return_type() {                                                                                                                          \
			return Variant::NIL;                                                                                                                                          \
		}                                                                                                                                                                 \
		static bool has_return_type() {                                                                                                                                   \
			return true;                                                                                                                                                  \
		}                                                                                                                                                                 \
		static bool is_vararg() {                                                                                                                                         \
			return false;                                                                                                                                                 \
		}                                                                                                                                                                 \
		static Variant::UtilityFunctionType get_type() {                                                                                                                  \
			return m_category;                                                                                                                                            \
		}                                                                                                                                                                 \
	};                                                                                                                                                                    \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARG(m_func, m_args, m_category)                                                               \
	class Func_##m_func {                                                                                        \
	public:                                                                                                      \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) { \
			r_error.error = Callable::CallError::CALL_OK;                                                        \
			*r_ret = VariantUtilityFunctions::m_func(p_args, p_argcount, r_error);                               \
		}                                                                                                        \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                     \
			Callable::CallError c;                                                                               \
			*r_ret = VariantUtilityFunctions::m_func(p_args, p_argcount, c);                                     \
		}                                                                                                        \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                    \
			Vector<Variant> args;                                                                                \
			for (int i = 0; i < p_argcount; i++) {                                                               \
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));                                           \
			}                                                                                                    \
			Vector<const Variant *> argsp;                                                                       \
			for (int i = 0; i < p_argcount; i++) {                                                               \
				argsp.push_back(&args[i]);                                                                       \
			}                                                                                                    \
			Variant r;                                                                                           \
			validated_call(&r, (const Variant **)argsp.ptr(), p_argcount);                                       \
			PtrToArg<Variant>::encode(r, ret);                                                                   \
		}                                                                                                        \
		static int get_argument_count() {                                                                        \
			return 2;                                                                                            \
		}                                                                                                        \
		static Variant::Type get_argument_type(int p_arg) {                                                      \
			return Variant::NIL;                                                                                 \
		}                                                                                                        \
		static Variant::Type get_return_type() {                                                                 \
			return Variant::NIL;                                                                                 \
		}                                                                                                        \
		static bool has_return_type() {                                                                          \
			return true;                                                                                         \
		}                                                                                                        \
		static bool is_vararg() {                                                                                \
			return true;                                                                                         \
		}                                                                                                        \
		static Variant::UtilityFunctionType get_type() {                                                         \
			return m_category;                                                                                   \
		}                                                                                                        \
	};                                                                                                           \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARGS(m_func, m_args, m_category)                                                              \
	class Func_##m_func {                                                                                        \
	public:                                                                                                      \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) { \
			r_error.error = Callable::CallError::CALL_OK;                                                        \
			*r_ret = VariantUtilityFunctions::m_func(p_args, p_argcount, r_error);                               \
		}                                                                                                        \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                     \
			Callable::CallError c;                                                                               \
			*r_ret = VariantUtilityFunctions::m_func(p_args, p_argcount, c);                                     \
		}                                                                                                        \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                    \
			Vector<Variant> args;                                                                                \
			for (int i = 0; i < p_argcount; i++) {                                                               \
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));                                           \
			}                                                                                                    \
			Vector<const Variant *> argsp;                                                                       \
			for (int i = 0; i < p_argcount; i++) {                                                               \
				argsp.push_back(&args[i]);                                                                       \
			}                                                                                                    \
			Variant r;                                                                                           \
			validated_call(&r, (const Variant **)argsp.ptr(), p_argcount);                                       \
			PtrToArg<String>::encode(r.operator String(), ret);                                                  \
		}                                                                                                        \
		static int get_argument_count() {                                                                        \
			return 1;                                                                                            \
		}                                                                                                        \
		static Variant::Type get_argument_type(int p_arg) {                                                      \
			return Variant::NIL;                                                                                 \
		}                                                                                                        \
		static Variant::Type get_return_type() {                                                                 \
			return Variant::STRING;                                                                              \
		}                                                                                                        \
		static bool has_return_type() {                                                                          \
			return true;                                                                                         \
		}                                                                                                        \
		static bool is_vararg() {                                                                                \
			return true;                                                                                         \
		}                                                                                                        \
		static Variant::UtilityFunctionType get_type() {                                                         \
			return m_category;                                                                                   \
		}                                                                                                        \
	};                                                                                                           \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARGV_CNAME(m_func, m_func_cname, m_args, m_category)                                          \
	class Func_##m_func {                                                                                        \
	public:                                                                                                      \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) { \
			r_error.error = Callable::CallError::CALL_OK;                                                        \
			VariantUtilityFunctions::m_func_cname(p_args, p_argcount, r_error);                                  \
		}                                                                                                        \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                     \
			Callable::CallError c;                                                                               \
			VariantUtilityFunctions::m_func_cname(p_args, p_argcount, c);                                        \
		}                                                                                                        \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                    \
			Vector<Variant> args;                                                                                \
			for (int i = 0; i < p_argcount; i++) {                                                               \
				args.push_back(PtrToArg<Variant>::convert(p_args[i]));                                           \
			}                                                                                                    \
			Vector<const Variant *> argsp;                                                                       \
			for (int i = 0; i < p_argcount; i++) {                                                               \
				argsp.push_back(&args[i]);                                                                       \
			}                                                                                                    \
			Variant r;                                                                                           \
			validated_call(&r, (const Variant **)argsp.ptr(), p_argcount);                                       \
		}                                                                                                        \
		static int get_argument_count() {                                                                        \
			return 1;                                                                                            \
		}                                                                                                        \
		static Variant::Type get_argument_type(int p_arg) {                                                      \
			return Variant::NIL;                                                                                 \
		}                                                                                                        \
		static Variant::Type get_return_type() {                                                                 \
			return Variant::NIL;                                                                                 \
		}                                                                                                        \
		static bool has_return_type() {                                                                          \
			return false;                                                                                        \
		}                                                                                                        \
		static bool is_vararg() {                                                                                \
			return true;                                                                                         \
		}                                                                                                        \
		static Variant::UtilityFunctionType get_type() {                                                         \
			return m_category;                                                                                   \
		}                                                                                                        \
	};                                                                                                           \
	register_utility_function<Func_##m_func>(#m_func, m_args)

#define FUNCBINDVARARGV(m_func, m_args, m_category) FUNCBINDVARARGV_CNAME(m_func, m_func, m_args, m_category)

#define FUNCBIND(m_func, m_args, m_category)                                                                     \
	class Func_##m_func {                                                                                        \
	public:                                                                                                      \
		static void call(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) { \
			call_helper(VariantUtilityFunctions::m_func, p_args, r_error);                                       \
		}                                                                                                        \
		static void validated_call(Variant *r_ret, const Variant **p_args, int p_argcount) {                     \
			validated_call_helper(VariantUtilityFunctions::m_func, p_args);                                      \
		}                                                                                                        \
		static void ptrcall(void *ret, const void **p_args, int p_argcount) {                                    \
			ptr_call_helper(VariantUtilityFunctions::m_func, p_args);                                            \
		}                                                                                                        \
		static int get_argument_count() {                                                                        \
			return get_arg_count_helper(VariantUtilityFunctions::m_func);                                        \
		}                                                                                                        \
		static Variant::Type get_argument_type(int p_arg) {                                                      \
			return get_arg_type_helper(VariantUtilityFunctions::m_func, p_arg);                                  \
		}                                                                                                        \
		static Variant::Type get_return_type() {                                                                 \
			return get_ret_type_helper(VariantUtilityFunctions::m_func);                                         \
		}                                                                                                        \
		static bool has_return_type() {                                                                          \
			return false;                                                                                        \
		}                                                                                                        \
		static bool is_vararg() {                                                                                \
			return false;                                                                                        \
		}                                                                                                        \
		static Variant::UtilityFunctionType get_type() {                                                         \
			return m_category;                                                                                   \
		}                                                                                                        \
	};                                                                                                           \
	register_utility_function<Func_##m_func>(#m_func, m_args)

struct VariantUtilityFunctionInfo {
	void (*call_utility)(Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) = nullptr;
	Variant::ValidatedUtilityFunction validated_call_utility = nullptr;
	Variant::PTRUtilityFunction ptr_call_utility = nullptr;
	Vector<String> argnames;
	bool is_vararg = false;
	bool returns_value = false;
	int argcount = 0;
	Variant::Type (*get_arg_type)(int) = nullptr;
	Variant::Type return_type;
	Variant::UtilityFunctionType type;
};

static OAHashMap<StringName, VariantUtilityFunctionInfo> utility_function_table;
static List<StringName> utility_function_name_table;

template <typename T>
static void register_utility_function(const String &p_name, const Vector<String> &argnames) {
	String name = p_name;
	if (name.begins_with("_")) {
		name = name.substr(1);
	}
	StringName sname = name;
	ERR_FAIL_COND(utility_function_table.has(sname));

	VariantUtilityFunctionInfo bfi;
	bfi.call_utility = T::call;
	bfi.validated_call_utility = T::validated_call;
	bfi.ptr_call_utility = T::ptrcall;
	bfi.is_vararg = T::is_vararg();
	bfi.argnames = argnames;
	bfi.argcount = T::get_argument_count();
	if (!bfi.is_vararg) {
		ERR_FAIL_COND_MSG(argnames.size() != bfi.argcount, vformat("Wrong number of arguments binding utility function: '%s'.", name));
	}
	bfi.get_arg_type = T::get_argument_type;
	bfi.return_type = T::get_return_type();
	bfi.type = T::get_type();
	bfi.returns_value = T::has_return_type();

	utility_function_table.insert(sname, bfi);
	utility_function_name_table.push_back(sname);
}

void Variant::_register_variant_utility_functions() {
	// Math

	FUNCBINDR(sin, sarray("angle_rad"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cos, sarray("angle_rad"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(tan, sarray("angle_rad"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(sinh, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cosh, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(tanh, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(asin, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(acos, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(atan, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(atan2, sarray("y", "x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(asinh, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(acosh, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(atanh, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(sqrt, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(fmod, sarray("x", "y"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(fposmod, sarray("x", "y"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(posmod, sarray("x", "y"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR(floor, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(floorf, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(floori, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR(ceil, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(ceilf, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(ceili, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR(round, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(roundf, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(roundi, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR(abs, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(absf, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(absi, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR(sign, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(signf, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(signi, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR2(snapped, sarray("x", "step"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(snappedf, sarray("x", "step"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(snappedi, sarray("x", "step"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(pow, sarray("base", "exp"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(log, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(exp, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(is_nan, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(is_inf, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(is_equal_approx, sarray("a", "b"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(is_zero_approx, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(is_finite, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(ease, sarray("x", "curve"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(step_decimals, sarray("x"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR3(lerp, sarray("from", "to", "weight"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(lerpf, sarray("from", "to", "weight"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate, sarray("from", "to", "pre", "post", "weight"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate_angle, sarray("from", "to", "pre", "post", "weight"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate_in_time, sarray("from", "to", "pre", "post", "weight", "to_t", "pre_t", "post_t"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(cubic_interpolate_angle_in_time, sarray("from", "to", "pre", "post", "weight", "to_t", "pre_t", "post_t"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(bezier_interpolate, sarray("start", "control_1", "control_2", "end", "t"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(bezier_derivative, sarray("start", "control_1", "control_2", "end", "t"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(angle_difference, sarray("from", "to"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(lerp_angle, sarray("from", "to", "weight"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(inverse_lerp, sarray("from", "to", "weight"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(remap, sarray("value", "istart", "istop", "ostart", "ostop"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(smoothstep, sarray("from", "to", "x"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(move_toward, sarray("from", "to", "delta"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(rotate_toward, sarray("from", "to", "delta"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(deg_to_rad, sarray("deg"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(rad_to_deg, sarray("rad"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(linear_to_db, sarray("lin"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(db_to_linear, sarray("db"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR3(wrap, sarray("value", "min", "max"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(wrapi, sarray("value", "min", "max"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(wrapf, sarray("value", "min", "max"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVARARG(max, sarray(), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(maxi, sarray("a", "b"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(maxf, sarray("a", "b"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVARARG(min, sarray(), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(mini, sarray("a", "b"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(minf, sarray("a", "b"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDVR3(clamp, sarray("value", "min", "max"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(clampi, sarray("value", "min", "max"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(clampf, sarray("value", "min", "max"), Variant::UTILITY_FUNC_TYPE_MATH);

	FUNCBINDR(nearest_po2, sarray("value"), Variant::UTILITY_FUNC_TYPE_MATH);
	FUNCBINDR(pingpong, sarray("value", "length"), Variant::UTILITY_FUNC_TYPE_MATH);

	// Random

	FUNCBIND(randomize, sarray(), Variant::UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randi, sarray(), Variant::UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randf, sarray(), Variant::UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randi_range, sarray("from", "to"), Variant::UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randf_range, sarray("from", "to"), Variant::UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(randfn, sarray("mean", "deviation"), Variant::UTILITY_FUNC_TYPE_RANDOM);
	FUNCBIND(seed, sarray("base"), Variant::UTILITY_FUNC_TYPE_RANDOM);
	FUNCBINDR(rand_from_seed, sarray("seed"), Variant::UTILITY_FUNC_TYPE_RANDOM);

	// Utility

	FUNCBINDVR(weakref, sarray("obj"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(_typeof, sarray("variable"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(type_convert, sarray("variant", "type"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGS(str, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(error_string, sarray("error"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(type_string, sarray("type"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(print, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(print_rich, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(printerr, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(printt, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(prints, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(printraw, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV_CNAME(print_verbose, _print_verbose, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(push_error, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDVARARGV(push_warning, sarray(), Variant::UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(var_to_str, sarray("variable"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(str_to_var, sarray("string"), Variant::UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(var_to_bytes, sarray("variable"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(bytes_to_var, sarray("bytes"), Variant::UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(var_to_bytes_with_objects, sarray("variable"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(bytes_to_var_with_objects, sarray("bytes"), Variant::UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(hash, sarray("variable"), Variant::UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(instance_from_id, sarray("instance_id"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(is_instance_id_valid, sarray("id"), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(is_instance_valid, sarray("instance"), Variant::UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(rid_allocate_id, Vector<String>(), Variant::UTILITY_FUNC_TYPE_GENERAL);
	FUNCBINDR(rid_from_int64, sarray("base"), Variant::UTILITY_FUNC_TYPE_GENERAL);

	FUNCBINDR(is_same, sarray("a", "b"), Variant::UTILITY_FUNC_TYPE_GENERAL);
}

void Variant::_unregister_variant_utility_functions() {
	utility_function_table.clear();
	utility_function_name_table.clear();
}

void Variant::call_utility_function(const StringName &p_name, Variant *r_ret, const Variant **p_args, int p_argcount, Callable::CallError &r_error) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		r_error.error = Callable::CallError::CALL_ERROR_INVALID_METHOD;
		r_error.argument = 0;
		r_error.expected = 0;
		return;
	}

	if (unlikely(!bfi->is_vararg && p_argcount < bfi->argcount)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_FEW_ARGUMENTS;
		r_error.expected = bfi->argcount;
		return;
	}

	if (unlikely(!bfi->is_vararg && p_argcount > bfi->argcount)) {
		r_error.error = Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS;
		r_error.expected = bfi->argcount;
		return;
	}

	bfi->call_utility(r_ret, p_args, p_argcount, r_error);
}

bool Variant::has_utility_function(const StringName &p_name) {
	return utility_function_table.has(p_name);
}

Variant::ValidatedUtilityFunction Variant::get_validated_utility_function(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return nullptr;
	}

	return bfi->validated_call_utility;
}

Variant::PTRUtilityFunction Variant::get_ptr_utility_function(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return nullptr;
	}

	return bfi->ptr_call_utility;
}

Variant::UtilityFunctionType Variant::get_utility_function_type(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return Variant::UTILITY_FUNC_TYPE_MATH;
	}

	return bfi->type;
}

MethodInfo Variant::get_utility_function_info(const StringName &p_name) {
	MethodInfo info;
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (bfi) {
		info.name = p_name;
		if (bfi->returns_value && bfi->return_type == Variant::NIL) {
			info.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
		}
		info.return_val.type = bfi->return_type;
		if (bfi->is_vararg) {
			info.flags |= METHOD_FLAG_VARARG;
		}
		for (int i = 0; i < bfi->argnames.size(); ++i) {
			PropertyInfo arg;
			arg.type = bfi->get_arg_type(i);
			arg.name = bfi->argnames[i];
			info.arguments.push_back(arg);
		}
	}
	return info;
}

int Variant::get_utility_function_argument_count(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return 0;
	}

	return bfi->argcount;
}

Variant::Type Variant::get_utility_function_argument_type(const StringName &p_name, int p_arg) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return Variant::NIL;
	}

	return bfi->get_arg_type(p_arg);
}

String Variant::get_utility_function_argument_name(const StringName &p_name, int p_arg) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return String();
	}
	ERR_FAIL_INDEX_V(p_arg, bfi->argnames.size(), String());
	ERR_FAIL_COND_V(bfi->is_vararg, String());
	return bfi->argnames[p_arg];
}

bool Variant::has_utility_function_return_value(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return false;
	}
	return bfi->returns_value;
}

Variant::Type Variant::get_utility_function_return_type(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return Variant::NIL;
	}

	return bfi->return_type;
}

bool Variant::is_utility_function_vararg(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	if (!bfi) {
		return false;
	}

	return bfi->is_vararg;
}

uint32_t Variant::get_utility_function_hash(const StringName &p_name) {
	const VariantUtilityFunctionInfo *bfi = utility_function_table.lookup_ptr(p_name);
	ERR_FAIL_NULL_V(bfi, 0);

	uint32_t hash = hash_murmur3_one_32(bfi->is_vararg);
	hash = hash_murmur3_one_32(bfi->returns_value, hash);
	if (bfi->returns_value) {
		hash = hash_murmur3_one_32(bfi->return_type, hash);
	}
	hash = hash_murmur3_one_32(bfi->argcount, hash);
	for (int i = 0; i < bfi->argcount; i++) {
		hash = hash_murmur3_one_32(bfi->get_arg_type(i), hash);
	}

	return hash_fmix32(hash);
}

void Variant::get_utility_function_list(List<StringName> *r_functions) {
	for (const StringName &E : utility_function_name_table) {
		r_functions->push_back(E);
	}
}

int Variant::get_utility_function_count() {
	return utility_function_name_table.size();
}
