/**************************************************************************/
/*  pck_packer.cpp                                                        */
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

#include "pck_packer.h"

#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/file_access_pack.h" // PACK_HEADER_MAGIC, PACK_FORMAT_VERSION
#include "core/version.h"

static int _get_pad(int p_alignment, int p_n) {
	int rest = p_n % p_alignment;
	int pad = 0;
	if (rest > 0) {
		pad = p_alignment - rest;
	}

	return pad;
}

void PCKPacker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("pck_start", "pck_name", "alignment", "key", "encrypt_directory"), &PCKPacker::pck_start, DEFVAL(32), DEFVAL("0000000000000000000000000000000000000000000000000000000000000000"), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_file", "pck_path", "source_path", "encrypt"), &PCKPacker::add_file, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("add_file_removal", "target_path"), &PCKPacker::add_file_removal);
	ClassDB::bind_method(D_METHOD("flush", "verbose"), &PCKPacker::flush, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("flush_and_sign", "key", "verbose"), &PCKPacker::flush_and_sign, DEFVAL(false));
}

Error PCKPacker::pck_start(const String &p_pck_path, int p_alignment, const String &p_key, bool p_encrypt_directory) {
	ERR_FAIL_COND_V_MSG((p_key.is_empty() || !p_key.is_valid_hex_number(false) || p_key.length() != 64), ERR_CANT_CREATE, "Invalid Encryption Key (must be 64 characters long).");
	ERR_FAIL_COND_V_MSG(p_alignment <= 0, ERR_CANT_CREATE, "Invalid alignment, must be greater then 0.");

	String _key = p_key.to_lower();
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		int v = 0;
		if (i * 2 < _key.length()) {
			char32_t ct = _key[i * 2];
			if (is_digit(ct)) {
				ct = ct - '0';
			} else if (ct >= 'a' && ct <= 'f') {
				ct = 10 + ct - 'a';
			}
			v |= ct << 4;
		}

		if (i * 2 + 1 < _key.length()) {
			char32_t ct = _key[i * 2 + 1];
			if (is_digit(ct)) {
				ct = ct - '0';
			} else if (ct >= 'a' && ct <= 'f') {
				ct = 10 + ct - 'a';
			}
			v |= ct;
		}
		key.write[i] = v;
	}
	enc_dir = p_encrypt_directory;

	file = FileAccess::open(p_pck_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_CANT_CREATE, vformat("Can't open file to write: '%s'.", String(p_pck_path)));

	alignment = p_alignment;

	file->store_32(PACK_HEADER_MAGIC);
	file->store_32(PACK_FORMAT_VERSION);
	file->store_32(VERSION_MAJOR);
	file->store_32(VERSION_MINOR);
	file->store_32(VERSION_PATCH);

	uint32_t pack_flags = 0;
	if (enc_dir) {
		pack_flags |= PACK_DIR_ENCRYPTED;
	}
	file->store_32(pack_flags); // flags

	files.clear();
	ofs = 0;

	return OK;
}

Error PCKPacker::add_file_removal(const String &p_target_path) {
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_INVALID_PARAMETER, "File must be opened before use.");

	File pf;
	// Simplify path here and on every 'files' access so that paths that have extra '/'
	// symbols or 'res://' in them still match the MD5 hash for the saved path.
	pf.path = p_target_path.simplify_path().trim_prefix("res://");
	pf.ofs = ofs;
	pf.size = 0;
	pf.removal = true;

	pf.sha256.resize(32);
	pf.sha256.fill(0);

	files.push_back(pf);

	return OK;
}

Error PCKPacker::add_file(const String &p_target_path, const String &p_source_path, bool p_encrypt) {
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_INVALID_PARAMETER, "File must be opened before use.");

	Ref<FileAccess> f = FileAccess::open(p_source_path, FileAccess::READ);
	if (f.is_null()) {
		return ERR_FILE_CANT_OPEN;
	}

	File pf;
	// Simplify path here and on every 'files' access so that paths that have extra '/'
	// symbols or 'res://' in them still match the MD5 hash for the saved path.
	pf.path = p_target_path.simplify_path().trim_prefix("res://");
	pf.src_path = p_source_path;
	pf.ofs = ofs;
	pf.size = f->get_length();

	Vector<uint8_t> data = FileAccess::get_file_as_bytes(p_source_path);
	{
		pf.sha256.resize(32);
		CryptoCore::sha256(data.ptr(), data.size(), pf.sha256.ptrw());
	}
	pf.encrypted = p_encrypt;

	uint64_t _size = pf.size;
	if (p_encrypt) { // Add encryption overhead.
		if (_size % 16) { // Pad to encryption block size.
			_size += 16 - (_size % 16);
		}
		_size += 16; // hash
		_size += 8; // data size
		_size += 16; // iv
	}

	int pad = _get_pad(alignment, ofs + _size);
	ofs = ofs + _size + pad;

	files.push_back(pf);

	return OK;
}

Error PCKPacker::flush(bool p_verbose) {
	return flush_and_sign(Ref<CryptoKey>(), p_verbose);
}

Error PCKPacker::flush_and_sign(const Ref<CryptoKey> &p_sign_key, bool p_verbose) {
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_INVALID_PARAMETER, "File must be opened before use.");

	int64_t file_base_ofs = file->get_position();
	file->store_64(0); // files base

	uint64_t signature_info_ofs = file->get_position();
	file->store_64(0); // signature ofs
	file->store_64(0); // signature size

	for (int i = 0; i < 12; i++) {
		file->store_32(0); // reserved
	}

	// write the index
	file->store_32(uint32_t(files.size()));

	Ref<FileAccessEncrypted> fae;
	Ref<FileAccess> fhead = file;

	if (enc_dir) {
		fae.instantiate();
		ERR_FAIL_COND_V(fae.is_null(), ERR_CANT_CREATE);

		Error err = fae->open_and_parse(file, key, FileAccessEncrypted::MODE_WRITE_AES256, false);
		ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);

		fhead = fae;
	}

	Vector<uint8_t> directory_hash;
	directory_hash.resize(32);
	CryptoCore::SHA256Context sha_ctx;
	sha_ctx.start();

	for (int i = 0; i < files.size(); i++) {
		static uint8_t zero = 0;
		CharString utf8_string = files[i].path.utf8();
		int string_len = utf8_string.length();
		int pad = _get_pad(4, string_len);

		uint32_t full_len = string_len + pad;
		fhead->store_32(full_len);
		sha_ctx.update((const unsigned char *)&full_len, 4);
		fhead->store_buffer((const uint8_t *)utf8_string.get_data(), string_len);
		sha_ctx.update((const unsigned char *)utf8_string.get_data(), string_len);
		for (int j = 0; j < pad; j++) {
			fhead->store_8(0);
			sha_ctx.update((const unsigned char *)&zero, 1);
		}

		fhead->store_64(files[i].ofs);
		sha_ctx.update((const unsigned char *)&files[i].ofs, 8);
		fhead->store_64(files[i].size); // pay attention here, this is where file is
		sha_ctx.update((const unsigned char *)&files[i].size, 8);
		fhead->store_buffer(files[i].sha256.ptr(), 32); //also save sha256 for file
		sha_ctx.update((const unsigned char *)files[i].sha256.ptr(), 32);

		uint32_t flags = 0;
		if (files[i].encrypted) {
			flags |= PACK_FILE_ENCRYPTED;
		}
		if (files[i].removal) {
			flags |= PACK_FILE_REMOVAL;
		}
		fhead->store_32(flags);
		sha_ctx.update((const unsigned char *)&flags, 4);
	}
	sha_ctx.finish((unsigned char *)directory_hash.ptrw());

	Vector<uint8_t> signature;
	if (p_sign_key.is_valid()) {
		Ref<Crypto> tls_ctx = Crypto::create();

		ERR_FAIL_COND_V(tls_ctx.is_null(), ERR_CANT_CREATE);

		signature = tls_ctx->sign(HashingContext::HASH_SHA256, directory_hash, p_sign_key);
		ERR_FAIL_COND_V_MSG(signature.is_empty(), ERR_CANT_CREATE, "Pack directory signing failed.");
	}

	if (fae.is_valid()) {
		fhead.unref();
		fae.unref();
	}

	int header_padding = _get_pad(alignment, file->get_position());
	for (int i = 0; i < header_padding; i++) {
		file->store_8(0);
	}

	uint64_t file_base = file->get_position();
	file->seek(file_base_ofs);
	file->store_64(file_base); // update files base
	file->seek(file_base);

	const uint32_t buf_max = 65536;
	uint8_t *buf = memnew_arr(uint8_t, buf_max);

	int count = 0;
	for (int i = 0; i < files.size(); i++) {
		if (files[i].removal) {
			continue;
		}

		Ref<FileAccess> src = FileAccess::open(files[i].src_path, FileAccess::READ);
		uint64_t to_write = files[i].size;

		Ref<FileAccess> ftmp = file;
		if (files[i].encrypted) {
			fae.instantiate();
			ERR_FAIL_COND_V(fae.is_null(), ERR_CANT_CREATE);

			Error err = fae->open_and_parse(file, key, FileAccessEncrypted::MODE_WRITE_AES256, false);
			ERR_FAIL_COND_V(err != OK, ERR_CANT_CREATE);
			ftmp = fae;
		}

		while (to_write > 0) {
			uint64_t read = src->get_buffer(buf, MIN(to_write, buf_max));
			ftmp->store_buffer(buf, read);
			to_write -= read;
		}

		if (fae.is_valid()) {
			ftmp.unref();
			fae.unref();
		}

		int pad = _get_pad(alignment, file->get_position());
		for (int j = 0; j < pad; j++) {
			file->store_8(0);
		}

		count += 1;
		const int file_num = files.size();
		if (p_verbose && (file_num > 0)) {
			print_line(vformat("[%d/%d - %d%%] PCKPacker flush: %s -> %s", count, file_num, float(count) / file_num * 100, files[i].src_path, files[i].path));
		}
	}

	if (p_sign_key.is_valid()) {
		int signature_padding = _get_pad(alignment, file->get_position());
		for (int i = 0; i < signature_padding; i++) {
			file->store_8(0);
		}
		uint64_t signature_offset = file->get_position();
		file->store_buffer(signature);
		uint64_t signature_end = file->get_position();

		// Update signature offset and size.
		file->seek(signature_info_ofs);
		file->store_64(signature_offset);
		file->store_64(signature.size());
		file->seek(signature_end);
	}

	file.unref();
	memdelete_arr(buf);

	return OK;
}
