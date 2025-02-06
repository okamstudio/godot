/**************************************************************************/
/*  gltf_document_extension_texture_webp.h                                */
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

#pragma once

#include "gltf_document_extension.h"

class GLTFDocumentExtensionTextureWebP : public GLTFDocumentExtension {
	GDCLASS(GLTFDocumentExtensionTextureWebP, GLTFDocumentExtension);

public:
	// Import process.
	Error import_preflight(Ref<GLTFState> p_state, Vector<String> p_extensions) override;
	Vector<String> get_supported_extensions() override;
	Error parse_image_data(Ref<GLTFState> p_state, const PackedByteArray &p_image_data, const String &p_mime_type, Ref<Image> r_image) override;
	String get_image_file_extension() override;
	Error parse_texture_json(Ref<GLTFState> p_state, const Dictionary &p_texture_json, Ref<GLTFTexture> r_gltf_texture) override;
	// Export process.
	Vector<String> get_saveable_image_formats() override;
	PackedByteArray serialize_image_to_bytes(Ref<GLTFState> p_state, Ref<Image> p_image, Dictionary p_image_dict, const String &p_image_format, float p_lossy_quality) override;
	Error save_image_at_path(Ref<GLTFState> p_state, Ref<Image> p_image, const String &p_full_path, const String &p_image_format, float p_lossy_quality) override;
	Error serialize_texture_json(Ref<GLTFState> p_state, Dictionary p_texture_json, Ref<GLTFTexture> p_gltf_texture, const String &p_image_format) override;
};
