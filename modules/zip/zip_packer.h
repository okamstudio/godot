/*************************************************************************/
/*  zip_packer.h                                                         */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2018 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2018 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef ZIP_PACKER_H
#define ZIP_PACKER_H

#include "core/reference.h"

#include "core/os/file_access.h"
#include "core/os/os.h"
#include "thirdparty/minizip/zip.h"

class ZIPPacker : public Reference {

	GDCLASS(ZIPPacker, Object);

	FileAccess *f;
	zipFile zf;

protected:
	static void _bind_methods();

public:
	enum ZipAppend {
		APPEND_CREATE = 0,
		APPEND_CREATEAFTER = 1,
		APPEND_ADDINZIP = 2,
	};

	Error open(String path, ZipAppend append);
	Error close();

	Error start_file(String path);
	Error write_file(Vector<uint8_t> data);
	Error close_file();

	ZIPPacker();
	~ZIPPacker();
};

VARIANT_ENUM_CAST(ZIPPacker::ZipAppend)

#endif // ZIP_PACKER_H
