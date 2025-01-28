/**************************************************************************/
/*  renderer_canvas_cull.h                                                */
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

#ifndef RENDERER_CANVAS_CULL_H
#define RENDERER_CANVAS_CULL_H

#include "core/templates/paged_allocator.h"
#include "renderer_compositor.h"
#include "renderer_viewport.h"
#include "servers/rendering/instance_uniforms.h"

class RendererCanvasCull {
	static void _dependency_changed(Dependency::DependencyChangedNotification p_notification, DependencyTracker *p_tracker);
	static void _dependency_deleted(const RID &p_dependency, DependencyTracker *p_tracker);

public:
	struct Item : public RendererCanvasRender::Item {
		RID parent; // canvas it belongs to
		RID self;
		List<Item *>::Element *E;
		int z_index;
		bool z_relative;
		bool sort_y;
		Color modulate;
		Color self_modulate;
		bool use_parent_material;
		int index;
		bool children_order_dirty;
		int ysort_children_count;
		Color ysort_modulate;
		Transform2D ysort_xform; // Relative to y-sorted subtree's root item (identity for such root). Its `origin.y` is used for sorting.
		int ysort_index;
		int ysort_parent_abs_z_index; // Absolute Z index of parent. Only populated and used when y-sorting.
		uint32_t visibility_layer = 0xffffffff;

		Vector<Item *> child_items;

		struct VisibilityNotifierData {
			Rect2 area;
			Callable enter_callable;
			Callable exit_callable;
			bool just_visible = false;
			uint64_t visible_in_frame = 0;
			SelfList<VisibilityNotifierData> visible_element;
			VisibilityNotifierData() :
					visible_element(this) {
			}
		};

		VisibilityNotifierData *visibility_notifier = nullptr;

		DependencyTracker dependency_tracker;
		InstanceUniforms instance_uniforms;
		SelfList<Item> update_item;

		bool update_dependencies = false;

		Item() :
				update_item(this) {
			children_order_dirty = true;
			E = nullptr;
			z_index = 0;
			modulate = Color(1, 1, 1, 1);
			self_modulate = Color(1, 1, 1, 1);
			sort_y = false;
			use_parent_material = false;
			z_relative = true;
			index = 0;
			ysort_children_count = -1;
			ysort_xform = Transform2D();
			ysort_index = 0;
			ysort_parent_abs_z_index = 0;

			dependency_tracker.userdata = this;
			dependency_tracker.changed_callback = &RendererCanvasCull::_dependency_changed;
			dependency_tracker.deleted_callback = &RendererCanvasCull::_dependency_deleted;
		}
	};

	void _item_queue_update(Item *p_item, bool p_update_dependencies);
	SelfList<Item>::List _item_update_list;

	struct ItemIndexSort {
		_FORCE_INLINE_ bool operator()(const Item *p_left, const Item *p_right) const {
			return p_left->index < p_right->index;
		}
	};

	struct ItemYSort {
		_FORCE_INLINE_ bool operator()(const Item *p_left, const Item *p_right) const {
			const real_t left_y = p_left->ysort_xform.columns[2].y;
			const real_t right_y = p_right->ysort_xform.columns[2].y;
			if (Math::is_equal_approx(left_y, right_y)) {
				return p_left->ysort_index < p_right->ysort_index;
			}

			return left_y < right_y;
		}
	};

	struct LightOccluderPolygon {
		bool active;
		Rect2 aabb;
		RS::CanvasOccluderPolygonCullMode cull_mode;
		RID occluder;
		HashSet<RendererCanvasRender::LightOccluderInstance *> owners;

		LightOccluderPolygon() {
			active = false;
			cull_mode = RS::CANVAS_OCCLUDER_POLYGON_CULL_DISABLED;
		}
	};

	RID_Owner<LightOccluderPolygon, true> canvas_light_occluder_polygon_owner;

	RID_Owner<RendererCanvasRender::LightOccluderInstance, true> canvas_light_occluder_owner;

	struct Canvas : public RendererViewport::CanvasBase {
		HashSet<RID> viewports;
		struct ChildItem {
			Item *item = nullptr;
			bool operator<(const ChildItem &p_item) const {
				return item->index < p_item.item->index;
			}
		};

		HashSet<RendererCanvasRender::Light *> lights;
		HashSet<RendererCanvasRender::Light *> directional_lights;

		HashSet<RendererCanvasRender::LightOccluderInstance *> occluders;

		bool children_order_dirty;
		Vector<ChildItem> child_items;
		Color modulate;
		RID parent;
		float parent_scale;

		int find_item(Item *p_item) {
			for (int i = 0; i < child_items.size(); i++) {
				if (child_items[i].item == p_item) {
					return i;
				}
			}
			return -1;
		}
		void erase_item(Item *p_item) {
			int idx = find_item(p_item);
			if (idx >= 0) {
				child_items.remove_at(idx);
			}
		}

		Canvas() {
			modulate = Color(1, 1, 1, 1);
			children_order_dirty = true;
			parent_scale = 1.0;
		}
	};

	mutable RID_Owner<Canvas, true> canvas_owner;
	RID_Owner<Item, true> canvas_item_owner;
	RID_Owner<RendererCanvasRender::Light, true> canvas_light_owner;

	template <typename T>
	void _free_rids(T &p_owner, const char *p_type);

	bool disable_scale;
	bool sdf_used = false;
	bool snapping_2d_transforms_to_pixel = false;

	bool debug_redraw = false;
	double debug_redraw_time = 0;
	Color debug_redraw_color;

	PagedAllocator<Item::VisibilityNotifierData> visibility_notifier_allocator;
	SelfList<Item::VisibilityNotifierData>::List visibility_notifier_list;

	_FORCE_INLINE_ void _attach_canvas_item_for_draw(Item *ci, Item *p_canvas_clip, RendererCanvasRender::Item **r_z_list, RendererCanvasRender::Item **r_z_last_list, const Transform2D &p_transform, const Rect2 &p_clip_rect, Rect2 p_global_rect, const Color &modulate, int p_z, RendererCanvasCull::Item *p_material_owner, bool p_use_canvas_group, RendererCanvasRender::Item *r_canvas_group_from);

private:
	void _render_canvas_item_tree(RID p_to_render_target, Canvas::ChildItem *p_child_items, int p_child_item_count, const Transform2D &p_transform, const Rect2 &p_clip_rect, const Color &p_modulate, RendererCanvasRender::Light *p_lights, RendererCanvasRender::Light *p_directional_lights, RS::CanvasItemTextureFilter p_default_filter, RS::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, uint32_t p_canvas_cull_mask, RenderingMethod::RenderInfo *r_render_info = nullptr);
	void _cull_canvas_item(Item *p_canvas_item, const Transform2D &p_parent_xform, const Rect2 &p_clip_rect, const Color &p_modulate, int p_z, RendererCanvasRender::Item **r_z_list, RendererCanvasRender::Item **r_z_last_list, Item *p_canvas_clip, Item *p_material_owner, bool p_is_already_y_sorted, uint32_t p_canvas_cull_mask, const Point2 &p_repeat_size, int p_repeat_times, RendererCanvasRender::Item *p_repeat_source_item);

	void _collect_ysort_children(RendererCanvasCull::Item *p_canvas_item, RendererCanvasCull::Item *p_material_owner, const Color &p_modulate, RendererCanvasCull::Item **r_items, int &r_index, int p_z);
	int _count_ysort_children(RendererCanvasCull::Item *p_canvas_item);
	void _mark_ysort_dirty(RendererCanvasCull::Item *ysort_owner);

	static constexpr int z_range = RS::CANVAS_ITEM_Z_MAX - RS::CANVAS_ITEM_Z_MIN + 1;

	RendererCanvasRender::Item **z_list;
	RendererCanvasRender::Item **z_last_list;

	Transform2D _current_camera_transform;

public:
	void render_canvas(RID p_render_target, Canvas *p_canvas, const Transform2D &p_transform, RendererCanvasRender::Light *p_lights, RendererCanvasRender::Light *p_directional_lights, const Rect2 &p_clip_rect, RS::CanvasItemTextureFilter p_default_filter, RS::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_transforms_to_pixel, bool p_snap_2d_vertices_to_pixel, uint32_t p_canvas_cull_mask, RenderingMethod::RenderInfo *r_render_info = nullptr);

	bool was_sdf_used();

	RID canvas_allocate();
	void canvas_initialize(RID p_rid);

	void canvas_set_item_mirroring(RID p_canvas, RID p_item, const Point2 &p_mirroring);
	void canvas_set_item_repeat(RID p_item, const Point2 &p_repeat_size, int p_repeat_times);
	void canvas_set_modulate(RID p_canvas, const Color &p_color);
	void canvas_set_parent(RID p_canvas, RID p_parent, float p_scale);
	void canvas_set_disable_scale(bool p_disable);

	RID canvas_item_allocate();
	void canvas_item_initialize(RID p_rid);

	void canvas_item_set_parent(RID p_item, RID p_parent);

	void canvas_item_set_visible(RID p_item, bool p_visible);
	void canvas_item_set_light_mask(RID p_item, int p_mask);

	void canvas_item_set_visibility_layer(RID p_item, uint32_t p_layer);
	uint32_t canvas_item_get_visibility_layer(RID p_item);

	void canvas_item_set_transform(RID p_item, const Transform2D &p_transform);
	void canvas_item_set_clip(RID p_item, bool p_clip);
	void canvas_item_set_distance_field_mode(RID p_item, bool p_enable);
	void canvas_item_set_custom_rect(RID p_item, bool p_custom_rect, const Rect2 &p_rect = Rect2());
	void canvas_item_set_modulate(RID p_item, const Color &p_color);
	void canvas_item_set_self_modulate(RID p_item, const Color &p_color);

	void canvas_item_set_draw_behind_parent(RID p_item, bool p_enable);
	void canvas_item_set_use_identity_transform(RID p_item, bool p_enable);

	void canvas_item_set_update_when_visible(RID p_item, bool p_update);

	void canvas_item_add_line(RID p_item, const Point2 &p_from, const Point2 &p_to, const Color &p_color, float p_width = -1.0, bool p_antialiased = false);
	void canvas_item_add_polyline(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, float p_width = -1.0, bool p_antialiased = false);
	void canvas_item_add_multiline(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, float p_width = -1.0, bool p_antialiased = false);
	void canvas_item_add_rect(RID p_item, const Rect2 &p_rect, const Color &p_color, bool p_antialiased);
	void canvas_item_add_circle(RID p_item, const Point2 &p_pos, float p_radius, const Color &p_color, bool p_antialiased);
	void canvas_item_add_texture_rect(RID p_item, const Rect2 &p_rect, RID p_texture, bool p_tile = false, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false);
	void canvas_item_add_texture_rect_region(RID p_item, const Rect2 &p_rect, RID p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), bool p_transpose = false, bool p_clip_uv = false);
	void canvas_item_add_msdf_texture_rect_region(RID p_item, const Rect2 &p_rect, RID p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1), int p_outline_size = 0, float p_px_range = 1.0, float p_scale = 1.0);
	void canvas_item_add_lcd_texture_rect_region(RID p_item, const Rect2 &p_rect, RID p_texture, const Rect2 &p_src_rect, const Color &p_modulate = Color(1, 1, 1));
	void canvas_item_add_nine_patch(RID p_item, const Rect2 &p_rect, const Rect2 &p_source, RID p_texture, const Vector2 &p_topleft, const Vector2 &p_bottomright, RS::NinePatchAxisMode p_x_axis_mode = RS::NINE_PATCH_STRETCH, RS::NinePatchAxisMode p_y_axis_mode = RS::NINE_PATCH_STRETCH, bool p_draw_center = true, const Color &p_modulate = Color(1, 1, 1));
	void canvas_item_add_primitive(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, RID p_texture);
	void canvas_item_add_polygon(RID p_item, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), RID p_texture = RID());
	void canvas_item_add_triangle_array(RID p_item, const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>(), RID p_texture = RID(), int p_count = -1);
	void canvas_item_add_mesh(RID p_item, const RID &p_mesh, const Transform2D &p_transform = Transform2D(), const Color &p_modulate = Color(1, 1, 1), RID p_texture = RID());
	void canvas_item_add_multimesh(RID p_item, RID p_mesh, RID p_texture = RID());
	void canvas_item_add_particles(RID p_item, RID p_particles, RID p_texture);
	void canvas_item_add_set_transform(RID p_item, const Transform2D &p_transform);
	void canvas_item_add_clip_ignore(RID p_item, bool p_ignore);
	void canvas_item_add_animation_slice(RID p_item, double p_animation_length, double p_slice_begin, double p_slice_end, double p_offset);

	void canvas_item_set_sort_children_by_y(RID p_item, bool p_enable);
	void canvas_item_set_z_index(RID p_item, int p_z);
	void canvas_item_set_z_as_relative_to_parent(RID p_item, bool p_enable);
	void canvas_item_set_copy_to_backbuffer(RID p_item, bool p_enable, const Rect2 &p_rect);
	void canvas_item_attach_skeleton(RID p_item, RID p_skeleton);

	void canvas_item_clear(RID p_item);
	void canvas_item_set_draw_index(RID p_item, int p_index);

	void canvas_item_set_material(RID p_item, RID p_material);

	void canvas_item_set_use_parent_material(RID p_item, bool p_enable);

	void canvas_item_set_instance_shader_parameter(RID p_item, const StringName &p_parameter, const Variant &p_value);
	void canvas_item_get_instance_shader_parameter_list(RID p_item, List<PropertyInfo> *p_parameters) const;
	Variant canvas_item_get_instance_shader_parameter(RID p_item, const StringName &p_parameter) const;
	Variant canvas_item_get_instance_shader_parameter_default_value(RID p_item, const StringName &p_parameter) const;

	void canvas_item_set_visibility_notifier(RID p_item, bool p_enable, const Rect2 &p_area, const Callable &p_enter_callable, const Callable &p_exit_callable);

	void canvas_item_set_canvas_group_mode(RID p_item, RS::CanvasGroupMode p_mode, float p_clear_margin = 5.0, bool p_fit_empty = false, float p_fit_margin = 0.0, bool p_blur_mipmaps = false);

	void canvas_item_set_debug_redraw(bool p_enabled);
	bool canvas_item_get_debug_redraw() const;

	void canvas_item_set_interpolated(RID p_item, bool p_interpolated);
	void canvas_item_reset_physics_interpolation(RID p_item);
	void canvas_item_transform_physics_interpolation(RID p_item, const Transform2D &p_transform);

	RID canvas_light_allocate();
	void canvas_light_initialize(RID p_rid);

	void update();

	void canvas_light_set_mode(RID p_light, RS::CanvasLightMode p_mode);
	void canvas_light_attach_to_canvas(RID p_light, RID p_canvas);
	void canvas_light_set_enabled(RID p_light, bool p_enabled);
	void canvas_light_set_texture_scale(RID p_light, float p_scale);
	void canvas_light_set_transform(RID p_light, const Transform2D &p_transform);
	void canvas_light_set_texture(RID p_light, RID p_texture);
	void canvas_light_set_texture_offset(RID p_light, const Vector2 &p_offset);
	void canvas_light_set_color(RID p_light, const Color &p_color);
	void canvas_light_set_height(RID p_light, float p_height);
	void canvas_light_set_energy(RID p_light, float p_energy);
	void canvas_light_set_z_range(RID p_light, int p_min_z, int p_max_z);
	void canvas_light_set_layer_range(RID p_light, int p_min_layer, int p_max_layer);
	void canvas_light_set_item_cull_mask(RID p_light, int p_mask);
	void canvas_light_set_item_shadow_cull_mask(RID p_light, int p_mask);
	void canvas_light_set_directional_distance(RID p_light, float p_distance);

	void canvas_light_set_blend_mode(RID p_light, RS::CanvasLightBlendMode p_mode);

	void canvas_light_set_shadow_enabled(RID p_light, bool p_enabled);
	void canvas_light_set_shadow_filter(RID p_light, RS::CanvasLightShadowFilter p_filter);
	void canvas_light_set_shadow_color(RID p_light, const Color &p_color);
	void canvas_light_set_shadow_smooth(RID p_light, float p_smooth);

	void canvas_light_set_interpolated(RID p_light, bool p_interpolated);
	void canvas_light_reset_physics_interpolation(RID p_light);
	void canvas_light_transform_physics_interpolation(RID p_light, const Transform2D &p_transform);

	RID canvas_light_occluder_allocate();
	void canvas_light_occluder_initialize(RID p_rid);

	void canvas_light_occluder_attach_to_canvas(RID p_occluder, RID p_canvas);
	void canvas_light_occluder_set_enabled(RID p_occluder, bool p_enabled);
	void canvas_light_occluder_set_polygon(RID p_occluder, RID p_polygon);
	void canvas_light_occluder_set_as_sdf_collision(RID p_occluder, bool p_enable);
	void canvas_light_occluder_set_transform(RID p_occluder, const Transform2D &p_xform);
	void canvas_light_occluder_set_light_mask(RID p_occluder, int p_mask);

	void canvas_light_occluder_set_interpolated(RID p_occluder, bool p_interpolated);
	void canvas_light_occluder_reset_physics_interpolation(RID p_occluder);
	void canvas_light_occluder_transform_physics_interpolation(RID p_occluder, const Transform2D &p_transform);

	RID canvas_occluder_polygon_allocate();
	void canvas_occluder_polygon_initialize(RID p_rid);

	void canvas_occluder_polygon_set_shape(RID p_occluder_polygon, const Vector<Vector2> &p_shape, bool p_closed);

	void canvas_occluder_polygon_set_cull_mode(RID p_occluder_polygon, RS::CanvasOccluderPolygonCullMode p_mode);

	void canvas_set_shadow_texture_size(int p_size);

	RID canvas_texture_allocate();
	void canvas_texture_initialize(RID p_rid);

	void canvas_texture_set_channel(RID p_canvas_texture, RS::CanvasTextureChannel p_channel, RID p_texture);
	void canvas_texture_set_shading_parameters(RID p_canvas_texture, const Color &p_base_color, float p_shininess);

	void canvas_texture_set_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter);
	void canvas_texture_set_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat);

	void canvas_item_set_default_texture_filter(RID p_item, RS::CanvasItemTextureFilter p_filter);
	void canvas_item_set_default_texture_repeat(RID p_item, RS::CanvasItemTextureRepeat p_repeat);

	void update_visibility_notifiers();
	void update_dirty_items();

	void _update_dirty_item(Item *p_item);

	Rect2 _debug_canvas_item_get_rect(RID p_item);

	bool free(RID p_rid);

	void finalize();

	/* INTERPOLATION */

	void tick();
	void update_interpolation_tick(bool p_process = true);
	void set_physics_interpolation_enabled(bool p_enabled) { _interpolation_data.interpolation_enabled = p_enabled; }

	struct InterpolationData {
		void notify_free_canvas_item(RID p_rid, RendererCanvasCull::Item &r_canvas_item);
		void notify_free_canvas_light(RID p_rid, RendererCanvasRender::Light &r_canvas_light);
		void notify_free_canvas_light_occluder(RID p_rid, RendererCanvasRender::LightOccluderInstance &r_canvas_light_occluder);

		LocalVector<RID> canvas_item_transform_update_lists[2];
		LocalVector<RID> *canvas_item_transform_update_list_curr = &canvas_item_transform_update_lists[0];
		LocalVector<RID> *canvas_item_transform_update_list_prev = &canvas_item_transform_update_lists[1];

		LocalVector<RID> canvas_light_transform_update_lists[2];
		LocalVector<RID> *canvas_light_transform_update_list_curr = &canvas_light_transform_update_lists[0];
		LocalVector<RID> *canvas_light_transform_update_list_prev = &canvas_light_transform_update_lists[1];

		LocalVector<RID> canvas_light_occluder_transform_update_lists[2];
		LocalVector<RID> *canvas_light_occluder_transform_update_list_curr = &canvas_light_occluder_transform_update_lists[0];
		LocalVector<RID> *canvas_light_occluder_transform_update_list_prev = &canvas_light_occluder_transform_update_lists[1];

		bool interpolation_enabled = false;
	} _interpolation_data;

	RendererCanvasCull();
	~RendererCanvasCull();
};

#endif // RENDERER_CANVAS_CULL_H
