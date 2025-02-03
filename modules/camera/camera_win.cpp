/**************************************************************************/
/*  camera_win.cpp                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "camera_win.h"

//////////////////////////////////////////////////////////////////////////
// MediaFoundationCapture

#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <dshow.h> // IAMVideoProcAmp and friends
#include <objbase.h> // IID_PPV_ARGS and friends
#include <shlwapi.h> // QITAB and friends

#include <math.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////////////////////
// Different versions of MinGW have different levels of support for the MediaFoundation API
// and it's corresponding symbols.

#if defined(__MINGW32__) || defined(__MINGW64__)

// Replaced with:
EXTERN_GUID(WINCAM_MF_MT_FRAME_SIZE, 0x1652c33d, 0xd6b2, 0x4012, 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d);
EXTERN_GUID(WINCAM_MF_MT_SUBTYPE, 0xf7e34c9a, 0x42e8, 0x4714, 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5);

EXTERN_GUID(WINCAM_MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, 0x60d0e559, 0x52f8, 0x4fa2, 0xbb, 0xce, 0xac, 0xdb, 0x34, 0xa8, 0xec, 0x1);
EXTERN_GUID(WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, 0x58f0aad8, 0x22bf, 0x4f8a, 0xbb, 0x3d, 0xd2, 0xc4, 0x97, 0x8c, 0x6e, 0x2f);
EXTERN_GUID(WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 0xc60ac5fe, 0x252a, 0x478f, 0xa0, 0xef, 0xbc, 0x8f, 0xa5, 0xf7, 0xca, 0xd3);
EXTERN_GUID(WINCAM_MF_READWRITE_DISABLE_CONVERTERS, 0x98d5b065, 0x1374, 0x4847, 0x8d, 0x5d, 0x31, 0x52, 0x0f, 0xee, 0x71, 0x56);

// Some MinGW versions provides incorrect for MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID (https://sourceforge.net/p/mingw-w64/mailman/message/36875669/)
EXTERN_GUID(WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID, 0x8ac3587a, 0x4ae7, 0x42d8, 0x99, 0xe0, 0x0a, 0x60, 0x13, 0xee, 0xf9, 0x0f);

#if __MINGW64_VERSION_MAJOR < 7

typedef struct {
	const IID *piid;
	DWORD dwOffset;
} QITAB, *LPQITAB;
typedef const QITAB *LPCQITAB;

#ifndef OFFSETOFCLASS
#define OFFSETOFCLASS(base, derived) ((DWORD)(DWORD_PTR)(static_cast<base *>((derived *)8)) - 8)
#endif
#ifndef QITABENTMULTI
#define QITABENTMULTI(Cthis, Ifoo, Iimpl) \
	{ &__uuidof(Ifoo), OFFSETOFCLASS(Iimpl, Cthis) }
#endif
#ifndef QITABENT
#define QITABENT(Cthis, Ifoo) QITABENTMULTI(Cthis, Ifoo, Ifoo)
#endif

#endif // mingw version check

#elif defined(_MSC_VER) // If not using MingW and using MSVC

#define WINCAM_MF_MT_FRAME_SIZE (MF_MT_FRAME_SIZE)
#define WINCAM_MF_MT_SUBTYPE (MF_MT_SUBTYPE)
#define WINCAM_MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME (MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME)
#define WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK (MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK)
#define WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE (MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE)
#define WINCAM_MF_READWRITE_DISABLE_CONVERTERS (MF_READWRITE_DISABLE_CONVERTERS)
#define WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID (MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID)

#endif

///////////////////////////////////////////////////////////////////////////////////////////////////
// Different MinGW versions *may* get linker errors for some of these functions, so we hook them
// up at runtime instead.
// Windows benefits too (e.g. QISearch must be linked dynamically on Windows 7).

HINSTANCE h_mf_dll = NULL;
HINSTANCE h_mfreadwrite_dll = NULL;
HINSTANCE h_shlwapi_dll = NULL;

typedef HRESULT(__cdecl *MFCreateSourceReaderFromMediaSource_t)(IMFMediaSource *pMediaSource, IMFAttributes *pAttributes, IMFSourceReader **ppSourceReader);
typedef HRESULT(__cdecl *QISearch_t)(void *that, LPCQITAB pqit, REFIID riid, void **ppv);
typedef HRESULT(__cdecl *MFEnumDeviceSources_t)(IMFAttributes *pAttributes, IMFActivate ***pppSourceActivate, UINT32 *pcSourceActivate);

MFCreateSourceReaderFromMediaSource_t mgh_proc_MFCreateSourceReaderFromMediaSource = NULL;
QISearch_t mgh_proc_QISearch = NULL;
MFEnumDeviceSources_t mgh_proc_MFEnumDeviceSources = NULL;

void mgh_init() {
	if (h_mf_dll == NULL) {
		h_mf_dll = LoadLibrary(TEXT("Mf.dll"));
		if (h_mf_dll != NULL) {
			mgh_proc_MFEnumDeviceSources = (MFEnumDeviceSources_t)GetProcAddress(h_mf_dll, "MFEnumDeviceSources");
			if (mgh_proc_MFEnumDeviceSources == NULL) {
				ERR_PRINT("WARNING: Couldn't dynamically link MFCreateSourceReaderFromMediaSource");
			}
		} else {
			ERR_PRINT("WARNING: Couldn't load Mf.dll");
		}
	}
	if (h_mfreadwrite_dll == NULL) {
		h_mfreadwrite_dll = LoadLibrary(TEXT("Mfreadwrite.dll"));
		if (h_mfreadwrite_dll != NULL) {
			mgh_proc_MFCreateSourceReaderFromMediaSource = (MFCreateSourceReaderFromMediaSource_t)GetProcAddress(h_mfreadwrite_dll, "MFCreateSourceReaderFromMediaSource");
			if (mgh_proc_MFCreateSourceReaderFromMediaSource == NULL) {
				ERR_PRINT("WARNING: Couldn't dynamically link MFCreateSourceReaderFromMediaSource");
			}
		} else {
			ERR_PRINT("WARNING: Couldn't load MFreadwrite.dll");
		}
	}
	if (h_shlwapi_dll == NULL) {
		h_shlwapi_dll = LoadLibrary(TEXT("Shlwapi.dll"));
		if (h_shlwapi_dll != NULL) {
			const int qisearch_ordinal = 219; // See https://docs.microsoft.com/en-us/windows/win32/api/shlwapi/nf-shlwapi-qisearch
			mgh_proc_QISearch = (QISearch_t)GetProcAddress(h_shlwapi_dll, MAKEINTRESOURCEA(qisearch_ordinal));
			if (mgh_proc_QISearch == NULL) {
				ERR_PRINT("WARNING: Couldn't dynamically link QISearch");
			}
		} else {
			ERR_PRINT("WARNING: Couldn't load Shlwapi.dll");
		}
	}
}

void mgh_free() {
	if (h_mf_dll != NULL) {
		FreeLibrary(h_mf_dll);
		h_mf_dll = NULL;
		mgh_proc_MFEnumDeviceSources = NULL;
	}
	if (h_mfreadwrite_dll != NULL) {
		FreeLibrary(h_mfreadwrite_dll);
		h_mfreadwrite_dll = NULL;
		mgh_proc_MFCreateSourceReaderFromMediaSource = NULL;
	}
	if (h_shlwapi_dll != NULL) {
		FreeLibrary(h_shlwapi_dll);
		h_shlwapi_dll = NULL;
		mgh_proc_QISearch = NULL;
	}
}

HRESULT mgh_MFCreateSourceReaderFromMediaSource(IMFMediaSource *pMediaSource, IMFAttributes *pAttributes, IMFSourceReader **ppSourceReader) {
	if (mgh_proc_MFCreateSourceReaderFromMediaSource != NULL) {
		return (mgh_proc_MFCreateSourceReaderFromMediaSource)(pMediaSource, pAttributes, ppSourceReader);
	} else {
		return MF_E_UNSUPPORTED_SERVICE; // Ideally should be MF_E_DRM_UNSUPPORTED, but this isn't available on early mingw versions
	}
}

HRESULT mgh_QISearch(void *that, LPCQITAB pqit, REFIID riid, void **ppv) {
	if (mgh_proc_QISearch != NULL) {
		return (mgh_proc_QISearch)(that, pqit, riid, ppv);
	} else {
		return E_NOINTERFACE;
	}
}

HRESULT mgh_MFEnumDeviceSources(IMFAttributes *pAttributes, IMFActivate ***pppSourceActivate, UINT32 *pcSourceActivate) {
	if (mgh_proc_MFEnumDeviceSources != NULL) {
		return (mgh_proc_MFEnumDeviceSources)(pAttributes, pppSourceActivate, pcSourceActivate);
	} else {
		return E_NOINTERFACE;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// MediaFoundationCapture class definition; wraps around the native MediaFoundation API

#include <string>

class MediaFoundationCapture : public IMFSourceReaderCallback {
public:
	enum OutputFormat {
		OUTPUT_UNSUPPORTED,
		OUTPUT_RGBA32,
		OUTPUT_RGB24,
		OUTPUT_YUY2,
		OUTPUT_NV12,
	};

	class FrameReader {
	public:
		virtual void prepareToCapture(OutputFormat format) {}

		// May be called on different thread.
		// Do not call any MediaFoundationCapture methods in these callbacks.
		virtual void onCapture(uint8_t *data, int64_t stride, int64_t width, int64_t height) {}
	};

	static int getCaptureDeviceCount();
	static void getCaptureDeviceInfo(int device, std::string *namestr, std::string *uidstr);

	MediaFoundationCapture();
	virtual ~MediaFoundationCapture();
	STDMETHODIMP QueryInterface(REFIID aRiid, void **aPpv);
	STDMETHODIMP_(ULONG)
	AddRef();
	STDMETHODIMP_(ULONG)
	Release();
	STDMETHODIMP OnReadSample(
			HRESULT aStatus,
			DWORD aStreamIndex,
			DWORD aStreamFlags,
			LONGLONG aTimestamp,
			IMFSample *aSample);
	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *);
	STDMETHODIMP OnFlush(DWORD);
	int escapiPropToMFProp(int aProperty);
	BOOL isFormatSupported(REFGUID aSubtype) const;
	HRESULT getFormat(DWORD aIndex, GUID *aSubtype) const;
	HRESULT prepareReaderToCapture(REFGUID aSubtype, FrameReader *reader);
	HRESULT setVideoType(IMFMediaType *aType, FrameReader *reader);
	int isMediaOk(IMFMediaType *aType, int aIndex);
	int scanMediaTypes(unsigned int aWidth, unsigned int aHeight);

	// Initializes capturing for device
	// preferredWidth and preferredHeight should be used to indicate a rough size
	// during render. The given FrameReader instance may receive different dimensions
	// in FrameReader::onCapture
	HRESULT initCapture(int aDevice, FrameReader *reader, int preferredWidth, int preferredHeight);

	// Deinitializes capturing for device
	void deinitCapture();

protected:
	struct SupportedFormat {
		GUID mSubtype;
		OutputFormat mOutputFormat;
	};

	static const SupportedFormat supportedFormats[];
	static const DWORD supportedFormatCount;

	class VideoBufferLock {
	public:
		VideoBufferLock(IMFMediaBuffer *aBuffer);
		~VideoBufferLock();
		HRESULT LockBuffer(
				LONG aDefaultStride, // Minimum stride (with no padding).
				DWORD aHeightInPixels, // Height of the image, in pixels.
				BYTE **aScanLine0, // Receives a pointer to the start of scan line 0.
				LONG *aStride // Receives the actual stride.
		);
		void UnlockBuffer();

	private:
		IMFMediaBuffer *mBuffer;
		IMF2DBuffer *m2DBuffer;
		BOOL mLocked;
	};

	long mRefCount; // Reference count.
	CRITICAL_SECTION mCritsec;

	IMFSourceReader *mReader;
	IMFMediaSource *mSource;

	LONG mDefaultStride;
	OutputFormat mOutputFormat;
	uint64_t mOutputBytesPerPixel;

	unsigned int mCaptureBufferWidth, mCaptureBufferHeight;
	int mErrorLine;
	int mErrorCode;
	unsigned int *mBadIndex;
	unsigned int mBadIndices;
	unsigned int mMaxBadIndices;
	unsigned int mUsedIndex;
	int mRedoFromStart;

	FrameReader *mFrameReader; // Used for handling callbacks
};

const MediaFoundationCapture::SupportedFormat MediaFoundationCapture::supportedFormats[] = {
	{ MFVideoFormat_RGB32, OUTPUT_RGB24 },
	{ MFVideoFormat_RGB24, OUTPUT_RGBA32 },
	{ MFVideoFormat_YUY2, OUTPUT_YUY2 },
	{ MFVideoFormat_NV12, OUTPUT_NV12 }
};

const DWORD MediaFoundationCapture::supportedFormatCount = 4;

///////////////////////////////////////////////////////////////////////////////////////////////////
// Misc helpers

template <class T>
class ScopedRelease {
public:
	T *mVar;
	ScopedRelease(T *aVar) {
		mVar = aVar;
	}
	~ScopedRelease() {
		if (mVar != 0)
			mVar->Release();
	}
};

struct ChooseDeviceParam {
	IMFActivate **mDevices; // Array of IMFActivate pointers.
	UINT32 mCount; // Number of elements in the array.
	UINT32 mSelection; // Selected device, by array index.

	~ChooseDeviceParam() {
		unsigned int i;
		for (i = 0; i < mCount; i++) {
			if (mDevices[i])
				mDevices[i]->Release();
		}
		CoTaskMemFree(mDevices);
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
// VideoBufferLock

MediaFoundationCapture::VideoBufferLock::VideoBufferLock(IMFMediaBuffer *aBuffer) :
		m2DBuffer(NULL), mLocked(FALSE) {
	mBuffer = aBuffer;
	mBuffer->AddRef();

	// Query for the 2-D buffer interface. OK if this fails.
	(void)mBuffer->QueryInterface(IID_PPV_ARGS(&m2DBuffer));
}

MediaFoundationCapture::VideoBufferLock::~VideoBufferLock() {
	UnlockBuffer();
	if (mBuffer)
		mBuffer->Release();
	if (m2DBuffer)
		m2DBuffer->Release();
}

HRESULT MediaFoundationCapture::VideoBufferLock::LockBuffer(
		LONG aDefaultStride, // Minimum stride (with no padding).
		DWORD aHeightInPixels, // Height of the image, in pixels.
		BYTE **aScanLine0, // Receives a pointer to the start of scan line 0.
		LONG *aStride // Receives the actual stride.
) {
	HRESULT hr = S_OK;

	// Use the 2-D version if available.
	if (m2DBuffer) {
		hr = m2DBuffer->Lock2D(aScanLine0, aStride);
	} else {
		// Use non-2D version.
		BYTE *data = NULL;

		hr = mBuffer->Lock(&data, NULL, NULL);
		if (SUCCEEDED(hr)) {
			*aStride = aDefaultStride;
			if (aDefaultStride < 0) {
				// Bottom-up orientation. Return a pointer to the start of the
				// last row *in memory* which is the top row of the image.
				*aScanLine0 = data + abs(aDefaultStride) * (aHeightInPixels - 1);
			} else {
				// Top-down orientation. Return a pointer to the start of the
				// buffer.
				*aScanLine0 = data;
			}
		}
	}

	mLocked = (SUCCEEDED(hr));

	return hr;
}

void MediaFoundationCapture::VideoBufferLock::UnlockBuffer() {
	if (mLocked) {
		if (m2DBuffer) {
			(void)m2DBuffer->Unlock2D();
		} else {
			(void)mBuffer->Unlock();
		}
		mLocked = FALSE;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// MediaFoundationCapture

#define DO_OR_DIE                  \
	{                              \
		if (mErrorLine)            \
			return hr;             \
		if (!SUCCEEDED(hr)) {      \
			mErrorLine = __LINE__; \
			mErrorCode = hr;       \
			return hr;             \
		}                          \
	}
#define DO_OR_DIE_CRITSECTION                \
	{                                        \
		if (mErrorLine) {                    \
			LeaveCriticalSection(&mCritsec); \
			return hr;                       \
		}                                    \
		if (!SUCCEEDED(hr)) {                \
			LeaveCriticalSection(&mCritsec); \
			mErrorLine = __LINE__;           \
			mErrorCode = hr;                 \
			return hr;                       \
		}                                    \
	}

int MediaFoundationCapture::getCaptureDeviceCount() {
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED(hr))
		return 0;

	hr = MFStartup(MF_VERSION);

	if (FAILED(hr))
		return 0;

	// choose device
	IMFAttributes *attributes = NULL;
	hr = MFCreateAttributes(&attributes, 1);
	ScopedRelease<IMFAttributes> attributes_s(attributes);

	if (FAILED(hr))
		return 0;

	hr = attributes->SetGUID(
			WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

	if (FAILED(hr))
		return 0;

	ChooseDeviceParam param = { 0 };
	hr = mgh_MFEnumDeviceSources(attributes, &param.mDevices, &param.mCount);

	if (FAILED(hr))
		return 0;

	return param.mCount;
}

void MediaFoundationCapture::getCaptureDeviceInfo(int device, std::string *namestr, std::string *uidstr) {
	if (namestr == nullptr && uidstr == nullptr) {
		return;
	}

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED(hr))
		return;

	hr = MFStartup(MF_VERSION);

	if (FAILED(hr))
		return;

	// choose device
	IMFAttributes *attributes = NULL;
	hr = MFCreateAttributes(&attributes, 1);
	ScopedRelease<IMFAttributes> attributes_s(attributes);

	if (FAILED(hr))
		return;

	hr = attributes->SetGUID(
			WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

	if (FAILED(hr))
		return;

	ChooseDeviceParam param = { 0 };
	hr = mgh_MFEnumDeviceSources(attributes, &param.mDevices, &param.mCount);

	if (FAILED(hr))
		return;

	if (device < (signed)param.mCount) {
		WCHAR *name = 0;
		UINT32 namelen = 255;
		char buf[255];
		hr = param.mDevices[device]->GetAllocatedString(
				WINCAM_MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
				&name,
				&namelen);
		// Name
		if (namestr != nullptr) {
			if (SUCCEEDED(hr) && name) {
				int len = 0;
				while (len < ((signed)namelen) && name[len] != 0) {
					buf[len] = (char)name[len];
					len++;
				}
				buf[len] = 0;

				*namestr = std::string(buf, len);

				CoTaskMemFree(name);
			}
		}

		// Device unique ID
		if (uidstr != nullptr) {
			hr = param.mDevices[device]->GetAllocatedString(
					WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
					&name,
					&namelen);
			if (SUCCEEDED(hr) && name) {
				int len = 0;
				while (len < ((signed)namelen) && name[len] != 0) {
					buf[len] = (char)name[len];
					len++;
				}
				buf[len] = 0;

				*uidstr = std::string(buf, len);

				CoTaskMemFree(name);
			}
		}
	}
}

MediaFoundationCapture::MediaFoundationCapture() {
	mRefCount = 1;
	mReader = 0;
	InitializeCriticalSection(&mCritsec);
	mCaptureBufferWidth = 0;
	mCaptureBufferHeight = 0;
	mErrorLine = 0;
	mErrorCode = 0;
	mBadIndices = 0;
	mMaxBadIndices = 16;
	mBadIndex = new unsigned int[mMaxBadIndices];
	mRedoFromStart = 0;

	mFrameReader = nullptr;
}

MediaFoundationCapture::~MediaFoundationCapture() {
	DeleteCriticalSection(&mCritsec);
	delete[] mBadIndex;
}

// IUnknown methods
STDMETHODIMP MediaFoundationCapture::QueryInterface(REFIID aRiid, void **aPpv) {
	static const QITAB qit[] = {
		QITABENT(MediaFoundationCapture, IMFSourceReaderCallback),
		{ 0 },
	};
	return mgh_QISearch(this, qit, aRiid, aPpv);
}

STDMETHODIMP_(ULONG)
MediaFoundationCapture::AddRef() {
	return InterlockedIncrement(&mRefCount);
}

STDMETHODIMP_(ULONG)
MediaFoundationCapture::Release() {
	ULONG count = InterlockedDecrement(&mRefCount);
	if (count == 0) {
		delete this;
	}
	// For thread safety, return a temporary variable.
	return count;
}

// IMFSourceReaderCallback methods
STDMETHODIMP MediaFoundationCapture::OnReadSample(
		HRESULT aStatus,
		DWORD aStreamIndex,
		DWORD aStreamFlags,
		LONGLONG aTimestamp,
		IMFSample *aSample) {
	HRESULT hr = S_OK;
	IMFMediaBuffer *mediabuffer = NULL;

	if (FAILED(aStatus)) {
		// Bug workaround: some resolutions may just return error.
		// http://stackoverflow.com/questions/22788043/imfsourcereader-giving-error-0x80070491-for-some-resolutions
		// we fix by marking the resolution bad and retrying, which should use the next best match.
		mRedoFromStart = 1;
		if (mBadIndices == mMaxBadIndices) {
			unsigned int *t = new unsigned int[mMaxBadIndices * 2];
			memcpy(t, mBadIndex, mMaxBadIndices * sizeof(unsigned int));
			delete[] mBadIndex;
			mBadIndex = t;
			mMaxBadIndices *= 2;
		}
		mBadIndex[mBadIndices] = mUsedIndex;
		mBadIndices++;
		return aStatus;
	}

	EnterCriticalSection(&mCritsec);

	if (SUCCEEDED(aStatus)) {
		if (aSample) {
			// Get the video frame buffer from the sample.

			hr = aSample->GetBufferByIndex(0, &mediabuffer);
			ScopedRelease<IMFMediaBuffer> mediabuffer_s(mediabuffer);

			DO_OR_DIE_CRITSECTION;

			// Draw the frame.
			VideoBufferLock buffer(mediabuffer); // Helper object to lock the video buffer.

			BYTE *scanline0 = NULL;
			LONG stride = 0;
			hr = buffer.LockBuffer(mDefaultStride, mCaptureBufferHeight, &scanline0, &stride);

			DO_OR_DIE_CRITSECTION;

			mFrameReader->onCapture(scanline0, int64_t(stride), int64_t(mCaptureBufferWidth), int64_t(mCaptureBufferHeight));
		}
	}

	// Request the next frame.
	hr = mReader->ReadSample(
			(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
			0,
			NULL, // actual
			NULL, // flags
			NULL, // timestamp
			NULL // sample
	);

	DO_OR_DIE_CRITSECTION;

	LeaveCriticalSection(&mCritsec);

	return hr;
}

STDMETHODIMP MediaFoundationCapture::OnEvent(DWORD, IMFMediaEvent *) {
	return S_OK;
}

STDMETHODIMP MediaFoundationCapture::OnFlush(DWORD) {
	return S_OK;
}

BOOL MediaFoundationCapture::isFormatSupported(REFGUID aSubtype) const {
	for (int i = 0; i < (signed)supportedFormatCount; i++) {
		if (aSubtype == supportedFormats[i].mSubtype) {
			return TRUE;
		}
	}
	return FALSE;
}

HRESULT MediaFoundationCapture::getFormat(DWORD aIndex, GUID *aSubtype) const {
	if (aIndex < supportedFormatCount) {
		*aSubtype = supportedFormats[aIndex].mSubtype;
		return S_OK;
	}
	return MF_E_NO_MORE_TYPES;
}

HRESULT MediaFoundationCapture::prepareReaderToCapture(REFGUID aSubtype, FrameReader *reader) {
	mOutputFormat = OUTPUT_UNSUPPORTED;
	for (int i = 0; i < (signed)supportedFormatCount; i++) {
		if (supportedFormats[i].mSubtype == aSubtype) {
			mOutputFormat = supportedFormats[i].mOutputFormat;
			break;
		}
	}
	if (mOutputFormat != OUTPUT_UNSUPPORTED) {
		reader->prepareToCapture(mOutputFormat);
		return S_OK;
	}

	return MF_E_INVALIDMEDIATYPE;
}

HRESULT MediaFoundationCapture::setVideoType(IMFMediaType *aType, FrameReader *reader) {
	HRESULT hr = S_OK;
	GUID subtype = { 0 };

	// Find the video subtype.
	hr = aType->GetGUID(WINCAM_MF_MT_SUBTYPE, &subtype);

	DO_OR_DIE;

	// Choose a conversion function.
	// (This also validates the format type.)

	hr = prepareReaderToCapture(subtype, reader);

	DO_OR_DIE;

	//
	// Get some video attributes.
	//

	subtype = GUID_NULL;

	UINT32 width = 0;
	UINT32 height = 0;

	// Get the subtype and the image size.
	hr = aType->GetGUID(WINCAM_MF_MT_SUBTYPE, &subtype);

	DO_OR_DIE;

	hr = MFGetAttributeSize(aType, WINCAM_MF_MT_FRAME_SIZE, &width, &height);

	DO_OR_DIE;

	hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &mDefaultStride);

	DO_OR_DIE;

	mCaptureBufferWidth = width;
	mCaptureBufferHeight = height;

	return hr;
}

int MediaFoundationCapture::isMediaOk(IMFMediaType *aType, int aIndex) {
	HRESULT hr = S_OK;

	int i;
	for (i = 0; i < (signed)mBadIndices; i++)
		if (mBadIndex[i] == (unsigned)aIndex)
			return FALSE;

	BOOL found = FALSE;
	GUID subtype = { 0 };

	hr = aType->GetGUID(WINCAM_MF_MT_SUBTYPE, &subtype);

	DO_OR_DIE;

	// Do we support this type directly?
	if (isFormatSupported(subtype)) {
		found = TRUE;
	} else {
		for (i = 0;; i++) {
			// Get the i'th format.
			hr = getFormat(i, &subtype);

			if (FAILED(hr)) {
				break;
			}

			hr = aType->SetGUID(WINCAM_MF_MT_SUBTYPE, subtype);

			if (FAILED(hr)) {
				break;
			}

			// Try to set this type on the source reader.
			hr = mReader->SetCurrentMediaType(
					(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
					NULL,
					aType);

			if (SUCCEEDED(hr)) {
				found = TRUE;
				break;
			}
		}
	}
	return found;
}

int MediaFoundationCapture::scanMediaTypes(unsigned int aWidth, unsigned int aHeight) {
	HRESULT hr;
	HRESULT nativeTypeErrorCode = S_OK;
	DWORD count = 0;
	int besterror = 0xfffffff;
	int bestfit = 0;

	while (nativeTypeErrorCode == S_OK && besterror) {
		IMFMediaType *nativeType = NULL;
		nativeTypeErrorCode = mReader->GetNativeMediaType(
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				count,
				&nativeType);
		ScopedRelease<IMFMediaType> nativeType_s(nativeType);

		if (nativeTypeErrorCode != S_OK)
			continue;

		// get the media type
		GUID nativeGuid = { 0 };
		hr = nativeType->GetGUID(WINCAM_MF_MT_SUBTYPE, &nativeGuid);

		if (FAILED(hr))
			return bestfit;

		if (isMediaOk(nativeType, count)) {
			UINT32 width, height;
			hr = MFGetAttributeSize(nativeType, WINCAM_MF_MT_FRAME_SIZE, &width, &height);

			if (FAILED(hr))
				return bestfit;

			int error = 0;

			// prefer (hugely) to get too much than too little data..

			if (aWidth < width)
				error += (width - aWidth);
			if (aHeight < height)
				error += (height - aHeight);
			if (aWidth > width)
				error += (aWidth - width) * 2;
			if (aHeight > height)
				error += (aHeight - height) * 2;

			if (aWidth == width && aHeight == height) // ..but perfect match is a perfect match
				error = 0;

			if (besterror > error) {
				besterror = error;
				bestfit = count;
			}
		}

		count++;
	}
	return bestfit;
}

HRESULT MediaFoundationCapture::initCapture(int aDevice, FrameReader *reader, int preferredWidth, int preferredHeight) {
	if (reader == nullptr) {
		return E_INVALIDARG;
	}

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	DO_OR_DIE;

	hr = MFStartup(MF_VERSION);

	DO_OR_DIE;

	// choose device
	IMFAttributes *attributes = NULL;
	hr = MFCreateAttributes(&attributes, 1);
	ScopedRelease<IMFAttributes> attributes_s(attributes);

	DO_OR_DIE;

	hr = attributes->SetGUID(
			WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			WINCAM_MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

	DO_OR_DIE;

	ChooseDeviceParam param = { 0 };
	hr = mgh_MFEnumDeviceSources(attributes, &param.mDevices, &param.mCount);

	DO_OR_DIE;

	if ((signed)param.mCount > aDevice) {
		IMFAttributes *dev_attributes = NULL;
		IMFMediaType *type = NULL;
		EnterCriticalSection(&mCritsec);

		hr = param.mDevices[aDevice]->ActivateObject(
				__uuidof(IMFMediaSource),
				(void **)&mSource);

		DO_OR_DIE_CRITSECTION;

		hr = MFCreateAttributes(&dev_attributes, 3);
		ScopedRelease<IMFAttributes> dev_attributes_s(dev_attributes);

		DO_OR_DIE_CRITSECTION;

		hr = dev_attributes->SetUINT32(WINCAM_MF_READWRITE_DISABLE_CONVERTERS, TRUE);

		DO_OR_DIE_CRITSECTION;

		hr = dev_attributes->SetUnknown(
				MF_SOURCE_READER_ASYNC_CALLBACK,
				this);

		DO_OR_DIE_CRITSECTION;

		hr = mgh_MFCreateSourceReaderFromMediaSource(
				mSource,
				dev_attributes,
				&mReader);

		DO_OR_DIE_CRITSECTION;

		int preferredmode = scanMediaTypes(preferredWidth, preferredHeight);
		mUsedIndex = preferredmode;

		hr = mReader->GetNativeMediaType(
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				preferredmode,
				&type);
		ScopedRelease<IMFMediaType> type_s(type);

		DO_OR_DIE_CRITSECTION;

		hr = setVideoType(type, reader);

		DO_OR_DIE_CRITSECTION;

		hr = mReader->SetCurrentMediaType(
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				NULL,
				type);

		DO_OR_DIE_CRITSECTION;

		hr = mReader->ReadSample(
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				0,
				NULL,
				NULL,
				NULL,
				NULL);

		DO_OR_DIE_CRITSECTION;

		mFrameReader = reader;

		LeaveCriticalSection(&mCritsec);
	} else {
		return MF_E_INVALIDINDEX;
	}

	return 0;
}

void MediaFoundationCapture::deinitCapture() {
	EnterCriticalSection(&mCritsec);

	mFrameReader = nullptr;

	mReader->Release();

	mSource->Shutdown();
	mSource->Release();

	LeaveCriticalSection(&mCritsec);
}

//////////////////////////////////////////////////////////////////////////
// CameraFeedWindows - Subclass for our camera feed on windows

#include <string.h>

class CameraFeedWindows : public CameraFeed {
protected:
	class FrameReader : public MediaFoundationCapture::FrameReader {
	public:
		FrameReader(CameraFeedWindows *owner) :
				camera_feed(owner) {
			img[0].instance();
			img[1].instance();
		}
		void prepareToCapture(MediaFoundationCapture::OutputFormat format) {
			outputFormat = format;
			switch (outputFormat) {
				case MediaFoundationCapture::OUTPUT_RGB24:
					print_verbose("CameraWindows: Preparing to capture RGB24 stream");
					break;
				case MediaFoundationCapture::OUTPUT_RGBA32:
					print_verbose("CameraWindows: Preparing to capture RGBA32 stream");
					break;
				case MediaFoundationCapture::OUTPUT_NV12:
					print_verbose("CameraWindows: Preparing to capture NV12 stream");
					break;
				case MediaFoundationCapture::OUTPUT_YUY2:
					print_verbose("CameraWindows: Preparing to capture YUY2 stream");
					break;
				default:
					ERR_PRINT("CameraWindows: Unsupported camera stream format");
					break;
			}
		}
		void onCapture(uint8_t *p_data, int64_t p_stride, int64_t p_width, int64_t p_height) {
			switch (outputFormat) {
				case MediaFoundationCapture::OUTPUT_RGB24:
					onCaptureRgb24(p_data, p_stride, p_width, p_height);
					break;
				case MediaFoundationCapture::OUTPUT_RGBA32:
					onCaptureRgba32(p_data, p_stride, p_width, p_height);
					break;
				case MediaFoundationCapture::OUTPUT_NV12:
					onCaptureNV12(p_data, p_stride, p_width, p_height);
					break;
				case MediaFoundationCapture::OUTPUT_YUY2:
					onCaptureYUY2(p_data, p_stride, p_width, p_height);
					break;
				default:
					// Deliberately empty
					break;
			}
		}
		void onCaptureYUY2(uint8_t *p_data, int64_t p_stride, int64_t p_width, int64_t p_height) {
			bool data_valid = p_width * p_height % 2 == 0 && p_width * p_height > 0;
			int64_t y_bytes = 0;
			int64_t uv_bytes = 0;
			if (p_width != last_width || p_height != last_height) {
				last_width = p_width;
				last_height = p_height;
				if (data_valid) {
					y_bytes = (p_width * p_height);
					uv_bytes = (p_width * p_height); // Half of Y's horizontal sampling period, but two bytes for each sample
					img_data.resize(y_bytes);
					img_data2.resize(uv_bytes);
				}
			}
			if (data_valid) {
				PoolVector<uint8_t>::Write y_write = img_data.write();
				uint8_t *y_dst = y_write.ptr();
				PoolVector<uint8_t>::Write uv_write = img_data2.write();
				uint8_t *uv_dst = uv_write.ptr();
				uint8_t *row_src = p_data;
				for (int64_t row = 0; row < p_height; row++) {
					uint8_t *src = row_src;
					for (int64_t x = 0; x < p_width; x += 2) { // UV is half the horizontal resolution of Y
						*y_dst++ = *src++; // Y[x]
						*uv_dst++ = *src++; // U[x]
						*y_dst++ = *src++; // Y[x+1]
						*uv_dst++ = *src++; // V[x]
					}
					row_src += p_stride;
				}
				// Make images
				img[0].instance();
				img[0]->create(p_width, p_height, false, Image::FORMAT_R8, img_data);
				img[1].instance();
				img[1]->create(p_width / 2, p_height, false, Image::FORMAT_RG8, img_data2);
				camera_feed->set_YCbCr_imgs(img[0], img[1]);
			}
		}
		void onCaptureNV12(uint8_t *p_data, int64_t p_stride, int64_t p_width, int64_t p_height) {
			bool data_valid = p_width * p_height % 2 == 0 && p_width * p_height > 0;
			int64_t y_bytes = 0;
			int64_t uv_bytes = 0;
			if (p_width != last_width || p_height != last_height) {
				last_width = p_width;
				last_height = p_height;
				if (data_valid) {
					y_bytes = (p_width * p_height);
					uv_bytes = (p_width * p_height) / 2;
					img_data.resize(y_bytes);
					img_data2.resize(uv_bytes);
				}
			}
			if (data_valid) {
				// First 2 thirds of data are Y
				PoolVector<uint8_t>::Write y_write = img_data.write();
				uint8_t *y_dst = y_write.ptr();
				uint8_t *src = p_data;
				for (int64_t i = 0; i < p_height; i += 1) {
					memcpy((void *)y_dst, (void *)src, p_width);
					src += p_stride;
					y_dst += p_width;
				}
				// Last third of data is interleaved UV
				PoolVector<uint8_t>::Write uv_write = img_data2.write();
				uint8_t *uv_dst = uv_write.ptr();
				for (int64_t i = 0; i < img_data2.size(); i += p_width) {
					memcpy((void *)uv_dst, (void *)src, p_width);
					src += p_stride;
					uv_dst += p_width;
				}
				// Make images
				img[0].instance();
				img[0]->create(p_width, p_height, false, Image::FORMAT_R8, img_data);
				img[1].instance();
				img[1]->create(p_width / 2, p_height / 2, false, Image::FORMAT_RG8, img_data2);
				camera_feed->set_YCbCr_imgs(img[0], img[1]);
			}
		}
		void onCaptureRgb24(uint8_t *p_rgb, int64_t p_stride, int64_t p_width, int64_t p_height) {
			const uint64_t dst_color_channels = 3;
			if (p_width != last_width || p_height != last_height) {
				last_width = p_width;
				last_height = p_height;
				img_data.resize(p_width * p_height * dst_color_channels);
			}
			if (p_width > 0 && p_height > 0) {
				PoolVector<uint8_t>::Write w = img_data.write();
				uint8_t *raw_dst = w.ptr();
				uint8_t *raw_src = p_rgb;
				for (DWORD y = 0; y < p_height; y++) {
					uint8_t *src_row = raw_src;
					uint8_t *dest_pel = raw_dst;
					for (uint64_t x = 0; x < p_width * 3; x += 3) {
						RGBTRIPLE *src_pel = (RGBTRIPLE *)(src_row + x);
						*dest_pel++ = src_pel[x].rgbtRed;
						*dest_pel++ = src_pel[x].rgbtGreen;
						*dest_pel++ = src_pel[x].rgbtBlue;
					}
					raw_src += p_stride;
					raw_dst += p_width * dst_color_channels;
				}
				img[0].instance();
				img[0]->create(p_width, p_height, false, Image::FORMAT_RGB8, img_data);
				camera_feed->set_RGB_img(img[0]);
			}
		}
		void onCaptureRgba32(uint8_t *p_rgb, int64_t p_stride, int64_t p_width, int64_t p_height) {
			// Camera feed doesn't accept image formats with alpha channel, so convert to RGB24
			const uint64_t dst_color_channels = 3;
			if (p_width != last_width || p_height != last_height) {
				last_width = p_width;
				last_height = p_height;
				img_data.resize(p_width * p_height * dst_color_channels);
			}
			if (p_width > 0 && p_height > 0) {
				PoolVector<uint8_t>::Write w = img_data.write();
				uint8_t *raw_dst = w.ptr();
				uint8_t *raw_src = p_rgb;
				for (DWORD y = 0; y < p_height; y++) {
					RGBTRIPLE *src_pel = (RGBTRIPLE *)raw_src;
					uint8_t *dest_pel = raw_dst;
					for (DWORD x = 0; x < p_width; x++) {
						*dest_pel++ = src_pel[x].rgbtRed;
						*dest_pel++ = src_pel[x].rgbtGreen;
						*dest_pel++ = src_pel[x].rgbtBlue;
					}
					raw_src += p_stride;
					raw_dst += p_width * dst_color_channels;
				}
				img[0].instance();
				img[0]->create(p_width, p_height, false, Image::FORMAT_RGB8, img_data);
				camera_feed->set_RGB_img(img[0]);
			}
		}

	private:
		MediaFoundationCapture::OutputFormat outputFormat;
		CameraFeedWindows *camera_feed;
		int last_width = -1;
		int last_height = -1;
		PoolVector<uint8_t> img_data;
		PoolVector<uint8_t> img_data2;
		Ref<Image> img[2];
	};
	FrameReader frame_reader;
	MediaFoundationCapture mf_capture;
	bool capturing;
	int mf_device_id;
	StringName device_uid;

public:
	StringName get_device_uid() { return device_uid; }

	const int MAX_WIDTH = 4096;
	const int MAX_HEIGHT = 4096;

	CameraFeedWindows(int p_mf_device_id, const StringName &p_device_name, const StringName &p_device_uid);
	virtual ~CameraFeedWindows();

	bool activate_feed();
	void deactivate_feed();
};

CameraFeedWindows::CameraFeedWindows(int p_mf_device_id, const StringName &p_device_name, const StringName &p_device_uid) :
		CameraFeed(p_device_name, CameraFeed::FEED_UNSPECIFIED),
		frame_reader(this),
		mf_device_id(p_mf_device_id),
		device_uid(p_device_uid) {
	capturing = false;
}

CameraFeedWindows::~CameraFeedWindows() {
	// make sure we stop recording if we are!
	if (is_active()) {
		deactivate_feed();
	}
}

bool CameraFeedWindows::activate_feed() {
	// Note that initCapture will attempt to find the nearest resolution supported
	// by the device driver, not the actual dimensions specified.
	HRESULT hr = mf_capture.initCapture(
			mf_device_id,
			&frame_reader,
			MAX_WIDTH,
			MAX_HEIGHT);
	capturing = hr == S_OK;
	return capturing;
}

void CameraFeedWindows::deactivate_feed() {
	if (capturing) {
		mf_capture.deinitCapture();
		capturing = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// CameraWindows

void CameraWindows::update_feeds() {
	print_verbose("CameraWindows: updating feeds...");

	// Get all video source unique IDs via media foundation api, comparing with our CameraFeed instances
	// and their unique IDs. Create and destroy CameraFeed instances as required.
	Array feeds = get_feeds();
	Vector<CameraFeedWindows *> check_feeds;
	check_feeds.resize(feeds.size());
	for (int fidx = 0; fidx < feeds.size(); fidx++) {
		check_feeds.set(fidx, Object::cast_to<CameraFeedWindows>(feeds.get(fidx).operator Object *()));
	}
	std::string device_name_buf, device_uid_buf;
	int devices = MediaFoundationCapture::getCaptureDeviceCount();
	for (int mf_device_id = 0; mf_device_id < devices; mf_device_id++) {
		MediaFoundationCapture::getCaptureDeviceInfo(mf_device_id, &device_name_buf, &device_uid_buf);
		StringName name(device_name_buf.c_str());
		StringName uid(device_uid_buf.c_str());
		bool device_needs_feed = true;
		for (int fidx = 0; fidx < check_feeds.size(); fidx++) {
			CameraFeedWindows *feed = check_feeds.get(fidx);
			if (feed != nullptr && feed->get_device_uid() == uid) {
				device_needs_feed = false;
				check_feeds.set(fidx, nullptr);
				break;
			}
		}
		if (device_needs_feed) {
			print_verbose("CameraWindows: Adding feed \"" + name + "\" with unique id \"" + uid + "\"");
			Ref<CameraFeedWindows> newfeed(memnew(CameraFeedWindows(mf_device_id, name, uid)));
			add_feed(newfeed);
		}
	}
	for (int fidx = 0; fidx < check_feeds.size(); fidx++) {
		CameraFeedWindows *feed = check_feeds.get(fidx);
		if (feed != nullptr) {
			print_verbose("CameraWindows: Removing feed \"" + feed->get_name() + "\" with unique id \"" + feed->get_device_uid() + "\"");
			remove_feed(get_feed_by_id(feed->get_id()));
		}
	}
}

CameraWindows::CameraWindows() {
	// Initialise MinGW MediaFoundation linking helpers
	mgh_init();

	// Find cameras active right now
	update_feeds();
}

CameraWindows::~CameraWindows() {
	mgh_free();
}
