/**************************************************************************/
/*  gltf_document_extension_audio.cpp                                     */
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

#include "gltf_document_extension_audio.h"

#include "core/core_bind.h"
#include "modules/modules_enabled.gen.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/resources/audio_stream_wav.h"

#ifdef MODULE_MINIMP3_ENABLED
#include "modules/minimp3/audio_stream_mp3.h"
#endif // MODULE_MINIMP3_ENABLED

#ifdef MODULE_VORBIS_ENABLED
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#endif // MODULE_VORBIS_ENABLED

// Import process.
Error GLTFDocumentExtensionAudio::import_preflight(Ref<GLTFState> p_state, Vector<String> p_extensions) {
	if (!p_extensions.has("KHR_audio_emitter")) {
		return ERR_SKIP;
	}
	Dictionary state_json = p_state->get_json();
	if (state_json.has("extensions")) {
		Dictionary state_extensions = state_json["extensions"];
		if (state_extensions.has("KHR_audio_emitter")) {
			// KHR_audio_emitter's data is all defined in the document-level
			// extensions and is designed to be highly reusable.
			Dictionary khr_audio_ext = state_extensions["KHR_audio_emitter"];
			Array audio_sources;
			if (khr_audio_ext.has("sources")) {
				audio_sources = khr_audio_ext["sources"];
				Array audio_data = khr_audio_ext.get("audio", Array());
				for (int i = 0; i < audio_sources.size(); i++) {
					Dictionary audio_source_dict = audio_sources[i];
					int audio_data_index = audio_source_dict.get("audio", -1);
					if (audio_data_index == -1) {
						continue;
					}
					ERR_FAIL_INDEX_V_MSG(audio_data_index, audio_data.size(), ERR_PARSE_ERROR, "GLTF audio: KHR_audio_emitter source audio data index " + itos(audio_data_index) + " is not in the KHR_audio_emitter audio data array (size=" + itos(audio_data.size()) + ").");
					Dictionary audio_data_dict = audio_data[audio_data_index];
				}
				p_state->set_additional_data(StringName("GLTFAudioData"), audio_data);
				p_state->set_additional_data(StringName("GLTFAudioSources"), audio_sources);
			}
			if (khr_audio_ext.has("emitters")) {
				Array audio_emitter_dicts = khr_audio_ext["emitters"];
				Array audio_emitters;
				for (int emitter_index = 0; emitter_index < audio_emitter_dicts.size(); emitter_index++) {
					Ref<GLTFAudioPlayer> audio_emitter = GLTFAudioPlayer::from_dictionary(audio_emitter_dicts[emitter_index]);
					audio_emitters.append(audio_emitter);
				}
				p_state->set_additional_data(StringName("GLTFAudioEmitters"), audio_emitters);
			}
		}
	}
	return OK;
}

Vector<String> GLTFDocumentExtensionAudio::get_supported_extensions() {
	Vector<String> ret;
	ret.push_back("KHR_audio_emitter");
	ret.push_back("OMI_audio_ogg_vorbis");
	return ret;
}

Error GLTFDocumentExtensionAudio::parse_node_extensions(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &p_extensions) {
	if (p_extensions.has("KHR_audio_emitter")) {
		Dictionary khr_audio_ext = p_extensions["KHR_audio_emitter"];
		if (khr_audio_ext.has("emitter")) {
			int emitter_index = khr_audio_ext["emitter"];
			p_gltf_node->set_additional_data(StringName("GLTFAudioEmitterIndex"), emitter_index);
		}
	}
	return OK;
}

// Helper methods for import_post_parse.
String _determine_mime_type_for_audio_dict(const Dictionary &p_audio_data_dict) {
	String mime_type = p_audio_data_dict.get("mimeType", "");
	if (!mime_type.is_empty()) {
		return mime_type;
	}
	// Determine the MIME type from the URI.
	String uri = p_audio_data_dict.get("uri", "");
	if (uri.begins_with("data:")) {
		return uri.substr(5, uri.find(";") - 5);
	} else if (uri.ends_with(".mp3")) {
		return "audio/mpeg";
	} else if (uri.ends_with(".wav")) {
		return "audio/wav";
	} else if (uri.ends_with(".ogg")) {
		return "audio/ogg";
	}
	// No MIME type was found, return an empty string.
	return mime_type;
}

Vector<uint8_t> _load_audio_bytes(Ref<GLTFState> p_state, Dictionary &p_audio_data_dict) {
	if (p_audio_data_dict.has("uri")) {
		String uri = p_audio_data_dict["uri"];
		if (uri.begins_with("data:")) {
			int comma = uri.find(",");
			ERR_FAIL_COND_V_MSG(comma == -1, Vector<uint8_t>(), "GLTF audio: Could not load audio URI data, no base64 data separator was found.");
			return core_bind::Marshalls::get_singleton()->base64_to_raw(uri.substr(comma + 1));
		}
		Error err;
		Vector<uint8_t> bytes = FileAccess::get_file_as_bytes(p_state->get_base_path().path_join(uri), &err);
		if (err == OK) {
			return bytes;
		}
	}
	if (p_audio_data_dict.has("bufferView")) {
		const GLTFBufferViewIndex bvi = p_audio_data_dict["bufferView"];
		const TypedArray<GLTFBufferView> &buffer_views = p_state->get_buffer_views();
		ERR_FAIL_INDEX_V(bvi, buffer_views.size(), Vector<uint8_t>());
		Ref<GLTFBufferView> bv = buffer_views[bvi];
		return bv->load_buffer_view_data(p_state);
	}
	ERR_PRINT("GLTF audio: Could not load audio data, neither uri nor bufferView was valid.");
	return Vector<uint8_t>();
}

void _load_audio_data(Ref<GLTFState> p_state, Dictionary &p_audio_data_dict, Dictionary &p_audio_source_dict) {
	Ref<AudioStream> audio_stream;
	String mime_type = _determine_mime_type_for_audio_dict(p_audio_data_dict);
	ERR_FAIL_COND_MSG(mime_type.is_empty(), "GLTF audio: Unable to determine the MIME type for the audio data.");
	Vector<uint8_t> audio_bytes = _load_audio_bytes(p_state, p_audio_data_dict);
	ERR_FAIL_COND_MSG(audio_bytes.is_empty(), "GLTF audio: Unable to load audio data, no data bytes were found.");
	const bool loop = p_audio_source_dict.get("loop", false);
	if (mime_type == "audio/wav") {
		Ref<AudioStreamWAV> audio_stream_wav;
		audio_stream_wav.instantiate();
		audio_stream_wav->set_data(audio_bytes);
		audio_stream_wav->set_loop_mode(loop ? AudioStreamWAV::LoopMode::LOOP_FORWARD : AudioStreamWAV::LoopMode::LOOP_DISABLED);
		audio_stream = audio_stream_wav;
#ifdef MODULE_MINIMP3_ENABLED
	} else if (mime_type == "audio/mpeg") {
		Ref<AudioStreamMP3> audio_stream_mp3;
		audio_stream_mp3.instantiate();
		audio_stream_mp3->set_data(audio_bytes);
		audio_stream_mp3->set_loop(loop);
		audio_stream = audio_stream_mp3;
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	} else if (mime_type == "audio/ogg") {
		Ref<AudioStreamOggVorbis> audio_stream_ogg = AudioStreamOggVorbis::load_from_buffer(audio_bytes);
		audio_stream_ogg->set_loop(loop);
		audio_stream = audio_stream_ogg;
#endif // MODULE_VORBIS_ENABLED
	} else {
		ERR_PRINT("GLTF audio: Unable to load audio data, the given MIME type is not supported: '" + mime_type + "'.");
		return;
	}
	ERR_FAIL_COND_MSG(audio_stream.is_null(), "GLTF audio: Unknown error loading audio with MIME type: '" + mime_type + "'.");
	audio_stream->set_name(p_audio_source_dict.get("name", ""));
	// Use snake_case because audio_stream only exists in memory, not in the exported JSON.
	p_audio_data_dict["audio_stream"] = audio_stream;
	p_audio_source_dict["audio_stream"] = audio_stream;
}

Array _generate_players_for_emitter(const Ref<GLTFAudioPlayer> p_audio_emitter, const Array &p_all_audio_sources) {
	// Generate audio players for each audio source in this audio emitter.
	Array audio_players;
	const Vector<int> audio_source_indices = p_audio_emitter->get_audio_sources();
	for (int source_number = 0; source_number < audio_source_indices.size(); source_number++) {
		int audio_source_index = audio_source_indices[source_number];
		ERR_FAIL_INDEX_V_MSG(audio_source_index, p_all_audio_sources.size(), audio_players, "GLTF audio: Emitter source index " + itos(audio_source_index) + " is not in the sources array (size=" + itos(p_all_audio_sources.size()) + ").");
		const Dictionary &audio_source_dict = p_all_audio_sources[audio_source_index];
		Ref<GLTFAudioPlayer> audio_player = p_audio_emitter->duplicate();
		if (likely(audio_source_dict.has("audio_stream"))) {
			audio_player->set_audio_stream(audio_source_dict["audio_stream"]);
		}
		audio_player->set_auto_play(audio_source_dict.get("autoPlay", false));
		audio_player->set_pitch_speed(audio_source_dict.get("pitchSpeed", 1.0));
		const real_t source_gain = audio_source_dict.get("gain", 1.0);
		audio_player->set_volume_gain(source_gain * audio_player->get_volume_gain());
		audio_players.append(audio_player);
	}
	// If no audio source was present, still generate an empty audio player for this emitter.
	if (audio_players.is_empty()) {
		audio_players.append(p_audio_emitter->duplicate());
	}
	return audio_players;
}

Array _generate_players_for_all_emitters(Ref<GLTFState> p_state) {
	// Generate audio players for each audio emitter + source pair.
	Array audio_sources = p_state->get_additional_data(StringName("GLTFAudioSources"));
	Array audio_emitters = p_state->get_additional_data(StringName("GLTFAudioEmitters"));
	Array audio_players;
	for (int emitter_index = 0; emitter_index < audio_emitters.size(); emitter_index++) {
		Ref<GLTFAudioPlayer> audio_emitter = audio_emitters[emitter_index];
		audio_players.append(_generate_players_for_emitter(audio_emitter, audio_sources));
	}
	p_state->set_additional_data(StringName("GLTFAudioPlayers"), audio_players);
	return audio_players;
}

Dictionary _get_main_scene_dictionary(Ref<GLTFState> p_state) {
	Dictionary state_json = p_state->get_json();
	int scene_index = state_json.get("scene", 0);
	Array scenes = state_json["scenes"];
	ERR_FAIL_INDEX_V_MSG(scene_index, scenes.size(), Dictionary(), "GLTF audio: Scene index " + itos(scene_index) + " is not in the scenes array (size=" + itos(scenes.size()) + ").");
	return scenes[scene_index];
}

Array _get_scene_level_audio_emitter_indices(Ref<GLTFState> p_state) {
	Dictionary scene_dict = _get_main_scene_dictionary(p_state);
	if (scene_dict.has("extensions")) {
		Dictionary scene_extensions = scene_dict["extensions"];
		if (scene_extensions.has("KHR_audio_emitter")) {
			Dictionary scene_khr_audio_ext = scene_extensions["KHR_audio_emitter"];
			if (scene_khr_audio_ext.has("emitters")) {
				return scene_khr_audio_ext["emitters"];
			}
		}
	}
	return Array();
}

Error GLTFDocumentExtensionAudio::import_post_parse(Ref<GLTFState> p_state) {
	// Load the audio bytes as audio streams and reference in the audio data and sources.
	Array audio_data = p_state->get_additional_data(StringName("GLTFAudioData"));
	Array audio_sources = p_state->get_additional_data(StringName("GLTFAudioSources"));
	for (int i = 0; i < audio_sources.size(); i++) {
		Dictionary audio_source_dict = audio_sources[i];
		int audio_data_index = audio_source_dict.get("audio", -1);
		if (audio_source_dict.has("extensions")) {
			Dictionary audio_source_extensions = audio_source_dict["extensions"];
			if (audio_source_extensions.has("OMI_audio_ogg_vorbis")) {
				Dictionary ogg_vorbis_ext = audio_source_extensions["OMI_audio_ogg_vorbis"];
				if (ogg_vorbis_ext.has("audio")) {
					audio_data_index = ogg_vorbis_ext["audio"];
				}
			}
		}
		if (audio_data_index == -1) {
			continue;
		}
		ERR_FAIL_INDEX_V_MSG(audio_data_index, audio_data.size(), ERR_PARSE_ERROR, "GLTF audio: Audio data index " + itos(audio_data_index) + " is not in the audio data array (size=" + itos(audio_data.size()) + ").");
		Dictionary audio_data_dict = audio_data[audio_data_index];
		_load_audio_data(p_state, audio_data_dict, audio_source_dict);
	}
	Array audio_players = _generate_players_for_all_emitters(p_state);
	// Set up audio players for the scene-level audio emitters.
	Array scene_emitter_indices = _get_scene_level_audio_emitter_indices(p_state);
	Array scene_audio_players;
	for (int i = 0; i < scene_emitter_indices.size(); i++) {
		int emitter_index = scene_emitter_indices[i];
		// Note: The size of the emitters array and players array will be the same.
		ERR_FAIL_INDEX_V_MSG(emitter_index, audio_players.size(), ERR_PARSE_ERROR, "GLTF audio: Scene-level emitter index " + itos(emitter_index) + " is not in the emitters array (size=" + itos(audio_players.size()) + ").");
		scene_audio_players.append_array(audio_players[emitter_index]);
	}
	p_state->set_additional_data(StringName("GLTFSceneAudioPlayers"), scene_audio_players);
	return OK;
}

Node3D *GLTFDocumentExtensionAudio::generate_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_parent) {
	Variant emitter_index_variant = p_gltf_node->get_additional_data(StringName("GLTFAudioEmitterIndex"));
	if (emitter_index_variant.get_type() != Variant::INT) {
		return nullptr;
	}
	// Grab the audio players for this node's emitter index from the state.
	int emitter_index = emitter_index_variant;
	Array state_all_audio_emitter_players = p_state->get_additional_data(StringName("GLTFAudioPlayers"));
	if (state_all_audio_emitter_players.is_empty()) {
		state_all_audio_emitter_players = _generate_players_for_all_emitters(p_state);
	}
	ERR_FAIL_INDEX_V_MSG(emitter_index, state_all_audio_emitter_players.size(), nullptr, "GLTF audio: Node emitter index " + itos(emitter_index) + " is not in the GLTF emitters array (size=" + itos(state_all_audio_emitter_players.size()) + ").");
	Array audio_players = state_all_audio_emitter_players[emitter_index];
	// Generate Godot nodes for the audio players.
	Node3D *ret = nullptr;
	for (int i = 0; i < audio_players.size(); i++) {
		Ref<GLTFAudioPlayer> audio_player = audio_players[i];
		if (audio_player.is_valid()) {
			Node *audio_node = audio_player->to_node();
			if (ret) {
				ret->add_child(audio_node);
			} else {
				Node3D *audio_node_3d = Object::cast_to<Node3D>(audio_node);
				if (audio_node_3d) {
					ret = audio_node_3d;
				} else {
					// The generated node must be a Node3D to ensure it has a transform,
					// otherwise the GLTF node transform hierarchy will be broken.
					ret = memnew(Node3D);
					ret->add_child(audio_node);
				}
			}
		}
	}
	return ret;
}

Error GLTFDocumentExtensionAudio::import_post(Ref<GLTFState> p_state, Node *p_root_node) {
	// Instantiate the scene-level audio players as children of the root node.
	Array scene_audio_players = p_state->get_additional_data(StringName("GLTFSceneAudioPlayers"));
	for (int i = 0; i < scene_audio_players.size(); i++) {
		Ref<GLTFAudioPlayer> audio_player = scene_audio_players[i];
		if (audio_player.is_valid()) {
			Node *audio_node = audio_player->to_node();
			p_root_node->add_child(audio_node);
			audio_node->set_owner(p_root_node);
		}
	}
	return OK;
}

// Export process.
Array _get_or_create_audio_data_in_state(Ref<GLTFState> p_state) {
	Variant state_data_variant = p_state->get_additional_data(StringName("GLTFAudioData"));
	if (state_data_variant.get_type() == Variant::ARRAY) {
		return state_data_variant;
	}
	Array state_data;
	p_state->set_additional_data(StringName("GLTFAudioData"), state_data);
	return state_data;
}

Array _get_or_create_audio_sources_in_state(Ref<GLTFState> p_state) {
	Variant state_sources_variant = p_state->get_additional_data(StringName("GLTFAudioSources"));
	if (state_sources_variant.get_type() == Variant::ARRAY) {
		return state_sources_variant;
	}
	Array state_sources;
	p_state->set_additional_data(StringName("GLTFAudioSources"), state_sources);
	return state_sources;
}

Array _get_or_create_audio_emitters_in_state(Ref<GLTFState> p_state) {
	Variant state_emitters_variant = p_state->get_additional_data(StringName("GLTFAudioEmitters"));
	if (state_emitters_variant.get_type() == Variant::ARRAY) {
		return state_emitters_variant;
	}
	Array state_emitters;
	p_state->set_additional_data(StringName("GLTFAudioEmitters"), state_emitters);
	return state_emitters;
}

int _get_or_insert_audio_stream_in_state(Ref<GLTFState> p_state, Ref<AudioStream> p_audio_stream) {
	Array audio_data = _get_or_create_audio_data_in_state(p_state);
	for (int i = 0; i < audio_data.size(); i++) {
		Dictionary audio_data_dict = audio_data[i];
		if (audio_data_dict.get("audio_stream", Variant()) == p_audio_stream) {
			return i;
		}
	}
	const int new_index = audio_data.size();
	Dictionary audio_data_dict;
	// Use snake_case because audio_stream only exists in memory, not in the exported JSON.
	audio_data_dict["audio_stream"] = p_audio_stream;
	audio_data.push_back(audio_data_dict);
	return new_index;
}

int _get_or_insert_audio_source_in_state(Ref<GLTFState> p_state, Dictionary &p_audio_source) {
	Array state_sources = _get_or_create_audio_sources_in_state(p_state);
	for (int i = 0; i < state_sources.size(); i++) {
		Dictionary state_source = state_sources[i];
		if (state_source == p_audio_source) {
			return i;
		}
	}
	const int new_index = state_sources.size();
	state_sources.push_back(p_audio_source);
	return new_index;
}

int _get_or_insert_audio_player_in_state(Ref<GLTFState> p_state, Ref<GLTFAudioPlayer> p_audio_player) {
	Array state_emitters = _get_or_create_audio_emitters_in_state(p_state);
	for (int i = 0; i < state_emitters.size(); i++) {
		Ref<GLTFAudioPlayer> state_emitter = state_emitters[i];
		if (state_emitter->to_dictionary() == p_audio_player->to_dictionary()) {
			return i;
		}
	}
	const int new_index = state_emitters.size();
	state_emitters.push_back(p_audio_player);
	return new_index;
}

void _copy_audio_stream_properties_to_audio_source(const Ref<AudioStream> p_audio_stream, Dictionary &p_audio_source) {
	Ref<AudioStreamWAV> audio_stream_wav = p_audio_stream;
#ifdef MODULE_MINIMP3_ENABLED
	Ref<AudioStreamMP3> audio_stream_mp3 = p_audio_stream;
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	Ref<AudioStreamOggVorbis> audio_stream_ogg = p_audio_stream;
#endif // MODULE_VORBIS_ENABLED
	if (audio_stream_wav.is_valid()) {
		const bool loop = audio_stream_wav->get_loop_mode() != AudioStreamWAV::LoopMode::LOOP_DISABLED;
		if (loop) {
			// If false, we don't need to write false, since that is the default value.
			p_audio_source["loop"] = true;
		}
#ifdef MODULE_MINIMP3_ENABLED
	} else if (audio_stream_mp3.is_valid()) {
		if (audio_stream_mp3->has_loop()) {
			// If false, we don't need to write false, since that is the default value.
			p_audio_source["loop"] = true;
		}
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	} else if (audio_stream_ogg.is_valid()) {
		if (audio_stream_ogg->has_loop()) {
			// If false, we don't need to write false, since that is the default value.
			p_audio_source["loop"] = true;
		}
#endif // MODULE_VORBIS_ENABLED
	}
	if (!p_audio_source.has("name")) {
		if (!p_audio_stream->get_name().is_empty()) {
			p_audio_source["name"] = p_audio_stream->get_name();
		} else if (!p_audio_stream->get_path().is_empty()) {
			p_audio_source["name"] = p_audio_stream->get_path().get_file();
		}
	}
}

void GLTFDocumentExtensionAudio::convert_scene_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Node *p_scene_node) {
	Ref<GLTFAudioPlayer> audio_player;
	Ref<AudioStream> audio_stream;
	Dictionary audio_source;
	if (cast_to<AudioStreamPlayer>(p_scene_node)) {
		AudioStreamPlayer *audio_player_node = Object::cast_to<AudioStreamPlayer>(p_scene_node);
		audio_player = GLTFAudioPlayer::from_node_0d(audio_player_node);
		if (audio_player_node->is_autoplay_enabled()) {
			// If false, we don't need to write false, since that is the default value.
			audio_source["autoPlay"] = true;
		}
		if (audio_player_node->get_pitch_scale() != 1.0f) {
			audio_source["pitchSpeed"] = audio_player_node->get_pitch_scale();
		}
		audio_stream = audio_player_node->get_stream();
	} else if (cast_to<AudioStreamPlayer3D>(p_scene_node)) {
		AudioStreamPlayer3D *audio_player_node = Object::cast_to<AudioStreamPlayer3D>(p_scene_node);
		audio_player = GLTFAudioPlayer::from_node_3d(audio_player_node);
		if (audio_player_node->is_autoplay_enabled()) {
			// If false, we don't need to write false, since that is the default value.
			audio_source["autoPlay"] = true;
		}
		if (audio_player_node->get_pitch_scale() != 1.0f) {
			audio_source["pitchSpeed"] = audio_player_node->get_pitch_scale();
		}
		audio_stream = audio_player_node->get_stream();
	}
	if (audio_player.is_null()) {
		return; // Not an audio node or could not convert.
	}
	if (audio_stream.is_valid()) {
		_copy_audio_stream_properties_to_audio_source(audio_stream, audio_source);
		audio_source["audio"] = _get_or_insert_audio_stream_in_state(p_state, audio_stream);
	}
	if (!audio_source.is_empty()) {
		const GLTFAudioSourceIndex source_index = _get_or_insert_audio_source_in_state(p_state, audio_source);
		Vector<GLTFAudioSourceIndex> audio_sources;
		audio_sources.push_back(source_index);
		audio_player->set_audio_sources(audio_sources);
	}
	const int emitter_index = _get_or_insert_audio_player_in_state(p_state, audio_player);
	p_gltf_node->set_additional_data(StringName("GLTFAudioEmitterIndex"), emitter_index);
}

GLTFBufferViewIndex _save_audio_data_to_buffer(Ref<GLTFState> p_state, Ref<AudioStream> p_audio_stream) {
	// Save to bytes in memory. Assign Ref<> types to perform a cast.
	Ref<AudioStreamWAV> audio_stream_wav = p_audio_stream;
#ifdef MODULE_MINIMP3_ENABLED
	Ref<AudioStreamMP3> audio_stream_mp3 = p_audio_stream;
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	Ref<AudioStreamOggVorbis> audio_stream_ogg = p_audio_stream;
#endif // MODULE_VORBIS_ENABLED
	Vector<uint8_t> audio_bytes;
	if (audio_stream_wav.is_valid()) {
		audio_bytes = audio_stream_wav->get_data();
#ifdef MODULE_MINIMP3_ENABLED
	} else if (audio_stream_mp3.is_valid()) {
		audio_bytes = audio_stream_mp3->get_data();
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	} else if (audio_stream_ogg.is_valid()) {
		ERR_PRINT("GLTF audio: Unable to export audio data, Godot is not capable of saving Ogg Vorbis files yet.");
		return -1;
#endif // MODULE_VORBIS_ENABLED
	} else {
		ERR_PRINT("GLTF audio: Unable to export audio data, unknown AudioStream class '" + p_audio_stream->get_class() + "'.");
		return -1;
	}
	ERR_FAIL_COND_V_MSG(audio_bytes.is_empty(), -1, "GLTF audio: Unable to export audio data, no data bytes were found.");
	// Write the bytes to a buffer.
	return p_state->append_data_to_buffers(audio_bytes, true);
}

String _determine_mime_type_for_audio_stream(const Ref<AudioStream> p_audio_stream) {
	Ref<AudioStreamWAV> audio_stream_wav = p_audio_stream;
	if (audio_stream_wav.is_valid()) {
		return "audio/wav";
	}
#ifdef MODULE_MINIMP3_ENABLED
	Ref<AudioStreamMP3> audio_stream_mp3 = p_audio_stream;
	if (audio_stream_mp3.is_valid()) {
		return "audio/mpeg";
	}
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	Ref<AudioStreamOggVorbis> audio_stream_ogg = p_audio_stream;
	if (audio_stream_ogg.is_valid()) {
		return "audio/ogg";
#endif // MODULE_VORBIS_ENABLED
	}
	ERR_PRINT("GLTF audio: Unable to determine MIME type for AudioStream, unknown class '" + p_audio_stream->get_class() + "'.");
	return "";
}

Dictionary _save_audio_data(const Ref<GLTFState> p_state, const Dictionary &p_audio_data_dict, const int p_audio_data_index) {
	Dictionary serialized_audio_data;
	if (!p_audio_data_dict.has("audio_stream")) {
		return serialized_audio_data;
	}
	// Use snake_case because audio_stream only exists in memory, not in the exported JSON.
	Ref<AudioStream> audio_stream = p_audio_data_dict["audio_stream"];
	serialized_audio_data["mimeType"] = _determine_mime_type_for_audio_stream(audio_stream);
	String filename = p_state->get_filename();
	String base_path = p_state->get_base_path();
	// If .gltf, it's text, save audio to a separate file.
	// If .glb, it's binary. If empty, it's a buffer, also binary.
	if (!filename.ends_with(".gltf")) {
		GLTFBufferViewIndex bvi = _save_audio_data_to_buffer(p_state, audio_stream);
		if (bvi >= 0) {
			serialized_audio_data["bufferView"] = bvi;
		}
		return serialized_audio_data;
	}
	String save_filename = p_audio_data_dict.get("uri", "");
	if (save_filename.is_empty()) {
		save_filename = audio_stream->get_name();
		if (save_filename.is_empty()) {
			save_filename = audio_stream->get_path().get_file().get_basename();
			if (save_filename.is_empty()) {
				save_filename = "audio_" + itos(p_audio_data_index);
			}
		}
	}
	// Save to a file. Assign Ref<> types to perform a cast.
	Ref<AudioStreamWAV> audio_stream_wav = audio_stream;
#ifdef MODULE_MINIMP3_ENABLED
	Ref<AudioStreamMP3> audio_stream_mp3 = audio_stream;
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	Ref<AudioStreamOggVorbis> audio_stream_ogg = audio_stream;
#endif // MODULE_VORBIS_ENABLED
	if (audio_stream_wav.is_valid()) {
		if (!save_filename.ends_with(".wav")) {
			save_filename += ".wav";
		}
		audio_stream_wav->save_to_wav(base_path + save_filename);
		serialized_audio_data["uri"] = save_filename;
#ifdef MODULE_MINIMP3_ENABLED
	} else if (audio_stream_mp3.is_valid()) {
		if (!save_filename.ends_with(".mp3")) {
			save_filename += ".mp3";
		}
		Ref<FileAccess> file = FileAccess::open(base_path.path_join(save_filename), FileAccess::WRITE);
		file->store_buffer(audio_stream_mp3->get_data());
		file->close();
		serialized_audio_data["uri"] = save_filename;
#endif // MODULE_MINIMP3_ENABLED
#ifdef MODULE_VORBIS_ENABLED
	} else if (audio_stream_ogg.is_valid()) {
		ERR_PRINT("GLTF audio: Unable to export audio data, Godot is not capable of saving Ogg Vorbis files yet.");
#endif // MODULE_VORBIS_ENABLED
	} else {
		ERR_PRINT("GLTF audio: Unable to export audio data, unknown AudioStream class '" + audio_stream->get_class() + "'.");
	}
	return serialized_audio_data;
}

void _insert_scene_level_audio_emtters_if_any(Ref<GLTFState> p_state) {
	Array scene_audio_players = p_state->get_additional_data(StringName("GLTFSceneAudioPlayers"));
	Array scene_audio_emitter_indices;
	for (int i = 0; i < scene_audio_players.size(); i++) {
		const int emitter_index = _get_or_insert_audio_player_in_state(p_state, scene_audio_players[i]);
		scene_audio_emitter_indices.push_back(emitter_index);
	}
	p_state->set_additional_data(StringName("GLTFSceneAudioEmitterIndices"), scene_audio_emitter_indices);
}

Error GLTFDocumentExtensionAudio::export_preserialize(Ref<GLTFState> p_state) {
	_insert_scene_level_audio_emtters_if_any(p_state);
	Array audio_data = p_state->get_additional_data(StringName("GLTFAudioData"));
	Array audio_sources = p_state->get_additional_data(StringName("GLTFAudioSources"));
	Array audio_emitters = p_state->get_additional_data(StringName("GLTFAudioEmitters"));
	if (audio_sources.is_empty() && audio_emitters.is_empty()) {
		return OK; // No audio data to export.
	}
	p_state->add_used_extension("KHR_audio_emitter");
	Dictionary state_json = p_state->get_json();
	Dictionary serialized_extensions = state_json.get_or_add("extensions", Dictionary());
	Dictionary khr_audio_emitter_ext = serialized_extensions.get_or_add("KHR_audio_emitter", Dictionary());
	Array serialized_audio_data;
	Array serialized_sources;
	Array serialized_emitters;
	for (int i = 0; i < audio_data.size(); i++) {
		Dictionary serialized = _save_audio_data(p_state, audio_data[i], i);
		serialized_audio_data.push_back(serialized);
	}
	for (int i = 0; i < audio_sources.size(); i++) {
		Dictionary serialized = Dictionary(audio_sources[i]).duplicate(false);
		Ref<AudioStream> audio_stream = serialized["audio_stream"];
		serialized.erase("audio_stream");
		serialized_sources.push_back(serialized);
	}
	for (int i = 0; i < audio_emitters.size(); i++) {
		Ref<GLTFAudioPlayer> audio_emitter = audio_emitters[i];
		serialized_emitters.push_back(audio_emitter->to_dictionary());
	}
	khr_audio_emitter_ext["audio"] = serialized_audio_data;
	khr_audio_emitter_ext["sources"] = serialized_sources;
	khr_audio_emitter_ext["emitters"] = serialized_emitters;
	return OK;
}

Error GLTFDocumentExtensionAudio::export_node(Ref<GLTFState> p_state, Ref<GLTFNode> p_gltf_node, Dictionary &r_node_json, Node *p_node) {
	Variant emitter_index = p_gltf_node->get_additional_data(StringName("GLTFAudioEmitterIndex"));
	if (emitter_index.get_type() != Variant::INT) {
		return OK;
	}
	Dictionary node_extensions = r_node_json["extensions"];
	Dictionary khr_audio_emitter_ext = node_extensions.get_or_add("KHR_audio_emitter", Dictionary());
	khr_audio_emitter_ext["emitter"] = emitter_index;
	return OK;
}

Error GLTFDocumentExtensionAudio::export_post(Ref<GLTFState> p_state) {
	Array scene_audio_emitter_indices = p_state->get_additional_data(StringName("GLTFSceneAudioEmitterIndices"));
	if (scene_audio_emitter_indices.is_empty()) {
		return OK;
	}
	Dictionary scene_dictionary = _get_main_scene_dictionary(p_state);
	Dictionary scene_extensions = scene_dictionary.get_or_add("extensions", Dictionary());
	Dictionary khr_audio_emitter_ext = scene_extensions.get_or_add("KHR_audio_emitter", Dictionary());
	khr_audio_emitter_ext["emitters"] = scene_audio_emitter_indices;
	return OK;
}
