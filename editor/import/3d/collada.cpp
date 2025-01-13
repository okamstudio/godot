/**************************************************************************/
/*  collada.cpp                                                           */
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

#include "collada.h"

#include <stdio.h>

//#define DEBUG_DEFAULT_ANIMATION
//#define DEBUG_COLLADA
#ifdef DEBUG_COLLADA
#define COLLADA_PRINT(m_what) print_line(m_what)
#else
#define COLLADA_PRINT(m_what)
#endif

#define COLLADA_IMPORT_SCALE_SCENE

/* HELPERS */

String Collada::Effect::get_texture_path(const String &p_source, Collada &p_state) const {
	const String &image = p_source;
	ERR_FAIL_COND_V(!p_state.state.image_map.has(image), "");
	return p_state.state.image_map[image].path;
}

Transform3D Collada::get_root_transform() const {
	Transform3D unit_scale_transform;
#ifndef COLLADA_IMPORT_SCALE_SCENE
	unit_scale_transform.scale(Vector3(state.unit_scale, state.unit_scale, state.unit_scale));
#endif
	return unit_scale_transform;
}

void Collada::Vertex::fix_unit_scale(const Collada &p_state) {
#ifdef COLLADA_IMPORT_SCALE_SCENE
	vertex *= p_state.state.unit_scale;
#endif
}

static String _uri_to_id(const String &p_uri) {
	if (p_uri.begins_with("#")) {
		return p_uri.substr(1, p_uri.size() - 1);
	} else {
		return p_uri;
	}
}

/** HELPER FUNCTIONS **/

Transform3D Collada::fix_transform(const Transform3D &p_transform) {
	Transform3D tr = p_transform;

#ifndef NO_UP_AXIS_SWAP

	if (state.up_axis != Vector3::AXIS_Y) {
		for (int i = 0; i < 3; i++) {
			SWAP(tr.basis[1][i], tr.basis[state.up_axis][i]);
		}
		for (int i = 0; i < 3; i++) {
			SWAP(tr.basis[i][1], tr.basis[i][state.up_axis]);
		}

		SWAP(tr.origin[1], tr.origin[state.up_axis]);

		tr.basis[state.up_axis][0] = -tr.basis[state.up_axis][0];
		tr.basis[state.up_axis][1] = -tr.basis[state.up_axis][1];
		tr.basis[0][state.up_axis] = -tr.basis[0][state.up_axis];
		tr.basis[1][state.up_axis] = -tr.basis[1][state.up_axis];
		tr.origin[state.up_axis] = -tr.origin[state.up_axis];
	}
#endif

	//tr.scale(Vector3(state.unit_scale.unit_scale.unit_scale));
	return tr;
	//return state.matrix_fix * p_transform;
}

static Transform3D _read_transform_from_array(const Vector<float> &p_array, int p_ofs = 0) {
	Transform3D tr;
	// i wonder why collada matrices are transposed, given that's opposed to opengl..
	tr.basis.rows[0][0] = p_array[0 + p_ofs];
	tr.basis.rows[0][1] = p_array[1 + p_ofs];
	tr.basis.rows[0][2] = p_array[2 + p_ofs];
	tr.basis.rows[1][0] = p_array[4 + p_ofs];
	tr.basis.rows[1][1] = p_array[5 + p_ofs];
	tr.basis.rows[1][2] = p_array[6 + p_ofs];
	tr.basis.rows[2][0] = p_array[8 + p_ofs];
	tr.basis.rows[2][1] = p_array[9 + p_ofs];
	tr.basis.rows[2][2] = p_array[10 + p_ofs];
	tr.origin.x = p_array[3 + p_ofs];
	tr.origin.y = p_array[7 + p_ofs];
	tr.origin.z = p_array[11 + p_ofs];
	return tr;
}

/* STRUCTURES */

Transform3D Collada::Node::compute_transform(const Collada &p_state) const {
	Transform3D xform;

	for (int i = 0; i < xform_list.size(); i++) {
		Transform3D xform_step;
		const XForm &xf = xform_list[i];
		switch (xf.op) {
			case XForm::OP_ROTATE: {
				if (xf.data.size() >= 4) {
					xform_step.rotate(Vector3(xf.data[0], xf.data[1], xf.data[2]), Math::deg_to_rad(xf.data[3]));
				}
			} break;
			case XForm::OP_SCALE: {
				if (xf.data.size() >= 3) {
					xform_step.scale(Vector3(xf.data[0], xf.data[1], xf.data[2]));
				}

			} break;
			case XForm::OP_TRANSLATE: {
				if (xf.data.size() >= 3) {
					xform_step.origin = Vector3(xf.data[0], xf.data[1], xf.data[2]);
				}

			} break;
			case XForm::OP_MATRIX: {
				if (xf.data.size() >= 16) {
					xform_step = _read_transform_from_array(xf.data, 0);
				}

			} break;
			default: {
			}
		}

		xform = xform * xform_step;
	}

#ifdef COLLADA_IMPORT_SCALE_SCENE
	xform.origin *= p_state.state.unit_scale;
#endif
	return xform;
}

Transform3D Collada::Node::get_transform() const {
	return default_transform;
}

Transform3D Collada::Node::get_global_transform() const {
	if (parent) {
		return parent->get_global_transform() * default_transform;
	} else {
		return default_transform;
	}
}

Vector<float> Collada::AnimationTrack::get_value_at_time(float p_time) const {
	ERR_FAIL_COND_V(keys.is_empty(), Vector<float>());
	int i = 0;

	for (i = 0; i < keys.size(); i++) {
		if (keys[i].time > p_time) {
			break;
		}
	}

	if (i == 0) {
		return keys[0].data;
	}
	if (i == keys.size()) {
		return keys[keys.size() - 1].data;
	}

	switch (keys[i].interp_type) {
		case INTERP_BEZIER: //wait for bezier
		case INTERP_LINEAR: {
			float c = (p_time - keys[i - 1].time) / (keys[i].time - keys[i - 1].time);

			if (keys[i].data.size() == 16) {
				//interpolate a matrix
				Transform3D src = _read_transform_from_array(keys[i - 1].data);
				Transform3D dst = _read_transform_from_array(keys[i].data);

				Transform3D interp = c < 0.001 ? src : src.interpolate_with(dst, c);

				Vector<float> ret;
				ret.resize(16);
				Transform3D tr;
				// i wonder why collada matrices are transposed, given that's opposed to opengl..
				ret.write[0] = interp.basis.rows[0][0];
				ret.write[1] = interp.basis.rows[0][1];
				ret.write[2] = interp.basis.rows[0][2];
				ret.write[4] = interp.basis.rows[1][0];
				ret.write[5] = interp.basis.rows[1][1];
				ret.write[6] = interp.basis.rows[1][2];
				ret.write[8] = interp.basis.rows[2][0];
				ret.write[9] = interp.basis.rows[2][1];
				ret.write[10] = interp.basis.rows[2][2];
				ret.write[3] = interp.origin.x;
				ret.write[7] = interp.origin.y;
				ret.write[11] = interp.origin.z;
				ret.write[12] = 0;
				ret.write[13] = 0;
				ret.write[14] = 0;
				ret.write[15] = 1;

				return ret;
			} else {
				Vector<float> dest;
				dest.resize(keys[i].data.size());
				for (int j = 0; j < dest.size(); j++) {
					dest.write[j] = keys[i].data[j] * c + keys[i - 1].data[j] * (1.0 - c);
				}
				return dest;
				//interpolate one by one
			}
		} break;
	}

	ERR_FAIL_V(Vector<float>());
}

void Collada::_parse_asset(XMLParser &p_parser) {
	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String name = p_parser.get_node_name();

			if (name == "up_axis") {
				p_parser.read();
				if (p_parser.get_node_data() == "X_UP") {
					state.up_axis = Vector3::AXIS_X;
				}
				if (p_parser.get_node_data() == "Y_UP") {
					state.up_axis = Vector3::AXIS_Y;
				}
				if (p_parser.get_node_data() == "Z_UP") {
					state.up_axis = Vector3::AXIS_Z;
				}

				COLLADA_PRINT("up axis: " + p_parser.get_node_data());
			} else if (name == "unit") {
				state.unit_scale = p_parser.get_named_attribute_value("meter").to_float();
				COLLADA_PRINT("unit scale: " + rtos(state.unit_scale));
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "asset") {
			break; //end of <asset>
		}
	}
}

void Collada::_parse_image(XMLParser &p_parser) {
	String id = p_parser.get_named_attribute_value("id");

	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	Image image;

	if (state.version < State::Version(1, 4, 0)) {
		/* <1.4 */
		String path = p_parser.get_named_attribute_value("source").strip_edges();
		if (!path.contains("://") && path.is_relative_path()) {
			// path is relative to file being loaded, so convert to a resource path
			image.path = ProjectSettings::get_singleton()->localize_path(state.local_path.get_base_dir().path_join(path.uri_decode()));
		}
	} else {
		while (p_parser.read() == OK) {
			if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
				String name = p_parser.get_node_name();

				if (name == "init_from") {
					p_parser.read();
					String path = p_parser.get_node_data().strip_edges().uri_decode();

					if (!path.contains("://") && path.is_relative_path()) {
						// path is relative to file being loaded, so convert to a resource path
						path = ProjectSettings::get_singleton()->localize_path(state.local_path.get_base_dir().path_join(path));

					} else if (path.find("file:///") == 0) {
						path = path.replace_first("file:///", "");
						path = ProjectSettings::get_singleton()->localize_path(path);
					}

					image.path = path;

				} else if (name == "data") {
					ERR_PRINT("COLLADA Embedded image data not supported!");

				} else if (name == "extra" && !p_parser.is_empty()) {
					p_parser.skip_section();
				}

			} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "image") {
				break; //end of <asset>
			}
		}
	}

	state.image_map[id] = image;
}

void Collada::_parse_material(XMLParser &p_parser) {
	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	Material material;

	String id = p_parser.get_named_attribute_value("id");
	if (p_parser.has_attribute("name")) {
		material.name = p_parser.get_named_attribute_value("name");
	}

	if (state.version < State::Version(1, 4, 0)) {
		/* <1.4 */
		ERR_PRINT("Collada Materials < 1.4 are not supported (yet)");
	} else {
		while (p_parser.read() == OK) {
			if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT && p_parser.get_node_name() == "instance_effect") {
				material.instance_effect = _uri_to_id(p_parser.get_named_attribute_value("url"));
			} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "material") {
				break; //end of <asset>
			}
		}
	}

	state.material_map[id] = material;
}

//! reads floats from inside of xml element until end of xml element
Vector<float> Collada::_read_float_array(XMLParser &p_parser) {
	if (p_parser.is_empty()) {
		return Vector<float>();
	}

	Vector<String> splitters;
	splitters.push_back(" ");
	splitters.push_back("\n");
	splitters.push_back("\r");
	splitters.push_back("\t");

	Vector<float> array;
	while (p_parser.read() == OK) {
		// TODO: check for comments inside the element
		// and ignore them.

		if (p_parser.get_node_type() == XMLParser::NODE_TEXT) {
			// parse float data
			String str = p_parser.get_node_data();
			array = str.split_floats_mk(splitters, false);
			//array=str.split_floats(" ",false);
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END) {
			break; // end parsing text
		}
	}

	return array;
}

Vector<String> Collada::_read_string_array(XMLParser &p_parser) {
	if (p_parser.is_empty()) {
		return Vector<String>();
	}

	Vector<String> array;
	while (p_parser.read() == OK) {
		// TODO: check for comments inside the element
		// and ignore them.

		if (p_parser.get_node_type() == XMLParser::NODE_TEXT) {
			// parse String data
			String str = p_parser.get_node_data();
			array = str.split_spaces();
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END) {
			break; // end parsing text
		}
	}

	return array;
}

Transform3D Collada::_read_transform(XMLParser &p_parser) {
	if (p_parser.is_empty()) {
		return Transform3D();
	}

	Vector<String> array;
	while (p_parser.read() == OK) {
		// TODO: check for comments inside the element
		// and ignore them.

		if (p_parser.get_node_type() == XMLParser::NODE_TEXT) {
			// parse float data
			String str = p_parser.get_node_data();
			array = str.split_spaces();
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END) {
			break; // end parsing text
		}
	}

	ERR_FAIL_COND_V(array.size() != 16, Transform3D());
	Vector<float> farr;
	farr.resize(16);
	for (int i = 0; i < 16; i++) {
		farr.write[i] = array[i].to_float();
	}

	return _read_transform_from_array(farr);
}

String Collada::_read_empty_draw_type(XMLParser &p_parser) {
	String empty_draw_type = "";

	if (p_parser.is_empty()) {
		return empty_draw_type;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_TEXT) {
			empty_draw_type = p_parser.get_node_data();
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END) {
			break; // end parsing text
		}
	}
	return empty_draw_type;
}

Variant Collada::_parse_param(XMLParser &p_parser) {
	if (p_parser.is_empty()) {
		return Variant();
	}

	String from = p_parser.get_node_name();
	Variant data;

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			if (p_parser.get_node_name() == "float") {
				p_parser.read();
				if (p_parser.get_node_type() == XMLParser::NODE_TEXT) {
					data = p_parser.get_node_data().to_float();
				}
			} else if (p_parser.get_node_name() == "float2") {
				Vector<float> v2 = _read_float_array(p_parser);

				if (v2.size() >= 2) {
					data = Vector2(v2[0], v2[1]);
				}
			} else if (p_parser.get_node_name() == "float3") {
				Vector<float> v3 = _read_float_array(p_parser);

				if (v3.size() >= 3) {
					data = Vector3(v3[0], v3[1], v3[2]);
				}
			} else if (p_parser.get_node_name() == "float4") {
				Vector<float> v4 = _read_float_array(p_parser);

				if (v4.size() >= 4) {
					data = Color(v4[0], v4[1], v4[2], v4[3]);
				}
			} else if (p_parser.get_node_name() == "sampler2D") {
				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "source") {
							p_parser.read();

							if (p_parser.get_node_type() == XMLParser::NODE_TEXT) {
								data = p_parser.get_node_data();
							}
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "sampler2D") {
						break;
					}
				}
			} else if (p_parser.get_node_name() == "surface") {
				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "init_from") {
							p_parser.read();

							if (p_parser.get_node_type() == XMLParser::NODE_TEXT) {
								data = p_parser.get_node_data();
							}
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "surface") {
						break;
					}
				}
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == from) {
			break;
		}
	}

	COLLADA_PRINT("newparam ending " + p_parser.get_node_name());
	return data;
}

void Collada::_parse_effect_material(XMLParser &p_parser, Effect &p_effect, String &p_id) {
	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			// first come the tags we descend, but ignore the top-levels

			COLLADA_PRINT("node name: " + p_parser.get_node_name());

			if (!p_parser.is_empty() &&
					(p_parser.get_node_name() == "profile_COMMON" ||
							p_parser.get_node_name() == "technique" ||
							p_parser.get_node_name() == "extra")) {
				_parse_effect_material(p_parser, p_effect, p_id); // try again

			} else if (p_parser.get_node_name() == "newparam") {
				String name = p_parser.get_named_attribute_value("sid");
				Variant value = _parse_param(p_parser);
				p_effect.params[name] = value;
				COLLADA_PRINT("param: " + name + " value:" + String(value));

			} else if (p_parser.get_node_name() == "constant" ||
					p_parser.get_node_name() == "lambert" ||
					p_parser.get_node_name() == "phong" ||
					p_parser.get_node_name() == "blinn") {
				COLLADA_PRINT("shade model: " + p_parser.get_node_name());
				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						String what = p_parser.get_node_name();

						if (what == "emission" ||
								what == "diffuse" ||
								what == "specular" ||
								what == "reflective") {
							// color or texture types
							while (p_parser.read() == OK) {
								if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
									if (p_parser.get_node_name() == "color") {
										Vector<float> colorarr = _read_float_array(p_parser);
										COLLADA_PRINT("colorarr size: " + rtos(colorarr.size()));

										if (colorarr.size() >= 3) {
											// alpha strangely not alright? maybe it needs to be multiplied by value as a channel intensity
											Color color(colorarr[0], colorarr[1], colorarr[2], 1.0);
											if (what == "diffuse") {
												p_effect.diffuse.color = color;
											}
											if (what == "specular") {
												p_effect.specular.color = color;
											}
											if (what == "emission") {
												p_effect.emission.color = color;
											}

											COLLADA_PRINT(what + " color: " + color);
										}

									} else if (p_parser.get_node_name() == "texture") {
										String sampler = p_parser.get_named_attribute_value("texture");
										if (!p_effect.params.has(sampler)) {
											ERR_PRINT(String("Couldn't find sampler: " + sampler + " in material:" + p_id).utf8().get_data());
										} else {
											String surface = p_effect.params[sampler];

											if (!p_effect.params.has(surface)) {
												ERR_PRINT(String("Couldn't find surface: " + surface + " in material:" + p_id).utf8().get_data());
											} else {
												String uri = p_effect.params[surface];

												if (what == "diffuse") {
													p_effect.diffuse.texture = uri;
												} else if (what == "specular") {
													p_effect.specular.texture = uri;
												} else if (what == "emission") {
													p_effect.emission.texture = uri;
												} else if (what == "bump") {
													if (p_parser.has_attribute("bumptype") && p_parser.get_named_attribute_value("bumptype") != "NORMALMAP") {
														WARN_PRINT("'bump' texture type is not NORMALMAP, only NORMALMAP is supported.");
													}

													p_effect.bump.texture = uri;
												}

												COLLADA_PRINT(what + " texture: " + uri);
											}
										}
									} else if (!p_parser.is_empty()) {
										p_parser.skip_section();
									}

								} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == what) {
									break;
								}
							}

						} else if (what == "shininess") {
							p_effect.shininess = _parse_param(p_parser);
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END &&
							(p_parser.get_node_name() == "constant" ||
									p_parser.get_node_name() == "lambert" ||
									p_parser.get_node_name() == "phong" ||
									p_parser.get_node_name() == "blinn")) {
						break;
					}
				}
			} else if (p_parser.get_node_name() == "double_sided" || p_parser.get_node_name() == "show_double_sided") { // colladamax / google earth

				// 3DS Max / Google Earth double sided extension
				p_parser.read();
				p_effect.found_double_sided = true;
				p_effect.double_sided = p_parser.get_node_data().to_int();
				COLLADA_PRINT("double sided: " + itos(p_parser.get_node_data().to_int()));
			} else if (p_parser.get_node_name() == "unshaded") {
				p_parser.read();
				p_effect.unshaded = p_parser.get_node_data().to_int();
			} else if (p_parser.get_node_name() == "bump") {
				// color or texture types
				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "texture") {
							String sampler = p_parser.get_named_attribute_value("texture");
							if (!p_effect.params.has(sampler)) {
								ERR_PRINT(String("Couldn't find sampler: " + sampler + " in material:" + p_id).utf8().get_data());
							} else {
								String surface = p_effect.params[sampler];

								if (!p_effect.params.has(surface)) {
									ERR_PRINT(String("Couldn't find surface: " + surface + " in material:" + p_id).utf8().get_data());
								} else {
									String uri = p_effect.params[surface];

									if (p_parser.has_attribute("bumptype") && p_parser.get_named_attribute_value("bumptype") != "NORMALMAP") {
										WARN_PRINT("'bump' texture type is not NORMALMAP, only NORMALMAP is supported.");
									}

									p_effect.bump.texture = uri;
									COLLADA_PRINT(" bump: " + uri);
								}
							}
						} else if (!p_parser.is_empty()) {
							p_parser.skip_section();
						}

					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "bump") {
						break;
					}
				}

			} else if (!p_parser.is_empty()) {
				p_parser.skip_section();
			}
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END &&
				(p_parser.get_node_name() == "effect" ||
						p_parser.get_node_name() == "profile_COMMON" ||
						p_parser.get_node_name() == "technique" ||
						p_parser.get_node_name() == "extra")) {
			break;
		}
	}
}

void Collada::_parse_effect(XMLParser &p_parser) {
	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	String id = p_parser.get_named_attribute_value("id");

	Effect effect;
	if (p_parser.has_attribute("name")) {
		effect.name = p_parser.get_named_attribute_value("name");
	}
	_parse_effect_material(p_parser, effect, id);

	state.effect_map[id] = effect;

	COLLADA_PRINT("Effect ID:" + id);
}

void Collada::_parse_camera(XMLParser &p_parser) {
	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	String id = p_parser.get_named_attribute_value("id");

	state.camera_data_map[id] = CameraData();
	CameraData &camera = state.camera_data_map[id];

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String name = p_parser.get_node_name();

			if (name == "perspective") {
				camera.mode = CameraData::MODE_PERSPECTIVE;
			} else if (name == "orthographic") {
				camera.mode = CameraData::MODE_ORTHOGONAL;
			} else if (name == "xfov") {
				p_parser.read();
				camera.perspective.x_fov = p_parser.get_node_data().to_float();

			} else if (name == "yfov") {
				p_parser.read();
				camera.perspective.y_fov = p_parser.get_node_data().to_float();
			} else if (name == "xmag") {
				p_parser.read();
				camera.orthogonal.x_mag = p_parser.get_node_data().to_float();

			} else if (name == "ymag") {
				p_parser.read();
				camera.orthogonal.y_mag = p_parser.get_node_data().to_float();
			} else if (name == "aspect_ratio") {
				p_parser.read();
				camera.aspect = p_parser.get_node_data().to_float();

			} else if (name == "znear") {
				p_parser.read();
				camera.z_near = p_parser.get_node_data().to_float();

			} else if (name == "zfar") {
				p_parser.read();
				camera.z_far = p_parser.get_node_data().to_float();
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "camera") {
			break; //end of <asset>
		}
	}

	COLLADA_PRINT("Camera ID:" + id);
}

void Collada::_parse_light(XMLParser &p_parser) {
	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	String id = p_parser.get_named_attribute_value("id");

	state.light_data_map[id] = LightData();
	LightData &light = state.light_data_map[id];

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String name = p_parser.get_node_name();

			if (name == "ambient") {
				light.mode = LightData::MODE_AMBIENT;
			} else if (name == "directional") {
				light.mode = LightData::MODE_DIRECTIONAL;
			} else if (name == "point") {
				light.mode = LightData::MODE_OMNI;
			} else if (name == "spot") {
				light.mode = LightData::MODE_SPOT;
			} else if (name == "color") {
				p_parser.read();
				Vector<float> colorarr = _read_float_array(p_parser);
				COLLADA_PRINT("colorarr size: " + rtos(colorarr.size()));

				if (colorarr.size() >= 4) {
					// alpha strangely not alright? maybe it needs to be multiplied by value as a channel intensity
					Color color(colorarr[0], colorarr[1], colorarr[2], 1.0);
					light.color = color;
				}

			} else if (name == "constant_attenuation") {
				p_parser.read();
				light.constant_att = p_parser.get_node_data().to_float();
			} else if (name == "linear_attenuation") {
				p_parser.read();
				light.linear_att = p_parser.get_node_data().to_float();
			} else if (name == "quadratic_attenuation") {
				p_parser.read();
				light.quad_att = p_parser.get_node_data().to_float();
			} else if (name == "falloff_angle") {
				p_parser.read();
				light.spot_angle = p_parser.get_node_data().to_float();

			} else if (name == "falloff_exponent") {
				p_parser.read();
				light.spot_exp = p_parser.get_node_data().to_float();
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "light") {
			break; //end of <asset>
		}
	}

	COLLADA_PRINT("Light ID:" + id);
}

void Collada::_parse_curve_geometry(XMLParser &p_parser, const String &p_id, const String &p_name) {
	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	//load everything into a pre dictionary

	state.curve_data_map[p_id] = CurveData();

	CurveData &curvedata = state.curve_data_map[p_id];
	curvedata.name = p_name;
	String closed = p_parser.get_named_attribute_value_safe("closed").to_lower();
	curvedata.closed = closed == "true" || closed == "1";

	COLLADA_PRINT("curve name: " + p_name);

	String current_source;
	// handles geometry node and the curve children in this loop
	// read sources with arrays and accessor for each curve
	if (p_parser.is_empty()) {
		return;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String section = p_parser.get_node_name();

			if (section == "source") {
				String id = p_parser.get_named_attribute_value("id");
				curvedata.sources[id] = CurveData::Source();
				current_source = id;
				COLLADA_PRINT("source data: " + id);

			} else if (section == "float_array" || section == "array") {
				// create a new array and read it.
				if (curvedata.sources.has(current_source)) {
					curvedata.sources[current_source].array = _read_float_array(p_parser);
					COLLADA_PRINT("section: " + current_source + " read " + itos(curvedata.sources[current_source].array.size()) + " values.");
				}
			} else if (section == "Name_array") {
				// create a new array and read it.
				if (curvedata.sources.has(current_source)) {
					curvedata.sources[current_source].sarray = _read_string_array(p_parser);
					COLLADA_PRINT("section: " + current_source + " read " + itos(curvedata.sources[current_source].array.size()) + " values.");
				}

			} else if (section == "technique_common") {
				//skip it
			} else if (section == "accessor") { // child of source (below a technique tag)

				if (curvedata.sources.has(current_source)) {
					curvedata.sources[current_source].stride = p_parser.get_named_attribute_value("stride").to_int();
					COLLADA_PRINT("section: " + current_source + " stride " + itos(curvedata.sources[current_source].stride));
				}
			} else if (section == "control_vertices") {
				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "input") {
							String semantic = p_parser.get_named_attribute_value("semantic");
							String source = _uri_to_id(p_parser.get_named_attribute_value("source"));

							curvedata.control_vertices[semantic] = source;

							COLLADA_PRINT(section + " input semantic: " + semantic + " source: " + source);
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == section) {
						break;
					}
				}

			} else if (!p_parser.is_empty()) {
				p_parser.skip_section();
			}
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "spline") {
			break;
		}
	}
}

void Collada::_parse_mesh_geometry(XMLParser &p_parser, const String &p_id, const String &p_name) {
	if (!(state.import_flags & IMPORT_FLAG_SCENE)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}
		return;
	}

	//load everything into a pre dictionary

	state.mesh_data_map[p_id] = MeshData();

	MeshData &meshdata = state.mesh_data_map[p_id];
	meshdata.name = p_name;

	COLLADA_PRINT("mesh name: " + p_name);

	String current_source;
	// handles geometry node and the mesh children in this loop
	// read sources with arrays and accessor for each mesh
	if (p_parser.is_empty()) {
		return;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String section = p_parser.get_node_name();

			if (section == "source") {
				String id = p_parser.get_named_attribute_value("id");
				meshdata.sources[id] = MeshData::Source();
				current_source = id;
				COLLADA_PRINT("source data: " + id);

			} else if (section == "float_array" || section == "array") {
				// create a new array and read it.
				if (meshdata.sources.has(current_source)) {
					meshdata.sources[current_source].array = _read_float_array(p_parser);
					COLLADA_PRINT("section: " + current_source + " read " + itos(meshdata.sources[current_source].array.size()) + " values.");
				}
			} else if (section == "technique_common") {
				//skip it
			} else if (section == "accessor") { // child of source (below a technique tag)

				if (meshdata.sources.has(current_source)) {
					meshdata.sources[current_source].stride = p_parser.get_named_attribute_value("stride").to_int();
					COLLADA_PRINT("section: " + current_source + " stride " + itos(meshdata.sources[current_source].stride));
				}
			} else if (section == "vertices") {
				MeshData::Vertices vert;
				String id = p_parser.get_named_attribute_value("id");
				int last_ref = 0;

				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "input") {
							String semantic = p_parser.get_named_attribute_value("semantic");
							String source = _uri_to_id(p_parser.get_named_attribute_value("source"));

							if (semantic == "TEXCOORD") {
								semantic = "TEXCOORD" + itos(last_ref++);
							}

							vert.sources[semantic] = source;

							COLLADA_PRINT(section + " input semantic: " + semantic + " source: " + source);
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == section) {
						break;
					}
				}

				meshdata.vertices[id] = vert;

			} else if (section == "triangles" || section == "polylist" || section == "polygons") {
				bool polygons = (section == "polygons");
				if (polygons) {
					WARN_PRINT("Primitive type \"polygons\" is not well supported (concave shapes may fail). To ensure that the geometry is properly imported, please re-export using \"triangles\" or \"polylist\".");
				}
				MeshData::Primitives prim;

				if (p_parser.has_attribute("material")) {
					prim.material = p_parser.get_named_attribute_value("material");
				}
				prim.count = p_parser.get_named_attribute_value("count").to_int();
				prim.vertex_size = 0;
				int last_ref = 0;

				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "input") {
							String semantic = p_parser.get_named_attribute_value("semantic");
							String source = _uri_to_id(p_parser.get_named_attribute_value("source"));

							if (semantic == "TEXCOORD") {
								semantic = "TEXCOORD" + itos(last_ref++);
							}
							int offset = p_parser.get_named_attribute_value("offset").to_int();

							MeshData::Primitives::SourceRef sref;
							sref.source = source;
							sref.offset = offset;
							prim.sources[semantic] = sref;
							prim.vertex_size = MAX(prim.vertex_size, offset + 1);

							COLLADA_PRINT(section + " input semantic: " + semantic + " source: " + source + " offset: " + itos(offset));

						} else if (p_parser.get_node_name() == "p") { //indices

							Vector<float> values = _read_float_array(p_parser);
							if (polygons) {
								ERR_CONTINUE(prim.vertex_size == 0);
								prim.polygons.push_back(values.size() / prim.vertex_size);
								int from = prim.indices.size();
								prim.indices.resize(from + values.size());
								for (int i = 0; i < values.size(); i++) {
									prim.indices.write[from + i] = values[i];
								}

							} else if (prim.vertex_size > 0) {
								prim.indices = values;
							}

							COLLADA_PRINT("read " + itos(values.size()) + " index values");

						} else if (p_parser.get_node_name() == "vcount") { // primitive

							Vector<float> values = _read_float_array(p_parser);
							prim.polygons = values;
							COLLADA_PRINT("read " + itos(values.size()) + " polygon values");
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == section) {
						break;
					}
				}

				meshdata.primitives.push_back(prim);

			} else if (p_parser.get_node_name() == "double_sided") {
				p_parser.read();
				meshdata.found_double_sided = true;
				meshdata.double_sided = p_parser.get_node_data().to_int();

			} else if (p_parser.get_node_name() == "polygons") {
				ERR_PRINT("Primitive type \"polygons\" not supported, re-export using \"polylist\" or \"triangles\".");
			} else if (!p_parser.is_empty()) {
				p_parser.skip_section();
			}
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "mesh") {
			break;
		}
	}
}

void Collada::_parse_skin_controller(XMLParser &p_parser, const String &p_id) {
	state.skin_controller_data_map[p_id] = SkinControllerData();
	SkinControllerData &skindata = state.skin_controller_data_map[p_id];

	skindata.base = _uri_to_id(p_parser.get_named_attribute_value("source"));

	String current_source;

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String section = p_parser.get_node_name();

			if (section == "bind_shape_matrix") {
				skindata.bind_shape = _read_transform(p_parser);
#ifdef COLLADA_IMPORT_SCALE_SCENE
				skindata.bind_shape.origin *= state.unit_scale;

#endif
				COLLADA_PRINT("skeleton bind shape transform: " + skindata.bind_shape);

			} else if (section == "source") {
				String id = p_parser.get_named_attribute_value("id");
				skindata.sources[id] = SkinControllerData::Source();
				current_source = id;
				COLLADA_PRINT("source data: " + id);

			} else if (section == "float_array" || section == "array") {
				// create a new array and read it.
				if (skindata.sources.has(current_source)) {
					skindata.sources[current_source].array = _read_float_array(p_parser);
					COLLADA_PRINT("section: " + current_source + " read " + itos(skindata.sources[current_source].array.size()) + " values.");
				}
			} else if (section == "Name_array" || section == "IDREF_array") {
				// create a new array and read it.

				if (section == "IDREF_array") {
					skindata.use_idrefs = true;
				}
				if (skindata.sources.has(current_source)) {
					skindata.sources[current_source].sarray = _read_string_array(p_parser);
					if (section == "IDREF_array") {
						Vector<String> sa = skindata.sources[current_source].sarray;
						for (int i = 0; i < sa.size(); i++) {
							state.idref_joints.insert(sa[i]);
						}
					}
					COLLADA_PRINT("section: " + current_source + " read " + itos(skindata.sources[current_source].array.size()) + " values.");
				}
			} else if (section == "technique_common") {
				//skip it
			} else if (section == "accessor") { // child of source (below a technique tag)

				if (skindata.sources.has(current_source)) {
					int stride = 1;
					if (p_parser.has_attribute("stride")) {
						stride = p_parser.get_named_attribute_value("stride").to_int();
					}

					skindata.sources[current_source].stride = stride;
					COLLADA_PRINT("section: " + current_source + " stride " + itos(skindata.sources[current_source].stride));
				}

			} else if (section == "joints") {
				SkinControllerData::Joints joint;

				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "input") {
							String semantic = p_parser.get_named_attribute_value("semantic");
							String source = _uri_to_id(p_parser.get_named_attribute_value("source"));

							joint.sources[semantic] = source;

							COLLADA_PRINT(section + " input semantic: " + semantic + " source: " + source);
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == section) {
						break;
					}
				}

				skindata.joints = joint;

			} else if (section == "vertex_weights") {
				SkinControllerData::Weights weights;

				weights.count = p_parser.get_named_attribute_value("count").to_int();

				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "input") {
							String semantic = p_parser.get_named_attribute_value("semantic");
							String source = _uri_to_id(p_parser.get_named_attribute_value("source"));

							int offset = p_parser.get_named_attribute_value("offset").to_int();

							SkinControllerData::Weights::SourceRef sref;
							sref.source = source;
							sref.offset = offset;
							weights.sources[semantic] = sref;

							COLLADA_PRINT(section + " input semantic: " + semantic + " source: " + source + " offset: " + itos(offset));

						} else if (p_parser.get_node_name() == "v") { //indices

							Vector<float> values = _read_float_array(p_parser);
							weights.indices = values;
							COLLADA_PRINT("read " + itos(values.size()) + " index values");

						} else if (p_parser.get_node_name() == "vcount") { // weightsitive

							Vector<float> values = _read_float_array(p_parser);
							weights.sets = values;
							COLLADA_PRINT("read " + itos(values.size()) + " polygon values");
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == section) {
						break;
					}
				}

				skindata.weights = weights;
			}
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "skin") {
			break;
		}
	}

	/* STORE REST MATRICES */

	Vector<Transform3D> rests;
	ERR_FAIL_COND(!skindata.joints.sources.has("JOINT"));
	ERR_FAIL_COND(!skindata.joints.sources.has("INV_BIND_MATRIX"));

	String joint_arr = skindata.joints.sources["JOINT"];
	String ibm = skindata.joints.sources["INV_BIND_MATRIX"];

	ERR_FAIL_COND(!skindata.sources.has(joint_arr));
	ERR_FAIL_COND(!skindata.sources.has(ibm));

	SkinControllerData::Source &joint_source = skindata.sources[joint_arr];
	SkinControllerData::Source &ibm_source = skindata.sources[ibm];

	ERR_FAIL_COND(joint_source.sarray.size() != ibm_source.array.size() / 16);

	for (int i = 0; i < joint_source.sarray.size(); i++) {
		String name = joint_source.sarray[i];
		Transform3D xform = _read_transform_from_array(ibm_source.array, i * 16); //<- this is a mistake, it must be applied to vertices
		xform.affine_invert(); // inverse for rest, because it's an inverse
#ifdef COLLADA_IMPORT_SCALE_SCENE
		xform.origin *= state.unit_scale;
#endif
		skindata.bone_rest_map[name] = xform;
	}
}

void Collada::_parse_morph_controller(XMLParser &p_parser, const String &p_id) {
	state.morph_controller_data_map[p_id] = MorphControllerData();
	MorphControllerData &morphdata = state.morph_controller_data_map[p_id];

	morphdata.mesh = _uri_to_id(p_parser.get_named_attribute_value("source"));
	morphdata.mode = p_parser.get_named_attribute_value("method");
	String current_source;

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String section = p_parser.get_node_name();

			if (section == "source") {
				String id = p_parser.get_named_attribute_value("id");
				morphdata.sources[id] = MorphControllerData::Source();
				current_source = id;
				COLLADA_PRINT("source data: " + id);

			} else if (section == "float_array" || section == "array") {
				// create a new array and read it.
				if (morphdata.sources.has(current_source)) {
					morphdata.sources[current_source].array = _read_float_array(p_parser);
					COLLADA_PRINT("section: " + current_source + " read " + itos(morphdata.sources[current_source].array.size()) + " values.");
				}
			} else if (section == "Name_array" || section == "IDREF_array") {
				// create a new array and read it.
				if (morphdata.sources.has(current_source)) {
					morphdata.sources[current_source].sarray = _read_string_array(p_parser);
					COLLADA_PRINT("section: " + current_source + " read " + itos(morphdata.sources[current_source].array.size()) + " values.");
				}
			} else if (section == "technique_common") {
				//skip it
			} else if (section == "accessor") { // child of source (below a technique tag)

				if (morphdata.sources.has(current_source)) {
					int stride = 1;
					if (p_parser.has_attribute("stride")) {
						stride = p_parser.get_named_attribute_value("stride").to_int();
					}

					morphdata.sources[current_source].stride = stride;
					COLLADA_PRINT("section: " + current_source + " stride " + itos(morphdata.sources[current_source].stride));
				}

			} else if (section == "targets") {
				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "input") {
							String semantic = p_parser.get_named_attribute_value("semantic");
							String source = _uri_to_id(p_parser.get_named_attribute_value("source"));

							morphdata.targets[semantic] = source;

							COLLADA_PRINT(section + " input semantic: " + semantic + " source: " + source);
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == section) {
						break;
					}
				}
			}
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "morph") {
			break;
		}
	}

	if (morphdata.targets.has("MORPH_WEIGHT")) {
		state.morph_name_map[morphdata.targets["MORPH_WEIGHT"]] = p_id;
	}
}

void Collada::_parse_controller(XMLParser &p_parser) {
	String id = p_parser.get_named_attribute_value("id");

	if (p_parser.is_empty()) {
		return;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String section = p_parser.get_node_name();

			if (section == "skin") {
				_parse_skin_controller(p_parser, id);
			} else if (section == "morph") {
				_parse_morph_controller(p_parser, id);
			}
		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "controller") {
			break;
		}
	}
}

Collada::Node *Collada::_parse_visual_instance_geometry(XMLParser &p_parser) {
	String type = p_parser.get_node_name();
	NodeGeometry *geom = memnew(NodeGeometry);
	geom->controller = type == "instance_controller";
	geom->source = _uri_to_id(p_parser.get_named_attribute_value_safe("url"));

	if (p_parser.is_empty()) { //nothing else to parse...
		return geom;
	}
	// try to find also many materials and skeletons!
	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			if (p_parser.get_node_name() == "instance_material") {
				String symbol = p_parser.get_named_attribute_value("symbol");
				String target = _uri_to_id(p_parser.get_named_attribute_value("target"));

				NodeGeometry::Material mat;
				mat.target = target;
				geom->material_map[symbol] = mat;
				COLLADA_PRINT("uses material: '" + target + "' on primitive'" + symbol + "'");
			} else if (p_parser.get_node_name() == "skeleton") {
				p_parser.read();
				String uri = _uri_to_id(p_parser.get_node_data());
				if (!uri.is_empty()) {
					geom->skeletons.push_back(uri);
				}
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == type) {
			break;
		}
	}

	if (geom->controller) {
		if (geom->skeletons.is_empty()) {
			//XSI style

			if (state.skin_controller_data_map.has(geom->source)) {
				SkinControllerData *skin = &state.skin_controller_data_map[geom->source];
				//case where skeletons reference bones with IDREF (XSI)
				ERR_FAIL_COND_V(!skin->joints.sources.has("JOINT"), geom);
				String joint_arr = skin->joints.sources["JOINT"];
				ERR_FAIL_COND_V(!skin->sources.has(joint_arr), geom);
				Collada::SkinControllerData::Source &joint_source = skin->sources[joint_arr];
				geom->skeletons = joint_source.sarray; //quite crazy, but should work.
			}
		}
	}

	return geom;
}

Collada::Node *Collada::_parse_visual_instance_camera(XMLParser &p_parser) {
	NodeCamera *cam = memnew(NodeCamera);
	cam->camera = _uri_to_id(p_parser.get_named_attribute_value_safe("url"));

	if (state.up_axis == Vector3::AXIS_Z) { //collada weirdness
		cam->post_transform.basis.rotate(Vector3(1, 0, 0), -Math_PI * 0.5);
	}

	if (p_parser.is_empty()) { //nothing else to parse...
		return cam;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "instance_camera") {
			break;
		}
	}

	return cam;
}

Collada::Node *Collada::_parse_visual_instance_light(XMLParser &p_parser) {
	NodeLight *cam = memnew(NodeLight);
	cam->light = _uri_to_id(p_parser.get_named_attribute_value_safe("url"));

	if (state.up_axis == Vector3::AXIS_Z) { //collada weirdness
		cam->post_transform.basis.rotate(Vector3(1, 0, 0), -Math_PI * 0.5);
	}

	if (p_parser.is_empty()) { //nothing else to parse...
		return cam;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "instance_light") {
			break;
		}
	}

	return cam;
}

Collada::Node *Collada::_parse_visual_node_instance_data(XMLParser &p_parser) {
	String instance_type = p_parser.get_node_name();

	if (instance_type == "instance_geometry" || instance_type == "instance_controller") {
		return _parse_visual_instance_geometry(p_parser);
	} else if (instance_type == "instance_camera") {
		return _parse_visual_instance_camera(p_parser);
	} else if (instance_type == "instance_light") {
		return _parse_visual_instance_light(p_parser);
	}

	if (p_parser.is_empty()) { //nothing else to parse...
		return nullptr;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == instance_type) {
			break;
		}
	}

	return nullptr;
}

Collada::Node *Collada::_parse_visual_scene_node(XMLParser &p_parser) {
	String name;

	String id = p_parser.get_named_attribute_value_safe("id");

	bool found_name = false;

	if (id.is_empty()) {
		id = "%NODEID%" + itos(Math::rand());

	} else {
		found_name = true;
	}

	Vector<Node::XForm> xform_list;
	Vector<Node *> children;

	String empty_draw_type = "";

	Node *node = nullptr;

	name = p_parser.has_attribute("name") ? p_parser.get_named_attribute_value_safe("name") : p_parser.get_named_attribute_value_safe("id");
	if (name.is_empty()) {
		name = id;
	} else {
		found_name = true;
	}

	if ((p_parser.has_attribute("type") && p_parser.get_named_attribute_value("type") == "JOINT") || state.idref_joints.has(name)) {
		// handle a bone

		NodeJoint *joint = memnew(NodeJoint);

		if (p_parser.has_attribute("sid")) { //bones may not have sid
			joint->sid = p_parser.get_named_attribute_value("sid");
			//state.bone_map[joint->sid]=joint;
		} else if (state.idref_joints.has(name)) {
			joint->sid = name; //kind of a cheat but..
		} else if (p_parser.has_attribute("name")) {
			joint->sid = p_parser.get_named_attribute_value_safe("name");
		}

		if (!joint->sid.is_empty()) {
			state.sid_to_node_map[joint->sid] = id;
		}

		node = joint;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String section = p_parser.get_node_name();

			if (section == "translate") {
				Node::XForm xf;
				if (p_parser.has_attribute("sid")) {
					xf.id = p_parser.get_named_attribute_value("sid");
				}
				xf.op = Node::XForm::OP_TRANSLATE;

				Vector<float> xlt = _read_float_array(p_parser);
				xf.data = xlt;
				xform_list.push_back(xf);

			} else if (section == "rotate") {
				Node::XForm xf;
				if (p_parser.has_attribute("sid")) {
					xf.id = p_parser.get_named_attribute_value("sid");
				}
				xf.op = Node::XForm::OP_ROTATE;

				Vector<float> rot = _read_float_array(p_parser);
				xf.data = rot;

				xform_list.push_back(xf);

			} else if (section == "scale") {
				Node::XForm xf;
				if (p_parser.has_attribute("sid")) {
					xf.id = p_parser.get_named_attribute_value("sid");
				}

				xf.op = Node::XForm::OP_SCALE;

				Vector<float> scale = _read_float_array(p_parser);

				xf.data = scale;

				xform_list.push_back(xf);

			} else if (section == "matrix") {
				Node::XForm xf;
				if (p_parser.has_attribute("sid")) {
					xf.id = p_parser.get_named_attribute_value("sid");
				}
				xf.op = Node::XForm::OP_MATRIX;

				Vector<float> matrix = _read_float_array(p_parser);

				xf.data = matrix;
				String mtx;
				for (int i = 0; i < matrix.size(); i++) {
					mtx += " " + rtos(matrix[i]);
				}

				xform_list.push_back(xf);

			} else if (section == "visibility") {
				Node::XForm xf;
				if (p_parser.has_attribute("sid")) {
					xf.id = p_parser.get_named_attribute_value("sid");
				}
				xf.op = Node::XForm::OP_VISIBILITY;

				Vector<float> visible = _read_float_array(p_parser);

				xf.data = visible;

				xform_list.push_back(xf);

			} else if (section == "empty_draw_type") {
				empty_draw_type = _read_empty_draw_type(p_parser);
			} else if (section == "technique" || section == "extra") {
			} else if (section != "node") {
				//usually what defines the type of node
				if (section.begins_with("instance_")) {
					if (!node) {
						node = _parse_visual_node_instance_data(p_parser);

					} else {
						ERR_PRINT("Multiple instance_* not supported.");
					}
				}

			} else {
				/* Found a child node!! what to do..*/

				Node *child = _parse_visual_scene_node(p_parser);
				children.push_back(child);
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "node") {
			break;
		}
	}

	if (!node) {
		node = memnew(Node); //generic node, nothing of relevance found
	}

	node->noname = !found_name;
	node->xform_list = xform_list;
	node->children = children;
	for (int i = 0; i < children.size(); i++) {
		node->children[i]->parent = node;
	}

	node->name = name;
	node->id = id;
	node->empty_draw_type = empty_draw_type;

	if (node->children.size() == 1) {
		if (node->children[0]->noname && !node->noname) {
			node->children[0]->name = node->name;
			node->name = node->name + "-base";
		}
	}

	node->default_transform = node->compute_transform(*this);
	state.scene_map[id] = node;

	return node;
}

void Collada::_parse_visual_scene(XMLParser &p_parser) {
	String id = p_parser.get_named_attribute_value("id");

	if (p_parser.is_empty()) {
		return;
	}

	state.visual_scene_map[id] = VisualScene();
	VisualScene &vscene = state.visual_scene_map[id];

	if (p_parser.has_attribute("name")) {
		vscene.name = p_parser.get_named_attribute_value("name");
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String section = p_parser.get_node_name();

			if (section == "node") {
				vscene.root_nodes.push_back(_parse_visual_scene_node(p_parser));
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "visual_scene") {
			break;
		}
	}

	COLLADA_PRINT("Scene ID:" + id);
}

void Collada::_parse_animation(XMLParser &p_parser) {
	if (!(state.import_flags & IMPORT_FLAG_ANIMATION)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}

		return;
	}

	HashMap<String, Vector<float>> float_sources;
	HashMap<String, Vector<String>> string_sources;
	HashMap<String, int> source_strides;
	HashMap<String, HashMap<String, String>> samplers;
	HashMap<String, Vector<String>> source_param_names;
	HashMap<String, Vector<String>> source_param_types;

	String id = "";
	if (p_parser.has_attribute("id")) {
		id = p_parser.get_named_attribute_value("id");
	}

	String current_source;
	String current_sampler;
	Vector<String> channel_sources;
	Vector<String> channel_targets;

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String name = p_parser.get_node_name();
			if (name == "source") {
				current_source = p_parser.get_named_attribute_value("id");
				source_param_names[current_source] = Vector<String>();
				source_param_types[current_source] = Vector<String>();

			} else if (name == "float_array") {
				if (!current_source.is_empty()) {
					float_sources[current_source] = _read_float_array(p_parser);
				}

			} else if (name == "Name_array") {
				if (!current_source.is_empty()) {
					string_sources[current_source] = _read_string_array(p_parser);
				}
			} else if (name == "accessor") {
				if (!current_source.is_empty() && p_parser.has_attribute("stride")) {
					source_strides[current_source] = p_parser.get_named_attribute_value("stride").to_int();
				}
			} else if (name == "sampler") {
				current_sampler = p_parser.get_named_attribute_value("id");
				samplers[current_sampler] = HashMap<String, String>();
			} else if (name == "param") {
				if (p_parser.has_attribute("name")) {
					source_param_names[current_source].push_back(p_parser.get_named_attribute_value("name"));
				} else {
					source_param_names[current_source].push_back("");
				}

				if (p_parser.has_attribute("type")) {
					source_param_types[current_source].push_back(p_parser.get_named_attribute_value("type"));
				} else {
					source_param_types[current_source].push_back("");
				}

			} else if (name == "input") {
				if (!current_sampler.is_empty()) {
					samplers[current_sampler][p_parser.get_named_attribute_value("semantic")] = p_parser.get_named_attribute_value("source");
				}

			} else if (name == "channel") {
				channel_sources.push_back(p_parser.get_named_attribute_value("source"));
				channel_targets.push_back(p_parser.get_named_attribute_value("target"));
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "animation") {
			break; //end of <asset>
		}
	}

	for (int i = 0; i < channel_sources.size(); i++) {
		String source = _uri_to_id(channel_sources[i]);
		const String &target = channel_targets[i];
		ERR_CONTINUE(!samplers.has(source));
		HashMap<String, String> &sampler = samplers[source];

		ERR_CONTINUE(!sampler.has("INPUT")); //no input semantic? wtf?
		String input_id = _uri_to_id(sampler["INPUT"]);
		COLLADA_PRINT("input id is " + input_id);
		ERR_CONTINUE(!float_sources.has(input_id));

		ERR_CONTINUE(!sampler.has("OUTPUT"));
		String output_id = _uri_to_id(sampler["OUTPUT"]);
		ERR_CONTINUE(!float_sources.has(output_id));

		ERR_CONTINUE(!source_param_names.has(output_id));

		Vector<String> &names = source_param_names[output_id];

		for (int l = 0; l < names.size(); l++) {
			String name = names[l];

			Vector<float> &time_keys = float_sources[input_id];
			int key_count = time_keys.size();

			AnimationTrack track; //begin crating track
			track.id = id;

			track.keys.resize(key_count);

			for (int j = 0; j < key_count; j++) {
				track.keys.write[j].time = time_keys[j];
				state.animation_length = MAX(state.animation_length, time_keys[j]);
			}

			//now read actual values

			int stride = 1;

			if (source_strides.has(output_id)) {
				stride = source_strides[output_id];
			}
			int output_len = stride / names.size();

			ERR_CONTINUE(output_len == 0);
			ERR_CONTINUE(!float_sources.has(output_id));

			Vector<float> &output = float_sources[output_id];

			ERR_CONTINUE_MSG((output.size() / stride) != key_count, "Wrong number of keys in output.");

			for (int j = 0; j < key_count; j++) {
				track.keys.write[j].data.resize(output_len);
				for (int k = 0; k < output_len; k++) {
					track.keys.write[j].data.write[k] = output[l + j * stride + k]; //super weird but should work:
				}
			}

			if (sampler.has("INTERPOLATION")) {
				String interp_id = _uri_to_id(sampler["INTERPOLATION"]);
				ERR_CONTINUE(!string_sources.has(interp_id));
				Vector<String> &interps = string_sources[interp_id];
				ERR_CONTINUE(interps.size() != key_count);

				for (int j = 0; j < key_count; j++) {
					if (interps[j] == "BEZIER") {
						track.keys.write[j].interp_type = AnimationTrack::INTERP_BEZIER;
					} else {
						track.keys.write[j].interp_type = AnimationTrack::INTERP_LINEAR;
					}
				}
			}

			if (sampler.has("IN_TANGENT") && sampler.has("OUT_TANGENT")) {
				//bezier control points..
				String intangent_id = _uri_to_id(sampler["IN_TANGENT"]);
				ERR_CONTINUE(!float_sources.has(intangent_id));
				Vector<float> &intangents = float_sources[intangent_id];

				ERR_CONTINUE(intangents.size() != key_count * 2 * names.size());

				String outangent_id = _uri_to_id(sampler["OUT_TANGENT"]);
				ERR_CONTINUE(!float_sources.has(outangent_id));
				Vector<float> &outangents = float_sources[outangent_id];
				ERR_CONTINUE(outangents.size() != key_count * 2 * names.size());

				for (int j = 0; j < key_count; j++) {
					track.keys.write[j].in_tangent = Vector2(intangents[j * 2 * names.size() + 0 + l * 2], intangents[j * 2 * names.size() + 1 + l * 2]);
					track.keys.write[j].out_tangent = Vector2(outangents[j * 2 * names.size() + 0 + l * 2], outangents[j * 2 * names.size() + 1 + l * 2]);
				}
			}

			if (target.contains_char('/')) { //transform component
				track.target = target.get_slicec('/', 0);
				track.param = target.get_slicec('/', 1);
				if (track.param.contains_char('.')) {
					track.component = track.param.get_slice(".", 1).to_upper();
				}
				track.param = track.param.get_slice(".", 0);
				if (names.size() > 1 && track.component.is_empty()) {
					//this is a guess because the collada spec is ambiguous here...
					//i suppose if you have many names (outputs) you can't use a component and i should abide to that.
					track.component = name;
				}
			} else {
				track.target = target;
			}

			state.animation_tracks.push_back(track);

			if (!state.referenced_tracks.has(target)) {
				state.referenced_tracks[target] = Vector<int>();
			}

			state.referenced_tracks[target].push_back(state.animation_tracks.size() - 1);

			if (!id.is_empty()) {
				if (!state.by_id_tracks.has(id)) {
					state.by_id_tracks[id] = Vector<int>();
				}

				state.by_id_tracks[id].push_back(state.animation_tracks.size() - 1);
			}

			COLLADA_PRINT("loaded animation with " + itos(key_count) + " keys");
		}
	}
}

void Collada::_parse_animation_clip(XMLParser &p_parser) {
	if (!(state.import_flags & IMPORT_FLAG_ANIMATION)) {
		if (!p_parser.is_empty()) {
			p_parser.skip_section();
		}

		return;
	}

	AnimationClip clip;

	if (p_parser.has_attribute("name")) {
		clip.name = p_parser.get_named_attribute_value("name");
	} else if (p_parser.has_attribute("id")) {
		clip.name = p_parser.get_named_attribute_value("id");
	}
	if (p_parser.has_attribute("start")) {
		clip.begin = p_parser.get_named_attribute_value("start").to_float();
	}
	if (p_parser.has_attribute("end")) {
		clip.end = p_parser.get_named_attribute_value("end").to_float();
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String name = p_parser.get_node_name();
			if (name == "instance_animation") {
				String url = _uri_to_id(p_parser.get_named_attribute_value("url"));
				clip.tracks.push_back(url);
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "animation_clip") {
			break; //end of <asset>
		}
	}

	state.animation_clips.push_back(clip);
}

void Collada::_parse_scene(XMLParser &p_parser) {
	if (p_parser.is_empty()) {
		return;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String name = p_parser.get_node_name();

			if (name == "instance_visual_scene") {
				state.root_visual_scene = _uri_to_id(p_parser.get_named_attribute_value("url"));
			} else if (name == "instance_physics_scene") {
				state.root_physics_scene = _uri_to_id(p_parser.get_named_attribute_value("url"));
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "scene") {
			break; //end of <asset>
		}
	}
}

void Collada::_parse_library(XMLParser &p_parser) {
	if (p_parser.is_empty()) {
		return;
	}

	while (p_parser.read() == OK) {
		if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			String name = p_parser.get_node_name();
			COLLADA_PRINT("library name is: " + name);
			if (name == "image") {
				_parse_image(p_parser);
			} else if (name == "material") {
				_parse_material(p_parser);
			} else if (name == "effect") {
				_parse_effect(p_parser);
			} else if (name == "camera") {
				_parse_camera(p_parser);
			} else if (name == "light") {
				_parse_light(p_parser);
			} else if (name == "geometry") {
				String id = p_parser.get_named_attribute_value("id");
				String name2 = p_parser.get_named_attribute_value_safe("name");
				while (p_parser.read() == OK) {
					if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT) {
						if (p_parser.get_node_name() == "mesh") {
							state.mesh_name_map[id] = (!name2.is_empty()) ? name2 : id;
							_parse_mesh_geometry(p_parser, id, name2);
						} else if (p_parser.get_node_name() == "spline") {
							state.mesh_name_map[id] = (!name2.is_empty()) ? name2 : id;
							_parse_curve_geometry(p_parser, id, name2);
						} else if (!p_parser.is_empty()) {
							p_parser.skip_section();
						}
					} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name() == "geometry") {
						break;
					}
				}

			} else if (name == "controller") {
				_parse_controller(p_parser);
			} else if (name == "animation") {
				_parse_animation(p_parser);
			} else if (name == "animation_clip") {
				_parse_animation_clip(p_parser);
			} else if (name == "visual_scene") {
				COLLADA_PRINT("visual scene");
				_parse_visual_scene(p_parser);
			} else if (!p_parser.is_empty()) {
				p_parser.skip_section();
			}

		} else if (p_parser.get_node_type() == XMLParser::NODE_ELEMENT_END && p_parser.get_node_name().begins_with("library_")) {
			break; //end of <asset>
		}
	}
}

void Collada::_joint_set_owner(Collada::Node *p_node, NodeSkeleton *p_owner) {
	if (p_node->type == Node::TYPE_JOINT) {
		NodeJoint *nj = static_cast<NodeJoint *>(p_node);
		nj->owner = p_owner;

		for (int i = 0; i < nj->children.size(); i++) {
			_joint_set_owner(nj->children.write[i], p_owner);
		}
	}
}

void Collada::_create_skeletons(Collada::Node **p_node, NodeSkeleton *p_skeleton) {
	Node *node = *p_node;

	if (node->type == Node::TYPE_JOINT) {
		if (!p_skeleton) {
			// ohohohoohoo it's a joint node, time to work!
			NodeSkeleton *sk = memnew(NodeSkeleton);
			*p_node = sk;
			sk->children.push_back(node);
			sk->parent = node->parent;
			node->parent = sk;
			p_skeleton = sk;
		}

		NodeJoint *nj = static_cast<NodeJoint *>(node);
		nj->owner = p_skeleton;
	} else {
		p_skeleton = nullptr;
	}

	for (int i = 0; i < node->children.size(); i++) {
		_create_skeletons(&node->children.write[i], p_skeleton);
	}
}

bool Collada::_remove_node(Node *p_parent, Node *p_node) {
	for (int i = 0; i < p_parent->children.size(); i++) {
		if (p_parent->children[i] == p_node) {
			p_parent->children.remove_at(i);
			return true;
		}
		if (_remove_node(p_parent->children[i], p_node)) {
			return true;
		}
	}

	return false;
}

void Collada::_remove_node(VisualScene *p_vscene, Node *p_node) {
	for (int i = 0; i < p_vscene->root_nodes.size(); i++) {
		if (p_vscene->root_nodes[i] == p_node) {
			p_vscene->root_nodes.remove_at(i);
			return;
		}
		if (_remove_node(p_vscene->root_nodes[i], p_node)) {
			return;
		}
	}

	ERR_PRINT("ERROR: Not found node to remove?");
}

void Collada::_merge_skeletons(VisualScene *p_vscene, Node *p_node) {
	if (p_node->type == Node::TYPE_GEOMETRY) {
		NodeGeometry *gnode = static_cast<NodeGeometry *>(p_node);
		if (gnode->controller) {
			// recount skeletons used
			HashSet<NodeSkeleton *> skeletons;

			for (int i = 0; i < gnode->skeletons.size(); i++) {
				String nodeid = gnode->skeletons[i];

				ERR_CONTINUE(!state.scene_map.has(nodeid)); //weird, it should have it...

				NodeJoint *nj = dynamic_cast<NodeJoint *>(state.scene_map[nodeid]);
				ERR_CONTINUE(!nj); //broken collada
				ERR_CONTINUE(!nj->owner); //weird, node should have a skeleton owner

				skeletons.insert(nj->owner);
			}

			if (skeletons.size() > 1) {
				//do the merger!!
				HashSet<NodeSkeleton *>::Iterator E = skeletons.begin();
				NodeSkeleton *base = *E;

				for (++E; E; ++E) {
					NodeSkeleton *merged = *E;
					_remove_node(p_vscene, merged);
					for (int i = 0; i < merged->children.size(); i++) {
						_joint_set_owner(merged->children[i], base);
						base->children.push_back(merged->children[i]);
						merged->children[i]->parent = base;
					}

					merged->children.clear(); //take children from it
					memdelete(merged);
				}
			}
		}
	}

	for (int i = 0; i < p_node->children.size(); i++) {
		_merge_skeletons(p_vscene, p_node->children[i]);
	}
}

void Collada::_merge_skeletons2(VisualScene *p_vscene) {
	for (KeyValue<String, SkinControllerData> &E : state.skin_controller_data_map) {
		SkinControllerData &cd = E.value;

		NodeSkeleton *skeleton = nullptr;

		for (const KeyValue<String, Transform3D> &F : cd.bone_rest_map) {
			String name;

			if (!state.sid_to_node_map.has(F.key)) {
				continue;
			}

			name = state.sid_to_node_map[F.key];

			ERR_CONTINUE(!state.scene_map.has(name));

			Node *node = state.scene_map[name];
			ERR_CONTINUE(node->type != Node::TYPE_JOINT);

			NodeSkeleton *sk = nullptr;

			while (node && !sk) {
				if (node->type == Node::TYPE_SKELETON) {
					sk = static_cast<NodeSkeleton *>(node);
				}
				node = node->parent;
			}

			ERR_CONTINUE(!sk);

			if (!skeleton) {
				skeleton = sk;
				continue;
			}

			if (skeleton != sk) {
				//whoa.. wtf, merge.
				_remove_node(p_vscene, sk);
				for (int i = 0; i < sk->children.size(); i++) {
					_joint_set_owner(sk->children[i], skeleton);
					skeleton->children.push_back(sk->children[i]);
					sk->children[i]->parent = skeleton;
				}

				sk->children.clear(); //take children from it
				memdelete(sk);
			}
		}
	}
}

bool Collada::_optimize_skeletons(VisualScene *p_vscene, Node *p_node) {
	Node *node = p_node;

	if (node->type == Node::TYPE_SKELETON && node->parent && node->parent->type == Node::TYPE_NODE && node->parent->children.size() == 1) {
		//replace parent by this...
		Node *parent = node->parent;

		//i wonder if this is alright.. i think it is since created skeleton (first joint) is already animated by bone..
		node->id = parent->id;
		node->name = parent->name;
		node->xform_list = parent->xform_list;
		node->default_transform = parent->default_transform;

		state.scene_map[node->id] = node;
		node->parent = parent->parent;

		if (parent->parent) {
			Node *gp = parent->parent;
			bool found = false;
			for (int i = 0; i < gp->children.size(); i++) {
				if (gp->children[i] == parent) {
					gp->children.write[i] = node;
					found = true;
					break;
				}
			}
			if (!found) {
				ERR_PRINT("BUG");
			}
		} else {
			bool found = false;

			for (int i = 0; i < p_vscene->root_nodes.size(); i++) {
				if (p_vscene->root_nodes[i] == parent) {
					p_vscene->root_nodes.write[i] = node;
					found = true;
					break;
				}
			}
			if (!found) {
				ERR_PRINT("BUG");
			}
		}

		parent->children.clear();
		memdelete(parent);
		return true;
	}

	for (int i = 0; i < node->children.size(); i++) {
		if (_optimize_skeletons(p_vscene, node->children[i])) {
			return false; //stop processing, go up
		}
	}

	return false;
}

bool Collada::_move_geometry_to_skeletons(VisualScene *p_vscene, Node *p_node, List<Node *> *p_mgeom) {
	// Bind Shape Matrix scales the bones and makes them gigantic, so the matrix then shrinks the model?
	// Solution: apply the Bind Shape Matrix to the VERTICES, and if the object comes scaled, it seems to be left alone!

	if (p_node->type == Node::TYPE_GEOMETRY) {
		NodeGeometry *ng = static_cast<NodeGeometry *>(p_node);
		if (ng->ignore_anim) {
			return false; //already made child of skeleton and processeg
		}

		if (ng->controller && ng->skeletons.size()) {
			String nodeid = ng->skeletons[0];

			ERR_FAIL_COND_V(!state.scene_map.has(nodeid), false); //weird, it should have it...
			NodeJoint *nj = dynamic_cast<NodeJoint *>(state.scene_map[nodeid]);
			ERR_FAIL_NULL_V(nj, false);
			ERR_FAIL_NULL_V(nj->owner, false); // Weird, node should have a skeleton owner.

			NodeSkeleton *sk = nj->owner;

			Node *p = sk->parent;
			bool node_is_parent_of_skeleton = false;

			while (p) {
				if (p == p_node) {
					node_is_parent_of_skeleton = true;
					break;
				}
				p = p->parent; // try again
			}

			ERR_FAIL_COND_V(node_is_parent_of_skeleton, false);

			//this should be correct
			ERR_FAIL_COND_V(!state.skin_controller_data_map.has(ng->source), false);
			SkinControllerData &skin = state.skin_controller_data_map[ng->source];
			Transform3D skel_inv = sk->get_global_transform().affine_inverse();
			p_node->default_transform = skel_inv * (skin.bind_shape /* p_node->get_global_transform()*/); // i honestly have no idea what to do with a previous model xform.. most exporters ignore it

			//make rests relative to the skeleton (they seem to be always relative to world)
			for (KeyValue<String, Transform3D> &E : skin.bone_rest_map) {
				E.value = skel_inv * E.value; //make the bone rest local to the skeleton
				state.bone_rest_map[E.key] = E.value; // make it remember where the bone is globally, now that it's relative
			}

			//but most exporters seem to work only if i do this..
			//p_node->default_transform = p_node->get_global_transform();

			//p_node->default_transform=Transform3D(); //this seems to be correct, because bind shape makes the object local to the skeleton
			p_node->ignore_anim = true; // collada may animate this later, if it does, then this is not supported (redo your original asset and don't animate the base mesh)
			p_node->parent = sk;
			//sk->children.push_back(0,p_node); //avoid INFINITE loop
			p_mgeom->push_back(p_node);
			return true;
		}
	}

	for (int i = 0; i < p_node->children.size(); i++) {
		if (_move_geometry_to_skeletons(p_vscene, p_node->children[i], p_mgeom)) {
			p_node->children.remove_at(i);
			i--;
		}
	}

	return false;
}

void Collada::_find_morph_nodes(VisualScene *p_vscene, Node *p_node) {
	if (p_node->type == Node::TYPE_GEOMETRY) {
		NodeGeometry *nj = static_cast<NodeGeometry *>(p_node);

		if (nj->controller) {
			String base = nj->source;

			while (!base.is_empty() && !state.mesh_data_map.has(base)) {
				if (state.skin_controller_data_map.has(base)) {
					SkinControllerData &sk = state.skin_controller_data_map[base];
					base = sk.base;
				} else if (state.morph_controller_data_map.has(base)) {
					state.morph_ownership_map[base] = nj->id;
					break;
				} else {
					ERR_FAIL_MSG("Invalid scene.");
				}
			}
		}
	}

	for (int i = 0; i < p_node->children.size(); i++) {
		_find_morph_nodes(p_vscene, p_node->children[i]);
	}
}

void Collada::_optimize() {
	for (KeyValue<String, VisualScene> &E : state.visual_scene_map) {
		VisualScene &vs = E.value;
		for (int i = 0; i < vs.root_nodes.size(); i++) {
			_create_skeletons(&vs.root_nodes.write[i]);
		}

		for (int i = 0; i < vs.root_nodes.size(); i++) {
			_merge_skeletons(&vs, vs.root_nodes[i]);
		}

		_merge_skeletons2(&vs);

		for (int i = 0; i < vs.root_nodes.size(); i++) {
			_optimize_skeletons(&vs, vs.root_nodes[i]);
		}

		for (int i = 0; i < vs.root_nodes.size(); i++) {
			List<Node *> mgeom;
			if (_move_geometry_to_skeletons(&vs, vs.root_nodes[i], &mgeom)) {
				vs.root_nodes.remove_at(i);
				i--;
			}

			while (!mgeom.is_empty()) {
				Node *n = mgeom.front()->get();
				n->parent->children.push_back(n);
				mgeom.pop_front();
			}
		}

		for (int i = 0; i < vs.root_nodes.size(); i++) {
			_find_morph_nodes(&vs, vs.root_nodes[i]);
		}
	}
}

int Collada::get_uv_channel(const String &p_name) {
	if (!channel_map.has(p_name)) {
		ERR_FAIL_COND_V(channel_map.size() == 2, 0);

		channel_map[p_name] = channel_map.size();
	}

	return channel_map[p_name];
}

Error Collada::load(const String &p_path, int p_flags) {
	Ref<XMLParser> parserr = memnew(XMLParser);
	XMLParser &parser = *parserr.ptr();
	Error err = parser.open(p_path);
	ERR_FAIL_COND_V_MSG(err, err, "Cannot open Collada file '" + p_path + "'.");

	state.local_path = ProjectSettings::get_singleton()->localize_path(p_path);
	state.import_flags = p_flags;
	/* Skip headers */
	while ((err = parser.read()) == OK) {
		if (parser.get_node_type() == XMLParser::NODE_ELEMENT) {
			if (parser.get_node_name() == "COLLADA") {
				break;
			} else if (!parser.is_empty()) {
				parser.skip_section(); // unknown section, likely headers
			}
		}
	}

	ERR_FAIL_COND_V_MSG(err != OK, ERR_FILE_CORRUPT, "Corrupted Collada file '" + p_path + "'.");

	/* Start loading Collada */

	{
		//version
		String version = parser.get_named_attribute_value("version");
		state.version.major = version.get_slice(".", 0).to_int();
		state.version.minor = version.get_slice(".", 1).to_int();
		state.version.rev = version.get_slice(".", 2).to_int();
		COLLADA_PRINT("Collada VERSION: " + version);
	}

	while ((err = parser.read()) == OK) {
		/* Read all the main sections.. */

		if (parser.get_node_type() != XMLParser::NODE_ELEMENT) {
			continue; //no idea what this may be, but skipping anyway
		}

		String section = parser.get_node_name();

		COLLADA_PRINT("section: " + section);

		if (section == "asset") {
			_parse_asset(parser);

		} else if (section.begins_with("library_")) {
			_parse_library(parser);
		} else if (section == "scene") {
			_parse_scene(parser);
		} else if (!parser.is_empty()) {
			parser.skip_section(); // unknown section, likely headers
		}
	}

	_optimize();
	return OK;
}

Collada::Collada() {
}
