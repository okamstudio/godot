/**
 * Copyright 2023 The AccessKit Authors. All rights reserved.
 * Licensed under the Apache License, Version 2.0 (found in
 * the LICENSE-APACHE file) or the MIT license (found in
 * the LICENSE-MIT file), at your option.
 */

#ifndef ACCESSKIT_H
#define ACCESSKIT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#endif

/**
 * An action to be taken on an accessibility node.
 */
enum accesskit_action
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  /**
   * Do the equivalent of a single click or tap.
   */
  ACCESSKIT_ACTION_CLICK,
  ACCESSKIT_ACTION_FOCUS,
  ACCESSKIT_ACTION_BLUR,
  ACCESSKIT_ACTION_COLLAPSE,
  ACCESSKIT_ACTION_EXPAND,
  /**
   * Requires [`ActionRequest::data`] to be set to [`ActionData::CustomAction`].
   */
  ACCESSKIT_ACTION_CUSTOM_ACTION,
  /**
   * Decrement a numeric value by one step.
   */
  ACCESSKIT_ACTION_DECREMENT,
  /**
   * Increment a numeric value by one step.
   */
  ACCESSKIT_ACTION_INCREMENT,
  ACCESSKIT_ACTION_HIDE_TOOLTIP,
  ACCESSKIT_ACTION_SHOW_TOOLTIP,
  /**
   * Delete any selected text in the control's text value and
   * insert the specified value in its place, like when typing or pasting.
   * Requires [`ActionRequest::data`] to be set to [`ActionData::Value`].
   */
  ACCESSKIT_ACTION_REPLACE_SELECTED_TEXT,
  ACCESSKIT_ACTION_SCROLL_BACKWARD,
  ACCESSKIT_ACTION_SCROLL_DOWN,
  ACCESSKIT_ACTION_SCROLL_FORWARD,
  ACCESSKIT_ACTION_SCROLL_LEFT,
  ACCESSKIT_ACTION_SCROLL_RIGHT,
  ACCESSKIT_ACTION_SCROLL_UP,
  /**
   * Scroll any scrollable containers to make the target object visible
   * on the screen.  Optionally set [`ActionRequest::data`] to
   * [`ActionData::ScrollTargetRect`].
   */
  ACCESSKIT_ACTION_SCROLL_INTO_VIEW,
  /**
   * Scroll the given object to a specified point in the tree's container
   * (e.g. window). Requires [`ActionRequest::data`] to be set to
   * [`ActionData::ScrollToPoint`].
   */
  ACCESSKIT_ACTION_SCROLL_TO_POINT,
  /**
   * Requires [`ActionRequest::data`] to be set to
   * [`ActionData::SetScrollOffset`].
   */
  ACCESSKIT_ACTION_SET_SCROLL_OFFSET,
  /**
   * Requires [`ActionRequest::data`] to be set to
   * [`ActionData::SetTextSelection`].
   */
  ACCESSKIT_ACTION_SET_TEXT_SELECTION,
  /**
   * Don't focus this node, but set it as the sequential focus navigation
   * starting point, so that pressing Tab moves to the next element
   * following this one, for example.
   */
  ACCESSKIT_ACTION_SET_SEQUENTIAL_FOCUS_NAVIGATION_STARTING_POINT,
  /**
   * Replace the value of the control with the specified value and
   * reset the selection, if applicable. Requires [`ActionRequest::data`]
   * to be set to [`ActionData::Value`] or [`ActionData::NumericValue`].
   */
  ACCESSKIT_ACTION_SET_VALUE,
  ACCESSKIT_ACTION_SHOW_CONTEXT_MENU,
};
#ifndef __cplusplus
typedef uint8_t accesskit_action;
#endif  // __cplusplus

enum accesskit_aria_current
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_ARIA_CURRENT_FALSE,
  ACCESSKIT_ARIA_CURRENT_TRUE,
  ACCESSKIT_ARIA_CURRENT_PAGE,
  ACCESSKIT_ARIA_CURRENT_STEP,
  ACCESSKIT_ARIA_CURRENT_LOCATION,
  ACCESSKIT_ARIA_CURRENT_DATE,
  ACCESSKIT_ARIA_CURRENT_TIME,
};
#ifndef __cplusplus
typedef uint8_t accesskit_aria_current;
#endif  // __cplusplus

enum accesskit_auto_complete
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_AUTO_COMPLETE_INLINE,
  ACCESSKIT_AUTO_COMPLETE_LIST,
  ACCESSKIT_AUTO_COMPLETE_BOTH,
};
#ifndef __cplusplus
typedef uint8_t accesskit_auto_complete;
#endif  // __cplusplus

enum accesskit_has_popup
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_HAS_POPUP_TRUE,
  ACCESSKIT_HAS_POPUP_MENU,
  ACCESSKIT_HAS_POPUP_LISTBOX,
  ACCESSKIT_HAS_POPUP_TREE,
  ACCESSKIT_HAS_POPUP_GRID,
  ACCESSKIT_HAS_POPUP_DIALOG,
};
#ifndef __cplusplus
typedef uint8_t accesskit_has_popup;
#endif  // __cplusplus

/**
 * Indicates if a form control has invalid input or if a web DOM element has an
 * [`aria-invalid`] attribute.
 *
 * [`aria-invalid`]: https://www.w3.org/TR/wai-aria-1.1/#aria-invalid
 */
enum accesskit_invalid
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_INVALID_TRUE,
  ACCESSKIT_INVALID_GRAMMAR,
  ACCESSKIT_INVALID_SPELLING,
};
#ifndef __cplusplus
typedef uint8_t accesskit_invalid;
#endif  // __cplusplus

enum accesskit_list_style
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_LIST_STYLE_CIRCLE,
  ACCESSKIT_LIST_STYLE_DISC,
  ACCESSKIT_LIST_STYLE_IMAGE,
  ACCESSKIT_LIST_STYLE_NUMERIC,
  ACCESSKIT_LIST_STYLE_SQUARE,
  /**
   * Language specific ordering (alpha, roman, cjk-ideographic, etc...)
   */
  ACCESSKIT_LIST_STYLE_OTHER,
};
#ifndef __cplusplus
typedef uint8_t accesskit_list_style;
#endif  // __cplusplus

enum accesskit_live
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_LIVE_OFF,
  ACCESSKIT_LIVE_POLITE,
  ACCESSKIT_LIVE_ASSERTIVE,
};
#ifndef __cplusplus
typedef uint8_t accesskit_live;
#endif  // __cplusplus

enum accesskit_orientation
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  /**
   * E.g. most toolbars and separators.
   */
  ACCESSKIT_ORIENTATION_HORIZONTAL,
  /**
   * E.g. menu or combo box.
   */
  ACCESSKIT_ORIENTATION_VERTICAL,
};
#ifndef __cplusplus
typedef uint8_t accesskit_orientation;
#endif  // __cplusplus

/**
 * The type of an accessibility node.
 *
 * The majority of these roles come from the ARIA specification. Reference
 * the latest draft for proper usage.
 *
 * Like the AccessKit schema as a whole, this list is largely taken
 * from Chromium. However, unlike Chromium's alphabetized list, this list
 * is ordered roughly by expected usage frequency (with the notable exception
 * of [`Role::Unknown`]). This is more efficient in serialization formats
 * where integers use a variable-length encoding.
 */
enum accesskit_role
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_ROLE_UNKNOWN,
  ACCESSKIT_ROLE_TEXT_RUN,
  ACCESSKIT_ROLE_CELL,
  ACCESSKIT_ROLE_LABEL,
  ACCESSKIT_ROLE_IMAGE,
  ACCESSKIT_ROLE_LINK,
  ACCESSKIT_ROLE_ROW,
  ACCESSKIT_ROLE_LIST_ITEM,
  /**
   * Contains the bullet, number, or other marker for a list item.
   */
  ACCESSKIT_ROLE_LIST_MARKER,
  ACCESSKIT_ROLE_TREE_ITEM,
  ACCESSKIT_ROLE_LIST_BOX_OPTION,
  ACCESSKIT_ROLE_MENU_ITEM,
  ACCESSKIT_ROLE_MENU_LIST_OPTION,
  ACCESSKIT_ROLE_PARAGRAPH,
  /**
   * A generic container that should be ignored by assistive technologies
   * and filtered out of platform accessibility trees. Equivalent to the ARIA
   * `none` or `presentation` role, or to an HTML `div` with no role.
   */
  ACCESSKIT_ROLE_GENERIC_CONTAINER,
  ACCESSKIT_ROLE_CHECK_BOX,
  ACCESSKIT_ROLE_RADIO_BUTTON,
  ACCESSKIT_ROLE_TEXT_INPUT,
  ACCESSKIT_ROLE_BUTTON,
  ACCESSKIT_ROLE_DEFAULT_BUTTON,
  ACCESSKIT_ROLE_PANE,
  ACCESSKIT_ROLE_ROW_HEADER,
  ACCESSKIT_ROLE_COLUMN_HEADER,
  ACCESSKIT_ROLE_ROW_GROUP,
  ACCESSKIT_ROLE_LIST,
  ACCESSKIT_ROLE_TABLE,
  ACCESSKIT_ROLE_LAYOUT_TABLE_CELL,
  ACCESSKIT_ROLE_LAYOUT_TABLE_ROW,
  ACCESSKIT_ROLE_LAYOUT_TABLE,
  ACCESSKIT_ROLE_SWITCH,
  ACCESSKIT_ROLE_MENU,
  ACCESSKIT_ROLE_MULTILINE_TEXT_INPUT,
  ACCESSKIT_ROLE_SEARCH_INPUT,
  ACCESSKIT_ROLE_DATE_INPUT,
  ACCESSKIT_ROLE_DATE_TIME_INPUT,
  ACCESSKIT_ROLE_WEEK_INPUT,
  ACCESSKIT_ROLE_MONTH_INPUT,
  ACCESSKIT_ROLE_TIME_INPUT,
  ACCESSKIT_ROLE_EMAIL_INPUT,
  ACCESSKIT_ROLE_NUMBER_INPUT,
  ACCESSKIT_ROLE_PASSWORD_INPUT,
  ACCESSKIT_ROLE_PHONE_NUMBER_INPUT,
  ACCESSKIT_ROLE_URL_INPUT,
  ACCESSKIT_ROLE_ABBR,
  ACCESSKIT_ROLE_ALERT,
  ACCESSKIT_ROLE_ALERT_DIALOG,
  ACCESSKIT_ROLE_APPLICATION,
  ACCESSKIT_ROLE_ARTICLE,
  ACCESSKIT_ROLE_AUDIO,
  ACCESSKIT_ROLE_BANNER,
  ACCESSKIT_ROLE_BLOCKQUOTE,
  ACCESSKIT_ROLE_CANVAS,
  ACCESSKIT_ROLE_CAPTION,
  ACCESSKIT_ROLE_CARET,
  ACCESSKIT_ROLE_CODE,
  ACCESSKIT_ROLE_COLOR_WELL,
  ACCESSKIT_ROLE_COMBO_BOX,
  ACCESSKIT_ROLE_EDITABLE_COMBO_BOX,
  ACCESSKIT_ROLE_COMPLEMENTARY,
  ACCESSKIT_ROLE_COMMENT,
  ACCESSKIT_ROLE_CONTENT_DELETION,
  ACCESSKIT_ROLE_CONTENT_INSERTION,
  ACCESSKIT_ROLE_CONTENT_INFO,
  ACCESSKIT_ROLE_DEFINITION,
  ACCESSKIT_ROLE_DESCRIPTION_LIST,
  ACCESSKIT_ROLE_DESCRIPTION_LIST_DETAIL,
  ACCESSKIT_ROLE_DESCRIPTION_LIST_TERM,
  ACCESSKIT_ROLE_DETAILS,
  ACCESSKIT_ROLE_DIALOG,
  ACCESSKIT_ROLE_DIRECTORY,
  ACCESSKIT_ROLE_DISCLOSURE_TRIANGLE,
  ACCESSKIT_ROLE_DOCUMENT,
  ACCESSKIT_ROLE_EMBEDDED_OBJECT,
  ACCESSKIT_ROLE_EMPHASIS,
  ACCESSKIT_ROLE_FEED,
  ACCESSKIT_ROLE_FIGURE_CAPTION,
  ACCESSKIT_ROLE_FIGURE,
  ACCESSKIT_ROLE_FOOTER,
  ACCESSKIT_ROLE_FOOTER_AS_NON_LANDMARK,
  ACCESSKIT_ROLE_FORM,
  ACCESSKIT_ROLE_GRID,
  ACCESSKIT_ROLE_GROUP,
  ACCESSKIT_ROLE_HEADER,
  ACCESSKIT_ROLE_HEADER_AS_NON_LANDMARK,
  ACCESSKIT_ROLE_HEADING,
  ACCESSKIT_ROLE_IFRAME,
  ACCESSKIT_ROLE_IFRAME_PRESENTATIONAL,
  ACCESSKIT_ROLE_IME_CANDIDATE,
  ACCESSKIT_ROLE_KEYBOARD,
  ACCESSKIT_ROLE_LEGEND,
  ACCESSKIT_ROLE_LINE_BREAK,
  ACCESSKIT_ROLE_LIST_BOX,
  ACCESSKIT_ROLE_LOG,
  ACCESSKIT_ROLE_MAIN,
  ACCESSKIT_ROLE_MARK,
  ACCESSKIT_ROLE_MARQUEE,
  ACCESSKIT_ROLE_MATH,
  ACCESSKIT_ROLE_MENU_BAR,
  ACCESSKIT_ROLE_MENU_ITEM_CHECK_BOX,
  ACCESSKIT_ROLE_MENU_ITEM_RADIO,
  ACCESSKIT_ROLE_MENU_LIST_POPUP,
  ACCESSKIT_ROLE_METER,
  ACCESSKIT_ROLE_NAVIGATION,
  ACCESSKIT_ROLE_NOTE,
  ACCESSKIT_ROLE_PLUGIN_OBJECT,
  ACCESSKIT_ROLE_PORTAL,
  ACCESSKIT_ROLE_PRE,
  ACCESSKIT_ROLE_PROGRESS_INDICATOR,
  ACCESSKIT_ROLE_RADIO_GROUP,
  ACCESSKIT_ROLE_REGION,
  ACCESSKIT_ROLE_ROOT_WEB_AREA,
  ACCESSKIT_ROLE_RUBY,
  ACCESSKIT_ROLE_RUBY_ANNOTATION,
  ACCESSKIT_ROLE_SCROLL_BAR,
  ACCESSKIT_ROLE_SCROLL_VIEW,
  ACCESSKIT_ROLE_SEARCH,
  ACCESSKIT_ROLE_SECTION,
  ACCESSKIT_ROLE_SLIDER,
  ACCESSKIT_ROLE_SPIN_BUTTON,
  ACCESSKIT_ROLE_SPLITTER,
  ACCESSKIT_ROLE_STATUS,
  ACCESSKIT_ROLE_STRONG,
  ACCESSKIT_ROLE_SUGGESTION,
  ACCESSKIT_ROLE_SVG_ROOT,
  ACCESSKIT_ROLE_TAB,
  ACCESSKIT_ROLE_TAB_LIST,
  ACCESSKIT_ROLE_TAB_PANEL,
  ACCESSKIT_ROLE_TERM,
  ACCESSKIT_ROLE_TIME,
  ACCESSKIT_ROLE_TIMER,
  ACCESSKIT_ROLE_TITLE_BAR,
  ACCESSKIT_ROLE_TOOLBAR,
  ACCESSKIT_ROLE_TOOLTIP,
  ACCESSKIT_ROLE_TREE,
  ACCESSKIT_ROLE_TREE_GRID,
  ACCESSKIT_ROLE_VIDEO,
  ACCESSKIT_ROLE_WEB_VIEW,
  ACCESSKIT_ROLE_WINDOW,
  ACCESSKIT_ROLE_PDF_ACTIONABLE_HIGHLIGHT,
  ACCESSKIT_ROLE_PDF_ROOT,
  ACCESSKIT_ROLE_GRAPHICS_DOCUMENT,
  ACCESSKIT_ROLE_GRAPHICS_OBJECT,
  ACCESSKIT_ROLE_GRAPHICS_SYMBOL,
  ACCESSKIT_ROLE_DOC_ABSTRACT,
  ACCESSKIT_ROLE_DOC_ACKNOWLEDGEMENTS,
  ACCESSKIT_ROLE_DOC_AFTERWORD,
  ACCESSKIT_ROLE_DOC_APPENDIX,
  ACCESSKIT_ROLE_DOC_BACK_LINK,
  ACCESSKIT_ROLE_DOC_BIBLIO_ENTRY,
  ACCESSKIT_ROLE_DOC_BIBLIOGRAPHY,
  ACCESSKIT_ROLE_DOC_BIBLIO_REF,
  ACCESSKIT_ROLE_DOC_CHAPTER,
  ACCESSKIT_ROLE_DOC_COLOPHON,
  ACCESSKIT_ROLE_DOC_CONCLUSION,
  ACCESSKIT_ROLE_DOC_COVER,
  ACCESSKIT_ROLE_DOC_CREDIT,
  ACCESSKIT_ROLE_DOC_CREDITS,
  ACCESSKIT_ROLE_DOC_DEDICATION,
  ACCESSKIT_ROLE_DOC_ENDNOTE,
  ACCESSKIT_ROLE_DOC_ENDNOTES,
  ACCESSKIT_ROLE_DOC_EPIGRAPH,
  ACCESSKIT_ROLE_DOC_EPILOGUE,
  ACCESSKIT_ROLE_DOC_ERRATA,
  ACCESSKIT_ROLE_DOC_EXAMPLE,
  ACCESSKIT_ROLE_DOC_FOOTNOTE,
  ACCESSKIT_ROLE_DOC_FOREWORD,
  ACCESSKIT_ROLE_DOC_GLOSSARY,
  ACCESSKIT_ROLE_DOC_GLOSS_REF,
  ACCESSKIT_ROLE_DOC_INDEX,
  ACCESSKIT_ROLE_DOC_INTRODUCTION,
  ACCESSKIT_ROLE_DOC_NOTE_REF,
  ACCESSKIT_ROLE_DOC_NOTICE,
  ACCESSKIT_ROLE_DOC_PAGE_BREAK,
  ACCESSKIT_ROLE_DOC_PAGE_FOOTER,
  ACCESSKIT_ROLE_DOC_PAGE_HEADER,
  ACCESSKIT_ROLE_DOC_PAGE_LIST,
  ACCESSKIT_ROLE_DOC_PART,
  ACCESSKIT_ROLE_DOC_PREFACE,
  ACCESSKIT_ROLE_DOC_PROLOGUE,
  ACCESSKIT_ROLE_DOC_PULLQUOTE,
  ACCESSKIT_ROLE_DOC_QNA,
  ACCESSKIT_ROLE_DOC_SUBTITLE,
  ACCESSKIT_ROLE_DOC_TIP,
  ACCESSKIT_ROLE_DOC_TOC,
  /**
   * Behaves similar to an ARIA grid but is primarily used by Chromium's
   * `TableView` and its subclasses, so they can be exposed correctly
   * on certain platforms.
   */
  ACCESSKIT_ROLE_LIST_GRID,
  /**
   * This is just like a multi-line document, but signals that assistive
   * technologies should implement behavior specific to a VT-100-style
   * terminal.
   */
  ACCESSKIT_ROLE_TERMINAL,
};
#ifndef __cplusplus
typedef uint8_t accesskit_role;
#endif  // __cplusplus

enum accesskit_sort_direction
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_SORT_DIRECTION_ASCENDING,
  ACCESSKIT_SORT_DIRECTION_DESCENDING,
  ACCESSKIT_SORT_DIRECTION_OTHER,
};
#ifndef __cplusplus
typedef uint8_t accesskit_sort_direction;
#endif  // __cplusplus

enum accesskit_text_align
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_TEXT_ALIGN_LEFT,
  ACCESSKIT_TEXT_ALIGN_RIGHT,
  ACCESSKIT_TEXT_ALIGN_CENTER,
  ACCESSKIT_TEXT_ALIGN_JUSTIFY,
};
#ifndef __cplusplus
typedef uint8_t accesskit_text_align;
#endif  // __cplusplus

enum accesskit_text_decoration
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_TEXT_DECORATION_SOLID,
  ACCESSKIT_TEXT_DECORATION_DOTTED,
  ACCESSKIT_TEXT_DECORATION_DASHED,
  ACCESSKIT_TEXT_DECORATION_DOUBLE,
  ACCESSKIT_TEXT_DECORATION_WAVY,
};
#ifndef __cplusplus
typedef uint8_t accesskit_text_decoration;
#endif  // __cplusplus

enum accesskit_text_direction
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_TEXT_DIRECTION_LEFT_TO_RIGHT,
  ACCESSKIT_TEXT_DIRECTION_RIGHT_TO_LEFT,
  ACCESSKIT_TEXT_DIRECTION_TOP_TO_BOTTOM,
  ACCESSKIT_TEXT_DIRECTION_BOTTOM_TO_TOP,
};
#ifndef __cplusplus
typedef uint8_t accesskit_text_direction;
#endif  // __cplusplus

enum accesskit_toggled
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_TOGGLED_FALSE,
  ACCESSKIT_TOGGLED_TRUE,
  ACCESSKIT_TOGGLED_MIXED,
};
#ifndef __cplusplus
typedef uint8_t accesskit_toggled;
#endif  // __cplusplus

enum accesskit_vertical_offset
#ifdef __cplusplus
    : uint8_t
#endif  // __cplusplus
{
  ACCESSKIT_VERTICAL_OFFSET_SUBSCRIPT,
  ACCESSKIT_VERTICAL_OFFSET_SUPERSCRIPT,
};
#ifndef __cplusplus
typedef uint8_t accesskit_vertical_offset;
#endif  // __cplusplus

#if defined(__APPLE__)
typedef struct accesskit_macos_adapter accesskit_macos_adapter;
#endif

#if defined(__APPLE__)
typedef struct accesskit_macos_queued_events accesskit_macos_queued_events;
#endif

#if defined(__APPLE__)
typedef struct accesskit_macos_subclassing_adapter
    accesskit_macos_subclassing_adapter;
#endif

typedef struct accesskit_node accesskit_node;

typedef struct accesskit_tree accesskit_tree;

typedef struct accesskit_tree_update accesskit_tree_update;

#if (defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
     defined(__NetBSD__) || defined(__OpenBSD__))
typedef struct accesskit_unix_adapter accesskit_unix_adapter;
#endif

#if defined(_WIN32)
typedef struct accesskit_windows_adapter accesskit_windows_adapter;
#endif

#if defined(_WIN32)
typedef struct accesskit_windows_queued_events accesskit_windows_queued_events;
#endif

#if defined(_WIN32)
typedef struct accesskit_windows_subclassing_adapter
    accesskit_windows_subclassing_adapter;
#endif

typedef uint64_t accesskit_node_id;

typedef struct accesskit_node_ids {
  size_t length;
  const accesskit_node_id *values;
} accesskit_node_ids;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_node_id {
  bool has_value;
  accesskit_node_id value;
} accesskit_opt_node_id;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_double {
  bool has_value;
  double value;
} accesskit_opt_double;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_index {
  bool has_value;
  size_t value;
} accesskit_opt_index;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_color {
  bool has_value;
  uint32_t value;
} accesskit_opt_color;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_text_decoration {
  bool has_value;
  accesskit_text_decoration value;
} accesskit_opt_text_decoration;

typedef struct accesskit_lengths {
  size_t length;
  const uint8_t *values;
} accesskit_lengths;

typedef struct accesskit_opt_coords {
  bool has_value;
  size_t length;
  const float *values;
} accesskit_opt_coords;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_bool {
  bool has_value;
  bool value;
} accesskit_opt_bool;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_invalid {
  bool has_value;
  accesskit_invalid value;
} accesskit_opt_invalid;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_toggled {
  bool has_value;
  accesskit_toggled value;
} accesskit_opt_toggled;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_live {
  bool has_value;
  accesskit_live value;
} accesskit_opt_live;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_text_direction {
  bool has_value;
  accesskit_text_direction value;
} accesskit_opt_text_direction;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_orientation {
  bool has_value;
  accesskit_orientation value;
} accesskit_opt_orientation;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_sort_direction {
  bool has_value;
  accesskit_sort_direction value;
} accesskit_opt_sort_direction;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_aria_current {
  bool has_value;
  accesskit_aria_current value;
} accesskit_opt_aria_current;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_auto_complete {
  bool has_value;
  accesskit_auto_complete value;
} accesskit_opt_auto_complete;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_has_popup {
  bool has_value;
  accesskit_has_popup value;
} accesskit_opt_has_popup;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_list_style {
  bool has_value;
  accesskit_list_style value;
} accesskit_opt_list_style;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_text_align {
  bool has_value;
  accesskit_text_align value;
} accesskit_opt_text_align;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_vertical_offset {
  bool has_value;
  accesskit_vertical_offset value;
} accesskit_opt_vertical_offset;

/**
 * A 2D affine transform. Derived from
 * [kurbo](https://github.com/linebender/kurbo).
 */
typedef struct accesskit_affine {
  double _0[6];
} accesskit_affine;

/**
 * A rectangle. Derived from [kurbo](https://github.com/linebender/kurbo).
 */
typedef struct accesskit_rect {
  /**
   * The minimum x coordinate (left edge).
   */
  double x0;
  /**
   * The minimum y coordinate (top edge in y-down spaces).
   */
  double y0;
  /**
   * The maximum x coordinate (right edge).
   */
  double x1;
  /**
   * The maximum y coordinate (bottom edge in y-down spaces).
   */
  double y1;
} accesskit_rect;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_rect {
  bool has_value;
  struct accesskit_rect value;
} accesskit_opt_rect;

typedef struct accesskit_text_position {
  accesskit_node_id node;
  size_t character_index;
} accesskit_text_position;

typedef struct accesskit_text_selection {
  struct accesskit_text_position anchor;
  struct accesskit_text_position focus;
} accesskit_text_selection;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_text_selection {
  bool has_value;
  struct accesskit_text_selection value;
} accesskit_opt_text_selection;

/**
 * Use `accesskit_custom_action_new` to create this struct. Do not reallocate
 * `description`.
 *
 * When you get this struct, you are responsible for freeing `description`.
 */
typedef struct accesskit_custom_action {
  int32_t id;
  char *description;
} accesskit_custom_action;

typedef struct accesskit_custom_actions {
  size_t length;
  struct accesskit_custom_action *values;
} accesskit_custom_actions;

/**
 * A 2D point. Derived from [kurbo](https://github.com/linebender/kurbo).
 */
typedef struct accesskit_point {
  /**
   * The x coordinate.
   */
  double x;
  /**
   * The y coordinate.
   */
  double y;
} accesskit_point;

typedef enum accesskit_action_data_Tag {
  ACCESSKIT_ACTION_DATA_CUSTOM_ACTION,
  ACCESSKIT_ACTION_DATA_VALUE,
  ACCESSKIT_ACTION_DATA_NUMERIC_VALUE,
  ACCESSKIT_ACTION_DATA_SCROLL_TARGET_RECT,
  ACCESSKIT_ACTION_DATA_SCROLL_TO_POINT,
  ACCESSKIT_ACTION_DATA_SET_SCROLL_OFFSET,
  ACCESSKIT_ACTION_DATA_SET_TEXT_SELECTION,
} accesskit_action_data_Tag;

typedef struct accesskit_action_data {
  accesskit_action_data_Tag tag;
  union {
    struct {
      int32_t custom_action;
    };
    struct {
      char *value;
    };
    struct {
      double numeric_value;
    };
    struct {
      struct accesskit_rect scroll_target_rect;
    };
    struct {
      struct accesskit_point scroll_to_point;
    };
    struct {
      struct accesskit_point set_scroll_offset;
    };
    struct {
      struct accesskit_text_selection set_text_selection;
    };
  };
} accesskit_action_data;

/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_action_data {
  bool has_value;
  struct accesskit_action_data value;
} accesskit_opt_action_data;

typedef struct accesskit_action_request {
  accesskit_action action;
  accesskit_node_id target;
  struct accesskit_opt_action_data data;
} accesskit_action_request;

/**
 * A 2D vector. Derived from [kurbo](https://github.com/linebender/kurbo).
 *
 * This is intended primarily for a vector in the mathematical sense,
 * but it can be interpreted as a translation, and converted to and
 * from a point (vector relative to the origin) and size.
 */
typedef struct accesskit_vec2 {
  /**
   * The x-coordinate.
   */
  double x;
  /**
   * The y-coordinate.
   */
  double y;
} accesskit_vec2;

/**
 * A 2D size. Derived from [kurbo](https://github.com/linebender/kurbo).
 */
typedef struct accesskit_size {
  /**
   * The width.
   */
  double width;
  /**
   * The height.
   */
  double height;
} accesskit_size;

/**
 * Ownership of `request` is transferred to the callback. `request` must
 * be freed using `accesskit_action_request_free`.
 */
typedef void (*accesskit_action_handler_callback)(
    struct accesskit_action_request *request, void *userdata);

typedef void *accesskit_tree_update_factory_userdata;

/**
 * This function can't return a null pointer. Ownership of the returned value
 * will be transferred to the caller.
 */
typedef struct accesskit_tree_update *(*accesskit_tree_update_factory)(
    accesskit_tree_update_factory_userdata);

typedef struct accesskit_tree_update *(*accesskit_activation_handler_callback)(
    void *userdata);

typedef void (*accesskit_deactivation_handler_callback)(void *userdata);

#if defined(_WIN32)
/**
 * Represents an optional value.
 *
 * If `has_value` is false, do not read the `value` field.
 */
typedef struct accesskit_opt_lresult {
  bool has_value;
  LRESULT value;
} accesskit_opt_lresult;
#endif

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

accesskit_role accesskit_node_role(const struct accesskit_node *node);

void accesskit_node_set_role(struct accesskit_node *node, accesskit_role value);

bool accesskit_node_supports_action(const struct accesskit_node *node,
                                    accesskit_action action);

void accesskit_node_add_action(struct accesskit_node *node,
                               accesskit_action action);

void accesskit_node_remove_action(struct accesskit_node *node,
                                  accesskit_action action);

void accesskit_node_clear_actions(struct accesskit_node *node);

bool accesskit_node_is_hidden(const struct accesskit_node *node);

void accesskit_node_set_hidden(struct accesskit_node *node);

void accesskit_node_clear_hidden(struct accesskit_node *node);

bool accesskit_node_is_linked(const struct accesskit_node *node);

void accesskit_node_set_linked(struct accesskit_node *node);

void accesskit_node_clear_linked(struct accesskit_node *node);

bool accesskit_node_is_multiselectable(const struct accesskit_node *node);

void accesskit_node_set_multiselectable(struct accesskit_node *node);

void accesskit_node_clear_multiselectable(struct accesskit_node *node);

bool accesskit_node_is_required(const struct accesskit_node *node);

void accesskit_node_set_required(struct accesskit_node *node);

void accesskit_node_clear_required(struct accesskit_node *node);

bool accesskit_node_is_visited(const struct accesskit_node *node);

void accesskit_node_set_visited(struct accesskit_node *node);

void accesskit_node_clear_visited(struct accesskit_node *node);

bool accesskit_node_is_busy(const struct accesskit_node *node);

void accesskit_node_set_busy(struct accesskit_node *node);

void accesskit_node_clear_busy(struct accesskit_node *node);

bool accesskit_node_is_live_atomic(const struct accesskit_node *node);

void accesskit_node_set_live_atomic(struct accesskit_node *node);

void accesskit_node_clear_live_atomic(struct accesskit_node *node);

bool accesskit_node_is_modal(const struct accesskit_node *node);

void accesskit_node_set_modal(struct accesskit_node *node);

void accesskit_node_clear_modal(struct accesskit_node *node);

bool accesskit_node_is_touch_transparent(const struct accesskit_node *node);

void accesskit_node_set_touch_transparent(struct accesskit_node *node);

void accesskit_node_clear_touch_transparent(struct accesskit_node *node);

bool accesskit_node_is_read_only(const struct accesskit_node *node);

void accesskit_node_set_read_only(struct accesskit_node *node);

void accesskit_node_clear_read_only(struct accesskit_node *node);

bool accesskit_node_is_disabled(const struct accesskit_node *node);

void accesskit_node_set_disabled(struct accesskit_node *node);

void accesskit_node_clear_disabled(struct accesskit_node *node);

bool accesskit_node_is_bold(const struct accesskit_node *node);

void accesskit_node_set_bold(struct accesskit_node *node);

void accesskit_node_clear_bold(struct accesskit_node *node);

bool accesskit_node_is_italic(const struct accesskit_node *node);

void accesskit_node_set_italic(struct accesskit_node *node);

void accesskit_node_clear_italic(struct accesskit_node *node);

bool accesskit_node_clips_children(const struct accesskit_node *node);

void accesskit_node_set_clips_children(struct accesskit_node *node);

void accesskit_node_clear_clips_children(struct accesskit_node *node);

bool accesskit_node_is_line_breaking_object(const struct accesskit_node *node);

void accesskit_node_set_is_line_breaking_object(struct accesskit_node *node);

void accesskit_node_clear_is_line_breaking_object(struct accesskit_node *node);

bool accesskit_node_is_page_breaking_object(const struct accesskit_node *node);

void accesskit_node_set_is_page_breaking_object(struct accesskit_node *node);

void accesskit_node_clear_is_page_breaking_object(struct accesskit_node *node);

bool accesskit_node_is_spelling_error(const struct accesskit_node *node);

void accesskit_node_set_is_spelling_error(struct accesskit_node *node);

void accesskit_node_clear_is_spelling_error(struct accesskit_node *node);

bool accesskit_node_is_grammar_error(const struct accesskit_node *node);

void accesskit_node_set_is_grammar_error(struct accesskit_node *node);

void accesskit_node_clear_is_grammar_error(struct accesskit_node *node);

bool accesskit_node_is_search_match(const struct accesskit_node *node);

void accesskit_node_set_is_search_match(struct accesskit_node *node);

void accesskit_node_clear_is_search_match(struct accesskit_node *node);

bool accesskit_node_is_suggestion(const struct accesskit_node *node);

void accesskit_node_set_is_suggestion(struct accesskit_node *node);

void accesskit_node_clear_is_suggestion(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_children(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_children(struct accesskit_node *node, size_t length,
                                 const accesskit_node_id *values);

void accesskit_node_push_child(struct accesskit_node *node,
                               accesskit_node_id item);

void accesskit_node_clear_children(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_controls(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_controls(struct accesskit_node *node, size_t length,
                                 const accesskit_node_id *values);

void accesskit_node_push_controlled(struct accesskit_node *node,
                                    accesskit_node_id item);

void accesskit_node_clear_controls(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_details(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_details(struct accesskit_node *node, size_t length,
                                const accesskit_node_id *values);

void accesskit_node_push_detail(struct accesskit_node *node,
                                accesskit_node_id item);

void accesskit_node_clear_details(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_described_by(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_described_by(struct accesskit_node *node, size_t length,
                                     const accesskit_node_id *values);

void accesskit_node_push_described_by(struct accesskit_node *node,
                                      accesskit_node_id item);

void accesskit_node_clear_described_by(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_flow_to(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_flow_to(struct accesskit_node *node, size_t length,
                                const accesskit_node_id *values);

void accesskit_node_push_flow_to(struct accesskit_node *node,
                                 accesskit_node_id item);

void accesskit_node_clear_flow_to(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_labelled_by(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_labelled_by(struct accesskit_node *node, size_t length,
                                    const accesskit_node_id *values);

void accesskit_node_push_labelled_by(struct accesskit_node *node,
                                     accesskit_node_id item);

void accesskit_node_clear_labelled_by(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_owns(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_owns(struct accesskit_node *node, size_t length,
                             const accesskit_node_id *values);

void accesskit_node_push_owned(struct accesskit_node *node,
                               accesskit_node_id item);

void accesskit_node_clear_owns(struct accesskit_node *node);

struct accesskit_node_ids accesskit_node_radio_group(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_radio_group(struct accesskit_node *node, size_t length,
                                    const accesskit_node_id *values);

void accesskit_node_push_to_radio_group(struct accesskit_node *node,
                                        accesskit_node_id item);

void accesskit_node_clear_radio_group(struct accesskit_node *node);

struct accesskit_opt_node_id accesskit_node_active_descendant(
    const struct accesskit_node *node);

void accesskit_node_set_active_descendant(struct accesskit_node *node,
                                          accesskit_node_id value);

void accesskit_node_clear_active_descendant(struct accesskit_node *node);

struct accesskit_opt_node_id accesskit_node_error_message(
    const struct accesskit_node *node);

void accesskit_node_set_error_message(struct accesskit_node *node,
                                      accesskit_node_id value);

void accesskit_node_clear_error_message(struct accesskit_node *node);

struct accesskit_opt_node_id accesskit_node_in_page_link_target(
    const struct accesskit_node *node);

void accesskit_node_set_in_page_link_target(struct accesskit_node *node,
                                            accesskit_node_id value);

void accesskit_node_clear_in_page_link_target(struct accesskit_node *node);

struct accesskit_opt_node_id accesskit_node_member_of(
    const struct accesskit_node *node);

void accesskit_node_set_member_of(struct accesskit_node *node,
                                  accesskit_node_id value);

void accesskit_node_clear_member_of(struct accesskit_node *node);

struct accesskit_opt_node_id accesskit_node_next_on_line(
    const struct accesskit_node *node);

void accesskit_node_set_next_on_line(struct accesskit_node *node,
                                     accesskit_node_id value);

void accesskit_node_clear_next_on_line(struct accesskit_node *node);

struct accesskit_opt_node_id accesskit_node_previous_on_line(
    const struct accesskit_node *node);

void accesskit_node_set_previous_on_line(struct accesskit_node *node,
                                         accesskit_node_id value);

void accesskit_node_clear_previous_on_line(struct accesskit_node *node);

struct accesskit_opt_node_id accesskit_node_popup_for(
    const struct accesskit_node *node);

void accesskit_node_set_popup_for(struct accesskit_node *node,
                                  accesskit_node_id value);

void accesskit_node_clear_popup_for(struct accesskit_node *node);

/**
 * Only call this function with a string that originated from AccessKit.
 */
void accesskit_string_free(char *string);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_label(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_label(struct accesskit_node *node, const char *value);

void accesskit_node_clear_label(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_description(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_description(struct accesskit_node *node,
                                    const char *value);

void accesskit_node_clear_description(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_value(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_value(struct accesskit_node *node, const char *value);

void accesskit_node_clear_value(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_access_key(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_access_key(struct accesskit_node *node,
                                   const char *value);

void accesskit_node_clear_access_key(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_author_id(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_author_id(struct accesskit_node *node,
                                  const char *value);

void accesskit_node_clear_author_id(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_class_name(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_class_name(struct accesskit_node *node,
                                   const char *value);

void accesskit_node_clear_class_name(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_font_family(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_font_family(struct accesskit_node *node,
                                    const char *value);

void accesskit_node_clear_font_family(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_html_tag(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_html_tag(struct accesskit_node *node,
                                 const char *value);

void accesskit_node_clear_html_tag(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_inner_html(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_inner_html(struct accesskit_node *node,
                                   const char *value);

void accesskit_node_clear_inner_html(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_keyboard_shortcut(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_keyboard_shortcut(struct accesskit_node *node,
                                          const char *value);

void accesskit_node_clear_keyboard_shortcut(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_language(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_language(struct accesskit_node *node,
                                 const char *value);

void accesskit_node_clear_language(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_placeholder(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_placeholder(struct accesskit_node *node,
                                    const char *value);

void accesskit_node_clear_placeholder(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_role_description(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_role_description(struct accesskit_node *node,
                                         const char *value);

void accesskit_node_clear_role_description(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_state_description(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_state_description(struct accesskit_node *node,
                                          const char *value);

void accesskit_node_clear_state_description(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_tooltip(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_tooltip(struct accesskit_node *node, const char *value);

void accesskit_node_clear_tooltip(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_url(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_url(struct accesskit_node *node, const char *value);

void accesskit_node_clear_url(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_row_index_text(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_row_index_text(struct accesskit_node *node,
                                       const char *value);

void accesskit_node_clear_row_index_text(struct accesskit_node *node);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_node_column_index_text(const struct accesskit_node *node);

/**
 * Caller is responsible for freeing the memory pointed by `value`.
 */
void accesskit_node_set_column_index_text(struct accesskit_node *node,
                                          const char *value);

void accesskit_node_clear_column_index_text(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_scroll_x(
    const struct accesskit_node *node);

void accesskit_node_set_scroll_x(struct accesskit_node *node, double value);

void accesskit_node_clear_scroll_x(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_scroll_x_min(
    const struct accesskit_node *node);

void accesskit_node_set_scroll_x_min(struct accesskit_node *node, double value);

void accesskit_node_clear_scroll_x_min(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_scroll_x_max(
    const struct accesskit_node *node);

void accesskit_node_set_scroll_x_max(struct accesskit_node *node, double value);

void accesskit_node_clear_scroll_x_max(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_scroll_y(
    const struct accesskit_node *node);

void accesskit_node_set_scroll_y(struct accesskit_node *node, double value);

void accesskit_node_clear_scroll_y(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_scroll_y_min(
    const struct accesskit_node *node);

void accesskit_node_set_scroll_y_min(struct accesskit_node *node, double value);

void accesskit_node_clear_scroll_y_min(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_scroll_y_max(
    const struct accesskit_node *node);

void accesskit_node_set_scroll_y_max(struct accesskit_node *node, double value);

void accesskit_node_clear_scroll_y_max(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_numeric_value(
    const struct accesskit_node *node);

void accesskit_node_set_numeric_value(struct accesskit_node *node,
                                      double value);

void accesskit_node_clear_numeric_value(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_min_numeric_value(
    const struct accesskit_node *node);

void accesskit_node_set_min_numeric_value(struct accesskit_node *node,
                                          double value);

void accesskit_node_clear_min_numeric_value(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_max_numeric_value(
    const struct accesskit_node *node);

void accesskit_node_set_max_numeric_value(struct accesskit_node *node,
                                          double value);

void accesskit_node_clear_max_numeric_value(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_numeric_value_step(
    const struct accesskit_node *node);

void accesskit_node_set_numeric_value_step(struct accesskit_node *node,
                                           double value);

void accesskit_node_clear_numeric_value_step(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_numeric_value_jump(
    const struct accesskit_node *node);

void accesskit_node_set_numeric_value_jump(struct accesskit_node *node,
                                           double value);

void accesskit_node_clear_numeric_value_jump(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_font_size(
    const struct accesskit_node *node);

void accesskit_node_set_font_size(struct accesskit_node *node, double value);

void accesskit_node_clear_font_size(struct accesskit_node *node);

struct accesskit_opt_double accesskit_node_font_weight(
    const struct accesskit_node *node);

void accesskit_node_set_font_weight(struct accesskit_node *node, double value);

void accesskit_node_clear_font_weight(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_row_count(
    const struct accesskit_node *node);

void accesskit_node_set_row_count(struct accesskit_node *node, size_t value);

void accesskit_node_clear_row_count(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_column_count(
    const struct accesskit_node *node);

void accesskit_node_set_column_count(struct accesskit_node *node, size_t value);

void accesskit_node_clear_column_count(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_row_index(
    const struct accesskit_node *node);

void accesskit_node_set_row_index(struct accesskit_node *node, size_t value);

void accesskit_node_clear_row_index(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_column_index(
    const struct accesskit_node *node);

void accesskit_node_set_column_index(struct accesskit_node *node, size_t value);

void accesskit_node_clear_column_index(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_row_span(
    const struct accesskit_node *node);

void accesskit_node_set_row_span(struct accesskit_node *node, size_t value);

void accesskit_node_clear_row_span(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_column_span(
    const struct accesskit_node *node);

void accesskit_node_set_column_span(struct accesskit_node *node, size_t value);

void accesskit_node_clear_column_span(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_level(
    const struct accesskit_node *node);

void accesskit_node_set_level(struct accesskit_node *node, size_t value);

void accesskit_node_clear_level(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_size_of_set(
    const struct accesskit_node *node);

void accesskit_node_set_size_of_set(struct accesskit_node *node, size_t value);

void accesskit_node_clear_size_of_set(struct accesskit_node *node);

struct accesskit_opt_index accesskit_node_position_in_set(
    const struct accesskit_node *node);

void accesskit_node_set_position_in_set(struct accesskit_node *node,
                                        size_t value);

void accesskit_node_clear_position_in_set(struct accesskit_node *node);

struct accesskit_opt_color accesskit_node_color_value(
    const struct accesskit_node *node);

void accesskit_node_set_color_value(struct accesskit_node *node,
                                    uint32_t value);

void accesskit_node_clear_color_value(struct accesskit_node *node);

struct accesskit_opt_color accesskit_node_background_color(
    const struct accesskit_node *node);

void accesskit_node_set_background_color(struct accesskit_node *node,
                                         uint32_t value);

void accesskit_node_clear_background_color(struct accesskit_node *node);

struct accesskit_opt_color accesskit_node_foreground_color(
    const struct accesskit_node *node);

void accesskit_node_set_foreground_color(struct accesskit_node *node,
                                         uint32_t value);

void accesskit_node_clear_foreground_color(struct accesskit_node *node);

struct accesskit_opt_text_decoration accesskit_node_overline(
    const struct accesskit_node *node);

void accesskit_node_set_overline(struct accesskit_node *node,
                                 accesskit_text_decoration value);

void accesskit_node_clear_overline(struct accesskit_node *node);

struct accesskit_opt_text_decoration accesskit_node_strikethrough(
    const struct accesskit_node *node);

void accesskit_node_set_strikethrough(struct accesskit_node *node,
                                      accesskit_text_decoration value);

void accesskit_node_clear_strikethrough(struct accesskit_node *node);

struct accesskit_opt_text_decoration accesskit_node_underline(
    const struct accesskit_node *node);

void accesskit_node_set_underline(struct accesskit_node *node,
                                  accesskit_text_decoration value);

void accesskit_node_clear_underline(struct accesskit_node *node);

struct accesskit_lengths accesskit_node_character_lengths(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_character_lengths(struct accesskit_node *node,
                                          size_t length, const uint8_t *values);

void accesskit_node_clear_character_lengths(struct accesskit_node *node);

struct accesskit_lengths accesskit_node_word_lengths(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_word_lengths(struct accesskit_node *node, size_t length,
                                     const uint8_t *values);

void accesskit_node_clear_word_lengths(struct accesskit_node *node);

struct accesskit_opt_coords accesskit_node_character_positions(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_character_positions(struct accesskit_node *node,
                                            size_t length, const float *values);

void accesskit_node_clear_character_positions(struct accesskit_node *node);

struct accesskit_opt_coords accesskit_node_character_widths(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_character_widths(struct accesskit_node *node,
                                         size_t length, const float *values);

void accesskit_node_clear_character_widths(struct accesskit_node *node);

struct accesskit_opt_bool accesskit_node_is_expanded(
    const struct accesskit_node *node);

void accesskit_node_set_expanded(struct accesskit_node *node, bool value);

void accesskit_node_clear_expanded(struct accesskit_node *node);

struct accesskit_opt_bool accesskit_node_is_selected(
    const struct accesskit_node *node);

void accesskit_node_set_selected(struct accesskit_node *node, bool value);

void accesskit_node_clear_selected(struct accesskit_node *node);

struct accesskit_opt_invalid accesskit_node_invalid(
    const struct accesskit_node *node);

void accesskit_node_set_invalid(struct accesskit_node *node,
                                accesskit_invalid value);

void accesskit_node_clear_invalid(struct accesskit_node *node);

struct accesskit_opt_toggled accesskit_node_toggled(
    const struct accesskit_node *node);

void accesskit_node_set_toggled(struct accesskit_node *node,
                                accesskit_toggled value);

void accesskit_node_clear_toggled(struct accesskit_node *node);

struct accesskit_opt_live accesskit_node_live(
    const struct accesskit_node *node);

void accesskit_node_set_live(struct accesskit_node *node, accesskit_live value);

void accesskit_node_clear_live(struct accesskit_node *node);

struct accesskit_opt_text_direction accesskit_node_text_direction(
    const struct accesskit_node *node);

void accesskit_node_set_text_direction(struct accesskit_node *node,
                                       accesskit_text_direction value);

void accesskit_node_clear_text_direction(struct accesskit_node *node);

struct accesskit_opt_orientation accesskit_node_orientation(
    const struct accesskit_node *node);

void accesskit_node_set_orientation(struct accesskit_node *node,
                                    accesskit_orientation value);

void accesskit_node_clear_orientation(struct accesskit_node *node);

struct accesskit_opt_sort_direction accesskit_node_sort_direction(
    const struct accesskit_node *node);

void accesskit_node_set_sort_direction(struct accesskit_node *node,
                                       accesskit_sort_direction value);

void accesskit_node_clear_sort_direction(struct accesskit_node *node);

struct accesskit_opt_aria_current accesskit_node_aria_current(
    const struct accesskit_node *node);

void accesskit_node_set_aria_current(struct accesskit_node *node,
                                     accesskit_aria_current value);

void accesskit_node_clear_aria_current(struct accesskit_node *node);

struct accesskit_opt_auto_complete accesskit_node_auto_complete(
    const struct accesskit_node *node);

void accesskit_node_set_auto_complete(struct accesskit_node *node,
                                      accesskit_auto_complete value);

void accesskit_node_clear_auto_complete(struct accesskit_node *node);

struct accesskit_opt_has_popup accesskit_node_has_popup(
    const struct accesskit_node *node);

void accesskit_node_set_has_popup(struct accesskit_node *node,
                                  accesskit_has_popup value);

void accesskit_node_clear_has_popup(struct accesskit_node *node);

struct accesskit_opt_list_style accesskit_node_list_style(
    const struct accesskit_node *node);

void accesskit_node_set_list_style(struct accesskit_node *node,
                                   accesskit_list_style value);

void accesskit_node_clear_list_style(struct accesskit_node *node);

struct accesskit_opt_text_align accesskit_node_text_align(
    const struct accesskit_node *node);

void accesskit_node_set_text_align(struct accesskit_node *node,
                                   accesskit_text_align value);

void accesskit_node_clear_text_align(struct accesskit_node *node);

struct accesskit_opt_vertical_offset accesskit_node_vertical_offset(
    const struct accesskit_node *node);

void accesskit_node_set_vertical_offset(struct accesskit_node *node,
                                        accesskit_vertical_offset value);

void accesskit_node_clear_vertical_offset(struct accesskit_node *node);

const struct accesskit_affine *accesskit_node_transform(
    const struct accesskit_node *node);

void accesskit_node_set_transform(struct accesskit_node *node,
                                  struct accesskit_affine value);

void accesskit_node_clear_transform(struct accesskit_node *node);

struct accesskit_opt_rect accesskit_node_bounds(
    const struct accesskit_node *node);

void accesskit_node_set_bounds(struct accesskit_node *node,
                               struct accesskit_rect value);

void accesskit_node_clear_bounds(struct accesskit_node *node);

struct accesskit_opt_text_selection accesskit_node_text_selection(
    const struct accesskit_node *node);

void accesskit_node_set_text_selection(struct accesskit_node *node,
                                       struct accesskit_text_selection value);

void accesskit_node_clear_text_selection(struct accesskit_node *node);

struct accesskit_custom_action accesskit_custom_action_new(
    int32_t id, const char *description);

void accesskit_custom_actions_free(struct accesskit_custom_actions *value);

/**
 * Caller is responsible for freeing the returned value.
 */
const struct accesskit_custom_actions *accesskit_node_custom_actions(
    const struct accesskit_node *node);

/**
 * Caller is responsible for freeing `values`.
 */
void accesskit_node_set_custom_actions(
    struct accesskit_node *node, size_t length,
    const struct accesskit_custom_action *values);

void accesskit_node_push_custom_action(struct accesskit_node *node,
                                       struct accesskit_custom_action item);

void accesskit_node_clear_custom_actions(struct accesskit_node *node);

struct accesskit_node *accesskit_node_new(accesskit_role role);

void accesskit_node_free(struct accesskit_node *node);

struct accesskit_tree *accesskit_tree_new(accesskit_node_id root);

void accesskit_tree_free(struct accesskit_tree *tree);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_tree_get_app_name(const struct accesskit_tree *tree);

void accesskit_tree_set_app_name(struct accesskit_tree *tree,
                                 const char *app_name);

void accesskit_tree_clear_app_name(struct accesskit_tree *tree);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_tree_get_toolkit_name(const struct accesskit_tree *tree);

void accesskit_tree_set_toolkit_name(struct accesskit_tree *tree,
                                     const char *toolkit_name);

void accesskit_tree_clear_toolkit_name(struct accesskit_tree *tree);

/**
 * Caller must call `accesskit_string_free` with the return value.
 */
char *accesskit_tree_get_toolkit_version(const struct accesskit_tree *tree);

void accesskit_tree_set_toolkit_version(struct accesskit_tree *tree,
                                        const char *toolkit_version);

void accesskit_tree_clear_toolkit_version(struct accesskit_tree *tree);

struct accesskit_tree_update *accesskit_tree_update_with_focus(
    accesskit_node_id focus);

struct accesskit_tree_update *accesskit_tree_update_with_capacity_and_focus(
    size_t capacity, accesskit_node_id focus);

void accesskit_tree_update_free(struct accesskit_tree_update *update);

/**
 * Appends the provided node to the tree update's list of nodes.
 * Takes ownership of `node`.
 */
void accesskit_tree_update_push_node(struct accesskit_tree_update *update,
                                     accesskit_node_id id,
                                     struct accesskit_node *node);

void accesskit_tree_update_set_tree(struct accesskit_tree_update *update,
                                    struct accesskit_tree *tree);

void accesskit_tree_update_clear_tree(struct accesskit_tree_update *update);

void accesskit_tree_update_set_focus(struct accesskit_tree_update *update,
                                     accesskit_node_id focus);

void accesskit_action_request_free(struct accesskit_action_request *request);

struct accesskit_affine accesskit_affine_identity(void);

struct accesskit_affine accesskit_affine_flip_y(void);

struct accesskit_affine accesskit_affine_flip_x(void);

struct accesskit_affine accesskit_affine_scale(double s);

struct accesskit_affine accesskit_affine_scale_non_uniform(double s_x,
                                                           double s_y);

struct accesskit_affine accesskit_affine_translate(struct accesskit_vec2 p);

struct accesskit_affine accesskit_affine_map_unit_square(
    struct accesskit_rect rect);

double accesskit_affine_determinant(struct accesskit_affine affine);

struct accesskit_affine accesskit_affine_inverse(
    struct accesskit_affine affine);

struct accesskit_rect accesskit_affine_transform_rect_bbox(
    struct accesskit_affine affine, struct accesskit_rect rect);

bool accesskit_affine_is_finite(const struct accesskit_affine *affine);

bool accesskit_affine_is_nan(const struct accesskit_affine *affine);

struct accesskit_vec2 accesskit_point_to_vec2(struct accesskit_point point);

struct accesskit_rect accesskit_rect_from_points(struct accesskit_point p0,
                                                 struct accesskit_point p1);

struct accesskit_rect accesskit_rect_from_origin_size(
    struct accesskit_point origin, struct accesskit_size size);

struct accesskit_rect accesskit_rect_with_origin(struct accesskit_rect rect,
                                                 struct accesskit_point origin);

struct accesskit_rect accesskit_rect_with_size(struct accesskit_rect rect,
                                               struct accesskit_size size);

double accesskit_rect_width(const struct accesskit_rect *rect);

double accesskit_rect_height(const struct accesskit_rect *rect);

double accesskit_rect_min_x(const struct accesskit_rect *rect);

double accesskit_rect_max_x(const struct accesskit_rect *rect);

double accesskit_rect_min_y(const struct accesskit_rect *rect);

double accesskit_rect_max_y(const struct accesskit_rect *rect);

struct accesskit_point accesskit_rect_origin(const struct accesskit_rect *rect);

struct accesskit_size accesskit_rect_size(const struct accesskit_rect *rect);

struct accesskit_rect accesskit_rect_abs(const struct accesskit_rect *rect);

double accesskit_rect_area(const struct accesskit_rect *rect);

bool accesskit_rect_is_empty(const struct accesskit_rect *rect);

bool accesskit_rect_contains(const struct accesskit_rect *rect,
                             struct accesskit_point point);

struct accesskit_rect accesskit_rect_union(const struct accesskit_rect *rect,
                                           struct accesskit_rect other);

struct accesskit_rect accesskit_rect_union_pt(const struct accesskit_rect *rect,
                                              struct accesskit_point pt);

struct accesskit_rect accesskit_rect_intersect(
    const struct accesskit_rect *rect, struct accesskit_rect other);

struct accesskit_vec2 accesskit_size_to_vec2(struct accesskit_size size);

struct accesskit_point accesskit_vec2_to_point(struct accesskit_vec2 vec2);

struct accesskit_size accesskit_vec2_to_size(struct accesskit_vec2 vec2);

#if defined(__APPLE__)
/**
 * Memory is also freed when calling this function.
 */
void accesskit_macos_queued_events_raise(
    struct accesskit_macos_queued_events *events);
#endif

#if defined(__APPLE__)
/**
 * # Safety
 *
 * `view` must be a valid, unreleased pointer to an `NSView`.
 */
struct accesskit_macos_adapter *accesskit_macos_adapter_new(
    void *view, bool is_view_focused,
    accesskit_action_handler_callback action_handler,
    void *action_handler_userdata);
#endif

#if defined(__APPLE__)
void accesskit_macos_adapter_free(struct accesskit_macos_adapter *adapter);
#endif

#if defined(__APPLE__)
/**
 * You must call `accesskit_macos_queued_events_raise` on the returned pointer.
 * It can be null if the adapter is not active.
 */
struct accesskit_macos_queued_events *accesskit_macos_adapter_update_if_active(
    struct accesskit_macos_adapter *adapter,
    accesskit_tree_update_factory update_factory,
    void *update_factory_userdata);
#endif

#if defined(__APPLE__)
/**
 * Update the tree state based on whether the window is focused.
 *
 * You must call `accesskit_macos_queued_events_raise` on the returned pointer.
 * It can be null if the adapter is not active.
 */
struct accesskit_macos_queued_events *
accesskit_macos_adapter_update_view_focus_state(
    struct accesskit_macos_adapter *adapter, bool is_focused);
#endif

#if defined(__APPLE__)
/**
 * Returns a pointer to an `NSArray`. Ownership of the pointer is not
 * transferred.
 */
void *accesskit_macos_adapter_view_children(
    struct accesskit_macos_adapter *adapter,
    accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata);
#endif

#if defined(__APPLE__)
/**
 * Returns a pointer to an `NSObject`. Ownership of the pointer is not
 * transferred.
 */
void *accesskit_macos_adapter_focus(
    struct accesskit_macos_adapter *adapter,
    accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata);
#endif

#if defined(__APPLE__)
/**
 * Returns a pointer to an `NSObject`. Ownership of the pointer is not
 * transferred.
 */
void *accesskit_macos_adapter_hit_test(
    struct accesskit_macos_adapter *adapter, double x, double y,
    accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata);
#endif

#if defined(__APPLE__)
/**
 * # Safety
 *
 * `view` must be a valid, unreleased pointer to an `NSView`.
 */
struct accesskit_macos_subclassing_adapter *
accesskit_macos_subclassing_adapter_new(
    void *view, accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata,
    accesskit_action_handler_callback action_handler,
    void *action_handler_userdata);
#endif

#if defined(__APPLE__)
/**
 * # Safety
 *
 * `window` must be a valid, unreleased pointer to an `NSWindow`.
 *
 * # Panics
 *
 * This function panics if the specified window doesn't currently have
 * a content view.
 */
struct accesskit_macos_subclassing_adapter *
accesskit_macos_subclassing_adapter_for_window(
    void *window, accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata,
    accesskit_action_handler_callback action_handler,
    void *action_handler_userdata);
#endif

#if defined(__APPLE__)
void accesskit_macos_subclassing_adapter_free(
    struct accesskit_macos_subclassing_adapter *adapter);
#endif

#if defined(__APPLE__)
/**
 * You must call `accesskit_macos_queued_events_raise` on the returned pointer.
 * It can be null if the adapter is not active.
 */
struct accesskit_macos_queued_events *
accesskit_macos_subclassing_adapter_update_if_active(
    struct accesskit_macos_subclassing_adapter *adapter,
    accesskit_tree_update_factory update_factory,
    void *update_factory_userdata);
#endif

#if defined(__APPLE__)
/**
 * Update the tree state based on whether the window is focused.
 *
 * You must call `accesskit_macos_queued_events_raise` on the returned pointer.
 * It can be null if the adapter is not active.
 */
struct accesskit_macos_queued_events *
accesskit_macos_subclassing_adapter_update_view_focus_state(
    struct accesskit_macos_subclassing_adapter *adapter, bool is_focused);
#endif

#if defined(__APPLE__)
/**
 * Modifies the specified class, which must be a subclass of `NSWindow`,
 * to include an `accessibilityFocusedUIElement` method that calls
 * the corresponding method on the window's content view. This is needed
 * for windowing libraries such as SDL that place the keyboard focus
 * directly on the window rather than the content view.
 *
 * # Safety
 *
 * This function is declared unsafe because the caller must ensure that the
 * code for this library is never unloaded from the application process,
 * since it's not possible to reverse this operation. It's safest
 * if this library is statically linked into the application's main executable.
 * Also, this function assumes that the specified class is a subclass
 * of `NSWindow`.
 */
void accesskit_macos_add_focus_forwarder_to_window_class(
    const char *class_name);
#endif

#if (defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
     defined(__NetBSD__) || defined(__OpenBSD__))
/**
 * All of the handlers will always be called from another thread.
 */
struct accesskit_unix_adapter *accesskit_unix_adapter_new(
    accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata,
    accesskit_action_handler_callback action_handler,
    void *action_handler_userdata,
    accesskit_deactivation_handler_callback deactivation_handler,
    void *deactivation_handler_userdata);
#endif

#if (defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
     defined(__NetBSD__) || defined(__OpenBSD__))
void accesskit_unix_adapter_free(struct accesskit_unix_adapter *adapter);
#endif

#if (defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
     defined(__NetBSD__) || defined(__OpenBSD__))
void accesskit_unix_adapter_set_root_window_bounds(
    struct accesskit_unix_adapter *adapter, struct accesskit_rect outer,
    struct accesskit_rect inner);
#endif

#if (defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
     defined(__NetBSD__) || defined(__OpenBSD__))
void accesskit_unix_adapter_update_if_active(
    struct accesskit_unix_adapter *adapter,
    accesskit_tree_update_factory update_factory,
    void *update_factory_userdata);
#endif

#if (defined(__linux__) || defined(__DragonFly__) || defined(__FreeBSD__) || \
     defined(__NetBSD__) || defined(__OpenBSD__))
/**
 * Update the tree state based on whether the window is focused.
 */
void accesskit_unix_adapter_update_window_focus_state(
    struct accesskit_unix_adapter *adapter, bool is_focused);
#endif

#if defined(_WIN32)
/**
 * Memory is also freed when calling this function.
 */
void accesskit_windows_queued_events_raise(
    struct accesskit_windows_queued_events *events);
#endif

#if defined(_WIN32)
struct accesskit_windows_adapter *accesskit_windows_adapter_new(
    HWND hwnd, bool is_window_focused,
    accesskit_action_handler_callback action_handler,
    void *action_handler_userdata);
#endif

#if defined(_WIN32)
void accesskit_windows_adapter_free(struct accesskit_windows_adapter *adapter);
#endif

#if defined(_WIN32)
/**
 * You must call `accesskit_windows_queued_events_raise` on the returned
 * pointer. It can be null if the adapter is not active.
 */
struct accesskit_windows_queued_events *
accesskit_windows_adapter_update_if_active(
    struct accesskit_windows_adapter *adapter,
    accesskit_tree_update_factory update_factory,
    void *update_factory_userdata);
#endif

#if defined(_WIN32)
/**
 * Update the tree state based on whether the window is focused.
 *
 * You must call `accesskit_windows_queued_events_raise` on the returned
 * pointer.
 */
struct accesskit_windows_queued_events *
accesskit_windows_adapter_update_window_focus_state(
    struct accesskit_windows_adapter *adapter, bool is_focused);
#endif

#if defined(_WIN32)
struct accesskit_opt_lresult accesskit_windows_adapter_handle_wm_getobject(
    struct accesskit_windows_adapter *adapter, WPARAM wparam, LPARAM lparam,
    accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata);
#endif

#if defined(_WIN32)
struct accesskit_windows_subclassing_adapter *
accesskit_windows_subclassing_adapter_new(
    HWND hwnd, accesskit_activation_handler_callback activation_handler,
    void *activation_handler_userdata,
    accesskit_action_handler_callback action_handler,
    void *action_handler_userdata);
#endif

#if defined(_WIN32)
void accesskit_windows_subclassing_adapter_free(
    struct accesskit_windows_subclassing_adapter *adapter);
#endif

#if defined(_WIN32)
/**
 * You must call `accesskit_windows_queued_events_raise` on the returned
 * pointer. It can be null if the adapter is not active.
 */
struct accesskit_windows_queued_events *
accesskit_windows_subclassing_adapter_update_if_active(
    struct accesskit_windows_subclassing_adapter *adapter,
    accesskit_tree_update_factory update_factory,
    void *update_factory_userdata);
#endif

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif /* ACCESSKIT_H */
