//
//  m3_code.h
//
//  Created by Steven Massey on 4/19/19.
//  Copyright © 2019 Steven Massey. All rights reserved.
//

#ifndef m3_code_h
#define m3_code_h

#include "m3_core.h"

d_m3BeginExternC

typedef struct M3CodePage
{
    M3CodePageHeader        info;
    code_t                  code                [1];
}
M3CodePage;

typedef M3CodePage *    IM3CodePage;


IM3CodePage             NewCodePage             (IM3Runtime i_runtime, u32 i_minNumLines);

void                    FreeCodePages           (IM3CodePage * io_list);

u32                     NumFreeLines            (IM3CodePage i_page);
pc_t                    GetPageStartPC          (IM3CodePage i_page);
pc_t                    GetPagePC               (IM3CodePage i_page);
void                    EmitWord_impl           (IM3CodePage i_page, void* i_word);
void                    EmitWord32              (IM3CodePage i_page, u32 i_word);
void                    EmitWord64              (IM3CodePage i_page, u64 i_word);
# if d_m3RecordBacktraces
void                    EmitMappingEntry        (IM3CodePage i_page, u32 i_moduleOffset);
# endif // d_m3RecordBacktraces

void                    PushCodePage            (IM3CodePage * io_list, IM3CodePage i_codePage);
IM3CodePage             PopCodePage             (IM3CodePage * io_list);

IM3CodePage             GetEndCodePage          (IM3CodePage i_list); // i_list = NULL is valid
u32                     CountCodePages          (IM3CodePage i_list); // i_list = NULL is valid

# if d_m3RecordBacktraces
bool                    ContainsPC              (IM3CodePage i_page, pc_t i_pc);
bool                    MapPCToOffset           (IM3CodePage i_page, pc_t i_pc, u32 * o_moduleOffset);
# endif // d_m3RecordBacktraces

# ifdef DEBUG
void                    dump_code_page            (IM3CodePage i_codePage, pc_t i_startPC);
# endif

#define EmitWord(page, val) EmitWord_impl(page, (void*)(val))

//---------------------------------------------------------------------------------------------------------------------------------

# if d_m3RecordBacktraces

typedef struct M3CodeMapEntry
{
    u32          pcOffset;
    u32          moduleOffset;
}
M3CodeMapEntry;

typedef struct M3CodeMappingPage
{
    pc_t              basePC;
    u32               size;
    u32               capacity;
    M3CodeMapEntry    entries     [];
}
M3CodeMappingPage;

# endif // d_m3RecordBacktraces

d_m3EndExternC

#endif // m3_code_h
