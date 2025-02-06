//
//  m3_function.c
//
//  Created by Steven Massey on 4/7/21.
//  Copyright © 2021 Steven Massey. All rights reserved.
//

#include "m3_function.h"
#include "m3_env.h"


M3Result AllocFuncType (IM3FuncType * o_functionType, u32 i_numTypes)
{
    *o_functionType = (IM3FuncType) m3_Malloc ("M3FuncType", sizeof (M3FuncType) + i_numTypes);
    return (*o_functionType) ? m3Err_none : m3Err_mallocFailed;
}


bool  AreFuncTypesEqual  (const IM3FuncType i_typeA, const IM3FuncType i_typeB)
{
    if (i_typeA->numRets == i_typeB->numRets && i_typeA->numArgs == i_typeB->numArgs)
    {
        return (memcmp (i_typeA->types, i_typeB->types, i_typeA->numRets + i_typeA->numArgs) == 0);
    }

    return false;
}

u16  GetFuncTypeNumParams  (const IM3FuncType i_funcType)
{
    return i_funcType ? i_funcType->numArgs : 0;
}


u8  GetFuncTypeParamType  (const IM3FuncType i_funcType, u16 i_index)
{
    u8 type = c_m3Type_unknown;

    if (i_funcType)
    {
        if (i_index < i_funcType->numArgs)
        {
            type = i_funcType->types [i_funcType->numRets + i_index];
        }
    }

    return type;
}



u16  GetFuncTypeNumResults  (const IM3FuncType i_funcType)
{
    return i_funcType ? i_funcType->numRets : 0;
}


u8  GetFuncTypeResultType  (const IM3FuncType i_funcType, u16 i_index)
{
    u8 type = c_m3Type_unknown;

    if (i_funcType)
    {
        if (i_index < i_funcType->numRets)
        {
            type = i_funcType->types [i_index];
        }
    }

    return type;
}


//---------------------------------------------------------------------------------------------------------------


void FreeImportInfo (M3ImportInfo * i_info)
{
    m3_Free (i_info->moduleUtf8);
    m3_Free (i_info->fieldUtf8);
}


void  Function_Release  (IM3Function i_function)
{
    m3_Free (i_function->constants);

    for (int i = 0; i < i_function->numNames; i++)
    {
        // name can be an alias of fieldUtf8
        if (i_function->names[i] != i_function->import.fieldUtf8)
        {
            m3_Free (i_function->names[i]);
        }
    }

    FreeImportInfo (& i_function->import);

    if (i_function->ownsWasmCode)
        m3_Free (i_function->wasm);

    // Function_FreeCompiledCode (func);

#   if (d_m3EnableCodePageRefCounting)
    {
        m3_Free (i_function->codePageRefs);
        i_function->numCodePageRefs = 0;
    }
#   endif
}


void  Function_FreeCompiledCode (IM3Function i_function)
{
#   if (d_m3EnableCodePageRefCounting)
    {
        i_function->compiled = NULL;

        while (i_function->numCodePageRefs--)
        {
            IM3CodePage page = i_function->codePageRefs [i_function->numCodePageRefs];

            if (--(page->info.usageCount) == 0)
            {
//                printf ("free %p\n", page);
            }
        }

        m3_Free (i_function->codePageRefs);

        Runtime_ReleaseCodePages (i_function->module->runtime);
    }
#   endif
}


cstr_t  m3_GetFunctionName  (IM3Function i_function)
{
    u16 numNames = 0;
    cstr_t *names = GetFunctionNames(i_function, &numNames);
    if (numNames > 0)
        return names[0];
    else
        return "<unnamed>";
}


IM3Module  m3_GetFunctionModule  (IM3Function i_function)
{
    return i_function ? i_function->module : NULL;
}


cstr_t *  GetFunctionNames  (IM3Function i_function, u16 * o_numNames)
{
    if (!i_function || !o_numNames)
        return NULL;

    if (i_function->import.fieldUtf8)
    {
        *o_numNames = 1;
        return &i_function->import.fieldUtf8;
    }
    else
    {
        *o_numNames = i_function->numNames;
        return i_function->names;
    }
}


cstr_t  GetFunctionImportModuleName  (IM3Function i_function)
{
    return (i_function->import.moduleUtf8) ? i_function->import.moduleUtf8 : "";
}


u16  GetFunctionNumArgs  (IM3Function i_function)
{
    u16 numArgs = 0;

    if (i_function)
    {
        if (i_function->funcType)
            numArgs = i_function->funcType->numArgs;
    }

    return numArgs;
}

u8  GetFunctionArgType  (IM3Function i_function, u32 i_index)
{
    u8 type = c_m3Type_none;

    if (i_index < GetFunctionNumArgs (i_function))
    {
        u32 numReturns = i_function->funcType->numRets;

        type = i_function->funcType->types [numReturns + i_index];
    }

    return type;
}


u16  GetFunctionNumReturns  (IM3Function i_function)
{
    u16 numReturns = 0;

    if (i_function)
    {
        if (i_function->funcType)
            numReturns = i_function->funcType->numRets;
    }

    return numReturns;
}


u8  GetFunctionReturnType  (const IM3Function i_function, u16 i_index)
{
    return i_function ? GetFuncTypeResultType (i_function->funcType, i_index) : c_m3Type_unknown;
}


u32  GetFunctionNumArgsAndLocals (IM3Function i_function)
{
    if (i_function)
        return i_function->numLocals + GetFunctionNumArgs (i_function);
    else
        return 0;
}

