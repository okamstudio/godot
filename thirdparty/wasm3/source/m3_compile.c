//
//  m3_compile.c
//
//  Created by Steven Massey on 4/17/19.
//  Copyright © 2019 Steven Massey. All rights reserved.
//

// Allow using opcodes for compilation process
#define M3_COMPILE_OPCODES

#include "m3_env.h"
#include "m3_compile.h"
#include "m3_exec.h"
#include "m3_exception.h"
#include "m3_info.h"

//----- EMIT --------------------------------------------------------------------------------------------------------------

static inline
pc_t GetPC (IM3Compilation o)
{
    return GetPagePC (o->page);
}

static M3_NOINLINE
M3Result  EnsureCodePageNumLines  (IM3Compilation o, u32 i_numLines)
{
    M3Result result = m3Err_none;

    i_numLines += 2; // room for Bridge

    if (NumFreeLines (o->page) < i_numLines)
    {
        IM3CodePage page = AcquireCodePageWithCapacity (o->runtime, i_numLines);

        if (page)
        {
            m3log (emit, "bridging new code page from: %d %p (free slots: %d) to: %d", o->page->info.sequence, GetPC (o), NumFreeLines (o->page), page->info.sequence);
            d_m3Assert (NumFreeLines (o->page) >= 2);

            EmitWord (o->page, op_Branch);
            EmitWord (o->page, GetPagePC (page));

            ReleaseCodePage (o->runtime, o->page);

            o->page = page;
        }
        else result = m3Err_mallocFailedCodePage;
    }

    return result;
}

static M3_NOINLINE
M3Result  EmitOp  (IM3Compilation o, IM3Operation i_operation)
{
    M3Result result = m3Err_none;                                 d_m3Assert (i_operation or IsStackPolymorphic (o));

    // it's OK for page to be null; when compile-walking the bytecode without emitting
    if (o->page)
    {
# if d_m3EnableOpTracing
        if (i_operation != op_DumpStack)
            o->numEmits++;
# endif

        // have execution jump to a new page if slots are critically low
        result = EnsureCodePageNumLines (o, d_m3CodePageFreeLinesThreshold);

        if (not result)
        {                                                           if (d_m3LogEmit) log_emit (o, i_operation);
# if d_m3RecordBacktraces
            EmitMappingEntry (o->page, o->lastOpcodeStart - o->module->wasmStart);
# endif // d_m3RecordBacktraces
            EmitWord (o->page, i_operation);
        }
    }

    return result;
}

// Push an immediate constant into the M3 codestream
static M3_NOINLINE
void  EmitConstant32  (IM3Compilation o, const u32 i_immediate)
{
    if (o->page)
        EmitWord32 (o->page, i_immediate);
}

static M3_NOINLINE
void  EmitSlotOffset  (IM3Compilation o, const i32 i_offset)
{
    if (o->page)
        EmitWord32 (o->page, i_offset);
}

static M3_NOINLINE
pc_t  EmitPointer  (IM3Compilation o, const void * const i_pointer)
{
    pc_t ptr = GetPagePC (o->page);

    if (o->page)
        EmitWord (o->page, i_pointer);

    return ptr;
}

static M3_NOINLINE
void * ReservePointer (IM3Compilation o)
{
    pc_t ptr = GetPagePC (o->page);
    EmitPointer (o, NULL);
    return (void *) ptr;
}


//-------------------------------------------------------------------------------------------------------------------------

#define d_indent "     | %s"

// just want less letters and numbers to stare at down the way in the compiler table
#define i_32    c_m3Type_i32
#define i_64    c_m3Type_i64
#define f_32    c_m3Type_f32
#define f_64    c_m3Type_f64
#define none    c_m3Type_none
#define any     (u8)-1

#if d_m3HasFloat
#   define FPOP(x) x
#else
#   define FPOP(x) NULL
#endif

static const IM3Operation c_preserveSetSlot [] = { NULL, op_PreserveSetSlot_i32,       op_PreserveSetSlot_i64,
                                                    FPOP(op_PreserveSetSlot_f32), FPOP(op_PreserveSetSlot_f64) };
static const IM3Operation c_setSetOps [] =       { NULL, op_SetSlot_i32,               op_SetSlot_i64,
                                                    FPOP(op_SetSlot_f32),         FPOP(op_SetSlot_f64) };
static const IM3Operation c_setGlobalOps [] =    { NULL, op_SetGlobal_i32,             op_SetGlobal_i64,
                                                    FPOP(op_SetGlobal_f32),       FPOP(op_SetGlobal_f64) };
static const IM3Operation c_setRegisterOps [] =  { NULL, op_SetRegister_i32,           op_SetRegister_i64,
                                                    FPOP(op_SetRegister_f32),     FPOP(op_SetRegister_f64) };

static const IM3Operation c_intSelectOps [2] [4] =      { { op_Select_i32_rss, op_Select_i32_srs, op_Select_i32_ssr, op_Select_i32_sss },
                                                          { op_Select_i64_rss, op_Select_i64_srs, op_Select_i64_ssr, op_Select_i64_sss } };
#if d_m3HasFloat
static const IM3Operation c_fpSelectOps [2] [2] [3] = { { { op_Select_f32_sss, op_Select_f32_srs, op_Select_f32_ssr },        // selector in slot
                                                          { op_Select_f32_rss, op_Select_f32_rrs, op_Select_f32_rsr } },      // selector in reg
                                                        { { op_Select_f64_sss, op_Select_f64_srs, op_Select_f64_ssr },        // selector in slot
                                                          { op_Select_f64_rss, op_Select_f64_rrs, op_Select_f64_rsr } } };    // selector in reg
#endif

// all args & returns are 64-bit aligned, so use 2 slots for a d_m3Use32BitSlots=1 build
static const u16 c_ioSlotCount = sizeof (u64) / sizeof (m3slot_t);

static
M3Result  AcquireCompilationCodePage  (IM3Compilation o, IM3CodePage * o_codePage)
{
    M3Result result = m3Err_none;

    IM3CodePage page = AcquireCodePage (o->runtime);

    if (page)
    {
#       if (d_m3EnableCodePageRefCounting)
        {
            if (o->function)
            {
                IM3Function func = o->function;
                page->info.usageCount++;

                u32 index = func->numCodePageRefs++;
_               (m3ReallocArray (& func->codePageRefs, IM3CodePage, func->numCodePageRefs, index));
                func->codePageRefs [index] = page;
            }
        }
#       endif
    }
    else _throw (m3Err_mallocFailedCodePage);

    _catch:

    * o_codePage = page;

    return result;
}

static inline
void  ReleaseCompilationCodePage  (IM3Compilation o)
{
    ReleaseCodePage (o->runtime, o->page);
}

static inline
u16 GetTypeNumSlots (u8 i_type)
{
#   if d_m3Use32BitSlots
        return Is64BitType (i_type) ? 2 : 1;
#   else
        return 1;
#   endif
}

static inline
void  AlignSlotToType  (u16 * io_slot, u8 i_type)
{
    // align 64-bit words to even slots (if d_m3Use32BitSlots)
    u16 numSlots = GetTypeNumSlots (i_type);

    u16 mask = numSlots - 1;
    * io_slot = (* io_slot + mask) & ~mask;
}

static inline
i16  GetStackTopIndex  (IM3Compilation o)
{                                                           d_m3Assert (o->stackIndex > o->stackFirstDynamicIndex or IsStackPolymorphic (o));
    return o->stackIndex - 1;
}


// Items in the static portion of the stack (args/locals) are hidden from GetStackTypeFromTop ()
// In other words, only "real" Wasm stack items can be inspected.  This is important when
// returning values, etc. and you need an accurate wasm-view of the stack.
static
u8  GetStackTypeFromTop  (IM3Compilation o, u16 i_offset)
{
    u8 type = c_m3Type_none;

    ++i_offset;
    if (o->stackIndex >= i_offset)
    {
        u16 index = o->stackIndex - i_offset;

        if (index >= o->stackFirstDynamicIndex)
            type = o->typeStack [index];
    }

    return type;
}

static inline
u8  GetStackTopType  (IM3Compilation o)
{
    return GetStackTypeFromTop (o, 0);
}

static inline
u8  GetStackTypeFromBottom  (IM3Compilation o, u16 i_offset)
{
    u8 type = c_m3Type_none;

    if (i_offset < o->stackIndex)
        type = o->typeStack [i_offset];

    return type;
}


static inline bool  IsConstantSlot    (IM3Compilation o, u16 i_slot)  { return (i_slot >= o->slotFirstConstIndex and i_slot < o->slotMaxConstIndex); }
static inline bool  IsSlotAllocated   (IM3Compilation o, u16 i_slot)  { return o->m3Slots [i_slot]; }

static inline
bool  IsStackIndexInRegister  (IM3Compilation o, i32 i_stackIndex)
{                                                                           d_m3Assert (i_stackIndex < o->stackIndex or IsStackPolymorphic (o));
    if (i_stackIndex >= 0 and i_stackIndex < o->stackIndex)
        return (o->wasmStack [i_stackIndex] >= d_m3Reg0SlotAlias);
    else
        return false;
}

static inline u16   GetNumBlockValuesOnStack      (IM3Compilation o)  { return o->stackIndex - o->block.blockStackIndex; }

static inline bool  IsStackTopInRegister          (IM3Compilation o)  { return IsStackIndexInRegister (o, (i32) GetStackTopIndex (o));       }
static inline bool  IsStackTopMinus1InRegister    (IM3Compilation o)  { return IsStackIndexInRegister (o, (i32) GetStackTopIndex (o) - 1);   }
static inline bool  IsStackTopMinus2InRegister    (IM3Compilation o)  { return IsStackIndexInRegister (o, (i32) GetStackTopIndex (o) - 2);   }

static inline bool  IsStackTopInSlot              (IM3Compilation o)  { return not IsStackTopInRegister (o); }

static inline bool  IsValidSlot                   (u16 i_slot)        { return (i_slot < d_m3MaxFunctionSlots); }

static inline
u16  GetStackTopSlotNumber  (IM3Compilation o)
{
    i16 i = GetStackTopIndex (o);

    u16 slot = c_slotUnused;

    if (i >= 0)
        slot = o->wasmStack [i];

    return slot;
}


// from bottom
static inline
u16  GetSlotForStackIndex  (IM3Compilation o, u16 i_stackIndex)
{                                                                   d_m3Assert (i_stackIndex < o->stackIndex or IsStackPolymorphic (o));
    u16 slot = c_slotUnused;

    if (i_stackIndex < o->stackIndex)
        slot = o->wasmStack [i_stackIndex];

    return slot;
}

static inline
u16  GetExtraSlotForStackIndex  (IM3Compilation o, u16 i_stackIndex)
{
    u16 baseSlot = GetSlotForStackIndex (o, i_stackIndex);

    if (baseSlot != c_slotUnused)
    {
        u16 extraSlot = GetTypeNumSlots (GetStackTypeFromBottom (o, i_stackIndex)) - 1;
        baseSlot += extraSlot;
    }

    return baseSlot;
}


static inline
void  TouchSlot  (IM3Compilation o, u16 i_slot)
{
    // op_Entry uses this value to track and detect stack overflow
    o->maxStackSlots = M3_MAX (o->maxStackSlots, i_slot + 1);
}

static inline
void  MarkSlotAllocated  (IM3Compilation o, u16 i_slot)
{                                                                   d_m3Assert (o->m3Slots [i_slot] == 0); // shouldn't be already allocated
    o->m3Slots [i_slot] = 1;

    o->slotMaxAllocatedIndexPlusOne = M3_MAX (o->slotMaxAllocatedIndexPlusOne, i_slot + 1);

    TouchSlot (o, i_slot);
}

static inline
void  MarkSlotsAllocated  (IM3Compilation o, u16 i_slot, u16 i_numSlots)
{
    while (i_numSlots--)
        MarkSlotAllocated (o, i_slot++);
}

static inline
void  MarkSlotsAllocatedByType  (IM3Compilation o, u16 i_slot, u8 i_type)
{
    u16 numSlots = GetTypeNumSlots (i_type);
    MarkSlotsAllocated (o, i_slot, numSlots);
}


static
M3Result  AllocateSlotsWithinRange  (IM3Compilation o, u16 * o_slot, u8 i_type, u16 i_startSlot, u16 i_endSlot)
{
    M3Result result = m3Err_functionStackOverflow;

    u16 numSlots = GetTypeNumSlots (i_type);
    u16 searchOffset = numSlots - 1;

    AlignSlotToType (& i_startSlot, i_type);

    // search for 1 or 2 consecutive slots in the execution stack
    u16 i = i_startSlot;
    while (i + searchOffset < i_endSlot)
    {
        if (o->m3Slots [i] == 0 and o->m3Slots [i + searchOffset] == 0)
        {
            MarkSlotsAllocated (o, i, numSlots);

            * o_slot = i;
            result = m3Err_none;
            break;
        }

        // keep 2-slot allocations even-aligned
        i += numSlots;
    }

    return result;
}

static inline
M3Result  AllocateSlots  (IM3Compilation o, u16 * o_slot, u8 i_type)
{
    return AllocateSlotsWithinRange (o, o_slot, i_type, o->slotFirstDynamicIndex, d_m3MaxFunctionSlots);
}

static inline
M3Result  AllocateConstantSlots  (IM3Compilation o, u16 * o_slot, u8 i_type)
{
    u16 maxTableIndex = o->slotFirstConstIndex + d_m3MaxConstantTableSize;
    return AllocateSlotsWithinRange (o, o_slot, i_type, o->slotFirstConstIndex, M3_MIN(o->slotFirstDynamicIndex, maxTableIndex));
}


// TOQUE: this usage count system could be eliminated. real world code doesn't frequently trigger it.  just copy to multiple
// unique slots.
static inline
M3Result  IncrementSlotUsageCount  (IM3Compilation o, u16 i_slot)
{                                                                                       d_m3Assert (i_slot < d_m3MaxFunctionSlots);
    M3Result result = m3Err_none;                                                       d_m3Assert (o->m3Slots [i_slot] > 0);

    // OPTZ (memory): 'm3Slots' could still be fused with 'typeStack' if 4 bits were used to indicate: [0,1,2,many]. The many-case
    // would scan 'wasmStack' to determine the actual usage count
    if (o->m3Slots [i_slot] < 0xFF)
    {
        o->m3Slots [i_slot]++;
    }
    else result = "slot usage count overflow";

    return result;
}

static inline
void DeallocateSlot (IM3Compilation o, i16 i_slot, u8 i_type)
{                                                                                       d_m3Assert (i_slot >= o->slotFirstDynamicIndex);
                                                                                        d_m3Assert (i_slot < o->slotMaxAllocatedIndexPlusOne);
    for (u16 i = 0; i < GetTypeNumSlots (i_type); ++i, ++i_slot)
    {                                                                                   d_m3Assert (o->m3Slots [i_slot]);
        -- o->m3Slots [i_slot];
    }
}


static inline
bool  IsRegisterTypeAllocated  (IM3Compilation o, u8 i_type)
{
    return IsRegisterAllocated (o, IsFpType (i_type));
}

static inline
void  AllocateRegister  (IM3Compilation o, u32 i_register, u16 i_stackIndex)
{                                                                                       d_m3Assert (not IsRegisterAllocated (o, i_register));
    o->regStackIndexPlusOne [i_register] = i_stackIndex + 1;
}

static inline
void  DeallocateRegister  (IM3Compilation o, u32 i_register)
{                                                                                       d_m3Assert (IsRegisterAllocated (o, i_register));
    o->regStackIndexPlusOne [i_register] = c_m3RegisterUnallocated;
}

static inline
u16  GetRegisterStackIndex  (IM3Compilation o, u32 i_register)
{                                                                                       d_m3Assert (IsRegisterAllocated (o, i_register));
    return o->regStackIndexPlusOne [i_register] - 1;
}

u16  GetMaxUsedSlotPlusOne  (IM3Compilation o)
{
    while (o->slotMaxAllocatedIndexPlusOne > o->slotFirstDynamicIndex)
    {
        if (IsSlotAllocated (o, o->slotMaxAllocatedIndexPlusOne - 1))
            break;

        o->slotMaxAllocatedIndexPlusOne--;
    }

#   ifdef DEBUG
        u16 maxSlot = o->slotMaxAllocatedIndexPlusOne;
        while (maxSlot < d_m3MaxFunctionSlots)
        {
            d_m3Assert (o->m3Slots [maxSlot] == 0);
            maxSlot++;
        }
#   endif

    return o->slotMaxAllocatedIndexPlusOne;
}

static
M3Result  PreserveRegisterIfOccupied  (IM3Compilation o, u8 i_registerType)
{
    M3Result result = m3Err_none;

    u32 regSelect = IsFpType (i_registerType);

    if (IsRegisterAllocated (o, regSelect))
    {
        u16 stackIndex = GetRegisterStackIndex (o, regSelect);
        DeallocateRegister (o, regSelect);

        u8 type = GetStackTypeFromBottom (o, stackIndex);

        // and point to a exec slot
        u16 slot = c_slotUnused;
_       (AllocateSlots (o, & slot, type));
        o->wasmStack [stackIndex] = slot;

_       (EmitOp (o, c_setSetOps [type]));
        EmitSlotOffset (o, slot);
    }

    _catch: return result;
}


// all values must be in slots before entering loop, if, and else blocks
// otherwise they'd end up preserve-copied in the block to probably different locations (if/else)
static inline
M3Result  PreserveRegisters  (IM3Compilation o)
{
    M3Result result;

_   (PreserveRegisterIfOccupied (o, c_m3Type_f64));
_   (PreserveRegisterIfOccupied (o, c_m3Type_i64));

    _catch: return result;
}

static
M3Result  PreserveNonTopRegisters  (IM3Compilation o)
{
    M3Result result = m3Err_none;

    i16 stackTop = GetStackTopIndex (o);

    if (stackTop >= 0)
    {
        if (IsRegisterAllocated (o, 0))     // r0
        {
            if (GetRegisterStackIndex (o, 0) != stackTop)
_               (PreserveRegisterIfOccupied (o, c_m3Type_i64));
        }

        if (IsRegisterAllocated (o, 1))     // fp0
        {
            if (GetRegisterStackIndex (o, 1) != stackTop)
_               (PreserveRegisterIfOccupied (o, c_m3Type_f64));
        }
    }

    _catch: return result;
}


//----------------------------------------------------------------------------------------------------------------------

static
M3Result  Push  (IM3Compilation o, u8 i_type, u16 i_slot)
{
    M3Result result = m3Err_none;

#if !d_m3HasFloat
    if (i_type == c_m3Type_f32 || i_type == c_m3Type_f64) {
        return m3Err_unknownOpcode;
    }
#endif

    u16 stackIndex = o->stackIndex++;                                       // printf ("push: %d\n", (i32) i);

    if (stackIndex < d_m3MaxFunctionStackHeight)
    {
        o->wasmStack        [stackIndex] = i_slot;
        o->typeStack        [stackIndex] = i_type;

        if (IsRegisterSlotAlias (i_slot))
        {
            u32 regSelect = IsFpRegisterSlotAlias (i_slot);
            AllocateRegister (o, regSelect, stackIndex);
        }

        if (d_m3LogWasmStack) dump_type_stack (o);
    }
    else result = m3Err_functionStackOverflow;

    return result;
}

static inline
M3Result  PushRegister  (IM3Compilation o, u8 i_type)
{
    M3Result result = m3Err_none;                                                       d_m3Assert ((u16) d_m3Reg0SlotAlias > (u16) d_m3MaxFunctionSlots);
    u16 slot = IsFpType (i_type) ? d_m3Fp0SlotAlias : d_m3Reg0SlotAlias;                d_m3Assert (i_type or IsStackPolymorphic (o));

_   (Push (o, i_type, slot));

    _catch: return result;
}

static
M3Result  Pop  (IM3Compilation o)
{
    M3Result result = m3Err_none;

    if (o->stackIndex > o->block.blockStackIndex)
    {
        o->stackIndex--;                                                //  printf ("pop: %d\n", (i32) o->stackIndex);

        u16 slot = o->wasmStack [o->stackIndex];
        u8 type = o->typeStack [o->stackIndex];

        if (IsRegisterSlotAlias (slot))
        {
            u32 regSelect = IsFpRegisterSlotAlias (slot);
            DeallocateRegister (o, regSelect);
        }
        else if (slot >= o->slotFirstDynamicIndex)
        {
            DeallocateSlot (o, slot, type);
        }
    }
    else if (not IsStackPolymorphic (o))
        result = m3Err_functionStackUnderrun;

    return result;
}

static
M3Result  PopType  (IM3Compilation o, u8 i_type)
{
    M3Result result = m3Err_none;

    u8 topType = GetStackTopType (o);

    if (i_type == topType or o->block.isPolymorphic)
    {
_       (Pop (o));
    }
    else _throw (m3Err_typeMismatch);

    _catch:
    return result;
}

static
M3Result  _PushAllocatedSlotAndEmit  (IM3Compilation o, u8 i_type, bool i_doEmit)
{
    M3Result result = m3Err_none;

    u16 slot = c_slotUnused;

_   (AllocateSlots (o, & slot, i_type));
_   (Push (o, i_type, slot));

    if (i_doEmit)
        EmitSlotOffset (o, slot);

//    printf ("push: %d\n", (u32) slot);

    _catch: return result;
}

static inline
M3Result  PushAllocatedSlotAndEmit  (IM3Compilation o, u8 i_type)
{
    return _PushAllocatedSlotAndEmit (o, i_type, true);
}

static inline
M3Result  PushAllocatedSlot  (IM3Compilation o, u8 i_type)
{
    return _PushAllocatedSlotAndEmit (o, i_type, false);
}

static
M3Result  PushConst  (IM3Compilation o, u64 i_word, u8 i_type)
{
    M3Result result = m3Err_none;

    // Early-exit if we're not emitting
    if (!o->page) return result;

    bool matchFound = false;
    bool is64BitType = Is64BitType (i_type);

    u16 numRequiredSlots = GetTypeNumSlots (i_type);
    u16 numUsedConstSlots = o->slotMaxConstIndex - o->slotFirstConstIndex;

    // search for duplicate matching constant slot to reuse
    if (numRequiredSlots == 2 and numUsedConstSlots >= 2)
    {
        u16 firstConstSlot = o->slotFirstConstIndex;
        AlignSlotToType (& firstConstSlot, c_m3Type_i64);

        for (u16 slot = firstConstSlot; slot < o->slotMaxConstIndex - 1; slot += 2)
        {
            if (IsSlotAllocated (o, slot) and IsSlotAllocated (o, slot + 1))
            {
                u64 constant;
                memcpy (&constant, &o->constants [slot - o->slotFirstConstIndex], sizeof(constant));

                if (constant == i_word)
                {
                    matchFound = true;
_                   (Push (o, i_type, slot));
                    break;
                }
            }
        }
    }
    else if (numRequiredSlots == 1)
    {
        for (u16 i = 0; i < numUsedConstSlots; ++i)
        {
            u16 slot = o->slotFirstConstIndex + i;

            if (IsSlotAllocated (o, slot))
            {
                bool matches;
                if (is64BitType) {
                    u64 constant;
                    memcpy (&constant, &o->constants [i], sizeof(constant));
                    matches = (constant == i_word);
                } else {
                    u32 constant;
                    memcpy (&constant, &o->constants [i], sizeof(constant));
                    matches = (constant == i_word);
                }
                if (matches)
                {
                    matchFound = true;
_                   (Push (o, i_type, slot));
                    break;
                }
            }
        }
    }

    if (not matchFound)
    {
        u16 slot = c_slotUnused;
        result = AllocateConstantSlots (o, & slot, i_type);

        if (result || slot == c_slotUnused) // no more constant table space; use inline constants
        {
            result = m3Err_none;

            if (is64BitType) {
_               (EmitOp (o, op_Const64));
                EmitWord64 (o->page, i_word);
            } else {
_               (EmitOp (o, op_Const32));
                EmitWord32 (o->page, (u32) i_word);
            }

_           (PushAllocatedSlotAndEmit (o, i_type));
        }
        else
        {
            u16 constTableIndex = slot - o->slotFirstConstIndex;

            d_m3Assert(constTableIndex < d_m3MaxConstantTableSize);

            if (is64BitType) {
                memcpy (& o->constants [constTableIndex], &i_word, sizeof(i_word));
            } else {
                u32 word32 = i_word;
                memcpy (& o->constants [constTableIndex], &word32, sizeof(word32));
            }

_           (Push (o, i_type, slot));

            o->slotMaxConstIndex = M3_MAX (slot + numRequiredSlots, o->slotMaxConstIndex);
        }
    }

    _catch: return result;
}

static inline
M3Result  EmitSlotNumOfStackTopAndPop  (IM3Compilation o)
{
    // no emit if value is in register
    if (IsStackTopInSlot (o))
        EmitSlotOffset (o, GetStackTopSlotNumber (o));

    return Pop (o);
}


// Or, maybe: EmitTrappingOp
M3Result  AddTrapRecord  (IM3Compilation o)
{
    M3Result result = m3Err_none;

    if (o->function)
    {
    }

    return result;
}

static
M3Result  UnwindBlockStack  (IM3Compilation o)
{
    M3Result result = m3Err_none;

    u32 popCount = 0;
    while (o->stackIndex > o->block.blockStackIndex)
    {
_       (Pop (o));
        ++popCount;
    }

    if (popCount)
    {
        m3log (compile, "unwound stack top: %d", popCount);
    }

    _catch: return result;
}

static inline
M3Result  SetStackPolymorphic  (IM3Compilation o)
{
    o->block.isPolymorphic = true;                              m3log (compile, "stack set polymorphic");
    return UnwindBlockStack (o);
}

static
void  PatchBranches  (IM3Compilation o)
{
    pc_t pc = GetPC (o);

    pc_t patches = o->block.patches;
    o->block.patches = NULL;

    while (patches)
    {                                                           m3log (compile, "patching location: %p to pc: %p", patches, pc);
        pc_t next = * (pc_t *) patches;
        * (pc_t *) patches = pc;
        patches = next;
    }
}

//-------------------------------------------------------------------------------------------------------------------------

static
M3Result  CopyStackIndexToSlot  (IM3Compilation o, u16 i_destSlot, u16 i_stackIndex)  // NoPushPop
{
    M3Result result = m3Err_none;

    IM3Operation op;

    u8 type = GetStackTypeFromBottom (o, i_stackIndex);
    bool inRegister = IsStackIndexInRegister (o, i_stackIndex);

    if (inRegister)
    {
        op = c_setSetOps [type];
    }
    else op = Is64BitType (type) ? op_CopySlot_64 : op_CopySlot_32;

_   (EmitOp (o, op));
    EmitSlotOffset (o, i_destSlot);

    if (not inRegister)
    {
        u16 srcSlot = GetSlotForStackIndex (o, i_stackIndex);
        EmitSlotOffset (o, srcSlot);
    }

    _catch: return result;
}

static
M3Result  CopyStackTopToSlot  (IM3Compilation o, u16 i_destSlot)  // NoPushPop
{
    M3Result result;

    i16 stackTop = GetStackTopIndex (o);
_   (CopyStackIndexToSlot (o, i_destSlot, (u16) stackTop));

    _catch: return result;
}


// a copy-on-write strategy is used with locals. when a get local occurs, it's not copied anywhere. the stack
// entry just has a index pointer to that local memory slot.
// then, when a previously referenced local is set, the current value needs to be preserved for those references

// TODO: consider getting rid of these specialized operations: PreserveSetSlot & PreserveCopySlot.
// They likely just take up space (which seems to reduce performance) without improving performance.
static
M3Result  PreservedCopyTopSlot  (IM3Compilation o, u16 i_destSlot, u16 i_preserveSlot)
{
    M3Result result = m3Err_none;             d_m3Assert (i_destSlot != i_preserveSlot);

    IM3Operation op;

    u8 type = GetStackTopType (o);

    if (IsStackTopInRegister (o))
    {
        op = c_preserveSetSlot [type];
    }
    else op = Is64BitType (type) ? op_PreserveCopySlot_64 : op_PreserveCopySlot_32;

_   (EmitOp (o, op));
    EmitSlotOffset (o, i_destSlot);

    if (IsStackTopInSlot (o))
        EmitSlotOffset (o, GetStackTopSlotNumber (o));

    EmitSlotOffset (o, i_preserveSlot);

    _catch: return result;
}

static
M3Result  CopyStackTopToRegister  (IM3Compilation o, bool i_updateStack)
{
    M3Result result = m3Err_none;

    if (IsStackTopInSlot (o))
    {
        u8 type = GetStackTopType (o);

_       (PreserveRegisterIfOccupied (o, type));

        IM3Operation op = c_setRegisterOps [type];

_       (EmitOp (o, op));
        EmitSlotOffset (o, GetStackTopSlotNumber (o));

        if (i_updateStack)
        {
_           (PopType (o, type));
_           (PushRegister (o, type));
        }
    }

    _catch: return result;
}


// if local is unreferenced, o_preservedSlotNumber will be equal to localIndex on return
static
M3Result  FindReferencedLocalWithinCurrentBlock  (IM3Compilation o, u16 * o_preservedSlotNumber, u32 i_localSlot)
{
    M3Result result = m3Err_none;

    IM3CompilationScope scope = & o->block;
    u16 startIndex = scope->blockStackIndex;

    while (scope->opcode == c_waOp_block)
    {
        scope = scope->outer;
        if (not scope)
            break;

        startIndex = scope->blockStackIndex;
    }

    * o_preservedSlotNumber = (u16) i_localSlot;

    for (u32 i = startIndex; i < o->stackIndex; ++i)
    {
        if (o->wasmStack [i] == i_localSlot)
        {
            if (* o_preservedSlotNumber == i_localSlot)
            {
                u8 type = GetStackTypeFromBottom (o, i);                    d_m3Assert (type != c_m3Type_none)

_               (AllocateSlots (o, o_preservedSlotNumber, type));
            }
            else
_               (IncrementSlotUsageCount (o, * o_preservedSlotNumber));

            o->wasmStack [i] = * o_preservedSlotNumber;
        }
    }

    _catch: return result;
}

static
M3Result  GetBlockScope  (IM3Compilation o, IM3CompilationScope * o_scope, u32 i_depth)
{
    M3Result result = m3Err_none;

    IM3CompilationScope scope = & o->block;

    while (i_depth--)
    {
        scope = scope->outer;
        _throwif ("invalid block depth", not scope);
    }

    * o_scope = scope;

    _catch:
    return result;
}

static
M3Result  CopyStackSlotsR  (IM3Compilation o, u16 i_targetSlotStackIndex, u16 i_stackIndex, u16 i_endStackIndex, u16 i_tempSlot)
{
    M3Result result = m3Err_none;

    if (i_stackIndex < i_endStackIndex)
    {
        u16 srcSlot = GetSlotForStackIndex (o, i_stackIndex);

        u8 type = GetStackTypeFromBottom (o, i_stackIndex);
        u16 numSlots = GetTypeNumSlots (type);
        u16 extraSlot = numSlots - 1;

        u16 targetSlot = GetSlotForStackIndex (o, i_targetSlotStackIndex);

        u16 preserveIndex = i_stackIndex;
        u16 collisionSlot = srcSlot;

        if (targetSlot != srcSlot)
        {
            // search for collisions
            u16 checkIndex = i_stackIndex + 1;
            while (checkIndex < i_endStackIndex)
            {
                u16 otherSlot1 = GetSlotForStackIndex (o, checkIndex);
                u16 otherSlot2 = GetExtraSlotForStackIndex (o, checkIndex);

                if (targetSlot == otherSlot1 or
                    targetSlot == otherSlot2 or
                    targetSlot + extraSlot == otherSlot1)
                {
                    _throwif (m3Err_functionStackOverflow, i_tempSlot >= d_m3MaxFunctionSlots);

_                   (CopyStackIndexToSlot (o, i_tempSlot, checkIndex));
                    o->wasmStack [checkIndex] = i_tempSlot;
                    i_tempSlot += GetTypeNumSlots (c_m3Type_i64);
                    TouchSlot (o, i_tempSlot - 1);

                    // restore this on the way back down
                    preserveIndex = checkIndex;
                    collisionSlot = otherSlot1;

                    break;
                }

                ++checkIndex;
            }

_           (CopyStackIndexToSlot (o, targetSlot, i_stackIndex));                                               m3log (compile, " copying slot: %d to slot: %d", srcSlot, targetSlot);
            o->wasmStack [i_stackIndex] = targetSlot;

        }

_       (CopyStackSlotsR (o, i_targetSlotStackIndex + 1, i_stackIndex + 1, i_endStackIndex, i_tempSlot));

        // restore the stack state
        o->wasmStack [i_stackIndex] = srcSlot;
        o->wasmStack [preserveIndex] = collisionSlot;
    }

    _catch:
    return result;
}

static
M3Result  ResolveBlockResults  (IM3Compilation o, IM3CompilationScope i_targetBlock, bool i_isBranch)
{
    M3Result result = m3Err_none;                                   if (d_m3LogWasmStack) dump_type_stack (o);

    bool isLoop = (i_targetBlock->opcode == c_waOp_loop and i_isBranch);

    u16 numParams = GetFuncTypeNumParams (i_targetBlock->type);
    u16 numResults = GetFuncTypeNumResults (i_targetBlock->type);

    u16 slotRecords = i_targetBlock->exitStackIndex;

    u16 numValues;

    if (not isLoop)
    {
        numValues = numResults;
        slotRecords += numParams;
    }
    else numValues = numParams;

    u16 blockHeight = GetNumBlockValuesOnStack (o);

    _throwif (m3Err_typeCountMismatch, i_isBranch ? (blockHeight < numValues) : (blockHeight != numValues));

    if (numValues)
    {
        u16 endIndex = GetStackTopIndex (o) + 1;
        u16 numRemValues = numValues;

        // The last result is taken from _fp0. See PushBlockResults.
        if (not isLoop and IsFpType (GetStackTopType (o)))
        {
_           (CopyStackTopToRegister (o, false));
            --endIndex;
            --numRemValues;
        }

        // TODO: tempslot affects maxStackSlots, so can grow unnecess each time.
        u16 tempSlot = o->maxStackSlots;// GetMaxUsedSlotPlusOne (o); doesn't work cause can collide with slotRecords
        AlignSlotToType (& tempSlot, c_m3Type_i64);

_       (CopyStackSlotsR (o, slotRecords, endIndex - numRemValues, endIndex, tempSlot));

        if (d_m3LogWasmStack) dump_type_stack (o);
    }

    _catch: return result;
}


static
M3Result  ReturnValues  (IM3Compilation o, IM3CompilationScope i_functionBlock, bool i_isBranch)
{
    M3Result result = m3Err_none;                                               if (d_m3LogWasmStack) dump_type_stack (o);

    u16 numReturns = GetFuncTypeNumResults (i_functionBlock->type);     // could just o->function too...
    u16 blockHeight = GetNumBlockValuesOnStack (o);

    if (not IsStackPolymorphic (o))
        _throwif (m3Err_typeCountMismatch, i_isBranch ? (blockHeight < numReturns) : (blockHeight != numReturns));

    if (numReturns)
    {
        // return slots like args are 64-bit aligned
        u16 returnSlot = numReturns * c_ioSlotCount;
        u16 stackTop = GetStackTopIndex (o);

        for (u16 i = 0; i < numReturns; ++i)
        {
            u8 returnType = GetFuncTypeResultType (i_functionBlock->type, numReturns - 1 - i);

            u8 stackType = GetStackTypeFromTop (o, i);  // using FromTop so that only dynamic items are checked

            if (IsStackPolymorphic (o) and stackType == c_m3Type_none)
                stackType = returnType;

            _throwif (m3Err_typeMismatch, returnType != stackType);

            if (not IsStackPolymorphic (o))
            {
                returnSlot -= c_ioSlotCount;
_               (CopyStackIndexToSlot (o, returnSlot, stackTop--));
            }
        }

        if (not i_isBranch)
        {
            while (numReturns--)
_               (Pop (o));
        }
    }

    _catch: return result;
}


//-------------------------------------------------------------------------------------------------------------------------

static
M3Result  Compile_Const_i32  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    i32 value;
_   (ReadLEB_i32 (& value, & o->wasm, o->wasmEnd));
_   (PushConst (o, value, c_m3Type_i32));                       m3log (compile, d_indent " (const i32 = %" PRIi32 ")", get_indention_string (o), value);
    _catch: return result;
}

static
M3Result  Compile_Const_i64  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    i64 value;
_   (ReadLEB_i64 (& value, & o->wasm, o->wasmEnd));
_   (PushConst (o, value, c_m3Type_i64));                       m3log (compile, d_indent " (const i64 = %" PRIi64 ")", get_indention_string (o), value);
    _catch: return result;
}


#if d_m3ImplementFloat
static
M3Result  Compile_Const_f32  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    union { u32 u; f32 f; } value = { 0 };

_   (Read_f32 (& value.f, & o->wasm, o->wasmEnd));              m3log (compile, d_indent " (const f32 = %" PRIf32 ")", get_indention_string (o), value.f);
_   (PushConst (o, value.u, c_m3Type_f32));

    _catch: return result;
}

static
M3Result  Compile_Const_f64  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    union { u64 u; f64 f; } value = { 0 };

_   (Read_f64 (& value.f, & o->wasm, o->wasmEnd));              m3log (compile, d_indent " (const f64 = %" PRIf64 ")", get_indention_string (o), value.f);
_   (PushConst (o, value.u, c_m3Type_f64));

    _catch: return result;
}
#endif

#if d_m3CascadedOpcodes

static
M3Result  Compile_ExtendedOpcode  (IM3Compilation o, m3opcode_t i_opcode)
{
_try {
    u8 opcode;
_   (Read_u8 (& opcode, & o->wasm, o->wasmEnd));             m3log (compile, d_indent " (FC: %" PRIi32 ")", get_indention_string (o), opcode);

    i_opcode = (i_opcode << 8) | opcode;

    //printf("Extended opcode: 0x%x\n", i_opcode);

    IM3OpInfo opInfo = GetOpInfo (i_opcode);
    _throwif (m3Err_unknownOpcode, not opInfo);

    M3Compiler compiler = opInfo->compiler;
    _throwif (m3Err_noCompiler, not compiler);

_   ((* compiler) (o, i_opcode));

    o->previousOpcode = i_opcode;

    } _catch: return result;
}
#endif

static
M3Result  Compile_Return  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result = m3Err_none;

    if (not IsStackPolymorphic (o))
    {
        IM3CompilationScope functionScope;
_       (GetBlockScope (o, & functionScope, o->block.depth));

_       (ReturnValues (o, functionScope, true));

_       (EmitOp (o, op_Return));

_       (SetStackPolymorphic (o));
    }

    _catch: return result;
}

static
M3Result  ValidateBlockEnd  (IM3Compilation o)
{
    M3Result result = m3Err_none;
/*
    u16 numResults = GetFuncTypeNumResults (o->block.type);
    u16 blockHeight = GetNumBlockValuesOnStack (o);

    if (IsStackPolymorphic (o))
    {
    }
    else
    {
    }

    _catch: */ return result;
}

static
M3Result  Compile_End  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result = m3Err_none;                   //dump_type_stack (o);

    // function end:
    if (o->block.depth == 0)
    {
        ValidateBlockEnd (o);

//      if (not IsStackPolymorphic (o))
        {
            if (o->function)
            {
_               (ReturnValues (o, & o->block, false));
            }

_           (EmitOp (o, op_Return));
        }
    }

    _catch: return result;
}


static
M3Result  Compile_SetLocal  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    u32 localIndex;
_   (ReadLEB_u32 (& localIndex, & o->wasm, o->wasmEnd));             //  printf ("--- set local: %d \n", localSlot);

    if (localIndex < GetFunctionNumArgsAndLocals (o->function))
    {
        u16 localSlot = GetSlotForStackIndex (o, localIndex);

        u16 preserveSlot;
_       (FindReferencedLocalWithinCurrentBlock (o, & preserveSlot, localSlot));  // preserve will be different than local, if referenced

        if (preserveSlot == localSlot)
_           (CopyStackTopToSlot (o, localSlot))
        else
_           (PreservedCopyTopSlot (o, localSlot, preserveSlot))

        if (i_opcode != c_waOp_teeLocal)
_           (Pop (o));
    }
    else _throw ("local index out of bounds");

    _catch: return result;
}

static
M3Result  Compile_GetLocal  (IM3Compilation o, m3opcode_t i_opcode)
{
_try {

    u32 localIndex;
_   (ReadLEB_u32 (& localIndex, & o->wasm, o->wasmEnd));

    if (localIndex >= GetFunctionNumArgsAndLocals (o->function))
        _throw ("local index out of bounds");

    u8 type = GetStackTypeFromBottom (o, localIndex);
    u16 slot = GetSlotForStackIndex (o, localIndex);

_   (Push (o, type, slot));

    } _catch: return result;
}

static
M3Result  Compile_GetGlobal  (IM3Compilation o, M3Global * i_global)
{
    M3Result result;

    IM3Operation op = Is64BitType (i_global->type) ? op_GetGlobal_s64 : op_GetGlobal_s32;
_   (EmitOp (o, op));
    EmitPointer (o, & i_global->i64Value);
_   (PushAllocatedSlotAndEmit (o, i_global->type));

    _catch: return result;
}

static
M3Result  Compile_SetGlobal  (IM3Compilation o, M3Global * i_global)
{
    M3Result result = m3Err_none;

    if (i_global->isMutable)
    {
        IM3Operation op;
        u8 type = GetStackTopType (o);

        if (IsStackTopInRegister (o))
        {
            op = c_setGlobalOps [type];
        }
        else op = Is64BitType (type) ? op_SetGlobal_s64 : op_SetGlobal_s32;

_      (EmitOp (o, op));
        EmitPointer (o, & i_global->i64Value);

        if (IsStackTopInSlot (o))
            EmitSlotOffset (o, GetStackTopSlotNumber (o));

_      (Pop (o));
    }
    else _throw (m3Err_settingImmutableGlobal);

    _catch: return result;
}

static
M3Result  Compile_GetSetGlobal  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result = m3Err_none;

    u32 globalIndex;
_   (ReadLEB_u32 (& globalIndex, & o->wasm, o->wasmEnd));

    if (globalIndex < o->module->numGlobals)
    {
        if (o->module->globals)
        {
            M3Global * global = & o->module->globals [globalIndex];

_           ((i_opcode == c_waOp_getGlobal) ? Compile_GetGlobal (o, global) : Compile_SetGlobal (o, global));
        }
        else _throw (ErrorCompile (m3Err_globalMemoryNotAllocated, o, "module '%s' is missing global memory", o->module->name));
    }
    else _throw (m3Err_globaIndexOutOfBounds);

    _catch: return result;
}

static
void  EmitPatchingBranchPointer  (IM3Compilation o, IM3CompilationScope i_scope)
{
    pc_t patch = EmitPointer (o, i_scope->patches);                     m3log (compile, "branch patch required at: %p", patch);
    i_scope->patches = patch;
}

static
M3Result  EmitPatchingBranch  (IM3Compilation o, IM3CompilationScope i_scope)
{
    M3Result result = m3Err_none;

_   (EmitOp (o, op_Branch));
    EmitPatchingBranchPointer (o, i_scope);

    _catch: return result;
}

static
M3Result  Compile_Branch  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    u32 depth;
_   (ReadLEB_u32 (& depth, & o->wasm, o->wasmEnd));

    IM3CompilationScope scope;
_   (GetBlockScope (o, & scope, depth));

    // branch target is a loop (continue)
    if (scope->opcode == c_waOp_loop)
    {
        if (i_opcode == c_waOp_branchIf)
        {
            if (GetFuncTypeNumParams (scope->type))
            {
                IM3Operation op = IsStackTopInRegister (o) ? op_BranchIfPrologue_r : op_BranchIfPrologue_s;

_               (EmitOp (o, op));
_               (EmitSlotNumOfStackTopAndPop (o));

                pc_t * jumpTo = (pc_t *) ReservePointer (o);

_               (ResolveBlockResults (o, scope, /* isBranch: */ true));

_               (EmitOp (o, op_ContinueLoop));
                EmitPointer (o, scope->pc);

                * jumpTo = GetPC (o);
            }
            else
            {
                // move the condition to a register
_               (CopyStackTopToRegister (o, false));
_               (PopType (o, c_m3Type_i32));

_               (EmitOp (o, op_ContinueLoopIf));
                EmitPointer (o, scope->pc);
            }

//          dump_type_stack(o);
        }
        else // is c_waOp_branch
        {
    _       (EmitOp (o, op_ContinueLoop));
            EmitPointer (o, scope->pc);
            o->block.isPolymorphic = true;
        }
    }
    else // forward branch
    {
        pc_t * jumpTo = NULL;

        bool isReturn = (scope->depth == 0);
        bool targetHasResults = GetFuncTypeNumResults (scope->type);

        if (i_opcode == c_waOp_branchIf)
        {
            if (targetHasResults or isReturn)
            {
                IM3Operation op = IsStackTopInRegister (o) ? op_BranchIfPrologue_r : op_BranchIfPrologue_s;

    _           (EmitOp (o, op));
    _           (EmitSlotNumOfStackTopAndPop (o)); // condition

                // this is continuation point, if the branch isn't taken
                jumpTo = (pc_t *) ReservePointer (o);
            }
            else
            {
                IM3Operation op = IsStackTopInRegister (o) ? op_BranchIf_r : op_BranchIf_s;

    _           (EmitOp (o, op));
    _           (EmitSlotNumOfStackTopAndPop (o)); // condition

                EmitPatchingBranchPointer (o, scope);
                goto _catch;
            }
        }

        if (not IsStackPolymorphic (o))
        {
            if (isReturn)
            {
_               (ReturnValues (o, scope, true));
_               (EmitOp (o, op_Return));
            }
            else
            {
_               (ResolveBlockResults (o, scope, true));
_               (EmitPatchingBranch (o, scope));
            }
        }

        if (jumpTo)
        {
            * jumpTo = GetPC (o);
        }

        if (i_opcode == c_waOp_branch)
_           (SetStackPolymorphic (o));
    }

    _catch: return result;
}

static
M3Result  Compile_BranchTable  (IM3Compilation o, m3opcode_t i_opcode)
{
_try {
    u32 targetCount;
_   (ReadLEB_u32 (& targetCount, & o->wasm, o->wasmEnd));

_   (PreserveRegisterIfOccupied (o, c_m3Type_i64));         // move branch operand to a slot
    u16 slot = GetStackTopSlotNumber (o);
_   (Pop (o));

    // OPTZ: according to spec: "forward branches that target a control instruction with a non-empty
    // result type consume matching operands first and push them back on the operand stack after unwinding"
    // So, this move-to-reg is only necessary if the target scopes have a type.

    u32 numCodeLines = targetCount + 4; // 3 => IM3Operation + slot + target_count + default_target
_   (EnsureCodePageNumLines (o, numCodeLines));

_   (EmitOp (o, op_BranchTable));
    EmitSlotOffset (o, slot);
    EmitConstant32 (o, targetCount);

    IM3CodePage continueOpPage = NULL;

    ++targetCount; // include default
    for (u32 i = 0; i < targetCount; ++i)
    {
        u32 target;
_       (ReadLEB_u32 (& target, & o->wasm, o->wasmEnd));

        IM3CompilationScope scope;
_       (GetBlockScope (o, & scope, target));

        // TODO: don't need codepage rigmarole for
        // no-param forward-branch targets

_       (AcquireCompilationCodePage (o, & continueOpPage));

        pc_t startPC = GetPagePC (continueOpPage);
        IM3CodePage savedPage = o->page;
        o->page = continueOpPage;

        if (scope->opcode == c_waOp_loop)
        {
_           (ResolveBlockResults (o, scope, true));

_           (EmitOp (o, op_ContinueLoop));
            EmitPointer (o, scope->pc);
        }
        else
        {
            // TODO: this could be fused with equivalent targets
            if (not IsStackPolymorphic (o))
            {
                if (scope->depth == 0)
                {
_                   (ReturnValues (o, scope, true));
_                   (EmitOp (o, op_Return));
                }
                else
                {
_                   (ResolveBlockResults (o, scope, true));

_                   (EmitPatchingBranch (o, scope));
                }
            }
        }

        ReleaseCompilationCodePage (o);     // FIX: continueOpPage can get lost if thrown
        o->page = savedPage;

        EmitPointer (o, startPC);
    }

_   (SetStackPolymorphic (o));

    }

    _catch: return result;
}

static
M3Result  CompileCallArgsAndReturn  (IM3Compilation o, u16 * o_stackOffset, IM3FuncType i_type, bool i_isIndirect)
{
_try {

    u16 topSlot = GetMaxUsedSlotPlusOne (o);

    // force use of at least one stack slot; this is to help ensure
    // the m3 stack overflows (and traps) before the native stack can overflow.
    // e.g. see Wasm spec test 'runaway' in call.wast
    topSlot = M3_MAX (1, topSlot);

    // stack frame is 64-bit aligned
    AlignSlotToType (& topSlot, c_m3Type_i64);

    * o_stackOffset = topSlot;

    // wait to pop this here so that topSlot search is correct
    if (i_isIndirect)
_       (Pop (o));

    u16 numArgs = GetFuncTypeNumParams (i_type);
    u16 numRets = GetFuncTypeNumResults (i_type);

    u16 argTop = topSlot + (numArgs + numRets) * c_ioSlotCount;

    while (numArgs--)
    {
_       (CopyStackTopToSlot (o, argTop -= c_ioSlotCount));
_       (Pop (o));
    }

    u16 i = 0;
    while (numRets--)
    {
        u8 type = GetFuncTypeResultType (i_type, i++);

_       (Push (o, type, topSlot));
        MarkSlotsAllocatedByType (o, topSlot, type);

        topSlot += c_ioSlotCount;
    }

    } _catch: return result;
}

static
M3Result  Compile_Call  (IM3Compilation o, m3opcode_t i_opcode)
{
_try {
    u32 functionIndex;
_   (ReadLEB_u32 (& functionIndex, & o->wasm, o->wasmEnd));

    IM3Function function = Module_GetFunction (o->module, functionIndex);

    if (function)
    {                                                                   m3log (compile, d_indent " (func= [%d] '%s'; args= %d)",
                                                                                get_indention_string (o), functionIndex, m3_GetFunctionName (function), function->funcType->numArgs);
        if (function->module)
        {
            u16 slotTop;
_           (CompileCallArgsAndReturn (o, & slotTop, function->funcType, false));

            IM3Operation op;
            const void * operand;

            if (function->compiled)
            {
                op = op_Call;
                operand = function->compiled;
            }
            else
            {
                op = op_Compile;
                operand = function;
            }

_           (EmitOp     (o, op));
            EmitPointer (o, operand);
            EmitSlotOffset  (o, slotTop);
        }
        else
        {
            _throw (ErrorCompile (m3Err_functionImportMissing, o, "'%s.%s'", GetFunctionImportModuleName (function), m3_GetFunctionName (function)));
        }
    }
    else _throw (m3Err_functionLookupFailed);

    } _catch: return result;
}

static
M3Result  Compile_CallIndirect  (IM3Compilation o, m3opcode_t i_opcode)
{
_try {
    u32 typeIndex;
_   (ReadLEB_u32 (& typeIndex, & o->wasm, o->wasmEnd));

    u32 tableIndex;
_   (ReadLEB_u32 (& tableIndex, & o->wasm, o->wasmEnd));

    _throwif ("function call type index out of range", typeIndex >= o->module->numFuncTypes);

    if (IsStackTopInRegister (o))
_       (PreserveRegisterIfOccupied (o, c_m3Type_i32));

    u16 tableIndexSlot = GetStackTopSlotNumber (o);

    u16 execTop;
    IM3FuncType type = o->module->funcTypes [typeIndex];
_   (CompileCallArgsAndReturn (o, & execTop, type, true));

_   (EmitOp         (o, op_CallIndirect));
    EmitSlotOffset  (o, tableIndexSlot);
    EmitPointer     (o, o->module);
    EmitPointer     (o, type);              // TODO: unify all types in M3Environment
    EmitSlotOffset  (o, execTop);

} _catch:
    return result;
}

static
M3Result  Compile_Memory_Size  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    i8 reserved;
_   (ReadLEB_i7 (& reserved, & o->wasm, o->wasmEnd));

_   (PreserveRegisterIfOccupied (o, c_m3Type_i32));

_   (EmitOp     (o, op_MemSize));

_   (PushRegister (o, c_m3Type_i32));

    _catch: return result;
}

static
M3Result  Compile_Memory_Grow  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    i8 reserved;
_   (ReadLEB_i7 (& reserved, & o->wasm, o->wasmEnd));

_   (CopyStackTopToRegister (o, false));
_   (PopType (o, c_m3Type_i32));

_   (EmitOp     (o, op_MemGrow));

_   (PushRegister (o, c_m3Type_i32));

    _catch: return result;
}

static
M3Result  Compile_Memory_CopyFill  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result = m3Err_none;

    u32 sourceMemoryIdx, targetMemoryIdx;
    IM3Operation op;
    if (i_opcode == c_waOp_memoryCopy)
    {
_       (ReadLEB_u32 (& sourceMemoryIdx, & o->wasm, o->wasmEnd));
        op = op_MemCopy;
    }
    else op = op_MemFill;

_   (ReadLEB_u32 (& targetMemoryIdx, & o->wasm, o->wasmEnd));

_   (CopyStackTopToRegister (o, false));

_   (EmitOp  (o, op));
_   (PopType (o, c_m3Type_i32));
_   (EmitSlotNumOfStackTopAndPop (o));
_   (EmitSlotNumOfStackTopAndPop (o));

    _catch: return result;
}


static
M3Result  ReadBlockType  (IM3Compilation o, IM3FuncType * o_blockType)
{
    M3Result result;

    i64 type;
_   (ReadLebSigned (& type, 33, & o->wasm, o->wasmEnd));

    if (type < 0)
    {
        u8 valueType;
_       (NormalizeType (&valueType, type));                                m3log (compile, d_indent " (type: %s)", get_indention_string (o), c_waTypes [valueType]);
        *o_blockType = o->module->environment->retFuncTypes[valueType];
    }
    else
    {
        _throwif("func type out of bounds", type >= o->module->numFuncTypes);
        *o_blockType = o->module->funcTypes[type];                         m3log (compile, d_indent " (type: %s)", get_indention_string (o), SPrintFuncTypeSignature (*o_blockType));
    }
    _catch: return result;
}

static
M3Result  PreserveArgsAndLocals  (IM3Compilation o)
{
    M3Result result = m3Err_none;

    if (o->stackIndex > o->stackFirstDynamicIndex)
    {
        u32 numArgsAndLocals = GetFunctionNumArgsAndLocals (o->function);

        for (u32 i = 0; i < numArgsAndLocals; ++i)
        {
            u16 slot = GetSlotForStackIndex (o, i);

            u16 preservedSlotNumber;
_           (FindReferencedLocalWithinCurrentBlock (o, & preservedSlotNumber, slot));

            if (preservedSlotNumber != slot)
            {
                u8 type = GetStackTypeFromBottom (o, i);                    d_m3Assert (type != c_m3Type_none)
                IM3Operation op = Is64BitType (type) ? op_CopySlot_64 : op_CopySlot_32;

                EmitOp          (o, op);
                EmitSlotOffset  (o, preservedSlotNumber);
                EmitSlotOffset  (o, slot);
            }
        }
    }

    _catch:
    return result;
}

static
M3Result  Compile_LoopOrBlock  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    // TODO: these shouldn't be necessary for non-loop blocks?
_   (PreserveRegisters (o));
_   (PreserveArgsAndLocals (o));

    IM3FuncType blockType;
_   (ReadBlockType (o, & blockType));

    if (i_opcode == c_waOp_loop)
    {
        u16 numParams = GetFuncTypeNumParams (blockType);
        if (numParams)
        {
            // instantiate constants
            u16 numValues = GetNumBlockValuesOnStack (o);                   // CompileBlock enforces this at comptime
                                                                            d_m3Assert (numValues >= numParams);
            if (numValues >= numParams)
            {
                u16 stackTop = GetStackTopIndex (o) + 1;

                for (u16 i = stackTop - numParams; i < stackTop; ++i)
                {
                    u16 slot = GetSlotForStackIndex (o, i);
                    u8 type = GetStackTypeFromBottom (o, i);

                    if (IsConstantSlot (o, slot))
                    {
                        u16 newSlot = c_slotUnused;
_                       (AllocateSlots (o, & newSlot, type));
_                       (CopyStackIndexToSlot (o, newSlot, i));
                        o->wasmStack [i] = newSlot;
                    }
                }
            }
        }

_       (EmitOp (o, op_Loop));
    }
    else
    {
    }

_   (CompileBlock (o, blockType, i_opcode));

    _catch: return result;
}

static
M3Result  CompileElseBlock  (IM3Compilation o, pc_t * o_startPC, IM3FuncType i_blockType)
{
    IM3CodePage savedPage = o->page;
_try {

    IM3CodePage elsePage;
_   (AcquireCompilationCodePage (o, & elsePage));

    * o_startPC = GetPagePC (elsePage);

    o->page = elsePage;

_   (CompileBlock (o, i_blockType, c_waOp_else));

_   (EmitOp (o, op_Branch));
    EmitPointer (o, GetPagePC (savedPage));
} _catch:
    if(o->page != savedPage) {
        ReleaseCompilationCodePage (o);
    }
    o->page = savedPage;
    return result;
}

static
M3Result  Compile_If  (IM3Compilation o, m3opcode_t i_opcode)
{
    /*      [   op_If   ]
            [ <else-pc> ]   ---->   [ ..else..  ]
            [  ..if..   ]           [ ..block.. ]
            [ ..block.. ]           [ op_Branch ]
            [    end    ]  <-----   [  <end-pc> ]       */

_try {

_   (PreserveNonTopRegisters (o));
_   (PreserveArgsAndLocals (o));

    IM3Operation op = IsStackTopInRegister (o) ? op_If_r : op_If_s;

_   (EmitOp (o, op));
_   (EmitSlotNumOfStackTopAndPop (o));

    pc_t * pc = (pc_t *) ReservePointer (o);

    IM3FuncType blockType;
_   (ReadBlockType (o, & blockType));

//  dump_type_stack (o);

    u16 stackIndex = o->stackIndex;

_   (CompileBlock (o, blockType, i_opcode));

    if (o->previousOpcode == c_waOp_else)
    {
        o->stackIndex = stackIndex;
_       (CompileElseBlock (o, pc, blockType));
    }
    else
    {
        // if block produces values and there isn't a defined else
        // case, then we need to make one up so that the pass-through
        // results end up in the right place
        if (GetFuncTypeNumResults (blockType))
        {
            // rewind to the if's end to create a fake else block
            o->wasm--;
            o->stackIndex = stackIndex;

//          dump_type_stack (o);

_           (CompileElseBlock (o, pc, blockType));
        }
        else * pc = GetPC (o);
    }

    } _catch: return result;
}

static
M3Result  Compile_Select  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result = m3Err_none;

    u16 slots [3] = { c_slotUnused, c_slotUnused, c_slotUnused };

    u8 type = GetStackTypeFromTop (o, 1); // get type of selection

    IM3Operation op = NULL;

    if (IsFpType (type))
    {
#   if d_m3HasFloat
        // not consuming a fp reg, so preserve
        if (not IsStackTopMinus1InRegister (o) and
            not IsStackTopMinus2InRegister (o))
        {
_           (PreserveRegisterIfOccupied (o, type));
        }

        bool selectorInReg = IsStackTopInRegister (o);
        slots [0] = GetStackTopSlotNumber (o);
_       (Pop (o));

        u32 opIndex = 0;

        for (u32 i = 1; i <= 2; ++i)
        {
            if (IsStackTopInRegister (o))
                opIndex = i;
            else
                slots [i] = GetStackTopSlotNumber (o);

_          (Pop (o));
        }

        op = c_fpSelectOps [type - c_m3Type_f32] [selectorInReg] [opIndex];
#   else
        _throw (m3Err_unknownOpcode);
#   endif
    }
    else if (IsIntType (type))
    {
        // 'sss' operation doesn't consume a register, so might have to protected its contents
        if (not IsStackTopInRegister (o) and
            not IsStackTopMinus1InRegister (o) and
            not IsStackTopMinus2InRegister (o))
        {
_           (PreserveRegisterIfOccupied (o, type));
        }

        u32 opIndex = 3;  // op_Select_*_sss

        for (u32 i = 0; i < 3; ++i)
        {
            if (IsStackTopInRegister (o))
                opIndex = i;
            else
                slots [i] = GetStackTopSlotNumber (o);

_          (Pop (o));
        }

        op = c_intSelectOps [type - c_m3Type_i32] [opIndex];
    }
    else if (not IsStackPolymorphic (o))
        _throw (m3Err_functionStackUnderrun);

    EmitOp (o, op);
    for (u32 i = 0; i < 3; i++)
    {
        if (IsValidSlot (slots [i]))
            EmitSlotOffset (o, slots [i]);
    }
_   (PushRegister (o, type));

    _catch: return result;
}

static
M3Result  Compile_Drop  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result = Pop (o);                                              if (d_m3LogWasmStack) dump_type_stack (o);
    return result;
}

static
M3Result  Compile_Nop  (IM3Compilation o, m3opcode_t i_opcode)
{
    return m3Err_none;
}

static
M3Result  Compile_Unreachable  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

_   (AddTrapRecord (o));

_   (EmitOp (o, op_Unreachable));
_   (SetStackPolymorphic (o));

    _catch:
    return result;
}


// OPTZ: currently all stack slot indices take up a full word, but
// dual stack source operands could be packed together
static
M3Result  Compile_Operator  (IM3Compilation o, m3opcode_t i_opcode)
{
    M3Result result;

    IM3OpInfo opInfo = GetOpInfo (i_opcode);
    _throwif (m3Err_unknownOpcode, not opInfo);

    IM3Operation op;

    // This preserve is for for FP compare operations.
    // either need additional slot destination operations or the
    // easy fix, move _r0 out of the way.
    // moving out the way might be the optimal solution most often?
    // otherwise, the _r0 reg can get buried down in the stack
    // and be idle & wasted for a moment.
    if (IsFpType (GetStackTopType (o)) and IsIntType (opInfo->type))
    {
_       (PreserveRegisterIfOccupied (o, opInfo->type));
    }

    if (opInfo->stackOffset == 0)
    {
        if (IsStackTopInRegister (o))
        {
            op = opInfo->operations [0]; // _s
        }
        else
        {
_           (PreserveRegisterIfOccupied (o, opInfo->type));
            op = opInfo->operations [1]; // _r
        }
    }
    else
    {
        if (IsStackTopInRegister (o))
        {
            op = opInfo->operations [0];  // _rs

            if (IsStackTopMinus1InRegister (o))
            {                                       d_m3Assert (i_opcode == c_waOp_store_f32 or i_opcode == c_waOp_store_f64);
                op = opInfo->operations [3]; // _rr for fp.store
            }
        }
        else if (IsStackTopMinus1InRegister (o))
        {
            op = opInfo->operations [1]; // _sr

            if (not op)  // must be commutative, then
                op = opInfo->operations [0];
        }
        else
        {
_           (PreserveRegisterIfOccupied (o, opInfo->type));     // _ss
            op = opInfo->operations [2];
        }
    }

    if (op)
    {
_       (EmitOp (o, op));

_       (EmitSlotNumOfStackTopAndPop (o));

        if (opInfo->stackOffset < 0)
_           (EmitSlotNumOfStackTopAndPop (o));

        if (opInfo->type != c_m3Type_none)
_           (PushRegister (o, opInfo->type));
    }
    else
    {
#       ifdef DEBUG
            result = ErrorCompile ("no operation found for opcode", o, "'%s'", opInfo->name);
#       else
            result = ErrorCompile ("no operation found for opcode", o, "%x", i_opcode);
#       endif
        _throw (result);
    }

    _catch: return result;
}

static
M3Result  Compile_Convert  (IM3Compilation o, m3opcode_t i_opcode)
{
_try {
    IM3OpInfo opInfo = GetOpInfo (i_opcode);
    _throwif (m3Err_unknownOpcode, not opInfo);

    bool destInSlot = IsRegisterTypeAllocated (o, opInfo->type);
    bool sourceInSlot = IsStackTopInSlot (o);

    IM3Operation op = opInfo->operations [destInSlot * 2 + sourceInSlot];

_   (EmitOp (o, op));
_   (EmitSlotNumOfStackTopAndPop (o));

    if (destInSlot)
_       (PushAllocatedSlotAndEmit (o, opInfo->type))
    else
_       (PushRegister (o, opInfo->type))

}
    _catch: return result;
}

static
M3Result  Compile_Load_Store  (IM3Compilation o, m3opcode_t i_opcode)
{
_try {
    u32 alignHint, memoryOffset;

_   (ReadLEB_u32 (& alignHint, & o->wasm, o->wasmEnd));
_   (ReadLEB_u32 (& memoryOffset, & o->wasm, o->wasmEnd));
                                                                        m3log (compile, d_indent " (offset = %d)", get_indention_string (o), memoryOffset);
    IM3OpInfo opInfo = GetOpInfo (i_opcode);
    _throwif (m3Err_unknownOpcode, not opInfo);

    if (IsFpType (opInfo->type))
_       (PreserveRegisterIfOccupied (o, c_m3Type_f64));

_   (Compile_Operator (o, i_opcode));

    EmitConstant32 (o, memoryOffset);
}
    _catch: return result;
}


M3Result  CompileRawFunction  (IM3Module io_module,  IM3Function io_function, const void * i_function, const void * i_userdata)
{
    d_m3Assert (io_module->runtime);

    IM3CodePage page = AcquireCodePageWithCapacity (io_module->runtime, 4);

    if (page)
    {
        io_function->compiled = GetPagePC (page);
        io_function->module = io_module;

        EmitWord (page, op_CallRawFunction);
        EmitWord (page, i_function);
        EmitWord (page, io_function);
        EmitWord (page, i_userdata);

        ReleaseCodePage (io_module->runtime, page);
        return m3Err_none;
    }
    else {
        return m3Err_mallocFailedCodePage;
    }
}



// d_logOp, d_logOp2 macros aren't actually used by the compiler, just codepage decoding (d_m3LogCodePages = 1)
#define d_logOp(OP)                         { op_##OP,                  NULL,                       NULL,                       NULL }
#define d_logOp2(OP1,OP2)                   { op_##OP1,                 op_##OP2,                   NULL,                       NULL }

#define d_emptyOpList                       { NULL,                     NULL,                       NULL,                       NULL }
#define d_unaryOpList(TYPE, NAME)           { op_##TYPE##_##NAME##_r,   op_##TYPE##_##NAME##_s,     NULL,                       NULL }
#define d_binOpList(TYPE, NAME)             { op_##TYPE##_##NAME##_rs,  op_##TYPE##_##NAME##_sr,    op_##TYPE##_##NAME##_ss,    NULL }
#define d_storeFpOpList(TYPE, NAME)         { op_##TYPE##_##NAME##_rs,  op_##TYPE##_##NAME##_sr,    op_##TYPE##_##NAME##_ss,    op_##TYPE##_##NAME##_rr }
#define d_commutativeBinOpList(TYPE, NAME)  { op_##TYPE##_##NAME##_rs,  NULL,                       op_##TYPE##_##NAME##_ss,    NULL }
#define d_convertOpList(OP)                 { op_##OP##_r_r,            op_##OP##_r_s,              op_##OP##_s_r,              op_##OP##_s_s }


const M3OpInfo c_operations [] =
{
    M3OP( "unreachable",         0, none,   d_logOp (Unreachable),              Compile_Unreachable ),  // 0x00
    M3OP( "nop",                 0, none,   d_emptyOpList,                      Compile_Nop ),          // 0x01 .
    M3OP( "block",               0, none,   d_emptyOpList,                      Compile_LoopOrBlock ),  // 0x02
    M3OP( "loop",                0, none,   d_logOp (Loop),                     Compile_LoopOrBlock ),  // 0x03
    M3OP( "if",                 -1, none,   d_emptyOpList,                      Compile_If ),           // 0x04
    M3OP( "else",                0, none,   d_emptyOpList,                      Compile_Nop ),          // 0x05

    M3OP_RESERVED, M3OP_RESERVED, M3OP_RESERVED, M3OP_RESERVED, M3OP_RESERVED,                          // 0x06...0x0a

    M3OP( "end",                 0, none,   d_emptyOpList,                      Compile_End ),          // 0x0b
    M3OP( "br",                  0, none,   d_logOp (Branch),                   Compile_Branch ),       // 0x0c
    M3OP( "br_if",              -1, none,   d_logOp2 (BranchIf_r, BranchIf_s),  Compile_Branch ),       // 0x0d
    M3OP( "br_table",           -1, none,   d_logOp (BranchTable),              Compile_BranchTable ),  // 0x0e
    M3OP( "return",              0, any,    d_logOp (Return),                   Compile_Return ),       // 0x0f
    M3OP( "call",                0, any,    d_logOp (Call),                     Compile_Call ),         // 0x10
    M3OP( "call_indirect",       0, any,    d_logOp (CallIndirect),             Compile_CallIndirect ), // 0x11
    M3OP( "return_call",         0, any,    d_emptyOpList,                      Compile_Call ),         // 0x12 TODO: Optimize
    M3OP( "return_call_indirect",0, any,    d_emptyOpList,                      Compile_CallIndirect ), // 0x13

    M3OP_RESERVED,  M3OP_RESERVED,                                                                      // 0x14...
    M3OP_RESERVED,  M3OP_RESERVED, M3OP_RESERVED, M3OP_RESERVED,                                        // ...0x19

    M3OP( "drop",               -1, none,   d_emptyOpList,                      Compile_Drop ),         // 0x1a
    M3OP( "select",             -2, any,    d_emptyOpList,                      Compile_Select  ),      // 0x1b

    M3OP_RESERVED,  M3OP_RESERVED, M3OP_RESERVED, M3OP_RESERVED,                                        // 0x1c...0x1f

    M3OP( "local.get",          1,  any,    d_emptyOpList,                      Compile_GetLocal ),     // 0x20
    M3OP( "local.set",          1,  none,   d_emptyOpList,                      Compile_SetLocal ),     // 0x21
    M3OP( "local.tee",          0,  any,    d_emptyOpList,                      Compile_SetLocal ),     // 0x22
    M3OP( "global.get",         1,  none,   d_emptyOpList,                      Compile_GetSetGlobal ), // 0x23
    M3OP( "global.set",         1,  none,   d_emptyOpList,                      Compile_GetSetGlobal ), // 0x24

    M3OP_RESERVED,  M3OP_RESERVED, M3OP_RESERVED,                                                       // 0x25...0x27

    M3OP( "i32.load",           0,  i_32,   d_unaryOpList (i32, Load_i32),      Compile_Load_Store ),   // 0x28
    M3OP( "i64.load",           0,  i_64,   d_unaryOpList (i64, Load_i64),      Compile_Load_Store ),   // 0x29
    M3OP_F( "f32.load",         0,  f_32,   d_unaryOpList (f32, Load_f32),      Compile_Load_Store ),   // 0x2a
    M3OP_F( "f64.load",         0,  f_64,   d_unaryOpList (f64, Load_f64),      Compile_Load_Store ),   // 0x2b

    M3OP( "i32.load8_s",        0,  i_32,   d_unaryOpList (i32, Load_i8),       Compile_Load_Store ),   // 0x2c
    M3OP( "i32.load8_u",        0,  i_32,   d_unaryOpList (i32, Load_u8),       Compile_Load_Store ),   // 0x2d
    M3OP( "i32.load16_s",       0,  i_32,   d_unaryOpList (i32, Load_i16),      Compile_Load_Store ),   // 0x2e
    M3OP( "i32.load16_u",       0,  i_32,   d_unaryOpList (i32, Load_u16),      Compile_Load_Store ),   // 0x2f

    M3OP( "i64.load8_s",        0,  i_64,   d_unaryOpList (i64, Load_i8),       Compile_Load_Store ),   // 0x30
    M3OP( "i64.load8_u",        0,  i_64,   d_unaryOpList (i64, Load_u8),       Compile_Load_Store ),   // 0x31
    M3OP( "i64.load16_s",       0,  i_64,   d_unaryOpList (i64, Load_i16),      Compile_Load_Store ),   // 0x32
    M3OP( "i64.load16_u",       0,  i_64,   d_unaryOpList (i64, Load_u16),      Compile_Load_Store ),   // 0x33
    M3OP( "i64.load32_s",       0,  i_64,   d_unaryOpList (i64, Load_i32),      Compile_Load_Store ),   // 0x34
    M3OP( "i64.load32_u",       0,  i_64,   d_unaryOpList (i64, Load_u32),      Compile_Load_Store ),   // 0x35

    M3OP( "i32.store",          -2, none,   d_binOpList (i32, Store_i32),       Compile_Load_Store ),   // 0x36
    M3OP( "i64.store",          -2, none,   d_binOpList (i64, Store_i64),       Compile_Load_Store ),   // 0x37
    M3OP_F( "f32.store",        -2, none,   d_storeFpOpList (f32, Store_f32),   Compile_Load_Store ),   // 0x38
    M3OP_F( "f64.store",        -2, none,   d_storeFpOpList (f64, Store_f64),   Compile_Load_Store ),   // 0x39

    M3OP( "i32.store8",         -2, none,   d_binOpList (i32, Store_u8),        Compile_Load_Store ),   // 0x3a
    M3OP( "i32.store16",        -2, none,   d_binOpList (i32, Store_i16),       Compile_Load_Store ),   // 0x3b

    M3OP( "i64.store8",         -2, none,   d_binOpList (i64, Store_u8),        Compile_Load_Store ),   // 0x3c
    M3OP( "i64.store16",        -2, none,   d_binOpList (i64, Store_i16),       Compile_Load_Store ),   // 0x3d
    M3OP( "i64.store32",        -2, none,   d_binOpList (i64, Store_i32),       Compile_Load_Store ),   // 0x3e

    M3OP( "memory.size",        1,  i_32,   d_logOp (MemSize),                  Compile_Memory_Size ),  // 0x3f
    M3OP( "memory.grow",        1,  i_32,   d_logOp (MemGrow),                  Compile_Memory_Grow ),  // 0x40

    M3OP( "i32.const",          1,  i_32,   d_logOp (Const32),                  Compile_Const_i32 ),    // 0x41
    M3OP( "i64.const",          1,  i_64,   d_logOp (Const64),                  Compile_Const_i64 ),    // 0x42
    M3OP_F( "f32.const",        1,  f_32,   d_emptyOpList,                      Compile_Const_f32 ),    // 0x43
    M3OP_F( "f64.const",        1,  f_64,   d_emptyOpList,                      Compile_Const_f64 ),    // 0x44

    M3OP( "i32.eqz",            0,  i_32,   d_unaryOpList (i32, EqualToZero)        , NULL  ),          // 0x45
    M3OP( "i32.eq",             -1, i_32,   d_commutativeBinOpList (i32, Equal)     , NULL  ),          // 0x46
    M3OP( "i32.ne",             -1, i_32,   d_commutativeBinOpList (i32, NotEqual)  , NULL  ),          // 0x47
    M3OP( "i32.lt_s",           -1, i_32,   d_binOpList (i32, LessThan)             , NULL  ),          // 0x48
    M3OP( "i32.lt_u",           -1, i_32,   d_binOpList (u32, LessThan)             , NULL  ),          // 0x49
    M3OP( "i32.gt_s",           -1, i_32,   d_binOpList (i32, GreaterThan)          , NULL  ),          // 0x4a
    M3OP( "i32.gt_u",           -1, i_32,   d_binOpList (u32, GreaterThan)          , NULL  ),          // 0x4b
    M3OP( "i32.le_s",           -1, i_32,   d_binOpList (i32, LessThanOrEqual)      , NULL  ),          // 0x4c
    M3OP( "i32.le_u",           -1, i_32,   d_binOpList (u32, LessThanOrEqual)      , NULL  ),          // 0x4d
    M3OP( "i32.ge_s",           -1, i_32,   d_binOpList (i32, GreaterThanOrEqual)   , NULL  ),          // 0x4e
    M3OP( "i32.ge_u",           -1, i_32,   d_binOpList (u32, GreaterThanOrEqual)   , NULL  ),          // 0x4f

    M3OP( "i64.eqz",            0,  i_32,   d_unaryOpList (i64, EqualToZero)        , NULL  ),          // 0x50
    M3OP( "i64.eq",             -1, i_32,   d_commutativeBinOpList (i64, Equal)     , NULL  ),          // 0x51
    M3OP( "i64.ne",             -1, i_32,   d_commutativeBinOpList (i64, NotEqual)  , NULL  ),          // 0x52
    M3OP( "i64.lt_s",           -1, i_32,   d_binOpList (i64, LessThan)             , NULL  ),          // 0x53
    M3OP( "i64.lt_u",           -1, i_32,   d_binOpList (u64, LessThan)             , NULL  ),          // 0x54
    M3OP( "i64.gt_s",           -1, i_32,   d_binOpList (i64, GreaterThan)          , NULL  ),          // 0x55
    M3OP( "i64.gt_u",           -1, i_32,   d_binOpList (u64, GreaterThan)          , NULL  ),          // 0x56
    M3OP( "i64.le_s",           -1, i_32,   d_binOpList (i64, LessThanOrEqual)      , NULL  ),          // 0x57
    M3OP( "i64.le_u",           -1, i_32,   d_binOpList (u64, LessThanOrEqual)      , NULL  ),          // 0x58
    M3OP( "i64.ge_s",           -1, i_32,   d_binOpList (i64, GreaterThanOrEqual)   , NULL  ),          // 0x59
    M3OP( "i64.ge_u",           -1, i_32,   d_binOpList (u64, GreaterThanOrEqual)   , NULL  ),          // 0x5a

    M3OP_F( "f32.eq",           -1, i_32,   d_commutativeBinOpList (f32, Equal)     , NULL  ),          // 0x5b
    M3OP_F( "f32.ne",           -1, i_32,   d_commutativeBinOpList (f32, NotEqual)  , NULL  ),          // 0x5c
    M3OP_F( "f32.lt",           -1, i_32,   d_binOpList (f32, LessThan)             , NULL  ),          // 0x5d
    M3OP_F( "f32.gt",           -1, i_32,   d_binOpList (f32, GreaterThan)          , NULL  ),          // 0x5e
    M3OP_F( "f32.le",           -1, i_32,   d_binOpList (f32, LessThanOrEqual)      , NULL  ),          // 0x5f
    M3OP_F( "f32.ge",           -1, i_32,   d_binOpList (f32, GreaterThanOrEqual)   , NULL  ),          // 0x60

    M3OP_F( "f64.eq",           -1, i_32,   d_commutativeBinOpList (f64, Equal)     , NULL  ),          // 0x61
    M3OP_F( "f64.ne",           -1, i_32,   d_commutativeBinOpList (f64, NotEqual)  , NULL  ),          // 0x62
    M3OP_F( "f64.lt",           -1, i_32,   d_binOpList (f64, LessThan)             , NULL  ),          // 0x63
    M3OP_F( "f64.gt",           -1, i_32,   d_binOpList (f64, GreaterThan)          , NULL  ),          // 0x64
    M3OP_F( "f64.le",           -1, i_32,   d_binOpList (f64, LessThanOrEqual)      , NULL  ),          // 0x65
    M3OP_F( "f64.ge",           -1, i_32,   d_binOpList (f64, GreaterThanOrEqual)   , NULL  ),          // 0x66

    M3OP( "i32.clz",            0,  i_32,   d_unaryOpList (u32, Clz)                , NULL  ),          // 0x67
    M3OP( "i32.ctz",            0,  i_32,   d_unaryOpList (u32, Ctz)                , NULL  ),          // 0x68
    M3OP( "i32.popcnt",         0,  i_32,   d_unaryOpList (u32, Popcnt)             , NULL  ),          // 0x69

    M3OP( "i32.add",            -1, i_32,   d_commutativeBinOpList (i32, Add)       , NULL  ),          // 0x6a
    M3OP( "i32.sub",            -1, i_32,   d_binOpList (i32, Subtract)             , NULL  ),          // 0x6b
    M3OP( "i32.mul",            -1, i_32,   d_commutativeBinOpList (i32, Multiply)  , NULL  ),          // 0x6c
    M3OP( "i32.div_s",          -1, i_32,   d_binOpList (i32, Divide)               , NULL  ),          // 0x6d
    M3OP( "i32.div_u",          -1, i_32,   d_binOpList (u32, Divide)               , NULL  ),          // 0x6e
    M3OP( "i32.rem_s",          -1, i_32,   d_binOpList (i32, Remainder)            , NULL  ),          // 0x6f
    M3OP( "i32.rem_u",          -1, i_32,   d_binOpList (u32, Remainder)            , NULL  ),          // 0x70
    M3OP( "i32.and",            -1, i_32,   d_commutativeBinOpList (u32, And)       , NULL  ),          // 0x71
    M3OP( "i32.or",             -1, i_32,   d_commutativeBinOpList (u32, Or)        , NULL  ),          // 0x72
    M3OP( "i32.xor",            -1, i_32,   d_commutativeBinOpList (u32, Xor)       , NULL  ),          // 0x73
    M3OP( "i32.shl",            -1, i_32,   d_binOpList (u32, ShiftLeft)            , NULL  ),          // 0x74
    M3OP( "i32.shr_s",          -1, i_32,   d_binOpList (i32, ShiftRight)           , NULL  ),          // 0x75
    M3OP( "i32.shr_u",          -1, i_32,   d_binOpList (u32, ShiftRight)           , NULL  ),          // 0x76
    M3OP( "i32.rotl",           -1, i_32,   d_binOpList (u32, Rotl)                 , NULL  ),          // 0x77
    M3OP( "i32.rotr",           -1, i_32,   d_binOpList (u32, Rotr)                 , NULL  ),          // 0x78

    M3OP( "i64.clz",            0,  i_64,   d_unaryOpList (u64, Clz)                , NULL  ),          // 0x79
    M3OP( "i64.ctz",            0,  i_64,   d_unaryOpList (u64, Ctz)                , NULL  ),          // 0x7a
    M3OP( "i64.popcnt",         0,  i_64,   d_unaryOpList (u64, Popcnt)             , NULL  ),          // 0x7b

    M3OP( "i64.add",            -1, i_64,   d_commutativeBinOpList (i64, Add)       , NULL  ),          // 0x7c
    M3OP( "i64.sub",            -1, i_64,   d_binOpList (i64, Subtract)             , NULL  ),          // 0x7d
    M3OP( "i64.mul",            -1, i_64,   d_commutativeBinOpList (i64, Multiply)  , NULL  ),          // 0x7e
    M3OP( "i64.div_s",          -1, i_64,   d_binOpList (i64, Divide)               , NULL  ),          // 0x7f
    M3OP( "i64.div_u",          -1, i_64,   d_binOpList (u64, Divide)               , NULL  ),          // 0x80
    M3OP( "i64.rem_s",          -1, i_64,   d_binOpList (i64, Remainder)            , NULL  ),          // 0x81
    M3OP( "i64.rem_u",          -1, i_64,   d_binOpList (u64, Remainder)            , NULL  ),          // 0x82
    M3OP( "i64.and",            -1, i_64,   d_commutativeBinOpList (u64, And)       , NULL  ),          // 0x83
    M3OP( "i64.or",             -1, i_64,   d_commutativeBinOpList (u64, Or)        , NULL  ),          // 0x84
    M3OP( "i64.xor",            -1, i_64,   d_commutativeBinOpList (u64, Xor)       , NULL  ),          // 0x85
    M3OP( "i64.shl",            -1, i_64,   d_binOpList (u64, ShiftLeft)            , NULL  ),          // 0x86
    M3OP( "i64.shr_s",          -1, i_64,   d_binOpList (i64, ShiftRight)           , NULL  ),          // 0x87
    M3OP( "i64.shr_u",          -1, i_64,   d_binOpList (u64, ShiftRight)           , NULL  ),          // 0x88
    M3OP( "i64.rotl",           -1, i_64,   d_binOpList (u64, Rotl)                 , NULL  ),          // 0x89
    M3OP( "i64.rotr",           -1, i_64,   d_binOpList (u64, Rotr)                 , NULL  ),          // 0x8a

    M3OP_F( "f32.abs",          0,  f_32,   d_unaryOpList(f32, Abs)                 , NULL  ),          // 0x8b
    M3OP_F( "f32.neg",          0,  f_32,   d_unaryOpList(f32, Negate)              , NULL  ),          // 0x8c
    M3OP_F( "f32.ceil",         0,  f_32,   d_unaryOpList(f32, Ceil)                , NULL  ),          // 0x8d
    M3OP_F( "f32.floor",        0,  f_32,   d_unaryOpList(f32, Floor)               , NULL  ),          // 0x8e
    M3OP_F( "f32.trunc",        0,  f_32,   d_unaryOpList(f32, Trunc)               , NULL  ),          // 0x8f
    M3OP_F( "f32.nearest",      0,  f_32,   d_unaryOpList(f32, Nearest)             , NULL  ),          // 0x90
    M3OP_F( "f32.sqrt",         0,  f_32,   d_unaryOpList(f32, Sqrt)                , NULL  ),          // 0x91

    M3OP_F( "f32.add",          -1, f_32,   d_commutativeBinOpList (f32, Add)       , NULL  ),          // 0x92
    M3OP_F( "f32.sub",          -1, f_32,   d_binOpList (f32, Subtract)             , NULL  ),          // 0x93
    M3OP_F( "f32.mul",          -1, f_32,   d_commutativeBinOpList (f32, Multiply)  , NULL  ),          // 0x94
    M3OP_F( "f32.div",          -1, f_32,   d_binOpList (f32, Divide)               , NULL  ),          // 0x95
    M3OP_F( "f32.min",          -1, f_32,   d_commutativeBinOpList (f32, Min)       , NULL  ),          // 0x96
    M3OP_F( "f32.max",          -1, f_32,   d_commutativeBinOpList (f32, Max)       , NULL  ),          // 0x97
    M3OP_F( "f32.copysign",     -1, f_32,   d_binOpList (f32, CopySign)             , NULL  ),          // 0x98

    M3OP_F( "f64.abs",          0,  f_64,   d_unaryOpList(f64, Abs)                 , NULL  ),          // 0x99
    M3OP_F( "f64.neg",          0,  f_64,   d_unaryOpList(f64, Negate)              , NULL  ),          // 0x9a
    M3OP_F( "f64.ceil",         0,  f_64,   d_unaryOpList(f64, Ceil)                , NULL  ),          // 0x9b
    M3OP_F( "f64.floor",        0,  f_64,   d_unaryOpList(f64, Floor)               , NULL  ),          // 0x9c
    M3OP_F( "f64.trunc",        0,  f_64,   d_unaryOpList(f64, Trunc)               , NULL  ),          // 0x9d
    M3OP_F( "f64.nearest",      0,  f_64,   d_unaryOpList(f64, Nearest)             , NULL  ),          // 0x9e
    M3OP_F( "f64.sqrt",         0,  f_64,   d_unaryOpList(f64, Sqrt)                , NULL  ),          // 0x9f

    M3OP_F( "f64.add",          -1, f_64,   d_commutativeBinOpList (f64, Add)       , NULL  ),          // 0xa0
    M3OP_F( "f64.sub",          -1, f_64,   d_binOpList (f64, Subtract)             , NULL  ),          // 0xa1
    M3OP_F( "f64.mul",          -1, f_64,   d_commutativeBinOpList (f64, Multiply)  , NULL  ),          // 0xa2
    M3OP_F( "f64.div",          -1, f_64,   d_binOpList (f64, Divide)               , NULL  ),          // 0xa3
    M3OP_F( "f64.min",          -1, f_64,   d_commutativeBinOpList (f64, Min)       , NULL  ),          // 0xa4
    M3OP_F( "f64.max",          -1, f_64,   d_commutativeBinOpList (f64, Max)       , NULL  ),          // 0xa5
    M3OP_F( "f64.copysign",     -1, f_64,   d_binOpList (f64, CopySign)             , NULL  ),          // 0xa6

    M3OP( "i32.wrap/i64",       0,  i_32,   d_unaryOpList (i32, Wrap_i64),          NULL    ),          // 0xa7
    M3OP_F( "i32.trunc_s/f32",  0,  i_32,   d_convertOpList (i32_Trunc_f32),        Compile_Convert ),  // 0xa8
    M3OP_F( "i32.trunc_u/f32",  0,  i_32,   d_convertOpList (u32_Trunc_f32),        Compile_Convert ),  // 0xa9
    M3OP_F( "i32.trunc_s/f64",  0,  i_32,   d_convertOpList (i32_Trunc_f64),        Compile_Convert ),  // 0xaa
    M3OP_F( "i32.trunc_u/f64",  0,  i_32,   d_convertOpList (u32_Trunc_f64),        Compile_Convert ),  // 0xab

    M3OP( "i64.extend_s/i32",   0,  i_64,   d_unaryOpList (i64, Extend_i32),        NULL    ),          // 0xac
    M3OP( "i64.extend_u/i32",   0,  i_64,   d_unaryOpList (i64, Extend_u32),        NULL    ),          // 0xad

    M3OP_F( "i64.trunc_s/f32",  0,  i_64,   d_convertOpList (i64_Trunc_f32),        Compile_Convert ),  // 0xae
    M3OP_F( "i64.trunc_u/f32",  0,  i_64,   d_convertOpList (u64_Trunc_f32),        Compile_Convert ),  // 0xaf
    M3OP_F( "i64.trunc_s/f64",  0,  i_64,   d_convertOpList (i64_Trunc_f64),        Compile_Convert ),  // 0xb0
    M3OP_F( "i64.trunc_u/f64",  0,  i_64,   d_convertOpList (u64_Trunc_f64),        Compile_Convert ),  // 0xb1

    M3OP_F( "f32.convert_s/i32",0,  f_32,   d_convertOpList (f32_Convert_i32),      Compile_Convert ),  // 0xb2
    M3OP_F( "f32.convert_u/i32",0,  f_32,   d_convertOpList (f32_Convert_u32),      Compile_Convert ),  // 0xb3
    M3OP_F( "f32.convert_s/i64",0,  f_32,   d_convertOpList (f32_Convert_i64),      Compile_Convert ),  // 0xb4
    M3OP_F( "f32.convert_u/i64",0,  f_32,   d_convertOpList (f32_Convert_u64),      Compile_Convert ),  // 0xb5

    M3OP_F( "f32.demote/f64",   0,  f_32,   d_unaryOpList (f32, Demote_f64),        NULL    ),          // 0xb6

    M3OP_F( "f64.convert_s/i32",0,  f_64,   d_convertOpList (f64_Convert_i32),      Compile_Convert ),  // 0xb7
    M3OP_F( "f64.convert_u/i32",0,  f_64,   d_convertOpList (f64_Convert_u32),      Compile_Convert ),  // 0xb8
    M3OP_F( "f64.convert_s/i64",0,  f_64,   d_convertOpList (f64_Convert_i64),      Compile_Convert ),  // 0xb9
    M3OP_F( "f64.convert_u/i64",0,  f_64,   d_convertOpList (f64_Convert_u64),      Compile_Convert ),  // 0xba

    M3OP_F( "f64.promote/f32",  0,  f_64,   d_unaryOpList (f64, Promote_f32),       NULL    ),          // 0xbb

    M3OP_F( "i32.reinterpret/f32",0,i_32,   d_convertOpList (i32_Reinterpret_f32),  Compile_Convert ),  // 0xbc
    M3OP_F( "i64.reinterpret/f64",0,i_64,   d_convertOpList (i64_Reinterpret_f64),  Compile_Convert ),  // 0xbd
    M3OP_F( "f32.reinterpret/i32",0,f_32,   d_convertOpList (f32_Reinterpret_i32),  Compile_Convert ),  // 0xbe
    M3OP_F( "f64.reinterpret/i64",0,f_64,   d_convertOpList (f64_Reinterpret_i64),  Compile_Convert ),  // 0xbf

    M3OP( "i32.extend8_s",       0,  i_32,   d_unaryOpList (i32, Extend8_s),        NULL    ),          // 0xc0
    M3OP( "i32.extend16_s",      0,  i_32,   d_unaryOpList (i32, Extend16_s),       NULL    ),          // 0xc1
    M3OP( "i64.extend8_s",       0,  i_64,   d_unaryOpList (i64, Extend8_s),        NULL    ),          // 0xc2
    M3OP( "i64.extend16_s",      0,  i_64,   d_unaryOpList (i64, Extend16_s),       NULL    ),          // 0xc3
    M3OP( "i64.extend32_s",      0,  i_64,   d_unaryOpList (i64, Extend32_s),       NULL    ),          // 0xc4

# ifdef DEBUG // for codepage logging. the order doesn't matter:
#   define d_m3DebugOp(OP) M3OP (#OP, 0, none, { op_##OP })

# if d_m3HasFloat
#   define d_m3DebugTypedOp(OP) M3OP (#OP, 0, none, { op_##OP##_i32, op_##OP##_i64, op_##OP##_f32, op_##OP##_f64, })
# else
#   define d_m3DebugTypedOp(OP) M3OP (#OP, 0, none, { op_##OP##_i32, op_##OP##_i64 })
# endif

    d_m3DebugOp (Compile),          d_m3DebugOp (Entry),            d_m3DebugOp (End),
    d_m3DebugOp (Unsupported),      d_m3DebugOp (CallRawFunction),

    d_m3DebugOp (GetGlobal_s32),    d_m3DebugOp (GetGlobal_s64),    d_m3DebugOp (ContinueLoop),     d_m3DebugOp (ContinueLoopIf),

    d_m3DebugOp (CopySlot_32),      d_m3DebugOp (PreserveCopySlot_32), d_m3DebugOp (If_s),          d_m3DebugOp (BranchIfPrologue_s),
    d_m3DebugOp (CopySlot_64),      d_m3DebugOp (PreserveCopySlot_64), d_m3DebugOp (If_r),          d_m3DebugOp (BranchIfPrologue_r),

    d_m3DebugOp (Select_i32_rss),   d_m3DebugOp (Select_i32_srs),   d_m3DebugOp (Select_i32_ssr),   d_m3DebugOp (Select_i32_sss),
    d_m3DebugOp (Select_i64_rss),   d_m3DebugOp (Select_i64_srs),   d_m3DebugOp (Select_i64_ssr),   d_m3DebugOp (Select_i64_sss),

# if d_m3HasFloat
    d_m3DebugOp (Select_f32_sss),   d_m3DebugOp (Select_f32_srs),   d_m3DebugOp (Select_f32_ssr),
    d_m3DebugOp (Select_f32_rss),   d_m3DebugOp (Select_f32_rrs),   d_m3DebugOp (Select_f32_rsr),

    d_m3DebugOp (Select_f64_sss),   d_m3DebugOp (Select_f64_srs),   d_m3DebugOp (Select_f64_ssr),
    d_m3DebugOp (Select_f64_rss),   d_m3DebugOp (Select_f64_rrs),   d_m3DebugOp (Select_f64_rsr),
# endif

    d_m3DebugOp (MemFill),          d_m3DebugOp (MemCopy),

    d_m3DebugTypedOp (SetGlobal),   d_m3DebugOp (SetGlobal_s32),    d_m3DebugOp (SetGlobal_s64),

    d_m3DebugTypedOp (SetRegister), d_m3DebugTypedOp (SetSlot),     d_m3DebugTypedOp (PreserveSetSlot),
# endif

# if d_m3CascadedOpcodes
    [c_waOp_extended] = M3OP( "0xFC", 0, c_m3Type_unknown,   d_emptyOpList,  Compile_ExtendedOpcode ),
# endif

# ifdef DEBUG
    M3OP( "termination", 0, c_m3Type_unknown ) // for find_operation_info
# endif
};

const M3OpInfo c_operationsFC [] =
{
    M3OP_F( "i32.trunc_s:sat/f32",0,  i_32,   d_convertOpList (i32_TruncSat_f32),        Compile_Convert ),  // 0x00
    M3OP_F( "i32.trunc_u:sat/f32",0,  i_32,   d_convertOpList (u32_TruncSat_f32),        Compile_Convert ),  // 0x01
    M3OP_F( "i32.trunc_s:sat/f64",0,  i_32,   d_convertOpList (i32_TruncSat_f64),        Compile_Convert ),  // 0x02
    M3OP_F( "i32.trunc_u:sat/f64",0,  i_32,   d_convertOpList (u32_TruncSat_f64),        Compile_Convert ),  // 0x03
    M3OP_F( "i64.trunc_s:sat/f32",0,  i_64,   d_convertOpList (i64_TruncSat_f32),        Compile_Convert ),  // 0x04
    M3OP_F( "i64.trunc_u:sat/f32",0,  i_64,   d_convertOpList (u64_TruncSat_f32),        Compile_Convert ),  // 0x05
    M3OP_F( "i64.trunc_s:sat/f64",0,  i_64,   d_convertOpList (i64_TruncSat_f64),        Compile_Convert ),  // 0x06
    M3OP_F( "i64.trunc_u:sat/f64",0,  i_64,   d_convertOpList (u64_TruncSat_f64),        Compile_Convert ),  // 0x07

    M3OP_RESERVED, M3OP_RESERVED,

    M3OP( "memory.copy",            0,  none,   d_emptyOpList,                           Compile_Memory_CopyFill ), // 0x0a
    M3OP( "memory.fill",            0,  none,   d_emptyOpList,                           Compile_Memory_CopyFill ), // 0x0b


# ifdef DEBUG
    M3OP( "termination", 0, c_m3Type_unknown ) // for find_operation_info
# endif
};


IM3OpInfo  GetOpInfo  (m3opcode_t opcode)
{
    switch (opcode >> 8) {
    case 0x00:
        if (M3_LIKELY(opcode < M3_COUNT_OF(c_operations))) {
            return &c_operations[opcode];
        }
        break;
    case c_waOp_extended:
        opcode &= 0xFF;
        if (M3_LIKELY(opcode < M3_COUNT_OF(c_operationsFC))) {
            return &c_operationsFC[opcode];
        }
        break;
    }
    return NULL;
}

M3Result  CompileBlockStatements  (IM3Compilation o)
{
    M3Result result = m3Err_none;
    bool validEnd = false;

    while (o->wasm < o->wasmEnd)
    {
# if d_m3EnableOpTracing
        if (o->numEmits)
        {
            EmitOp          (o, op_DumpStack);
            EmitConstant32  (o, o->numOpcodes);
            EmitConstant32  (o, GetMaxUsedSlotPlusOne(o));
            EmitPointer     (o, o->function);

            o->numEmits = 0;
        }
# endif
        m3opcode_t opcode;
        o->lastOpcodeStart = o->wasm;
_       (Read_opcode (& opcode, & o->wasm, o->wasmEnd));                log_opcode (o, opcode);

        // Restrict opcodes when evaluating expressions
        if (not o->function) {
            switch (opcode) {
            case c_waOp_i32_const: case c_waOp_i64_const:
            case c_waOp_f32_const: case c_waOp_f64_const:
            case c_waOp_getGlobal: case c_waOp_end:
                break;
            default:
                _throw(m3Err_restrictedOpcode);
            }
        }

        IM3OpInfo opinfo = GetOpInfo (opcode);

        if (opinfo == NULL)
            _throw (ErrorCompile (m3Err_unknownOpcode, o, "opcode '%x' not available", opcode));

        if (opinfo->compiler) {
_           ((* opinfo->compiler) (o, opcode))
        } else {
_           (Compile_Operator (o, opcode));
        }

        o->previousOpcode = opcode;

        if (opcode == c_waOp_else)
        {
            _throwif (m3Err_wasmMalformed, o->block.opcode != c_waOp_if);
            validEnd = true;
            break;
        }
        else if (opcode == c_waOp_end)
        {
            validEnd = true;
            break;
        }
    }
    _throwif(m3Err_wasmMalformed, !(validEnd));

_catch:
    return result;
}

static
M3Result  PushBlockResults  (IM3Compilation o)
{
    M3Result result = m3Err_none;

    u16 numResults = GetFuncTypeNumResults (o->block.type);

    for (u16 i = 0; i < numResults; ++i)
    {
        u8 type = GetFuncTypeResultType (o->block.type, i);

        if (i == numResults - 1 and IsFpType (type))
        {
_           (PushRegister (o, type));
        }
        else
_           (PushAllocatedSlot (o, type));
    }

    _catch: return result;
}


M3Result  CompileBlock  (IM3Compilation o, IM3FuncType i_blockType, m3opcode_t i_blockOpcode)
{
                                                                                        d_m3Assert (not IsRegisterAllocated (o, 0));
                                                                                        d_m3Assert (not IsRegisterAllocated (o, 1));
    M3CompilationScope outerScope = o->block;
    M3CompilationScope * block = & o->block;

    block->outer            = & outerScope;
    block->pc               = GetPagePC (o->page);
    block->patches          = NULL;
    block->type             = i_blockType;
    block->depth            ++;
    block->opcode           = i_blockOpcode;

    /*
     The block stack frame is a little strange but for good reasons.  Because blocks need to be restarted to
     compile different pathways (if/else), the incoming params must be saved.  The parameters are popped
     and validated.  But, then the stack top is readjusted so they aren't subsequently overwritten.
     Next, the result are preallocated to find destination slots.  But again these are immediately popped
     (deallocated) and the stack top is readjusted to keep these records in pace. This allows branch instructions
     to find their result landing pads.  Finally, the params are copied from the "dead" records and pushed back
     onto the stack as active stack items for the CompileBlockStatements () call.

    [     block      ]
    [     params     ]
    ------------------
    [     result     ]  <---- blockStackIndex
    [      slots     ]
    ------------------
    [   saved param  ]
    [     records    ]
                        <----- exitStackIndex
    */

_try {
    // validate and dealloc params ----------------------------

    u16 stackIndex = o->stackIndex;

    u16 numParams = GetFuncTypeNumParams (i_blockType);

    if (i_blockOpcode != c_waOp_else)
    {
        for (u16 i = 0; i < numParams; ++i)
        {
            u8 type = GetFuncTypeParamType (i_blockType, numParams - 1 - i);
_           (PopType (o, type));
        }
    }
    else {
        if (IsStackPolymorphic (o) && o->block.blockStackIndex + numParams > o->stackIndex) {
            o->stackIndex = o->block.blockStackIndex;
        } else {
            o->stackIndex -= numParams;
        }
    }

    u16 paramIndex = o->stackIndex;
    block->exitStackIndex = paramIndex; // consume the params at block exit

    // keep copies of param slots in the stack
    o->stackIndex = stackIndex;

    // find slots for the results ----------------------------
    PushBlockResults (o);

    stackIndex = o->stackIndex;

    // dealloc but keep record of the result slots in the stack
    u16 numResults = GetFuncTypeNumResults (i_blockType);
    while (numResults--)
        Pop (o);

    block->blockStackIndex = o->stackIndex = stackIndex;

    // push the params back onto the stack -------------------
    for (u16 i = 0; i < numParams; ++i)
    {
        u8 type = GetFuncTypeParamType (i_blockType, i);

        u16 slot = GetSlotForStackIndex (o, paramIndex + i);
        Push (o, type, slot);

        if (slot >= o->slotFirstDynamicIndex && slot != c_slotUnused)
            MarkSlotsAllocatedByType (o, slot, type);
    }

    //--------------------------------------------------------

_   (CompileBlockStatements (o));

_   (ValidateBlockEnd (o));

    if (o->function)    // skip for expressions
    {
        if (not IsStackPolymorphic (o))
_           (ResolveBlockResults (o, & o->block, /* isBranch: */ false));

_       (UnwindBlockStack (o))

        if (not ((i_blockOpcode == c_waOp_if and numResults) or o->previousOpcode == c_waOp_else))
        {
            o->stackIndex = o->block.exitStackIndex;
_           (PushBlockResults (o));
        }
    }

    PatchBranches (o);

    o->block = outerScope;

}   _catch: return result;
}

static
M3Result  CompileLocals  (IM3Compilation o)
{
    M3Result result;

    u32 numLocals = 0;
    u32 numLocalBlocks;
_   (ReadLEB_u32 (& numLocalBlocks, & o->wasm, o->wasmEnd));

    for (u32 l = 0; l < numLocalBlocks; ++l)
    {
        u32 varCount;
        i8 waType;
        u8 localType;

_       (ReadLEB_u32 (& varCount, & o->wasm, o->wasmEnd));
_       (ReadLEB_i7 (& waType, & o->wasm, o->wasmEnd));
_       (NormalizeType (& localType, waType));
        numLocals += varCount;                                                          m3log (compile, "pushing locals. count: %d; type: %s", varCount, c_waTypes [localType]);
        while (varCount--)
_           (PushAllocatedSlot (o, localType));
    }

    if (o->function)
        o->function->numLocals = numLocals;

    _catch: return result;
}

static
M3Result  ReserveConstants  (IM3Compilation o)
{
    M3Result result = m3Err_none;

    // in the interest of speed, this blindly scans the Wasm code looking for any byte
    // that looks like an const opcode.
    u16 numConstantSlots = 0;

    bytes_t wa = o->wasm;
    while (wa < o->wasmEnd)
    {
        u8 code = * wa++;
        u16 addSlots = 0;

        if (code == c_waOp_i32_const or code == c_waOp_f32_const)
            addSlots = 1;
        else if (code == c_waOp_i64_const or code == c_waOp_f64_const)
            addSlots = GetTypeNumSlots (c_m3Type_i64);

        if (numConstantSlots + addSlots >= d_m3MaxConstantTableSize)
            break;

        numConstantSlots += addSlots;
    }

    // if constants overflow their reserved stack space, the compiler simply emits op_Const
    // operations as needed. Compiled expressions (global inits) don't pass through this
    // ReserveConstants function and thus always produce inline constants.

    AlignSlotToType (& numConstantSlots, c_m3Type_i64);                                         m3log (compile, "reserved constant slots: %d", numConstantSlots);

    o->slotFirstDynamicIndex = o->slotFirstConstIndex + numConstantSlots;

    if (o->slotFirstDynamicIndex >= d_m3MaxFunctionSlots)
        _throw (m3Err_functionStackOverflow);

    _catch:
    return result;
}


M3Result  CompileFunction  (IM3Function io_function)
{
    if (!io_function->wasm) return "function body is missing";

    IM3FuncType funcType = io_function->funcType;                   m3log (compile, "compiling: [%d] %s %s; wasm-size: %d",
                                                                        io_function->index, m3_GetFunctionName (io_function), SPrintFuncTypeSignature (funcType), (u32) (io_function->wasmEnd - io_function->wasm));
    IM3Runtime runtime = io_function->module->runtime;

    IM3Compilation o = & runtime->compilation;                      d_m3Assert (d_m3MaxFunctionSlots >= d_m3MaxFunctionStackHeight * (d_m3Use32BitSlots + 1))  // need twice as many slots in 32-bit mode
    memset (o, 0x0, sizeof (M3Compilation));

    o->runtime  = runtime;
    o->module   = io_function->module;
    o->function = io_function;
    o->wasm     = io_function->wasm;
    o->wasmEnd  = io_function->wasmEnd;
    o->block.type = funcType;

_try {
    // skip over code size. the end was already calculated during parse phase
    u32 size;
_   (ReadLEB_u32 (& size, & o->wasm, o->wasmEnd));                  d_m3Assert (size == (o->wasmEnd - o->wasm))

_   (AcquireCompilationCodePage (o, & o->page));

    pc_t pc = GetPagePC (o->page);

    u16 numRetSlots = GetFunctionNumReturns (o->function) * c_ioSlotCount;

    for (u16 i = 0; i < numRetSlots; ++i)
        MarkSlotAllocated (o, i);

    o->function->numRetSlots = o->slotFirstDynamicIndex = numRetSlots;

    u16 numArgs = GetFunctionNumArgs (o->function);

    // push the arg types to the type stack
    for (u16 i = 0; i < numArgs; ++i)
    {
        u8 type = GetFunctionArgType (o->function, i);
_       (PushAllocatedSlot (o, type));

        // prevent allocator fill-in
        o->slotFirstDynamicIndex += c_ioSlotCount;
    }

    o->slotMaxAllocatedIndexPlusOne = o->function->numRetAndArgSlots = o->slotFirstLocalIndex = o->slotFirstDynamicIndex;

_   (CompileLocals (o));

    u16 maxSlot = GetMaxUsedSlotPlusOne (o);

    o->function->numLocalBytes = (maxSlot - o->slotFirstLocalIndex) * sizeof (m3slot_t);

    o->slotFirstConstIndex = o->slotMaxConstIndex = maxSlot;

    // ReserveConstants initializes o->firstDynamicSlotNumber
_   (ReserveConstants (o));

    // start tracking the max stack used (Push() also updates this value) so that op_Entry can precisely detect stack overflow
    o->maxStackSlots = o->slotMaxAllocatedIndexPlusOne = o->slotFirstDynamicIndex;

    o->block.blockStackIndex = o->stackFirstDynamicIndex = o->stackIndex;                           m3log (compile, "start stack index: %d",
                                                                                                          (u32) o->stackFirstDynamicIndex);
_   (EmitOp (o, op_Entry));
    EmitPointer (o, io_function);

_   (CompileBlockStatements (o));

    // TODO: validate opcode sequences
    _throwif(m3Err_wasmMalformed, o->previousOpcode != c_waOp_end);

    io_function->compiled = pc;
    io_function->maxStackSlots = o->maxStackSlots;

    u16 numConstantSlots = o->slotMaxConstIndex - o->slotFirstConstIndex;                           m3log (compile, "unique constant slots: %d; unused slots: %d",
                                                                                                           numConstantSlots, o->slotFirstDynamicIndex - o->slotMaxConstIndex);
    io_function->numConstantBytes = numConstantSlots * sizeof (m3slot_t);

    if (numConstantSlots)
    {
        io_function->constants = m3_CopyMem (o->constants, io_function->numConstantBytes);
        _throwifnull(io_function->constants);
    }

} _catch:

    ReleaseCompilationCodePage (o);

    return result;
}
