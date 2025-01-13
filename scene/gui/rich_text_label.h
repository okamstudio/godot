/**************************************************************************/
/*  rich_text_label.h                                                     */
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

#ifndef RICH_TEXT_LABEL_H
#define RICH_TEXT_LABEL_H

#include "core/object/worker_thread_pool.h"
#include "core/templates/rid_owner.h"
#include "scene/gui/popup_menu.h"
#include "scene/gui/scroll_bar.h"
#include "scene/resources/text_paragraph.h"

class CharFXTransform;
class RichTextEffect;

class RichTextLabel : public Control {
	GDCLASS(RichTextLabel, Control);

	enum RTLDrawStep {
		DRAW_STEP_BACKGROUND,
		DRAW_STEP_SHADOW,
		DRAW_STEP_OUTLINE,
		DRAW_STEP_TEXT,
		DRAW_STEP_FOREGROUND,
		DRAW_STEP_MAX,
	};

public:
	enum ListType {
		LIST_NUMBERS,
		LIST_LETTERS,
		LIST_ROMAN,
		LIST_DOTS
	};

	enum MetaUnderline {
		META_UNDERLINE_NEVER,
		META_UNDERLINE_ALWAYS,
		META_UNDERLINE_ON_HOVER,
	};

	enum ItemType {
		ITEM_FRAME,
		ITEM_TEXT,
		ITEM_IMAGE,
		ITEM_NEWLINE,
		ITEM_FONT,
		ITEM_FONT_SIZE,
		ITEM_FONT_FEATURES,
		ITEM_COLOR,
		ITEM_OUTLINE_SIZE,
		ITEM_OUTLINE_COLOR,
		ITEM_UNDERLINE,
		ITEM_STRIKETHROUGH,
		ITEM_PARAGRAPH,
		ITEM_INDENT,
		ITEM_LIST,
		ITEM_TABLE,
		ITEM_FADE,
		ITEM_SHAKE,
		ITEM_WAVE,
		ITEM_TORNADO,
		ITEM_RAINBOW,
		ITEM_PULSE,
		ITEM_BGCOLOR,
		ITEM_FGCOLOR,
		ITEM_META,
		ITEM_HINT,
		ITEM_DROPCAP,
		ITEM_CUSTOMFX,
		ITEM_CONTEXT,
		ITEM_LANGUAGE,
	};

	enum MenuItems {
		MENU_COPY,
		MENU_SELECT_ALL,
		MENU_MAX
	};

	enum DefaultFont {
		NORMAL_FONT,
		BOLD_FONT,
		ITALICS_FONT,
		BOLD_ITALICS_FONT,
		MONO_FONT,
		CUSTOM_FONT,
	};

	enum ImageUpdateMask {
		UPDATE_TEXTURE = 1 << 0,
		UPDATE_SIZE = 1 << 1,
		UPDATE_COLOR = 1 << 2,
		UPDATE_ALIGNMENT = 1 << 3,
		UPDATE_REGION = 1 << 4,
		UPDATE_PAD = 1 << 5,
		UPDATE_TOOLTIP = 1 << 6,
		UPDATE_WIDTH_IN_PERCENT = 1 << 7,
	};

protected:
	virtual void _update_theme_item_cache() override;

	void _notification(int p_what);
	static void _bind_methods();

#ifndef DISABLE_DEPRECATED
	void _push_meta_bind_compat_99481(const Variant &p_meta, MetaUnderline p_underline_mode);
	void _push_meta_bind_compat_89024(const Variant &p_meta);
	void _add_image_bind_compat_80410(const Ref<Texture2D> &p_image, const int p_width, const int p_height, const Color &p_color, InlineAlignment p_alignment, const Rect2 &p_region);
	bool _remove_paragraph_bind_compat_91098(int p_paragraph);
	static void _bind_compatibility_methods();
#endif

private:
	struct Item;

	struct Line {
		Item *from = nullptr;

		Ref<TextLine> text_prefix;
		float prefix_width = 0;
		Ref<TextParagraph> text_buf;
		Color dc_color;
		int dc_ol_size = 0;
		Color dc_ol_color;

		Vector2 offset;
		float indent = 0.0;
		int char_offset = 0;
		int char_count = 0;

		Line() { text_buf.instantiate(); }

		_FORCE_INLINE_ float get_height(float line_separation) const {
			return offset.y + text_buf->get_size().y + text_buf->get_line_count() * line_separation;
		}
	};

	struct Item {
		int index = 0;
		int char_ofs = 0;
		Item *parent = nullptr;
		ItemType type = ITEM_FRAME;
		List<Item *> subitems;
		List<Item *>::Element *E = nullptr;
		ObjectID owner;
		int line = 0;
		RID rid;

		void _clear_children() {
			RichTextLabel *owner_rtl = Object::cast_to<RichTextLabel>(ObjectDB::get_instance(owner));
			while (subitems.size()) {
				Item *subitem = subitems.front()->get();
				if (subitem && subitem->rid.is_valid() && owner_rtl) {
					owner_rtl->items.free(subitem->rid);
				}
				memdelete(subitem);
				subitems.pop_front();
			}
		}

		virtual ~Item() { _clear_children(); }
	};

	struct ItemFrame : public Item {
		bool cell = false;

		LocalVector<Line> lines;
		std::atomic<int> first_invalid_line;
		std::atomic<int> first_invalid_font_line;
		std::atomic<int> first_resized_line;

		ItemFrame *parent_frame = nullptr;

		Color odd_row_bg = Color(0, 0, 0, 0);
		Color even_row_bg = Color(0, 0, 0, 0);
		Color border = Color(0, 0, 0, 0);
		Size2 min_size_over = Size2(-1, -1);
		Size2 max_size_over = Size2(-1, -1);
		Rect2 padding;
		int indent_level = 0;

		ItemFrame() {
			type = ITEM_FRAME;
			first_invalid_line.store(0);
			first_invalid_font_line.store(0);
			first_resized_line.store(0);
		}
	};

	struct ItemText : public Item {
		String text;
		ItemText() { type = ITEM_TEXT; }
	};

	struct ItemDropcap : public Item {
		String text;
		Ref<Font> font;
		int font_size = 0;
		Color color;
		int ol_size = 0;
		Color ol_color;
		Rect2 dropcap_margins;
		ItemDropcap() { type = ITEM_DROPCAP; }
	};

	struct ItemImage : public Item {
		Ref<Texture2D> image;
		InlineAlignment inline_align = INLINE_ALIGNMENT_CENTER;
		bool pad = false;
		bool size_in_percent = false;
		Rect2 region;
		Size2 size;
		Size2 rq_size;
		Color color;
		Variant key;
		String tooltip;
		ItemImage() { type = ITEM_IMAGE; }
		~ItemImage() {
			if (image.is_valid()) {
				RichTextLabel *owner_rtl = Object::cast_to<RichTextLabel>(ObjectDB::get_instance(owner));
				if (owner_rtl) {
					image->disconnect_changed(callable_mp(owner_rtl, &RichTextLabel::_texture_changed));
				}
			}
		}
	};

	struct ItemFont : public Item {
		DefaultFont def_font = CUSTOM_FONT;
		Ref<Font> font;
		bool variation = false;
		bool def_size = false;
		int font_size = 0;
		ItemFont() { type = ITEM_FONT; }
	};

	struct ItemFontSize : public Item {
		int font_size = 16;
		ItemFontSize() { type = ITEM_FONT_SIZE; }
	};

	struct ItemColor : public Item {
		Color color;
		ItemColor() { type = ITEM_COLOR; }
	};

	struct ItemOutlineSize : public Item {
		int outline_size = 0;
		ItemOutlineSize() { type = ITEM_OUTLINE_SIZE; }
	};

	struct ItemOutlineColor : public Item {
		Color color;
		ItemOutlineColor() { type = ITEM_OUTLINE_COLOR; }
	};

	struct ItemUnderline : public Item {
		ItemUnderline() { type = ITEM_UNDERLINE; }
	};

	struct ItemStrikethrough : public Item {
		ItemStrikethrough() { type = ITEM_STRIKETHROUGH; }
	};

	struct ItemMeta : public Item {
		Variant meta;
		MetaUnderline underline = META_UNDERLINE_ALWAYS;
		String tooltip;
		ItemMeta() { type = ITEM_META; }
	};

	struct ItemHint : public Item {
		String description;
		ItemHint() { type = ITEM_HINT; }
	};

	struct ItemLanguage : public Item {
		String language;
		ItemLanguage() { type = ITEM_LANGUAGE; }
	};

	struct ItemParagraph : public Item {
		HorizontalAlignment alignment = HORIZONTAL_ALIGNMENT_LEFT;
		String language;
		Control::TextDirection direction = Control::TEXT_DIRECTION_AUTO;
		TextServer::StructuredTextParser st_parser = TextServer::STRUCTURED_TEXT_DEFAULT;
		BitField<TextServer::JustificationFlag> jst_flags = TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_SKIP_LAST_LINE | TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE;
		PackedFloat32Array tab_stops;
		ItemParagraph() { type = ITEM_PARAGRAPH; }
	};

	struct ItemIndent : public Item {
		int level = 0;
		ItemIndent() { type = ITEM_INDENT; }
	};

	struct ItemList : public Item {
		ListType list_type = LIST_DOTS;
		bool capitalize = false;
		int level = 0;
		String bullet = U"•";
		float max_width = 0;
		ItemList() { type = ITEM_LIST; }
	};

	struct ItemNewline : public Item {
		ItemNewline() { type = ITEM_NEWLINE; }
	};

	struct ItemTable : public Item {
		struct Column {
			bool expand = false;
			int expand_ratio = 0;
			int min_width = 0;
			int max_width = 0;
			int width = 0;
		};

		LocalVector<Column> columns;
		LocalVector<float> rows;
		LocalVector<float> rows_baseline;

		int align_to_row = -1;
		int total_width = 0;
		int total_height = 0;
		InlineAlignment inline_align = INLINE_ALIGNMENT_TOP;
		ItemTable() { type = ITEM_TABLE; }
	};

	struct ItemFade : public Item {
		int starting_index = 0;
		int length = 0;

		ItemFade() { type = ITEM_FADE; }
	};

	struct ItemFX : public Item {
		double elapsed_time = 0.f;
		bool connected = true;
	};

	struct ItemShake : public ItemFX {
		int strength = 0;
		float rate = 0.0f;
		uint64_t _current_rng = 0;
		uint64_t _previous_rng = 0;
		Vector2 prev_off;

		ItemShake() { type = ITEM_SHAKE; }

		void reroll_random() {
			_previous_rng = _current_rng;
			_current_rng = Math::rand();
		}

		uint64_t offset_random(int p_index) {
			return (_current_rng >> (p_index % 64)) |
					(_current_rng << (64 - (p_index % 64)));
		}

		uint64_t offset_previous_random(int p_index) {
			return (_previous_rng >> (p_index % 64)) |
					(_previous_rng << (64 - (p_index % 64)));
		}
	};

	struct ItemWave : public ItemFX {
		float frequency = 1.0f;
		float amplitude = 1.0f;
		Vector2 prev_off;

		ItemWave() { type = ITEM_WAVE; }
	};

	struct ItemTornado : public ItemFX {
		float radius = 1.0f;
		float frequency = 1.0f;
		Vector2 prev_off;

		ItemTornado() { type = ITEM_TORNADO; }
	};

	struct ItemRainbow : public ItemFX {
		float saturation = 0.8f;
		float value = 0.8f;
		float frequency = 1.0f;

		ItemRainbow() { type = ITEM_RAINBOW; }
	};

	struct ItemPulse : public ItemFX {
		Color color = Color(1.0, 1.0, 1.0, 0.25);
		float frequency = 1.0f;
		float ease = -2.0f;

		ItemPulse() { type = ITEM_PULSE; }
	};

	struct ItemBGColor : public Item {
		Color color;
		ItemBGColor() { type = ITEM_BGCOLOR; }
	};

	struct ItemFGColor : public Item {
		Color color;
		ItemFGColor() { type = ITEM_FGCOLOR; }
	};

	struct ItemCustomFX : public ItemFX {
		Ref<CharFXTransform> char_fx_transform;
		Ref<RichTextEffect> custom_effect;

		ItemCustomFX();

		virtual ~ItemCustomFX();
	};

	struct ItemContext : public Item {
		ItemContext() { type = ITEM_CONTEXT; }
	};

	ItemFrame *main = nullptr;
	Item *current = nullptr;
	ItemFrame *current_frame = nullptr;

	WorkerThreadPool::TaskID task = WorkerThreadPool::INVALID_TASK_ID;
	Mutex data_mutex;
	bool threaded = false;
	std::atomic<bool> stop_thread;
	std::atomic<bool> updating;
	std::atomic<bool> validating;
	std::atomic<double> loaded;
	std::atomic<bool> parsing_bbcode;

	uint64_t loading_started = 0;
	int progress_delay = 1000;

	VScrollBar *vscroll = nullptr;

	TextServer::AutowrapMode autowrap_mode = TextServer::AUTOWRAP_WORD_SMART;

	bool scroll_visible = false;
	bool scroll_follow = false;
	bool scroll_following = false;
	bool scroll_active = true;
	int scroll_w = 0;
	bool scroll_updated = false;
	bool updating_scroll = false;
	int current_idx = 1;
	int current_char_ofs = 0;
	int visible_paragraph_count = 0;
	int visible_line_count = 0;

	int tab_size = 4;
	bool underline_meta = true;
	bool underline_hint = true;
	bool use_selected_font_color = false;

	HorizontalAlignment default_alignment = HORIZONTAL_ALIGNMENT_LEFT;
	VerticalAlignment vertical_alignment = VERTICAL_ALIGNMENT_TOP;
	BitField<TextServer::JustificationFlag> default_jst_flags = TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_SKIP_LAST_LINE | TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE;
	PackedFloat32Array default_tab_stops;

	ItemMeta *meta_hovering = nullptr;
	Variant current_meta;

	Array custom_effects;

	void _invalidate_current_line(ItemFrame *p_frame);

	void _thread_function(void *p_userdata);
	void _thread_end();
	void _stop_thread();
	bool _validate_line_caches();
	void _process_line_caches();
	_FORCE_INLINE_ float _update_scroll_exceeds(float p_total_height, float p_ctrl_height, float p_width, int p_idx, float p_old_scroll, float p_text_rect_height);

	void _add_item(Item *p_item, bool p_enter = false, bool p_ensure_newline = false);
	void _remove_frame(HashSet<Item *> &r_erase_list, ItemFrame *p_frame, int p_line, bool p_erase, int p_char_offset, int p_line_offset);

	void _texture_changed(RID p_item);

	RID_PtrOwner<Item> items;
	List<String> tag_stack;

	String language;
	TextDirection text_direction = TEXT_DIRECTION_AUTO;
	TextServer::StructuredTextParser st_parser = TextServer::STRUCTURED_TEXT_DEFAULT;
	Array st_args;

	struct Selection {
		ItemFrame *click_frame = nullptr;
		int click_line = 0;
		Item *click_item = nullptr;
		int click_char = 0;

		ItemFrame *from_frame = nullptr;
		int from_line = 0;
		Item *from_item = nullptr;
		int from_char = 0;

		ItemFrame *to_frame = nullptr;
		int to_line = 0;
		Item *to_item = nullptr;
		int to_char = 0;

		bool active = false; // anything selected? i.e. from, to, etc. valid?
		bool enabled = false; // allow selections?
		bool drag_attempt = false;
	};

	Selection selection;
	bool deselect_on_focus_loss_enabled = true;
	bool drag_and_drop_selection_enabled = true;

	bool context_menu_enabled = false;
	bool shortcut_keys_enabled = true;

	// Context menu.
	PopupMenu *menu = nullptr;
	void _generate_context_menu();
	void _update_context_menu();
	Key _get_menu_action_accelerator(const String &p_action);

	int visible_characters = -1;
	float visible_ratio = 1.0;
	TextServer::VisibleCharactersBehavior visible_chars_behavior = TextServer::VC_CHARS_BEFORE_SHAPING;

	bool _is_click_inside_selection() const;
	void _find_click(ItemFrame *p_frame, const Point2i &p_click, ItemFrame **r_click_frame = nullptr, int *r_click_line = nullptr, Item **r_click_item = nullptr, int *r_click_char = nullptr, bool *r_outside = nullptr, bool p_meta = false);

	String _get_line_text(ItemFrame *p_frame, int p_line, Selection p_sel) const;
	bool _search_line(ItemFrame *p_frame, int p_line, const String &p_string, int p_char_idx, bool p_reverse_search);
	bool _search_table(ItemTable *p_table, List<Item *>::Element *p_from, const String &p_string, bool p_reverse_search);

	float _shape_line(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size, int p_width, float p_h, int *r_char_offset);
	float _resize_line(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size, int p_width, float p_h);

	void _set_table_size(ItemTable *p_table, int p_available_width);

	void _update_line_font(ItemFrame *p_frame, int p_line, const Ref<Font> &p_base_font, int p_base_font_size);
	int _draw_line(ItemFrame *p_frame, int p_line, const Vector2 &p_ofs, int p_width, float p_vsep, const Color &p_base_color, int p_outline_size, const Color &p_outline_color, const Color &p_font_shadow_color, int p_shadow_outline_size, const Point2 &p_shadow_ofs, int &r_processed_glyphs);
	float _find_click_in_line(ItemFrame *p_frame, int p_line, const Vector2 &p_ofs, int p_width, float p_vsep, const Point2i &p_click, ItemFrame **r_click_frame = nullptr, int *r_click_line = nullptr, Item **r_click_item = nullptr, int *r_click_char = nullptr, bool p_table = false, bool p_meta = false);

	String _roman(int p_num, bool p_capitalize) const;
	String _letters(int p_num, bool p_capitalize) const;

	Item *_find_indentable(Item *p_item);
	Item *_get_item_at_pos(Item *p_item_from, Item *p_item_to, int p_position);
	void _find_frame(Item *p_item, ItemFrame **r_frame, int *r_line);
	ItemFontSize *_find_font_size(Item *p_item);
	ItemFont *_find_font(Item *p_item);
	int _find_outline_size(Item *p_item, int p_default);
	ItemList *_find_list_item(Item *p_item);
	ItemDropcap *_find_dc_item(Item *p_item);
	int _find_list(Item *p_item, Vector<int> &r_index, Vector<int> &r_count, Vector<ItemList *> &r_list);
	int _find_margin(Item *p_item, const Ref<Font> &p_base_font, int p_base_font_size);
	PackedFloat32Array _find_tab_stops(Item *p_item);
	HorizontalAlignment _find_alignment(Item *p_item);
	BitField<TextServer::JustificationFlag> _find_jst_flags(Item *p_item);
	TextServer::Direction _find_direction(Item *p_item);
	TextServer::StructuredTextParser _find_stt(Item *p_item);
	String _find_language(Item *p_item);
	Color _find_color(Item *p_item, const Color &p_default_color);
	Color _find_outline_color(Item *p_item, const Color &p_default_color);
	bool _find_underline(Item *p_item);
	bool _find_strikethrough(Item *p_item);
	bool _find_meta(Item *p_item, Variant *r_meta, ItemMeta **r_item = nullptr);
	bool _find_hint(Item *p_item, String *r_description);
	Color _find_bgcolor(Item *p_item);
	Color _find_fgcolor(Item *p_item);
	bool _find_layout_subitem(Item *from, Item *to);
	void _fetch_item_fx_stack(Item *p_item, Vector<ItemFX *> &r_stack);
	void _normalize_subtags(Vector<String> &subtags);

	void _update_fx(ItemFrame *p_frame, double p_delta_time);
	void _scroll_changed(double);
	int _find_first_line(int p_from, int p_to, int p_vofs) const;

	_FORCE_INLINE_ float _calculate_line_vertical_offset(const Line &line) const;

	virtual void gui_input(const Ref<InputEvent> &p_event) override;
	virtual String get_tooltip(const Point2 &p_pos) const override;
	Item *_get_next_item(Item *p_item, bool p_free = false) const;
	Item *_get_prev_item(Item *p_item, bool p_free = false) const;

	Rect2 _get_text_rect();
	Ref<RichTextEffect> _get_custom_effect_by_code(String p_bbcode_identifier);
	virtual Dictionary parse_expressions_for_values(Vector<String> p_expressions);

	Size2 _get_image_size(const Ref<Texture2D> &p_image, int p_width = 0, int p_height = 0, const Rect2 &p_region = Rect2());

	String _get_prefix(Item *p_item, const Vector<int> &p_list_index, const Vector<ItemList *> &p_list_items);

	static int _find_unquoted(const String &p_src, char32_t p_chr, int p_from);
	static Vector<String> _split_unquoted(const String &p_src, char32_t p_splitter);
	static String _get_tag_value(const String &p_tag);

#ifndef DISABLE_DEPRECATED
	// Kept for compatibility from 3.x to 4.0.
	bool _set(const StringName &p_name, const Variant &p_value);
#endif
	bool use_bbcode = false;
	String text;
	void _apply_translation();

	bool internal_stack_editing = false;
	bool stack_externally_modified = false;

	bool fit_content = false;

	struct ThemeCache {
		Ref<StyleBox> normal_style;
		Ref<StyleBox> focus_style;
		Ref<StyleBox> progress_bg_style;
		Ref<StyleBox> progress_fg_style;

		int line_separation;

		Ref<Font> normal_font;
		int normal_font_size;

		Color default_color;
		Color font_selected_color;
		Color selection_color;
		Color font_outline_color;
		Color font_shadow_color;
		int shadow_outline_size;
		int shadow_offset_x;
		int shadow_offset_y;
		int outline_size;
		Color outline_color;

		Ref<Font> bold_font;
		int bold_font_size;
		Ref<Font> bold_italics_font;
		int bold_italics_font_size;
		Ref<Font> italics_font;
		int italics_font_size;
		Ref<Font> mono_font;
		int mono_font_size;

		int text_highlight_h_padding;
		int text_highlight_v_padding;

		int table_h_separation;
		int table_v_separation;
		Color table_odd_row_bg;
		Color table_even_row_bg;
		Color table_border;

		float base_scale = 1.0;
	} theme_cache;

public:
	String get_parsed_text() const;
	void add_text(const String &p_text);
	void add_image(const Ref<Texture2D> &p_image, int p_width = 0, int p_height = 0, const Color &p_color = Color(1.0, 1.0, 1.0), InlineAlignment p_alignment = INLINE_ALIGNMENT_CENTER, const Rect2 &p_region = Rect2(), const Variant &p_key = Variant(), bool p_pad = false, const String &p_tooltip = String(), bool p_size_in_percent = false);
	void update_image(const Variant &p_key, BitField<ImageUpdateMask> p_mask, const Ref<Texture2D> &p_image, int p_width = 0, int p_height = 0, const Color &p_color = Color(1.0, 1.0, 1.0), InlineAlignment p_alignment = INLINE_ALIGNMENT_CENTER, const Rect2 &p_region = Rect2(), bool p_pad = false, const String &p_tooltip = String(), bool p_size_in_percent = false);
	void add_newline();
	bool remove_paragraph(int p_paragraph, bool p_no_invalidate = false);
	bool invalidate_paragraph(int p_paragraph);
	void push_dropcap(const String &p_string, const Ref<Font> &p_font, int p_size, const Rect2 &p_dropcap_margins = Rect2(), const Color &p_color = Color(1, 1, 1), int p_ol_size = 0, const Color &p_ol_color = Color(0, 0, 0, 0));
	void _push_def_font(DefaultFont p_def_font);
	void _push_def_font_var(DefaultFont p_def_font, const Ref<Font> &p_font, int p_size = -1);
	void push_font(const Ref<Font> &p_font, int p_size = 0);
	void push_font_size(int p_font_size);
	void push_outline_size(int p_font_size);
	void push_normal();
	void push_bold();
	void push_bold_italics();
	void push_italics();
	void push_mono();
	void push_color(const Color &p_color);
	void push_outline_color(const Color &p_color);
	void push_underline();
	void push_strikethrough();
	void push_language(const String &p_language);
	void push_paragraph(HorizontalAlignment p_alignment, Control::TextDirection p_direction = Control::TEXT_DIRECTION_INHERITED, const String &p_language = "", TextServer::StructuredTextParser p_st_parser = TextServer::STRUCTURED_TEXT_DEFAULT, BitField<TextServer::JustificationFlag> p_jst_flags = TextServer::JUSTIFICATION_WORD_BOUND | TextServer::JUSTIFICATION_KASHIDA | TextServer::JUSTIFICATION_SKIP_LAST_LINE | TextServer::JUSTIFICATION_DO_NOT_SKIP_SINGLE_LINE, const PackedFloat32Array &p_tab_stops = PackedFloat32Array());
	void push_indent(int p_level);
	void push_list(int p_level, ListType p_list, bool p_capitalize, const String &p_bullet = String::utf8("•"));
	void push_meta(const Variant &p_meta, MetaUnderline p_underline_mode = META_UNDERLINE_ALWAYS, const String &p_tooltip = String());
	void push_hint(const String &p_string);
	void push_table(int p_columns, InlineAlignment p_alignment = INLINE_ALIGNMENT_TOP, int p_align_to_row = -1);
	void push_fade(int p_start_index, int p_length);
	void push_shake(int p_strength, float p_rate, bool p_connected);
	void push_wave(float p_frequency, float p_amplitude, bool p_connected);
	void push_tornado(float p_frequency, float p_radius, bool p_connected);
	void push_rainbow(float p_saturation, float p_value, float p_frequency);
	void push_pulse(const Color &p_color, float p_frequency, float p_ease);
	void push_bgcolor(const Color &p_color);
	void push_fgcolor(const Color &p_color);
	void push_customfx(Ref<RichTextEffect> p_custom_effect, Dictionary p_environment);
	void push_context();
	void set_table_column_expand(int p_column, bool p_expand, int p_ratio = 1);
	void set_cell_row_background_color(const Color &p_odd_row_bg, const Color &p_even_row_bg);
	void set_cell_border_color(const Color &p_color);
	void set_cell_size_override(const Size2 &p_min_size, const Size2 &p_max_size);
	void set_cell_padding(const Rect2 &p_padding);
	int get_current_table_column() const;
	void push_cell();
	void pop();
	void pop_context();
	void pop_all();

	void clear();

	void set_offset(int p_pixel);

	void set_meta_underline(bool p_underline);
	bool is_meta_underlined() const;

	void set_hint_underline(bool p_underline);
	bool is_hint_underlined() const;

	void set_scroll_active(bool p_active);
	bool is_scroll_active() const;

	void set_scroll_follow(bool p_follow);
	bool is_scroll_following() const;

	void set_tab_size(int p_spaces);
	int get_tab_size() const;

	void set_context_menu_enabled(bool p_enabled);
	bool is_context_menu_enabled() const;

	void set_shortcut_keys_enabled(bool p_enabled);
	bool is_shortcut_keys_enabled() const;

	void set_fit_content(bool p_enabled);
	bool is_fit_content_enabled() const;

	bool search(const String &p_string, bool p_from_selection = false, bool p_search_previous = false);

	void scroll_to_paragraph(int p_paragraph);
	int get_paragraph_count() const;
	int get_visible_paragraph_count() const;

	float get_line_offset(int p_line);
	float get_paragraph_offset(int p_paragraph);

	void scroll_to_line(int p_line);
	int get_line_count() const;
	int get_visible_line_count() const;

	int get_content_height() const;
	int get_content_width() const;

	void scroll_to_selection();

	VScrollBar *get_v_scroll_bar() { return vscroll; }

	virtual CursorShape get_cursor_shape(const Point2 &p_pos) const override;
	virtual Variant get_drag_data(const Point2 &p_point) override;

	void set_selection_enabled(bool p_enabled);
	bool is_selection_enabled() const;
	int get_selection_from() const;
	int get_selection_to() const;
	String get_selected_text() const;
	void select_all();
	void selection_copy();

	void set_deselect_on_focus_loss_enabled(const bool p_enabled);
	bool is_deselect_on_focus_loss_enabled() const;

	void set_drag_and_drop_selection_enabled(const bool p_enabled);
	bool is_drag_and_drop_selection_enabled() const;

	void deselect();

	int get_pending_paragraphs() const;
	bool is_finished() const;
	bool is_updating() const;

	void set_threaded(bool p_threaded);
	bool is_threaded() const;

	void set_progress_bar_delay(int p_delay_ms);
	int get_progress_bar_delay() const;

	// Context menu.
	PopupMenu *get_menu() const;
	bool is_menu_visible() const;
	void menu_option(int p_option);

	void parse_bbcode(const String &p_bbcode);
	void append_text(const String &p_bbcode);

	void set_use_bbcode(bool p_enable);
	bool is_using_bbcode() const;

	void set_text(const String &p_bbcode);
	String get_text() const;

	void set_horizontal_alignment(HorizontalAlignment p_alignment);
	HorizontalAlignment get_horizontal_alignment() const;

	void set_vertical_alignment(VerticalAlignment p_alignment);
	VerticalAlignment get_vertical_alignment() const;

	void set_justification_flags(BitField<TextServer::JustificationFlag> p_flags);
	BitField<TextServer::JustificationFlag> get_justification_flags() const;

	void set_tab_stops(const PackedFloat32Array &p_tab_stops);
	PackedFloat32Array get_tab_stops() const;

	void set_text_direction(TextDirection p_text_direction);
	TextDirection get_text_direction() const;

	void set_language(const String &p_language);
	String get_language() const;

	void set_autowrap_mode(TextServer::AutowrapMode p_mode);
	TextServer::AutowrapMode get_autowrap_mode() const;

	void set_structured_text_bidi_override(TextServer::StructuredTextParser p_parser);
	TextServer::StructuredTextParser get_structured_text_bidi_override() const;

	void set_structured_text_bidi_override_options(Array p_args);
	Array get_structured_text_bidi_override_options() const;

	void set_visible_characters(int p_visible);
	int get_visible_characters() const;
	int get_character_line(int p_char);
	int get_character_paragraph(int p_char);
	int get_total_character_count() const;
	int get_total_glyph_count() const;

	void set_visible_ratio(float p_ratio);
	float get_visible_ratio() const;

	TextServer::VisibleCharactersBehavior get_visible_characters_behavior() const;
	void set_visible_characters_behavior(TextServer::VisibleCharactersBehavior p_behavior);

	void set_effects(Array p_effects);
	Array get_effects();

	void install_effect(const Variant effect);

	virtual Size2 get_minimum_size() const override;

	RichTextLabel(const String &p_text = String());
	~RichTextLabel();
};

VARIANT_ENUM_CAST(RichTextLabel::ListType);
VARIANT_ENUM_CAST(RichTextLabel::MenuItems);
VARIANT_ENUM_CAST(RichTextLabel::MetaUnderline);
VARIANT_BITFIELD_CAST(RichTextLabel::ImageUpdateMask);

#endif // RICH_TEXT_LABEL_H
