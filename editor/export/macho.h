/**************************************************************************/
/*  macho.h                                                               */
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

#ifndef MACHO_H
#define MACHO_H

// Mach-O binary object file format parser and editor.

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"

class MachO : public RefCounted {
public:
	struct MachHeader {
		uint32_t cputype;
		uint32_t cpusubtype;
		uint32_t filetype;
		uint32_t ncmds;
		uint32_t sizeofcmds;
		uint32_t flags;
	};

	enum LoadCommandID {
		LC_SEGMENT = 0x00000001,
		LC_SYMTAB = 0x00000002,
		LC_SYMSEG = 0x00000003,
		LC_THREAD = 0x00000004,
		LC_UNIXTHREAD = 0x00000005,
		LC_LOADFVMLIB = 0x00000006,
		LC_IDFVMLIB = 0x00000007,
		LC_IDENT = 0x00000008,
		LC_FVMFILE = 0x00000009,
		LC_PREPAGE = 0x0000000a,
		LC_DYSYMTAB = 0x0000000b,
		LC_LOAD_DYLIB = 0x0000000c,
		LC_ID_DYLIB = 0x0000000d,
		LC_LOAD_DYLINKER = 0x0000000e,
		LC_ID_DYLINKER = 0x0000000f,
		LC_PREBOUND_DYLIB = 0x00000010,
		LC_ROUTINES = 0x00000011,
		LC_SUB_FRAMEWORK = 0x00000012,
		LC_SUB_UMBRELLA = 0x00000013,
		LC_SUB_CLIENT = 0x00000014,
		LC_SUB_LIBRARY = 0x00000015,
		LC_TWOLEVEL_HINTS = 0x00000016,
		LC_PREBIND_CKSUM = 0x00000017,
		LC_LOAD_WEAK_DYLIB = 0x80000018,
		LC_SEGMENT_64 = 0x00000019,
		LC_ROUTINES_64 = 0x0000001a,
		LC_UUID = 0x0000001b,
		LC_RPATH = 0x8000001c,
		LC_CODE_SIGNATURE = 0x0000001d,
		LC_SEGMENT_SPLIT_INFO = 0x0000001e,
		LC_REEXPORT_DYLIB = 0x8000001f,
		LC_LAZY_LOAD_DYLIB = 0x00000020,
		LC_ENCRYPTION_INFO = 0x00000021,
		LC_DYLD_INFO = 0x00000022,
		LC_DYLD_INFO_ONLY = 0x80000022,
		LC_LOAD_UPWARD_DYLIB = 0x80000023,
		LC_VERSION_MIN_MACOSX = 0x00000024,
		LC_VERSION_MIN_IPHONEOS = 0x00000025,
		LC_FUNCTION_STARTS = 0x00000026,
		LC_DYLD_ENVIRONMENT = 0x00000027,
		LC_MAIN = 0x80000028,
		LC_DATA_IN_CODE = 0x00000029,
		LC_SOURCE_VERSION = 0x0000002a,
		LC_DYLIB_CODE_SIGN_DRS = 0x0000002b,
		LC_ENCRYPTION_INFO_64 = 0x0000002c,
		LC_LINKER_OPTION = 0x0000002d,
		LC_LINKER_OPTIMIZATION_HINT = 0x0000002e,
		LC_VERSION_MIN_TVOS = 0x0000002f,
		LC_VERSION_MIN_WATCHOS = 0x00000030,
		LC_BUILD_VERSION = 0x00000032,
	};

	enum PlatformID {
		PLATFORM_UNKNOWN = 0,
		PLATFORM_MACOS = 1,
		PLATFORM_IOS = 2,
		PLATFORM_TVOS = 3,
		PLATFORM_WATCHOS = 4,
		PLATFORM_BRIDGEOS = 5,
		PLATFORM_MACCATALYST = 6,
		PLATFORM_IOSSIMULATOR = 7,
		PLATFORM_TVOSSIMULATOR = 8,
		PLATFORM_WATCHOSSIMULATOR = 9,
		PLATFORM_DRIVERKIT = 10,
	};

	struct LoadCommandHeader {
		uint32_t cmd;
		uint32_t cmdsize;
	};

	struct LoadCommandSegment {
		char segname[16];
		uint32_t vmaddr;
		uint32_t vmsize;
		uint32_t fileoff;
		uint32_t filesize;
		uint32_t maxprot;
		uint32_t initprot;
		uint32_t nsects;
		uint32_t flags;
	};

	struct LoadCommandSegment64 {
		char segname[16];
		uint64_t vmaddr;
		uint64_t vmsize;
		uint64_t fileoff;
		uint64_t filesize;
		uint32_t maxprot;
		uint32_t initprot;
		uint32_t nsects;
		uint32_t flags;
	};

	struct Section {
		char sectname[16];
		char segname[16];
		uint32_t addr;
		uint32_t size;
		uint32_t offset;
		uint32_t align;
		uint32_t reloff;
		uint32_t nreloc;
		uint32_t flags;
		uint32_t reserved1;
		uint32_t reserved2;
	};

	struct Section64 {
		char sectname[16];
		char segname[16];
		uint64_t addr;
		uint64_t size;
		uint32_t offset;
		uint32_t align;
		uint32_t reloff;
		uint32_t nreloc;
		uint32_t flags;
		uint32_t reserved1;
		uint32_t reserved2;
		uint32_t reserved3;
	};

private:
	Ref<FileAccess> fa;
	bool swap = false;

	uint64_t lc_limit = 0;

	uint64_t exe_limit = 0;
	uint64_t exe_base = std::numeric_limits<uint64_t>::max(); // Start of first __text section.
	uint32_t align = 0;
	uint32_t cputype = 0;
	uint32_t cpusubtype = 0;

	uint64_t link_edit_offset = 0; // __LINKEDIT segment offset.
	uint64_t signature_offset = 0; // Load command offset.

	uint32_t seg_align(uint64_t p_vmaddr, uint32_t p_min, uint32_t p_max);
	bool alloc_signature(uint64_t p_size);

	static inline size_t PAD(size_t s, size_t a) {
		return (a - s % a);
	}

public:
	static bool is_macho(const String &p_path);
	static uint32_t get_filetype(const String &p_path);

	bool open_file(const String &p_path);

	uint64_t get_exe_base();
	uint64_t get_exe_limit();
	int32_t get_align();
	uint32_t get_cputype();
	uint32_t get_cpusubtype();
	uint64_t get_size();
	uint64_t get_code_limit();

	uint64_t get_signature_offset();
	bool is_signed();

	PackedByteArray get_cdhash_sha1();
	PackedByteArray get_cdhash_sha256();

	PackedByteArray get_requirements();

	const Ref<FileAccess> get_file() const;
	Ref<FileAccess> get_file();

	uint64_t get_signature_size();
	bool set_signature_size(uint64_t p_size);
};

#endif // MACHO_H
