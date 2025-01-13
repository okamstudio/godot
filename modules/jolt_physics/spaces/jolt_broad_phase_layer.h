/**************************************************************************/
/*  jolt_broad_phase_layer.h                                              */
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

#ifndef JOLT_BROAD_PHASE_LAYER_H
#define JOLT_BROAD_PHASE_LAYER_H

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h"

#include <stdint.h>

namespace JoltBroadPhaseLayer {

constexpr JPH::BroadPhaseLayer BODY_STATIC(0);
constexpr JPH::BroadPhaseLayer BODY_STATIC_BIG(1);
constexpr JPH::BroadPhaseLayer BODY_DYNAMIC(2);
constexpr JPH::BroadPhaseLayer AREA_DETECTABLE(3);
constexpr JPH::BroadPhaseLayer AREA_UNDETECTABLE(4);

constexpr uint32_t COUNT = 5;

} // namespace JoltBroadPhaseLayer

#endif // JOLT_BROAD_PHASE_LAYER_H
