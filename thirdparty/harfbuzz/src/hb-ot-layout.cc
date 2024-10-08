/*
 * Copyright © 1998-2004  David Turner and Werner Lemberg
 * Copyright © 2006  Behdad Esfahbod
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2012,2013  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#include "hb.hh"

#ifndef HB_NO_OT_LAYOUT

#ifdef HB_NO_OT_TAG
#error "Cannot compile hb-ot-layout.cc with HB_NO_OT_TAG."
#endif

#include "hb-open-type.hh"
#include "hb-ot-layout.hh"
#include "hb-ot-face.hh"
#include "hb-ot-map.hh"
#include "hb-map.hh"

#include "hb-ot-kern-table.hh"
#include "hb-ot-layout-gdef-table.hh"
#include "hb-ot-layout-gsub-table.hh"
#include "hb-ot-layout-gpos-table.hh"
#include "hb-ot-layout-base-table.hh"
#include "hb-ot-layout-jstf-table.hh" // Just so we compile it; unused otherwise.
#include "hb-ot-name-table.hh"
#include "hb-ot-os2-table.hh"

#include "hb-aat-layout-morx-table.hh"
#include "hb-aat-layout-opbd-table.hh" // Just so we compile it; unused otherwise.

using OT::Layout::GSUB;
using OT::Layout::GPOS;

/**
 * SECTION:hb-ot-layout
 * @title: hb-ot-layout
 * @short_description: OpenType Layout
 * @include: hb-ot.h
 *
 * Functions for querying OpenType Layout features in the font face.
 * See the [OpenType specification](http://www.microsoft.com/typography/otspec/)
 * for details.
 **/


/*
 * kern
 */

#ifndef HB_NO_OT_KERN
/**
 * hb_ot_layout_has_kerning:
 * @face: The #hb_face_t to work on
 *
 * Tests whether a face includes any kerning data in the 'kern' table.
 * Does NOT test for kerning lookups in the GPOS table.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 **/
bool
hb_ot_layout_has_kerning (hb_face_t *face)
{
  return face->table.kern->table->has_data ();
}

/**
 * hb_ot_layout_has_machine_kerning:
 * @face: The #hb_face_t to work on
 *
 * Tests whether a face includes any state-machine kerning in the 'kern' table.
 * Does NOT examine the GPOS table.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 **/
bool
hb_ot_layout_has_machine_kerning (hb_face_t *face)
{
  return face->table.kern->table->has_state_machine ();
}

/**
 * hb_ot_layout_has_cross_kerning:
 * @face: The #hb_face_t to work on
 *
 * Tests whether a face has any cross-stream kerning (i.e., kerns
 * that make adjustments perpendicular to the direction of the text
 * flow: Y adjustments in horizontal text or X adjustments in
 * vertical text) in the 'kern' table.
 *
 * Does NOT examine the GPOS table.
 *
 * Return value: `true` is data found, `false` otherwise
 *
 **/
bool
hb_ot_layout_has_cross_kerning (hb_face_t *face)
{
  return face->table.kern->table->has_cross_stream ();
}

void
hb_ot_layout_kern (const hb_ot_shape_plan_t *plan,
		   hb_font_t *font,
		   hb_buffer_t  *buffer)
{
  hb_blob_t *blob = font->face->table.kern.get_blob ();
  const auto& kern = *font->face->table.kern;

  AAT::hb_aat_apply_context_t c (plan, font, buffer, blob);

  if (!buffer->message (font, "start table kern")) return;
  kern.apply (&c);
  (void) buffer->message (font, "end table kern");
}
#endif


/*
 * GDEF
 */

bool
OT::GDEF::is_blocklisted (hb_blob_t *blob,
			  hb_face_t *face) const
{
#ifdef HB_NO_OT_LAYOUT_BLOCKLIST
  return false;
#endif
  /* The ugly business of blocklisting individual fonts' tables happen here!
   * See this thread for why we finally had to bend in and do this:
   * https://lists.freedesktop.org/archives/harfbuzz/2016-February/005489.html
   *
   * In certain versions of Times New Roman Italic and Bold Italic,
   * ASCII double quotation mark U+0022 has wrong glyph class 3 (mark)
   * in GDEF.  Many versions of Tahoma have bad GDEF tables that
   * incorrectly classify some spacing marks such as certain IPA
   * symbols as glyph class 3. So do older versions of Microsoft
   * Himalaya, and the version of Cantarell shipped by Ubuntu 16.04.
   *
   * Nuke the GDEF tables of to avoid unwanted width-zeroing.
   *
   * See https://bugzilla.mozilla.org/show_bug.cgi?id=1279925
   *     https://bugzilla.mozilla.org/show_bug.cgi?id=1279693
   *     https://bugzilla.mozilla.org/show_bug.cgi?id=1279875
   */
  switch HB_CODEPOINT_ENCODE3(blob->length,
			      face->table.GSUB->table.get_length (),
			      face->table.GPOS->table.get_length ())
  {
    /* sha1sum:c5ee92f0bca4bfb7d06c4d03e8cf9f9cf75d2e8a Windows 7? timesi.ttf */
    case HB_CODEPOINT_ENCODE3 (442, 2874, 42038):
    /* sha1sum:37fc8c16a0894ab7b749e35579856c73c840867b Windows 7? timesbi.ttf */
    case HB_CODEPOINT_ENCODE3 (430, 2874, 40662):
    /* sha1sum:19fc45110ea6cd3cdd0a5faca256a3797a069a80 Windows 7 timesi.ttf */
    case HB_CODEPOINT_ENCODE3 (442, 2874, 39116):
    /* sha1sum:6d2d3c9ed5b7de87bc84eae0df95ee5232ecde26 Windows 7 timesbi.ttf */
    case HB_CODEPOINT_ENCODE3 (430, 2874, 39374):
    /* sha1sum:8583225a8b49667c077b3525333f84af08c6bcd8 OS X 10.11.3 Times New Roman Italic.ttf */
    case HB_CODEPOINT_ENCODE3 (490, 3046, 41638):
    /* sha1sum:ec0f5a8751845355b7c3271d11f9918a966cb8c9 OS X 10.11.3 Times New Roman Bold Italic.ttf */
    case HB_CODEPOINT_ENCODE3 (478, 3046, 41902):
    /* sha1sum:96eda93f7d33e79962451c6c39a6b51ee893ce8c  tahoma.ttf from Windows 8 */
    case HB_CODEPOINT_ENCODE3 (898, 12554, 46470):
    /* sha1sum:20928dc06014e0cd120b6fc942d0c3b1a46ac2bc  tahomabd.ttf from Windows 8 */
    case HB_CODEPOINT_ENCODE3 (910, 12566, 47732):
    /* sha1sum:4f95b7e4878f60fa3a39ca269618dfde9721a79e  tahoma.ttf from Windows 8.1 */
    case HB_CODEPOINT_ENCODE3 (928, 23298, 59332):
    /* sha1sum:6d400781948517c3c0441ba42acb309584b73033  tahomabd.ttf from Windows 8.1 */
    case HB_CODEPOINT_ENCODE3 (940, 23310, 60732):
    /* tahoma.ttf v6.04 from Windows 8.1 x64, see https://bugzilla.mozilla.org/show_bug.cgi?id=1279925 */
    case HB_CODEPOINT_ENCODE3 (964, 23836, 60072):
    /* tahomabd.ttf v6.04 from Windows 8.1 x64, see https://bugzilla.mozilla.org/show_bug.cgi?id=1279925 */
    case HB_CODEPOINT_ENCODE3 (976, 23832, 61456):
    /* sha1sum:e55fa2dfe957a9f7ec26be516a0e30b0c925f846  tahoma.ttf from Windows 10 */
    case HB_CODEPOINT_ENCODE3 (994, 24474, 60336):
    /* sha1sum:7199385abb4c2cc81c83a151a7599b6368e92343  tahomabd.ttf from Windows 10 */
    case HB_CODEPOINT_ENCODE3 (1006, 24470, 61740):
    /* tahoma.ttf v6.91 from Windows 10 x64, see https://bugzilla.mozilla.org/show_bug.cgi?id=1279925 */
    case HB_CODEPOINT_ENCODE3 (1006, 24576, 61346):
    /* tahomabd.ttf v6.91 from Windows 10 x64, see https://bugzilla.mozilla.org/show_bug.cgi?id=1279925 */
    case HB_CODEPOINT_ENCODE3 (1018, 24572, 62828):
    /* sha1sum:b9c84d820c49850d3d27ec498be93955b82772b5  tahoma.ttf from Windows 10 AU */
    case HB_CODEPOINT_ENCODE3 (1006, 24576, 61352):
    /* sha1sum:2bdfaab28174bdadd2f3d4200a30a7ae31db79d2  tahomabd.ttf from Windows 10 AU */
    case HB_CODEPOINT_ENCODE3 (1018, 24572, 62834):
    /* sha1sum:b0d36cf5a2fbe746a3dd277bffc6756a820807a7  Tahoma.ttf from Mac OS X 10.9 */
    case HB_CODEPOINT_ENCODE3 (832, 7324, 47162):
    /* sha1sum:12fc4538e84d461771b30c18b5eb6bd434e30fba  Tahoma Bold.ttf from Mac OS X 10.9 */
    case HB_CODEPOINT_ENCODE3 (844, 7302, 45474):
    /* sha1sum:eb8afadd28e9cf963e886b23a30b44ab4fd83acc  himalaya.ttf from Windows 7 */
    case HB_CODEPOINT_ENCODE3 (180, 13054, 7254):
    /* sha1sum:73da7f025b238a3f737aa1fde22577a6370f77b0  himalaya.ttf from Windows 8 */
    case HB_CODEPOINT_ENCODE3 (192, 12638, 7254):
    /* sha1sum:6e80fd1c0b059bbee49272401583160dc1e6a427  himalaya.ttf from Windows 8.1 */
    case HB_CODEPOINT_ENCODE3 (192, 12690, 7254):
    /* 8d9267aea9cd2c852ecfb9f12a6e834bfaeafe44  cantarell-fonts-0.0.21/otf/Cantarell-Regular.otf */
    /* 983988ff7b47439ab79aeaf9a45bd4a2c5b9d371  cantarell-fonts-0.0.21/otf/Cantarell-Oblique.otf */
    case HB_CODEPOINT_ENCODE3 (188, 248, 3852):
    /* 2c0c90c6f6087ffbfea76589c93113a9cbb0e75f  cantarell-fonts-0.0.21/otf/Cantarell-Bold.otf */
    /* 55461f5b853c6da88069ffcdf7f4dd3f8d7e3e6b  cantarell-fonts-0.0.21/otf/Cantarell-Bold-Oblique.otf */
    case HB_CODEPOINT_ENCODE3 (188, 264, 3426):
    /* d125afa82a77a6475ac0e74e7c207914af84b37a padauk-2.80/Padauk.ttf RHEL 7.2 */
    case HB_CODEPOINT_ENCODE3 (1058, 47032, 11818):
    /* 0f7b80437227b90a577cc078c0216160ae61b031 padauk-2.80/Padauk-Bold.ttf RHEL 7.2*/
    case HB_CODEPOINT_ENCODE3 (1046, 47030, 12600):
    /* d3dde9aa0a6b7f8f6a89ef1002e9aaa11b882290 padauk-2.80/Padauk.ttf Ubuntu 16.04 */
    case HB_CODEPOINT_ENCODE3 (1058, 71796, 16770):
    /* 5f3c98ccccae8a953be2d122c1b3a77fd805093f padauk-2.80/Padauk-Bold.ttf Ubuntu 16.04 */
    case HB_CODEPOINT_ENCODE3 (1046, 71790, 17862):
    /* 6c93b63b64e8b2c93f5e824e78caca555dc887c7 padauk-2.80/Padauk-book.ttf */
    case HB_CODEPOINT_ENCODE3 (1046, 71788, 17112):
    /* d89b1664058359b8ec82e35d3531931125991fb9 padauk-2.80/Padauk-bookbold.ttf */
    case HB_CODEPOINT_ENCODE3 (1058, 71794, 17514):
    /* 824cfd193aaf6234b2b4dc0cf3c6ef576c0d00ef padauk-3.0/Padauk-book.ttf */
    case HB_CODEPOINT_ENCODE3 (1330, 109904, 57938):
    /* 91fcc10cf15e012d27571e075b3b4dfe31754a8a padauk-3.0/Padauk-bookbold.ttf */
    case HB_CODEPOINT_ENCODE3 (1330, 109904, 58972):
    /* sha1sum: c26e41d567ed821bed997e937bc0c41435689e85  Padauk.ttf
     *  "Padauk Regular" "Version 2.5", see https://crbug.com/681813 */
    case HB_CODEPOINT_ENCODE3 (1004, 59092, 14836):
      return true;
  }
  return false;
}

static void
_hb_ot_layout_set_glyph_props (hb_font_t *font,
			       hb_buffer_t *buffer)
{
  _hb_buffer_assert_gsubgpos_vars (buffer);

  const auto &gdef = *font->face->table.GDEF;
  unsigned int count = buffer->len;
  hb_glyph_info_t *info = buffer->info;
  for (unsigned int i = 0; i < count; i++)
  {
    _hb_glyph_info_set_glyph_props (&info[i], gdef.get_glyph_props (info[i].codepoint));
    _hb_glyph_info_clear_lig_props (&info[i]);
  }
}

/* Public API */

/**
 * hb_ot_layout_has_glyph_classes:
 * @face: #hb_face_t to work upon
 *
 * Tests whether a face has any glyph classes defined in its GDEF table.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 **/
hb_bool_t
hb_ot_layout_has_glyph_classes (hb_face_t *face)
{
  return face->table.GDEF->table->has_glyph_classes ();
}

/**
 * hb_ot_layout_get_glyph_class:
 * @face: The #hb_face_t to work on
 * @glyph: The #hb_codepoint_t code point to query
 *
 * Fetches the GDEF class of the requested glyph in the specified face.
 *
 * Return value: The #hb_ot_layout_glyph_class_t glyph class of the given code
 * point in the GDEF table of the face.
 *
 * Since: 0.9.7
 **/
hb_ot_layout_glyph_class_t
hb_ot_layout_get_glyph_class (hb_face_t      *face,
			      hb_codepoint_t  glyph)
{
  return (hb_ot_layout_glyph_class_t) face->table.GDEF->table->get_glyph_class (glyph);
}

/**
 * hb_ot_layout_get_glyphs_in_class:
 * @face: The #hb_face_t to work on
 * @klass: The #hb_ot_layout_glyph_class_t GDEF class to retrieve
 * @glyphs: (out): The #hb_set_t set of all glyphs belonging to the requested
 *          class.
 *
 * Retrieves the set of all glyphs from the face that belong to the requested
 * glyph class in the face's GDEF table.
 *
 * Since: 0.9.7
 **/
void
hb_ot_layout_get_glyphs_in_class (hb_face_t                  *face,
				  hb_ot_layout_glyph_class_t  klass,
				  hb_set_t                   *glyphs /* OUT */)
{
  return face->table.GDEF->table->get_glyphs_in_class (klass, glyphs);
}

#ifndef HB_NO_LAYOUT_UNUSED
/**
 * hb_ot_layout_get_attach_points:
 * @face: The #hb_face_t to work on
 * @glyph: The #hb_codepoint_t code point to query
 * @start_offset: offset of the first attachment point to retrieve
 * @point_count: (inout) (optional): Input = the maximum number of attachment points to return;
 *               Output = the actual number of attachment points returned (may be zero)
 * @point_array: (out) (array length=point_count): The array of attachment points found for the query
 *
 * Fetches a list of all attachment points for the specified glyph in the GDEF
 * table of the face. The list returned will begin at the offset provided.
 *
 * Useful if the client program wishes to cache the list.
 *
 * Return value: Total number of attachment points for @glyph.
 *
 **/
unsigned int
hb_ot_layout_get_attach_points (hb_face_t      *face,
				hb_codepoint_t  glyph,
				unsigned int    start_offset,
				unsigned int   *point_count /* IN/OUT */,
				unsigned int   *point_array /* OUT */)
{
  return face->table.GDEF->table->get_attach_points (glyph,
						     start_offset,
						     point_count,
						     point_array);
}
/**
 * hb_ot_layout_get_ligature_carets:
 * @font: The #hb_font_t to work on
 * @direction: The #hb_direction_t text direction to use
 * @glyph: The #hb_codepoint_t code point to query
 * @start_offset: offset of the first caret position to retrieve
 * @caret_count: (inout) (optional): Input = the maximum number of caret positions to return;
 *               Output = the actual number of caret positions returned (may be zero)
 * @caret_array: (out) (array length=caret_count): The array of caret positions found for the query
 *
 * Fetches a list of the caret positions defined for a ligature glyph in the GDEF
 * table of the font. The list returned will begin at the offset provided.
 *
 * Note that a ligature that is formed from n characters will have n-1
 * caret positions. The first character is not represented in the array,
 * since its caret position is the glyph position.
 *
 * The positions returned by this function are 'unshaped', and will have to
 * be fixed up for kerning that may be applied to the ligature glyph.
 *
 * Return value: Total number of ligature caret positions for @glyph.
 *
 **/
unsigned int
hb_ot_layout_get_ligature_carets (hb_font_t      *font,
				  hb_direction_t  direction,
				  hb_codepoint_t  glyph,
				  unsigned int    start_offset,
				  unsigned int   *caret_count /* IN/OUT */,
				  hb_position_t  *caret_array /* OUT */)
{
  return font->face->table.GDEF->table->get_lig_carets (font, direction, glyph, start_offset, caret_count, caret_array);
}
#endif


/*
 * GSUB/GPOS
 */

bool
GSUB::is_blocklisted (hb_blob_t *blob HB_UNUSED,
			  hb_face_t *face) const
{
#ifdef HB_NO_OT_LAYOUT_BLOCKLIST
  return false;
#endif
  return false;
}

bool
GPOS::is_blocklisted (hb_blob_t *blob HB_UNUSED,
			  hb_face_t *face HB_UNUSED) const
{
#ifdef HB_NO_OT_LAYOUT_BLOCKLIST
  return false;
#endif
  return false;
}

static const OT::GSUBGPOS&
get_gsubgpos_table (hb_face_t *face,
		    hb_tag_t   table_tag)
{
  switch (table_tag) {
    case HB_OT_TAG_GSUB: return *face->table.GSUB->table;
    case HB_OT_TAG_GPOS: return *face->table.GPOS->table;
    default:             return Null (OT::GSUBGPOS);
  }
}


/**
 * hb_ot_layout_table_get_script_tags:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @start_offset: offset of the first script tag to retrieve
 * @script_count: (inout) (optional): Input = the maximum number of script tags to return;
 *                Output = the actual number of script tags returned (may be zero)
 * @script_tags: (out) (array length=script_count): The array of #hb_tag_t script tags found for the query
 *
 * Fetches a list of all scripts enumerated in the specified face's GSUB table
 * or GPOS table. The list returned will begin at the offset provided.
 *
 * Return value: Total number of script tags.
 *
 **/
unsigned int
hb_ot_layout_table_get_script_tags (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  start_offset,
				    unsigned int *script_count /* IN/OUT */,
				    hb_tag_t     *script_tags  /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  return g.get_script_tags (start_offset, script_count, script_tags);
}

#define HB_OT_TAG_LATIN_SCRIPT		HB_TAG ('l', 'a', 't', 'n')

/**
 * hb_ot_layout_table_find_script:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_tag: #hb_tag_t of the script tag requested
 * @script_index: (out): The index of the requested script tag
 *
 * Fetches the index if a given script tag in the specified face's GSUB table
 * or GPOS table.
 *
 * Return value: `true` if the script is found, `false` otherwise
 *
 **/
hb_bool_t
hb_ot_layout_table_find_script (hb_face_t    *face,
				hb_tag_t      table_tag,
				hb_tag_t      script_tag,
				unsigned int *script_index /* OUT */)
{
  static_assert ((OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX), "");
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  if (g.find_script_index (script_tag, script_index))
    return true;

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_SCRIPT, script_index))
    return false;

  /* try with 'dflt'; MS site has had typos and many fonts use it now :(.
   * including many versions of DejaVu Sans Mono! */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_LANGUAGE, script_index))
    return false;

  /* try with 'latn'; some old fonts put their features there even though
     they're really trying to support Thai, for example :( */
  if (g.find_script_index (HB_OT_TAG_LATIN_SCRIPT, script_index))
    return false;

  if (script_index) *script_index = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  return false;
}

#ifndef HB_DISABLE_DEPRECATED
/**
 * hb_ot_layout_table_choose_script:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_tags: Array of #hb_tag_t script tags
 * @script_index: (out): The index of the chosen script
 * @chosen_script: (out): #hb_tag_t of the chosen script
 *
 * Deprecated since 2.0.0
 **/
hb_bool_t
hb_ot_layout_table_choose_script (hb_face_t      *face,
				  hb_tag_t        table_tag,
				  const hb_tag_t *script_tags,
				  unsigned int   *script_index  /* OUT */,
				  hb_tag_t       *chosen_script /* OUT */)
{
  const hb_tag_t *t;
  for (t = script_tags; *t; t++);
  return hb_ot_layout_table_select_script (face, table_tag, t - script_tags, script_tags, script_index, chosen_script);
}
#endif

/**
 * hb_ot_layout_table_select_script:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_count: Number of script tags in the array
 * @script_tags: Array of #hb_tag_t script tags
 * @script_index: (out) (optional): The index of the requested script
 * @chosen_script: (out) (optional): #hb_tag_t of the requested script
 *
 * Selects an OpenType script for @table_tag from the @script_tags array.
 *
 * If the table does not have any of the requested scripts, then `DFLT`,
 * `dflt`, and `latn` tags are tried in that order. If the table still does not
 * have any of these scripts, @script_index is set to
 * #HB_OT_LAYOUT_NO_SCRIPT_INDEX and @chosen_script is set to #HB_TAG_NONE.
 *
 * Return value:
 * `true` if one of the requested scripts is selected, `false` if a fallback
 * script is selected or if no scripts are selected.
 *
 * Since: 2.0.0
 **/
hb_bool_t
hb_ot_layout_table_select_script (hb_face_t      *face,
				  hb_tag_t        table_tag,
				  unsigned int    script_count,
				  const hb_tag_t *script_tags,
				  unsigned int   *script_index  /* OUT */,
				  hb_tag_t       *chosen_script /* OUT */)
{
  static_assert ((OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_SCRIPT_INDEX), "");
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  unsigned int i;

  for (i = 0; i < script_count; i++)
  {
    if (g.find_script_index (script_tags[i], script_index))
    {
      if (chosen_script)
	*chosen_script = script_tags[i];
      return true;
    }
  }

  /* try finding 'DFLT' */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_SCRIPT, script_index)) {
    if (chosen_script)
      *chosen_script = HB_OT_TAG_DEFAULT_SCRIPT;
    return false;
  }

  /* try with 'dflt'; MS site has had typos and many fonts use it now :( */
  if (g.find_script_index (HB_OT_TAG_DEFAULT_LANGUAGE, script_index)) {
    if (chosen_script)
      *chosen_script = HB_OT_TAG_DEFAULT_LANGUAGE;
    return false;
  }

  /* try with 'latn'; some old fonts put their features there even though
     they're really trying to support Thai, for example :( */
  if (g.find_script_index (HB_OT_TAG_LATIN_SCRIPT, script_index)) {
    if (chosen_script)
      *chosen_script = HB_OT_TAG_LATIN_SCRIPT;
    return false;
  }

  if (script_index) *script_index = HB_OT_LAYOUT_NO_SCRIPT_INDEX;
  if (chosen_script)
    *chosen_script = HB_TAG_NONE;
  return false;
}


/**
 * hb_ot_layout_table_get_feature_tags:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @start_offset: offset of the first feature tag to retrieve
 * @feature_count: (inout) (optional): Input = the maximum number of feature tags to return;
 *                 Output = the actual number of feature tags returned (may be zero)
 * @feature_tags: (out) (array length=feature_count): Array of feature tags found in the table
 *
 * Fetches a list of all feature tags in the given face's GSUB or GPOS table.
 * Note that there might be duplicate feature tags, belonging to different
 * script/language-system pairs of the table.
 *
 * Return value: Total number of feature tags.
 *
 * Since: 0.6.0
 *
 **/
unsigned int
hb_ot_layout_table_get_feature_tags (hb_face_t    *face,
				     hb_tag_t      table_tag,
				     unsigned int  start_offset,
				     unsigned int *feature_count /* IN/OUT */,
				     hb_tag_t     *feature_tags  /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  return g.get_feature_tags (start_offset, feature_count, feature_tags);
}


/**
 * hb_ot_layout_table_find_feature:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @feature_tag: The #hb_tag_t of the requested feature tag
 * @feature_index: (out): The index of the requested feature
 *
 * Fetches the index for a given feature tag in the specified face's GSUB table
 * or GPOS table.
 *
 * Return value: `true` if the feature is found, `false` otherwise
 *
 * Since: 0.6.0
 *
 **/
bool
hb_ot_layout_table_find_feature (hb_face_t    *face,
				 hb_tag_t      table_tag,
				 hb_tag_t      feature_tag,
				 unsigned int *feature_index /* OUT */)
{
  static_assert ((OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_FEATURE_INDEX), "");
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  unsigned int num_features = g.get_feature_count ();
  for (unsigned int i = 0; i < num_features; i++)
  {
    if (feature_tag == g.get_feature_tag (i)) {
      if (feature_index) *feature_index = i;
      return true;
    }
  }

  if (feature_index) *feature_index = HB_OT_LAYOUT_NO_FEATURE_INDEX;
  return false;
}


/**
 * hb_ot_layout_script_get_language_tags:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @start_offset: offset of the first language tag to retrieve
 * @language_count: (inout) (optional): Input = the maximum number of language tags to return;
 *                  Output = the actual number of language tags returned (may be zero)
 * @language_tags: (out) (array length=language_count): Array of language tags found in the table
 *
 * Fetches a list of language tags in the given face's GSUB or GPOS table, underneath
 * the specified script index. The list returned will begin at the offset provided.
 *
 * Return value: Total number of language tags.
 *
 * Since: 0.6.0
 *
 **/
unsigned int
hb_ot_layout_script_get_language_tags (hb_face_t    *face,
				       hb_tag_t      table_tag,
				       unsigned int  script_index,
				       unsigned int  start_offset,
				       unsigned int *language_count /* IN/OUT */,
				       hb_tag_t     *language_tags  /* OUT */)
{
  const OT::Script &s = get_gsubgpos_table (face, table_tag).get_script (script_index);

  return s.get_lang_sys_tags (start_offset, language_count, language_tags);
}


#ifndef HB_DISABLE_DEPRECATED
/**
 * hb_ot_layout_script_find_language:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_tag: The #hb_tag_t of the requested language
 * @language_index: The index of the requested language
 *
 * Fetches the index of a given language tag in the specified face's GSUB table
 * or GPOS table, underneath the specified script tag.
 *
 * Return value: `true` if the language tag is found, `false` otherwise
 *
 * Since: 0.6.0
 * Deprecated: 2.0.0
 **/
hb_bool_t
hb_ot_layout_script_find_language (hb_face_t    *face,
				   hb_tag_t      table_tag,
				   unsigned int  script_index,
				   hb_tag_t      language_tag,
				   unsigned int *language_index)
{
  return hb_ot_layout_script_select_language (face,
					      table_tag,
					      script_index,
					      1,
					      &language_tag,
					      language_index);
}
#endif


/**
 * hb_ot_layout_script_select_language2:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_count: The number of languages in the specified script
 * @language_tags: The array of language tags
 * @language_index: (out): The index of the chosen language
 * @chosen_language: (out): #hb_tag_t of the chosen language
 *
 * Fetches the index of the first language tag fom @language_tags that is present
 * in the specified face's GSUB or GPOS table, underneath the specified script
 * index.
 *
 * If none of the given language tags is found, `false` is returned and
 * @language_index is set to #HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX and
 * @chosen_language is set to #HB_TAG_NONE.
 *
 * Return value: `true` if one of the given language tags is found, `false` otherwise
 *
 * Since: 7.0.0
 **/
hb_bool_t
hb_ot_layout_script_select_language2 (hb_face_t      *face,
				     hb_tag_t        table_tag,
				     unsigned int    script_index,
				     unsigned int    language_count,
				     const hb_tag_t *language_tags,
				     unsigned int   *language_index /* OUT */,
				     hb_tag_t       *chosen_language /* OUT */)
{
  static_assert ((OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX), "");
  const OT::Script &s = get_gsubgpos_table (face, table_tag).get_script (script_index);
  unsigned int i;

  for (i = 0; i < language_count; i++)
  {
    if (s.find_lang_sys_index (language_tags[i], language_index))
    {
      if (chosen_language)
        *chosen_language = language_tags[i];
      return true;
    }
  }

  /* try finding 'dflt' */
  if (s.find_lang_sys_index (HB_OT_TAG_DEFAULT_LANGUAGE, language_index))
  {
    if (chosen_language)
      *chosen_language = HB_OT_TAG_DEFAULT_LANGUAGE;
    return false;
  }

  if (language_index)
    *language_index = HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX;
  if (chosen_language)
    *chosen_language = HB_TAG_NONE;
  return false;
}

/**
 * hb_ot_layout_script_select_language:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_count: The number of languages in the specified script
 * @language_tags: The array of language tags
 * @language_index: (out): The index of the requested language
 *
 * Fetches the index of the first language tag fom @language_tags that is present
 * in the specified face's GSUB or GPOS table, underneath the specified script
 * index.
 *
 * If none of the given language tags is found, `false` is returned and
 * @language_index is set to the default language index.
 *
 * Return value: `true` if one of the given language tags is found, `false` otherwise
 *
 * Since: 2.0.0
 **/
hb_bool_t
hb_ot_layout_script_select_language (hb_face_t      *face,
				     hb_tag_t        table_tag,
				     unsigned int    script_index,
				     unsigned int    language_count,
				     const hb_tag_t *language_tags,
				     unsigned int   *language_index /* OUT */)
{
  return hb_ot_layout_script_select_language2 (face, table_tag,
					       script_index,
					       language_count, language_tags,
					       language_index, nullptr);
}

/**
 * hb_ot_layout_language_get_required_feature_index:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_index: The index of the requested language tag
 * @feature_index: (out): The index of the requested feature
 *
 * Fetches the index of a requested feature in the given face's GSUB or GPOS table,
 * underneath the specified script and language.
 *
 * Return value: `true` if the feature is found, `false` otherwise
 *
 * Since: 0.6.0
 *
 **/
hb_bool_t
hb_ot_layout_language_get_required_feature_index (hb_face_t    *face,
						  hb_tag_t      table_tag,
						  unsigned int  script_index,
						  unsigned int  language_index,
						  unsigned int *feature_index /* OUT */)
{
  return hb_ot_layout_language_get_required_feature (face,
						     table_tag,
						     script_index,
						     language_index,
						     feature_index,
						     nullptr);
}


/**
 * hb_ot_layout_language_get_required_feature:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_index: The index of the requested language tag
 * @feature_index: (out): The index of the requested feature
 * @feature_tag: (out): The #hb_tag_t of the requested feature
 *
 * Fetches the tag of a requested feature index in the given face's GSUB or GPOS table,
 * underneath the specified script and language.
 *
 * Return value: `true` if the feature is found, `false` otherwise
 *
 * Since: 0.9.30
 **/
hb_bool_t
hb_ot_layout_language_get_required_feature (hb_face_t    *face,
					    hb_tag_t      table_tag,
					    unsigned int  script_index,
					    unsigned int  language_index,
					    unsigned int *feature_index /* OUT */,
					    hb_tag_t     *feature_tag   /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  unsigned int index = l.get_required_feature_index ();
  if (feature_index) *feature_index = index;
  if (feature_tag) *feature_tag = g.get_feature_tag (index);

  return l.has_required_feature ();
}


/**
 * hb_ot_layout_language_get_feature_indexes:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_index: The index of the requested language tag
 * @start_offset: offset of the first feature tag to retrieve
 * @feature_count: (inout) (optional): Input = the maximum number of feature tags to return;
 *                 Output: the actual number of feature tags returned (may be zero)
 * @feature_indexes: (out) (array length=feature_count): The array of feature indexes found for the query
 *
 * Fetches a list of all features in the specified face's GSUB table
 * or GPOS table, underneath the specified script and language. The list
 * returned will begin at the offset provided.
 *
 * Return value: Total number of features.
 *
 * Since: 0.6.0
 *
 **/
unsigned int
hb_ot_layout_language_get_feature_indexes (hb_face_t    *face,
					   hb_tag_t      table_tag,
					   unsigned int  script_index,
					   unsigned int  language_index,
					   unsigned int  start_offset,
					   unsigned int *feature_count   /* IN/OUT */,
					   unsigned int *feature_indexes /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  return l.get_feature_indexes (start_offset, feature_count, feature_indexes);
}


/**
 * hb_ot_layout_language_get_feature_tags:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_index: The index of the requested language tag
 * @start_offset: offset of the first feature tag to retrieve
 * @feature_count: (inout) (optional): Input = the maximum number of feature tags to return;
 *                 Output = the actual number of feature tags returned (may be zero)
 * @feature_tags: (out) (array length=feature_count): The array of #hb_tag_t feature tags found for the query
 *
 * Fetches a list of all features in the specified face's GSUB table
 * or GPOS table, underneath the specified script and language. The list
 * returned will begin at the offset provided.
 *
 * Return value: Total number of feature tags.
 *
 * Since: 0.6.0
 *
 **/
unsigned int
hb_ot_layout_language_get_feature_tags (hb_face_t    *face,
					hb_tag_t      table_tag,
					unsigned int  script_index,
					unsigned int  language_index,
					unsigned int  start_offset,
					unsigned int *feature_count /* IN/OUT */,
					hb_tag_t     *feature_tags  /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  static_assert ((sizeof (unsigned int) == sizeof (hb_tag_t)), "");
  unsigned int ret = l.get_feature_indexes (start_offset, feature_count, (unsigned int *) feature_tags);

  if (feature_tags) {
    unsigned int count = *feature_count;
    for (unsigned int i = 0; i < count; i++)
      feature_tags[i] = g.get_feature_tag ((unsigned int) feature_tags[i]);
  }

  return ret;
}


/**
 * hb_ot_layout_language_find_feature:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_index: The index of the requested language tag
 * @feature_tag: #hb_tag_t of the feature tag requested
 * @feature_index: (out): The index of the requested feature
 *
 * Fetches the index of a given feature tag in the specified face's GSUB table
 * or GPOS table, underneath the specified script and language.
 *
 * Return value: `true` if the feature is found, `false` otherwise
 *
 * Since: 0.6.0
 *
 **/
hb_bool_t
hb_ot_layout_language_find_feature (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  script_index,
				    unsigned int  language_index,
				    hb_tag_t      feature_tag,
				    unsigned int *feature_index /* OUT */)
{
  static_assert ((OT::Index::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_FEATURE_INDEX), "");
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  unsigned int num_features = l.get_feature_count ();
  for (unsigned int i = 0; i < num_features; i++) {
    unsigned int f_index = l.get_feature_index (i);

    if (feature_tag == g.get_feature_tag (f_index)) {
      if (feature_index) *feature_index = f_index;
      return true;
    }
  }

  if (feature_index) *feature_index = HB_OT_LAYOUT_NO_FEATURE_INDEX;
  return false;
}


/**
 * hb_ot_layout_feature_get_lookups:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @feature_index: The index of the requested feature
 * @start_offset: offset of the first lookup to retrieve
 * @lookup_count: (inout) (optional): Input = the maximum number of lookups to return;
 *                Output = the actual number of lookups returned (may be zero)
 * @lookup_indexes: (out) (array length=lookup_count): The array of lookup indexes found for the query
 *
 * Fetches a list of all lookups enumerated for the specified feature, in
 * the specified face's GSUB table or GPOS table. The list returned will
 * begin at the offset provided.
 *
 * Return value: Total number of lookups.
 *
 * Since: 0.9.7
 **/
unsigned int
hb_ot_layout_feature_get_lookups (hb_face_t    *face,
				  hb_tag_t      table_tag,
				  unsigned int  feature_index,
				  unsigned int  start_offset,
				  unsigned int *lookup_count   /* IN/OUT */,
				  unsigned int *lookup_indexes /* OUT */)
{
  return hb_ot_layout_feature_with_variations_get_lookups (face,
							   table_tag,
							   feature_index,
							   HB_OT_LAYOUT_NO_VARIATIONS_INDEX,
							   start_offset,
							   lookup_count,
							   lookup_indexes);
}


/**
 * hb_ot_layout_table_get_lookup_count:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 *
 * Fetches the total number of lookups enumerated in the specified
 * face's GSUB table or GPOS table.
 *
 * Return value: Total number of lookups.
 *
 * Since: 0.9.22
 **/
unsigned int
hb_ot_layout_table_get_lookup_count (hb_face_t    *face,
				     hb_tag_t      table_tag)
{
  return get_gsubgpos_table (face, table_tag).get_lookup_count ();
}


struct hb_collect_features_context_t
{
  hb_collect_features_context_t (hb_face_t *face,
				 hb_tag_t   table_tag,
				 hb_set_t  *feature_indices_,
				 const hb_tag_t *features)

    : g (get_gsubgpos_table (face, table_tag)),
      feature_indices (feature_indices_),
      has_feature_filter (false),
      script_count (0),langsys_count (0), feature_index_count (0)
  {
    compute_feature_filter (features);
  }

  void compute_feature_filter (const hb_tag_t *features)
  {
    if (features == nullptr)
    {
      has_feature_filter = false;
      return;
    }

    has_feature_filter = true;
    hb_set_t features_set;
    for (; *features; features++)
      features_set.add (*features);

    for (unsigned i = 0; i < g.get_feature_count (); i++)
    {
      hb_tag_t tag = g.get_feature_tag (i);
      if (features_set.has (tag))
	feature_indices_filter.add(i);
    }
  }

  bool visited (const OT::Script &s)
  {
    /* We might have Null() object here.  Don't want to involve
     * that in the memoize.  So, detect empty objects and return. */
    if (unlikely (!s.has_default_lang_sys () &&
		  !s.get_lang_sys_count ()))
      return true;

    if (script_count++ > HB_MAX_SCRIPTS)
      return true;

    return visited (s, visited_script);
  }
  bool visited (const OT::LangSys &l)
  {
    /* We might have Null() object here.  Don't want to involve
     * that in the memoize.  So, detect empty objects and return. */
    if (unlikely (!l.has_required_feature () &&
		  !l.get_feature_count ()))
      return true;

    if (langsys_count++ > HB_MAX_LANGSYS)
      return true;

    return visited (l, visited_langsys);
  }

  bool visited_feature_indices (unsigned count)
  {
    feature_index_count += count;
    return feature_index_count > HB_MAX_FEATURE_INDICES;
  }

  private:
  template <typename T>
  bool visited (const T &p, hb_set_t &visited_set)
  {
    hb_codepoint_t delta = (hb_codepoint_t) ((uintptr_t) &p - (uintptr_t) &g);
     if (visited_set.has (delta))
      return true;

    visited_set.add (delta);
    return false;
  }

  public:
  const OT::GSUBGPOS &g;
  hb_set_t *feature_indices;
  hb_set_t  feature_indices_filter;
  bool has_feature_filter;

  private:
  hb_set_t visited_script;
  hb_set_t visited_langsys;
  unsigned int script_count;
  unsigned int langsys_count;
  unsigned int feature_index_count;
};

static void
langsys_collect_features (hb_collect_features_context_t *c,
			  const OT::LangSys  &l)
{
  if (c->visited (l)) return;

  if (!c->has_feature_filter)
  {
    /* All features. */
    if (l.has_required_feature () && !c->visited_feature_indices (1))
      c->feature_indices->add (l.get_required_feature_index ());

    // TODO(garretrieger): filter out indices >= feature count?
    if (!c->visited_feature_indices (l.featureIndex.len))
      l.add_feature_indexes_to (c->feature_indices);
  }
  else
  {
    if (c->feature_indices_filter.is_empty()) return;
    unsigned int num_features = l.get_feature_count ();
    for (unsigned int i = 0; i < num_features; i++)
    {
      unsigned int feature_index = l.get_feature_index (i);
      if (!c->feature_indices_filter.has (feature_index)) continue;

      c->feature_indices->add (feature_index);
      c->feature_indices_filter.del (feature_index);
    }
  }
}

static void
script_collect_features (hb_collect_features_context_t *c,
			 const OT::Script   &s,
			 const hb_tag_t *languages)
{
  if (c->visited (s)) return;

  if (!languages)
  {
    /* All languages. */
    if (s.has_default_lang_sys ())
      langsys_collect_features (c,
				s.get_default_lang_sys ());


    unsigned int count = s.get_lang_sys_count ();
    for (unsigned int language_index = 0; language_index < count; language_index++)
      langsys_collect_features (c,
				s.get_lang_sys (language_index));
  }
  else
  {
    for (; *languages; languages++)
    {
      unsigned int language_index;
      if (s.find_lang_sys_index (*languages, &language_index))
	langsys_collect_features (c,
				  s.get_lang_sys (language_index));

    }
  }
}


/**
 * hb_ot_layout_collect_features:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @scripts: (nullable) (array zero-terminated=1): The array of scripts to collect features for,
 *   terminated by %HB_TAG_NONE
 * @languages: (nullable) (array zero-terminated=1): The array of languages to collect features for,
 *   terminated by %HB_TAG_NONE
 * @features: (nullable) (array zero-terminated=1): The array of features to collect,
 *   terminated by %HB_TAG_NONE
 * @feature_indexes: (out): The set of feature indexes found for the query
 *
 * Fetches a list of all feature indexes in the specified face's GSUB table
 * or GPOS table, underneath the specified scripts, languages, and features.
 * If no list of scripts is provided, all scripts will be queried. If no list
 * of languages is provided, all languages will be queried. If no list of
 * features is provided, all features will be queried.
 *
 * Since: 1.8.5
 **/
void
hb_ot_layout_collect_features (hb_face_t      *face,
			       hb_tag_t        table_tag,
			       const hb_tag_t *scripts,
			       const hb_tag_t *languages,
			       const hb_tag_t *features,
			       hb_set_t       *feature_indexes /* OUT */)
{
  hb_collect_features_context_t c (face, table_tag, feature_indexes, features);
  if (!scripts)
  {
    /* All scripts. */
    unsigned int count = c.g.get_script_count ();
    for (unsigned int script_index = 0; script_index < count; script_index++)
      script_collect_features (&c,
			       c.g.get_script (script_index),
			       languages);
  }
  else
  {
    for (; *scripts; scripts++)
    {
      unsigned int script_index;
      if (c.g.find_script_index (*scripts, &script_index))
	script_collect_features (&c,
				 c.g.get_script (script_index),
				 languages);
    }
  }
}

/**
 * hb_ot_layout_collect_features_map:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @script_index: The index of the requested script tag
 * @language_index: The index of the requested language tag
 * @feature_map: (out): The map of feature tag to feature index.
 *
 * Fetches the mapping from feature tags to feature indexes for
 * the specified script and language.
 *
 * Since: 8.1.0
 **/
void
hb_ot_layout_collect_features_map (hb_face_t      *face,
				   hb_tag_t        table_tag,
				   unsigned        script_index,
				   unsigned        language_index,
				   hb_map_t       *feature_map /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::LangSys &l = g.get_script (script_index).get_lang_sys (language_index);

  unsigned int count = l.get_feature_indexes (0, nullptr, nullptr);
  feature_map->alloc (count);

  /* Loop in reverse, such that earlier entries win. That emulates
   * a linear search, which seems to be what other implementations do.
   * We found that with arialuni_t.ttf, the "ur" language system has
   * duplicate features, and the earlier ones work but not later ones.
   */
  for (unsigned int i = count; i; i--)
  {
    unsigned feature_index = 0;
    unsigned feature_count = 1;
    l.get_feature_indexes (i - 1, &feature_count, &feature_index);
    if (!feature_count)
      break;
    hb_tag_t feature_tag = g.get_feature_tag (feature_index);
    feature_map->set (feature_tag, feature_index);
  }
}


/**
 * hb_ot_layout_collect_lookups:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @scripts: (nullable) (array zero-terminated=1): The array of scripts to collect lookups for,
 *   terminated by %HB_TAG_NONE
 * @languages: (nullable) (array zero-terminated=1): The array of languages to collect lookups for,
 *   terminated by %HB_TAG_NONE
 * @features: (nullable) (array zero-terminated=1): The array of features to collect lookups for,
 *   terminated by %HB_TAG_NONE
 * @lookup_indexes: (out): The array of lookup indexes found for the query
 *
 * Fetches a list of all feature-lookup indexes in the specified face's GSUB
 * table or GPOS table, underneath the specified scripts, languages, and
 * features. If no list of scripts is provided, all scripts will be queried.
 * If no list of languages is provided, all languages will be queried. If no
 * list of features is provided, all features will be queried.
 *
 * Since: 0.9.8
 **/
void
hb_ot_layout_collect_lookups (hb_face_t      *face,
			      hb_tag_t        table_tag,
			      const hb_tag_t *scripts,
			      const hb_tag_t *languages,
			      const hb_tag_t *features,
			      hb_set_t       *lookup_indexes /* OUT */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  hb_set_t feature_indexes;
  hb_ot_layout_collect_features (face, table_tag, scripts, languages, features, &feature_indexes);

  for (auto feature_index : feature_indexes)
    g.get_feature (feature_index).add_lookup_indexes_to (lookup_indexes);

  g.feature_variation_collect_lookups (&feature_indexes, nullptr, lookup_indexes);
}


#ifndef HB_NO_LAYOUT_COLLECT_GLYPHS
/**
 * hb_ot_layout_lookup_collect_glyphs:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @lookup_index: The index of the feature lookup to query
 * @glyphs_before: (out): Array of glyphs preceding the substitution range
 * @glyphs_input: (out): Array of input glyphs that would be substituted by the lookup
 * @glyphs_after: (out): Array of glyphs following the substitution range
 * @glyphs_output: (out): Array of glyphs that would be the substituted output of the lookup
 *
 * Fetches a list of all glyphs affected by the specified lookup in the
 * specified face's GSUB table or GPOS table.
 *
 * Since: 0.9.7
 **/
void
hb_ot_layout_lookup_collect_glyphs (hb_face_t    *face,
				    hb_tag_t      table_tag,
				    unsigned int  lookup_index,
				    hb_set_t     *glyphs_before, /* OUT.  May be NULL */
				    hb_set_t     *glyphs_input,  /* OUT.  May be NULL */
				    hb_set_t     *glyphs_after,  /* OUT.  May be NULL */
				    hb_set_t     *glyphs_output  /* OUT.  May be NULL */)
{
  OT::hb_collect_glyphs_context_t c (face,
				     glyphs_before,
				     glyphs_input,
				     glyphs_after,
				     glyphs_output);

  switch (table_tag)
  {
    case HB_OT_TAG_GSUB:
    {
      const OT::SubstLookup& l = face->table.GSUB->table->get_lookup (lookup_index);
      l.collect_glyphs (&c);
      return;
    }
    case HB_OT_TAG_GPOS:
    {
      const OT::PosLookup& l = face->table.GPOS->table->get_lookup (lookup_index);
      l.collect_glyphs (&c);
      return;
    }
  }
}
#endif


/* Variations support */


/**
 * hb_ot_layout_table_find_feature_variations:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @coords: The variation coordinates to query
 * @num_coords: The number of variation coordinates
 * @variations_index: (out): The array of feature variations found for the query
 *
 * Fetches a list of feature variations in the specified face's GSUB table
 * or GPOS table, at the specified variation coordinates.
 *
 * Return value: `true` if feature variations were found, `false` otherwise.
 *
 * Since: 1.4.0
 *
 **/
hb_bool_t
hb_ot_layout_table_find_feature_variations (hb_face_t    *face,
					    hb_tag_t      table_tag,
					    const int    *coords,
					    unsigned int  num_coords,
					    unsigned int *variations_index /* out */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  const OT::GDEF &gdef = *face->table.GDEF->table;

  auto instancer = OT::ItemVarStoreInstancer(&gdef.get_var_store(), nullptr,
					     hb_array (coords, num_coords));

  return g.find_variations_index (coords, num_coords, variations_index, &instancer);
}


/**
 * hb_ot_layout_feature_with_variations_get_lookups:
 * @face: #hb_face_t to work upon
 * @table_tag: #HB_OT_TAG_GSUB or #HB_OT_TAG_GPOS
 * @feature_index: The index of the feature to query
 * @variations_index: The index of the feature variation to query
 * @start_offset: offset of the first lookup to retrieve
 * @lookup_count: (inout) (optional): Input = the maximum number of lookups to return;
 *                Output = the actual number of lookups returned (may be zero)
 * @lookup_indexes: (out) (array length=lookup_count): The array of lookups found for the query
 *
 * Fetches a list of all lookups enumerated for the specified feature, in
 * the specified face's GSUB table or GPOS table, enabled at the specified
 * variations index. The list returned will begin at the offset provided.
 *
 * Return value: Total number of lookups.
 *
 * Since: 1.4.0
 *
 **/
unsigned int
hb_ot_layout_feature_with_variations_get_lookups (hb_face_t    *face,
						  hb_tag_t      table_tag,
						  unsigned int  feature_index,
						  unsigned int  variations_index,
						  unsigned int  start_offset,
						  unsigned int *lookup_count /* IN/OUT */,
						  unsigned int *lookup_indexes /* OUT */)
{
  static_assert ((OT::FeatureVariations::NOT_FOUND_INDEX == HB_OT_LAYOUT_NO_VARIATIONS_INDEX), "");
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  const OT::Feature &f = g.get_feature_variation (feature_index, variations_index);

  return f.get_lookup_indexes (start_offset, lookup_count, lookup_indexes);
}


/*
 * OT::GSUB
 */


/**
 * hb_ot_layout_has_substitution:
 * @face: #hb_face_t to work upon
 *
 * Tests whether the specified face includes any GSUB substitutions.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.6.0
 *
 **/
hb_bool_t
hb_ot_layout_has_substitution (hb_face_t *face)
{
  return face->table.GSUB->table->has_data ();
}


/**
 * hb_ot_layout_lookup_would_substitute:
 * @face: #hb_face_t to work upon
 * @lookup_index: The index of the lookup to query
 * @glyphs: The sequence of glyphs to query for substitution
 * @glyphs_length: The length of the glyph sequence
 * @zero_context: #hb_bool_t indicating whether pre-/post-context are disallowed
 * in substitutions
 *
 * Tests whether a specified lookup in the specified face would
 * trigger a substitution on the given glyph sequence.
 *
 * Return value: `true` if a substitution would be triggered, `false` otherwise
 *
 * Since: 0.9.7
 **/
hb_bool_t
hb_ot_layout_lookup_would_substitute (hb_face_t            *face,
				      unsigned int          lookup_index,
				      const hb_codepoint_t *glyphs,
				      unsigned int          glyphs_length,
				      hb_bool_t             zero_context)
{
  auto &gsub = face->table.GSUB;
  if (unlikely (lookup_index >= gsub->lookup_count)) return false;
  OT::hb_would_apply_context_t c (face, glyphs, glyphs_length, (bool) zero_context);

  const OT::SubstLookup& l = gsub->table->get_lookup (lookup_index);
  auto *accel = gsub->get_accel (lookup_index);
  return accel && l.would_apply (&c, accel);
}


/**
 * hb_ot_layout_substitute_start:
 * @font: #hb_font_t to use
 * @buffer: #hb_buffer_t buffer to work upon
 *
 * Called before substitution lookups are performed, to ensure that glyph
 * class and other properties are set on the glyphs in the buffer.
 *
 **/
void
hb_ot_layout_substitute_start (hb_font_t    *font,
			       hb_buffer_t  *buffer)
{
  _hb_ot_layout_set_glyph_props (font, buffer);
}

/**
 * hb_ot_layout_lookup_substitute_closure:
 * @face: #hb_face_t to work upon
 * @lookup_index: index of the feature lookup to query
 * @glyphs: (out): Array of glyphs comprising the transitive closure of the lookup
 *
 * Compute the transitive closure of glyphs needed for a
 * specified lookup.
 *
 * Since: 0.9.7
 **/
void
hb_ot_layout_lookup_substitute_closure (hb_face_t    *face,
					unsigned int  lookup_index,
					hb_set_t     *glyphs /* OUT */)
{
  hb_map_t done_lookups_glyph_count;
  hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> done_lookups_glyph_set;
  OT::hb_closure_context_t c (face, glyphs, &done_lookups_glyph_count, &done_lookups_glyph_set);

  const OT::SubstLookup& l = face->table.GSUB->table->get_lookup (lookup_index);

  l.closure (&c, lookup_index);
}

/**
 * hb_ot_layout_lookups_substitute_closure:
 * @face: #hb_face_t to work upon
 * @lookups: The set of lookups to query
 * @glyphs: (out): Array of glyphs comprising the transitive closure of the lookups
 *
 * Compute the transitive closure of glyphs needed for all of the
 * provided lookups.
 *
 * Since: 1.8.1
 **/
void
hb_ot_layout_lookups_substitute_closure (hb_face_t      *face,
					 const hb_set_t *lookups,
					 hb_set_t       *glyphs /* OUT */)
{
  hb_map_t done_lookups_glyph_count;
  hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> done_lookups_glyph_set;
  OT::hb_closure_context_t c (face, glyphs, &done_lookups_glyph_count, &done_lookups_glyph_set);
  const GSUB& gsub = *face->table.GSUB->table;

  unsigned int iteration_count = 0;
  unsigned int glyphs_length;
  do
  {
    c.reset_lookup_visit_count ();
    glyphs_length = glyphs->get_population ();
    if (lookups)
    {
      for (auto lookup_index : *lookups)
	gsub.get_lookup (lookup_index).closure (&c, lookup_index);
    }
    else
    {
      for (unsigned int i = 0; i < gsub.get_lookup_count (); i++)
	gsub.get_lookup (i).closure (&c, i);
    }
  } while (iteration_count++ <= HB_CLOSURE_MAX_STAGES &&
	   glyphs_length != glyphs->get_population ());
}

/*
 * GPOS
 */


/**
 * hb_ot_layout_has_positioning:
 * @face: #hb_face_t to work upon
 *
 * Tests whether the specified face includes any GPOS positioning.
 *
 * Return value: `true` if the face has GPOS data, `false` otherwise
 *
 **/
hb_bool_t
hb_ot_layout_has_positioning (hb_face_t *face)
{
  return face->table.GPOS->table->has_data ();
}

/**
 * hb_ot_layout_position_start:
 * @font: #hb_font_t to use
 * @buffer: #hb_buffer_t buffer to work upon
 *
 * Called before positioning lookups are performed, to ensure that glyph
 * attachment types and glyph-attachment chains are set for the glyphs in the buffer.
 *
 **/
void
hb_ot_layout_position_start (hb_font_t *font, hb_buffer_t *buffer)
{
  GPOS::position_start (font, buffer);
}


/**
 * hb_ot_layout_position_finish_advances:
 * @font: #hb_font_t to use
 * @buffer: #hb_buffer_t buffer to work upon
 *
 * Called after positioning lookups are performed, to finish glyph advances.
 *
 **/
void
hb_ot_layout_position_finish_advances (hb_font_t *font, hb_buffer_t *buffer)
{
  GPOS::position_finish_advances (font, buffer);
}

/**
 * hb_ot_layout_position_finish_offsets:
 * @font: #hb_font_t to use
 * @buffer: #hb_buffer_t buffer to work upon
 *
 * Called after positioning lookups are performed, to finish glyph offsets.
 *
 **/
void
hb_ot_layout_position_finish_offsets (hb_font_t *font, hb_buffer_t *buffer)
{
  GPOS::position_finish_offsets (font, buffer);
}


#ifndef HB_NO_LAYOUT_FEATURE_PARAMS
/**
 * hb_ot_layout_get_size_params:
 * @face: #hb_face_t to work upon
 * @design_size: (out): The design size of the face
 * @subfamily_id: (out): The identifier of the face within the font subfamily
 * @subfamily_name_id: (out): The ‘name’ table name ID of the face within the font subfamily
 * @range_start: (out): The minimum size of the recommended size range for the face
 * @range_end: (out): The maximum size of the recommended size range for the face
 *
 * Fetches optical-size feature data (i.e., the `size` feature from GPOS). Note that
 * the subfamily_id and the subfamily name string (accessible via the subfamily_name_id)
 * as used here are defined as pertaining only to fonts within a font family that differ
 * specifically in their respective size ranges; other ways to differentiate fonts within
 * a subfamily are not covered by the `size` feature.
 *
 * For more information on this distinction, see the [`size` feature documentation](
 * https://docs.microsoft.com/en-us/typography/opentype/spec/features_pt#tag-size).
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.10
 **/
hb_bool_t
hb_ot_layout_get_size_params (hb_face_t       *face,
			      unsigned int    *design_size,       /* OUT.  May be NULL */
			      unsigned int    *subfamily_id,      /* OUT.  May be NULL */
			      hb_ot_name_id_t *subfamily_name_id, /* OUT.  May be NULL */
			      unsigned int    *range_start,       /* OUT.  May be NULL */
			      unsigned int    *range_end          /* OUT.  May be NULL */)
{
  const GPOS &gpos = *face->table.GPOS->table;
  const hb_tag_t tag = HB_TAG ('s','i','z','e');

  unsigned int num_features = gpos.get_feature_count ();
  for (unsigned int i = 0; i < num_features; i++)
  {
    if (tag == gpos.get_feature_tag (i))
    {
      const OT::Feature &f = gpos.get_feature (i);
      const OT::FeatureParamsSize &params = f.get_feature_params ().get_size_params (tag);

      if (params.designSize)
      {
	if (design_size) *design_size = params.designSize;
	if (subfamily_id) *subfamily_id = params.subfamilyID;
	if (subfamily_name_id) *subfamily_name_id = params.subfamilyNameID;
	if (range_start) *range_start = params.rangeStart;
	if (range_end) *range_end = params.rangeEnd;

	return true;
      }
    }
  }

  if (design_size) *design_size = 0;
  if (subfamily_id) *subfamily_id = 0;
  if (subfamily_name_id) *subfamily_name_id = HB_OT_NAME_ID_INVALID;
  if (range_start) *range_start = 0;
  if (range_end) *range_end = 0;

  return false;
}


/**
 * hb_ot_layout_feature_get_name_ids:
 * @face: #hb_face_t to work upon
 * @table_tag: table tag to query, "GSUB" or "GPOS".
 * @feature_index: index of feature to query.
 * @label_id: (out) (optional): The ‘name’ table name ID that specifies a string
 *            for a user-interface label for this feature. (May be NULL.)
 * @tooltip_id: (out) (optional): The ‘name’ table name ID that specifies a string
 *              that an application can use for tooltip text for this
 *              feature. (May be NULL.)
 * @sample_id: (out) (optional): The ‘name’ table name ID that specifies sample text
 *             that illustrates the effect of this feature. (May be NULL.)
 * @num_named_parameters: (out) (optional):  Number of named parameters. (May be zero.)
 * @first_param_id: (out) (optional): The first ‘name’ table name ID used to specify
 *                  strings for user-interface labels for the feature
 *                  parameters. (Must be zero if numParameters is zero.)
 *
 * Fetches name indices from feature parameters for "Stylistic Set" ('ssXX') or
 * "Character Variant" ('cvXX') features.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 2.0.0
 **/
hb_bool_t
hb_ot_layout_feature_get_name_ids (hb_face_t       *face,
				   hb_tag_t         table_tag,
				   unsigned int     feature_index,
				   hb_ot_name_id_t *label_id,             /* OUT.  May be NULL */
				   hb_ot_name_id_t *tooltip_id,           /* OUT.  May be NULL */
				   hb_ot_name_id_t *sample_id,            /* OUT.  May be NULL */
				   unsigned int    *num_named_parameters, /* OUT.  May be NULL */
				   hb_ot_name_id_t *first_param_id        /* OUT.  May be NULL */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);

  hb_tag_t feature_tag = g.get_feature_tag (feature_index);
  const OT::Feature &f = g.get_feature (feature_index);

  const OT::FeatureParams &feature_params = f.get_feature_params ();
  if (&feature_params != &Null (OT::FeatureParams))
  {
    const OT::FeatureParamsStylisticSet& ss_params =
      feature_params.get_stylistic_set_params (feature_tag);
    if (&ss_params != &Null (OT::FeatureParamsStylisticSet)) /* ssXX */
    {
      if (label_id) *label_id = ss_params.uiNameID;
      // ssXX features don't have the rest
      if (tooltip_id) *tooltip_id = HB_OT_NAME_ID_INVALID;
      if (sample_id) *sample_id = HB_OT_NAME_ID_INVALID;
      if (num_named_parameters) *num_named_parameters = 0;
      if (first_param_id) *first_param_id = HB_OT_NAME_ID_INVALID;
      return true;
    }
    const OT::FeatureParamsCharacterVariants& cv_params =
      feature_params.get_character_variants_params (feature_tag);
    if (&cv_params != &Null (OT::FeatureParamsCharacterVariants)) /* cvXX */
    {
      if (label_id) *label_id = cv_params.featUILableNameID;
      if (tooltip_id) *tooltip_id = cv_params.featUITooltipTextNameID;
      if (sample_id) *sample_id = cv_params.sampleTextNameID;
      if (num_named_parameters) *num_named_parameters = cv_params.numNamedParameters;
      if (first_param_id) *first_param_id = cv_params.firstParamUILabelNameID;
      return true;
    }
  }

  if (label_id) *label_id = HB_OT_NAME_ID_INVALID;
  if (tooltip_id) *tooltip_id = HB_OT_NAME_ID_INVALID;
  if (sample_id) *sample_id = HB_OT_NAME_ID_INVALID;
  if (num_named_parameters) *num_named_parameters = 0;
  if (first_param_id) *first_param_id = HB_OT_NAME_ID_INVALID;
  return false;
}
/**
 * hb_ot_layout_feature_get_characters:
 * @face: #hb_face_t to work upon
 * @table_tag: table tag to query, "GSUB" or "GPOS".
 * @feature_index: index of feature to query.
 * @start_offset: offset of the first character to retrieve
 * @char_count: (inout) (optional): Input = the maximum number of characters to return;
 *              Output = the actual number of characters returned (may be zero)
 * @characters: (out caller-allocates) (array length=char_count): A buffer pointer.
 *              The Unicode codepoints of the characters for which this feature provides
 *               glyph variants.
 *
 * Fetches a list of the characters defined as having a variant under the specified
 * "Character Variant" ("cvXX") feature tag.
 *
 * Return value: Number of total sample characters in the cvXX feature.
 *
 * Since: 2.0.0
 **/
unsigned int
hb_ot_layout_feature_get_characters (hb_face_t      *face,
				     hb_tag_t        table_tag,
				     unsigned int    feature_index,
				     unsigned int    start_offset,
				     unsigned int   *char_count, /* IN/OUT.  May be NULL */
				     hb_codepoint_t *characters  /* OUT.     May be NULL */)
{
  const OT::GSUBGPOS &g = get_gsubgpos_table (face, table_tag);
  return g.get_feature (feature_index)
	  .get_feature_params ()
	  .get_character_variants_params(g.get_feature_tag (feature_index))
	  .get_characters (start_offset, char_count, characters);
}
#endif


/*
 * Parts of different types are implemented here such that they have direct
 * access to GSUB/GPOS lookups.
 */


struct GSUBProxy
{
  static constexpr unsigned table_index = 0u;
  static constexpr bool always_inplace = false;
  typedef OT::SubstLookup Lookup;

  GSUBProxy (hb_face_t *face) :
    accel (*face->table.GSUB) {}

  const GSUB::accelerator_t &accel;
};

struct GPOSProxy
{
  static constexpr unsigned table_index = 1u;
  static constexpr bool always_inplace = true;
  typedef OT::PosLookup Lookup;

  GPOSProxy (hb_face_t *face) :
    accel (*face->table.GPOS) {}

  const GPOS::accelerator_t &accel;
};


static inline bool
apply_forward (OT::hb_ot_apply_context_t *c,
	       const OT::hb_ot_layout_lookup_accelerator_t &accel,
	       unsigned subtable_count)
{
  bool use_cache = accel.cache_enter (c);

  bool ret = false;
  hb_buffer_t *buffer = c->buffer;
  while (buffer->idx < buffer->len && buffer->successful)
  {
    bool applied = false;
    if (accel.digest.may_have (buffer->cur().codepoint) &&
	(buffer->cur().mask & c->lookup_mask) &&
	c->check_glyph_property (&buffer->cur(), c->lookup_props))
     {
       applied = accel.apply (c, subtable_count, use_cache);
     }

    if (applied)
      ret = true;
    else
      (void) buffer->next_glyph ();
  }

  if (use_cache)
    accel.cache_leave (c);

  return ret;
}

static inline bool
apply_backward (OT::hb_ot_apply_context_t *c,
	       const OT::hb_ot_layout_lookup_accelerator_t &accel,
	       unsigned subtable_count)
{
  bool ret = false;
  hb_buffer_t *buffer = c->buffer;
  do
  {
    if (accel.digest.may_have (buffer->cur().codepoint) &&
	(buffer->cur().mask & c->lookup_mask) &&
	c->check_glyph_property (&buffer->cur(), c->lookup_props))
      ret |= accel.apply (c, subtable_count, false);

    /* The reverse lookup doesn't "advance" cursor (for good reason). */
    buffer->idx--;

  }
  while ((int) buffer->idx >= 0);
  return ret;
}

template <typename Proxy>
static inline bool
apply_string (OT::hb_ot_apply_context_t *c,
	      const typename Proxy::Lookup &lookup,
	      const OT::hb_ot_layout_lookup_accelerator_t &accel)
{
  hb_buffer_t *buffer = c->buffer;
  unsigned subtable_count = lookup.get_subtable_count ();

  if (unlikely (!buffer->len || !c->lookup_mask))
    return false;

  bool ret = false;

  c->set_lookup_props (lookup.get_props ());

  if (likely (!lookup.is_reverse ()))
  {
    /* in/out forward substitution/positioning */
    if (!Proxy::always_inplace)
      buffer->clear_output ();

    buffer->idx = 0;
    ret = apply_forward (c, accel, subtable_count);

    if (!Proxy::always_inplace)
      buffer->sync ();
  }
  else
  {
    /* in-place backward substitution/positioning */
    assert (!buffer->have_output);
    buffer->idx = buffer->len - 1;
    ret = apply_backward (c, accel, subtable_count);
  }

  return ret;
}

template <typename Proxy>
inline void hb_ot_map_t::apply (const Proxy &proxy,
				const hb_ot_shape_plan_t *plan,
				hb_font_t *font,
				hb_buffer_t *buffer) const
{
  const unsigned int table_index = proxy.table_index;
  unsigned int i = 0;
  OT::hb_ot_apply_context_t c (table_index, font, buffer, proxy.accel.get_blob ());
  c.set_recurse_func (Proxy::Lookup::template dispatch_recurse_func<OT::hb_ot_apply_context_t>);

  for (unsigned int stage_index = 0; stage_index < stages[table_index].length; stage_index++)
  {
    const stage_map_t *stage = &stages[table_index][stage_index];
    for (; i < stage->last_lookup; i++)
    {
      auto &lookup = lookups[table_index][i];

      unsigned int lookup_index = lookup.index;

      auto *accel = proxy.accel.get_accel (lookup_index);
      if (unlikely (!accel)) continue;

      if (buffer->messaging () &&
	  !buffer->message (font, "start lookup %u feature '%c%c%c%c'", lookup_index, HB_UNTAG (lookup.feature_tag))) continue;

      /* c.digest is a digest of all the current glyphs in the buffer
       * (plus some past glyphs).
       *
       * Only try applying the lookup if there is any overlap. */
      if (accel->digest.may_have (c.digest))
      {
	c.set_lookup_index (lookup_index);
	c.set_lookup_mask (lookup.mask, false);
	c.set_auto_zwj (lookup.auto_zwj, false);
	c.set_auto_zwnj (lookup.auto_zwnj, false);
	c.set_random (lookup.random);
	c.set_per_syllable (lookup.per_syllable, false);
	/* apply_string's set_lookup_props initializes the iterators. */

	apply_string<Proxy> (&c,
			     proxy.accel.table->get_lookup (lookup_index),
			     *accel);
      }
      else if (buffer->messaging ())
	(void) buffer->message (font, "skipped lookup %u feature '%c%c%c%c' because no glyph matches", lookup_index, HB_UNTAG (lookup.feature_tag));

      if (buffer->messaging ())
	(void) buffer->message (font, "end lookup %u feature '%c%c%c%c'", lookup_index, HB_UNTAG (lookup.feature_tag));
    }

    if (stage->pause_func)
    {
      if (stage->pause_func (plan, font, buffer))
      {
	/* Refresh working buffer digest since buffer changed. */
	c.digest = buffer->digest ();
      }
    }
  }
}

void hb_ot_map_t::substitute (const hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer) const
{
  GSUBProxy proxy (font->face);
  if (buffer->messaging () &&
      !buffer->message (font, "start table GSUB script tag '%c%c%c%c'", HB_UNTAG (chosen_script[0]))) return;
  apply (proxy, plan, font, buffer);
  if (buffer->messaging ())
    (void) buffer->message (font, "end table GSUB script tag '%c%c%c%c'", HB_UNTAG (chosen_script[0]));
}

void hb_ot_map_t::position (const hb_ot_shape_plan_t *plan, hb_font_t *font, hb_buffer_t *buffer) const
{
  GPOSProxy proxy (font->face);
  if (buffer->messaging () &&
      !buffer->message (font, "start table GPOS script tag '%c%c%c%c'", HB_UNTAG (chosen_script[1]))) return;
  apply (proxy, plan, font, buffer);
  if (buffer->messaging ())
    (void) buffer->message (font, "end table GPOS script tag '%c%c%c%c'", HB_UNTAG (chosen_script[1]));
}

void
hb_ot_layout_substitute_lookup (OT::hb_ot_apply_context_t *c,
				const OT::SubstLookup &lookup,
				const OT::hb_ot_layout_lookup_accelerator_t &accel)
{
  apply_string<GSUBProxy> (c, lookup, accel);
}

#ifndef HB_NO_BASE

static void
choose_base_tags (hb_script_t    script,
		  hb_language_t  language,
		  hb_tag_t      *script_tag,
		  hb_tag_t      *language_tag)
{
  hb_tag_t script_tags[HB_OT_MAX_TAGS_PER_SCRIPT];
  unsigned script_count = ARRAY_LENGTH (script_tags);

  hb_tag_t language_tags[HB_OT_MAX_TAGS_PER_LANGUAGE];
  unsigned language_count = ARRAY_LENGTH (language_tags);

  hb_ot_tags_from_script_and_language (script, language,
				       &script_count, script_tags,
				       &language_count, language_tags);

  *script_tag = script_count ? script_tags[script_count - 1] : HB_OT_TAG_DEFAULT_SCRIPT;
  *language_tag = language_count ? language_tags[language_count - 1] : HB_OT_TAG_DEFAULT_LANGUAGE;
}

/**
 * hb_ot_layout_get_font_extents:
 * @font: a font
 * @direction: text direction.
 * @script_tag:  script tag.
 * @language_tag: language tag.
 * @extents: (out) (nullable): font extents if found.
 *
 * Fetches script/language-specific font extents.  These values are
 * looked up in the `BASE` table's `MinMax` records.
 *
 * If no such extents are found, the default extents for the font are
 * fetched. As such, the return value of this function can for the
 * most part be ignored.  Note that the per-script/language extents
 * do not have a line-gap value, and the line-gap is set to zero in
 * that case.
 *
 * Return value: `true` if found script/language-specific font extents.
 *
 * Since: 8.0.0
 **/
hb_bool_t
hb_ot_layout_get_font_extents (hb_font_t         *font,
			       hb_direction_t     direction,
			       hb_tag_t           script_tag,
			       hb_tag_t           language_tag,
			       hb_font_extents_t *extents)
{
  hb_position_t min = 0, max = 0;
  if (font->face->table.BASE->get_min_max (font, direction, script_tag, language_tag, HB_TAG_NONE,
					   &min, &max))
  {
    if (extents)
    {
      extents->ascender  = max;
      extents->descender = min;
      extents->line_gap  = 0;
    }
    return true;
  }

  hb_font_get_extents_for_direction (font, direction, extents);
  return false;
}

/**
 * hb_ot_layout_get_font_extents2:
 * @font: a font
 * @direction: text direction.
 * @script:  script.
 * @language: (nullable): language.
 * @extents: (out) (nullable): font extents if found.
 *
 * Fetches script/language-specific font extents.  These values are
 * looked up in the `BASE` table's `MinMax` records.
 *
 * If no such extents are found, the default extents for the font are
 * fetched. As such, the return value of this function can for the
 * most part be ignored.  Note that the per-script/language extents
 * do not have a line-gap value, and the line-gap is set to zero in
 * that case.
 *
 * This function is like hb_ot_layout_get_font_extents() but takes
 * #hb_script_t and #hb_language_t instead of OpenType #hb_tag_t.
 *
 * Return value: `true` if found script/language-specific font extents.
 *
 * Since: 8.0.0
 **/
hb_bool_t
hb_ot_layout_get_font_extents2 (hb_font_t         *font,
				hb_direction_t     direction,
				hb_script_t        script,
				hb_language_t      language,
				hb_font_extents_t *extents)
{
  hb_tag_t script_tag, language_tag;
  choose_base_tags (script, language, &script_tag, &language_tag);
  return hb_ot_layout_get_font_extents (font,
					direction,
					script_tag,
					language_tag,
					extents);
}

/**
 * hb_ot_layout_get_horizontal_baseline_tag_for_script:
 * @script: a script tag.
 *
 * Fetches the dominant horizontal baseline tag used by @script.
 *
 * Return value: dominant baseline tag for the @script.
 *
 * Since: 4.0.0
 **/
hb_ot_layout_baseline_tag_t
hb_ot_layout_get_horizontal_baseline_tag_for_script (hb_script_t script)
{
  /* Keep in sync with hb_ot_layout_get_baseline_with_fallback */
  switch ((int) script)
  {
    /* Unicode-1.1 additions */
    case HB_SCRIPT_BENGALI:
    case HB_SCRIPT_DEVANAGARI:
    case HB_SCRIPT_GUJARATI:
    case HB_SCRIPT_GURMUKHI:
    /* Unicode-2.0 additions */
    case HB_SCRIPT_TIBETAN:
    /* Unicode-4.0 additions */
    case HB_SCRIPT_LIMBU:
    /* Unicode-4.1 additions */
    case HB_SCRIPT_SYLOTI_NAGRI:
    /* Unicode-5.0 additions */
    case HB_SCRIPT_PHAGS_PA:
    /* Unicode-5.2 additions */
    case HB_SCRIPT_MEETEI_MAYEK:
    /* Unicode-6.1 additions */
    case HB_SCRIPT_SHARADA:
    case HB_SCRIPT_TAKRI:
    /* Unicode-7.0 additions */
    case HB_SCRIPT_MODI:
    case HB_SCRIPT_SIDDHAM:
    case HB_SCRIPT_TIRHUTA:
    /* Unicode-9.0 additions */
    case HB_SCRIPT_MARCHEN:
    case HB_SCRIPT_NEWA:
    /* Unicode-10.0 additions */
    case HB_SCRIPT_SOYOMBO:
    case HB_SCRIPT_ZANABAZAR_SQUARE:
    /* Unicode-11.0 additions */
    case HB_SCRIPT_DOGRA:
    case HB_SCRIPT_GUNJALA_GONDI:
    /* Unicode-12.0 additions */
    case HB_SCRIPT_NANDINAGARI:
      return HB_OT_LAYOUT_BASELINE_TAG_HANGING;

    /* Unicode-1.1 additions */
    case HB_SCRIPT_HANGUL:
    case HB_SCRIPT_HAN:
    case HB_SCRIPT_HIRAGANA:
    case HB_SCRIPT_KATAKANA:
    /* Unicode-3.0 additions */
    case HB_SCRIPT_BOPOMOFO:
    /* Unicode-9.0 additions */
    case HB_SCRIPT_TANGUT:
    /* Unicode-10.0 additions */
    case HB_SCRIPT_NUSHU:
    /* Unicode-13.0 additions */
    case HB_SCRIPT_KHITAN_SMALL_SCRIPT:
      return HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT;

    default:
      return HB_OT_LAYOUT_BASELINE_TAG_ROMAN;
  }
}

/**
 * hb_ot_layout_get_baseline:
 * @font: a font
 * @baseline_tag: a baseline tag
 * @direction: text direction.
 * @script_tag:  script tag.
 * @language_tag: language tag, currently unused.
 * @coord: (out) (nullable): baseline value if found.
 *
 * Fetches a baseline value from the face.
 *
 * Return value: `true` if found baseline value in the font.
 *
 * Since: 2.6.0
 **/
hb_bool_t
hb_ot_layout_get_baseline (hb_font_t                   *font,
			   hb_ot_layout_baseline_tag_t  baseline_tag,
			   hb_direction_t               direction,
			   hb_tag_t                     script_tag,
			   hb_tag_t                     language_tag,
			   hb_position_t               *coord        /* OUT.  May be NULL. */)
{
  return font->face->table.BASE->get_baseline (font, baseline_tag, direction, script_tag, language_tag, coord);
}

/**
 * hb_ot_layout_get_baseline2:
 * @font: a font
 * @baseline_tag: a baseline tag
 * @direction: text direction.
 * @script:  script.
 * @language: (nullable): language, currently unused.
 * @coord: (out) (nullable): baseline value if found.
 *
 * Fetches a baseline value from the face.
 *
 * This function is like hb_ot_layout_get_baseline() but takes
 * #hb_script_t and #hb_language_t instead of OpenType #hb_tag_t.
 *
 * Return value: `true` if found baseline value in the font.
 *
 * Since: 8.0.0
 **/
hb_bool_t
hb_ot_layout_get_baseline2 (hb_font_t                   *font,
			    hb_ot_layout_baseline_tag_t  baseline_tag,
			    hb_direction_t               direction,
			    hb_script_t                  script,
			    hb_language_t                language,
			    hb_position_t               *coord        /* OUT.  May be NULL. */)
{
  hb_tag_t script_tag, language_tag;
  choose_base_tags (script, language, &script_tag, &language_tag);
  return hb_ot_layout_get_baseline (font,
				    baseline_tag,
				    direction,
				    script_tag,
				    language_tag,
				    coord);
}

/**
 * hb_ot_layout_get_baseline_with_fallback:
 * @font: a font
 * @baseline_tag: a baseline tag
 * @direction: text direction.
 * @script_tag:  script tag.
 * @language_tag: language tag, currently unused.
 * @coord: (out): baseline value if found.
 *
 * Fetches a baseline value from the face, and synthesizes
 * it if the font does not have it.
 *
 * Since: 4.0.0
 **/
void
hb_ot_layout_get_baseline_with_fallback (hb_font_t                   *font,
					 hb_ot_layout_baseline_tag_t  baseline_tag,
					 hb_direction_t               direction,
					 hb_tag_t                     script_tag,
					 hb_tag_t                     language_tag,
					 hb_position_t               *coord /* OUT */)
{
  if (hb_ot_layout_get_baseline (font,
				 baseline_tag,
				 direction,
				 script_tag,
				 language_tag,
				 coord))
    return;

  /* Synthesize missing baselines.
   * See https://www.w3.org/TR/css-inline-3/#baseline-synthesis-fonts
   */
  switch (baseline_tag)
  {
  case HB_OT_LAYOUT_BASELINE_TAG_ROMAN:
    *coord = 0; // FIXME origin ?
    break;

  case HB_OT_LAYOUT_BASELINE_TAG_MATH:
    {
      hb_codepoint_t glyph;
      hb_glyph_extents_t extents;
      if (HB_DIRECTION_IS_HORIZONTAL (direction) &&
	  (hb_font_get_nominal_glyph (font, 0x2212u, &glyph) ||
	   hb_font_get_nominal_glyph (font, '-', &glyph)) &&
	  hb_font_get_glyph_extents (font, glyph, &extents))
      {
	*coord = extents.y_bearing + extents.height / 2;
      }
      else
      {
	hb_position_t x_height = font->y_scale / 2;
#ifndef HB_NO_METRICS
	hb_ot_metrics_get_position_with_fallback (font, HB_OT_METRICS_TAG_X_HEIGHT, &x_height);
#endif
	*coord = x_height / 2;
      }
    }
    break;

  case HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT:
  case HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT:
    {
      hb_position_t embox_top, embox_bottom;

      hb_ot_layout_get_baseline_with_fallback (font,
					       HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT,
					       direction,
					       script_tag,
					       language_tag,
					       &embox_top);
      hb_ot_layout_get_baseline_with_fallback (font,
					       HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
					       direction,
					       script_tag,
					       language_tag,
					       &embox_bottom);

      if (baseline_tag == HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT)
	*coord = embox_top + (embox_bottom - embox_top) / 10;
      else
	*coord = embox_bottom + (embox_top - embox_bottom) / 10;
    }
    break;

  case HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT:
    if (hb_ot_layout_get_baseline (font,
				   HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
				   direction,
				   script_tag,
				   language_tag,
				   coord))
      *coord += HB_DIRECTION_IS_HORIZONTAL (direction) ? font->y_scale : font->x_scale;
    else
    {
      hb_font_extents_t font_extents;
      hb_font_get_extents_for_direction (font, direction, &font_extents);
      *coord = font_extents.ascender;
    }
    break;

  case HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT:
    if (hb_ot_layout_get_baseline (font,
				   HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT,
				   direction,
				   script_tag,
				   language_tag,
				   coord))
      *coord -= HB_DIRECTION_IS_HORIZONTAL (direction) ? font->y_scale : font->x_scale;
    else
    {
      hb_font_extents_t font_extents;
      hb_font_get_extents_for_direction (font, direction, &font_extents);
      *coord = font_extents.descender;
    }
    break;

  case HB_OT_LAYOUT_BASELINE_TAG_HANGING:
    if (HB_DIRECTION_IS_HORIZONTAL (direction))
    {
      hb_codepoint_t ch;
      hb_codepoint_t glyph;
      hb_glyph_extents_t extents;

      /* Keep in sync with hb_ot_layout_get_horizontal_baseline_for_script */
      switch ((int) script_tag)
      {
      /* Unicode-1.1 additions */
      case HB_SCRIPT_BENGALI:          ch = 0x0995u; break;
      case HB_SCRIPT_DEVANAGARI:       ch = 0x0915u; break;
      case HB_SCRIPT_GUJARATI:         ch = 0x0a95u; break;
      case HB_SCRIPT_GURMUKHI:         ch = 0x0a15u; break;
      /* Unicode-2.0 additions */
      case HB_SCRIPT_TIBETAN:          ch = 0x0f40u; break;
      /* Unicode-4.0 additions */
      case HB_SCRIPT_LIMBU:            ch = 0x1901u; break;
      /* Unicode-4.1 additions */
      case HB_SCRIPT_SYLOTI_NAGRI:     ch = 0xa807u; break;
      /* Unicode-5.0 additions */
      case HB_SCRIPT_PHAGS_PA:         ch = 0xa840u; break;
      /* Unicode-5.2 additions */
      case HB_SCRIPT_MEETEI_MAYEK:     ch = 0xabc0u; break;
      /* Unicode-6.1 additions */
      case HB_SCRIPT_SHARADA:          ch = 0x11191u; break;
      case HB_SCRIPT_TAKRI:            ch = 0x1168cu; break;
      /* Unicode-7.0 additions */
      case HB_SCRIPT_MODI:             ch = 0x1160eu;break;
      case HB_SCRIPT_SIDDHAM:          ch = 0x11590u; break;
      case HB_SCRIPT_TIRHUTA:          ch = 0x1148fu; break;
      /* Unicode-9.0 additions */
      case HB_SCRIPT_MARCHEN:          ch = 0x11c72u; break;
      case HB_SCRIPT_NEWA:             ch = 0x1140eu; break;
      /* Unicode-10.0 additions */
      case HB_SCRIPT_SOYOMBO:          ch = 0x11a5cu; break;
      case HB_SCRIPT_ZANABAZAR_SQUARE: ch = 0x11a0bu; break;
      /* Unicode-11.0 additions */
      case HB_SCRIPT_DOGRA:            ch = 0x1180au; break;
      case HB_SCRIPT_GUNJALA_GONDI:    ch = 0x11d6cu; break;
      /* Unicode-12.0 additions */
      case HB_SCRIPT_NANDINAGARI:      ch = 0x119b0u; break;
      default:                         ch = 0;        break;
      }

      if (ch &&
	  hb_font_get_nominal_glyph (font, ch, &glyph) &&
	  hb_font_get_glyph_extents (font, glyph, &extents))
	*coord = extents.y_bearing;
      else
	*coord = font->y_scale * 6 / 10; // FIXME makes assumptions about origin
    }
    else
      *coord = font->x_scale * 6 / 10; // FIXME makes assumptions about origin
    break;

  case HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_CENTRAL:
    {
      hb_position_t top, bottom;
      hb_ot_layout_get_baseline_with_fallback (font,
					       HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_TOP_OR_RIGHT,
					       direction,
					       script_tag,
					       language_tag,
					       &top);
      hb_ot_layout_get_baseline_with_fallback (font,
					       HB_OT_LAYOUT_BASELINE_TAG_IDEO_EMBOX_BOTTOM_OR_LEFT,
					       direction,
					       script_tag,
					       language_tag,
					       &bottom);
      *coord = (top + bottom) / 2;

    }
    break;

  case HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_CENTRAL:
    {
      hb_position_t top, bottom;
      hb_ot_layout_get_baseline_with_fallback (font,
					       HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_TOP_OR_RIGHT,
					       direction,
					       script_tag,
					       language_tag,
					       &top);
      hb_ot_layout_get_baseline_with_fallback (font,
					       HB_OT_LAYOUT_BASELINE_TAG_IDEO_FACE_BOTTOM_OR_LEFT,
					       direction,
					       script_tag,
					       language_tag,
					       &bottom);
      *coord = (top + bottom) / 2;

    }
    break;

  case _HB_OT_LAYOUT_BASELINE_TAG_MAX_VALUE:
  default:
    *coord = 0;
    break;
  }
}

/**
 * hb_ot_layout_get_baseline_with_fallback2:
 * @font: a font
 * @baseline_tag: a baseline tag
 * @direction: text direction.
 * @script:  script.
 * @language: (nullable): language, currently unused.
 * @coord: (out): baseline value if found.
 *
 * Fetches a baseline value from the face, and synthesizes
 * it if the font does not have it.
 *
 * This function is like hb_ot_layout_get_baseline_with_fallback() but takes
 * #hb_script_t and #hb_language_t instead of OpenType #hb_tag_t.
 *
 * Since: 8.0.0
 **/
void
hb_ot_layout_get_baseline_with_fallback2 (hb_font_t                   *font,
					  hb_ot_layout_baseline_tag_t  baseline_tag,
					  hb_direction_t               direction,
					  hb_script_t                  script,
					  hb_language_t                language,
					  hb_position_t               *coord        /* OUT */)
{
  hb_tag_t script_tag, language_tag;
  choose_base_tags (script, language, &script_tag, &language_tag);
  hb_ot_layout_get_baseline_with_fallback (font,
					   baseline_tag,
					   direction,
					   script_tag,
					   language_tag,
					   coord);
}

#endif


struct hb_get_glyph_alternates_dispatch_t :
       hb_dispatch_context_t<hb_get_glyph_alternates_dispatch_t, unsigned>
{
  static return_t default_return_value () { return 0; }
  bool stop_sublookup_iteration (return_t r) const { return r; }

  private:
  template <typename T, typename ...Ts> auto
  _dispatch (const T &obj, hb_priority<1>, Ts&&... ds) HB_AUTO_RETURN
  ( obj.get_glyph_alternates (std::forward<Ts> (ds)...) )
  template <typename T, typename ...Ts> auto
  _dispatch (const T &obj, hb_priority<0>, Ts&&... ds) HB_AUTO_RETURN
  ( default_return_value () )
  public:
  template <typename T, typename ...Ts> auto
  dispatch (const T &obj, Ts&&... ds) HB_AUTO_RETURN
  ( _dispatch (obj, hb_prioritize, std::forward<Ts> (ds)...) )
};

#ifndef HB_NO_LAYOUT_RARELY_USED
/**
 * hb_ot_layout_lookup_get_glyph_alternates:
 * @face: a face.
 * @lookup_index: index of the feature lookup to query.
 * @glyph: a glyph id.
 * @start_offset: starting offset.
 * @alternate_count: (inout) (optional): Input = the maximum number of alternate glyphs to return;
 *                   Output = the actual number of alternate glyphs returned (may be zero).
 * @alternate_glyphs: (out caller-allocates) (array length=alternate_count): A glyphs buffer.
 *                    Alternate glyphs associated with the glyph id.
 *
 * Fetches alternates of a glyph from a given GSUB lookup index.
 *
 * Return value: Total number of alternates found in the specific lookup index for the given glyph id.
 *
 * Since: 2.6.8
 **/
HB_EXTERN unsigned
hb_ot_layout_lookup_get_glyph_alternates (hb_face_t      *face,
					  unsigned        lookup_index,
					  hb_codepoint_t  glyph,
					  unsigned        start_offset,
					  unsigned       *alternate_count  /* IN/OUT.  May be NULL. */,
					  hb_codepoint_t *alternate_glyphs /* OUT.     May be NULL. */)
{
  hb_get_glyph_alternates_dispatch_t c;
  const OT::SubstLookup &lookup = face->table.GSUB->table->get_lookup (lookup_index);
  auto ret = lookup.dispatch (&c, glyph, start_offset, alternate_count, alternate_glyphs);
  if (!ret && alternate_count) *alternate_count = 0;
  return ret;
}


struct hb_position_single_dispatch_t :
       hb_dispatch_context_t<hb_position_single_dispatch_t, bool>
{
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }

  private:
  template <typename T, typename ...Ts> auto
  _dispatch (const T &obj, hb_priority<1>, Ts&&... ds) HB_AUTO_RETURN
  ( obj.position_single (std::forward<Ts> (ds)...) )
  template <typename T, typename ...Ts> auto
  _dispatch (const T &obj, hb_priority<0>, Ts&&... ds) HB_AUTO_RETURN
  ( default_return_value () )
  public:
  template <typename T, typename ...Ts> auto
  dispatch (const T &obj, Ts&&... ds) HB_AUTO_RETURN
  ( _dispatch (obj, hb_prioritize, std::forward<Ts> (ds)...) )
};

/**
 * hb_ot_layout_lookup_get_optical_bound:
 * @font: a font.
 * @lookup_index: index of the feature lookup to query.
 * @direction: edge of the glyph to query.
 * @glyph: a glyph id.
 *
 * Fetches the optical bound of a glyph positioned at the margin of text.
 * The direction identifies which edge of the glyph to query.
 *
 * Return value: Adjustment value. Negative values mean the glyph will stick out of the margin.
 *
 * Since: 5.3.0
 **/
hb_position_t
hb_ot_layout_lookup_get_optical_bound (hb_font_t      *font,
				       unsigned        lookup_index,
				       hb_direction_t  direction,
				       hb_codepoint_t  glyph)
{
  const OT::PosLookup &lookup = font->face->table.GPOS->table->get_lookup (lookup_index);
  hb_blob_t *blob = font->face->table.GPOS->get_blob ();
  hb_glyph_position_t pos = {0};
  hb_position_single_dispatch_t c;
  lookup.dispatch (&c, font, blob, direction, glyph, pos);
  hb_position_t ret = 0;
  switch (direction)
  {
    case HB_DIRECTION_LTR:
      ret = pos.x_offset;
      break;
    case HB_DIRECTION_RTL:
      ret = pos.x_advance - pos.x_offset;
      break;
    case HB_DIRECTION_TTB:
      ret = pos.y_offset;
      break;
    case HB_DIRECTION_BTT:
      ret = pos.y_advance - pos.y_offset;
      break;
    case HB_DIRECTION_INVALID:
    default:
      break;
  }
  return ret;
}
#endif


#endif
