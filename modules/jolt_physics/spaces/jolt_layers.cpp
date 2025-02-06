/**************************************************************************/
/*  jolt_layers.cpp                                                       */
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

#include "jolt_layers.h"

#include "../jolt_project_settings.h"
#include "jolt_broad_phase_layer.h"

#include "core/error/error_macros.h"
#include "core/variant/variant.h"

static_assert(sizeof(JPH::ObjectLayer) == 2, "Size of Jolt's object layer has changed.");
static_assert(sizeof(JPH::BroadPhaseLayer::Type) == 1, "Size of Jolt's broadphase layer has changed.");
static_assert(JoltBroadPhaseLayer::COUNT <= 8, "Maximum number of broadphase layers exceeded.");

namespace {

template <uint8_t TSize = JoltBroadPhaseLayer::COUNT>
class JoltBroadPhaseMatrix {
	typedef JPH::BroadPhaseLayer LayerType;
	typedef LayerType::Type UnderlyingType;

public:
	JoltBroadPhaseMatrix() {
		using namespace JoltBroadPhaseLayer;

		allow_collision(BODY_STATIC, BODY_DYNAMIC);
		allow_collision(BODY_STATIC_BIG, BODY_DYNAMIC);
		allow_collision(BODY_DYNAMIC, BODY_STATIC);
		allow_collision(BODY_DYNAMIC, BODY_STATIC_BIG);
		allow_collision(BODY_DYNAMIC, BODY_DYNAMIC);
		allow_collision(BODY_DYNAMIC, AREA_DETECTABLE);
		allow_collision(BODY_DYNAMIC, AREA_UNDETECTABLE);
		allow_collision(AREA_DETECTABLE, BODY_DYNAMIC);
		allow_collision(AREA_DETECTABLE, AREA_DETECTABLE);
		allow_collision(AREA_DETECTABLE, AREA_UNDETECTABLE);
		allow_collision(AREA_UNDETECTABLE, BODY_DYNAMIC);
		allow_collision(AREA_UNDETECTABLE, AREA_DETECTABLE);

		if (JoltProjectSettings::areas_detect_static_bodies) {
			allow_collision(BODY_STATIC, AREA_DETECTABLE);
			allow_collision(BODY_STATIC, AREA_UNDETECTABLE);
			allow_collision(BODY_STATIC_BIG, AREA_DETECTABLE);
			allow_collision(BODY_STATIC_BIG, AREA_UNDETECTABLE);
			allow_collision(AREA_DETECTABLE, BODY_STATIC);
			allow_collision(AREA_DETECTABLE, BODY_STATIC_BIG);
			allow_collision(AREA_UNDETECTABLE, BODY_STATIC);
			allow_collision(AREA_UNDETECTABLE, BODY_STATIC_BIG);
		}
	}

	void allow_collision(UnderlyingType p_layer1, UnderlyingType p_layer2) { masks[p_layer1] |= uint8_t(1U << p_layer2); }
	void allow_collision(LayerType p_layer1, LayerType p_layer2) { allow_collision((UnderlyingType)p_layer1, (UnderlyingType)p_layer2); }

	bool should_collide(UnderlyingType p_layer1, UnderlyingType p_layer2) const { return (masks[p_layer1] & uint8_t(1U << p_layer2)) != 0; }
	bool should_collide(LayerType p_layer1, LayerType p_layer2) const { return should_collide((UnderlyingType)p_layer1, (UnderlyingType)p_layer2); }

private:
	uint8_t masks[TSize] = {};
};

constexpr JPH::ObjectLayer encode_layers(JPH::BroadPhaseLayer p_broad_phase_layer, JPH::ObjectLayer p_object_layer) {
	const uint16_t upper_bits = uint16_t((uint8_t)p_broad_phase_layer << 13U);
	const uint16_t lower_bits = uint16_t(p_object_layer);
	return JPH::ObjectLayer(upper_bits | lower_bits);
}

constexpr void decode_layers(JPH::ObjectLayer p_encoded_layers, JPH::BroadPhaseLayer &r_broad_phase_layer, JPH::ObjectLayer &r_object_layer) {
	r_broad_phase_layer = JPH::BroadPhaseLayer(uint8_t(p_encoded_layers >> 13U));
	r_object_layer = JPH::ObjectLayer(p_encoded_layers & 0b0001'1111'1111'1111U);
}

constexpr uint64_t encode_collision(uint32_t p_collision_layer, uint32_t p_collision_mask) {
	const uint64_t upper_bits = (uint64_t)p_collision_layer << 32U;
	const uint64_t lower_bits = (uint64_t)p_collision_mask;
	return upper_bits | lower_bits;
}

constexpr void decode_collision(uint64_t p_collision, uint32_t &r_collision_layer, uint32_t &r_collision_mask) {
	r_collision_layer = uint32_t(p_collision >> 32U);
	r_collision_mask = uint32_t(p_collision & 0xFFFFFFFFU);
}

} // namespace

uint32_t JoltLayers::GetNumBroadPhaseLayers() const {
	return JoltBroadPhaseLayer::COUNT;
}

JPH::BroadPhaseLayer JoltLayers::GetBroadPhaseLayer(JPH::ObjectLayer p_layer) const {
	JPH::BroadPhaseLayer broad_phase_layer = JoltBroadPhaseLayer::BODY_STATIC;
	JPH::ObjectLayer object_layer = 0;
	decode_layers(p_layer, broad_phase_layer, object_layer);

	return broad_phase_layer;
}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)

const char *JoltLayers::GetBroadPhaseLayerName(JPH::BroadPhaseLayer p_layer) const {
	switch ((JPH::BroadPhaseLayer::Type)p_layer) {
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::BODY_STATIC: {
			return "BODY_STATIC";
		}
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::BODY_STATIC_BIG: {
			return "BODY_STATIC_BIG";
		}
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::BODY_DYNAMIC: {
			return "BODY_DYNAMIC";
		}
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::AREA_DETECTABLE: {
			return "AREA_DETECTABLE";
		}
		case (JPH::BroadPhaseLayer::Type)JoltBroadPhaseLayer::AREA_UNDETECTABLE: {
			return "AREA_UNDETECTABLE";
		}
		default: {
			return "UNKNOWN";
		}
	}
}

#endif

bool JoltLayers::ShouldCollide(JPH::ObjectLayer p_encoded_layer1, JPH::ObjectLayer p_encoded_layer2) const {
	JPH::BroadPhaseLayer broad_phase_layer1 = JoltBroadPhaseLayer::BODY_STATIC;
	uint32_t collision_layer1 = 0;
	uint32_t collision_mask1 = 0;
	from_object_layer(p_encoded_layer1, broad_phase_layer1, collision_layer1, collision_mask1);

	JPH::BroadPhaseLayer broad_phase_layer2 = JoltBroadPhaseLayer::BODY_STATIC;
	uint32_t collision_layer2 = 0;
	uint32_t collision_mask2 = 0;
	from_object_layer(p_encoded_layer2, broad_phase_layer2, collision_layer2, collision_mask2);

	const bool first_scans_second = (collision_mask1 & collision_layer2) != 0;
	const bool second_scans_first = (collision_mask2 & collision_layer1) != 0;

	return first_scans_second || second_scans_first;
}

bool JoltLayers::ShouldCollide(JPH::ObjectLayer p_encoded_layer1, JPH::BroadPhaseLayer p_broad_phase_layer2) const {
	static const JoltBroadPhaseMatrix matrix;

	JPH::BroadPhaseLayer broad_phase_layer1 = JoltBroadPhaseLayer::BODY_STATIC;
	JPH::ObjectLayer object_layer1 = 0;
	decode_layers(p_encoded_layer1, broad_phase_layer1, object_layer1);

	return matrix.should_collide(broad_phase_layer1, p_broad_phase_layer2);
}

JPH::ObjectLayer JoltLayers::_allocate_object_layer(uint64_t p_collision) {
	const JPH::ObjectLayer new_object_layer = next_object_layer++;

	collisions_by_layer.resize(new_object_layer + 1);
	collisions_by_layer[new_object_layer] = p_collision;

	layers_by_collision[p_collision] = new_object_layer;

	return new_object_layer;
}

JoltLayers::JoltLayers() {
	_allocate_object_layer(0);
}

// MinGW GCC using LTO will emit errors during linking if this is defined in the header file, implicitly or otherwise.
// Likely caused by this GCC bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94156
JoltLayers::~JoltLayers() = default;

JPH::ObjectLayer JoltLayers::to_object_layer(JPH::BroadPhaseLayer p_broad_phase_layer, uint32_t p_collision_layer, uint32_t p_collision_mask) {
	const uint64_t collision = encode_collision(p_collision_layer, p_collision_mask);

	JPH::ObjectLayer object_layer = 0;

	HashMap<uint64_t, JPH::ObjectLayer>::Iterator iter = layers_by_collision.find(collision);
	if (iter != layers_by_collision.end()) {
		object_layer = iter->value;
	} else {
		constexpr uint16_t object_layer_count = 1U << 13U;

		ERR_FAIL_COND_V_MSG(next_object_layer == object_layer_count, 0,
				vformat("Maximum number of object layers (%d) reached. "
						"This means there are %d combinations of collision layers and masks."
						"This should not happen under normal circumstances. Consider reporting this.",
						object_layer_count, object_layer_count));

		object_layer = _allocate_object_layer(collision);
	}

	return encode_layers(p_broad_phase_layer, object_layer);
}

void JoltLayers::from_object_layer(JPH::ObjectLayer p_encoded_layer, JPH::BroadPhaseLayer &r_broad_phase_layer, uint32_t &r_collision_layer, uint32_t &r_collision_mask) const {
	JPH::ObjectLayer object_layer = 0;
	decode_layers(p_encoded_layer, r_broad_phase_layer, object_layer);

	const uint64_t collision = collisions_by_layer[object_layer];
	decode_collision(collision, r_collision_layer, r_collision_mask);
}
