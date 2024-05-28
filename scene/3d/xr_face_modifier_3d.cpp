/**************************************************************************/
/*  xr_face_modifier_3d.cpp                                               */
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

#include "xr_face_modifier_3d.h"

#include "servers/xr/xr_face_tracker.h"
#include "servers/xr_server.h"

// This method takes the name of a mesh blend shape and returns the
// corresponding XRFaceTracker blend shape. If no match is
// found then the function returns -1.
static int find_face_blend_shape(const StringName &p_name) {
	// Entry for blend shape name table.
	struct blend_map_entry {
		int blend;
		const char *name[4];
	};

	// Table of blend shape names.
	//
	// This table consists of the XRFaceTracker blend shape and
	// the corresponding names (lowercase and no underscore) of:
	// - The Unified Expression blend shape name.
	// - The ARKit blend shape name (if present and different).
	// - The SRanipal blend shape name (if present and different).
	// - The Meta blend shape name (if present and different).
	static constexpr blend_map_entry blend_map[] = {
		{ XRFaceTracker::FT_EYE_LOOK_OUT_RIGHT,
				{ "eyelookoutright", "eyerightright", "eyeslookoutr" } },
		{ XRFaceTracker::FT_EYE_LOOK_IN_RIGHT,
				{ "eyelookinright", "eyerightleft", "eyeslookinr" } },
		{ XRFaceTracker::FT_EYE_LOOK_UP_RIGHT,
				{ "eyelookupright", "eyerightlookup", "eyeslookupr" } },
		{ XRFaceTracker::FT_EYE_LOOK_DOWN_RIGHT,
				{ "eyelookdownright", "eyerightlookdown", "eyeslookdownr" } },
		{ XRFaceTracker::FT_EYE_LOOK_OUT_LEFT,
				{ "eyelookoutleft", "eyeleftleft", "eyeslookoutl" } },
		{ XRFaceTracker::FT_EYE_LOOK_IN_LEFT,
				{ "eyelookinleft", "eyeleftright", "eyeslookinl" } },
		{ XRFaceTracker::FT_EYE_LOOK_UP_LEFT,
				{ "eyelookupleft", "eyeleftlookup", "eyeslookupl" } },
		{ XRFaceTracker::FT_EYE_LOOK_DOWN_LEFT,
				{ "eyelookdownleft", "eyeleftlookdown", "eyeslookdownl" } },
		{ XRFaceTracker::FT_EYE_CLOSED_RIGHT,
				{ "eyeclosedright", "eyeblinkright", "eyerightblink", "eyesclosedr" } },
		{ XRFaceTracker::FT_EYE_CLOSED_LEFT,
				{ "eyeclosedleft", "eyeblinkleft", "eyeleftblink", "eyesclosedl" } },
		{ XRFaceTracker::FT_EYE_SQUINT_RIGHT,
				{ "eyesquintright", "eyessquintr" } },
		{ XRFaceTracker::FT_EYE_SQUINT_LEFT,
				{ "eyesquintleft", "eyessquintl" } },
		{ XRFaceTracker::FT_EYE_WIDE_RIGHT,
				{ "eyewideright", "eyerightwide", "eyeswidenr" } },
		{ XRFaceTracker::FT_EYE_WIDE_LEFT,
				{ "eyewideleft", "eyeleftwide", "eyeswidenl" } },
		{ XRFaceTracker::FT_EYE_DILATION_RIGHT,
				{ "eyedilationright", "eyerightdilation" } },
		{ XRFaceTracker::FT_EYE_DILATION_LEFT,
				{ "eyedilationleft", "eyeleftdilation" } },
		{ XRFaceTracker::FT_EYE_CONSTRICT_RIGHT,
				{ "eyeconstrictright", "eyerightconstrict" } },
		{ XRFaceTracker::FT_EYE_CONSTRICT_LEFT,
				{ "eyeconstrictleft", "eyeleftconstrict" } },
		{ XRFaceTracker::FT_BROW_PINCH_RIGHT,
				{ "browpinchright" } },
		{ XRFaceTracker::FT_BROW_PINCH_LEFT,
				{ "browpinchleft" } },
		{ XRFaceTracker::FT_BROW_LOWERER_RIGHT,
				{ "browlowererright" } },
		{ XRFaceTracker::FT_BROW_LOWERER_LEFT,
				{ "browlowererleft" } },
		{ XRFaceTracker::FT_BROW_INNER_UP_RIGHT,
				{ "browinnerupright", "innerbrowraiserr" } },
		{ XRFaceTracker::FT_BROW_INNER_UP_LEFT,
				{ "browinnerupleft", "innerbrowraiserl" } },
		{ XRFaceTracker::FT_BROW_OUTER_UP_RIGHT,
				{ "browouterupright", "outerbrowraiserr" } },
		{ XRFaceTracker::FT_BROW_OUTER_UP_LEFT,
				{ "browouterupleft", "outerbrowraiserl" } },
		{ XRFaceTracker::FT_NOSE_SNEER_RIGHT,
				{ "nosesneerright", "nosewrinklerr" } },
		{ XRFaceTracker::FT_NOSE_SNEER_LEFT,
				{ "nosesneerleft", "nosewrinklerl" } },
		{ XRFaceTracker::FT_NASAL_DILATION_RIGHT,
				{ "nasaldilationright" } },
		{ XRFaceTracker::FT_NASAL_DILATION_LEFT,
				{ "nasaldilationleft" } },
		{ XRFaceTracker::FT_NASAL_CONSTRICT_RIGHT,
				{ "nasalconstrictright" } },
		{ XRFaceTracker::FT_NASAL_CONSTRICT_LEFT,
				{ "nasalconstrictleft" } },
		{ XRFaceTracker::FT_CHEEK_SQUINT_RIGHT,
				{ "cheeksquintright", "cheekraiserr" } },
		{ XRFaceTracker::FT_CHEEK_SQUINT_LEFT,
				{ "cheeksquintleft", "cheekraiserl" } },
		{ XRFaceTracker::FT_CHEEK_PUFF_RIGHT,
				{ "cheekpuffright", "cheekpuffr" } },
		{ XRFaceTracker::FT_CHEEK_PUFF_LEFT,
				{ "cheekpuffleft", "cheekpuffl" } },
		{ XRFaceTracker::FT_CHEEK_SUCK_RIGHT,
				{ "cheeksuckright", "cheeksuckr" } },
		{ XRFaceTracker::FT_CHEEK_SUCK_LEFT,
				{ "cheeksuckleft", "cheeksuckl" } },
		{ XRFaceTracker::FT_JAW_OPEN,
				{ "jawopen", "jawdrop" } },
		{ XRFaceTracker::FT_MOUTH_CLOSED,
				{ "mouthclosed", "mouthclose", "mouthapeshape", "lipstoward" } },
		{ XRFaceTracker::FT_JAW_RIGHT,
				{ "jawright", "jawsidewaysright" } },
		{ XRFaceTracker::FT_JAW_LEFT,
				{ "jawleft", "jawsidewaysleft" } },
		{ XRFaceTracker::FT_JAW_FORWARD,
				{ "jawforward", "jawthrust" } },
		{ XRFaceTracker::FT_JAW_BACKWARD,
				{ "jawbackward" } },
		{ XRFaceTracker::FT_JAW_CLENCH,
				{ "jawclench" } },
		{ XRFaceTracker::FT_JAW_MANDIBLE_RAISE,
				{ "jawmandibleraise" } },
		{ XRFaceTracker::FT_LIP_SUCK_UPPER_RIGHT,
				{ "lipsuckupperright", "lipsuckrt" } },
		{ XRFaceTracker::FT_LIP_SUCK_UPPER_LEFT,
				{ "lipsuckupperleft", "lipsucklt" } },
		{ XRFaceTracker::FT_LIP_SUCK_LOWER_RIGHT,
				{ "lipsucklowerright", "lipsuckrb" } },
		{ XRFaceTracker::FT_LIP_SUCK_LOWER_LEFT,
				{ "lipsucklowerleft", "lipsucklb" } },
		{ XRFaceTracker::FT_LIP_SUCK_CORNER_RIGHT,
				{ "lipsuckcornerright" } },
		{ XRFaceTracker::FT_LIP_SUCK_CORNER_LEFT,
				{ "lipsuckcornerleft" } },
		{ XRFaceTracker::FT_LIP_FUNNEL_UPPER_RIGHT,
				{ "lipfunnelupperright", "lipfunnelerrt" } },
		{ XRFaceTracker::FT_LIP_FUNNEL_UPPER_LEFT,
				{ "lipfunnelupperleft", "lipfunnelerlt" } },
		{ XRFaceTracker::FT_LIP_FUNNEL_LOWER_RIGHT,
				{ "lipfunnellowerright", "lipsuckrb" } },
		{ XRFaceTracker::FT_LIP_FUNNEL_LOWER_LEFT,
				{ "lipfunnellowerleft", "lipsucklb" } },
		{ XRFaceTracker::FT_LIP_PUCKER_UPPER_RIGHT,
				{ "lippuckerupperright" } },
		{ XRFaceTracker::FT_LIP_PUCKER_UPPER_LEFT,
				{ "lippuckerupperleft" } },
		{ XRFaceTracker::FT_LIP_PUCKER_LOWER_RIGHT,
				{ "lippuckerlowerright" } },
		{ XRFaceTracker::FT_LIP_PUCKER_LOWER_LEFT,
				{ "lippuckerlowerleft" } },
		{ XRFaceTracker::FT_MOUTH_UPPER_UP_RIGHT,
				{ "mouthupperupright", "upperlipraiserr" } },
		{ XRFaceTracker::FT_MOUTH_UPPER_UP_LEFT,
				{ "mouthupperupleft", "upperlipraiserl" } },
		{ XRFaceTracker::FT_MOUTH_LOWER_DOWN_RIGHT,
				{ "mouthlowerdownright", "mouthlowerupright", "lowerlipdepressorr" } },
		{ XRFaceTracker::FT_MOUTH_LOWER_DOWN_LEFT,
				{ "mouthlowerdownleft", "mouthlowerupleft", "lowerlipdepressorl" } },
		{ XRFaceTracker::FT_MOUTH_UPPER_DEEPEN_RIGHT,
				{ "mouthupperdeepenright" } },
		{ XRFaceTracker::FT_MOUTH_UPPER_DEEPEN_LEFT,
				{ "mouthupperdeepenleft" } },
		{ XRFaceTracker::FT_MOUTH_UPPER_RIGHT,
				{ "mouthupperright" } },
		{ XRFaceTracker::FT_MOUTH_UPPER_LEFT,
				{ "mouthupperleft" } },
		{ XRFaceTracker::FT_MOUTH_LOWER_RIGHT,
				{ "mouthlowerright" } },
		{ XRFaceTracker::FT_MOUTH_LOWER_LEFT,
				{ "mouthlowerleft" } },
		{ XRFaceTracker::FT_MOUTH_CORNER_PULL_RIGHT,
				{ "mouthcornerpullright" } },
		{ XRFaceTracker::FT_MOUTH_CORNER_PULL_LEFT,
				{ "mouthcornerpullleft" } },
		{ XRFaceTracker::FT_MOUTH_CORNER_SLANT_RIGHT,
				{ "mouthcornerslantright" } },
		{ XRFaceTracker::FT_MOUTH_CORNER_SLANT_LEFT,
				{ "mouthcornerslantleft" } },
		{ XRFaceTracker::FT_MOUTH_FROWN_RIGHT,
				{ "mouthfrownright", "lipcornerdepressorr" } },
		{ XRFaceTracker::FT_MOUTH_FROWN_LEFT,
				{ "mouthfrownleft", "lipcornerdepressorl" } },
		{ XRFaceTracker::FT_MOUTH_STRETCH_RIGHT,
				{ "mouthstretchright", "lipstretcherr" } },
		{ XRFaceTracker::FT_MOUTH_STRETCH_LEFT,
				{ "mouthstretchleft", "lipstretcherl" } },
		{ XRFaceTracker::FT_MOUTH_DIMPLE_RIGHT,
				{ "mouthdimplerright", "mouthdimpleright", "dimplerr" } },
		{ XRFaceTracker::FT_MOUTH_DIMPLE_LEFT,
				{ "mouthdimplerleft", "mouthdimpleleft", "dimplerl" } },
		{ XRFaceTracker::FT_MOUTH_RAISER_UPPER,
				{ "mouthraiserupper", "mouthshrugupper", "chinraisert" } },
		{ XRFaceTracker::FT_MOUTH_RAISER_LOWER,
				{ "mouthraiserlower", "mouthshruglower", "mouthloweroverlay", "chinraiserb" } },
		{ XRFaceTracker::FT_MOUTH_PRESS_RIGHT,
				{ "mouthpressright", "lippressorr" } },
		{ XRFaceTracker::FT_MOUTH_PRESS_LEFT,
				{ "mouthpressleft", "lippressorl" } },
		{ XRFaceTracker::FT_MOUTH_TIGHTENER_RIGHT,
				{ "mouthtightenerright", "liptightenerr" } },
		{ XRFaceTracker::FT_MOUTH_TIGHTENER_LEFT,
				{ "mouthtightenerleft", "liptightenerl" } },
		{ XRFaceTracker::FT_TONGUE_OUT,
				{ "tongueout", "tonguelongstep2" } },
		{ XRFaceTracker::FT_TONGUE_UP,
				{ "tongueup" } },
		{ XRFaceTracker::FT_TONGUE_DOWN,
				{ "tonguedown" } },
		{ XRFaceTracker::FT_TONGUE_RIGHT,
				{ "tongueright" } },
		{ XRFaceTracker::FT_TONGUE_LEFT,
				{ "tongueleft" } },
		{ XRFaceTracker::FT_TONGUE_ROLL,
				{ "tongueroll" } },
		{ XRFaceTracker::FT_TONGUE_BLEND_DOWN,
				{ "tongueblenddown" } },
		{ XRFaceTracker::FT_TONGUE_CURL_UP,
				{ "tonguecurlup" } },
		{ XRFaceTracker::FT_TONGUE_SQUISH,
				{ "tonguesquish" } },
		{ XRFaceTracker::FT_TONGUE_FLAT,
				{ "tongueflat" } },
		{ XRFaceTracker::FT_TONGUE_TWIST_RIGHT,
				{ "tonguetwistright" } },
		{ XRFaceTracker::FT_TONGUE_TWIST_LEFT,
				{ "tonguetwistleft" } },
		{ XRFaceTracker::FT_SOFT_PALATE_CLOSE,
				{ "softpalateclose" } },
		{ XRFaceTracker::FT_THROAT_SWALLOW,
				{ "throatswallow" } },
		{ XRFaceTracker::FT_NECK_FLEX_RIGHT,
				{ "neckflexright" } },
		{ XRFaceTracker::FT_NECK_FLEX_LEFT,
				{ "neckflexleft" } },
		{ XRFaceTracker::FT_EYE_CLOSED,
				{ "eyeclosed" } },
		{ XRFaceTracker::FT_EYE_WIDE,
				{ "eyewide" } },
		{ XRFaceTracker::FT_EYE_SQUINT,
				{ "eyesquint" } },
		{ XRFaceTracker::FT_EYE_DILATION,
				{ "eyedilation" } },
		{ XRFaceTracker::FT_EYE_CONSTRICT,
				{ "eyeconstrict" } },
		{ XRFaceTracker::FT_BROW_DOWN_RIGHT,
				{ "browdownright", "browlowererr" } },
		{ XRFaceTracker::FT_BROW_DOWN_LEFT,
				{ "browdownleft", "browlowererl" } },
		{ XRFaceTracker::FT_BROW_DOWN,
				{ "browdown" } },
		{ XRFaceTracker::FT_BROW_UP_RIGHT,
				{ "browupright" } },
		{ XRFaceTracker::FT_BROW_UP_LEFT,
				{ "browupleft" } },
		{ XRFaceTracker::FT_BROW_UP,
				{ "browup" } },
		{ XRFaceTracker::FT_NOSE_SNEER,
				{ "nosesneer" } },
		{ XRFaceTracker::FT_NASAL_DILATION,
				{ "nasaldilation" } },
		{ XRFaceTracker::FT_NASAL_CONSTRICT,
				{ "nasalconstrict" } },
		{ XRFaceTracker::FT_CHEEK_PUFF,
				{ "cheekpuff" } },
		{ XRFaceTracker::FT_CHEEK_SUCK,
				{ "cheeksuck" } },
		{ XRFaceTracker::FT_CHEEK_SQUINT,
				{ "cheeksquint" } },
		{ XRFaceTracker::FT_LIP_SUCK_UPPER,
				{ "lipsuckupper", "mouthrollupper", "mouthupperinside" } },
		{ XRFaceTracker::FT_LIP_SUCK_LOWER,
				{ "lipsucklower", "mouthrolllower", "mouthlowerinside" } },
		{ XRFaceTracker::FT_LIP_SUCK,
				{ "lipsuck" } },
		{ XRFaceTracker::FT_LIP_FUNNEL_UPPER,
				{ "lipfunnelupper", "mouthupperoverturn" } },
		{ XRFaceTracker::FT_LIP_FUNNEL_LOWER,
				{ "lipfunnellower", "mouthloweroverturn" } },
		{ XRFaceTracker::FT_LIP_FUNNEL,
				{ "lipfunnel", "mouthfunnel" } },
		{ XRFaceTracker::FT_LIP_PUCKER_UPPER,
				{ "lippuckerupper" } },
		{ XRFaceTracker::FT_LIP_PUCKER_LOWER,
				{ "lippuckerlower" } },
		{ XRFaceTracker::FT_LIP_PUCKER,
				{ "lippucker", "mouthpucker", "mouthpout" } },
		{ XRFaceTracker::FT_MOUTH_UPPER_UP,
				{ "mouthupperup" } },
		{ XRFaceTracker::FT_MOUTH_LOWER_DOWN,
				{ "mouthlowerdown" } },
		{ XRFaceTracker::FT_MOUTH_OPEN,
				{ "mouthopen" } },
		{ XRFaceTracker::FT_MOUTH_RIGHT,
				{ "mouthright" } },
		{ XRFaceTracker::FT_MOUTH_LEFT,
				{ "mouthleft" } },
		{ XRFaceTracker::FT_MOUTH_SMILE_RIGHT,
				{ "mouthsmileright", "lipcornerpullerr" } },
		{ XRFaceTracker::FT_MOUTH_SMILE_LEFT,
				{ "mouthsmileleft", "lipcornerpullerl" } },
		{ XRFaceTracker::FT_MOUTH_SMILE,
				{ "mouthsmile" } },
		{ XRFaceTracker::FT_MOUTH_SAD_RIGHT,
				{ "mouthsadright" } },
		{ XRFaceTracker::FT_MOUTH_SAD_LEFT,
				{ "mouthsadleft" } },
		{ XRFaceTracker::FT_MOUTH_SAD,
				{ "mouthsad" } },
		{ XRFaceTracker::FT_MOUTH_STRETCH,
				{ "mouthstretch" } },
		{ XRFaceTracker::FT_MOUTH_DIMPLE,
				{ "mouthdimple" } },
		{ XRFaceTracker::FT_MOUTH_TIGHTENER,
				{ "mouthtightener" } },
		{ XRFaceTracker::FT_MOUTH_PRESS,
				{ "mouthpress" } }
	};

	// Convert the name to lower-case and strip non-alphanumeric characters.
	const String name = String(p_name).to_lower().remove_char('_');

	// Iterate through the blend map.
	for (const blend_map_entry &entry : blend_map) {
		for (const char *n : entry.name) {
			if (n == nullptr) {
				break;
			}

			if (name == n) {
				return entry.blend;
			}
		}
	}

	// Blend shape not found.
	return -1;
}

// This method adds all the identified XRFaceTracker blend shapes of
// the mesh to the p_blend_mapping map. The map is indexed by the
// XRFaceTracker blend shape, and the value is the index of the mesh
// blend shape.
static void identify_face_blend_shapes(RBMap<int, int> &p_blend_mapping, const Ref<Mesh> &mesh) {
	// Find all blend shapes.
	const int count = mesh->get_blend_shape_count();
	for (int i = 0; i < count; i++) {
		const int blend = find_face_blend_shape(mesh->get_blend_shape_name(i));
		if (blend >= 0) {
			p_blend_mapping[blend] = i;
		}
	}
}

// This method removes any unified blend shapes from the p_blend_mapping map
// if all the individual blend shapes are found and going to be driven.
static void remove_driven_unified_blend_shapes(RBMap<int, int> &p_blend_mapping) {
	// Entry for unified blend table.
	struct unified_blend_entry {
		int unified;
		int individual[4];
	};

	// Table of unified blend shapes.
	//
	// This table consists of:
	// - The XRFaceTracker unified blend shape.
	// - The individual blend shapes that make up the unified blend shape.
	static constexpr unified_blend_entry unified_blends[] = {
		{ XRFaceTracker::FT_EYE_CLOSED,
				{ XRFaceTracker::FT_EYE_CLOSED_RIGHT, XRFaceTracker::FT_EYE_CLOSED_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_EYE_WIDE,
				{ XRFaceTracker::FT_EYE_WIDE_RIGHT, XRFaceTracker::FT_EYE_WIDE_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_EYE_SQUINT,
				{ XRFaceTracker::FT_EYE_SQUINT_RIGHT, XRFaceTracker::FT_EYE_SQUINT_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_EYE_DILATION,
				{ XRFaceTracker::FT_EYE_DILATION_RIGHT, XRFaceTracker::FT_EYE_DILATION_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_EYE_CONSTRICT,
				{ XRFaceTracker::FT_EYE_CONSTRICT_RIGHT, XRFaceTracker::FT_EYE_CONSTRICT_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_BROW_DOWN_RIGHT,
				{ XRFaceTracker::FT_BROW_LOWERER_RIGHT, XRFaceTracker::FT_BROW_PINCH_RIGHT, -1, -1 } },
		{ XRFaceTracker::FT_BROW_DOWN_LEFT,
				{ XRFaceTracker::FT_BROW_LOWERER_LEFT, XRFaceTracker::FT_BROW_PINCH_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_BROW_DOWN,
				{ XRFaceTracker::FT_BROW_LOWERER_RIGHT, XRFaceTracker::FT_BROW_PINCH_RIGHT, XRFaceTracker::FT_BROW_LOWERER_LEFT, XRFaceTracker::FT_BROW_PINCH_LEFT } },
		{ XRFaceTracker::FT_BROW_UP_RIGHT,
				{ XRFaceTracker::FT_BROW_INNER_UP_RIGHT, XRFaceTracker::FT_BROW_OUTER_UP_RIGHT, -1, -1 } },
		{ XRFaceTracker::FT_BROW_UP_LEFT,
				{ XRFaceTracker::FT_BROW_INNER_UP_LEFT, XRFaceTracker::FT_BROW_OUTER_UP_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_BROW_UP,
				{ XRFaceTracker::FT_BROW_INNER_UP_RIGHT, XRFaceTracker::FT_BROW_OUTER_UP_RIGHT, XRFaceTracker::FT_BROW_INNER_UP_LEFT, XRFaceTracker::FT_BROW_OUTER_UP_LEFT } },
		{ XRFaceTracker::FT_NOSE_SNEER,
				{ XRFaceTracker::FT_NOSE_SNEER_RIGHT, XRFaceTracker::FT_NOSE_SNEER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_NASAL_DILATION,
				{ XRFaceTracker::FT_NASAL_DILATION_RIGHT, XRFaceTracker::FT_NASAL_DILATION_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_NASAL_CONSTRICT,
				{ XRFaceTracker::FT_NASAL_CONSTRICT_RIGHT, XRFaceTracker::FT_NASAL_CONSTRICT_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_CHEEK_PUFF,
				{ XRFaceTracker::FT_CHEEK_PUFF_RIGHT, XRFaceTracker::FT_CHEEK_PUFF_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_CHEEK_SUCK,
				{ XRFaceTracker::FT_CHEEK_SUCK_RIGHT, XRFaceTracker::FT_CHEEK_SUCK_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_CHEEK_SQUINT,
				{ XRFaceTracker::FT_CHEEK_SQUINT_RIGHT, XRFaceTracker::FT_CHEEK_SQUINT_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_LIP_SUCK_UPPER,
				{ XRFaceTracker::FT_LIP_SUCK_UPPER_RIGHT, XRFaceTracker::FT_LIP_SUCK_UPPER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_LIP_SUCK_LOWER,
				{ XRFaceTracker::FT_LIP_SUCK_LOWER_RIGHT, XRFaceTracker::FT_LIP_SUCK_LOWER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_LIP_SUCK,
				{ XRFaceTracker::FT_LIP_SUCK_UPPER_RIGHT, XRFaceTracker::FT_LIP_SUCK_UPPER_LEFT, XRFaceTracker::FT_LIP_SUCK_LOWER_RIGHT, XRFaceTracker::FT_LIP_SUCK_LOWER_LEFT } },
		{ XRFaceTracker::FT_LIP_FUNNEL_UPPER,
				{ XRFaceTracker::FT_LIP_FUNNEL_UPPER_RIGHT, XRFaceTracker::FT_LIP_FUNNEL_UPPER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_LIP_FUNNEL_LOWER,
				{ XRFaceTracker::FT_LIP_FUNNEL_LOWER_RIGHT, XRFaceTracker::FT_LIP_FUNNEL_LOWER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_LIP_FUNNEL,
				{ XRFaceTracker::FT_LIP_FUNNEL_UPPER_RIGHT, XRFaceTracker::FT_LIP_FUNNEL_UPPER_LEFT, XRFaceTracker::FT_LIP_FUNNEL_LOWER_RIGHT, XRFaceTracker::FT_LIP_FUNNEL_LOWER_LEFT } },
		{ XRFaceTracker::FT_LIP_PUCKER_UPPER,
				{ XRFaceTracker::FT_LIP_PUCKER_UPPER_RIGHT, XRFaceTracker::FT_LIP_PUCKER_UPPER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_LIP_PUCKER_LOWER,
				{ XRFaceTracker::FT_LIP_PUCKER_LOWER_RIGHT, XRFaceTracker::FT_LIP_PUCKER_LOWER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_LIP_PUCKER,
				{ XRFaceTracker::FT_LIP_PUCKER_UPPER_RIGHT, XRFaceTracker::FT_LIP_PUCKER_UPPER_LEFT, XRFaceTracker::FT_LIP_PUCKER_LOWER_RIGHT, XRFaceTracker::FT_LIP_PUCKER_LOWER_LEFT } },
		{ XRFaceTracker::FT_MOUTH_UPPER_UP,
				{ XRFaceTracker::FT_MOUTH_UPPER_UP_RIGHT, XRFaceTracker::FT_MOUTH_UPPER_UP_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_LOWER_DOWN,
				{ XRFaceTracker::FT_MOUTH_LOWER_DOWN_RIGHT, XRFaceTracker::FT_MOUTH_LOWER_DOWN_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_OPEN,
				{ XRFaceTracker::FT_MOUTH_UPPER_UP_RIGHT, XRFaceTracker::FT_MOUTH_UPPER_UP_LEFT, XRFaceTracker::FT_MOUTH_LOWER_DOWN_RIGHT, XRFaceTracker::FT_MOUTH_LOWER_DOWN_LEFT } },
		{ XRFaceTracker::FT_MOUTH_RIGHT,
				{ XRFaceTracker::FT_MOUTH_UPPER_RIGHT, XRFaceTracker::FT_MOUTH_LOWER_RIGHT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_LEFT,
				{ XRFaceTracker::FT_MOUTH_UPPER_LEFT, XRFaceTracker::FT_MOUTH_LOWER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_SMILE_RIGHT,
				{ XRFaceTracker::FT_MOUTH_CORNER_PULL_RIGHT, XRFaceTracker::FT_MOUTH_CORNER_SLANT_RIGHT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_SMILE_LEFT,
				{ XRFaceTracker::FT_MOUTH_CORNER_PULL_LEFT, XRFaceTracker::FT_MOUTH_CORNER_SLANT_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_SMILE,
				{ XRFaceTracker::FT_MOUTH_CORNER_PULL_RIGHT, XRFaceTracker::FT_MOUTH_CORNER_SLANT_RIGHT, XRFaceTracker::FT_MOUTH_CORNER_PULL_LEFT, XRFaceTracker::FT_MOUTH_CORNER_SLANT_LEFT } },
		{ XRFaceTracker::FT_MOUTH_SAD_RIGHT,
				{ XRFaceTracker::FT_MOUTH_FROWN_RIGHT, XRFaceTracker::FT_MOUTH_STRETCH_RIGHT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_SAD_LEFT,
				{ XRFaceTracker::FT_MOUTH_FROWN_LEFT, XRFaceTracker::FT_MOUTH_STRETCH_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_SAD,
				{ XRFaceTracker::FT_MOUTH_FROWN_RIGHT, XRFaceTracker::FT_MOUTH_STRETCH_RIGHT, XRFaceTracker::FT_MOUTH_FROWN_LEFT, XRFaceTracker::FT_MOUTH_STRETCH_LEFT } },
		{ XRFaceTracker::FT_MOUTH_STRETCH,
				{ XRFaceTracker::FT_MOUTH_STRETCH_RIGHT, XRFaceTracker::FT_MOUTH_STRETCH_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_DIMPLE,
				{ XRFaceTracker::FT_MOUTH_DIMPLE_RIGHT, XRFaceTracker::FT_MOUTH_DIMPLE_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_TIGHTENER,
				{ XRFaceTracker::FT_MOUTH_TIGHTENER_RIGHT, XRFaceTracker::FT_MOUTH_TIGHTENER_LEFT, -1, -1 } },
		{ XRFaceTracker::FT_MOUTH_PRESS,
				{ XRFaceTracker::FT_MOUTH_PRESS_RIGHT, XRFaceTracker::FT_MOUTH_PRESS_LEFT, -1, -1 } }
	};

	// Remove unified blend shapes if individual blend shapes are found.
	for (const unified_blend_entry &entry : unified_blends) {
		// Check if all individual blend shapes are found.
		bool found = true;
		for (const int i : entry.individual) {
			if (i >= 0 && !p_blend_mapping.find(i)) {
				found = false;
				break;
			}
		}

		// If all individual blend shapes are found then remove the unified blend shape.
		if (found) {
			p_blend_mapping.erase(entry.unified);
		}
	}
}

void XRFaceModifier3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_face_tracker", "tracker_name"), &XRFaceModifier3D::set_face_tracker);
	ClassDB::bind_method(D_METHOD("get_face_tracker"), &XRFaceModifier3D::get_face_tracker);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "face_tracker", PROPERTY_HINT_ENUM_SUGGESTION, "/user/face_tracker"), "set_face_tracker", "get_face_tracker");

	ClassDB::bind_method(D_METHOD("set_target", "target"), &XRFaceModifier3D::set_target);
	ClassDB::bind_method(D_METHOD("get_target"), &XRFaceModifier3D::get_target);
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "target", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "MeshInstance3D"), "set_target", "get_target");
}

void XRFaceModifier3D::set_face_tracker(const StringName &p_tracker_name) {
	tracker_name = p_tracker_name;
}

StringName XRFaceModifier3D::get_face_tracker() const {
	return tracker_name;
}

void XRFaceModifier3D::set_target(const NodePath &p_target) {
	target = p_target;

	if (is_inside_tree()) {
		_get_blend_data();
	}
}

NodePath XRFaceModifier3D::get_target() const {
	return target;
}

MeshInstance3D *XRFaceModifier3D::get_mesh_instance() const {
	if (!has_node(target)) {
		return nullptr;
	}

	Node *node = get_node(target);
	if (!node) {
		return nullptr;
	}

	return Object::cast_to<MeshInstance3D>(node);
}

void XRFaceModifier3D::_get_blend_data() {
	// This method constructs the blend mapping from the XRFaceTracker
	// blend shapes to the available blend shapes of the target mesh. It does this
	// by:
	//
	// 1. Identifying the blend shapes of the target mesh and identifying what
	//    XRFaceTracker blend shape they correspond to. The results are
	//    placed in the blend_mapping map.
	// 2. Prevent over-driving facial blend-shapes by removing any unified blend
	//    shapes from the map if all the individual blend shapes are already
	//    found and going to be driven.

	blend_mapping.clear();

	// Get the target MeshInstance3D.
	const MeshInstance3D *mesh_instance = get_mesh_instance();
	if (!mesh_instance) {
		return;
	}

	// Get the mesh.
	const Ref<Mesh> mesh = mesh_instance->get_mesh();
	if (mesh.is_null()) {
		return;
	}

	// Identify all face blend shapes and populate the map.
	identify_face_blend_shapes(blend_mapping, mesh);

	// Remove the unified blend shapes if all the individual blend shapes are found.
	remove_driven_unified_blend_shapes(blend_mapping);
}

void XRFaceModifier3D::_update_face_blends() const {
	// Get the XR Server.
	const XRServer *xr_server = XRServer::get_singleton();
	if (!xr_server) {
		return;
	}

	// Get the face tracker.
	const Ref<XRFaceTracker> tracker = xr_server->get_tracker(tracker_name);
	if (tracker.is_null()) {
		return;
	}

	// Get the face mesh.
	MeshInstance3D *mesh_instance = get_mesh_instance();
	if (!mesh_instance) {
		return;
	}

	// Get the blend weights.
	const PackedFloat32Array weights = tracker->get_blend_shapes();

	// Apply all the face blend weights to the mesh.
	for (const KeyValue<int, int> &it : blend_mapping) {
		mesh_instance->set_blend_shape_value(it.value, weights[it.key]);
	}
}

void XRFaceModifier3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			_get_blend_data();
			set_process_internal(true);
		} break;
		case NOTIFICATION_EXIT_TREE: {
			set_process_internal(false);
			blend_mapping.clear();
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			_update_face_blends();
		} break;
		default: {
		} break;
	}
}
