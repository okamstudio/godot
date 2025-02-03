/**************************************************************************/
/*  keyboard.cpp                                                          */
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

#include "keyboard.h"

#include "core/os/os.h"

struct _KeyCodeText {
	int code;
	const char *text;
};

static const _KeyCodeText _keycodes[] = {

	/* clang-format off */
		{KEY_ESCAPE                        ,"Escape"},
		{KEY_TAB                           ,"Tab"},
		{KEY_BACKTAB                       ,"BackTab"},
		{KEY_BACKSPACE                     ,"BackSpace"},
		{KEY_ENTER                         ,"Enter"},
		{KEY_KP_ENTER                      ,"Kp Enter"},
		{KEY_INSERT                        ,"Insert"},
		{KEY_DELETE                        ,"Delete"},
		{KEY_PAUSE                         ,"Pause"},
		{KEY_PRINT                         ,"Print"},
		{KEY_SYSREQ                        ,"SysReq"},
		{KEY_CLEAR                         ,"Clear"},
		{KEY_HOME                          ,"Home"},
		{KEY_END                           ,"End"},
		{KEY_LEFT                          ,"Left"},
		{KEY_UP                            ,"Up"},
		{KEY_RIGHT                         ,"Right"},
		{KEY_DOWN                          ,"Down"},
		{KEY_PAGEUP                        ,"PageUp"},
		{KEY_PAGEDOWN                      ,"PageDown"},
		{KEY_SHIFT                         ,"Shift"},
		{KEY_CONTROL                       ,"Control"},
#ifdef OSX_ENABLED
		{KEY_META                          ,"Command"},
#else
		{KEY_META                          ,"Meta"},
#endif
		{KEY_ALT                           ,"Alt"},
		{KEY_CAPSLOCK                      ,"CapsLock"},
		{KEY_NUMLOCK                       ,"NumLock"},
		{KEY_SCROLLLOCK                    ,"ScrollLock"},
		{KEY_F1                            ,"F1"},
		{KEY_F2                            ,"F2"},
		{KEY_F3                            ,"F3"},
		{KEY_F4                            ,"F4"},
		{KEY_F5                            ,"F5"},
		{KEY_F6                            ,"F6"},
		{KEY_F7                            ,"F7"},
		{KEY_F8                            ,"F8"},
		{KEY_F9                            ,"F9"},
		{KEY_F10                           ,"F10"},
		{KEY_F11                           ,"F11"},
		{KEY_F12                           ,"F12"},
		{KEY_F13                           ,"F13"},
		{KEY_F14                           ,"F14"},
		{KEY_F15                           ,"F15"},
		{KEY_F16                           ,"F16"},
		{KEY_KP_MULTIPLY                   ,"Kp Multiply"},
		{KEY_KP_DIVIDE                     ,"Kp Divide"},
		{KEY_KP_SUBTRACT                   ,"Kp Subtract"},
		{KEY_KP_PERIOD                     ,"Kp Period"},
		{KEY_KP_ADD                        ,"Kp Add"},
		{KEY_KP_0                          ,"Kp 0"},
		{KEY_KP_1                          ,"Kp 1"},
		{KEY_KP_2                          ,"Kp 2"},
		{KEY_KP_3                          ,"Kp 3"},
		{KEY_KP_4                          ,"Kp 4"},
		{KEY_KP_5                          ,"Kp 5"},
		{KEY_KP_6                          ,"Kp 6"},
		{KEY_KP_7                          ,"Kp 7"},
		{KEY_KP_8                          ,"Kp 8"},
		{KEY_KP_9                          ,"Kp 9"},
		{KEY_SUPER_L                       ,"Super L"},
		{KEY_SUPER_R                       ,"Super R"},
		{KEY_MENU                          ,"Menu"},
		{KEY_HYPER_L                       ,"Hyper L"},
		{KEY_HYPER_R                       ,"Hyper R"},
		{KEY_HELP                          ,"Help"},
		{KEY_DIRECTION_L                   ,"Direction L"},
		{KEY_DIRECTION_R                   ,"Direction R"},
		{KEY_BACK                          ,"Back"},
		{KEY_FORWARD                       ,"Forward"},
		{KEY_STOP                          ,"Stop"},
		{KEY_REFRESH                       ,"Refresh"},
		{KEY_VOLUMEDOWN                    ,"VolumeDown"},
		{KEY_VOLUMEMUTE                    ,"VolumeMute"},
		{KEY_VOLUMEUP                      ,"VolumeUp"},
		{KEY_BASSBOOST                     ,"BassBoost"},
		{KEY_BASSUP                        ,"BassUp"},
		{KEY_BASSDOWN                      ,"BassDown"},
		{KEY_TREBLEUP                      ,"TrebleUp"},
		{KEY_TREBLEDOWN                    ,"TrebleDown"},
		{KEY_MEDIAPLAY                     ,"MediaPlay"},
		{KEY_MEDIASTOP                     ,"MediaStop"},
		{KEY_MEDIAPREVIOUS                 ,"MediaPrevious"},
		{KEY_MEDIANEXT                     ,"MediaNext"},
		{KEY_MEDIARECORD                   ,"MediaRecord"},
		{KEY_HOMEPAGE                      ,"HomePage"},
		{KEY_FAVORITES                     ,"Favorites"},
		{KEY_SEARCH                        ,"Search"},
		{KEY_STANDBY                       ,"StandBy"},
		{KEY_LAUNCHMAIL                    ,"LaunchMail"},
		{KEY_LAUNCHMEDIA                   ,"LaunchMedia"},
		{KEY_LAUNCH0                       ,"Launch0"},
		{KEY_LAUNCH1                       ,"Launch1"},
		{KEY_LAUNCH2                       ,"Launch2"},
		{KEY_LAUNCH3                       ,"Launch3"},
		{KEY_LAUNCH4                       ,"Launch4"},
		{KEY_LAUNCH5                       ,"Launch5"},
		{KEY_LAUNCH6                       ,"Launch6"},
		{KEY_LAUNCH7                       ,"Launch7"},
		{KEY_LAUNCH8                       ,"Launch8"},
		{KEY_LAUNCH9                       ,"Launch9"},
		{KEY_LAUNCHA                       ,"LaunchA"},
		{KEY_LAUNCHB                       ,"LaunchB"},
		{KEY_LAUNCHC                       ,"LaunchC"},
		{KEY_LAUNCHD                       ,"LaunchD"},
		{KEY_LAUNCHE                       ,"LaunchE"},
		{KEY_LAUNCHF                       ,"LaunchF"},

		{KEY_UNKNOWN                       ,"Unknown"},

		{KEY_SPACE                         ,"Space"},
		{KEY_EXCLAM                        ,"Exclam"},
		{KEY_QUOTEDBL                      ,"QuoteDbl"},
		{KEY_NUMBERSIGN                    ,"NumberSign"},
		{KEY_DOLLAR                        ,"Dollar"},
		{KEY_PERCENT                       ,"Percent"},
		{KEY_AMPERSAND                     ,"Ampersand"},
		{KEY_APOSTROPHE                    ,"Apostrophe"},
		{KEY_PARENLEFT                     ,"ParenLeft"},
		{KEY_PARENRIGHT                    ,"ParenRight"},
		{KEY_ASTERISK                      ,"Asterisk"},
		{KEY_PLUS                          ,"Plus"},
		{KEY_COMMA                         ,"Comma"},
		{KEY_MINUS                         ,"Minus"},
		{KEY_PERIOD                        ,"Period"},
		{KEY_SLASH                         ,"Slash"},
		{KEY_0                             ,"0"},
		{KEY_1                             ,"1"},
		{KEY_2                             ,"2"},
		{KEY_3                             ,"3"},
		{KEY_4                             ,"4"},
		{KEY_5                             ,"5"},
		{KEY_6                             ,"6"},
		{KEY_7                             ,"7"},
		{KEY_8                             ,"8"},
		{KEY_9                             ,"9"},
		{KEY_COLON                         ,"Colon"},
		{KEY_SEMICOLON                     ,"Semicolon"},
		{KEY_LESS                          ,"Less"},
		{KEY_EQUAL                         ,"Equal"},
		{KEY_GREATER                       ,"Greater"},
		{KEY_QUESTION                      ,"Question"},
		{KEY_AT                            ,"At"},
		{KEY_A                             ,"A"},
		{KEY_B                             ,"B"},
		{KEY_C                             ,"C"},
		{KEY_D                             ,"D"},
		{KEY_E                             ,"E"},
		{KEY_F                             ,"F"},
		{KEY_G                             ,"G"},
		{KEY_H                             ,"H"},
		{KEY_I                             ,"I"},
		{KEY_J                             ,"J"},
		{KEY_K                             ,"K"},
		{KEY_L                             ,"L"},
		{KEY_M                             ,"M"},
		{KEY_N                             ,"N"},
		{KEY_O                             ,"O"},
		{KEY_P                             ,"P"},
		{KEY_Q                             ,"Q"},
		{KEY_R                             ,"R"},
		{KEY_S                             ,"S"},
		{KEY_T                             ,"T"},
		{KEY_U                             ,"U"},
		{KEY_V                             ,"V"},
		{KEY_W                             ,"W"},
		{KEY_X                             ,"X"},
		{KEY_Y                             ,"Y"},
		{KEY_Z                             ,"Z"},
		{KEY_BRACKETLEFT                   ,"BracketLeft"},
		{KEY_BACKSLASH                     ,"BackSlash"},
		{KEY_BRACKETRIGHT                  ,"BracketRight"},
		{KEY_ASCIICIRCUM                   ,"AsciiCircum"},
		{KEY_UNDERSCORE                    ,"UnderScore"},
		{KEY_QUOTELEFT                     ,"QuoteLeft"},
		{KEY_BRACELEFT                     ,"BraceLeft"},
		{KEY_BAR                           ,"Bar"},
		{KEY_BRACERIGHT                    ,"BraceRight"},
		{KEY_ASCIITILDE                    ,"AsciiTilde"},
		{KEY_NOBREAKSPACE                  ,"NoBreakSpace"},
		{KEY_EXCLAMDOWN                    ,"ExclamDown"},
		{KEY_CENT                          ,"Cent"},
		{KEY_STERLING                      ,"Sterling"},
		{KEY_CURRENCY                      ,"Currency"},
		{KEY_YEN                           ,"Yen"},
		{KEY_BROKENBAR                     ,"BrokenBar"},
		{KEY_SECTION                       ,"Section"},
		{KEY_DIAERESIS                     ,"Diaeresis"},
		{KEY_COPYRIGHT                     ,"Copyright"},
		{KEY_ORDFEMININE                   ,"Ordfeminine"},
		{KEY_GUILLEMOTLEFT                 ,"GuillemotLeft"},
		{KEY_NOTSIGN                       ,"NotSign"},
		{KEY_HYPHEN                        ,"Hyphen"},
		{KEY_REGISTERED                    ,"Registered"},
		{KEY_MACRON                        ,"Macron"},
		{KEY_DEGREE                        ,"Degree"},
		{KEY_PLUSMINUS                     ,"PlusMinus"},
		{KEY_TWOSUPERIOR                   ,"TwoSuperior"},
		{KEY_THREESUPERIOR                 ,"ThreeSuperior"},
		{KEY_ACUTE                         ,"Acute"},
		{KEY_MU                            ,"Mu"},
		{KEY_PARAGRAPH                     ,"Paragraph"},
		{KEY_PERIODCENTERED                ,"PeriodCentered"},
		{KEY_CEDILLA                       ,"Cedilla"},
		{KEY_ONESUPERIOR                   ,"OneSuperior"},
		{KEY_MASCULINE                     ,"Masculine"},
		{KEY_GUILLEMOTRIGHT                ,"GuillemotRight"},
		{KEY_ONEQUARTER                    ,"OneQuarter"},
		{KEY_ONEHALF                       ,"OneHalf"},
		{KEY_THREEQUARTERS                 ,"ThreeQuarters"},
		{KEY_QUESTIONDOWN                  ,"QuestionDown"},
		{KEY_AGRAVE                        ,"Agrave"},
		{KEY_AACUTE                        ,"Aacute"},
		{KEY_ACIRCUMFLEX                   ,"AcircumFlex"},
		{KEY_ATILDE                        ,"Atilde"},
		{KEY_ADIAERESIS                    ,"Adiaeresis"},
		{KEY_ARING                         ,"Aring"},
		{KEY_AE                            ,"Ae"},
		{KEY_CCEDILLA                      ,"Ccedilla"},
		{KEY_EGRAVE                        ,"Egrave"},
		{KEY_EACUTE                        ,"Eacute"},
		{KEY_ECIRCUMFLEX                   ,"Ecircumflex"},
		{KEY_EDIAERESIS                    ,"Ediaeresis"},
		{KEY_IGRAVE                        ,"Igrave"},
		{KEY_IACUTE                        ,"Iacute"},
		{KEY_ICIRCUMFLEX                   ,"Icircumflex"},
		{KEY_IDIAERESIS                    ,"Idiaeresis"},
		{KEY_ETH                           ,"Eth"},
		{KEY_NTILDE                        ,"Ntilde"},
		{KEY_OGRAVE                        ,"Ograve"},
		{KEY_OACUTE                        ,"Oacute"},
		{KEY_OCIRCUMFLEX                   ,"Ocircumflex"},
		{KEY_OTILDE                        ,"Otilde"},
		{KEY_ODIAERESIS                    ,"Odiaeresis"},
		{KEY_MULTIPLY                      ,"Multiply"},
		{KEY_OOBLIQUE                      ,"Ooblique"},
		{KEY_UGRAVE                        ,"Ugrave"},
		{KEY_UACUTE                        ,"Uacute"},
		{KEY_UCIRCUMFLEX                   ,"Ucircumflex"},
		{KEY_UDIAERESIS                    ,"Udiaeresis"},
		{KEY_YACUTE                        ,"Yacute"},
		{KEY_THORN                         ,"Thorn"},
		{KEY_SSHARP                        ,"Ssharp"},

		{KEY_DIVISION                      ,"Division"},
		{KEY_YDIAERESIS                    ,"Ydiaeresis"},
		{0                                 ,nullptr}
	/* clang-format on */
};

bool keycode_has_unicode(uint32_t p_keycode) {
	switch (p_keycode) {
		case KEY_ESCAPE:
		case KEY_TAB:
		case KEY_BACKTAB:
		case KEY_BACKSPACE:
		case KEY_ENTER:
		case KEY_KP_ENTER:
		case KEY_INSERT:
		case KEY_DELETE:
		case KEY_PAUSE:
		case KEY_PRINT:
		case KEY_SYSREQ:
		case KEY_CLEAR:
		case KEY_HOME:
		case KEY_END:
		case KEY_LEFT:
		case KEY_UP:
		case KEY_RIGHT:
		case KEY_DOWN:
		case KEY_PAGEUP:
		case KEY_PAGEDOWN:
		case KEY_SHIFT:
		case KEY_CONTROL:
		case KEY_META:
		case KEY_ALT:
		case KEY_CAPSLOCK:
		case KEY_NUMLOCK:
		case KEY_SCROLLLOCK:
		case KEY_F1:
		case KEY_F2:
		case KEY_F3:
		case KEY_F4:
		case KEY_F5:
		case KEY_F6:
		case KEY_F7:
		case KEY_F8:
		case KEY_F9:
		case KEY_F10:
		case KEY_F11:
		case KEY_F12:
		case KEY_F13:
		case KEY_F14:
		case KEY_F15:
		case KEY_F16:
		case KEY_SUPER_L:
		case KEY_SUPER_R:
		case KEY_MENU:
		case KEY_HYPER_L:
		case KEY_HYPER_R:
		case KEY_HELP:
		case KEY_DIRECTION_L:
		case KEY_DIRECTION_R:
		case KEY_BACK:
		case KEY_FORWARD:
		case KEY_STOP:
		case KEY_REFRESH:
		case KEY_VOLUMEDOWN:
		case KEY_VOLUMEMUTE:
		case KEY_VOLUMEUP:
		case KEY_BASSBOOST:
		case KEY_BASSUP:
		case KEY_BASSDOWN:
		case KEY_TREBLEUP:
		case KEY_TREBLEDOWN:
		case KEY_MEDIAPLAY:
		case KEY_MEDIASTOP:
		case KEY_MEDIAPREVIOUS:
		case KEY_MEDIANEXT:
		case KEY_MEDIARECORD:
		case KEY_HOMEPAGE:
		case KEY_FAVORITES:
		case KEY_SEARCH:
		case KEY_STANDBY:
		case KEY_OPENURL:
		case KEY_LAUNCHMAIL:
		case KEY_LAUNCHMEDIA:
		case KEY_LAUNCH0:
		case KEY_LAUNCH1:
		case KEY_LAUNCH2:
		case KEY_LAUNCH3:
		case KEY_LAUNCH4:
		case KEY_LAUNCH5:
		case KEY_LAUNCH6:
		case KEY_LAUNCH7:
		case KEY_LAUNCH8:
		case KEY_LAUNCH9:
		case KEY_LAUNCHA:
		case KEY_LAUNCHB:
		case KEY_LAUNCHC:
		case KEY_LAUNCHD:
		case KEY_LAUNCHE:
		case KEY_LAUNCHF:
			return false;
	}

	return true;
}

String keycode_get_string(uint32_t p_code) {
	String codestr;
	if (p_code & KEY_MASK_SHIFT) {
		codestr += find_keycode_name(KEY_SHIFT);
		codestr += "+";
	}
	if (p_code & KEY_MASK_ALT) {
		codestr += find_keycode_name(KEY_ALT);
		codestr += "+";
	}
	if (p_code & KEY_MASK_CTRL) {
		codestr += find_keycode_name(KEY_CONTROL);
		codestr += "+";
	}
	if (p_code & KEY_MASK_META) {
		codestr += find_keycode_name(KEY_META);
		codestr += "+";
	}

	p_code &= KEY_CODE_MASK;

	const _KeyCodeText *kct = &_keycodes[0];

	while (kct->text) {
		if (kct->code == (int)p_code) {
			codestr += kct->text;
			return codestr;
		}
		kct++;
	}

	codestr += String::chr(p_code);

	return codestr;
}

int find_keycode(const String &p_codestr) {
	Vector<String> code_parts = p_codestr.split("+");
	if (code_parts.size() < 1) {
		return 0;
	}

	int code = 0;
	String last_part = code_parts[code_parts.size() - 1];
	const _KeyCodeText *kct = &_keycodes[0];

	while (kct->text) {
		if (last_part.nocasecmp_to(kct->text) == 0) {
			code = kct->code;
			break;
		}
		kct++;
	}

	for (int part = 0; part < code_parts.size() - 1; part++) {
		String code_part = code_parts[part];
		if (code_part.nocasecmp_to(find_keycode_name(KEY_SHIFT)) == 0) {
			code += KEY_MASK_SHIFT;
		} else if (code_part.nocasecmp_to(find_keycode_name(KEY_ALT)) == 0) {
			code += KEY_MASK_ALT;
		} else if (code_part.nocasecmp_to(find_keycode_name(KEY_CONTROL)) == 0) {
			code += KEY_MASK_CTRL;
		} else if (code_part.nocasecmp_to(find_keycode_name(KEY_META)) == 0) {
			code += KEY_MASK_META;
		}
	}

	return code;
}

const char *find_keycode_name(int p_keycode) {
	const _KeyCodeText *kct = &_keycodes[0];

	while (kct->text) {
		if (kct->code == p_keycode) {
			return kct->text;
		}
		kct++;
	}

	return "";
}

int keycode_get_count() {
	const _KeyCodeText *kct = &_keycodes[0];

	int count = 0;
	while (kct->text) {
		count++;
		kct++;
	}
	return count;
}

int keycode_get_value_by_index(int p_index) {
	return _keycodes[p_index].code;
}

const char *keycode_get_name_by_index(int p_index) {
	return _keycodes[p_index].text;
}
