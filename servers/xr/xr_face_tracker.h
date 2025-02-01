/**************************************************************************/
/*  xr_face_tracker.h                                                     */
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

#include "servers/xr/xr_tracker.h"

/**
	The XRFaceTracker class provides face blend shape weights.

	The supported blend shapes are based on the Unified Expressions
	standard, and as such have a well defined mapping to ARKit, SRanipal,
	and Meta Movement standards.
 */

class XRFaceTracker : public XRTracker {
	GDCLASS(XRFaceTracker, XRTracker);
	_THREAD_SAFE_CLASS_

public:
	enum BlendShapeEntry {
		// Base Shapes
		FT_EYE_LOOK_OUT_RIGHT, // Right eye looks outwards.
		FT_EYE_LOOK_IN_RIGHT, // Right eye looks inwards.
		FT_EYE_LOOK_UP_RIGHT, // Right eye looks upwards.
		FT_EYE_LOOK_DOWN_RIGHT, // Right eye looks downwards.
		FT_EYE_LOOK_OUT_LEFT, // Left eye looks outwards.
		FT_EYE_LOOK_IN_LEFT, // Left eye looks inwards.
		FT_EYE_LOOK_UP_LEFT, // Left eye looks upwards.
		FT_EYE_LOOK_DOWN_LEFT, // Left eye looks downwards.
		FT_EYE_CLOSED_RIGHT, // Closes the right eyelid.
		FT_EYE_CLOSED_LEFT, // Closes the left eyelid.
		FT_EYE_SQUINT_RIGHT, // Squeezes the right eye socket muscles.
		FT_EYE_SQUINT_LEFT, // Squeezes the left eye socket muscles.
		FT_EYE_WIDE_RIGHT, // Right eyelid widens beyond relaxed.
		FT_EYE_WIDE_LEFT, // Left eyelid widens beyond relaxed.
		FT_EYE_DILATION_RIGHT, // Dilates the right eye pupil.
		FT_EYE_DILATION_LEFT, // Dilates the left eye pupil.
		FT_EYE_CONSTRICT_RIGHT, // Constricts the right eye pupil.
		FT_EYE_CONSTRICT_LEFT, // Constricts the left eye pupil.
		FT_BROW_PINCH_RIGHT, // Right eyebrow pinches in.
		FT_BROW_PINCH_LEFT, // Left eyebrow pinches in.
		FT_BROW_LOWERER_RIGHT, // Outer right eyebrow pulls down.
		FT_BROW_LOWERER_LEFT, // Outer left eyebrow pulls down.
		FT_BROW_INNER_UP_RIGHT, // Inner right eyebrow pulls up.
		FT_BROW_INNER_UP_LEFT, // Inner left eyebrow pulls up.
		FT_BROW_OUTER_UP_RIGHT, // Outer right eyebrow pulls up.
		FT_BROW_OUTER_UP_LEFT, // Outer left eyebrow pulls up.
		FT_NOSE_SNEER_RIGHT, // Right side face sneers.
		FT_NOSE_SNEER_LEFT, // Left side face sneers.
		FT_NASAL_DILATION_RIGHT, // Right side nose canal dilates.
		FT_NASAL_DILATION_LEFT, // Left side nose canal dilates.
		FT_NASAL_CONSTRICT_RIGHT, // Right side nose canal constricts.
		FT_NASAL_CONSTRICT_LEFT, // Left side nose canal constricts.
		FT_CHEEK_SQUINT_RIGHT, // Raises the right side cheek.
		FT_CHEEK_SQUINT_LEFT, // Raises the left side cheek.
		FT_CHEEK_PUFF_RIGHT, // Puffs the right side cheek.
		FT_CHEEK_PUFF_LEFT, // Puffs the left side cheek.
		FT_CHEEK_SUCK_RIGHT, // Sucks in the right side cheek.
		FT_CHEEK_SUCK_LEFT, // Sucks in the left side cheek.
		FT_JAW_OPEN, // Opens jawbone.
		FT_MOUTH_CLOSED, // Closes the mouth.
		FT_JAW_RIGHT, // Pushes jawbone right.
		FT_JAW_LEFT, // Pushes jawbone left.
		FT_JAW_FORWARD, // Pushes jawbone forward.
		FT_JAW_BACKWARD, // Pushes jawbone backward.
		FT_JAW_CLENCH, // Flexes jaw muscles.
		FT_JAW_MANDIBLE_RAISE, // Raises the jawbone.
		FT_LIP_SUCK_UPPER_RIGHT, // Upper right lip part tucks in the mouth.
		FT_LIP_SUCK_UPPER_LEFT, // Upper left lip part tucks in the mouth.
		FT_LIP_SUCK_LOWER_RIGHT, // Lower right lip part tucks in the mouth.
		FT_LIP_SUCK_LOWER_LEFT, // Lower left lip part tucks in the mouth.
		FT_LIP_SUCK_CORNER_RIGHT, // Right lip corner folds into the mouth.
		FT_LIP_SUCK_CORNER_LEFT, // Left lip corner folds into the mouth.
		FT_LIP_FUNNEL_UPPER_RIGHT, // Upper right lip part pushes into a funnel.
		FT_LIP_FUNNEL_UPPER_LEFT, // Upper left lip part pushes into a funnel.
		FT_LIP_FUNNEL_LOWER_RIGHT, // Lower right lip part pushes into a funnel.
		FT_LIP_FUNNEL_LOWER_LEFT, // Lower left lip part pushes into a funnel.
		FT_LIP_PUCKER_UPPER_RIGHT, // Upper right lip part pushes outwards.
		FT_LIP_PUCKER_UPPER_LEFT, // Upper left lip part pushes outwards.
		FT_LIP_PUCKER_LOWER_RIGHT, // Lower right lip part pushes outwards.
		FT_LIP_PUCKER_LOWER_LEFT, // Lower left lip part pushes outwards.
		FT_MOUTH_UPPER_UP_RIGHT, // Upper right part of the lip pulls up.
		FT_MOUTH_UPPER_UP_LEFT, // Upper left part of the lip pulls up.
		FT_MOUTH_LOWER_DOWN_RIGHT, // Lower right part of the lip pulls up.
		FT_MOUTH_LOWER_DOWN_LEFT, // Lower left part of the lip pulls up.
		FT_MOUTH_UPPER_DEEPEN_RIGHT, // Upper right lip part pushes in the cheek.
		FT_MOUTH_UPPER_DEEPEN_LEFT, // Upper left lip part pushes in the cheek.
		FT_MOUTH_UPPER_RIGHT, // Moves upper lip right.
		FT_MOUTH_UPPER_LEFT, // Moves upper lip left.
		FT_MOUTH_LOWER_RIGHT, // Moves lower lip right.
		FT_MOUTH_LOWER_LEFT, // Moves lower lip left.
		FT_MOUTH_CORNER_PULL_RIGHT, // Right lip corner pulls diagonally up and out.
		FT_MOUTH_CORNER_PULL_LEFT, // Left lip corner pulls diagonally up and out.
		FT_MOUTH_CORNER_SLANT_RIGHT, // Right corner lip slants up.
		FT_MOUTH_CORNER_SLANT_LEFT, // Left corner lip slants up.
		FT_MOUTH_FROWN_RIGHT, // Right corner lip pulls down.
		FT_MOUTH_FROWN_LEFT, // Left corner lip pulls down.
		FT_MOUTH_STRETCH_RIGHT, // Mouth corner lip pulls out and down.
		FT_MOUTH_STRETCH_LEFT, // Mouth corner lip pulls out and down.
		FT_MOUTH_DIMPLE_RIGHT, // Right lip corner is pushed backwards.
		FT_MOUTH_DIMPLE_LEFT, // Left lip corner is pushed backwards.
		FT_MOUTH_RAISER_UPPER, // Raises and slightly pushes out the upper mouth.
		FT_MOUTH_RAISER_LOWER, // Raises and slightly pushes out the lower mouth.
		FT_MOUTH_PRESS_RIGHT, // Right side lips press and flatten together vertically.
		FT_MOUTH_PRESS_LEFT, // Left side lips press and flatten together vertically.
		FT_MOUTH_TIGHTENER_RIGHT, // Right side lips squeeze together horizontally.
		FT_MOUTH_TIGHTENER_LEFT, // Left side lips squeeze together horizontally.
		FT_TONGUE_OUT, // Tongue visibly sticks out of the mouth.
		FT_TONGUE_UP, // Tongue points upwards.
		FT_TONGUE_DOWN, // Tongue points downwards.
		FT_TONGUE_RIGHT, // Tongue points right.
		FT_TONGUE_LEFT, // Tongue points left.
		FT_TONGUE_ROLL, // Sides of the tongue funnel, creating a roll.
		FT_TONGUE_BLEND_DOWN, // Tongue arches up then down inside the mouth.
		FT_TONGUE_CURL_UP, // Tongue arches down then up inside the mouth.
		FT_TONGUE_SQUISH, // Tongue squishes together and thickens.
		FT_TONGUE_FLAT, // Tongue flattens and thins out.
		FT_TONGUE_TWIST_RIGHT, // Tongue tip rotates clockwise, with the rest following gradually.
		FT_TONGUE_TWIST_LEFT, // Tongue tip rotates counter-clockwise, with the rest following gradually.
		FT_SOFT_PALATE_CLOSE, // Inner mouth throat closes.
		FT_THROAT_SWALLOW, // The Adam's apple visibly swallows.
		FT_NECK_FLEX_RIGHT, // Right side neck visibly flexes.
		FT_NECK_FLEX_LEFT, // Left side neck visibly flexes.
		// Blended Shapes
		FT_EYE_CLOSED, // Closes both eye lids.
		FT_EYE_WIDE, // Widens both eye lids.
		FT_EYE_SQUINT, // Squints both eye lids.
		FT_EYE_DILATION, // Dilates both pupils.
		FT_EYE_CONSTRICT, // Constricts both pupils.
		FT_BROW_DOWN_RIGHT, // Pulls the right eyebrow down and in.
		FT_BROW_DOWN_LEFT, // Pulls the left eyebrow down and in.
		FT_BROW_DOWN, // Pulls both eyebrows down and in.
		FT_BROW_UP_RIGHT, // Right brow appears worried.
		FT_BROW_UP_LEFT, // Left brow appears worried.
		FT_BROW_UP, // Both brows appear worried.
		FT_NOSE_SNEER, // Entire face sneers.
		FT_NASAL_DILATION, // Both nose canals dilate.
		FT_NASAL_CONSTRICT, // Both nose canals constrict.
		FT_CHEEK_PUFF, // Puffs both cheeks.
		FT_CHEEK_SUCK, // Sucks in both cheeks.
		FT_CHEEK_SQUINT, // Raises both cheeks.
		FT_LIP_SUCK_UPPER, // Tucks in the upper lips.
		FT_LIP_SUCK_LOWER, // Tucks in the lower lips.
		FT_LIP_SUCK, // Tucks in both lips.
		FT_LIP_FUNNEL_UPPER, // Funnels in the upper lips.
		FT_LIP_FUNNEL_LOWER, // Funnels in the lower lips.
		FT_LIP_FUNNEL, // Funnels in both lips.
		FT_LIP_PUCKER_UPPER, // Upper lip part pushes outwards.
		FT_LIP_PUCKER_LOWER, // Lower lip part pushes outwards.
		FT_LIP_PUCKER, // Lips push outwards.
		FT_MOUTH_UPPER_UP, // Raises the upper lips.
		FT_MOUTH_LOWER_DOWN, // Lowers the lower lips.
		FT_MOUTH_OPEN, // Mouth opens, revealing teeth.
		FT_MOUTH_RIGHT, // Moves mouth right.
		FT_MOUTH_LEFT, // Moves mouth left.
		FT_MOUTH_SMILE_RIGHT, // Right side of the mouth smiles.
		FT_MOUTH_SMILE_LEFT, // Left side of the mouth smiles.
		FT_MOUTH_SMILE, // Mouth expresses a smile.
		FT_MOUTH_SAD_RIGHT, // Right side of the mouth expresses sadness.
		FT_MOUTH_SAD_LEFT, // Left side of the mouth expresses sadness.
		FT_MOUTH_SAD, // Mouth expresses sadness.
		FT_MOUTH_STRETCH, // Mouth stretches.
		FT_MOUTH_DIMPLE, // Lip corners dimple.
		FT_MOUTH_TIGHTENER, // Mouth tightens.
		FT_MOUTH_PRESS, // Mouth presses together.
		FT_MAX // Maximum blend shape.
	};

	void set_tracker_type(XRServer::TrackerType p_type) override;

	float get_blend_shape(BlendShapeEntry p_blend_shape) const;
	void set_blend_shape(BlendShapeEntry p_blend_shape, float p_value);

	PackedFloat32Array get_blend_shapes() const;
	void set_blend_shapes(const PackedFloat32Array &p_blend_shapes);

	XRFaceTracker();

protected:
	static void _bind_methods();

private:
	float blend_shape_values[FT_MAX] = {};
};

VARIANT_ENUM_CAST(XRFaceTracker::BlendShapeEntry);
