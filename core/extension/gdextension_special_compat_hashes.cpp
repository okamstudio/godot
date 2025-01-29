/**************************************************************************/
/*  gdextension_special_compat_hashes.cpp                                 */
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

#include "gdextension_special_compat_hashes.h"

#ifndef DISABLE_DEPRECATED

#include "core/object/class_db.h"
#include "core/variant/variant.h"

HashMap<StringName, LocalVector<GDExtensionSpecialCompatHashes::Mapping>> GDExtensionSpecialCompatHashes::mappings;

bool GDExtensionSpecialCompatHashes::lookup_current_hash(const StringName &p_class, const StringName &p_method, uint32_t p_legacy_hash, uint32_t *r_current_hash) {
	LocalVector<Mapping> *methods = mappings.getptr(p_class);
	if (!methods) {
		return false;
	}

	for (const Mapping &mapping : *methods) {
		if (mapping.method == p_method && mapping.legacy_hash == p_legacy_hash) {
			*r_current_hash = mapping.current_hash;
			return true;
		}
	}

	return false;
}

bool GDExtensionSpecialCompatHashes::get_legacy_hashes(const StringName &p_class, const StringName &p_method, Array &r_hashes, bool p_check_valid) {
	LocalVector<Mapping> *methods = mappings.getptr(p_class);
	if (!methods) {
		return false;
	}

	bool found = false;
	for (const Mapping &mapping : *methods) {
		if (mapping.method == p_method) {
			if (p_check_valid) {
				MethodBind *mb = ClassDB::get_method_with_compatibility(p_class, p_method, mapping.current_hash);
				if (!mb) {
					WARN_PRINT(vformat("Compatibility hash %d for %s::%s() mapped to non-existent hash %d. Please update gdextension_special_compat_hashes.cpp.", mapping.legacy_hash, p_class, p_method, mapping.current_hash));
					continue;
				}
			}
			r_hashes.push_back(mapping.legacy_hash);
			found = true;
		}
	}

	return found;
}

void GDExtensionSpecialCompatHashes::initialize() {
	// clang-format off
	mappings.insert("AESContext", {
		{ "start", 3167574919, 3122411423 },
	});
	mappings.insert("AStar2D", {
		{ "add_point", 3370185124, 4074201818 },
		{ "set_point_disabled", 4023243586, 972357352 },
		{ "connect_points", 3785370599, 3710494224 },
		{ "disconnect_points", 3785370599, 3710494224 },
		{ "are_points_connected", 4063588998, 2288175859 },
	});
	mappings.insert("AStar3D", {
		{ "add_point", 2920922839, 1038703438 },
		{ "set_point_disabled", 4023243586, 972357352 },
		{ "connect_points", 3785370599, 3710494224 },
		{ "disconnect_points", 3785370599, 3710494224 },
		{ "are_points_connected", 4063588998, 2288175859 },
	});
	mappings.insert("AStarGrid2D", {
		{ "set_point_solid", 2825551965, 1765703753 },
		{ "fill_solid_region", 1152863744, 2261970063 },
	});
	mappings.insert("AcceptDialog", {
		{ "add_button", 4158837846, 3328440682 },
	});
	mappings.insert("AnimatedSprite2D", {
		{ "play", 2372066587, 3269405555 },
		{ "play_backwards", 1421762485, 3323268493 },
	});
	mappings.insert("AnimatedSprite3D", {
		{ "play", 2372066587, 3269405555 },
		{ "play_backwards", 1421762485, 3323268493 },
	});
	mappings.insert("Animation", {
		{ "add_track", 2393815928, 3843682357 },
		{ "track_insert_key", 1985425300, 808952278 },
		{ "track_find_key", 3898229885, 3245197284 },
#ifdef REAL_T_IS_DOUBLE
		{ "bezier_track_insert_key", 1057544502, 3767441357 },
#else
		{ "bezier_track_insert_key", 1057544502, 3656773645 },
#endif
		{ "bezier_track_set_key_in_handle", 1028302688, 1719223284 },
		{ "bezier_track_set_key_out_handle", 1028302688, 1719223284 },
		{ "audio_track_insert_key", 3489962123, 4021027286 },
	});
	mappings.insert("AnimationNode", {
		{ "blend_animation", 11797022, 1630801826 },
		{ "blend_node", 263389446, 1746075988 },
		{ "blend_input", 2709059328, 1361527350 },
	});
	mappings.insert("AnimationNodeBlendSpace1D", {
		{ "add_blend_point", 4069484420, 285050433 },
	});
	mappings.insert("AnimationNodeBlendSpace2D", {
		{ "add_blend_point", 1533588937, 402261981 },
		{ "add_triangle", 642454959, 753017335 },
	});
	mappings.insert("AnimationNodeBlendTree", {
#ifdef REAL_T_IS_DOUBLE
		{ "add_node", 2055804584, 1407702499 },
#else
		{ "add_node", 2055804584, 1980270704 },
#endif
	});
	mappings.insert("AnimationNodeStateMachine", {
#ifdef REAL_T_IS_DOUBLE
		{ "add_node", 2055804584, 1407702499 },
#else
		{ "add_node", 2055804584, 1980270704 },
#endif
	});
	mappings.insert("AnimationNodeStateMachinePlayback", {
		{ "travel", 3683006648, 3823612587 },
		{ "start", 3683006648, 3823612587 },
	});
	mappings.insert("AnimationPlayer", {
		{ "play", 3697947785, 3118260607 },
		{ "play", 2221377757, 3118260607 },
		{ "play_backwards", 3890664824, 2787282401 },
		{ "play_with_capture", 3180464118, 1572969103 },
	});
	mappings.insert("ArrayMesh", {
		{ "add_surface_from_arrays", 172284304, 1796411378 },
	});
	mappings.insert("AudioEffectSpectrumAnalyzerInstance", {
		{ "get_magnitude_for_frequency_range", 2693213071, 797993915 },
	});
	mappings.insert("AudioServer", {
		{ "add_bus_effect", 4147765248, 4068819785 },
		{ "get_bus_effect_instance", 2887144608, 1829771234 },
	});
	mappings.insert("AudioStreamPlaybackPolyphonic", {
		{ "play_stream", 3792189967, 604492179 },
	});
	mappings.insert("AudioStreamRandomizer", {
		{ "add_stream", 3197802065, 1892018854 },
	});
	mappings.insert("BitMap", {
		{ "create_from_image_alpha", 505265891, 106271684 },
		{ "opaque_to_polygons", 876132484, 48478126 },
	});
	mappings.insert("CanvasItem", {
		{ "draw_line", 2516941890, 1562330099 },
		{ "draw_dashed_line", 2175215884, 684651049 },
		{ "draw_polyline", 4175878946, 3797364428 },
		{ "draw_polyline_colors", 2239164197, 2311979562 },
		{ "draw_arc", 3486841771, 4140652635 },
		{ "draw_multiline", 4230657331, 2239075205 },
		{ "draw_multiline_colors", 235933050, 4072951537 },
		{ "draw_rect", 84391229, 2417231121 },
		{ "draw_texture", 1695860435, 520200117 },
		{ "draw_texture_rect", 3204081724, 3832805018 },
		{ "draw_texture_rect_region", 3196597532, 3883821411 },
		{ "draw_msdf_texture_rect_region", 2672026175, 4219163252 },
		{ "draw_lcd_texture_rect_region", 169610548, 3212350954 },
		{ "draw_primitive", 2248678295, 3288481815 },
		{ "draw_polygon", 2683625537, 974537912 },
		{ "draw_colored_polygon", 1659099617, 15245644 },
		{ "draw_string", 2552080639, 728290553 },
		{ "draw_multiline_string", 4002645436, 1927038192 },
		{ "draw_string_outline", 850005221, 340562381 },
		{ "draw_multiline_string_outline", 3717870722, 1912318525 },
		{ "draw_char", 2329089032, 3339793283 },
		{ "draw_char_outline", 419453826, 3302344391 },
#ifdef REAL_T_IS_DOUBLE
		{ "draw_mesh", 1634855856, 4036154158 },
		{ "draw_set_transform", 3283884939, 156553079 },
#else
		{ "draw_mesh", 1634855856, 153818295 },
		{ "draw_set_transform", 3283884939, 288975085 },
#endif
		{ "draw_animation_slice", 2295343543, 3112831842 },
	});
	mappings.insert("CodeEdit", {
		{ "is_in_string", 3294126239, 688195400 },
		{ "is_in_comment", 3294126239, 688195400 },
		{ "add_code_completion_option", 1629240608, 947964390 },
	});
	mappings.insert("Control", {
		{ "set_offsets_preset", 3651818904, 3724524307 },
		{ "set_anchors_and_offsets_preset", 3651818904, 3724524307 },
		{ "set_anchor", 2589937826, 2302782885 },
		{ "get_theme_icon", 2336455395, 3163973443 },
		{ "get_theme_stylebox", 2759935355, 604739069 },
		{ "get_theme_font", 387378635, 2826986490 },
		{ "get_theme_font_size", 229578101, 1327056374 },
		{ "get_theme_color", 2377051548, 2798751242 },
		{ "get_theme_constant", 229578101, 1327056374 },
		{ "has_theme_icon", 1187511791, 866386512 },
		{ "has_theme_stylebox", 1187511791, 866386512 },
		{ "has_theme_font", 1187511791, 866386512 },
		{ "has_theme_font_size", 1187511791, 866386512 },
		{ "has_theme_color", 1187511791, 866386512 },
		{ "has_theme_constant", 1187511791, 866386512 },
	});
	mappings.insert("Crypto", {
		{ "generate_self_signed_certificate", 947314696, 492266173 },
	});
	mappings.insert("Curve", {
		{ "add_point", 2766148617, 434072736 },
	});
	mappings.insert("Curve2D", {
#ifdef REAL_T_IS_DOUBLE
		{ "add_point", 529706502, 3343370600 },
#else
		{ "add_point", 2437345566, 4175465202 },
#endif
	});
	mappings.insert("Curve3D", {
#ifdef REAL_T_IS_DOUBLE
		{ "add_point", 3544159631, 917388502 },
#else
		{ "add_point", 3836314258, 2931053748 },
#endif
	});
	mappings.insert("DirAccess", {
		{ "list_dir_begin", 2018049411, 166280745 },
		{ "list_dir_begin", 2610976713, 166280745 },
		{ "copy", 198434953, 1063198817 },
		{ "copy_absolute", 198434953, 1063198817 },
	});
	mappings.insert("DisplayServer", {
		{ "global_menu_add_submenu_item", 3806306913, 2828985934 },
		{ "global_menu_add_item", 3415468211, 3616842746 },
		{ "global_menu_add_item", 3401266716, 3616842746 },
		{ "global_menu_add_check_item", 3415468211, 3616842746 },
		{ "global_menu_add_check_item", 3401266716, 3616842746 },
		{ "global_menu_add_icon_item", 1700867534, 3867083847 },
		{ "global_menu_add_icon_item", 4245856523, 3867083847 },
		{ "global_menu_add_icon_check_item", 1700867534, 3867083847 },
		{ "global_menu_add_icon_check_item", 4245856523, 3867083847 },
		{ "global_menu_add_radio_check_item", 3415468211, 3616842746 },
		{ "global_menu_add_radio_check_item", 3401266716, 3616842746 },
		{ "global_menu_add_icon_radio_check_item", 1700867534, 3867083847 },
		{ "global_menu_add_icon_radio_check_item", 4245856523, 3867083847 },
		{ "global_menu_add_multistate_item", 635750054, 3297554655 },
		{ "global_menu_add_multistate_item", 3431222859, 3297554655 },
		{ "global_menu_add_separator", 1041533178, 3214812433 },
		{ "tts_speak", 3741216677, 903992738 },
		{ "is_touchscreen_available", 4162880507, 36873697 },
		{ "is_touchscreen_available", 3323674545, 36873697 },
		{ "screen_set_orientation", 2629526904, 2211511631 },
		{ "window_get_native_handle", 2709193271, 1096425680 },
		{ "window_set_title", 3043792800, 441246282 },
		{ "window_set_mouse_passthrough", 3958815166, 1993637420 },
		{ "window_set_current_screen", 3023605688, 2230941749 },
		{ "window_set_position", 3614040015, 2019273902 },
		{ "window_set_size", 3614040015, 2019273902 },
		{ "window_set_rect_changed_callback", 3653650673, 1091192925 },
		{ "window_set_window_event_callback", 3653650673, 1091192925 },
		{ "window_set_input_event_callback", 3653650673, 1091192925 },
		{ "window_set_input_text_callback", 3653650673, 1091192925 },
		{ "window_set_drop_files_callback", 3653650673, 1091192925 },
		{ "window_set_max_size", 3614040015, 2019273902 },
		{ "window_set_min_size", 3614040015, 2019273902 },
		{ "window_set_mode", 2942569511, 1319965401 },
		{ "window_set_flag", 3971592565, 254894155 },
		{ "window_get_flag", 2662949986, 802816991 },
		{ "window_set_window_buttons_offset", 3614040015, 2019273902 },
		{ "window_set_ime_active", 450484987, 1661950165 },
		{ "window_set_ime_position", 3614040015, 2019273902 },
		{ "window_set_vsync_mode", 1708924624, 2179333492 },
#ifdef REAL_T_IS_DOUBLE
		{ "cursor_set_custom_image", 1358907026, 4163678968 },
		{ "virtual_keyboard_show", 384539973, 1323934605 },
#else
		{ "cursor_set_custom_image", 1358907026, 1816663697 },
		{ "virtual_keyboard_show", 860410478, 3042891259 },
#endif
	});
	mappings.insert("EditorExportPlatform", {
		{ "export_project_files", 425454869, 1063735070 },
	});
	mappings.insert("EditorProperty", {
		{ "emit_changed", 3069422438, 1822500399 },
	});
	mappings.insert("ENetConnection", {
		{ "create_host_bound", 866250949, 1515002313 },
		{ "connect_to_host", 385984708, 2171300490 },
		{ "dtls_client_setup", 3097527179, 1966198364 },
	});
	mappings.insert("ENetMultiplayerPeer", {
		{ "create_server", 1616151701, 2917761309 },
		{ "create_client", 920217784, 2327163476 },
	});
	mappings.insert("EditorCommandPalette", {
		{ "add_command", 3664614892, 864043298 },
	});
	mappings.insert("EditorDebuggerSession", {
		{ "send_message", 3780025912, 85656714 },
		{ "toggle_profiler", 35674246, 1198443697 },
	});
	mappings.insert("EditorFileDialog", {
		{ "add_filter", 233059325, 3388804757 },
	});
	mappings.insert("EditorImportPlugin", {
		{ "append_import_external_resource", 3645925746, 320493106 },
	});
	mappings.insert("EditorInterface", {
		{ "popup_dialog", 2478844058, 2015770942 },
		{ "popup_dialog_centered", 1723337679, 346557367 },
		{ "popup_dialog_centered_ratio", 1310934579, 2093669136 },
		{ "popup_dialog_centered_clamped", 3433759678, 3763385571 },
		{ "inspect_object", 2564140749, 127962172 },
		{ "edit_script", 3664508569, 219829402 },
		{ "save_scene_as", 1168363258, 3647332257 },
	});
	mappings.insert("EditorNode3DGizmo", {
		{ "add_lines", 302451090, 2910971437 },
	#ifdef REAL_T_IS_DOUBLE
		{ "add_mesh", 3332776472, 2161761131 },
	#else
		{ "add_mesh", 1868867708, 1579955111 },
	#endif
		{ "add_unscaled_billboard", 3719733075, 520007164 },
	});
	mappings.insert("EditorNode3DGizmoPlugin", {
		{ "create_icon_material", 2976007329, 3804976916 },
		{ "get_material", 3501703615, 974464017 },
	});
	mappings.insert("EditorScenePostImportPlugin", {
		{ "add_import_option_advanced", 3774155785, 3674075649 },
	});
	mappings.insert("EditorUndoRedoManager", {
		{ "create_action", 3577985681, 2107025470 },
	});
	mappings.insert("EngineDebugger", {
		{ "profiler_enable", 438160728, 3192561009 },
	});
	mappings.insert("Expression", {
		{ "parse", 3658149758, 3069722906 },
	});
	mappings.insert("FileAccess", {
		{ "open_compressed", 2874458257, 3686439335 },
		{ "store_csv_line", 2217842308, 2173791505 },
	});
	mappings.insert("FileDialog", {
		{ "add_filter", 233059325, 3388804757 },
	});
	mappings.insert("Font", {
		{ "get_string_size", 3678918099, 1868866121 },
		{ "get_multiline_string_size", 2427690650, 519636710 },
		{ "draw_string", 2565402639, 1983721962 },
		{ "draw_multiline_string", 348869189, 1171506176 },
		{ "draw_string_outline", 657875837, 623754045 },
		{ "draw_multiline_string_outline", 1649790182, 3206388178 },
		{ "draw_char", 1462476057, 3815617597 },
		{ "draw_char_outline", 4161008124, 209525354 },
	#ifdef REAL_T_IS_DOUBLE
		{ "find_variation", 625117670, 2196349508 },
	#else
		{ "find_variation", 1222433716, 3344325384 },
		// Pre-existing compatibility hash.
		{ "find_variation", 1149405976, 1851767612 },
	#endif
	});
	mappings.insert("GLTFDocument", {
		{ "append_from_file", 1862991421, 866380864 },
		{ "append_from_buffer", 2818062664, 1616081266 },
		{ "append_from_scene", 374125375, 1622574258 },
		{ "generate_scene", 2770277081, 596118388 },
	});
	mappings.insert("Geometry2D", {
		{ "offset_polygon", 3837618924, 1275354010 },
		{ "offset_polyline", 328033063, 2328231778 },
	});
	mappings.insert("Geometry3D", {
		{ "build_cylinder_planes", 3142160516, 449920067 },
		{ "build_capsule_planes", 410870045, 2113592876 },
	});
	mappings.insert("GraphNode", {
		{ "set_slot", 902131739, 2873310869 },
	});
	mappings.insert("GridMap", {
		{ "set_cell_item", 4177201334, 3449088946 },
	});
	mappings.insert("HTTPClient", {
		{ "connect_to_host", 1970282951, 504540374 },
		{ "request", 3249905507, 3778990155 },
	});
	mappings.insert("HTTPRequest", {
		{ "request", 2720304520, 3215244323 },
		{ "request_raw", 4282724657, 2714829993 },
	});
	mappings.insert("IP", {
		{ "resolve_hostname", 396864159, 4283295457 },
		{ "resolve_hostname_addresses", 3462780090, 773767525 },
		{ "resolve_hostname_queue_item", 3936392508, 1749894742 },
	});
	mappings.insert("Image", {
		{ "resize", 2461393748, 994498151 },
		{ "save_jpg", 578836491, 2800019068 },
		{ "save_webp", 3594949219, 2781156876 },
		{ "compress", 4094210332, 2975424957 },
		{ "compress_from_channels", 279105990, 4212890953 },
		{ "load_svg_from_buffer", 1822513750, 311853421 },
		{ "load_svg_from_string", 1461766635, 3254053600 },
	});
	mappings.insert("ImmediateMesh", {
		{ "surface_begin", 3716480242, 2794442543 },
	});
	mappings.insert("ImporterMesh", {
		{ "add_surface", 4122361985, 1740448849 },
	});
	mappings.insert("Input", {
		{ "get_vector", 1517139831, 2479607902 },
		{ "start_joy_vibration", 1890603622, 2576575033 },
		{ "action_press", 573731101, 1713091165 },
#ifdef REAL_T_IS_DOUBLE
		{ "set_custom_mouse_cursor", 3489634142, 1277868338 },
#else
		{ "set_custom_mouse_cursor", 3489634142, 703945977 },
#endif
	});
	mappings.insert("InputEvent", {
		{ "is_match", 3392494811, 1754951977 },
#ifdef REAL_T_IS_DOUBLE
		{ "xformed_by", 2747409789, 3242949850 },
#else
		{ "xformed_by", 2747409789, 1282766827 },
#endif
	});
	mappings.insert("InputMap", {
		{ "add_action", 573731101, 4100757082 },
	});
	mappings.insert("ItemList", {
		{ "add_item", 4086250691, 359861678 },
		{ "add_icon_item", 3332687421, 4256579627 },
		{ "get_item_rect", 1501513492, 159227807 },
		{ "select", 4023243586, 972357352 },
	});
	mappings.insert("JSON", {
		{ "stringify", 2656701787, 462733549 },
	});
	mappings.insert("JavaScriptBridge", {
		{ "download_buffer", 4123979296, 3352272093 },
	});
	mappings.insert("Line2D", {
		{ "add_point", 468506575, 2654014372 },
	});
	mappings.insert("MultiplayerAPI", {
		{ "rpc", 1833408346, 2077486355 },
	});
	mappings.insert("NativeMenu", {
		{ "add_item", 2553375659, 980552939 },
		{ "add_check_item", 2553375659, 980552939 },
		{ "add_icon_item", 2987595282, 1372188274 },
		{ "add_icon_check_item", 2987595282, 1372188274 },
		{ "add_radio_check_item", 2553375659, 980552939 },
		{ "add_icon_radio_check_item", 2987595282, 1372188274 },
		{ "add_multistate_item", 1558592568, 2674635658 },
	});
	mappings.insert("NavigationMeshGenerator", {
		{ "parse_source_geometry_data", 3703028813, 3172802542 },
		{ "parse_source_geometry_data", 685862123, 3172802542 },
		{ "bake_from_source_geometry_data", 3669016597, 1286748856 },
		{ "bake_from_source_geometry_data", 2469318639, 1286748856 },
	});
	mappings.insert("NavigationServer2D", {
		{ "map_get_path", 56240621, 3146466012 },
		{ "parse_source_geometry_data", 1176164995, 1766905497 },
		{ "bake_from_source_geometry_data", 2909414286, 2179660022 },
		{ "bake_from_source_geometry_data_async", 2909414286, 2179660022 },
	});
	mappings.insert("NavigationServer3D", {
		{ "map_get_path", 2121045993, 1187418690 },
		{ "parse_source_geometry_data", 3703028813, 3172802542 },
		{ "parse_source_geometry_data", 685862123, 3172802542 },
		{ "bake_from_source_geometry_data", 3669016597, 1286748856 },
		{ "bake_from_source_geometry_data", 2469318639, 1286748856 },
		{ "bake_from_source_geometry_data_async", 3669016597, 1286748856 },
		{ "bake_from_source_geometry_data_async", 2469318639, 1286748856 },
	});
	mappings.insert("Node", {
		{ "add_child", 3070154285, 3863233950 },
		{ "reparent", 2570952461, 3685795103 },
		{ "find_child", 4253159453, 2008217037 },
		{ "find_children", 1585018254, 2560337219 },
		{ "propagate_call", 1667910434, 1871007965 },
		{ "set_multiplayer_authority", 4023243586, 972357352 },
	});
	mappings.insert("Node3D", {
#ifdef REAL_T_IS_DOUBLE
		{ "look_at", 136915519, 819337406 },
		{ "look_at_from_position", 4067663783, 1809580162 },
#else
		{ "look_at", 3123400617, 2882425029 },
		{ "look_at_from_position", 4067663783, 2086826090 },
#endif
	});
	mappings.insert("Noise", {
		{ "get_image", 2569233413, 3180683109 },
		{ "get_seamless_image", 2210827790, 2770743602 },
		{ "get_image_3d", 2358868431, 3977814329 },
		{ "get_seamless_image_3d", 3328694319, 451006340 },
	});
	mappings.insert("OS", {
		{ "alert", 233059325, 1783970740 },
		{ "get_system_font_path", 2262142305, 626580860 },
		{ "get_system_font_path_for_text", 3824042574, 197317981 },
		{ "execute", 2881709059, 1488299882 },
		{ "shell_show_in_file_manager", 885841341, 3565188097 },
		{ "set_restart_on_exit", 611198603, 3331453935 },
		{ "get_system_dir", 1965199849, 3073895123 },
	});
	mappings.insert("Object", {
		{ "add_user_signal", 3780025912, 85656714 },
		{ "connect", 1469446357, 1518946055 },
		{ "tr", 2475554935, 1195764410 },
		{ "tr_n", 4021311862, 162698058 },
	});
	mappings.insert("OpenXRAPIExtension", {
		{ "transform_from_pose", 3255299855, 2963875352 },
	});
	mappings.insert("OptionButton", {
		{ "add_item", 3043792800, 2697778442 },
		{ "add_icon_item", 3944051090, 3781678508 },
	});
	mappings.insert("PCKPacker", {
		{ "pck_start", 3232891339, 508410629 },
	});
	mappings.insert("PacketPeerDTLS", {
		{ "connect_to_peer", 1801538152, 2880188099 },
	});
	mappings.insert("PacketPeerUDP", {
		{ "bind", 4290438434, 4051239242 },
	});
	mappings.insert("Performance", {
		{ "add_custom_monitor", 2865980031, 4099036814 },
	});
	mappings.insert("PhysicalBone3D", {
#ifdef REAL_T_IS_DOUBLE
		{ "apply_impulse", 1002852006, 2485728502 },
#else
		{ "apply_impulse", 1002852006, 2754756483 },
#endif
	});
	mappings.insert("PhysicsBody2D", {
		{ "move_and_collide", 1529961754, 3681923724 },
		{ "test_move", 1369208982, 3324464701 },
	});
	mappings.insert("PhysicsBody3D", {
		{ "move_and_collide", 2825704414, 3208792678 },
		{ "test_move", 680299713, 2481691619 },
	});
	mappings.insert("PhysicsDirectBodyState2D", {
#ifdef REAL_T_IS_DOUBLE
		{ "apply_impulse", 496058220, 1271588277 },
		{ "apply_force", 496058220, 1271588277 },
		{ "add_constant_force", 496058220, 1271588277 },
#else
		{ "apply_impulse", 496058220, 4288681949 },
		{ "apply_force", 496058220, 4288681949 },
		{ "add_constant_force", 496058220, 4288681949 },
#endif
	});
	mappings.insert("PhysicsDirectBodyState3D", {
#ifdef REAL_T_IS_DOUBLE
		{ "apply_impulse", 1002852006, 2485728502 },
		{ "apply_force", 1002852006, 2485728502 },
		{ "add_constant_force", 1002852006, 2485728502 },
#else
		{ "apply_impulse", 1002852006, 2754756483 },
		{ "apply_force", 1002852006, 2754756483 },
		{ "add_constant_force", 1002852006, 2754756483 },
#endif
	});
	mappings.insert("PhysicsDirectSpaceState2D", {
		{ "intersect_point", 3278207904, 2118456068 },
		{ "intersect_shape", 3803848594, 2488867228 },
		{ "collide_shape", 3803848594, 2488867228 },
	});
	mappings.insert("PhysicsDirectSpaceState3D", {
		{ "intersect_point", 45993382, 975173756 },
		{ "intersect_shape", 550215980, 3762137681 },
		{ "collide_shape", 550215980, 3762137681 },
	});
	mappings.insert("PhysicsRayQueryParameters2D", {
		{ "create", 1118143851, 3196569324 },
	});
	mappings.insert("PhysicsRayQueryParameters3D", {
		{ "create", 680321959, 3110599579 },
	});
	mappings.insert("PhysicsServer2D", {
#ifdef REAL_T_IS_DOUBLE
		{ "area_add_shape", 754862190, 3597527023 },
		{ "body_add_shape", 754862190, 3597527023 },
		{ "body_apply_impulse", 34330743, 1124035137 },
		{ "body_apply_force", 34330743, 1124035137 },
		{ "body_add_constant_force", 34330743, 1124035137 },
#else
		{ "area_add_shape", 754862190, 339056240 },
		{ "body_add_shape", 754862190, 339056240 },
		{ "body_apply_impulse", 34330743, 205485391 },
		{ "body_apply_force", 34330743, 205485391 },
		{ "body_add_constant_force", 34330743, 205485391 },
#endif
		{ "joint_make_pin", 2288600450, 1612646186 },
		{ "joint_make_groove", 3573265764, 481430435 },
		{ "joint_make_damped_spring", 206603952, 1994657646 },
	});
	mappings.insert("PhysicsServer3D", {
#ifdef REAL_T_IS_DOUBLE
		{ "area_add_shape", 4040559639, 183938777 },
		{ "body_add_shape", 4040559639, 183938777 },
		{ "body_apply_impulse", 110375048, 2238283471 },
		{ "body_apply_force", 110375048, 2238283471 },
		{ "body_add_constant_force", 110375048, 2238283471 },
#else
		{ "area_add_shape", 4040559639, 3711419014 },
		{ "body_add_shape", 4040559639, 3711419014 },
		{ "body_apply_impulse", 110375048, 390416203 },
		{ "body_apply_force", 110375048, 390416203 },
		{ "body_add_constant_force", 110375048, 390416203 },
#endif
	});
	mappings.insert("PopupMenu", {
		{ "add_item", 3224536192, 3674230041 },
		{ "add_icon_item", 1200674553, 1086190128 },
		{ "add_check_item", 3224536192, 3674230041 },
		{ "add_icon_check_item", 1200674553, 1086190128 },
		{ "add_radio_check_item", 3224536192, 3674230041 },
		{ "add_icon_radio_check_item", 1200674553, 1086190128 },
		{ "add_multistate_item", 1585218420, 150780458 },
		{ "add_shortcut", 2482211467, 3451850107 },
		{ "add_icon_shortcut", 3060251822, 2997871092 },
		{ "add_check_shortcut", 2168272394, 1642193386 },
		{ "add_icon_check_shortcut", 68101841, 3856247530 },
		{ "add_radio_check_shortcut", 2168272394, 1642193386 },
		{ "add_icon_radio_check_shortcut", 68101841, 3856247530 },
		{ "add_submenu_item", 3728518296, 2979222410 },
		// Pre-existing compatibility hashes.
		{ "add_icon_shortcut", 68101841, 3856247530 },
		{ "add_shortcut", 2168272394, 1642193386 },
	});
	mappings.insert("PortableCompressedTexture2D", {
		{ "create_from_image", 97251393, 3679243433 },
	});
	mappings.insert("ProjectSettings", {
		{ "load_resource_pack", 3001721055, 708980503 },
	});
	mappings.insert("RDShaderFile", {
		{ "bake_from_source_geometry_data_async", 2469318639, 1286748856 },
		{ "set_bytecode", 1558064255, 1526857008 },
		{ "get_spirv", 3340165340, 2689310080 },
	});
	mappings.insert("RegEx", {
		{ "search", 4087180739, 3365977994 },
		{ "search_all", 3354100289, 849021363 },
		{ "sub", 758293621, 54019702 },
	});
	mappings.insert("RenderingDevice", {
		{ "texture_create", 3011278298, 3709173589 },
		{ "texture_create_shared_from_slice", 864132525, 1808971279 },
		{ "texture_update", 2736912341, 2096463824 },
		{ "texture_copy", 3741367532, 2339493201 },
		{ "texture_clear", 3423681478, 3396867530 },
		{ "texture_resolve_multisample", 2126834943, 594679454 },
		{ "framebuffer_format_create", 2635475316, 697032759 },
		{ "framebuffer_format_create_multipass", 1992489524, 2647479094 },
		{ "framebuffer_format_get_texture_samples", 1036806638, 4223391010 },
		{ "framebuffer_create", 1884747791, 3284231055 },
		{ "framebuffer_create_multipass", 452534725, 1750306695 },
		{ "framebuffer_create_empty", 382373098, 3058360618 },
		{ "vertex_buffer_create", 3491282828, 3410049843 },
		{ "vertex_array_create", 3137892244, 3799816279 },
		{ "index_buffer_create", 975915977, 3935920523 },
		{ "shader_compile_spirv_from_source", 3459523685, 1178973306 },
		{ "shader_compile_binary_from_spirv", 1395027180, 134910450 },
		{ "shader_create_from_spirv", 3297482566, 342949005 },
		{ "shader_create_from_bytecode", 2078349841, 1687031350 },
		{ "uniform_buffer_create", 1453158401, 34556762 },
		{ "storage_buffer_create", 1173156076, 2316365934 },
		{ "texture_buffer_create", 2344087557, 1470338698 },
		{ "buffer_update", 652628289, 3793150683 },
		{ "buffer_clear", 1645170096, 2797041220 },
		{ "buffer_get_data", 125363422, 3101830688 },
		{ "render_pipeline_create", 2911419500, 2385451958 },
		{ "compute_pipeline_create", 403593840, 1448838280 },
		{ "draw_list_draw", 3710874499, 4230067973 },
#ifdef REAL_T_IS_DOUBLE
		{ "draw_list_begin", 4252992020, 848735039 },
		{ "draw_list_begin_split", 832527510, 2228306807 },
		{ "draw_list_enable_scissor", 338791288, 730833978 },
#else
		{ "draw_list_begin", 4252992020, 2468082605 },
		{ "draw_list_begin_split", 832527510, 2406300660 },
		{ "draw_list_enable_scissor", 338791288, 244650101 },
#endif
	});
	mappings.insert("RenderingServer", {
		{ "texture_rd_create", 3291180269, 1434128712 },
		{ "shader_set_default_texture_parameter", 3864903085, 4094001817 },
		{ "shader_get_default_texture_parameter", 2523186822, 1464608890 },
		{ "mesh_create_from_surfaces", 4007581507, 4291747531 },
		{ "mesh_add_surface_from_arrays", 1247008646, 2342446560 },
		{ "environment_set_ambient_light", 491659071, 1214961493 },
		{ "instances_cull_aabb", 2031554939, 2570105777 },
		{ "instances_cull_ray", 3388524336, 2208759584 },
		{ "instances_cull_convex", 3690700105, 2488539944 },
		{ "canvas_item_add_line", 2843922985, 1819681853 },
		{ "canvas_item_add_polyline", 3438017257, 3098767073 },
		{ "canvas_item_add_multiline", 3176074788, 2088642721 },
		{ "canvas_item_add_texture_rect", 3205360868, 324864032 },
		{ "canvas_item_add_msdf_texture_rect_region", 349157222, 97408773 },
		{ "canvas_item_add_texture_rect_region", 2782979504, 485157892 },
		{ "canvas_item_add_nine_patch", 904428547, 389957886 },
		{ "canvas_item_add_polygon", 2907936855, 3580000528 },
		{ "canvas_item_add_triangle_array", 749685193, 660261329 },
		{ "canvas_item_add_multimesh", 1541595251, 2131855138 },
		{ "canvas_item_add_animation_slice", 4107531031, 2646834499 },
		{ "canvas_item_set_canvas_group_mode", 41973386, 3973586316 },
		{ "set_boot_image", 2244367877, 3759744527 },
#ifdef REAL_T_IS_DOUBLE
		{ "viewport_attach_to_screen", 1410474027, 2248302004 },
		{ "canvas_item_set_custom_rect", 2180266943, 1134449082 },
		{ "canvas_item_add_mesh", 3877492181, 3024949314 },
#else
		{ "viewport_attach_to_screen", 1278520651, 1062245816 },
		{ "canvas_item_set_custom_rect", 2180266943, 1333997032 },
		{ "canvas_item_add_mesh", 3548053052, 316450961 },
#endif
	});
	mappings.insert("ResourceLoader", {
		{ "load_threaded_request", 1939848623, 3614384323 },
		{ "load_threaded_get_status", 3931021148, 4137685479 },
		{ "load", 2622212233, 3358495409 },
		{ "exists", 2220807150, 4185558881 },
	});
	mappings.insert("ResourceSaver", {
		{ "save", 2303056517, 2983274697 },
	});
	mappings.insert("RichTextLabel", {
		{ "push_font", 814287596, 2347424842 },
		{ "push_paragraph", 3218895358, 3089306873 },
		{ "push_list", 4036303897, 3017143144 },
		{ "push_table", 1125058220, 2623499273 },
		{ "set_table_column_expand", 4258957458, 2185176273 },
#ifdef REAL_T_IS_DOUBLE
		{ "add_image", 3346058748, 1507062345 },
		{ "push_dropcap", 981432822, 763534173 },
#else
		{ "add_image", 3346058748, 3580801207 },
		{ "push_dropcap", 311501835, 4061635501 },
#endif
	});
	mappings.insert("RigidBody2D", {
#ifdef REAL_T_IS_DOUBLE
		{ "apply_impulse", 496058220, 1271588277 },
		{ "apply_force", 496058220, 1271588277 },
		{ "add_constant_force", 496058220, 1271588277 },
#else
		{ "apply_impulse", 496058220, 4288681949 },
		{ "apply_force", 496058220, 4288681949 },
		{ "add_constant_force", 496058220, 4288681949 },
#endif
	});
	mappings.insert("RigidBody3D", {
#ifdef REAL_T_IS_DOUBLE
		{ "apply_impulse", 1002852006, 2485728502 },
		{ "apply_force", 1002852006, 2485728502 },
		{ "add_constant_force", 1002852006, 2485728502 },
#else
		{ "apply_impulse", 1002852006, 2754756483 },
		{ "apply_force", 1002852006, 2754756483 },
		{ "add_constant_force", 1002852006, 2754756483 },
#endif
	});
	mappings.insert("SceneMultiplayer", {
		{ "send_bytes", 2742700601, 1307428718 },
	});
	mappings.insert("SceneReplicationConfig", {
		{ "add_property", 3818401521, 4094619021 },
	});
	mappings.insert("SceneTree", {
		{ "create_timer", 1780978058, 2709170273 },
	});
	mappings.insert("ScriptCreateDialog", {
		{ "config", 4210001628, 869314288 },
	});
	mappings.insert("Shader", {
		{ "set_default_texture_parameter", 1628453603, 2750740428 },
		{ "get_default_texture_parameter", 3823812009, 3090538643 },
	});
	mappings.insert("Skeleton3D", {
		{ "set_bone_enabled", 4023243586, 972357352 },
	});
	mappings.insert("SpriteFrames", {
		{ "add_frame", 407562921, 1351332740 },
		{ "set_frame", 3155743884, 56804795 },
	});
	mappings.insert("StreamPeerTCP", {
		{ "bind", 4025329869, 3167955072 },
	});
	mappings.insert("StreamPeerTLS", {
		{ "connect_to_stream", 1325480781, 57169517 },
	});
	mappings.insert("SurfaceTool", {
		{ "add_triangle_fan", 297960074, 2235017613 },
		{ "generate_lod", 1894448909, 1938056459 },
	});
	mappings.insert("TCPServer", {
		{ "listen", 4025329869, 3167955072 },
	});
	mappings.insert("TextEdit", {
		{ "get_line_width", 3294126239, 688195400 },
		{ "insert_text_at_caret", 3043792800, 2697778442 },
		{ "get_line_column_at_pos", 850652858, 239517838 },
		{ "is_mouse_over_selection", 1099474134, 1840282309 },
		{ "set_caret_line", 1413195636, 1302582944 },
		{ "set_caret_column", 1071284433, 3796796178 },
		{ "set_selection_mode", 2920622473, 1443345937 },
		{ "select", 4269665324, 2560984452 },
		{ "get_scroll_pos_for_line", 3274652423, 3929084198 },
		{ "set_line_as_first_visible", 3023605688, 2230941749 },
		{ "set_line_as_center_visible", 3023605688, 2230941749 },
		{ "set_line_as_last_visible", 3023605688, 2230941749 },
	});
	mappings.insert("TextLine", {
		{ "add_string", 867188035, 621426851 },
		{ "add_object", 735420116, 1316529304 },
		{ "resize_object", 960819067, 2095776372 },
		{ "draw", 1164457837, 856975658 },
		{ "draw_outline", 1364491366, 1343401456 },
	});
	mappings.insert("TextParagraph", {
#ifdef REAL_T_IS_DOUBLE
		{ "set_dropcap", 2613124475, 2897844600 },
#else
		{ "set_dropcap", 2613124475, 2498990330 },
#endif
		{ "add_string", 867188035, 621426851 },
		{ "add_object", 735420116, 1316529304 },
		{ "resize_object", 960819067, 2095776372 },
		{ "draw", 367324453, 1567802413 },
		{ "draw_outline", 2159523405, 1893131224 },
		{ "draw_line", 3963848920, 1242169894 },
		{ "draw_line_outline", 1814903311, 2664926980 },
		{ "draw_dropcap", 1164457837, 856975658 },
		{ "draw_dropcap_outline", 1364491366, 1343401456 },
	});
	mappings.insert("TextServer", {
		{ "font_draw_glyph", 1821196351, 1339057948 },
		{ "font_draw_glyph_outline", 1124898203, 2626165733 },
		{ "shaped_text_set_direction", 2616949700, 1551430183 },
		{ "shaped_text_set_orientation", 104095128, 3019609126 },
		{ "shaped_text_add_string", 2621279422, 623473029 },
		{ "shaped_text_add_object", 2838446185, 3664424789 },
		{ "shaped_text_resize_object", 2353789835, 790361552 },
		{ "shaped_set_span_update_font", 1578983057, 2022725822 },
		{ "shaped_text_fit_to_width", 603718830, 530670926 },
		{ "shaped_text_get_line_breaks_adv", 4206849830, 2376991424 },
		{ "shaped_text_get_line_breaks", 303410369, 2651359741 },
		{ "shaped_text_get_word_breaks", 3299477123, 185957063 },
		{ "shaped_text_overrun_trim_to_width", 1572579718, 2723146520 },
		{ "shaped_text_draw", 70679950, 880389142 },
		{ "shaped_text_draw_outline", 2673671346, 2559184194 },
		{ "format_number", 2305636099, 2664628024 },
		{ "parse_number", 2305636099, 2664628024 },
		{ "string_get_word_breaks", 1398910359, 581857818 },
		{ "string_get_character_breaks", 1586579831, 2333794773 },
		{ "string_to_upper", 2305636099, 2664628024 },
		{ "string_to_lower", 2305636099, 2664628024 },
	});
	mappings.insert("Texture2D", {
		{ "draw", 1115460088, 2729649137 },
		{ "draw_rect", 575156982, 3499451691 },
		{ "draw_rect_region", 1066564656, 2963678660 },
	});
	mappings.insert("Thread", {
		{ "start", 2779832528, 1327203254 },
	});
	mappings.insert("TileMap", {
		{ "set_cell", 1732664643, 966713560 },
		{ "set_cells_terrain_connect", 3072115677, 3578627656 },
		{ "set_cells_terrain_path", 3072115677, 3578627656 },
		{ "get_used_cells_by_id", 4152068407, 2931012785 },
	});
	mappings.insert("TileMapLayer", {
		{ "notify_runtime_tile_data_update", 2275361663, 3218959716 },
	});
	mappings.insert("TileMapPattern", {
		{ "set_cell", 634000503, 2224802556 },
	});
	mappings.insert("TileSet", {
		{ "add_source", 276991387, 1059186179 },
		{ "add_terrain", 3023605688, 1230568737 },
		{ "add_pattern", 3009264082, 763712015 },
	});
	mappings.insert("TileSetAtlasSource", {
		{ "create_tile", 1583819816, 190528769 },
		{ "move_tile_in_atlas", 1375626516, 3870111920 },
		{ "has_room_for_tile", 4182444377, 3018597268 },
		{ "create_alternative_tile", 3531100812, 2226298068 },
		{ "get_tile_texture_region", 1321423751, 241857547 },
	});
	mappings.insert("TileSetScenesCollectionSource", {
		{ "create_scene_tile", 2633389122, 1117465415 },
	});
	mappings.insert("Translation", {
		{ "add_message", 971803314, 3898530326 },
		{ "add_plural_message", 360316719, 2356982266 },
		{ "get_message", 58037827, 1829228469 },
		{ "get_plural_message", 1333931916, 229954002 },
		{ "erase_message", 3919944288, 3959009644 },
	});
	mappings.insert("TranslationServer", {
		{ "translate", 58037827, 1829228469 },
		{ "translate_plural", 1333931916, 229954002 },
	});
	mappings.insert("Tree", {
		{ "get_item_area_rect", 1235226180, 47968679 },
	});
	mappings.insert("TreeItem", {
		{ "propagate_check", 4023243586, 972357352 },
		{ "add_button", 1507727907, 1688223362 },
	});
	mappings.insert("UDPServer", {
		{ "listen", 4025329869, 3167955072 },
	});
	mappings.insert("UPNP", {
		{ "add_port_mapping", 3358934458, 818314583 },
		{ "delete_port_mapping", 760296170, 3444187325 },
	});
	mappings.insert("UPNPDevice", {
		{ "add_port_mapping", 3358934458, 818314583 },
		{ "delete_port_mapping", 760296170, 3444187325 },
	});
	mappings.insert("UndoRedo", {
		{ "create_action", 3900135403, 3171901514 },
	});
	mappings.insert("VideoStreamPlayback", {
		{ "mix_audio", 1369271885, 93876830 },
	});
	mappings.insert("WebRTCMultiplayerPeer", {
		{ "create_client", 1777354631, 2641732907 },
		{ "create_mesh", 1777354631, 2641732907 },
		{ "add_peer", 2555866323, 4078953270 },
	});
	mappings.insert("WebRTCPeerConnection", {
		{ "create_data_channel", 3997447457, 1288557393 },
	});
	mappings.insert("WebSocketMultiplayerPeer", {
		{ "create_client", 3097527179, 1966198364 },
		{ "create_server", 337374795, 2400822951 },
	});
	mappings.insert("WebSocketPeer", {
		{ "connect_to_url", 3097527179, 1966198364 },
		{ "send", 3440492527, 2780360567 },
	});
	mappings.insert("Window", {
		{ "get_theme_icon", 2336455395, 3163973443 },
		{ "get_theme_stylebox", 2759935355, 604739069 },
		{ "get_theme_font", 387378635, 2826986490 },
		{ "get_theme_font_size", 229578101, 1327056374 },
		{ "get_theme_color", 2377051548, 2798751242 },
		{ "get_theme_constant", 229578101, 1327056374 },
		{ "has_theme_icon", 1187511791, 866386512 },
		{ "has_theme_stylebox", 1187511791, 866386512 },
		{ "has_theme_font", 1187511791, 866386512 },
		{ "has_theme_font_size", 1187511791, 866386512 },
		{ "has_theme_color", 1187511791, 866386512 },
		{ "has_theme_constant", 1187511791, 866386512 },
		{ "popup_exclusive", 1728044812, 2134721627 },
		{ "popup_exclusive_centered", 2561668109, 3357594017 },
		{ "popup_exclusive_centered_ratio", 4257659513, 2284776287 },
		{ "popup_exclusive_centered_clamped", 224798062, 2612708785 },
	});
	mappings.insert("WorkerThreadPool", {
		{ "add_task", 3976347598, 3745067146 },
		{ "add_group_task", 2377228549, 1801953219 },
	});
	mappings.insert("ZIPPacker", {
		{ "open", 3715508516, 1936816515 },
	});
	mappings.insert("ZIPReader", {
		{ "read_file", 156385007, 740857591 },
		{ "file_exists", 1676256, 35364943 },
	});
	// clang-format on
}

void GDExtensionSpecialCompatHashes::finalize() {
	mappings.clear();
}

#endif // DISABLE_DEPRECATED
