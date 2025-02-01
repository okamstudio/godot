/**************************************************************************/
/*  fuzzy_search.h                                                        */
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

#pragma once

#include "core/variant/variant.h"

class FuzzyTokenMatch;

struct FuzzySearchToken {
	int idx = -1;
	String string;

	bool try_exact_match(FuzzyTokenMatch &p_match, const String &p_target, int p_offset) const;
	bool try_fuzzy_match(FuzzyTokenMatch &p_match, const String &p_target, int p_offset, int p_miss_budget) const;
};

class FuzzyTokenMatch {
	friend struct FuzzySearchToken;
	friend class FuzzySearchResult;
	friend class FuzzySearch;

	int matched_length = 0;
	int token_length = 0;
	int token_idx = -1;
	Vector2i interval = Vector2i(-1, -1); // x and y are both inclusive indices.

	void add_substring(int p_substring_start, int p_substring_length);
	bool intersects(const Vector2i &p_other_interval) const;
	bool is_case_insensitive(const String &p_original, const String &p_adjusted) const;
	int get_miss_count() const { return token_length - matched_length; }

public:
	int score = 0;
	Vector<Vector2i> substrings; // x is start index, y is length.
};

class FuzzySearchResult {
	friend class FuzzySearch;

	int miss_budget = 0;
	Vector2i match_interval = Vector2i(-1, -1);

	bool can_add_token_match(const FuzzyTokenMatch &p_match) const;
	void score_token_match(FuzzyTokenMatch &p_match, bool p_case_insensitive) const;
	void add_token_match(const FuzzyTokenMatch &p_match);
	void maybe_apply_score_bonus();

public:
	String target;
	int score = 0;
	int dir_index = -1;
	Vector<FuzzyTokenMatch> token_matches;
};

class FuzzySearch {
	Vector<FuzzySearchToken> tokens;

	void sort_and_filter(Vector<FuzzySearchResult> &p_results) const;

public:
	int start_offset = 0;
	bool case_sensitive = false;
	int max_results = 100;
	int max_misses = 2;
	bool allow_subsequences = true;

	void set_query(const String &p_query);
	bool search(const String &p_target, FuzzySearchResult &p_result) const;
	void search_all(const PackedStringArray &p_targets, Vector<FuzzySearchResult> &p_results) const;
};
