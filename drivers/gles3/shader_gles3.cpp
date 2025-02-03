/**************************************************************************/
/*  shader_gles3.cpp                                                      */
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

#include "shader_gles3.h"

#include "core/local_vector.h"
#include "core/os/os.h"
#include "core/print_string.h"
#include "core/threaded_callable_queue.h"
#include "drivers/gles3/rasterizer_storage_gles3.h"
#include "drivers/gles3/shader_cache_gles3.h"
#include "servers/visual_server.h"

//#define DEBUG_OPENGL

#ifdef DEBUG_OPENGL

#define DEBUG_TEST_ERROR(m_section)                                         \
	{                                                                       \
		uint32_t err = glGetError();                                        \
		if (err) {                                                          \
			print_line("OpenGL Error #" + itos(err) + " at: " + m_section); \
		}                                                                   \
	}
#else

#define DEBUG_TEST_ERROR(m_section)

#endif

ShaderGLES3 *ShaderGLES3::active = nullptr;
SelfList<ShaderGLES3::Version>::List ShaderGLES3::versions_compiling;

ShaderCacheGLES3 *ShaderGLES3::shader_cache;
ThreadedCallableQueue<GLuint> *ShaderGLES3::cache_write_queue;

ThreadedCallableQueue<GLuint> *ShaderGLES3::compile_queue;
bool ShaderGLES3::parallel_compile_supported;

bool ShaderGLES3::async_hidden_forbidden;
uint32_t *ShaderGLES3::compiles_started_this_frame;
uint32_t *ShaderGLES3::max_frame_compiles_in_progress;
uint32_t ShaderGLES3::max_simultaneous_compiles;
uint32_t ShaderGLES3::active_compiles_count;
#ifdef DEBUG_ENABLED
bool ShaderGLES3::log_active_async_compiles_count;
#endif

uint64_t ShaderGLES3::current_frame;

//#define DEBUG_SHADER

#ifdef DEBUG_SHADER
#define DEBUG_PRINT(m_text) print_line(m_text);
#else
#define DEBUG_PRINT(m_text)
#endif

#define _EXT_COMPLETION_STATUS 0x91B1

GLint ShaderGLES3::get_uniform_location(int p_index) const {
	ERR_FAIL_COND_V(!version, -1);

	return version->uniform_location[p_index];
}

bool ShaderGLES3::bind() {
	return _bind(false);
}

bool ShaderGLES3::_bind(bool p_binding_fallback) {
	// Same base shader and version valid version?
	if (active == this && version) {
		if (new_conditional_version.code_version == conditional_version.code_version) {
			if (new_conditional_version.version == conditional_version.version) {
				return false;
			}
			// From ubershader to ubershader of the same code?
			if ((conditional_version.version & VersionKey::UBERSHADER_FLAG) && (new_conditional_version.version & VersionKey::UBERSHADER_FLAG)) {
				conditional_version.version = new_conditional_version.version;
				return false;
			}
		}
	}

	bool must_be_ready_now = !is_async_compilation_supported() || p_binding_fallback;

	conditional_version = new_conditional_version;
	version = get_current_version(must_be_ready_now);
	ERR_FAIL_COND_V(!version, false);

	bool ready = false;
	ready = _process_program_state(version, must_be_ready_now);
	if (version->compile_status == Version::COMPILE_STATUS_RESTART_NEEDED) {
		get_current_version(must_be_ready_now); // Trigger recompile
		ready = _process_program_state(version, must_be_ready_now);
	}

#ifdef DEBUG_ENABLED
	if (ready) {
		if (VS::get_singleton()->is_force_shader_fallbacks_enabled() && !must_be_ready_now && get_ubershader_flags_uniform() != -1) {
			ready = false;
		}
	}
#endif

	if (ready) {
		glUseProgram(version->ids.main);
		if (!version->uniforms_ready) {
			_setup_uniforms(custom_code_map.getptr(conditional_version.code_version));
			version->uniforms_ready = true;
		}
		DEBUG_TEST_ERROR("Use Program");
		active = this;
		return true;
	} else if (!must_be_ready_now && version->async_mode == ASYNC_MODE_VISIBLE && !p_binding_fallback && get_ubershader_flags_uniform() != -1) {
		// We can and have to fall back to the ubershader
		return _bind_ubershader();
	} else {
		// We have a compile error or must fall back by skipping render
		unbind();
		return false;
	}
}

bool ShaderGLES3::is_custom_code_ready_for_render(uint32_t p_code_id) {
	if (p_code_id == 0) {
		return true;
	}
	if (!is_async_compilation_supported() || get_ubershader_flags_uniform() == -1) {
		return true;
	}

	CustomCode *cc = custom_code_map.getptr(p_code_id);
	ERR_FAIL_COND_V(!cc, false);
	if (cc->async_mode == ASYNC_MODE_HIDDEN) {
#ifdef DEBUG_ENABLED
		if (VS::get_singleton()->is_force_shader_fallbacks_enabled()) {
			return false;
		}
#endif
		VersionKey effective_version;
		effective_version.version = new_conditional_version.version;
		effective_version.code_version = p_code_id;
		Version *v = version_map.getptr(effective_version);
		if (!v || cc->version != v->code_version || v->compile_status != Version::COMPILE_STATUS_OK) {
			return false;
		}
	}

	return true;
}

bool ShaderGLES3::_bind_ubershader(bool p_for_warmup) {
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V(!is_async_compilation_supported(), false);
	ERR_FAIL_COND_V(get_ubershader_flags_uniform() == -1, false);
#endif
	new_conditional_version.version |= VersionKey::UBERSHADER_FLAG;
	bool bound = _bind(true);
	new_conditional_version.version &= ~VersionKey::UBERSHADER_FLAG;
	if (p_for_warmup) {
		// Avoid GL UB message id 131222 caused by shadow samplers not properly set up yet
		unbind();
		return bound;
	}
	int conditionals_uniform = _get_uniform(get_ubershader_flags_uniform());
#ifdef DEBUG_ENABLED
	ERR_FAIL_COND_V(conditionals_uniform == -1, false);
#endif
#ifdef DEV_ENABLED
	// So far we don't need bit 31 for conditionals. That allows us to use signed integers,
	// which are more compatible across GL driver vendors.
	CRASH_COND(new_conditional_version.version >= 0x80000000);
#endif
	glUniform1i(conditionals_uniform, new_conditional_version.version);
	return bound;
}

void ShaderGLES3::advance_async_shaders_compilation() {
	SelfList<ShaderGLES3::Version> *curr = versions_compiling.first();
	while (curr) {
		SelfList<ShaderGLES3::Version> *next = curr->next();

		ShaderGLES3::Version *v = curr->self();
		// Only if it didn't already have a chance to be processed in this frame
		if (v->last_frame_processed != current_frame) {
			v->shader->_process_program_state(v, false);
		}

		curr = next;
	}
}

void ShaderGLES3::set_conditional_version(uint64_t p_version) {
	conditional_version = p_version;
	new_conditional_version = p_version;
}

uint64_t ShaderGLES3::get_conditional_version() {
	return conditional_version.key;
}

void ShaderGLES3::_log_active_compiles() {
#ifdef DEBUG_ENABLED
	if (log_active_async_compiles_count) {
		if (parallel_compile_supported) {
			print_line("Async. shader compiles: " + itos(active_compiles_count));
		} else if (compile_queue) {
			print_line("Queued shader compiles: " + itos(active_compiles_count));
		} else {
			CRASH_NOW();
		}
	}
#endif
}

bool ShaderGLES3::_process_program_state(Version *p_version, bool p_async_forbidden) {
	bool ready = false;
	bool run_next_step = true;
	while (run_next_step) {
		run_next_step = false;
		switch (p_version->compile_status) {
			case Version::COMPILE_STATUS_OK: {
				// Yeaaah!
				ready = true;
			} break;
			case Version::COMPILE_STATUS_ERROR: {
				// Sad, but we have to accept it
			} break;
			case Version::COMPILE_STATUS_PENDING:
			case Version::COMPILE_STATUS_RESTART_NEEDED: {
				// These lead to nowhere unless other piece of code starts the compile process
			} break;
			case Version::COMPILE_STATUS_SOURCE_PROVIDED: {
				uint32_t start_compiles_count = p_async_forbidden ? 2 : 0;
				if (!start_compiles_count) {
					uint32_t used_async_slots = MAX(active_compiles_count, *compiles_started_this_frame);
					uint32_t free_async_slots = used_async_slots < max_simultaneous_compiles ? max_simultaneous_compiles - used_async_slots : 0;
					start_compiles_count = MIN(2, free_async_slots);
				}
				if (start_compiles_count >= 1) {
					glCompileShader(p_version->ids.vert);
					if (start_compiles_count == 1) {
						p_version->compile_status = Version::COMPILE_STATUS_COMPILING_VERTEX;
					} else {
						glCompileShader(p_version->ids.frag);
						p_version->compile_status = Version::COMPILE_STATUS_COMPILING_VERTEX_AND_FRAGMENT;
					}
					if (!p_async_forbidden) {
						versions_compiling.add_last(&p_version->compiling_list);
						// Vertex and fragment shaders take independent compile slots
						active_compiles_count += start_compiles_count;
						*max_frame_compiles_in_progress = MAX(*max_frame_compiles_in_progress, active_compiles_count);
						_log_active_compiles();
					}
					(*compiles_started_this_frame) += start_compiles_count;
					run_next_step = p_async_forbidden;
				}
			} break;
			case Version::COMPILE_STATUS_COMPILING_VERTEX: {
				bool must_compile_frag_now = p_async_forbidden;
				if (!must_compile_frag_now) {
					if (active_compiles_count < max_simultaneous_compiles && *compiles_started_this_frame < max_simultaneous_compiles) {
						must_compile_frag_now = true;
					}
				}
				if (must_compile_frag_now) {
					glCompileShader(p_version->ids.frag);
					if (p_version->compiling_list.in_list()) {
						active_compiles_count++;
						*max_frame_compiles_in_progress = MAX(*max_frame_compiles_in_progress, active_compiles_count);
						_log_active_compiles();
					}
					p_version->compile_status = Version::COMPILE_STATUS_COMPILING_VERTEX_AND_FRAGMENT;
				} else if (parallel_compile_supported) {
					GLint completed = 0;
					glGetShaderiv(p_version->ids.vert, _EXT_COMPLETION_STATUS, &completed);
					if (completed) {
						// Not touching compiles count since the same slot used for vertex is now used for fragment
						glCompileShader(p_version->ids.frag);
						p_version->compile_status = Version::COMPILE_STATUS_COMPILING_FRAGMENT;
					}
				}
				run_next_step = p_async_forbidden;
			} break;
			case Version::COMPILE_STATUS_COMPILING_FRAGMENT:
			case Version::COMPILE_STATUS_COMPILING_VERTEX_AND_FRAGMENT: {
				bool must_complete_now = p_async_forbidden;
				if (!must_complete_now && parallel_compile_supported) {
					GLint vertex_completed = 0;
					if (p_version->compile_status == Version::COMPILE_STATUS_COMPILING_FRAGMENT) {
						vertex_completed = true;
					} else {
						glGetShaderiv(p_version->ids.vert, _EXT_COMPLETION_STATUS, &vertex_completed);
						if (p_version->compiling_list.in_list()) {
							active_compiles_count--;
#ifdef DEV_ENABLED
							CRASH_COND(active_compiles_count == UINT32_MAX);
#endif
							*max_frame_compiles_in_progress = MAX(*max_frame_compiles_in_progress, active_compiles_count);
							_log_active_compiles();
						}
						p_version->compile_status = Version::COMPILE_STATUS_COMPILING_FRAGMENT;
					}
					if (vertex_completed) {
						GLint frag_completed = 0;
						glGetShaderiv(p_version->ids.frag, _EXT_COMPLETION_STATUS, &frag_completed);
						if (frag_completed) {
							must_complete_now = true;
						}
					}
				}
				if (must_complete_now) {
					bool must_save_to_cache = p_version->version_key.is_subject_to_caching() && p_version->program_binary.source != Version::ProgramBinary::SOURCE_CACHE && shader_cache;
					bool ok = p_version->shader->_complete_compile(p_version->ids, must_save_to_cache);
					if (ok) {
						p_version->compile_status = Version::COMPILE_STATUS_LINKING;
						run_next_step = p_async_forbidden;
					} else {
						p_version->compile_status = Version::COMPILE_STATUS_ERROR;
						if (p_version->compiling_list.in_list()) {
							p_version->compiling_list.remove_from_list();
							active_compiles_count--;
#ifdef DEV_ENABLED
							CRASH_COND(active_compiles_count == UINT32_MAX);
#endif
							_log_active_compiles();
						}
					}
				}
			} break;
			case Version::COMPILE_STATUS_PROCESSING_AT_QUEUE: {
				// This is from the async. queue
				switch (p_version->program_binary.result_from_queue.get()) {
					case -1: { // Error
						p_version->compile_status = Version::COMPILE_STATUS_ERROR;
						p_version->compiling_list.remove_from_list();
						active_compiles_count--;
#ifdef DEV_ENABLED
						CRASH_COND(active_compiles_count == UINT32_MAX);
#endif
						_log_active_compiles();
					} break;
					case 0: { // In progress
						if (p_async_forbidden) {
							OS::get_singleton()->delay_usec(1000);
							run_next_step = true;
						}
					} break;
					case 1: { // Complete
						p_version->compile_status = Version::COMPILE_STATUS_BINARY_READY;
						run_next_step = true;
					} break;
				}
			} break;
			case Version::COMPILE_STATUS_BINARY_READY_FROM_CACHE: {
				bool eat_binary_now = p_async_forbidden;
				if (!eat_binary_now) {
					if (active_compiles_count < max_simultaneous_compiles && *compiles_started_this_frame < max_simultaneous_compiles) {
						eat_binary_now = true;
					}
				}
				if (eat_binary_now) {
					p_version->compile_status = Version::COMPILE_STATUS_BINARY_READY;
					run_next_step = true;
					if (!p_async_forbidden) {
						versions_compiling.add_last(&p_version->compiling_list);
						active_compiles_count++;
						*max_frame_compiles_in_progress = MAX(*max_frame_compiles_in_progress, active_compiles_count);
						_log_active_compiles();
						(*compiles_started_this_frame)++;
					}
				}
			} break;
			case Version::COMPILE_STATUS_BINARY_READY: {
				PoolByteArray::Read r = p_version->program_binary.data.read();
				glProgramBinary(p_version->ids.main, static_cast<GLenum>(p_version->program_binary.format), r.ptr(), p_version->program_binary.data.size());
				p_version->compile_status = Version::COMPILE_STATUS_LINKING;
				run_next_step = true;
			} break;
			case Version::COMPILE_STATUS_LINKING: {
				bool must_complete_now = p_async_forbidden || p_version->program_binary.source == Version::ProgramBinary::SOURCE_QUEUE;
				if (!must_complete_now && parallel_compile_supported) {
					GLint link_completed;
					glGetProgramiv(p_version->ids.main, _EXT_COMPLETION_STATUS, &link_completed);
					must_complete_now = link_completed;
				}
				if (must_complete_now) {
					bool must_save_to_cache = p_version->version_key.is_subject_to_caching() && p_version->program_binary.source != Version::ProgramBinary::SOURCE_CACHE && shader_cache;
					bool ok = false;
					if (must_save_to_cache && p_version->program_binary.source == Version::ProgramBinary::SOURCE_LOCAL) {
						ok = p_version->shader->_complete_link(p_version->ids, &p_version->program_binary.format, &p_version->program_binary.data);
					} else {
						ok = p_version->shader->_complete_link(p_version->ids);
#ifdef DEBUG_ENABLED
#if 0
						// Simulate GL rejecting program from cache
						if (p_version->program_binary.source == Version::ProgramBinary::SOURCE_CACHE) {
							ok = false;
						}
#endif
#endif
					}
					if (ok) {
						if (must_save_to_cache) {
							String &tmp_hash = p_version->program_binary.cache_hash;
							GLenum &tmp_format = p_version->program_binary.format;
							PoolByteArray &tmp_data = p_version->program_binary.data;
							cache_write_queue->enqueue(p_version->ids.main, [=]() {
								shader_cache->store(tmp_hash, static_cast<uint32_t>(tmp_format), tmp_data);
							});
						}
						p_version->compile_status = Version::COMPILE_STATUS_OK;
						ready = true;
					} else {
						if (p_version->program_binary.source == Version::ProgramBinary::SOURCE_CACHE) {
#ifdef DEBUG_ENABLED
							WARN_PRINT("Program binary from cache has been rejected by the GL. Removing from cache.");
#endif
							shader_cache->remove(p_version->program_binary.cache_hash);
							p_version->compile_status = Version::COMPILE_STATUS_RESTART_NEEDED;
						} else {
							if (p_version->program_binary.source == Version::ProgramBinary::SOURCE_QUEUE) {
								ERR_PRINT("Program binary from compile queue has been rejected by the GL. Bug?");
							}
							p_version->compile_status = Version::COMPILE_STATUS_ERROR;
						}
					}
					p_version->program_binary.data = PoolByteArray();
					p_version->program_binary.cache_hash.clear();
					if (p_version->compiling_list.in_list()) {
						p_version->compiling_list.remove_from_list();
						active_compiles_count--;
#ifdef DEV_ENABLED
						CRASH_COND(active_compiles_count == UINT32_MAX);
#endif
						_log_active_compiles();
					}
				}
			} break;
		}
	}
	return ready;
}

void ShaderGLES3::unbind() {
	version = nullptr;
	glUseProgram(0);
	active = nullptr;
}

static void _display_error_with_code(const String &p_error, GLuint p_shader_id) {
	int line = 1;

	GLint source_len;
	glGetShaderiv(p_shader_id, GL_SHADER_SOURCE_LENGTH, &source_len);
	LocalVector<GLchar> source_buffer;
	source_buffer.resize(source_len);
	glGetShaderSource(p_shader_id, source_len, NULL, source_buffer.ptr());

	String total_code(source_buffer.ptr());
	Vector<String> lines = String(total_code).split("\n");

	for (int j = 0; j < lines.size(); j++) {
		print_line(vformat("%4d | %s", line, lines[j]));
		line++;
	}

	ERR_PRINT(p_error);
}

static CharString _prepare_ubershader_chunk(const CharString &p_chunk) {
	String s(p_chunk.get_data());
	Vector<String> lines = s.split("\n");
	s.clear();
	for (int i = 0; i < lines.size(); ++i) {
		if (lines[i].ends_with("//ubershader-skip")) {
			continue;
		} else if (lines[i].ends_with("//ubershader-runtime")) {
			// Move from the preprocessor world to the true code realm
			String l = lines[i].trim_suffix("//ubershader-runtime").strip_edges();
			{
				// Ignore other comments
				Vector<String> pieces = l.split("//");
				l = pieces[0].strip_edges();
			}
			if (l == "#else") {
				s += "} else {\n";
			} else if (l == "#endif") {
				s += "}\n";
			} else if (l.begins_with("#ifdef")) {
				Vector<String> pieces = l.split_spaces();
				CRASH_COND(pieces.size() != 2);
				s += "if ((ubershader_flags & FLAG_" + pieces[1] + ") != 0) {\n";
			} else if (l.begins_with("#ifndef")) {
				Vector<String> pieces = l.split_spaces();
				CRASH_COND(pieces.size() != 2);
				s += "if ((ubershader_flags & FLAG_" + pieces[1] + ") == 0) {\n";
			} else {
				CRASH_NOW_MSG("The shader template is using too complex syntax in a line marked with ubershader-runtime.");
			}
			continue;
		}
		s += lines[i] + "\n";
	}
	return s.ascii();
}

// Possible source-status pairs after this:
// Local - Source provided
// Queue - Processing / Binary ready / Error
// Cache - Binary ready
ShaderGLES3::Version *ShaderGLES3::get_current_version(bool &r_async_forbidden) {
	VersionKey effective_version;
	effective_version.key = conditional_version.key;
	// Store and look up ubershader with all other version bits set to zero
	if ((conditional_version.version & VersionKey::UBERSHADER_FLAG)) {
		effective_version.version = VersionKey::UBERSHADER_FLAG;
	}

	Version *_v = version_map.getptr(effective_version);
	CustomCode *cc = nullptr;
	if (_v) {
		if (_v->compile_status == Version::COMPILE_STATUS_RESTART_NEEDED) {
			_v->program_binary.source = Version::ProgramBinary::SOURCE_NONE;
		} else {
			if (effective_version.code_version != 0) {
				cc = custom_code_map.getptr(effective_version.code_version);
				ERR_FAIL_COND_V(!cc, _v);
				if (cc->version == _v->code_version) {
					return _v;
				}
			} else {
				return _v;
			}
		}
	}

	if (!_v) {
		_v = &version_map[effective_version];
		_v->version_key = effective_version;
		_v->shader = this;
		_v->uniform_location = memnew_arr(GLint, uniform_count);
	}

	Version &v = *_v;

	/* SETUP CONDITIONALS */

	LocalVector<const char *> strings_common;
#ifdef GLES_OVER_GL
	strings_common.push_back("#version 330\n");
	strings_common.push_back("#define GLES_OVER_GL\n");
#else
	strings_common.push_back("#version 300 es\n");
#endif

#ifdef ANDROID_ENABLED
	strings_common.push_back("#define ANDROID_ENABLED\n");
#endif

	for (int i = 0; i < custom_defines.size(); i++) {
		strings_common.push_back(custom_defines[i].get_data());
		strings_common.push_back("\n");
	}

	if (is_async_compilation_supported() && get_ubershader_flags_uniform() != -1) {
		// Indicate that this shader may be used both as ubershader and conditioned during the session
		strings_common.push_back("#define UBERSHADER_COMPAT\n");
	}

	LocalVector<CharString> flag_macros;
	bool build_ubershader = get_ubershader_flags_uniform() != -1 && (effective_version.version & VersionKey::UBERSHADER_FLAG);
	if (build_ubershader) {
		strings_common.push_back("#define IS_UBERSHADER\n");
		for (int i = 0; i < conditional_count; i++) {
			String s = vformat("#define FLAG_%s (1 << %d)\n", String(conditional_defines[i]).strip_edges().trim_prefix("#define "), i);
			CharString cs = s.ascii();
			flag_macros.push_back(cs);
			strings_common.push_back(cs.ptr());
		}
		strings_common.push_back("\n");
	} else {
		for (int i = 0; i < conditional_count; i++) {
			bool enable = ((1 << i) & effective_version.version);
			strings_common.push_back(enable ? conditional_defines[i] : "");

			if (enable) {
				DEBUG_PRINT(conditional_defines[i]);
			}
		}
	}

	//keep them around during the function
	struct {
		CharString code_string;
		CharString code_globals;
		CharString material_string;
	} vert;
	struct {
		CharString code_string;
		CharString code_string2;
		CharString code_globals;
		CharString material_string;
	} frag;

	if (effective_version.code_version != 0) {
		ERR_FAIL_COND_V(!custom_code_map.has(effective_version.code_version), nullptr);
		if (!cc) {
			cc = &custom_code_map[effective_version.code_version];
		}
		if (cc->version != v.code_version) {
			v.code_version = cc->version;
			v.async_mode = cc->async_mode;
			v.uniforms_ready = false;
		}
	}

	/* CREATE PROGRAM */

	v.ids.main = glCreateProgram();

	ERR_FAIL_COND_V(v.ids.main == 0, nullptr);

	// To create the ubershader we need to modify the static strings;
	// they'll go in this array
	LocalVector<CharString> filtered_strings;

	/* VERTEX SHADER */

	if (cc) {
		for (int i = 0; i < cc->custom_defines.size(); i++) {
			strings_common.push_back(cc->custom_defines[i].get_data());
			DEBUG_PRINT("CD #" + itos(i) + ": " + String(cc->custom_defines[i]));
		}
	}

	LocalVector<const char *> strings_vertex(strings_common);

	//vertex precision is high
	strings_vertex.push_back("precision highp float;\n");
	strings_vertex.push_back("precision highp int;\n");
#ifndef GLES_OVER_GL
	strings_vertex.push_back("precision highp sampler2D;\n");
	strings_vertex.push_back("precision highp samplerCube;\n");
	strings_vertex.push_back("precision highp sampler2DArray;\n");
#endif

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(vertex_code0);
		filtered_strings.push_back(s);
		strings_vertex.push_back(s.get_data());
	} else {
		strings_vertex.push_back(vertex_code0.get_data());
	}

	if (cc) {
		vert.material_string = cc->uniforms.ascii();
		strings_vertex.push_back(vert.material_string.get_data());
	}

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(vertex_code1);
		filtered_strings.push_back(s);
		strings_vertex.push_back(s.get_data());
	} else {
		strings_vertex.push_back(vertex_code1.get_data());
	}

	if (cc) {
		vert.code_globals = cc->vertex_globals.ascii();
		strings_vertex.push_back(vert.code_globals.get_data());
	}

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(vertex_code2);
		filtered_strings.push_back(s);
		strings_vertex.push_back(s.get_data());
	} else {
		strings_vertex.push_back(vertex_code2.get_data());
	}

	if (cc) {
		vert.code_string = cc->vertex.ascii();
		strings_vertex.push_back(vert.code_string.get_data());
	}

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(vertex_code3);
		filtered_strings.push_back(s);
		strings_vertex.push_back(s.get_data());
	} else {
		strings_vertex.push_back(vertex_code3.get_data());
	}

#ifdef DEBUG_SHADER
	DEBUG_PRINT("\nVertex Code:\n\n" + String(code_string.get_data()));
	for (int i = 0; i < strings_vertex.size(); i++) {
		//print_line("vert strings "+itos(i)+":"+String(strings_vertex[i]));
	}
#endif

	/* FRAGMENT SHADER */

	LocalVector<const char *> strings_fragment(strings_common);

	//fragment precision is medium
	strings_fragment.push_back("precision highp float;\n");
	strings_fragment.push_back("precision highp int;\n");
#ifndef GLES_OVER_GL
	strings_fragment.push_back("precision highp sampler2D;\n");
	strings_fragment.push_back("precision highp samplerCube;\n");
	strings_fragment.push_back("precision highp sampler2DArray;\n");
#endif

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(fragment_code0);
		filtered_strings.push_back(s);
		strings_fragment.push_back(s.get_data());
	} else {
		strings_fragment.push_back(fragment_code0.get_data());
	}

	if (cc) {
		frag.material_string = cc->uniforms.ascii();
		strings_fragment.push_back(frag.material_string.get_data());
	}

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(fragment_code1);
		filtered_strings.push_back(s);
		strings_fragment.push_back(s.get_data());
	} else {
		strings_fragment.push_back(fragment_code1.get_data());
	}

	if (cc) {
		frag.code_globals = cc->fragment_globals.ascii();
		strings_fragment.push_back(frag.code_globals.get_data());
	}

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(fragment_code2);
		filtered_strings.push_back(s);
		strings_fragment.push_back(s.get_data());
	} else {
		strings_fragment.push_back(fragment_code2.get_data());
	}

	if (cc) {
		frag.code_string = cc->light.ascii();
		strings_fragment.push_back(frag.code_string.get_data());
	}

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(fragment_code3);
		filtered_strings.push_back(s);
		strings_fragment.push_back(s.get_data());
	} else {
		strings_fragment.push_back(fragment_code3.get_data());
	}

	if (cc) {
		frag.code_string2 = cc->fragment.ascii();
		strings_fragment.push_back(frag.code_string2.get_data());
	}

	if (build_ubershader) {
		CharString s = _prepare_ubershader_chunk(fragment_code4);
		filtered_strings.push_back(s);
		strings_fragment.push_back(s.get_data());
	} else {
		strings_fragment.push_back(fragment_code4.get_data());
	}

#ifdef DEBUG_SHADER
	DEBUG_PRINT("\nFragment Globals:\n\n" + String(code_globals.get_data()));
	DEBUG_PRINT("\nFragment Code:\n\n" + String(code_string2.get_data()));
	for (int i = 0; i < strings_fragment.size(); i++) {
		//print_line("frag strings "+itos(i)+":"+String(strings_fragment[i]));
	}
#endif

	if (!r_async_forbidden) {
		r_async_forbidden =
				(v.async_mode == ASYNC_MODE_HIDDEN && async_hidden_forbidden) ||
				(v.async_mode == ASYNC_MODE_VISIBLE && get_ubershader_flags_uniform() == -1);
	}

	bool in_cache = false;
	if (shader_cache && effective_version.is_subject_to_caching()) {
		const char *strings_platform[] = {
			reinterpret_cast<const char *>(glGetString(GL_VENDOR)),
			reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
			reinterpret_cast<const char *>(glGetString(GL_VERSION)),
			nullptr,
		};
		v.program_binary.cache_hash = ShaderCacheGLES3::hash_program(strings_platform, strings_vertex, strings_fragment);
		if (shader_cache->retrieve(v.program_binary.cache_hash, &v.program_binary.format, &v.program_binary.data)) {
			in_cache = true;
			v.program_binary.source = Version::ProgramBinary::SOURCE_CACHE;
			v.compile_status = Version::COMPILE_STATUS_BINARY_READY_FROM_CACHE;
		}
	}
	if (!in_cache) {
		if (compile_queue && !r_async_forbidden) {
			// Asynchronous compilation via queue (secondary context)
			// Remarks:
			// 1. We need to save vertex and fragment strings because they will not live beyond this function.
			// 2. We'll create another program since the other GL context is not shared.
			//    We are doing it that way since GL drivers can implement context sharing via locking, which
			//    would render (no pun intended) this whole effort to asynchronous useless.

			auto concat_shader_strings = [](const LocalVector<const char *> &p_shader_strings, LocalVector<char> *r_out) {
				r_out->clear();
				for (uint32_t i = 0; i < p_shader_strings.size(); i++) {
					uint32_t initial_size = r_out->size();
					uint32_t piece_len = strlen(reinterpret_cast<const char *>(p_shader_strings[i]));
					r_out->resize(initial_size + piece_len + 1);
					memcpy(r_out->ptr() + initial_size, p_shader_strings[i], piece_len);
					*(r_out->ptr() + initial_size + piece_len) = '\n';
				}
				*(r_out->ptr() + r_out->size() - 1) = '\0';
			};

			LocalVector<char> vertex_code;
			concat_shader_strings(strings_vertex, &vertex_code);
			LocalVector<char> fragment_code;
			concat_shader_strings(strings_fragment, &fragment_code);

			v.program_binary.source = Version::ProgramBinary::SOURCE_QUEUE;
			v.compile_status = Version::COMPILE_STATUS_PROCESSING_AT_QUEUE;
			versions_compiling.add_last(&v.compiling_list);
			active_compiles_count++;
			*max_frame_compiles_in_progress = MAX(*max_frame_compiles_in_progress, active_compiles_count);
			_log_active_compiles();
			(*compiles_started_this_frame)++;

			compile_queue->enqueue(v.ids.main, [this, &v, vertex_code, fragment_code]() {
				Version::Ids async_ids;
				async_ids.main = glCreateProgram();
				async_ids.vert = glCreateShader(GL_VERTEX_SHADER);
				async_ids.frag = glCreateShader(GL_FRAGMENT_SHADER);

				LocalVector<const char *> async_strings_vertex;
				async_strings_vertex.push_back(vertex_code.ptr());
				LocalVector<const char *> async_strings_fragment;
				async_strings_fragment.push_back(fragment_code.ptr());

				_set_source(async_ids, async_strings_vertex, async_strings_fragment);
				glCompileShader(async_ids.vert);
				glCompileShader(async_ids.frag);
				if (_complete_compile(async_ids, true) && _complete_link(async_ids, &v.program_binary.format, &v.program_binary.data)) {
					glDeleteShader(async_ids.frag);
					glDeleteShader(async_ids.vert);
					glDeleteProgram(async_ids.main);
					v.program_binary.result_from_queue.set(1);
				} else {
					v.program_binary.result_from_queue.set(0);
				}
			});
		} else {
			// Synchronous compilation, or async. via native support
			v.ids.vert = glCreateShader(GL_VERTEX_SHADER);
			v.ids.frag = glCreateShader(GL_FRAGMENT_SHADER);
			_set_source(v.ids, strings_vertex, strings_fragment);
			v.program_binary.source = Version::ProgramBinary::SOURCE_LOCAL;
			v.compile_status = Version::COMPILE_STATUS_SOURCE_PROVIDED;
		}
	}

	if (cc) {
		cc->versions.insert(effective_version.version);
	}

	return &v;
}

void ShaderGLES3::_set_source(Version::Ids p_ids, const LocalVector<const char *> &p_vertex_strings, const LocalVector<const char *> &p_fragment_strings) const {
	glShaderSource(p_ids.vert, p_vertex_strings.size(), p_vertex_strings.ptr(), nullptr);
	glShaderSource(p_ids.frag, p_fragment_strings.size(), p_fragment_strings.ptr(), nullptr);
}

bool ShaderGLES3::_complete_compile(Version::Ids p_ids, bool p_retrievable) const {
	GLint status;

	glGetShaderiv(p_ids.vert, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		// error compiling
		GLsizei iloglen;
		glGetShaderiv(p_ids.vert, GL_INFO_LOG_LENGTH, &iloglen);

		if (iloglen < 0) {
			glDeleteShader(p_ids.frag);
			glDeleteShader(p_ids.vert);
			glDeleteProgram(p_ids.main);

			ERR_PRINT("Vertex shader compilation failed with empty log");
		} else {
			if (iloglen == 0) {
				iloglen = 4096; //buggy driver (Adreno 220+....)
			}

			char *ilogmem = (char *)memalloc(iloglen + 1);
			ilogmem[iloglen] = 0;
			glGetShaderInfoLog(p_ids.vert, iloglen, &iloglen, ilogmem);

			String err_string = get_shader_name() + ": Vertex Program Compilation Failed:\n";

			err_string += ilogmem;
			_display_error_with_code(err_string, p_ids.vert);
			ERR_PRINT(err_string.ascii().get_data());
			memfree(ilogmem);
			glDeleteShader(p_ids.frag);
			glDeleteShader(p_ids.vert);
			glDeleteProgram(p_ids.main);
		}

		return false;
	}

	glGetShaderiv(p_ids.frag, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE) {
		// error compiling
		GLsizei iloglen;
		glGetShaderiv(p_ids.frag, GL_INFO_LOG_LENGTH, &iloglen);

		if (iloglen < 0) {
			glDeleteShader(p_ids.frag);
			glDeleteShader(p_ids.vert);
			glDeleteProgram(p_ids.main);
			ERR_PRINT("Fragment shader compilation failed with empty log");
		} else {
			if (iloglen == 0) {
				iloglen = 4096; //buggy driver (Adreno 220+....)
			}

			char *ilogmem = (char *)memalloc(iloglen + 1);
			ilogmem[iloglen] = 0;
			glGetShaderInfoLog(p_ids.frag, iloglen, &iloglen, ilogmem);

			String err_string = get_shader_name() + ": Fragment Program Compilation Failed:\n";

			err_string += ilogmem;
			_display_error_with_code(err_string, p_ids.frag);
			ERR_PRINT(err_string.ascii().get_data());
			memfree(ilogmem);
			glDeleteShader(p_ids.frag);
			glDeleteShader(p_ids.vert);
			glDeleteProgram(p_ids.main);
		}

		return false;
	}

	glAttachShader(p_ids.main, p_ids.frag);
	glAttachShader(p_ids.main, p_ids.vert);

	// bind attributes before linking
	for (int i = 0; i < attribute_pair_count; i++) {
		glBindAttribLocation(p_ids.main, attribute_pairs[i].index, attribute_pairs[i].name);
	}

	//if feedback exists, set it up

	if (feedback_count) {
		Vector<const char *> feedback;
		for (int i = 0; i < feedback_count; i++) {
			if (feedbacks[i].conditional == -1 || (1 << feedbacks[i].conditional) & conditional_version.version) {
				//conditional for this feedback is enabled
				feedback.push_back(feedbacks[i].name);
			}
		}

		if (feedback.size()) {
			glTransformFeedbackVaryings(p_ids.main, feedback.size(), feedback.ptr(), GL_INTERLEAVED_ATTRIBS);
		}
	}

	if (p_retrievable) {
		glProgramParameteri(p_ids.main, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
	}
	glLinkProgram(p_ids.main);

	return true;
}

bool ShaderGLES3::_complete_link(Version::Ids p_ids, GLenum *r_program_format, PoolByteArray *r_program_binary) const {
	GLint status;
	glGetProgramiv(p_ids.main, GL_LINK_STATUS, &status);

	if (status == GL_FALSE) {
		// error linking
		GLsizei iloglen;
		glGetProgramiv(p_ids.main, GL_INFO_LOG_LENGTH, &iloglen);

		if (iloglen < 0) {
			glDeleteShader(p_ids.frag);
			glDeleteShader(p_ids.vert);
			glDeleteProgram(p_ids.main);
			ERR_FAIL_COND_V(iloglen < 0, false);
		}

		if (iloglen == 0) {
			iloglen = 4096; //buggy driver (Adreno 220+....)
		}

		char *ilogmem = (char *)Memory::alloc_static(iloglen + 1);
		ilogmem[iloglen] = 0;
		glGetProgramInfoLog(p_ids.main, iloglen, &iloglen, ilogmem);

		String err_string = get_shader_name() + ": Program LINK FAILED:\n";

		err_string += ilogmem;
		ERR_PRINT(err_string.ascii().get_data());
		Memory::free_static(ilogmem);
		glDeleteShader(p_ids.frag);
		glDeleteShader(p_ids.vert);
		glDeleteProgram(p_ids.main);

		return false;
	}

	if (r_program_binary) {
		GLint program_len;
		glGetProgramiv(p_ids.main, GL_PROGRAM_BINARY_LENGTH, &program_len);
		r_program_binary->resize(program_len);
		PoolByteArray::Write w = r_program_binary->write();
		glGetProgramBinary(p_ids.main, program_len, NULL, r_program_format, w.ptr());
	}

	return true;
}

void ShaderGLES3::_setup_uniforms(CustomCode *p_cc) const {
	//print_line("uniforms:  ");
	for (int j = 0; j < uniform_count; j++) {
		version->uniform_location[j] = glGetUniformLocation(version->ids.main, uniform_names[j]);
		//print_line("uniform "+String(uniform_names[j])+" location "+itos(version->uniform_location[j]));
	}

	// set texture uniforms
	for (int i = 0; i < texunit_pair_count; i++) {
		GLint loc = glGetUniformLocation(version->ids.main, texunit_pairs[i].name);
		if (loc >= 0) {
			if (texunit_pairs[i].index < 0) {
				glUniform1i(loc, max_image_units + texunit_pairs[i].index); //negative, goes down
			} else {
				glUniform1i(loc, texunit_pairs[i].index);
			}
		}
	}

	// assign uniform block bind points
	for (int i = 0; i < ubo_count; i++) {
		GLint loc = glGetUniformBlockIndex(version->ids.main, ubo_pairs[i].name);
		if (loc >= 0)
			glUniformBlockBinding(version->ids.main, loc, ubo_pairs[i].index);
	}

	if (p_cc) {
		version->texture_uniform_locations.resize(p_cc->texture_uniforms.size());
		for (int i = 0; i < p_cc->texture_uniforms.size(); i++) {
			version->texture_uniform_locations.write[i] = glGetUniformLocation(version->ids.main, String(p_cc->texture_uniforms[i]).ascii().get_data());
			glUniform1i(version->texture_uniform_locations[i], i + base_material_tex_index);
		}
	}
}

void ShaderGLES3::_dispose_program(Version *p_version) {
	if (compile_queue) {
		if (p_version->compile_status == Version::COMPILE_STATUS_PROCESSING_AT_QUEUE) {
			compile_queue->cancel(p_version->ids.main);
		}
	}
	glDeleteShader(p_version->ids.vert);
	glDeleteShader(p_version->ids.frag);
	glDeleteProgram(p_version->ids.main);

	if (p_version->compiling_list.in_list()) {
		p_version->compiling_list.remove_from_list();
		active_compiles_count--;
#ifdef DEV_ENABLED
		CRASH_COND(active_compiles_count == UINT32_MAX);
#endif
		if (p_version->compile_status == Version::COMPILE_STATUS_COMPILING_VERTEX_AND_FRAGMENT) {
			active_compiles_count--;
#ifdef DEV_ENABLED
			CRASH_COND(active_compiles_count == UINT32_MAX);
#endif
		}
		_log_active_compiles();
	}
	p_version->compile_status = Version::COMPILE_STATUS_ERROR;
}

GLint ShaderGLES3::get_uniform_location(const String &p_name) const {
	ERR_FAIL_COND_V(!version, -1);
	return glGetUniformLocation(version->ids.main, p_name.ascii().get_data());
}

void ShaderGLES3::setup(const char **p_conditional_defines, int p_conditional_count, const char **p_uniform_names, int p_uniform_count, const AttributePair *p_attribute_pairs, int p_attribute_count, const TexUnitPair *p_texunit_pairs, int p_texunit_pair_count, const UBOPair *p_ubo_pairs, int p_ubo_pair_count, const Feedback *p_feedback, int p_feedback_count, const char *p_vertex_code, const char *p_fragment_code, int p_vertex_code_start, int p_fragment_code_start) {
	ERR_FAIL_COND(version);
	conditional_version.key = 0;
	new_conditional_version.key = 0;
	uniform_count = p_uniform_count;
	conditional_count = p_conditional_count;
	conditional_defines = p_conditional_defines;
	uniform_names = p_uniform_names;
	vertex_code = p_vertex_code;
	fragment_code = p_fragment_code;
	texunit_pairs = p_texunit_pairs;
	texunit_pair_count = p_texunit_pair_count;
	vertex_code_start = p_vertex_code_start;
	fragment_code_start = p_fragment_code_start;
	attribute_pairs = p_attribute_pairs;
	attribute_pair_count = p_attribute_count;
	ubo_pairs = p_ubo_pairs;
	ubo_count = p_ubo_pair_count;
	feedbacks = p_feedback;
	feedback_count = p_feedback_count;

	//split vertex and shader code (thank you, shader compiler programmers from you know what company).
	{
		String globals_tag = "\nVERTEX_SHADER_GLOBALS";
		String material_tag = "\nMATERIAL_UNIFORMS";
		String code_tag = "\nVERTEX_SHADER_CODE";
		String code = vertex_code;
		int cpos = code.find(material_tag);
		if (cpos == -1) {
			vertex_code0 = code.ascii();
		} else {
			vertex_code0 = code.substr(0, cpos).ascii();
			code = code.substr(cpos + material_tag.length(), code.length());

			cpos = code.find(globals_tag);

			if (cpos == -1) {
				vertex_code1 = code.ascii();
			} else {
				vertex_code1 = code.substr(0, cpos).ascii();
				String code2 = code.substr(cpos + globals_tag.length(), code.length());

				cpos = code2.find(code_tag);
				if (cpos == -1) {
					vertex_code2 = code2.ascii();
				} else {
					vertex_code2 = code2.substr(0, cpos).ascii();
					vertex_code3 = code2.substr(cpos + code_tag.length(), code2.length()).ascii();
				}
			}
		}
	}

	{
		String globals_tag = "\nFRAGMENT_SHADER_GLOBALS";
		String material_tag = "\nMATERIAL_UNIFORMS";
		String code_tag = "\nFRAGMENT_SHADER_CODE";
		String light_code_tag = "\nLIGHT_SHADER_CODE";
		String code = fragment_code;
		int cpos = code.find(material_tag);
		if (cpos == -1) {
			fragment_code0 = code.ascii();
		} else {
			fragment_code0 = code.substr(0, cpos).ascii();
			//print_line("CODE0:\n"+String(fragment_code0.get_data()));
			code = code.substr(cpos + material_tag.length(), code.length());
			cpos = code.find(globals_tag);

			if (cpos == -1) {
				fragment_code1 = code.ascii();
			} else {
				fragment_code1 = code.substr(0, cpos).ascii();
				//print_line("CODE1:\n"+String(fragment_code1.get_data()));

				String code2 = code.substr(cpos + globals_tag.length(), code.length());
				cpos = code2.find(light_code_tag);

				if (cpos == -1) {
					fragment_code2 = code2.ascii();
				} else {
					fragment_code2 = code2.substr(0, cpos).ascii();
					//print_line("CODE2:\n"+String(fragment_code2.get_data()));

					String code3 = code2.substr(cpos + light_code_tag.length(), code2.length());

					cpos = code3.find(code_tag);
					if (cpos == -1) {
						fragment_code3 = code3.ascii();
					} else {
						fragment_code3 = code3.substr(0, cpos).ascii();
						//print_line("CODE3:\n"+String(fragment_code3.get_data()));
						fragment_code4 = code3.substr(cpos + code_tag.length(), code3.length()).ascii();
						//print_line("CODE4:\n"+String(fragment_code4.get_data()));
					}
				}
			}
		}
	}

	// The upper limit must match the version used in storage.
	max_image_units = RasterizerStorageGLES3::safe_gl_get_integer(GL_MAX_TEXTURE_IMAGE_UNITS, RasterizerStorageGLES3::Config::max_desired_texture_image_units);
}

void ShaderGLES3::init_async_compilation() {
	if (is_async_compilation_supported() && get_ubershader_flags_uniform() != -1) {
		// Warm up the ubershader for the case of no custom code
		new_conditional_version.code_version = 0;
		_bind_ubershader(true);
	}
}

bool ShaderGLES3::is_async_compilation_supported() {
	return max_simultaneous_compiles > 0 && (compile_queue || parallel_compile_supported);
}

void ShaderGLES3::finish() {
	const VersionKey *V = nullptr;
	while ((V = version_map.next(V))) {
		Version &v = version_map[*V];
		_dispose_program(&v);
		memdelete_arr(v.uniform_location);
	}
	ERR_FAIL_COND(versions_compiling.first());
	ERR_FAIL_COND(active_compiles_count != 0);
}

void ShaderGLES3::clear_caches() {
	const VersionKey *V = nullptr;
	while ((V = version_map.next(V))) {
		Version &v = version_map[*V];
		_dispose_program(&v);
		memdelete_arr(v.uniform_location);
	}
	ERR_FAIL_COND(versions_compiling.first());
	ERR_FAIL_COND(active_compiles_count != 0);

	version_map.clear();

	custom_code_map.clear();
	version = nullptr;
	last_custom_code = 1;
}

uint32_t ShaderGLES3::create_custom_shader() {
	custom_code_map[last_custom_code] = CustomCode();
	custom_code_map[last_custom_code].version = 1;
	return last_custom_code++;
}

void ShaderGLES3::set_custom_shader_code(uint32_t p_code_id, const String &p_vertex, const String &p_vertex_globals, const String &p_fragment, const String &p_light, const String &p_fragment_globals, const String &p_uniforms, const Vector<StringName> &p_texture_uniforms, const Vector<CharString> &p_custom_defines, AsyncMode p_async_mode) {
	ERR_FAIL_COND(!custom_code_map.has(p_code_id));
	CustomCode *cc = &custom_code_map[p_code_id];

	cc->vertex = p_vertex;
	cc->vertex_globals = p_vertex_globals;
	cc->fragment = p_fragment;
	cc->fragment_globals = p_fragment_globals;
	cc->light = p_light;
	cc->texture_uniforms = p_texture_uniforms;
	cc->uniforms = p_uniforms;
	cc->custom_defines = p_custom_defines;
	cc->async_mode = p_async_mode;
	cc->version++;

	if (p_async_mode == ASYNC_MODE_VISIBLE && is_async_compilation_supported() && get_ubershader_flags_uniform() != -1) {
		// Warm up the ubershader for this custom code
		new_conditional_version.code_version = p_code_id;
		_bind_ubershader(true);
	}
}

void ShaderGLES3::set_custom_shader(uint32_t p_code_id) {
	new_conditional_version.code_version = p_code_id;
}

void ShaderGLES3::free_custom_shader(uint32_t p_code_id) {
	ERR_FAIL_COND(!custom_code_map.has(p_code_id));
	if (conditional_version.code_version == p_code_id) {
		conditional_version.code_version = 0; //do not keep using a version that is going away
		unbind();
	}

	VersionKey key;
	key.code_version = p_code_id;
	for (Set<uint32_t>::Element *E = custom_code_map[p_code_id].versions.front(); E; E = E->next()) {
		key.version = E->get();
		ERR_CONTINUE(!version_map.has(key));
		Version &v = version_map[key];

		_dispose_program(&v);
		memdelete_arr(v.uniform_location);

		version_map.erase(key);
	}

	custom_code_map.erase(p_code_id);
}

void ShaderGLES3::set_base_material_tex_index(int p_idx) {
	base_material_tex_index = p_idx;
}

ShaderGLES3::ShaderGLES3() {
	version = nullptr;
	last_custom_code = 1;
	base_material_tex_index = 0;
}

ShaderGLES3::~ShaderGLES3() {
	finish();
}
