/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2012  Google, Inc.
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

#ifndef HB_OPEN_FILE_HH
#define HB_OPEN_FILE_HH

#include "hb-open-type.hh"
#include "hb-ot-head-table.hh"


namespace OT {

/*
 *
 * The OpenType Font File
 *
 */


/*
 * Organization of an OpenType Font
 */

struct OpenTypeFontFile;
struct OpenTypeOffsetTable;
struct TTCHeader;


typedef struct TableRecord
{
  int cmp (Tag t) const { return -t.cmp (tag); }

  HB_INTERNAL static int cmp (const void *pa, const void *pb)
  {
    const TableRecord *a = (const TableRecord *) pa;
    const TableRecord *b = (const TableRecord *) pb;
    return b->cmp (a->tag);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  Tag		tag;		/* 4-byte identifier. */
  CheckSum	checkSum;	/* CheckSum for this table. */
  Offset32	offset;		/* Offset from beginning of TrueType font
				 * file. */
  HBUINT32	length;		/* Length of this table. */
  public:
  DEFINE_SIZE_STATIC (16);
} OpenTypeTable;

typedef struct OpenTypeOffsetTable
{
  friend struct OpenTypeFontFile;

  unsigned int get_table_count () const { return tables.len; }
  const TableRecord& get_table (unsigned int i) const
  { return tables[i]; }
  unsigned int get_table_tags (unsigned int  start_offset,
			       unsigned int *table_count, /* IN/OUT */
			       hb_tag_t     *table_tags /* OUT */) const
  {
    if (table_count)
    {
      + tables.as_array ().sub_array (start_offset, table_count)
      | hb_map (&TableRecord::tag)
      | hb_sink (hb_array (table_tags, *table_count))
      ;
    }
    return tables.len;
  }
  bool find_table_index (hb_tag_t tag, unsigned int *table_index) const
  {
    Tag t;
    t = tag;
    /* Use lfind for small fonts; there are fonts that have unsorted table entries;
     * those tend to work in other tools, so tolerate them.
     * https://github.com/harfbuzz/harfbuzz/issues/3065 */
    if (tables.len < 16)
      return tables.lfind (t, table_index, HB_NOT_FOUND_STORE, Index::NOT_FOUND_INDEX);
    else
      return tables.bfind (t, table_index, HB_NOT_FOUND_STORE, Index::NOT_FOUND_INDEX);
  }
  const TableRecord& get_table_by_tag (hb_tag_t tag) const
  {
    unsigned int table_index;
    find_table_index (tag, &table_index);
    return get_table (table_index);
  }

  public:

  template <typename Iterator,
	    hb_requires ((hb_is_source_of<Iterator, hb_pair_t<hb_tag_t, hb_blob_t *>>::value))>
  bool serialize (hb_serialize_context_t *c,
		  hb_tag_t sfnt_tag,
		  Iterator it)
  {
    TRACE_SERIALIZE (this);
    /* Alloc 12 for the OTHeader. */
    if (unlikely (!c->extend_min (this))) return_trace (false);
    /* Write sfntVersion (bytes 0..3). */
    sfnt_version = sfnt_tag;
    /* Take space for numTables, searchRange, entrySelector, RangeShift
     * and the TableRecords themselves.  */
    unsigned num_items = hb_len (it);
    if (unlikely (!tables.serialize (c, num_items))) return_trace (false);

    const char *dir_end = (const char *) c->head;
    HBUINT32 *checksum_adjustment = nullptr;

    /* Write OffsetTables, alloc for and write actual table blobs. */
    unsigned i = 0;
    for (hb_pair_t<hb_tag_t, hb_blob_t*> entry : it)
    {
      hb_blob_t *blob = entry.second;
      unsigned len = blob->length;

      /* Allocate room for the table and copy it. */
      char *start = (char *) c->allocate_size<void> (len, false);
      if (unlikely (!start)) return false;

      TableRecord &rec = tables.arrayZ[i];
      rec.tag = entry.first;
      rec.length = len;
      rec.offset = 0;
      if (unlikely (!c->check_assign (rec.offset,
				      (unsigned) ((char *) start - (char *) this),
				      HB_SERIALIZE_ERROR_OFFSET_OVERFLOW)))
        return_trace (false);

      if (likely (len))
	hb_memcpy (start, blob->data, len);

      /* 4-byte alignment. */
      c->align (4);
      const char *end = (const char *) c->head;

      if (entry.first == HB_OT_TAG_head &&
	  (unsigned) (end - start) >= head::static_size)
      {
	head *h = (head *) start;
	checksum_adjustment = &h->checkSumAdjustment;
	*checksum_adjustment = 0;
      }

      rec.checkSum.set_for_data (start, end - start);
      i++;
    }

    tables.qsort ();

    if (checksum_adjustment)
    {
      CheckSum checksum;

      /* The following line is a slower version of the following block. */
      //checksum.set_for_data (this, (const char *) c->head - (const char *) this);
      checksum.set_for_data (this, dir_end - (const char *) this);
      for (unsigned int i = 0; i < num_items; i++)
      {
	TableRecord &rec = tables.arrayZ[i];
	checksum = checksum + rec.checkSum;
      }

      *checksum_adjustment = 0xB1B0AFBAu - checksum;
    }

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && tables.sanitize (c));
  }

  protected:
  Tag		sfnt_version;	/* '\0\001\0\00' if TrueType / 'OTTO' if CFF */
  BinSearchArrayOf<TableRecord>
		tables;
  public:
  DEFINE_SIZE_ARRAY (12, tables);
} OpenTypeFontFace;


/*
 * TrueType Collections
 */

struct TTCHeaderVersion1
{
  friend struct TTCHeader;

  unsigned int get_face_count () const { return table.len; }
  const OpenTypeFontFace& get_face (unsigned int i) const { return this+table[i]; }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (table.sanitize (c, this));
  }

  protected:
  Tag		ttcTag;		/* TrueType Collection ID string: 'ttcf' */
  FixedVersion<>version;	/* Version of the TTC Header (1.0),
				 * 0x00010000u */
  Array32Of<Offset32To<OpenTypeOffsetTable>>
		table;		/* Array of offsets to the OffsetTable for each font
				 * from the beginning of the file */
  public:
  DEFINE_SIZE_ARRAY (12, table);
};

struct TTCHeader
{
  friend struct OpenTypeFontFile;

  private:

  unsigned int get_face_count () const
  {
    switch (u.header.version.major) {
    case 2: /* version 2 is compatible with version 1 */
    case 1: hb_barrier (); return u.version1.get_face_count ();
    default:return 0;
    }
  }
  const OpenTypeFontFace& get_face (unsigned int i) const
  {
    switch (u.header.version.major) {
    case 2: /* version 2 is compatible with version 1 */
    case 1: hb_barrier (); return u.version1.get_face (i);
    default:return Null (OpenTypeFontFace);
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!u.header.version.sanitize (c))) return_trace (false);
    hb_barrier ();
    switch (u.header.version.major) {
    case 2: /* version 2 is compatible with version 1 */
    case 1: hb_barrier (); return_trace (u.version1.sanitize (c));
    default:return_trace (true);
    }
  }

  protected:
  union {
  struct {
  Tag		ttcTag;		/* TrueType Collection ID string: 'ttcf' */
  FixedVersion<>version;	/* Version of the TTC Header (1.0 or 2.0),
				 * 0x00010000u or 0x00020000u */
  }			header;
  TTCHeaderVersion1	version1;
  } u;
};

/*
 * Mac Resource Fork
 *
 * http://mirror.informatimago.com/next/developer.apple.com/documentation/mac/MoreToolbox/MoreToolbox-99.html
 */

struct ResourceRecord
{
  const OpenTypeFontFace & get_face (const void *data_base) const
  { return * reinterpret_cast<const OpenTypeFontFace *> ((data_base+offset).arrayZ); }

  bool sanitize (hb_sanitize_context_t *c,
		 const void *data_base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  offset.sanitize (c, data_base) &&
		  hb_barrier () &&
		  get_face (data_base).sanitize (c));
  }

  protected:
  HBUINT16	id;		/* Resource ID. */
  HBINT16	nameOffset;	/* Offset from beginning of resource name list
				 * to resource name, -1 means there is none. */
  HBUINT8	attrs;		/* Resource attributes */
  NNOffset24To<Array32Of<HBUINT8>>
		offset;		/* Offset from beginning of data block to
				 * data for this resource */
  HBUINT32	reserved;	/* Reserved for handle to resource */
  public:
  DEFINE_SIZE_STATIC (12);
};

#define HB_TAG_sfnt HB_TAG ('s','f','n','t')

struct ResourceTypeRecord
{
  unsigned int get_resource_count () const
  { return tag == HB_TAG_sfnt ? resCountM1 + 1 : 0; }

  bool is_sfnt () const { return tag == HB_TAG_sfnt; }

  const ResourceRecord& get_resource_record (unsigned int i,
					     const void *type_base) const
  { return (type_base+resourcesZ).as_array (get_resource_count ())[i]; }

  bool sanitize (hb_sanitize_context_t *c,
		 const void *type_base,
		 const void *data_base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  resourcesZ.sanitize (c, type_base,
				       get_resource_count (),
				       data_base));
  }

  protected:
  Tag		tag;		/* Resource type. */
  HBUINT16	resCountM1;	/* Number of resources minus 1. */
  NNOffset16To<UnsizedArrayOf<ResourceRecord>>
		resourcesZ;	/* Offset from beginning of resource type list
				 * to reference item list for this type. */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct ResourceMap
{
  unsigned int get_face_count () const
  {
    unsigned int count = get_type_count ();
    for (unsigned int i = 0; i < count; i++)
    {
      const ResourceTypeRecord& type = get_type_record (i);
      if (type.is_sfnt ())
	return type.get_resource_count ();
    }
    return 0;
  }

  const OpenTypeFontFace& get_face (unsigned int idx,
				    const void *data_base) const
  {
    unsigned int count = get_type_count ();
    for (unsigned int i = 0; i < count; i++)
    {
      const ResourceTypeRecord& type = get_type_record (i);
      /* The check for idx < count is here because ResourceRecord is NOT null-safe.
       * Because an offset of 0 there does NOT mean null. */
      if (type.is_sfnt () && idx < type.get_resource_count ())
	return type.get_resource_record (idx, &(this+typeList)).get_face (data_base);
    }
    return Null (OpenTypeFontFace);
  }

  bool sanitize (hb_sanitize_context_t *c, const void *data_base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  typeList.sanitize (c, this,
				     &(this+typeList),
				     data_base));
  }

  private:
  unsigned int get_type_count () const { return (this+typeList).lenM1 + 1; }

  const ResourceTypeRecord& get_type_record (unsigned int i) const
  { return (this+typeList)[i]; }

  protected:
  HBUINT8	reserved0[16];	/* Reserved for copy of resource header */
  HBUINT32	reserved1;	/* Reserved for handle to next resource map */
  HBUINT16	resreved2;	/* Reserved for file reference number */
  HBUINT16	attrs;		/* Resource fork attribute */
  NNOffset16To<ArrayOfM1<ResourceTypeRecord>>
		typeList;	/* Offset from beginning of map to
				 * resource type list */
  Offset16	nameList;	/* Offset from beginning of map to
				 * resource name list */
  public:
  DEFINE_SIZE_STATIC (28);
};

struct ResourceForkHeader
{
  unsigned int get_face_count () const
  { return (this+map).get_face_count (); }

  const OpenTypeFontFace& get_face (unsigned int idx,
				    unsigned int *base_offset = nullptr) const
  {
    const OpenTypeFontFace &face = (this+map).get_face (idx, &(this+data));
    if (base_offset)
      *base_offset = (const char *) &face - (const char *) this;
    return face;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  data.sanitize (c, this, dataLen) &&
		  map.sanitize (c, this, &(this+data)));
  }

  protected:
  NNOffset32To<UnsizedArrayOf<HBUINT8>>
		data;		/* Offset from beginning of resource fork
				 * to resource data */
  NNOffset32To<ResourceMap >
		map;		/* Offset from beginning of resource fork
				 * to resource map */
  HBUINT32	dataLen;	/* Length of resource data */
  HBUINT32	mapLen;		/* Length of resource map */
  public:
  DEFINE_SIZE_STATIC (16);
};

/*
 * OpenType Font File
 */

struct OpenTypeFontFile
{
  enum {
    CFFTag		= HB_TAG ('O','T','T','O'), /* OpenType with Postscript outlines */
    TrueTypeTag		= HB_TAG ( 0 , 1 , 0 , 0 ), /* OpenType with TrueType outlines */
    TTCTag		= HB_TAG ('t','t','c','f'), /* TrueType Collection */
    DFontTag		= HB_TAG ( 0 , 0 , 1 , 0 ), /* DFont Mac Resource Fork */
    TrueTag		= HB_TAG ('t','r','u','e'), /* Obsolete Apple TrueType */
    Typ1Tag		= HB_TAG ('t','y','p','1')  /* Obsolete Apple Type1 font in SFNT container */
  };

  hb_tag_t get_tag () const { return u.tag; }

  unsigned int get_face_count () const
  {
    switch (u.tag) {
    case CFFTag:	/* All the non-collection tags */
    case TrueTag:
    case Typ1Tag:
    case TrueTypeTag:	return 1;
    case TTCTag:	return u.ttcHeader.get_face_count ();
    case DFontTag:	return u.rfHeader.get_face_count ();
    default:		return 0;
    }
  }
  const OpenTypeFontFace& get_face (unsigned int i, unsigned int *base_offset = nullptr) const
  {
    if (base_offset)
      *base_offset = 0;
    switch (u.tag) {
    /* Note: for non-collection SFNT data we ignore index.  This is because
     * Apple dfont container is a container of SFNT's.  So each SFNT is a
     * non-TTC, but the index is more than zero. */
    case CFFTag:	/* All the non-collection tags */
    case TrueTag:
    case Typ1Tag:
    case TrueTypeTag:	return u.fontFace;
    case TTCTag:	return u.ttcHeader.get_face (i);
    case DFontTag:	return u.rfHeader.get_face (i, base_offset);
    default:		return Null (OpenTypeFontFace);
    }
  }

  template <typename Iterator,
	    hb_requires ((hb_is_source_of<Iterator, hb_pair_t<hb_tag_t, hb_blob_t *>>::value))>
  bool serialize_single (hb_serialize_context_t *c,
			 hb_tag_t sfnt_tag,
			 Iterator items)
  {
    TRACE_SERIALIZE (this);
    assert (sfnt_tag != TTCTag);
    if (unlikely (!c->extend_min (this))) return_trace (false);
    return_trace (u.fontFace.serialize (c, sfnt_tag, items));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!u.tag.sanitize (c))) return_trace (false);
    hb_barrier ();
    switch (u.tag) {
    case CFFTag:	/* All the non-collection tags */
    case TrueTag:
    case Typ1Tag:
    case TrueTypeTag:	return_trace (u.fontFace.sanitize (c));
    case TTCTag:	return_trace (u.ttcHeader.sanitize (c));
    case DFontTag:	return_trace (u.rfHeader.sanitize (c));
    default:		return_trace (true);
    }
  }

  protected:
  union {
  Tag			tag;		/* 4-byte identifier. */
  OpenTypeFontFace	fontFace;
  TTCHeader		ttcHeader;
  ResourceForkHeader	rfHeader;
  } u;
  public:
  DEFINE_SIZE_UNION (4, tag);
};


} /* namespace OT */


#endif /* HB_OPEN_FILE_HH */
