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
#include "servers/camera/camera_feed.h"

// Implemented using Microsoft Media Foundation based on Microsoft sample (MFCaptureD3D).
//
/// @TODO first cut. doesn't work for some webcams / virtual cameras. refactor etc.

//////////////////////////////////////////////////////////////////////////
// CameraFeedWindows - Subclass for our camera feed on windows

/// @TODO need to implement this

#include <comdef.h>

// Media Foundation Includes
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

#include <shlwapi.h>

#pragma comment(lib, "Mfplat.lib")
#pragma comment(lib, "Mf.lib")
#pragma comment(lib, "Mfreadwrite.lib")
#pragma comment(lib, "Mfuuid.lib")

template <class T>
void SafeRelease(T **ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

struct ChooseDeviceParam {
	IMFActivate **ppDevices; // Array of IMFActivate pointers.
	UINT32 count; // Number of elements in the array.
	UINT32 selection; // Selected device, by array index.
};

class VideoBufferLock {
public:
	VideoBufferLock(IMFMediaBuffer *pBuffer) :
			m_p2DBuffer(NULL), m_bLocked(FALSE) {
		m_pBuffer = pBuffer;
		m_pBuffer->AddRef();

		// Query for the 2-D buffer interface. OK if this fails.
		(void)m_pBuffer->QueryInterface(IID_PPV_ARGS(&m_p2DBuffer));
	}

	~VideoBufferLock() {
		UnlockBuffer();
		SafeRelease(&m_pBuffer);
		SafeRelease(&m_p2DBuffer);
	}

	//-------------------------------------------------------------------
	// LockBuffer
	//
	// Locks the buffer. Returns a pointer to scan line 0 and returns the stride.
	//
	// The caller must provide the default stride as an input parameter, in case
	// the buffer does not expose IMF2DBuffer. You can calculate the default stride
	// from the media type.
	//-------------------------------------------------------------------

	HRESULT LockBuffer(
			LONG lDefaultStride, // Minimum stride (with no padding).
			DWORD dwHeightInPixels, // Height of the image, in pixels.
			BYTE **ppbScanLine0, // Receives a pointer to the start of scan line 0.
			LONG *plStride // Receives the actual stride.
	) {
		HRESULT hr = S_OK;

		// Use the 2-D version if available.
		if (m_p2DBuffer) {
			hr = m_p2DBuffer->Lock2D(ppbScanLine0, plStride);
		} else {
			// Use non-2D version.
			BYTE *pData = NULL;

			hr = m_pBuffer->Lock(&pData, NULL, NULL);
			if (SUCCEEDED(hr)) {
				*plStride = lDefaultStride;
				if (lDefaultStride < 0) {
					// Bottom-up orientation. Return a pointer to the start of the
					// last row *in memory* which is the top row of the image.
					*ppbScanLine0 = pData + abs(lDefaultStride) * (dwHeightInPixels - 1);
				} else {
					// Top-down orientation. Return a pointer to the start of the
					// buffer.
					*ppbScanLine0 = pData;
				}
			}
		}

		m_bLocked = (SUCCEEDED(hr));

		return hr;
	}

	//-------------------------------------------------------------------
	// UnlockBuffer
	//
	// Unlocks the buffer. Called automatically by the destructor.
	//-------------------------------------------------------------------

	void UnlockBuffer() {
		if (m_bLocked) {
			if (m_p2DBuffer) {
				(void)m_p2DBuffer->Unlock2D();
			} else {
				(void)m_pBuffer->Unlock();
			}
			m_bLocked = FALSE;
		}
	}

private:
	IMFMediaBuffer *m_pBuffer;
	IMF2DBuffer *m_p2DBuffer;

	BOOL m_bLocked;
};

__forceinline BYTE Clip(int clr) {
	return (BYTE)(clr < 0 ? 0 : (clr > 255 ? 255 : clr));
}

__forceinline RGBQUAD ConvertYCrCbToRGB(
		int y,
		int cr,
		int cb) {
	RGBQUAD rgbq;

	int c = y - 16;
	int d = cb - 128;
	int e = cr - 128;

	rgbq.rgbRed = Clip((298 * c + 409 * e + 128) >> 8);
	rgbq.rgbGreen = Clip((298 * c - 100 * d - 208 * e + 128) >> 8);
	rgbq.rgbBlue = Clip((298 * c + 516 * d + 128) >> 8);

	return rgbq;
}

void TransformImage_YUY2(
		BYTE *pDest,
		LONG lDestStride,
		const BYTE *pSrc,
		LONG lSrcStride,
		DWORD dwWidthInPixels,
		DWORD dwHeightInPixels) {
	for (DWORD y = 0; y < dwHeightInPixels; y++) {
		RGBQUAD *pDestPel = (RGBQUAD *)pDest;
		WORD *pSrcPel = (WORD *)pSrc;

		for (DWORD x = 0; x < dwWidthInPixels; x += 2) {
			// Byte order is U0 Y0 V0 Y1

			int y0 = (int)LOBYTE(pSrcPel[x]);
			int u0 = (int)HIBYTE(pSrcPel[x]);
			int y1 = (int)LOBYTE(pSrcPel[x + 1]);
			int v0 = (int)HIBYTE(pSrcPel[x + 1]);

			pDestPel[x] = ConvertYCrCbToRGB(y0, v0, u0);
			pDestPel[x + 1] = ConvertYCrCbToRGB(y1, v0, u0);
		}

		pSrc += lSrcStride;
		pDest += lDestStride;
	}
}

class CameraFeedWindows : public CameraFeed, public IMFSourceReaderCallback {
private:
	// using ms samples naming
	IMFActivate *pActivate;
	HRESULT CloseDevice();

	// device.h
	UINT m_width;
	UINT m_height;
	const UINT m_depth = 4;
	LONG m_lDefaultStride;
	MFRatio m_PixelAR;
	MFVideoInterlaceMode m_interlace;

	PoolVector<uint8_t> img_data;

protected:
	void NotifyError(HRESULT hr);
	long m_nRefCount; // Reference count.
	CRITICAL_SECTION m_critsec;

	IMFSourceReader *m_pReader;
	WCHAR *m_pwszSymbolicLink;
	UINT32 m_cchSymbolicLink;

public:
	CameraFeedWindows();
	virtual ~CameraFeedWindows();

	void set_device(IMFActivate *p_device);

	bool activate_feed();
	void deactivate_feed();

	// virtual functions needed to be implemented for callbacks

	// IUnknown methods
	STDMETHODIMP QueryInterface(REFIID iid, void **ppv);
	STDMETHODIMP_(ULONG)
	AddRef();
	STDMETHODIMP_(ULONG)
	Release();

	// IMFSourceReaderCallback methods
	STDMETHODIMP OnReadSample(
			HRESULT hrStatus,
			DWORD dwStreamIndex,
			DWORD dwStreamFlags,
			LONGLONG llTimestamp,
			IMFSample *pSample);

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *) {
		return S_OK;
	}

	STDMETHODIMP OnFlush(DWORD) {
		return S_OK;
	}
};

void CameraFeedWindows::NotifyError(HRESULT hr) {
	printf("NotifyError: %d\n", hr);
	//{ PostMessage(m_hwndEvent, WM_APP_PREVIEW_ERROR, (WPARAM)hr, 0L); }
}

//-------------------------------------------------------------------
//  AddRef
//-------------------------------------------------------------------

ULONG CameraFeedWindows::AddRef() {
	return InterlockedIncrement(&m_nRefCount);
}

//-------------------------------------------------------------------
//  Release
//-------------------------------------------------------------------

ULONG CameraFeedWindows::Release() {
	ULONG uCount = InterlockedDecrement(&m_nRefCount);
	if (uCount == 0) {
		delete this;
	}
	// For thread safety, return a temporary variable.
	return uCount;
}

//-------------------------------------------------------------------
//  QueryInterface
//-------------------------------------------------------------------

HRESULT CameraFeedWindows::QueryInterface(REFIID riid, void **ppv) {
	static const QITAB qit[] = {
		QITABENT(CameraFeedWindows, IMFSourceReaderCallback),
		{ 0 },
	};
	return QISearch(this, qit, riid, ppv);
}

/////////////// IMFSourceReaderCallback methods ///////////////

//-------------------------------------------------------------------
// OnReadSample
//
// Called when the IMFMediaSource::ReadSample method completes.
//-------------------------------------------------------------------

HRESULT CameraFeedWindows::OnReadSample(
		HRESULT hrStatus,
		DWORD /* dwStreamIndex */,
		DWORD /* dwStreamFlags */,
		LONGLONG /* llTimestamp */,
		IMFSample *pSample // Can be NULL
) {
	HRESULT hr = S_OK;
	IMFMediaBuffer *pBuffer = NULL;

	BYTE *pbScanline0 = NULL;
	LONG lStride = 0;

	EnterCriticalSection(&m_critsec);

	if (FAILED(hrStatus)) {
		hr = hrStatus;
	}

	if (SUCCEEDED(hr)) {
		if (pSample) {
			// Get the video frame buffer from the sample.

			hr = pSample->GetBufferByIndex(0, &pBuffer);

			if (SUCCEEDED(hr)) {
				Ref<Image> img;
				VideoBufferLock buffer(pBuffer);

				hr = buffer.LockBuffer(m_lDefaultStride, m_height, &pbScanline0, &lStride);

				PoolVector<uint8_t>::Write w = img_data.write();

				TransformImage_YUY2(
						(BYTE *)w.ptr(), //(BYTE *)lr.pBits,
						m_width * m_depth, //lr.Pitch, //?
						pbScanline0,
						lStride,
						m_width,
						m_height);

				img.instance();

				img->create(m_width, m_height, 0, Image::FORMAT_RGBA8, img_data);
				img->convert(Image::FORMAT_RGB8);

				set_RGB_img(img);
			}
		}
	}

	// Request the next frame.
	if (SUCCEEDED(hr)) {
		hr = m_pReader->ReadSample(
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				0,
				NULL, // actual
				NULL, // flags
				NULL, // timestamp
				NULL // sample
		);
	}

	if (FAILED(hr)) {
		NotifyError(hr);
	}
	SafeRelease(&pBuffer);

	LeaveCriticalSection(&m_critsec);
	return hr;
}

HRESULT CameraFeedWindows::CloseDevice() {
	EnterCriticalSection(&m_critsec);

	SafeRelease(&m_pReader);

	CoTaskMemFree(m_pwszSymbolicLink);
	m_pwszSymbolicLink = NULL;
	m_cchSymbolicLink = 0;

	LeaveCriticalSection(&m_critsec);
	return S_OK;
}

CameraFeedWindows::CameraFeedWindows() :
		m_pReader(NULL),
		m_nRefCount(1),
		m_pwszSymbolicLink(NULL),
		m_cchSymbolicLink(0),
		pActivate(NULL),
		m_height(0),
		m_width(0) {
	///@TODO implement this, should store information about our available camera

	InitializeCriticalSection(&m_critsec);
};

CameraFeedWindows::~CameraFeedWindows() {
	// make sure we stop recording if we are!
	if (is_active()) {
		deactivate_feed();
	};

	///@TODO free up anything used by this
	DeleteCriticalSection(&m_critsec);
};

bool CameraFeedWindows::activate_feed() {
	HRESULT hr = S_OK;
	IMFMediaSource *pSource = NULL;
	IMFAttributes *pAttributes = NULL;
	IMFMediaType *pType = NULL;
	UINT32 p_type_str_len = 0;
	_Post_ _Notnull_ LPWSTR p_type_str = NULL;

	GUID subtype = { 0 };
	MFRatio PAR = { 0 };

	///@TODO this should activate our camera and start the process of capturing frames
	printf("activate_feed\n");

	EnterCriticalSection(&m_critsec);

	hr = CloseDevice();

	// Create the media source for the device.
	if (SUCCEEDED(hr)) {
		hr = pActivate->ActivateObject(
				__uuidof(IMFMediaSource),
				(void **)&pSource);

		if (FAILED(hr)) {
			printf("ActivateObject Failed\n");
		}
	}

	// Get the symbolic link
	if (SUCCEEDED(hr)) {
		hr = pActivate->GetAllocatedString(
				MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
				&m_pwszSymbolicLink,
				&m_cchSymbolicLink);
		if (FAILED(hr)) {
			printf("Getting Symbolic Link Failed\n");
		}
	}

	// Create an attribute store to hold initialization settings.
	if (SUCCEEDED(hr)) {
		hr = MFCreateAttributes(&pAttributes, 2);
		if (FAILED(hr)) {
			printf("Attribute creation failed\n");
		}
	}

	if (SUCCEEDED(hr)) {
		hr = pAttributes->SetUINT32(MF_READWRITE_DISABLE_CONVERTERS, TRUE);
		if (FAILED(hr)) {
			printf("Set attribute (disable converter) failed\n");
		}
	}

	//trying to stop shutdown() so that we can reactivate feed
	if (SUCCEEDED(hr)) {
		hr = pAttributes->SetUINT32(MF_SOURCE_READER_DISCONNECT_MEDIASOURCE_ON_SHUTDOWN, TRUE);
		if (FAILED(hr)) {
			printf("Set attribute (disable shutdown on disconnect) failed\n");
		}
	}

	// Set the callback pointer.
	if (SUCCEEDED(hr)) {
		hr = pAttributes->SetUnknown(
				MF_SOURCE_READER_ASYNC_CALLBACK,
				this);
		if (FAILED(hr)) {
			printf("Unable to set callback pointer \n");
		}
	}

	if (SUCCEEDED(hr)) {
		hr = MFCreateSourceReaderFromMediaSource(
				pSource,
				pAttributes,
				&m_pReader);
		if (FAILED(hr)) {
			_com_error err(hr);
			printf("Unable to create source reader, %x, %s\n", hr, err.ErrorMessage());
		}
	}

	// Try to find a suitable output type.
	if (SUCCEEDED(hr)) {
		for (DWORD i = 0;; i++) {
			hr = m_pReader->GetNativeMediaType(
					(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
					i,
					&pType);

			if (FAILED(hr)) {
				// reached end of list.
				break;
			}

			//we are not going to d3d, so let's get the most convenient one
			//TryMediaType includes more configuration, so don't ignore that.
			//hr = TryMediaType(pType);

			hr = pType->GetGUID(MF_MT_SUBTYPE, &subtype);

			if (SUCCEEDED(hr)) {
				if (IsEqualGUID(subtype, MFVideoFormat_YUY2)) {
					printf("%d, YUY2 found\n", i);

					hr = m_pReader->SetCurrentMediaType(
							(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
							NULL,
							pType);
				}
			}

			//			SafeRelease(&pType); // ?????

			if (SUCCEEDED(hr)) {
				//hr = m_draw.SetVideoType(pType);

				// hr = SetConversionFunction(subtype); // TBD

				// Get the frame size.
				hr = MFGetAttributeSize(pType, MF_MT_FRAME_SIZE, &m_width, &m_height);

				printf("width: %d, height: %d\n", m_width, m_height);

				// Get the interlace mode. Default: assume progressive.
				m_interlace = (MFVideoInterlaceMode)MFGetAttributeUINT32(
						pType,
						MF_MT_INTERLACE_MODE,
						MFVideoInterlace_Progressive);

				printf("frames interlaced: %d\n", m_interlace);

				// Get the image stride.
				//hr = GetDefaultStride(pType, &m_lDefaultStride);
				hr = pType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32 *)&m_lDefaultStride);

				printf("default stride: %d\n", m_lDefaultStride);

				// Get the pixel aspect ratio. Default: Assume square pixels (1:1)
				hr = MFGetAttributeRatio(
						pType,
						MF_MT_PIXEL_ASPECT_RATIO,
						(UINT32 *)&PAR.Numerator,
						(UINT32 *)&PAR.Denominator);

				if (SUCCEEDED(hr)) {
					m_PixelAR = PAR;
				} else {
					m_PixelAR.Numerator = m_PixelAR.Denominator = 1;
				}
				printf("Pixel Aspect Ratio: %d/%d\n", m_PixelAR.Numerator, m_PixelAR.Denominator);
			}
			if (SUCCEEDED(hr)) {
				img_data.resize(m_width * m_height * m_depth); // RGB8
				printf("camera configured\n");
				break;
			}
		}
	}

	if (SUCCEEDED(hr)) {
		// Ask for the first sample.
		hr = m_pReader->ReadSample(
				(DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM,
				0,
				NULL,
				NULL,
				NULL,
				NULL);
		if (FAILED(hr)) {
			printf("first sample failed\n");
		}
	}

	if (FAILED(hr)) {
		printf("failed to activate feed\n");
		if (pSource) {
			pSource->Shutdown();

			// NOTE: The source reader shuts down the media source
			// by default, but we might not have gotten that far.
		}
		CloseDevice();
	}

	SafeRelease(&pSource);
	SafeRelease(&pAttributes);
	SafeRelease(&pType);

	LeaveCriticalSection(&m_critsec);

	if (hr == S_OK)
		return true;
	else
		return false;
};

void CameraFeedWindows::deactivate_feed() {
	printf("deactivate_feed\n");
	CloseDevice();
};

void CameraFeedWindows::set_device(IMFActivate *p_device) {
	// put some data in
	UINT32 cam_str_len;
	HRESULT hr = S_OK;

	LPWSTR cam_str = 0;
	pActivate = p_device;

	hr = pActivate->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &cam_str_len);
	cam_str = (wchar_t *)malloc(cam_str_len * sizeof(wchar_t) + 1);
	hr = pActivate->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, cam_str, cam_str_len + 1, NULL);
	// wprintf(L"%ws\n", cam_str);
	name = cam_str;
	free(cam_str);

	position = CameraFeed::FEED_UNSPECIFIED;
}

//////////////////////////////////////////////////////////////////////////
// CameraWindows - Subclass for our camera server on windows

// update_feeds on OSX version
void CameraWindows::add_active_cameras() {
	///@TODO scan through any active cameras and create CameraFeedWindows objects for them
	printf("CameraWindows::add_active_cameras()\n");
	HRESULT hr = S_OK;
	ChooseDeviceParam param = { 0 };
	UINT iDevice = 0;
	IMFAttributes *pAttributes = NULL;

	hr = MFCreateAttributes(&pAttributes, 1);
	if (FAILED(hr)) {
		goto done;
	}

	hr = pAttributes->SetGUID(
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
	if (FAILED(hr)) {
		goto done;
	}

	hr = MFEnumDeviceSources(pAttributes, &param.ppDevices, &param.count);

	// only for enumerating all devices

	printf("%d cameras detected\n", param.count);
	for (int i = 0; i < param.count; i++) {
		UINT32 cam_str_len;
		_Post_ _Notnull_ LPWSTR cam_str = 0;
		param.ppDevices[i]->GetStringLength(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &cam_str_len);
		//printf("%d" , cam_str_len);
		cam_str = (wchar_t *)malloc(cam_str_len * sizeof(wchar_t) + 1);
		hr = param.ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, cam_str, cam_str_len + 1, NULL);
		if (FAILED(hr)) {
			wprintf(L"can't get string\n");
			free(cam_str);
			goto done;
		}
		wprintf(L"%d-%ws\n", i, cam_str);
		free(cam_str);

		Ref<CameraFeedWindows> newfeed;
		newfeed.instance();
		newfeed->set_device(param.ppDevices[i]);
		add_feed(newfeed);
	}

done:
	SafeRelease(&pAttributes);
};

CameraWindows::CameraWindows() {
	// Find cameras active right now
	HRESULT hr = MFStartup(MF_VERSION);
	add_active_cameras();

	// need to add something that will react to devices being connected/removed...
};

CameraWindows::~CameraWindows() {
	MFShutdown();
};
