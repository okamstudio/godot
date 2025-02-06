/**************************************************************************/
/*  world_environment.cpp                                                 */
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

#include "world_environment.h"

#include "core/config/project_settings.h"
#include "scene/3d/node_3d.h"
#include "scene/main/viewport.h"

void WorldEnvironment::_notification(int p_what) {
	switch (p_what) {
		case Node3D::NOTIFICATION_ENTER_WORLD:
		case Node3D::NOTIFICATION_ENTER_TREE: {
			_check_sky_warning();

			if (environment.is_valid()) {
				add_to_group("_world_environment_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
				_update_current_environment();
			}

			if (camera_attributes.is_valid()) {
				add_to_group("_world_camera_attributes_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
				_update_current_camera_attributes();
			}

			if (compositor.is_valid()) {
				add_to_group("_world_compositor_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
				_update_current_compositor();
			}
		} break;

		case Node3D::NOTIFICATION_EXIT_WORLD:
		case Node3D::NOTIFICATION_EXIT_TREE: {
			if (environment.is_valid()) {
				remove_from_group("_world_environment_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
				_update_current_environment();
			}

			if (camera_attributes.is_valid()) {
				remove_from_group("_world_camera_attributes_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
				_update_current_camera_attributes();
			}

			if (compositor.is_valid()) {
				remove_from_group("_world_compositor_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
				_update_current_compositor();
			}
		} break;
	}
}

void WorldEnvironment::_update_current_environment() {
	WorldEnvironment *first = Object::cast_to<WorldEnvironment>(get_tree()->get_first_node_in_group("_world_environment_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id())));

	if (first) {
		get_viewport()->find_world_3d()->set_environment(first->environment);
	} else {
		get_viewport()->find_world_3d()->set_environment(Ref<Environment>());
	}
	get_tree()->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, "_world_environment_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()), "update_configuration_warnings");
}

void WorldEnvironment::_update_current_camera_attributes() {
	WorldEnvironment *first = Object::cast_to<WorldEnvironment>(get_tree()->get_first_node_in_group("_world_camera_attributes_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id())));
	if (first) {
		get_viewport()->find_world_3d()->set_camera_attributes(first->camera_attributes);
	} else {
		get_viewport()->find_world_3d()->set_camera_attributes(Ref<CameraAttributes>());
	}

	get_tree()->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, "_world_camera_attributes_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()), "update_configuration_warnings");
}

void WorldEnvironment::_update_current_compositor() {
	WorldEnvironment *first = Object::cast_to<WorldEnvironment>(get_tree()->get_first_node_in_group("_world_compositor_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id())));
	if (first) {
		get_viewport()->find_world_3d()->set_compositor(first->compositor);
	} else {
		get_viewport()->find_world_3d()->set_compositor(Ref<Compositor>());
	}

	get_tree()->call_group_flags(SceneTree::GROUP_CALL_DEFERRED, "_world_compositor_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()), "update_configuration_warnings");
}

void WorldEnvironment::set_environment(const Ref<Environment> &p_environment) {
	if (environment == p_environment) {
		return;
	}
	if (environment.is_valid()) {
		environment->disconnect(StringName("sky_changed"), callable_mp(this, &WorldEnvironment::_check_sky_warning));
		if (is_inside_tree()) {
			remove_from_group("_world_environment_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
		}
	}

	environment = p_environment;

	if (environment.is_valid()) {
		environment->connect(StringName("sky_changed"), callable_mp(this, &WorldEnvironment::_check_sky_warning));
		if (is_inside_tree()) {
			add_to_group("_world_environment_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
		}
	}

	if (is_inside_tree()) {
		_update_current_environment();
	} else {
		update_configuration_warnings();
	}
}

Ref<Environment> WorldEnvironment::get_environment() const {
	return environment;
}

void WorldEnvironment::set_camera_attributes(const Ref<CameraAttributes> &p_camera_attributes) {
	if (camera_attributes == p_camera_attributes) {
		return;
	}

	if (is_inside_tree() && camera_attributes.is_valid() && get_viewport()->find_world_3d()->get_camera_attributes() == camera_attributes) {
		remove_from_group("_world_camera_attributes_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
	}

	camera_attributes = p_camera_attributes;
	if (is_inside_tree() && camera_attributes.is_valid()) {
		add_to_group("_world_camera_attributes_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
	}

	if (is_inside_tree()) {
		_update_current_camera_attributes();
	} else {
		update_configuration_warnings();
	}
}

Ref<CameraAttributes> WorldEnvironment::get_camera_attributes() const {
	return camera_attributes;
}

void WorldEnvironment::set_compositor(const Ref<Compositor> &p_compositor) {
	if (compositor == p_compositor) {
		return;
	}
	if (is_inside_tree() && compositor.is_valid()) {
		remove_from_group("_world_compositor_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
	}

	compositor = p_compositor;

	if (is_inside_tree() && compositor.is_valid()) {
		add_to_group("_world_compositor_" + itos(get_viewport()->find_world_3d()->get_scenario().get_id()));
	}

	if (is_inside_tree()) {
		_update_current_compositor();
	} else {
		update_configuration_warnings();
	}
}

Ref<Compositor> WorldEnvironment::get_compositor() const {
	return compositor;
}

PackedStringArray WorldEnvironment::get_configuration_warnings() const {
	PackedStringArray warnings = Node::get_configuration_warnings();

	if (environment.is_null() && camera_attributes.is_null()) {
		warnings.push_back(RTR("To have any visible effect, WorldEnvironment requires its \"Environment\" property to contain an Environment, its \"Camera Attributes\" property to contain a CameraAttributes resource, or both."));
	}

	if (!is_inside_tree()) {
		return warnings;
	}

	if (environment.is_valid() && get_viewport()->find_world_3d()->get_environment() != environment) {
		warnings.push_back(("Only the first Environment has an effect in a scene (or set of instantiated scenes)."));
	}

	if (camera_attributes.is_valid() && get_viewport()->find_world_3d()->get_camera_attributes() != camera_attributes) {
		warnings.push_back(RTR("Only one WorldEnvironment is allowed per scene (or set of instantiated scenes)."));
	}

	if (compositor.is_valid() && get_viewport()->find_world_3d()->get_compositor() != compositor) {
		warnings.push_back(("Only the first Compositor has an effect in a scene (or set of instantiated scenes)."));
	}

	return warnings;
}

void WorldEnvironment::_check_sky_warning() {
	if ((environment.is_valid() && !environment->get_sky().is_valid()) || !is_inside_tree()) {
		return;
	}
	Viewport *viewport = get_viewport();
	if (viewport->get_parent_viewport() == get_tree()->get_root() || String(get_name()).begins_with("@")) {
		if (GLOBAL_GET("rendering/viewport/transparent_background")) {
			WARN_PRINT("Environment sky will not render when transparent background is active in project settings.");
		}
	} else if (viewport->has_transparent_background()) {
		WARN_PRINT("Environment sky will not render when transparent background is active in subviewport.");
	}
}

void WorldEnvironment::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_environment", "env"), &WorldEnvironment::set_environment);
	ClassDB::bind_method(D_METHOD("get_environment"), &WorldEnvironment::get_environment);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "environment", PROPERTY_HINT_RESOURCE_TYPE, "Environment"), "set_environment", "get_environment");

	ClassDB::bind_method(D_METHOD("set_camera_attributes", "camera_attributes"), &WorldEnvironment::set_camera_attributes);
	ClassDB::bind_method(D_METHOD("get_camera_attributes"), &WorldEnvironment::get_camera_attributes);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "camera_attributes", PROPERTY_HINT_RESOURCE_TYPE, "CameraAttributesPractical,CameraAttributesPhysical"), "set_camera_attributes", "get_camera_attributes");

	ClassDB::bind_method(D_METHOD("set_compositor", "compositor"), &WorldEnvironment::set_compositor);
	ClassDB::bind_method(D_METHOD("get_compositor"), &WorldEnvironment::get_compositor);
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "compositor", PROPERTY_HINT_RESOURCE_TYPE, "Compositor"), "set_compositor", "get_compositor");
}

WorldEnvironment::WorldEnvironment() {
}

WorldEnvironment::~WorldEnvironment() {
	if (environment.is_valid() && !environment->get_sky().is_valid()) {
		environment->disconnect(StringName("sky_changed"), callable_mp(this, &WorldEnvironment::_check_sky_warning));
	}
}
