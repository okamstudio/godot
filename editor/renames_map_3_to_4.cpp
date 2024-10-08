/**************************************************************************/
/*  renames_map_3_to_4.cpp                                                */
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

#include "renames_map_3_to_4.h"

#ifndef DISABLE_DEPRECATED

const char *RenamesMap3To4::enum_renames[][2] = {
	// Constants
	// @GlobalScope
	{ "BUTTON_LEFT", "MOUSE_BUTTON_LEFT" },
	{ "BUTTON_MASK_LEFT", "MOUSE_BUTTON_MASK_LEFT" },
	{ "BUTTON_MASK_MIDDLE", "MOUSE_BUTTON_MASK_MIDDLE" },
	{ "BUTTON_MASK_RIGHT", "MOUSE_BUTTON_MASK_RIGHT" },
	{ "BUTTON_MASK_XBUTTON1", "MOUSE_BUTTON_MASK_XBUTTON1" },
	{ "BUTTON_MASK_XBUTTON2", "MOUSE_BUTTON_MASK_XBUTTON2" },
	{ "BUTTON_MIDDLE", "MOUSE_BUTTON_MIDDLE" },
	{ "BUTTON_RIGHT", "MOUSE_BUTTON_RIGHT" },
	{ "BUTTON_WHEEL_DOWN", "MOUSE_BUTTON_WHEEL_DOWN" },
	{ "BUTTON_WHEEL_LEFT", "MOUSE_BUTTON_WHEEL_LEFT" },
	{ "BUTTON_WHEEL_RIGHT", "MOUSE_BUTTON_WHEEL_RIGHT" },
	{ "BUTTON_WHEEL_UP", "MOUSE_BUTTON_WHEEL_UP" },
	{ "BUTTON_XBUTTON1", "MOUSE_BUTTON_XBUTTON1" },
	{ "BUTTON_XBUTTON2", "MOUSE_BUTTON_XBUTTON2" },
	{ "KEY_CONTROL", "KEY_CTRL" },
	{ "SIDE_BOTTOM", "MARGIN_BOTTOM" },
	{ "SIDE_LEFT", "MARGIN_LEFT" },
	{ "SIDE_RIGHT", "MARGIN_RIGHT" },
	{ "SIDE_TOP", "MARGIN_TOP" },
	{ "TYPE_COLOR_ARRAY", "TYPE_PACKED_COLOR_ARRAY" },
	{ "TYPE_FLOAT64_ARRAY", "TYPE_PACKED_FLOAT64_ARRAY" },
	{ "TYPE_INT64_ARRAY", "TYPE_PACKED_INT64_ARRAY" },
	{ "TYPE_INT_ARRAY", "TYPE_PACKED_INT32_ARRAY" },
	{ "TYPE_QUAT", "TYPE_QUATERNION" },
	{ "TYPE_RAW_ARRAY", "TYPE_PACKED_BYTE_ARRAY" },
	{ "TYPE_REAL", "TYPE_FLOAT" },
	{ "TYPE_REAL_ARRAY", "TYPE_PACKED_FLOAT32_ARRAY" },
	{ "TYPE_STRING_ARRAY", "TYPE_PACKED_STRING_ARRAY" },
	{ "TYPE_TRANSFORM", "TYPE_TRANSFORM3D" },
	{ "TYPE_VECTOR2_ARRAY", "TYPE_PACKED_VECTOR2_ARRAY" },
	{ "TYPE_VECTOR3_ARRAY", "TYPE_PACKED_VECTOR3_ARRAY" },

	// { "FLAG_MAX", "PARTICLE_FLAG_MAX" }, // CPUParticles2D -- Used in more classes.
	{ "ALIGN_BEGIN", "ALIGNMENT_BEGIN" }, // AspectRatioContainer
	{ "ALIGN_CENTER", "ALIGNMENT_CENTER" }, // AspectRatioContainer
	{ "ALIGN_END", "ALIGNMENT_END" }, // AspectRatioContainer
	{ "ARRAY_COMPRESS_BASE", "ARRAY_COMPRESS_FLAGS_BASE" }, // Mesh
	{ "ARVR_AR", "XR_AR" }, // XRInterface
	{ "ARVR_EXCESSIVE_MOTION", "XR_EXCESSIVE_MOTION" }, // XRInterface
	{ "ARVR_EXTERNAL", "XR_EXTERNAL" }, // XRInterface
	{ "ARVR_INSUFFICIENT_FEATURES", "XR_INSUFFICIENT_FEATURES" }, // XRInterface
	{ "ARVR_MONO", "XR_MONO" }, // XRInterface
	{ "ARVR_NONE", "XR_NONE" }, // XRInterface
	{ "ARVR_NORMAL_TRACKING", "XR_NORMAL_TRACKING" }, // XRInterface
	{ "ARVR_NOT_TRACKING", "XR_NOT_TRACKING" }, // XRInterface
	{ "ARVR_STEREO", "XR_STEREO" }, // XRInterface
	{ "ARVR_UNKNOWN_TRACKING", "XR_UNKNOWN_TRACKING" }, // XRInterface
	{ "BAKE_ERROR_INVALID_MESH", "BAKE_ERROR_MESHES_INVALID" }, // LightmapGI
	{ "BODY_MODE_CHARACTER", "BODY_MODE_RIGID_LINEAR" }, // PhysicsServer
	{ "CLEAR_MODE_ONLY_NEXT_FRAME", "CLEAR_MODE_ONCE" }, // SubViewport
	{ "COMPRESS_PVRTC4", "COMPRESS_PVRTC1_4" }, // Image
	{ "CONNECT_ONESHOT", "CONNECT_ONE_SHOT" }, // Object
	{ "CONTAINER_PROPERTY_EDITOR_BOTTOM", "CONTAINER_INSPECTOR_BOTTOM" }, // EditorPlugin
	{ "CUBEMAP_BACK", "CUBEMAP_LAYER_BACK" }, // RenderingServer
	{ "CUBEMAP_BOTTOM", "CUBEMAP_LAYER_BOTTOM" }, // RenderingServer
	{ "CUBEMAP_FRONT", "CUBEMAP_LAYER_FRONT" }, // RenderingServer
	{ "CUBEMAP_LEFT", "CUBEMAP_LAYER_LEFT" }, // RenderingServer
	{ "CUBEMAP_RIGHT", "CUBEMAP_LAYER_RIGHT" }, // RenderingServer
	{ "CUBEMAP_TOP", "CUBEMAP_LAYER_TOP" }, // RenderingServer
	{ "DAMPED_STRING_DAMPING", "DAMPED_SPRING_DAMPING" }, // PhysicsServer2D
	{ "DAMPED_STRING_REST_LENGTH", "DAMPED_SPRING_REST_LENGTH" }, // PhysicsServer2D
	{ "DAMPED_STRING_STIFFNESS", "DAMPED_SPRING_STIFFNESS" }, // PhysicsServer2D
	{ "FLAG_ALIGN_Y_TO_VELOCITY", "PARTICLE_FLAG_ALIGN_Y_TO_VELOCITY" }, // CPUParticles2D
	{ "FLAG_DISABLE_Z", "PARTICLE_FLAG_DISABLE_Z" }, // CPUParticles2D
	{ "FLAG_ROTATE_Y", "PARTICLE_FLAG_ROTATE_Y" }, // CPUParticles2D
	{ "FLAG_USE_BAKED_LIGHT", "GI_MODE_BAKED" }, // GeometryInstance3D
	{ "FORMAT_PVRTC2", "FORMAT_PVRTC1_2" }, // Image
	{ "FORMAT_PVRTC2A", "FORMAT_PVRTC1_2A" }, // Image
	{ "FORMAT_PVRTC4", "FORMAT_PVRTC1_4" }, // Image
	{ "FORMAT_PVRTC4A", "FORMAT_PVRTC1_4A" }, // Image
	{ "FUNC_FRAC", "FUNC_FRACT" }, // VisualShaderNodeVectorFunc
	{ "INSTANCE_LIGHTMAP_CAPTURE", "INSTANCE_LIGHTMAP" }, // RenderingServer
	{ "JOINT_6DOF", "JOINT_TYPE_6DOF" }, // PhysicsServer3D
	{ "JOINT_CONE_TWIST", "JOINT_TYPE_CONE_TWIST" }, // PhysicsServer3D
	{ "JOINT_DAMPED_SPRING", "JOINT_TYPE_DAMPED_SPRING" }, // PhysicsServer2D
	{ "JOINT_GROOVE", "JOINT_TYPE_GROOVE" }, // PhysicsServer2D
	{ "JOINT_HINGE", "JOINT_TYPE_HINGE" }, // PhysicsServer3D
	{ "JOINT_PIN", "JOINT_TYPE_PIN" }, // PhysicsServer2D
	{ "JOINT_SLIDER", "JOINT_TYPE_SLIDER" }, // PhysicsServer3D
	{ "LOOP_PING_PONG", "LOOP_PINGPONG" }, // AudioStreamWAV
	{ "MODE_KINEMATIC", "FREEZE_MODE_KINEMATIC" }, // RigidBody
	{ "MODE_OPEN_ANY", "FILE_MODE_OPEN_ANY" }, // FileDialog
	{ "MODE_OPEN_DIR", "FILE_MODE_OPEN_DIR" }, // FileDialog
	{ "MODE_OPEN_FILE", "FILE_MODE_OPEN_FILE" }, // FileDialog
	{ "MODE_OPEN_FILES", "FILE_MODE_OPEN_FILES" }, // FileDialog
	{ "MODE_SAVE_FILE", "FILE_MODE_SAVE_FILE" }, // FileDialog
	{ "MODE_STATIC", "FREEZE_MODE_STATIC" }, // RigidBody
	{ "NOTIFICATION_APP_PAUSED", "NOTIFICATION_APPLICATION_PAUSED" }, // MainLoop
	{ "NOTIFICATION_APP_RESUMED", "NOTIFICATION_APPLICATION_RESUMED" }, // MainLoop
	{ "NOTIFICATION_INSTANCED", "NOTIFICATION_SCENE_INSTANTIATED" }, // Node
	{ "NOTIFICATION_PATH_CHANGED", "NOTIFICATION_PATH_RENAMED" }, //Node
	{ "NOTIFICATION_WM_FOCUS_IN", "NOTIFICATION_APPLICATION_FOCUS_IN" }, // MainLoop
	{ "NOTIFICATION_WM_FOCUS_OUT", "NOTIFICATION_APPLICATION_FOCUS_OUT" }, // MainLoop
	{ "NOTIFICATION_WM_UNFOCUS_REQUEST", "NOTIFICATION_WM_WINDOW_FOCUS_OUT" }, //Node
	{ "PAUSE_MODE_INHERIT", "PROCESS_MODE_INHERIT" }, // Node
	{ "PAUSE_MODE_PROCESS", "PROCESS_MODE_ALWAYS" }, // Node
	{ "PAUSE_MODE_STOP", "PROCESS_MODE_PAUSABLE" }, // Node
	{ "RENDER_DRAW_CALLS_IN_FRAME", "RENDER_TOTAL_DRAW_CALLS_IN_FRAME" }, // Performance
	{ "RENDER_OBJECTS_IN_FRAME", "RENDER_TOTAL_OBJECTS_IN_FRAME" }, // Performance
	{ "SOURCE_GEOMETRY_NAVMESH_CHILDREN", "SOURCE_GEOMETRY_ROOT_NODE_CHILDREN" }, // NavigationMesh
	{ "TEXTURE_TYPE_2D_ARRAY", "TEXTURE_LAYERED_2D_ARRAY" }, // RenderingServer
	{ "TEXTURE_TYPE_CUBEMAP", "TEXTURE_LAYERED_CUBEMAP_ARRAY" }, // RenderingServer
	{ "TRACKER_LEFT_HAND", "TRACKER_HAND_LEFT" }, // XRPositionalTracker
	{ "TRACKER_RIGHT_HAND", "TRACKER_HAND_RIGHT" }, // XRPositionalTracker
	{ "TYPE_NORMALMAP", "TYPE_NORMAL_MAP" }, // VisualShaderNodeCubemap

	// Enums
	{ "AlignMode", "AlignmentMode" }, // AspectRatioContainer
	{ "AnimationProcessMode", "AnimationProcessCallback" }, // AnimationTree, AnimationPlayer
	{ "Camera2DProcessMode", "Camera2DProcessCallback" }, // Camera2D
	{ "CubeMapSide", "CubeMapLayer" }, // RenderingServer
	{ "DampedStringParam", "DampedSpringParam" }, // PhysicsServer2D
	{ "FFT_Size", "FFTSize" }, // AudioEffectPitchShift, AudioEffectSpectrumAnalyzer
	{ "PauseMode", "ProcessMode" }, // Node
	{ "TimerProcessMode", "TimerProcessCallback" }, // Timer
	{ "Tracking_status", "TrackingStatus" }, // XRInterface
	{ nullptr, nullptr },
};

const char *RenamesMap3To4::gdscript_function_renames[][2] = {
	// NOTE: Commented out renames are disabled because deemed not suitable for
	// the current way the regex-based converter works.
	// When uncommenting any of those as suitable for conversion, please move it
	// to the block with other enabled conversions, ordered alphabetically, and
	// make sure to add it to the C# rename map too.

	// { "_set_name", "get_tracker_name" }, // XRPositionalTracker -- CameraFeed uses this.
	// { "_unhandled_input", "_unhandled_key_input" }, // BaseButton, ViewportContainer -- Breaks Node, FileDialog, SubViewportContainer.
	// { "add_animation", "add_animation_library" }, // AnimationPlayer -- Breaks SpriteFrames (and isn't a correct conversion).
	// { "create_gizmo", "_create_gizmo" }, // EditorNode3DGizmoPlugin -- May be used.
	// { "get_dependencies", "_get_dependencies" }, // ResourceFormatLoader -- Breaks ResourceLoader.
	// { "get_extents", "get_size" }, // BoxShape, RectangleShape -- Breaks Decal, VoxelGI, GPUParticlesCollisionBox, GPUParticlesCollisionSDF, GPUParticlesCollisionHeightField, GPUParticlesAttractorBox, GPUParticlesAttractorVectorField, FogVolume
	// { "get_h_offset", "get_drag_horizontal_offset" }, // Camera2D -- Breaks PathFollow, Camera.
	// { "get_mode", "get_file_mode" }, // FileDialog -- Breaks Panel, Shader, CSGPolygon, TileMap.
	// { "get_motion", "get_travel" }, // PhysicsTestMotionResult2D -- Breaks ParallaxLayer.
	// { "get_name", "get_tracker_name" }, // XRPositionalTracker -- Breaks OS, Node
	// { "get_network_connected_peers", "get_peers" }, // MultiplayerAPI -- Breaks SceneTree.
	// { "get_network_peer", "has_multiplayer_peer" }, // MultiplayerAPI -- Breaks SceneTree.
	// { "get_network_unique_id", "get_unique_id"}, // MultiplayerAPI -- Breaks SceneTree.
	// { "get_offset", "get_position_offset" }, // GraphNode -- Breaks Gradient.
	// { "get_peer_port", "get_peer" }, // ENetMultiplayerPeer -- Breaks WebSocketServer.
	// { "get_points", "get_points_id" }, // AStar -- Breaks Line2D, ConvexPolygonShape.
	// { "get_process_mode", "get_process_callback" }, // ClippedCamera3D -- Breaks Node, Sky.
	// { "get_render_info", "get_rendering_info" }, // RenderingServer -- Breaks Viewport.
	// { "get_stylebox", "get_theme_stylebox" }, // Control -- Would rename the method in Theme as well, skipping.
	// { "get_type", "get_tracker_type" }, // XRPositionalTracker -- Breaks GLTFAccessor, GLTFLight.
	// { "get_v_offset", "get_drag_vertical_offset" }, // Camera2D -- Breaks PathFollow, Camera.
	// { "get_v_scroll", "get_v_scroll_bar" }, // ItemList -- Breaks TextView.
	// { "has_network_peer", "has_multiplayer_peer" }, // MultiplayerAPI -- Breaks SceneTree.
	// { "instance", "instantiate" }, // PackedScene, ClassDB -- Breaks FileSystemDock signal, and also .tscn files ("[instance=ExtResource( 17 )]"). This is implemented as custom rule.
	// { "is_listening", "is_bound"}, // PacketPeerUDP -- Breaks TCPServer, UDPServer.
	// { "is_refusing_new_network_connections", "is_refusing_new_connections"}, // MultiplayerAPI -- Breaks SceneTree.
	// { "is_valid", "has_valid_event" }, // Shortcut -- Breaks Callable, and more.
	// { "listen", "bound"}, // PacketPeerUDP -- Breaks TCPServer, UDPServer.
	// { "load", "_load"}, // ResourceFormatLoader -- Breaks ConfigFile, Image, StreamTexture2D.
	// { "make_current", "set_current" }, // Camera2D -- Breaks Camera3D, Listener2D.
	// { "process", "_process" }, // AnimationNode -- This word is too commonly used.
	// { "raise", "move_to_front" }, // CanvasItem -- Too common.
	// { "save", "_save"}, // ResourceFormatLoader -- Breaks ConfigFile, Image, StreamTexture2D.
	// { "set_autowrap", "set_autowrap_mode" }, // AcceptDialog -- Breaks Label, also a cyclic rename.
	// { "set_color", "surface_set_color"}, // ImmediateMesh -- Breaks Light2D, Theme, SurfaceTool.
	// { "set_event", "set_shortcut" }, // BaseButton -- Cyclic rename.
	// { "set_extents", "set_size"}, // BoxShape, RectangleShape -- Breaks ReflectionProbe.
	// { "set_flag", "set_particle_flag"}, // ParticleProcessMaterial -- Breaks Window, HingeJoint3D.
	// { "set_h_offset", "set_drag_horizontal_offset" }, // Camera2D -- Breaks Camera3D, PathFollow3D, PathFollow2D.
	// { "set_margin", "set_offset" }, // Control -- Breaks Shape3D, AtlasTexture.
	// { "set_mode", "set_mode_file_mode" }, // FileDialog -- Breaks Panel, Shader, CSGPolygon, TileMap.
	// { "set_normal", "surface_set_normal"}, // ImmediateGeometry -- Breaks SurfaceTool, WorldMarginShape2D.
	// { "set_offset", "set_progress" }, // PathFollow2D, PathFollow3D -- Too common.
	// { "set_process_mode", "set_process_callback" }, // AnimationTree -- Breaks Node, Tween, Sky.
	// { "set_refuse_new_network_connections", "set_refuse_new_connections"}, // MultiplayerAPI -- Breaks SceneTree.
	// { "set_tooltip", "set_tooltip_text" }, // Control -- Breaks TreeItem, at least for now.
	// { "set_uv", "surface_set_uv" }, // ImmediateMesh -- Breaks Polygon2D.
	// { "set_v_offset", "set_drag_vertical_offset" }, // Camera2D -- Breaks Camera3D, PathFollow3D, PathFollow2D.

	{ "_about_to_show", "_about_to_popup" }, // ColorPickerButton
	{ "_get_configuration_warning", "_get_configuration_warnings" }, // Node
	{ "_set_current", "set_current" }, // Camera2D
	{ "_set_editor_description", "set_editor_description" }, // Node
	{ "_toplevel_raise_self", "_top_level_raise_self" }, // CanvasItem
	{ "add_cancel", "add_cancel_button" }, // AcceptDialog
	{ "add_central_force", "apply_central_force" }, //RigidBody2D
	{ "add_child_below_node", "add_sibling" }, // Node
	{ "add_color_override", "add_theme_color_override" }, // Control
	{ "add_constant_override", "add_theme_constant_override" }, // Control
	{ "add_font_override", "add_theme_font_override" }, // Control
	{ "add_force", "apply_force" }, //RigidBody2D
	{ "add_icon_override", "add_theme_icon_override" }, // Control
	{ "add_scene_import_plugin", "add_scene_format_importer_plugin" }, //EditorPlugin
	{ "add_spatial_gizmo_plugin", "add_node_3d_gizmo_plugin" }, // EditorPlugin
	{ "add_stylebox_override", "add_theme_stylebox_override" }, // Control
	{ "add_torque", "apply_torque" }, //RigidBody2D
	{ "agent_set_neighbor_dist", "agent_set_neighbor_distance" }, // NavigationServer2D, NavigationServer3D
	{ "apply_changes", "_apply_changes" }, // EditorPlugin
	{ "body_add_force", "body_apply_force" }, // PhysicsServer2D
	{ "body_add_torque", "body_apply_torque" }, // PhysicsServer2D
	{ "bumpmap_to_normalmap", "bump_map_to_normal_map" }, // Image
	{ "can_be_hidden", "_can_be_hidden" }, // EditorNode3DGizmoPlugin
	{ "can_drop_data", "_can_drop_data" }, // Control
	{ "can_generate_small_preview", "_can_generate_small_preview" }, // EditorResourcePreviewGenerator
	{ "can_instance", "can_instantiate" }, // PackedScene, Script
	{ "canvas_light_set_scale", "canvas_light_set_texture_scale" }, // RenderingServer
	{ "capture_get_device", "get_input_device" }, // AudioServer
	{ "capture_get_device_list", "get_input_device_list" }, // AudioServer
	{ "capture_set_device", "set_input_device" }, // AudioServer
	{ "center_viewport_to_cursor", "center_viewport_to_caret" }, // TextEdit
	{ "change_scene", "change_scene_to_file" }, // SceneTree
	{ "change_scene_to", "change_scene_to_packed" }, // SceneTree
	{ "clip_polygons_2d", "clip_polygons" }, // Geometry2D
	{ "clip_polyline_with_polygon_2d", "clip_polyline_with_polygon" }, //Geometry2D
	{ "commit_handle", "_commit_handle" }, // EditorNode3DGizmo
	{ "convex_hull_2d", "convex_hull" }, // Geometry2D
	{ "create_gizmo", "_create_gizmo" }, // EditorNode3DGizmoPlugin
	{ "cursor_get_blink_speed", "get_caret_blink_interval" }, // TextEdit
	{ "cursor_get_column", "get_caret_column" }, // TextEdit
	{ "cursor_get_line", "get_caret_line" }, // TextEdit
	{ "cursor_set_blink_enabled", "set_caret_blink_enabled" }, // TextEdit
	{ "cursor_set_blink_speed", "set_caret_blink_interval" }, // TextEdit
	{ "cursor_set_column", "set_caret_column" }, // TextEdit
	{ "cursor_set_line", "set_caret_line" }, // TextEdit
	{ "damped_spring_joint_create", "joint_make_damped_spring" }, // PhysicsServer2D
	{ "damped_string_joint_get_param", "damped_spring_joint_get_param" }, // PhysicsServer2D
	{ "damped_string_joint_set_param", "damped_spring_joint_set_param" }, // PhysicsServer2D
	{ "dectime", "move_toward" }, // GDScript, Math functions
	{ "delete_char_at_cursor", "delete_char_at_caret" }, // LineEdit
	{ "deselect_items", "deselect_all" }, // FileDialog
	{ "disable_plugin", "_disable_plugin" }, // EditorPlugin
	{ "drop_data", "_drop_data" }, // Control
	{ "exclude_polygons_2d", "exclude_polygons" }, // Geometry2D
	{ "find_node", "find_child" }, // Node
	{ "find_scancode_from_string", "find_keycode_from_string" }, // OS
	{ "forward_canvas_draw_over_viewport", "_forward_canvas_draw_over_viewport" }, // EditorPlugin
	{ "forward_canvas_force_draw_over_viewport", "_forward_canvas_force_draw_over_viewport" }, // EditorPlugin
	{ "forward_canvas_gui_input", "_forward_canvas_gui_input" }, // EditorPlugin
	{ "forward_spatial_draw_over_viewport", "_forward_3d_draw_over_viewport" }, // EditorPlugin
	{ "forward_spatial_force_draw_over_viewport", "_forward_3d_force_draw_over_viewport" }, // EditorPlugin
	{ "forward_spatial_gui_input", "_forward_3d_gui_input" }, // EditorPlugin
	{ "generate_from_path", "_generate_from_path" }, // EditorResourcePreviewGenerator
	{ "generate_small_preview_automatically", "_generate_small_preview_automatically" }, // EditorResourcePreviewGenerator
	{ "get_action_list", "action_get_events" }, // InputMap
	{ "get_alt", "is_alt_pressed" }, // InputEventWithModifiers
	{ "get_animation_process_mode", "get_process_callback" }, // AnimationPlayer
	{ "get_applied_force", "get_constant_force" }, //RigidBody2D
	{ "get_applied_torque", "get_constant_torque" }, //RigidBody2D
	{ "get_audio_bus", "get_audio_bus_name" }, // Area3D
	{ "get_bound_child_nodes_to_bone", "get_bone_children" }, // Skeleton3D
	{ "get_camera", "get_camera_3d" }, // Viewport -- This is also convertible to get_camera_2d. Breaks GLTFNode.
	{ "get_cancel", "get_cancel_button" }, // ConfirmationDialog
	{ "get_caption", "_get_caption" }, // AnimationNode
	{ "get_cast_to", "get_target_position" }, // RayCast2D, RayCast3D
	{ "get_child_by_name", "_get_child_by_name" }, // AnimationNode
	{ "get_child_nodes", "_get_child_nodes" }, // AnimationNode
	{ "get_closest_point_to_segment_2d", "get_closest_point_to_segment" }, // Geometry2D
	{ "get_closest_point_to_segment_uncapped_2d", "get_closest_point_to_segment_uncapped" }, // Geometry2D
	{ "get_closest_points_between_segments_2d", "get_closest_point_to_segment" }, // Geometry2D
	{ "get_collision_layer_bit", "get_collision_layer_value" }, // CSGShape3D, and a lot of others like GridMap.
	{ "get_collision_mask_bit", "get_collision_mask_value" }, // CSGShape3D, and a lot of others like GridMap.
	{ "get_color_types", "get_color_type_list" }, // Theme
	{ "get_command", "is_command_or_control_pressed" }, // InputEventWithModifiers
	{ "get_constant_types", "get_constant_type_list" }, // Theme
	{ "get_control", "is_ctrl_pressed" }, // InputEventWithModifiers
	{ "get_cull_mask_bit", "get_cull_mask_value" }, // Camera3D
	{ "get_cursor_position", "get_caret_column" }, // LineEdit
	{ "get_d", "get_distance" }, // LineShape2D
	{ "get_default_length", "get_length" }, // Bone2D
	{ "get_depth_bias_enable", "get_depth_bias_enabled" }, // RDPipelineRasterizationState
	{ "get_device", "get_output_device" }, // AudioServer
	{ "get_device_list", "get_output_device_list" }, // AudioServer
	{ "get_drag_data", "_get_drag_data" }, // Control
	{ "get_editor_viewport", "get_editor_main_screen" }, // EditorPlugin
	{ "get_enabled_focus_mode", "get_focus_mode" }, // BaseButton
	{ "get_endian_swap", "is_big_endian" }, // File
	{ "get_error_string", "get_error_message" }, // JSON
	{ "get_filename", "get_scene_file_path" }, // Node -- WARNING: This may be used in a lot of other places.
	{ "get_final_location", "get_final_position" }, // NavigationAgent2D, NavigationAgent3D
	{ "get_focus_neighbour", "get_focus_neighbor" }, // Control
	{ "get_follow_smoothing", "get_position_smoothing_speed" }, // Camera2D
	{ "get_font_types", "get_font_type_list" }, // Theme
	{ "get_frame_color", "get_color" }, // ColorRect
	{ "get_global_rate_scale", "get_playback_speed_scale" }, // AudioServer
	{ "get_gravity_distance_scale", "get_gravity_point_unit_distance" }, // Area2D, Area3D
	{ "get_gravity_vector", "get_gravity_direction" }, // Area(2D/3D)
	{ "get_h_scrollbar", "get_h_scroll_bar" }, //ScrollContainer
	{ "get_hand", "get_tracker_hand" }, // XRPositionalTracker
	{ "get_handle_name", "_get_handle_name" }, // EditorNode3DGizmo
	{ "get_handle_value", "_get_handle_value" }, // EditorNode3DGizmo
	{ "get_icon_align", "get_icon_alignment" }, // Button
	{ "get_icon_types", "get_icon_type_list" }, // Theme
	{ "get_idle_frames", "get_process_frames" }, // Engine
	{ "get_import_options", "_get_import_options" }, // EditorImportPlugin
	{ "get_import_order", "_get_import_order" }, // EditorImportPlugin
	{ "get_importer_name", "_get_importer_name" }, // EditorImportPlugin
	{ "get_interior_ambient", "get_ambient_color" }, // ReflectionProbe
	{ "get_interior_ambient_energy", "get_ambient_color_energy" }, // ReflectionProbe
	{ "get_item_navmesh", "get_item_navigation_mesh" }, // MeshLibrary
	{ "get_item_navmesh_transform", "get_item_navigation_mesh_transform" }, // MeshLibrary
	{ "get_iterations_per_second", "get_physics_ticks_per_second" }, // Engine
	{ "get_last_mouse_speed", "get_last_mouse_velocity" }, // Input
	{ "get_layer_mask_bit", "get_layer_mask_value" }, // VisualInstance3D
	{ "get_len", "get_length" }, // File
	{ "get_max_atlas_size", "get_max_texture_size" }, // LightmapGI
	{ "get_metakey", "is_meta_pressed" }, // InputEventWithModifiers
	{ "get_mid_height", "get_height" }, // CapsuleMesh
	{ "get_motion_remainder", "get_remainder" }, // PhysicsTestMotionResult2D
	{ "get_nav_path", "get_current_navigation_path" }, // NavigationAgent2D, NavigationAgent3D
	{ "get_nav_path_index", "get_current_navigation_path_index" }, // NavigationAgent2D, NavigationAgent3D
	{ "get_neighbor_dist", "get_neighbor_distance" }, // NavigationAgent2D, NavigationAgent3D
	{ "get_network_connected_peers", "get_peers" }, // Multiplayer API
	{ "get_network_master", "get_multiplayer_authority" }, // Node
	{ "get_network_peer", "get_multiplayer_peer" }, // Multiplayer API
	{ "get_network_unique_id", "get_unique_id" }, // Multiplayer API
	{ "get_next_location", "get_next_path_position" }, // NavigationAgent2D, NavigationAgent3D
	{ "get_ok", "get_ok_button" }, // AcceptDialog
	{ "get_oneshot", "get_one_shot" }, // AnimatedTexture
	{ "get_option_visibility", "_get_option_visibility" }, // EditorImportPlugin
	{ "get_parameter_default_value", "_get_parameter_default_value" }, // AnimationNode
	{ "get_parameter_list", "_get_parameter_list" }, // AnimationNode
	{ "get_parent_spatial", "get_parent_node_3d" }, // Node3D
	{ "get_pause_mode", "get_process_mode" }, // Node
	{ "get_physical_scancode", "get_physical_keycode" }, // InputEventKey
	{ "get_physical_scancode_with_modifiers", "get_physical_keycode_with_modifiers" }, // InputEventKey
	{ "get_plugin_icon", "_get_plugin_icon" }, // EditorPlugin
	{ "get_plugin_name", "_get_plugin_name" }, // EditorPlugin
	{ "get_preset_count", "_get_preset_count" }, // EditorImportPlugin
	{ "get_preset_name", "_get_preset_name" }, // EditorImportPlugin
	{ "get_recognized_extensions", "_get_recognized_extensions" }, // ResourceFormatLoader, EditorImportPlugin -- Breaks ResourceSaver.
	{ "get_render_info", "get_rendering_info" }, // RenderingServer
	{ "get_render_targetsize", "get_render_target_size" }, // XRInterface
	{ "get_resource_type", "_get_resource_type" }, // ResourceFormatLoader
	{ "get_result", "get_data" }, // JSON
	{ "get_reverb_bus", "set_reverb_bus_name" }, // Area3D
	{ "get_rpc_sender_id", "get_remote_sender_id" }, // Multiplayer API
	{ "get_save_extension", "_get_save_extension" }, // EditorImportPlugin
	{ "get_scancode", "get_keycode" }, // InputEventKey
	{ "get_scancode_string", "get_keycode_string" }, // OS
	{ "get_scancode_with_modifiers", "get_keycode_with_modifiers" }, // InputEventKey
	{ "get_selected_path", "get_current_directory" }, // EditorInterface
	{ "get_shader_param", "get_shader_parameter" }, // ShaderMaterial
	{ "get_shift", "is_shift_pressed" }, // InputEventWithModifiers
	{ "get_size_override", "get_size_2d_override" }, // SubViewport
	{ "get_slide_count", "get_slide_collision_count" }, // CharacterBody2D, CharacterBody3D
	{ "get_slips_on_slope", "get_slide_on_slope" }, // SeparationRayShape2D, SeparationRayShape3D
	{ "get_space_override_mode", "get_gravity_space_override_mode" }, // Area2D
	{ "get_spatial_node", "get_node_3d" }, // EditorNode3DGizmo
	{ "get_speed", "get_velocity" }, // InputEventMouseMotion
	{ "get_stylebox_types", "get_stylebox_type_list" }, // Theme
	{ "get_surface_material", "get_surface_override_material" }, // MeshInstance3D -- Breaks ImporterMesh.
	{ "get_surface_material_count", "get_surface_override_material_count" }, // MeshInstance3D
	{ "get_tab_disabled", "is_tab_disabled" }, // Tab
	{ "get_tab_hidden", "is_tab_hidden" }, // Tab
	{ "get_target_location", "get_target_position" }, // NavigationAgent2D, NavigationAgent3D
	{ "get_text_align", "get_text_alignment" }, // Button
	{ "get_theme_item_types", "get_theme_item_type_list" }, // Theme
	{ "get_timer_process_mode", "get_timer_process_callback" }, // Timer
	{ "get_translation", "get_position" }, // Node3D -- Breaks GLTFNode, but it is used rarely.
	{ "get_uniform_name", "get_parameter_name" }, // ParameterRef
	{ "get_unit_db", "get_volume_db" }, // AudioStreamPlayer3D
	{ "get_unit_offset", "get_progress_ratio" }, // PathFollow2D, PathFollow3D
	{ "get_use_in_baked_light", "is_baking_navigation" }, // GridMap
	{ "get_verts_per_poly", "get_vertices_per_polygon" }, // NavigationMesh
	{ "get_v_scrollbar", "get_v_scroll_bar" }, // ScrollContainer
	{ "get_visible_name", "_get_visible_name" }, // EditorImportPlugin
	{ "get_window_layout", "_get_window_layout" }, // EditorPlugin
	{ "get_word_under_cursor", "get_word_under_caret" }, // TextEdit
	{ "get_world", "get_world_3d" }, // Viewport, Node3D
	{ "get_zfar", "get_far" }, // Camera3D -- Breaks GLTFCamera
	{ "get_znear", "get_near" }, // Camera3D -- Breaks GLTFCamera
	{ "groove_joint_create", "joint_make_groove" }, // PhysicsServer2D
	{ "handle_menu_selected", "_handle_menu_selected" }, // EditorResourcePicker
	{ "handles_type", "_handles_type" }, // ResourceFormatLoader
	{ "has_color", "has_theme_color" }, // Control -- Breaks Theme
	{ "has_color_override", "has_theme_color_override" }, // Control -- Breaks Theme
	{ "has_constant", "has_theme_constant" }, // Control
	{ "has_constant_override", "has_theme_constant_override" }, // Control
	{ "has_filter", "_has_filter" }, // AnimationNode
	{ "has_font", "has_theme_font" }, // Control -- Breaks Theme
	{ "has_font_override", "has_theme_font_override" }, // Control
	{ "has_icon", "has_theme_icon" }, // Control -- Breaks Theme
	{ "has_icon_override", "has_theme_icon_override" }, // Control
	{ "has_main_screen", "_has_main_screen" }, // EditorPlugin
	{ "has_network_peer", "has_multiplayer_peer" }, // Multiplayer API
	{ "has_stylebox", "has_theme_stylebox" }, // Control -- Breaks Theme
	{ "has_stylebox_override", "has_theme_stylebox_override" }, // Control
	{ "http_escape", "uri_encode" }, // String
	{ "http_unescape", "uri_decode" }, // String
	{ "import_scene_from_other_importer", "_import_scene" }, // EditorSceneFormatImporter
	{ "instance_set_surface_material", "instance_set_surface_override_material" }, // RenderingServer
	{ "interpolate", "sample" }, // Curve, Curve2D, Curve3D, Gradient
	{ "intersect_polygons_2d", "intersect_polygons" }, // Geometry2D
	{ "intersect_polyline_with_polygon_2d", "intersect_polyline_with_polygon" }, // Geometry2D
	{ "is_a_parent_of", "is_ancestor_of" }, // Node
	{ "is_commiting_action", "is_committing_action" }, // UndoRedo
	{ "is_doubleclick", "is_double_click" }, // InputEventMouseButton
	{ "is_draw_red", "is_draw_warning" }, // EditorProperty
	{ "is_follow_smoothing_enabled", "is_position_smoothing_enabled" }, // Camera2D
	{ "is_h_drag_enabled", "is_drag_horizontal_enabled" }, // Camera2D
	{ "is_handle_highlighted", "_is_handle_highlighted" }, // EditorNode3DGizmo, EditorNode3DGizmoPlugin
	{ "is_inverting_faces", "get_flip_faces" }, // CSGPrimitive3D
	{ "is_network_master", "is_multiplayer_authority" }, // Node
	{ "is_network_server", "is_server" }, // Multiplayer API
	{ "is_normalmap", "is_normal_map" }, // NoiseTexture
	{ "is_refusing_new_network_connections", "is_refusing_new_connections" }, // Multiplayer API
	{ "is_region", "is_region_enabled" }, // Sprite2D
	{ "is_rotating", "is_ignoring_rotation" }, // Camera2D
	{ "is_scancode_unicode", "is_keycode_unicode" }, // OS
	{ "is_selectable_when_hidden", "_is_selectable_when_hidden" }, // EditorNode3DGizmoPlugin
	{ "is_set_as_toplevel", "is_set_as_top_level" }, // CanvasItem
	{ "is_shortcut", "matches_event" }, // Shortcut
	{ "is_size_override_stretch_enabled", "is_size_2d_override_stretch_enabled" }, // SubViewport
	{ "is_sort_enabled", "is_y_sort_enabled" }, // Node2D
	{ "is_static_body", "is_able_to_sleep" }, // PhysicalBone3D -- Not sure.
	{ "is_v_drag_enabled", "is_drag_vertical_enabled" }, // Camera2D
	{ "joint_create_cone_twist", "joint_make_cone_twist" }, // PhysicsServer3D
	{ "joint_create_generic_6dof", "joint_make_generic_6dof" }, // PhysicsServer3D
	{ "joint_create_hinge", "joint_make_hinge" }, // PhysicsServer3D
	{ "joint_create_pin", "joint_make_pin" }, // PhysicsServer3D
	{ "joint_create_slider", "joint_make_slider" }, // PhysicsServer3D
	{ "line_intersects_line_2d", "line_intersects_line" }, // Geometry2D
	{ "load_from_globals", "load_from_project_settings" }, // InputMap
	{ "load_interactive", "load_threaded_request" }, // ResourceLoader -- "load_threaded_request" could be an alternative, but it is used differently.
	{ "make_convex_from_brothers", "make_convex_from_siblings" }, // CollisionShape3D
	{ "make_visible", "_make_visible" }, // EditorPlugin
	{ "merge_polygons_2d", "merge_polygons" }, // Geometry2D
	{ "mesh_surface_get_format", "mesh_surface_get_format_attribute_stride" }, // RenderingServer
	{ "mesh_surface_update_region", "mesh_surface_update_attribute_region" }, // RenderingServer
	{ "move_to_bottom", "move_after" }, // Skeleton3D
	{ "move_to_top", "move_before" }, // Skeleton3D
	{ "multimesh_allocate", "multimesh_allocate_data" }, // RenderingServer
	{ "normalmap_to_xy", "normal_map_to_xy" }, // Image
	{ "offset_polygon_2d", "offset_polygon" }, // Geometry2D
	{ "offset_polyline_2d", "offset_polyline" }, // Geometry2D
	{ "percent_decode", "uri_decode" }, // String
	{ "percent_encode", "uri_encode" }, // String
	{ "pin_joint_create", "joint_make_pin" }, // PhysicsServer2D
	{ "popup_centered_minsize", "popup_centered_clamped" }, // Window
	{ "post_import", "_post_import" }, // EditorScenePostImport
	{ "print_stray_nodes", "print_orphan_nodes" }, // Node
	{ "property_list_changed_notify", "notify_property_list_changed" }, // Object
	{ "recognize", "_recognize" }, // ResourceFormatLoader
	{ "regen_normalmaps", "regen_normal_maps" }, // ArrayMesh
	{ "region_bake_navmesh", "region_bake_navigation_mesh" }, // Navigation3DServer
	{ "region_set_navmesh", "region_set_navigation_mesh" }, // Navigation3DServer
	{ "region_set_navpoly", "region_set_navigation_polygon" }, // Navigation2DServer
	{ "remove_animation", "remove_animation_library" }, // AnimationPlayer
	{ "remove_color_override", "remove_theme_color_override" }, // Control
	{ "remove_constant_override", "remove_theme_constant_override" }, // Control
	{ "remove_font_override", "remove_theme_font_override" }, // Control
	{ "remove_icon_override", "remove_theme_icon_override" }, // Control
	{ "remove_scene_import_plugin", "remove_scene_format_importer_plugin" }, //EditorPlugin
	{ "remove_spatial_gizmo_plugin", "remove_node_3d_gizmo_plugin" }, // EditorPlugin
	{ "remove_stylebox_override", "remove_theme_stylebox_override" }, // Control
	{ "rename_animation", "rename_animation_library" }, // AnimationPlayer
	{ "rename_dependencies", "_rename_dependencies" }, // ResourceFormatLoader
	{ "save_external_data", "_save_external_data" }, // EditorPlugin
	{ "segment_intersects_segment_2d", "segment_intersects_segment" }, // Geometry2D
	{ "set_adjustment_enable", "set_adjustment_enabled" }, // Environment
	{ "set_alt", "set_alt_pressed" }, // InputEventWithModifiers
	{ "set_anchor_and_margin", "set_anchor_and_offset" }, // Control
	{ "set_anchors_and_margins_preset", "set_anchors_and_offsets_preset" }, // Control
	{ "set_animation_process_mode", "set_process_callback" }, // AnimationPlayer
	{ "set_as_bulk_array", "set_buffer" }, // MultiMesh
	{ "set_as_normalmap", "set_as_normal_map" }, // NoiseTexture
	{ "set_as_toplevel", "set_as_top_level" }, // CanvasItem
	{ "set_audio_bus", "set_audio_bus_name" }, // Area3D
	{ "set_autowrap", "set_autowrap_mode" }, // Label -- Breaks AcceptDialog.
	{ "set_cast_to", "set_target_position" }, // RayCast2D, RayCast3D
	{ "set_collision_layer_bit", "set_collision_layer_value" }, // CSGShape3D, and a lot of others like GridMap.
	{ "set_collision_mask_bit", "set_collision_mask_value" }, // CSGShape3D, and a lot of others like GridMap.
	{ "set_column_min_width", "set_column_custom_minimum_width" }, // Tree
	{ "set_command", "set_meta_pressed" }, // InputEventWithModifiers
	{ "set_control", "set_ctrl_pressed" }, // InputEventWithModifiers
	{ "set_create_options", "_set_create_options" }, //  EditorResourcePicker
	{ "set_cull_mask_bit", "set_cull_mask_value" }, // Camera3D
	{ "set_cursor_position", "set_caret_column" }, // LineEdit
	{ "set_d", "set_distance" }, // WorldMarginShape2D
	{ "set_default_length", "set_length" }, // Bone2D
	{ "set_depth_bias_enable", "set_depth_bias_enabled" }, // RDPipelineRasterizationState
	{ "set_device", "set_output_device" }, // AudioServer
	{ "set_doubleclick", "set_double_click" }, // InputEventMouseButton
	{ "set_draw_red", "set_draw_warning" }, // EditorProperty
	{ "set_enable_follow_smoothing", "set_position_smoothing_enabled" }, // Camera2D
	{ "set_enabled_focus_mode", "set_focus_mode" }, // BaseButton
	{ "set_endian_swap", "set_big_endian" }, // File
	{ "set_expand_to_text_length", "set_expand_to_text_length_enabled" }, // LineEdit
	{ "set_filename", "set_scene_file_path" }, // Node -- WARNING: This may be used in a lot of other places.
	{ "set_focus_neighbour", "set_focus_neighbor" }, // Control
	{ "set_follow_smoothing", "set_position_smoothing_speed" }, // Camera2D
	{ "set_frame_color", "set_color" }, // ColorRect
	{ "set_global_rate_scale", "set_playback_speed_scale" }, // AudioServer
	{ "set_gravity_distance_scale", "set_gravity_point_unit_distance" }, // Area2D, Area3D
	{ "set_gravity_vector", "set_gravity_direction" }, // Area2D, Area3D
	{ "set_h_drag_enabled", "set_drag_horizontal_enabled" }, // Camera2D
	{ "set_icon_align", "set_icon_alignment" }, // Button
	{ "set_interior_ambient", "set_ambient_color" }, // ReflectionProbe
	{ "set_interior_ambient_energy", "set_ambient_color_energy" }, // ReflectionProbe
	{ "set_invert_faces", "set_flip_faces" }, // CSGPrimitive3D
	{ "set_is_initialized", "_is_initialized" }, // XRInterface
	{ "set_is_primary", "set_primary" }, // XRInterface
	{ "set_item_navmesh", "set_item_navigation_mesh" }, // MeshLibrary
	{ "set_item_navmesh_transform", "set_item_navigation_mesh_transform" }, // MeshLibrary
	{ "set_iterations_per_second", "set_physics_ticks_per_second" }, // Engine
	{ "set_layer_mask_bit", "set_layer_mask_value" }, // VisualInstance3D
	{ "set_margins_preset", "set_offsets_preset" }, //  Control
	{ "set_max_atlas_size", "set_max_texture_size" }, // LightmapGI
	{ "set_metakey", "set_meta_pressed" }, // InputEventWithModifiers
	{ "set_mid_height", "set_height" }, // CapsuleMesh
	{ "set_neighbor_dist", "set_neighbor_distance" }, // NavigationAgent2D, NavigationAgent3D
	{ "set_network_master", "set_multiplayer_authority" }, // Node
	{ "set_network_peer", "set_multiplayer_peer" }, // Multiplayer API
	{ "set_oneshot", "set_one_shot" }, // AnimatedTexture
	{ "set_pause_mode", "set_process_mode" }, // Node
	{ "set_physical_scancode", "set_physical_keycode" }, // InputEventKey
	{ "set_proximity_fade", "set_proximity_fade_enabled" }, // Material
	{ "set_refuse_new_network_connections", "set_refuse_new_connections" }, // Multiplayer API
	{ "set_region", "set_region_enabled" }, // Sprite2D -- Sprite breaks AtlasTexture.
	{ "set_region_filter_clip", "set_region_filter_clip_enabled" }, // Sprite2D
	{ "set_reverb_bus", "set_reverb_bus_name" }, // Area3D
	{ "set_rotate", "set_rotates" }, // PathFollow2D
	{ "set_scancode", "set_keycode" }, // InputEventKey
	{ "set_shader_param", "set_shader_parameter" }, // ShaderMaterial
	{ "set_shift", "set_shift_pressed" }, // InputEventWithModifiers
	{ "set_size_override", "set_size_2d_override" }, // SubViewport -- Breaks ImageTexture.
	{ "set_size_override_stretch", "set_size_2d_override_stretch" }, // SubViewport
	{ "set_slips_on_slope", "set_slide_on_slope" }, // SeparationRayShape2D, SeparationRayShape3D
	{ "set_sort_enabled", "set_y_sort_enabled" }, // Node2D
	{ "set_space_override_mode", "set_gravity_space_override_mode" }, // Area2D
	{ "set_spatial_node", "set_node_3d" }, // EditorNode3DGizmo
	{ "set_speed", "set_velocity" }, // InputEventMouseMotion
	{ "set_ssao_edge_sharpness", "set_ssao_sharpness" }, // Environment
	{ "set_surface_material", "set_surface_override_material" }, // MeshInstance3D -- Breaks ImporterMesh.
	{ "set_tab_align", "set_tab_alignment" }, // TabContainer
	{ "set_tangent", "surface_set_tangent" }, // ImmediateGeometry -- Breaks SurfaceTool.
	{ "set_target_location", "set_target_position" }, // NavigationAgent2D, NavigationAgent3D
	{ "set_text_align", "set_text_alignment" }, // Button
	{ "set_timer_process_mode", "set_timer_process_callback" }, // Timer
	{ "set_translation", "set_position" }, // Node3D -- This breaks GLTFNode, but it is used rarely.
	{ "set_uniform_name", "set_parameter_name" }, // ParameterRef
	{ "set_unit_db", "set_volume_db" }, // AudioStreamPlayer3D
	{ "set_unit_offset", "set_progress_ratio" }, // PathFollow2D, PathFollow3D
	{ "set_uv2", "surface_set_uv2" }, // ImmediateMesh -- Breaks SurfaceTool.
	{ "set_verts_per_poly", "set_vertices_per_polygon" }, // NavigationMesh
	{ "set_v_drag_enabled", "set_drag_vertical_enabled" }, // Camera2D
	{ "set_valign", "set_vertical_alignment" }, // Label
	{ "set_window_layout", "_set_window_layout" }, // EditorPlugin
	{ "set_zfar", "set_far" }, // Camera3D -- Breaks GLTFCamera.
	{ "set_znear", "set_near" }, // Camera3D -- Breaks GLTFCamera.
	{ "shortcut_match", "is_match" }, // InputEvent
	{ "skeleton_allocate", "skeleton_allocate_data" }, // RenderingServer
	{ "surface_update_region", "surface_update_attribute_region" }, // ArrayMesh
	{ "track_remove_key_at_position", "track_remove_key_at_time" }, // Animation
	{ "triangulate_delaunay_2d", "triangulate_delaunay" }, // Geometry2D
	{ "unselect", "deselect" }, // ItemList
	{ "unselect_all", "deselect_all" }, // ItemList
	{ "update_configuration_warning", "update_configuration_warnings" }, // Node
	{ "update_gizmo", "update_gizmos" }, // Node3D
	{ "viewport_set_use_arvr", "viewport_set_use_xr" }, // RenderingServer
	{ "warp_mouse_position", "warp_mouse" }, // Input
	{ "world_to_map", "local_to_map" }, // TileMap, GridMap

	// Builtin types
	// Remember to add them to the builtin_types_excluded_functions variable, because for now these functions cannot be listed.
	// { "empty", "is_empty" }, // Array -- Used as custom rule. Be careful, this will be used everywhere.
	// { "invert", "reverse" }, // Array -- Give it a check. Be careful, this will be used everywhere.
	// { "remove", "remove_at" }, // Array -- Breaks Directory and several more.
	{ "clamped", "limit_length" }, // Vector2
	{ "get_rotation_quat", "get_rotation_quaternion" }, // Basis
	{ "grow_margin", "grow_side" }, // Rect2
	{ "is_abs_path", "is_absolute_path" }, // String
	{ "is_valid_integer", "is_valid_int" }, // String
	{ "linear_interpolate", "lerp" }, // Color
	{ "find_last", "rfind" }, // Array, String
	{ "to_ascii", "to_ascii_buffer" }, // String
	{ "to_utf8", "to_utf8_buffer" }, // String
	{ "to_wchar", "to_wchar_buffer" }, // String

	// @GlobalScope
	// Remember to add them to the builtin_types_excluded_functions variable, because for now these functions cannot be listed.
	{ "bytes2var", "bytes_to_var" },
	{ "bytes2var_with_objects", "bytes_to_var_with_objects" },
	{ "db2linear", "db_to_linear" },
	{ "deg2rad", "deg_to_rad" },
	{ "linear2db", "linear_to_db" },
	{ "rad2deg", "rad_to_deg" },
	{ "rand_range", "randf_range" },
	{ "range_lerp", "remap" },
	{ "stepify", "snapped" },
	{ "str2var", "str_to_var" },
	{ "var2str", "var_to_str" },
	{ "var2bytes", "var_to_bytes" },
	{ "var2bytes_with_objects", "var_to_bytes_with_objects" },

	// @GDScript
	// Remember to add them to the builtin_types_excluded_functions variable, because for now these functions cannot be listed.
	{ "dict2inst", "dict_to_inst" },
	{ "inst2dict", "inst_to_dict" },

	{ nullptr, nullptr },
};

// gdscript_function_renames clone with CamelCase
const char *RenamesMap3To4::csharp_function_renames[][2] = {
	{ "_AboutToShow", "_AboutToPopup" }, // ColorPickerButton
	{ "_GetConfigurationWarning", "_GetConfigurationWarnings" }, // Node
	{ "_SetCurrent", "SetCurrent" }, // Camera2D
	{ "_SetEditorDescription", "SetEditorDescription" }, // Node
	{ "_SetPlaying", "SetPlaying" }, // AnimatedSprite3D
	{ "_ToplevelRaiseSelf", "_TopLevelRaiseSelf" }, // CanvasItem
	{ "AddCancel", "AddCancelButton" }, // AcceptDialog
	{ "AddCentralForce", "AddConstantCentralForce" }, //RigidBody2D
	{ "AddChildBelowNode", "AddSibling" }, // Node
	{ "AddColorOverride", "AddThemeColorOverride" }, // Control
	{ "AddConstantOverride", "AddThemeConstantOverride" }, // Control
	{ "AddFontOverride", "AddThemeFontOverride" }, // Control
	{ "AddForce", "AddConstantForce" }, //RigidBody2D
	{ "AddIconOverride", "AddThemeIconOverride" }, // Control
	{ "AddSceneImportPlugin", "AddSceneFormatImporterPlugin" }, //EditorPlugin
	{ "AddSpatialGizmoPlugin", "AddNode3dGizmoPlugin" }, // EditorPlugin
	{ "AddStyleboxOverride", "AddThemeStyleboxOverride" }, // Control
	{ "AddTorque", "AddConstantTorque" }, //RigidBody2D
	{ "AgentSetNeighborDist", "AgentSetNeighborDistance" }, // NavigationServer2D, NavigationServer3D
	{ "BindChildNodeToBone", "SetBoneChildren" }, // Skeleton3D
	{ "BumpmapToNormalmap", "BumpMapToNormalMap" }, // Image
	{ "CanBeHidden", "_CanBeHidden" }, // EditorNode3DGizmoPlugin
	{ "CanDropData", "_CanDropData" }, // Control
	{ "CanDropDataFw", "_CanDropDataFw" }, // ScriptEditor
	{ "CanGenerateSmallPreview", "_CanGenerateSmallPreview" }, // EditorResourcePreviewGenerator
	{ "CanInstance", "CanInstantiate" }, // PackedScene, Script
	{ "CanvasLightSetScale", "CanvasLightSetTextureScale" }, // RenderingServer
	{ "CaptureGetDevice", "GetInputDevice" }, // AudioServer
	{ "CaptureGetDeviceList", "GetInputDeviceList" }, // AudioServer
	{ "CaptureSetDevice", "SetInputDevice" }, // AudioServer
	{ "CenterViewportToCursor", "CenterViewportToCaret" }, // TextEdit
	{ "ChangeScene", "ChangeSceneToFile" }, // SceneTree
	{ "ChangeSceneTo", "ChangeSceneToPacked" }, // SceneTree
	{ "ClipPolygons2d", "ClipPolygons" }, // Geometry2D
	{ "ClipPolylineWithPolygon2d", "ClipPolylineWithPolygon" }, //Geometry2D
	{ "CommitHandle", "_CommitHandle" }, // EditorNode3DGizmo
	{ "ConvexHull2d", "ConvexHull" }, // Geometry2D
	{ "CursorGetBlinkSpeed", "GetCaretBlinkInterval" }, // TextEdit
	{ "CursorGetColumn", "GetCaretColumn" }, // TextEdit
	{ "CursorGetLine", "GetCaretLine" }, // TextEdit
	{ "CursorSetBlinkEnabled", "SetCaretBlinkEnabled" }, // TextEdit
	{ "CursorSetBlinkSpeed", "SetCaretBlinkInterval" }, // TextEdit
	{ "CursorSetColumn", "SetCaretColumn" }, // TextEdit
	{ "CursorSetLine", "SetCaretLine" }, // TextEdit
	{ "DampedSpringJointCreate", "JointMakeDampedSpring" }, // PhysicsServer2D
	{ "DampedStringJointGetParam", "DampedSpringJointGetParam" }, // PhysicsServer2D
	{ "DampedStringJointSetParam", "DampedSpringJointSetParam" }, // PhysicsServer2D
	{ "DeleteCharAtCursor", "DeleteCharAtCaret" }, // LineEdit
	{ "DeselectItems", "DeselectAll" }, // FileDialog
	{ "DropData", "_DropData" }, // Control
	{ "DropDataFw", "_DropDataFw" }, // ScriptEditor
	{ "ExcludePolygons2d", "ExcludePolygons" }, // Geometry2D
	{ "FindScancodeFromString", "FindKeycodeFromString" }, // OS
	{ "ForwardCanvasDrawOverViewport", "_ForwardCanvasDrawOverViewport" }, // EditorPlugin
	{ "ForwardCanvasForceDrawOverViewport", "_ForwardCanvasForceDrawOverViewport" }, // EditorPlugin
	{ "ForwardCanvasGuiInput", "_ForwardCanvasGuiInput" }, // EditorPlugin
	{ "ForwardSpatialDrawOverViewport", "_Forward3dDrawOverViewport" }, // EditorPlugin
	{ "ForwardSpatialForceDrawOverViewport", "_Forward3dForceDrawOverViewport" }, // EditorPlugin
	{ "ForwardSpatialGuiInput", "_Forward3dGuiInput" }, // EditorPlugin
	{ "GenerateFromPath", "_GenerateFromPath" }, // EditorResourcePreviewGenerator
	{ "GenerateSmallPreviewAutomatically", "_GenerateSmallPreviewAutomatically" }, // EditorResourcePreviewGenerator
	{ "GetActionList", "ActionGetEvents" }, // InputMap
	{ "GetAlt", "IsAltPressed" }, // InputEventWithModifiers
	{ "GetAnimationProcessMode", "GetProcessCallback" }, // AnimationPlayer
	{ "GetAppliedForce", "GetConstantForce" }, //RigidBody2D
	{ "GetAppliedTorque", "GetConstantTorque" }, //RigidBody2D
	{ "GetAudioBus", "GetAudioBusName" }, // Area3D
	{ "GetBoundChildNodesToBone", "GetBoneChildren" }, // Skeleton3D
	{ "GetCamera", "GetCamera3d" }, // Viewport -- This is also convertible to GetCamera2d. Breaks GLTFNode.
	{ "GetCancel", "GetCancelButton" }, // ConfirmationDialog
	{ "GetCaption", "_GetCaption" }, // AnimationNode
	{ "GetCastTo", "GetTargetPosition" }, // RayCast2D, RayCast3D
	{ "GetChildByName", "_GetChildByName" }, // AnimationNode
	{ "GetChildNodes", "_GetChildNodes" }, // AnimationNode
	{ "GetClosestPointToSegment2d", "GetClosestPointToSegment" }, // Geometry2D
	{ "GetClosestPointToSegmentUncapped2d", "GetClosestPointToSegmentUncapped" }, // Geometry2D
	{ "GetClosestPointsBetweenSegments2d", "GetClosestPointToSegment" }, // Geometry2D
	{ "GetCollisionLayerBit", "GetCollisionLayerValue" }, // CSGShape3D, and a lot of others like GridMap.
	{ "GetCollisionMaskBit", "GetCollisionMaskValue" }, // CSGShape3D, and a lot of others like GridMap.
	{ "GetColorTypes", "GetColorTypeList" }, // Theme
	{ "GetCommand", "IsCommandPressed" }, // InputEventWithModifiers
	{ "GetConstantTypes", "GetConstantTypeList" }, // Theme
	{ "GetControl", "IsCtrlPressed" }, // InputEventWithModifiers
	{ "GetCullMaskBit", "GetCullMaskValue" }, // Camera3D
	{ "GetCursorPosition", "GetCaretColumn" }, // LineEdit
	{ "GetD", "GetDistance" }, // LineShape2D
	{ "GetDefaultLength", "GetLength" }, // Bone2D
	{ "GetDepthBiasEnable", "GetDepthBiasEnabled" }, // RDPipelineRasterizationState
	{ "GetDevice", "GetOutputDevice" }, // AudioServer
	{ "GetDeviceList", "GetOutputDeviceList" }, // AudioServer
	{ "GetDragDataFw", "_GetDragDataFw" }, // ScriptEditor
	{ "GetEditorViewport", "GetViewport" }, // EditorPlugin
	{ "GetEnabledFocusMode", "GetFocusMode" }, // BaseButton
	{ "GetEndianSwap", "IsBigEndian" }, // File
	{ "GetErrorString", "GetErrorMessage" }, // JSON
	{ "GetFinalLocation", "GetFinalPosition" }, // NavigationAgent2D, NavigationAgent3D
	{ "GetFocusNeighbour", "GetFocusNeighbor" }, // Control
	{ "GetFollowSmoothing", "GetPositionSmoothingSpeed" }, // Camera2D
	{ "GetFontTypes", "GetFontTypeList" }, // Theme
	{ "GetFrameColor", "GetColor" }, // ColorRect
	{ "GetGlobalRateScale", "GetPlaybackSpeedScale" }, // AudioServer
	{ "GetGravityDistanceScale", "GetGravityPointDistanceScale" }, // Area2D
	{ "GetGravityVector", "GetGravityDirection" }, // Area2D
	{ "GetHScrollbar", "GetHScrollBar" }, // ScrollContainer
	{ "GetHand", "GetTrackerHand" }, // XRPositionalTracker
	{ "GetHandleName", "_GetHandleName" }, // EditorNode3DGizmo
	{ "GetHandleValue", "_GetHandleValue" }, // EditorNode3DGizmo
	{ "GetIconAlign", "GetIconAlignment" }, // Button
	{ "GetIconTypes", "GetIconTypeList" }, // Theme
	{ "GetIdleFrames", "GetProcessFrames" }, // Engine
	{ "GetImportOptions", "_GetImportOptions" }, // EditorImportPlugin
	{ "GetImportOrder", "_GetImportOrder" }, // EditorImportPlugin
	{ "GetImporterName", "_GetImporterName" }, // EditorImportPlugin
	{ "GetInteriorAmbient", "GetAmbientColor" }, // ReflectionProbe
	{ "GetInteriorAmbientEnergy", "GetAmbientColorEnergy" }, // ReflectionProbe
	{ "GetItemNavmesh", "GetItemMavigationMesh" }, // MeshLibrary
	{ "GetItemNavmeshTransform", "GetItemNavigationMeshTransform" }, // MeshLibrary
	{ "GetIterationsPerSecond", "GetPhysicsTicksPerSecond" }, // Engine
	{ "GetLastMouseSpeed", "GetLastMouseVelocity" }, // Input
	{ "GetLayerMaskBit", "GetLayerMaskValue" }, // VisualInstance3D
	{ "GetLen", "GetLength" }, // File
	{ "GetMaxAtlasSize", "GetMaxTextureSize" }, // LightmapGI
	{ "GetMetakey", "IsMetaPressed" }, // InputEventWithModifiers
	{ "GetMidHeight", "GetHeight" }, // CapsuleMesh
	{ "GetMotionRemainder", "GetRemainder" }, // PhysicsTestMotionResult2D
	{ "GetNavPath", "GetCurrentNavigationPath" }, // NavigationAgent2D, NavigationAgent3D
	{ "GetNavPathIndex", "GetCurrentNavigationPathIndex" }, // NavigationAgent2D, NavigationAgent3D
	{ "GetNeighborDist", "GetNeighborDistance" }, // NavigationAgent2D, NavigationAgent3D
	{ "GetNetworkConnectedPeers", "GetPeers" }, // Multiplayer API
	{ "GetNetworkMaster", "GetMultiplayerAuthority" }, // Node
	{ "GetNetworkPeer", "GetMultiplayerPeer" }, // Multiplayer API
	{ "GetNetworkUniqueId", "GetUniqueId" }, // Multiplayer API
	{ "GetNextLocation", "GetNextPathPosition" }, // NavigationAgent2D, NavigationAgent3D
	{ "GetOneshot", "GetOneShot" }, // AnimatedTexture
	{ "GetOk", "GetOkButton" }, // AcceptDialog
	{ "GetOptionVisibility", "_GetOptionVisibility" }, // EditorImportPlugin
	{ "GetParameterDefaultValue", "_GetParameterDefaultValue" }, // AnimationNode
	{ "GetParameterList", "_GetParameterList" }, // AnimationNode
	{ "GetParentSpatial", "GetParentNode3d" }, // Node3D
	{ "GetPhysicalScancode", "GetPhysicalKeycode" }, // InputEventKey
	{ "GetPhysicalScancodeWithModifiers", "GetPhysicalKeycodeWithModifiers" }, // InputEventKey
	{ "GetPluginIcon", "_GetPluginIcon" }, // EditorPlugin
	{ "GetPluginName", "_GetPluginName" }, // EditorPlugin
	{ "GetPresetCount", "_GetPresetCount" }, // EditorImportPlugin
	{ "GetPresetName", "_GetPresetName" }, // EditorImportPlugin
	{ "GetRecognizedExtensions", "_GetRecognizedExtensions" }, // ResourceFormatLoader, EditorImportPlugin -- Breaks ResourceSaver.
	{ "GetRenderInfo", "GetRenderingInfo" }, // RenderingServer
	{ "GetRenderTargetsize", "GetRenderTargetSize" }, // XRInterface
	{ "GetResourceType", "_GetResourceType" }, // ResourceFormatLoader
	{ "GetResult", "GetData" }, // JSON
	{ "GetReverbBus", "GetReverbBusName" }, // Area3D
	{ "GetRpcSenderId", "GetRemoteSenderId" }, // Multiplayer API
	{ "GetSaveExtension", "_GetSaveExtension" }, // EditorImportPlugin
	{ "GetScancode", "GetKeycode" }, // InputEventKey
	{ "GetScancodeString", "GetKeycodeString" }, // OS
	{ "GetScancodeWithModifiers", "GetKeycodeWithModifiers" }, // InputEventKey
	{ "GetShaderParam", "GetShaderParameter" }, // ShaderMaterial
	{ "GetShift", "IsShiftPressed" }, // InputEventWithModifiers
	{ "GetSizeOverride", "GetSize2dOverride" }, // SubViewport
	{ "GetSlipsOnSlope", "GetSlideOnSlope" }, // SeparationRayShape2D, SeparationRayShape3D
	{ "GetSpaceOverrideMode", "GetGravitySpaceOverrideMode" }, // Area2D
	{ "GetSpatialNode", "GetNode3d" }, // EditorNode3DGizmo
	{ "GetSpeed", "GetVelocity" }, // InputEventMouseMotion
	{ "GetStyleboxTypes", "GetStyleboxTypeList" }, // Theme
	{ "GetSurfaceMaterial", "GetSurfaceOverrideMaterial" }, // MeshInstance3D -- Breaks ImporterMesh.
	{ "GetSurfaceMaterialCount", "GetSurfaceOverrideMaterialCount" }, // MeshInstance3D
	{ "GetTabDisabled", "IsTabDisabled" }, // Tab
	{ "GetTabHidden", "IsTabHidden" }, // Tab
	{ "GetTargetLocation", "GetTargetPosition" }, // NavigationAgent2D, NavigationAgent3D
	{ "GetTextAlign", "GetTextAlignment" }, // Button
	{ "GetThemeItemTypes", "GetThemeItemTypeList" }, // Theme
	{ "GetTimerProcessMode", "GetTimerProcessCallback" }, // Timer
	{ "GetTranslation", "GetPosition" }, // Node3D -- Breaks GLTFNode, but it is used rarely.
	{ "GetUniformName", "GetParameterName" }, // ParameterRef
	{ "GetUnitDb", "GetVolumeDb" }, // AudioStreamPlayer3D
	{ "GetUnitOffset", "GetProgressRatio" }, // PathFollow2D, PathFollow3D
	{ "GetUseInBakedLight", "IsBakingNavigation" }, // GridMap
	{ "GetVertsPerPoly", "GetVerticesPerPolygon" }, // NavigationMesh
	{ "GetVScrollbar", "GetVScrollBar" }, // ScrollContainer
	{ "GetVisibleName", "_GetVisibleName" }, // EditorImportPlugin
	{ "GetWindowLayout", "_GetWindowLayout" }, // EditorPlugin
	{ "GetWordUnderCursor", "GetWordUnderCaret" }, // TextEdit
	{ "GetWorld", "GetWorld3d" }, // Viewport, Node3D
	{ "GetZfar", "GetFar" }, // Camera3D -- Breaks GLTFCamera
	{ "GetZnear", "GetNear" }, // Camera3D -- Breaks GLTFCamera
	{ "GrooveJointCreate", "JointMakeGroove" }, // PhysicsServer2D
	{ "HandleMenuSelected", "_HandleMenuSelected" }, // EditorResourcePicker
	{ "HandlesType", "_HandlesType" }, // ResourceFormatLoader
	{ "HasColor", "HasThemeColor" }, // Control -- Breaks Theme
	{ "HasColorOverride", "HasThemeColorOverride" }, // Control -- Breaks Theme
	{ "HasConstant", "HasThemeConstant" }, // Control
	{ "HasConstantOverride", "HasThemeConstantOverride" }, // Control
	{ "HasFilter", "_HasFilter" }, // AnimationNode
	{ "HasFont", "HasThemeFont" }, // Control -- Breaks Theme
	{ "HasFontOverride", "HasThemeFontOverride" }, // Control
	{ "HasIcon", "HasThemeIcon" }, // Control -- Breaks Theme
	{ "HasIconOverride", "HasThemeIconOverride" }, // Control
	{ "HasMainScreen", "_HasMainScreen" }, // EditorPlugin
	{ "HasNetworkPeer", "HasMultiplayerPeer" }, // Multiplayer API
	{ "HasStylebox", "HasThemeStylebox" }, // Control -- Breaks Theme
	{ "HasStyleboxOverride", "HasThemeStyleboxOverride" }, // Control
	{ "HttpEscape", "UriEncode" }, // String
	{ "HttpUnescape", "UriDecode" }, // String
	{ "ImportAnimationFromOtherImporter", "_ImportAnimation" }, // EditorSceneFormatImporter
	{ "ImportSceneFromOtherImporter", "_ImportScene" }, // EditorSceneFormatImporter
	{ "InstanceSetSurfaceMaterial", "InstanceSetSurfaceOverrideMaterial" }, // RenderingServer
	{ "IntersectPolygons2d", "IntersectPolygons" }, // Geometry2D
	{ "IntersectPolylineWithPolygon2d", "IntersectPolylineWithPolygon" }, // Geometry2D
	{ "IsAParentOf", "IsAncestorOf" }, // Node
	{ "IsCommitingAction", "IsCommittingAction" }, // UndoRedo
	{ "IsDoubleclick", "IsDoubleClick" }, // InputEventMouseButton
	{ "IsFollowSmoothingEnabled", "IsPositionSmoothingEnabled" }, // Camera2D
	{ "IsHDragEnabled", "IsDragHorizontalEnabled" }, // Camera2D
	{ "IsHandleHighlighted", "_IsHandleHighlighted" }, // EditorNode3DGizmo, EditorNode3DGizmoPlugin
	{ "IsNetworkMaster", "IsMultiplayerAuthority" }, // Node
	{ "IsNetworkServer", "IsServer" }, // Multiplayer API
	{ "IsNormalmap", "IsNormalMap" }, // NoiseTexture
	{ "IsRefusingNewNetworkConnections", "IsRefusingNewConnections" }, // Multiplayer API
	{ "IsRegion", "IsRegionEnabled" }, // Sprite2D
	{ "IsRotating", "IsIgnoringRotation" }, // Camera2D
	{ "IsScancodeUnicode", "IsKeycodeUnicode" }, // OS
	{ "IsSelectableWhenHidden", "_IsSelectableWhenHidden" }, // EditorNode3DGizmoPlugin
	{ "IsSetAsToplevel", "IsSetAsTopLevel" }, // CanvasItem
	{ "IsShortcut", "MatchesEvent" }, // Shortcut
	{ "IsSizeOverrideStretchEnabled", "IsSize2dOverrideStretchEnabled" }, // SubViewport
	{ "IsSortEnabled", "IsYSortEnabled" }, // Node2D
	{ "IsStaticBody", "IsAbleToSleep" }, // PhysicalBone3D -- Not sure.
	{ "IsVDragEnabled", "IsDragVerticalEnabled" }, // Camera2D
	{ "JointCreateConeTwist", "JointMakeConeTwist" }, // PhysicsServer3D
	{ "JointCreateGeneric6dof", "JointMakeGeneric6dof" }, // PhysicsServer3D
	{ "JointCreateHinge", "JointMakeHinge" }, // PhysicsServer3D
	{ "JointCreatePin", "JointMakePin" }, // PhysicsServer3D
	{ "JointCreateSlider", "JointMakeSlider" }, // PhysicsServer3D
	{ "LineIntersectsLine2d", "LineIntersectsLine" }, // Geometry2D
	{ "LoadFromGlobals", "LoadFromProjectSettings" }, // InputMap
	{ "MakeConvexFromBrothers", "MakeConvexFromSiblings" }, // CollisionShape3D
	{ "MergePolygons2d", "MergePolygons" }, // Geometry2D
	{ "MeshSurfaceGetFormat", "MeshSurfaceGetFormatAttributeStride" }, // RenderingServer
	{ "MeshSurfaceUpdateRegion", "MeshSurfaceUpdateAttributeRegion" }, // RenderingServer
	{ "MoveToBottom", "MoveAfter" }, // Skeleton3D
	{ "MoveToTop", "MoveBefore" }, // Skeleton3D
	{ "MultimeshAllocate", "MultimeshAllocateData" }, // RenderingServer
	{ "NormalmapToXy", "NormalMapToXy" }, // Image
	{ "OffsetPolygon2d", "OffsetPolygon" }, // Geometry2D
	{ "OffsetPolyline2d", "OffsetPolyline" }, // Geometry2D
	{ "PercentDecode", "UriDecode" }, // String
	{ "PercentEncode", "UriEncode" }, // String
	{ "PinJointCreate", "JointMakePin" }, // PhysicsServer2D
	{ "PopupCenteredMinsize", "PopupCenteredClamped" }, // Window
	{ "PostImport", "_PostImport" }, // EditorScenePostImport
	{ "PrintStrayNodes", "PrintOrphanNodes" }, // Node
	{ "PropertyListChangedNotify", "NotifyPropertyListChanged" }, // Object
	{ "Recognize", "_Recognize" }, // ResourceFormatLoader
	{ "RegenNormalmaps", "RegenNormalMaps" }, // ArrayMesh
	{ "RegionBakeNavmesh", "region_bake_navigation_mesh" }, // Navigation3DServer
	{ "RegionSetNavmesh", "RegionSetNavigationMesh" }, // Navigation3DServer
	{ "RegionSetNavpoly", "RegionSetNavigationPolygon" }, // Navigation2DServer
	{ "RemoveAnimation", "RemoveAnimationLibrary" }, // AnimationPlayer
	{ "RemoveColorOverride", "RemoveThemeColorOverride" }, // Control
	{ "RemoveConstantOverride", "RemoveThemeConstantOverride" }, // Control
	{ "RemoveFontOverride", "RemoveThemeFontOverride" }, // Control
	{ "RemoveSceneImportPlugin", "RemoveSceneFormatImporterPlugin" }, //EditorPlugin
	{ "RemoveSpatialGizmoPlugin", "RemoveNode3dGizmoPlugin" }, // EditorPlugin
	{ "RemoveStyleboxOverride", "RemoveThemeStyleboxOverride" }, // Control
	{ "RenameAnimation", "RenameAnimationLibrary" }, // AnimationPlayer
	{ "RenameDependencies", "_RenameDependencies" }, // ResourceFormatLoader
	{ "SaveExternalData", "_SaveExternalData" }, // EditorPlugin
	{ "SegmentIntersectsSegment2d", "SegmentIntersectsSegment" }, // Geometry2D
	{ "SetAdjustmentEnable", "SetAdjustmentEnabled" }, // Environment
	{ "SetAlt", "SetAltPressed" }, // InputEventWithModifiers
	{ "SetAnchorAndMargin", "SetAnchorAndOffset" }, // Control
	{ "SetAnchorsAndMarginsPreset", "SetAnchorsAndOffsetsPreset" }, // Control
	{ "SetAnimationProcessMode", "SetProcessCallback" }, // AnimationPlayer
	{ "SetAsBulkArray", "SetBuffer" }, // MultiMesh
	{ "SetAsNormalmap", "SetAsNormalMap" }, // NoiseTexture
	{ "SetAsToplevel", "SetAsTopLevel" }, // CanvasItem
	{ "SetAudioBus", "SetAudioBusName" }, // Area3D
	{ "SetAutowrap", "SetAutowrapMode" }, // Label -- Breaks AcceptDialog.
	{ "SetCastTo", "SetTargetPosition" }, // RayCast2D, RayCast3D
	{ "SetCollisionLayerBit", "SetCollisionLayerValue" }, // CSGShape3D, and a lot of others like GridMap.
	{ "SetCollisionMaskBit", "SetCollisionMaskValue" }, // CSGShape3D, and a lot of others like GridMap.
	{ "SetColumnMinWidth", "SetColumnCustomMinimumWidth" }, // Tree
	{ "SetCommand", "SetCommandPressed" }, // InputEventWithModifiers
	{ "SetControl", "SetCtrlPressed" }, // InputEventWithModifiers
	{ "SetCreateOptions", "_SetCreateOptions" }, //  EditorResourcePicker
	{ "SetCullMaskBit", "SetCullMaskValue" }, // Camera3D
	{ "SetCursorPosition", "SetCaretColumn" }, // LineEdit
	{ "SetD", "SetDistance" }, // WorldMarginShape2D
	{ "SetDefaultLength", "SetLength" }, // Bone2D
	{ "SetDepthBiasEnable", "SetDepthBiasEnabled" }, // RDPipelineRasterizationState
	{ "SetDevice", "SetOutputDevice" }, // AudioServer
	{ "SetDoubleclick", "SetDoubleClick" }, // InputEventMouseButton
	{ "SetEnableFollowSmoothing", "SetPositionSmoothingEnabled" }, // Camera2D
	{ "SetEnabledFocusMode", "SetFocusMode" }, // BaseButton
	{ "SetEndianSwap", "SetBigEndian" }, // File
	{ "SetExpandToTextLength", "SetExpandToTextLengthEnabled" }, // LineEdit
	{ "SetFocusNeighbour", "SetFocusNeighbor" }, // Control
	{ "SetFollowSmoothing", "SetPositionSmoothingSpeed" }, // Camera2D
	{ "SetFrameColor", "SetColor" }, // ColorRect
	{ "SetGlobalRateScale", "SetPlaybackSpeedScale" }, // AudioServer
	{ "SetGravityDistanceScale", "SetGravityPointDistanceScale" }, // Area2D
	{ "SetGravityVector", "SetGravityDirection" }, // Area2D
	{ "SetHDragEnabled", "SetDragHorizontalEnabled" }, // Camera2D
	{ "SetIconAlign", "SetIconAlignment" }, // Button
	{ "SetInteriorAmbient", "SetAmbientColor" }, // ReflectionProbe
	{ "SetInteriorAmbientEnergy", "SetAmbientColorEnergy" }, // ReflectionProbe
	{ "SetIsInitialized", "_IsInitialized" }, // XRInterface
	{ "SetIsPrimary", "SetPrimary" }, // XRInterface
	{ "SetItemNavmesh", "SetItemNavigationMesh" }, // MeshLibrary
	{ "SetItemNavmeshTransform", "SetItemNavigationMeshTransform" }, // MeshLibrary
	{ "SetIterationsPerSecond", "SetPhysicsTicksPerSecond" }, // Engine
	{ "SetLayerMaskBit", "SetLayerMaskValue" }, // VisualInstance3D
	{ "SetMarginsPreset", "SetOffsetsPreset" }, // Control
	{ "SetMaxAtlasSize", "SetMaxTextureSize" }, // LightmapGI
	{ "SetMetakey", "SetMetaPressed" }, // InputEventWithModifiers
	{ "SetMidHeight", "SetHeight" }, // CapsuleMesh
	{ "SetNeighborDist", "SetNeighborDistance" }, // NavigationAgent2D, NavigationAgent3D
	{ "SetNetworkMaster", "SetMultiplayerAuthority" }, // Node
	{ "SetNetworkPeer", "SetMultiplayerPeer" }, // Multiplayer API
	{ "SetOneshot", "SetOneShot" }, // AnimatedTexture
	{ "SetPhysicalScancode", "SetPhysicalKeycode" }, // InputEventKey
	{ "SetProximityFade", "SetProximityFadeEnabled" }, // Material
	{ "SetRefuseNewNetworkConnections", "SetRefuseNewConnections" }, // Multiplayer API
	{ "SetRegion", "SetRegionEnabled" }, // Sprite2D -- Sprite breaks AtlasTexture.
	{ "SetRegionFilterClip", "SetRegionFilterClipEnabled" }, // Sprite2D
	{ "SetReverbBus", "SetReverbBusName" }, // Area3D
	{ "SetRotate", "SetRotates" }, // PathFollow2D
	{ "SetScancode", "SetKeycode" }, // InputEventKey
	{ "SetShaderParam", "SetShaderParameter" }, // ShaderMaterial
	{ "SetShift", "SetShiftPressed" }, // InputEventWithModifiers
	{ "SetSizeOverride", "SetSize2dOverride" }, // SubViewport -- Breaks ImageTexture.
	{ "SetSizeOverrideStretch", "SetSize2dOverrideStretch" }, // SubViewport
	{ "SetSlipsOnSlope", "SetSlideOnSlope" }, // SeparationRayShape2D, SeparationRayShape3D
	{ "SetSortEnabled", "SetYSortEnabled" }, // Node2D
	{ "SetSpaceOverrideMode", "SetGravitySpaceOverrideMode" }, // Area2D
	{ "SetSpatialNode", "SetNode3d" }, // EditorNode3DGizmo
	{ "SetSpeed", "SetVelocity" }, // InputEventMouseMotion
	{ "SetSsaoEdgeSharpness", "SetSsaoSharpness" }, // Environment
	{ "SetSurfaceMaterial", "SetSurfaceOverrideMaterial" }, // MeshInstance3D -- Breaks ImporterMesh.
	{ "SetTabAlign", "SetTabAlignment" }, // TabContainer
	{ "SetTangent", "SurfaceSetTangent" }, // ImmediateGeometry -- Breaks SurfaceTool.
	{ "SetTargetLocation", "SetTargetPosition" }, // NavigationAgent2D, NavigationAgent3D
	{ "SetTextAlign", "SetTextAlignment" }, // Button
	{ "SetTimerProcessMode", "SetTimerProcessCallback" }, // Timer
	{ "SetTonemapAutoExposure", "SetTonemapAutoExposureEnabled" }, // Environment
	{ "SetTranslation", "SetPosition" }, // Node3D -- This breaks GLTFNode, but it is used rarely.
	{ "SetUniformName", "SetParameterName" }, // ParameterRef
	{ "SetUnitDb", "SetVolumeDb" }, // AudioStreamPlayer3D
	{ "SetUnitOffset", "SetProgressRatio" }, // PathFollow2D, PathFollow3D
	{ "SetUv2", "SurfaceSetUv2" }, // ImmediateMesh -- Breaks SurfaceTool.
	{ "SetVertsPerPoly", "SetVerticesPerPolygon" }, // NavigationMesh
	{ "SetVDragEnabled", "SetDragVerticalEnabled" }, // Camera2D
	{ "SetValign", "SetVerticalAlignment" }, // Label
	{ "SetWindowLayout", "_SetWindowLayout" }, // EditorPlugin
	{ "SetZfar", "SetFar" }, // Camera3D -- Breaks GLTFCamera.
	{ "SetZnear", "SetNear" }, // Camera3D -- Breaks GLTFCamera.
	{ "ShortcutMatch", "IsMatch" }, // InputEvent
	{ "SkeletonAllocate", "SkeletonAllocateData" }, // RenderingServer
	{ "SurfaceUpdateRegion", "SurfaceUpdateAttributeRegion" }, // ArrayMesh
	{ "TrackRemoveKeyAtPosition", "TrackRemoveKeyAtTime" }, // Animation
	{ "TriangulateDelaunay2d", "TriangulateDelaunay" }, // Geometry2D
	{ "UnbindChildNodeFromBone", "RemoveBoneChild" }, // Skeleton3D
	{ "Unselect", "Deselect" }, // ItemList
	{ "UnselectAll", "DeselectAll" }, // ItemList
	{ "UpdateConfigurationWarning", "UpdateConfigurationWarnings" }, // Node
	{ "UpdateGizmo", "UpdateGizmos" }, // Node3D
	{ "ViewportSetUseArvr", "ViewportSetUseXr" }, // RenderingServer
	{ "WarpMousePosition", "WarpMouse" }, // Input
	{ "WorldToMap", "LocalToMap" }, // TileMap, GridMap

	// Builtin types
	{ "Clamped", "LimitLength" }, // Vector2
	{ "GetRotationQuat", "GetRotationQuaternion" }, // Basis
	{ "GrowMargin", "GrowSide" }, // Rect2
	{ "IsAbsPath", "IsAbsolutePath" }, // String
	{ "IsValidInteger", "IsValidInt" }, // String
	{ "LinearInterpolate", "Lerp" }, // Color
	{ "ToAscii", "ToAsciiBuffer" }, // String
	{ "ToUtf8", "ToUtf8Buffer" }, // String

	// @GlobalScope
	{ "Bytes2Var", "BytesToVar" },
	{ "Bytes2VarWithObjects", "BytesToVarWithObjects" },
	{ "Db2Linear", "DbToLinear" },
	{ "Deg2Rad", "DegToRad" },
	{ "Linear2Db", "LinearToDb" },
	{ "Rad2Deg", "RadToDeg" },
	{ "RandRange", "RandfRange" },
	{ "RangeLerp", "Remap" },
	{ "Stepify", "Snapped" },
	{ "Str2Var", "StrToVar" },
	{ "Var2Str", "VarToStr" },
	{ "Var2Bytes", "VarToBytes" },
	{ "Var2BytesWithObjects", "VarToBytesWithObjects" },

	// @GDScript
	{ "Dict2Inst", "DictToInst" },
	{ "Inst2Dict", "InstToDict" },

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::gdscript_properties_renames[][2] = {
	// NOTE: Commented out renames are disabled because deemed not suitable for
	// the current way the regex-based converter works.
	// When uncommenting any of those as suitable for conversion, please move it
	// to the block with other enabled conversions, ordered alphabetically, and
	// make sure to add it to the C# rename map too.

	// Too common words, users may use these names for variables or in comments.
	// { "bg", SceneStringName(panel) }, // Theme
	// { "alt", "alt_pressed" }, // InputEventWithModifiers
	// { "command", "command_pressed" }, // InputEventWithModifiers
	// { "control", "ctrl_pressed" }, // InputEventWithModifiers
	// { "d", "distance" }, // WorldMarginShape2D
	// { "device", "output_device" }, // AudioServer
	// { "doubleclick", "double_click" }, // InputEventMouseButton
	// { "filename", "scene_file_path" }, // Node
	// { "group", "button_group" }, // BaseButton
	// { "meta", "meta_pressed" }, // InputEventWithModifiers
	// { "rotate", "rotates" }, // PathFollow2D
	// { "off", "unchecked" }, // Theme
	// { "ofs", "offset" }, // Theme
	// { "offset", "progress" }, // PathFollow2D, PathFollow3D
	// { "on", "checked" }, // Theme
	// { "shift", "shift_pressed" }, // InputEventWithModifiers
	// { "window_title", "title" }, // Window
	// { "zfar", "far" }, // Camera3D
	// { "znear", "near" }, // Camera3D

	// Would need bespoke solution.
	// { "autowrap", "autowrap_mode" }, // Label -- Changed from bool to enum.
	// { "extents", "size" }, // BoxShape3D, LightmapGI, ReflectionProbe
	// { "frames", "sprite_frames" }, // AnimatedSprite2D, AnimatedSprite3D -- GH-73696
	// { "percent_visible, "show_percentage }, // ProgressBar -- Breaks Label and RichTextLabel.
	// { "pressed", "button_pressed" }, // BaseButton -- Would also rename the signal.
	// { "process_mode", "process_callback" }, // AnimationTree, Camera2D -- conflicts with Node.
	// { "wrap_enabled", "wrap_mode" }, // TextEdit -- Changed from bool to enum.

	{ "as_normalmap", "as_normal_map" }, // NoiseTexture
	{ "bbcode_text", "text" }, // RichTextLabel
	{ "bg_focus", "focus" }, // Theme
	{ "capture_device", "input_device" }, // AudioServer
	{ "caret_blink_speed", "caret_blink_interval" }, // TextEdit, LineEdit
	{ "caret_moving_by_right_click", "caret_move_on_right_click" }, // TextEdit
	{ "caret_position", "caret_column" }, // LineEdit
	{ "cast_to", "target_position" }, // RayCast2D, RayCast3D
	{ "check_vadjust", "check_v_offset" }, // Theme
	{ "close_h_ofs", "close_h_offset" }, // Theme
	{ "close_v_ofs", "close_v_offset" }, // Theme
	{ "commentfocus", "comment_focus" }, // Theme
	{ "contacts_reported", "max_contacts_reported" }, // RigidBody
	{ "depth_bias_enable", "depth_bias_enabled" }, // RDPipelineRasterizationState
	{ "drag_margin_bottom", "drag_bottom_margin" }, // Camera2D
	{ "drag_margin_h_enabled", "drag_horizontal_enabled" }, // Camera2D
	{ "drag_margin_left", "drag_left_margin" }, // Camera2D
	{ "drag_margin_right", "drag_right_margin" }, // Camera2D
	{ "drag_margin_top", "drag_top_margin" }, // Camera2D
	{ "drag_margin_v_enabled", "drag_vertical_enabled" }, // Camera2D
	{ "enabled_focus_mode", "focus_mode" }, // BaseButton - Removed
	{ "extra_spacing_bottom", "spacing_bottom" }, // Font
	{ "extra_spacing_top", "spacing_top" }, // Font
	{ "focus_neighbour_bottom", "focus_neighbor_bottom" }, // Control
	{ "focus_neighbour_left", "focus_neighbor_left" }, // Control
	{ "focus_neighbour_right", "focus_neighbor_right" }, // Control
	{ "focus_neighbour_top", "focus_neighbor_top" }, // Control
	{ "follow_viewport_enable", "follow_viewport_enabled" }, // CanvasItem
	{ "file_icon_modulate", "file_icon_color" }, // Theme
	{ "files_disabled", "file_disabled_color" }, // Theme
	{ "folder_icon_modulate", "folder_icon_color" }, // Theme
	{ "global_rate_scale", "playback_speed_scale" }, // AudioServer
	{ "global_translation", "global_position" }, // Node3D
	{ "gravity_distance_scale", "gravity_point_unit_distance" }, // Area(2D/3D)
	{ "gravity_vec", "gravity_direction" }, // Area(2D/3D)
	{ "hint_tooltip", "tooltip_text" }, // Control
	{ "hseparation", "h_separation" }, // Theme
	{ "icon_align", "icon_alignment" }, // Button
	{ "iterations_per_second", "physics_ticks_per_second" }, // Engine
	{ "invert_enable", "invert_enabled" }, // Polygon2D
	{ "margin_bottom", "offset_bottom" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "margin_left", "offset_left" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "margin_right", "offset_right" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "margin_top", "offset_top" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "mid_height", "height" }, // CapsuleMesh
	{ "navpoly", "navigation_polygon" }, // NavigationRegion2D
	{ "navmesh", "navigation_mesh" }, // NavigationRegion3D
	{ "neighbor_dist", "neighbor_distance" }, // NavigationAgent2D, NavigationAgent3D
	{ "octaves", "fractal_octaves" }, // OpenSimplexNoise -> FastNoiseLite
	{ "offset_h", "drag_horizontal_offset" }, // Camera2D
	{ "offset_v", "drag_vertical_offset" }, // Camera2D
	{ "off_disabled", "unchecked_disabled" }, // Theme
	{ "on_disabled", "checked_disabled" }, // Theme
	{ "oneshot", "one_shot" }, // AnimatedTexture
	{ "out_of_range_mode", "max_polyphony" }, // AudioStreamPlayer3D
	{ "pause_mode", "process_mode" }, // Node
	{ "physical_scancode", "physical_keycode" }, // InputEventKey
	{ "polygon_verts_per_poly", "polygon_vertices_per_polyon" }, // NavigationMesh
	{ "popup_exclusive", "exclusive" }, // Window
	{ "proximity_fade_enable", "proximity_fade_enabled" }, // Material
	{ "rect_position", "position" }, // Control
	{ "rect_global_position", "global_position" }, // Control
	{ "rect_size", "size" }, // Control
	{ "rect_min_size", "custom_minimum_size" }, // Control
	{ "rect_rotation", "rotation" }, // Control
	{ "rect_scale", "scale" }, // Control
	{ "rect_pivot_offset", "pivot_offset" }, // Control
	{ "rect_clip_content", "clip_contents" }, // Control
	{ "refuse_new_network_connections", "refuse_new_connections" }, // MultiplayerAPI
	{ "region_filter_clip", "region_filter_clip_enabled" }, // Sprite2D
	{ "reverb_bus_enable", "reverb_bus_enabled" }, // Area3D
	{ "scancode", "keycode" }, // InputEventKey
	{ "selectedframe", "selected_frame" }, // Theme
	{ "size_override_stretch", "size_2d_override_stretch" }, // SubViewport
	{ "slips_on_slope", "slide_on_slope" }, // SeparationRayShape2D
	{ "smoothing_enabled", "position_smoothing_enabled" }, // Camera2D
	{ "smoothing_speed", "position_smoothing_speed" }, // Camera2D
	{ "ss_reflections_depth_tolerance", "ssr_depth_tolerance" }, // Environment
	{ "ss_reflections_enabled", "ssr_enabled" }, // Environment
	{ "ss_reflections_fade_in", "ssr_fade_in" }, // Environment
	{ "ss_reflections_fade_out", "ssr_fade_out" }, // Environment
	{ "ss_reflections_max_steps", "ssr_max_steps" }, // Environment
	{ "state_machine_selectedframe", "state_machine_selected_frame" }, // Theme
	{ "syntax_highlighting", "syntax_highlighter" }, // TextEdit
	{ "tab_align", "tab_alignment" }, // TabContainer
	{ "table_hseparation", "table_h_separation" }, // Theme
	{ "table_vseparation", "table_v_separation" }, // Theme
	{ "tangent", "orthogonal" }, // Vector2
	{ "target_location", "target_position" }, // NavigationAgent2D, NavigationAgent3D
	{ "toplevel", "top_level" }, // Node
	{ "translation", "position" }, // Node3D
	{ "unit_db", "volume_db" }, // AudioStreamPlayer3D
	{ "unit_offset", "progress_ratio" }, // PathFollow2D, PathFollow3D
	{ "vseparation", "v_separation" }, // Theme

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::csharp_properties_renames[][2] = {
	{ "AsNormalmap", "AsNormalMap" }, // NoiseTexture
	{ "BbcodeText", "Text" }, // RichTextLabel
	{ "BgFocus", "Focus" }, // Theme
	{ "CaptureDevice", "InputDevice" }, // AudioServer
	{ "CaretBlinkSpeed", "CaretBlinkInterval" }, // TextEdit, LineEdit
	{ "CaretMovingByRightClick", "CaretMoveOnRightClick" }, // TextEdit
	{ "CaretPosition", "CaretColumn" }, // LineEdit
	{ "CastTo", "TargetPosition" }, // RayCast2D, RayCast3D
	{ "CheckVadjust", "CheckVAdjust" }, // Theme
	{ "CloseHOfs", "CloseHOffset" }, // Theme
	{ "CloseVOfs", "CloseVOffset" }, // Theme
	{ "Commentfocus", "CommentFocus" }, // Theme
	{ "ContactsReported", "MaxContactsReported" }, // RigidBody
	{ "DepthBiasEnable", "DepthBiasEnabled" }, // RDPipelineRasterizationState
	{ "DragMarginBottom", "DragBottomMargin" }, // Camera2D
	{ "DragMarginHEnabled", "DragHorizontalEnabled" }, // Camera2D
	{ "DragMarginLeft", "DragLeftMargin" }, // Camera2D
	{ "DragMarginRight", "DragRightMargin" }, // Camera2D
	{ "DragMarginTop", "DragTopMargin" }, // Camera2D
	{ "DragMarginVEnabled", "DragVerticalEnabled" }, // Camera2D
	{ "EnabledFocusMode", "FocusMode" }, // BaseButton - Removed
	{ "ExtraSpacingBottom", "SpacingBottom" }, // Font
	{ "ExtraSpacingTop", "SpacingTop" }, // Font
	{ "FocusNeighbourBottom", "FocusNeighborBottom" }, // Control
	{ "FocusNeighbourLeft", "FocusNeighborLeft" }, // Control
	{ "FocusNeighbourRight", "FocusNeighborRight" }, // Control
	{ "FocusNeighbourTop", "FocusNeighborTop" }, // Control
	{ "FollowViewportEnable", "FollowViewportEnabled" }, // CanvasItem
	{ "FileIconModulate", "FileIconColor" }, // Theme
	{ "FilesDisabled", "FileDisabledColor" }, // Theme
	{ "FolderIconModulate", "FolderIconColor" }, // Theme
	{ "GlobalRateScale", "PlaybackSpeedScale" }, // AudioServer
	{ "GravityDistanceScale", "GravityPointDistanceScale" }, // Area2D
	{ "GravityVec", "GravityDirection" }, // Area2D
	{ "HintTooltip", "TooltipText" }, // Control
	{ "Hseparation", "HSeparation" }, // Theme
	{ "IconAlign", "IconAlignment" }, // Button
	{ "IterationsPerSecond", "PhysicsTicksPerSecond" }, // Engine
	{ "InvertEnable", "InvertEnabled" }, // Polygon2D
	{ "MarginBottom", "OffsetBottom" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "MarginLeft", "OffsetLeft" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "MarginRight", "OffsetRight" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "MarginTop", "OffsetTop" }, // Control -- Breaks NinePatchRect, StyleBox.
	{ "MidHeight", "Height" }, // CapsuleMesh
	{ "Navpoly", "NavigationPolygon" }, // NavigationRegion2D
	{ "Navmesh", "NavigationMesh" }, // NavigationRegion3D
	{ "NeighborDist", "NeighborDistance" }, // NavigationAgent2D, NavigationAgent3D
	{ "Octaves", "FractalOctaves" }, // OpenSimplexNoise -> FastNoiseLite
	{ "OffsetH", "DragHorizontalOffset" }, // Camera2D
	{ "OffsetV", "DragVerticalOffset" }, // Camera2D
	{ "OffDisabled", "UncheckedDisabled" }, // Theme
	{ "OnDisabled", "CheckedDisabled" }, // Theme
	{ "Oneshot", "OneShot" }, // AnimatedTexture
	{ "OutOfRangeMode", "MaxPolyphony" }, // AudioStreamPlayer3D
	{ "PauseMode", "ProcessMode" }, // Node
	{ "Perpendicular", "Orthogonal" }, // Vector2 - Only exists in C#
	{ "PhysicalScancode", "PhysicalKeycode" }, // InputEventKey
	{ "PopupExclusive", "Exclusive" }, // Window
	{ "ProximityFadeEnable", "ProximityFadeEnabled" }, // Material
	{ "RectPosition", "Position" }, // Control
	{ "RectGlobalPosition", "GlobalPosition" }, // Control
	{ "RectSize", "Size" }, // Control
	{ "RectMinSize", "CustomMinimumSize" }, // Control
	{ "RectRotation", "Rotation" }, // Control
	{ "RectScale", "Scale" }, // Control
	{ "RectPivotOffset", "PivotOffset" }, // Control
	{ "RectClipContent", "ClipContents" }, // Control
	{ "RefuseNewNetworkConnections", "RefuseNewConnections" }, // MultiplayerAPI
	{ "RegionFilterClip", "RegionFilterClipEnabled" }, // Sprite2D
	{ "ReverbBusEnable", "ReverbBusEnabled" }, // Area3D
	{ "Scancode", "Keycode" }, // InputEventKey
	{ "Selectedframe", "SelectedFrame" }, // Theme
	{ "SizeOverrideStretch", "Size2dOverrideStretch" }, // SubViewport
	{ "SlipsOnSlope", "SlideOnSlope" }, // SeparationRayShape2D
	{ "SmoothingEnabled", "PositionSmoothingEnabled" }, // Camera2D
	{ "SmoothingSpeed", "PositionSmoothingSpeed" }, // Camera2D
	{ "SsReflectionsDepthTolerance", "SsrDepthTolerance" }, // Environment
	{ "SsReflectionsEnabled", "SsrEnabled" }, // Environment
	{ "SsReflectionsFadeIn", "SsrFadeIn" }, // Environment
	{ "SsReflectionsFadeOut", "SsrFadeOut" }, // Environment
	{ "SsReflectionsMaxSteps", "SsrMaxSteps" }, // Environment
	{ "StateMachineSelectedframe", "StateMachineSelectedFrame" }, // Theme
	{ "SyntaxHighlighting", "SyntaxHighlighter" }, // TextEdit
	{ "TabAlign", "TabAlignment" }, // TabContainer
	{ "TableHseparation", "TableHSeparation" }, // Theme
	{ "TableVseparation", "TableVSeparation" }, // Theme
	{ "Tangent", "Orthogonal" }, // Vector2
	{ "TargetLocation", "TargetPosition" }, // NavigationAgent2D, NavigationAgent3D
	{ "Toplevel", "TopLevel" }, // Node
	{ "Translation", "Position" }, // Node3D
	{ "UnitDb", "VolumeDb" }, // AudioStreamPlayer3D
	{ "UnitOffset", "ProgressRatio" }, // PathFollow2D, PathFollow3D
	{ "Vseparation", "VSeparation" }, // Theme

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::gdscript_signals_renames[][2] = {
	// NOTE: Commented out renames are disabled because deemed not suitable for
	// the current way the regex-based converter works.
	// When uncommenting any of those as suitable for conversion, please move it
	// to the block with other enabled conversions, ordered alphabetically, and
	// make sure to add it to the C# rename map too.

	// Too common words, users may use these names for variables or in comments.
	// { "hide", "hidden" }, // CanvasItem
	// { "changed", "settings_changed" }, // EditorSettings

	{ "about_to_show", "about_to_popup" }, // Popup
	{ "button_release", "button_released" }, // XRController3D
	{ "cancelled", "canceled" }, // AcceptDialog
	{ "item_double_clicked", "item_icon_double_clicked" }, // Tree
	{ "network_peer_connected", "peer_connected" }, // MultiplayerAPI
	{ "network_peer_disconnected", "peer_disconnected" }, // MultiplayerAPI
	{ "network_peer_packet", "peer_packet" }, // MultiplayerAPI
	{ "node_unselected", "node_deselected" }, // GraphEdit
	{ "offset_changed", "position_offset_changed" }, // GraphNode
	{ "settings_changed", "changed" }, // TileMap -- Breaks EditorSettings
	{ "skeleton_updated", "pose_updated" }, // Skeleton3D
	{ "tab_close", "tab_closed" }, // TextEdit
	{ "tab_hover", "tab_hovered" }, // TextEdit
	{ "text_entered", "text_submitted" }, // LineEdit

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::csharp_signals_renames[][2] = {
	{ "AboutToShow", "AboutToPopup" }, // Popup
	{ "ButtonRelease", "ButtonReleased" }, // XRController3D
	{ "Cancelled", "Canceled" }, // AcceptDialog
	{ "ItemDoubleClicked", "ItemIconDoubleClicked" }, // Tree
	{ "NetworkPeerConnected", "PeerConnected" }, // MultiplayerAPI
	{ "NetworkPeerDisconnected", "PeerDisconnected" }, // MultiplayerAPI
	{ "NetworkPeerPacket", "PeerPacket" }, // MultiplayerAPI
	{ "NodeUnselected", "NodeDeselected" }, // GraphEdit
	{ "OffsetChanged", "PositionOffsetChanged" }, // GraphNode
	{ "SettingsChanged", "Changed" }, // TileMap -- Breaks EditorSettings
	{ "SkeletonUpdated", "PoseUpdated" }, //
	{ "TabClose", "TabClosed" }, // TextEdit
	{ "TabHover", "TabHovered" }, // TextEdit
	{ "TextEntered", "TextSubmitted" }, // LineEdit

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::project_settings_renames[][2] = {
	// Project setting paths in scripts include the category, but in project.godot,
	// the category is the section delimiter, so we need to support the paths without it.
	// The project.godot remaps are defined in the project_godot_renames, keep them in sync!
	{ "audio/channel_disable_threshold_db", "audio/buses/channel_disable_threshold_db" },
	{ "audio/channel_disable_time", "audio/buses/channel_disable_time" },
	{ "audio/default_bus_layout", "audio/buses/default_bus_layout" },
	{ "audio/driver", "audio/driver/driver" },
	{ "audio/enable_audio_input", "audio/driver/enable_input" },
	{ "audio/mix_rate", "audio/driver/mix_rate" },
	{ "audio/output_latency", "audio/driver/output_latency" },
	{ "audio/output_latency.web", "audio/driver/output_latency.web" },
	{ "audio/video_delay_compensation_ms", "audio/video/video_delay_compensation_ms" },
	{ "display/window/size/width", "display/window/size/viewport_width" },
	{ "display/window/size/height", "display/window/size/viewport_height" },
	{ "display/window/size/test_width", "display/window/size/window_width_override" },
	{ "display/window/size/test_height", "display/window/size/window_height_override" },
	{ "display/window/vsync/use_vsync", "display/window/vsync/vsync_mode" },
	{ "editor/main_run_args", "editor/run/main_run_args" },
	{ "gui/common/swap_ok_cancel", "gui/common/swap_cancel_ok" },
	{ "network/limits/debugger_stdout/max_chars_per_second", "network/limits/debugger/max_chars_per_second" },
	{ "network/limits/debugger_stdout/max_errors_per_second", "network/limits/debugger/max_errors_per_second" },
	{ "network/limits/debugger_stdout/max_messages_per_frame", "network/limits/debugger/max_queued_messages" },
	{ "network/limits/debugger_stdout/max_warnings_per_second", "network/limits/debugger/max_warnings_per_second" },
	{ "network/ssl/certificates", "network/tls/certificate_bundle_override" },
	{ "physics/2d/thread_model", "physics/2d/run_on_thread" }, // TODO: Not sure.
	{ "rendering/environment/default_clear_color", "rendering/environment/defaults/default_clear_color" },
	{ "rendering/environment/default_environment", "rendering/environment/defaults/default_environment" },
	{ "rendering/quality/depth_prepass/disable_for_vendors", "rendering/driver/depth_prepass/disable_for_vendors" },
	{ "rendering/quality/depth_prepass/enable", "rendering/driver/depth_prepass/enable" },
	{ "rendering/quality/shading/force_blinn_over_ggx", "rendering/shading/overrides/force_blinn_over_ggx" },
	{ "rendering/quality/shading/force_blinn_over_ggx.mobile", "rendering/shading/overrides/force_blinn_over_ggx.mobile" },
	{ "rendering/quality/shading/force_lambert_over_burley", "rendering/shading/overrides/force_lambert_over_burley" },
	{ "rendering/quality/shading/force_lambert_over_burley.mobile", "rendering/shading/overrides/force_lambert_over_burley.mobile" },
	{ "rendering/quality/shading/force_vertex_shading", "rendering/shading/overrides/force_vertex_shading" },
	{ "rendering/quality/shadow_atlas/quadrant_0_subdiv", "rendering/lights_and_shadows/shadow_atlas/quadrant_0_subdiv" },
	{ "rendering/quality/shadow_atlas/quadrant_1_subdiv", "rendering/lights_and_shadows/shadow_atlas/quadrant_1_subdiv" },
	{ "rendering/quality/shadow_atlas/quadrant_2_subdiv", "rendering/lights_and_shadows/shadow_atlas/quadrant_2_subdiv" },
	{ "rendering/quality/shadow_atlas/quadrant_3_subdiv", "rendering/lights_and_shadows/shadow_atlas/quadrant_3_subdiv" },
	{ "rendering/quality/shadow_atlas/size", "rendering/lights_and_shadows/shadow_atlas/size" },
	{ "rendering/quality/shadow_atlas/size.mobile", "rendering/lights_and_shadows/shadow_atlas/size.mobile" },
	{ "rendering/vram_compression/import_etc2", "rendering/textures/vram_compression/import_etc2_astc" },
	{ "rendering/vram_compression/import_s3tc", "rendering/textures/vram_compression/import_s3tc_bptc" },

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::project_godot_renames[][2] = {
	// Should be kept in sync with project_settings_renames.
	{ "channel_disable_threshold_db", "buses/channel_disable_threshold_db" },
	{ "channel_disable_time", "buses/channel_disable_time" },
	{ "default_bus_layout", "buses/default_bus_layout" },
	// { "driver", "driver/driver" }, -- Risk of conflicts.
	{ "enable_audio_input", "driver/enable_input" },
	// { "mix_rate", "driver/mix_rate" }, -- Risk of conflicts.
	{ "output_latency", "driver/output_latency" },
	{ "output_latency.web", "driver/output_latency.web" },
	{ "video_delay_compensation_ms", "video/video_delay_compensation_ms" },
	{ "window/size/width", "window/size/viewport_width" },
	{ "window/size/height", "window/size/viewport_height" },
	{ "window/size/test_width", "window/size/window_width_override" },
	{ "window/size/test_height", "window/size/window_height_override" },
	{ "window/vsync/use_vsync", "window/vsync/vsync_mode" },
	{ "main_run_args", "run/main_run_args" },
	{ "common/swap_ok_cancel", "common/swap_cancel_ok" },
	{ "limits/debugger_stdout/max_chars_per_second", "limits/debugger/max_chars_per_second" },
	{ "limits/debugger_stdout/max_errors_per_second", "limits/debugger/max_errors_per_second" },
	{ "limits/debugger_stdout/max_messages_per_frame", "limits/debugger/max_queued_messages" },
	{ "limits/debugger_stdout/max_warnings_per_second", "limits/debugger/max_warnings_per_second" },
	{ "ssl/certificates", "tls/certificate_bundle_override" },
	{ "2d/thread_model", "2d/run_on_thread" }, // TODO: Not sure.
	{ "environment/default_clear_color", "environment/defaults/default_clear_color" },
	{ "environment/default_environment", "environment/defaults/default_environment" },
	{ "quality/depth_prepass/disable_for_vendors", "driver/depth_prepass/disable_for_vendors" },
	{ "quality/depth_prepass/enable", "driver/depth_prepass/enable" },
	{ "quality/shading/force_blinn_over_ggx", "shading/overrides/force_blinn_over_ggx" },
	{ "quality/shading/force_blinn_over_ggx.mobile", "shading/overrides/force_blinn_over_ggx.mobile" },
	{ "quality/shading/force_lambert_over_burley", "shading/overrides/force_lambert_over_burley" },
	{ "quality/shading/force_lambert_over_burley.mobile", "shading/overrides/force_lambert_over_burley.mobile" },
	{ "quality/shading/force_vertex_shading", "shading/overrides/force_vertex_shading" },
	{ "quality/shadow_atlas/quadrant_0_subdiv", "lights_and_shadows/shadow_atlas/quadrant_0_subdiv" },
	{ "quality/shadow_atlas/quadrant_1_subdiv", "lights_and_shadows/shadow_atlas/quadrant_1_subdiv" },
	{ "quality/shadow_atlas/quadrant_2_subdiv", "lights_and_shadows/shadow_atlas/quadrant_2_subdiv" },
	{ "quality/shadow_atlas/quadrant_3_subdiv", "lights_and_shadows/shadow_atlas/quadrant_3_subdiv" },
	{ "quality/shadow_atlas/size", "lights_and_shadows/shadow_atlas/size" },
	{ "quality/shadow_atlas/size.mobile", "lights_and_shadows/shadow_atlas/size.mobile" },
	{ "vram_compression/import_etc2", "textures/vram_compression/import_etc2_astc" },
	{ "vram_compression/import_s3tc", "textures/vram_compression/import_s3tc_bptc" },

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::input_map_renames[][2] = {
	{ ",\"alt\":", ",\"alt_pressed\":" },
	{ ",\"shift\":", ",\"shift_pressed\":" },
	{ ",\"control\":", ",\"ctrl_pressed\":" },
	{ ",\"meta\":", ",\"meta_pressed\":" },
	{ ",\"scancode\":", ",\"keycode\":" },
	{ ",\"physical_scancode\":", ",\"physical_keycode\":" },
	{ ",\"doubleclick\":", ",\"double_click\":" },

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::builtin_types_renames[][2] = {
	{ "PoolByteArray", "PackedByteArray" },
	{ "PoolColorArray", "PackedColorArray" },
	{ "PoolIntArray", "PackedInt32Array" },
	{ "PoolRealArray", "PackedFloat32Array" },
	{ "PoolStringArray", "PackedStringArray" },
	{ "PoolVector2Array", "PackedVector2Array" },
	{ "PoolVector3Array", "PackedVector3Array" },
	{ "Quat", "Quaternion" },
	{ "Transform", "Transform3D" },

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::shaders_renames[][2] = {
	{ "ALPHA_SCISSOR", "ALPHA_SCISSOR_THRESHOLD" },
	{ "CAMERA_MATRIX", "INV_VIEW_MATRIX" },
	{ "INV_CAMERA_MATRIX", "VIEW_MATRIX" },
	{ "NORMALMAP", "NORMAL_MAP" },
	{ "NORMALMAP_DEPTH", "NORMAL_MAP_DEPTH" },
	{ "TRANSMISSION", "BACKLIGHT" },
	{ "WORLD_MATRIX", "MODEL_MATRIX" },
	{ "depth_draw_alpha_prepass", "depth_prepass_alpha" },
	{ "hint_albedo", "source_color" },
	{ "hint_aniso", "hint_anisotropy" },
	{ "hint_black", "hint_default_black" },
	{ "hint_black_albedo", "hint_default_black" },
	{ "hint_color", "source_color" },
	{ "hint_white", "hint_default_white" },
	{ nullptr, nullptr },
};

const char *RenamesMap3To4::class_renames[][2] = {
	// { "Particles", "GPUParticles3D" }, // Common word, and incompatible class.
	// { "World", "World3D" }, // Too common.

	// Risky as fairly common words, but worth it given how ubiquitous they are.
	{ "Area", "Area3D" },
	{ "Camera", "Camera3D" },
	{ "Path", "Path3D" },
	{ "Reference", "RefCounted" },
	{ "Shape", "Shape3D" },
	{ "Tabs", "TabBar" },

	{ "ARVRAnchor", "XRAnchor3D" },
	{ "ARVRCamera", "XRCamera3D" },
	{ "ARVRController", "XRController3D" },
	{ "ARVRInterface", "XRInterface" },
	{ "ARVRInterfaceGDNative", "Node3D" },
	{ "ARVROrigin", "XROrigin3D" },
	{ "ARVRPositionalTracker", "XRPositionalTracker" },
	{ "ARVRServer", "XRServer" },
	{ "AStar", "AStar3D" },
	{ "AnimatedSprite", "AnimatedSprite2D" },
	{ "AudioStreamOGGVorbis", "AudioStreamOggVorbis" },
	{ "AudioStreamRandomPitch", "AudioStreamRandomizer" },
	{ "AudioStreamSample", "AudioStreamWAV" },
	{ "BakedLightmap", "LightmapGI" },
	{ "BakedLightmapData", "LightmapGIData" },
	{ "BitmapFont", "FontFile" },
	{ "BoneAttachment", "BoneAttachment3D" },
	{ "BoxShape", "BoxShape3D" },
	{ "CPUParticles", "CPUParticles3D" },
	{ "CSGBox", "CSGBox3D" },
	{ "CSGCombiner", "CSGCombiner3D" },
	{ "CSGCylinder", "CSGCylinder3D" },
	{ "CSGMesh", "CSGMesh3D" },
	{ "CSGPolygon", "CSGPolygon3D" },
	{ "CSGPrimitive", "CSGPrimitive3D" },
	{ "CSGShape", "CSGShape3D" },
	{ "CSGSphere", "CSGSphere3D" },
	{ "CSGTorus", "CSGTorus3D" },
	{ "CapsuleShape", "CapsuleShape3D" },
	{ "ClippedCamera", "Camera3D" },
	{ "CollisionObject", "CollisionObject3D" },
	{ "CollisionPolygon", "CollisionPolygon3D" },
	{ "CollisionShape", "CollisionShape3D" },
	{ "ConcavePolygonShape", "ConcavePolygonShape3D" },
	{ "ConeTwistJoint", "ConeTwistJoint3D" },
	{ "ConvexPolygonShape", "ConvexPolygonShape3D" },
	{ "CubeMap", "Cubemap" },
	{ "CubeMesh", "BoxMesh" },
	{ "CylinderShape", "CylinderShape3D" },
	{ "DirectionalLight", "DirectionalLight3D" },
	{ "Directory", "DirAccess" },
	{ "DynamicFont", "FontFile" },
	{ "DynamicFontData", "FontFile" },
	{ "EditorNavigationMeshGenerator", "NavigationMeshGenerator" },
	{ "EditorSceneImporter", "EditorSceneFormatImporter" },
	{ "EditorSceneImporterFBX", "EditorSceneFormatImporterFBX2GLTF" },
	{ "EditorSceneImporterGLTF", "EditorSceneFormatImporterGLTF" },
	{ "EditorSpatialGizmo", "EditorNode3DGizmo" },
	{ "EditorSpatialGizmoPlugin", "EditorNode3DGizmoPlugin" },
	{ "GIProbe", "VoxelGI" },
	{ "GIProbeData", "VoxelGIData" },
	{ "Generic6DOFJoint", "Generic6DOFJoint3D" },
	{ "GeometryInstance", "GeometryInstance3D" },
	{ "GradientTexture", "GradientTexture2D" },
	{ "HeightMapShape", "HeightMapShape3D" },
	{ "HingeJoint", "HingeJoint3D" },
	{ "IP_Unix", "IPUnix" },
	{ "ImmediateGeometry", "ImmediateMesh" },
	{ "ImmediateGeometry3D", "ImmediateMesh" },
	{ "InterpolatedCamera", "Camera3D" },
	{ "InterpolatedCamera3D", "Camera3D" },
	{ "JSONParseResult", "JSON" },
	{ "Joint", "Joint3D" },
	{ "KinematicBody", "CharacterBody3D" },
	{ "KinematicBody2D", "CharacterBody2D" },
	{ "KinematicCollision", "KinematicCollision3D" },
	{ "LargeTexture", "ImageTexture" },
	{ "Light", "Light3D" },
	{ "Light2D", "PointLight2D" },
	{ "LineShape2D", "WorldBoundaryShape2D" },
	{ "Listener", "AudioListener3D" },
	{ "Listener2D", "AudioListener2D" },
	{ "MeshInstance", "MeshInstance3D" },
	{ "MultiMeshInstance", "MultiMeshInstance3D" },
	{ "MultiplayerPeerGDNative", "MultiplayerPeerExtension" },
	{ "Navigation2DServer", "NavigationServer2D" },
	{ "NavigationAgent", "NavigationAgent3D" },
	{ "NavigationMeshInstance", "NavigationRegion3D" },
	{ "NavigationObstacle", "NavigationObstacle3D" },
	{ "NavigationPolygonInstance", "NavigationRegion2D" },
	{ "NavigationRegion", "NavigationRegion3D" },
	{ "NavigationServer", "NavigationServer3D" },
	{ "NetworkedMultiplayerCustom", "MultiplayerPeerExtension" },
	{ "NetworkedMultiplayerENet", "ENetMultiplayerPeer" },
	{ "NetworkedMultiplayerPeer", "MultiplayerPeer" },
	{ "Occluder", "OccluderInstance3D" },
	{ "OmniLight", "OmniLight3D" },
	{ "OpenSimplexNoise", "FastNoiseLite" },
	{ "PHashTranslation", "OptimizedTranslation" },
	{ "PacketPeerGDNative", "PacketPeerExtension" },
	{ "PanoramaSky", "Sky" },
	{ "Particles2D", "GPUParticles2D" },
	{ "ParticlesMaterial", "ParticleProcessMaterial" },
	{ "PathFollow", "PathFollow3D" },
	{ "PhysicalBone", "PhysicalBone3D" },
	{ "Physics2DDirectBodyState", "PhysicsDirectBodyState2D" },
	{ "Physics2DDirectSpaceState", "PhysicsDirectSpaceState2D" },
	{ "Physics2DServer", "PhysicsServer2D" },
	{ "Physics2DServerSW", "GodotPhysicsServer2D" },
	{ "Physics2DShapeQueryParameters", "PhysicsShapeQueryParameters2D" },
	{ "Physics2DTestMotionResult", "PhysicsTestMotionResult2D" },
	{ "PhysicsBody", "PhysicsBody3D" },
	{ "PhysicsDirectBodyState", "PhysicsDirectBodyState3D" },
	{ "PhysicsDirectSpaceState", "PhysicsDirectSpaceState3D" },
	{ "PhysicsServer", "PhysicsServer3D" },
	{ "PhysicsShapeQueryParameters", "PhysicsShapeQueryParameters3D" },
	{ "PhysicsTestMotionResult", "PhysicsTestMotionResult3D" },
	{ "PinJoint", "PinJoint3D" },
	{ "PlaneShape", "WorldBoundaryShape3D" },
	{ "PopupDialog", "Popup" },
	{ "Position2D", "Marker2D" },
	{ "Position3D", "Marker3D" },
	{ "ProceduralSky", "Sky" },
	{ "RayCast", "RayCast3D" },
	{ "RayShape", "SeparationRayShape3D" },
	{ "RayShape2D", "SeparationRayShape2D" },
	{ "RemoteTransform", "RemoteTransform3D" },
	{ "ResourceInteractiveLoader", "ResourceLoader" },
	{ "RigidBody", "RigidBody3D" },
	{ "SceneTreeTween", "Tween" },
	{ "ShortCut", "Shortcut" },
	{ "Skeleton", "Skeleton3D" },
	{ "SkeletonIK", "SkeletonIK3D" },
	{ "SliderJoint", "SliderJoint3D" },
	{ "SoftBody", "SoftBody3D" },
	{ "Spatial", "Node3D" },
	{ "SpatialGizmo", "Node3DGizmo" },
	{ "SpatialMaterial", "StandardMaterial3D" },
	{ "SphereShape", "SphereShape3D" },
	{ "SpotLight", "SpotLight3D" },
	{ "SpringArm", "SpringArm3D" },
	{ "Sprite", "Sprite2D" },
	{ "StaticBody", "StaticBody3D" },
	{ "StreamCubemap", "CompressedCubemap" },
	{ "StreamCubemapArray", "CompressedCubemapArray" },
	{ "StreamPeerGDNative", "StreamPeerExtension" },
	{ "StreamPeerSSL", "StreamPeerTLS" },
	{ "StreamTexture", "CompressedTexture2D" },
	{ "StreamTexture2D", "CompressedTexture2D" },
	{ "StreamTexture2DArray", "CompressedTexture2DArray" },
	{ "StreamTextureLayered", "CompressedTextureLayered" },
	{ "TCP_Server", "TCPServer" },
	{ "TextFile", "Node3D" },
	{ "Texture", "Texture2D" }, // May break TextureRect.
	{ "TextureArray", "Texture2DArray" },
	{ "TextureProgress", "TextureProgressBar" },
	{ "ToolButton", "Button" },
	{ "VehicleBody", "VehicleBody3D" },
	{ "VehicleWheel", "VehicleWheel3D" },
	{ "VideoPlayer", "VideoStreamPlayer" },
	{ "Viewport", "SubViewport" },
	{ "ViewportContainer", "SubViewportContainer" },
	{ "VisibilityEnabler", "VisibleOnScreenEnabler3D" },
	{ "VisibilityEnabler2D", "VisibleOnScreenEnabler2D" },
	{ "VisibilityNotifier", "VisibleOnScreenNotifier3D" },
	{ "VisibilityNotifier2D", "VisibleOnScreenNotifier2D" },
	{ "VisibilityNotifier3D", "VisibleOnScreenNotifier3D" },
	{ "VisualInstance", "VisualInstance3D" },
	{ "VisualServer", "RenderingServer" },
	{ "VisualShaderNodeCubeMap", "VisualShaderNodeCubemap" },
	{ "VisualShaderNodeScalarClamp", "VisualShaderNodeClamp" },
	{ "VisualShaderNodeScalarConstant", "VisualShaderNodeFloatConstant" },
	{ "VisualShaderNodeScalarFunc", "VisualShaderNodeFloatFunc" },
	{ "VisualShaderNodeScalarInterp", "VisualShaderNodeMix" },
	{ "VisualShaderNodeScalarOp", "VisualShaderNodeFloatOp" },
	{ "VisualShaderNodeScalarSmoothStep", "VisualShaderNodeSmoothStep" },
	{ "VisualShaderNodeScalarSwitch", "VisualShaderNodeSwitch" },
	{ "VisualShaderNodeScalarTransformMult", "VisualShaderNodeTransformOp" },
	{ "VisualShaderNodeTransformMult", "VisualShaderNode" },
	{ "VisualShaderNodeVectorClamp", "VisualShaderNodeClamp" },
	{ "VisualShaderNodeVectorInterp", "VisualShaderNodeMix" },
	{ "VisualShaderNodeVectorScalarMix", "VisualShaderNodeMix" },
	{ "VisualShaderNodeVectorScalarSmoothStep", "VisualShaderNodeSmoothStep" },
	{ "VisualShaderNodeVectorScalarStep", "VisualShaderNodeStep" },
	{ "VisualShaderNodeVectorSmoothStep", "VisualShaderNodeSmoothStep" },
	{ "VisualShaderNodeBooleanUniform", "VisualShaderNodeBooleanParameter" },
	{ "VisualShaderNodeColorUniform", "VisualShaderNodeColorParameter" },
	{ "VisualShaderNodeScalarUniform", "VisualShaderNodeFloatParameter" },
	{ "VisualShaderNodeCubemapUniform", "VisualShaderNodeCubemapParameter" },
	{ "VisualShaderNodeTextureUniform", "VisualShaderNodeTexture2DParameter" },
	{ "VisualShaderNodeTextureUniformTriplanar", "VisualShaderNodeTextureParameterTriplanar" },
	{ "VisualShaderNodeTransformUniform", "VisualShaderNodeTransformParameter" },
	{ "VisualShaderNodeVec3Uniform", "VisualShaderNodeVec3Parameter" },
	{ "VisualShaderNodeUniform", "VisualShaderNodeParameter" },
	{ "VisualShaderNodeUniformRef", "VisualShaderNodeParameterRef" },
	{ "WebRTCDataChannelGDNative", "WebRTCDataChannelExtension" },
	{ "WebRTCMultiplayer", "WebRTCMultiplayerPeer" },
	{ "WebRTCPeerConnectionGDNative", "WebRTCPeerConnectionExtension" },
	{ "WindowDialog", "Window" },
	{ "XRAnchor", "XRAnchor3D" },
	{ "XRController", "XRController3D" },
	{ "XROrigin", "XROrigin3D" },
	{ "YSort", "Node2D" }, // CanvasItem has a new "y_sort_enabled" property.

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::color_renames[][2] = {
	{ "aliceblue", "ALICE_BLUE" },
	{ "antiquewhite", "ANTIQUE_WHITE" },
	{ "aqua", "AQUA" },
	{ "aquamarine", "AQUAMARINE" },
	{ "azure", "AZURE" },
	{ "beige", "BEIGE" },
	{ "bisque", "BISQUE" },
	{ "black", "BLACK" },
	{ "blanchedalmond", "BLANCHED_ALMOND" },
	{ "blue", "BLUE" },
	{ "blueviolet", "BLUE_VIOLET" },
	{ "brown", "BROWN" },
	{ "burlywood", "BURLYWOOD" },
	{ "cadetblue", "CADET_BLUE" },
	{ "chartreuse", "CHARTREUSE" },
	{ "chocolate", "CHOCOLATE" },
	{ "coral", "CORAL" },
	{ "cornflowerblue", "CORNFLOWER_BLUE" },
	{ "cornsilk", "CORNSILK" },
	{ "crimson", "CRIMSON" },
	{ "cyan", "CYAN" },
	{ "darkblue", "DARK_BLUE" },
	{ "darkcyan", "DARK_CYAN" },
	{ "darkgoldenrod", "DARK_GOLDENROD" },
	{ "darkgray", "DARK_GRAY" },
	{ "darkgreen", "DARK_GREEN" },
	{ "darkkhaki", "DARK_KHAKI" },
	{ "darkmagenta", "DARK_MAGENTA" },
	{ "darkolivegreen", "DARK_OLIVE_GREEN" },
	{ "darkorange", "DARK_ORANGE" },
	{ "darkorchid", "DARK_ORCHID" },
	{ "darkred", "DARK_RED" },
	{ "darksalmon", "DARK_SALMON" },
	{ "darkseagreen", "DARK_SEA_GREEN" },
	{ "darkslateblue", "DARK_SLATE_BLUE" },
	{ "darkslategray", "DARK_SLATE_GRAY" },
	{ "darkturquoise", "DARK_TURQUOISE" },
	{ "darkviolet", "DARK_VIOLET" },
	{ "deeppink", "DEEP_PINK" },
	{ "deepskyblue", "DEEP_SKY_BLUE" },
	{ "dimgray", "DIM_GRAY" },
	{ "dodgerblue", "DODGER_BLUE" },
	{ "firebrick", "FIREBRICK" },
	{ "floralwhite", "FLORAL_WHITE" },
	{ "forestgreen", "FOREST_GREEN" },
	{ "fuchsia", "FUCHSIA" },
	{ "gainsboro", "GAINSBORO" },
	{ "ghostwhite", "GHOST_WHITE" },
	{ "gold", "GOLD" },
	{ "goldenrod", "GOLDENROD" },
	{ "gray", "GRAY" },
	{ "green", "GREEN" },
	{ "greenyellow", "GREEN_YELLOW" },
	{ "honeydew", "HONEYDEW" },
	{ "hotpink", "HOT_PINK" },
	{ "indianred", "INDIAN_RED" },
	{ "indigo", "INDIGO" },
	{ "ivory", "IVORY" },
	{ "khaki", "KHAKI" },
	{ "lavender", "LAVENDER" },
	{ "lavenderblush", "LAVENDER_BLUSH" },
	{ "lawngreen", "LAWN_GREEN" },
	{ "lemonchiffon", "LEMON_CHIFFON" },
	{ "lightblue", "LIGHT_BLUE" },
	{ "lightcoral", "LIGHT_CORAL" },
	{ "lightcyan", "LIGHT_CYAN" },
	{ "lightgoldenrod", "LIGHT_GOLDENROD" },
	{ "lightgray", "LIGHT_GRAY" },
	{ "lightgreen", "LIGHT_GREEN" },
	{ "lightpink", "LIGHT_PINK" },
	{ "lightsalmon", "LIGHT_SALMON" },
	{ "lightseagreen", "LIGHT_SEA_GREEN" },
	{ "lightskyblue", "LIGHT_SKY_BLUE" },
	{ "lightslategray", "LIGHT_SLATE_GRAY" },
	{ "lightsteelblue", "LIGHT_STEEL_BLUE" },
	{ "lightyellow", "LIGHT_YELLOW" },
	{ "lime", "LIME" },
	{ "limegreen", "LIME_GREEN" },
	{ "linen", "LINEN" },
	{ "magenta", "MAGENTA" },
	{ "maroon", "MAROON" },
	{ "mediumaquamarine", "MEDIUM_AQUAMARINE" },
	{ "mediumblue", "MEDIUM_BLUE" },
	{ "mediumorchid", "MEDIUM_ORCHID" },
	{ "mediumpurple", "MEDIUM_PURPLE" },
	{ "mediumseagreen", "MEDIUM_SEA_GREEN" },
	{ "mediumslateblue", "MEDIUM_SLATE_BLUE" },
	{ "mediumspringgreen", "MEDIUM_SPRING_GREEN" },
	{ "mediumturquoise", "MEDIUM_TURQUOISE" },
	{ "mediumvioletred", "MEDIUM_VIOLET_RED" },
	{ "midnightblue", "MIDNIGHT_BLUE" },
	{ "mintcream", "MINT_CREAM" },
	{ "mistyrose", "MISTY_ROSE" },
	{ "moccasin", "MOCCASIN" },
	{ "navajowhite", "NAVAJO_WHITE" },
	{ "navyblue", "NAVY_BLUE" },
	{ "oldlace", "OLD_LACE" },
	{ "olive", "OLIVE" },
	{ "olivedrab", "OLIVE_DRAB" },
	{ "orange", "ORANGE" },
	{ "orangered", "ORANGE_RED" },
	{ "orchid", "ORCHID" },
	{ "palegoldenrod", "PALE_GOLDENROD" },
	{ "palegreen", "PALE_GREEN" },
	{ "paleturquoise", "PALE_TURQUOISE" },
	{ "palevioletred", "PALE_VIOLET_RED" },
	{ "papayawhip", "PAPAYA_WHIP" },
	{ "peachpuff", "PEACH_PUFF" },
	{ "peru", "PERU" },
	{ "pink", "PINK" },
	{ "plum", "PLUM" },
	{ "powderblue", "POWDER_BLUE" },
	{ "purple", "PURPLE" },
	{ "rebeccapurple", "REBECCA_PURPLE" },
	{ "red", "RED" },
	{ "rosybrown", "ROSY_BROWN" },
	{ "royalblue", "ROYAL_BLUE" },
	{ "saddlebrown", "SADDLE_BROWN" },
	{ "salmon", "SALMON" },
	{ "sandybrown", "SANDY_BROWN" },
	{ "seagreen", "SEA_GREEN" },
	{ "seashell", "SEASHELL" },
	{ "sienna", "SIENNA" },
	{ "silver", "SILVER" },
	{ "skyblue", "SKY_BLUE" },
	{ "slateblue", "SLATE_BLUE" },
	{ "slategray", "SLATE_GRAY" },
	{ "snow", "SNOW" },
	{ "springgreen", "SPRING_GREEN" },
	{ "steelblue", "STEEL_BLUE" },
	{ "tan", "TAN" },
	{ "teal", "TEAL" },
	{ "thistle", "THISTLE" },
	{ "tomato", "TOMATO" },
	{ "transparent", "TRANSPARENT" },
	{ "turquoise", "TURQUOISE" },
	{ "violet", "VIOLET" },
	{ "webgray", "WEB_GRAY" },
	{ "webgreen", "WEB_GREEN" },
	{ "webmaroon", "WEB_MAROON" },
	{ "webpurple", "WEB_PURPLE" },
	{ "wheat", "WHEAT" },
	{ "white", "WHITE" },
	{ "whitesmoke", "WHITE_SMOKE" },
	{ "yellow", "YELLOW" },
	{ "yellowgreen", "YELLOW_GREEN" },

	{ nullptr, nullptr },
};

const char *RenamesMap3To4::theme_override_renames[][2] = {
	// First rename the generic prefixes.
	{ "custom_colors/", "theme_override_colors/" },
	{ "custom_constants/", "theme_override_constants/" },
	{ "custom_fonts/", "theme_override_fonts/" },
	{ "custom_icons/", "theme_override_icons/" },
	{ "custom_styles/", "theme_override_styles/" },

	// MarginContainer
	// The margin_* properties are renamed to offset_* in a previous conversion step.
	// This is fine everywhere except for the MarginContainer theme_override_constants.
	{ "theme_override_constants/offset_right", "theme_override_constants/margin_right" },
	{ "theme_override_constants/offset_top", "theme_override_constants/margin_top" },
	{ "theme_override_constants/offset_left", "theme_override_constants/margin_left" },
	{ "theme_override_constants/offset_bottom", "theme_override_constants/margin_bottom" },

	// Panel/PanelContainer/TabContainer/PopupPanel/PopupMenu
	{ "theme_override_styles/panel", "theme_override_styles/panel" },

	// TabContainer/Tabs(TabBar)
	{ "theme_override_styles/tab_bg", "theme_override_styles/tab_unselected" },
	{ "theme_override_styles/tab_fg", "theme_override_styles/tab_selected" },

	// { "theme_override_styles/bg", "theme_override_styles/bg" }, // GraphEdit
	// { "theme_override_styles/bg", "theme_override_styles/panel" }, // ScrollContainer
	// { "theme_override_styles/bg", "theme_override_styles/background" }, // ProgressBar
	// { "theme_override_styles/fg", "theme_override_styles/fill" }, // ProgressBar

	{ "theme_override_colors/font_color_hover", "theme_override_colors/font_hover_color" },
	{ "theme_override_colors/font_color_pressed", "theme_override_colors/font_pressed_color" },
	{ "theme_override_colors/font_color_disabled", "theme_override_colors/font_disabled_color" },
	{ "theme_override_colors/font_color_focus", "theme_override_colors/font_focus_color" },
	{ "theme_override_colors/font_color_hover_pressed", "theme_override_colors/font_hover_pressed_color" },

	{ "theme_override_colors/font_outline_modulate", "theme_override_colors/font_outline_color" },
	{ "theme_override_colors/font_color_shadow", "theme_override_colors/font_shadow_color" },

	{ "theme_override_constants/shadow_as_outline", "theme_override_constants/shadow_outline_size" }, // 0 or 1

	{ "theme_override_constants/table_vseparation", "theme_override_constants/table_v_separation" },
	{ "theme_override_constants/table_hseparation", "theme_override_constants/table_h_separation" },

	{ nullptr, nullptr },
};

#endif // DISABLE_DEPRECATED
