// This file is part of the Anti-Lag 2.0 SDK.
//
// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#pragma once

namespace AMD {
namespace AntiLag2DX12 {

    struct Context;

    // Initialize function - call this once before the Update function.
    // context - Declare a persistent Context variable in your game code. Ensure the contents are zero'ed, and pass the address in to initialize it.
    //           Be sure to use the *same* context object everywhere when calling the Anti-Lag 2.0 SDK functions.
    // device - The game's D3D12 device.
    // A return value of S_OK indicates that Anti-Lag 2.0 is available on the system.
    HRESULT Initialize( Context* context, ID3D12Device* device );

    // DeInitialize function - call this before destroying the device.
    // context - address of the game's context object.
    // The return value is the reference count of the internal API. It should be 0.
    ULONG DeInitialize( Context* context );

    // Update function - call this just before the input to the game is polled.
    // context - address of the game's context object.
    // enable - enables or disables Anti-Lag 2.0.
    // maxFPS - sets a framerate limit. Zero will disable the limiter.
    HRESULT Update( Context* context, bool enable, unsigned int maxFPS );

    // Call this on the game render thread once the game's main rendering workload has been submitted in an ExecuteCommandLists call.
    // Call before the FSR 3 Present call is made.
    // This is only required if frame generation is enabled, but calling it anyway is harmless.
    // context - address of the game's context object.
    HRESULT MarkEndOfFrameRendering( Context* context );

    // Call this on the presentation thread just before the Present call.
    // This is only required if frame generation is enabled, but calling it anyway is harmless.
    // context - address of the game's context object.
    // bInterpolatedFrame - whether the frame about to be presented is interpolated.
    HRESULT SetFrameGenFrameType( Context* context, bool bInterpolatedFrame );

    //
    // End of public API section.
    // Private implementation details below.
    //

    // Forward declaration of the Anti-Lag interface into the DX12 driver
    MIDL_INTERFACE("44085fbe-e839-40c5-bf38-0ebc5ab4d0a6")
    IAmdExtAntiLagApi: public IUnknown
    {
    public:
        virtual HRESULT UpdateAntiLagState(VOID* pData) = 0;
    };

    // Context structure for the SDK. Declare a persistent object of this type *once* in your game code.
    // Ensure the contents are initialized to zero before calling Initialize() but do not modify these members directly after that.
    struct Context
    {
        IAmdExtAntiLagApi*  m_pAntiLagAPI = nullptr;
        bool                m_enabled = false;
        unsigned int        m_maxFPS = 0;
    };

    // Structure version 1 for Anti-Lag 2.0:
    struct APIData_v1
    {
        unsigned int    uiSize;
        unsigned int    uiVersion;
        unsigned int    eMode;
        const char*     sControlStr;
        unsigned int    uiControlStrLength;
        unsigned int    maxFPS;
    };
    static_assert(sizeof(APIData_v1) == 32, "Check structure packing compiler settings.");

    // Structure version 2 for Anti-Lag 2.0:
    struct APIData_v2
    {
        unsigned int    uiSize;
        unsigned int    uiVersion;
        struct Flags
        {
            unsigned int unused0               : 1;
            unsigned int unused1               : 1;

            unsigned int signalFgFrameType     : 1;
            unsigned int isInterpolatedFrame   : 1;

            unsigned int signalGetUserInputIdx : 1;
            unsigned int signalEndOfFrameIdx   : 1;

            unsigned int reserved              :26;
        }               flags;
        unsigned __int64    iiFrameIdx;
        unsigned __int64    uiiReserved[19];
    };
    static_assert(sizeof(APIData_v2) == 176, "Check structure packing compiler settings.");

    inline HRESULT Initialize( Context* context, ID3D12Device* device )
    {
        HRESULT hr = E_INVALIDARG;
        if ( context && device && context->m_pAntiLagAPI == nullptr )
        {
            HMODULE hModule = GetModuleHandleA("amdxc64.dll");
            if ( hModule )
            {
                typedef HRESULT(__cdecl* PFNAmdExtD3DCreateInterface)( IUnknown* pOuter, REFIID riid, void** ppvObject );
                PFNAmdExtD3DCreateInterface AmdExtD3DCreateInterface = reinterpret_cast<PFNAmdExtD3DCreateInterface>( (VOID*)GetProcAddress(hModule, "AmdExtD3DCreateInterface") );
                if ( AmdExtD3DCreateInterface )
                {
                    hr = AmdExtD3DCreateInterface( device, __uuidof(IAmdExtAntiLagApi), (void**)&context->m_pAntiLagAPI );
                    if ( hr == S_OK && context->m_pAntiLagAPI )
                    {
                        APIData_v1 data = {};
                        data.uiSize = sizeof(data);
                        data.uiVersion = 1;
                        data.eMode = 2; // Anti-Lag 2.0 is disabled during initialization
                        data.sControlStr = nullptr;
                        data.uiControlStrLength = 0;
                        data.maxFPS = 0;

                        hr = context->m_pAntiLagAPI->UpdateAntiLagState( &data );
                    }

                    if ( hr != S_OK )
                    {
                        DeInitialize( context );
                    }
                }
            }
            else
            {
                hr = E_HANDLE;
            }
        }
        return hr;
    }

    inline ULONG DeInitialize( Context* context )
    {
        ULONG refCount = 0;
        if ( context )
        {
            if ( context->m_pAntiLagAPI )
            {
                refCount = context->m_pAntiLagAPI->Release();
                context->m_pAntiLagAPI = nullptr;
            }
            context->m_enabled = false;
        }

        return refCount;
    }

    inline HRESULT Update( Context* context, bool enabled, unsigned int maxFPS )
    {
        // This function needs to be called once per frame, before the user input
        // is sampled - or optionally also when the UI settings are modified.
        if ( context && context->m_pAntiLagAPI )
        {
            // Update the Anti-Lag 2.0 internal state only when necessary:
            if ( context->m_enabled != enabled || context->m_maxFPS != maxFPS )
            {
                context->m_enabled = enabled;
                context->m_maxFPS = maxFPS;

                APIData_v1 data = {};
                data.uiSize = sizeof(data);
                data.uiVersion = 1;
                data.eMode = enabled ? 1 : 2;
                data.sControlStr = nullptr;
                data.uiControlStrLength = 0;
                data.maxFPS = maxFPS;

                // Only call the function with non-null arguments when setting state.
                // Make sure not to set the state every frame.
                context->m_pAntiLagAPI->UpdateAntiLagState( &data );
            }

            // Call the function with a nullptr to insert the latency-reducing delay.
            // (if the state has not been set to 'enabled' this call will have no effect)
            HRESULT hr = context->m_pAntiLagAPI->UpdateAntiLagState( nullptr );
            if ( hr == S_OK || hr == S_FALSE )
            {
                return S_OK;
            }
            else
            {
                return hr;
            }
        }
        else
        {
            return E_NOINTERFACE;
        }
    }

    // Internal helper function
    inline HRESULT SetFrameGenParamsInternal( Context* context, APIData_v2::Flags flags )
    {
        if ( context && context->m_pAntiLagAPI )
        {
            APIData_v2 data = {};
            data.uiSize = sizeof(data);
            data.uiVersion = 2;
            data.flags = flags;
            data.iiFrameIdx = 0;

            return context->m_pAntiLagAPI->UpdateAntiLagState( &data );
        }
        else
        {
            return E_NOINTERFACE;
        }
    }

    inline HRESULT MarkEndOfFrameRendering( Context* context )
    {
        APIData_v2::Flags flags   = {};
        flags.signalEndOfFrameIdx = 1;
        return SetFrameGenParamsInternal( context, flags );
    }

    inline HRESULT SetFrameGenFrameType( Context* context, bool bInterpolatedFrame )
    {
        APIData_v2::Flags flags   = {};
        flags.signalFgFrameType   = 1;
        flags.isInterpolatedFrame = bInterpolatedFrame ? 1 : 0;
        return SetFrameGenParamsInternal( context, flags );
    }
} // namespace AntiLag2DX12
} // namespace AMD
