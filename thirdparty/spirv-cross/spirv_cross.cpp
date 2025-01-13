/*
 * Copyright 2015-2021 Arm Limited
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

#include "spirv_cross.hpp"
#include "GLSL.std.450.h"
#include "spirv_cfg.hpp"
#include "spirv_common.hpp"
#include "spirv_parser.hpp"
#include <algorithm>
#include <cstring>
#include <utility>

using namespace std;
using namespace spv;
using namespace SPIRV_CROSS_NAMESPACE;

Compiler::Compiler(vector<uint32_t> ir_)
{
	Parser parser(std::move(ir_));
	parser.parse();
	set_ir(std::move(parser.get_parsed_ir()));
}

Compiler::Compiler(const uint32_t *ir_, size_t word_count)
{
	Parser parser(ir_, word_count);
	parser.parse();
	set_ir(std::move(parser.get_parsed_ir()));
}

Compiler::Compiler(const ParsedIR &ir_)
{
	set_ir(ir_);
}

Compiler::Compiler(ParsedIR &&ir_)
{
	set_ir(std::move(ir_));
}

void Compiler::set_ir(ParsedIR &&ir_)
{
	ir = std::move(ir_);
	parse_fixup();
}

void Compiler::set_ir(const ParsedIR &ir_)
{
	ir = ir_;
	parse_fixup();
}

string Compiler::compile()
{
	return "";
}

bool Compiler::variable_storage_is_aliased(const SPIRVariable &v)
{
	auto &type = get<SPIRType>(v.basetype);
	bool ssbo = v.storage == StorageClassStorageBuffer ||
	            ir.meta[type.self].decoration.decoration_flags.get(DecorationBufferBlock);
	bool image = type.basetype == SPIRType::Image;
	bool counter = type.basetype == SPIRType::AtomicCounter;
	bool buffer_reference = type.storage == StorageClassPhysicalStorageBufferEXT;

	bool is_restrict;
	if (ssbo)
		is_restrict = ir.get_buffer_block_flags(v).get(DecorationRestrict);
	else
		is_restrict = has_decoration(v.self, DecorationRestrict);

	return !is_restrict && (ssbo || image || counter || buffer_reference);
}

bool Compiler::block_is_control_dependent(const SPIRBlock &block)
{
	for (auto &i : block.ops)
	{
		auto ops = stream(i);
		auto op = static_cast<Op>(i.op);

		switch (op)
		{
		case OpFunctionCall:
		{
			uint32_t func = ops[2];
			if (function_is_control_dependent(get<SPIRFunction>(func)))
				return true;
			break;
		}

		// Derivatives
		case OpDPdx:
		case OpDPdxCoarse:
		case OpDPdxFine:
		case OpDPdy:
		case OpDPdyCoarse:
		case OpDPdyFine:
		case OpFwidth:
		case OpFwidthCoarse:
		case OpFwidthFine:

		// Anything implicit LOD
		case OpImageSampleImplicitLod:
		case OpImageSampleDrefImplicitLod:
		case OpImageSampleProjImplicitLod:
		case OpImageSampleProjDrefImplicitLod:
		case OpImageSparseSampleImplicitLod:
		case OpImageSparseSampleDrefImplicitLod:
		case OpImageSparseSampleProjImplicitLod:
		case OpImageSparseSampleProjDrefImplicitLod:
		case OpImageQueryLod:
		case OpImageDrefGather:
		case OpImageGather:
		case OpImageSparseDrefGather:
		case OpImageSparseGather:

		// Anything subgroups
		case OpGroupNonUniformElect:
		case OpGroupNonUniformAll:
		case OpGroupNonUniformAny:
		case OpGroupNonUniformAllEqual:
		case OpGroupNonUniformBroadcast:
		case OpGroupNonUniformBroadcastFirst:
		case OpGroupNonUniformBallot:
		case OpGroupNonUniformInverseBallot:
		case OpGroupNonUniformBallotBitExtract:
		case OpGroupNonUniformBallotBitCount:
		case OpGroupNonUniformBallotFindLSB:
		case OpGroupNonUniformBallotFindMSB:
		case OpGroupNonUniformShuffle:
		case OpGroupNonUniformShuffleXor:
		case OpGroupNonUniformShuffleUp:
		case OpGroupNonUniformShuffleDown:
		case OpGroupNonUniformIAdd:
		case OpGroupNonUniformFAdd:
		case OpGroupNonUniformIMul:
		case OpGroupNonUniformFMul:
		case OpGroupNonUniformSMin:
		case OpGroupNonUniformUMin:
		case OpGroupNonUniformFMin:
		case OpGroupNonUniformSMax:
		case OpGroupNonUniformUMax:
		case OpGroupNonUniformFMax:
		case OpGroupNonUniformBitwiseAnd:
		case OpGroupNonUniformBitwiseOr:
		case OpGroupNonUniformBitwiseXor:
		case OpGroupNonUniformLogicalAnd:
		case OpGroupNonUniformLogicalOr:
		case OpGroupNonUniformLogicalXor:
		case OpGroupNonUniformQuadBroadcast:
		case OpGroupNonUniformQuadSwap:

		// Control barriers
		case OpControlBarrier:
			return true;

		default:
			break;
		}
	}

	return false;
}

bool Compiler::block_is_pure(const SPIRBlock &block)
{
	// This is a global side effect of the function.
	if (block.terminator == SPIRBlock::Kill ||
	    block.terminator == SPIRBlock::TerminateRay ||
	    block.terminator == SPIRBlock::IgnoreIntersection ||
	    block.terminator == SPIRBlock::EmitMeshTasks)
		return false;

	for (auto &i : block.ops)
	{
		auto ops = stream(i);
		auto op = static_cast<Op>(i.op);

		switch (op)
		{
		case OpFunctionCall:
		{
			uint32_t func = ops[2];
			if (!function_is_pure(get<SPIRFunction>(func)))
				return false;
			break;
		}

		case OpCopyMemory:
		case OpStore:
		{
			auto &type = expression_type(ops[0]);
			if (type.storage != StorageClassFunction)
				return false;
			break;
		}

		case OpImageWrite:
			return false;

		// Atomics are impure.
		case OpAtomicLoad:
		case OpAtomicStore:
		case OpAtomicExchange:
		case OpAtomicCompareExchange:
		case OpAtomicCompareExchangeWeak:
		case OpAtomicIIncrement:
		case OpAtomicIDecrement:
		case OpAtomicIAdd:
		case OpAtomicISub:
		case OpAtomicSMin:
		case OpAtomicUMin:
		case OpAtomicSMax:
		case OpAtomicUMax:
		case OpAtomicAnd:
		case OpAtomicOr:
		case OpAtomicXor:
			return false;

		// Geometry shader builtins modify global state.
		case OpEndPrimitive:
		case OpEmitStreamVertex:
		case OpEndStreamPrimitive:
		case OpEmitVertex:
			return false;

		// Mesh shader functions modify global state.
		// (EmitMeshTasks is a terminator).
		case OpSetMeshOutputsEXT:
			return false;

		// Barriers disallow any reordering, so we should treat blocks with barrier as writing.
		case OpControlBarrier:
		case OpMemoryBarrier:
			return false;

		// Ray tracing builtins are impure.
		case OpReportIntersectionKHR:
		case OpIgnoreIntersectionNV:
		case OpTerminateRayNV:
		case OpTraceNV:
		case OpTraceRayKHR:
		case OpExecuteCallableNV:
		case OpExecuteCallableKHR:
		case OpRayQueryInitializeKHR:
		case OpRayQueryTerminateKHR:
		case OpRayQueryGenerateIntersectionKHR:
		case OpRayQueryConfirmIntersectionKHR:
		case OpRayQueryProceedKHR:
			// There are various getters in ray query, but they are considered pure.
			return false;

			// OpExtInst is potentially impure depending on extension, but GLSL builtins are at least pure.

		case OpDemoteToHelperInvocationEXT:
			// This is a global side effect of the function.
			return false;

		case OpExtInst:
		{
			uint32_t extension_set = ops[2];
			if (get<SPIRExtension>(extension_set).ext == SPIRExtension::GLSL)
			{
				auto op_450 = static_cast<GLSLstd450>(ops[3]);
				switch (op_450)
				{
				case GLSLstd450Modf:
				case GLSLstd450Frexp:
				{
					auto &type = expression_type(ops[5]);
					if (type.storage != StorageClassFunction)
						return false;
					break;
				}

				default:
					break;
				}
			}
			break;
		}

		default:
			break;
		}
	}

	return true;
}

string Compiler::to_name(uint32_t id, bool allow_alias) const
{
	if (allow_alias && ir.ids[id].get_type() == TypeType)
	{
		// If this type is a simple alias, emit the
		// name of the original type instead.
		// We don't want to override the meta alias
		// as that can be overridden by the reflection APIs after parse.
		auto &type = get<SPIRType>(id);
		if (type.type_alias)
		{
			// If the alias master has been specially packed, we will have emitted a clean variant as well,
			// so skip the name aliasing here.
			if (!has_extended_decoration(type.type_alias, SPIRVCrossDecorationBufferBlockRepacked))
				return to_name(type.type_alias);
		}
	}

	auto &alias = ir.get_name(id);
	if (alias.empty())
		return join("_", id);
	else
		return alias;
}

bool Compiler::function_is_pure(const SPIRFunction &func)
{
	for (auto block : func.blocks)
		if (!block_is_pure(get<SPIRBlock>(block)))
			return false;

	return true;
}

bool Compiler::function_is_control_dependent(const SPIRFunction &func)
{
	for (auto block : func.blocks)
		if (block_is_control_dependent(get<SPIRBlock>(block)))
			return true;

	return false;
}

void Compiler::register_global_read_dependencies(const SPIRBlock &block, uint32_t id)
{
	for (auto &i : block.ops)
	{
		auto ops = stream(i);
		auto op = static_cast<Op>(i.op);

		switch (op)
		{
		case OpFunctionCall:
		{
			uint32_t func = ops[2];
			register_global_read_dependencies(get<SPIRFunction>(func), id);
			break;
		}

		case OpLoad:
		case OpImageRead:
		{
			// If we're in a storage class which does not get invalidated, adding dependencies here is no big deal.
			auto *var = maybe_get_backing_variable(ops[2]);
			if (var && var->storage != StorageClassFunction)
			{
				auto &type = get<SPIRType>(var->basetype);

				// InputTargets are immutable.
				if (type.basetype != SPIRType::Image && type.image.dim != DimSubpassData)
					var->dependees.push_back(id);
			}
			break;
		}

		default:
			break;
		}
	}
}

void Compiler::register_global_read_dependencies(const SPIRFunction &func, uint32_t id)
{
	for (auto block : func.blocks)
		register_global_read_dependencies(get<SPIRBlock>(block), id);
}

SPIRVariable *Compiler::maybe_get_backing_variable(uint32_t chain)
{
	auto *var = maybe_get<SPIRVariable>(chain);
	if (!var)
	{
		auto *cexpr = maybe_get<SPIRExpression>(chain);
		if (cexpr)
			var = maybe_get<SPIRVariable>(cexpr->loaded_from);

		auto *access_chain = maybe_get<SPIRAccessChain>(chain);
		if (access_chain)
			var = maybe_get<SPIRVariable>(access_chain->loaded_from);
	}

	return var;
}

void Compiler::register_read(uint32_t expr, uint32_t chain, bool forwarded)
{
	auto &e = get<SPIRExpression>(expr);
	auto *var = maybe_get_backing_variable(chain);

	if (var)
	{
		e.loaded_from = var->self;

		// If the backing variable is immutable, we do not need to depend on the variable.
		if (forwarded && !is_immutable(var->self))
			var->dependees.push_back(e.self);

		// If we load from a parameter, make sure we create "inout" if we also write to the parameter.
		// The default is "in" however, so we never invalidate our compilation by reading.
		if (var && var->parameter)
			var->parameter->read_count++;
	}
}

void Compiler::register_write(uint32_t chain)
{
	auto *var = maybe_get<SPIRVariable>(chain);
	if (!var)
	{
		// If we're storing through an access chain, invalidate the backing variable instead.
		auto *expr = maybe_get<SPIRExpression>(chain);
		if (expr && expr->loaded_from)
			var = maybe_get<SPIRVariable>(expr->loaded_from);

		auto *access_chain = maybe_get<SPIRAccessChain>(chain);
		if (access_chain && access_chain->loaded_from)
			var = maybe_get<SPIRVariable>(access_chain->loaded_from);
	}

	auto &chain_type = expression_type(chain);

	if (var)
	{
		bool check_argument_storage_qualifier = true;
		auto &type = expression_type(chain);

		// If our variable is in a storage class which can alias with other buffers,
		// invalidate all variables which depend on aliased variables. And if this is a
		// variable pointer, then invalidate all variables regardless.
		if (get_variable_data_type(*var).pointer)
		{
			flush_all_active_variables();

			if (type.pointer_depth == 1)
			{
				// We have a backing variable which is a pointer-to-pointer type.
				// We are storing some data through a pointer acquired through that variable,
				// but we are not writing to the value of the variable itself,
				// i.e., we are not modifying the pointer directly.
				// If we are storing a non-pointer type (pointer_depth == 1),
				// we know that we are storing some unrelated data.
				// A case here would be
				// void foo(Foo * const *arg) {
				//   Foo *bar = *arg;
				//   bar->unrelated = 42;
				// }
				// arg, the argument is constant.
				check_argument_storage_qualifier = false;
			}
		}

		if (type.storage == StorageClassPhysicalStorageBufferEXT || variable_storage_is_aliased(*var))
			flush_all_aliased_variables();
		else if (var)
			flush_dependees(*var);

		// We tried to write to a parameter which is not marked with out qualifier, force a recompile.
		if (check_argument_storage_qualifier && var->parameter && var->parameter->write_count == 0)
		{
			var->parameter->write_count++;
			force_recompile();
		}
	}
	else if (chain_type.pointer)
	{
		// If we stored through a variable pointer, then we don't know which
		// variable we stored to. So *all* expressions after this point need to
		// be invalidated.
		// FIXME: If we can prove that the variable pointer will point to
		// only certain variables, we can invalidate only those.
		flush_all_active_variables();
	}

	// If chain_type.pointer is false, we're not writing to memory backed variables, but temporaries instead.
	// This can happen in copy_logical_type where we unroll complex reads and writes to temporaries.
}

void Compiler::flush_dependees(SPIRVariable &var)
{
	for (auto expr : var.dependees)
		invalid_expressions.insert(expr);
	var.dependees.clear();
}

void Compiler::flush_all_aliased_variables()
{
	for (auto aliased : aliased_variables)
		flush_dependees(get<SPIRVariable>(aliased));
}

void Compiler::flush_all_atomic_capable_variables()
{
	for (auto global : global_variables)
		flush_dependees(get<SPIRVariable>(global));
	flush_all_aliased_variables();
}

void Compiler::flush_control_dependent_expressions(uint32_t block_id)
{
	auto &block = get<SPIRBlock>(block_id);
	for (auto &expr : block.invalidate_expressions)
		invalid_expressions.insert(expr);
	block.invalidate_expressions.clear();
}

void Compiler::flush_all_active_variables()
{
	// Invalidate all temporaries we read from variables in this block since they were forwarded.
	// Invalidate all temporaries we read from globals.
	for (auto &v : current_function->local_variables)
		flush_dependees(get<SPIRVariable>(v));
	for (auto &arg : current_function->arguments)
		flush_dependees(get<SPIRVariable>(arg.id));
	for (auto global : global_variables)
		flush_dependees(get<SPIRVariable>(global));

	flush_all_aliased_variables();
}

uint32_t Compiler::expression_type_id(uint32_t id) const
{
	switch (ir.ids[id].get_type())
	{
	case TypeVariable:
		return get<SPIRVariable>(id).basetype;

	case TypeExpression:
		return get<SPIRExpression>(id).expression_type;

	case TypeConstant:
		return get<SPIRConstant>(id).constant_type;

	case TypeConstantOp:
		return get<SPIRConstantOp>(id).basetype;

	case TypeUndef:
		return get<SPIRUndef>(id).basetype;

	case TypeCombinedImageSampler:
		return get<SPIRCombinedImageSampler>(id).combined_type;

	case TypeAccessChain:
		return get<SPIRAccessChain>(id).basetype;

	default:
		SPIRV_CROSS_THROW("Cannot resolve expression type.");
	}
}

const SPIRType &Compiler::expression_type(uint32_t id) const
{
	return get<SPIRType>(expression_type_id(id));
}

bool Compiler::expression_is_lvalue(uint32_t id) const
{
	auto &type = expression_type(id);
	switch (type.basetype)
	{
	case SPIRType::SampledImage:
	case SPIRType::Image:
	case SPIRType::Sampler:
		return false;

	default:
		return true;
	}
}

bool Compiler::is_immutable(uint32_t id) const
{
	if (ir.ids[id].get_type() == TypeVariable)
	{
		auto &var = get<SPIRVariable>(id);

		// Anything we load from the UniformConstant address space is guaranteed to be immutable.
		bool pointer_to_const = var.storage == StorageClassUniformConstant;
		return pointer_to_const || var.phi_variable || !expression_is_lvalue(id);
	}
	else if (ir.ids[id].get_type() == TypeAccessChain)
		return get<SPIRAccessChain>(id).immutable;
	else if (ir.ids[id].get_type() == TypeExpression)
		return get<SPIRExpression>(id).immutable;
	else if (ir.ids[id].get_type() == TypeConstant || ir.ids[id].get_type() == TypeConstantOp ||
	         ir.ids[id].get_type() == TypeUndef)
		return true;
	else
		return false;
}

static inline bool storage_class_is_interface(spv::StorageClass storage)
{
	switch (storage)
	{
	case StorageClassInput:
	case StorageClassOutput:
	case StorageClassUniform:
	case StorageClassUniformConstant:
	case StorageClassAtomicCounter:
	case StorageClassPushConstant:
	case StorageClassStorageBuffer:
		return true;

	default:
		return false;
	}
}

bool Compiler::is_hidden_variable(const SPIRVariable &var, bool include_builtins) const
{
	if ((is_builtin_variable(var) && !include_builtins) || var.remapped_variable)
		return true;

	// Combined image samplers are always considered active as they are "magic" variables.
	if (find_if(begin(combined_image_samplers), end(combined_image_samplers), [&var](const CombinedImageSampler &samp) {
		    return samp.combined_id == var.self;
	    }) != end(combined_image_samplers))
	{
		return false;
	}

	// In SPIR-V 1.4 and up we must also use the active variable interface to disable global variables
	// which are not part of the entry point.
	if (ir.get_spirv_version() >= 0x10400 && var.storage != spv::StorageClassGeneric &&
	    var.storage != spv::StorageClassFunction && !interface_variable_exists_in_entry_point(var.self))
	{
		return true;
	}

	return check_active_interface_variables && storage_class_is_interface(var.storage) &&
	       active_interface_variables.find(var.self) == end(active_interface_variables);
}

bool Compiler::is_builtin_type(const SPIRType &type) const
{
	auto *type_meta = ir.find_meta(type.self);

	// We can have builtin structs as well. If one member of a struct is builtin, the struct must also be builtin.
	if (type_meta)
		for (auto &m : type_meta->members)
			if (m.builtin)
				return true;

	return false;
}

bool Compiler::is_builtin_variable(const SPIRVariable &var) const
{
	auto *m = ir.find_meta(var.self);

	if (var.compat_builtin || (m && m->decoration.builtin))
		return true;
	else
		return is_builtin_type(get<SPIRType>(var.basetype));
}

bool Compiler::is_member_builtin(const SPIRType &type, uint32_t index, BuiltIn *builtin) const
{
	auto *type_meta = ir.find_meta(type.self);

	if (type_meta)
	{
		auto &memb = type_meta->members;
		if (index < memb.size() && memb[index].builtin)
		{
			if (builtin)
				*builtin = memb[index].builtin_type;
			return true;
		}
	}

	return false;
}

bool Compiler::is_scalar(const SPIRType &type) const
{
	return type.basetype != SPIRType::Struct && type.vecsize == 1 && type.columns == 1;
}

bool Compiler::is_vector(const SPIRType &type) const
{
	return type.vecsize > 1 && type.columns == 1;
}

bool Compiler::is_matrix(const SPIRType &type) const
{
	return type.vecsize > 1 && type.columns > 1;
}

bool Compiler::is_array(const SPIRType &type) const
{
	return type.op == OpTypeArray || type.op == OpTypeRuntimeArray;
}

bool Compiler::is_pointer(const SPIRType &type) const
{
	return type.op == OpTypePointer && type.basetype != SPIRType::Unknown; // Ignore function pointers.
}

bool Compiler::is_physical_pointer(const SPIRType &type) const
{
	return type.op == OpTypePointer && type.storage == StorageClassPhysicalStorageBuffer;
}

bool Compiler::is_physical_pointer_to_buffer_block(const SPIRType &type) const
{
	return is_physical_pointer(type) && get_pointee_type(type).self == type.parent_type &&
	       (has_decoration(type.self, DecorationBlock) ||
	        has_decoration(type.self, DecorationBufferBlock));
}

bool Compiler::is_runtime_size_array(const SPIRType &type)
{
	return type.op == OpTypeRuntimeArray;
}

ShaderResources Compiler::get_shader_resources() const
{
	return get_shader_resources(nullptr);
}

ShaderResources Compiler::get_shader_resources(const unordered_set<VariableID> &active_variables) const
{
	return get_shader_resources(&active_variables);
}

bool Compiler::InterfaceVariableAccessHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	uint32_t variable = 0;
	switch (opcode)
	{
	// Need this first, otherwise, GCC complains about unhandled switch statements.
	default:
		break;

	case OpFunctionCall:
	{
		// Invalid SPIR-V.
		if (length < 3)
			return false;

		uint32_t count = length - 3;
		args += 3;
		for (uint32_t i = 0; i < count; i++)
		{
			auto *var = compiler.maybe_get<SPIRVariable>(args[i]);
			if (var && storage_class_is_interface(var->storage))
				variables.insert(args[i]);
		}
		break;
	}

	case OpSelect:
	{
		// Invalid SPIR-V.
		if (length < 5)
			return false;

		uint32_t count = length - 3;
		args += 3;
		for (uint32_t i = 0; i < count; i++)
		{
			auto *var = compiler.maybe_get<SPIRVariable>(args[i]);
			if (var && storage_class_is_interface(var->storage))
				variables.insert(args[i]);
		}
		break;
	}

	case OpPhi:
	{
		// Invalid SPIR-V.
		if (length < 2)
			return false;

		uint32_t count = length - 2;
		args += 2;
		for (uint32_t i = 0; i < count; i += 2)
		{
			auto *var = compiler.maybe_get<SPIRVariable>(args[i]);
			if (var && storage_class_is_interface(var->storage))
				variables.insert(args[i]);
		}
		break;
	}

	case OpAtomicStore:
	case OpStore:
		// Invalid SPIR-V.
		if (length < 1)
			return false;
		variable = args[0];
		break;

	case OpCopyMemory:
	{
		if (length < 2)
			return false;

		auto *var = compiler.maybe_get<SPIRVariable>(args[0]);
		if (var && storage_class_is_interface(var->storage))
			variables.insert(args[0]);

		var = compiler.maybe_get<SPIRVariable>(args[1]);
		if (var && storage_class_is_interface(var->storage))
			variables.insert(args[1]);
		break;
	}

	case OpExtInst:
	{
		if (length < 3)
			return false;
		auto &extension_set = compiler.get<SPIRExtension>(args[2]);
		switch (extension_set.ext)
		{
		case SPIRExtension::GLSL:
		{
			auto op = static_cast<GLSLstd450>(args[3]);

			switch (op)
			{
			case GLSLstd450InterpolateAtCentroid:
			case GLSLstd450InterpolateAtSample:
			case GLSLstd450InterpolateAtOffset:
			{
				auto *var = compiler.maybe_get<SPIRVariable>(args[4]);
				if (var && storage_class_is_interface(var->storage))
					variables.insert(args[4]);
				break;
			}

			case GLSLstd450Modf:
			case GLSLstd450Fract:
			{
				auto *var = compiler.maybe_get<SPIRVariable>(args[5]);
				if (var && storage_class_is_interface(var->storage))
					variables.insert(args[5]);
				break;
			}

			default:
				break;
			}
			break;
		}
		case SPIRExtension::SPV_AMD_shader_explicit_vertex_parameter:
		{
			enum AMDShaderExplicitVertexParameter
			{
				InterpolateAtVertexAMD = 1
			};

			auto op = static_cast<AMDShaderExplicitVertexParameter>(args[3]);

			switch (op)
			{
			case InterpolateAtVertexAMD:
			{
				auto *var = compiler.maybe_get<SPIRVariable>(args[4]);
				if (var && storage_class_is_interface(var->storage))
					variables.insert(args[4]);
				break;
			}

			default:
				break;
			}
			break;
		}
		default:
			break;
		}
		break;
	}

	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpPtrAccessChain:
	case OpLoad:
	case OpCopyObject:
	case OpImageTexelPointer:
	case OpAtomicLoad:
	case OpAtomicExchange:
	case OpAtomicCompareExchange:
	case OpAtomicCompareExchangeWeak:
	case OpAtomicIIncrement:
	case OpAtomicIDecrement:
	case OpAtomicIAdd:
	case OpAtomicISub:
	case OpAtomicSMin:
	case OpAtomicUMin:
	case OpAtomicSMax:
	case OpAtomicUMax:
	case OpAtomicAnd:
	case OpAtomicOr:
	case OpAtomicXor:
	case OpArrayLength:
		// Invalid SPIR-V.
		if (length < 3)
			return false;
		variable = args[2];
		break;
	}

	if (variable)
	{
		auto *var = compiler.maybe_get<SPIRVariable>(variable);
		if (var && storage_class_is_interface(var->storage))
			variables.insert(variable);
	}
	return true;
}

unordered_set<VariableID> Compiler::get_active_interface_variables() const
{
	// Traverse the call graph and find all interface variables which are in use.
	unordered_set<VariableID> variables;
	InterfaceVariableAccessHandler handler(*this, variables);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, const SPIRVariable &var) {
		if (var.storage != StorageClassOutput)
			return;
		if (!interface_variable_exists_in_entry_point(var.self))
			return;

		// An output variable which is just declared (but uninitialized) might be read by subsequent stages
		// so we should force-enable these outputs,
		// since compilation will fail if a subsequent stage attempts to read from the variable in question.
		// Also, make sure we preserve output variables which are only initialized, but never accessed by any code.
		if (var.initializer != ID(0) || get_execution_model() != ExecutionModelFragment)
			variables.insert(var.self);
	});

	// If we needed to create one, we'll need it.
	if (dummy_sampler_id)
		variables.insert(dummy_sampler_id);

	return variables;
}

void Compiler::set_enabled_interface_variables(std::unordered_set<VariableID> active_variables)
{
	active_interface_variables = std::move(active_variables);
	check_active_interface_variables = true;
}

ShaderResources Compiler::get_shader_resources(const unordered_set<VariableID> *active_variables) const
{
	ShaderResources res;

	bool ssbo_instance_name = reflection_ssbo_instance_name_is_significant();

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, const SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);

		// It is possible for uniform storage classes to be passed as function parameters, so detect
		// that. To detect function parameters, check of StorageClass of variable is function scope.
		if (var.storage == StorageClassFunction || !type.pointer)
			return;

		if (active_variables && active_variables->find(var.self) == end(*active_variables))
			return;

		// In SPIR-V 1.4 and up, every global must be present in the entry point interface list,
		// not just IO variables.
		bool active_in_entry_point = true;
		if (ir.get_spirv_version() < 0x10400)
		{
			if (var.storage == StorageClassInput || var.storage == StorageClassOutput)
				active_in_entry_point = interface_variable_exists_in_entry_point(var.self);
		}
		else
			active_in_entry_point = interface_variable_exists_in_entry_point(var.self);

		if (!active_in_entry_point)
			return;

		bool is_builtin = is_builtin_variable(var);

		if (is_builtin)
		{
			if (var.storage != StorageClassInput && var.storage != StorageClassOutput)
				return;

			auto &list = var.storage == StorageClassInput ? res.builtin_inputs : res.builtin_outputs;
			BuiltInResource resource;

			if (has_decoration(type.self, DecorationBlock))
			{
				resource.resource = { var.self, var.basetype, type.self,
				                      get_remapped_declared_block_name(var.self, false) };

				for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++)
				{
					resource.value_type_id = type.member_types[i];
					resource.builtin = BuiltIn(get_member_decoration(type.self, i, DecorationBuiltIn));
					list.push_back(resource);
				}
			}
			else
			{
				bool strip_array =
						!has_decoration(var.self, DecorationPatch) && (
								get_execution_model() == ExecutionModelTessellationControl ||
								(get_execution_model() == ExecutionModelTessellationEvaluation &&
								 var.storage == StorageClassInput));

				resource.resource = { var.self, var.basetype, type.self, get_name(var.self) };

				if (strip_array && !type.array.empty())
					resource.value_type_id = get_variable_data_type(var).parent_type;
				else
					resource.value_type_id = get_variable_data_type_id(var);

				assert(resource.value_type_id);

				resource.builtin = BuiltIn(get_decoration(var.self, DecorationBuiltIn));
				list.push_back(std::move(resource));
			}
			return;
		}

		// Input
		if (var.storage == StorageClassInput)
		{
			if (has_decoration(type.self, DecorationBlock))
			{
				res.stage_inputs.push_back(
						{ var.self, var.basetype, type.self,
						  get_remapped_declared_block_name(var.self, false) });
			}
			else
				res.stage_inputs.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
		}
		// Subpass inputs
		else if (var.storage == StorageClassUniformConstant && type.image.dim == DimSubpassData)
		{
			res.subpass_inputs.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
		}
		// Outputs
		else if (var.storage == StorageClassOutput)
		{
			if (has_decoration(type.self, DecorationBlock))
			{
				res.stage_outputs.push_back(
						{ var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self, false) });
			}
			else
				res.stage_outputs.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
		}
		// UBOs
		else if (type.storage == StorageClassUniform && has_decoration(type.self, DecorationBlock))
		{
			res.uniform_buffers.push_back(
			    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self, false) });
		}
		// Old way to declare SSBOs.
		else if (type.storage == StorageClassUniform && has_decoration(type.self, DecorationBufferBlock))
		{
			res.storage_buffers.push_back(
			    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self, ssbo_instance_name) });
		}
		// Modern way to declare SSBOs.
		else if (type.storage == StorageClassStorageBuffer)
		{
			res.storage_buffers.push_back(
			    { var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self, ssbo_instance_name) });
		}
		// Push constant blocks
		else if (type.storage == StorageClassPushConstant)
		{
			// There can only be one push constant block, but keep the vector in case this restriction is lifted
			// in the future.
			res.push_constant_buffers.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
		}
		else if (type.storage == StorageClassShaderRecordBufferKHR)
		{
			res.shader_record_buffers.push_back({ var.self, var.basetype, type.self, get_remapped_declared_block_name(var.self, ssbo_instance_name) });
		}
		// Atomic counters
		else if (type.storage == StorageClassAtomicCounter)
		{
			res.atomic_counters.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
		}
		else if (type.storage == StorageClassUniformConstant)
		{
			if (type.basetype == SPIRType::Image)
			{
				// Images
				if (type.image.sampled == 2)
				{
					res.storage_images.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
				}
				// Separate images
				else if (type.image.sampled == 1)
				{
					res.separate_images.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
				}
			}
			// Separate samplers
			else if (type.basetype == SPIRType::Sampler)
			{
				res.separate_samplers.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
			}
			// Textures
			else if (type.basetype == SPIRType::SampledImage)
			{
				res.sampled_images.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
			}
			// Acceleration structures
			else if (type.basetype == SPIRType::AccelerationStructure)
			{
				res.acceleration_structures.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
			}
			else
			{
				res.gl_plain_uniforms.push_back({ var.self, var.basetype, type.self, get_name(var.self) });
			}
		}
	});

	return res;
}

bool Compiler::type_is_top_level_block(const SPIRType &type) const
{
	if (type.basetype != SPIRType::Struct)
		return false;
	return has_decoration(type.self, DecorationBlock) || has_decoration(type.self, DecorationBufferBlock);
}

bool Compiler::type_is_block_like(const SPIRType &type) const
{
	if (type_is_top_level_block(type))
		return true;

	if (type.basetype == SPIRType::Struct)
	{
		// Block-like types may have Offset decorations.
		for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++)
			if (has_member_decoration(type.self, i, DecorationOffset))
				return true;
	}

	return false;
}

void Compiler::parse_fixup()
{
	// Figure out specialization constants for work group sizes.
	for (auto id_ : ir.ids_for_constant_or_variable)
	{
		auto &id = ir.ids[id_];

		if (id.get_type() == TypeConstant)
		{
			auto &c = id.get<SPIRConstant>();
			if (has_decoration(c.self, DecorationBuiltIn) &&
			    BuiltIn(get_decoration(c.self, DecorationBuiltIn)) == BuiltInWorkgroupSize)
			{
				// In current SPIR-V, there can be just one constant like this.
				// All entry points will receive the constant value.
				// WorkgroupSize take precedence over LocalSizeId.
				for (auto &entry : ir.entry_points)
				{
					entry.second.workgroup_size.constant = c.self;
					entry.second.workgroup_size.x = c.scalar(0, 0);
					entry.second.workgroup_size.y = c.scalar(0, 1);
					entry.second.workgroup_size.z = c.scalar(0, 2);
				}
			}
		}
		else if (id.get_type() == TypeVariable)
		{
			auto &var = id.get<SPIRVariable>();
			if (var.storage == StorageClassPrivate || var.storage == StorageClassWorkgroup ||
			    var.storage == StorageClassTaskPayloadWorkgroupEXT ||
			    var.storage == StorageClassOutput)
			{
				global_variables.push_back(var.self);
			}
			if (variable_storage_is_aliased(var))
				aliased_variables.push_back(var.self);
		}
	}
}

void Compiler::update_name_cache(unordered_set<string> &cache_primary, const unordered_set<string> &cache_secondary,
                                 string &name)
{
	if (name.empty())
		return;

	const auto find_name = [&](const string &n) -> bool {
		if (cache_primary.find(n) != end(cache_primary))
			return true;

		if (&cache_primary != &cache_secondary)
			if (cache_secondary.find(n) != end(cache_secondary))
				return true;

		return false;
	};

	const auto insert_name = [&](const string &n) { cache_primary.insert(n); };

	if (!find_name(name))
	{
		insert_name(name);
		return;
	}

	uint32_t counter = 0;
	auto tmpname = name;

	bool use_linked_underscore = true;

	if (tmpname == "_")
	{
		// We cannot just append numbers, as we will end up creating internally reserved names.
		// Make it like _0_<counter> instead.
		tmpname += "0";
	}
	else if (tmpname.back() == '_')
	{
		// The last_character is an underscore, so we don't need to link in underscore.
		// This would violate double underscore rules.
		use_linked_underscore = false;
	}

	// If there is a collision (very rare),
	// keep tacking on extra identifier until it's unique.
	do
	{
		counter++;
		name = tmpname + (use_linked_underscore ? "_" : "") + convert_to_string(counter);
	} while (find_name(name));
	insert_name(name);
}

void Compiler::update_name_cache(unordered_set<string> &cache, string &name)
{
	update_name_cache(cache, cache, name);
}

void Compiler::set_name(ID id, const std::string &name)
{
	ir.set_name(id, name);
}

const SPIRType &Compiler::get_type(TypeID id) const
{
	return get<SPIRType>(id);
}

const SPIRType &Compiler::get_type_from_variable(VariableID id) const
{
	return get<SPIRType>(get<SPIRVariable>(id).basetype);
}

uint32_t Compiler::get_pointee_type_id(uint32_t type_id) const
{
	auto *p_type = &get<SPIRType>(type_id);
	if (p_type->pointer)
	{
		assert(p_type->parent_type);
		type_id = p_type->parent_type;
	}
	return type_id;
}

const SPIRType &Compiler::get_pointee_type(const SPIRType &type) const
{
	auto *p_type = &type;
	if (p_type->pointer)
	{
		assert(p_type->parent_type);
		p_type = &get<SPIRType>(p_type->parent_type);
	}
	return *p_type;
}

const SPIRType &Compiler::get_pointee_type(uint32_t type_id) const
{
	return get_pointee_type(get<SPIRType>(type_id));
}

uint32_t Compiler::get_variable_data_type_id(const SPIRVariable &var) const
{
	if (var.phi_variable || var.storage == spv::StorageClass::StorageClassAtomicCounter)
		return var.basetype;
	return get_pointee_type_id(var.basetype);
}

SPIRType &Compiler::get_variable_data_type(const SPIRVariable &var)
{
	return get<SPIRType>(get_variable_data_type_id(var));
}

const SPIRType &Compiler::get_variable_data_type(const SPIRVariable &var) const
{
	return get<SPIRType>(get_variable_data_type_id(var));
}

SPIRType &Compiler::get_variable_element_type(const SPIRVariable &var)
{
	SPIRType *type = &get_variable_data_type(var);
	if (is_array(*type))
		type = &get<SPIRType>(type->parent_type);
	return *type;
}

const SPIRType &Compiler::get_variable_element_type(const SPIRVariable &var) const
{
	const SPIRType *type = &get_variable_data_type(var);
	if (is_array(*type))
		type = &get<SPIRType>(type->parent_type);
	return *type;
}

bool Compiler::is_sampled_image_type(const SPIRType &type)
{
	return (type.basetype == SPIRType::Image || type.basetype == SPIRType::SampledImage) && type.image.sampled == 1 &&
	       type.image.dim != DimBuffer;
}

void Compiler::set_member_decoration_string(TypeID id, uint32_t index, spv::Decoration decoration,
                                            const std::string &argument)
{
	ir.set_member_decoration_string(id, index, decoration, argument);
}

void Compiler::set_member_decoration(TypeID id, uint32_t index, Decoration decoration, uint32_t argument)
{
	ir.set_member_decoration(id, index, decoration, argument);
}

void Compiler::set_member_name(TypeID id, uint32_t index, const std::string &name)
{
	ir.set_member_name(id, index, name);
}

const std::string &Compiler::get_member_name(TypeID id, uint32_t index) const
{
	return ir.get_member_name(id, index);
}

void Compiler::set_qualified_name(uint32_t id, const string &name)
{
	ir.meta[id].decoration.qualified_alias = name;
}

void Compiler::set_member_qualified_name(uint32_t type_id, uint32_t index, const std::string &name)
{
	ir.meta[type_id].members.resize(max(ir.meta[type_id].members.size(), size_t(index) + 1));
	ir.meta[type_id].members[index].qualified_alias = name;
}

const string &Compiler::get_member_qualified_name(TypeID type_id, uint32_t index) const
{
	auto *m = ir.find_meta(type_id);
	if (m && index < m->members.size())
		return m->members[index].qualified_alias;
	else
		return ir.get_empty_string();
}

uint32_t Compiler::get_member_decoration(TypeID id, uint32_t index, Decoration decoration) const
{
	return ir.get_member_decoration(id, index, decoration);
}

const Bitset &Compiler::get_member_decoration_bitset(TypeID id, uint32_t index) const
{
	return ir.get_member_decoration_bitset(id, index);
}

bool Compiler::has_member_decoration(TypeID id, uint32_t index, Decoration decoration) const
{
	return ir.has_member_decoration(id, index, decoration);
}

void Compiler::unset_member_decoration(TypeID id, uint32_t index, Decoration decoration)
{
	ir.unset_member_decoration(id, index, decoration);
}

void Compiler::set_decoration_string(ID id, spv::Decoration decoration, const std::string &argument)
{
	ir.set_decoration_string(id, decoration, argument);
}

void Compiler::set_decoration(ID id, Decoration decoration, uint32_t argument)
{
	ir.set_decoration(id, decoration, argument);
}

void Compiler::set_extended_decoration(uint32_t id, ExtendedDecorations decoration, uint32_t value)
{
	auto &dec = ir.meta[id].decoration;
	dec.extended.flags.set(decoration);
	dec.extended.values[decoration] = value;
}

void Compiler::set_extended_member_decoration(uint32_t type, uint32_t index, ExtendedDecorations decoration,
                                              uint32_t value)
{
	ir.meta[type].members.resize(max(ir.meta[type].members.size(), size_t(index) + 1));
	auto &dec = ir.meta[type].members[index];
	dec.extended.flags.set(decoration);
	dec.extended.values[decoration] = value;
}

static uint32_t get_default_extended_decoration(ExtendedDecorations decoration)
{
	switch (decoration)
	{
	case SPIRVCrossDecorationResourceIndexPrimary:
	case SPIRVCrossDecorationResourceIndexSecondary:
	case SPIRVCrossDecorationResourceIndexTertiary:
	case SPIRVCrossDecorationResourceIndexQuaternary:
	case SPIRVCrossDecorationInterfaceMemberIndex:
		return ~(0u);

	default:
		return 0;
	}
}

uint32_t Compiler::get_extended_decoration(uint32_t id, ExtendedDecorations decoration) const
{
	auto *m = ir.find_meta(id);
	if (!m)
		return 0;

	auto &dec = m->decoration;

	if (!dec.extended.flags.get(decoration))
		return get_default_extended_decoration(decoration);

	return dec.extended.values[decoration];
}

uint32_t Compiler::get_extended_member_decoration(uint32_t type, uint32_t index, ExtendedDecorations decoration) const
{
	auto *m = ir.find_meta(type);
	if (!m)
		return 0;

	if (index >= m->members.size())
		return 0;

	auto &dec = m->members[index];
	if (!dec.extended.flags.get(decoration))
		return get_default_extended_decoration(decoration);
	return dec.extended.values[decoration];
}

bool Compiler::has_extended_decoration(uint32_t id, ExtendedDecorations decoration) const
{
	auto *m = ir.find_meta(id);
	if (!m)
		return false;

	auto &dec = m->decoration;
	return dec.extended.flags.get(decoration);
}

bool Compiler::has_extended_member_decoration(uint32_t type, uint32_t index, ExtendedDecorations decoration) const
{
	auto *m = ir.find_meta(type);
	if (!m)
		return false;

	if (index >= m->members.size())
		return false;

	auto &dec = m->members[index];
	return dec.extended.flags.get(decoration);
}

void Compiler::unset_extended_decoration(uint32_t id, ExtendedDecorations decoration)
{
	auto &dec = ir.meta[id].decoration;
	dec.extended.flags.clear(decoration);
	dec.extended.values[decoration] = 0;
}

void Compiler::unset_extended_member_decoration(uint32_t type, uint32_t index, ExtendedDecorations decoration)
{
	ir.meta[type].members.resize(max(ir.meta[type].members.size(), size_t(index) + 1));
	auto &dec = ir.meta[type].members[index];
	dec.extended.flags.clear(decoration);
	dec.extended.values[decoration] = 0;
}

StorageClass Compiler::get_storage_class(VariableID id) const
{
	return get<SPIRVariable>(id).storage;
}

const std::string &Compiler::get_name(ID id) const
{
	return ir.get_name(id);
}

const std::string Compiler::get_fallback_name(ID id) const
{
	return join("_", id);
}

const std::string Compiler::get_block_fallback_name(VariableID id) const
{
	auto &var = get<SPIRVariable>(id);
	if (get_name(id).empty())
		return join("_", get<SPIRType>(var.basetype).self, "_", id);
	else
		return get_name(id);
}

const Bitset &Compiler::get_decoration_bitset(ID id) const
{
	return ir.get_decoration_bitset(id);
}

bool Compiler::has_decoration(ID id, Decoration decoration) const
{
	return ir.has_decoration(id, decoration);
}

const string &Compiler::get_decoration_string(ID id, Decoration decoration) const
{
	return ir.get_decoration_string(id, decoration);
}

const string &Compiler::get_member_decoration_string(TypeID id, uint32_t index, Decoration decoration) const
{
	return ir.get_member_decoration_string(id, index, decoration);
}

uint32_t Compiler::get_decoration(ID id, Decoration decoration) const
{
	return ir.get_decoration(id, decoration);
}

void Compiler::unset_decoration(ID id, Decoration decoration)
{
	ir.unset_decoration(id, decoration);
}

bool Compiler::get_binary_offset_for_decoration(VariableID id, spv::Decoration decoration, uint32_t &word_offset) const
{
	auto *m = ir.find_meta(id);
	if (!m)
		return false;

	auto &word_offsets = m->decoration_word_offset;
	auto itr = word_offsets.find(decoration);
	if (itr == end(word_offsets))
		return false;

	word_offset = itr->second;
	return true;
}

bool Compiler::block_is_noop(const SPIRBlock &block) const
{
	if (block.terminator != SPIRBlock::Direct)
		return false;

	auto &child = get<SPIRBlock>(block.next_block);

	// If this block participates in PHI, the block isn't really noop.
	for (auto &phi : block.phi_variables)
		if (phi.parent == block.self || phi.parent == child.self)
			return false;

	for (auto &phi : child.phi_variables)
		if (phi.parent == block.self)
			return false;

	// Verify all instructions have no semantic impact.
	for (auto &i : block.ops)
	{
		auto op = static_cast<Op>(i.op);

		switch (op)
		{
		// Non-Semantic instructions.
		case OpLine:
		case OpNoLine:
			break;

		case OpExtInst:
		{
			auto *ops = stream(i);
			auto ext = get<SPIRExtension>(ops[2]).ext;

			bool ext_is_nonsemantic_only =
				ext == SPIRExtension::NonSemanticShaderDebugInfo ||
				ext == SPIRExtension::SPV_debug_info ||
				ext == SPIRExtension::NonSemanticGeneric;

			if (!ext_is_nonsemantic_only)
				return false;

			break;
		}

		default:
			return false;
		}
	}

	return true;
}

bool Compiler::block_is_loop_candidate(const SPIRBlock &block, SPIRBlock::Method method) const
{
	// Tried and failed.
	if (block.disable_block_optimization || block.complex_continue)
		return false;

	if (method == SPIRBlock::MergeToSelectForLoop || method == SPIRBlock::MergeToSelectContinueForLoop)
	{
		// Try to detect common for loop pattern
		// which the code backend can use to create cleaner code.
		// for(;;) { if (cond) { some_body; } else { break; } }
		// is the pattern we're looking for.
		const auto *false_block = maybe_get<SPIRBlock>(block.false_block);
		const auto *true_block = maybe_get<SPIRBlock>(block.true_block);
		const auto *merge_block = maybe_get<SPIRBlock>(block.merge_block);

		bool false_block_is_merge = block.false_block == block.merge_block ||
		                            (false_block && merge_block && execution_is_noop(*false_block, *merge_block));

		bool true_block_is_merge = block.true_block == block.merge_block ||
		                           (true_block && merge_block && execution_is_noop(*true_block, *merge_block));

		bool positive_candidate =
		    block.true_block != block.merge_block && block.true_block != block.self && false_block_is_merge;

		bool negative_candidate =
		    block.false_block != block.merge_block && block.false_block != block.self && true_block_is_merge;

		bool ret = block.terminator == SPIRBlock::Select && block.merge == SPIRBlock::MergeLoop &&
		           (positive_candidate || negative_candidate);

		if (ret && positive_candidate && method == SPIRBlock::MergeToSelectContinueForLoop)
			ret = block.true_block == block.continue_block;
		else if (ret && negative_candidate && method == SPIRBlock::MergeToSelectContinueForLoop)
			ret = block.false_block == block.continue_block;

		// If we have OpPhi which depends on branches which came from our own block,
		// we need to flush phi variables in else block instead of a trivial break,
		// so we cannot assume this is a for loop candidate.
		if (ret)
		{
			for (auto &phi : block.phi_variables)
				if (phi.parent == block.self)
					return false;

			auto *merge = maybe_get<SPIRBlock>(block.merge_block);
			if (merge)
				for (auto &phi : merge->phi_variables)
					if (phi.parent == block.self)
						return false;
		}
		return ret;
	}
	else if (method == SPIRBlock::MergeToDirectForLoop)
	{
		// Empty loop header that just sets up merge target
		// and branches to loop body.
		bool ret = block.terminator == SPIRBlock::Direct && block.merge == SPIRBlock::MergeLoop && block_is_noop(block);

		if (!ret)
			return false;

		auto &child = get<SPIRBlock>(block.next_block);

		const auto *false_block = maybe_get<SPIRBlock>(child.false_block);
		const auto *true_block = maybe_get<SPIRBlock>(child.true_block);
		const auto *merge_block = maybe_get<SPIRBlock>(block.merge_block);

		bool false_block_is_merge = child.false_block == block.merge_block ||
		                            (false_block && merge_block && execution_is_noop(*false_block, *merge_block));

		bool true_block_is_merge = child.true_block == block.merge_block ||
		                           (true_block && merge_block && execution_is_noop(*true_block, *merge_block));

		bool positive_candidate =
		    child.true_block != block.merge_block && child.true_block != block.self && false_block_is_merge;

		bool negative_candidate =
		    child.false_block != block.merge_block && child.false_block != block.self && true_block_is_merge;

		ret = child.terminator == SPIRBlock::Select && child.merge == SPIRBlock::MergeNone &&
		      (positive_candidate || negative_candidate);

		if (ret)
		{
			auto *merge = maybe_get<SPIRBlock>(block.merge_block);
			if (merge)
				for (auto &phi : merge->phi_variables)
					if (phi.parent == block.self || phi.parent == child.false_block)
						return false;
		}

		return ret;
	}
	else
		return false;
}

bool Compiler::execution_is_noop(const SPIRBlock &from, const SPIRBlock &to) const
{
	if (!execution_is_branchless(from, to))
		return false;

	auto *start = &from;
	for (;;)
	{
		if (start->self == to.self)
			return true;

		if (!block_is_noop(*start))
			return false;

		auto &next = get<SPIRBlock>(start->next_block);
		start = &next;
	}
}

bool Compiler::execution_is_branchless(const SPIRBlock &from, const SPIRBlock &to) const
{
	auto *start = &from;
	for (;;)
	{
		if (start->self == to.self)
			return true;

		if (start->terminator == SPIRBlock::Direct && start->merge == SPIRBlock::MergeNone)
			start = &get<SPIRBlock>(start->next_block);
		else
			return false;
	}
}

bool Compiler::execution_is_direct_branch(const SPIRBlock &from, const SPIRBlock &to) const
{
	return from.terminator == SPIRBlock::Direct && from.merge == SPIRBlock::MergeNone && from.next_block == to.self;
}

SPIRBlock::ContinueBlockType Compiler::continue_block_type(const SPIRBlock &block) const
{
	// The block was deemed too complex during code emit, pick conservative fallback paths.
	if (block.complex_continue)
		return SPIRBlock::ComplexLoop;

	// In older glslang output continue block can be equal to the loop header.
	// In this case, execution is clearly branchless, so just assume a while loop header here.
	if (block.merge == SPIRBlock::MergeLoop)
		return SPIRBlock::WhileLoop;

	if (block.loop_dominator == BlockID(SPIRBlock::NoDominator))
	{
		// Continue block is never reached from CFG.
		return SPIRBlock::ComplexLoop;
	}

	auto &dominator = get<SPIRBlock>(block.loop_dominator);

	if (execution_is_noop(block, dominator))
		return SPIRBlock::WhileLoop;
	else if (execution_is_branchless(block, dominator))
		return SPIRBlock::ForLoop;
	else
	{
		const auto *false_block = maybe_get<SPIRBlock>(block.false_block);
		const auto *true_block = maybe_get<SPIRBlock>(block.true_block);
		const auto *merge_block = maybe_get<SPIRBlock>(dominator.merge_block);

		// If we need to flush Phi in this block, we cannot have a DoWhile loop.
		bool flush_phi_to_false = false_block && flush_phi_required(block.self, block.false_block);
		bool flush_phi_to_true = true_block && flush_phi_required(block.self, block.true_block);
		if (flush_phi_to_false || flush_phi_to_true)
			return SPIRBlock::ComplexLoop;

		bool positive_do_while = block.true_block == dominator.self &&
		                         (block.false_block == dominator.merge_block ||
		                          (false_block && merge_block && execution_is_noop(*false_block, *merge_block)));

		bool negative_do_while = block.false_block == dominator.self &&
		                         (block.true_block == dominator.merge_block ||
		                          (true_block && merge_block && execution_is_noop(*true_block, *merge_block)));

		if (block.merge == SPIRBlock::MergeNone && block.terminator == SPIRBlock::Select &&
		    (positive_do_while || negative_do_while))
		{
			return SPIRBlock::DoWhileLoop;
		}
		else
			return SPIRBlock::ComplexLoop;
	}
}

const SmallVector<SPIRBlock::Case> &Compiler::get_case_list(const SPIRBlock &block) const
{
	uint32_t width = 0;

	// First we check if we can get the type directly from the block.condition
	// since it can be a SPIRConstant or a SPIRVariable.
	if (const auto *constant = maybe_get<SPIRConstant>(block.condition))
	{
		const auto &type = get<SPIRType>(constant->constant_type);
		width = type.width;
	}
	else if (const auto *op = maybe_get<SPIRConstantOp>(block.condition))
	{
		const auto &type = get<SPIRType>(op->basetype);
		width = type.width;
	}
	else if (const auto *var = maybe_get<SPIRVariable>(block.condition))
	{
		const auto &type = get<SPIRType>(var->basetype);
		width = type.width;
	}
	else if (const auto *undef = maybe_get<SPIRUndef>(block.condition))
	{
		const auto &type = get<SPIRType>(undef->basetype);
		width = type.width;
	}
	else
	{
		auto search = ir.load_type_width.find(block.condition);
		if (search == ir.load_type_width.end())
		{
			SPIRV_CROSS_THROW("Use of undeclared variable on a switch statement.");
		}

		width = search->second;
	}

	if (width > 32)
		return block.cases_64bit;

	return block.cases_32bit;
}

bool Compiler::traverse_all_reachable_opcodes(const SPIRBlock &block, OpcodeHandler &handler) const
{
	handler.set_current_block(block);
	handler.rearm_current_block(block);

	// Ideally, perhaps traverse the CFG instead of all blocks in order to eliminate dead blocks,
	// but this shouldn't be a problem in practice unless the SPIR-V is doing insane things like recursing
	// inside dead blocks ...
	for (auto &i : block.ops)
	{
		auto ops = stream(i);
		auto op = static_cast<Op>(i.op);

		if (!handler.handle(op, ops, i.length))
			return false;

		if (op == OpFunctionCall)
		{
			auto &func = get<SPIRFunction>(ops[2]);
			if (handler.follow_function_call(func))
			{
				if (!handler.begin_function_scope(ops, i.length))
					return false;
				if (!traverse_all_reachable_opcodes(get<SPIRFunction>(ops[2]), handler))
					return false;
				if (!handler.end_function_scope(ops, i.length))
					return false;

				handler.rearm_current_block(block);
			}
		}
	}

	if (!handler.handle_terminator(block))
		return false;

	return true;
}

bool Compiler::traverse_all_reachable_opcodes(const SPIRFunction &func, OpcodeHandler &handler) const
{
	for (auto block : func.blocks)
		if (!traverse_all_reachable_opcodes(get<SPIRBlock>(block), handler))
			return false;

	return true;
}

uint32_t Compiler::type_struct_member_offset(const SPIRType &type, uint32_t index) const
{
	auto *type_meta = ir.find_meta(type.self);
	if (type_meta)
	{
		// Decoration must be set in valid SPIR-V, otherwise throw.
		auto &dec = type_meta->members[index];
		if (dec.decoration_flags.get(DecorationOffset))
			return dec.offset;
		else
			SPIRV_CROSS_THROW("Struct member does not have Offset set.");
	}
	else
		SPIRV_CROSS_THROW("Struct member does not have Offset set.");
}

uint32_t Compiler::type_struct_member_array_stride(const SPIRType &type, uint32_t index) const
{
	auto *type_meta = ir.find_meta(type.member_types[index]);
	if (type_meta)
	{
		// Decoration must be set in valid SPIR-V, otherwise throw.
		// ArrayStride is part of the array type not OpMemberDecorate.
		auto &dec = type_meta->decoration;
		if (dec.decoration_flags.get(DecorationArrayStride))
			return dec.array_stride;
		else
			SPIRV_CROSS_THROW("Struct member does not have ArrayStride set.");
	}
	else
		SPIRV_CROSS_THROW("Struct member does not have ArrayStride set.");
}

uint32_t Compiler::type_struct_member_matrix_stride(const SPIRType &type, uint32_t index) const
{
	auto *type_meta = ir.find_meta(type.self);
	if (type_meta)
	{
		// Decoration must be set in valid SPIR-V, otherwise throw.
		// MatrixStride is part of OpMemberDecorate.
		auto &dec = type_meta->members[index];
		if (dec.decoration_flags.get(DecorationMatrixStride))
			return dec.matrix_stride;
		else
			SPIRV_CROSS_THROW("Struct member does not have MatrixStride set.");
	}
	else
		SPIRV_CROSS_THROW("Struct member does not have MatrixStride set.");
}

size_t Compiler::get_declared_struct_size(const SPIRType &type) const
{
	if (type.member_types.empty())
		SPIRV_CROSS_THROW("Declared struct in block cannot be empty.");

	// Offsets can be declared out of order, so we need to deduce the actual size
	// based on last member instead.
	uint32_t member_index = 0;
	size_t highest_offset = 0;
	for (uint32_t i = 0; i < uint32_t(type.member_types.size()); i++)
	{
		size_t offset = type_struct_member_offset(type, i);
		if (offset > highest_offset)
		{
			highest_offset = offset;
			member_index = i;
		}
	}

	size_t size = get_declared_struct_member_size(type, member_index);
	return highest_offset + size;
}

size_t Compiler::get_declared_struct_size_runtime_array(const SPIRType &type, size_t array_size) const
{
	if (type.member_types.empty())
		SPIRV_CROSS_THROW("Declared struct in block cannot be empty.");

	size_t size = get_declared_struct_size(type);
	auto &last_type = get<SPIRType>(type.member_types.back());
	if (!last_type.array.empty() && last_type.array_size_literal[0] && last_type.array[0] == 0) // Runtime array
		size += array_size * type_struct_member_array_stride(type, uint32_t(type.member_types.size() - 1));

	return size;
}

uint32_t Compiler::evaluate_spec_constant_u32(const SPIRConstantOp &spec) const
{
	auto &result_type = get<SPIRType>(spec.basetype);
	if (result_type.basetype != SPIRType::UInt && result_type.basetype != SPIRType::Int &&
	    result_type.basetype != SPIRType::Boolean)
	{
		SPIRV_CROSS_THROW(
		    "Only 32-bit integers and booleans are currently supported when evaluating specialization constants.\n");
	}

	if (!is_scalar(result_type))
		SPIRV_CROSS_THROW("Spec constant evaluation must be a scalar.\n");

	uint32_t value = 0;

	const auto eval_u32 = [&](uint32_t id) -> uint32_t {
		auto &type = expression_type(id);
		if (type.basetype != SPIRType::UInt && type.basetype != SPIRType::Int && type.basetype != SPIRType::Boolean)
		{
			SPIRV_CROSS_THROW("Only 32-bit integers and booleans are currently supported when evaluating "
			                  "specialization constants.\n");
		}

		if (!is_scalar(type))
			SPIRV_CROSS_THROW("Spec constant evaluation must be a scalar.\n");
		if (const auto *c = this->maybe_get<SPIRConstant>(id))
			return c->scalar();
		else
			return evaluate_spec_constant_u32(this->get<SPIRConstantOp>(id));
	};

#define binary_spec_op(op, binary_op)                                              \
	case Op##op:                                                                   \
		value = eval_u32(spec.arguments[0]) binary_op eval_u32(spec.arguments[1]); \
		break
#define binary_spec_op_cast(op, binary_op, type)                                                         \
	case Op##op:                                                                                         \
		value = uint32_t(type(eval_u32(spec.arguments[0])) binary_op type(eval_u32(spec.arguments[1]))); \
		break

	// Support the basic opcodes which are typically used when computing array sizes.
	switch (spec.opcode)
	{
		binary_spec_op(IAdd, +);
		binary_spec_op(ISub, -);
		binary_spec_op(IMul, *);
		binary_spec_op(BitwiseAnd, &);
		binary_spec_op(BitwiseOr, |);
		binary_spec_op(BitwiseXor, ^);
		binary_spec_op(LogicalAnd, &);
		binary_spec_op(LogicalOr, |);
		binary_spec_op(ShiftLeftLogical, <<);
		binary_spec_op(ShiftRightLogical, >>);
		binary_spec_op_cast(ShiftRightArithmetic, >>, int32_t);
		binary_spec_op(LogicalEqual, ==);
		binary_spec_op(LogicalNotEqual, !=);
		binary_spec_op(IEqual, ==);
		binary_spec_op(INotEqual, !=);
		binary_spec_op(ULessThan, <);
		binary_spec_op(ULessThanEqual, <=);
		binary_spec_op(UGreaterThan, >);
		binary_spec_op(UGreaterThanEqual, >=);
		binary_spec_op_cast(SLessThan, <, int32_t);
		binary_spec_op_cast(SLessThanEqual, <=, int32_t);
		binary_spec_op_cast(SGreaterThan, >, int32_t);
		binary_spec_op_cast(SGreaterThanEqual, >=, int32_t);
#undef binary_spec_op
#undef binary_spec_op_cast

	case OpLogicalNot:
		value = uint32_t(!eval_u32(spec.arguments[0]));
		break;

	case OpNot:
		value = ~eval_u32(spec.arguments[0]);
		break;

	case OpSNegate:
		value = uint32_t(-int32_t(eval_u32(spec.arguments[0])));
		break;

	case OpSelect:
		value = eval_u32(spec.arguments[0]) ? eval_u32(spec.arguments[1]) : eval_u32(spec.arguments[2]);
		break;

	case OpUMod:
	{
		uint32_t a = eval_u32(spec.arguments[0]);
		uint32_t b = eval_u32(spec.arguments[1]);
		if (b == 0)
			SPIRV_CROSS_THROW("Undefined behavior in UMod, b == 0.\n");
		value = a % b;
		break;
	}

	case OpSRem:
	{
		auto a = int32_t(eval_u32(spec.arguments[0]));
		auto b = int32_t(eval_u32(spec.arguments[1]));
		if (b == 0)
			SPIRV_CROSS_THROW("Undefined behavior in SRem, b == 0.\n");
		value = a % b;
		break;
	}

	case OpSMod:
	{
		auto a = int32_t(eval_u32(spec.arguments[0]));
		auto b = int32_t(eval_u32(spec.arguments[1]));
		if (b == 0)
			SPIRV_CROSS_THROW("Undefined behavior in SMod, b == 0.\n");
		auto v = a % b;

		// Makes sure we match the sign of b, not a.
		if ((b < 0 && v > 0) || (b > 0 && v < 0))
			v += b;
		value = v;
		break;
	}

	case OpUDiv:
	{
		uint32_t a = eval_u32(spec.arguments[0]);
		uint32_t b = eval_u32(spec.arguments[1]);
		if (b == 0)
			SPIRV_CROSS_THROW("Undefined behavior in UDiv, b == 0.\n");
		value = a / b;
		break;
	}

	case OpSDiv:
	{
		auto a = int32_t(eval_u32(spec.arguments[0]));
		auto b = int32_t(eval_u32(spec.arguments[1]));
		if (b == 0)
			SPIRV_CROSS_THROW("Undefined behavior in SDiv, b == 0.\n");
		value = a / b;
		break;
	}

	default:
		SPIRV_CROSS_THROW("Unsupported spec constant opcode for evaluation.\n");
	}

	return value;
}

uint32_t Compiler::evaluate_constant_u32(uint32_t id) const
{
	if (const auto *c = maybe_get<SPIRConstant>(id))
		return c->scalar();
	else
		return evaluate_spec_constant_u32(get<SPIRConstantOp>(id));
}

size_t Compiler::get_declared_struct_member_size(const SPIRType &struct_type, uint32_t index) const
{
	if (struct_type.member_types.empty())
		SPIRV_CROSS_THROW("Declared struct in block cannot be empty.");

	auto &flags = get_member_decoration_bitset(struct_type.self, index);
	auto &type = get<SPIRType>(struct_type.member_types[index]);

	switch (type.basetype)
	{
	case SPIRType::Unknown:
	case SPIRType::Void:
	case SPIRType::Boolean: // Bools are purely logical, and cannot be used for externally visible types.
	case SPIRType::AtomicCounter:
	case SPIRType::Image:
	case SPIRType::SampledImage:
	case SPIRType::Sampler:
		SPIRV_CROSS_THROW("Querying size for object with opaque size.");

	default:
		break;
	}

	if (type.pointer && type.storage == StorageClassPhysicalStorageBuffer)
	{
		// Check if this is a top-level pointer type, and not an array of pointers.
		if (type.pointer_depth > get<SPIRType>(type.parent_type).pointer_depth)
			return 8;
	}

	if (!type.array.empty())
	{
		// For arrays, we can use ArrayStride to get an easy check.
		bool array_size_literal = type.array_size_literal.back();
		uint32_t array_size = array_size_literal ? type.array.back() : evaluate_constant_u32(type.array.back());
		return type_struct_member_array_stride(struct_type, index) * array_size;
	}
	else if (type.basetype == SPIRType::Struct)
	{
		return get_declared_struct_size(type);
	}
	else
	{
		unsigned vecsize = type.vecsize;
		unsigned columns = type.columns;

		// Vectors.
		if (columns == 1)
		{
			size_t component_size = type.width / 8;
			return vecsize * component_size;
		}
		else
		{
			uint32_t matrix_stride = type_struct_member_matrix_stride(struct_type, index);

			// Per SPIR-V spec, matrices must be tightly packed and aligned up for vec3 accesses.
			if (flags.get(DecorationRowMajor))
				return matrix_stride * vecsize;
			else if (flags.get(DecorationColMajor))
				return matrix_stride * columns;
			else
				SPIRV_CROSS_THROW("Either row-major or column-major must be declared for matrices.");
		}
	}
}

bool Compiler::BufferAccessHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	if (opcode != OpAccessChain && opcode != OpInBoundsAccessChain && opcode != OpPtrAccessChain)
		return true;

	bool ptr_chain = (opcode == OpPtrAccessChain);

	// Invalid SPIR-V.
	if (length < (ptr_chain ? 5u : 4u))
		return false;

	if (args[2] != id)
		return true;

	// Don't bother traversing the entire access chain tree yet.
	// If we access a struct member, assume we access the entire member.
	uint32_t index = compiler.get<SPIRConstant>(args[ptr_chain ? 4 : 3]).scalar();

	// Seen this index already.
	if (seen.find(index) != end(seen))
		return true;
	seen.insert(index);

	auto &type = compiler.expression_type(id);
	uint32_t offset = compiler.type_struct_member_offset(type, index);

	size_t range;
	// If we have another member in the struct, deduce the range by looking at the next member.
	// This is okay since structs in SPIR-V can have padding, but Offset decoration must be
	// monotonically increasing.
	// Of course, this doesn't take into account if the SPIR-V for some reason decided to add
	// very large amounts of padding, but that's not really a big deal.
	if (index + 1 < type.member_types.size())
	{
		range = compiler.type_struct_member_offset(type, index + 1) - offset;
	}
	else
	{
		// No padding, so just deduce it from the size of the member directly.
		range = compiler.get_declared_struct_member_size(type, index);
	}

	ranges.push_back({ index, offset, range });
	return true;
}

SmallVector<BufferRange> Compiler::get_active_buffer_ranges(VariableID id) const
{
	SmallVector<BufferRange> ranges;
	BufferAccessHandler handler(*this, ranges, id);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);
	return ranges;
}

bool Compiler::types_are_logically_equivalent(const SPIRType &a, const SPIRType &b) const
{
	if (a.basetype != b.basetype)
		return false;
	if (a.width != b.width)
		return false;
	if (a.vecsize != b.vecsize)
		return false;
	if (a.columns != b.columns)
		return false;
	if (a.array.size() != b.array.size())
		return false;

	size_t array_count = a.array.size();
	if (array_count && memcmp(a.array.data(), b.array.data(), array_count * sizeof(uint32_t)) != 0)
		return false;

	if (a.basetype == SPIRType::Image || a.basetype == SPIRType::SampledImage)
	{
		if (memcmp(&a.image, &b.image, sizeof(SPIRType::Image)) != 0)
			return false;
	}

	if (a.member_types.size() != b.member_types.size())
		return false;

	size_t member_types = a.member_types.size();
	for (size_t i = 0; i < member_types; i++)
	{
		if (!types_are_logically_equivalent(get<SPIRType>(a.member_types[i]), get<SPIRType>(b.member_types[i])))
			return false;
	}

	return true;
}

const Bitset &Compiler::get_execution_mode_bitset() const
{
	return get_entry_point().flags;
}

void Compiler::set_execution_mode(ExecutionMode mode, uint32_t arg0, uint32_t arg1, uint32_t arg2)
{
	auto &execution = get_entry_point();

	execution.flags.set(mode);
	switch (mode)
	{
	case ExecutionModeLocalSize:
		execution.workgroup_size.x = arg0;
		execution.workgroup_size.y = arg1;
		execution.workgroup_size.z = arg2;
		break;

	case ExecutionModeLocalSizeId:
		execution.workgroup_size.id_x = arg0;
		execution.workgroup_size.id_y = arg1;
		execution.workgroup_size.id_z = arg2;
		break;

	case ExecutionModeInvocations:
		execution.invocations = arg0;
		break;

	case ExecutionModeOutputVertices:
		execution.output_vertices = arg0;
		break;

	case ExecutionModeOutputPrimitivesEXT:
		execution.output_primitives = arg0;
		break;

	default:
		break;
	}
}

void Compiler::unset_execution_mode(ExecutionMode mode)
{
	auto &execution = get_entry_point();
	execution.flags.clear(mode);
}

uint32_t Compiler::get_work_group_size_specialization_constants(SpecializationConstant &x, SpecializationConstant &y,
                                                                SpecializationConstant &z) const
{
	auto &execution = get_entry_point();
	x = { 0, 0 };
	y = { 0, 0 };
	z = { 0, 0 };

	// WorkgroupSize builtin takes precedence over LocalSize / LocalSizeId.
	if (execution.workgroup_size.constant != 0)
	{
		auto &c = get<SPIRConstant>(execution.workgroup_size.constant);

		if (c.m.c[0].id[0] != ID(0))
		{
			x.id = c.m.c[0].id[0];
			x.constant_id = get_decoration(c.m.c[0].id[0], DecorationSpecId);
		}

		if (c.m.c[0].id[1] != ID(0))
		{
			y.id = c.m.c[0].id[1];
			y.constant_id = get_decoration(c.m.c[0].id[1], DecorationSpecId);
		}

		if (c.m.c[0].id[2] != ID(0))
		{
			z.id = c.m.c[0].id[2];
			z.constant_id = get_decoration(c.m.c[0].id[2], DecorationSpecId);
		}
	}
	else if (execution.flags.get(ExecutionModeLocalSizeId))
	{
		auto &cx = get<SPIRConstant>(execution.workgroup_size.id_x);
		if (cx.specialization)
		{
			x.id = execution.workgroup_size.id_x;
			x.constant_id = get_decoration(execution.workgroup_size.id_x, DecorationSpecId);
		}

		auto &cy = get<SPIRConstant>(execution.workgroup_size.id_y);
		if (cy.specialization)
		{
			y.id = execution.workgroup_size.id_y;
			y.constant_id = get_decoration(execution.workgroup_size.id_y, DecorationSpecId);
		}

		auto &cz = get<SPIRConstant>(execution.workgroup_size.id_z);
		if (cz.specialization)
		{
			z.id = execution.workgroup_size.id_z;
			z.constant_id = get_decoration(execution.workgroup_size.id_z, DecorationSpecId);
		}
	}

	return execution.workgroup_size.constant;
}

uint32_t Compiler::get_execution_mode_argument(spv::ExecutionMode mode, uint32_t index) const
{
	auto &execution = get_entry_point();
	switch (mode)
	{
	case ExecutionModeLocalSizeId:
		if (execution.flags.get(ExecutionModeLocalSizeId))
		{
			switch (index)
			{
			case 0:
				return execution.workgroup_size.id_x;
			case 1:
				return execution.workgroup_size.id_y;
			case 2:
				return execution.workgroup_size.id_z;
			default:
				return 0;
			}
		}
		else
			return 0;

	case ExecutionModeLocalSize:
		switch (index)
		{
		case 0:
			if (execution.flags.get(ExecutionModeLocalSizeId) && execution.workgroup_size.id_x != 0)
				return get<SPIRConstant>(execution.workgroup_size.id_x).scalar();
			else
				return execution.workgroup_size.x;
		case 1:
			if (execution.flags.get(ExecutionModeLocalSizeId) && execution.workgroup_size.id_y != 0)
				return get<SPIRConstant>(execution.workgroup_size.id_y).scalar();
			else
				return execution.workgroup_size.y;
		case 2:
			if (execution.flags.get(ExecutionModeLocalSizeId) && execution.workgroup_size.id_z != 0)
				return get<SPIRConstant>(execution.workgroup_size.id_z).scalar();
			else
				return execution.workgroup_size.z;
		default:
			return 0;
		}

	case ExecutionModeInvocations:
		return execution.invocations;

	case ExecutionModeOutputVertices:
		return execution.output_vertices;

	case ExecutionModeOutputPrimitivesEXT:
		return execution.output_primitives;

	default:
		return 0;
	}
}

ExecutionModel Compiler::get_execution_model() const
{
	auto &execution = get_entry_point();
	return execution.model;
}

bool Compiler::is_tessellation_shader(ExecutionModel model)
{
	return model == ExecutionModelTessellationControl || model == ExecutionModelTessellationEvaluation;
}

bool Compiler::is_vertex_like_shader() const
{
	auto model = get_execution_model();
	return model == ExecutionModelVertex || model == ExecutionModelGeometry ||
	       model == ExecutionModelTessellationControl || model == ExecutionModelTessellationEvaluation;
}

bool Compiler::is_tessellation_shader() const
{
	return is_tessellation_shader(get_execution_model());
}

bool Compiler::is_tessellating_triangles() const
{
	return get_execution_mode_bitset().get(ExecutionModeTriangles);
}

void Compiler::set_remapped_variable_state(VariableID id, bool remap_enable)
{
	get<SPIRVariable>(id).remapped_variable = remap_enable;
}

bool Compiler::get_remapped_variable_state(VariableID id) const
{
	return get<SPIRVariable>(id).remapped_variable;
}

void Compiler::set_subpass_input_remapped_components(VariableID id, uint32_t components)
{
	get<SPIRVariable>(id).remapped_components = components;
}

uint32_t Compiler::get_subpass_input_remapped_components(VariableID id) const
{
	return get<SPIRVariable>(id).remapped_components;
}

void Compiler::add_implied_read_expression(SPIRExpression &e, uint32_t source)
{
	auto itr = find(begin(e.implied_read_expressions), end(e.implied_read_expressions), ID(source));
	if (itr == end(e.implied_read_expressions))
		e.implied_read_expressions.push_back(source);
}

void Compiler::add_implied_read_expression(SPIRAccessChain &e, uint32_t source)
{
	auto itr = find(begin(e.implied_read_expressions), end(e.implied_read_expressions), ID(source));
	if (itr == end(e.implied_read_expressions))
		e.implied_read_expressions.push_back(source);
}

void Compiler::add_active_interface_variable(uint32_t var_id)
{
	active_interface_variables.insert(var_id);

	// In SPIR-V 1.4 and up we must also track the interface variable in the entry point.
	if (ir.get_spirv_version() >= 0x10400)
	{
		auto &vars = get_entry_point().interface_variables;
		if (find(begin(vars), end(vars), VariableID(var_id)) == end(vars))
			vars.push_back(var_id);
	}
}

void Compiler::inherit_expression_dependencies(uint32_t dst, uint32_t source_expression)
{
	auto *ptr_e = maybe_get<SPIRExpression>(dst);

	if (is_position_invariant() && ptr_e && maybe_get<SPIRExpression>(source_expression))
	{
		auto &deps = ptr_e->invariance_dependencies;
		if (std::find(deps.begin(), deps.end(), source_expression) == deps.end())
			deps.push_back(source_expression);
	}

	// Don't inherit any expression dependencies if the expression in dst
	// is not a forwarded temporary.
	if (forwarded_temporaries.find(dst) == end(forwarded_temporaries) ||
	    forced_temporaries.find(dst) != end(forced_temporaries))
	{
		return;
	}

	auto &e = *ptr_e;
	auto *phi = maybe_get<SPIRVariable>(source_expression);
	if (phi && phi->phi_variable)
	{
		// We have used a phi variable, which can change at the end of the block,
		// so make sure we take a dependency on this phi variable.
		phi->dependees.push_back(dst);
	}

	auto *s = maybe_get<SPIRExpression>(source_expression);
	if (!s)
		return;

	auto &e_deps = e.expression_dependencies;
	auto &s_deps = s->expression_dependencies;

	// If we depend on a expression, we also depend on all sub-dependencies from source.
	e_deps.push_back(source_expression);
	e_deps.insert(end(e_deps), begin(s_deps), end(s_deps));

	// Eliminate duplicated dependencies.
	sort(begin(e_deps), end(e_deps));
	e_deps.erase(unique(begin(e_deps), end(e_deps)), end(e_deps));
}

SmallVector<EntryPoint> Compiler::get_entry_points_and_stages() const
{
	SmallVector<EntryPoint> entries;
	for (auto &entry : ir.entry_points)
		entries.push_back({ entry.second.orig_name, entry.second.model });
	return entries;
}

void Compiler::rename_entry_point(const std::string &old_name, const std::string &new_name, spv::ExecutionModel model)
{
	auto &entry = get_entry_point(old_name, model);
	entry.orig_name = new_name;
	entry.name = new_name;
}

void Compiler::set_entry_point(const std::string &name, spv::ExecutionModel model)
{
	auto &entry = get_entry_point(name, model);
	ir.default_entry_point = entry.self;
}

SPIREntryPoint &Compiler::get_first_entry_point(const std::string &name)
{
	auto itr = find_if(
	    begin(ir.entry_points), end(ir.entry_points),
	    [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool { return entry.second.orig_name == name; });

	if (itr == end(ir.entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

const SPIREntryPoint &Compiler::get_first_entry_point(const std::string &name) const
{
	auto itr = find_if(
	    begin(ir.entry_points), end(ir.entry_points),
	    [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool { return entry.second.orig_name == name; });

	if (itr == end(ir.entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

SPIREntryPoint &Compiler::get_entry_point(const std::string &name, ExecutionModel model)
{
	auto itr = find_if(begin(ir.entry_points), end(ir.entry_points),
	                   [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool {
		                   return entry.second.orig_name == name && entry.second.model == model;
	                   });

	if (itr == end(ir.entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

const SPIREntryPoint &Compiler::get_entry_point(const std::string &name, ExecutionModel model) const
{
	auto itr = find_if(begin(ir.entry_points), end(ir.entry_points),
	                   [&](const std::pair<uint32_t, SPIREntryPoint> &entry) -> bool {
		                   return entry.second.orig_name == name && entry.second.model == model;
	                   });

	if (itr == end(ir.entry_points))
		SPIRV_CROSS_THROW("Entry point does not exist.");

	return itr->second;
}

const string &Compiler::get_cleansed_entry_point_name(const std::string &name, ExecutionModel model) const
{
	return get_entry_point(name, model).name;
}

const SPIREntryPoint &Compiler::get_entry_point() const
{
	return ir.entry_points.find(ir.default_entry_point)->second;
}

SPIREntryPoint &Compiler::get_entry_point()
{
	return ir.entry_points.find(ir.default_entry_point)->second;
}

bool Compiler::interface_variable_exists_in_entry_point(uint32_t id) const
{
	auto &var = get<SPIRVariable>(id);

	if (ir.get_spirv_version() < 0x10400)
	{
		if (var.storage != StorageClassInput && var.storage != StorageClassOutput &&
		    var.storage != StorageClassUniformConstant)
			SPIRV_CROSS_THROW("Only Input, Output variables and Uniform constants are part of a shader linking interface.");

		// This is to avoid potential problems with very old glslang versions which did
		// not emit input/output interfaces properly.
		// We can assume they only had a single entry point, and single entry point
		// shaders could easily be assumed to use every interface variable anyways.
		if (ir.entry_points.size() <= 1)
			return true;
	}

	// In SPIR-V 1.4 and later, all global resource variables must be present.

	auto &execution = get_entry_point();
	return find(begin(execution.interface_variables), end(execution.interface_variables), VariableID(id)) !=
	       end(execution.interface_variables);
}

void Compiler::CombinedImageSamplerHandler::push_remap_parameters(const SPIRFunction &func, const uint32_t *args,
                                                                  uint32_t length)
{
	// If possible, pipe through a remapping table so that parameters know
	// which variables they actually bind to in this scope.
	unordered_map<uint32_t, uint32_t> remapping;
	for (uint32_t i = 0; i < length; i++)
		remapping[func.arguments[i].id] = remap_parameter(args[i]);
	parameter_remapping.push(std::move(remapping));
}

void Compiler::CombinedImageSamplerHandler::pop_remap_parameters()
{
	parameter_remapping.pop();
}

uint32_t Compiler::CombinedImageSamplerHandler::remap_parameter(uint32_t id)
{
	auto *var = compiler.maybe_get_backing_variable(id);
	if (var)
		id = var->self;

	if (parameter_remapping.empty())
		return id;

	auto &remapping = parameter_remapping.top();
	auto itr = remapping.find(id);
	if (itr != end(remapping))
		return itr->second;
	else
		return id;
}

bool Compiler::CombinedImageSamplerHandler::begin_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;

	auto &callee = compiler.get<SPIRFunction>(args[2]);
	args += 3;
	length -= 3;
	push_remap_parameters(callee, args, length);
	functions.push(&callee);
	return true;
}

bool Compiler::CombinedImageSamplerHandler::end_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;

	auto &callee = compiler.get<SPIRFunction>(args[2]);
	args += 3;

	// There are two types of cases we have to handle,
	// a callee might call sampler2D(texture2D, sampler) directly where
	// one or more parameters originate from parameters.
	// Alternatively, we need to provide combined image samplers to our callees,
	// and in this case we need to add those as well.

	pop_remap_parameters();

	// Our callee has now been processed at least once.
	// No point in doing it again.
	callee.do_combined_parameters = false;

	auto &params = functions.top()->combined_parameters;
	functions.pop();
	if (functions.empty())
		return true;

	auto &caller = *functions.top();
	if (caller.do_combined_parameters)
	{
		for (auto &param : params)
		{
			VariableID image_id = param.global_image ? param.image_id : VariableID(args[param.image_id]);
			VariableID sampler_id = param.global_sampler ? param.sampler_id : VariableID(args[param.sampler_id]);

			auto *i = compiler.maybe_get_backing_variable(image_id);
			auto *s = compiler.maybe_get_backing_variable(sampler_id);
			if (i)
				image_id = i->self;
			if (s)
				sampler_id = s->self;

			register_combined_image_sampler(caller, 0, image_id, sampler_id, param.depth);
		}
	}

	return true;
}

void Compiler::CombinedImageSamplerHandler::register_combined_image_sampler(SPIRFunction &caller,
                                                                            VariableID combined_module_id,
                                                                            VariableID image_id, VariableID sampler_id,
                                                                            bool depth)
{
	// We now have a texture ID and a sampler ID which will either be found as a global
	// or a parameter in our own function. If both are global, they will not need a parameter,
	// otherwise, add it to our list.
	SPIRFunction::CombinedImageSamplerParameter param = {
		0u, image_id, sampler_id, true, true, depth,
	};

	auto texture_itr = find_if(begin(caller.arguments), end(caller.arguments),
	                           [image_id](const SPIRFunction::Parameter &p) { return p.id == image_id; });
	auto sampler_itr = find_if(begin(caller.arguments), end(caller.arguments),
	                           [sampler_id](const SPIRFunction::Parameter &p) { return p.id == sampler_id; });

	if (texture_itr != end(caller.arguments))
	{
		param.global_image = false;
		param.image_id = uint32_t(texture_itr - begin(caller.arguments));
	}

	if (sampler_itr != end(caller.arguments))
	{
		param.global_sampler = false;
		param.sampler_id = uint32_t(sampler_itr - begin(caller.arguments));
	}

	if (param.global_image && param.global_sampler)
		return;

	auto itr = find_if(begin(caller.combined_parameters), end(caller.combined_parameters),
	                   [&param](const SPIRFunction::CombinedImageSamplerParameter &p) {
		                   return param.image_id == p.image_id && param.sampler_id == p.sampler_id &&
		                          param.global_image == p.global_image && param.global_sampler == p.global_sampler;
	                   });

	if (itr == end(caller.combined_parameters))
	{
		uint32_t id = compiler.ir.increase_bound_by(3);
		auto type_id = id + 0;
		auto ptr_type_id = id + 1;
		auto combined_id = id + 2;
		auto &base = compiler.expression_type(image_id);
		auto &type = compiler.set<SPIRType>(type_id, OpTypeSampledImage);
		auto &ptr_type = compiler.set<SPIRType>(ptr_type_id, OpTypePointer);

		type = base;
		type.self = type_id;
		type.basetype = SPIRType::SampledImage;
		type.pointer = false;
		type.storage = StorageClassGeneric;
		type.image.depth = depth;

		ptr_type = type;
		ptr_type.pointer = true;
		ptr_type.storage = StorageClassUniformConstant;
		ptr_type.parent_type = type_id;

		// Build new variable.
		compiler.set<SPIRVariable>(combined_id, ptr_type_id, StorageClassFunction, 0);

		// Inherit RelaxedPrecision.
		// If any of OpSampledImage, underlying image or sampler are marked, inherit the decoration.
		bool relaxed_precision =
		    compiler.has_decoration(sampler_id, DecorationRelaxedPrecision) ||
		    compiler.has_decoration(image_id, DecorationRelaxedPrecision) ||
		    (combined_module_id && compiler.has_decoration(combined_module_id, DecorationRelaxedPrecision));

		if (relaxed_precision)
			compiler.set_decoration(combined_id, DecorationRelaxedPrecision);

		param.id = combined_id;

		compiler.set_name(combined_id,
		                  join("SPIRV_Cross_Combined", compiler.to_name(image_id), compiler.to_name(sampler_id)));

		caller.combined_parameters.push_back(param);
		caller.shadow_arguments.push_back({ ptr_type_id, combined_id, 0u, 0u, true });
	}
}

bool Compiler::DummySamplerForCombinedImageHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	if (need_dummy_sampler)
	{
		// No need to traverse further, we know the result.
		return false;
	}

	switch (opcode)
	{
	case OpLoad:
	{
		if (length < 3)
			return false;

		uint32_t result_type = args[0];

		auto &type = compiler.get<SPIRType>(result_type);
		bool separate_image =
		    type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer;

		// If not separate image, don't bother.
		if (!separate_image)
			return true;

		uint32_t id = args[1];
		uint32_t ptr = args[2];
		compiler.set<SPIRExpression>(id, "", result_type, true);
		compiler.register_read(id, ptr, true);
		break;
	}

	case OpImageFetch:
	case OpImageQuerySizeLod:
	case OpImageQuerySize:
	case OpImageQueryLevels:
	case OpImageQuerySamples:
	{
		// If we are fetching or querying LOD from a plain OpTypeImage, we must pre-combine with our dummy sampler.
		auto *var = compiler.maybe_get_backing_variable(args[2]);
		if (var)
		{
			auto &type = compiler.get<SPIRType>(var->basetype);
			if (type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer)
				need_dummy_sampler = true;
		}

		break;
	}

	case OpInBoundsAccessChain:
	case OpAccessChain:
	case OpPtrAccessChain:
	{
		if (length < 3)
			return false;

		uint32_t result_type = args[0];
		auto &type = compiler.get<SPIRType>(result_type);
		bool separate_image =
		    type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer;
		if (!separate_image)
			return true;

		uint32_t id = args[1];
		uint32_t ptr = args[2];
		compiler.set<SPIRExpression>(id, "", result_type, true);
		compiler.register_read(id, ptr, true);

		// Other backends might use SPIRAccessChain for this later.
		compiler.ir.ids[id].set_allow_type_rewrite();
		break;
	}

	default:
		break;
	}

	return true;
}

bool Compiler::CombinedImageSamplerHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	// We need to figure out where samplers and images are loaded from, so do only the bare bones compilation we need.
	bool is_fetch = false;

	switch (opcode)
	{
	case OpLoad:
	{
		if (length < 3)
			return false;

		uint32_t result_type = args[0];

		auto &type = compiler.get<SPIRType>(result_type);
		bool separate_image = type.basetype == SPIRType::Image && type.image.sampled == 1;
		bool separate_sampler = type.basetype == SPIRType::Sampler;

		// If not separate image or sampler, don't bother.
		if (!separate_image && !separate_sampler)
			return true;

		uint32_t id = args[1];
		uint32_t ptr = args[2];
		compiler.set<SPIRExpression>(id, "", result_type, true);
		compiler.register_read(id, ptr, true);
		return true;
	}

	case OpInBoundsAccessChain:
	case OpAccessChain:
	case OpPtrAccessChain:
	{
		if (length < 3)
			return false;

		// Technically, it is possible to have arrays of textures and arrays of samplers and combine them, but this becomes essentially
		// impossible to implement, since we don't know which concrete sampler we are accessing.
		// One potential way is to create a combinatorial explosion where N textures and M samplers are combined into N * M sampler2Ds,
		// but this seems ridiculously complicated for a problem which is easy to work around.
		// Checking access chains like this assumes we don't have samplers or textures inside uniform structs, but this makes no sense.

		uint32_t result_type = args[0];

		auto &type = compiler.get<SPIRType>(result_type);
		bool separate_image = type.basetype == SPIRType::Image && type.image.sampled == 1;
		bool separate_sampler = type.basetype == SPIRType::Sampler;
		if (separate_sampler)
			SPIRV_CROSS_THROW(
			    "Attempting to use arrays or structs of separate samplers. This is not possible to statically "
			    "remap to plain GLSL.");

		if (separate_image)
		{
			uint32_t id = args[1];
			uint32_t ptr = args[2];
			compiler.set<SPIRExpression>(id, "", result_type, true);
			compiler.register_read(id, ptr, true);
		}
		return true;
	}

	case OpImageFetch:
	case OpImageQuerySizeLod:
	case OpImageQuerySize:
	case OpImageQueryLevels:
	case OpImageQuerySamples:
	{
		// If we are fetching from a plain OpTypeImage or querying LOD, we must pre-combine with our dummy sampler.
		auto *var = compiler.maybe_get_backing_variable(args[2]);
		if (!var)
			return true;

		auto &type = compiler.get<SPIRType>(var->basetype);
		if (type.basetype == SPIRType::Image && type.image.sampled == 1 && type.image.dim != DimBuffer)
		{
			if (compiler.dummy_sampler_id == 0)
				SPIRV_CROSS_THROW("texelFetch without sampler was found, but no dummy sampler has been created with "
				                  "build_dummy_sampler_for_combined_images().");

			// Do it outside.
			is_fetch = true;
			break;
		}

		return true;
	}

	case OpSampledImage:
		// Do it outside.
		break;

	default:
		return true;
	}

	// Registers sampler2D calls used in case they are parameters so
	// that their callees know which combined image samplers to propagate down the call stack.
	if (!functions.empty())
	{
		auto &callee = *functions.top();
		if (callee.do_combined_parameters)
		{
			uint32_t image_id = args[2];

			auto *image = compiler.maybe_get_backing_variable(image_id);
			if (image)
				image_id = image->self;

			uint32_t sampler_id = is_fetch ? compiler.dummy_sampler_id : args[3];
			auto *sampler = compiler.maybe_get_backing_variable(sampler_id);
			if (sampler)
				sampler_id = sampler->self;

			uint32_t combined_id = args[1];

			auto &combined_type = compiler.get<SPIRType>(args[0]);
			register_combined_image_sampler(callee, combined_id, image_id, sampler_id, combined_type.image.depth);
		}
	}

	// For function calls, we need to remap IDs which are function parameters into global variables.
	// This information is statically known from the current place in the call stack.
	// Function parameters are not necessarily pointers, so if we don't have a backing variable, remapping will know
	// which backing variable the image/sample came from.
	VariableID image_id = remap_parameter(args[2]);
	VariableID sampler_id = is_fetch ? compiler.dummy_sampler_id : remap_parameter(args[3]);

	auto itr = find_if(begin(compiler.combined_image_samplers), end(compiler.combined_image_samplers),
	                   [image_id, sampler_id](const CombinedImageSampler &combined) {
		                   return combined.image_id == image_id && combined.sampler_id == sampler_id;
	                   });

	if (itr == end(compiler.combined_image_samplers))
	{
		uint32_t sampled_type;
		uint32_t combined_module_id;
		if (is_fetch)
		{
			// Have to invent the sampled image type.
			sampled_type = compiler.ir.increase_bound_by(1);
			auto &type = compiler.set<SPIRType>(sampled_type, OpTypeSampledImage);
			type = compiler.expression_type(args[2]);
			type.self = sampled_type;
			type.basetype = SPIRType::SampledImage;
			type.image.depth = false;
			combined_module_id = 0;
		}
		else
		{
			sampled_type = args[0];
			combined_module_id = args[1];
		}

		auto id = compiler.ir.increase_bound_by(2);
		auto type_id = id + 0;
		auto combined_id = id + 1;

		// Make a new type, pointer to OpTypeSampledImage, so we can make a variable of this type.
		// We will probably have this type lying around, but it doesn't hurt to make duplicates for internal purposes.
		auto &type = compiler.set<SPIRType>(type_id, OpTypePointer);
		auto &base = compiler.get<SPIRType>(sampled_type);
		type = base;
		type.pointer = true;
		type.storage = StorageClassUniformConstant;
		type.parent_type = type_id;

		// Build new variable.
		compiler.set<SPIRVariable>(combined_id, type_id, StorageClassUniformConstant, 0);

		// Inherit RelaxedPrecision (and potentially other useful flags if deemed relevant).
		// If any of OpSampledImage, underlying image or sampler are marked, inherit the decoration.
		bool relaxed_precision =
		    (sampler_id && compiler.has_decoration(sampler_id, DecorationRelaxedPrecision)) ||
		    (image_id && compiler.has_decoration(image_id, DecorationRelaxedPrecision)) ||
		    (combined_module_id && compiler.has_decoration(combined_module_id, DecorationRelaxedPrecision));

		if (relaxed_precision)
			compiler.set_decoration(combined_id, DecorationRelaxedPrecision);

		// Propagate the array type for the original image as well.
		auto *var = compiler.maybe_get_backing_variable(image_id);
		if (var)
		{
			auto &parent_type = compiler.get<SPIRType>(var->basetype);
			type.array = parent_type.array;
			type.array_size_literal = parent_type.array_size_literal;
		}

		compiler.combined_image_samplers.push_back({ combined_id, image_id, sampler_id });
	}

	return true;
}

VariableID Compiler::build_dummy_sampler_for_combined_images()
{
	DummySamplerForCombinedImageHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);
	if (handler.need_dummy_sampler)
	{
		uint32_t offset = ir.increase_bound_by(3);
		auto type_id = offset + 0;
		auto ptr_type_id = offset + 1;
		auto var_id = offset + 2;

		auto &sampler = set<SPIRType>(type_id, OpTypeSampler);
		sampler.basetype = SPIRType::Sampler;

		auto &ptr_sampler = set<SPIRType>(ptr_type_id, OpTypePointer);
		ptr_sampler = sampler;
		ptr_sampler.self = type_id;
		ptr_sampler.storage = StorageClassUniformConstant;
		ptr_sampler.pointer = true;
		ptr_sampler.parent_type = type_id;

		set<SPIRVariable>(var_id, ptr_type_id, StorageClassUniformConstant, 0);
		set_name(var_id, "SPIRV_Cross_DummySampler");
		dummy_sampler_id = var_id;
		return var_id;
	}
	else
		return 0;
}

void Compiler::build_combined_image_samplers()
{
	ir.for_each_typed_id<SPIRFunction>([&](uint32_t, SPIRFunction &func) {
		func.combined_parameters.clear();
		func.shadow_arguments.clear();
		func.do_combined_parameters = true;
	});

	combined_image_samplers.clear();
	CombinedImageSamplerHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);
}

SmallVector<SpecializationConstant> Compiler::get_specialization_constants() const
{
	SmallVector<SpecializationConstant> spec_consts;
	ir.for_each_typed_id<SPIRConstant>([&](uint32_t, const SPIRConstant &c) {
		if (c.specialization && has_decoration(c.self, DecorationSpecId))
			spec_consts.push_back({ c.self, get_decoration(c.self, DecorationSpecId) });
	});
	return spec_consts;
}

SPIRConstant &Compiler::get_constant(ConstantID id)
{
	return get<SPIRConstant>(id);
}

const SPIRConstant &Compiler::get_constant(ConstantID id) const
{
	return get<SPIRConstant>(id);
}

static bool exists_unaccessed_path_to_return(const CFG &cfg, uint32_t block, const unordered_set<uint32_t> &blocks,
                                             unordered_set<uint32_t> &visit_cache)
{
	// This block accesses the variable.
	if (blocks.find(block) != end(blocks))
		return false;

	// We are at the end of the CFG.
	if (cfg.get_succeeding_edges(block).empty())
		return true;

	// If any of our successors have a path to the end, there exists a path from block.
	for (auto &succ : cfg.get_succeeding_edges(block))
	{
		if (visit_cache.count(succ) == 0)
		{
			if (exists_unaccessed_path_to_return(cfg, succ, blocks, visit_cache))
				return true;
			visit_cache.insert(succ);
		}
	}

	return false;
}

void Compiler::analyze_parameter_preservation(
    SPIRFunction &entry, const CFG &cfg, const unordered_map<uint32_t, unordered_set<uint32_t>> &variable_to_blocks,
    const unordered_map<uint32_t, unordered_set<uint32_t>> &complete_write_blocks)
{
	for (auto &arg : entry.arguments)
	{
		// Non-pointers are always inputs.
		auto &type = get<SPIRType>(arg.type);
		if (!type.pointer)
			continue;

		// Opaque argument types are always in
		bool potential_preserve;
		switch (type.basetype)
		{
		case SPIRType::Sampler:
		case SPIRType::Image:
		case SPIRType::SampledImage:
		case SPIRType::AtomicCounter:
			potential_preserve = false;
			break;

		default:
			potential_preserve = true;
			break;
		}

		if (!potential_preserve)
			continue;

		auto itr = variable_to_blocks.find(arg.id);
		if (itr == end(variable_to_blocks))
		{
			// Variable is never accessed.
			continue;
		}

		// We have accessed a variable, but there was no complete writes to that variable.
		// We deduce that we must preserve the argument.
		itr = complete_write_blocks.find(arg.id);
		if (itr == end(complete_write_blocks))
		{
			arg.read_count++;
			continue;
		}

		// If there is a path through the CFG where no block completely writes to the variable, the variable will be in an undefined state
		// when the function returns. We therefore need to implicitly preserve the variable in case there are writers in the function.
		// Major case here is if a function is
		// void foo(int &var) { if (cond) var = 10; }
		// Using read/write counts, we will think it's just an out variable, but it really needs to be inout,
		// because if we don't write anything whatever we put into the function must return back to the caller.
		unordered_set<uint32_t> visit_cache;
		if (exists_unaccessed_path_to_return(cfg, entry.entry_block, itr->second, visit_cache))
			arg.read_count++;
	}
}

Compiler::AnalyzeVariableScopeAccessHandler::AnalyzeVariableScopeAccessHandler(Compiler &compiler_,
                                                                               SPIRFunction &entry_)
    : compiler(compiler_)
    , entry(entry_)
{
}

bool Compiler::AnalyzeVariableScopeAccessHandler::follow_function_call(const SPIRFunction &)
{
	// Only analyze within this function.
	return false;
}

void Compiler::AnalyzeVariableScopeAccessHandler::set_current_block(const SPIRBlock &block)
{
	current_block = &block;

	// If we're branching to a block which uses OpPhi, in GLSL
	// this will be a variable write when we branch,
	// so we need to track access to these variables as well to
	// have a complete picture.
	const auto test_phi = [this, &block](uint32_t to) {
		auto &next = compiler.get<SPIRBlock>(to);
		for (auto &phi : next.phi_variables)
		{
			if (phi.parent == block.self)
			{
				accessed_variables_to_block[phi.function_variable].insert(block.self);
				// Phi variables are also accessed in our target branch block.
				accessed_variables_to_block[phi.function_variable].insert(next.self);

				notify_variable_access(phi.local_variable, block.self);
			}
		}
	};

	switch (block.terminator)
	{
	case SPIRBlock::Direct:
		notify_variable_access(block.condition, block.self);
		test_phi(block.next_block);
		break;

	case SPIRBlock::Select:
		notify_variable_access(block.condition, block.self);
		test_phi(block.true_block);
		test_phi(block.false_block);
		break;

	case SPIRBlock::MultiSelect:
	{
		notify_variable_access(block.condition, block.self);
		auto &cases = compiler.get_case_list(block);
		for (auto &target : cases)
			test_phi(target.block);
		if (block.default_block)
			test_phi(block.default_block);
		break;
	}

	default:
		break;
	}
}

void Compiler::AnalyzeVariableScopeAccessHandler::notify_variable_access(uint32_t id, uint32_t block)
{
	if (id == 0)
		return;

	// Access chains used in multiple blocks mean hoisting all the variables used to construct the access chain as not all backends can use pointers.
	auto itr = rvalue_forward_children.find(id);
	if (itr != end(rvalue_forward_children))
		for (auto child_id : itr->second)
			notify_variable_access(child_id, block);

	if (id_is_phi_variable(id))
		accessed_variables_to_block[id].insert(block);
	else if (id_is_potential_temporary(id))
		accessed_temporaries_to_block[id].insert(block);
}

bool Compiler::AnalyzeVariableScopeAccessHandler::id_is_phi_variable(uint32_t id) const
{
	if (id >= compiler.get_current_id_bound())
		return false;
	auto *var = compiler.maybe_get<SPIRVariable>(id);
	return var && var->phi_variable;
}

bool Compiler::AnalyzeVariableScopeAccessHandler::id_is_potential_temporary(uint32_t id) const
{
	if (id >= compiler.get_current_id_bound())
		return false;

	// Temporaries are not created before we start emitting code.
	return compiler.ir.ids[id].empty() || (compiler.ir.ids[id].get_type() == TypeExpression);
}

bool Compiler::AnalyzeVariableScopeAccessHandler::handle_terminator(const SPIRBlock &block)
{
	switch (block.terminator)
	{
	case SPIRBlock::Return:
		if (block.return_value)
			notify_variable_access(block.return_value, block.self);
		break;

	case SPIRBlock::Select:
	case SPIRBlock::MultiSelect:
		notify_variable_access(block.condition, block.self);
		break;

	default:
		break;
	}

	return true;
}

bool Compiler::AnalyzeVariableScopeAccessHandler::handle(spv::Op op, const uint32_t *args, uint32_t length)
{
	// Keep track of the types of temporaries, so we can hoist them out as necessary.
	uint32_t result_type = 0, result_id = 0;
	if (compiler.instruction_to_result_type(result_type, result_id, op, args, length))
	{
		// For some opcodes, we will need to override the result id.
		// If we need to hoist the temporary, the temporary type is the input, not the result.
		if (op == OpConvertUToAccelerationStructureKHR)
		{
			auto itr = result_id_to_type.find(args[2]);
			if (itr != result_id_to_type.end())
				result_type = itr->second;
		}

		result_id_to_type[result_id] = result_type;
	}

	switch (op)
	{
	case OpStore:
	{
		if (length < 2)
			return false;

		ID ptr = args[0];
		auto *var = compiler.maybe_get_backing_variable(ptr);

		// If we store through an access chain, we have a partial write.
		if (var)
		{
			accessed_variables_to_block[var->self].insert(current_block->self);
			if (var->self == ptr)
				complete_write_variables_to_block[var->self].insert(current_block->self);
			else
				partial_write_variables_to_block[var->self].insert(current_block->self);
		}

		// args[0] might be an access chain we have to track use of.
		notify_variable_access(args[0], current_block->self);
		// Might try to store a Phi variable here.
		notify_variable_access(args[1], current_block->self);
		break;
	}

	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpPtrAccessChain:
	{
		if (length < 3)
			return false;

		// Access chains used in multiple blocks mean hoisting all the variables used to construct the access chain as not all backends can use pointers.
		uint32_t ptr = args[2];
		auto *var = compiler.maybe_get<SPIRVariable>(ptr);
		if (var)
		{
			accessed_variables_to_block[var->self].insert(current_block->self);
			rvalue_forward_children[args[1]].insert(var->self);
		}

		// args[2] might be another access chain we have to track use of.
		for (uint32_t i = 2; i < length; i++)
		{
			notify_variable_access(args[i], current_block->self);
			rvalue_forward_children[args[1]].insert(args[i]);
		}

		// Also keep track of the access chain pointer itself.
		// In exceptionally rare cases, we can end up with a case where
		// the access chain is generated in the loop body, but is consumed in continue block.
		// This means we need complex loop workarounds, and we must detect this via CFG analysis.
		notify_variable_access(args[1], current_block->self);

		// The result of an access chain is a fixed expression and is not really considered a temporary.
		auto &e = compiler.set<SPIRExpression>(args[1], "", args[0], true);
		auto *backing_variable = compiler.maybe_get_backing_variable(ptr);
		e.loaded_from = backing_variable ? VariableID(backing_variable->self) : VariableID(0);

		// Other backends might use SPIRAccessChain for this later.
		compiler.ir.ids[args[1]].set_allow_type_rewrite();
		access_chain_expressions.insert(args[1]);
		break;
	}

	case OpCopyMemory:
	{
		if (length < 2)
			return false;

		ID lhs = args[0];
		ID rhs = args[1];
		auto *var = compiler.maybe_get_backing_variable(lhs);

		// If we store through an access chain, we have a partial write.
		if (var)
		{
			accessed_variables_to_block[var->self].insert(current_block->self);
			if (var->self == lhs)
				complete_write_variables_to_block[var->self].insert(current_block->self);
			else
				partial_write_variables_to_block[var->self].insert(current_block->self);
		}

		// args[0:1] might be access chains we have to track use of.
		for (uint32_t i = 0; i < 2; i++)
			notify_variable_access(args[i], current_block->self);

		var = compiler.maybe_get_backing_variable(rhs);
		if (var)
			accessed_variables_to_block[var->self].insert(current_block->self);
		break;
	}

	case OpCopyObject:
	{
		// OpCopyObject copies the underlying non-pointer type, 
		// so any temp variable should be declared using the underlying type.
		// If the type is a pointer, get its base type and overwrite the result type mapping.
		auto &type = compiler.get<SPIRType>(result_type);
		if (type.pointer)
			result_id_to_type[result_id] = type.parent_type;

		if (length < 3)
			return false;

		auto *var = compiler.maybe_get_backing_variable(args[2]);
		if (var)
			accessed_variables_to_block[var->self].insert(current_block->self);

		// Might be an access chain which we have to keep track of.
		notify_variable_access(args[1], current_block->self);
		if (access_chain_expressions.count(args[2]))
			access_chain_expressions.insert(args[1]);

		// Might try to copy a Phi variable here.
		notify_variable_access(args[2], current_block->self);
		break;
	}

	case OpLoad:
	{
		if (length < 3)
			return false;
		uint32_t ptr = args[2];
		auto *var = compiler.maybe_get_backing_variable(ptr);
		if (var)
			accessed_variables_to_block[var->self].insert(current_block->self);

		// Loaded value is a temporary.
		notify_variable_access(args[1], current_block->self);

		// Might be an access chain we have to track use of.
		notify_variable_access(args[2], current_block->self);

		// If we're loading an opaque type we cannot lower it to a temporary,
		// we must defer access of args[2] until it's used.
		auto &type = compiler.get<SPIRType>(args[0]);
		if (compiler.type_is_opaque_value(type))
			rvalue_forward_children[args[1]].insert(args[2]);
		break;
	}

	case OpFunctionCall:
	{
		if (length < 3)
			return false;

		// Return value may be a temporary.
		if (compiler.get_type(args[0]).basetype != SPIRType::Void)
			notify_variable_access(args[1], current_block->self);

		length -= 3;
		args += 3;

		for (uint32_t i = 0; i < length; i++)
		{
			auto *var = compiler.maybe_get_backing_variable(args[i]);
			if (var)
			{
				accessed_variables_to_block[var->self].insert(current_block->self);
				// Assume we can get partial writes to this variable.
				partial_write_variables_to_block[var->self].insert(current_block->self);
			}

			// Cannot easily prove if argument we pass to a function is completely written.
			// Usually, functions write to a dummy variable,
			// which is then copied to in full to the real argument.

			// Might try to copy a Phi variable here.
			notify_variable_access(args[i], current_block->self);
		}
		break;
	}

	case OpSelect:
	{
		// In case of variable pointers, we might access a variable here.
		// We cannot prove anything about these accesses however.
		for (uint32_t i = 1; i < length; i++)
		{
			if (i >= 3)
			{
				auto *var = compiler.maybe_get_backing_variable(args[i]);
				if (var)
				{
					accessed_variables_to_block[var->self].insert(current_block->self);
					// Assume we can get partial writes to this variable.
					partial_write_variables_to_block[var->self].insert(current_block->self);
				}
			}

			// Might try to copy a Phi variable here.
			notify_variable_access(args[i], current_block->self);
		}
		break;
	}

	case OpExtInst:
	{
		for (uint32_t i = 4; i < length; i++)
			notify_variable_access(args[i], current_block->self);
		notify_variable_access(args[1], current_block->self);

		uint32_t extension_set = args[2];
		if (compiler.get<SPIRExtension>(extension_set).ext == SPIRExtension::GLSL)
		{
			auto op_450 = static_cast<GLSLstd450>(args[3]);
			switch (op_450)
			{
			case GLSLstd450Modf:
			case GLSLstd450Frexp:
			{
				uint32_t ptr = args[5];
				auto *var = compiler.maybe_get_backing_variable(ptr);
				if (var)
				{
					accessed_variables_to_block[var->self].insert(current_block->self);
					if (var->self == ptr)
						complete_write_variables_to_block[var->self].insert(current_block->self);
					else
						partial_write_variables_to_block[var->self].insert(current_block->self);
				}
				break;
			}

			default:
				break;
			}
		}
		break;
	}

	case OpArrayLength:
		// Only result is a temporary.
		notify_variable_access(args[1], current_block->self);
		break;

	case OpLine:
	case OpNoLine:
		// Uses literals, but cannot be a phi variable or temporary, so ignore.
		break;

		// Atomics shouldn't be able to access function-local variables.
		// Some GLSL builtins access a pointer.

	case OpCompositeInsert:
	case OpVectorShuffle:
		// Specialize for opcode which contains literals.
		for (uint32_t i = 1; i < 4; i++)
			notify_variable_access(args[i], current_block->self);
		break;

	case OpCompositeExtract:
		// Specialize for opcode which contains literals.
		for (uint32_t i = 1; i < 3; i++)
			notify_variable_access(args[i], current_block->self);
		break;

	case OpImageWrite:
		for (uint32_t i = 0; i < length; i++)
		{
			// Argument 3 is a literal.
			if (i != 3)
				notify_variable_access(args[i], current_block->self);
		}
		break;

	case OpImageSampleImplicitLod:
	case OpImageSampleExplicitLod:
	case OpImageSparseSampleImplicitLod:
	case OpImageSparseSampleExplicitLod:
	case OpImageSampleProjImplicitLod:
	case OpImageSampleProjExplicitLod:
	case OpImageSparseSampleProjImplicitLod:
	case OpImageSparseSampleProjExplicitLod:
	case OpImageFetch:
	case OpImageSparseFetch:
	case OpImageRead:
	case OpImageSparseRead:
		for (uint32_t i = 1; i < length; i++)
		{
			// Argument 4 is a literal.
			if (i != 4)
				notify_variable_access(args[i], current_block->self);
		}
		break;

	case OpImageSampleDrefImplicitLod:
	case OpImageSampleDrefExplicitLod:
	case OpImageSparseSampleDrefImplicitLod:
	case OpImageSparseSampleDrefExplicitLod:
	case OpImageSampleProjDrefImplicitLod:
	case OpImageSampleProjDrefExplicitLod:
	case OpImageSparseSampleProjDrefImplicitLod:
	case OpImageSparseSampleProjDrefExplicitLod:
	case OpImageGather:
	case OpImageSparseGather:
	case OpImageDrefGather:
	case OpImageSparseDrefGather:
		for (uint32_t i = 1; i < length; i++)
		{
			// Argument 5 is a literal.
			if (i != 5)
				notify_variable_access(args[i], current_block->self);
		}
		break;

	default:
	{
		// Rather dirty way of figuring out where Phi variables are used.
		// As long as only IDs are used, we can scan through instructions and try to find any evidence that
		// the ID of a variable has been used.
		// There are potential false positives here where a literal is used in-place of an ID,
		// but worst case, it does not affect the correctness of the compile.
		// Exhaustive analysis would be better here, but it's not worth it for now.
		for (uint32_t i = 0; i < length; i++)
			notify_variable_access(args[i], current_block->self);
		break;
	}
	}
	return true;
}

Compiler::StaticExpressionAccessHandler::StaticExpressionAccessHandler(Compiler &compiler_, uint32_t variable_id_)
    : compiler(compiler_)
    , variable_id(variable_id_)
{
}

bool Compiler::StaticExpressionAccessHandler::follow_function_call(const SPIRFunction &)
{
	return false;
}

bool Compiler::StaticExpressionAccessHandler::handle(spv::Op op, const uint32_t *args, uint32_t length)
{
	switch (op)
	{
	case OpStore:
		if (length < 2)
			return false;
		if (args[0] == variable_id)
		{
			static_expression = args[1];
			write_count++;
		}
		break;

	case OpLoad:
		if (length < 3)
			return false;
		if (args[2] == variable_id && static_expression == 0) // Tried to read from variable before it was initialized.
			return false;
		break;

	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpPtrAccessChain:
		if (length < 3)
			return false;
		if (args[2] == variable_id) // If we try to access chain our candidate variable before we store to it, bail.
			return false;
		break;

	default:
		break;
	}

	return true;
}

void Compiler::find_function_local_luts(SPIRFunction &entry, const AnalyzeVariableScopeAccessHandler &handler,
                                        bool single_function)
{
	auto &cfg = *function_cfgs.find(entry.self)->second;

	// For each variable which is statically accessed.
	for (auto &accessed_var : handler.accessed_variables_to_block)
	{
		auto &blocks = accessed_var.second;
		auto &var = get<SPIRVariable>(accessed_var.first);
		auto &type = expression_type(accessed_var.first);

		// First check if there are writes to the variable. Later, if there are none, we'll
		// reconsider it as globally accessed LUT.
		if (!var.is_written_to)
		{
			var.is_written_to = handler.complete_write_variables_to_block.count(var.self) != 0 ||
			                    handler.partial_write_variables_to_block.count(var.self) != 0;
		}

		// Only consider function local variables here.
		// If we only have a single function in our CFG, private storage is also fine,
		// since it behaves like a function local variable.
		bool allow_lut = var.storage == StorageClassFunction || (single_function && var.storage == StorageClassPrivate);
		if (!allow_lut)
			continue;

		// We cannot be a phi variable.
		if (var.phi_variable)
			continue;

		// Only consider arrays here.
		if (type.array.empty())
			continue;

		// If the variable has an initializer, make sure it is a constant expression.
		uint32_t static_constant_expression = 0;
		if (var.initializer)
		{
			if (ir.ids[var.initializer].get_type() != TypeConstant)
				continue;
			static_constant_expression = var.initializer;

			// There can be no stores to this variable, we have now proved we have a LUT.
			if (var.is_written_to)
				continue;
		}
		else
		{
			// We can have one, and only one write to the variable, and that write needs to be a constant.

			// No partial writes allowed.
			if (handler.partial_write_variables_to_block.count(var.self) != 0)
				continue;

			auto itr = handler.complete_write_variables_to_block.find(var.self);

			// No writes?
			if (itr == end(handler.complete_write_variables_to_block))
				continue;

			// We write to the variable in more than one block.
			auto &write_blocks = itr->second;
			if (write_blocks.size() != 1)
				continue;

			// The write needs to happen in the dominating block.
			DominatorBuilder builder(cfg);
			for (auto &block : blocks)
				builder.add_block(block);
			uint32_t dominator = builder.get_dominator();

			// The complete write happened in a branch or similar, cannot deduce static expression.
			if (write_blocks.count(dominator) == 0)
				continue;

			// Find the static expression for this variable.
			StaticExpressionAccessHandler static_expression_handler(*this, var.self);
			traverse_all_reachable_opcodes(get<SPIRBlock>(dominator), static_expression_handler);

			// We want one, and exactly one write
			if (static_expression_handler.write_count != 1 || static_expression_handler.static_expression == 0)
				continue;

			// Is it a constant expression?
			if (ir.ids[static_expression_handler.static_expression].get_type() != TypeConstant)
				continue;

			// We found a LUT!
			static_constant_expression = static_expression_handler.static_expression;
		}

		get<SPIRConstant>(static_constant_expression).is_used_as_lut = true;
		var.static_expression = static_constant_expression;
		var.statically_assigned = true;
		var.remapped_variable = true;
	}
}

void Compiler::analyze_variable_scope(SPIRFunction &entry, AnalyzeVariableScopeAccessHandler &handler)
{
	// First, we map out all variable access within a function.
	// Essentially a map of block -> { variables accessed in the basic block }
	traverse_all_reachable_opcodes(entry, handler);

	auto &cfg = *function_cfgs.find(entry.self)->second;

	// Analyze if there are parameters which need to be implicitly preserved with an "in" qualifier.
	analyze_parameter_preservation(entry, cfg, handler.accessed_variables_to_block,
	                               handler.complete_write_variables_to_block);

	unordered_map<uint32_t, uint32_t> potential_loop_variables;

	// Find the loop dominator block for each block.
	for (auto &block_id : entry.blocks)
	{
		auto &block = get<SPIRBlock>(block_id);

		auto itr = ir.continue_block_to_loop_header.find(block_id);
		if (itr != end(ir.continue_block_to_loop_header) && itr->second != block_id)
		{
			// Continue block might be unreachable in the CFG, but we still like to know the loop dominator.
			// Edge case is when continue block is also the loop header, don't set the dominator in this case.
			block.loop_dominator = itr->second;
		}
		else
		{
			uint32_t loop_dominator = cfg.find_loop_dominator(block_id);
			if (loop_dominator != block_id)
				block.loop_dominator = loop_dominator;
			else
				block.loop_dominator = SPIRBlock::NoDominator;
		}
	}

	// For each variable which is statically accessed.
	for (auto &var : handler.accessed_variables_to_block)
	{
		// Only deal with variables which are considered local variables in this function.
		if (find(begin(entry.local_variables), end(entry.local_variables), VariableID(var.first)) ==
		    end(entry.local_variables))
			continue;

		DominatorBuilder builder(cfg);
		auto &blocks = var.second;
		auto &type = expression_type(var.first);
		BlockID potential_continue_block = 0;

		// Figure out which block is dominating all accesses of those variables.
		for (auto &block : blocks)
		{
			// If we're accessing a variable inside a continue block, this variable might be a loop variable.
			// We can only use loop variables with scalars, as we cannot track static expressions for vectors.
			if (is_continue(block))
			{
				// Potentially awkward case to check for.
				// We might have a variable inside a loop, which is touched by the continue block,
				// but is not actually a loop variable.
				// The continue block is dominated by the inner part of the loop, which does not make sense in high-level
				// language output because it will be declared before the body,
				// so we will have to lift the dominator up to the relevant loop header instead.
				builder.add_block(ir.continue_block_to_loop_header[block]);

				// Arrays or structs cannot be loop variables.
				if (type.vecsize == 1 && type.columns == 1 && type.basetype != SPIRType::Struct && type.array.empty())
				{
					// The variable is used in multiple continue blocks, this is not a loop
					// candidate, signal that by setting block to -1u.
					if (potential_continue_block == 0)
						potential_continue_block = block;
					else
						potential_continue_block = ~(0u);
				}
			}

			builder.add_block(block);
		}

		builder.lift_continue_block_dominator();

		// Add it to a per-block list of variables.
		BlockID dominating_block = builder.get_dominator();

		if (dominating_block && potential_continue_block != 0 && potential_continue_block != ~0u)
		{
			auto &inner_block = get<SPIRBlock>(dominating_block);

			BlockID merge_candidate = 0;

			// Analyze the dominator. If it lives in a different loop scope than the candidate continue
			// block, reject the loop variable candidate.
			if (inner_block.merge == SPIRBlock::MergeLoop)
				merge_candidate = inner_block.merge_block;
			else if (inner_block.loop_dominator != SPIRBlock::NoDominator)
				merge_candidate = get<SPIRBlock>(inner_block.loop_dominator).merge_block;

			if (merge_candidate != 0 && cfg.is_reachable(merge_candidate))
			{
				// If the merge block has a higher post-visit order, we know that continue candidate
				// cannot reach the merge block, and we have two separate scopes.
				if (!cfg.is_reachable(potential_continue_block) ||
				    cfg.get_visit_order(merge_candidate) > cfg.get_visit_order(potential_continue_block))
				{
					potential_continue_block = 0;
				}
			}
		}

		if (potential_continue_block != 0 && potential_continue_block != ~0u)
			potential_loop_variables[var.first] = potential_continue_block;

		// For variables whose dominating block is inside a loop, there is a risk that these variables
		// actually need to be preserved across loop iterations. We can express this by adding
		// a "read" access to the loop header.
		// In the dominating block, we must see an OpStore or equivalent as the first access of an OpVariable.
		// Should that fail, we look for the outermost loop header and tack on an access there.
		// Phi nodes cannot have this problem.
		if (dominating_block)
		{
			auto &variable = get<SPIRVariable>(var.first);
			if (!variable.phi_variable)
			{
				auto *block = &get<SPIRBlock>(dominating_block);
				bool preserve = may_read_undefined_variable_in_block(*block, var.first);
				if (preserve)
				{
					// Find the outermost loop scope.
					while (block->loop_dominator != BlockID(SPIRBlock::NoDominator))
						block = &get<SPIRBlock>(block->loop_dominator);

					if (block->self != dominating_block)
					{
						builder.add_block(block->self);
						dominating_block = builder.get_dominator();
					}
				}
			}
		}

		// If all blocks here are dead code, this will be 0, so the variable in question
		// will be completely eliminated.
		if (dominating_block)
		{
			auto &block = get<SPIRBlock>(dominating_block);
			block.dominated_variables.push_back(var.first);
			get<SPIRVariable>(var.first).dominator = dominating_block;
		}
	}

	for (auto &var : handler.accessed_temporaries_to_block)
	{
		auto itr = handler.result_id_to_type.find(var.first);

		if (itr == end(handler.result_id_to_type))
		{
			// We found a false positive ID being used, ignore.
			// This should probably be an assert.
			continue;
		}

		// There is no point in doing domination analysis for opaque types.
		auto &type = get<SPIRType>(itr->second);
		if (type_is_opaque_value(type))
			continue;

		DominatorBuilder builder(cfg);
		bool force_temporary = false;
		bool used_in_header_hoisted_continue_block = false;

		// Figure out which block is dominating all accesses of those temporaries.
		auto &blocks = var.second;
		for (auto &block : blocks)
		{
			builder.add_block(block);

			if (blocks.size() != 1 && is_continue(block))
			{
				// The risk here is that inner loop can dominate the continue block.
				// Any temporary we access in the continue block must be declared before the loop.
				// This is moot for complex loops however.
				auto &loop_header_block = get<SPIRBlock>(ir.continue_block_to_loop_header[block]);
				assert(loop_header_block.merge == SPIRBlock::MergeLoop);
				builder.add_block(loop_header_block.self);
				used_in_header_hoisted_continue_block = true;
			}
		}

		uint32_t dominating_block = builder.get_dominator();

		if (blocks.size() != 1 && is_single_block_loop(dominating_block))
		{
			// Awkward case, because the loop header is also the continue block,
			// so hoisting to loop header does not help.
			force_temporary = true;
		}

		if (dominating_block)
		{
			// If we touch a variable in the dominating block, this is the expected setup.
			// SPIR-V normally mandates this, but we have extra cases for temporary use inside loops.
			bool first_use_is_dominator = blocks.count(dominating_block) != 0;

			if (!first_use_is_dominator || force_temporary)
			{
				if (handler.access_chain_expressions.count(var.first))
				{
					// Exceptionally rare case.
					// We cannot declare temporaries of access chains (except on MSL perhaps with pointers).
					// Rather than do that, we force the indexing expressions to be declared in the right scope by
					// tracking their usage to that end. There is no temporary to hoist.
					// However, we still need to observe declaration order of the access chain.

					if (used_in_header_hoisted_continue_block)
					{
						// For this scenario, we used an access chain inside a continue block where we also registered an access to header block.
						// This is a problem as we need to declare an access chain properly first with full definition.
						// We cannot use temporaries for these expressions,
						// so we must make sure the access chain is declared ahead of time.
						// Force a complex for loop to deal with this.
						// TODO: Out-of-order declaring for loops where continue blocks are emitted last might be another option.
						auto &loop_header_block = get<SPIRBlock>(dominating_block);
						assert(loop_header_block.merge == SPIRBlock::MergeLoop);
						loop_header_block.complex_continue = true;
					}
				}
				else
				{
					// This should be very rare, but if we try to declare a temporary inside a loop,
					// and that temporary is used outside the loop as well (spirv-opt inliner likes this)
					// we should actually emit the temporary outside the loop.
					hoisted_temporaries.insert(var.first);
					forced_temporaries.insert(var.first);

					auto &block_temporaries = get<SPIRBlock>(dominating_block).declare_temporary;
					block_temporaries.emplace_back(handler.result_id_to_type[var.first], var.first);
				}
			}
			else if (blocks.size() > 1)
			{
				// Keep track of the temporary as we might have to declare this temporary.
				// This can happen if the loop header dominates a temporary, but we have a complex fallback loop.
				// In this case, the header is actually inside the for (;;) {} block, and we have problems.
				// What we need to do is hoist the temporaries outside the for (;;) {} block in case the header block
				// declares the temporary.
				auto &block_temporaries = get<SPIRBlock>(dominating_block).potential_declare_temporary;
				block_temporaries.emplace_back(handler.result_id_to_type[var.first], var.first);
			}
		}
	}

	unordered_set<uint32_t> seen_blocks;

	// Now, try to analyze whether or not these variables are actually loop variables.
	for (auto &loop_variable : potential_loop_variables)
	{
		auto &var = get<SPIRVariable>(loop_variable.first);
		auto dominator = var.dominator;
		BlockID block = loop_variable.second;

		// The variable was accessed in multiple continue blocks, ignore.
		if (block == BlockID(~(0u)) || block == BlockID(0))
			continue;

		// Dead code.
		if (dominator == ID(0))
			continue;

		BlockID header = 0;

		// Find the loop header for this block if we are a continue block.
		{
			auto itr = ir.continue_block_to_loop_header.find(block);
			if (itr != end(ir.continue_block_to_loop_header))
			{
				header = itr->second;
			}
			else if (get<SPIRBlock>(block).continue_block == block)
			{
				// Also check for self-referential continue block.
				header = block;
			}
		}

		assert(header);
		auto &header_block = get<SPIRBlock>(header);
		auto &blocks = handler.accessed_variables_to_block[loop_variable.first];

		// If a loop variable is not used before the loop, it's probably not a loop variable.
		bool has_accessed_variable = blocks.count(header) != 0;

		// Now, there are two conditions we need to meet for the variable to be a loop variable.
		// 1. The dominating block must have a branch-free path to the loop header,
		// this way we statically know which expression should be part of the loop variable initializer.

		// Walk from the dominator, if there is one straight edge connecting
		// dominator and loop header, we statically know the loop initializer.
		bool static_loop_init = true;
		while (dominator != header)
		{
			if (blocks.count(dominator) != 0)
				has_accessed_variable = true;

			auto &succ = cfg.get_succeeding_edges(dominator);
			if (succ.size() != 1)
			{
				static_loop_init = false;
				break;
			}

			auto &pred = cfg.get_preceding_edges(succ.front());
			if (pred.size() != 1 || pred.front() != dominator)
			{
				static_loop_init = false;
				break;
			}

			dominator = succ.front();
		}

		if (!static_loop_init || !has_accessed_variable)
			continue;

		// The second condition we need to meet is that no access after the loop
		// merge can occur. Walk the CFG to see if we find anything.

		seen_blocks.clear();
		cfg.walk_from(seen_blocks, header_block.merge_block, [&](uint32_t walk_block) -> bool {
			// We found a block which accesses the variable outside the loop.
			if (blocks.find(walk_block) != end(blocks))
				static_loop_init = false;
			return true;
		});

		if (!static_loop_init)
			continue;

		// We have a loop variable.
		header_block.loop_variables.push_back(loop_variable.first);
		// Need to sort here as variables come from an unordered container, and pushing stuff in wrong order
		// will break reproducability in regression runs.
		sort(begin(header_block.loop_variables), end(header_block.loop_variables));
		get<SPIRVariable>(loop_variable.first).loop_variable = true;
	}
}

bool Compiler::may_read_undefined_variable_in_block(const SPIRBlock &block, uint32_t var)
{
	for (auto &op : block.ops)
	{
		auto *ops = stream(op);
		switch (op.op)
		{
		case OpStore:
		case OpCopyMemory:
			if (ops[0] == var)
				return false;
			break;

		case OpAccessChain:
		case OpInBoundsAccessChain:
		case OpPtrAccessChain:
			// Access chains are generally used to partially read and write. It's too hard to analyze
			// if all constituents are written fully before continuing, so just assume it's preserved.
			// This is the same as the parameter preservation analysis.
			if (ops[2] == var)
				return true;
			break;

		case OpSelect:
			// Variable pointers.
			// We might read before writing.
			if (ops[3] == var || ops[4] == var)
				return true;
			break;

		case OpPhi:
		{
			// Variable pointers.
			// We might read before writing.
			if (op.length < 2)
				break;

			uint32_t count = op.length - 2;
			for (uint32_t i = 0; i < count; i += 2)
				if (ops[i + 2] == var)
					return true;
			break;
		}

		case OpCopyObject:
		case OpLoad:
			if (ops[2] == var)
				return true;
			break;

		case OpFunctionCall:
		{
			if (op.length < 3)
				break;

			// May read before writing.
			uint32_t count = op.length - 3;
			for (uint32_t i = 0; i < count; i++)
				if (ops[i + 3] == var)
					return true;
			break;
		}

		default:
			break;
		}
	}

	// Not accessed somehow, at least not in a usual fashion.
	// It's likely accessed in a branch, so assume we must preserve.
	return true;
}

Bitset Compiler::get_buffer_block_flags(VariableID id) const
{
	return ir.get_buffer_block_flags(get<SPIRVariable>(id));
}

bool Compiler::get_common_basic_type(const SPIRType &type, SPIRType::BaseType &base_type)
{
	if (type.basetype == SPIRType::Struct)
	{
		base_type = SPIRType::Unknown;
		for (auto &member_type : type.member_types)
		{
			SPIRType::BaseType member_base;
			if (!get_common_basic_type(get<SPIRType>(member_type), member_base))
				return false;

			if (base_type == SPIRType::Unknown)
				base_type = member_base;
			else if (base_type != member_base)
				return false;
		}
		return true;
	}
	else
	{
		base_type = type.basetype;
		return true;
	}
}

void Compiler::ActiveBuiltinHandler::handle_builtin(const SPIRType &type, BuiltIn builtin,
                                                    const Bitset &decoration_flags)
{
	// If used, we will need to explicitly declare a new array size for these builtins.

	if (builtin == BuiltInClipDistance)
	{
		if (!type.array_size_literal[0])
			SPIRV_CROSS_THROW("Array size for ClipDistance must be a literal.");
		uint32_t array_size = type.array[0];
		if (array_size == 0)
			SPIRV_CROSS_THROW("Array size for ClipDistance must not be unsized.");
		compiler.clip_distance_count = array_size;
	}
	else if (builtin == BuiltInCullDistance)
	{
		if (!type.array_size_literal[0])
			SPIRV_CROSS_THROW("Array size for CullDistance must be a literal.");
		uint32_t array_size = type.array[0];
		if (array_size == 0)
			SPIRV_CROSS_THROW("Array size for CullDistance must not be unsized.");
		compiler.cull_distance_count = array_size;
	}
	else if (builtin == BuiltInPosition)
	{
		if (decoration_flags.get(DecorationInvariant))
			compiler.position_invariant = true;
	}
}

void Compiler::ActiveBuiltinHandler::add_if_builtin(uint32_t id, bool allow_blocks)
{
	// Only handle plain variables here.
	// Builtins which are part of a block are handled in AccessChain.
	// If allow_blocks is used however, this is to handle initializers of blocks,
	// which implies that all members are written to.

	auto *var = compiler.maybe_get<SPIRVariable>(id);
	auto *m = compiler.ir.find_meta(id);
	if (var && m)
	{
		auto &type = compiler.get<SPIRType>(var->basetype);
		auto &decorations = m->decoration;
		auto &flags = type.storage == StorageClassInput ?
		              compiler.active_input_builtins : compiler.active_output_builtins;
		if (decorations.builtin)
		{
			flags.set(decorations.builtin_type);
			handle_builtin(type, decorations.builtin_type, decorations.decoration_flags);
		}
		else if (allow_blocks && compiler.has_decoration(type.self, DecorationBlock))
		{
			uint32_t member_count = uint32_t(type.member_types.size());
			for (uint32_t i = 0; i < member_count; i++)
			{
				if (compiler.has_member_decoration(type.self, i, DecorationBuiltIn))
				{
					auto &member_type = compiler.get<SPIRType>(type.member_types[i]);
					BuiltIn builtin = BuiltIn(compiler.get_member_decoration(type.self, i, DecorationBuiltIn));
					flags.set(builtin);
					handle_builtin(member_type, builtin, compiler.get_member_decoration_bitset(type.self, i));
				}
			}
		}
	}
}

void Compiler::ActiveBuiltinHandler::add_if_builtin(uint32_t id)
{
	add_if_builtin(id, false);
}

void Compiler::ActiveBuiltinHandler::add_if_builtin_or_block(uint32_t id)
{
	add_if_builtin(id, true);
}

bool Compiler::ActiveBuiltinHandler::handle(spv::Op opcode, const uint32_t *args, uint32_t length)
{
	switch (opcode)
	{
	case OpStore:
		if (length < 1)
			return false;

		add_if_builtin(args[0]);
		break;

	case OpCopyMemory:
		if (length < 2)
			return false;

		add_if_builtin(args[0]);
		add_if_builtin(args[1]);
		break;

	case OpCopyObject:
	case OpLoad:
		if (length < 3)
			return false;

		add_if_builtin(args[2]);
		break;

	case OpSelect:
		if (length < 5)
			return false;

		add_if_builtin(args[3]);
		add_if_builtin(args[4]);
		break;

	case OpPhi:
	{
		if (length < 2)
			return false;

		uint32_t count = length - 2;
		args += 2;
		for (uint32_t i = 0; i < count; i += 2)
			add_if_builtin(args[i]);
		break;
	}

	case OpFunctionCall:
	{
		if (length < 3)
			return false;

		uint32_t count = length - 3;
		args += 3;
		for (uint32_t i = 0; i < count; i++)
			add_if_builtin(args[i]);
		break;
	}

	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpPtrAccessChain:
	{
		if (length < 4)
			return false;

		// Only consider global variables, cannot consider variables in functions yet, or other
		// access chains as they have not been created yet.
		auto *var = compiler.maybe_get<SPIRVariable>(args[2]);
		if (!var)
			break;

		// Required if we access chain into builtins like gl_GlobalInvocationID.
		add_if_builtin(args[2]);

		// Start traversing type hierarchy at the proper non-pointer types.
		auto *type = &compiler.get_variable_data_type(*var);

		auto &flags =
		    var->storage == StorageClassInput ? compiler.active_input_builtins : compiler.active_output_builtins;

		uint32_t count = length - 3;
		args += 3;
		for (uint32_t i = 0; i < count; i++)
		{
			// Pointers
			// PtrAccessChain functions more like a pointer offset. Type remains the same.
			if (opcode == OpPtrAccessChain && i == 0)
				continue;

			// Arrays
			if (!type->array.empty())
			{
				type = &compiler.get<SPIRType>(type->parent_type);
			}
			// Structs
			else if (type->basetype == SPIRType::Struct)
			{
				uint32_t index = compiler.get<SPIRConstant>(args[i]).scalar();

				if (index < uint32_t(compiler.ir.meta[type->self].members.size()))
				{
					auto &decorations = compiler.ir.meta[type->self].members[index];
					if (decorations.builtin)
					{
						flags.set(decorations.builtin_type);
						handle_builtin(compiler.get<SPIRType>(type->member_types[index]), decorations.builtin_type,
						               decorations.decoration_flags);
					}
				}

				type = &compiler.get<SPIRType>(type->member_types[index]);
			}
			else
			{
				// No point in traversing further. We won't find any extra builtins.
				break;
			}
		}
		break;
	}

	default:
		break;
	}

	return true;
}

void Compiler::update_active_builtins()
{
	active_input_builtins.reset();
	active_output_builtins.reset();
	cull_distance_count = 0;
	clip_distance_count = 0;
	ActiveBuiltinHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);

	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, const SPIRVariable &var) {
		if (var.storage != StorageClassOutput)
			return;
		if (!interface_variable_exists_in_entry_point(var.self))
			return;

		// Also, make sure we preserve output variables which are only initialized, but never accessed by any code.
		if (var.initializer != ID(0))
			handler.add_if_builtin_or_block(var.self);
	});
}

// Returns whether this shader uses a builtin of the storage class
bool Compiler::has_active_builtin(BuiltIn builtin, StorageClass storage) const
{
	const Bitset *flags;
	switch (storage)
	{
	case StorageClassInput:
		flags = &active_input_builtins;
		break;
	case StorageClassOutput:
		flags = &active_output_builtins;
		break;

	default:
		return false;
	}
	return flags->get(builtin);
}

void Compiler::analyze_image_and_sampler_usage()
{
	CombinedImageSamplerDrefHandler dref_handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), dref_handler);

	CombinedImageSamplerUsageHandler handler(*this, dref_handler.dref_combined_samplers);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);

	// Need to run this traversal twice. First time, we propagate any comparison sampler usage from leaf functions
	// down to main().
	// In the second pass, we can propagate up forced depth state coming from main() up into leaf functions.
	handler.dependency_hierarchy.clear();
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);

	comparison_ids = std::move(handler.comparison_ids);
	need_subpass_input = handler.need_subpass_input;
	need_subpass_input_ms = handler.need_subpass_input_ms;

	// Forward information from separate images and samplers into combined image samplers.
	for (auto &combined : combined_image_samplers)
		if (comparison_ids.count(combined.sampler_id))
			comparison_ids.insert(combined.combined_id);
}

bool Compiler::CombinedImageSamplerDrefHandler::handle(spv::Op opcode, const uint32_t *args, uint32_t)
{
	// Mark all sampled images which are used with Dref.
	switch (opcode)
	{
	case OpImageSampleDrefExplicitLod:
	case OpImageSampleDrefImplicitLod:
	case OpImageSampleProjDrefExplicitLod:
	case OpImageSampleProjDrefImplicitLod:
	case OpImageSparseSampleProjDrefImplicitLod:
	case OpImageSparseSampleDrefImplicitLod:
	case OpImageSparseSampleProjDrefExplicitLod:
	case OpImageSparseSampleDrefExplicitLod:
	case OpImageDrefGather:
	case OpImageSparseDrefGather:
		dref_combined_samplers.insert(args[2]);
		return true;

	default:
		break;
	}

	return true;
}

const CFG &Compiler::get_cfg_for_current_function() const
{
	assert(current_function);
	return get_cfg_for_function(current_function->self);
}

const CFG &Compiler::get_cfg_for_function(uint32_t id) const
{
	auto cfg_itr = function_cfgs.find(id);
	assert(cfg_itr != end(function_cfgs));
	assert(cfg_itr->second);
	return *cfg_itr->second;
}

void Compiler::build_function_control_flow_graphs_and_analyze()
{
	CFGBuilder handler(*this);
	handler.function_cfgs[ir.default_entry_point].reset(new CFG(*this, get<SPIRFunction>(ir.default_entry_point)));
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);
	function_cfgs = std::move(handler.function_cfgs);
	bool single_function = function_cfgs.size() <= 1;

	for (auto &f : function_cfgs)
	{
		auto &func = get<SPIRFunction>(f.first);
		AnalyzeVariableScopeAccessHandler scope_handler(*this, func);
		analyze_variable_scope(func, scope_handler);
		find_function_local_luts(func, scope_handler, single_function);

		// Check if we can actually use the loop variables we found in analyze_variable_scope.
		// To use multiple initializers, we need the same type and qualifiers.
		for (auto block : func.blocks)
		{
			auto &b = get<SPIRBlock>(block);
			if (b.loop_variables.size() < 2)
				continue;

			auto &flags = get_decoration_bitset(b.loop_variables.front());
			uint32_t type = get<SPIRVariable>(b.loop_variables.front()).basetype;
			bool invalid_initializers = false;
			for (auto loop_variable : b.loop_variables)
			{
				if (flags != get_decoration_bitset(loop_variable) ||
				    type != get<SPIRVariable>(b.loop_variables.front()).basetype)
				{
					invalid_initializers = true;
					break;
				}
			}

			if (invalid_initializers)
			{
				for (auto loop_variable : b.loop_variables)
					get<SPIRVariable>(loop_variable).loop_variable = false;
				b.loop_variables.clear();
			}
		}
	}

	// Find LUTs which are not function local. Only consider this case if the CFG is multi-function,
	// otherwise we treat Private as Function trivially.
	// Needs to be analyzed from the outside since we have to block the LUT optimization if at least
	// one function writes to it.
	if (!single_function)
	{
		for (auto &id : global_variables)
		{
			auto &var = get<SPIRVariable>(id);
			auto &type = get_variable_data_type(var);

			if (is_array(type) && var.storage == StorageClassPrivate &&
			    var.initializer && !var.is_written_to &&
			    ir.ids[var.initializer].get_type() == TypeConstant)
			{
				get<SPIRConstant>(var.initializer).is_used_as_lut = true;
				var.static_expression = var.initializer;
				var.statically_assigned = true;
				var.remapped_variable = true;
			}
		}
	}
}

Compiler::CFGBuilder::CFGBuilder(Compiler &compiler_)
    : compiler(compiler_)
{
}

bool Compiler::CFGBuilder::handle(spv::Op, const uint32_t *, uint32_t)
{
	return true;
}

bool Compiler::CFGBuilder::follow_function_call(const SPIRFunction &func)
{
	if (function_cfgs.find(func.self) == end(function_cfgs))
	{
		function_cfgs[func.self].reset(new CFG(compiler, func));
		return true;
	}
	else
		return false;
}

void Compiler::CombinedImageSamplerUsageHandler::add_dependency(uint32_t dst, uint32_t src)
{
	dependency_hierarchy[dst].insert(src);
	// Propagate up any comparison state if we're loading from one such variable.
	if (comparison_ids.count(src))
		comparison_ids.insert(dst);
}

bool Compiler::CombinedImageSamplerUsageHandler::begin_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;

	auto &func = compiler.get<SPIRFunction>(args[2]);
	const auto *arg = &args[3];
	length -= 3;

	for (uint32_t i = 0; i < length; i++)
	{
		auto &argument = func.arguments[i];
		add_dependency(argument.id, arg[i]);
	}

	return true;
}

void Compiler::CombinedImageSamplerUsageHandler::add_hierarchy_to_comparison_ids(uint32_t id)
{
	// Traverse the variable dependency hierarchy and tag everything in its path with comparison ids.
	comparison_ids.insert(id);

	for (auto &dep_id : dependency_hierarchy[id])
		add_hierarchy_to_comparison_ids(dep_id);
}

bool Compiler::CombinedImageSamplerUsageHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	switch (opcode)
	{
	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpPtrAccessChain:
	case OpLoad:
	{
		if (length < 3)
			return false;

		add_dependency(args[1], args[2]);

		// Ideally defer this to OpImageRead, but then we'd need to track loaded IDs.
		// If we load an image, we're going to use it and there is little harm in declaring an unused gl_FragCoord.
		auto &type = compiler.get<SPIRType>(args[0]);
		if (type.image.dim == DimSubpassData)
		{
			need_subpass_input = true;
			if (type.image.ms)
				need_subpass_input_ms = true;
		}

		// If we load a SampledImage and it will be used with Dref, propagate the state up.
		if (dref_combined_samplers.count(args[1]) != 0)
			add_hierarchy_to_comparison_ids(args[1]);
		break;
	}

	case OpSampledImage:
	{
		if (length < 4)
			return false;

		// If the underlying resource has been used for comparison then duplicate loads of that resource must be too.
		// This image must be a depth image.
		uint32_t result_id = args[1];
		uint32_t image = args[2];
		uint32_t sampler = args[3];

		if (dref_combined_samplers.count(result_id) != 0)
		{
			add_hierarchy_to_comparison_ids(image);

			// This sampler must be a SamplerComparisonState, and not a regular SamplerState.
			add_hierarchy_to_comparison_ids(sampler);

			// Mark the OpSampledImage itself as being comparison state.
			comparison_ids.insert(result_id);
		}
		return true;
	}

	default:
		break;
	}

	return true;
}

bool Compiler::buffer_is_hlsl_counter_buffer(VariableID id) const
{
	auto *m = ir.find_meta(id);
	return m && m->hlsl_is_magic_counter_buffer;
}

bool Compiler::buffer_get_hlsl_counter_buffer(VariableID id, uint32_t &counter_id) const
{
	auto *m = ir.find_meta(id);

	// First, check for the proper decoration.
	if (m && m->hlsl_magic_counter_buffer != 0)
	{
		counter_id = m->hlsl_magic_counter_buffer;
		return true;
	}
	else
		return false;
}

void Compiler::make_constant_null(uint32_t id, uint32_t type)
{
	auto &constant_type = get<SPIRType>(type);

	if (constant_type.pointer)
	{
		auto &constant = set<SPIRConstant>(id, type);
		constant.make_null(constant_type);
	}
	else if (!constant_type.array.empty())
	{
		assert(constant_type.parent_type);
		uint32_t parent_id = ir.increase_bound_by(1);
		make_constant_null(parent_id, constant_type.parent_type);

		if (!constant_type.array_size_literal.back())
			SPIRV_CROSS_THROW("Array size of OpConstantNull must be a literal.");

		SmallVector<uint32_t> elements(constant_type.array.back());
		for (uint32_t i = 0; i < constant_type.array.back(); i++)
			elements[i] = parent_id;
		set<SPIRConstant>(id, type, elements.data(), uint32_t(elements.size()), false);
	}
	else if (!constant_type.member_types.empty())
	{
		uint32_t member_ids = ir.increase_bound_by(uint32_t(constant_type.member_types.size()));
		SmallVector<uint32_t> elements(constant_type.member_types.size());
		for (uint32_t i = 0; i < constant_type.member_types.size(); i++)
		{
			make_constant_null(member_ids + i, constant_type.member_types[i]);
			elements[i] = member_ids + i;
		}
		set<SPIRConstant>(id, type, elements.data(), uint32_t(elements.size()), false);
	}
	else
	{
		auto &constant = set<SPIRConstant>(id, type);
		constant.make_null(constant_type);
	}
}

const SmallVector<spv::Capability> &Compiler::get_declared_capabilities() const
{
	return ir.declared_capabilities;
}

const SmallVector<std::string> &Compiler::get_declared_extensions() const
{
	return ir.declared_extensions;
}

std::string Compiler::get_remapped_declared_block_name(VariableID id) const
{
	return get_remapped_declared_block_name(id, false);
}

std::string Compiler::get_remapped_declared_block_name(uint32_t id, bool fallback_prefer_instance_name) const
{
	auto itr = declared_block_names.find(id);
	if (itr != end(declared_block_names))
	{
		return itr->second;
	}
	else
	{
		auto &var = get<SPIRVariable>(id);

		if (fallback_prefer_instance_name)
		{
			return to_name(var.self);
		}
		else
		{
			auto &type = get<SPIRType>(var.basetype);
			auto *type_meta = ir.find_meta(type.self);
			auto *block_name = type_meta ? &type_meta->decoration.alias : nullptr;
			return (!block_name || block_name->empty()) ? get_block_fallback_name(id) : *block_name;
		}
	}
}

bool Compiler::reflection_ssbo_instance_name_is_significant() const
{
	if (ir.source.known)
	{
		// UAVs from HLSL source tend to be declared in a way where the type is reused
		// but the instance name is significant, and that's the name we should report.
		// For GLSL, SSBOs each have their own block type as that's how GLSL is written.
		return ir.source.hlsl;
	}

	unordered_set<uint32_t> ssbo_type_ids;
	bool aliased_ssbo_types = false;

	// If we don't have any OpSource information, we need to perform some shaky heuristics.
	ir.for_each_typed_id<SPIRVariable>([&](uint32_t, const SPIRVariable &var) {
		auto &type = this->get<SPIRType>(var.basetype);
		if (!type.pointer || var.storage == StorageClassFunction)
			return;

		bool ssbo = var.storage == StorageClassStorageBuffer ||
		            (var.storage == StorageClassUniform && has_decoration(type.self, DecorationBufferBlock));

		if (ssbo)
		{
			if (ssbo_type_ids.count(type.self))
				aliased_ssbo_types = true;
			else
				ssbo_type_ids.insert(type.self);
		}
	});

	// If the block name is aliased, assume we have HLSL-style UAV declarations.
	return aliased_ssbo_types;
}

bool Compiler::instruction_to_result_type(uint32_t &result_type, uint32_t &result_id, spv::Op op,
                                          const uint32_t *args, uint32_t length)
{
	if (length < 2)
		return false;

	bool has_result_id = false, has_result_type = false;
	HasResultAndType(op, &has_result_id, &has_result_type);
	if (has_result_id && has_result_type)
	{
		result_type = args[0];
		result_id = args[1];
		return true;
	}
	else
		return false;
}

Bitset Compiler::combined_decoration_for_member(const SPIRType &type, uint32_t index) const
{
	Bitset flags;
	auto *type_meta = ir.find_meta(type.self);

	if (type_meta)
	{
		auto &members = type_meta->members;
		if (index >= members.size())
			return flags;
		auto &dec = members[index];

		flags.merge_or(dec.decoration_flags);

		auto &member_type = get<SPIRType>(type.member_types[index]);

		// If our member type is a struct, traverse all the child members as well recursively.
		auto &member_childs = member_type.member_types;
		for (uint32_t i = 0; i < member_childs.size(); i++)
		{
			auto &child_member_type = get<SPIRType>(member_childs[i]);
			if (!child_member_type.pointer)
				flags.merge_or(combined_decoration_for_member(member_type, i));
		}
	}

	return flags;
}

bool Compiler::is_desktop_only_format(spv::ImageFormat format)
{
	switch (format)
	{
	// Desktop-only formats
	case ImageFormatR11fG11fB10f:
	case ImageFormatR16f:
	case ImageFormatRgb10A2:
	case ImageFormatR8:
	case ImageFormatRg8:
	case ImageFormatR16:
	case ImageFormatRg16:
	case ImageFormatRgba16:
	case ImageFormatR16Snorm:
	case ImageFormatRg16Snorm:
	case ImageFormatRgba16Snorm:
	case ImageFormatR8Snorm:
	case ImageFormatRg8Snorm:
	case ImageFormatR8ui:
	case ImageFormatRg8ui:
	case ImageFormatR16ui:
	case ImageFormatRgb10a2ui:
	case ImageFormatR8i:
	case ImageFormatRg8i:
	case ImageFormatR16i:
		return true;
	default:
		break;
	}

	return false;
}

// An image is determined to be a depth image if it is marked as a depth image and is not also
// explicitly marked with a color format, or if there are any sample/gather compare operations on it.
bool Compiler::is_depth_image(const SPIRType &type, uint32_t id) const
{
	return (type.image.depth && type.image.format == ImageFormatUnknown) || comparison_ids.count(id);
}

bool Compiler::type_is_opaque_value(const SPIRType &type) const
{
	return !type.pointer && (type.basetype == SPIRType::SampledImage || type.basetype == SPIRType::Image ||
	                         type.basetype == SPIRType::Sampler);
}

// Make these member functions so we can easily break on any force_recompile events.
void Compiler::force_recompile()
{
	is_force_recompile = true;
}

void Compiler::force_recompile_guarantee_forward_progress()
{
	force_recompile();
	is_force_recompile_forward_progress = true;
}

bool Compiler::is_forcing_recompilation() const
{
	return is_force_recompile;
}

void Compiler::clear_force_recompile()
{
	is_force_recompile = false;
	is_force_recompile_forward_progress = false;
}

Compiler::PhysicalStorageBufferPointerHandler::PhysicalStorageBufferPointerHandler(Compiler &compiler_)
    : compiler(compiler_)
{
}

Compiler::PhysicalBlockMeta *Compiler::PhysicalStorageBufferPointerHandler::find_block_meta(uint32_t id) const
{
	auto chain_itr = access_chain_to_physical_block.find(id);
	if (chain_itr != access_chain_to_physical_block.end())
		return chain_itr->second;
	else
		return nullptr;
}

void Compiler::PhysicalStorageBufferPointerHandler::mark_aligned_access(uint32_t id, const uint32_t *args, uint32_t length)
{
	uint32_t mask = *args;
	args++;
	length--;
	if (length && (mask & MemoryAccessVolatileMask) != 0)
	{
		args++;
		length--;
	}

	if (length && (mask & MemoryAccessAlignedMask) != 0)
	{
		uint32_t alignment = *args;
		auto *meta = find_block_meta(id);

		// This makes the assumption that the application does not rely on insane edge cases like:
		// Bind buffer with ADDR = 8, use block offset of 8 bytes, load/store with 16 byte alignment.
		// If we emit the buffer with alignment = 16 here, the first element at offset = 0 should
		// actually have alignment of 8 bytes, but this is too theoretical and awkward to support.
		// We could potentially keep track of any offset in the access chain, but it's
		// practically impossible for high level compilers to emit code like that,
		// so deducing overall alignment requirement based on maximum observed Alignment value is probably fine.
		if (meta && alignment > meta->alignment)
			meta->alignment = alignment;
	}
}

bool Compiler::PhysicalStorageBufferPointerHandler::type_is_bda_block_entry(uint32_t type_id) const
{
	auto &type = compiler.get<SPIRType>(type_id);
	return compiler.is_physical_pointer(type);
}

uint32_t Compiler::PhysicalStorageBufferPointerHandler::get_minimum_scalar_alignment(const SPIRType &type) const
{
	if (type.storage == spv::StorageClassPhysicalStorageBufferEXT)
		return 8;
	else if (type.basetype == SPIRType::Struct)
	{
		uint32_t alignment = 0;
		for (auto &member_type : type.member_types)
		{
			uint32_t member_align = get_minimum_scalar_alignment(compiler.get<SPIRType>(member_type));
			if (member_align > alignment)
				alignment = member_align;
		}
		return alignment;
	}
	else
		return type.width / 8;
}

void Compiler::PhysicalStorageBufferPointerHandler::setup_meta_chain(uint32_t type_id, uint32_t var_id)
{
	if (type_is_bda_block_entry(type_id))
	{
		auto &meta = physical_block_type_meta[type_id];
		access_chain_to_physical_block[var_id] = &meta;

		auto &type = compiler.get<SPIRType>(type_id);

		if (!compiler.is_physical_pointer_to_buffer_block(type))
			non_block_types.insert(type_id);

		if (meta.alignment == 0)
			meta.alignment = get_minimum_scalar_alignment(compiler.get_pointee_type(type));
	}
}

bool Compiler::PhysicalStorageBufferPointerHandler::handle(Op op, const uint32_t *args, uint32_t length)
{
	// When a BDA pointer comes to life, we need to keep a mapping of SSA ID -> type ID for the pointer type.
	// For every load and store, we'll need to be able to look up the type ID being accessed and mark any alignment
	// requirements.
	switch (op)
	{
	case OpConvertUToPtr:
	case OpBitcast:
	case OpCompositeExtract:
		// Extract can begin a new chain if we had a struct or array of pointers as input.
		// We don't begin chains before we have a pure scalar pointer.
		setup_meta_chain(args[0], args[1]);
		break;

	case OpAccessChain:
	case OpInBoundsAccessChain:
	case OpPtrAccessChain:
	case OpCopyObject:
	{
		auto itr = access_chain_to_physical_block.find(args[2]);
		if (itr != access_chain_to_physical_block.end())
			access_chain_to_physical_block[args[1]] = itr->second;
		break;
	}

	case OpLoad:
	{
		setup_meta_chain(args[0], args[1]);
		if (length >= 4)
			mark_aligned_access(args[2], args + 3, length - 3);
		break;
	}

	case OpStore:
	{
		if (length >= 3)
			mark_aligned_access(args[0], args + 2, length - 2);
		break;
	}

	default:
		break;
	}

	return true;
}

uint32_t Compiler::PhysicalStorageBufferPointerHandler::get_base_non_block_type_id(uint32_t type_id) const
{
	auto *type = &compiler.get<SPIRType>(type_id);
	while (compiler.is_physical_pointer(*type) && !type_is_bda_block_entry(type_id))
	{
		type_id = type->parent_type;
		type = &compiler.get<SPIRType>(type_id);
	}

	assert(type_is_bda_block_entry(type_id));
	return type_id;
}

void Compiler::PhysicalStorageBufferPointerHandler::analyze_non_block_types_from_block(const SPIRType &type)
{
	for (auto &member : type.member_types)
	{
		auto &subtype = compiler.get<SPIRType>(member);

		if (compiler.is_physical_pointer(subtype) && !compiler.is_physical_pointer_to_buffer_block(subtype))
			non_block_types.insert(get_base_non_block_type_id(member));
		else if (subtype.basetype == SPIRType::Struct && !compiler.is_pointer(subtype))
			analyze_non_block_types_from_block(subtype);
	}
}

void Compiler::analyze_non_block_pointer_types()
{
	PhysicalStorageBufferPointerHandler handler(*this);
	traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);

	// Analyze any block declaration we have to make. It might contain
	// physical pointers to POD types which we never used, and thus never added to the list.
	// We'll need to add those pointer types to the set of types we declare.
	ir.for_each_typed_id<SPIRType>([&](uint32_t id, SPIRType &type) {
		// Only analyze the raw block struct, not any pointer-to-struct, since that's just redundant.
		if (type.self == id &&
		    (has_decoration(type.self, DecorationBlock) ||
		     has_decoration(type.self, DecorationBufferBlock)))
		{
			handler.analyze_non_block_types_from_block(type);
		}
	});

	physical_storage_non_block_pointer_types.reserve(handler.non_block_types.size());
	for (auto type : handler.non_block_types)
		physical_storage_non_block_pointer_types.push_back(type);
	sort(begin(physical_storage_non_block_pointer_types), end(physical_storage_non_block_pointer_types));
	physical_storage_type_to_alignment = std::move(handler.physical_block_type_meta);
}

bool Compiler::InterlockedResourceAccessPrepassHandler::handle(Op op, const uint32_t *, uint32_t)
{
	if (op == OpBeginInvocationInterlockEXT || op == OpEndInvocationInterlockEXT)
	{
		if (interlock_function_id != 0 && interlock_function_id != call_stack.back())
		{
			// Most complex case, we have no sensible way of dealing with this
			// other than taking the 100% conservative approach, exit early.
			split_function_case = true;
			return false;
		}
		else
		{
			interlock_function_id = call_stack.back();
			// If this call is performed inside control flow we have a problem.
			auto &cfg = compiler.get_cfg_for_function(interlock_function_id);

			uint32_t from_block_id = compiler.get<SPIRFunction>(interlock_function_id).entry_block;
			bool outside_control_flow = cfg.node_terminates_control_flow_in_sub_graph(from_block_id, current_block_id);
			if (!outside_control_flow)
				control_flow_interlock = true;
		}
	}
	return true;
}

void Compiler::InterlockedResourceAccessPrepassHandler::rearm_current_block(const SPIRBlock &block)
{
	current_block_id = block.self;
}

bool Compiler::InterlockedResourceAccessPrepassHandler::begin_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;
	call_stack.push_back(args[2]);
	return true;
}

bool Compiler::InterlockedResourceAccessPrepassHandler::end_function_scope(const uint32_t *, uint32_t)
{
	call_stack.pop_back();
	return true;
}

bool Compiler::InterlockedResourceAccessHandler::begin_function_scope(const uint32_t *args, uint32_t length)
{
	if (length < 3)
		return false;

	if (args[2] == interlock_function_id)
		call_stack_is_interlocked = true;

	call_stack.push_back(args[2]);
	return true;
}

bool Compiler::InterlockedResourceAccessHandler::end_function_scope(const uint32_t *, uint32_t)
{
	if (call_stack.back() == interlock_function_id)
		call_stack_is_interlocked = false;

	call_stack.pop_back();
	return true;
}

void Compiler::InterlockedResourceAccessHandler::access_potential_resource(uint32_t id)
{
	if ((use_critical_section && in_crit_sec) || (control_flow_interlock && call_stack_is_interlocked) ||
	    split_function_case)
	{
		compiler.interlocked_resources.insert(id);
	}
}

bool Compiler::InterlockedResourceAccessHandler::handle(Op opcode, const uint32_t *args, uint32_t length)
{
	// Only care about critical section analysis if we have simple case.
	if (use_critical_section)
	{
		if (opcode == OpBeginInvocationInterlockEXT)
		{
			in_crit_sec = true;
			return true;
		}

		if (opcode == OpEndInvocationInterlockEXT)
		{
			// End critical section--nothing more to do.
			return false;
		}
	}

	// We need to figure out where images and buffers are loaded from, so do only the bare bones compilation we need.
	switch (opcode)
	{
	case OpLoad:
	{
		if (length < 3)
			return false;

		uint32_t ptr = args[2];
		auto *var = compiler.maybe_get_backing_variable(ptr);

		// We're only concerned with buffer and image memory here.
		if (!var)
			break;

		switch (var->storage)
		{
		default:
			break;

		case StorageClassUniformConstant:
		{
			uint32_t result_type = args[0];
			uint32_t id = args[1];
			compiler.set<SPIRExpression>(id, "", result_type, true);
			compiler.register_read(id, ptr, true);
			break;
		}

		case StorageClassUniform:
			// Must have BufferBlock; we only care about SSBOs.
			if (!compiler.has_decoration(compiler.get<SPIRType>(var->basetype).self, DecorationBufferBlock))
				break;
			// fallthrough
		case StorageClassStorageBuffer:
			access_potential_resource(var->self);
			break;
		}
		break;
	}

	case OpInBoundsAccessChain:
	case OpAccessChain:
	case OpPtrAccessChain:
	{
		if (length < 3)
			return false;

		uint32_t result_type = args[0];

		auto &type = compiler.get<SPIRType>(result_type);
		if (type.storage == StorageClassUniform || type.storage == StorageClassUniformConstant ||
		    type.storage == StorageClassStorageBuffer)
		{
			uint32_t id = args[1];
			uint32_t ptr = args[2];
			compiler.set<SPIRExpression>(id, "", result_type, true);
			compiler.register_read(id, ptr, true);
			compiler.ir.ids[id].set_allow_type_rewrite();
		}
		break;
	}

	case OpImageTexelPointer:
	{
		if (length < 3)
			return false;

		uint32_t result_type = args[0];
		uint32_t id = args[1];
		uint32_t ptr = args[2];
		auto &e = compiler.set<SPIRExpression>(id, "", result_type, true);
		auto *var = compiler.maybe_get_backing_variable(ptr);
		if (var)
			e.loaded_from = var->self;
		break;
	}

	case OpStore:
	case OpImageWrite:
	case OpAtomicStore:
	{
		if (length < 1)
			return false;

		uint32_t ptr = args[0];
		auto *var = compiler.maybe_get_backing_variable(ptr);
		if (var && (var->storage == StorageClassUniform || var->storage == StorageClassUniformConstant ||
		            var->storage == StorageClassStorageBuffer))
		{
			access_potential_resource(var->self);
		}

		break;
	}

	case OpCopyMemory:
	{
		if (length < 2)
			return false;

		uint32_t dst = args[0];
		uint32_t src = args[1];
		auto *dst_var = compiler.maybe_get_backing_variable(dst);
		auto *src_var = compiler.maybe_get_backing_variable(src);

		if (dst_var && (dst_var->storage == StorageClassUniform || dst_var->storage == StorageClassStorageBuffer))
			access_potential_resource(dst_var->self);

		if (src_var)
		{
			if (src_var->storage != StorageClassUniform && src_var->storage != StorageClassStorageBuffer)
				break;

			if (src_var->storage == StorageClassUniform &&
			    !compiler.has_decoration(compiler.get<SPIRType>(src_var->basetype).self, DecorationBufferBlock))
			{
				break;
			}

			access_potential_resource(src_var->self);
		}

		break;
	}

	case OpImageRead:
	case OpAtomicLoad:
	{
		if (length < 3)
			return false;

		uint32_t ptr = args[2];
		auto *var = compiler.maybe_get_backing_variable(ptr);

		// We're only concerned with buffer and image memory here.
		if (!var)
			break;

		switch (var->storage)
		{
		default:
			break;

		case StorageClassUniform:
			// Must have BufferBlock; we only care about SSBOs.
			if (!compiler.has_decoration(compiler.get<SPIRType>(var->basetype).self, DecorationBufferBlock))
				break;
			// fallthrough
		case StorageClassUniformConstant:
		case StorageClassStorageBuffer:
			access_potential_resource(var->self);
			break;
		}
		break;
	}

	case OpAtomicExchange:
	case OpAtomicCompareExchange:
	case OpAtomicIIncrement:
	case OpAtomicIDecrement:
	case OpAtomicIAdd:
	case OpAtomicISub:
	case OpAtomicSMin:
	case OpAtomicUMin:
	case OpAtomicSMax:
	case OpAtomicUMax:
	case OpAtomicAnd:
	case OpAtomicOr:
	case OpAtomicXor:
	{
		if (length < 3)
			return false;

		uint32_t ptr = args[2];
		auto *var = compiler.maybe_get_backing_variable(ptr);
		if (var && (var->storage == StorageClassUniform || var->storage == StorageClassUniformConstant ||
		            var->storage == StorageClassStorageBuffer))
		{
			access_potential_resource(var->self);
		}

		break;
	}

	default:
		break;
	}

	return true;
}

void Compiler::analyze_interlocked_resource_usage()
{
	if (get_execution_model() == ExecutionModelFragment &&
	    (get_entry_point().flags.get(ExecutionModePixelInterlockOrderedEXT) ||
	     get_entry_point().flags.get(ExecutionModePixelInterlockUnorderedEXT) ||
	     get_entry_point().flags.get(ExecutionModeSampleInterlockOrderedEXT) ||
	     get_entry_point().flags.get(ExecutionModeSampleInterlockUnorderedEXT)))
	{
		InterlockedResourceAccessPrepassHandler prepass_handler(*this, ir.default_entry_point);
		traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), prepass_handler);

		InterlockedResourceAccessHandler handler(*this, ir.default_entry_point);
		handler.interlock_function_id = prepass_handler.interlock_function_id;
		handler.split_function_case = prepass_handler.split_function_case;
		handler.control_flow_interlock = prepass_handler.control_flow_interlock;
		handler.use_critical_section = !handler.split_function_case && !handler.control_flow_interlock;

		traverse_all_reachable_opcodes(get<SPIRFunction>(ir.default_entry_point), handler);

		// For GLSL. If we hit any of these cases, we have to fall back to conservative approach.
		interlocked_is_complex =
		    !handler.use_critical_section || handler.interlock_function_id != ir.default_entry_point;
	}
}

// Helper function
bool Compiler::check_internal_recursion(const SPIRType &type, std::unordered_set<uint32_t> &checked_ids)
{
	if (type.basetype != SPIRType::Struct)
		return false;

	if (checked_ids.count(type.self))
		return true;

	// Recurse into struct members
	bool is_recursive = false;
	checked_ids.insert(type.self);
	uint32_t mbr_cnt = uint32_t(type.member_types.size());
	for (uint32_t mbr_idx = 0; !is_recursive && mbr_idx < mbr_cnt; mbr_idx++)
	{
		uint32_t mbr_type_id = type.member_types[mbr_idx];
		auto &mbr_type = get<SPIRType>(mbr_type_id);
		is_recursive |= check_internal_recursion(mbr_type, checked_ids);
	}
	checked_ids.erase(type.self);
	return is_recursive;
}

// Return whether the struct type contains a structural recursion nested somewhere within its content.
bool Compiler::type_contains_recursion(const SPIRType &type)
{
	std::unordered_set<uint32_t> checked_ids;
	return check_internal_recursion(type, checked_ids);
}

bool Compiler::type_is_array_of_pointers(const SPIRType &type) const
{
	if (!is_array(type))
		return false;

	// BDA types must have parent type hierarchy.
	if (!type.parent_type)
		return false;

	// Punch through all array layers.
	auto *parent = &get<SPIRType>(type.parent_type);
	while (is_array(*parent))
		parent = &get<SPIRType>(parent->parent_type);

	return is_pointer(*parent);
}

bool Compiler::flush_phi_required(BlockID from, BlockID to) const
{
	auto &child = get<SPIRBlock>(to);
	for (auto &phi : child.phi_variables)
		if (phi.parent == from)
			return true;
	return false;
}

void Compiler::add_loop_level()
{
	current_loop_level++;
}
