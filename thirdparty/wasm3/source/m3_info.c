//
//  m3_info.c
//
//  Created by Steven Massey on 4/27/19.
//  Copyright © 2019 Steven Massey. All rights reserved.
//

#include "m3_env.h"
#include "m3_info.h"
#include "m3_compile.h"

#if defined(DEBUG) || (d_m3EnableStrace >= 2)

size_t  SPrintArg  (char * o_string, size_t i_stringBufferSize, voidptr_t i_sp, u8 i_type)
{
    int len = 0;

    * o_string = 0;

    if      (i_type == c_m3Type_i32)
        len = snprintf (o_string, i_stringBufferSize, "%" PRIi32, * (i32 *) i_sp);
    else if (i_type == c_m3Type_i64)
        len = snprintf (o_string, i_stringBufferSize, "%" PRIi64, * (i64 *) i_sp);
#if d_m3HasFloat
    else if (i_type == c_m3Type_f32)
        len = snprintf (o_string, i_stringBufferSize, "%" PRIf32, * (f32 *) i_sp);
    else if (i_type == c_m3Type_f64)
        len = snprintf (o_string, i_stringBufferSize, "%" PRIf64, * (f64 *) i_sp);
#endif

    len = M3_MAX (0, len);

    return len;
}


cstr_t  SPrintFunctionArgList  (IM3Function i_function, m3stack_t i_sp)
{
    int ret;
    static char string [256];

    char * s = string;
    ccstr_t e = string + sizeof(string) - 1;

    ret = snprintf (s, e-s, "(");
    s += M3_MAX (0, ret);

    u64 * argSp = (u64 *) i_sp;

    IM3FuncType funcType = i_function->funcType;
    if (funcType)
    {
        u32 numArgs = funcType->numArgs;

        for (u32 i = 0; i < numArgs; ++i)
        {
            u8 type = d_FuncArgType(funcType, i);

            ret = snprintf (s, e-s, "%s: ", c_waTypes [type]);
            s += M3_MAX (0, ret);

            s += SPrintArg (s, e-s, argSp + i, type);

            if (i != numArgs - 1) {
                ret = snprintf (s, e-s, ", ");
                s += M3_MAX (0, ret);
            }
        }
    }
    else printf ("null signature");

    ret = snprintf (s, e-s, ")");
    s += M3_MAX (0, ret);

    return string;
}

#endif

#ifdef DEBUG

// a central function you can be breakpoint:
void ExceptionBreakpoint (cstr_t i_exception, cstr_t i_message)
{
    printf ("\nexception: '%s' @ %s\n", i_exception, i_message);
    return;
}


typedef struct OpInfo
{
    IM3OpInfo   info;
    m3opcode_t  opcode;
}
OpInfo;

void  m3_PrintM3Info  ()
{
    printf ("\n-- m3 configuration --------------------------------------------\n");
//  printf (" sizeof M3CodePage    : %zu bytes  (%d slots) \n", sizeof (M3CodePage), c_m3CodePageNumSlots);
    printf (" sizeof M3MemPage     : %u bytes              \n", d_m3DefaultMemPageSize);
    printf (" sizeof M3Compilation : %zu bytes             \n", sizeof (M3Compilation));
    printf (" sizeof M3Function    : %zu bytes             \n", sizeof (M3Function));
    printf ("----------------------------------------------------------------\n\n");
}


void *  v_PrintEnvModuleInfo  (IM3Module i_module, u32 * io_index)
{
    printf (" module [%u]  name: '%s'; funcs: %d  \n", * io_index++, i_module->name, i_module->numFunctions);

    return NULL;
}


void  m3_PrintRuntimeInfo  (IM3Runtime i_runtime)
{
    printf ("\n-- m3 runtime -------------------------------------------------\n");

    printf (" stack-size: %zu   \n\n", i_runtime->numStackSlots * sizeof (m3slot_t));

    u32 moduleIndex = 0;
    ForEachModule (i_runtime, (ModuleVisitor) v_PrintEnvModuleInfo, & moduleIndex);

    printf ("----------------------------------------------------------------\n\n");
}


cstr_t  GetTypeName  (u8 i_m3Type)
{
    if (i_m3Type < 5)
        return c_waTypes [i_m3Type];
    else
        return "?";
}


// TODO: these 'static char string []' aren't thread-friendly.  though these functions are
// mainly for simple diagnostics during development, it'd be nice if they were fully reliable.

cstr_t  SPrintFuncTypeSignature  (IM3FuncType i_funcType)
{
    static char string [256];

    sprintf (string, "(");

    for (u32 i = 0; i < i_funcType->numArgs; ++i)
    {
        if (i != 0)
            strcat (string, ", ");

        strcat (string, GetTypeName (d_FuncArgType(i_funcType, i)));
    }

    strcat (string, ") -> ");

    for (u32 i = 0; i < i_funcType->numRets; ++i)
    {
        if (i != 0)
            strcat (string, ", ");

        strcat (string, GetTypeName (d_FuncRetType(i_funcType, i)));
    }

    return string;
}


cstr_t  SPrintValue  (void * i_value, u8 i_type)
{
    static char string [100];
    SPrintArg (string, 100, (m3stack_t) i_value, i_type);
    return string;
}

static
OpInfo find_operation_info  (IM3Operation i_operation)
{
    OpInfo opInfo = { NULL, 0 };

    if (!i_operation) return opInfo;

    // TODO: find also extended opcodes
    for (u32 i = 0; i <= 0xff; ++i)
    {
        IM3OpInfo oi = GetOpInfo (i);

        if (oi->type != c_m3Type_unknown)
        {
            for (u32 o = 0; o < 4; ++o)
            {
                if (oi->operations [o] == i_operation)
                {
                    opInfo.info = oi;
                    opInfo.opcode = i;
                    break;
                }
            }
        }
        else break;
    }

    return opInfo;
}


#undef fetch
#define fetch(TYPE) (* (TYPE *) ((*o_pc)++))

#define d_m3Decoder(FUNC) void Decode_##FUNC (char * o_string, u8 i_opcode, IM3Operation i_operation, IM3OpInfo i_opInfo, pc_t * o_pc)

d_m3Decoder  (Call)
{
    void * function = fetch (void *);
    i32 stackOffset = fetch (i32);

    sprintf (o_string, "%p; stack-offset: %d", function, stackOffset);
}


d_m3Decoder (Entry)
{
    IM3Function function = fetch (IM3Function);

    // only prints out the first registered name for the function
    sprintf (o_string, "%s", m3_GetFunctionName(function));
}


d_m3Decoder (f64_Store)
{
    if (i_operation == i_opInfo->operations [0])
    {
        u32 operand = fetch (u32);
        u32 offset = fetch (u32);

        sprintf (o_string, "offset= slot:%d + immediate:%d", operand, offset);
    }

//    sprintf (o_string, "%s", function->name);
}


d_m3Decoder  (Branch)
{
    void * target = fetch (void *);
    sprintf (o_string, "%p", target);
}

d_m3Decoder  (BranchTable)
{
    u32 slot = fetch (u32);

    o_string += sprintf (o_string, "slot: %" PRIu32 "; targets: ", slot);

//    IM3Function function = fetch2 (IM3Function);

    i32 targets = fetch (i32);

    for (i32 i = 0; i < targets; ++i)
    {
        pc_t addr = fetch (pc_t);
        o_string += sprintf (o_string, "%" PRIi32 "=%p, ", i, addr);
    }

    pc_t addr = fetch (pc_t);
    sprintf (o_string, "def=%p ", addr);
}


d_m3Decoder  (Const)
{
    u64 value = fetch (u64); i32 offset = fetch (i32);
    sprintf (o_string, " slot [%d] = %" PRIu64, offset, value);
}


#undef fetch

void  DecodeOperation  (char * o_string, u8 i_opcode, IM3Operation i_operation, IM3OpInfo i_opInfo, pc_t * o_pc)
{
    #define d_m3Decode(OPCODE, FUNC) case OPCODE: Decode_##FUNC (o_string, i_opcode, i_operation, i_opInfo, o_pc); break;

    switch (i_opcode)
    {
//        d_m3Decode (0xc0,                  Const)
        d_m3Decode (0xc5,                  Entry)
        d_m3Decode (c_waOp_call,           Call)
        d_m3Decode (c_waOp_branch,         Branch)
        d_m3Decode (c_waOp_branchTable,    BranchTable)
        d_m3Decode (0x39,                  f64_Store)
    }
}

// WARNING/TODO: this isn't fully implemented. it blindly assumes each word is a Operation pointer
// and, if an operation happens to missing from the c_operations table it won't be recognized here
void  dump_code_page  (IM3CodePage i_codePage, pc_t i_startPC)
{
        m3log (code, "code page seq: %d", i_codePage->info.sequence);

        pc_t pc = i_startPC ? i_startPC : GetPageStartPC (i_codePage);
        pc_t end = GetPagePC (i_codePage);

        m3log (code, "---------------------------------------------------------------------------------------");

        while (pc < end)
        {
            pc_t operationPC = pc;
            IM3Operation op = (IM3Operation) (* pc++);

                OpInfo i = find_operation_info (op);

                if (i.info)
                {
                    char infoString [8*1024] = { 0 };

                    DecodeOperation (infoString, i.opcode, op, i.info, & pc);

                    m3log (code, "%p | %20s  %s", operationPC, i.info->name, infoString);
                }
                else
                    m3log (code, "%p | %p", operationPC, op);

        }

        m3log (code, "---------------------------------------------------------------------------------------");

        m3log (code, "free-lines: %d", i_codePage->info.numLines - i_codePage->info.lineIndex);
}


void  dump_type_stack  (IM3Compilation o)
{
    /* Reminders about how the stack works! :)
     -- args & locals remain on the type stack for duration of the function. Denoted with a constant 'A' and 'L' in this dump.
     -- the initial stack dumps originate from the CompileLocals () function, so these identifiers won't/can't be
     applied until this compilation stage is finished
     -- constants are not statically represented in the type stack (like args & constants) since they don't have/need
     write counts

     -- the number shown for static args and locals (value in wasmStack [i]) represents the write count for the variable

     -- (does Wasm ever write to an arg? I dunno/don't remember.)
     -- the number for the dynamic stack values represents the slot number.
     -- if the slot index points to arg, local or constant it's denoted with a lowercase 'a', 'l' or 'c'

     */

    // for the assert at end of dump:
    i32 regAllocated [2] = { (i32) IsRegisterAllocated (o, 0), (i32) IsRegisterAllocated (o, 1) };

    // display whether r0 or fp0 is allocated. these should then also be reflected somewhere in the stack too.
    d_m3Log(stack, "\n");
    d_m3Log(stack, "        ");
    printf ("%s %s    ", regAllocated [0] ? "(r0)" : "    ", regAllocated [1] ? "(fp0)" : "     ");
    printf("\n");

    for (u32 p = 1; p <= 2; ++p)
    {
        d_m3Log(stack, "        ");

        for (u16 i = 0; i < o->stackIndex; ++i)
        {
            if (i > 0 and i == o->stackFirstDynamicIndex)
                printf ("#");

            if (i == o->block.blockStackIndex)
                printf (">");

            const char * type = c_waCompactTypes [o->typeStack [i]];

            const char * location = "";

            i32 slot = o->wasmStack [i];

            if (IsRegisterSlotAlias (slot))
            {
                bool isFp = IsFpRegisterSlotAlias (slot);
                location = isFp ? "/f" : "/r";

                regAllocated [isFp]--;
                slot = -1;
            }
            else
            {
                if (slot < o->slotFirstDynamicIndex)
                {
                    if (slot >= o->slotFirstConstIndex)
                        location = "c";
                    else if (slot >= o->function->numRetAndArgSlots)
                        location = "L";
                    else
                        location = "a";
                }
            }

            char item [100];

            if (slot >= 0)
                sprintf (item, "%s%s%d", type, location, slot);
            else
                sprintf (item, "%s%s", type, location);

            if (p == 1)
            {
                size_t s = strlen (item);

                sprintf (item, "%d", i);

                while (strlen (item) < s)
                    strcat (item, " ");
            }

            printf ("|%s ", item);

        }
        printf ("\n");
    }

//    for (u32 r = 0; r < 2; ++r)
//        d_m3Assert (regAllocated [r] == 0);         // reg allocation & stack out of sync

    u16 maxSlot = GetMaxUsedSlotPlusOne (o);

    if (maxSlot > o->slotFirstDynamicIndex)
    {
        d_m3Log (stack, "                      -");

        for (u16 i = o->slotFirstDynamicIndex; i < maxSlot; ++i)
            printf ("----");

        printf ("\n");

        d_m3Log (stack, "                 slot |");
        for (u16 i = o->slotFirstDynamicIndex; i < maxSlot; ++i)
            printf ("%3d|", i);

        printf ("\n");
        d_m3Log (stack, "                alloc |");

        for (u16 i = o->slotFirstDynamicIndex; i < maxSlot; ++i)
        {
            printf ("%3d|", o->m3Slots [i]);
        }

        printf ("\n");
    }
    d_m3Log(stack, "\n");
}


static const char *  GetOpcodeIndentionString  (i32 blockDepth)
{
    blockDepth += 1;

    if (blockDepth < 0)
        blockDepth = 0;

    static const char * s_spaces = ".......................................................................................";
    const char * indent = s_spaces + strlen (s_spaces);
    indent -= (blockDepth * 2);
    if (indent < s_spaces)
        indent = s_spaces;

    return indent;
}


const char *  get_indention_string  (IM3Compilation o)
{
    return GetOpcodeIndentionString (o->block.depth+4);
}


void  log_opcode  (IM3Compilation o, m3opcode_t i_opcode)
{
    i32 depth = o->block.depth;
    if (i_opcode == c_waOp_end or i_opcode == c_waOp_else)
        depth--;

    m3log (compile, "%4d | 0x%02x  %s %s", o->numOpcodes++, i_opcode, GetOpcodeIndentionString (depth), GetOpInfo(i_opcode)->name);
}


void  log_emit  (IM3Compilation o, IM3Operation i_operation)
{
    OpInfo i = find_operation_info (i_operation);

    d_m3Log(emit, "");
    if (i.info)
    {
        printf ("%p: %s\n", GetPagePC (o->page),  i.info->name);
    }
    else printf ("not found: %p\n", i_operation);
}

#endif // DEBUG


# if d_m3EnableOpProfiling

typedef struct M3ProfilerSlot
{
    cstr_t      opName;
    u64         hitCount;
}
M3ProfilerSlot;

static M3ProfilerSlot s_opProfilerCounts [d_m3ProfilerSlotMask + 1] = {};

void  ProfileHit  (cstr_t i_operationName)
{
    u64 ptr = (u64) i_operationName;

    M3ProfilerSlot * slot = & s_opProfilerCounts [ptr & d_m3ProfilerSlotMask];

    if (slot->opName)
    {
        if (slot->opName != i_operationName)
        {
            m3_Abort ("profiler slot collision; increase d_m3ProfilerSlotMask");
        }
    }

    slot->opName = i_operationName;
    slot->hitCount++;
}


void  m3_PrintProfilerInfo  ()
{
    M3ProfilerSlot dummy;
    M3ProfilerSlot * maxSlot = & dummy;

    do
    {
        maxSlot->hitCount = 0;

        for (u32 i = 0; i <= d_m3ProfilerSlotMask; ++i)
        {
            M3ProfilerSlot * slot = & s_opProfilerCounts [i];

            if (slot->opName)
            {
                if (slot->hitCount > maxSlot->hitCount)
                    maxSlot = slot;
            }
        }

        if (maxSlot->opName)
        {
            fprintf (stderr, "%13llu  %s\n", maxSlot->hitCount, maxSlot->opName);
            maxSlot->opName = NULL;
        }
    }
    while (maxSlot->hitCount);
}

# else

void  m3_PrintProfilerInfo  () {}

# endif

