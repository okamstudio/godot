//
// Copyright (C) 2014-2015 LunarG, Inc.
// Copyright (C) 2015-2020 Google, Inc.
// Copyright (C) 2017 ARM Limited.
// Modifications Copyright (C) 2020 Advanced Micro Devices, Inc. All rights reserved.
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
// FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
// COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
// INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
// BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
// ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

//
// "Builder" is an interface to fully build SPIR-V IR.   Allocate one of
// these to build (a thread safe) internal SPIR-V representation (IR),
// and then dump it as a binary stream according to the SPIR-V specification.
//
// A Builder has a 1:1 relationship with a SPIR-V module.
//

#pragma once
#ifndef SpvBuilder_H
#define SpvBuilder_H

#include "Logger.h"
#include "spirv.hpp"
#include "spvIR.h"
namespace spv {
    #include "GLSL.ext.KHR.h"
    #include "NonSemanticShaderDebugInfo100.h"
}

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stack>
#include <unordered_map>
#include <map>

namespace spv {

typedef enum {
    Spv_1_0 = (1 << 16),
    Spv_1_1 = (1 << 16) | (1 << 8),
    Spv_1_2 = (1 << 16) | (2 << 8),
    Spv_1_3 = (1 << 16) | (3 << 8),
    Spv_1_4 = (1 << 16) | (4 << 8),
    Spv_1_5 = (1 << 16) | (5 << 8),
} SpvVersion;

class Builder {
public:
    Builder(unsigned int spvVersion, unsigned int userNumber, SpvBuildLogger* logger);
    virtual ~Builder();

    static const int maxMatrixSize = 4;

    unsigned int getSpvVersion() const { return spvVersion; }

    void setSource(spv::SourceLanguage lang, int version)
    {
        sourceLang = lang;
        sourceVersion = version;
    }
    spv::Id getStringId(const std::string& str)
    {
        auto sItr = stringIds.find(str);
        if (sItr != stringIds.end())
            return sItr->second;
        spv::Id strId = getUniqueId();
        Instruction* fileString = new Instruction(strId, NoType, OpString);
        const char* file_c_str = str.c_str();
        fileString->addStringOperand(file_c_str);
        strings.push_back(std::unique_ptr<Instruction>(fileString));
        module.mapInstruction(fileString);
        stringIds[file_c_str] = strId;
        return strId;
    }

    spv::Id getMainFileId() const { return mainFileId; }

    // Initialize the main source file name
    void setDebugSourceFile(const std::string& file)
    {
        if (trackDebugInfo) {
            dirtyLineTracker = true;
            mainFileId = getStringId(file);
            currentFileId = mainFileId;
        }
    }

    // Set the debug source location tracker in the builder.
    // The upcoming instructions in basic blocks will be associated to this location.
    void setDebugSourceLocation(int line, const char* filename)
    {
        if (trackDebugInfo) {
            dirtyLineTracker = true;
            if (line != 0) {
                // TODO: This is special handling of some AST nodes having (untracked) line 0. 
                //       But they should have a valid line number.
                currentLine = line;
                if (filename) {
                    currentFileId = getStringId(filename);
                }
            }
        }
    }

    void setSourceText(const std::string& text) { sourceText = text; }
    void addSourceExtension(const char* ext) { sourceExtensions.push_back(ext); }
    void addModuleProcessed(const std::string& p) { moduleProcesses.push_back(p.c_str()); }
    void setEmitSpirvDebugInfo()
    {
        trackDebugInfo = true;
        emitSpirvDebugInfo = true;
    }
    void setEmitNonSemanticShaderDebugInfo(bool emitSourceText)
    {
        trackDebugInfo = true;
        emitNonSemanticShaderDebugInfo = true;
        importNonSemanticShaderDebugInfoInstructions();

        if (emitSourceText) {
            emitNonSemanticShaderDebugSource = emitSourceText;
        }
    }
    void addExtension(const char* ext) { extensions.insert(ext); }
    void removeExtension(const char* ext)
    {
        extensions.erase(ext);
    }
    void addIncorporatedExtension(const char* ext, SpvVersion incorporatedVersion)
    {
        if (getSpvVersion() < static_cast<unsigned>(incorporatedVersion))
            addExtension(ext);
    }
    void promoteIncorporatedExtension(const char* baseExt, const char* promoExt, SpvVersion incorporatedVersion)
    {
        removeExtension(baseExt);
        addIncorporatedExtension(promoExt, incorporatedVersion);
    }
    void addInclude(const std::string& name, const std::string& text)
    {
        spv::Id incId = getStringId(name);
        includeFiles[incId] = &text;
    }
    Id import(const char*);
    void setMemoryModel(spv::AddressingModel addr, spv::MemoryModel mem)
    {
        addressModel = addr;
        memoryModel = mem;
    }

    void addCapability(spv::Capability cap) { capabilities.insert(cap); }

    // To get a new <id> for anything needing a new one.
    Id getUniqueId() { return ++uniqueId; }

    // To get a set of new <id>s, e.g., for a set of function parameters
    Id getUniqueIds(int numIds)
    {
        Id id = uniqueId + 1;
        uniqueId += numIds;
        return id;
    }

    // For creating new types (will return old type if the requested one was already made).
    Id makeVoidType();
    Id makeBoolType();
    Id makePointer(StorageClass, Id pointee);
    Id makeForwardPointer(StorageClass);
    Id makePointerFromForwardPointer(StorageClass, Id forwardPointerType, Id pointee);
    Id makeIntegerType(int width, bool hasSign);   // generic
    Id makeIntType(int width) { return makeIntegerType(width, true); }
    Id makeUintType(int width) { return makeIntegerType(width, false); }
    Id makeFloatType(int width);
    Id makeStructType(const std::vector<Id>& members, const char* name, bool const compilerGenerated = true);
    Id makeStructResultType(Id type0, Id type1);
    Id makeVectorType(Id component, int size);
    Id makeMatrixType(Id component, int cols, int rows);
    Id makeArrayType(Id element, Id sizeId, int stride);  // 0 stride means no stride decoration
    Id makeRuntimeArray(Id element);
    Id makeFunctionType(Id returnType, const std::vector<Id>& paramTypes);
    Id makeImageType(Id sampledType, Dim, bool depth, bool arrayed, bool ms, unsigned sampled, ImageFormat format);
    Id makeSamplerType();
    Id makeSampledImageType(Id imageType);
    Id makeCooperativeMatrixTypeKHR(Id component, Id scope, Id rows, Id cols, Id use);
    Id makeCooperativeMatrixTypeNV(Id component, Id scope, Id rows, Id cols);
    Id makeCooperativeMatrixTypeWithSameShape(Id component, Id otherType);
    Id makeGenericType(spv::Op opcode, std::vector<spv::IdImmediate>& operands);

    // SPIR-V NonSemantic Shader DebugInfo Instructions
    struct DebugTypeLoc {
        std::string name {};
        int line {0};
        int column {0};
    };
    std::unordered_map<Id, DebugTypeLoc> debugTypeLocs;
    Id makeDebugInfoNone();
    Id makeBoolDebugType(int const size);
    Id makeIntegerDebugType(int const width, bool const hasSign);
    Id makeFloatDebugType(int const width);
    Id makeSequentialDebugType(Id const baseType, Id const componentCount, NonSemanticShaderDebugInfo100Instructions const sequenceType);
    Id makeArrayDebugType(Id const baseType, Id const componentCount);
    Id makeVectorDebugType(Id const baseType, int const componentCount);
    Id makeMatrixDebugType(Id const vectorType, int const vectorCount, bool columnMajor = true);
    Id makeMemberDebugType(Id const memberType, DebugTypeLoc const& debugTypeLoc);
    Id makeCompositeDebugType(std::vector<Id> const& memberTypes, char const*const name,
        NonSemanticShaderDebugInfo100DebugCompositeType const tag, bool const isOpaqueType = false);
    Id makePointerDebugType(StorageClass storageClass, Id const baseType);
    Id makeDebugSource(const Id fileName);
    Id makeDebugCompilationUnit();
    Id createDebugGlobalVariable(Id const type, char const*const name, Id const variable);
    Id createDebugLocalVariable(Id type, char const*const name, size_t const argNumber = 0);
    Id makeDebugExpression();
    Id makeDebugDeclare(Id const debugLocalVariable, Id const pointer);
    Id makeDebugValue(Id const debugLocalVariable, Id const value);
    Id makeDebugFunctionType(Id returnType, const std::vector<Id>& paramTypes);
    Id makeDebugFunction(Function* function, Id nameId, Id funcTypeId);
    Id makeDebugLexicalBlock(uint32_t line);
    std::string unmangleFunctionName(std::string const& name) const;
    void setupDebugFunctionEntry(Function* function, const char* name, int line, 
                                 const std::vector<Id>& paramTypes,
                                 const std::vector<char const*>& paramNames);

    // accelerationStructureNV type
    Id makeAccelerationStructureType();
    // rayQueryEXT type
    Id makeRayQueryType();
    // hitObjectNV type
    Id makeHitObjectNVType();

    // For querying about types.
    Id getTypeId(Id resultId) const { return module.getTypeId(resultId); }
    Id getDerefTypeId(Id resultId) const;
    Op getOpCode(Id id) const { return module.getInstruction(id)->getOpCode(); }
    Op getTypeClass(Id typeId) const { return getOpCode(typeId); }
    Op getMostBasicTypeClass(Id typeId) const;
    int getNumComponents(Id resultId) const { return getNumTypeComponents(getTypeId(resultId)); }
    int getNumTypeConstituents(Id typeId) const;
    int getNumTypeComponents(Id typeId) const { return getNumTypeConstituents(typeId); }
    Id getScalarTypeId(Id typeId) const;
    Id getContainedTypeId(Id typeId) const;
    Id getContainedTypeId(Id typeId, int) const;
    StorageClass getTypeStorageClass(Id typeId) const { return module.getStorageClass(typeId); }
    ImageFormat getImageTypeFormat(Id typeId) const
        { return (ImageFormat)module.getInstruction(typeId)->getImmediateOperand(6); }
    Id getResultingAccessChainType() const;
    Id getIdOperand(Id resultId, int idx) { return module.getInstruction(resultId)->getIdOperand(idx); }

    bool isPointer(Id resultId)      const { return isPointerType(getTypeId(resultId)); }
    bool isScalar(Id resultId)       const { return isScalarType(getTypeId(resultId)); }
    bool isVector(Id resultId)       const { return isVectorType(getTypeId(resultId)); }
    bool isMatrix(Id resultId)       const { return isMatrixType(getTypeId(resultId)); }
    bool isCooperativeMatrix(Id resultId)const { return isCooperativeMatrixType(getTypeId(resultId)); }
    bool isAggregate(Id resultId)    const { return isAggregateType(getTypeId(resultId)); }
    bool isSampledImage(Id resultId) const { return isSampledImageType(getTypeId(resultId)); }

    bool isBoolType(Id typeId)
        { return groupedTypes[OpTypeBool].size() > 0 && typeId == groupedTypes[OpTypeBool].back()->getResultId(); }
    bool isIntType(Id typeId)          const
        { return getTypeClass(typeId) == OpTypeInt && module.getInstruction(typeId)->getImmediateOperand(1) != 0; }
    bool isUintType(Id typeId)         const
        { return getTypeClass(typeId) == OpTypeInt && module.getInstruction(typeId)->getImmediateOperand(1) == 0; }
    bool isFloatType(Id typeId)        const { return getTypeClass(typeId) == OpTypeFloat; }
    bool isPointerType(Id typeId)      const { return getTypeClass(typeId) == OpTypePointer; }
    bool isScalarType(Id typeId)       const
        { return getTypeClass(typeId) == OpTypeFloat || getTypeClass(typeId) == OpTypeInt ||
          getTypeClass(typeId) == OpTypeBool; }
    bool isVectorType(Id typeId)       const { return getTypeClass(typeId) == OpTypeVector; }
    bool isMatrixType(Id typeId)       const { return getTypeClass(typeId) == OpTypeMatrix; }
    bool isStructType(Id typeId)       const { return getTypeClass(typeId) == OpTypeStruct; }
    bool isArrayType(Id typeId)        const { return getTypeClass(typeId) == OpTypeArray; }
    bool isCooperativeMatrixType(Id typeId)const
    {
        return getTypeClass(typeId) == OpTypeCooperativeMatrixKHR || getTypeClass(typeId) == OpTypeCooperativeMatrixNV;
    }
    bool isAggregateType(Id typeId)    const
        { return isArrayType(typeId) || isStructType(typeId) || isCooperativeMatrixType(typeId); }
    bool isImageType(Id typeId)        const { return getTypeClass(typeId) == OpTypeImage; }
    bool isSamplerType(Id typeId)      const { return getTypeClass(typeId) == OpTypeSampler; }
    bool isSampledImageType(Id typeId) const { return getTypeClass(typeId) == OpTypeSampledImage; }
    bool containsType(Id typeId, Op typeOp, unsigned int width) const;
    bool containsPhysicalStorageBufferOrArray(Id typeId) const;

    bool isConstantOpCode(Op opcode) const;
    bool isSpecConstantOpCode(Op opcode) const;
    bool isConstant(Id resultId) const { return isConstantOpCode(getOpCode(resultId)); }
    bool isConstantScalar(Id resultId) const { return getOpCode(resultId) == OpConstant; }
    bool isSpecConstant(Id resultId) const { return isSpecConstantOpCode(getOpCode(resultId)); }
    unsigned int getConstantScalar(Id resultId) const
        { return module.getInstruction(resultId)->getImmediateOperand(0); }
    StorageClass getStorageClass(Id resultId) const { return getTypeStorageClass(getTypeId(resultId)); }

    bool isVariableOpCode(Op opcode) const { return opcode == OpVariable; }
    bool isVariable(Id resultId) const { return isVariableOpCode(getOpCode(resultId)); }
    bool isGlobalStorage(Id resultId) const { return getStorageClass(resultId) != StorageClassFunction; }
    bool isGlobalVariable(Id resultId) const { return isVariable(resultId) && isGlobalStorage(resultId); }
    // See if a resultId is valid for use as an initializer.
    bool isValidInitializer(Id resultId) const { return isConstant(resultId) || isGlobalVariable(resultId); }

    int getScalarTypeWidth(Id typeId) const
    {
        Id scalarTypeId = getScalarTypeId(typeId);
        assert(getTypeClass(scalarTypeId) == OpTypeInt || getTypeClass(scalarTypeId) == OpTypeFloat);
        return module.getInstruction(scalarTypeId)->getImmediateOperand(0);
    }

    int getTypeNumColumns(Id typeId) const
    {
        assert(isMatrixType(typeId));
        return getNumTypeConstituents(typeId);
    }
    int getNumColumns(Id resultId) const { return getTypeNumColumns(getTypeId(resultId)); }
    int getTypeNumRows(Id typeId) const
    {
        assert(isMatrixType(typeId));
        return getNumTypeComponents(getContainedTypeId(typeId));
    }
    int getNumRows(Id resultId) const { return getTypeNumRows(getTypeId(resultId)); }

    Dim getTypeDimensionality(Id typeId) const
    {
        assert(isImageType(typeId));
        return (Dim)module.getInstruction(typeId)->getImmediateOperand(1);
    }
    Id getImageType(Id resultId) const
    {
        Id typeId = getTypeId(resultId);
        assert(isImageType(typeId) || isSampledImageType(typeId));
        return isSampledImageType(typeId) ? module.getInstruction(typeId)->getIdOperand(0) : typeId;
    }
    bool isArrayedImageType(Id typeId) const
    {
        assert(isImageType(typeId));
        return module.getInstruction(typeId)->getImmediateOperand(3) != 0;
    }

    // For making new constants (will return old constant if the requested one was already made).
    Id makeNullConstant(Id typeId);
    Id makeBoolConstant(bool b, bool specConstant = false);
    Id makeInt8Constant(int i, bool specConstant = false)
        { return makeIntConstant(makeIntType(8),  (unsigned)i, specConstant); }
    Id makeUint8Constant(unsigned u, bool specConstant = false)
        { return makeIntConstant(makeUintType(8),           u, specConstant); }
    Id makeInt16Constant(int i, bool specConstant = false)
        { return makeIntConstant(makeIntType(16),  (unsigned)i, specConstant); }
    Id makeUint16Constant(unsigned u, bool specConstant = false)
        { return makeIntConstant(makeUintType(16),           u, specConstant); }
    Id makeIntConstant(int i, bool specConstant = false)
        { return makeIntConstant(makeIntType(32),  (unsigned)i, specConstant); }
    Id makeUintConstant(unsigned u, bool specConstant = false)
        { return makeIntConstant(makeUintType(32),           u, specConstant); }
    Id makeInt64Constant(long long i, bool specConstant = false)
        { return makeInt64Constant(makeIntType(64),  (unsigned long long)i, specConstant); }
    Id makeUint64Constant(unsigned long long u, bool specConstant = false)
        { return makeInt64Constant(makeUintType(64),                     u, specConstant); }
    Id makeFloatConstant(float f, bool specConstant = false);
    Id makeDoubleConstant(double d, bool specConstant = false);
    Id makeFloat16Constant(float f16, bool specConstant = false);
    Id makeFpConstant(Id type, double d, bool specConstant = false);

    Id importNonSemanticShaderDebugInfoInstructions();

    // Turn the array of constants into a proper spv constant of the requested type.
    Id makeCompositeConstant(Id type, const std::vector<Id>& comps, bool specConst = false);

    // Methods for adding information outside the CFG.
    Instruction* addEntryPoint(ExecutionModel, Function*, const char* name);
    void addExecutionMode(Function*, ExecutionMode mode, int value1 = -1, int value2 = -1, int value3 = -1);
    void addExecutionMode(Function*, ExecutionMode mode, const std::vector<unsigned>& literals);
    void addExecutionModeId(Function*, ExecutionMode mode, const std::vector<Id>& operandIds);
    void addName(Id, const char* name);
    void addMemberName(Id, int member, const char* name);
    void addDecoration(Id, Decoration, int num = -1);
    void addDecoration(Id, Decoration, const char*);
    void addDecoration(Id, Decoration, const std::vector<unsigned>& literals);
    void addDecoration(Id, Decoration, const std::vector<const char*>& strings);
    void addLinkageDecoration(Id id, const char* name, spv::LinkageType linkType);
    void addDecorationId(Id id, Decoration, Id idDecoration);
    void addDecorationId(Id id, Decoration, const std::vector<Id>& operandIds);
    void addMemberDecoration(Id, unsigned int member, Decoration, int num = -1);
    void addMemberDecoration(Id, unsigned int member, Decoration, const char*);
    void addMemberDecoration(Id, unsigned int member, Decoration, const std::vector<unsigned>& literals);
    void addMemberDecoration(Id, unsigned int member, Decoration, const std::vector<const char*>& strings);

    // At the end of what block do the next create*() instructions go?
    // Also reset current last DebugScope and current source line to unknown
    void setBuildPoint(Block* bp) {
        buildPoint = bp;
        // TODO: Technically, change of build point should set line tracker dirty. But we'll have bad line info for
        //       branch instructions. Commenting this for now because at least this matches the old behavior.
        dirtyScopeTracker = true;
    }
    Block* getBuildPoint() const { return buildPoint; }

    // Append an instruction to the end of the current build point.
    // Optionally, additional debug info instructions may also be prepended.
    void addInstruction(std::unique_ptr<Instruction> inst);

    // Make the entry-point function. The returned pointer is only valid
    // for the lifetime of this builder.
    Function* makeEntryPoint(const char*);

    // Make a shader-style function, and create its entry block if entry is non-zero.
    // Return the function, pass back the entry.
    // The returned pointer is only valid for the lifetime of this builder.
    Function* makeFunctionEntry(Decoration precision, Id returnType, const char* name, LinkageType linkType,
                                const std::vector<Id>& paramTypes,
                                const std::vector<std::vector<Decoration>>& precisions, Block** entry = nullptr);

    // Create a return. An 'implicit' return is one not appearing in the source
    // code.  In the case of an implicit return, no post-return block is inserted.
    void makeReturn(bool implicit, Id retVal = 0);

    // Initialize state and generate instructions for new lexical scope
    void enterLexicalBlock(uint32_t line);

    // Set state and generate instructions to exit current lexical scope
    void leaveLexicalBlock();

    // Prepare builder for generation of instructions for a function.
    void enterFunction(Function const* function);

    // Generate all the code needed to finish up a function.
    void leaveFunction();

    // Create block terminator instruction for certain statements like
    // discard, terminate-invocation, terminateRayEXT, or ignoreIntersectionEXT
    void makeStatementTerminator(spv::Op opcode, const char *name);

    // Create block terminator instruction for statements that have input operands
    // such as OpEmitMeshTasksEXT
    void makeStatementTerminator(spv::Op opcode, const std::vector<Id>& operands, const char* name);

    // Create a global or function local or IO variable.
    Id createVariable(Decoration precision, StorageClass storageClass, Id type, const char* name = nullptr,
        Id initializer = NoResult, bool const compilerGenerated = true);

    // Create an intermediate with an undefined value.
    Id createUndefined(Id type);

    // Store into an Id and return the l-value
    void createStore(Id rValue, Id lValue, spv::MemoryAccessMask memoryAccess = spv::MemoryAccessMaskNone,
        spv::Scope scope = spv::ScopeMax, unsigned int alignment = 0);

    // Load from an Id and return it
    Id createLoad(Id lValue, spv::Decoration precision,
        spv::MemoryAccessMask memoryAccess = spv::MemoryAccessMaskNone,
        spv::Scope scope = spv::ScopeMax, unsigned int alignment = 0);

    // Create an OpAccessChain instruction
    Id createAccessChain(StorageClass, Id base, const std::vector<Id>& offsets);

    // Create an OpArrayLength instruction
    Id createArrayLength(Id base, unsigned int member);

    // Create an OpCooperativeMatrixLengthKHR instruction
    Id createCooperativeMatrixLengthKHR(Id type);
    // Create an OpCooperativeMatrixLengthNV instruction
    Id createCooperativeMatrixLengthNV(Id type);

    // Create an OpCompositeExtract instruction
    Id createCompositeExtract(Id composite, Id typeId, unsigned index);
    Id createCompositeExtract(Id composite, Id typeId, const std::vector<unsigned>& indexes);
    Id createCompositeInsert(Id object, Id composite, Id typeId, unsigned index);
    Id createCompositeInsert(Id object, Id composite, Id typeId, const std::vector<unsigned>& indexes);

    Id createVectorExtractDynamic(Id vector, Id typeId, Id componentIndex);
    Id createVectorInsertDynamic(Id vector, Id typeId, Id component, Id componentIndex);

    void createNoResultOp(Op);
    void createNoResultOp(Op, Id operand);
    void createNoResultOp(Op, const std::vector<Id>& operands);
    void createNoResultOp(Op, const std::vector<IdImmediate>& operands);
    void createControlBarrier(Scope execution, Scope memory, MemorySemanticsMask);
    void createMemoryBarrier(unsigned executionScope, unsigned memorySemantics);
    Id createUnaryOp(Op, Id typeId, Id operand);
    Id createBinOp(Op, Id typeId, Id operand1, Id operand2);
    Id createTriOp(Op, Id typeId, Id operand1, Id operand2, Id operand3);
    Id createOp(Op, Id typeId, const std::vector<Id>& operands);
    Id createOp(Op, Id typeId, const std::vector<IdImmediate>& operands);
    Id createFunctionCall(spv::Function*, const std::vector<spv::Id>&);
    Id createSpecConstantOp(Op, Id typeId, const std::vector<spv::Id>& operands, const std::vector<unsigned>& literals);

    // Take an rvalue (source) and a set of channels to extract from it to
    // make a new rvalue, which is returned.
    Id createRvalueSwizzle(Decoration precision, Id typeId, Id source, const std::vector<unsigned>& channels);

    // Take a copy of an lvalue (target) and a source of components, and set the
    // source components into the lvalue where the 'channels' say to put them.
    // An updated version of the target is returned.
    // (No true lvalue or stores are used.)
    Id createLvalueSwizzle(Id typeId, Id target, Id source, const std::vector<unsigned>& channels);

    // If both the id and precision are valid, the id
    // gets tagged with the requested precision.
    // The passed in id is always the returned id, to simplify use patterns.
    Id setPrecision(Id id, Decoration precision)
    {
        if (precision != NoPrecision && id != NoResult)
            addDecoration(id, precision);

        return id;
    }

    // Can smear a scalar to a vector for the following forms:
    //   - promoteScalar(scalar, vector)  // smear scalar to width of vector
    //   - promoteScalar(vector, scalar)  // smear scalar to width of vector
    //   - promoteScalar(pointer, scalar) // smear scalar to width of what pointer points to
    //   - promoteScalar(scalar, scalar)  // do nothing
    // Other forms are not allowed.
    //
    // Generally, the type of 'scalar' does not need to be the same type as the components in 'vector'.
    // The type of the created vector is a vector of components of the same type as the scalar.
    //
    // Note: One of the arguments will change, with the result coming back that way rather than
    // through the return value.
    void promoteScalar(Decoration precision, Id& left, Id& right);

    // Make a value by smearing the scalar to fill the type.
    // vectorType should be the correct type for making a vector of scalarVal.
    // (No conversions are done.)
    Id smearScalar(Decoration precision, Id scalarVal, Id vectorType);

    // Create a call to a built-in function.
    Id createBuiltinCall(Id resultType, Id builtins, int entryPoint, const std::vector<Id>& args);

    // List of parameters used to create a texture operation
    struct TextureParameters {
        Id sampler;
        Id coords;
        Id bias;
        Id lod;
        Id Dref;
        Id offset;
        Id offsets;
        Id gradX;
        Id gradY;
        Id sample;
        Id component;
        Id texelOut;
        Id lodClamp;
        Id granularity;
        Id coarse;
        bool nonprivate;
        bool volatil;
    };

    // Select the correct texture operation based on all inputs, and emit the correct instruction
    Id createTextureCall(Decoration precision, Id resultType, bool sparse, bool fetch, bool proj, bool gather,
        bool noImplicit, const TextureParameters&, ImageOperandsMask);

    // Emit the OpTextureQuery* instruction that was passed in.
    // Figure out the right return value and type, and return it.
    Id createTextureQueryCall(Op, const TextureParameters&, bool isUnsignedResult);

    Id createSamplePositionCall(Decoration precision, Id, Id);

    Id createBitFieldExtractCall(Decoration precision, Id, Id, Id, bool isSigned);
    Id createBitFieldInsertCall(Decoration precision, Id, Id, Id, Id);

    // Reduction comparison for composites:  For equal and not-equal resulting in a scalar.
    Id createCompositeCompare(Decoration precision, Id, Id, bool /* true if for equal, false if for not-equal */);

    // OpCompositeConstruct
    Id createCompositeConstruct(Id typeId, const std::vector<Id>& constituents);

    // vector or scalar constructor
    Id createConstructor(Decoration precision, const std::vector<Id>& sources, Id resultTypeId);

    // matrix constructor
    Id createMatrixConstructor(Decoration precision, const std::vector<Id>& sources, Id constructee);

    // Helper to use for building nested control flow with if-then-else.
    class If {
    public:
        If(Id condition, unsigned int ctrl, Builder& builder);
        ~If() {}

        void makeBeginElse();
        void makeEndIf();

    private:
        If(const If&);
        If& operator=(If&);

        Builder& builder;
        Id condition;
        unsigned int control;
        Function* function;
        Block* headerBlock;
        Block* thenBlock;
        Block* elseBlock;
        Block* mergeBlock;
    };

    // Make a switch statement.  A switch has 'numSegments' of pieces of code, not containing
    // any case/default labels, all separated by one or more case/default labels.  Each possible
    // case value v is a jump to the caseValues[v] segment.  The defaultSegment is also in this
    // number space.  How to compute the value is given by 'condition', as in switch(condition).
    //
    // The SPIR-V Builder will maintain the stack of post-switch merge blocks for nested switches.
    //
    // Use a defaultSegment < 0 if there is no default segment (to branch to post switch).
    //
    // Returns the right set of basic blocks to start each code segment with, so that the caller's
    // recursion stack can hold the memory for it.
    //
    void makeSwitch(Id condition, unsigned int control, int numSegments, const std::vector<int>& caseValues,
                    const std::vector<int>& valueToSegment, int defaultSegment, std::vector<Block*>& segmentBB);

    // Add a branch to the innermost switch's merge block.
    void addSwitchBreak();

    // Move to the next code segment, passing in the return argument in makeSwitch()
    void nextSwitchSegment(std::vector<Block*>& segmentBB, int segment);

    // Finish off the innermost switch.
    void endSwitch(std::vector<Block*>& segmentBB);

    struct LoopBlocks {
        LoopBlocks(Block& head, Block& body, Block& merge, Block& continue_target) :
            head(head), body(body), merge(merge), continue_target(continue_target) { }
        Block &head, &body, &merge, &continue_target;
    private:
        LoopBlocks();
        LoopBlocks& operator=(const LoopBlocks&) = delete;
    };

    // Start a new loop and prepare the builder to generate code for it.  Until
    // closeLoop() is called for this loop, createLoopContinue() and
    // createLoopExit() will target its corresponding blocks.
    LoopBlocks& makeNewLoop();

    // Create a new block in the function containing the build point.  Memory is
    // owned by the function object.
    Block& makeNewBlock();

    // Add a branch to the continue_target of the current (innermost) loop.
    void createLoopContinue();

    // Add an exit (e.g. "break") from the innermost loop that we're currently
    // in.
    void createLoopExit();

    // Close the innermost loop that you're in
    void closeLoop();

    //
    // Access chain design for an R-Value vs. L-Value:
    //
    // There is a single access chain the builder is building at
    // any particular time.  Such a chain can be used to either to a load or
    // a store, when desired.
    //
    // Expressions can be r-values, l-values, or both, or only r-values:
    //    a[b.c].d = ....  // l-value
    //    ... = a[b.c].d;  // r-value, that also looks like an l-value
    //    ++a[b.c].d;      // r-value and l-value
    //    (x + y)[2];      // r-value only, can't possibly be l-value
    //
    // Computing an r-value means generating code.  Hence,
    // r-values should only be computed when they are needed, not speculatively.
    //
    // Computing an l-value means saving away information for later use in the compiler,
    // no code is generated until the l-value is later dereferenced.  It is okay
    // to speculatively generate an l-value, just not okay to speculatively dereference it.
    //
    // The base of the access chain (the left-most variable or expression
    // from which everything is based) can be set either as an l-value
    // or as an r-value.  Most efficient would be to set an l-value if one
    // is available.  If an expression was evaluated, the resulting r-value
    // can be set as the chain base.
    //
    // The users of this single access chain can save and restore if they
    // want to nest or manage multiple chains.
    //

    struct AccessChain {
        Id base;                       // for l-values, pointer to the base object, for r-values, the base object
        std::vector<Id> indexChain;
        Id instr;                      // cache the instruction that generates this access chain
        std::vector<unsigned> swizzle; // each std::vector element selects the next GLSL component number
        Id component;                  // a dynamic component index, can coexist with a swizzle,
                                       // done after the swizzle, NoResult if not present
        Id preSwizzleBaseType;         // dereferenced type, before swizzle or component is applied;
                                       // NoType unless a swizzle or component is present
        bool isRValue;                 // true if 'base' is an r-value, otherwise, base is an l-value
        unsigned int alignment;        // bitwise OR of alignment values passed in. Accumulates worst alignment.
                                       // Only tracks base and (optional) component selection alignment.

        // Accumulate whether anything in the chain of structures has coherent decorations.
        struct CoherentFlags {
            CoherentFlags() { clear(); }
            bool isVolatile() const { return volatil; }
            bool isNonUniform() const { return nonUniform; }
            bool anyCoherent() const {
                return coherent || devicecoherent || queuefamilycoherent || workgroupcoherent ||
                    subgroupcoherent || shadercallcoherent;
            }

            unsigned coherent : 1;
            unsigned devicecoherent : 1;
            unsigned queuefamilycoherent : 1;
            unsigned workgroupcoherent : 1;
            unsigned subgroupcoherent : 1;
            unsigned shadercallcoherent : 1;
            unsigned nonprivate : 1;
            unsigned volatil : 1;
            unsigned isImage : 1;
            unsigned nonUniform : 1;

            void clear() {
                coherent = 0;
                devicecoherent = 0;
                queuefamilycoherent = 0;
                workgroupcoherent = 0;
                subgroupcoherent = 0;
                shadercallcoherent = 0;
                nonprivate = 0;
                volatil = 0;
                isImage = 0;
                nonUniform = 0;
            }

            CoherentFlags operator |=(const CoherentFlags &other) {
                coherent |= other.coherent;
                devicecoherent |= other.devicecoherent;
                queuefamilycoherent |= other.queuefamilycoherent;
                workgroupcoherent |= other.workgroupcoherent;
                subgroupcoherent |= other.subgroupcoherent;
                shadercallcoherent |= other.shadercallcoherent;
                nonprivate |= other.nonprivate;
                volatil |= other.volatil;
                isImage |= other.isImage;
                nonUniform |= other.nonUniform;
                return *this;
            }
        };
        CoherentFlags coherentFlags;
    };

    //
    // the SPIR-V builder maintains a single active chain that
    // the following methods operate on
    //

    // for external save and restore
    AccessChain getAccessChain() { return accessChain; }
    void setAccessChain(AccessChain newChain) { accessChain = newChain; }

    // clear accessChain
    void clearAccessChain();

    // set new base as an l-value base
    void setAccessChainLValue(Id lValue)
    {
        assert(isPointer(lValue));
        accessChain.base = lValue;
    }

    // set new base value as an r-value
    void setAccessChainRValue(Id rValue)
    {
        accessChain.isRValue = true;
        accessChain.base = rValue;
    }

    // push offset onto the end of the chain
    void accessChainPush(Id offset, AccessChain::CoherentFlags coherentFlags, unsigned int alignment)
    {
        accessChain.indexChain.push_back(offset);
        accessChain.coherentFlags |= coherentFlags;
        accessChain.alignment |= alignment;
    }

    // push new swizzle onto the end of any existing swizzle, merging into a single swizzle
    void accessChainPushSwizzle(std::vector<unsigned>& swizzle, Id preSwizzleBaseType,
        AccessChain::CoherentFlags coherentFlags, unsigned int alignment);

    // push a dynamic component selection onto the access chain, only applicable with a
    // non-trivial swizzle or no swizzle
    void accessChainPushComponent(Id component, Id preSwizzleBaseType, AccessChain::CoherentFlags coherentFlags,
        unsigned int alignment)
    {
        if (accessChain.swizzle.size() != 1) {
            accessChain.component = component;
            if (accessChain.preSwizzleBaseType == NoType)
                accessChain.preSwizzleBaseType = preSwizzleBaseType;
        }
        accessChain.coherentFlags |= coherentFlags;
        accessChain.alignment |= alignment;
    }

    // use accessChain and swizzle to store value
    void accessChainStore(Id rvalue, Decoration nonUniform,
        spv::MemoryAccessMask memoryAccess = spv::MemoryAccessMaskNone,
        spv::Scope scope = spv::ScopeMax, unsigned int alignment = 0);

    // use accessChain and swizzle to load an r-value
    Id accessChainLoad(Decoration precision, Decoration l_nonUniform, Decoration r_nonUniform, Id ResultType,
        spv::MemoryAccessMask memoryAccess = spv::MemoryAccessMaskNone, spv::Scope scope = spv::ScopeMax,
            unsigned int alignment = 0);

    // Return whether or not the access chain can be represented in SPIR-V
    // as an l-value.
    // E.g., a[3].yx cannot be, while a[3].y and a[3].y[x] can be.
    bool isSpvLvalue() const { return accessChain.swizzle.size() <= 1; }

    // get the direct pointer for an l-value
    Id accessChainGetLValue();

    // Get the inferred SPIR-V type of the result of the current access chain,
    // based on the type of the base and the chain of dereferences.
    Id accessChainGetInferredType();

    // Add capabilities, extensions, remove unneeded decorations, etc.,
    // based on the resulting SPIR-V.
    void postProcess(bool compileOnly);

    // Prune unreachable blocks in the CFG and remove unneeded decorations.
    void postProcessCFG();

    // Add capabilities, extensions based on instructions in the module.
    void postProcessFeatures();
    // Hook to visit each instruction in a block in a function
    void postProcess(Instruction&);
    // Hook to visit each non-32-bit sized float/int operation in a block.
    void postProcessType(const Instruction&, spv::Id typeId);

    void dump(std::vector<unsigned int>&) const;

    void createBranch(Block* block);
    void createConditionalBranch(Id condition, Block* thenBlock, Block* elseBlock);
    void createLoopMerge(Block* mergeBlock, Block* continueBlock, unsigned int control,
        const std::vector<unsigned int>& operands);

    // Sets to generate opcode for specialization constants.
    void setToSpecConstCodeGenMode() { generatingOpCodeForSpecConst = true; }
    // Sets to generate opcode for non-specialization constants (normal mode).
    void setToNormalCodeGenMode() { generatingOpCodeForSpecConst = false; }
    // Check if the builder is generating code for spec constants.
    bool isInSpecConstCodeGenMode() { return generatingOpCodeForSpecConst; }

 protected:
    Id makeIntConstant(Id typeId, unsigned value, bool specConstant);
    Id makeInt64Constant(Id typeId, unsigned long long value, bool specConstant);
    Id findScalarConstant(Op typeClass, Op opcode, Id typeId, unsigned value);
    Id findScalarConstant(Op typeClass, Op opcode, Id typeId, unsigned v1, unsigned v2);
    Id findCompositeConstant(Op typeClass, Id typeId, const std::vector<Id>& comps);
    Id findStructConstant(Id typeId, const std::vector<Id>& comps);
    Id collapseAccessChain();
    void remapDynamicSwizzle();
    void transferAccessChainSwizzle(bool dynamic);
    void simplifyAccessChainSwizzle();
    void createAndSetNoPredecessorBlock(const char*);
    void createSelectionMerge(Block* mergeBlock, unsigned int control);
    void dumpSourceInstructions(std::vector<unsigned int>&) const;
    void dumpSourceInstructions(const spv::Id fileId, const std::string& text, std::vector<unsigned int>&) const;
    void dumpInstructions(std::vector<unsigned int>&, const std::vector<std::unique_ptr<Instruction> >&) const;
    void dumpModuleProcesses(std::vector<unsigned int>&) const;
    spv::MemoryAccessMask sanitizeMemoryAccessForStorageClass(spv::MemoryAccessMask memoryAccess, StorageClass sc)
        const;

    unsigned int spvVersion;     // the version of SPIR-V to emit in the header
    SourceLanguage sourceLang;
    int sourceVersion;
    spv::Id nonSemanticShaderCompilationUnitId {0};
    spv::Id nonSemanticShaderDebugInfo {0};
    spv::Id debugInfoNone {0};
    spv::Id debugExpression {0}; // Debug expression with zero operations.
    std::string sourceText;

    // True if an new OpLine/OpDebugLine may need to be inserted. Either:
    // 1. The current debug location changed
    // 2. The current build point changed
    bool dirtyLineTracker;
    int currentLine = 0;
    // OpString id of the current file name. Always 0 if debug info is off.
    spv::Id currentFileId = 0;
    // OpString id of the main file name. Always 0 if debug info is off.
    spv::Id mainFileId = 0;

    // True if an new OpDebugScope may need to be inserted. Either:
    // 1. A new lexical block is pushed
    // 2. The current build point changed
    bool dirtyScopeTracker;
    std::stack<spv::Id> currentDebugScopeId;

    // This flag toggles tracking of debug info while building the SPIR-V.
    bool trackDebugInfo = false;
    // This flag toggles emission of SPIR-V debug instructions, like OpLine and OpSource.
    bool emitSpirvDebugInfo = false;
    // This flag toggles emission of Non-Semantic Debug extension debug instructions.
    bool emitNonSemanticShaderDebugInfo = false;
    bool restoreNonSemanticShaderDebugInfo = false;
    bool emitNonSemanticShaderDebugSource = false;

    std::set<std::string> extensions;
    std::vector<const char*> sourceExtensions;
    std::vector<const char*> moduleProcesses;
    AddressingModel addressModel;
    MemoryModel memoryModel;
    std::set<spv::Capability> capabilities;
    int builderNumber;
    Module module;
    Block* buildPoint;
    Id uniqueId;
    Function* entryPointFunction;
    bool generatingOpCodeForSpecConst;
    AccessChain accessChain;

    // special blocks of instructions for output
    std::vector<std::unique_ptr<Instruction> > strings;
    std::vector<std::unique_ptr<Instruction> > imports;
    std::vector<std::unique_ptr<Instruction> > entryPoints;
    std::vector<std::unique_ptr<Instruction> > executionModes;
    std::vector<std::unique_ptr<Instruction> > names;
    std::vector<std::unique_ptr<Instruction> > decorations;
    std::vector<std::unique_ptr<Instruction> > constantsTypesGlobals;
    std::vector<std::unique_ptr<Instruction> > externals;
    std::vector<std::unique_ptr<Function> > functions;

    // not output, internally used for quick & dirty canonical (unique) creation

    // map type opcodes to constant inst.
    std::unordered_map<unsigned int, std::vector<Instruction*>> groupedConstants;
    // map struct-id to constant instructions
    std::unordered_map<unsigned int, std::vector<Instruction*>> groupedStructConstants;
    // map type opcodes to type instructions
    std::unordered_map<unsigned int, std::vector<Instruction*>> groupedTypes;
    // map type opcodes to debug type instructions
    std::unordered_map<unsigned int, std::vector<Instruction*>> groupedDebugTypes;
    // list of OpConstantNull instructions
    std::vector<Instruction*> nullConstants;

    // stack of switches
    std::stack<Block*> switchMerges;

    // Our loop stack.
    std::stack<LoopBlocks> loops;

    // map from strings to their string ids
    std::unordered_map<std::string, spv::Id> stringIds;

    // map from include file name ids to their contents
    std::map<spv::Id, const std::string*> includeFiles;

    // map from core id to debug id
    std::map <spv::Id, spv::Id> debugId;

    // map from file name string id to DebugSource id
    std::unordered_map<spv::Id, spv::Id> debugSourceId;

    // The stream for outputting warnings and errors.
    SpvBuildLogger* logger;
};  // end Builder class

};  // end spv namespace

#endif // SpvBuilder_H
