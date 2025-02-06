//
//  m3_function.h
//
//  Created by Steven Massey on 4/7/21.
//  Copyright © 2021 Steven Massey. All rights reserved.
//

#ifndef m3_function_h
#define m3_function_h

#include "m3_core.h"

d_m3BeginExternC

//---------------------------------------------------------------------------------------------------------------------------------

typedef struct M3FuncType
{
    struct M3FuncType *     next;

    u16                     numRets;
    u16                     numArgs;
    u8                      types [];        // returns, then args
}
M3FuncType;

typedef M3FuncType *        IM3FuncType;


M3Result    AllocFuncType                   (IM3FuncType * o_functionType, u32 i_numTypes);
bool        AreFuncTypesEqual               (const IM3FuncType i_typeA, const IM3FuncType i_typeB);

u16         GetFuncTypeNumParams            (const IM3FuncType i_funcType);
u8          GetFuncTypeParamType            (const IM3FuncType i_funcType, u16 i_index);

u16         GetFuncTypeNumResults           (const IM3FuncType i_funcType);
u8          GetFuncTypeResultType           (const IM3FuncType i_funcType, u16 i_index);

//---------------------------------------------------------------------------------------------------------------------------------

typedef struct M3Function
{
    struct M3Module *       module;

    M3ImportInfo            import;

    bytes_t                 wasm;
    bytes_t                 wasmEnd;

    cstr_t                  names[d_m3MaxDuplicateFunctionImpl];
    cstr_t                  export_name;                            // should be a part of "names"
    u16                     numNames;                               // maximum of d_m3MaxDuplicateFunctionImpl

    IM3FuncType             funcType;

    pc_t                    compiled;

# if (d_m3EnableCodePageRefCounting)
    IM3CodePage *           codePageRefs;                           // array of all pages used
    u32                     numCodePageRefs;
# endif

# if defined (DEBUG)
    u32                     hits;
    u32                     index;
# endif

    u16                     maxStackSlots;

    u16                     numRetSlots;
    u16                     numRetAndArgSlots;

    u16                     numLocals;                              // not including args
    u16                     numLocalBytes;

    bool                    ownsWasmCode;

    u16                     numConstantBytes;
    void *                  constants;
}
M3Function;

void        Function_Release            (IM3Function i_function);
void        Function_FreeCompiledCode   (IM3Function i_function);

cstr_t      GetFunctionImportModuleName (IM3Function i_function);
cstr_t *    GetFunctionNames            (IM3Function i_function, u16 * o_numNames);
u16         GetFunctionNumArgs          (IM3Function i_function);
u8          GetFunctionArgType          (IM3Function i_function, u32 i_index);

u16         GetFunctionNumReturns       (IM3Function i_function);
u8          GetFunctionReturnType       (const IM3Function i_function, u16 i_index);

u32         GetFunctionNumArgsAndLocals (IM3Function i_function);

cstr_t      SPrintFunctionArgList       (IM3Function i_function, m3stack_t i_sp);

//---------------------------------------------------------------------------------------------------------------------------------


d_m3EndExternC

#endif /* m3_function_h */
