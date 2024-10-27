/**************************************************************************/
/*  resource_importer_wav.cpp                                             */
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

#include "resource_importer_wav.h"

#include "core/io/file_access.h"
#include "core/io/marshalls.h"
#include "core/io/resource_saver.h"
#include "scene/resources/audio_stream_wav.h"

const float TRIM_DB_LIMIT = -50;
const int TRIM_FADE_OUT_FRAMES = 500;

String ResourceImporterWAV::get_importer_name() const {
	return "wav";
}

String ResourceImporterWAV::get_visible_name() const {
	return "Microsoft WAV";
}

void ResourceImporterWAV::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("wav");
}

String ResourceImporterWAV::get_save_extension() const {
	return "sample";
}

String ResourceImporterWAV::get_resource_type() const {
	return "AudioStreamWAV";
}

bool ResourceImporterWAV::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_option == "force/max_rate_hz" && !bool(p_options["force/max_rate"])) {
		return false;
	}

	// Don't show begin/end loop points if loop mode is auto-detected or disabled.
	if ((int)p_options["edit/loop_mode"] < 2 && (p_option == "edit/loop_begin" || p_option == "edit/loop_end")) {
		return false;
	}

	return true;
}

int ResourceImporterWAV::get_preset_count() const {
	return 0;
}

String ResourceImporterWAV::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterWAV::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/8_bit"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/mono"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "force/max_rate", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "force/max_rate_hz", PROPERTY_HINT_RANGE, "11025,192000,1,exp"), 44100));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "edit/trim"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "edit/normalize"), false));
	// Keep the `edit/loop_mode` enum in sync with AudioStreamWAV::LoopMode (note: +1 offset due to "Detect From WAV").
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_mode", PROPERTY_HINT_ENUM, "Detect From WAV,Disabled,Forward,Ping-Pong,Backward", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_begin"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "edit/loop_end"), -1));
	// Quite OK Audio is lightweight enough and supports virtually every significant AudioStreamWAV feature.
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "PCM (Uncompressed),IMA ADPCM,Quite OK Audio"), 2));
}

Error ResourceImporterWAV::import(const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	/* STEP 1, READ WAVE FILE */

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_OPEN, "Cannot open file '" + p_source_file + "'.");

	/* CHECK RIFF */
	char riff[5];
	riff[4] = 0;
	file->get_buffer((uint8_t *)&riff, 4); //RIFF

	if (riff[0] != 'R' || riff[1] != 'I' || riff[2] != 'F' || riff[3] != 'F') {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("Not a WAV file. File should start with 'RIFF', but found '%s', in file of size %d bytes", riff, file->get_length()));
	}

	/* GET FILESIZE */

	// The file size in header is 8 bytes less than the actual size.
	// See https://docs.fileformat.com/audio/wav/
	const int FILE_SIZE_HEADER_OFFSET = 8;
	uint32_t file_size_header = file->get_32() + FILE_SIZE_HEADER_OFFSET;
	uint64_t file_size = file->get_length();
	if (file_size != file_size_header) {
		WARN_PRINT(vformat("File size %d is %s than the expected size %d. (%s)", file_size, file_size > file_size_header ? "larger" : "smaller", file_size_header, p_source_file));
	}

	/* CHECK WAVE */

	char wave[5];
	wave[4] = 0;
	file->get_buffer((uint8_t *)&wave, 4); //WAVE

	if (wave[0] != 'W' || wave[1] != 'A' || wave[2] != 'V' || wave[3] != 'E') {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, vformat("Not a WAV file. Header should contain 'WAVE', but found '%s', in file of size %d bytes", wave, file->get_length()));
	}

	// Let users override potential loop points from the WAV.
	// We parse the WAV loop points only with "Detect From WAV" (0).
	int import_loop_mode = p_options["edit/loop_mode"];

	int format_bits = 0;
	int format_channels = 0;

	AudioStreamWAV::LoopMode loop_mode = AudioStreamWAV::LOOP_DISABLED;
	uint16_t compression_code = 1;
	bool format_found = false;
	bool data_found = false;
	int format_freq = 0;
	int loop_begin = 0;
	int loop_end = 0;
	int frames = 0;

	Vector<float> data;

	while (!file->eof_reached()) {
		/* chunk */
		char chunkID[4];
		file->get_buffer((uint8_t *)&chunkID, 4); //RIFF

		/* chunk size */
		uint32_t chunksize = file->get_32();
		uint32_t file_pos = file->get_position(); //save file pos, so we can skip to next chunk safely

		if (file->eof_reached()) {
			//ERR_PRINT("EOF REACH");
			break;
		}

		if (chunkID[0] == 'f' && chunkID[1] == 'm' && chunkID[2] == 't' && chunkID[3] == ' ' && !format_found) {
			/* IS FORMAT CHUNK */

			//Issue: #7755 : Not a bug - usage of other formats (format codes) are unsupported in current importer version.
			//Consider revision for engine version 3.0
			compression_code = file->get_16();
			if (compression_code != 1 && compression_code != 3) {
				ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Format not supported for WAVE file (not PCM). Save WAVE files as uncompressed PCM or IEEE float instead.");
			}

			format_channels = file->get_16();
			if (format_channels != 1 && format_channels != 2) {
				ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Format not supported for WAVE file (not stereo or mono).");
			}

			format_freq = file->get_32(); //sampling rate

			file->get_32(); // average bits/second (unused)
			file->get_16(); // block align (unused)
			format_bits = file->get_16(); // bits per sample

			if (format_bits % 8 || format_bits == 0) {
				ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid amount of bits in the sample (should be one of 8, 16, 24 or 32).");
			}

			if (compression_code == 3 && format_bits % 32) {
				ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid amount of bits in the IEEE float sample (should be 32 or 64).");
			}

			/* Don't need anything else, continue */
			format_found = true;
		}

		if (chunkID[0] == 'd' && chunkID[1] == 'a' && chunkID[2] == 't' && chunkID[3] == 'a' && !data_found) {
			/* IS DATA CHUNK */
			data_found = true;

			if (!format_found) {
				ERR_PRINT("'data' chunk before 'format' chunk found.");
				break;
			}

			uint64_t remaining_bytes = file_size - file_pos;
			frames = chunksize;
			if (remaining_bytes < chunksize) {
				WARN_PRINT(vformat("Data chunk size is smaller than expected. Proceeding with actual data size. (%s)", p_source_file));
				frames = remaining_bytes;
			}

			if (format_channels == 0) {
				ERR_FAIL_COND_V(format_channels == 0, ERR_INVALID_DATA);
			}
			frames /= format_channels;
			frames /= (format_bits >> 3);

			/*print_line("chunksize: "+itos(chunksize));
			print_line("channels: "+itos(format_channels));
			print_line("bits: "+itos(format_bits));
			*/

			data.resize(frames * format_channels);

			if (compression_code == 1) {
				if (format_bits == 8) {
					for (int i = 0; i < frames * format_channels; i++) {
						// 8 bit samples are UNSIGNED

						data.write[i] = int8_t(file->get_8() - 128) / 128.f;
					}
				} else if (format_bits == 16) {
					for (int i = 0; i < frames * format_channels; i++) {
						//16 bit SIGNED

						data.write[i] = int16_t(file->get_16()) / 32768.f;
					}
				} else {
					for (int i = 0; i < frames * format_channels; i++) {
						//16+ bits samples are SIGNED
						// if sample is > 16 bits, just read extra bytes

						uint32_t s = 0;
						for (int b = 0; b < (format_bits >> 3); b++) {
							s |= ((uint32_t)file->get_8()) << (b * 8);
						}
						s <<= (32 - format_bits);

						data.write[i] = (int32_t(s) >> 16) / 32768.f;
					}
				}
			} else if (compression_code == 3) {
				if (format_bits == 32) {
					for (int i = 0; i < frames * format_channels; i++) {
						//32 bit IEEE Float

						data.write[i] = file->get_float();
					}
				} else if (format_bits == 64) {
					for (int i = 0; i < frames * format_channels; i++) {
						//64 bit IEEE Float

						data.write[i] = file->get_double();
					}
				}
			}

			if (file->eof_reached()) {
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Premature end of file.");
			}
		}

		if (import_loop_mode == 0 && chunkID[0] == 's' && chunkID[1] == 'm' && chunkID[2] == 'p' && chunkID[3] == 'l') {
			// Loop point info!

			/**
			 *	Consider exploring next document:
			 *		http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/Docs/RIFFNEW.pdf
			 *	Especially on page:
			 *		16 - 17
			 *	Timestamp:
			 *		22:38 06.07.2017 GMT
			 **/

			for (int i = 0; i < 10; i++) {
				file->get_32(); // i wish to know why should i do this... no doc!
			}

			// only read 0x00 (loop forward), 0x01 (loop ping-pong) and 0x02 (loop backward)
			// Skip anything else because it's not supported, reserved for future uses or sampler specific
			// from https://sites.google.com/site/musicgapi/technical-documents/wav-file-format#smpl (loop type values table)
			int loop_type = file->get_32();
			if (loop_type == 0x00 || loop_type == 0x01 || loop_type == 0x02) {
				if (loop_type == 0x00) {
					loop_mode = AudioStreamWAV::LOOP_FORWARD;
				} else if (loop_type == 0x01) {
					loop_mode = AudioStreamWAV::LOOP_PINGPONG;
				} else if (loop_type == 0x02) {
					loop_mode = AudioStreamWAV::LOOP_BACKWARD;
				}
				loop_begin = file->get_32();
				loop_end = file->get_32();
			}
		}
		// Move to the start of the next chunk. Note that RIFF requires a padding byte for odd
		// chunk sizes.
		file->seek(file_pos + chunksize + (chunksize & 1));
	}

	// STEP 2, APPLY CONVERSIONS

	bool is16 = format_bits != 8;
	int rate = format_freq;

	/*
	print_line("Input Sample: ");
	print_line("\tframes: " + itos(frames));
	print_line("\tformat_channels: " + itos(format_channels));
	print_line("\t16bits: " + itos(is16));
	print_line("\trate: " + itos(rate));
	print_line("\tloop: " + itos(loop));
	print_line("\tloop begin: " + itos(loop_begin));
	print_line("\tloop end: " + itos(loop_end));
	*/

	//apply frequency limit

	bool limit_rate = p_options["force/max_rate"];
	int limit_rate_hz = p_options["force/max_rate_hz"];
	if (limit_rate && rate > limit_rate_hz && rate > 0 && frames > 0) {
		// resample!
		int new_data_frames = (int)(frames * (float)limit_rate_hz / (float)rate);

		Vector<float> new_data;
		new_data.resize(new_data_frames * format_channels);
		for (int c = 0; c < format_channels; c++) {
			float frac = .0f;
			int ipos = 0;

			for (int i = 0; i < new_data_frames; i++) {
				// Cubic interpolation should be enough.

				float y0 = data[MAX(0, ipos - 1) * format_channels + c];
				float y1 = data[ipos * format_channels + c];
				float y2 = data[MIN(frames - 1, ipos + 1) * format_channels + c];
				float y3 = data[MIN(frames - 1, ipos + 2) * format_channels + c];

				new_data.write[i * format_channels + c] = Math::cubic_interpolate(y1, y2, y0, y3, frac);

				// update position and always keep fractional part within ]0...1]
				// in order to avoid 32bit floating point precision errors

				frac += (float)rate / (float)limit_rate_hz;
				int tpos = (int)Math::floor(frac);
				ipos += tpos;
				frac -= tpos;
			}
		}

		if (loop_mode) {
			loop_begin = (int)(loop_begin * (float)new_data_frames / (float)frames);
			loop_end = (int)(loop_end * (float)new_data_frames / (float)frames);
		}

		data = new_data;
		rate = limit_rate_hz;
		frames = new_data_frames;
	}

	bool normalize = p_options["edit/normalize"];

	if (normalize) {
		float max = 0;
		for (int i = 0; i < data.size(); i++) {
			float amp = Math::abs(data[i]);
			if (amp > max) {
				max = amp;
			}
		}

		if (max > 0) {
			float mult = 1.0 / max;
			for (int i = 0; i < data.size(); i++) {
				data.write[i] *= mult;
			}
		}
	}

	bool trim = p_options["edit/trim"];

	if (trim && (loop_mode == AudioStreamWAV::LOOP_DISABLED) && format_channels > 0) {
		int first = 0;
		int last = (frames / format_channels) - 1;
		bool found = false;
		float limit = Math::db_to_linear(TRIM_DB_LIMIT);

		for (int i = 0; i < data.size() / format_channels; i++) {
			float ampChannelSum = 0;
			for (int j = 0; j < format_channels; j++) {
				ampChannelSum += Math::abs(data[(i * format_channels) + j]);
			}

			float amp = Math::abs(ampChannelSum / (float)format_channels);

			if (!found && amp > limit) {
				first = i;
				found = true;
			}

			if (found && amp > limit) {
				last = i;
			}
		}

		if (first < last) {
			Vector<float> new_data;
			new_data.resize((last - first) * format_channels);
			for (int i = first; i < last; i++) {
				float fadeOutMult = 1;

				if (last - i < TRIM_FADE_OUT_FRAMES) {
					fadeOutMult = ((float)(last - i - 1) / (float)TRIM_FADE_OUT_FRAMES);
				}

				for (int j = 0; j < format_channels; j++) {
					new_data.write[((i - first) * format_channels) + j] = data[(i * format_channels) + j] * fadeOutMult;
				}
			}

			data = new_data;
			frames = data.size() / format_channels;
		}
	}

	if (import_loop_mode >= 2) {
		loop_mode = (AudioStreamWAV::LoopMode)(import_loop_mode - 1);
		loop_begin = p_options["edit/loop_begin"];
		loop_end = p_options["edit/loop_end"];
		// Wrap around to max frames, so `-1` can be used to select the end, etc.
		if (loop_begin < 0) {
			loop_begin = CLAMP(loop_begin + frames, 0, frames - 1);
		}
		if (loop_end < 0) {
			loop_end = CLAMP(loop_end + frames, 0, frames - 1);
		}
	}

	int compression = p_options["compress/mode"];
	bool force_mono = p_options["force/mono"];

	if (force_mono && format_channels == 2) {
		Vector<float> new_data;
		new_data.resize(data.size() / 2);
		for (int i = 0; i < frames; i++) {
			new_data.write[i] = (data[i * 2 + 0] + data[i * 2 + 1]) / 2.0;
		}

		data = new_data;
		format_channels = 1;
	}

	bool force_8_bit = p_options["force/8_bit"];
	if (force_8_bit) {
		is16 = false;
	}

	Vector<uint8_t> pcm_data;
	AudioStreamWAV::Format dst_format;

	if (compression == 1) {
		dst_format = AudioStreamWAV::FORMAT_IMA_ADPCM;
		if (format_channels == 1) {
			_compress_ima_adpcm(data, pcm_data);
		} else {
			//byte interleave
			Vector<float> left;
			Vector<float> right;

			int tframes = data.size() / 2;
			left.resize(tframes);
			right.resize(tframes);

			for (int i = 0; i < tframes; i++) {
				left.write[i] = data[i * 2 + 0];
				right.write[i] = data[i * 2 + 1];
			}

			Vector<uint8_t> bleft;
			Vector<uint8_t> bright;

			_compress_ima_adpcm(left, bleft);
			_compress_ima_adpcm(right, bright);

			int dl = bleft.size();
			pcm_data.resize(dl * 2);

			uint8_t *w = pcm_data.ptrw();
			const uint8_t *rl = bleft.ptr();
			const uint8_t *rr = bright.ptr();

			for (int i = 0; i < dl; i++) {
				w[i * 2 + 0] = rl[i];
				w[i * 2 + 1] = rr[i];
			}
		}

	} else {
		dst_format = is16 ? AudioStreamWAV::FORMAT_16_BITS : AudioStreamWAV::FORMAT_8_BITS;
		bool enforce16 = is16 || compression == 2;
		pcm_data.resize(data.size() * (enforce16 ? 2 : 1));
		{
			uint8_t *w = pcm_data.ptrw();

			int ds = data.size();
			for (int i = 0; i < ds; i++) {
				if (enforce16) {
					int16_t v = CLAMP(data[i] * 32768, -32768, 32767);
					encode_uint16(v, &w[i * 2]);
				} else {
					int8_t v = CLAMP(data[i] * 128, -128, 127);
					w[i] = v;
				}
			}
		}
	}

	Vector<uint8_t> dst_data;
	if (compression == 2) {
		dst_format = AudioStreamWAV::FORMAT_QOA;
		qoa_desc desc = {};
		uint32_t qoa_len = 0;

		desc.samplerate = rate;
		desc.samples = frames;
		desc.channels = format_channels;

		void *encoded = qoa_encode((short *)pcm_data.ptr(), &desc, &qoa_len);
		if (encoded) {
			dst_data.resize(qoa_len);
			memcpy(dst_data.ptrw(), encoded, qoa_len);
			QOA_FREE(encoded);
		}
	} else {
		dst_data = pcm_data;
	}

	Ref<AudioStreamWAV> sample;
	sample.instantiate();
	sample->set_data(dst_data);
	sample->set_format(dst_format);
	sample->set_mix_rate(rate);
	sample->set_loop_mode(loop_mode);
	sample->set_loop_begin(loop_begin);
	sample->set_loop_end(loop_end);
	sample->set_stereo(format_channels == 2);

	ResourceSaver::save(sample, p_save_path + ".sample");

	return OK;
}

ResourceImporterWAV::ResourceImporterWAV() {
}
