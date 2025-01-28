/**************************************************************************/
/*  image_loader_hdr.cpp                                                  */
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

#include "image_loader_hdr.h"

Error ImageLoaderHDR::load_image(Ref<Image> p_image, Ref<FileAccess> f, BitField<ImageFormatLoader::LoaderFlags> p_flags, float p_scale) {
	String header = f->get_token();

	ERR_FAIL_COND_V_MSG(header != "#?RADIANCE" && header != "#?RGBE", ERR_FILE_UNRECOGNIZED, "Unsupported header information in HDR: " + header + ".");

	while (true) {
		String line = f->get_line();
		ERR_FAIL_COND_V(f->eof_reached(), ERR_FILE_UNRECOGNIZED);
		if (line.is_empty()) { // empty line indicates end of header
			break;
		}
		if (line.begins_with("FORMAT=")) { // leave option to implement other commands
			ERR_FAIL_COND_V_MSG(line != "FORMAT=32-bit_rle_rgbe", ERR_FILE_UNRECOGNIZED, "Only 32-bit_rle_rgbe is supported for HDR files.");
		} else if (!line.begins_with("#")) { // not comment
			WARN_PRINT("Ignoring unsupported header information in HDR: " + line + ".");
		}
	}

	String token = f->get_token();

	ERR_FAIL_COND_V(token != "-Y", ERR_FILE_CORRUPT);

	int height = f->get_token().to_int();

	token = f->get_token();

	ERR_FAIL_COND_V(token != "+X", ERR_FILE_CORRUPT);

	int width = f->get_line().to_int();

	Vector<uint8_t> imgdata;

	imgdata.resize(height * width * (int)sizeof(uint32_t));

	{
		uint8_t *ptr = imgdata.ptrw();

		Vector<uint8_t> temp_read_data;
		temp_read_data.resize(128);
		uint8_t *temp_read_ptr = temp_read_data.ptrw();

		if (width < 8 || width >= 32768) {
			// Read flat data

			f->get_buffer(ptr, (uint64_t)width * height * 4);
		} else {
			// Read RLE-encoded data

			for (int j = 0; j < height; ++j) {
				int c1 = f->get_8();
				int c2 = f->get_8();
				int len = f->get_8();
				if (c1 != 2 || c2 != 2 || (len & 0x80)) {
					// not run-length encoded, so we have to actually use THIS data as a decoded
					// pixel (note this can't be a valid pixel--one of RGB must be >= 128)

					ptr[(j * width) * 4 + 0] = uint8_t(c1);
					ptr[(j * width) * 4 + 1] = uint8_t(c2);
					ptr[(j * width) * 4 + 2] = uint8_t(len);
					ptr[(j * width) * 4 + 3] = f->get_8();

					f->get_buffer(&ptr[(j * width + 1) * 4], (width - 1) * 4);
					continue;
				}
				len <<= 8;
				len |= f->get_8();

				ERR_FAIL_COND_V_MSG(len != width, ERR_FILE_CORRUPT, "Invalid decoded scanline length, corrupt HDR.");

				for (int k = 0; k < 4; ++k) {
					int i = 0;
					while (i < width) {
						int count = f->get_8();
						if (count > 128) {
							// Run
							int value = f->get_8();
							count -= 128;
							for (int z = 0; z < count; ++z) {
								ptr[(j * width + i++) * 4 + k] = uint8_t(value);
							}
						} else {
							// Dump
							f->get_buffer(temp_read_ptr, count);
							for (int z = 0; z < count; ++z) {
								ptr[(j * width + i++) * 4 + k] = temp_read_ptr[z];
							}
						}
					}
				}
			}
		}

		const bool force_linear = p_flags & FLAG_FORCE_LINEAR;

		//convert
		for (int i = 0; i < width * height; i++) {
			int e = ptr[3] - 128;

			if (force_linear || (e < -15 || e > 15)) {
				float exp = pow(2.0f, e);
				Color c(ptr[0] * exp / 255.0, ptr[1] * exp / 255.0, ptr[2] * exp / 255.0);

				if (force_linear) {
					c = c.srgb_to_linear();
				}

				*(uint32_t *)ptr = c.to_rgbe9995();
			} else {
				// https://github.com/george-steel/rgbe-rs/blob/e7cc33b7f42b4eb3272c166dac75385e48687c92/src/types.rs#L123-L129
				uint32_t e5 = (uint32_t)(e + 15);
				*(uint32_t *)ptr = ((e5 << 27) | ((uint32_t)ptr[2] << 19) | ((uint32_t)ptr[1] << 10) | ((uint32_t)ptr[0] << 1));
			}

			ptr += 4;
		}
	}

	p_image->set_data(width, height, false, Image::FORMAT_RGBE9995, imgdata);

	return OK;
}

void ImageLoaderHDR::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("hdr");
}

ImageLoaderHDR::ImageLoaderHDR() {
}
