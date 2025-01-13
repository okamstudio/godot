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

#ifndef SPIRV_CROSS_COMMON_HPP
#define SPIRV_CROSS_COMMON_HPP

#ifndef SPV_ENABLE_UTILITY_CODE
#define SPV_ENABLE_UTILITY_CODE
#endif
#include "spirv.hpp"

#include "spirv_cross_containers.hpp"
#include "spirv_cross_error_handling.hpp"
#include <functional>

// A bit crude, but allows projects which embed SPIRV-Cross statically to
// effectively hide all the symbols from other projects.
// There is a case where we have:
// - Project A links against SPIRV-Cross statically.
// - Project A links against Project B statically.
// - Project B links against SPIRV-Cross statically (might be a different version).
// This leads to a conflict with extremely bizarre results.
// By overriding the namespace in one of the project builds, we can work around this.
// If SPIRV-Cross is embedded in dynamic libraries,
// prefer using -fvisibility=hidden on GCC/Clang instead.
#ifdef SPIRV_CROSS_NAMESPACE_OVERRIDE
#define SPIRV_CROSS_NAMESPACE SPIRV_CROSS_NAMESPACE_OVERRIDE
#else
#define SPIRV_CROSS_NAMESPACE spirv_cross
#endif

namespace SPIRV_CROSS_NAMESPACE
{
namespace inner
{
template <typename T>
void join_helper(StringStream<> &stream, T &&t)
{
	stream << std::forward<T>(t);
}

template <typename T, typename... Ts>
void join_helper(StringStream<> &stream, T &&t, Ts &&... ts)
{
	stream << std::forward<T>(t);
	join_helper(stream, std::forward<Ts>(ts)...);
}
} // namespace inner

class Bitset
{
public:
	Bitset() = default;
	explicit inline Bitset(uint64_t lower_)
	    : lower(lower_)
	{
	}

	inline bool get(uint32_t bit) const
	{
		if (bit < 64)
			return (lower & (1ull << bit)) != 0;
		else
			return higher.count(bit) != 0;
	}

	inline void set(uint32_t bit)
	{
		if (bit < 64)
			lower |= 1ull << bit;
		else
			higher.insert(bit);
	}

	inline void clear(uint32_t bit)
	{
		if (bit < 64)
			lower &= ~(1ull << bit);
		else
			higher.erase(bit);
	}

	inline uint64_t get_lower() const
	{
		return lower;
	}

	inline void reset()
	{
		lower = 0;
		higher.clear();
	}

	inline void merge_and(const Bitset &other)
	{
		lower &= other.lower;
		std::unordered_set<uint32_t> tmp_set;
		for (auto &v : higher)
			if (other.higher.count(v) != 0)
				tmp_set.insert(v);
		higher = std::move(tmp_set);
	}

	inline void merge_or(const Bitset &other)
	{
		lower |= other.lower;
		for (auto &v : other.higher)
			higher.insert(v);
	}

	inline bool operator==(const Bitset &other) const
	{
		if (lower != other.lower)
			return false;

		if (higher.size() != other.higher.size())
			return false;

		for (auto &v : higher)
			if (other.higher.count(v) == 0)
				return false;

		return true;
	}

	inline bool operator!=(const Bitset &other) const
	{
		return !(*this == other);
	}

	template <typename Op>
	void for_each_bit(const Op &op) const
	{
		// TODO: Add ctz-based iteration.
		for (uint32_t i = 0; i < 64; i++)
		{
			if (lower & (1ull << i))
				op(i);
		}

		if (higher.empty())
			return;

		// Need to enforce an order here for reproducible results,
		// but hitting this path should happen extremely rarely, so having this slow path is fine.
		SmallVector<uint32_t> bits;
		bits.reserve(higher.size());
		for (auto &v : higher)
			bits.push_back(v);
		std::sort(std::begin(bits), std::end(bits));

		for (auto &v : bits)
			op(v);
	}

	inline bool empty() const
	{
		return lower == 0 && higher.empty();
	}

private:
	// The most common bits to set are all lower than 64,
	// so optimize for this case. Bits spilling outside 64 go into a slower data structure.
	// In almost all cases, higher data structure will not be used.
	uint64_t lower = 0;
	std::unordered_set<uint32_t> higher;
};

// Helper template to avoid lots of nasty string temporary munging.
template <typename... Ts>
std::string join(Ts &&... ts)
{
	StringStream<> stream;
	inner::join_helper(stream, std::forward<Ts>(ts)...);
	return stream.str();
}

inline std::string merge(const SmallVector<std::string> &list, const char *between = ", ")
{
	StringStream<> stream;
	for (auto &elem : list)
	{
		stream << elem;
		if (&elem != &list.back())
			stream << between;
	}
	return stream.str();
}

// Make sure we don't accidentally call this with float or doubles with SFINAE.
// Have to use the radix-aware overload.
template <typename T, typename std::enable_if<!std::is_floating_point<T>::value, int>::type = 0>
inline std::string convert_to_string(const T &t)
{
	return std::to_string(t);
}

static inline std::string convert_to_string(int32_t value)
{
	// INT_MIN is ... special on some backends. If we use a decimal literal, and negate it, we
	// could accidentally promote the literal to long first, then negate.
	// To workaround it, emit int(0x80000000) instead.
	if (value == (std::numeric_limits<int32_t>::min)())
		return "int(0x80000000)";
	else
		return std::to_string(value);
}

static inline std::string convert_to_string(int64_t value, const std::string &int64_type, bool long_long_literal_suffix)
{
	// INT64_MIN is ... special on some backends.
	// If we use a decimal literal, and negate it, we might overflow the representable numbers.
	// To workaround it, emit int(0x80000000) instead.
	if (value == (std::numeric_limits<int64_t>::min)())
		return join(int64_type, "(0x8000000000000000u", (long_long_literal_suffix ? "ll" : "l"), ")");
	else
		return std::to_string(value) + (long_long_literal_suffix ? "ll" : "l");
}

// Allow implementations to set a convenient standard precision
#ifndef SPIRV_CROSS_FLT_FMT
#define SPIRV_CROSS_FLT_FMT "%.32g"
#endif

// Disable sprintf and strcat warnings.
// We cannot rely on snprintf and family existing because, ..., MSVC.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

static inline void fixup_radix_point(char *str, char radix_point)
{
	// Setting locales is a very risky business in multi-threaded program,
	// so just fixup locales instead. We only need to care about the radix point.
	if (radix_point != '.')
	{
		while (*str != '\0')
		{
			if (*str == radix_point)
				*str = '.';
			str++;
		}
	}
}

inline std::string convert_to_string(float t, char locale_radix_point)
{
	// std::to_string for floating point values is broken.
	// Fallback to something more sane.
	char buf[64];
	sprintf(buf, SPIRV_CROSS_FLT_FMT, t);
	fixup_radix_point(buf, locale_radix_point);

	// Ensure that the literal is float.
	if (!strchr(buf, '.') && !strchr(buf, 'e'))
		strcat(buf, ".0");
	return buf;
}

inline std::string convert_to_string(double t, char locale_radix_point)
{
	// std::to_string for floating point values is broken.
	// Fallback to something more sane.
	char buf[64];
	sprintf(buf, SPIRV_CROSS_FLT_FMT, t);
	fixup_radix_point(buf, locale_radix_point);

	// Ensure that the literal is float.
	if (!strchr(buf, '.') && !strchr(buf, 'e'))
		strcat(buf, ".0");
	return buf;
}

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

class FloatFormatter
{
public:
	virtual ~FloatFormatter() = default;
	virtual std::string format_float(float value) = 0;
	virtual std::string format_double(double value) = 0;
};

template <typename T>
struct ValueSaver
{
	explicit ValueSaver(T &current_)
	    : current(current_)
	    , saved(current_)
	{
	}

	void release()
	{
		current = saved;
	}

	~ValueSaver()
	{
		release();
	}

	T &current;
	T saved;
};

struct Instruction
{
	uint16_t op = 0;
	uint16_t count = 0;
	// If offset is 0 (not a valid offset into the instruction stream),
	// we have an instruction stream which is embedded in the object.
	uint32_t offset = 0;
	uint32_t length = 0;

	inline bool is_embedded() const
	{
		return offset == 0;
	}
};

struct EmbeddedInstruction : Instruction
{
	SmallVector<uint32_t> ops;
};

enum Types
{
	TypeNone,
	TypeType,
	TypeVariable,
	TypeConstant,
	TypeFunction,
	TypeFunctionPrototype,
	TypeBlock,
	TypeExtension,
	TypeExpression,
	TypeConstantOp,
	TypeCombinedImageSampler,
	TypeAccessChain,
	TypeUndef,
	TypeString,
	TypeCount
};

template <Types type>
class TypedID;

template <>
class TypedID<TypeNone>
{
public:
	TypedID() = default;
	TypedID(uint32_t id_)
	    : id(id_)
	{
	}

	template <Types U>
	TypedID(const TypedID<U> &other)
	{
		*this = other;
	}

	template <Types U>
	TypedID &operator=(const TypedID<U> &other)
	{
		id = uint32_t(other);
		return *this;
	}

	// Implicit conversion to u32 is desired here.
	// As long as we block implicit conversion between TypedID<A> and TypedID<B> we're good.
	operator uint32_t() const
	{
		return id;
	}

	template <Types U>
	operator TypedID<U>() const
	{
		return TypedID<U>(*this);
	}

private:
	uint32_t id = 0;
};

template <Types type>
class TypedID
{
public:
	TypedID() = default;
	TypedID(uint32_t id_)
	    : id(id_)
	{
	}

	explicit TypedID(const TypedID<TypeNone> &other)
	    : id(uint32_t(other))
	{
	}

	operator uint32_t() const
	{
		return id;
	}

private:
	uint32_t id = 0;
};

using VariableID = TypedID<TypeVariable>;
using TypeID = TypedID<TypeType>;
using ConstantID = TypedID<TypeConstant>;
using FunctionID = TypedID<TypeFunction>;
using BlockID = TypedID<TypeBlock>;
using ID = TypedID<TypeNone>;

// Helper for Variant interface.
struct IVariant
{
	virtual ~IVariant() = default;
	virtual IVariant *clone(ObjectPoolBase *pool) = 0;
	ID self = 0;

protected:
	IVariant() = default;
	IVariant(const IVariant&) = default;
	IVariant &operator=(const IVariant&) = default;
};

#define SPIRV_CROSS_DECLARE_CLONE(T)                                \
	IVariant *clone(ObjectPoolBase *pool) override                  \
	{                                                               \
		return static_cast<ObjectPool<T> *>(pool)->allocate(*this); \
	}

struct SPIRUndef : IVariant
{
	enum
	{
		type = TypeUndef
	};

	explicit SPIRUndef(TypeID basetype_)
	    : basetype(basetype_)
	{
	}
	TypeID basetype;

	SPIRV_CROSS_DECLARE_CLONE(SPIRUndef)
};

struct SPIRString : IVariant
{
	enum
	{
		type = TypeString
	};

	explicit SPIRString(std::string str_)
	    : str(std::move(str_))
	{
	}

	std::string str;

	SPIRV_CROSS_DECLARE_CLONE(SPIRString)
};

// This type is only used by backends which need to access the combined image and sampler IDs separately after
// the OpSampledImage opcode.
struct SPIRCombinedImageSampler : IVariant
{
	enum
	{
		type = TypeCombinedImageSampler
	};
	SPIRCombinedImageSampler(TypeID type_, VariableID image_, VariableID sampler_)
	    : combined_type(type_)
	    , image(image_)
	    , sampler(sampler_)
	{
	}
	TypeID combined_type;
	VariableID image;
	VariableID sampler;

	SPIRV_CROSS_DECLARE_CLONE(SPIRCombinedImageSampler)
};

struct SPIRConstantOp : IVariant
{
	enum
	{
		type = TypeConstantOp
	};

	SPIRConstantOp(TypeID result_type, spv::Op op, const uint32_t *args, uint32_t length)
	    : opcode(op)
	    , basetype(result_type)
	{
		arguments.reserve(length);
		for (uint32_t i = 0; i < length; i++)
			arguments.push_back(args[i]);
	}

	spv::Op opcode;
	SmallVector<uint32_t> arguments;
	TypeID basetype;

	SPIRV_CROSS_DECLARE_CLONE(SPIRConstantOp)
};

struct SPIRType : IVariant
{
	enum
	{
		type = TypeType
	};

	spv::Op op = spv::Op::OpNop;
	explicit SPIRType(spv::Op op_) : op(op_) {}

	enum BaseType
	{
		Unknown,
		Void,
		Boolean,
		SByte,
		UByte,
		Short,
		UShort,
		Int,
		UInt,
		Int64,
		UInt64,
		AtomicCounter,
		Half,
		Float,
		Double,
		Struct,
		Image,
		SampledImage,
		Sampler,
		AccelerationStructure,
		RayQuery,

		// Keep internal types at the end.
		ControlPointArray,
		Interpolant,
		Char,
		// MSL specific type, that is used by 'object'(analog of 'task' from glsl) shader.
		MeshGridProperties
	};

	// Scalar/vector/matrix support.
	BaseType basetype = Unknown;
	uint32_t width = 0;
	uint32_t vecsize = 1;
	uint32_t columns = 1;

	// Arrays, support array of arrays by having a vector of array sizes.
	SmallVector<uint32_t> array;

	// Array elements can be either specialization constants or specialization ops.
	// This array determines how to interpret the array size.
	// If an element is true, the element is a literal,
	// otherwise, it's an expression, which must be resolved on demand.
	// The actual size is not really known until runtime.
	SmallVector<bool> array_size_literal;

	// Pointers
	// Keep track of how many pointer layers we have.
	uint32_t pointer_depth = 0;
	bool pointer = false;
	bool forward_pointer = false;

	spv::StorageClass storage = spv::StorageClassGeneric;

	SmallVector<TypeID> member_types;

	// If member order has been rewritten to handle certain scenarios with Offset,
	// allow codegen to rewrite the index.
	SmallVector<uint32_t> member_type_index_redirection;

	struct ImageType
	{
		TypeID type;
		spv::Dim dim;
		bool depth;
		bool arrayed;
		bool ms;
		uint32_t sampled;
		spv::ImageFormat format;
		spv::AccessQualifier access;
	} image = {};

	// Structs can be declared multiple times if they are used as part of interface blocks.
	// We want to detect this so that we only emit the struct definition once.
	// Since we cannot rely on OpName to be equal, we need to figure out aliases.
	TypeID type_alias = 0;

	// Denotes the type which this type is based on.
	// Allows the backend to traverse how a complex type is built up during access chains.
	TypeID parent_type = 0;

	// Used in backends to avoid emitting members with conflicting names.
	std::unordered_set<std::string> member_name_cache;

	SPIRV_CROSS_DECLARE_CLONE(SPIRType)
};

struct SPIRExtension : IVariant
{
	enum
	{
		type = TypeExtension
	};

	enum Extension
	{
		Unsupported,
		GLSL,
		SPV_debug_info,
		SPV_AMD_shader_ballot,
		SPV_AMD_shader_explicit_vertex_parameter,
		SPV_AMD_shader_trinary_minmax,
		SPV_AMD_gcn_shader,
		NonSemanticDebugPrintf,
		NonSemanticShaderDebugInfo,
		NonSemanticGeneric
	};

	explicit SPIRExtension(Extension ext_)
	    : ext(ext_)
	{
	}

	Extension ext;
	SPIRV_CROSS_DECLARE_CLONE(SPIRExtension)
};

// SPIREntryPoint is not a variant since its IDs are used to decorate OpFunction,
// so in order to avoid conflicts, we can't stick them in the ids array.
struct SPIREntryPoint
{
	SPIREntryPoint(FunctionID self_, spv::ExecutionModel execution_model, const std::string &entry_name)
	    : self(self_)
	    , name(entry_name)
	    , orig_name(entry_name)
	    , model(execution_model)
	{
	}
	SPIREntryPoint() = default;

	FunctionID self = 0;
	std::string name;
	std::string orig_name;
	SmallVector<VariableID> interface_variables;

	Bitset flags;
	struct WorkgroupSize
	{
		uint32_t x = 0, y = 0, z = 0;
		uint32_t id_x = 0, id_y = 0, id_z = 0;
		uint32_t constant = 0; // Workgroup size can be expressed as a constant/spec-constant instead.
	} workgroup_size;
	uint32_t invocations = 0;
	uint32_t output_vertices = 0;
	uint32_t output_primitives = 0;
	spv::ExecutionModel model = spv::ExecutionModelMax;
	bool geometry_passthrough = false;
};

struct SPIRExpression : IVariant
{
	enum
	{
		type = TypeExpression
	};

	// Only created by the backend target to avoid creating tons of temporaries.
	SPIRExpression(std::string expr, TypeID expression_type_, bool immutable_)
	    : expression(std::move(expr))
	    , expression_type(expression_type_)
	    , immutable(immutable_)
	{
	}

	// If non-zero, prepend expression with to_expression(base_expression).
	// Used in amortizing multiple calls to to_expression()
	// where in certain cases that would quickly force a temporary when not needed.
	ID base_expression = 0;

	std::string expression;
	TypeID expression_type = 0;

	// If this expression is a forwarded load,
	// allow us to reference the original variable.
	ID loaded_from = 0;

	// If this expression will never change, we can avoid lots of temporaries
	// in high level source.
	// An expression being immutable can be speculative,
	// it is assumed that this is true almost always.
	bool immutable = false;

	// Before use, this expression must be transposed.
	// This is needed for targets which don't support row_major layouts.
	bool need_transpose = false;

	// Whether or not this is an access chain expression.
	bool access_chain = false;

	// Whether or not gl_MeshVerticesEXT[].gl_Position (as a whole or .y) is referenced
	bool access_meshlet_position_y = false;

	// A list of expressions which this expression depends on.
	SmallVector<ID> expression_dependencies;

	// Similar as expression dependencies, but does not stop the tracking for force-temporary variables.
	// We need to know the full chain from store back to any SSA variable.
	SmallVector<ID> invariance_dependencies;

	// By reading this expression, we implicitly read these expressions as well.
	// Used by access chain Store and Load since we read multiple expressions in this case.
	SmallVector<ID> implied_read_expressions;

	// The expression was emitted at a certain scope. Lets us track when an expression read means multiple reads.
	uint32_t emitted_loop_level = 0;

	SPIRV_CROSS_DECLARE_CLONE(SPIRExpression)
};

struct SPIRFunctionPrototype : IVariant
{
	enum
	{
		type = TypeFunctionPrototype
	};

	explicit SPIRFunctionPrototype(TypeID return_type_)
	    : return_type(return_type_)
	{
	}

	TypeID return_type;
	SmallVector<uint32_t> parameter_types;

	SPIRV_CROSS_DECLARE_CLONE(SPIRFunctionPrototype)
};

struct SPIRBlock : IVariant
{
	enum
	{
		type = TypeBlock
	};

	enum Terminator
	{
		Unknown,
		Direct, // Emit next block directly without a particular condition.

		Select, // Block ends with an if/else block.
		MultiSelect, // Block ends with switch statement.

		Return, // Block ends with return.
		Unreachable, // Noop
		Kill, // Discard
		IgnoreIntersection, // Ray Tracing
		TerminateRay, // Ray Tracing
		EmitMeshTasks // Mesh shaders
	};

	enum Merge
	{
		MergeNone,
		MergeLoop,
		MergeSelection
	};

	enum Hints
	{
		HintNone,
		HintUnroll,
		HintDontUnroll,
		HintFlatten,
		HintDontFlatten
	};

	enum Method
	{
		MergeToSelectForLoop,
		MergeToDirectForLoop,
		MergeToSelectContinueForLoop
	};

	enum ContinueBlockType
	{
		ContinueNone,

		// Continue block is branchless and has at least one instruction.
		ForLoop,

		// Noop continue block.
		WhileLoop,

		// Continue block is conditional.
		DoWhileLoop,

		// Highly unlikely that anything will use this,
		// since it is really awkward/impossible to express in GLSL.
		ComplexLoop
	};

	enum : uint32_t
	{
		NoDominator = 0xffffffffu
	};

	Terminator terminator = Unknown;
	Merge merge = MergeNone;
	Hints hint = HintNone;
	BlockID next_block = 0;
	BlockID merge_block = 0;
	BlockID continue_block = 0;

	ID return_value = 0; // If 0, return nothing (void).
	ID condition = 0;
	BlockID true_block = 0;
	BlockID false_block = 0;
	BlockID default_block = 0;

	// If terminator is EmitMeshTasksEXT.
	struct
	{
		ID groups[3];
		ID payload;
	} mesh = {};

	SmallVector<Instruction> ops;

	struct Phi
	{
		ID local_variable; // flush local variable ...
		BlockID parent; // If we're in from_block and want to branch into this block ...
		VariableID function_variable; // to this function-global "phi" variable first.
	};

	// Before entering this block flush out local variables to magical "phi" variables.
	SmallVector<Phi> phi_variables;

	// Declare these temporaries before beginning the block.
	// Used for handling complex continue blocks which have side effects.
	SmallVector<std::pair<TypeID, ID>> declare_temporary;

	// Declare these temporaries, but only conditionally if this block turns out to be
	// a complex loop header.
	SmallVector<std::pair<TypeID, ID>> potential_declare_temporary;

	struct Case
	{
		uint64_t value;
		BlockID block;
	};
	SmallVector<Case> cases_32bit;
	SmallVector<Case> cases_64bit;

	// If we have tried to optimize code for this block but failed,
	// keep track of this.
	bool disable_block_optimization = false;

	// If the continue block is complex, fallback to "dumb" for loops.
	bool complex_continue = false;

	// Do we need a ladder variable to defer breaking out of a loop construct after a switch block?
	bool need_ladder_break = false;

	// If marked, we have explicitly handled Phi from this block, so skip any flushes related to that on a branch.
	// Used to handle an edge case with switch and case-label fallthrough where fall-through writes to Phi.
	BlockID ignore_phi_from_block = 0;

	// The dominating block which this block might be within.
	// Used in continue; blocks to determine if we really need to write continue.
	BlockID loop_dominator = 0;

	// All access to these variables are dominated by this block,
	// so before branching anywhere we need to make sure that we declare these variables.
	SmallVector<VariableID> dominated_variables;

	// These are variables which should be declared in a for loop header, if we
	// fail to use a classic for-loop,
	// we remove these variables, and fall back to regular variables outside the loop.
	SmallVector<VariableID> loop_variables;

	// Some expressions are control-flow dependent, i.e. any instruction which relies on derivatives or
	// sub-group-like operations.
	// Make sure that we only use these expressions in the original block.
	SmallVector<ID> invalidate_expressions;

	SPIRV_CROSS_DECLARE_CLONE(SPIRBlock)
};

struct SPIRFunction : IVariant
{
	enum
	{
		type = TypeFunction
	};

	SPIRFunction(TypeID return_type_, TypeID function_type_)
	    : return_type(return_type_)
	    , function_type(function_type_)
	{
	}

	struct Parameter
	{
		TypeID type;
		ID id;
		uint32_t read_count;
		uint32_t write_count;

		// Set to true if this parameter aliases a global variable,
		// used mostly in Metal where global variables
		// have to be passed down to functions as regular arguments.
		// However, for this kind of variable, we should not care about
		// read and write counts as access to the function arguments
		// is not local to the function in question.
		bool alias_global_variable;
	};

	// When calling a function, and we're remapping separate image samplers,
	// resolve these arguments into combined image samplers and pass them
	// as additional arguments in this order.
	// It gets more complicated as functions can pull in their own globals
	// and combine them with parameters,
	// so we need to distinguish if something is local parameter index
	// or a global ID.
	struct CombinedImageSamplerParameter
	{
		VariableID id;
		VariableID image_id;
		VariableID sampler_id;
		bool global_image;
		bool global_sampler;
		bool depth;
	};

	TypeID return_type;
	TypeID function_type;
	SmallVector<Parameter> arguments;

	// Can be used by backends to add magic arguments.
	// Currently used by combined image/sampler implementation.

	SmallVector<Parameter> shadow_arguments;
	SmallVector<VariableID> local_variables;
	BlockID entry_block = 0;
	SmallVector<BlockID> blocks;
	SmallVector<CombinedImageSamplerParameter> combined_parameters;

	struct EntryLine
	{
		uint32_t file_id = 0;
		uint32_t line_literal = 0;
	};
	EntryLine entry_line;

	void add_local_variable(VariableID id)
	{
		local_variables.push_back(id);
	}

	void add_parameter(TypeID parameter_type, ID id, bool alias_global_variable = false)
	{
		// Arguments are read-only until proven otherwise.
		arguments.push_back({ parameter_type, id, 0u, 0u, alias_global_variable });
	}

	// Hooks to be run when the function returns.
	// Mostly used for lowering internal data structures onto flattened structures.
	// Need to defer this, because they might rely on things which change during compilation.
	// Intentionally not a small vector, this one is rare, and std::function can be large.
	Vector<std::function<void()>> fixup_hooks_out;

	// Hooks to be run when the function begins.
	// Mostly used for populating internal data structures from flattened structures.
	// Need to defer this, because they might rely on things which change during compilation.
	// Intentionally not a small vector, this one is rare, and std::function can be large.
	Vector<std::function<void()>> fixup_hooks_in;

	// On function entry, make sure to copy a constant array into thread addr space to work around
	// the case where we are passing a constant array by value to a function on backends which do not
	// consider arrays value types.
	SmallVector<ID> constant_arrays_needed_on_stack;

	bool active = false;
	bool flush_undeclared = true;
	bool do_combined_parameters = true;

	SPIRV_CROSS_DECLARE_CLONE(SPIRFunction)
};

struct SPIRAccessChain : IVariant
{
	enum
	{
		type = TypeAccessChain
	};

	SPIRAccessChain(TypeID basetype_, spv::StorageClass storage_, std::string base_, std::string dynamic_index_,
	                int32_t static_index_)
	    : basetype(basetype_)
	    , storage(storage_)
	    , base(std::move(base_))
	    , dynamic_index(std::move(dynamic_index_))
	    , static_index(static_index_)
	{
	}

	// The access chain represents an offset into a buffer.
	// Some backends need more complicated handling of access chains to be able to use buffers, like HLSL
	// which has no usable buffer type ala GLSL SSBOs.
	// StructuredBuffer is too limited, so our only option is to deal with ByteAddressBuffer which works with raw addresses.

	TypeID basetype;
	spv::StorageClass storage;
	std::string base;
	std::string dynamic_index;
	int32_t static_index;

	VariableID loaded_from = 0;
	uint32_t matrix_stride = 0;
	uint32_t array_stride = 0;
	bool row_major_matrix = false;
	bool immutable = false;

	// By reading this expression, we implicitly read these expressions as well.
	// Used by access chain Store and Load since we read multiple expressions in this case.
	SmallVector<ID> implied_read_expressions;

	SPIRV_CROSS_DECLARE_CLONE(SPIRAccessChain)
};

struct SPIRVariable : IVariant
{
	enum
	{
		type = TypeVariable
	};

	SPIRVariable() = default;
	SPIRVariable(TypeID basetype_, spv::StorageClass storage_, ID initializer_ = 0, VariableID basevariable_ = 0)
	    : basetype(basetype_)
	    , storage(storage_)
	    , initializer(initializer_)
	    , basevariable(basevariable_)
	{
	}

	TypeID basetype = 0;
	spv::StorageClass storage = spv::StorageClassGeneric;
	uint32_t decoration = 0;
	ID initializer = 0;
	VariableID basevariable = 0;

	SmallVector<uint32_t> dereference_chain;
	bool compat_builtin = false;

	// If a variable is shadowed, we only statically assign to it
	// and never actually emit a statement for it.
	// When we read the variable as an expression, just forward
	// shadowed_id as the expression.
	bool statically_assigned = false;
	ID static_expression = 0;

	// Temporaries which can remain forwarded as long as this variable is not modified.
	SmallVector<ID> dependees;

	bool deferred_declaration = false;
	bool phi_variable = false;

	// Used to deal with Phi variable flushes. See flush_phi().
	bool allocate_temporary_copy = false;

	bool remapped_variable = false;
	uint32_t remapped_components = 0;

	// The block which dominates all access to this variable.
	BlockID dominator = 0;
	// If true, this variable is a loop variable, when accessing the variable
	// outside a loop,
	// we should statically forward it.
	bool loop_variable = false;
	// Set to true while we're inside the for loop.
	bool loop_variable_enable = false;

	// Used to find global LUTs
	bool is_written_to = false;

	SPIRFunction::Parameter *parameter = nullptr;

	SPIRV_CROSS_DECLARE_CLONE(SPIRVariable)
};

struct SPIRConstant : IVariant
{
	enum
	{
		type = TypeConstant
	};

	union Constant
	{
		uint32_t u32;
		int32_t i32;
		float f32;

		uint64_t u64;
		int64_t i64;
		double f64;
	};

	struct ConstantVector
	{
		Constant r[4];
		// If != 0, this element is a specialization constant, and we should keep track of it as such.
		ID id[4];
		uint32_t vecsize = 1;

		ConstantVector()
		{
			memset(r, 0, sizeof(r));
		}
	};

	struct ConstantMatrix
	{
		ConstantVector c[4];
		// If != 0, this column is a specialization constant, and we should keep track of it as such.
		ID id[4];
		uint32_t columns = 1;
	};

	static inline float f16_to_f32(uint16_t u16_value)
	{
		// Based on the GLM implementation.
		int s = (u16_value >> 15) & 0x1;
		int e = (u16_value >> 10) & 0x1f;
		int m = (u16_value >> 0) & 0x3ff;

		union
		{
			float f32;
			uint32_t u32;
		} u;

		if (e == 0)
		{
			if (m == 0)
			{
				u.u32 = uint32_t(s) << 31;
				return u.f32;
			}
			else
			{
				while ((m & 0x400) == 0)
				{
					m <<= 1;
					e--;
				}

				e++;
				m &= ~0x400;
			}
		}
		else if (e == 31)
		{
			if (m == 0)
			{
				u.u32 = (uint32_t(s) << 31) | 0x7f800000u;
				return u.f32;
			}
			else
			{
				u.u32 = (uint32_t(s) << 31) | 0x7f800000u | (m << 13);
				return u.f32;
			}
		}

		e += 127 - 15;
		m <<= 13;
		u.u32 = (uint32_t(s) << 31) | (e << 23) | m;
		return u.f32;
	}

	inline uint32_t specialization_constant_id(uint32_t col, uint32_t row) const
	{
		return m.c[col].id[row];
	}

	inline uint32_t specialization_constant_id(uint32_t col) const
	{
		return m.id[col];
	}

	inline uint32_t scalar(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].u32;
	}

	inline int16_t scalar_i16(uint32_t col = 0, uint32_t row = 0) const
	{
		return int16_t(m.c[col].r[row].u32 & 0xffffu);
	}

	inline uint16_t scalar_u16(uint32_t col = 0, uint32_t row = 0) const
	{
		return uint16_t(m.c[col].r[row].u32 & 0xffffu);
	}

	inline int8_t scalar_i8(uint32_t col = 0, uint32_t row = 0) const
	{
		return int8_t(m.c[col].r[row].u32 & 0xffu);
	}

	inline uint8_t scalar_u8(uint32_t col = 0, uint32_t row = 0) const
	{
		return uint8_t(m.c[col].r[row].u32 & 0xffu);
	}

	inline float scalar_f16(uint32_t col = 0, uint32_t row = 0) const
	{
		return f16_to_f32(scalar_u16(col, row));
	}

	inline float scalar_f32(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].f32;
	}

	inline int32_t scalar_i32(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].i32;
	}

	inline double scalar_f64(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].f64;
	}

	inline int64_t scalar_i64(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].i64;
	}

	inline uint64_t scalar_u64(uint32_t col = 0, uint32_t row = 0) const
	{
		return m.c[col].r[row].u64;
	}

	inline const ConstantVector &vector() const
	{
		return m.c[0];
	}

	inline uint32_t vector_size() const
	{
		return m.c[0].vecsize;
	}

	inline uint32_t columns() const
	{
		return m.columns;
	}

	inline void make_null(const SPIRType &constant_type_)
	{
		m = {};
		m.columns = constant_type_.columns;
		for (auto &c : m.c)
			c.vecsize = constant_type_.vecsize;
	}

	inline bool constant_is_null() const
	{
		if (specialization)
			return false;
		if (!subconstants.empty())
			return false;

		for (uint32_t col = 0; col < columns(); col++)
			for (uint32_t row = 0; row < vector_size(); row++)
				if (scalar_u64(col, row) != 0)
					return false;

		return true;
	}

	explicit SPIRConstant(uint32_t constant_type_)
	    : constant_type(constant_type_)
	{
	}

	SPIRConstant() = default;

	SPIRConstant(TypeID constant_type_, const uint32_t *elements, uint32_t num_elements, bool specialized)
	    : constant_type(constant_type_)
	    , specialization(specialized)
	{
		subconstants.reserve(num_elements);
		for (uint32_t i = 0; i < num_elements; i++)
			subconstants.push_back(elements[i]);
		specialization = specialized;
	}

	// Construct scalar (32-bit).
	SPIRConstant(TypeID constant_type_, uint32_t v0, bool specialized)
	    : constant_type(constant_type_)
	    , specialization(specialized)
	{
		m.c[0].r[0].u32 = v0;
		m.c[0].vecsize = 1;
		m.columns = 1;
	}

	// Construct scalar (64-bit).
	SPIRConstant(TypeID constant_type_, uint64_t v0, bool specialized)
	    : constant_type(constant_type_)
	    , specialization(specialized)
	{
		m.c[0].r[0].u64 = v0;
		m.c[0].vecsize = 1;
		m.columns = 1;
	}

	// Construct vectors and matrices.
	SPIRConstant(TypeID constant_type_, const SPIRConstant *const *vector_elements, uint32_t num_elements,
	             bool specialized)
	    : constant_type(constant_type_)
	    , specialization(specialized)
	{
		bool matrix = vector_elements[0]->m.c[0].vecsize > 1;

		if (matrix)
		{
			m.columns = num_elements;

			for (uint32_t i = 0; i < num_elements; i++)
			{
				m.c[i] = vector_elements[i]->m.c[0];
				if (vector_elements[i]->specialization)
					m.id[i] = vector_elements[i]->self;
			}
		}
		else
		{
			m.c[0].vecsize = num_elements;
			m.columns = 1;

			for (uint32_t i = 0; i < num_elements; i++)
			{
				m.c[0].r[i] = vector_elements[i]->m.c[0].r[0];
				if (vector_elements[i]->specialization)
					m.c[0].id[i] = vector_elements[i]->self;
			}
		}
	}

	TypeID constant_type = 0;
	ConstantMatrix m;

	// If this constant is a specialization constant (i.e. created with OpSpecConstant*).
	bool specialization = false;
	// If this constant is used as an array length which creates specialization restrictions on some backends.
	bool is_used_as_array_length = false;

	// If true, this is a LUT, and should always be declared in the outer scope.
	bool is_used_as_lut = false;

	// For composites which are constant arrays, etc.
	SmallVector<ConstantID> subconstants;

	// Non-Vulkan GLSL, HLSL and sometimes MSL emits defines for each specialization constant,
	// and uses them to initialize the constant. This allows the user
	// to still be able to specialize the value by supplying corresponding
	// preprocessor directives before compiling the shader.
	std::string specialization_constant_macro_name;

	SPIRV_CROSS_DECLARE_CLONE(SPIRConstant)
};

// Variants have a very specific allocation scheme.
struct ObjectPoolGroup
{
	std::unique_ptr<ObjectPoolBase> pools[TypeCount];
};

class Variant
{
public:
	explicit Variant(ObjectPoolGroup *group_)
	    : group(group_)
	{
	}

	~Variant()
	{
		if (holder)
			group->pools[type]->deallocate_opaque(holder);
	}

	// Marking custom move constructor as noexcept is important.
	Variant(Variant &&other) SPIRV_CROSS_NOEXCEPT
	{
		*this = std::move(other);
	}

	// We cannot copy from other variant without our own pool group.
	// Have to explicitly copy.
	Variant(const Variant &variant) = delete;

	// Marking custom move constructor as noexcept is important.
	Variant &operator=(Variant &&other) SPIRV_CROSS_NOEXCEPT
	{
		if (this != &other)
		{
			if (holder)
				group->pools[type]->deallocate_opaque(holder);
			holder = other.holder;
			group = other.group;
			type = other.type;
			allow_type_rewrite = other.allow_type_rewrite;

			other.holder = nullptr;
			other.type = TypeNone;
		}
		return *this;
	}

	// This copy/clone should only be called in the Compiler constructor.
	// If this is called inside ::compile(), we invalidate any references we took higher in the stack.
	// This should never happen.
	Variant &operator=(const Variant &other)
	{
//#define SPIRV_CROSS_COPY_CONSTRUCTOR_SANITIZE
#ifdef SPIRV_CROSS_COPY_CONSTRUCTOR_SANITIZE
		abort();
#endif
		if (this != &other)
		{
			if (holder)
				group->pools[type]->deallocate_opaque(holder);

			if (other.holder)
				holder = other.holder->clone(group->pools[other.type].get());
			else
				holder = nullptr;

			type = other.type;
			allow_type_rewrite = other.allow_type_rewrite;
		}
		return *this;
	}

	void set(IVariant *val, Types new_type)
	{
		if (holder)
			group->pools[type]->deallocate_opaque(holder);
		holder = nullptr;

		if (!allow_type_rewrite && type != TypeNone && type != new_type)
		{
			if (val)
				group->pools[new_type]->deallocate_opaque(val);
			SPIRV_CROSS_THROW("Overwriting a variant with new type.");
		}

		holder = val;
		type = new_type;
		allow_type_rewrite = false;
	}

	template <typename T, typename... Ts>
	T *allocate_and_set(Types new_type, Ts &&... ts)
	{
		T *val = static_cast<ObjectPool<T> &>(*group->pools[new_type]).allocate(std::forward<Ts>(ts)...);
		set(val, new_type);
		return val;
	}

	template <typename T>
	T &get()
	{
		if (!holder)
			SPIRV_CROSS_THROW("nullptr");
		if (static_cast<Types>(T::type) != type)
			SPIRV_CROSS_THROW("Bad cast");
		return *static_cast<T *>(holder);
	}

	template <typename T>
	const T &get() const
	{
		if (!holder)
			SPIRV_CROSS_THROW("nullptr");
		if (static_cast<Types>(T::type) != type)
			SPIRV_CROSS_THROW("Bad cast");
		return *static_cast<const T *>(holder);
	}

	Types get_type() const
	{
		return type;
	}

	ID get_id() const
	{
		return holder ? holder->self : ID(0);
	}

	bool empty() const
	{
		return !holder;
	}

	void reset()
	{
		if (holder)
			group->pools[type]->deallocate_opaque(holder);
		holder = nullptr;
		type = TypeNone;
	}

	void set_allow_type_rewrite()
	{
		allow_type_rewrite = true;
	}

private:
	ObjectPoolGroup *group = nullptr;
	IVariant *holder = nullptr;
	Types type = TypeNone;
	bool allow_type_rewrite = false;
};

template <typename T>
T &variant_get(Variant &var)
{
	return var.get<T>();
}

template <typename T>
const T &variant_get(const Variant &var)
{
	return var.get<T>();
}

template <typename T, typename... P>
T &variant_set(Variant &var, P &&... args)
{
	auto *ptr = var.allocate_and_set<T>(static_cast<Types>(T::type), std::forward<P>(args)...);
	return *ptr;
}

struct AccessChainMeta
{
	uint32_t storage_physical_type = 0;
	bool need_transpose = false;
	bool storage_is_packed = false;
	bool storage_is_invariant = false;
	bool flattened_struct = false;
	bool relaxed_precision = false;
	bool access_meshlet_position_y = false;
	bool chain_is_builtin = false;
	spv::BuiltIn builtin = {};
};

enum ExtendedDecorations
{
	// Marks if a buffer block is re-packed, i.e. member declaration might be subject to PhysicalTypeID remapping and padding.
	SPIRVCrossDecorationBufferBlockRepacked = 0,

	// A type in a buffer block might be declared with a different physical type than the logical type.
	// If this is not set, PhysicalTypeID == the SPIR-V type as declared.
	SPIRVCrossDecorationPhysicalTypeID,

	// Marks if the physical type is to be declared with tight packing rules, i.e. packed_floatN on MSL and friends.
	// If this is set, PhysicalTypeID might also be set. It can be set to same as logical type if all we're doing
	// is converting float3 to packed_float3 for example.
	// If this is marked on a struct, it means the struct itself must use only Packed types for all its members.
	SPIRVCrossDecorationPhysicalTypePacked,

	// The padding in bytes before declaring this struct member.
	// If used on a struct type, marks the target size of a struct.
	SPIRVCrossDecorationPaddingTarget,

	SPIRVCrossDecorationInterfaceMemberIndex,
	SPIRVCrossDecorationInterfaceOrigID,
	SPIRVCrossDecorationResourceIndexPrimary,
	// Used for decorations like resource indices for samplers when part of combined image samplers.
	// A variable might need to hold two resource indices in this case.
	SPIRVCrossDecorationResourceIndexSecondary,
	// Used for resource indices for multiplanar images when part of combined image samplers.
	SPIRVCrossDecorationResourceIndexTertiary,
	SPIRVCrossDecorationResourceIndexQuaternary,

	// Marks a buffer block for using explicit offsets (GLSL/HLSL).
	SPIRVCrossDecorationExplicitOffset,

	// Apply to a variable in the Input storage class; marks it as holding the base group passed to vkCmdDispatchBase(),
	// or the base vertex and instance indices passed to vkCmdDrawIndexed().
	// In MSL, this is used to adjust the WorkgroupId and GlobalInvocationId variables in compute shaders,
	// and to hold the BaseVertex and BaseInstance variables in vertex shaders.
	SPIRVCrossDecorationBuiltInDispatchBase,

	// Apply to a variable that is a function parameter; marks it as being a "dynamic"
	// combined image-sampler. In MSL, this is used when a function parameter might hold
	// either a regular combined image-sampler or one that has an attached sampler
	// Y'CbCr conversion.
	SPIRVCrossDecorationDynamicImageSampler,

	// Apply to a variable in the Input storage class; marks it as holding the size of the stage
	// input grid.
	// In MSL, this is used to hold the vertex and instance counts in a tessellation pipeline
	// vertex shader.
	SPIRVCrossDecorationBuiltInStageInputSize,

	// Apply to any access chain of a tessellation I/O variable; stores the type of the sub-object
	// that was chained to, as recorded in the input variable itself. This is used in case the pointer
	// is itself used as the base of an access chain, to calculate the original type of the sub-object
	// chained to, in case a swizzle needs to be applied. This should not happen normally with valid
	// SPIR-V, but the MSL backend can change the type of input variables, necessitating the
	// addition of swizzles to keep the generated code compiling.
	SPIRVCrossDecorationTessIOOriginalInputTypeID,

	// Apply to any access chain of an interface variable used with pull-model interpolation, where the variable is a
	// vector but the resulting pointer is a scalar; stores the component index that is to be accessed by the chain.
	// This is used when emitting calls to interpolation functions on the chain in MSL: in this case, the component
	// must be applied to the result, since pull-model interpolants in MSL cannot be swizzled directly, but the
	// results of interpolation can.
	SPIRVCrossDecorationInterpolantComponentExpr,

	// Apply to any struct type that is used in the Workgroup storage class.
	// This causes matrices in MSL prior to Metal 3.0 to be emitted using a special
	// class that is convertible to the standard matrix type, to work around the
	// lack of constructors in the 'threadgroup' address space.
	SPIRVCrossDecorationWorkgroupStruct,

	SPIRVCrossDecorationOverlappingBinding,

	SPIRVCrossDecorationCount
};

struct Meta
{
	struct Decoration
	{
		std::string alias;
		std::string qualified_alias;
		std::string hlsl_semantic;
		std::string user_type;
		Bitset decoration_flags;
		spv::BuiltIn builtin_type = spv::BuiltInMax;
		uint32_t location = 0;
		uint32_t component = 0;
		uint32_t set = 0;
		uint32_t binding = 0;
		uint32_t offset = 0;
		uint32_t xfb_buffer = 0;
		uint32_t xfb_stride = 0;
		uint32_t stream = 0;
		uint32_t array_stride = 0;
		uint32_t matrix_stride = 0;
		uint32_t input_attachment = 0;
		uint32_t spec_id = 0;
		uint32_t index = 0;
		spv::FPRoundingMode fp_rounding_mode = spv::FPRoundingModeMax;
		bool builtin = false;
		bool qualified_alias_explicit_override = false;

		struct Extended
		{
			Extended()
			{
				// MSVC 2013 workaround to init like this.
				for (auto &v : values)
					v = 0;
			}

			Bitset flags;
			uint32_t values[SPIRVCrossDecorationCount];
		} extended;
	};

	Decoration decoration;

	// Intentionally not a SmallVector. Decoration is large and somewhat rare.
	Vector<Decoration> members;

	std::unordered_map<uint32_t, uint32_t> decoration_word_offset;

	// For SPV_GOOGLE_hlsl_functionality1.
	bool hlsl_is_magic_counter_buffer = false;
	// ID for the sibling counter buffer.
	uint32_t hlsl_magic_counter_buffer = 0;
};

// A user callback that remaps the type of any variable.
// var_name is the declared name of the variable.
// name_of_type is the textual name of the type which will be used in the code unless written to by the callback.
using VariableTypeRemapCallback =
    std::function<void(const SPIRType &type, const std::string &var_name, std::string &name_of_type)>;

class Hasher
{
public:
	inline void u32(uint32_t value)
	{
		h = (h * 0x100000001b3ull) ^ value;
	}

	inline uint64_t get() const
	{
		return h;
	}

private:
	uint64_t h = 0xcbf29ce484222325ull;
};

static inline bool type_is_floating_point(const SPIRType &type)
{
	return type.basetype == SPIRType::Half || type.basetype == SPIRType::Float || type.basetype == SPIRType::Double;
}

static inline bool type_is_integral(const SPIRType &type)
{
	return type.basetype == SPIRType::SByte || type.basetype == SPIRType::UByte || type.basetype == SPIRType::Short ||
	       type.basetype == SPIRType::UShort || type.basetype == SPIRType::Int || type.basetype == SPIRType::UInt ||
	       type.basetype == SPIRType::Int64 || type.basetype == SPIRType::UInt64;
}

static inline SPIRType::BaseType to_signed_basetype(uint32_t width)
{
	switch (width)
	{
	case 8:
		return SPIRType::SByte;
	case 16:
		return SPIRType::Short;
	case 32:
		return SPIRType::Int;
	case 64:
		return SPIRType::Int64;
	default:
		SPIRV_CROSS_THROW("Invalid bit width.");
	}
}

static inline SPIRType::BaseType to_unsigned_basetype(uint32_t width)
{
	switch (width)
	{
	case 8:
		return SPIRType::UByte;
	case 16:
		return SPIRType::UShort;
	case 32:
		return SPIRType::UInt;
	case 64:
		return SPIRType::UInt64;
	default:
		SPIRV_CROSS_THROW("Invalid bit width.");
	}
}

// Returns true if an arithmetic operation does not change behavior depending on signedness.
static inline bool opcode_is_sign_invariant(spv::Op opcode)
{
	switch (opcode)
	{
	case spv::OpIEqual:
	case spv::OpINotEqual:
	case spv::OpISub:
	case spv::OpIAdd:
	case spv::OpIMul:
	case spv::OpShiftLeftLogical:
	case spv::OpBitwiseOr:
	case spv::OpBitwiseXor:
	case spv::OpBitwiseAnd:
		return true;

	default:
		return false;
	}
}

static inline bool opcode_can_promote_integer_implicitly(spv::Op opcode)
{
	switch (opcode)
	{
	case spv::OpSNegate:
	case spv::OpNot:
	case spv::OpBitwiseAnd:
	case spv::OpBitwiseOr:
	case spv::OpBitwiseXor:
	case spv::OpShiftLeftLogical:
	case spv::OpShiftRightLogical:
	case spv::OpShiftRightArithmetic:
	case spv::OpIAdd:
	case spv::OpISub:
	case spv::OpIMul:
	case spv::OpSDiv:
	case spv::OpUDiv:
	case spv::OpSRem:
	case spv::OpUMod:
	case spv::OpSMod:
		return true;

	default:
		return false;
	}
}

struct SetBindingPair
{
	uint32_t desc_set;
	uint32_t binding;

	inline bool operator==(const SetBindingPair &other) const
	{
		return desc_set == other.desc_set && binding == other.binding;
	}

	inline bool operator<(const SetBindingPair &other) const
	{
		return desc_set < other.desc_set || (desc_set == other.desc_set && binding < other.binding);
	}
};

struct LocationComponentPair
{
	uint32_t location;
	uint32_t component;

	inline bool operator==(const LocationComponentPair &other) const
	{
		return location == other.location && component == other.component;
	}

	inline bool operator<(const LocationComponentPair &other) const
	{
		return location < other.location || (location == other.location && component < other.component);
	}
};

struct StageSetBinding
{
	spv::ExecutionModel model;
	uint32_t desc_set;
	uint32_t binding;

	inline bool operator==(const StageSetBinding &other) const
	{
		return model == other.model && desc_set == other.desc_set && binding == other.binding;
	}
};

struct InternalHasher
{
	inline size_t operator()(const SetBindingPair &value) const
	{
		// Quality of hash doesn't really matter here.
		auto hash_set = std::hash<uint32_t>()(value.desc_set);
		auto hash_binding = std::hash<uint32_t>()(value.binding);
		return (hash_set * 0x10001b31) ^ hash_binding;
	}

	inline size_t operator()(const LocationComponentPair &value) const
	{
		// Quality of hash doesn't really matter here.
		auto hash_set = std::hash<uint32_t>()(value.location);
		auto hash_binding = std::hash<uint32_t>()(value.component);
		return (hash_set * 0x10001b31) ^ hash_binding;
	}

	inline size_t operator()(const StageSetBinding &value) const
	{
		// Quality of hash doesn't really matter here.
		auto hash_model = std::hash<uint32_t>()(value.model);
		auto hash_set = std::hash<uint32_t>()(value.desc_set);
		auto tmp_hash = (hash_model * 0x10001b31) ^ hash_set;
		return (tmp_hash * 0x10001b31) ^ value.binding;
	}
};

// Special constant used in a {MSL,HLSL}ResourceBinding desc_set
// element to indicate the bindings for the push constants.
static const uint32_t ResourceBindingPushConstantDescriptorSet = ~(0u);

// Special constant used in a {MSL,HLSL}ResourceBinding binding
// element to indicate the bindings for the push constants.
static const uint32_t ResourceBindingPushConstantBinding = 0;
} // namespace SPIRV_CROSS_NAMESPACE

namespace std
{
template <SPIRV_CROSS_NAMESPACE::Types type>
struct hash<SPIRV_CROSS_NAMESPACE::TypedID<type>>
{
	size_t operator()(const SPIRV_CROSS_NAMESPACE::TypedID<type> &value) const
	{
		return std::hash<uint32_t>()(value);
	}
};
} // namespace std

#endif
