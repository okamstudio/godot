// basis_etc.cpp
// Copyright (C) 2019-2024 Binomial LLC. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "basisu_etc.h"

#if BASISU_SUPPORT_SSE
#define CPPSPMD_NAME(a) a##_sse41
#include "basisu_kernels_declares.h"
#endif

#define BASISU_DEBUG_ETC_ENCODER 0
#define BASISU_DEBUG_ETC_ENCODER_DEEPER 0

namespace basisu
{
	const int8_t g_etc2_eac_tables[16][8] =
	{
		{ -3, -6, -9, -15, 2, 5, 8, 14 }, { -3, -7, -10, -13, 2, 6, 9, 12 }, { -2, -5, -8, -13, 1, 4, 7, 12 }, { -2, -4, -6, -13, 1, 3, 5, 12 },
		{ -3, -6, -8, -12, 2, 5, 7, 11 }, { -3, -7, -9, -11, 2, 6, 8, 10 }, { -4, -7, -8, -11, 3, 6, 7, 10 }, { -3, -5, -8, -11, 2, 4, 7, 10 },
		{ -2, -6, -8, -10, 1, 5, 7, 9 }, { -2, -5, -8, -10, 1, 4, 7, 9 }, { -2, -4, -8, -10, 1, 3, 7, 9 }, { -2, -5, -7, -10, 1, 4, 6, 9 },
		{ -3, -4, -7, -10, 2, 3, 6, 9 }, { -1, -2, -3, -10, 0, 1, 2, 9 }, { -4, -6, -8, -9, 3, 5, 7, 8 }, { -3, -5, -7, -9, 2, 4, 6, 8 }
	};

	const int8_t g_etc2_eac_tables8[16][8] =
	{
		{ -24, -48, -72, -120, 16, 40, 64, 112 }, { -24,-56,-80,-104,16,48,72,96 }, { -16,-40,-64,-104,8,32,56,96 }, { -16,-32,-48,-104,8,24,40,96 },
		{ -24,-48,-64,-96,16,40,56,88 }, { -24,-56,-72,-88,16,48,64,80 }, { -32,-56,-64,-88,24,48,56,80 }, { -24,-40,-64,-88,16,32,56,80 },
		{ -16,-48,-64,-80,8,40,56,72 }, { -16,-40,-64,-80,8,32,56,72 }, { -16,-32,-64,-80,8,24,56,72 }, { -16,-40,-56,-80,8,32,48,72 },
		{ -24,-32,-56,-80,16,24,48,72 }, { -8,-16,-24,-80,0,8,16,72 }, { -32,-48,-64,-72,24,40,56,64 },	{ -24,-40,-56,-72,16,32,48,64 }
	};
		
	// Given an ETC1 diff/inten_table/selector, and an 8-bit desired color, this table encodes the best packed_color in the low byte, and the abs error in the high byte.
	static uint16_t g_etc1_inverse_lookup[2 * 8 * 4][256];      // [ diff/inten_table/selector][desired_color ]

	// g_color8_to_etc_block_config[color][table_index] = Supplies for each 8-bit color value a list of packed ETC1 diff/intensity table/selectors/packed_colors that map to that color.
	// To pack: diff | (inten << 1) | (selector << 4) | (packed_c << 8)
	static const uint16_t g_etc1_color8_to_etc_block_config_0_255[2][33] =
	{
		{ 0x0000,  0x0010,  0x0002,  0x0012,  0x0004,  0x0014,  0x0006,  0x0016,  0x0008,  0x0018,  0x000A,  0x001A,  0x000C,  0x001C,  0x000E,  0x001E,		  0x0001,  0x0011,  0x0003,  0x0013,  0x0005,  0x0015,  0x0007,  0x0017,  0x0009,  0x0019,  0x000B,  0x001B,  0x000D,  0x001D,  0x000F,  0x001F, 0xFFFF },
		{ 0x0F20,  0x0F30,  0x0E32,  0x0F22,  0x0E34,  0x0F24,  0x0D36,  0x0F26,  0x0C38,  0x0E28,  0x0B3A,  0x0E2A,  0x093C,  0x0E2C,  0x053E,  0x0D2E,		  0x1E31,  0x1F21,  0x1D33,  0x1F23,  0x1C35,  0x1E25,  0x1A37,  0x1E27,  0x1839,  0x1D29,  0x163B,  0x1C2B,  0x133D,  0x1B2D,  0x093F,  0x1A2F, 0xFFFF },
	};

	// Really only [254][11].
	static const uint16_t g_etc1_color8_to_etc_block_config_1_to_254[254][12] =
	{
		{ 0x021C, 0x0D0D, 0xFFFF }, { 0x0020, 0x0021, 0x0A0B, 0x061F, 0xFFFF }, { 0x0113, 0x0217, 0xFFFF }, { 0x0116, 0x031E,		0x0B0E, 0x0405, 0xFFFF }, { 0x0022, 0x0204, 0x050A, 0x0023, 0xFFFF }, { 0x0111, 0x0319, 0x0809, 0x170F, 0xFFFF }, {
		0x0303, 0x0215, 0x0607, 0xFFFF }, { 0x0030, 0x0114, 0x0408, 0x0031, 0x0201, 0x051D, 0xFFFF }, { 0x0100, 0x0024, 0x0306,		0x0025, 0x041B, 0x0E0D, 0xFFFF }, { 0x021A, 0x0121, 0x0B0B, 0x071F, 0xFFFF }, { 0x0213, 0x0317, 0xFFFF }, { 0x0112,
		0x0505, 0xFFFF }, { 0x0026, 0x070C, 0x0123, 0x0027, 0xFFFF }, { 0x0211, 0x0909, 0xFFFF }, { 0x0110, 0x0315, 0x0707,		0x0419, 0x180F, 0xFFFF }, { 0x0218, 0x0131, 0x0301, 0x0403, 0x061D, 0xFFFF }, { 0x0032, 0x0202, 0x0033, 0x0125, 0x051B,
		0x0F0D, 0xFFFF }, { 0x0028, 0x031C, 0x0221, 0x0029, 0xFFFF }, { 0x0120, 0x0313, 0x0C0B, 0x081F, 0xFFFF }, { 0x0605,		0x0417, 0xFFFF }, { 0x0216, 0x041E, 0x0C0E, 0x0223, 0x0127, 0xFFFF }, { 0x0122, 0x0304, 0x060A, 0x0311, 0x0A09, 0xFFFF
		}, { 0x0519, 0x190F, 0xFFFF }, { 0x002A, 0x0231, 0x0503, 0x0415, 0x0807, 0x002B, 0x071D, 0xFFFF }, { 0x0130, 0x0214,		0x0508, 0x0401, 0x0133, 0x0225, 0x061B, 0xFFFF }, { 0x0200, 0x0124, 0x0406, 0x0321, 0x0129, 0x100D, 0xFFFF }, { 0x031A,
		0x0D0B, 0x091F, 0xFFFF }, { 0x0413, 0x0705, 0x0517, 0xFFFF }, { 0x0212, 0x0034, 0x0323, 0x0035, 0x0227, 0xFFFF }, {		0x0126, 0x080C, 0x0B09, 0xFFFF }, { 0x0411, 0x0619, 0x1A0F, 0xFFFF }, { 0x0210, 0x0331, 0x0603, 0x0515, 0x0907, 0x012B,
		0xFFFF }, { 0x0318, 0x002C, 0x0501, 0x0233, 0x0325, 0x071B, 0x002D, 0x081D, 0xFFFF }, { 0x0132, 0x0302, 0x0229, 0x110D,		0xFFFF }, { 0x0128, 0x041C, 0x0421, 0x0E0B, 0x0A1F, 0xFFFF }, { 0x0220, 0x0513, 0x0617, 0xFFFF }, { 0x0135, 0x0805,
		0x0327, 0xFFFF }, { 0x0316, 0x051E, 0x0D0E, 0x0423, 0xFFFF }, { 0x0222, 0x0404, 0x070A, 0x0511, 0x0719, 0x0C09, 0x1B0F,		0xFFFF }, { 0x0703, 0x0615, 0x0A07, 0x022B, 0xFFFF }, { 0x012A, 0x0431, 0x0601, 0x0333, 0x012D, 0x091D, 0xFFFF }, {
		0x0230, 0x0314, 0x0036, 0x0608, 0x0425, 0x0037, 0x0329, 0x081B, 0x120D, 0xFFFF }, { 0x0300, 0x0224, 0x0506, 0x0521,		0x0F0B, 0x0B1F, 0xFFFF }, { 0x041A, 0x0613, 0x0717, 0xFFFF }, { 0x0235, 0x0905, 0xFFFF }, { 0x0312, 0x0134, 0x0523,
		0x0427, 0xFFFF }, { 0x0226, 0x090C, 0x002E, 0x0611, 0x0D09, 0x002F, 0xFFFF }, { 0x0715, 0x0B07, 0x0819, 0x032B, 0x1C0F,		0xFFFF }, { 0x0310, 0x0531, 0x0701, 0x0803, 0x022D, 0x0A1D, 0xFFFF }, { 0x0418, 0x012C, 0x0433, 0x0525, 0x0137, 0x091B,
		0x130D, 0xFFFF }, { 0x0232, 0x0402, 0x0621, 0x0429, 0xFFFF }, { 0x0228, 0x051C, 0x0713, 0x100B, 0x0C1F, 0xFFFF }, {		0x0320, 0x0335, 0x0A05, 0x0817, 0xFFFF }, { 0x0623, 0x0527, 0xFFFF }, { 0x0416, 0x061E, 0x0E0E, 0x0711, 0x0E09, 0x012F,
		0xFFFF }, { 0x0322, 0x0504, 0x080A, 0x0919, 0x1D0F, 0xFFFF }, { 0x0631, 0x0903, 0x0815, 0x0C07, 0x042B, 0x032D, 0x0B1D,		0xFFFF }, { 0x022A, 0x0801, 0x0533, 0x0625, 0x0237, 0x0A1B, 0xFFFF }, { 0x0330, 0x0414, 0x0136, 0x0708, 0x0721, 0x0529,
		0x140D, 0xFFFF }, { 0x0400, 0x0324, 0x0606, 0x0038, 0x0039, 0x110B, 0x0D1F, 0xFFFF }, { 0x051A, 0x0813, 0x0B05, 0x0917,		0xFFFF }, { 0x0723, 0x0435, 0x0627, 0xFFFF }, { 0x0412, 0x0234, 0x0F09, 0x022F, 0xFFFF }, { 0x0326, 0x0A0C, 0x012E,
		0x0811, 0x0A19, 0x1E0F, 0xFFFF }, { 0x0731, 0x0A03, 0x0915, 0x0D07, 0x052B, 0xFFFF }, { 0x0410, 0x0901, 0x0633, 0x0725,		0x0337, 0x0B1B, 0x042D, 0x0C1D, 0xFFFF }, { 0x0518, 0x022C, 0x0629, 0x150D, 0xFFFF }, { 0x0332, 0x0502, 0x0821, 0x0139,
		0x120B, 0x0E1F, 0xFFFF }, { 0x0328, 0x061C, 0x0913, 0x0A17, 0xFFFF }, { 0x0420, 0x0535, 0x0C05, 0x0727, 0xFFFF }, {		0x0823, 0x032F, 0xFFFF }, { 0x0516, 0x071E, 0x0F0E, 0x0911, 0x0B19, 0x1009, 0x1F0F, 0xFFFF }, { 0x0422, 0x0604, 0x090A,
		0x0B03, 0x0A15, 0x0E07, 0x062B, 0xFFFF }, { 0x0831, 0x0A01, 0x0733, 0x052D, 0x0D1D, 0xFFFF }, { 0x032A, 0x0825, 0x0437,		0x0729, 0x0C1B, 0x160D, 0xFFFF }, { 0x0430, 0x0514, 0x0236, 0x0808, 0x0921, 0x0239, 0x130B, 0x0F1F, 0xFFFF }, { 0x0500,
		0x0424, 0x0706, 0x0138, 0x0A13, 0x0B17, 0xFFFF }, { 0x061A, 0x0635, 0x0D05, 0xFFFF }, { 0x0923, 0x0827, 0xFFFF }, {		0x0512, 0x0334, 0x003A, 0x0A11, 0x1109, 0x003B, 0x042F, 0xFFFF }, { 0x0426, 0x0B0C, 0x022E, 0x0B15, 0x0F07, 0x0C19,
		0x072B, 0xFFFF }, { 0x0931, 0x0B01, 0x0C03, 0x062D, 0x0E1D, 0xFFFF }, { 0x0510, 0x0833, 0x0925, 0x0537, 0x0D1B, 0x170D,		0xFFFF }, { 0x0618, 0x032C, 0x0A21, 0x0339, 0x0829, 0xFFFF }, { 0x0432, 0x0602, 0x0B13, 0x140B, 0x101F, 0xFFFF }, {
		0x0428, 0x071C, 0x0735, 0x0E05, 0x0C17, 0xFFFF }, { 0x0520, 0x0A23, 0x0927, 0xFFFF }, { 0x0B11, 0x1209, 0x013B, 0x052F,		0xFFFF }, { 0x0616, 0x081E, 0x0D19, 0xFFFF }, { 0x0522, 0x0704, 0x0A0A, 0x0A31, 0x0D03, 0x0C15, 0x1007, 0x082B, 0x072D,
		0x0F1D, 0xFFFF }, { 0x0C01, 0x0933, 0x0A25, 0x0637, 0x0E1B, 0xFFFF }, { 0x042A, 0x0B21, 0x0929, 0x180D, 0xFFFF }, {		0x0530, 0x0614, 0x0336, 0x0908, 0x0439, 0x150B, 0x111F, 0xFFFF }, { 0x0600, 0x0524, 0x0806, 0x0238, 0x0C13, 0x0F05,
		0x0D17, 0xFFFF }, { 0x071A, 0x0B23, 0x0835, 0x0A27, 0xFFFF }, { 0x1309, 0x023B, 0x062F, 0xFFFF }, { 0x0612, 0x0434,		0x013A, 0x0C11, 0x0E19, 0xFFFF }, { 0x0526, 0x0C0C, 0x032E, 0x0B31, 0x0E03, 0x0D15, 0x1107, 0x092B, 0xFFFF }, { 0x0D01,
		0x0A33, 0x0B25, 0x0737, 0x0F1B, 0x082D, 0x101D, 0xFFFF }, { 0x0610, 0x0A29, 0x190D, 0xFFFF }, { 0x0718, 0x042C, 0x0C21,		0x0539, 0x160B, 0x121F, 0xFFFF }, { 0x0532, 0x0702, 0x0D13, 0x0E17, 0xFFFF }, { 0x0528, 0x081C, 0x0935, 0x1005, 0x0B27,
		0xFFFF }, { 0x0620, 0x0C23, 0x033B, 0x072F, 0xFFFF }, { 0x0D11, 0x0F19, 0x1409, 0xFFFF }, { 0x0716, 0x003C, 0x091E,		0x0F03, 0x0E15, 0x1207, 0x0A2B, 0x003D, 0xFFFF }, { 0x0622, 0x0804, 0x0B0A, 0x0C31, 0x0E01, 0x0B33, 0x092D, 0x111D,
		0xFFFF }, { 0x0C25, 0x0837, 0x0B29, 0x101B, 0x1A0D, 0xFFFF }, { 0x052A, 0x0D21, 0x0639, 0x170B, 0x131F, 0xFFFF }, {		0x0630, 0x0714, 0x0436, 0x0A08, 0x0E13, 0x0F17, 0xFFFF }, { 0x0700, 0x0624, 0x0906, 0x0338, 0x0A35, 0x1105, 0xFFFF }, {
		0x081A, 0x0D23, 0x0C27, 0xFFFF }, { 0x0E11, 0x1509, 0x043B, 0x082F, 0xFFFF }, { 0x0712, 0x0534, 0x023A, 0x0F15, 0x1307,		0x1019, 0x0B2B, 0x013D, 0xFFFF }, { 0x0626, 0x0D0C, 0x042E, 0x0D31, 0x0F01, 0x1003, 0x0A2D, 0x121D, 0xFFFF }, { 0x0C33,
		0x0D25, 0x0937, 0x111B, 0x1B0D, 0xFFFF }, { 0x0710, 0x0E21, 0x0739, 0x0C29, 0xFFFF }, { 0x0818, 0x052C, 0x0F13, 0x180B,		0x141F, 0xFFFF }, { 0x0632, 0x0802, 0x0B35, 0x1205, 0x1017, 0xFFFF }, { 0x0628, 0x091C, 0x0E23, 0x0D27, 0xFFFF }, {
		0x0720, 0x0F11, 0x1609, 0x053B, 0x092F, 0xFFFF }, { 0x1119, 0x023D, 0xFFFF }, { 0x0816, 0x013C, 0x0A1E, 0x0E31, 0x1103,		0x1015, 0x1407, 0x0C2B, 0x0B2D, 0x131D, 0xFFFF }, { 0x0722, 0x0904, 0x0C0A, 0x1001, 0x0D33, 0x0E25, 0x0A37, 0x121B,
		0xFFFF }, { 0x0F21, 0x0D29, 0x1C0D, 0xFFFF }, { 0x062A, 0x0839, 0x190B, 0x151F, 0xFFFF }, { 0x0730, 0x0814, 0x0536,		0x0B08, 0x1013, 0x1305, 0x1117, 0xFFFF }, { 0x0800, 0x0724, 0x0A06, 0x0438, 0x0F23, 0x0C35, 0x0E27, 0xFFFF }, { 0x091A,
		0x1709, 0x063B, 0x0A2F, 0xFFFF }, { 0x1011, 0x1219, 0x033D, 0xFFFF }, { 0x0812, 0x0634, 0x033A, 0x0F31, 0x1203, 0x1115,		0x1507, 0x0D2B, 0xFFFF }, { 0x0726, 0x0E0C, 0x052E, 0x1101, 0x0E33, 0x0F25, 0x0B37, 0x131B, 0x0C2D, 0x141D, 0xFFFF }, {
		0x0E29, 0x1D0D, 0xFFFF }, { 0x0810, 0x1021, 0x0939, 0x1A0B, 0x161F, 0xFFFF }, { 0x0918, 0x062C, 0x1113, 0x1217, 0xFFFF		}, { 0x0732, 0x0902, 0x0D35, 0x1405, 0x0F27, 0xFFFF }, { 0x0728, 0x0A1C, 0x1023, 0x073B, 0x0B2F, 0xFFFF }, { 0x0820,
		0x1111, 0x1319, 0x1809, 0xFFFF }, { 0x1303, 0x1215, 0x1607, 0x0E2B, 0x043D, 0xFFFF }, { 0x0916, 0x023C, 0x0B1E, 0x1031,		0x1201, 0x0F33, 0x0D2D, 0x151D, 0xFFFF }, { 0x0822, 0x0A04, 0x0D0A, 0x1025, 0x0C37, 0x0F29, 0x141B, 0x1E0D, 0xFFFF }, {
		0x1121, 0x0A39, 0x1B0B, 0x171F, 0xFFFF }, { 0x072A, 0x1213, 0x1317, 0xFFFF }, { 0x0830, 0x0914, 0x0636, 0x0C08, 0x0E35,		0x1505, 0xFFFF }, { 0x0900, 0x0824, 0x0B06, 0x0538, 0x1123, 0x1027, 0xFFFF }, { 0x0A1A, 0x1211, 0x1909, 0x083B, 0x0C2F,
		0xFFFF }, { 0x1315, 0x1707, 0x1419, 0x0F2B, 0x053D, 0xFFFF }, { 0x0912, 0x0734, 0x043A, 0x1131, 0x1301, 0x1403, 0x0E2D,		0x161D, 0xFFFF }, { 0x0826, 0x0F0C, 0x062E, 0x1033, 0x1125, 0x0D37, 0x151B, 0x1F0D, 0xFFFF }, { 0x1221, 0x0B39, 0x1029,
		0xFFFF }, { 0x0910, 0x1313, 0x1C0B, 0x181F, 0xFFFF }, { 0x0A18, 0x072C, 0x0F35, 0x1605, 0x1417, 0xFFFF }, { 0x0832,		0x0A02, 0x1223, 0x1127, 0xFFFF }, { 0x0828, 0x0B1C, 0x1311, 0x1A09, 0x093B, 0x0D2F, 0xFFFF }, { 0x0920, 0x1519, 0x063D,
		0xFFFF }, { 0x1231, 0x1503, 0x1415, 0x1807, 0x102B, 0x0F2D, 0x171D, 0xFFFF }, { 0x0A16, 0x033C, 0x0C1E, 0x1401, 0x1133,		0x1225, 0x0E37, 0x161B, 0xFFFF }, { 0x0922, 0x0B04, 0x0E0A, 0x1321, 0x1129, 0xFFFF }, { 0x0C39, 0x1D0B, 0x191F, 0xFFFF
		}, { 0x082A, 0x1413, 0x1705, 0x1517, 0xFFFF }, { 0x0930, 0x0A14, 0x0736, 0x0D08, 0x1323, 0x1035, 0x1227, 0xFFFF }, {		0x0A00, 0x0924, 0x0C06, 0x0638, 0x1B09, 0x0A3B, 0x0E2F, 0xFFFF }, { 0x0B1A, 0x1411, 0x1619, 0x073D, 0xFFFF }, { 0x1331,
		0x1603, 0x1515, 0x1907, 0x112B, 0xFFFF }, { 0x0A12, 0x0834, 0x053A, 0x1501, 0x1233, 0x1325, 0x0F37, 0x171B, 0x102D,		0x181D, 0xFFFF }, { 0x0926, 0x072E, 0x1229, 0xFFFF }, { 0x1421, 0x0D39, 0x1E0B, 0x1A1F, 0xFFFF }, { 0x0A10, 0x1513,
		0x1617, 0xFFFF }, { 0x0B18, 0x082C, 0x1135, 0x1805, 0x1327, 0xFFFF }, { 0x0932, 0x0B02, 0x1423, 0x0B3B, 0x0F2F, 0xFFFF		}, { 0x0928, 0x0C1C, 0x1511, 0x1719, 0x1C09, 0xFFFF }, { 0x0A20, 0x1703, 0x1615, 0x1A07, 0x122B, 0x083D, 0xFFFF }, {
		0x1431, 0x1601, 0x1333, 0x112D, 0x191D, 0xFFFF }, { 0x0B16, 0x043C, 0x0D1E, 0x1425, 0x1037, 0x1329, 0x181B, 0xFFFF }, {		0x0A22, 0x0C04, 0x0F0A, 0x1521, 0x0E39, 0x1F0B, 0x1B1F, 0xFFFF }, { 0x1613, 0x1717, 0xFFFF }, { 0x092A, 0x1235, 0x1905,
		0xFFFF }, { 0x0A30, 0x0B14, 0x0836, 0x0E08, 0x1523, 0x1427, 0xFFFF }, { 0x0B00, 0x0A24, 0x0D06, 0x0738, 0x1611, 0x1D09,		0x0C3B, 0x102F, 0xFFFF }, { 0x0C1A, 0x1715, 0x1B07, 0x1819, 0x132B, 0x093D, 0xFFFF }, { 0x1531, 0x1701, 0x1803, 0x122D,
		0x1A1D, 0xFFFF }, { 0x0B12, 0x0934, 0x063A, 0x1433, 0x1525, 0x1137, 0x191B, 0xFFFF }, { 0x0A26, 0x003E, 0x082E, 0x1621,		0x0F39, 0x1429, 0x003F, 0xFFFF }, { 0x1713, 0x1C1F, 0xFFFF }, { 0x0B10, 0x1335, 0x1A05, 0x1817, 0xFFFF }, { 0x0C18,
		0x092C, 0x1623, 0x1527, 0xFFFF }, { 0x0A32, 0x0C02, 0x1711, 0x1E09, 0x0D3B, 0x112F, 0xFFFF }, { 0x0A28, 0x0D1C, 0x1919,		0x0A3D, 0xFFFF }, { 0x0B20, 0x1631, 0x1903, 0x1815, 0x1C07, 0x142B, 0x132D, 0x1B1D, 0xFFFF }, { 0x1801, 0x1533, 0x1625,
		0x1237, 0x1A1B, 0xFFFF }, { 0x0C16, 0x053C, 0x0E1E, 0x1721, 0x1529, 0x013F, 0xFFFF }, { 0x0B22, 0x0D04, 0x1039, 0x1D1F,		0xFFFF }, { 0x1813, 0x1B05, 0x1917, 0xFFFF }, { 0x0A2A, 0x1723, 0x1435, 0x1627, 0xFFFF }, { 0x0B30, 0x0C14, 0x0936,
		0x0F08, 0x1F09, 0x0E3B, 0x122F, 0xFFFF }, { 0x0C00, 0x0B24, 0x0E06, 0x0838, 0x1811, 0x1A19, 0x0B3D, 0xFFFF }, { 0x0D1A,		0x1731, 0x1A03, 0x1915, 0x1D07, 0x152B, 0xFFFF }, { 0x1901, 0x1633, 0x1725, 0x1337, 0x1B1B, 0x142D, 0x1C1D, 0xFFFF }, {
		0x0C12, 0x0A34, 0x073A, 0x1629, 0x023F, 0xFFFF }, { 0x0B26, 0x013E, 0x092E, 0x1821, 0x1139, 0x1E1F, 0xFFFF }, { 0x1913,		0x1A17, 0xFFFF }, { 0x0C10, 0x1535, 0x1C05, 0x1727, 0xFFFF }, { 0x0D18, 0x0A2C, 0x1823, 0x0F3B, 0x132F, 0xFFFF }, {
		0x0B32, 0x0D02, 0x1911, 0x1B19, 0xFFFF }, { 0x0B28, 0x0E1C, 0x1B03, 0x1A15, 0x1E07, 0x162B, 0x0C3D, 0xFFFF }, { 0x0C20,		0x1831, 0x1A01, 0x1733, 0x152D, 0x1D1D, 0xFFFF }, { 0x1825, 0x1437, 0x1729, 0x1C1B, 0x033F, 0xFFFF }, { 0x0D16, 0x063C,
		0x0F1E, 0x1921, 0x1239, 0x1F1F, 0xFFFF }, { 0x0C22, 0x0E04, 0x1A13, 0x1B17, 0xFFFF }, { 0x1635, 0x1D05, 0xFFFF }, {		0x0B2A, 0x1923, 0x1827, 0xFFFF }, { 0x0C30, 0x0D14, 0x0A36, 0x1A11, 0x103B, 0x142F, 0xFFFF }, { 0x0D00, 0x0C24, 0x0F06,
		0x0938, 0x1B15, 0x1F07, 0x1C19, 0x172B, 0x0D3D, 0xFFFF }, { 0x0E1A, 0x1931, 0x1B01, 0x1C03, 0x162D, 0x1E1D, 0xFFFF }, {		0x1833, 0x1925, 0x1537, 0x1D1B, 0xFFFF }, { 0x0D12, 0x0B34, 0x083A, 0x1A21, 0x1339, 0x1829, 0x043F, 0xFFFF }, { 0x0C26,
		0x023E, 0x0A2E, 0x1B13, 0xFFFF }, { 0x1735, 0x1E05, 0x1C17, 0xFFFF }, { 0x0D10, 0x1A23, 0x1927, 0xFFFF }, { 0x0E18,		0x0B2C, 0x1B11, 0x113B, 0x152F, 0xFFFF }, { 0x0C32, 0x0E02, 0x1D19, 0x0E3D, 0xFFFF }, { 0x0C28, 0x0F1C, 0x1A31, 0x1D03,
		0x1C15, 0x182B, 0x172D, 0x1F1D, 0xFFFF }, { 0x0D20, 0x1C01, 0x1933, 0x1A25, 0x1637, 0x1E1B, 0xFFFF }, { 0x1B21, 0x1929,		0x053F, 0xFFFF }, { 0x0E16, 0x073C, 0x1439, 0xFFFF }, { 0x0D22, 0x0F04, 0x1C13, 0x1F05, 0x1D17, 0xFFFF }, { 0x1B23,
		0x1835, 0x1A27, 0xFFFF }, { 0x0C2A, 0x123B, 0x162F, 0xFFFF }, { 0x0D30, 0x0E14, 0x0B36, 0x1C11, 0x1E19, 0x0F3D, 0xFFFF		}, { 0x0E00, 0x0D24, 0x0A38, 0x1B31, 0x1E03, 0x1D15, 0x192B, 0xFFFF }, { 0x0F1A, 0x1D01, 0x1A33, 0x1B25, 0x1737, 0x1F1B,
		0x182D, 0xFFFF }, { 0x1A29, 0x063F, 0xFFFF }, { 0x0E12, 0x0C34, 0x093A, 0x1C21, 0x1539, 0xFFFF }, { 0x0D26, 0x033E,		0x0B2E, 0x1D13, 0x1E17, 0xFFFF }, { 0x1935, 0x1B27, 0xFFFF }, { 0x0E10, 0x1C23, 0x133B, 0x172F, 0xFFFF }, { 0x0F18,
		0x0C2C, 0x1D11, 0x1F19, 0xFFFF }, { 0x0D32, 0x0F02, 0x1F03, 0x1E15, 0x1A2B, 0x103D, 0xFFFF }, { 0x0D28, 0x1C31, 0x1E01,		0x1B33, 0x192D, 0xFFFF }, { 0x0E20, 0x1C25, 0x1837, 0x1B29, 0x073F, 0xFFFF }, { 0x1D21, 0x1639, 0xFFFF }, { 0x0F16,
		0x083C, 0x1E13, 0x1F17, 0xFFFF }, { 0x0E22, 0x1A35, 0xFFFF }, { 0x1D23, 0x1C27, 0xFFFF }, { 0x0D2A, 0x1E11, 0x143B,		0x182F, 0xFFFF }, { 0x0E30, 0x0F14, 0x0C36, 0x1F15, 0x1B2B, 0x113D, 0xFFFF }, { 0x0F00, 0x0E24, 0x0B38, 0x1D31, 0x1F01,
		0x1A2D, 0xFFFF }, { 0x1C33, 0x1D25, 0x1937, 0xFFFF }, { 0x1E21, 0x1739, 0x1C29, 0x083F, 0xFFFF }, { 0x0F12, 0x0D34,		0x0A3A, 0x1F13, 0xFFFF }, { 0x0E26, 0x043E, 0x0C2E, 0x1B35, 0xFFFF }, { 0x1E23, 0x1D27, 0xFFFF }, { 0x0F10, 0x1F11,		0x153B, 0x192F, 0xFFFF }, { 0x0D2C, 0x123D, 0xFFFF },
	};

	static uint32_t etc1_decode_value(uint32_t diff, uint32_t inten, uint32_t selector, uint32_t packed_c)
	{
		const uint32_t limit = diff ? 32 : 16; 
		BASISU_NOTE_UNUSED(limit);
		assert((diff < 2) && (inten < 8) && (selector < 4) && (packed_c < limit));
		int c;
		if (diff)
			c = (packed_c >> 2) | (packed_c << 3);
		else
			c = packed_c | (packed_c << 4);
		c += g_etc1_inten_tables[inten][selector];
		c = clamp<int>(c, 0, 255);
		return c;
	}

	void pack_etc1_solid_color_init()
	{
		for (uint32_t diff = 0; diff < 2; diff++)
		{
			const uint32_t limit = diff ? 32 : 16;

			for (uint32_t inten = 0; inten < 8; inten++)
			{
				for (uint32_t selector = 0; selector < 4; selector++)
				{
					const uint32_t inverse_table_index = diff + (inten << 1) + (selector << 4);
					for (uint32_t color = 0; color < 256; color++)
					{
						uint32_t best_error = UINT32_MAX, best_packed_c = 0;
						for (uint32_t packed_c = 0; packed_c < limit; packed_c++)
						{
							int v = etc1_decode_value(diff, inten, selector, packed_c);
							uint32_t err = (uint32_t)labs(v - static_cast<int>(color));
							if (err < best_error)
							{
								best_error = err;
								best_packed_c = packed_c;
								if (!best_error)
									break;
							}
						}
						assert(best_error <= 255);
						g_etc1_inverse_lookup[inverse_table_index][color] = static_cast<uint16_t>(best_packed_c | (best_error << 8));
					}
				}
			}
		}

#if 0
		for (uint32_t y = 0; y < 64; y++)
		{
			printf("{");
			for (uint32_t x = 0; x < 256; x++)
			{
				printf("0x%X", g_etc1_inverse_lookup[y][x]);
				if (x != 255)
					printf(",");
				if (((x & 63) == 63) && (x != 255))
					printf("\n");
			}
			printf("},\n");
		}
#endif
	}

	// Packs solid color blocks efficiently using a set of small precomputed tables.
	// For random 888 inputs, MSE results are better than Erricson's ETC1 packer in "slow" mode ~9.5% of the time, is slightly worse only ~.01% of the time, and is equal the rest of the time.
	uint64_t pack_etc1_block_solid_color(etc_block& block, const uint8_t* pColor)
	{
		assert(g_etc1_inverse_lookup[0][255]);

		static uint32_t s_next_comp[4] = { 1, 2, 0, 1 };

		uint32_t best_error = UINT32_MAX, best_i = 0;
		int best_x = 0, best_packed_c1 = 0, best_packed_c2 = 0;

		// For each possible 8-bit value, there is a precomputed list of diff/inten/selector configurations that allow that 8-bit value to be encoded with no error.
		for (uint32_t i = 0; i < 3; i++)
		{
			const uint32_t c1 = pColor[s_next_comp[i]], c2 = pColor[s_next_comp[i + 1]];

			const int delta_range = 1;
			for (int delta = -delta_range; delta <= delta_range; delta++)
			{
				const int c_plus_delta = clamp<int>(pColor[i] + delta, 0, 255);

				const uint16_t* pTable;
				if (!c_plus_delta)
					pTable = g_etc1_color8_to_etc_block_config_0_255[0];
				else if (c_plus_delta == 255)
					pTable = g_etc1_color8_to_etc_block_config_0_255[1];
				else
					pTable = g_etc1_color8_to_etc_block_config_1_to_254[c_plus_delta - 1];

				do
				{
					const uint32_t x = *pTable++;

#ifdef _DEBUG
					const uint32_t diff = x & 1;
					const uint32_t inten = (x >> 1) & 7;
					const uint32_t selector = (x >> 4) & 3;
					const uint32_t p0 = (x >> 8) & 255;
					assert(etc1_decode_value(diff, inten, selector, p0) == (uint32_t)c_plus_delta);
#endif

					const uint16_t* pInverse_table = g_etc1_inverse_lookup[x & 0xFF];
					uint16_t p1 = pInverse_table[c1];
					uint16_t p2 = pInverse_table[c2];
					const uint32_t trial_error = square(c_plus_delta - pColor[i]) + square(p1 >> 8) + square(p2 >> 8);
					if (trial_error < best_error)
					{
						best_error = trial_error;
						best_x = x;
						best_packed_c1 = p1 & 0xFF;
						best_packed_c2 = p2 & 0xFF;
						best_i = i;
						if (!best_error)
							goto found_perfect_match;
					}
				} while (*pTable != 0xFFFF);
			}
		}
	found_perfect_match:

		const uint32_t diff = best_x & 1;
		const uint32_t inten = (best_x >> 1) & 7;

		block.m_bytes[3] = static_cast<uint8_t>(((inten | (inten << 3)) << 2) | (diff << 1));

		const uint32_t etc1_selector = g_selector_index_to_etc1[(best_x >> 4) & 3];
		*reinterpret_cast<uint16_t*>(&block.m_bytes[4]) = (etc1_selector & 2) ? 0xFFFF : 0;
		*reinterpret_cast<uint16_t*>(&block.m_bytes[6]) = (etc1_selector & 1) ? 0xFFFF : 0;

		const uint32_t best_packed_c0 = (best_x >> 8) & 255;
		if (diff)
		{
			block.m_bytes[best_i] = static_cast<uint8_t>(best_packed_c0 << 3);
			block.m_bytes[s_next_comp[best_i]] = static_cast<uint8_t>(best_packed_c1 << 3);
			block.m_bytes[s_next_comp[best_i + 1]] = static_cast<uint8_t>(best_packed_c2 << 3);
		}
		else
		{
			block.m_bytes[best_i] = static_cast<uint8_t>(best_packed_c0 | (best_packed_c0 << 4));
			block.m_bytes[s_next_comp[best_i]] = static_cast<uint8_t>(best_packed_c1 | (best_packed_c1 << 4));
			block.m_bytes[s_next_comp[best_i + 1]] = static_cast<uint8_t>(best_packed_c2 | (best_packed_c2 << 4));
		}

		return best_error;
	}
	
	const uint32_t BASISU_ETC1_CLUSTER_FIT_ORDER_TABLE_SIZE = 165;

	static const struct { uint8_t m_v[4]; } g_cluster_fit_order_tab[BASISU_ETC1_CLUSTER_FIT_ORDER_TABLE_SIZE] =
	{
		{ { 0, 0, 0, 8 } },{ { 0, 5, 2, 1 } },{ { 0, 6, 1, 1 } },{ { 0, 7, 0, 1 } },{ { 0, 7, 1, 0 } },
		{ { 0, 0, 8, 0 } },{ { 0, 0, 3, 5 } },{ { 0, 1, 7, 0 } },{ { 0, 0, 4, 4 } },{ { 0, 0, 2, 6 } },
		{ { 0, 0, 7, 1 } },{ { 0, 0, 1, 7 } },{ { 0, 0, 5, 3 } },{ { 1, 6, 0, 1 } },{ { 0, 0, 6, 2 } },
		{ { 0, 2, 6, 0 } },{ { 2, 4, 2, 0 } },{ { 0, 3, 5, 0 } },{ { 3, 3, 1, 1 } },{ { 4, 2, 0, 2 } },
		{ { 1, 5, 2, 0 } },{ { 0, 5, 3, 0 } },{ { 0, 6, 2, 0 } },{ { 2, 4, 1, 1 } },{ { 5, 1, 0, 2 } },
		{ { 6, 1, 1, 0 } },{ { 3, 3, 0, 2 } },{ { 6, 0, 0, 2 } },{ { 0, 8, 0, 0 } },{ { 6, 1, 0, 1 } },
		{ { 0, 1, 6, 1 } },{ { 1, 6, 1, 0 } },{ { 4, 1, 3, 0 } },{ { 0, 2, 5, 1 } },{ { 5, 0, 3, 0 } },
		{ { 5, 3, 0, 0 } },{ { 0, 1, 5, 2 } },{ { 0, 3, 4, 1 } },{ { 2, 5, 1, 0 } },{ { 1, 7, 0, 0 } },
		{ { 0, 1, 4, 3 } },{ { 6, 0, 2, 0 } },{ { 0, 4, 4, 0 } },{ { 2, 6, 0, 0 } },{ { 0, 2, 4, 2 } },
		{ { 0, 5, 1, 2 } },{ { 0, 6, 0, 2 } },{ { 3, 5, 0, 0 } },{ { 0, 4, 3, 1 } },{ { 3, 4, 1, 0 } },
		{ { 4, 3, 1, 0 } },{ { 1, 5, 0, 2 } },{ { 0, 3, 3, 2 } },{ { 1, 4, 1, 2 } },{ { 0, 4, 2, 2 } },
		{ { 2, 3, 3, 0 } },{ { 4, 4, 0, 0 } },{ { 1, 2, 4, 1 } },{ { 0, 5, 0, 3 } },{ { 0, 1, 3, 4 } },
		{ { 1, 5, 1, 1 } },{ { 1, 4, 2, 1 } },{ { 1, 3, 2, 2 } },{ { 5, 2, 1, 0 } },{ { 1, 3, 3, 1 } },
		{ { 0, 1, 2, 5 } },{ { 1, 1, 5, 1 } },{ { 0, 3, 2, 3 } },{ { 2, 5, 0, 1 } },{ { 3, 2, 2, 1 } },
		{ { 2, 3, 0, 3 } },{ { 1, 4, 3, 0 } },{ { 2, 2, 1, 3 } },{ { 6, 2, 0, 0 } },{ { 1, 0, 6, 1 } },
		{ { 3, 3, 2, 0 } },{ { 7, 1, 0, 0 } },{ { 3, 1, 4, 0 } },{ { 0, 2, 3, 3 } },{ { 0, 4, 1, 3 } },
		{ { 0, 4, 0, 4 } },{ { 0, 1, 0, 7 } },{ { 2, 0, 5, 1 } },{ { 2, 0, 4, 2 } },{ { 3, 0, 2, 3 } },
		{ { 2, 2, 4, 0 } },{ { 2, 2, 3, 1 } },{ { 4, 0, 3, 1 } },{ { 3, 2, 3, 0 } },{ { 2, 3, 2, 1 } },
		{ { 1, 3, 4, 0 } },{ { 7, 0, 1, 0 } },{ { 3, 0, 4, 1 } },{ { 1, 0, 5, 2 } },{ { 8, 0, 0, 0 } },
		{ { 3, 0, 1, 4 } },{ { 4, 1, 1, 2 } },{ { 4, 0, 2, 2 } },{ { 1, 2, 5, 0 } },{ { 4, 2, 1, 1 } },
		{ { 3, 4, 0, 1 } },{ { 2, 0, 3, 3 } },{ { 5, 0, 1, 2 } },{ { 5, 0, 0, 3 } },{ { 2, 4, 0, 2 } },
		{ { 2, 1, 4, 1 } },{ { 4, 0, 1, 3 } },{ { 2, 1, 5, 0 } },{ { 4, 2, 2, 0 } },{ { 4, 0, 4, 0 } },
		{ { 1, 0, 4, 3 } },{ { 1, 4, 0, 3 } },{ { 3, 0, 3, 2 } },{ { 4, 3, 0, 1 } },{ { 0, 1, 1, 6 } },
		{ { 1, 3, 1, 3 } },{ { 0, 2, 2, 4 } },{ { 2, 0, 2, 4 } },{ { 5, 1, 1, 1 } },{ { 3, 0, 5, 0 } },
		{ { 2, 3, 1, 2 } },{ { 3, 0, 0, 5 } },{ { 0, 3, 1, 4 } },{ { 5, 0, 2, 1 } },{ { 2, 1, 3, 2 } },
		{ { 2, 0, 6, 0 } },{ { 3, 1, 3, 1 } },{ { 5, 1, 2, 0 } },{ { 1, 0, 3, 4 } },{ { 1, 1, 6, 0 } },
		{ { 4, 0, 0, 4 } },{ { 2, 0, 1, 5 } },{ { 0, 3, 0, 5 } },{ { 1, 3, 0, 4 } },{ { 4, 1, 2, 1 } },
		{ { 1, 2, 3, 2 } },{ { 3, 1, 0, 4 } },{ { 5, 2, 0, 1 } },{ { 1, 2, 2, 3 } },{ { 3, 2, 1, 2 } },
		{ { 2, 2, 2, 2 } },{ { 6, 0, 1, 1 } },{ { 1, 2, 1, 4 } },{ { 1, 1, 4, 2 } },{ { 3, 2, 0, 3 } },
		{ { 1, 2, 0, 5 } },{ { 1, 0, 7, 0 } },{ { 3, 1, 2, 2 } },{ { 1, 0, 2, 5 } },{ { 2, 0, 0, 6 } },
		{ { 2, 1, 1, 4 } },{ { 2, 2, 0, 4 } },{ { 1, 1, 3, 3 } },{ { 7, 0, 0, 1 } },{ { 1, 0, 0, 7 } },
		{ { 2, 1, 2, 3 } },{ { 4, 1, 0, 3 } },{ { 3, 1, 1, 3 } },{ { 1, 1, 2, 4 } },{ { 2, 1, 0, 5 } },
		{ { 1, 0, 1, 6 } },{ { 0, 2, 1, 5 } },{ { 0, 2, 0, 6 } },{ { 1, 1, 1, 5 } },{ { 1, 1, 0, 6 } }
	};
		
	const int g_etc1_inten_tables[cETC1IntenModifierValues][cETC1SelectorValues] =
	{
		{ -8,  -2,   2,   8 }, { -17,  -5,  5,  17 }, { -29,  -9,   9,  29 }, {  -42, -13, 13,  42 },
		{ -60, -18, 18,  60 }, { -80, -24, 24,  80 }, { -106, -33, 33, 106 }, { -183, -47, 47, 183 }
	};

	const uint8_t g_etc1_to_selector_index[cETC1SelectorValues] = { 2, 3, 1, 0 };
	const uint8_t g_selector_index_to_etc1[cETC1SelectorValues] = { 3, 2, 0, 1 };

	// [flip][subblock][pixel_index]
	const etc_coord2 g_etc1_pixel_coords[2][2][8] =
	{
		{
		  {
			 { 0, 0 }, { 0, 1 }, { 0, 2 }, { 0, 3 },
			 { 1, 0 }, { 1, 1 }, { 1, 2 }, { 1, 3 }
		  },
		  {
			 { 2, 0 }, { 2, 1 }, { 2, 2 }, { 2, 3 },
			 { 3, 0 }, { 3, 1 }, { 3, 2 }, { 3, 3 }
		  }
		},
		{
		  {
			 { 0, 0 }, { 1, 0 }, { 2, 0 }, { 3, 0 },
			 { 0, 1 }, { 1, 1 }, { 2, 1 }, { 3, 1 }
		  },
		  {
			 { 0, 2 }, { 1, 2 }, { 2, 2 }, { 3, 2 },
			 { 0, 3 }, { 1, 3 }, { 2, 3 }, { 3, 3 }
		  },
		}
	};

	// [flip][subblock][pixel_index]
	const uint32_t g_etc1_pixel_indices[2][2][8] =
	{
		{
			{
				0 + 4 * 0, 0 + 4 * 1, 0 + 4 * 2, 0 + 4 * 3,
				1 + 4 * 0, 1 + 4 * 1, 1 + 4 * 2, 1 + 4 * 3
			},
			{
				2 + 4 * 0, 2 + 4 * 1, 2 + 4 * 2, 2 + 4 * 3,
				3 + 4 * 0, 3 + 4 * 1, 3 + 4 * 2, 3 + 4 * 3
			}
		},
		{
			{
				0 + 4 * 0, 1 + 4 * 0, 2 + 4 * 0, 3 + 4 * 0,
				0 + 4 * 1, 1 + 4 * 1, 2 + 4 * 1, 3 + 4 * 1
			},
			{
				0 + 4 * 2, 1 + 4 * 2, 2 + 4 * 2, 3 + 4 * 2,
				0 + 4 * 3, 1 + 4 * 3, 2 + 4 * 3, 3 + 4 * 3
			},
		}
	};

	uint16_t etc_block::pack_color5(const color_rgba& color, bool scaled, uint32_t bias)
	{
		return pack_color5(color.r, color.g, color.b, scaled, bias);
	}

	uint16_t etc_block::pack_color5(uint32_t r, uint32_t g, uint32_t b, bool scaled, uint32_t bias)
	{
		if (scaled)
		{
			r = (r * 31U + bias) / 255U;
			g = (g * 31U + bias) / 255U;
			b = (b * 31U + bias) / 255U;
		}

		r = minimum(r, 31U);
		g = minimum(g, 31U);
		b = minimum(b, 31U);

		return static_cast<uint16_t>(b | (g << 5U) | (r << 10U));
	}

	color_rgba etc_block::unpack_color5(uint16_t packed_color5, bool scaled, uint32_t alpha)
	{
		uint32_t b = packed_color5 & 31U;
		uint32_t g = (packed_color5 >> 5U) & 31U;
		uint32_t r = (packed_color5 >> 10U) & 31U;

		if (scaled)
		{
			b = (b << 3U) | (b >> 2U);
			g = (g << 3U) | (g >> 2U);
			r = (r << 3U) | (r >> 2U);
		}

		return color_rgba(cNoClamp, r, g, b, minimum(alpha, 255U));
	}

	void etc_block::unpack_color5(color_rgba& result, uint16_t packed_color5, bool scaled)
	{
		result = unpack_color5(packed_color5, scaled, 255);
	}

	void etc_block::unpack_color5(uint32_t& r, uint32_t& g, uint32_t& b, uint16_t packed_color5, bool scaled)
	{
		color_rgba c(unpack_color5(packed_color5, scaled, 0));
		r = c.r;
		g = c.g;
		b = c.b;
	}

	bool etc_block::unpack_color5(color_rgba& result, uint16_t packed_color5, uint16_t packed_delta3, bool scaled, uint32_t alpha)
	{
		color_rgba_i16 dc(unpack_delta3(packed_delta3));

		int b = (packed_color5 & 31U) + dc.b;
		int g = ((packed_color5 >> 5U) & 31U) + dc.g;
		int r = ((packed_color5 >> 10U) & 31U) + dc.r;

		bool success = true;
		if (static_cast<uint32_t>(r | g | b) > 31U)
		{
			success = false;
			r = clamp<int>(r, 0, 31);
			g = clamp<int>(g, 0, 31);
			b = clamp<int>(b, 0, 31);
		}

		if (scaled)
		{
			b = (b << 3U) | (b >> 2U);
			g = (g << 3U) | (g >> 2U);
			r = (r << 3U) | (r >> 2U);
		}

		result.set_noclamp_rgba(r, g, b, minimum(alpha, 255U));
		return success;
	}

	bool etc_block::unpack_color5(uint32_t& r, uint32_t& g, uint32_t& b, uint16_t packed_color5, uint16_t packed_delta3, bool scaled, uint32_t alpha)
	{
		color_rgba result;
		const bool success = unpack_color5(result, packed_color5, packed_delta3, scaled, alpha);
		r = result.r;
		g = result.g;
		b = result.b;
		return success;
	}

	uint16_t etc_block::pack_delta3(const color_rgba_i16& color)
	{
		return pack_delta3(color.r, color.g, color.b);
	}

	uint16_t etc_block::pack_delta3(int r, int g, int b)
	{
		assert((r >= cETC1ColorDeltaMin) && (r <= cETC1ColorDeltaMax));
		assert((g >= cETC1ColorDeltaMin) && (g <= cETC1ColorDeltaMax));
		assert((b >= cETC1ColorDeltaMin) && (b <= cETC1ColorDeltaMax));
		if (r < 0) r += 8;
		if (g < 0) g += 8;
		if (b < 0) b += 8;
		return static_cast<uint16_t>(b | (g << 3) | (r << 6));
	}

	color_rgba_i16 etc_block::unpack_delta3(uint16_t packed_delta3)
	{
		int r = (packed_delta3 >> 6) & 7;
		int g = (packed_delta3 >> 3) & 7;
		int b = packed_delta3 & 7;
		if (r >= 4) r -= 8;
		if (g >= 4) g -= 8;
		if (b >= 4) b -= 8;
		return color_rgba_i16(r, g, b, 255);
	}

	void etc_block::unpack_delta3(int& r, int& g, int& b, uint16_t packed_delta3)
	{
		r = (packed_delta3 >> 6) & 7;
		g = (packed_delta3 >> 3) & 7;
		b = packed_delta3 & 7;
		if (r >= 4) r -= 8;
		if (g >= 4) g -= 8;
		if (b >= 4) b -= 8;
	}

	uint16_t etc_block::pack_color4(const color_rgba& color, bool scaled, uint32_t bias)
	{
		return pack_color4(color.r, color.g, color.b, scaled, bias);
	}

	uint16_t etc_block::pack_color4(uint32_t r, uint32_t g, uint32_t b, bool scaled, uint32_t bias)
	{
		if (scaled)
		{
			r = (r * 15U + bias) / 255U;
			g = (g * 15U + bias) / 255U;
			b = (b * 15U + bias) / 255U;
		}

		r = minimum(r, 15U);
		g = minimum(g, 15U);
		b = minimum(b, 15U);

		return static_cast<uint16_t>(b | (g << 4U) | (r << 8U));
	}

	color_rgba etc_block::unpack_color4(uint16_t packed_color4, bool scaled, uint32_t alpha)
	{
		uint32_t b = packed_color4 & 15U;
		uint32_t g = (packed_color4 >> 4U) & 15U;
		uint32_t r = (packed_color4 >> 8U) & 15U;

		if (scaled)
		{
			b = (b << 4U) | b;
			g = (g << 4U) | g;
			r = (r << 4U) | r;
		}

		return color_rgba(cNoClamp, r, g, b, minimum(alpha, 255U));
	}

	void etc_block::unpack_color4(uint32_t& r, uint32_t& g, uint32_t& b, uint16_t packed_color4, bool scaled)
	{
		color_rgba c(unpack_color4(packed_color4, scaled, 0));
		r = c.r;
		g = c.g;
		b = c.b;
	}

	void etc_block::get_diff_subblock_colors(color_rgba* pDst, uint16_t packed_color5, uint32_t table_idx)
	{
		assert(table_idx < cETC1IntenModifierValues);
		const int *pInten_modifer_table = &g_etc1_inten_tables[table_idx][0];

		uint32_t r, g, b;
		unpack_color5(r, g, b, packed_color5, true);

		const int ir = static_cast<int>(r), ig = static_cast<int>(g), ib = static_cast<int>(b);

		const int y0 = pInten_modifer_table[0];
		pDst[0].set(ir + y0, ig + y0, ib + y0, 255);

		const int y1 = pInten_modifer_table[1];
		pDst[1].set(ir + y1, ig + y1, ib + y1, 255);

		const int y2 = pInten_modifer_table[2];
		pDst[2].set(ir + y2, ig + y2, ib + y2, 255);

		const int y3 = pInten_modifer_table[3];
		pDst[3].set(ir + y3, ig + y3, ib + y3, 255);
	}

	bool etc_block::get_diff_subblock_colors(color_rgba* pDst, uint16_t packed_color5, uint16_t packed_delta3, uint32_t table_idx)
	{
		assert(table_idx < cETC1IntenModifierValues);
		const int *pInten_modifer_table = &g_etc1_inten_tables[table_idx][0];

		uint32_t r, g, b;
		bool success = unpack_color5(r, g, b, packed_color5, packed_delta3, true);

		const int ir = static_cast<int>(r), ig = static_cast<int>(g), ib = static_cast<int>(b);

		const int y0 = pInten_modifer_table[0];
		pDst[0].set(ir + y0, ig + y0, ib + y0, 255);

		const int y1 = pInten_modifer_table[1];
		pDst[1].set(ir + y1, ig + y1, ib + y1, 255);

		const int y2 = pInten_modifer_table[2];
		pDst[2].set(ir + y2, ig + y2, ib + y2, 255);

		const int y3 = pInten_modifer_table[3];
		pDst[3].set(ir + y3, ig + y3, ib + y3, 255);

		return success;
	}

	void etc_block::get_abs_subblock_colors(color_rgba* pDst, uint16_t packed_color4, uint32_t table_idx)
	{
		assert(table_idx < cETC1IntenModifierValues);
		const int *pInten_modifer_table = &g_etc1_inten_tables[table_idx][0];

		uint32_t r, g, b;
		unpack_color4(r, g, b, packed_color4, true);

		const int ir = static_cast<int>(r), ig = static_cast<int>(g), ib = static_cast<int>(b);

		const int y0 = pInten_modifer_table[0];
		pDst[0].set(ir + y0, ig + y0, ib + y0, 255);

		const int y1 = pInten_modifer_table[1];
		pDst[1].set(ir + y1, ig + y1, ib + y1, 255);

		const int y2 = pInten_modifer_table[2];
		pDst[2].set(ir + y2, ig + y2, ib + y2, 255);

		const int y3 = pInten_modifer_table[3];
		pDst[3].set(ir + y3, ig + y3, ib + y3, 255);
	}
		
	bool unpack_etc1(const etc_block& block, color_rgba *pDst, bool preserve_alpha)
	{
		const bool diff_flag = block.get_diff_bit();
		const bool flip_flag = block.get_flip_bit();
		const uint32_t table_index0 = block.get_inten_table(0);
		const uint32_t table_index1 = block.get_inten_table(1);

		color_rgba subblock_colors0[4];
		color_rgba subblock_colors1[4];

		if (diff_flag)
		{
			const uint16_t base_color5 = block.get_base5_color();
			const uint16_t delta_color3 = block.get_delta3_color();
			etc_block::get_diff_subblock_colors(subblock_colors0, base_color5, table_index0);

			if (!etc_block::get_diff_subblock_colors(subblock_colors1, base_color5, delta_color3, table_index1))
				return false;
		}
		else
		{
			const uint16_t base_color4_0 = block.get_base4_color(0);
			etc_block::get_abs_subblock_colors(subblock_colors0, base_color4_0, table_index0);

			const uint16_t base_color4_1 = block.get_base4_color(1);
			etc_block::get_abs_subblock_colors(subblock_colors1, base_color4_1, table_index1);
		}

		if (preserve_alpha)
		{
			if (flip_flag)
			{
				for (uint32_t y = 0; y < 2; y++)
				{
					pDst[0].set_rgb(subblock_colors0[block.get_selector(0, y)]);
					pDst[1].set_rgb(subblock_colors0[block.get_selector(1, y)]);
					pDst[2].set_rgb(subblock_colors0[block.get_selector(2, y)]);
					pDst[3].set_rgb(subblock_colors0[block.get_selector(3, y)]);
					pDst += 4;
				}

				for (uint32_t y = 2; y < 4; y++)
				{
					pDst[0].set_rgb(subblock_colors1[block.get_selector(0, y)]);
					pDst[1].set_rgb(subblock_colors1[block.get_selector(1, y)]);
					pDst[2].set_rgb(subblock_colors1[block.get_selector(2, y)]);
					pDst[3].set_rgb(subblock_colors1[block.get_selector(3, y)]);
					pDst += 4;
				}
			}
			else
			{
				for (uint32_t y = 0; y < 4; y++)
				{
					pDst[0].set_rgb(subblock_colors0[block.get_selector(0, y)]);
					pDst[1].set_rgb(subblock_colors0[block.get_selector(1, y)]);
					pDst[2].set_rgb(subblock_colors1[block.get_selector(2, y)]);
					pDst[3].set_rgb(subblock_colors1[block.get_selector(3, y)]);
					pDst += 4;
				}
			}
		}
		else
		{
			if (flip_flag)
			{
				// 0000
				// 0000
				// 1111
				// 1111
				for (uint32_t y = 0; y < 2; y++)
				{
					pDst[0] = subblock_colors0[block.get_selector(0, y)];
					pDst[1] = subblock_colors0[block.get_selector(1, y)];
					pDst[2] = subblock_colors0[block.get_selector(2, y)];
					pDst[3] = subblock_colors0[block.get_selector(3, y)];
					pDst += 4;
				}

				for (uint32_t y = 2; y < 4; y++)
				{
					pDst[0] = subblock_colors1[block.get_selector(0, y)];
					pDst[1] = subblock_colors1[block.get_selector(1, y)];
					pDst[2] = subblock_colors1[block.get_selector(2, y)];
					pDst[3] = subblock_colors1[block.get_selector(3, y)];
					pDst += 4;
				}
			}
			else
			{
				// 0011
				// 0011
				// 0011
				// 0011
				for (uint32_t y = 0; y < 4; y++)
				{
					pDst[0] = subblock_colors0[block.get_selector(0, y)];
					pDst[1] = subblock_colors0[block.get_selector(1, y)];
					pDst[2] = subblock_colors1[block.get_selector(2, y)];
					pDst[3] = subblock_colors1[block.get_selector(3, y)];
					pDst += 4;
				}
			}
		}

		return true;
	}

	inline int extend_6_to_8(uint32_t n)
	{
		return (n << 2) | (n >> 4);
	}

	inline int extend_7_to_8(uint32_t n)
	{
		return (n << 1) | (n >> 6);
	}

	inline int extend_4_to_8(uint32_t n)
	{
		return (n << 4) | n;
	}
		
	uint64_t etc_block::evaluate_etc1_error(const color_rgba* pBlock_pixels, bool perceptual, int subblock_index) const
	{
		color_rgba unpacked_block[16];

		unpack_etc1(*this, unpacked_block);

		uint64_t total_error = 0;

		if (subblock_index < 0)
		{
			for (uint32_t i = 0; i < 16; i++)
				total_error += color_distance(perceptual, pBlock_pixels[i], unpacked_block[i], false);
		}
		else
		{
			const bool flip_bit = get_flip_bit();

			for (uint32_t i = 0; i < 8; i++)
			{
				const uint32_t idx = g_etc1_pixel_indices[flip_bit][subblock_index][i];

				total_error += color_distance(perceptual, pBlock_pixels[idx], unpacked_block[idx], false);
			}
		}

		return total_error;
	}

	void etc_block::get_subblock_pixels(color_rgba* pPixels, int subblock_index) const
	{
		if (subblock_index < 0)
			unpack_etc1(*this, pPixels);
		else
		{
			color_rgba unpacked_block[16];

			unpack_etc1(*this, unpacked_block);

			const bool flip_bit = get_flip_bit();

			for (uint32_t i = 0; i < 8; i++)
			{
				const uint32_t idx = g_etc1_pixel_indices[flip_bit][subblock_index][i];

				pPixels[i] = unpacked_block[idx];
			}
		}
	}
								
	bool etc1_optimizer::compute()
	{
		assert(m_pResult->m_pSelectors);

		if (m_pParams->m_pForce_selectors)
		{
			assert(m_pParams->m_quality >= cETCQualitySlow);
			if (m_pParams->m_quality < cETCQualitySlow)
				return false;
		}

		const uint32_t n = m_pParams->m_num_src_pixels;

		if (m_pParams->m_cluster_fit)
		{
			if (m_pParams->m_quality == cETCQualityFast)
				compute_internal_cluster_fit(4);
			else if (m_pParams->m_quality == cETCQualityMedium)
				compute_internal_cluster_fit(16);
			else if (m_pParams->m_quality == cETCQualitySlow)
				compute_internal_cluster_fit(64);
			else
				compute_internal_cluster_fit(BASISU_ETC1_CLUSTER_FIT_ORDER_TABLE_SIZE);
		}
		else
			compute_internal_neighborhood(m_br, m_bg, m_bb);

		if (!m_best_solution.m_valid)
		{
			m_pResult->m_error = UINT32_MAX;
			return false;
		}

		//const uint8_t* pSelectors = &m_best_solution.m_selectors[0];
		const uint8_t* pSelectors = m_pParams->m_pForce_selectors ? m_pParams->m_pForce_selectors : &m_best_solution.m_selectors[0];

#if defined(DEBUG) || defined(_DEBUG)
		{
			// sanity check the returned error
			color_rgba block_colors[4];
			m_best_solution.m_coords.get_block_colors(block_colors);

			const color_rgba* pSrc_pixels = m_pParams->m_pSrc_pixels;
			uint64_t actual_error = 0;
			
			bool perceptual;
			if (m_pParams->m_quality >= cETCQualityMedium)
				perceptual = m_pParams->m_perceptual;
			else
				perceptual = (m_pParams->m_quality == cETCQualityFast) ? false : m_pParams->m_perceptual;
						
			for (uint32_t i = 0; i < n; i++)
				actual_error += color_distance(perceptual, pSrc_pixels[i], block_colors[pSelectors[i]], false);

			assert(actual_error == m_best_solution.m_error);
		}
#endif      

		m_pResult->m_error = m_best_solution.m_error;

		m_pResult->m_block_color_unscaled = m_best_solution.m_coords.m_unscaled_color;
		m_pResult->m_block_color4 = m_best_solution.m_coords.m_color4;

		m_pResult->m_block_inten_table = m_best_solution.m_coords.m_inten_table;
		memcpy(m_pResult->m_pSelectors, pSelectors, n);
		m_pResult->m_n = n;

		return true;
	}

	void etc1_optimizer::refine_solution(uint32_t max_refinement_trials)
	{
		// Now we have the input block, the avg. color of the input pixels, a set of trial selector indices, and the block color+intensity index.
		// Now, for each component, attempt to refine the current solution by solving a simple linear equation. For example, for 4 colors:
		// The goal is:
		// pixel0 - (block_color+inten_table[selector0]) + pixel1 - (block_color+inten_table[selector1]) + pixel2 - (block_color+inten_table[selector2]) + pixel3 - (block_color+inten_table[selector3]) = 0
		// Rearranging this:
		// (pixel0 + pixel1 + pixel2 + pixel3) - (block_color+inten_table[selector0]) - (block_color+inten_table[selector1]) - (block_color+inten_table[selector2]) - (block_color+inten_table[selector3]) = 0
		// (pixel0 + pixel1 + pixel2 + pixel3) - block_color - inten_table[selector0] - block_color-inten_table[selector1] - block_color-inten_table[selector2] - block_color-inten_table[selector3] = 0
		// (pixel0 + pixel1 + pixel2 + pixel3) - 4*block_color - inten_table[selector0] - inten_table[selector1] - inten_table[selector2] - inten_table[selector3] = 0
		// (pixel0 + pixel1 + pixel2 + pixel3) - 4*block_color - (inten_table[selector0] + inten_table[selector1] + inten_table[selector2] + inten_table[selector3]) = 0
		// (pixel0 + pixel1 + pixel2 + pixel3)/4 - block_color - (inten_table[selector0] + inten_table[selector1] + inten_table[selector2] + inten_table[selector3])/4 = 0
		// block_color = (pixel0 + pixel1 + pixel2 + pixel3)/4 - (inten_table[selector0] + inten_table[selector1] + inten_table[selector2] + inten_table[selector3])/4
		// So what this means:
		// optimal_block_color = avg_input - avg_inten_delta
		// So the optimal block color can be computed by taking the average block color and subtracting the current average of the intensity delta.
		// Unfortunately, optimal_block_color must then be quantized to 555 or 444 so it's not always possible to improve matters using this formula.
		// Also, the above formula is for unclamped intensity deltas. The actual implementation takes into account clamping.

		const uint32_t n = m_pParams->m_num_src_pixels;

		for (uint32_t refinement_trial = 0; refinement_trial < max_refinement_trials; refinement_trial++)
		{
			const uint8_t* pSelectors = &m_best_solution.m_selectors[0];
			const int* pInten_table = g_etc1_inten_tables[m_best_solution.m_coords.m_inten_table];

			int delta_sum_r = 0, delta_sum_g = 0, delta_sum_b = 0;
			const color_rgba base_color(m_best_solution.m_coords.get_scaled_color());
			for (uint32_t r = 0; r < n; r++)
			{
				const uint32_t s = *pSelectors++;
				const int yd_temp = pInten_table[s];
				// Compute actual delta being applied to each pixel, taking into account clamping.
				delta_sum_r += clamp<int>(base_color.r + yd_temp, 0, 255) - base_color.r;
				delta_sum_g += clamp<int>(base_color.g + yd_temp, 0, 255) - base_color.g;
				delta_sum_b += clamp<int>(base_color.b + yd_temp, 0, 255) - base_color.b;
			}

			if ((!delta_sum_r) && (!delta_sum_g) && (!delta_sum_b))
				break;

			const float avg_delta_r_f = static_cast<float>(delta_sum_r) / n;
			const float avg_delta_g_f = static_cast<float>(delta_sum_g) / n;
			const float avg_delta_b_f = static_cast<float>(delta_sum_b) / n;
			const int br1 = clamp<int>(static_cast<int32_t>((m_avg_color[0] - avg_delta_r_f) * m_limit / 255.0f + .5f), 0, m_limit);
			const int bg1 = clamp<int>(static_cast<int32_t>((m_avg_color[1] - avg_delta_g_f) * m_limit / 255.0f + .5f), 0, m_limit);
			const int bb1 = clamp<int>(static_cast<int32_t>((m_avg_color[2] - avg_delta_b_f) * m_limit / 255.0f + .5f), 0, m_limit);

#if BASISU_DEBUG_ETC_ENCODER_DEEPER
			printf("Refinement trial %u, avg_delta %f %f %f\n", refinement_trial, avg_delta_r_f, avg_delta_g_f, avg_delta_b_f);
#endif

			if (!evaluate_solution(etc1_solution_coordinates(br1, bg1, bb1, 0, m_pParams->m_use_color4), m_trial_solution, &m_best_solution))
				break;

		}  // refinement_trial
	}

	void etc1_optimizer::compute_internal_neighborhood(int scan_r, int scan_g, int scan_b)
	{
		if (m_best_solution.m_error == 0)
			return;

		//const uint32_t n = m_pParams->m_num_src_pixels;
		const int scan_delta_size = m_pParams->m_scan_delta_size;

		// Scan through a subset of the 3D lattice centered around the avg block color trying each 3D (555 or 444) lattice point as a potential block color.
		// Each time a better solution is found try to refine the current solution's block color based of the current selectors and intensity table index.
		for (int zdi = 0; zdi < scan_delta_size; zdi++)
		{
			const int zd = m_pParams->m_pScan_deltas[zdi];
			const int mbb = scan_b + zd;
			if (mbb < 0) continue; else if (mbb > m_limit) break;

			for (int ydi = 0; ydi < scan_delta_size; ydi++)
			{
				const int yd = m_pParams->m_pScan_deltas[ydi];
				const int mbg = scan_g + yd;
				if (mbg < 0) continue; else if (mbg > m_limit) break;

				for (int xdi = 0; xdi < scan_delta_size; xdi++)
				{
					const int xd = m_pParams->m_pScan_deltas[xdi];
					const int mbr = scan_r + xd;
					if (mbr < 0) continue; else if (mbr > m_limit) break;

					etc1_solution_coordinates coords(mbr, mbg, mbb, 0, m_pParams->m_use_color4);

					if (!evaluate_solution(coords, m_trial_solution, &m_best_solution))
						continue;

					if (m_pParams->m_refinement)
					{
						refine_solution((m_pParams->m_quality == cETCQualityFast) ? 2 : (((xd | yd | zd) == 0) ? 4 : 2));
					}

				} // xdi
			} // ydi
		} // zdi
	}

	void etc1_optimizer::compute_internal_cluster_fit(uint32_t total_perms_to_try)
	{
		if ((!m_best_solution.m_valid) || ((m_br != m_best_solution.m_coords.m_unscaled_color.r) || (m_bg != m_best_solution.m_coords.m_unscaled_color.g) || (m_bb != m_best_solution.m_coords.m_unscaled_color.b)))
		{
			evaluate_solution(etc1_solution_coordinates(m_br, m_bg, m_bb, 0, m_pParams->m_use_color4), m_trial_solution, &m_best_solution);
		}

		if ((m_best_solution.m_error == 0) || (!m_best_solution.m_valid))
			return;

		for (uint32_t i = 0; i < total_perms_to_try; i++)
		{
			int delta_sum_r = 0, delta_sum_g = 0, delta_sum_b = 0;

			const int *pInten_table = g_etc1_inten_tables[m_best_solution.m_coords.m_inten_table];
			const color_rgba base_color(m_best_solution.m_coords.get_scaled_color());

			const uint8_t *pNum_selectors = g_cluster_fit_order_tab[i].m_v;

			for (uint32_t q = 0; q < 4; q++)
			{
				const int yd_temp = pInten_table[q];

				delta_sum_r += pNum_selectors[q] * (clamp<int>(base_color.r + yd_temp, 0, 255) - base_color.r);
				delta_sum_g += pNum_selectors[q] * (clamp<int>(base_color.g + yd_temp, 0, 255) - base_color.g);
				delta_sum_b += pNum_selectors[q] * (clamp<int>(base_color.b + yd_temp, 0, 255) - base_color.b);
			}

			if ((!delta_sum_r) && (!delta_sum_g) && (!delta_sum_b))
				continue;

			const float avg_delta_r_f = static_cast<float>(delta_sum_r) / 8;
			const float avg_delta_g_f = static_cast<float>(delta_sum_g) / 8;
			const float avg_delta_b_f = static_cast<float>(delta_sum_b) / 8;

			const int br1 = clamp<int>(static_cast<int32_t>((m_avg_color[0] - avg_delta_r_f) * m_limit / 255.0f + .5f), 0, m_limit);
			const int bg1 = clamp<int>(static_cast<int32_t>((m_avg_color[1] - avg_delta_g_f) * m_limit / 255.0f + .5f), 0, m_limit);
			const int bb1 = clamp<int>(static_cast<int32_t>((m_avg_color[2] - avg_delta_b_f) * m_limit / 255.0f + .5f), 0, m_limit);

#if BASISU_DEBUG_ETC_ENCODER_DEEPER
			printf("Second refinement trial %u, avg_delta %f %f %f\n", i, avg_delta_r_f, avg_delta_g_f, avg_delta_b_f);
#endif

			evaluate_solution(etc1_solution_coordinates(br1, bg1, bb1, 0, m_pParams->m_use_color4), m_trial_solution, &m_best_solution);

			if (m_best_solution.m_error == 0)
				break;
		}
	}

	void etc1_optimizer::init(const params& params, results& result)
	{
		m_pParams = &params;
		m_pResult = &result;

		const uint32_t n = m_pParams->m_num_src_pixels;

		m_selectors.resize(n);
		m_best_selectors.resize(n);
		m_temp_selectors.resize(n);
		m_trial_solution.m_selectors.resize(n);
		m_best_solution.m_selectors.resize(n);

		m_limit = m_pParams->m_use_color4 ? 15 : 31;

		vec3F avg_color(0.0f);

		m_luma.resize(n);
		m_sorted_luma_indices.resize(n);
		m_sorted_luma.resize(n);
		
		int min_r = 255, min_g = 255, min_b = 255;
		int max_r = 0, max_g = 0, max_b = 0;
		
		for (uint32_t i = 0; i < n; i++)
		{
			const color_rgba& c = m_pParams->m_pSrc_pixels[i];

			min_r = basisu::minimum<int>(min_r, c.r);
			min_g = basisu::minimum<int>(min_g, c.g);
			min_b = basisu::minimum<int>(min_b, c.b);

			max_r = basisu::maximum<int>(max_r, c.r);
			max_g = basisu::maximum<int>(max_g, c.g);
			max_b = basisu::maximum<int>(max_b, c.b);

			const vec3F fc(c.r, c.g, c.b);

			avg_color += fc;

			m_luma[i] = static_cast<uint16_t>(c.r + c.g + c.b);
			m_sorted_luma_indices[i] = i;
		}
		avg_color /= static_cast<float>(n);
		m_avg_color = avg_color;
		m_max_comp_spread = basisu::maximum(basisu::maximum(max_r - min_r, max_g - min_g), max_b - min_b);

		m_br = clamp<int>(static_cast<uint32_t>(m_avg_color[0] * m_limit / 255.0f + .5f), 0, m_limit);
		m_bg = clamp<int>(static_cast<uint32_t>(m_avg_color[1] * m_limit / 255.0f + .5f), 0, m_limit);
		m_bb = clamp<int>(static_cast<uint32_t>(m_avg_color[2] * m_limit / 255.0f + .5f), 0, m_limit);

#if BASISU_DEBUG_ETC_ENCODER_DEEPER
		printf("Avg block color: %u %u %u\n", m_br, m_bg, m_bb);
#endif

		if (m_pParams->m_quality == cETCQualityFast)
		{
			indirect_sort(n, &m_sorted_luma_indices[0], &m_luma[0]);

			m_pSorted_luma = &m_sorted_luma[0];
			m_pSorted_luma_indices = &m_sorted_luma_indices[0];
			
			for (uint32_t i = 0; i < n; i++)
				m_pSorted_luma[i] = m_luma[m_pSorted_luma_indices[i]];
		}

		m_best_solution.m_coords.clear();
		m_best_solution.m_valid = false;
		m_best_solution.m_error = UINT64_MAX;

		clear_obj(m_solutions_tried);
	}

	// Return false if we've probably already tried this solution, true if we have definitely not.
	bool etc1_optimizer::check_for_redundant_solution(const etc1_solution_coordinates& coords)
	{
		// Hash first 3 bytes of color (RGB)
		uint32_t kh = hash_hsieh((uint8_t*)&coords.m_unscaled_color.r, 3);

		uint32_t h0 = kh & cSolutionsTriedHashMask;
		uint32_t h1 = (kh >> cSolutionsTriedHashBits) & cSolutionsTriedHashMask;

		// Simple Bloom filter lookup with k=2
		if ( ((m_solutions_tried[h0 >> 3] & (1 << (h0 & 7))) != 0) &&
		     ((m_solutions_tried[h1 >> 3] & (1 << (h1 & 7))) != 0) )
			return false;

		m_solutions_tried[h0 >> 3] |= (1 << (h0 & 7));
		m_solutions_tried[h1 >> 3] |= (1 << (h1 & 7));

		return true;
	}
		
	static uint8_t g_eval_dist_tables[8][256] =
	{
		// 99% threshold
		{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,},
		{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,},
		{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,1,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,},
		{ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,1,0,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,},
		{ 1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,0,1,0,0,0,0,0,0,0,0,0,1,0,0,1,},
		{ 1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,1,0,0,0,0,1,0,1,1,0,1,1,1,1,1,0,1,1,1,0,1,1,0,0,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,},
		{ 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,},
		{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0,0,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,}
	};

	bool etc1_optimizer::evaluate_solution_slow(const etc1_solution_coordinates& coords, potential_solution& trial_solution, potential_solution* pBest_solution)
	{
		if (!check_for_redundant_solution(coords))
			return false;

#if BASISU_DEBUG_ETC_ENCODER_DEEPER
		printf("Eval solution: %u %u %u\n", coords.m_unscaled_color.r, coords.m_unscaled_color.g, coords.m_unscaled_color.b);
#endif

		trial_solution.m_valid = false;

		if (m_pParams->m_constrain_against_base_color5)
		{
			const int dr = (int)coords.m_unscaled_color.r - (int)m_pParams->m_base_color5.r;
			const int dg = (int)coords.m_unscaled_color.g - (int)m_pParams->m_base_color5.g;
			const int db = (int)coords.m_unscaled_color.b - (int)m_pParams->m_base_color5.b;

			if ((minimum(dr, dg, db) < cETC1ColorDeltaMin) || (maximum(dr, dg, db) > cETC1ColorDeltaMax))
			{
#if BASISU_DEBUG_ETC_ENCODER_DEEPER
				printf("Eval failed due to constraint from %u %u %u\n", m_pParams->m_base_color5.r, m_pParams->m_base_color5.g, m_pParams->m_base_color5.b);
#endif
				return false;
			}
		}

		const color_rgba base_color(coords.get_scaled_color());

		const uint32_t n = m_pParams->m_num_src_pixels;
		assert(trial_solution.m_selectors.size() == n);

		trial_solution.m_error = INT64_MAX;

		const uint8_t *pSelectors_to_use = m_pParams->m_pForce_selectors;

		for (uint32_t inten_table = 0; inten_table < cETC1IntenModifierValues; inten_table++)
		{
			if (m_pParams->m_quality <= cETCQualityMedium)
			{
				if (!g_eval_dist_tables[inten_table][m_max_comp_spread])
					continue;
			}

#if 0
			if (m_pParams->m_quality <= cETCQualityMedium)
			{
				// For tables 5-7, if the max component spread falls within certain ranges, skip the inten table. Statistically they are extremely unlikely to result in lower error.
				if (inten_table == 7)
				{
					if (m_max_comp_spread < 42)
						continue;
				}
				else if (inten_table == 6)
				{
					if ((m_max_comp_spread >= 12) && (m_max_comp_spread <= 31))
						continue;
				}
				else if (inten_table == 5)
				{
					if ((m_max_comp_spread >= 13) && (m_max_comp_spread <= 21))
						continue;
				}
			}
#endif

			const int* pInten_table = g_etc1_inten_tables[inten_table];

			color_rgba block_colors[4];
			for (uint32_t s = 0; s < 4; s++)
			{
				const int yd = pInten_table[s];
				block_colors[s].set(base_color.r + yd, base_color.g + yd, base_color.b + yd, 255);
			}

			uint64_t total_error = 0;

			const color_rgba* pSrc_pixels = m_pParams->m_pSrc_pixels;

			if (!g_cpu_supports_sse41)
			{
				for (uint32_t c = 0; c < n; c++)
				{
					const color_rgba& src_pixel = *pSrc_pixels++;

					uint32_t best_selector_index = 0;
					uint32_t best_error = 0;

					if (pSelectors_to_use)
					{
						best_selector_index = pSelectors_to_use[c];
						best_error = color_distance(m_pParams->m_perceptual, src_pixel, block_colors[best_selector_index], false);
					}
					else
					{
						best_error = color_distance(m_pParams->m_perceptual, src_pixel, block_colors[0], false);

						uint32_t trial_error = color_distance(m_pParams->m_perceptual, src_pixel, block_colors[1], false);
						if (trial_error < best_error)
						{
							best_error = trial_error;
							best_selector_index = 1;
						}

						trial_error = color_distance(m_pParams->m_perceptual, src_pixel, block_colors[2], false);
						if (trial_error < best_error)
						{
							best_error = trial_error;
							best_selector_index = 2;
						}

						trial_error = color_distance(m_pParams->m_perceptual, src_pixel, block_colors[3], false);
						if (trial_error < best_error)
						{
							best_error = trial_error;
							best_selector_index = 3;
						}
					}

					m_temp_selectors[c] = static_cast<uint8_t>(best_selector_index);

					total_error += best_error;
					if (total_error >= trial_solution.m_error)
						break;
				}
			}
			else
			{
#if BASISU_SUPPORT_SSE
				if (pSelectors_to_use)
				{
					if (m_pParams->m_perceptual)
						perceptual_distance_rgb_4_N_sse41((int64_t*)&total_error, pSelectors_to_use, block_colors, pSrc_pixels, n, trial_solution.m_error);
					else
						linear_distance_rgb_4_N_sse41((int64_t*)&total_error, pSelectors_to_use, block_colors, pSrc_pixels, n, trial_solution.m_error);
				}
				else
				{
					if (m_pParams->m_perceptual)
						find_selectors_perceptual_rgb_4_N_sse41((int64_t*)&total_error, &m_temp_selectors[0], block_colors, pSrc_pixels, n, trial_solution.m_error);
					else
						find_selectors_linear_rgb_4_N_sse41((int64_t*)&total_error, &m_temp_selectors[0], block_colors, pSrc_pixels, n, trial_solution.m_error);
				}
#endif
			}

			if (total_error < trial_solution.m_error)
			{
				trial_solution.m_error = total_error;
				trial_solution.m_coords.m_inten_table = inten_table;
				trial_solution.m_selectors.swap(m_temp_selectors);
				trial_solution.m_valid = true;
			}
		}
		trial_solution.m_coords.m_unscaled_color = coords.m_unscaled_color;
		trial_solution.m_coords.m_color4 = m_pParams->m_use_color4;
				
#if BASISU_DEBUG_ETC_ENCODER_DEEPER
		printf("Eval done: %u error: %I64u best error so far: %I64u\n", (trial_solution.m_error < pBest_solution->m_error), trial_solution.m_error, pBest_solution->m_error);
#endif

		bool success = false;
		if (pBest_solution)
		{
			if (trial_solution.m_error < pBest_solution->m_error)
			{
				*pBest_solution = trial_solution;
				success = true;
			}
		}
				
		return success;
	}

	bool etc1_optimizer::evaluate_solution_fast(const etc1_solution_coordinates& coords, potential_solution& trial_solution, potential_solution* pBest_solution)
	{
		if (!check_for_redundant_solution(coords))
			return false;

#if BASISU_DEBUG_ETC_ENCODER_DEEPER
		printf("Eval solution fast: %u %u %u\n", coords.m_unscaled_color.r, coords.m_unscaled_color.g, coords.m_unscaled_color.b);
#endif

		if (m_pParams->m_constrain_against_base_color5)
		{
			const int dr = (int)coords.m_unscaled_color.r - (int)m_pParams->m_base_color5.r;
			const int dg = (int)coords.m_unscaled_color.g - (int)m_pParams->m_base_color5.g;
			const int db = (int)coords.m_unscaled_color.b - (int)m_pParams->m_base_color5.b;

			if ((minimum(dr, dg, db) < cETC1ColorDeltaMin) || (maximum(dr, dg, db) > cETC1ColorDeltaMax))
			{
				trial_solution.m_valid = false;

#if BASISU_DEBUG_ETC_ENCODER_DEEPER
				printf("Eval failed due to constraint from %u %u %u\n", m_pParams->m_base_color5.r, m_pParams->m_base_color5.g, m_pParams->m_base_color5.b);
#endif
				return false;
			}
		}

		const color_rgba base_color(coords.get_scaled_color());
		
		const uint32_t n = m_pParams->m_num_src_pixels;
		assert(trial_solution.m_selectors.size() == n);

		trial_solution.m_error = UINT64_MAX;
								
		const bool perceptual = (m_pParams->m_quality == cETCQualityFast) ? false : m_pParams->m_perceptual;
				
		for (int inten_table = cETC1IntenModifierValues - 1; inten_table >= 0; --inten_table)
		{
			const int* pInten_table = g_etc1_inten_tables[inten_table];

			uint32_t block_inten[4];
			color_rgba block_colors[4];
			for (uint32_t s = 0; s < 4; s++)
			{
				const int yd = pInten_table[s];
				color_rgba block_color(base_color.r + yd, base_color.g + yd, base_color.b + yd, 255);
				block_colors[s] = block_color;
				block_inten[s] = block_color.r + block_color.g + block_color.b;
			}

			// evaluate_solution_fast() enforces/assumes a total ordering of the input colors along the intensity (1,1,1) axis to more quickly classify the inputs to selectors.
			// The inputs colors have been presorted along the projection onto this axis, and ETC1 block colors are always ordered along the intensity axis, so this classification is fast.
			// 0   1   2   3
			//   01  12  23
			const uint32_t block_inten_midpoints[3] = { block_inten[0] + block_inten[1], block_inten[1] + block_inten[2], block_inten[2] + block_inten[3] };
															
			uint64_t total_error = 0;
			const color_rgba* pSrc_pixels = m_pParams->m_pSrc_pixels;
						
			if (perceptual)
			{
				if ((m_pSorted_luma[n - 1] * 2) < block_inten_midpoints[0])
				{
					if (block_inten[0] > m_pSorted_luma[n - 1])
					{
						const uint32_t min_error = iabs((int)block_inten[0] - (int)m_pSorted_luma[n - 1]);
						if (min_error >= trial_solution.m_error)
							continue;
					}

					memset(&m_temp_selectors[0], 0, n);

					for (uint32_t c = 0; c < n; c++)
						total_error += color_distance(true, block_colors[0], pSrc_pixels[c], false);
				}
				else if ((m_pSorted_luma[0] * 2) >= block_inten_midpoints[2])
				{
					if (m_pSorted_luma[0] > block_inten[3])
					{
						const uint32_t min_error = iabs((int)m_pSorted_luma[0] - (int)block_inten[3]);
						if (min_error >= trial_solution.m_error)
							continue;
					}

					memset(&m_temp_selectors[0], 3, n);

					for (uint32_t c = 0; c < n; c++)
						total_error += color_distance(true, block_colors[3], pSrc_pixels[c], false);
				}
				else
				{
					if (!g_cpu_supports_sse41)
					{
						uint32_t cur_selector = 0, c;
						for (c = 0; c < n; c++)
						{
							const uint32_t y = m_pSorted_luma[c];
							while ((y * 2) >= block_inten_midpoints[cur_selector])
								if (++cur_selector > 2)
									goto done;
							const uint32_t sorted_pixel_index = m_pSorted_luma_indices[c];
							m_temp_selectors[sorted_pixel_index] = static_cast<uint8_t>(cur_selector);
							total_error += color_distance(true, block_colors[cur_selector], pSrc_pixels[sorted_pixel_index], false);
						}
					done:
						while (c < n)
						{
							const uint32_t sorted_pixel_index = m_pSorted_luma_indices[c];
							m_temp_selectors[sorted_pixel_index] = 3;
							total_error += color_distance(true, block_colors[3], pSrc_pixels[sorted_pixel_index], false);
							++c;
						}
					}
					else
					{
#if BASISU_SUPPORT_SSE
						uint32_t cur_selector = 0, c;

						for (c = 0; c < n; c++)
						{
							const uint32_t y = m_pSorted_luma[c];
							while ((y * 2) >= block_inten_midpoints[cur_selector])
							{
								if (++cur_selector > 2)
									goto done3;
							}
							const uint32_t sorted_pixel_index = m_pSorted_luma_indices[c];
							m_temp_selectors[sorted_pixel_index] = static_cast<uint8_t>(cur_selector);
						}
					done3:

						while (c < n)
						{
							const uint32_t sorted_pixel_index = m_pSorted_luma_indices[c];
							m_temp_selectors[sorted_pixel_index] = 3;
							++c;
						}

						int64_t block_error;
						perceptual_distance_rgb_4_N_sse41(&block_error, &m_temp_selectors[0], block_colors, pSrc_pixels, n, INT64_MAX);
						total_error += block_error;
#endif
					}
				}
			}
			else
			{
				if ((m_pSorted_luma[n - 1] * 2) < block_inten_midpoints[0])
				{
					if (block_inten[0] > m_pSorted_luma[n - 1])
					{
						const uint32_t min_error = iabs((int)block_inten[0] - (int)m_pSorted_luma[n - 1]);
						if (min_error >= trial_solution.m_error)
							continue;
					}

					memset(&m_temp_selectors[0], 0, n);

					for (uint32_t c = 0; c < n; c++)
						total_error += color_distance(block_colors[0], pSrc_pixels[c], false);
				}
				else if ((m_pSorted_luma[0] * 2) >= block_inten_midpoints[2])
				{
					if (m_pSorted_luma[0] > block_inten[3])
					{
						const uint32_t min_error = iabs((int)m_pSorted_luma[0] - (int)block_inten[3]);
						if (min_error >= trial_solution.m_error)
							continue;
					}

					memset(&m_temp_selectors[0], 3, n);

					for (uint32_t c = 0; c < n; c++)
						total_error += color_distance(block_colors[3], pSrc_pixels[c], false);
				}
				else
				{
					uint32_t cur_selector = 0, c;
					for (c = 0; c < n; c++)
					{
						const uint32_t y = m_pSorted_luma[c];
						while ((y * 2) >= block_inten_midpoints[cur_selector])
							if (++cur_selector > 2)
								goto done2;
						const uint32_t sorted_pixel_index = m_pSorted_luma_indices[c];
						m_temp_selectors[sorted_pixel_index] = static_cast<uint8_t>(cur_selector);
						total_error += color_distance(block_colors[cur_selector], pSrc_pixels[sorted_pixel_index], false);
					}
				done2:
					while (c < n)
					{
						const uint32_t sorted_pixel_index = m_pSorted_luma_indices[c];
						m_temp_selectors[sorted_pixel_index] = 3;
						total_error += color_distance(block_colors[3], pSrc_pixels[sorted_pixel_index], false);
						++c;
					}
				}
			}

			if (total_error < trial_solution.m_error)
			{
				trial_solution.m_error = total_error;
				trial_solution.m_coords.m_inten_table = inten_table;
				trial_solution.m_selectors.swap(m_temp_selectors);
				trial_solution.m_valid = true;
				if (!total_error)
					break;
			}
		}
		trial_solution.m_coords.m_unscaled_color = coords.m_unscaled_color;
		trial_solution.m_coords.m_color4 = m_pParams->m_use_color4;

#if BASISU_DEBUG_ETC_ENCODER_DEEPER
		printf("Eval done: %u error: %I64u best error so far: %I64u\n", (trial_solution.m_error < pBest_solution->m_error), trial_solution.m_error, pBest_solution->m_error);
#endif

		bool success = false;
		if (pBest_solution)
		{
			if (trial_solution.m_error < pBest_solution->m_error)
			{
				*pBest_solution = trial_solution;
				success = true;
			}
		}

		return success;
	}

	uint64_t pack_eac_a8(pack_eac_a8_results& results, const uint8_t* pPixels, uint32_t num_pixels, uint32_t base_search_rad, uint32_t mul_search_rad, uint32_t table_mask)
	{
		results.m_selectors.resize(num_pixels);
		results.m_selectors_temp.resize(num_pixels);

		uint32_t min_alpha = 255, max_alpha = 0;
		for (uint32_t i = 0; i < num_pixels; i++)
		{
			const uint32_t a = pPixels[i];
			if (a < min_alpha) min_alpha = a;
			if (a > max_alpha) max_alpha = a;
		}

		if (min_alpha == max_alpha)
		{
			results.m_base = min_alpha;
			results.m_table = 13;
			results.m_multiplier = 1;
			for (uint32_t i = 0; i < num_pixels; i++)
				results.m_selectors[i] = 4;
			return 0;
		}

		const uint32_t alpha_range = max_alpha - min_alpha;

		uint64_t best_err = UINT64_MAX;

		for (uint32_t table = 0; table < 16; table++)
		{
			if ((table_mask & (1U << table)) == 0)
				continue;

			const float range = (float)(g_etc2_eac_tables[table][ETC2_EAC_MAX_VALUE_SELECTOR] - g_etc2_eac_tables[table][ETC2_EAC_MIN_VALUE_SELECTOR]);
			const int center = (int)roundf(lerp((float)min_alpha, (float)max_alpha, (float)(0 - g_etc2_eac_tables[table][ETC2_EAC_MIN_VALUE_SELECTOR]) / range));

			const int base_min = clamp255(center - base_search_rad);
			const int base_max = clamp255(center + base_search_rad);

			const int mul = (int)roundf(alpha_range / range);
			const int mul_low = clamp<int>(mul - mul_search_rad, 1, 15);
			const int mul_high = clamp<int>(mul + mul_search_rad, 1, 15);

			for (int base = base_min; base <= base_max; base++)
			{
				for (int multiplier = mul_low; multiplier <= mul_high; multiplier++)
				{
					uint64_t total_err = 0;

					for (uint32_t i = 0; i < num_pixels; i++)
					{
						const int a = pPixels[i];

						uint32_t best_s_err = UINT32_MAX;
						uint32_t best_s = 0;
						for (uint32_t s = 0; s < 8; s++)
						{
							const int v = clamp255((int)multiplier * g_etc2_eac_tables[table][s] + (int)base);

							uint32_t err = iabs(a - v);
							if (err < best_s_err)
							{
								best_s_err = err;
								best_s = s;
							}
						}

						results.m_selectors_temp[i] = static_cast<uint8_t>(best_s);

						total_err += best_s_err * best_s_err;
						if (total_err >= best_err)
							break;
					}

					if (total_err < best_err)
					{
						best_err = total_err;
						results.m_base = base;
						results.m_multiplier = multiplier;
						results.m_table = table;
						results.m_selectors.swap(results.m_selectors_temp);
						if (!best_err)
							return best_err;
					}

				} // table

			} // multiplier

		} // base

		return best_err;
	}

	void pack_eac_a8(eac_a8_block* pBlock, const uint8_t* pPixels, uint32_t base_search_rad, uint32_t mul_search_rad, uint32_t table_mask)
	{
		pack_eac_a8_results results;
		pack_eac_a8(results, pPixels, 16, base_search_rad, mul_search_rad, table_mask);

		pBlock->m_base = results.m_base;
		pBlock->m_multiplier = results.m_multiplier;
		pBlock->m_table = results.m_table;
		for (uint32_t y = 0; y < 4; y++)
			for (uint32_t x = 0; x < 4; x++)
				pBlock->set_selector(x, y, results.m_selectors[x + y * 4]);
	}

} // namespace basisu
