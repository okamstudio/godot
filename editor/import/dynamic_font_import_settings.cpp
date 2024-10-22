/**************************************************************************/
/*  dynamic_font_import_settings.cpp                                      */
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

#include "dynamic_font_import_settings.h"

#include "core/config/project_settings.h"
#include "core/string/translation_server.h"
#include "editor/editor_file_system.h"
#include "editor/editor_inspector.h"
#include "editor/editor_locale_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_property_name_processor.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/themes/editor_scale.h"

/*************************************************************************/
/* Settings data                                                         */
/*************************************************************************/

bool DynamicFontImportSettingsData::_set(const StringName &p_name, const Variant &p_value) {
	if (defaults.has(p_name) && defaults[p_name] == p_value) {
		settings.erase(p_name);
	} else {
		settings[p_name] = p_value;
	}
	return true;
}

bool DynamicFontImportSettingsData::_get(const StringName &p_name, Variant &r_ret) const {
	if (settings.has(p_name)) {
		r_ret = settings[p_name];
		return true;
	}
	if (defaults.has(p_name)) {
		r_ret = defaults[p_name];
		return true;
	}
	return false;
}

void DynamicFontImportSettingsData::_get_property_list(List<PropertyInfo> *p_list) const {
	for (const List<ResourceImporter::ImportOption>::Element *E = options.front(); E; E = E->next()) {
		if (owner && owner->import_settings_data.is_valid()) {
			if (owner->import_settings_data->get("multichannel_signed_distance_field") && (E->get().option.name == "size" || E->get().option.name == "outline_size" || E->get().option.name == "oversampling")) {
				continue;
			}
			if (!owner->import_settings_data->get("multichannel_signed_distance_field") && (E->get().option.name == "msdf_pixel_range" || E->get().option.name == "msdf_size")) {
				continue;
			}
		}
		p_list->push_back(E->get().option);
	}
}

Ref<FontFile> DynamicFontImportSettingsData::get_font() const {
	return fd;
}

/*************************************************************************/
/* Glyph ranges                                                          */
/*************************************************************************/

struct UniRange {
	int32_t start;
	int32_t end;
	String name;
};

// Unicode Character Blocks
// Source: https://www.unicode.org/Public/16.0.0/ucd/Blocks.txt
static UniRange unicode_ranges[] = {
	{ 0x0000, 0x007F, U"Basic Latin" },
	{ 0x0080, 0x00FF, U"Latin-1 Supplement" },
	{ 0x0100, 0x017F, U"Latin Extended-A" },
	{ 0x0180, 0x024F, U"Latin Extended-B" },
	{ 0x0250, 0x02AF, U"IPA Extensions" },
	{ 0x02B0, 0x02FF, U"Spacing Modifier Letters" },
	{ 0x0300, 0x036F, U"Combining Diacritical Marks" },
	{ 0x0370, 0x03FF, U"Greek and Coptic" },
	{ 0x0400, 0x04FF, U"Cyrillic" },
	{ 0x0500, 0x052F, U"Cyrillic Supplement" },
	{ 0x0530, 0x058F, U"Armenian" },
	{ 0x0590, 0x05FF, U"Hebrew" },
	{ 0x0600, 0x06FF, U"Arabic" },
	{ 0x0700, 0x074F, U"Syriac" },
	{ 0x0750, 0x077F, U"Arabic Supplement" },
	{ 0x0780, 0x07BF, U"Thaana" },
	{ 0x07C0, 0x07FF, U"NKo" },
	{ 0x0800, 0x083F, U"Samaritan" },
	{ 0x0840, 0x085F, U"Mandaic" },
	{ 0x0860, 0x086F, U"Syriac Supplement" },
	{ 0x0870, 0x089F, U"Arabic Extended-B" },
	{ 0x08A0, 0x08FF, U"Arabic Extended-A" },
	{ 0x0900, 0x097F, U"Devanagari" },
	{ 0x0980, 0x09FF, U"Bengali" },
	{ 0x0A00, 0x0A7F, U"Gurmukhi" },
	{ 0x0A80, 0x0AFF, U"Gujarati" },
	{ 0x0B00, 0x0B7F, U"Oriya" },
	{ 0x0B80, 0x0BFF, U"Tamil" },
	{ 0x0C00, 0x0C7F, U"Telugu" },
	{ 0x0C80, 0x0CFF, U"Kannada" },
	{ 0x0D00, 0x0D7F, U"Malayalam" },
	{ 0x0D80, 0x0DFF, U"Sinhala" },
	{ 0x0E00, 0x0E7F, U"Thai" },
	{ 0x0E80, 0x0EFF, U"Lao" },
	{ 0x0F00, 0x0FFF, U"Tibetan" },
	{ 0x1000, 0x109F, U"Myanmar" },
	{ 0x10A0, 0x10FF, U"Georgian" },
	{ 0x1100, 0x11FF, U"Hangul Jamo" },
	{ 0x1200, 0x137F, U"Ethiopic" },
	{ 0x1380, 0x139F, U"Ethiopic Supplement" },
	{ 0x13A0, 0x13FF, U"Cherokee" },
	{ 0x1400, 0x167F, U"Unified Canadian Aboriginal Syllabics" },
	{ 0x1680, 0x169F, U"Ogham" },
	{ 0x16A0, 0x16FF, U"Runic" },
	{ 0x1700, 0x171F, U"Tagalog" },
	{ 0x1720, 0x173F, U"Hanunoo" },
	{ 0x1740, 0x175F, U"Buhid" },
	{ 0x1760, 0x177F, U"Tagbanwa" },
	{ 0x1780, 0x17FF, U"Khmer" },
	{ 0x1800, 0x18AF, U"Mongolian" },
	{ 0x18B0, 0x18FF, U"Unified Canadian Aboriginal Syllabics Extended" },
	{ 0x1900, 0x194F, U"Limbu" },
	{ 0x1950, 0x197F, U"Tai Le" },
	{ 0x1980, 0x19DF, U"New Tai Lue" },
	{ 0x19E0, 0x19FF, U"Khmer Symbols" },
	{ 0x1A00, 0x1A1F, U"Buginese" },
	{ 0x1A20, 0x1AAF, U"Tai Tham" },
	{ 0x1AB0, 0x1AFF, U"Combining Diacritical Marks Extended" },
	{ 0x1B00, 0x1B7F, U"Balinese" },
	{ 0x1B80, 0x1BBF, U"Sundanese" },
	{ 0x1BC0, 0x1BFF, U"Batak" },
	{ 0x1C00, 0x1C4F, U"Lepcha" },
	{ 0x1C50, 0x1C7F, U"Ol Chiki" },
	{ 0x1C80, 0x1C8F, U"Cyrillic Extended-C" },
	{ 0x1C90, 0x1CBF, U"Georgian Extended" },
	{ 0x1CC0, 0x1CCF, U"Sundanese Supplement" },
	{ 0x1CD0, 0x1CFF, U"Vedic Extensions" },
	{ 0x1D00, 0x1D7F, U"Phonetic Extensions" },
	{ 0x1D80, 0x1DBF, U"Phonetic Extensions Supplement" },
	{ 0x1DC0, 0x1DFF, U"Combining Diacritical Marks Supplement" },
	{ 0x1E00, 0x1EFF, U"Latin Extended Additional" },
	{ 0x1F00, 0x1FFF, U"Greek Extended" },
	{ 0x2000, 0x206F, U"General Punctuation" },
	{ 0x2070, 0x209F, U"Superscripts and Subscripts" },
	{ 0x20A0, 0x20CF, U"Currency Symbols" },
	{ 0x20D0, 0x20FF, U"Combining Diacritical Marks for Symbols" },
	{ 0x2100, 0x214F, U"Letterlike Symbols" },
	{ 0x2150, 0x218F, U"Number Forms" },
	{ 0x2190, 0x21FF, U"Arrows" },
	{ 0x2200, 0x22FF, U"Mathematical Operators" },
	{ 0x2300, 0x23FF, U"Miscellaneous Technical" },
	{ 0x2400, 0x243F, U"Control Pictures" },
	{ 0x2440, 0x245F, U"Optical Character Recognition" },
	{ 0x2460, 0x24FF, U"Enclosed Alphanumerics" },
	{ 0x2500, 0x257F, U"Box Drawing" },
	{ 0x2580, 0x259F, U"Block Elements" },
	{ 0x25A0, 0x25FF, U"Geometric Shapes" },
	{ 0x2600, 0x26FF, U"Miscellaneous Symbols" },
	{ 0x2700, 0x27BF, U"Dingbats" },
	{ 0x27C0, 0x27EF, U"Miscellaneous Mathematical Symbols-A" },
	{ 0x27F0, 0x27FF, U"Supplemental Arrows-A" },
	{ 0x2800, 0x28FF, U"Braille Patterns" },
	{ 0x2900, 0x297F, U"Supplemental Arrows-B" },
	{ 0x2980, 0x29FF, U"Miscellaneous Mathematical Symbols-B" },
	{ 0x2A00, 0x2AFF, U"Supplemental Mathematical Operators" },
	{ 0x2B00, 0x2BFF, U"Miscellaneous Symbols and Arrows" },
	{ 0x2C00, 0x2C5F, U"Glagolitic" },
	{ 0x2C60, 0x2C7F, U"Latin Extended-C" },
	{ 0x2C80, 0x2CFF, U"Coptic" },
	{ 0x2D00, 0x2D2F, U"Georgian Supplement" },
	{ 0x2D30, 0x2D7F, U"Tifinagh" },
	{ 0x2D80, 0x2DDF, U"Ethiopic Extended" },
	{ 0x2DE0, 0x2DFF, U"Cyrillic Extended-A" },
	{ 0x2E00, 0x2E7F, U"Supplemental Punctuation" },
	{ 0x2E80, 0x2EFF, U"CJK Radicals Supplement" },
	{ 0x2F00, 0x2FDF, U"Kangxi Radicals" },
	{ 0x2FF0, 0x2FFF, U"Ideographic Description Characters" },
	{ 0x3000, 0x303F, U"CJK Symbols and Punctuation" },
	{ 0x3040, 0x309F, U"Hiragana" },
	{ 0x30A0, 0x30FF, U"Katakana" },
	{ 0x3100, 0x312F, U"Bopomofo" },
	{ 0x3130, 0x318F, U"Hangul Compatibility Jamo" },
	{ 0x3190, 0x319F, U"Kanbun" },
	{ 0x31A0, 0x31BF, U"Bopomofo Extended" },
	{ 0x31C0, 0x31EF, U"CJK Strokes" },
	{ 0x31F0, 0x31FF, U"Katakana Phonetic Extensions" },
	{ 0x3200, 0x32FF, U"Enclosed CJK Letters and Months" },
	{ 0x3300, 0x33FF, U"CJK Compatibility" },
	{ 0x3400, 0x4DBF, U"CJK Unified Ideographs Extension A" },
	{ 0x4DC0, 0x4DFF, U"Yijing Hexagram Symbols" },
	{ 0x4E00, 0x9FFF, U"CJK Unified Ideographs" },
	{ 0xA000, 0xA48F, U"Yi Syllables" },
	{ 0xA490, 0xA4CF, U"Yi Radicals" },
	{ 0xA4D0, 0xA4FF, U"Lisu" },
	{ 0xA500, 0xA63F, U"Vai" },
	{ 0xA640, 0xA69F, U"Cyrillic Extended-B" },
	{ 0xA6A0, 0xA6FF, U"Bamum" },
	{ 0xA700, 0xA71F, U"Modifier Tone Letters" },
	{ 0xA720, 0xA7FF, U"Latin Extended-D" },
	{ 0xA800, 0xA82F, U"Syloti Nagri" },
	{ 0xA830, 0xA83F, U"Common Indic Number Forms" },
	{ 0xA840, 0xA87F, U"Phags-pa" },
	{ 0xA880, 0xA8DF, U"Saurashtra" },
	{ 0xA8E0, 0xA8FF, U"Devanagari Extended" },
	{ 0xA900, 0xA92F, U"Kayah Li" },
	{ 0xA930, 0xA95F, U"Rejang" },
	{ 0xA960, 0xA97F, U"Hangul Jamo Extended-A" },
	{ 0xA980, 0xA9DF, U"Javanese" },
	{ 0xA9E0, 0xA9FF, U"Myanmar Extended-B" },
	{ 0xAA00, 0xAA5F, U"Cham" },
	{ 0xAA60, 0xAA7F, U"Myanmar Extended-A" },
	{ 0xAA80, 0xAADF, U"Tai Viet" },
	{ 0xAAE0, 0xAAFF, U"Meetei Mayek Extensions" },
	{ 0xAB00, 0xAB2F, U"Ethiopic Extended-A" },
	{ 0xAB30, 0xAB6F, U"Latin Extended-E" },
	{ 0xAB70, 0xABBF, U"Cherokee Supplement" },
	{ 0xABC0, 0xABFF, U"Meetei Mayek" },
	{ 0xAC00, 0xD7AF, U"Hangul Syllables" },
	{ 0xD7B0, 0xD7FF, U"Hangul Jamo Extended-B" },
	//{ 0xD800, 0xDB7F, U"High Surrogates" },
	//{ 0xDB80, 0xDBFF, U"High Private Use Surrogates" },
	//{ 0xDC00, 0xDFFF, U"Low Surrogates" },
	{ 0xE000, 0xF8FF, U"Private Use Area" },
	{ 0xF900, 0xFAFF, U"CJK Compatibility Ideographs" },
	{ 0xFB00, 0xFB4F, U"Alphabetic Presentation Forms" },
	{ 0xFB50, 0xFDFF, U"Arabic Presentation Forms-A" },
	//{ 0xFE00, 0xFE0F, U"Variation Selectors" },
	{ 0xFE10, 0xFE1F, U"Vertical Forms" },
	{ 0xFE20, 0xFE2F, U"Combining Half Marks" },
	{ 0xFE30, 0xFE4F, U"CJK Compatibility Forms" },
	{ 0xFE50, 0xFE6F, U"Small Form Variants" },
	{ 0xFE70, 0xFEFF, U"Arabic Presentation Forms-B" },
	{ 0xFF00, 0xFFEF, U"Halfwidth and Fullwidth Forms" },
	//{ 0xFFF0, 0xFFFF, U"Specials" },
	{ 0x10000, 0x1007F, U"Linear B Syllabary" },
	{ 0x10080, 0x100FF, U"Linear B Ideograms" },
	{ 0x10100, 0x1013F, U"Aegean Numbers" },
	{ 0x10140, 0x1018F, U"Ancient Greek Numbers" },
	{ 0x10190, 0x101CF, U"Ancient Symbols" },
	{ 0x101D0, 0x101FF, U"Phaistos Disc" },
	{ 0x10280, 0x1029F, U"Lycian" },
	{ 0x102A0, 0x102DF, U"Carian" },
	{ 0x102E0, 0x102FF, U"Coptic Epact Numbers" },
	{ 0x10300, 0x1032F, U"Old Italic" },
	{ 0x10330, 0x1034F, U"Gothic" },
	{ 0x10350, 0x1037F, U"Old Permic" },
	{ 0x10380, 0x1039F, U"Ugaritic" },
	{ 0x103A0, 0x103DF, U"Old Persian" },
	{ 0x10400, 0x1044F, U"Deseret" },
	{ 0x10450, 0x1047F, U"Shavian" },
	{ 0x10480, 0x104AF, U"Osmanya" },
	{ 0x104B0, 0x104FF, U"Osage" },
	{ 0x10500, 0x1052F, U"Elbasan" },
	{ 0x10530, 0x1056F, U"Caucasian Albanian" },
	{ 0x10570, 0x105BF, U"Vithkuqi" },
	{ 0x105C0, 0x105FF, U"Todhri" },
	{ 0x10600, 0x1077F, U"Linear A" },
	{ 0x10780, 0x107BF, U"Latin Extended-F" },
	{ 0x10800, 0x1083F, U"Cypriot Syllabary" },
	{ 0x10840, 0x1085F, U"Imperial Aramaic" },
	{ 0x10860, 0x1087F, U"Palmyrene" },
	{ 0x10880, 0x108AF, U"Nabataean" },
	{ 0x108E0, 0x108FF, U"Hatran" },
	{ 0x10900, 0x1091F, U"Phoenician" },
	{ 0x10920, 0x1093F, U"Lydian" },
	{ 0x10980, 0x1099F, U"Meroitic Hieroglyphs" },
	{ 0x109A0, 0x109FF, U"Meroitic Cursive" },
	{ 0x10A00, 0x10A5F, U"Kharoshthi" },
	{ 0x10A60, 0x10A7F, U"Old South Arabian" },
	{ 0x10A80, 0x10A9F, U"Old North Arabian" },
	{ 0x10AC0, 0x10AFF, U"Manichaean" },
	{ 0x10B00, 0x10B3F, U"Avestan" },
	{ 0x10B40, 0x10B5F, U"Inscriptional Parthian" },
	{ 0x10B60, 0x10B7F, U"Inscriptional Pahlavi" },
	{ 0x10B80, 0x10BAF, U"Psalter Pahlavi" },
	{ 0x10C00, 0x10C4F, U"Old Turkic" },
	{ 0x10C80, 0x10CFF, U"Old Hungarian" },
	{ 0x10D00, 0x10D3F, U"Hanifi Rohingya" },
	{ 0x10D40, 0x10D8F, U"Garay" },
	{ 0x10E60, 0x10E7F, U"Rumi Numeral Symbols" },
	{ 0x10E80, 0x10EBF, U"Yezidi" },
	{ 0x10EC0, 0x10EFF, U"Arabic Extended-C" },
	{ 0x10F00, 0x10F2F, U"Old Sogdian" },
	{ 0x10F30, 0x10F6F, U"Sogdian" },
	{ 0x10F70, 0x10FAF, U"Old Uyghur" },
	{ 0x10FB0, 0x10FDF, U"Chorasmian" },
	{ 0x10FE0, 0x10FFF, U"Elymaic" },
	{ 0x11000, 0x1107F, U"Brahmi" },
	{ 0x11080, 0x110CF, U"Kaithi" },
	{ 0x110D0, 0x110FF, U"Sora Sompeng" },
	{ 0x11100, 0x1114F, U"Chakma" },
	{ 0x11150, 0x1117F, U"Mahajani" },
	{ 0x11180, 0x111DF, U"Sharada" },
	{ 0x111E0, 0x111FF, U"Sinhala Archaic Numbers" },
	{ 0x11200, 0x1124F, U"Khojki" },
	{ 0x11280, 0x112AF, U"Multani" },
	{ 0x112B0, 0x112FF, U"Khudawadi" },
	{ 0x11300, 0x1137F, U"Grantha" },
	{ 0x11380, 0x113FF, U"Tulu-Tigalari" },
	{ 0x11400, 0x1147F, U"Newa" },
	{ 0x11480, 0x114DF, U"Tirhuta" },
	{ 0x11580, 0x115FF, U"Siddham" },
	{ 0x11600, 0x1165F, U"Modi" },
	{ 0x11660, 0x1167F, U"Mongolian Supplement" },
	{ 0x11680, 0x116CF, U"Takri" },
	{ 0x116D0, 0x116FF, U"Myanmar Extended-C" },
	{ 0x11700, 0x1174F, U"Ahom" },
	{ 0x11800, 0x1184F, U"Dogra" },
	{ 0x118A0, 0x118FF, U"Warang Citi" },
	{ 0x11900, 0x1195F, U"Dives Akuru" },
	{ 0x119A0, 0x119FF, U"Nandinagari" },
	{ 0x11A00, 0x11A4F, U"Zanabazar Square" },
	{ 0x11A50, 0x11AAF, U"Soyombo" },
	{ 0x11AB0, 0x11ABF, U"Unified Canadian Aboriginal Syllabics Extended-A" },
	{ 0x11AC0, 0x11AFF, U"Pau Cin Hau" },
	{ 0x11B00, 0x11B5F, U"Devanagari Extended-A" },
	{ 0x11BC0, 0x11BFF, U"Sunuwar" },
	{ 0x11C00, 0x11C6F, U"Bhaiksuki" },
	{ 0x11C70, 0x11CBF, U"Marchen" },
	{ 0x11D00, 0x11D5F, U"Masaram Gondi" },
	{ 0x11D60, 0x11DAF, U"Gunjala Gondi" },
	{ 0x11EE0, 0x11EFF, U"Makasar" },
	{ 0x11F00, 0x11F5F, U"Kawi" },
	{ 0x11FB0, 0x11FBF, U"Lisu Supplement" },
	{ 0x11FC0, 0x11FFF, U"Tamil Supplement" },
	{ 0x12000, 0x123FF, U"Cuneiform" },
	{ 0x12400, 0x1247F, U"Cuneiform Numbers and Punctuation" },
	{ 0x12480, 0x1254F, U"Early Dynastic Cuneiform" },
	{ 0x12F90, 0x12FFF, U"Cypro-Minoan" },
	{ 0x13000, 0x1342F, U"Egyptian Hieroglyphs" },
	{ 0x13430, 0x1343F, U"Egyptian Hieroglyph Format Controls" },
	{ 0x13460, 0x143FF, U"Egyptian Hieroglyphs Extended-A" },
	{ 0x14400, 0x1467F, U"Anatolian Hieroglyphs" },
	{ 0x16100, 0x1613F, U"Gurung Khema" },
	{ 0x16800, 0x16A3F, U"Bamum Supplement" },
	{ 0x16A40, 0x16A6F, U"Mro" },
	{ 0x16A70, 0x16ACF, U"Tangsa" },
	{ 0x16AD0, 0x16AFF, U"Bassa Vah" },
	{ 0x16B00, 0x16B8F, U"Pahawh Hmong" },
	{ 0x16D40, 0x16D7F, U"Kirat Rai" },
	{ 0x16E40, 0x16E9F, U"Medefaidrin" },
	{ 0x16F00, 0x16F9F, U"Miao" },
	{ 0x16FE0, 0x16FFF, U"Ideographic Symbols and Punctuation" },
	{ 0x17000, 0x187FF, U"Tangut" },
	{ 0x18800, 0x18AFF, U"Tangut Components" },
	{ 0x18B00, 0x18CFF, U"Khitan Small Script" },
	{ 0x18D00, 0x18D7F, U"Tangut Supplement" },
	{ 0x1AFF0, 0x1AFFF, U"Kana Extended-B" },
	{ 0x1B000, 0x1B0FF, U"Kana Supplement" },
	{ 0x1B100, 0x1B12F, U"Kana Extended-A" },
	{ 0x1B130, 0x1B16F, U"Small Kana Extension" },
	{ 0x1B170, 0x1B2FF, U"Nushu" },
	{ 0x1BC00, 0x1BC9F, U"Duployan" },
	{ 0x1BCA0, 0x1BCAF, U"Shorthand Format Controls" },
	{ 0x1CC00, 0x1CEBF, U"Symbols for Legacy Computing Supplement" },
	{ 0x1CF00, 0x1CFCF, U"Znamenny Musical Notation" },
	{ 0x1D000, 0x1D0FF, U"Byzantine Musical Symbols" },
	{ 0x1D100, 0x1D1FF, U"Musical Symbols" },
	{ 0x1D200, 0x1D24F, U"Ancient Greek Musical Notation" },
	{ 0x1D2C0, 0x1D2DF, U"Kaktovik Numerals" },
	{ 0x1D2E0, 0x1D2FF, U"Mayan Numerals" },
	{ 0x1D300, 0x1D35F, U"Tai Xuan Jing Symbols" },
	{ 0x1D360, 0x1D37F, U"Counting Rod Numerals" },
	{ 0x1D400, 0x1D7FF, U"Mathematical Alphanumeric Symbols" },
	{ 0x1D800, 0x1DAAF, U"Sutton SignWriting" },
	{ 0x1DF00, 0x1DFFF, U"Latin Extended-G" },
	{ 0x1E000, 0x1E02F, U"Glagolitic Supplement" },
	{ 0x1E030, 0x1E08F, U"Cyrillic Extended-D" },
	{ 0x1E100, 0x1E14F, U"Nyiakeng Puachue Hmong" },
	{ 0x1E290, 0x1E2BF, U"Toto" },
	{ 0x1E2C0, 0x1E2FF, U"Wancho" },
	{ 0x1E4D0, 0x1E4FF, U"Nag Mundari" },
	{ 0x1E5D0, 0x1E5FF, U"Ol Onal" },
	{ 0x1E7E0, 0x1E7FF, U"Ethiopic Extended-B" },
	{ 0x1E800, 0x1E8DF, U"Mende Kikakui" },
	{ 0x1E900, 0x1E95F, U"Adlam" },
	{ 0x1EC70, 0x1ECBF, U"Indic Siyaq Numbers" },
	{ 0x1ED00, 0x1ED4F, U"Ottoman Siyaq Numbers" },
	{ 0x1EE00, 0x1EEFF, U"Arabic Mathematical Alphabetic Symbols" },
	{ 0x1F000, 0x1F02F, U"Mahjong Tiles" },
	{ 0x1F030, 0x1F09F, U"Domino Tiles" },
	{ 0x1F0A0, 0x1F0FF, U"Playing Cards" },
	{ 0x1F100, 0x1F1FF, U"Enclosed Alphanumeric Supplement" },
	{ 0x1F200, 0x1F2FF, U"Enclosed Ideographic Supplement" },
	{ 0x1F300, 0x1F5FF, U"Miscellaneous Symbols and Pictographs" },
	{ 0x1F600, 0x1F64F, U"Emoticons" },
	{ 0x1F650, 0x1F67F, U"Ornamental Dingbats" },
	{ 0x1F680, 0x1F6FF, U"Transport and Map Symbols" },
	{ 0x1F700, 0x1F77F, U"Alchemical Symbols" },
	{ 0x1F780, 0x1F7FF, U"Geometric Shapes Extended" },
	{ 0x1F800, 0x1F8FF, U"Supplemental Arrows-C" },
	{ 0x1F900, 0x1F9FF, U"Supplemental Symbols and Pictographs" },
	{ 0x1FA00, 0x1FA6F, U"Chess Symbols" },
	{ 0x1FA70, 0x1FAFF, U"Symbols and Pictographs Extended-A" },
	{ 0x1FB00, 0x1FBFF, U"Symbols for Legacy Computing" },
	{ 0x20000, 0x2A6DF, U"CJK Unified Ideographs Extension B" },
	{ 0x2A700, 0x2B73F, U"CJK Unified Ideographs Extension C" },
	{ 0x2B740, 0x2B81F, U"CJK Unified Ideographs Extension D" },
	{ 0x2B820, 0x2CEAF, U"CJK Unified Ideographs Extension E" },
	{ 0x2CEB0, 0x2EBEF, U"CJK Unified Ideographs Extension F" },
	{ 0x2EBF0, 0x2EE5F, U"CJK Unified Ideographs Extension I" },
	{ 0x2F800, 0x2FA1F, U"CJK Compatibility Ideographs Supplement" },
	{ 0x30000, 0x3134F, U"CJK Unified Ideographs Extension G" },
	{ 0x31350, 0x323AF, U"CJK Unified Ideographs Extension H" },
	//{ 0xE0000, 0xE007F, U"Tags" },
	//{ 0xE0100, 0xE01EF, U"Variation Selectors Supplement" },
	{ 0xF0000, 0xFFFFF, U"Supplementary Private Use Area-A" },
	{ 0x100000, 0x10FFFF, U"Supplementary Private Use Area-B" },
	{ 0x10FFFF, 0x10FFFF, String() }
};

void DynamicFontImportSettingsDialog::_add_glyph_range_item(int32_t p_start, int32_t p_end, const String &p_name) {
	const int page_size = 512;
	int pages = (p_end - p_start) / page_size;
	int remain = (p_end - p_start) % page_size;

	int32_t start = p_start;
	for (int i = 0; i < pages; i++) {
		TreeItem *item = glyph_tree->create_item(glyph_root);
		ERR_FAIL_NULL(item);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_text(0, _pad_zeros(String::num_int64(start, 16)) + " - " + _pad_zeros(String::num_int64(start + page_size, 16)));
		item->set_text(1, p_name);
		item->set_metadata(0, Vector2i(start, start + page_size));
		start += page_size;
	}
	if (remain > 0) {
		TreeItem *item = glyph_tree->create_item(glyph_root);
		ERR_FAIL_NULL(item);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_text(0, _pad_zeros(String::num_int64(start, 16)) + " - " + _pad_zeros(String::num_int64(p_end, 16)));
		item->set_text(1, p_name);
		item->set_metadata(0, Vector2i(start, p_end));
	}
}

/*************************************************************************/
/* Page 1 callbacks: Rendering Options                                   */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_main_prop_changed(const String &p_edited_property) {
	// Update font preview.

	if (font_preview.is_valid()) {
		if (p_edited_property == "antialiasing") {
			font_preview->set_antialiasing((TextServer::FontAntialiasing)import_settings_data->get("antialiasing").operator int());
		} else if (p_edited_property == "generate_mipmaps") {
			font_preview->set_generate_mipmaps(import_settings_data->get("generate_mipmaps"));
		} else if (p_edited_property == "disable_embedded_bitmaps") {
			font_preview->set_disable_embedded_bitmaps(import_settings_data->get("disable_embedded_bitmaps"));
		} else if (p_edited_property == "multichannel_signed_distance_field") {
			font_preview->set_multichannel_signed_distance_field(import_settings_data->get("multichannel_signed_distance_field"));
			_variation_selected();
			_variations_validate();
		} else if (p_edited_property == "msdf_pixel_range") {
			font_preview->set_msdf_pixel_range(import_settings_data->get("msdf_pixel_range"));
		} else if (p_edited_property == "msdf_size") {
			font_preview->set_msdf_size(import_settings_data->get("msdf_size"));
		} else if (p_edited_property == "allow_system_fallback") {
			font_preview->set_allow_system_fallback(import_settings_data->get("allow_system_fallback"));
		} else if (p_edited_property == "force_autohinter") {
			font_preview->set_force_autohinter(import_settings_data->get("force_autohinter"));
		} else if (p_edited_property == "hinting") {
			font_preview->set_hinting((TextServer::Hinting)import_settings_data->get("hinting").operator int());
		} else if (p_edited_property == "subpixel_positioning") {
			font_preview->set_subpixel_positioning((TextServer::SubpixelPositioning)import_settings_data->get("subpixel_positioning").operator int());
		} else if (p_edited_property == "oversampling") {
			font_preview->set_oversampling(import_settings_data->get("oversampling"));
		}
	}

	font_preview_label->add_theme_font_override(SceneStringName(font), font_preview);
	font_preview_label->add_theme_font_size_override(SceneStringName(font_size), 200 * EDSCALE);
	font_preview_label->queue_redraw();
}

/*************************************************************************/
/* Page 2 callbacks: Configurations                                      */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_variation_add() {
	TreeItem *vars_item = vars_list->create_item(vars_list_root);
	ERR_FAIL_NULL(vars_item);

	vars_item->set_text(0, TTR("New Configuration"));
	vars_item->set_editable(0, true);
	vars_item->add_button(1, get_editor_theme_icon(SNAME("Remove")), BUTTON_REMOVE_VAR, false, TTR("Remove Variation"));
	vars_item->set_button_color(1, 0, Color(1, 1, 1, 0.75));

	Ref<DynamicFontImportSettingsData> import_variation_data;
	import_variation_data.instantiate();
	import_variation_data->owner = this;
	ERR_FAIL_COND(import_variation_data.is_null());

	for (List<ResourceImporter::ImportOption>::Element *E = options_variations.front(); E; E = E->next()) {
		import_variation_data->defaults[E->get().option.name] = E->get().default_value;
	}

	import_variation_data->options = options_variations;
	inspector_vars->edit(import_variation_data.ptr());
	import_variation_data->notify_property_list_changed();
	import_variation_data->fd = font_main;

	vars_item->set_metadata(0, import_variation_data);

	_variations_validate();
}

void DynamicFontImportSettingsDialog::_variation_selected() {
	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		Ref<DynamicFontImportSettingsData> import_variation_data = vars_item->get_metadata(0);
		ERR_FAIL_COND(import_variation_data.is_null());

		inspector_vars->edit(import_variation_data.ptr());
		import_variation_data->notify_property_list_changed();

		label_glyphs->set_text(vformat(TTR("Preloaded glyphs: %d"), import_variation_data->selected_glyphs.size()));
		_range_selected();
		_change_text_opts();

		btn_fill->set_disabled(false);
		btn_fill_locales->set_disabled(false);
	} else {
		btn_fill->set_disabled(true);
		btn_fill_locales->set_disabled(true);
	}
}

void DynamicFontImportSettingsDialog::_variation_remove(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	TreeItem *vars_item = (TreeItem *)p_item;
	ERR_FAIL_NULL(vars_item);

	inspector_vars->edit(nullptr);

	vars_list_root->remove_child(vars_item);
	memdelete(vars_item);

	if (vars_list_root->get_first_child()) {
		Ref<DynamicFontImportSettingsData> import_variation_data = vars_list_root->get_first_child()->get_metadata(0);
		inspector_vars->edit(import_variation_data.ptr());
		import_variation_data->notify_property_list_changed();
	}

	_variations_validate();

	vars_item = vars_list->get_selected();
	if (vars_item) {
		btn_fill->set_disabled(false);
		btn_fill_locales->set_disabled(false);
	} else {
		btn_fill->set_disabled(true);
		btn_fill_locales->set_disabled(true);
	}
}

void DynamicFontImportSettingsDialog::_variation_changed(const String &p_edited_property) {
	_variations_validate();
}

void DynamicFontImportSettingsDialog::_variations_validate() {
	String warn;
	if (!vars_list_root->get_first_child()) {
		warn = TTR("Warning: There are no configurations specified, no glyphs will be pre-rendered.");
	}
	for (TreeItem *vars_item_a = vars_list_root->get_first_child(); vars_item_a; vars_item_a = vars_item_a->get_next()) {
		Ref<DynamicFontImportSettingsData> import_variation_data_a = vars_item_a->get_metadata(0);
		ERR_FAIL_COND(import_variation_data_a.is_null());

		for (TreeItem *vars_item_b = vars_list_root->get_first_child(); vars_item_b; vars_item_b = vars_item_b->get_next()) {
			if (vars_item_b != vars_item_a) {
				bool match = true;
				for (const KeyValue<StringName, Variant> &E : import_variation_data_a->settings) {
					Ref<DynamicFontImportSettingsData> import_variation_data_b = vars_item_b->get_metadata(0);
					ERR_FAIL_COND(import_variation_data_b.is_null());
					match = match && (import_variation_data_b->settings[E.key] == E.value);
				}
				if (match) {
					warn = TTR("Warning: Multiple configurations have identical settings. Duplicates will be ignored.");
					break;
				}
			}
		}
	}
	if ((TextServer::FontAntialiasing)(int)import_settings_data->get("antialiasing") == TextServer::FONT_ANTIALIASING_LCD) {
		warn += "\n" + TTR("Note: LCD Subpixel antialiasing is selected, each of the glyphs will be pre-rendered for all supported subpixel layouts (5x).");
	}
	if ((TextServer::SubpixelPositioning)(int)import_settings_data->get("subpixel_positioning") != TextServer::SUBPIXEL_POSITIONING_DISABLED) {
		warn += "\n" + TTR("Note: Subpixel positioning is selected, each of the glyphs might be pre-rendered for multiple subpixel offsets (up to 4x).");
	}
	if (warn.is_empty()) {
		label_warn->set_text("");
		label_warn->hide();
	} else {
		label_warn->set_text(warn);
		label_warn->show();
	}
}

/*************************************************************************/
/* Page 2.1 callbacks: Text to select glyphs                             */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_change_text_opts() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	Ref<FontVariation> font_main_text;
	font_main_text.instantiate();
	font_main_text->set_base_font(font_main);
	font_main_text->set_opentype_features(text_settings_data->get("opentype_features"));
	font_main_text->set_variation_opentype(import_variation_data->get("variation_opentype"));
	font_main_text->set_variation_embolden(import_variation_data->get("variation_embolden"));
	font_main_text->set_variation_face_index(import_variation_data->get("variation_face_index"));
	font_main_text->set_variation_transform(import_variation_data->get("variation_transform"));

	text_edit->add_theme_font_override(SceneStringName(font), font_main_text);
}

void DynamicFontImportSettingsDialog::_glyph_update_lbl() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	int linked_glyphs = 0;
	for (const char32_t &c : import_variation_data->selected_chars) {
		if (import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, c))) {
			linked_glyphs++;
		}
	}
	int unlinked_glyphs = import_variation_data->selected_glyphs.size() - linked_glyphs;
	label_glyphs->set_text(vformat(TTR("Preloaded glyphs: %d"), unlinked_glyphs + import_variation_data->selected_chars.size()));
}

void DynamicFontImportSettingsDialog::_glyph_clear() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	import_variation_data->selected_glyphs.clear();
	_glyph_update_lbl();
	_range_selected();
}

void DynamicFontImportSettingsDialog::_glyph_text_selected() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}
	RID text_rid = TS->create_shaped_text();
	if (text_rid.is_valid()) {
		TS->shaped_text_add_string(text_rid, text_edit->get_text(), font_main->get_rids(), 16, text_settings_data->get("opentype_features"), text_settings_data->get("language"));
		TS->shaped_text_shape(text_rid);
		const Glyph *gl = TS->shaped_text_get_glyphs(text_rid);
		const int gl_size = TS->shaped_text_get_glyph_count(text_rid);

		for (int i = 0; i < gl_size; i++) {
			if (gl[i].font_rid.is_valid() && gl[i].index != 0) {
				import_variation_data->selected_glyphs.insert(gl[i].index);
			}
		}
		TS->free_rid(text_rid);
		_glyph_update_lbl();
	}
	_range_selected();
}

/*************************************************************************/
/* Page 2.2 callbacks: Character map                                     */
/*************************************************************************/

void DynamicFontImportSettingsDialog::_glyph_selected() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	TreeItem *item = glyph_table->get_selected();
	ERR_FAIL_NULL(item);

	Color scol = glyph_table->get_theme_color(SNAME("box_selection_fill_color"), EditorStringName(Editor));
	Color fcol = glyph_table->get_theme_color(SNAME("font_selected_color"), EditorStringName(Editor));
	scol.a = 1.f;

	int32_t c = item->get_metadata(glyph_table->get_selected_column());
	if (font_main->has_char(c)) {
		if (_char_update(c)) {
			item->set_custom_color(glyph_table->get_selected_column(), fcol);
			item->set_custom_bg_color(glyph_table->get_selected_column(), scol);
		} else {
			item->clear_custom_color(glyph_table->get_selected_column());
			item->clear_custom_bg_color(glyph_table->get_selected_column());
		}
	}
	_glyph_update_lbl();

	item = glyph_tree->get_selected();
	ERR_FAIL_NULL(item);
	Vector2i range = item->get_metadata(0);

	int total_chars = range.y - range.x;
	int selected_count = 0;
	for (int i = range.x; i < range.y; i++) {
		if (!font_main->has_char(i)) {
			total_chars--;
		}

		if (import_variation_data->selected_chars.has(i)) {
			selected_count++;
		}
	}

	if (selected_count == total_chars) {
		item->set_checked(0, true);
	} else if (selected_count > 0) {
		item->set_indeterminate(0, true);
	} else {
		item->set_checked(0, false);
	}
}

void DynamicFontImportSettingsDialog::_range_edited() {
	TreeItem *item = glyph_tree->get_selected();
	ERR_FAIL_NULL(item);
	Vector2i range = item->get_metadata(0);
	_range_update(range.x, range.y);
}

void DynamicFontImportSettingsDialog::_range_selected() {
	TreeItem *item = glyph_tree->get_selected();
	if (item) {
		Vector2i range = item->get_metadata(0);
		_edit_range(range.x, range.y);
	}
}

void DynamicFontImportSettingsDialog::_edit_range(int32_t p_start, int32_t p_end) {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	glyph_table->clear();

	TreeItem *root = glyph_table->create_item();
	ERR_FAIL_NULL(root);

	Color scol = glyph_table->get_theme_color(SNAME("box_selection_fill_color"), EditorStringName(Editor));
	Color fcol = glyph_table->get_theme_color(SNAME("font_selected_color"), EditorStringName(Editor));
	scol.a = 1.f;

	TreeItem *item = nullptr;
	int col = 0;

	Ref<Font> font_main_big = font_main->duplicate();

	for (int32_t c = p_start; c <= p_end; c++) {
		if (col == 0) {
			item = glyph_table->create_item(root);
			ERR_FAIL_NULL(item);
			item->set_text(0, _pad_zeros(String::num_int64(c, 16)));
			item->set_text_alignment(0, HORIZONTAL_ALIGNMENT_LEFT);
			item->set_selectable(0, false);
			item->set_custom_bg_color(0, glyph_table->get_theme_color(SNAME("dark_color_3"), EditorStringName(Editor)));
		}
		if (font_main->has_char(c)) {
			item->set_text(col + 1, String::chr(c));
			item->set_custom_color(col + 1, Color(1, 1, 1));
			if (import_variation_data->selected_chars.has(c) || import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, c))) {
				item->set_custom_color(col + 1, fcol);
				item->set_custom_bg_color(col + 1, scol);
			} else {
				item->clear_custom_color(col + 1);
				item->clear_custom_bg_color(col + 1);
			}
		} else {
			item->set_custom_bg_color(col + 1, glyph_table->get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor)));
		}
		item->set_metadata(col + 1, c);
		item->set_text_alignment(col + 1, HORIZONTAL_ALIGNMENT_CENTER);
		item->set_selectable(col + 1, true);

		item->set_custom_font(col + 1, font_main_big);
		item->set_custom_font_size(col + 1, get_theme_font_size(SceneStringName(font_size)) * 2);

		col++;
		if (col == 16) {
			col = 0;
		}
	}
	_glyph_update_lbl();
}

bool DynamicFontImportSettingsDialog::_char_update(int32_t p_char) {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return false;
	}

	if (import_variation_data->selected_chars.has(p_char)) {
		import_variation_data->selected_chars.erase(p_char);
		return false;
	} else if (font_main.is_valid() && import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, p_char))) {
		import_variation_data->selected_glyphs.erase(font_main->get_glyph_index(16, p_char));
		return false;
	} else {
		import_variation_data->selected_chars.insert(p_char);
		return true;
	}
}

void DynamicFontImportSettingsDialog::_range_update(int32_t p_start, int32_t p_end) {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	bool all_selected = true;
	for (int32_t i = p_start; i <= p_end; i++) {
		if (font_main->has_char(i)) {
			if (font_main.is_valid()) {
				all_selected = all_selected && (import_variation_data->selected_chars.has(i) || import_variation_data->selected_glyphs.has(font_main->get_glyph_index(16, i)));
			} else {
				all_selected = all_selected && import_variation_data->selected_chars.has(i);
			}
		}
	}
	for (int32_t i = p_start; i <= p_end; i++) {
		if (font_main->has_char(i)) {
			if (!all_selected) {
				import_variation_data->selected_chars.insert(i);
			} else {
				import_variation_data->selected_chars.erase(i);
				if (font_main.is_valid()) {
					import_variation_data->selected_glyphs.erase(font_main->get_glyph_index(16, i));
				}
			}
		}
	}
	_edit_range(p_start, p_end);

	TreeItem *item = glyph_tree->get_selected();
	ERR_FAIL_NULL(item);
	item->set_checked(0, !all_selected);
}

/*************************************************************************/
/* Common                                                                */
/*************************************************************************/

DynamicFontImportSettingsDialog *DynamicFontImportSettingsDialog::singleton = nullptr;

String DynamicFontImportSettingsDialog::_pad_zeros(const String &p_hex) const {
	int len = CLAMP(5 - p_hex.length(), 0, 5);
	return String("0").repeat(len) + p_hex;
}

void DynamicFontImportSettingsDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			connect(SceneStringName(confirmed), callable_mp(this, &DynamicFontImportSettingsDialog::_re_import));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			add_var->set_icon(get_editor_theme_icon(SNAME("Add")));
			label_warn->add_theme_color_override(SceneStringName(font_color), get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		} break;
	}
}

void DynamicFontImportSettingsDialog::_re_import() {
	HashMap<StringName, Variant> main_settings;

	main_settings["face_index"] = import_settings_data->get("face_index");
	main_settings["antialiasing"] = import_settings_data->get("antialiasing");
	main_settings["generate_mipmaps"] = import_settings_data->get("generate_mipmaps");
	main_settings["disable_embedded_bitmaps"] = import_settings_data->get("disable_embedded_bitmaps");
	main_settings["multichannel_signed_distance_field"] = import_settings_data->get("multichannel_signed_distance_field");
	main_settings["msdf_pixel_range"] = import_settings_data->get("msdf_pixel_range");
	main_settings["msdf_size"] = import_settings_data->get("msdf_size");
	main_settings["allow_system_fallback"] = import_settings_data->get("allow_system_fallback");
	main_settings["force_autohinter"] = import_settings_data->get("force_autohinter");
	main_settings["hinting"] = import_settings_data->get("hinting");
	main_settings["subpixel_positioning"] = import_settings_data->get("subpixel_positioning");
	main_settings["oversampling"] = import_settings_data->get("oversampling");
	main_settings["fallbacks"] = import_settings_data->get("fallbacks");
	main_settings["compress"] = import_settings_data->get("compress");

	Array configurations;
	for (TreeItem *vars_item = vars_list_root->get_first_child(); vars_item; vars_item = vars_item->get_next()) {
		Ref<DynamicFontImportSettingsData> import_variation_data = vars_item->get_metadata(0);
		ERR_FAIL_COND(import_variation_data.is_null());

		Dictionary preload_config;
		preload_config["name"] = vars_item->get_text(0);

		Size2i conf_size = Vector2i(16, 0);
		for (const KeyValue<StringName, Variant> &E : import_variation_data->settings) {
			if (E.key == "size") {
				conf_size.x = E.value;
			}
			if (E.key == "outline_size") {
				conf_size.y = E.value;
			} else {
				preload_config[E.key] = E.value;
			}
		}
		preload_config["size"] = conf_size;

		Array chars;
		for (const char32_t &E : import_variation_data->selected_chars) {
			chars.push_back(E);
		}
		preload_config["chars"] = chars;

		Array glyphs;
		for (const int32_t &E : import_variation_data->selected_glyphs) {
			glyphs.push_back(E);
		}
		preload_config["glyphs"] = glyphs;

		configurations.push_back(preload_config);
	}
	main_settings["preload"] = configurations;
	main_settings["language_support"] = import_settings_data->get("language_support");
	main_settings["script_support"] = import_settings_data->get("script_support");
	main_settings["opentype_features"] = import_settings_data->get("opentype_features");

	if (OS::get_singleton()->is_stdout_verbose()) {
		print_line("Import settings:");
		for (const KeyValue<StringName, Variant> &E : main_settings) {
			print_line(String("    ") + String(E.key).utf8().get_data() + " == " + String(E.value).utf8().get_data());
		}
	}

	EditorFileSystem::get_singleton()->reimport_file_with_custom_parameters(base_path, "font_data_dynamic", main_settings);
}

void DynamicFontImportSettingsDialog::_locale_edited() {
	TreeItem *item = locale_tree->get_selected();
	ERR_FAIL_NULL(item);
	item->set_checked(0, !item->is_checked(0));
}

void DynamicFontImportSettingsDialog::_process_locales() {
	Ref<DynamicFontImportSettingsData> import_variation_data;

	TreeItem *vars_item = vars_list->get_selected();
	if (vars_item) {
		import_variation_data = vars_item->get_metadata(0);
	}
	if (import_variation_data.is_null()) {
		return;
	}

	for (int i = 0; i < locale_root->get_child_count(); i++) {
		TreeItem *item = locale_root->get_child(i);
		if (item) {
			if (item->is_checked(0)) {
				String locale = item->get_text(0);
				Ref<Translation> tr = ResourceLoader::load(locale);
				if (tr.is_valid()) {
					Vector<String> messages = tr->get_translated_message_list();
					for (const String &E : messages) {
						RID text_rid = TS->create_shaped_text();
						if (text_rid.is_valid()) {
							TS->shaped_text_add_string(text_rid, E, font_main->get_rids(), 16, Dictionary(), tr->get_locale());
							TS->shaped_text_shape(text_rid);
							const Glyph *gl = TS->shaped_text_get_glyphs(text_rid);
							const int gl_size = TS->shaped_text_get_glyph_count(text_rid);

							for (int j = 0; j < gl_size; j++) {
								if (gl[j].font_rid.is_valid() && gl[j].index != 0) {
									import_variation_data->selected_glyphs.insert(gl[j].index);
								}
							}
							TS->free_rid(text_rid);
						}
					}
				}
			}
		}
	}

	_glyph_update_lbl();
	_range_selected();
}

void DynamicFontImportSettingsDialog::open_settings(const String &p_path) {
	// Load base font data.
	Vector<uint8_t> font_data = FileAccess::get_file_as_bytes(p_path);

	// Load project locale list.
	locale_tree->clear();
	locale_root = locale_tree->create_item();
	ERR_FAIL_NULL(locale_root);

	Vector<String> translations = GLOBAL_GET("internationalization/locale/translations");
	for (const String &E : translations) {
		TreeItem *item = locale_tree->create_item(locale_root);
		ERR_FAIL_NULL(item);
		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_text(0, E);
	}

	// Load font for preview.
	font_preview.instantiate();
	font_preview->set_data(font_data);

	String font_name = vformat("%s (%s)", font_preview->get_font_name(), font_preview->get_font_style_name());
	String sample;
	static const String sample_base = U"12漢字ԱբΑαАбΑαאבابܐܒހށआআਆઆଆஆఆಆആආกิກິༀကႠა한글ሀᎣᐁᚁᚠᜀᜠᝀᝠកᠠᤁᥐAb😀";
	for (int i = 0; i < sample_base.length(); i++) {
		if (font_preview->has_char(sample_base[i])) {
			sample += sample_base[i];
		}
	}
	if (sample.is_empty()) {
		sample = font_preview->get_supported_chars().substr(0, 6);
	}
	font_preview_label->set_text(sample);

	Ref<Font> bold_font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
	if (bold_font.is_valid()) {
		font_name_label->add_theme_font_override("bold_font", bold_font);
	}
	font_name_label->set_text(font_name);

	// Load second copy of font with MSDF disabled for the glyph table and metadata extraction.
	font_main.instantiate();
	font_main->set_data(font_data);
	font_main->set_multichannel_signed_distance_field(false);

	text_edit->add_theme_font_override(SceneStringName(font), font_main);

	base_path = p_path;

	inspector_vars->edit(nullptr);
	inspector_text->edit(nullptr);
	inspector_general->edit(nullptr);

	text_settings_data.instantiate();
	ERR_FAIL_COND(text_settings_data.is_null());

	text_settings_data->owner = this;

	for (List<ResourceImporter::ImportOption>::Element *F = options_text.front(); F; F = F->next()) {
		text_settings_data->defaults[F->get().option.name] = F->get().default_value;
	}

	text_settings_data->fd = font_main;
	text_settings_data->options = options_text;

	inspector_text->edit(text_settings_data.ptr());

	int gww = get_theme_font(SceneStringName(font))->get_string_size("00000").x + 50;
	glyph_table->set_column_custom_minimum_width(0, gww);
	glyph_table->clear();
	vars_list->clear();

	glyph_tree->set_selected(glyph_root->get_child(0));

	vars_list_root = vars_list->create_item();

	import_settings_data->settings.clear();
	import_settings_data->defaults.clear();
	for (List<ResourceImporter::ImportOption>::Element *E = options_general.front(); E; E = E->next()) {
		import_settings_data->defaults[E->get().option.name] = E->get().default_value;
	}

	Ref<ConfigFile> config;
	config.instantiate();
	ERR_FAIL_COND(config.is_null());

	Error err = config->load(p_path + ".import");
	print_verbose("Loading import settings:");
	if (err == OK) {
		List<String> keys;
		config->get_section_keys("params", &keys);
		for (List<String>::Element *E = keys.front(); E; E = E->next()) {
			String key = E->get();
			print_verbose(String("    ") + key + " == " + String(config->get_value("params", key)));
			if (key == "preload") {
				Array preload_configurations = config->get_value("params", key);
				for (int i = 0; i < preload_configurations.size(); i++) {
					Dictionary preload_config = preload_configurations[i];

					Dictionary variation = preload_config.has("variation_opentype") ? preload_config["variation_opentype"].operator Dictionary() : Dictionary();
					double embolden = preload_config.has("variation_embolden") ? preload_config["variation_embolden"].operator double() : 0;
					int face_index = preload_config.has("variation_face_index") ? preload_config["variation_face_index"].operator int() : 0;
					Transform2D transform = preload_config.has("variation_transform") ? preload_config["variation_transform"].operator Transform2D() : Transform2D();
					Vector2i font_size = preload_config.has("size") ? preload_config["size"].operator Vector2i() : Vector2i(16, 0);
					String cfg_name = preload_config.has("name") ? preload_config["name"].operator String() : vformat("Configuration %d", i);

					TreeItem *vars_item = vars_list->create_item(vars_list_root);
					ERR_FAIL_NULL(vars_item);

					vars_item->set_text(0, cfg_name);
					vars_item->set_editable(0, true);
					vars_item->add_button(1, get_editor_theme_icon(SNAME("Remove")), BUTTON_REMOVE_VAR, false, TTR("Remove Variation"));
					vars_item->set_button_color(1, 0, Color(1, 1, 1, 0.75));

					Ref<DynamicFontImportSettingsData> import_variation_data_custom;
					import_variation_data_custom.instantiate();
					ERR_FAIL_COND(import_variation_data_custom.is_null());

					import_variation_data_custom->owner = this;
					for (List<ResourceImporter::ImportOption>::Element *F = options_variations.front(); F; F = F->next()) {
						import_variation_data_custom->defaults[F->get().option.name] = F->get().default_value;
					}

					import_variation_data_custom->fd = font_main;

					import_variation_data_custom->options = options_variations;
					vars_item->set_metadata(0, import_variation_data_custom);

					import_variation_data_custom->set("size", font_size.x);
					import_variation_data_custom->set("outline_size", font_size.y);
					import_variation_data_custom->set("variation_opentype", variation);
					import_variation_data_custom->set("variation_embolden", embolden);
					import_variation_data_custom->set("variation_face_index", face_index);
					import_variation_data_custom->set("variation_transform", transform);

					Array chars = preload_config["chars"];
					for (int j = 0; j < chars.size(); j++) {
						char32_t c = chars[j].operator int();
						import_variation_data_custom->selected_chars.insert(c);
					}

					Array glyphs = preload_config["glyphs"];
					for (int j = 0; j < glyphs.size(); j++) {
						int32_t c = glyphs[j];
						import_variation_data_custom->selected_glyphs.insert(c);
					}
				}
				if (preload_configurations.is_empty()) {
					_variation_add(); // Add default variation.
				}
				vars_list->set_selected(vars_list_root->get_child(0));
			} else {
				Variant value = config->get_value("params", key);
				import_settings_data->defaults[key] = value;
			}
		}
	}

	import_settings_data->fd = font_main;
	import_settings_data->options = options_general;
	inspector_general->edit(import_settings_data.ptr());
	import_settings_data->notify_property_list_changed();

	if (font_preview.is_valid()) {
		font_preview->set_antialiasing((TextServer::FontAntialiasing)import_settings_data->get("antialiasing").operator int());
		font_preview->set_multichannel_signed_distance_field(import_settings_data->get("multichannel_signed_distance_field"));
		font_preview->set_msdf_pixel_range(import_settings_data->get("msdf_pixel_range"));
		font_preview->set_msdf_size(import_settings_data->get("msdf_size"));
		font_preview->set_allow_system_fallback(import_settings_data->get("allow_system_fallback"));
		font_preview->set_force_autohinter(import_settings_data->get("force_autohinter"));
		font_preview->set_hinting((TextServer::Hinting)import_settings_data->get("hinting").operator int());
		font_preview->set_subpixel_positioning((TextServer::SubpixelPositioning)import_settings_data->get("subpixel_positioning").operator int());
		font_preview->set_oversampling(import_settings_data->get("oversampling"));
	}
	font_preview_label->add_theme_font_override(SceneStringName(font), font_preview);
	font_preview_label->add_theme_font_size_override(SceneStringName(font_size), 200 * EDSCALE);
	font_preview_label->queue_redraw();

	_variations_validate();

	popup_centered_ratio();

	set_title(vformat(TTR("Advanced Import Settings for '%s'"), base_path.get_file()));
}

DynamicFontImportSettingsDialog *DynamicFontImportSettingsDialog::get_singleton() {
	return singleton;
}

DynamicFontImportSettingsDialog::DynamicFontImportSettingsDialog() {
	singleton = this;

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Rendering", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "antialiasing", PROPERTY_HINT_ENUM, "None,Grayscale,LCD Subpixel"), 1));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "generate_mipmaps"), false));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "disable_embedded_bitmaps"), true));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "multichannel_signed_distance_field", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), true));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "msdf_pixel_range", PROPERTY_HINT_RANGE, "1,100,1"), 8));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "msdf_size", PROPERTY_HINT_RANGE, "1,250,1"), 48));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "allow_system_fallback"), true));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "force_autohinter"), false));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "hinting", PROPERTY_HINT_ENUM, "None,Light,Normal"), 1));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "subpixel_positioning", PROPERTY_HINT_ENUM, "Disabled,Auto,One Half of a Pixel,One Quarter of a Pixel"), 1));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "oversampling", PROPERTY_HINT_RANGE, "0,10,0.1"), 0.0));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Metadata Overrides", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "language_support"), Dictionary()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "script_support"), Dictionary()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "opentype_features"), Dictionary()));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Fallbacks", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::ARRAY, "fallbacks", PROPERTY_HINT_ARRAY_TYPE, MAKE_RESOURCE_TYPE_HINT("Font")), Array()));

	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Compress", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_GROUP), Variant()));
	options_general.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::BOOL, "compress", PROPERTY_HINT_NONE, ""), false));

	options_text.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "opentype_features"), Dictionary()));
	options_text.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::STRING, "language", PROPERTY_HINT_LOCALE_ID, ""), ""));

	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "size", PROPERTY_HINT_RANGE, "0,127,1"), 16));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "outline_size", PROPERTY_HINT_RANGE, "0,127,1"), 0));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::NIL, "Variation", PROPERTY_HINT_NONE, "variation", PROPERTY_USAGE_GROUP), Variant()));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::DICTIONARY, "variation_opentype"), Dictionary()));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::FLOAT, "variation_embolden", PROPERTY_HINT_RANGE, "-2,2,0.01"), 0));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::INT, "variation_face_index"), 0));
	options_variations.push_back(ResourceImporter::ImportOption(PropertyInfo(Variant::TRANSFORM2D, "variation_transform"), Transform2D()));

	// Root layout

	VBoxContainer *root_vb = memnew(VBoxContainer);
	add_child(root_vb);

	main_pages = memnew(TabContainer);
	main_pages->set_tab_alignment(TabBar::ALIGNMENT_CENTER);
	main_pages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	main_pages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_pages->set_theme_type_variation("TabContainerOdd");
	root_vb->add_child(main_pages);

	label_warn = memnew(Label);
	label_warn->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	label_warn->set_text("");
	root_vb->add_child(label_warn);
	label_warn->hide();

	// Page 1 layout: Rendering Options

	VBoxContainer *page1_vb = memnew(VBoxContainer);
	page1_vb->set_name(TTR("Rendering Options"));
	main_pages->add_child(page1_vb);

	page1_description = memnew(Label);
	page1_description->set_text(TTR("Select font rendering options, fallback font, and metadata override:"));
	page1_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_vb->add_child(page1_description);

	HSplitContainer *page1_hb = memnew(HSplitContainer);
	page1_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page1_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_vb->add_child(page1_hb);

	VBoxContainer *page1_lbl_vb = memnew(VBoxContainer);
	page1_lbl_vb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page1_lbl_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_hb->add_child(page1_lbl_vb);

	font_name_label = memnew(Label);
	font_name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	font_name_label->set_clip_text(true);
	font_name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_lbl_vb->add_child(font_name_label);

	font_preview_label = memnew(Label);
	font_preview_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	font_preview_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	font_preview_label->set_autowrap_mode(TextServer::AUTOWRAP_ARBITRARY);
	font_preview_label->set_clip_text(true);
	font_preview_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	font_preview_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page1_lbl_vb->add_child(font_preview_label);

	inspector_general = memnew(EditorInspector);
	inspector_general->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	inspector_general->set_custom_minimum_size(Size2(300 * EDSCALE, 250 * EDSCALE));
	page1_hb->add_child(inspector_general);
	inspector_general->connect("property_edited", callable_mp(this, &DynamicFontImportSettingsDialog::_main_prop_changed));

	// Page 2 layout: Configurations
	VBoxContainer *page2_vb = memnew(VBoxContainer);
	page2_vb->set_name(TTR("Pre-render Configurations"));
	main_pages->add_child(page2_vb);

	page2_description = memnew(Label);
	page2_description->set_text(TTR("Add font size, and variation coordinates, and select glyphs to pre-render:"));
	page2_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_vb->add_child(page2_description);

	HSplitContainer *page2_hb = memnew(HSplitContainer);
	page2_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_vb->add_child(page2_hb);

	VBoxContainer *page2_side_vb = memnew(VBoxContainer);
	page2_hb->add_child(page2_side_vb);

	HBoxContainer *page2_hb_vars = memnew(HBoxContainer);
	page2_side_vb->add_child(page2_hb_vars);

	label_vars = memnew(Label);
	label_vars->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	label_vars->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	label_vars->set_text(TTR("Configuration:"));
	page2_hb_vars->add_child(label_vars);

	add_var = memnew(Button);
	add_var->set_tooltip_text(TTR("Add configuration"));
	page2_hb_vars->add_child(add_var);
	add_var->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_variation_add));

	vars_list = memnew(Tree);
	vars_list->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	vars_list->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	vars_list->set_hide_root(true);
	vars_list->set_columns(2);
	vars_list->set_column_expand(0, true);
	vars_list->set_column_custom_minimum_width(0, 80 * EDSCALE);
	vars_list->set_column_expand(1, false);
	vars_list->set_column_custom_minimum_width(1, 50 * EDSCALE);
	vars_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_side_vb->add_child(vars_list);
	vars_list->connect(SceneStringName(item_selected), callable_mp(this, &DynamicFontImportSettingsDialog::_variation_selected));
	vars_list->connect("button_clicked", callable_mp(this, &DynamicFontImportSettingsDialog::_variation_remove));

	inspector_vars = memnew(EditorInspector);
	inspector_vars->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_side_vb->add_child(inspector_vars);
	inspector_vars->connect("property_edited", callable_mp(this, &DynamicFontImportSettingsDialog::_variation_changed));

	VBoxContainer *preload_pages_vb = memnew(VBoxContainer);
	page2_hb->add_child(preload_pages_vb);

	preload_pages = memnew(TabContainer);
	preload_pages->set_tab_alignment(TabBar::ALIGNMENT_CENTER);
	preload_pages->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	preload_pages->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preload_pages_vb->add_child(preload_pages);

	HBoxContainer *gl_hb = memnew(HBoxContainer);
	gl_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	preload_pages_vb->add_child(gl_hb);

	label_glyphs = memnew(Label);
	label_glyphs->set_text(vformat(TTR("Preloaded glyphs: %d"), 0));
	label_glyphs->set_custom_minimum_size(Size2(50 * EDSCALE, 0));
	gl_hb->add_child(label_glyphs);

	Button *btn_clear = memnew(Button);
	btn_clear->set_text(TTR("Clear Glyph List"));
	gl_hb->add_child(btn_clear);
	btn_clear->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_glyph_clear));

	VBoxContainer *page2_0_vb = memnew(VBoxContainer);
	page2_0_vb->set_name(TTR("Glyphs from the Translations"));
	preload_pages->add_child(page2_0_vb);

	page2_0_description = memnew(Label);
	page2_0_description->set_text(TTR("Select translations to add all required glyphs to pre-render list:"));
	page2_0_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_0_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_0_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_0_vb->add_child(page2_0_description);

	locale_tree = memnew(Tree);
	locale_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	locale_tree->set_columns(1);
	locale_tree->set_hide_root(true);
	locale_tree->set_column_expand(0, true);
	locale_tree->set_column_custom_minimum_width(0, 120 * EDSCALE);
	locale_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_0_vb->add_child(locale_tree);
	locale_tree->connect("item_activated", callable_mp(this, &DynamicFontImportSettingsDialog::_locale_edited));

	locale_root = locale_tree->create_item();

	HBoxContainer *locale_hb = memnew(HBoxContainer);
	locale_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_0_vb->add_child(locale_hb);

	btn_fill_locales = memnew(Button);
	btn_fill_locales->set_text(TTR("Shape all Strings in the Translations and Add Glyphs"));
	locale_hb->add_child(btn_fill_locales);
	btn_fill_locales->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_process_locales));

	// Page 2.1 layout: Text to select glyphs
	VBoxContainer *page2_1_vb = memnew(VBoxContainer);
	page2_1_vb->set_name(TTR("Glyphs from the Text"));
	preload_pages->add_child(page2_1_vb);

	page2_1_description = memnew(Label);
	page2_1_description->set_text(TTR("Enter a text and select OpenType features to shape and add all required glyphs to pre-render list:"));
	page2_1_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_1_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_1_vb->add_child(page2_1_description);

	HSplitContainer *page2_1_hb = memnew(HSplitContainer);
	page2_1_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_hb->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_vb->add_child(page2_1_hb);

	inspector_text = memnew(EditorInspector);

	inspector_text->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	inspector_text->set_custom_minimum_size(Size2(300 * EDSCALE, 250 * EDSCALE));
	page2_1_hb->add_child(inspector_text);
	inspector_text->connect("property_edited", callable_mp(this, &DynamicFontImportSettingsDialog::_change_text_opts));

	text_edit = memnew(TextEdit);
	text_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	text_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_hb->add_child(text_edit);

	HBoxContainer *text_hb = memnew(HBoxContainer);
	text_hb->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_1_vb->add_child(text_hb);

	btn_fill = memnew(Button);
	btn_fill->set_text(TTR("Shape Text and Add Glyphs"));
	text_hb->add_child(btn_fill);
	btn_fill->connect(SceneStringName(pressed), callable_mp(this, &DynamicFontImportSettingsDialog::_glyph_text_selected));

	// Page 2.2 layout: Character map
	VBoxContainer *page2_2_vb = memnew(VBoxContainer);
	page2_2_vb->set_name(TTR("Glyphs from the Character Map"));
	preload_pages->add_child(page2_2_vb);

	page2_2_description = memnew(Label);
	page2_2_description->set_text(TTR("Add or remove glyphs from the character map to pre-render list:\nNote: Some stylistic alternatives and glyph variants do not have one-to-one correspondence to character, and not shown in this map, use \"Glyphs from the text\" tab to add these."));
	page2_2_description->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_2_description->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	page2_2_description->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
	page2_2_vb->add_child(page2_2_description);

	HSplitContainer *glyphs_split = memnew(HSplitContainer);
	glyphs_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	glyphs_split->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	page2_2_vb->add_child(glyphs_split);

	glyph_table = memnew(Tree);
	glyph_table->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	glyph_table->set_custom_minimum_size(Size2((30 * 16 + 100) * EDSCALE, 0));
	glyph_table->set_columns(17);
	glyph_table->set_column_expand(0, false);
	glyph_table->set_hide_root(true);
	glyph_table->set_allow_reselect(true);
	glyph_table->set_select_mode(Tree::SELECT_SINGLE);
	glyph_table->set_column_titles_visible(true);
	for (int i = 0; i < 16; i++) {
		glyph_table->set_column_title(i + 1, String::num_int64(i, 16));
	}
	glyph_table->add_theme_style_override("selected", glyph_table->get_theme_stylebox(SceneStringName(panel)));
	glyph_table->add_theme_style_override("selected_focus", glyph_table->get_theme_stylebox(SceneStringName(panel)));
	glyph_table->add_theme_constant_override("h_separation", 0);
	glyph_table->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	glyph_table->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	glyphs_split->add_child(glyph_table);
	glyph_table->connect("item_activated", callable_mp(this, &DynamicFontImportSettingsDialog::_glyph_selected));

	glyph_tree = memnew(Tree);
	glyph_tree->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	glyph_tree->set_custom_minimum_size(Size2(300 * EDSCALE, 0));
	glyph_tree->set_columns(2);
	glyph_tree->set_hide_root(true);
	glyph_tree->set_column_expand(0, false);
	glyph_tree->set_column_expand(1, true);
	glyph_tree->set_column_custom_minimum_width(0, 120 * EDSCALE);
	glyph_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	glyph_root = glyph_tree->create_item();
	for (int i = 0; !unicode_ranges[i].name.is_empty(); i++) {
		_add_glyph_range_item(unicode_ranges[i].start, unicode_ranges[i].end, unicode_ranges[i].name);
	}
	glyphs_split->add_child(glyph_tree);
	glyph_tree->connect("item_activated", callable_mp(this, &DynamicFontImportSettingsDialog::_range_edited));
	glyph_tree->connect(SceneStringName(item_selected), callable_mp(this, &DynamicFontImportSettingsDialog::_range_selected));

	// Common

	import_settings_data.instantiate();
	import_settings_data->owner = this;

	set_ok_button_text(TTR("Reimport"));
	set_cancel_button_text(TTR("Close"));
}
