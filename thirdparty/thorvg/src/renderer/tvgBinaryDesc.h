/*
 * Copyright (c) 2021 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _TVG_BINARY_DESC_H_
#define _TVG_BINARY_DESC_H_

/* TODO: Need to consider whether uin8_t is enough size for extension...
   Rather than optimal data, we can use enough size and data compress? */

using TvgBinByte = uint8_t;
using TvgBinCounter = uint32_t;
using TvgBinTag = TvgBinByte;
using TvgBinFlag = TvgBinByte;


//Header
#define TVG_HEADER_SIZE 33                //TVG_HEADER_SIGNATURE_LENGTH + TVG_HEADER_VERSION_LENGTH + 2*SIZE(float) + TVG_HEADER_RESERVED_LENGTH + TVG_HEADER_COMPRESS_SIZE
#define TVG_HEADER_SIGNATURE "ThorVG"
#define TVG_HEADER_SIGNATURE_LENGTH 6
#define TVG_HEADER_VERSION "001500"       //Major 00, Minor 15, Micro 00
#define TVG_HEADER_VERSION_LENGTH 6
#define TVG_HEADER_RESERVED_LENGTH 1      //Storing flags for extensions
#define TVG_HEADER_COMPRESS_SIZE 12       //TVG_HEADER_UNCOMPRESSED_SIZE + TVG_HEADER_COMPRESSED_SIZE + TVG_HEADER_COMPRESSED_SIZE_BITS
//Compress Size
#define TVG_HEADER_UNCOMPRESSED_SIZE 4     //SIZE (TvgBinCounter)
#define TVG_HEADER_COMPRESSED_SIZE 4       //SIZE (TvgBinCounter)
#define TVG_HEADER_COMPRESSED_SIZE_BITS 4  //SIZE (TvgBinCounter)
//Reserved Flag
#define TVG_HEAD_FLAG_COMPRESSED                    0x01

//Paint Type
#define TVG_TAG_CLASS_PICTURE                       (TvgBinTag)0xfc
#define TVG_TAG_CLASS_SHAPE                         (TvgBinTag)0xfd
#define TVG_TAG_CLASS_SCENE                         (TvgBinTag)0xfe


//Paint
#define TVG_TAG_PAINT_OPACITY                       (TvgBinTag)0x10
#define TVG_TAG_PAINT_TRANSFORM                     (TvgBinTag)0x11
#define TVG_TAG_PAINT_CMP_TARGET                    (TvgBinTag)0x01
#define TVG_TAG_PAINT_CMP_METHOD                    (TvgBinTag)0x20


//TODO: Keep this for the compatibility, Remove in TVG 1.0 release
//Scene
 #define TVG_TAG_SCENE_RESERVEDCNT                   (TvgBinTag)0x30


//Shape
#define TVG_TAG_SHAPE_PATH                          (TvgBinTag)0x40
#define TVG_TAG_SHAPE_STROKE                        (TvgBinTag)0x41
#define TVG_TAG_SHAPE_FILL                          (TvgBinTag)0x42
#define TVG_TAG_SHAPE_COLOR                         (TvgBinTag)0x43
#define TVG_TAG_SHAPE_FILLRULE                      (TvgBinTag)0x44


//Stroke
#define TVG_TAG_SHAPE_STROKE_CAP                    (TvgBinTag)0x50
#define TVG_TAG_SHAPE_STROKE_JOIN                   (TvgBinTag)0x51
#define TVG_TAG_SHAPE_STROKE_WIDTH                  (TvgBinTag)0x52
#define TVG_TAG_SHAPE_STROKE_COLOR                  (TvgBinTag)0x53
#define TVG_TAG_SHAPE_STROKE_FILL                   (TvgBinTag)0x54
#define TVG_TAG_SHAPE_STROKE_DASHPTRN               (TvgBinTag)0x55
#define TVG_TAG_SHAPE_STROKE_MITERLIMIT             (TvgBinTag)0x56
#define TVG_TAG_SHAPE_STROKE_ORDER                  (TvgBinTag)0x57
#define TVG_TAG_SHAPE_STROKE_DASH_OFFSET            (TvgBinTag)0x58


//Fill
#define TVG_TAG_FILL_LINEAR_GRADIENT                (TvgBinTag)0x60
#define TVG_TAG_FILL_RADIAL_GRADIENT                (TvgBinTag)0x61
#define TVG_TAG_FILL_COLORSTOPS                     (TvgBinTag)0x62
#define TVG_TAG_FILL_FILLSPREAD                     (TvgBinTag)0x63
#define TVG_TAG_FILL_TRANSFORM                      (TvgBinTag)0x64
#define TVG_TAG_FILL_RADIAL_GRADIENT_FOCAL          (TvgBinTag)0x65

//Picture
#define TVG_TAG_PICTURE_RAW_IMAGE                   (TvgBinTag)0x70
#define TVG_TAG_PICTURE_MESH                        (TvgBinTag)0x71

#endif //_TVG_BINARY_DESC_H_
