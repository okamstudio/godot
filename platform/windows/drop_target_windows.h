/**************************************************************************/
/*  drop_target_windows.h                                                 */
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

#pragma once

#include "display_server_windows.h"

#include <shlobj.h>

// Silence warning due to a COM API weirdness.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

// https://learn.microsoft.com/en-us/windows/win32/api/ole2/nf-ole2-dodragdrop#remarks
class DropTargetWindows : public IDropTarget {
	LONG ref_count;
	DisplayServerWindows::WindowData *window_data = nullptr;
	CLIPFORMAT cf_filedescriptor = 0;
	CLIPFORMAT cf_filecontents = 0;
	String tmp_path;

	bool is_valid_filedescriptor();
	HRESULT handle_hdrop_format(Vector<String> *p_files, IDataObject *pDataObj);
	HRESULT handle_filedescriptor_format(Vector<String> *p_files, IDataObject *pDataObj);
	HRESULT save_as_file(const String &p_out_dir, FILEDESCRIPTORW *p_file_desc, IDataObject *pDataObj, int p_file_idx);

public:
	DropTargetWindows(DisplayServerWindows::WindowData *p_window_data);
	virtual ~DropTargetWindows() {}

	// IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override;
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;

	// IDropTarget
	HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
	HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
	HRESULT STDMETHODCALLTYPE DragLeave() override;
	HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) override;
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
