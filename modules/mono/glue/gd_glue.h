/**************************************************************************/
/*  gd_glue.h                                                             */
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

#ifndef GD_GLUE_H
#define GD_GLUE_H

#ifdef MONO_GLUE_ENABLED

#include "../mono_gd/gd_mono_marshal.h"

MonoObject *godot_icall_GD_bytes2var(MonoArray *p_bytes, MonoBoolean p_allow_objects);

MonoObject *godot_icall_GD_convert(MonoObject *p_what, int32_t p_type);

int godot_icall_GD_hash(MonoObject *p_var);

MonoObject *godot_icall_GD_instance_from_id(uint64_t p_instance_id);

void godot_icall_GD_print(MonoArray *p_what);

void godot_icall_GD_printerr(MonoArray *p_what);

void godot_icall_GD_printraw(MonoArray *p_what);

void godot_icall_GD_print_verbose(MonoArray *p_what);

void godot_icall_GD_prints(MonoArray *p_what);

void godot_icall_GD_printt(MonoArray *p_what);

float godot_icall_GD_randf();

uint32_t godot_icall_GD_randi();

void godot_icall_GD_randomize();

double godot_icall_GD_rand_range(double from, double to);

uint32_t godot_icall_GD_rand_seed(uint64_t seed, uint64_t *newSeed);

void godot_icall_GD_seed(uint64_t p_seed);

MonoString *godot_icall_GD_str(MonoArray *p_what);

MonoObject *godot_icall_GD_str2var(MonoString *p_str);

MonoBoolean godot_icall_GD_type_exists(MonoString *p_type);

MonoArray *godot_icall_GD_var2bytes(MonoObject *p_var, MonoBoolean p_full_objects);

MonoString *godot_icall_GD_var2str(MonoObject *p_var);

MonoObject *godot_icall_DefaultGodotTaskScheduler();

// Register internal calls

void godot_register_gd_icalls();

#endif // MONO_GLUE_ENABLED

#endif // GD_GLUE_H
