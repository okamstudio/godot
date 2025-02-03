/**************************************************************************/
/*  register_scene_types.cpp                                              */
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

#include "register_scene_types.h"

#include "core/class_db.h"
#include "core/os/os.h"
#include "core/project_settings.h"
#include "scene/2d/animated_sprite.h"
#include "scene/2d/area_2d.h"
#include "scene/2d/audio_stream_player_2d.h"
#include "scene/2d/back_buffer_copy.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/canvas_item.h"
#include "scene/2d/canvas_modulate.h"
#include "scene/2d/collision_polygon_2d.h"
#include "scene/2d/collision_shape_2d.h"
#include "scene/2d/cpu_particles_2d.h"
#include "scene/2d/joints_2d.h"
#include "scene/2d/light_2d.h"
#include "scene/2d/light_occluder_2d.h"
#include "scene/2d/line_2d.h"
#include "scene/2d/listener_2d.h"
#include "scene/2d/mesh_instance_2d.h"
#include "scene/2d/multimesh_instance_2d.h"
#include "scene/2d/navigation_2d.h"
#include "scene/2d/navigation_agent_2d.h"
#include "scene/2d/navigation_obstacle_2d.h"
#include "scene/2d/parallax_background.h"
#include "scene/2d/parallax_layer.h"
#include "scene/2d/particles_2d.h"
#include "scene/2d/path_2d.h"
#include "scene/2d/physics_body_2d.h"
#include "scene/2d/polygon_2d.h"
#include "scene/2d/position_2d.h"
#include "scene/2d/ray_cast_2d.h"
#include "scene/2d/remote_transform_2d.h"
#include "scene/2d/shape_cast_2d.h"
#include "scene/2d/skeleton_2d.h"
#include "scene/2d/sprite.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/touch_screen_button.h"
#include "scene/2d/visibility_notifier_2d.h"
#include "scene/2d/y_sort.h"
#include "scene/3d/spatial.h"
#include "scene/3d/world_environment.h"
#include "scene/animation/animation_blend_space_1d.h"
#include "scene/animation/animation_blend_space_2d.h"
#include "scene/animation/animation_blend_tree.h"
#include "scene/animation/animation_node_state_machine.h"
#include "scene/animation/animation_player.h"
#include "scene/animation/animation_tree.h"
#include "scene/animation/animation_tree_player.h"
#include "scene/animation/root_motion_view.h"
#include "scene/animation/scene_tree_tween.h"
#include "scene/animation/tween.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/gui/aspect_ratio_container.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/center_container.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/graph_edit.h"
#include "scene/gui/graph_node.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/link_button.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/nine_patch_rect.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/reference_rect.h"
#include "scene/gui/rich_text_effect.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/separator.h"
#include "scene/gui/slider.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/tabs.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_progress.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/tool_button.h"
#include "scene/gui/tree.h"
#include "scene/gui/video_player.h"
#include "scene/gui/viewport_container.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/http_request.h"
#include "scene/main/instance_placeholder.h"
#include "scene/main/resource_preloader.h"
#include "scene/main/scene_tree.h"
#include "scene/main/timer.h"
#include "scene/main/viewport.h"
#include "scene/resources/audio_stream_sample.h"
#include "scene/resources/bit_map.h"
#include "scene/resources/box_shape.h"
#include "scene/resources/capsule_shape.h"
#include "scene/resources/capsule_shape_2d.h"
#include "scene/resources/circle_shape_2d.h"
#include "scene/resources/concave_polygon_shape.h"
#include "scene/resources/concave_polygon_shape_2d.h"
#include "scene/resources/convex_polygon_shape.h"
#include "scene/resources/convex_polygon_shape_2d.h"
#include "scene/resources/cylinder_shape.h"
#include "scene/resources/default_theme/default_theme.h"
#include "scene/resources/dynamic_font.h"
#include "scene/resources/gradient.h"
#include "scene/resources/height_map_shape.h"
#include "scene/resources/line_shape_2d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/mesh_data_tool.h"
#include "scene/resources/navigation_mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/particles_material.h"
#include "scene/resources/physics_material.h"
#include "scene/resources/plane_shape.h"
#include "scene/resources/polygon_path_finder.h"
#include "scene/resources/primitive_meshes.h"
#include "scene/resources/ray_shape.h"
#include "scene/resources/rectangle_shape_2d.h"
#include "scene/resources/resource_format_text.h"
#include "scene/resources/segment_shape_2d.h"
#include "scene/resources/sky.h"
#include "scene/resources/sphere_shape.h"
#include "scene/resources/surface_tool.h"
#include "scene/resources/text_file.h"
#include "scene/resources/texture.h"
#include "scene/resources/tile_set.h"
#include "scene/resources/video_stream.h"
#include "scene/resources/visual_shader.h"
#include "scene/resources/visual_shader_nodes.h"
#include "scene/resources/world.h"
#include "scene/resources/world_2d.h"
#include "scene/scene_string_names.h"

#ifndef _3D_DISABLED
#include "scene/3d/area.h"
#include "scene/3d/arvr_nodes.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/baked_lightmap.h"
#include "scene/3d/bone_attachment.h"
#include "scene/3d/camera.h"
#include "scene/3d/collision_polygon.h"
#include "scene/3d/collision_shape.h"
#include "scene/3d/cpu_particles.h"
#include "scene/3d/gi_probe.h"
#include "scene/3d/immediate_geometry.h"
#include "scene/3d/interpolated_camera.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/light.h"
#include "scene/3d/listener.h"
#include "scene/3d/lod.h"
#include "scene/3d/merge_group.h"
#include "scene/3d/mesh_instance.h"
#include "scene/3d/multimesh_instance.h"
#include "scene/3d/navigation.h"
#include "scene/3d/navigation_agent.h"
#include "scene/3d/navigation_mesh_instance.h"
#include "scene/3d/navigation_obstacle.h"
#include "scene/3d/occluder.h"
#include "scene/3d/particles.h"
#include "scene/3d/path.h"
#include "scene/3d/physics_body.h"
#include "scene/3d/physics_joint.h"
#include "scene/3d/portal.h"
#include "scene/3d/position_3d.h"
#include "scene/3d/proximity_group.h"
#include "scene/3d/ray_cast.h"
#include "scene/3d/reflection_probe.h"
#include "scene/3d/remote_transform.h"
#include "scene/3d/room.h"
#include "scene/3d/room_group.h"
#include "scene/3d/room_manager.h"
#include "scene/3d/shape_cast.h"
#include "scene/3d/skeleton.h"
#include "scene/3d/soft_body.h"
#include "scene/3d/spring_arm.h"
#include "scene/3d/sprite_3d.h"
#include "scene/3d/vehicle_body.h"
#include "scene/3d/visibility_notifier.h"
#include "scene/animation/skeleton_ik.h"
#include "scene/resources/environment.h"
#include "scene/resources/mesh_library.h"
#include "scene/resources/occluder_shape.h"
#include "scene/resources/occluder_shape_polygon.h"
#endif

#include "modules/modules_enabled.gen.h" // For freetype.

static Ref<ResourceFormatSaverText> resource_saver_text;
static Ref<ResourceFormatLoaderText> resource_loader_text;

#ifdef MODULE_FREETYPE_ENABLED
static Ref<ResourceFormatLoaderDynamicFont> resource_loader_dynamic_font;
#endif // MODULE_FREETYPE_ENABLED

static Ref<ResourceFormatLoaderStreamTexture> resource_loader_stream_texture;
static Ref<ResourceFormatLoaderTextureLayered> resource_loader_texture_layered;

static Ref<ResourceFormatLoaderBMFont> resource_loader_bmfont;

static Ref<ResourceFormatSaverShader> resource_saver_shader;
static Ref<ResourceFormatLoaderShader> resource_loader_shader;

void register_scene_types() {
	SceneStringNames::create();

	OS::get_singleton()->yield(); //may take time to init

	Node::init_node_hrcr();

#ifdef MODULE_FREETYPE_ENABLED
	resource_loader_dynamic_font.instance();
	ResourceLoader::add_resource_format_loader(resource_loader_dynamic_font);
#endif // MODULE_FREETYPE_ENABLED

	resource_loader_stream_texture.instance();
	ResourceLoader::add_resource_format_loader(resource_loader_stream_texture);

	resource_loader_texture_layered.instance();
	ResourceLoader::add_resource_format_loader(resource_loader_texture_layered);

	resource_saver_text.instance();
	ResourceSaver::add_resource_format_saver(resource_saver_text, true);

	resource_loader_text.instance();
	ResourceLoader::add_resource_format_loader(resource_loader_text, true);

	resource_saver_shader.instance();
	ResourceSaver::add_resource_format_saver(resource_saver_shader, true);

	resource_loader_shader.instance();
	ResourceLoader::add_resource_format_loader(resource_loader_shader, true);

	resource_loader_bmfont.instance();
	ResourceLoader::add_resource_format_loader(resource_loader_bmfont, true);

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_class<Object>();

	ClassDB::register_class<Node>();
	ClassDB::register_virtual_class<InstancePlaceholder>();

	ClassDB::register_class<Viewport>();
	ClassDB::register_class<ViewportTexture>();
	ClassDB::register_class<HTTPRequest>();
	ClassDB::register_class<Timer>();
	ClassDB::register_class<CanvasLayer>();
	ClassDB::register_class<CanvasModulate>();
	ClassDB::register_class<ResourcePreloader>();

	/* REGISTER GUI */
	ClassDB::register_class<ButtonGroup>();
	ClassDB::register_virtual_class<BaseButton>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_class<ShortCut>();
	ClassDB::register_class<Control>();
	ClassDB::register_class<Button>();
	ClassDB::register_class<Label>();
	ClassDB::register_virtual_class<ScrollBar>();
	ClassDB::register_class<HScrollBar>();
	ClassDB::register_class<VScrollBar>();
	ClassDB::register_class<ProgressBar>();
	ClassDB::register_virtual_class<Slider>();
	ClassDB::register_class<HSlider>();
	ClassDB::register_class<VSlider>();
	ClassDB::register_class<Popup>();
	ClassDB::register_class<PopupPanel>();
	ClassDB::register_class<MenuButton>();
	ClassDB::register_class<CheckBox>();
	ClassDB::register_class<CheckButton>();
	ClassDB::register_class<ToolButton>();
	ClassDB::register_class<LinkButton>();
	ClassDB::register_class<Panel>();
	ClassDB::register_virtual_class<Range>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_class<TextureRect>();
	ClassDB::register_class<ColorRect>();
	ClassDB::register_class<NinePatchRect>();
	ClassDB::register_class<ReferenceRect>();
	ClassDB::register_class<AspectRatioContainer>();
	ClassDB::register_class<TabContainer>();
	ClassDB::register_class<Tabs>();
	ClassDB::register_virtual_class<Separator>();
	ClassDB::register_class<HSeparator>();
	ClassDB::register_class<VSeparator>();
	ClassDB::register_class<TextureButton>();
	ClassDB::register_class<Container>();
	ClassDB::register_virtual_class<BoxContainer>();
	ClassDB::register_class<HBoxContainer>();
	ClassDB::register_class<VBoxContainer>();
	ClassDB::register_class<GridContainer>();
	ClassDB::register_class<CenterContainer>();
	ClassDB::register_class<ScrollContainer>();
	ClassDB::register_class<PanelContainer>();
	ClassDB::register_virtual_class<FlowContainer>();
	ClassDB::register_class<HFlowContainer>();
	ClassDB::register_class<VFlowContainer>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_class<TextureProgress>();
	ClassDB::register_class<ItemList>();

	ClassDB::register_class<LineEdit>();
	ClassDB::register_class<VideoPlayer>();

#ifndef ADVANCED_GUI_DISABLED

	ClassDB::register_class<FileDialog>();

	ClassDB::register_class<PopupMenu>();
	ClassDB::register_class<Tree>();

	ClassDB::register_class<TextEdit>();

	ClassDB::register_virtual_class<TreeItem>();
	ClassDB::register_class<OptionButton>();
	ClassDB::register_class<SpinBox>();
	ClassDB::register_class<ColorPicker>();
	ClassDB::register_class<ColorPickerButton>();
	ClassDB::register_class<RichTextLabel>();
	ClassDB::register_class<RichTextEffect>();
	ClassDB::register_class<CharFXTransform>();
	ClassDB::register_class<PopupDialog>();
	ClassDB::register_class<WindowDialog>();
	ClassDB::register_class<AcceptDialog>();
	ClassDB::register_class<ConfirmationDialog>();
	ClassDB::register_class<MarginContainer>();
	ClassDB::register_class<ViewportContainer>();
	ClassDB::register_virtual_class<SplitContainer>();
	ClassDB::register_class<HSplitContainer>();
	ClassDB::register_class<VSplitContainer>();
	ClassDB::register_class<GraphNode>();
	ClassDB::register_class<GraphEdit>();

	OS::get_singleton()->yield(); //may take time to init

#endif

	/* REGISTER 3D */

	ClassDB::register_class<Skin>();
	ClassDB::register_virtual_class<SkinReference>();

	ClassDB::register_class<Spatial>();
	ClassDB::register_virtual_class<SpatialGizmo>();
	ClassDB::register_class<Skeleton>();
	ClassDB::register_class<AnimationPlayer>();
	ClassDB::register_class<Tween>();
	ClassDB::register_class<SceneTreeTween>();
	ClassDB::register_virtual_class<Tweener>();
	ClassDB::register_class<PropertyTweener>();
	ClassDB::register_class<IntervalTweener>();
	ClassDB::register_class<CallbackTweener>();
	ClassDB::register_class<MethodTweener>();
	ClassDB::register_class<YieldTweener>();

	ClassDB::register_class<AnimationTreePlayer>();
	ClassDB::register_class<AnimationTree>();
	ClassDB::register_class<AnimationNode>();
	ClassDB::register_class<AnimationRootNode>();
	ClassDB::register_class<AnimationNodeBlendTree>();
	ClassDB::register_class<AnimationNodeBlendSpace1D>();
	ClassDB::register_class<AnimationNodeBlendSpace2D>();
	ClassDB::register_class<AnimationNodeStateMachine>();
	ClassDB::register_class<AnimationNodeStateMachinePlayback>();

	ClassDB::register_class<AnimationNodeStateMachineTransition>();
	ClassDB::register_class<AnimationNodeOutput>();
	ClassDB::register_class<AnimationNodeOneShot>();
	ClassDB::register_class<AnimationNodeAnimation>();
	ClassDB::register_class<AnimationNodeAdd2>();
	ClassDB::register_class<AnimationNodeAdd3>();
	ClassDB::register_class<AnimationNodeBlend2>();
	ClassDB::register_class<AnimationNodeBlend3>();
	ClassDB::register_class<AnimationNodeTimeScale>();
	ClassDB::register_class<AnimationNodeTimeSeek>();
	ClassDB::register_class<AnimationNodeTransition>();

	OS::get_singleton()->yield(); //may take time to init

#ifndef _3D_DISABLED
	ClassDB::register_virtual_class<VisualInstance>();
	ClassDB::register_virtual_class<CullInstance>();
	ClassDB::register_virtual_class<GeometryInstance>();
	ClassDB::register_class<Camera>();
	ClassDB::register_class<ClippedCamera>();
	ClassDB::register_class<Listener>();
	ClassDB::register_class<ARVRCamera>();
	ClassDB::register_class<ARVRController>();
	ClassDB::register_class<ARVRAnchor>();
	ClassDB::register_class<ARVROrigin>();
	ClassDB::register_class<InterpolatedCamera>();
	ClassDB::register_class<MeshInstance>();
	ClassDB::register_class<LOD>();
	ClassDB::register_class<ImmediateGeometry>();
	ClassDB::register_virtual_class<SpriteBase3D>();
	ClassDB::register_class<Sprite3D>();
	ClassDB::register_class<AnimatedSprite3D>();
	ClassDB::register_class<Label3D>();
	ClassDB::register_virtual_class<Light>();
	ClassDB::register_class<DirectionalLight>();
	ClassDB::register_class<OmniLight>();
	ClassDB::register_class<SpotLight>();
	ClassDB::register_class<ReflectionProbe>();
	ClassDB::register_class<GIProbe>();
	ClassDB::register_class<GIProbeData>();
	ClassDB::register_class<BakedLightmap>();
	ClassDB::register_class<BakedLightmapData>();
	ClassDB::register_class<Particles>();
	ClassDB::register_class<CPUParticles>();
	ClassDB::register_class<Position3D>();
	ClassDB::register_class<NavigationMeshInstance>();
	ClassDB::register_class<NavigationMesh>();
	ClassDB::register_class<Navigation>();
	ClassDB::register_class<Room>();
	ClassDB::register_class<RoomGroup>();
	ClassDB::register_class<RoomManager>();
	ClassDB::register_class<Occluder>();
	ClassDB::register_class<Portal>();
	ClassDB::register_class<MergeGroup>();

	ClassDB::register_class<RootMotionView>();
	ClassDB::set_class_enabled("RootMotionView", false); //disabled by default, enabled by editor

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_virtual_class<CollisionObject>();
	ClassDB::register_virtual_class<PhysicsBody>();
	ClassDB::register_class<StaticBody>();
	ClassDB::register_class<RigidBody>();
	ClassDB::register_class<KinematicCollision>();
	ClassDB::register_class<KinematicBody>();
	ClassDB::register_class<SpringArm>();

	ClassDB::register_class<PhysicalBone>();
	ClassDB::register_class<SoftBody>();

	ClassDB::register_class<SkeletonIK>();
	ClassDB::register_class<BoneAttachment>();

	ClassDB::register_class<VehicleBody>();
	ClassDB::register_class<VehicleWheel>();
	ClassDB::register_class<Area>();
	ClassDB::register_class<ProximityGroup>();
	ClassDB::register_class<CollisionShape>();
	ClassDB::register_class<CollisionPolygon>();
	ClassDB::register_class<RayCast>();
	ClassDB::register_class<ShapeCast>();
	ClassDB::register_class<MultiMeshInstance>();

	ClassDB::register_class<Curve3D>();
	ClassDB::register_class<Path>();
	ClassDB::register_class<PathFollow>();
	ClassDB::register_class<VisibilityNotifier>();
	ClassDB::register_class<VisibilityEnabler>();
	ClassDB::register_class<WorldEnvironment>();
	ClassDB::register_class<RemoteTransform>();

	ClassDB::register_virtual_class<Joint>();
	ClassDB::register_class<PinJoint>();
	ClassDB::register_class<HingeJoint>();
	ClassDB::register_class<SliderJoint>();
	ClassDB::register_class<ConeTwistJoint>();
	ClassDB::register_class<Generic6DOFJoint>();

	ClassDB::register_class<Navigation>();
	ClassDB::register_class<NavigationMeshInstance>();
	ClassDB::register_class<NavigationAgent>();
	ClassDB::register_class<NavigationObstacle>();

	OS::get_singleton()->yield(); //may take time to init

#endif
	ClassDB::register_class<NavigationMesh>();

	AcceptDialog::set_swap_ok_cancel(GLOBAL_DEF_NOVAL("gui/common/swap_ok_cancel", bool(OS::get_singleton()->get_swap_ok_cancel())));

	ClassDB::register_class<Shader>();
	ClassDB::register_class<VisualShader>();
	ClassDB::register_virtual_class<VisualShaderNode>();
	ClassDB::register_class<VisualShaderNodeCustom>();
	ClassDB::register_class<VisualShaderNodeInput>();
	ClassDB::register_virtual_class<VisualShaderNodeOutput>();
	ClassDB::register_class<VisualShaderNodeGroupBase>();
	ClassDB::register_class<VisualShaderNodeScalarConstant>();
	ClassDB::register_class<VisualShaderNodeBooleanConstant>();
	ClassDB::register_class<VisualShaderNodeColorConstant>();
	ClassDB::register_class<VisualShaderNodeVec3Constant>();
	ClassDB::register_class<VisualShaderNodeTransformConstant>();
	ClassDB::register_class<VisualShaderNodeScalarOp>();
	ClassDB::register_class<VisualShaderNodeVectorOp>();
	ClassDB::register_class<VisualShaderNodeColorOp>();
	ClassDB::register_class<VisualShaderNodeTransformMult>();
	ClassDB::register_class<VisualShaderNodeTransformVecMult>();
	ClassDB::register_class<VisualShaderNodeScalarFunc>();
	ClassDB::register_class<VisualShaderNodeVectorFunc>();
	ClassDB::register_class<VisualShaderNodeColorFunc>();
	ClassDB::register_class<VisualShaderNodeTransformFunc>();
	ClassDB::register_class<VisualShaderNodeDotProduct>();
	ClassDB::register_class<VisualShaderNodeVectorLen>();
	ClassDB::register_class<VisualShaderNodeDeterminant>();
	ClassDB::register_class<VisualShaderNodeScalarDerivativeFunc>();
	ClassDB::register_class<VisualShaderNodeVectorDerivativeFunc>();
	ClassDB::register_class<VisualShaderNodeScalarClamp>();
	ClassDB::register_class<VisualShaderNodeVectorClamp>();
	ClassDB::register_class<VisualShaderNodeFaceForward>();
	ClassDB::register_class<VisualShaderNodeOuterProduct>();
	ClassDB::register_class<VisualShaderNodeVectorScalarStep>();
	ClassDB::register_class<VisualShaderNodeScalarSmoothStep>();
	ClassDB::register_class<VisualShaderNodeVectorSmoothStep>();
	ClassDB::register_class<VisualShaderNodeVectorScalarSmoothStep>();
	ClassDB::register_class<VisualShaderNodeVectorDistance>();
	ClassDB::register_class<VisualShaderNodeVectorRefract>();
	ClassDB::register_class<VisualShaderNodeScalarInterp>();
	ClassDB::register_class<VisualShaderNodeVectorInterp>();
	ClassDB::register_class<VisualShaderNodeVectorScalarMix>();
	ClassDB::register_class<VisualShaderNodeVectorCompose>();
	ClassDB::register_class<VisualShaderNodeTransformCompose>();
	ClassDB::register_class<VisualShaderNodeVectorDecompose>();
	ClassDB::register_class<VisualShaderNodeTransformDecompose>();
	ClassDB::register_class<VisualShaderNodeTexture>();
	ClassDB::register_class<VisualShaderNodeCubeMap>();
	ClassDB::register_virtual_class<VisualShaderNodeUniform>();
	ClassDB::register_class<VisualShaderNodeUniformRef>();
	ClassDB::register_class<VisualShaderNodeScalarUniform>();
	ClassDB::register_class<VisualShaderNodeBooleanUniform>();
	ClassDB::register_class<VisualShaderNodeColorUniform>();
	ClassDB::register_class<VisualShaderNodeVec3Uniform>();
	ClassDB::register_class<VisualShaderNodeTransformUniform>();
	ClassDB::register_class<VisualShaderNodeTextureUniform>();
	ClassDB::register_class<VisualShaderNodeTextureUniformTriplanar>();
	ClassDB::register_class<VisualShaderNodeCubeMapUniform>();
	ClassDB::register_class<VisualShaderNodeIf>();
	ClassDB::register_class<VisualShaderNodeSwitch>();
	ClassDB::register_class<VisualShaderNodeScalarSwitch>();
	ClassDB::register_class<VisualShaderNodeFresnel>();
	ClassDB::register_class<VisualShaderNodeExpression>();
	ClassDB::register_class<VisualShaderNodeGlobalExpression>();
	ClassDB::register_class<VisualShaderNodeIs>();
	ClassDB::register_class<VisualShaderNodeCompare>();

	ClassDB::register_class<ShaderMaterial>();
	ClassDB::register_virtual_class<CanvasItem>();
	ClassDB::register_class<CanvasItemMaterial>();
	SceneTree::add_idle_callback(CanvasItemMaterial::flush_changes);
	CanvasItemMaterial::init_shaders();
	ClassDB::register_class<Node2D>();
	ClassDB::register_class<CPUParticles2D>();
	ClassDB::register_class<Particles2D>();
	//ClassDB::register_class<ParticleAttractor2D>();
	ClassDB::register_class<Sprite>();
	//ClassDB::register_type<ViewportSprite>();
	ClassDB::register_class<SpriteFrames>();
	ClassDB::register_class<AnimatedSprite>();
	ClassDB::register_class<Position2D>();
	ClassDB::register_class<Line2D>();
	ClassDB::register_class<MeshInstance2D>();
	ClassDB::register_class<MultiMeshInstance2D>();
	ClassDB::register_virtual_class<CollisionObject2D>();
	ClassDB::register_virtual_class<PhysicsBody2D>();
	ClassDB::register_class<StaticBody2D>();
	ClassDB::register_class<RigidBody2D>();
	ClassDB::register_class<KinematicBody2D>();
	ClassDB::register_class<KinematicCollision2D>();
	ClassDB::register_class<Area2D>();
	ClassDB::register_class<CollisionShape2D>();
	ClassDB::register_class<CollisionPolygon2D>();
	ClassDB::register_class<RayCast2D>();
	ClassDB::register_class<ShapeCast2D>();
	ClassDB::register_class<VisibilityNotifier2D>();
	ClassDB::register_class<VisibilityEnabler2D>();
	ClassDB::register_class<Polygon2D>();
	ClassDB::register_class<Skeleton2D>();
	ClassDB::register_class<Bone2D>();
	ClassDB::register_class<Light2D>();
	ClassDB::register_class<LightOccluder2D>();
	ClassDB::register_class<OccluderPolygon2D>();
	ClassDB::register_class<YSort>();
	ClassDB::register_class<BackBufferCopy>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_class<Camera2D>();
	ClassDB::register_class<Listener2D>();
	ClassDB::register_virtual_class<Joint2D>();
	ClassDB::register_class<PinJoint2D>();
	ClassDB::register_class<GrooveJoint2D>();
	ClassDB::register_class<DampedSpringJoint2D>();
	ClassDB::register_class<TileSet>();
	ClassDB::register_class<TileMap>();
	ClassDB::register_class<ParallaxBackground>();
	ClassDB::register_class<ParallaxLayer>();
	ClassDB::register_class<TouchScreenButton>();
	ClassDB::register_class<RemoteTransform2D>();

	OS::get_singleton()->yield(); //may take time to init

	/* REGISTER RESOURCES */

	ClassDB::register_virtual_class<Shader>();
	ClassDB::register_class<ParticlesMaterial>();
	SceneTree::add_idle_callback(ParticlesMaterial::flush_changes);
	ParticlesMaterial::init_shaders();

	ClassDB::register_virtual_class<Mesh>();
	ClassDB::register_class<ArrayMesh>();
	ClassDB::register_class<MultiMesh>();
	ClassDB::register_class<SurfaceTool>();
	ClassDB::register_class<MeshDataTool>();

#ifndef _3D_DISABLED
	ClassDB::register_virtual_class<PrimitiveMesh>();
	ClassDB::register_class<CapsuleMesh>();
	ClassDB::register_class<CubeMesh>();
	ClassDB::register_class<CylinderMesh>();
	ClassDB::register_class<PlaneMesh>();
	ClassDB::register_class<PrismMesh>();
	ClassDB::register_class<QuadMesh>();
	ClassDB::register_class<SphereMesh>();
	ClassDB::register_class<TextMesh>();
	ClassDB::register_class<TorusMesh>();
	ClassDB::register_class<PointMesh>();
	ClassDB::register_virtual_class<Material>();
	ClassDB::register_virtual_class<Material3D>();
	ClassDB::register_class<SpatialMaterial>();
	ClassDB::register_class<ORMSpatialMaterial>();
	SceneTree::add_idle_callback(Material3D::flush_changes);
	Material3D::init_shaders();

	ClassDB::register_class<MeshLibrary>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_virtual_class<Shape>();
	ClassDB::register_class<RayShape>();
	ClassDB::register_class<SphereShape>();
	ClassDB::register_class<BoxShape>();
	ClassDB::register_class<CapsuleShape>();
	ClassDB::register_class<CylinderShape>();
	ClassDB::register_class<HeightMapShape>();
	ClassDB::register_class<PlaneShape>();
	ClassDB::register_class<ConvexPolygonShape>();
	ClassDB::register_class<ConcavePolygonShape>();
	ClassDB::register_virtual_class<OccluderShape>();
	ClassDB::register_class<OccluderShapeSphere>();
	ClassDB::register_class<OccluderShapePolygon>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_class<SpatialVelocityTracker>();

#endif
	ClassDB::register_class<PhysicsMaterial>();
	ClassDB::register_class<World>();
	ClassDB::register_class<Environment>();
	ClassDB::register_class<World2D>();
	ClassDB::register_virtual_class<Texture>();
	ClassDB::register_virtual_class<Sky>();
	ClassDB::register_class<PanoramaSky>();
	ClassDB::register_class<ProceduralSky>();
	ClassDB::register_class<StreamTexture>();
	ClassDB::register_class<ImageTexture>();
	ClassDB::register_class<AtlasTexture>();
	ClassDB::register_class<MeshTexture>();
	ClassDB::register_class<LargeTexture>();
	ClassDB::register_class<CurveTexture>();
	ClassDB::register_class<GradientTexture>();
	ClassDB::register_class<GradientTexture2D>();
	ClassDB::register_class<ProxyTexture>();
	ClassDB::register_class<AnimatedTexture>();
	ClassDB::register_class<CameraTexture>();
	ClassDB::register_class<ExternalTexture>();
	ClassDB::register_class<CubeMap>();
	ClassDB::register_virtual_class<TextureLayered>();
	ClassDB::register_class<Texture3D>();
	ClassDB::register_class<TextureArray>();
	ClassDB::register_class<Animation>();
	ClassDB::register_virtual_class<Font>();
	ClassDB::register_class<BitmapFont>();
	ClassDB::register_class<Curve>();

	ClassDB::register_class<TextFile>();

#ifdef MODULE_FREETYPE_ENABLED
	ClassDB::register_class<DynamicFontData>();
	ClassDB::register_class<DynamicFont>();

	DynamicFont::initialize_dynamic_fonts();
#endif // MODULE_FREETYPE_ENABLED

	ClassDB::register_virtual_class<StyleBox>();
	ClassDB::register_class<StyleBoxEmpty>();
	ClassDB::register_class<StyleBoxTexture>();
	ClassDB::register_class<StyleBoxFlat>();
	ClassDB::register_class<StyleBoxLine>();
	ClassDB::register_class<Theme>();

	ClassDB::register_class<PolygonPathFinder>();
	ClassDB::register_class<BitMap>();
	ClassDB::register_class<Gradient>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_class<AudioStreamPlayer>();
	ClassDB::register_class<AudioStreamPlayer2D>();
#ifndef _3D_DISABLED
	ClassDB::register_class<AudioStreamPlayer3D>();
#endif
	ClassDB::register_virtual_class<VideoStream>();
	ClassDB::register_class<AudioStreamSample>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_virtual_class<Shape2D>();
	ClassDB::register_class<LineShape2D>();
	ClassDB::register_class<SegmentShape2D>();
	ClassDB::register_class<RayShape2D>();
	ClassDB::register_class<CircleShape2D>();
	ClassDB::register_class<RectangleShape2D>();
	ClassDB::register_class<CapsuleShape2D>();
	ClassDB::register_class<ConvexPolygonShape2D>();
	ClassDB::register_class<ConcavePolygonShape2D>();
	ClassDB::register_class<Curve2D>();
	ClassDB::register_class<Path2D>();
	ClassDB::register_class<PathFollow2D>();

	ClassDB::register_class<Navigation2D>();
	ClassDB::register_class<NavigationPolygon>();
	ClassDB::register_class<NavigationPolygonInstance>();
	ClassDB::register_class<NavigationAgent2D>();
	ClassDB::register_class<NavigationObstacle2D>();

	OS::get_singleton()->yield(); //may take time to init

	ClassDB::register_virtual_class<SceneState>();
	ClassDB::register_class<PackedScene>();

	ClassDB::register_class<SceneTree>();
	ClassDB::register_virtual_class<SceneTreeTimer>(); //sorry, you can't create it

#ifndef DISABLE_DEPRECATED
	ClassDB::add_compatibility_class("ImageSkyBox", "PanoramaSky");
	ClassDB::add_compatibility_class("FixedSpatialMaterial", "SpatialMaterial");
	ClassDB::add_compatibility_class("Mesh", "ArrayMesh");

#endif

	OS::get_singleton()->yield(); //may take time to init

	for (int i = 0; i < 20; i++) {
		GLOBAL_DEF(vformat("%s/layer_%d", PNAME("layer_names/2d_render"), i + 1), "");
		GLOBAL_DEF(vformat("%s/layer_%d", PNAME("layer_names/3d_render"), i + 1), "");
	}

	for (int i = 0; i < 32; i++) {
		GLOBAL_DEF(vformat("%s/layer_%d", PNAME("layer_names/2d_physics"), i + 1), "");
		GLOBAL_DEF(vformat("%s/layer_%d", PNAME("layer_names/3d_physics"), i + 1), "");
	}

	for (int i = 0; i < 32; i++) {
		GLOBAL_DEF(vformat("%s/layer_%d", PNAME("layer_names/2d_navigation"), i + 1), "");
		GLOBAL_DEF(vformat("%s/layer_%d", PNAME("layer_names/3d_navigation"), i + 1), "");
	};
}

void initialize_theme() {
	bool default_theme_hidpi = GLOBAL_DEF("gui/theme/use_hidpi", false);
	ProjectSettings::get_singleton()->set_custom_property_info("gui/theme/use_hidpi", PropertyInfo(Variant::BOOL, "gui/theme/use_hidpi", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED));
	String theme_path = GLOBAL_DEF_RST("gui/theme/custom", "");
	ProjectSettings::get_singleton()->set_custom_property_info("gui/theme/custom", PropertyInfo(Variant::STRING, "gui/theme/custom", PROPERTY_HINT_FILE, "*.tres,*.res,*.theme", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED));
	String font_path = GLOBAL_DEF_RST("gui/theme/custom_font", "");
	ProjectSettings::get_singleton()->set_custom_property_info("gui/theme/custom_font", PropertyInfo(Variant::STRING, "gui/theme/custom_font", PROPERTY_HINT_FILE, "*.tres,*.res,*.font", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_RESTART_IF_CHANGED));

	Ref<Font> font;
	if (font_path != String()) {
		font = ResourceLoader::load(font_path);
		if (!font.is_valid()) {
			ERR_PRINT("Error loading custom font '" + font_path + "'");
		}
	}

	// Always make the default theme to avoid invalid default font/icon/style in the given theme
	make_default_theme(default_theme_hidpi, font);

	if (theme_path != String()) {
		Ref<Theme> theme = ResourceLoader::load(theme_path);
		if (theme.is_valid()) {
			Theme::set_project_default(theme);
			if (font.is_valid()) {
				Theme::set_default_font(font);
			}
		} else {
			ERR_PRINT("Error loading custom theme '" + theme_path + "'");
		}
	}
}

void unregister_scene_types() {
	clear_default_theme();

#ifdef MODULE_FREETYPE_ENABLED
	ResourceLoader::remove_resource_format_loader(resource_loader_dynamic_font);
	resource_loader_dynamic_font.unref();

	DynamicFont::finish_dynamic_fonts();
#endif // MODULE_FREETYPE_ENABLED

	ResourceLoader::remove_resource_format_loader(resource_loader_texture_layered);
	resource_loader_texture_layered.unref();

	ResourceLoader::remove_resource_format_loader(resource_loader_stream_texture);
	resource_loader_stream_texture.unref();

	ResourceSaver::remove_resource_format_saver(resource_saver_text);
	resource_saver_text.unref();

	ResourceLoader::remove_resource_format_loader(resource_loader_text);
	resource_loader_text.unref();

	ResourceSaver::remove_resource_format_saver(resource_saver_shader);
	resource_saver_shader.unref();

	ResourceLoader::remove_resource_format_loader(resource_loader_shader);
	resource_loader_shader.unref();

	ResourceLoader::remove_resource_format_loader(resource_loader_bmfont);
	resource_loader_bmfont.unref();

	//SpatialMaterial is not initialised when 3D is disabled, so it shouldn't be cleaned up either
#ifndef _3D_DISABLED
	Material3D::finish_shaders();
#endif // _3D_DISABLED

	ParticlesMaterial::finish_shaders();
	CanvasItemMaterial::finish_shaders();
	SceneStringNames::free();
}
