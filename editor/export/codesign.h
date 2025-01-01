/**************************************************************************/
/*  codesign.h                                                            */
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

#ifndef CODESIGN_H
#define CODESIGN_H

// macOS code signature creation utility.
//
// Current implementation has the following limitation:
//  - Only version 11.3.0 signatures are supported.
//  - Only "framework" and "app" bundle types are supported.
//  - Page hash array scattering is not supported.
//  - Reading and writing binary property lists i snot supported (third-party frameworks with binary Info.plist will not work unless .plist is converted to text format).
//  - Requirements code generator is not implemented (only hard-coded requirements for the ad-hoc signing is supported).
//  - RFC5652/CMS blob generation is not implemented, supports ad-hoc signing only.

#include "core/io/file_access.h"
#include "core/object/ref_counted.h"

#include "modules/modules_enabled.gen.h" // For regex.

#ifdef MODULE_REGEX_ENABLED

/*************************************************************************/
/* CodeSignCodeResources                                                 */
/*************************************************************************/

class CodeSignCodeResources {
public:
	enum class CRMatch {
		CR_MATCH_NO,
		CR_MATCH_YES,
		CR_MATCH_NESTED,
		CR_MATCH_OPTIONAL,
	};

private:
	struct CRFile {
		String name;
		String hash;
		String hash2;
		bool optional;
		bool nested;
		String requirements;
	};

	struct CRRule {
		String file_pattern;
		String key;
		int weight;
		bool store;
		CRRule() {
			weight = 1;
			store = true;
		}
		CRRule(const String &p_file_pattern, const String &p_key, int p_weight, bool p_store) {
			file_pattern = p_file_pattern;
			key = p_key;
			weight = p_weight;
			store = p_store;
		}
	};

	Vector<CRRule> rules1;
	Vector<CRRule> rules2;

	Vector<CRFile> files1;
	Vector<CRFile> files2;

	String hash_sha1_base64(const String &p_path);
	String hash_sha256_base64(const String &p_path);

public:
	void add_rule1(const String &p_rule, const String &p_key = "", int p_weight = 0, bool p_store = true);
	void add_rule2(const String &p_rule, const String &p_key = "", int p_weight = 0, bool p_store = true);

	CRMatch match_rules1(const String &p_path) const;
	CRMatch match_rules2(const String &p_path) const;

	bool add_file1(const String &p_root, const String &p_path);
	bool add_file2(const String &p_root, const String &p_path);
	bool add_nested_file(const String &p_root, const String &p_path, const String &p_exepath);

	bool add_folder_recursive(const String &p_root, const String &p_path = "", const String &p_main_exe_path = "");

	bool save_to_file(const String &p_path);
};

/*************************************************************************/
/* CodeSignBlob                                                          */
/*************************************************************************/

class CodeSignBlob : public RefCounted {
public:
	virtual PackedByteArray get_hash_sha1() const = 0;
	virtual PackedByteArray get_hash_sha256() const = 0;

	virtual int get_size() const = 0;
	virtual uint32_t get_index_type() const = 0;

	virtual void write_to_file(Ref<FileAccess> p_file) const = 0;
};

/*************************************************************************/
/* CodeSignRequirements                                                  */
/*************************************************************************/

// Note: Proper code generator is not implemented (any we probably won't ever need it), just a hardcoded bytecode for the limited set of cases.

class CodeSignRequirements : public CodeSignBlob {
	PackedByteArray blob;

	static inline size_t PAD(size_t s, size_t a) {
		return (s % a == 0) ? 0 : (a - s % a);
	}

	_FORCE_INLINE_ void _parse_certificate_slot(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const;
	_FORCE_INLINE_ void _parse_key(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const;
	_FORCE_INLINE_ void _parse_oid_key(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const;
	_FORCE_INLINE_ void _parse_hash_string(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const;
	_FORCE_INLINE_ void _parse_value(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const;
	_FORCE_INLINE_ void _parse_date(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const;
	_FORCE_INLINE_ bool _parse_match(uint32_t &r_pos, String &r_out, uint32_t p_rq_size) const;

public:
	CodeSignRequirements();
	CodeSignRequirements(const PackedByteArray &p_data);

	Vector<String> parse_requirements() const;

	virtual PackedByteArray get_hash_sha1() const override;
	virtual PackedByteArray get_hash_sha256() const override;

	virtual int get_size() const override;

	virtual uint32_t get_index_type() const override { return 0x00000002; }
	virtual void write_to_file(Ref<FileAccess> p_file) const override;
};

/*************************************************************************/
/* CodeSignEntitlementsText                                             */
/*************************************************************************/

// PList formatted entitlements.

class CodeSignEntitlementsText : public CodeSignBlob {
	PackedByteArray blob;

public:
	CodeSignEntitlementsText();
	CodeSignEntitlementsText(const String &p_string);

	virtual PackedByteArray get_hash_sha1() const override;
	virtual PackedByteArray get_hash_sha256() const override;

	virtual int get_size() const override;

	virtual uint32_t get_index_type() const override { return 0x00000005; }
	virtual void write_to_file(Ref<FileAccess> p_file) const override;
};

/*************************************************************************/
/* CodeSignEntitlementsBinary                                           */
/*************************************************************************/

// ASN.1 serialized entitlements.

class CodeSignEntitlementsBinary : public CodeSignBlob {
	PackedByteArray blob;

public:
	CodeSignEntitlementsBinary();
	CodeSignEntitlementsBinary(const String &p_string);

	virtual PackedByteArray get_hash_sha1() const override;
	virtual PackedByteArray get_hash_sha256() const override;

	virtual int get_size() const override;

	virtual uint32_t get_index_type() const override { return 0x00000007; }
	virtual void write_to_file(Ref<FileAccess> p_file) const override;
};

/*************************************************************************/
/* CodeSignCodeDirectory                                                 */
/*************************************************************************/

// Code Directory, runtime options, code segment and special structure hashes.

class CodeSignCodeDirectory : public CodeSignBlob {
public:
	enum Slot {
		SLOT_INFO_PLIST = -1,
		SLOT_REQUIREMENTS = -2,
		SLOT_RESOURCES = -3,
		SLOT_APP_SPECIFIC = -4, // Unused.
		SLOT_ENTITLEMENTS = -5,
		SLOT_RESERVER1 = -6, // Unused.
		SLOT_DER_ENTITLEMENTS = -7,
	};

	enum CodeSignExecSegFlags {
		EXECSEG_MAIN_BINARY = 0x1,
		EXECSEG_ALLOW_UNSIGNED = 0x10,
		EXECSEG_DEBUGGER = 0x20,
		EXECSEG_JIT = 0x40,
		EXECSEG_SKIP_LV = 0x80,
		EXECSEG_CAN_LOAD_CDHASH = 0x100,
		EXECSEG_CAN_EXEC_CDHASH = 0x200,
	};

	enum CodeSignatureFlags {
		SIGNATURE_HOST = 0x0001,
		SIGNATURE_ADHOC = 0x0002,
		SIGNATURE_TASK_ALLOW = 0x0004,
		SIGNATURE_INSTALLER = 0x0008,
		SIGNATURE_FORCED_LV = 0x0010,
		SIGNATURE_INVALID_ALLOWED = 0x0020,
		SIGNATURE_FORCE_HARD = 0x0100,
		SIGNATURE_FORCE_KILL = 0x0200,
		SIGNATURE_FORCE_EXPIRATION = 0x0400,
		SIGNATURE_RESTRICT = 0x0800,
		SIGNATURE_ENFORCEMENT = 0x1000,
		SIGNATURE_LIBRARY_VALIDATION = 0x2000,
		SIGNATURE_ENTITLEMENTS_VALIDATED = 0x4000,
		SIGNATURE_NVRAM_UNRESTRICTED = 0x8000,
		SIGNATURE_RUNTIME = 0x10000,
		SIGNATURE_LINKER_SIGNED = 0x20000,
	};

private:
	PackedByteArray blob;

	struct CodeDirectoryHeader {
		uint32_t version; // Using version 0x0020500.
		uint32_t flags; // // Option flags.
		uint32_t hash_offset; // Slot zero offset.
		uint32_t ident_offset; // Identifier string offset.
		uint32_t special_slots; // Nr. of slots with negative index.
		uint32_t code_slots; // Nr. of slots with index >= 0, (code_limit / page_size).
		uint32_t code_limit; // Everything before code signature load command offset.
		uint8_t hash_size; // 20 (SHA-1) or 32 (SHA-256).
		uint8_t hash_type; // 1 (SHA-1) or 2 (SHA-256).
		uint8_t platform; // Not used.
		uint8_t page_size; // Page size, power of two, 2^12 (4096).
		uint32_t spare2; // Not used.
		// Version 0x20100
		uint32_t scatter_vector_offset; // Set to 0 and ignore.
		// Version 0x20200
		uint32_t team_offset; // Team id string offset.
		// Version 0x20300
		uint32_t spare3; // Not used.
		uint64_t code_limit_64; // Set to 0 and ignore.
		// Version 0x20400
		uint64_t exec_seg_base; // Start of the signed code segmet.
		uint64_t exec_seg_limit; // Code segment (__TEXT) vmsize.
		uint64_t exec_seg_flags; // Executable segment flags.
		// Version 0x20500
		uint32_t runtime; // Runtime version.
		uint32_t pre_encrypt_offset; // Set to 0 and ignore.
	};

	int32_t pages = 0;
	int32_t remain = 0;
	int32_t code_slots = 0;
	int32_t special_slots = 0;

public:
	CodeSignCodeDirectory();
	CodeSignCodeDirectory(uint8_t p_hash_size, uint8_t p_hash_type, bool p_main, const CharString &p_id, const CharString &p_team_id, uint32_t p_page_size, uint64_t p_exe_limit, uint64_t p_code_limit);

	int32_t get_page_count();
	int32_t get_page_remainder();

	bool set_hash_in_slot(const PackedByteArray &p_hash, int p_slot);

	virtual PackedByteArray get_hash_sha1() const override;
	virtual PackedByteArray get_hash_sha256() const override;

	virtual int get_size() const override;
	virtual uint32_t get_index_type() const override { return 0x00000000; }

	virtual void write_to_file(Ref<FileAccess> p_file) const override;
};

/*************************************************************************/
/* CodeSignSignature                                                     */
/*************************************************************************/

class CodeSignSignature : public CodeSignBlob {
	PackedByteArray blob;

public:
	CodeSignSignature();

	virtual PackedByteArray get_hash_sha1() const override;
	virtual PackedByteArray get_hash_sha256() const override;

	virtual int get_size() const override;
	virtual uint32_t get_index_type() const override { return 0x00010000; }

	virtual void write_to_file(Ref<FileAccess> p_file) const override;
};

/*************************************************************************/
/* CodeSignSuperBlob                                                     */
/*************************************************************************/

class CodeSignSuperBlob {
	Vector<Ref<CodeSignBlob>> blobs;

public:
	bool add_blob(const Ref<CodeSignBlob> &p_blob);

	int get_size() const;
	void write_to_file(Ref<FileAccess> p_file) const;
};

/*************************************************************************/
/* CodeSign                                                              */
/*************************************************************************/

class CodeSign {
	static PackedByteArray file_hash_sha1(const String &p_path);
	static PackedByteArray file_hash_sha256(const String &p_path);
	static Error _codesign_file(bool p_use_hardened_runtime, bool p_force, const String &p_info, const String &p_exe_path, const String &p_bundle_path, const String &p_ent_path, bool p_ios_bundle, String &r_error_msg);

public:
	static Error codesign(bool p_use_hardened_runtime, bool p_force, const String &p_path, const String &p_ent_path, String &r_error_msg);
};

#endif // MODULE_REGEX_ENABLED

#endif // CODESIGN_H
