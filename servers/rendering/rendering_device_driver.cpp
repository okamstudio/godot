/**************************************************************************/
/*  rendering_device_driver.cpp                                           */
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

#include "rendering_device_driver.h"

#include "thirdparty/spirv-reflect/spirv_reflect.h"

/****************/
/**** SHADER ****/
/****************/

Error RenderingDeviceDriver::_reflect_spirv(VectorView<ShaderStageSPIRVData> p_spirv, ShaderReflection &r_reflection) {
	r_reflection = {};

	for (uint32_t i = 0; i < p_spirv.size(); i++) {
		ShaderStage stage = p_spirv[i].shader_stage;
		ShaderStage stage_flag = (ShaderStage)(1 << p_spirv[i].shader_stage);

		if (p_spirv[i].shader_stage == SHADER_STAGE_COMPUTE) {
			r_reflection.is_compute = true;
			ERR_FAIL_COND_V_MSG(p_spirv.size() != 1, FAILED,
					"Compute shaders can only receive one stage, dedicated to compute.");
		}
		ERR_FAIL_COND_V_MSG(r_reflection.stages.has_flag(stage_flag), FAILED,
				"Stage " + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + " submitted more than once.");

		{
			SpvReflectShaderModule module;
			const uint8_t *spirv = p_spirv[i].spirv.ptr();
			SpvReflectResult result = spvReflectCreateShaderModule(p_spirv[i].spirv.size(), spirv, &module);
			ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed parsing shader.");

			if (r_reflection.is_compute) {
				r_reflection.compute_local_size[0] = module.entry_points->local_size.x;
				r_reflection.compute_local_size[1] = module.entry_points->local_size.y;
				r_reflection.compute_local_size[2] = module.entry_points->local_size.z;
			}
			uint32_t binding_count = 0;
			result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, nullptr);
			ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating descriptor bindings.");

			if (binding_count > 0) {
				// Parse bindings.

				Vector<SpvReflectDescriptorBinding *> bindings;
				bindings.resize(binding_count);
				result = spvReflectEnumerateDescriptorBindings(&module, &binding_count, bindings.ptrw());

				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed getting descriptor bindings.");

				for (uint32_t j = 0; j < binding_count; j++) {
					const SpvReflectDescriptorBinding &binding = *bindings[j];

					ShaderUniform uniform;

					bool need_array_dimensions = false;
					bool need_block_size = false;
					bool may_be_writable = false;

					switch (binding.descriptor_type) {
						case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: {
							uniform.type = UNIFORM_TYPE_SAMPLER;
							need_array_dimensions = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
							uniform.type = UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
							need_array_dimensions = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
							uniform.type = UNIFORM_TYPE_TEXTURE;
							need_array_dimensions = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
							uniform.type = UNIFORM_TYPE_IMAGE;
							need_array_dimensions = true;
							may_be_writable = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: {
							uniform.type = UNIFORM_TYPE_TEXTURE_BUFFER;
							need_array_dimensions = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: {
							uniform.type = UNIFORM_TYPE_IMAGE_BUFFER;
							need_array_dimensions = true;
							may_be_writable = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
							uniform.type = UNIFORM_TYPE_UNIFORM_BUFFER;
							need_block_size = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
							uniform.type = UNIFORM_TYPE_STORAGE_BUFFER;
							need_block_size = true;
							may_be_writable = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: {
							ERR_PRINT("Dynamic uniform buffer not supported.");
							continue;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: {
							ERR_PRINT("Dynamic storage buffer not supported.");
							continue;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: {
							uniform.type = UNIFORM_TYPE_INPUT_ATTACHMENT;
							need_array_dimensions = true;
						} break;
						case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
							ERR_PRINT("Acceleration structure not supported.");
							continue;
						} break;
					}

					if (need_array_dimensions) {
						if (binding.array.dims_count == 0) {
							uniform.length = 1;
						} else {
							for (uint32_t k = 0; k < binding.array.dims_count; k++) {
								if (k == 0) {
									uniform.length = binding.array.dims[0];
								} else {
									uniform.length *= binding.array.dims[k];
								}
							}
						}

					} else if (need_block_size) {
						uniform.length = binding.block.size;
					} else {
						uniform.length = 0;
					}

					if (may_be_writable) {
						uniform.writable = !(binding.type_description->decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE) && !(binding.block.decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE);
					} else {
						uniform.writable = false;
					}

					uniform.binding = binding.binding;
					uint32_t set = binding.set;

					ERR_FAIL_COND_V_MSG(set >= MAX_UNIFORM_SETS, FAILED,
							"On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' uses a set (" + itos(set) + ") index larger than what is supported (" + itos(MAX_UNIFORM_SETS) + ").");

					if (set < (uint32_t)r_reflection.uniform_sets.size()) {
						// Check if this already exists.
						bool exists = false;
						for (int k = 0; k < r_reflection.uniform_sets[set].size(); k++) {
							if (r_reflection.uniform_sets[set][k].binding == uniform.binding) {
								// Already exists, verify that it's the same type.
								ERR_FAIL_COND_V_MSG(r_reflection.uniform_sets[set][k].type != uniform.type, FAILED,
										"On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different uniform type.");

								// Also, verify that it's the same size.
								ERR_FAIL_COND_V_MSG(r_reflection.uniform_sets[set][k].length != uniform.length, FAILED,
										"On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different uniform size.");

								// Also, verify that it has the same writability.
								ERR_FAIL_COND_V_MSG(r_reflection.uniform_sets[set][k].writable != uniform.writable, FAILED,
										"On shader stage '" + String(SHADER_STAGE_NAMES[stage]) + "', uniform '" + binding.name + "' trying to reuse location for set=" + itos(set) + ", binding=" + itos(uniform.binding) + " with different writability.");

								// Just append stage mask and return.
								r_reflection.uniform_sets.write[set].write[k].stages.set_flag(stage_flag);
								exists = true;
								break;
							}
						}

						if (exists) {
							continue; // Merged.
						}
					}

					uniform.stages.set_flag(stage_flag);

					if (set >= (uint32_t)r_reflection.uniform_sets.size()) {
						r_reflection.uniform_sets.resize(set + 1);
					}

					r_reflection.uniform_sets.write[set].push_back(uniform);
				}
			}

			{
				// Specialization constants.

				uint32_t sc_count = 0;
				result = spvReflectEnumerateSpecializationConstants(&module, &sc_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating specialization constants.");

				if (sc_count) {
					Vector<SpvReflectSpecializationConstant *> spec_constants;
					spec_constants.resize(sc_count);

					result = spvReflectEnumerateSpecializationConstants(&module, &sc_count, spec_constants.ptrw());
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining specialization constants.");

					for (uint32_t j = 0; j < sc_count; j++) {
						int32_t existing = -1;
						ShaderSpecializationConstant sconst;
						SpvReflectSpecializationConstant *spc = spec_constants[j];

						sconst.constant_id = spc->constant_id;
						sconst.int_value = 0; // Clear previous value JIC.
						switch (spc->constant_type) {
							case SPV_REFLECT_SPECIALIZATION_CONSTANT_BOOL: {
								sconst.type = PIPELINE_SPECIALIZATION_CONSTANT_TYPE_BOOL;
								sconst.bool_value = spc->default_value.int_bool_value != 0;
							} break;
							case SPV_REFLECT_SPECIALIZATION_CONSTANT_INT: {
								sconst.type = PIPELINE_SPECIALIZATION_CONSTANT_TYPE_INT;
								sconst.int_value = spc->default_value.int_bool_value;
							} break;
							case SPV_REFLECT_SPECIALIZATION_CONSTANT_FLOAT: {
								sconst.type = PIPELINE_SPECIALIZATION_CONSTANT_TYPE_FLOAT;
								sconst.float_value = spc->default_value.float_value;
							} break;
						}
						sconst.stages.set_flag(stage_flag);

						for (int k = 0; k < r_reflection.specialization_constants.size(); k++) {
							if (r_reflection.specialization_constants[k].constant_id == sconst.constant_id) {
								ERR_FAIL_COND_V_MSG(r_reflection.specialization_constants[k].type != sconst.type, FAILED, "More than one specialization constant used for id (" + itos(sconst.constant_id) + "), but their types differ.");
								ERR_FAIL_COND_V_MSG(r_reflection.specialization_constants[k].int_value != sconst.int_value, FAILED, "More than one specialization constant used for id (" + itos(sconst.constant_id) + "), but their default values differ.");
								existing = k;
								break;
							}
						}

						if (existing > 0) {
							r_reflection.specialization_constants.write[existing].stages.set_flag(stage_flag);
						} else {
							r_reflection.specialization_constants.push_back(sconst);
						}
					}

					r_reflection.specialization_constants.sort();
				}
			}

			if (stage == SHADER_STAGE_VERTEX) {
				uint32_t iv_count = 0;
				result = spvReflectEnumerateInputVariables(&module, &iv_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating input variables.");

				if (iv_count) {
					Vector<SpvReflectInterfaceVariable *> input_vars;
					input_vars.resize(iv_count);

					result = spvReflectEnumerateInputVariables(&module, &iv_count, input_vars.ptrw());
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining input variables.");

					for (uint32_t j = 0; j < iv_count; j++) {
						if (input_vars[j] && input_vars[j]->decoration_flags == 0) { // Regular input.
							r_reflection.vertex_input_mask |= (((uint64_t)1) << input_vars[j]->location);
						}
					}
				}
			}

			if (stage == SHADER_STAGE_FRAGMENT) {
				uint32_t ov_count = 0;
				result = spvReflectEnumerateOutputVariables(&module, &ov_count, nullptr);
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating output variables.");

				if (ov_count) {
					Vector<SpvReflectInterfaceVariable *> output_vars;
					output_vars.resize(ov_count);

					result = spvReflectEnumerateOutputVariables(&module, &ov_count, output_vars.ptrw());
					ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
							"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining output variables.");

					for (uint32_t j = 0; j < ov_count; j++) {
						const SpvReflectInterfaceVariable *refvar = output_vars[j];
						if (refvar != nullptr && refvar->built_in != SpvBuiltInFragDepth) {
							r_reflection.fragment_output_mask |= 1 << refvar->location;
						}
					}
				}
			}

			uint32_t pc_count = 0;
			result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, nullptr);
			ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
					"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed enumerating push constants.");

			if (pc_count) {
				ERR_FAIL_COND_V_MSG(pc_count > 1, FAILED,
						"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "': Only one push constant is supported, which should be the same across shader stages.");

				Vector<SpvReflectBlockVariable *> pconstants;
				pconstants.resize(pc_count);
				result = spvReflectEnumeratePushConstantBlocks(&module, &pc_count, pconstants.ptrw());
				ERR_FAIL_COND_V_MSG(result != SPV_REFLECT_RESULT_SUCCESS, FAILED,
						"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "' failed obtaining push constants.");
#if 0
				if (pconstants[0] == nullptr) {
					Ref<FileAccess> f = FileAccess::open("res://popo.spv", FileAccess::WRITE);
					f->store_buffer((const uint8_t *)&SpirV[0], SpirV.size() * sizeof(uint32_t));
				}
#endif

				ERR_FAIL_COND_V_MSG(r_reflection.push_constant_size && r_reflection.push_constant_size != pconstants[0]->size, FAILED,
						"Reflection of SPIR-V shader stage '" + String(SHADER_STAGE_NAMES[p_spirv[i].shader_stage]) + "': Push constant block must be the same across shader stages.");

				r_reflection.push_constant_size = pconstants[0]->size;
				r_reflection.push_constant_stages.set_flag(stage_flag);

				//print_line("Stage: " + String(SHADER_STAGE_NAMES[stage]) + " push constant of size=" + itos(push_constant.push_constant_size));
			}

			// Destroy the reflection data when no longer required.
			spvReflectDestroyShaderModule(&module);
		}

		r_reflection.stages.set_flag(stage_flag);
	}

	return OK;
}

/**************/
/**** MISC ****/
/**************/

uint64_t RenderingDeviceDriver::api_trait_get(ApiTrait p_trait) {
	// Sensible canonical defaults.
	switch (p_trait) {
		case API_TRAIT_HONORS_PIPELINE_BARRIERS:
			return 1;
		case API_TRAIT_SHADER_CHANGE_INVALIDATION:
			return SHADER_CHANGE_INVALIDATION_ALL_BOUND_UNIFORM_SETS;
		case API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT:
			return 1;
		case API_TRAIT_TEXTURE_DATA_ROW_PITCH_STEP:
			return 1;
		case API_TRAIT_SECONDARY_VIEWPORT_SCISSOR:
			return 1;
		case API_TRAIT_CLEARS_WITH_COPY_ENGINE:
			return true;
		case API_TRAIT_USE_GENERAL_IN_COPY_QUEUES:
			return false;
		default:
			ERR_FAIL_V(0);
	}
}

/******************/

RenderingDeviceDriver::~RenderingDeviceDriver() {}
