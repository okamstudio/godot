//
//  m3_compile.h
//
//  Created by Steven Massey on 4/17/19.
//  Copyright © 2019 Steven Massey. All rights reserved.
//

#ifndef m3_compile_h
#define m3_compile_h

#include "m3_code.h"
#include "m3_exec_defs.h"
#include "m3_function.h"

d_m3BeginExternC

enum
{
    c_waOp_block                = 0x02,
    c_waOp_loop                 = 0x03,
    c_waOp_if                   = 0x04,
    c_waOp_else                 = 0x05,
    c_waOp_end                  = 0x0b,
    c_waOp_branch               = 0x0c,
    c_waOp_branchTable          = 0x0e,
    c_waOp_branchIf             = 0x0d,
    c_waOp_call                 = 0x10,
    c_waOp_getLocal             = 0x20,
    c_waOp_setLocal             = 0x21,
    c_waOp_teeLocal             = 0x22,

    c_waOp_getGlobal            = 0x23,

    c_waOp_store_f32            = 0x38,
    c_waOp_store_f64            = 0x39,

    c_waOp_i32_const            = 0x41,
    c_waOp_i64_const            = 0x42,
    c_waOp_f32_const            = 0x43,
    c_waOp_f64_const            = 0x44,

    c_waOp_extended             = 0xfc,

    c_waOp_memoryCopy           = 0xfc0a,
    c_waOp_memoryFill           = 0xfc0b
};


#define d_FuncRetType(ftype,i)  ((ftype)->types[(i)])
#define d_FuncArgType(ftype,i)  ((ftype)->types[(ftype)->numRets + (i)])

//-----------------------------------------------------------------------------------------------------------------------------------

typedef struct M3CompilationScope
{
    struct M3CompilationScope *     outer;

    pc_t                            pc;                 // used by ContinueLoop's
    pc_t                            patches;
    i32                             depth;
    u16                             exitStackIndex;
    u16                             blockStackIndex;
//    u16                             topSlot;
    IM3FuncType                     type;
    m3opcode_t                      opcode;
    bool                            isPolymorphic;
}
M3CompilationScope;

typedef M3CompilationScope *        IM3CompilationScope;

typedef struct
{
    IM3Runtime          runtime;
    IM3Module           module;

    bytes_t             wasm;
    bytes_t             wasmEnd;
    bytes_t             lastOpcodeStart;

    M3CompilationScope  block;

    IM3Function         function;

    IM3CodePage         page;

#ifdef DEBUG
    u32                 numEmits;
    u32                 numOpcodes;
#endif

    u16                 stackFirstDynamicIndex;     // args and locals are pushed to the stack so that their slot locations can be tracked. the wasm model itself doesn't
                                                    // treat these values as being on the stack, so stackFirstDynamicIndex marks the start of the real Wasm stack
    u16                 stackIndex;                 // current stack top

    u16                 slotFirstConstIndex;
    u16                 slotMaxConstIndex;          // as const's are encountered during compilation this tracks their location in the "real" stack

    u16                 slotFirstLocalIndex;
    u16                 slotFirstDynamicIndex;      // numArgs + numLocals + numReservedConstants. the first mutable slot available to the compiler.

    u16                 maxStackSlots;

    m3slot_t            constants                   [d_m3MaxConstantTableSize];

    // 'wasmStack' holds slot locations
    u16                 wasmStack                   [d_m3MaxFunctionStackHeight];
    u8                  typeStack                   [d_m3MaxFunctionStackHeight];

    // 'm3Slots' contains allocation usage counts
    u8                  m3Slots                     [d_m3MaxFunctionSlots];

    u16                 slotMaxAllocatedIndexPlusOne;

    u16                 regStackIndexPlusOne        [2];

    m3opcode_t          previousOpcode;
}
M3Compilation;

typedef M3Compilation *                 IM3Compilation;

typedef M3Result (* M3Compiler)         (IM3Compilation, m3opcode_t);


//-----------------------------------------------------------------------------------------------------------------------------------


typedef struct M3OpInfo
{
#ifdef DEBUG
    const char * const      name;
#endif

    i8                      stackOffset;
    u8                      type;

    // for most operations:
    // [0]= top operand in register, [1]= top operand in stack, [2]= both operands in stack
    IM3Operation            operations [4];

    M3Compiler              compiler;
}
M3OpInfo;

typedef const M3OpInfo *    IM3OpInfo;

IM3OpInfo  GetOpInfo  (m3opcode_t opcode);

// TODO: This helper should be removed, when MultiValue is implemented
static inline
u8 GetSingleRetType(IM3FuncType ftype) {
    return (ftype && ftype->numRets) ? ftype->types[0] : (u8)c_m3Type_none;
}

static const u16 c_m3RegisterUnallocated = 0;
static const u16 c_slotUnused = 0xffff;

static inline
bool  IsRegisterAllocated  (IM3Compilation o, u32 i_register)
{
    return (o->regStackIndexPlusOne [i_register] != c_m3RegisterUnallocated);
}

static inline
bool  IsStackPolymorphic  (IM3Compilation o)
{
    return o->block.isPolymorphic;
}

static inline bool  IsRegisterSlotAlias        (u16 i_slot)    { return (i_slot >= d_m3Reg0SlotAlias and i_slot != c_slotUnused); }
static inline bool  IsFpRegisterSlotAlias      (u16 i_slot)    { return (i_slot == d_m3Fp0SlotAlias);  }
static inline bool  IsIntRegisterSlotAlias     (u16 i_slot)    { return (i_slot == d_m3Reg0SlotAlias); }


#ifdef DEBUG
    #define M3OP(...)       { __VA_ARGS__ }
    #define M3OP_RESERVED   { "reserved" }
#else
    // Strip-off name
    #define M3OP(name, ...) { __VA_ARGS__ }
    #define M3OP_RESERVED   { 0 }
#endif

#if d_m3HasFloat
    #define M3OP_F          M3OP
#elif d_m3NoFloatDynamic
    #define M3OP_F(n,o,t,op,...)        M3OP(n, o, t, { op_Unsupported, op_Unsupported, op_Unsupported, op_Unsupported }, __VA_ARGS__)
#else
    #define M3OP_F(...)     { 0 }
#endif

//-----------------------------------------------------------------------------------------------------------------------------------

u16         GetMaxUsedSlotPlusOne       (IM3Compilation o);

M3Result    CompileBlock                (IM3Compilation io, IM3FuncType i_blockType, m3opcode_t i_blockOpcode);

M3Result    CompileBlockStatements      (IM3Compilation io);
M3Result    CompileFunction             (IM3Function io_function);

M3Result    CompileRawFunction          (IM3Module io_module, IM3Function io_function, const void * i_function, const void * i_userdata);

d_m3EndExternC

#endif // m3_compile_h
