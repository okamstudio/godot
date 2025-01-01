/*
 * Copyright 2018-2021 Arm Limited
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * At your option, you may choose to accept this material under either:
 *  1. The Apache License, Version 2.0, found at <http://www.apache.org/licenses/LICENSE-2.0>, or
 *  2. The MIT License, found at <http://opensource.org/licenses/MIT>.
 */

#include "spirv_cross_parsed_ir.hpp"
#include <algorithm>
#include <assert.h>

using namespace std;
using namespace spv;

namespace SPIRV_CROSS_NAMESPACE
{
ParsedIR::ParsedIR()
{
	// If we move ParsedIR, we need to make sure the pointer stays fixed since the child Variant objects consume a pointer to this group,
	// so need an extra pointer here.
	pool_group.reset(new ObjectPoolGroup);

	pool_group->pools[TypeType].reset(new ObjectPool<SPIRType>);
	pool_group->pools[TypeVariable].reset(new ObjectPool<SPIRVariable>);
	pool_group->pools[TypeConstant].reset(new ObjectPool<SPIRConstant>);
	pool_group->pools[TypeFunction].reset(new ObjectPool<SPIRFunction>);
	pool_group->pools[TypeFunctionPrototype].reset(new ObjectPool<SPIRFunctionPrototype>);
	pool_group->pools[TypeBlock].reset(new ObjectPool<SPIRBlock>);
	pool_group->pools[TypeExtension].reset(new ObjectPool<SPIRExtension>);
	pool_group->pools[TypeExpression].reset(new ObjectPool<SPIRExpression>);
	pool_group->pools[TypeConstantOp].reset(new ObjectPool<SPIRConstantOp>);
	pool_group->pools[TypeCombinedImageSampler].reset(new ObjectPool<SPIRCombinedImageSampler>);
	pool_group->pools[TypeAccessChain].reset(new ObjectPool<SPIRAccessChain>);
	pool_group->pools[TypeUndef].reset(new ObjectPool<SPIRUndef>);
	pool_group->pools[TypeString].reset(new ObjectPool<SPIRString>);
}

// Should have been default-implemented, but need this on MSVC 2013.
ParsedIR::ParsedIR(ParsedIR &&other) SPIRV_CROSS_NOEXCEPT
{
	*this = std::move(other);
}

ParsedIR &ParsedIR::operator=(ParsedIR &&other) SPIRV_CROSS_NOEXCEPT
{
	if (this != &other)
	{
		pool_group = std::move(other.pool_group);
		spirv = std::move(other.spirv);
		meta = std::move(other.meta);
		for (int i = 0; i < TypeCount; i++)
			ids_for_type[i] = std::move(other.ids_for_type[i]);
		ids_for_constant_undef_or_type = std::move(other.ids_for_constant_undef_or_type);
		ids_for_constant_or_variable = std::move(other.ids_for_constant_or_variable);
		declared_capabilities = std::move(other.declared_capabilities);
		declared_extensions = std::move(other.declared_extensions);
		block_meta = std::move(other.block_meta);
		continue_block_to_loop_header = std::move(other.continue_block_to_loop_header);
		entry_points = std::move(other.entry_points);
		ids = std::move(other.ids);
		addressing_model = other.addressing_model;
		memory_model = other.memory_model;

		default_entry_point = other.default_entry_point;
		source = other.source;
		loop_iteration_depth_hard = other.loop_iteration_depth_hard;
		loop_iteration_depth_soft = other.loop_iteration_depth_soft;

		meta_needing_name_fixup = std::move(other.meta_needing_name_fixup);
		load_type_width = std::move(other.load_type_width);
	}
	return *this;
}

ParsedIR::ParsedIR(const ParsedIR &other)
    : ParsedIR()
{
	*this = other;
}

ParsedIR &ParsedIR::operator=(const ParsedIR &other)
{
	if (this != &other)
	{
		spirv = other.spirv;
		meta = other.meta;
		for (int i = 0; i < TypeCount; i++)
			ids_for_type[i] = other.ids_for_type[i];
		ids_for_constant_undef_or_type = other.ids_for_constant_undef_or_type;
		ids_for_constant_or_variable = other.ids_for_constant_or_variable;
		declared_capabilities = other.declared_capabilities;
		declared_extensions = other.declared_extensions;
		block_meta = other.block_meta;
		continue_block_to_loop_header = other.continue_block_to_loop_header;
		entry_points = other.entry_points;
		default_entry_point = other.default_entry_point;
		source = other.source;
		loop_iteration_depth_hard = other.loop_iteration_depth_hard;
		loop_iteration_depth_soft = other.loop_iteration_depth_soft;
		addressing_model = other.addressing_model;
		memory_model = other.memory_model;


		meta_needing_name_fixup = other.meta_needing_name_fixup;
		load_type_width = other.load_type_width;

		// Very deliberate copying of IDs. There is no default copy constructor, nor a simple default constructor.
		// Construct object first so we have the correct allocator set-up, then we can copy object into our new pool group.
		ids.clear();
		ids.reserve(other.ids.size());
		for (size_t i = 0; i < other.ids.size(); i++)
		{
			ids.emplace_back(pool_group.get());
			ids.back() = other.ids[i];
		}
	}
	return *this;
}

void ParsedIR::set_id_bounds(uint32_t bounds)
{
	ids.reserve(bounds);
	while (ids.size() < bounds)
		ids.emplace_back(pool_group.get());

	block_meta.resize(bounds);
}

// Roll our own versions of these functions to avoid potential locale shenanigans.
static bool is_alpha(char c)
{
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static bool is_numeric(char c)
{
	return c >= '0' && c <= '9';
}

static bool is_alphanumeric(char c)
{
	return is_alpha(c) || is_numeric(c);
}

static bool is_valid_identifier(const string &name)
{
	if (name.empty())
		return true;

	if (is_numeric(name[0]))
		return false;

	for (auto c : name)
		if (!is_alphanumeric(c) && c != '_')
			return false;

	bool saw_underscore = false;
	// Two underscores in a row is not a valid identifier either.
	// Technically reserved, but it's easier to treat it as invalid.
	for (auto c : name)
	{
		bool is_underscore = c == '_';
		if (is_underscore && saw_underscore)
			return false;
		saw_underscore = is_underscore;
	}

	return true;
}

static bool is_reserved_prefix(const string &name)
{
	// Generic reserved identifiers used by the implementation.
	return name.compare(0, 3, "gl_", 3) == 0 ||
	       // Ignore this case for now, might rewrite internal code to always use spv prefix.
	       //name.compare(0, 11, "SPIRV_Cross", 11) == 0 ||
	       name.compare(0, 3, "spv", 3) == 0;
}

static bool is_reserved_identifier(const string &name, bool member, bool allow_reserved_prefixes)
{
	if (!allow_reserved_prefixes && is_reserved_prefix(name))
		return true;

	if (member)
	{
		// Reserved member identifiers come in one form:
		// _m[0-9]+$.
		if (name.size() < 3)
			return false;

		if (name.compare(0, 2, "_m", 2) != 0)
			return false;

		size_t index = 2;
		while (index < name.size() && is_numeric(name[index]))
			index++;

		return index == name.size();
	}
	else
	{
		// Reserved non-member identifiers come in two forms:
		// _[0-9]+$, used for temporaries which map directly to a SPIR-V ID.
		// _[0-9]+_, used for auxillary temporaries which derived from a SPIR-V ID.
		if (name.size() < 2)
			return false;

		if (name[0] != '_' || !is_numeric(name[1]))
			return false;

		size_t index = 2;
		while (index < name.size() && is_numeric(name[index]))
			index++;

		return index == name.size() || (index < name.size() && name[index] == '_');
	}
}

bool ParsedIR::is_globally_reserved_identifier(std::string &str, bool allow_reserved_prefixes)
{
	return is_reserved_identifier(str, false, allow_reserved_prefixes);
}

uint32_t ParsedIR::get_spirv_version() const
{
	return spirv[1];
}

static string make_unreserved_identifier(const string &name)
{
	if (is_reserved_prefix(name))
		return "_RESERVED_IDENTIFIER_FIXUP_" + name;
	else
		return "_RESERVED_IDENTIFIER_FIXUP" + name;
}

void ParsedIR::sanitize_underscores(std::string &str)
{
	// Compact adjacent underscores to make it valid.
	auto dst = str.begin();
	auto src = dst;
	bool saw_underscore = false;
	while (src != str.end())
	{
		bool is_underscore = *src == '_';
		if (saw_underscore && is_underscore)
		{
			src++;
		}
		else
		{
			if (dst != src)
				*dst = *src;
			dst++;
			src++;
			saw_underscore = is_underscore;
		}
	}
	str.erase(dst, str.end());
}

static string ensure_valid_identifier(const string &name)
{
	// Functions in glslangValidator are mangled with name(<mangled> stuff.
	// Normally, we would never see '(' in any legal identifiers, so just strip them out.
	auto str = name.substr(0, name.find('('));

	if (str.empty())
		return str;

	if (is_numeric(str[0]))
		str[0] = '_';

	for (auto &c : str)
		if (!is_alphanumeric(c) && c != '_')
			c = '_';

	ParsedIR::sanitize_underscores(str);
	return str;
}

const string &ParsedIR::get_name(ID id) const
{
	auto *m = find_meta(id);
	if (m)
		return m->decoration.alias;
	else
		return empty_string;
}

const string &ParsedIR::get_member_name(TypeID id, uint32_t index) const
{
	auto *m = find_meta(id);
	if (m)
	{
		if (index >= m->members.size())
			return empty_string;
		return m->members[index].alias;
	}
	else
		return empty_string;
}

void ParsedIR::sanitize_identifier(std::string &name, bool member, bool allow_reserved_prefixes)
{
	if (!is_valid_identifier(name))
		name = ensure_valid_identifier(name);
	if (is_reserved_identifier(name, member, allow_reserved_prefixes))
		name = make_unreserved_identifier(name);
}

void ParsedIR::fixup_reserved_names()
{
	for (uint32_t id : meta_needing_name_fixup)
	{
		// Don't rename remapped variables like 'gl_LastFragDepthARM'.
		if (ids[id].get_type() == TypeVariable && get<SPIRVariable>(id).remapped_variable)
			continue;

		auto &m = meta[id];
		sanitize_identifier(m.decoration.alias, false, false);
		for (auto &memb : m.members)
			sanitize_identifier(memb.alias, true, false);
	}
	meta_needing_name_fixup.clear();
}

void ParsedIR::set_name(ID id, const string &name)
{
	auto &m = meta[id];
	m.decoration.alias = name;
	if (!is_valid_identifier(name) || is_reserved_identifier(name, false, false))
		meta_needing_name_fixup.insert(id);
}

void ParsedIR::set_member_name(TypeID id, uint32_t index, const string &name)
{
	auto &m = meta[id];
	m.members.resize(max(m.members.size(), size_t(index) + 1));
	m.members[index].alias = name;
	if (!is_valid_identifier(name) || is_reserved_identifier(name, true, false))
		meta_needing_name_fixup.insert(id);
}

void ParsedIR::set_decoration_string(ID id, Decoration decoration, const string &argument)
{
	auto &dec = meta[id].decoration;
	dec.decoration_flags.set(decoration);

	switch (decoration)
	{
	case DecorationHlslSemanticGOOGLE:
		dec.hlsl_semantic = argument;
		break;

	case DecorationUserTypeGOOGLE:
		dec.user_type = argument;
		break;

	default:
		break;
	}
}

void ParsedIR::set_decoration(ID id, Decoration decoration, uint32_t argument)
{
	auto &dec = meta[id].decoration;
	dec.decoration_flags.set(decoration);

	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = true;
		dec.builtin_type = static_cast<BuiltIn>(argument);
		break;

	case DecorationLocation:
		dec.location = argument;
		break;

	case DecorationComponent:
		dec.component = argument;
		break;

	case DecorationOffset:
		dec.offset = argument;
		break;

	case DecorationXfbBuffer:
		dec.xfb_buffer = argument;
		break;

	case DecorationXfbStride:
		dec.xfb_stride = argument;
		break;

	case DecorationStream:
		dec.stream = argument;
		break;

	case DecorationArrayStride:
		dec.array_stride = argument;
		break;

	case DecorationMatrixStride:
		dec.matrix_stride = argument;
		break;

	case DecorationBinding:
		dec.binding = argument;
		break;

	case DecorationDescriptorSet:
		dec.set = argument;
		break;

	case DecorationInputAttachmentIndex:
		dec.input_attachment = argument;
		break;

	case DecorationSpecId:
		dec.spec_id = argument;
		break;

	case DecorationIndex:
		dec.index = argument;
		break;

	case DecorationHlslCounterBufferGOOGLE:
		meta[id].hlsl_magic_counter_buffer = argument;
		meta[argument].hlsl_is_magic_counter_buffer = true;
		break;

	case DecorationFPRoundingMode:
		dec.fp_rounding_mode = static_cast<FPRoundingMode>(argument);
		break;

	default:
		break;
	}
}

void ParsedIR::set_member_decoration(TypeID id, uint32_t index, Decoration decoration, uint32_t argument)
{
	auto &m = meta[id];
	m.members.resize(max(m.members.size(), size_t(index) + 1));
	auto &dec = m.members[index];
	dec.decoration_flags.set(decoration);

	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = true;
		dec.builtin_type = static_cast<BuiltIn>(argument);
		break;

	case DecorationLocation:
		dec.location = argument;
		break;

	case DecorationComponent:
		dec.component = argument;
		break;

	case DecorationBinding:
		dec.binding = argument;
		break;

	case DecorationOffset:
		dec.offset = argument;
		break;

	case DecorationXfbBuffer:
		dec.xfb_buffer = argument;
		break;

	case DecorationXfbStride:
		dec.xfb_stride = argument;
		break;

	case DecorationStream:
		dec.stream = argument;
		break;

	case DecorationSpecId:
		dec.spec_id = argument;
		break;

	case DecorationMatrixStride:
		dec.matrix_stride = argument;
		break;

	case DecorationIndex:
		dec.index = argument;
		break;

	default:
		break;
	}
}

// Recursively marks any constants referenced by the specified constant instruction as being used
// as an array length. The id must be a constant instruction (SPIRConstant or SPIRConstantOp).
void ParsedIR::mark_used_as_array_length(ID id)
{
	switch (ids[id].get_type())
	{
	case TypeConstant:
		get<SPIRConstant>(id).is_used_as_array_length = true;
		break;

	case TypeConstantOp:
	{
		auto &cop = get<SPIRConstantOp>(id);
		if (cop.opcode == OpCompositeExtract)
			mark_used_as_array_length(cop.arguments[0]);
		else if (cop.opcode == OpCompositeInsert)
		{
			mark_used_as_array_length(cop.arguments[0]);
			mark_used_as_array_length(cop.arguments[1]);
		}
		else
			for (uint32_t arg_id : cop.arguments)
				mark_used_as_array_length(arg_id);
		break;
	}

	case TypeUndef:
		break;

	default:
		assert(0);
	}
}

Bitset ParsedIR::get_buffer_block_type_flags(const SPIRType &type) const
{
	if (type.member_types.empty())
		return {};

	Bitset all_members_flags = get_member_decoration_bitset(type.self, 0);
	for (uint32_t i = 1; i < uint32_t(type.member_types.size()); i++)
		all_members_flags.merge_and(get_member_decoration_bitset(type.self, i));
	return all_members_flags;
}

Bitset ParsedIR::get_buffer_block_flags(const SPIRVariable &var) const
{
	auto &type = get<SPIRType>(var.basetype);
	if (type.basetype != SPIRType::Struct)
		SPIRV_CROSS_THROW("Cannot get buffer block flags for non-buffer variable.");

	// Some flags like non-writable, non-readable are actually found
	// as member decorations. If all members have a decoration set, propagate
	// the decoration up as a regular variable decoration.
	Bitset base_flags;
	auto *m = find_meta(var.self);
	if (m)
		base_flags = m->decoration.decoration_flags;

	if (type.member_types.empty())
		return base_flags;

	auto all_members_flags = get_buffer_block_type_flags(type);
	base_flags.merge_or(all_members_flags);
	return base_flags;
}

const Bitset &ParsedIR::get_member_decoration_bitset(TypeID id, uint32_t index) const
{
	auto *m = find_meta(id);
	if (m)
	{
		if (index >= m->members.size())
			return cleared_bitset;
		return m->members[index].decoration_flags;
	}
	else
		return cleared_bitset;
}

bool ParsedIR::has_decoration(ID id, Decoration decoration) const
{
	return get_decoration_bitset(id).get(decoration);
}

uint32_t ParsedIR::get_decoration(ID id, Decoration decoration) const
{
	auto *m = find_meta(id);
	if (!m)
		return 0;

	auto &dec = m->decoration;
	if (!dec.decoration_flags.get(decoration))
		return 0;

	switch (decoration)
	{
	case DecorationBuiltIn:
		return dec.builtin_type;
	case DecorationLocation:
		return dec.location;
	case DecorationComponent:
		return dec.component;
	case DecorationOffset:
		return dec.offset;
	case DecorationXfbBuffer:
		return dec.xfb_buffer;
	case DecorationXfbStride:
		return dec.xfb_stride;
	case DecorationStream:
		return dec.stream;
	case DecorationBinding:
		return dec.binding;
	case DecorationDescriptorSet:
		return dec.set;
	case DecorationInputAttachmentIndex:
		return dec.input_attachment;
	case DecorationSpecId:
		return dec.spec_id;
	case DecorationArrayStride:
		return dec.array_stride;
	case DecorationMatrixStride:
		return dec.matrix_stride;
	case DecorationIndex:
		return dec.index;
	case DecorationFPRoundingMode:
		return dec.fp_rounding_mode;
	default:
		return 1;
	}
}

const string &ParsedIR::get_decoration_string(ID id, Decoration decoration) const
{
	auto *m = find_meta(id);
	if (!m)
		return empty_string;

	auto &dec = m->decoration;

	if (!dec.decoration_flags.get(decoration))
		return empty_string;

	switch (decoration)
	{
	case DecorationHlslSemanticGOOGLE:
		return dec.hlsl_semantic;

	case DecorationUserTypeGOOGLE:
		return dec.user_type;

	default:
		return empty_string;
	}
}

void ParsedIR::unset_decoration(ID id, Decoration decoration)
{
	auto &dec = meta[id].decoration;
	dec.decoration_flags.clear(decoration);
	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = false;
		break;

	case DecorationLocation:
		dec.location = 0;
		break;

	case DecorationComponent:
		dec.component = 0;
		break;

	case DecorationOffset:
		dec.offset = 0;
		break;

	case DecorationXfbBuffer:
		dec.xfb_buffer = 0;
		break;

	case DecorationXfbStride:
		dec.xfb_stride = 0;
		break;

	case DecorationStream:
		dec.stream = 0;
		break;

	case DecorationBinding:
		dec.binding = 0;
		break;

	case DecorationDescriptorSet:
		dec.set = 0;
		break;

	case DecorationInputAttachmentIndex:
		dec.input_attachment = 0;
		break;

	case DecorationSpecId:
		dec.spec_id = 0;
		break;

	case DecorationHlslSemanticGOOGLE:
		dec.hlsl_semantic.clear();
		break;

	case DecorationFPRoundingMode:
		dec.fp_rounding_mode = FPRoundingModeMax;
		break;

	case DecorationHlslCounterBufferGOOGLE:
	{
		auto &counter = meta[id].hlsl_magic_counter_buffer;
		if (counter)
		{
			meta[counter].hlsl_is_magic_counter_buffer = false;
			counter = 0;
		}
		break;
	}

	default:
		break;
	}
}

bool ParsedIR::has_member_decoration(TypeID id, uint32_t index, Decoration decoration) const
{
	return get_member_decoration_bitset(id, index).get(decoration);
}

uint32_t ParsedIR::get_member_decoration(TypeID id, uint32_t index, Decoration decoration) const
{
	auto *m = find_meta(id);
	if (!m)
		return 0;

	if (index >= m->members.size())
		return 0;

	auto &dec = m->members[index];
	if (!dec.decoration_flags.get(decoration))
		return 0;

	switch (decoration)
	{
	case DecorationBuiltIn:
		return dec.builtin_type;
	case DecorationLocation:
		return dec.location;
	case DecorationComponent:
		return dec.component;
	case DecorationBinding:
		return dec.binding;
	case DecorationOffset:
		return dec.offset;
	case DecorationXfbBuffer:
		return dec.xfb_buffer;
	case DecorationXfbStride:
		return dec.xfb_stride;
	case DecorationStream:
		return dec.stream;
	case DecorationSpecId:
		return dec.spec_id;
	case DecorationMatrixStride:
		return dec.matrix_stride;
	case DecorationIndex:
		return dec.index;
	default:
		return 1;
	}
}

const Bitset &ParsedIR::get_decoration_bitset(ID id) const
{
	auto *m = find_meta(id);
	if (m)
	{
		auto &dec = m->decoration;
		return dec.decoration_flags;
	}
	else
		return cleared_bitset;
}

void ParsedIR::set_member_decoration_string(TypeID id, uint32_t index, Decoration decoration, const string &argument)
{
	auto &m = meta[id];
	m.members.resize(max(m.members.size(), size_t(index) + 1));
	auto &dec = meta[id].members[index];
	dec.decoration_flags.set(decoration);

	switch (decoration)
	{
	case DecorationHlslSemanticGOOGLE:
		dec.hlsl_semantic = argument;
		break;

	default:
		break;
	}
}

const string &ParsedIR::get_member_decoration_string(TypeID id, uint32_t index, Decoration decoration) const
{
	auto *m = find_meta(id);
	if (m)
	{
		if (!has_member_decoration(id, index, decoration))
			return empty_string;

		auto &dec = m->members[index];

		switch (decoration)
		{
		case DecorationHlslSemanticGOOGLE:
			return dec.hlsl_semantic;

		default:
			return empty_string;
		}
	}
	else
		return empty_string;
}

void ParsedIR::unset_member_decoration(TypeID id, uint32_t index, Decoration decoration)
{
	auto &m = meta[id];
	if (index >= m.members.size())
		return;

	auto &dec = m.members[index];

	dec.decoration_flags.clear(decoration);
	switch (decoration)
	{
	case DecorationBuiltIn:
		dec.builtin = false;
		break;

	case DecorationLocation:
		dec.location = 0;
		break;

	case DecorationComponent:
		dec.component = 0;
		break;

	case DecorationOffset:
		dec.offset = 0;
		break;

	case DecorationXfbBuffer:
		dec.xfb_buffer = 0;
		break;

	case DecorationXfbStride:
		dec.xfb_stride = 0;
		break;

	case DecorationStream:
		dec.stream = 0;
		break;

	case DecorationSpecId:
		dec.spec_id = 0;
		break;

	case DecorationHlslSemanticGOOGLE:
		dec.hlsl_semantic.clear();
		break;

	default:
		break;
	}
}

uint32_t ParsedIR::increase_bound_by(uint32_t incr_amount)
{
	auto curr_bound = ids.size();
	auto new_bound = curr_bound + incr_amount;

	ids.reserve(ids.size() + incr_amount);
	for (uint32_t i = 0; i < incr_amount; i++)
		ids.emplace_back(pool_group.get());

	block_meta.resize(new_bound);
	return uint32_t(curr_bound);
}

void ParsedIR::remove_typed_id(Types type, ID id)
{
	auto &type_ids = ids_for_type[type];
	type_ids.erase(remove(begin(type_ids), end(type_ids), id), end(type_ids));
}

void ParsedIR::reset_all_of_type(Types type)
{
	for (auto &id : ids_for_type[type])
		if (ids[id].get_type() == type)
			ids[id].reset();

	ids_for_type[type].clear();
}

void ParsedIR::add_typed_id(Types type, ID id)
{
	assert(id < ids.size());

	if (loop_iteration_depth_hard != 0)
		SPIRV_CROSS_THROW("Cannot add typed ID while looping over it.");

	if (loop_iteration_depth_soft != 0)
	{
		if (!ids[id].empty())
			SPIRV_CROSS_THROW("Cannot override IDs when loop is soft locked.");
		return;
	}

	if (ids[id].empty() || ids[id].get_type() != type)
	{
		switch (type)
		{
		case TypeConstant:
			ids_for_constant_or_variable.push_back(id);
			ids_for_constant_undef_or_type.push_back(id);
			break;

		case TypeVariable:
			ids_for_constant_or_variable.push_back(id);
			break;

		case TypeType:
		case TypeConstantOp:
		case TypeUndef:
			ids_for_constant_undef_or_type.push_back(id);
			break;

		default:
			break;
		}
	}

	if (ids[id].empty())
	{
		ids_for_type[type].push_back(id);
	}
	else if (ids[id].get_type() != type)
	{
		remove_typed_id(ids[id].get_type(), id);
		ids_for_type[type].push_back(id);
	}
}

const Meta *ParsedIR::find_meta(ID id) const
{
	auto itr = meta.find(id);
	if (itr != end(meta))
		return &itr->second;
	else
		return nullptr;
}

Meta *ParsedIR::find_meta(ID id)
{
	auto itr = meta.find(id);
	if (itr != end(meta))
		return &itr->second;
	else
		return nullptr;
}

ParsedIR::LoopLock ParsedIR::create_loop_hard_lock() const
{
	return ParsedIR::LoopLock(&loop_iteration_depth_hard);
}

ParsedIR::LoopLock ParsedIR::create_loop_soft_lock() const
{
	return ParsedIR::LoopLock(&loop_iteration_depth_soft);
}

ParsedIR::LoopLock::~LoopLock()
{
	if (lock)
		(*lock)--;
}

ParsedIR::LoopLock::LoopLock(uint32_t *lock_)
    : lock(lock_)
{
	if (lock)
		(*lock)++;
}

ParsedIR::LoopLock::LoopLock(LoopLock &&other) SPIRV_CROSS_NOEXCEPT
{
	*this = std::move(other);
}

ParsedIR::LoopLock &ParsedIR::LoopLock::operator=(LoopLock &&other) SPIRV_CROSS_NOEXCEPT
{
	if (lock)
		(*lock)--;
	lock = other.lock;
	other.lock = nullptr;
	return *this;
}

void ParsedIR::make_constant_null(uint32_t id, uint32_t type, bool add_to_typed_id_set)
{
	assert(id < ids.size());

	auto &constant_type = get<SPIRType>(type);

	if (constant_type.pointer)
	{
		if (add_to_typed_id_set)
			add_typed_id(TypeConstant, id);
		auto &constant = variant_set<SPIRConstant>(ids[id], type);
		constant.self = id;
		constant.make_null(constant_type);
	}
	else if (!constant_type.array.empty())
	{
		assert(constant_type.parent_type);
		uint32_t parent_id = increase_bound_by(1);
		make_constant_null(parent_id, constant_type.parent_type, add_to_typed_id_set);

		if (!constant_type.array_size_literal.back())
			SPIRV_CROSS_THROW("Array size of OpConstantNull must be a literal.");

		SmallVector<uint32_t> elements(constant_type.array.back());
		for (uint32_t i = 0; i < constant_type.array.back(); i++)
			elements[i] = parent_id;

		if (add_to_typed_id_set)
			add_typed_id(TypeConstant, id);
		variant_set<SPIRConstant>(ids[id], type, elements.data(), uint32_t(elements.size()), false).self = id;
	}
	else if (!constant_type.member_types.empty())
	{
		uint32_t member_ids = increase_bound_by(uint32_t(constant_type.member_types.size()));
		SmallVector<uint32_t> elements(constant_type.member_types.size());
		for (uint32_t i = 0; i < constant_type.member_types.size(); i++)
		{
			make_constant_null(member_ids + i, constant_type.member_types[i], add_to_typed_id_set);
			elements[i] = member_ids + i;
		}

		if (add_to_typed_id_set)
			add_typed_id(TypeConstant, id);
		variant_set<SPIRConstant>(ids[id], type, elements.data(), uint32_t(elements.size()), false).self = id;
	}
	else
	{
		if (add_to_typed_id_set)
			add_typed_id(TypeConstant, id);
		auto &constant = variant_set<SPIRConstant>(ids[id], type);
		constant.self = id;
		constant.make_null(constant_type);
	}
}

} // namespace SPIRV_CROSS_NAMESPACE
