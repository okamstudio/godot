/**************************************************************************/
/*  file_access_compressed.h                                              */
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

#include "core/io/compression.h"
#include "core/io/file_access.h"

class FileAccessCompressed : public FileAccess {
	Compression::Mode cmode = Compression::MODE_ZSTD;
	bool writing = false;
	uint64_t write_pos = 0;
	uint8_t *write_ptr = nullptr;
	uint32_t write_buffer_size = 0;
	uint64_t write_max = 0;
	uint32_t block_size = 0;
	mutable bool read_eof = false;
	mutable bool at_end = false;

	struct ReadBlock {
		uint32_t csize;
		uint64_t offset;
	};

	mutable Vector<uint8_t> comp_buffer;
	uint8_t *read_ptr = nullptr;
	mutable uint32_t read_block = 0;
	uint32_t read_block_count = 0;
	mutable uint32_t read_block_size = 0;
	mutable uint64_t read_pos = 0;
	Vector<ReadBlock> read_blocks;
	uint64_t read_total = 0;

	String magic = "GCMP";
	mutable Vector<uint8_t> buffer;
	Ref<FileAccess> f;

	void _close();

public:
	void configure(const String &p_magic, Compression::Mode p_mode = Compression::MODE_ZSTD, uint32_t p_block_size = 4096);

	Error open_after_magic(Ref<FileAccess> p_base);

	virtual Error open_internal(const String &p_path, int p_mode_flags) override; ///< open a file
	virtual bool is_open() const override; ///< true when file is open

	virtual String get_path() const override; /// returns the path for the current open file
	virtual String get_path_absolute() const override; /// returns the absolute path for the current open file

	virtual void seek(uint64_t p_position) override; ///< seek to a given position
	virtual void seek_end(int64_t p_position = 0) override; ///< seek from the end of file
	virtual uint64_t get_position() const override; ///< get position in the file
	virtual uint64_t get_length() const override; ///< get size of the file

	virtual bool eof_reached() const override; ///< reading passed EOF

	virtual uint64_t get_buffer(uint8_t *p_dst, uint64_t p_length) const override;

	virtual Error get_error() const override; ///< get last error

	virtual Error resize(int64_t p_length) override { return ERR_UNAVAILABLE; }
	virtual void flush() override;
	virtual bool store_buffer(const uint8_t *p_src, uint64_t p_length) override;

	virtual bool file_exists(const String &p_name) override; ///< return true if a file exists

	virtual uint64_t _get_modified_time(const String &p_file) override;
	virtual BitField<FileAccess::UnixPermissionFlags> _get_unix_permissions(const String &p_file) override;
	virtual Error _set_unix_permissions(const String &p_file, BitField<FileAccess::UnixPermissionFlags> p_permissions) override;

	virtual bool _get_hidden_attribute(const String &p_file) override;
	virtual Error _set_hidden_attribute(const String &p_file, bool p_hidden) override;
	virtual bool _get_read_only_attribute(const String &p_file) override;
	virtual Error _set_read_only_attribute(const String &p_file, bool p_ro) override;

	virtual void close() override;

	FileAccessCompressed() {}
	virtual ~FileAccessCompressed();
};
