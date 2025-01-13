/**************************************************************************/
/*  pass_func.h                                                           */
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

#ifndef PASS_FUNC_H
#define PASS_FUNC_H

#define PASS0R(m_r, m_name)        \
	m_r m_name() {                 \
		return PASSBASE->m_name(); \
	}
#define PASS0RC(m_r, m_name)       \
	m_r m_name() const {           \
		return PASSBASE->m_name(); \
	}
#define PASS1R(m_r, m_name, m_type1)   \
	m_r m_name(m_type1 arg1) {         \
		return PASSBASE->m_name(arg1); \
	}
#define PASS1RC(m_r, m_name, m_type1)  \
	m_r m_name(m_type1 arg1) const {   \
		return PASSBASE->m_name(arg1); \
	}
#define PASS2R(m_r, m_name, m_type1, m_type2) \
	m_r m_name(m_type1 arg1, m_type2 arg2) {  \
		return PASSBASE->m_name(arg1, arg2);  \
	}
#define PASS2RC(m_r, m_name, m_type1, m_type2)     \
	m_r m_name(m_type1 arg1, m_type2 arg2) const { \
		return PASSBASE->m_name(arg1, arg2);       \
	}
#define PASS3R(m_r, m_name, m_type1, m_type2, m_type3)     \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3) { \
		return PASSBASE->m_name(arg1, arg2, arg3);         \
	}
#define PASS3RC(m_r, m_name, m_type1, m_type2, m_type3)          \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3) const { \
		return PASSBASE->m_name(arg1, arg2, arg3);               \
	}
#define PASS4R(m_r, m_name, m_type1, m_type2, m_type3, m_type4)          \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4) { \
		return PASSBASE->m_name(arg1, arg2, arg3, arg4);                 \
	}
#define PASS4RC(m_r, m_name, m_type1, m_type2, m_type3, m_type4)               \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4) const { \
		return PASSBASE->m_name(arg1, arg2, arg3, arg4);                       \
	}
#define PASS5R(m_r, m_name, m_type1, m_type2, m_type3, m_type4, m_type5)               \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5) { \
		return PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5);                         \
	}
#define PASS5RC(m_r, m_name, m_type1, m_type2, m_type3, m_type4, m_type5)                    \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5) const { \
		return PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5);                               \
	}
#define PASS6R(m_r, m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6)                    \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6) { \
		return PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6);                                 \
	}
#define PASS6RC(m_r, m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6)                         \
	m_r m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6) const { \
		return PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6);                                       \
	}

#define PASS0(m_name)       \
	void m_name() {         \
		PASSBASE->m_name(); \
	}
#define PASS1(m_name, m_type1)  \
	void m_name(m_type1 arg1) { \
		PASSBASE->m_name(arg1); \
	}
#define PASS1C(m_name, m_type1)       \
	void m_name(m_type1 arg1) const { \
		PASSBASE->m_name(arg1);       \
	}
#define PASS2(m_name, m_type1, m_type2)       \
	void m_name(m_type1 arg1, m_type2 arg2) { \
		PASSBASE->m_name(arg1, arg2);         \
	}
#define PASS2C(m_name, m_type1, m_type2)            \
	void m_name(m_type1 arg1, m_type2 arg2) const { \
		PASSBASE->m_name(arg1, arg2);               \
	}
#define PASS3(m_name, m_type1, m_type2, m_type3)            \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3) { \
		PASSBASE->m_name(arg1, arg2, arg3);                 \
	}
#define PASS4(m_name, m_type1, m_type2, m_type3, m_type4)                 \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4);                         \
	}
#define PASS5(m_name, m_type1, m_type2, m_type3, m_type4, m_type5)                      \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5);                                 \
	}
#define PASS6(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6)                           \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6);                                         \
	}
#define PASS7(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7)                                \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7);                                                 \
	}
#define PASS8(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8)                                     \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);                                                         \
	}
#define PASS9(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9)                                          \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);                                                                 \
	}
#define PASS10(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10)                                               \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);                                                                          \
	}
#define PASS11(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11)                                                     \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);                                                                                   \
	}
#define PASS12(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12)                                                           \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12);                                                                                            \
	}
#define PASS13(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12, m_type13)                                                                 \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12, m_type13 arg13) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13);                                                                                                     \
	}
#define PASS14(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12, m_type13, m_type14)                                                                       \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12, m_type13 arg13, m_type14 arg14) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14);                                                                                                              \
	}
#define PASS15(m_name, m_type1, m_type2, m_type3, m_type4, m_type5, m_type6, m_type7, m_type8, m_type9, m_type10, m_type11, m_type12, m_type13, m_type14, m_type15)                                                                             \
	void m_name(m_type1 arg1, m_type2 arg2, m_type3 arg3, m_type4 arg4, m_type5 arg5, m_type6 arg6, m_type7 arg7, m_type8 arg8, m_type9 arg9, m_type10 arg10, m_type11 arg11, m_type12 arg12, m_type13 arg13, m_type14 arg14, m_type15 arg15) { \
		PASSBASE->m_name(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15);                                                                                                                       \
	}

#endif // PASS_FUNC_H
