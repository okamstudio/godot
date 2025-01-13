/**************************************************************************/
/*  jolt_query_collectors.h                                               */
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

#ifndef JOLT_QUERY_COLLECTORS_H
#define JOLT_QUERY_COLLECTORS_H

#include "../jolt_project_settings.h"
#include "jolt_space_3d.h"

#include "core/templates/local_vector.h"

#include "Jolt/Jolt.h"

#include "Jolt/Physics/Collision/InternalEdgeRemovingCollector.h"
#include "Jolt/Physics/Collision/Shape/Shape.h"

template <typename TBase, int TDefaultCapacity>
class JoltQueryCollectorAll final : public TBase {
public:
	typedef typename TBase::ResultType Hit;

private:
	JPH::Array<Hit> hits;

public:
	bool had_hit() const {
		return !hits.is_empty();
	}

	int get_hit_count() const {
		return hits.size();
	}

	const Hit &get_hit(int p_index) const {
		return hits[p_index];
	}

	void reset() { Reset(); }

	virtual void Reset() override {
		TBase::Reset();
		hits.clear();
	}

	virtual void AddHit(const Hit &p_hit) override {
		hits.push_back(p_hit);
	}
};

template <typename TBase>
class JoltQueryCollectorAny final : public TBase {
public:
	typedef typename TBase::ResultType Hit;

private:
	Hit hit;
	bool valid = false;

public:
	bool had_hit() const { return valid; }

	const Hit &get_hit() const { return hit; }

	void reset() {
		Reset();
	}

	virtual void Reset() override {
		TBase::Reset();
		valid = false;
	}

	virtual void AddHit(const Hit &p_hit) override {
		hit = p_hit;
		valid = true;

		TBase::ForceEarlyOut();
	}
};

template <typename TBase, int TDefaultCapacity>
class JoltQueryCollectorAnyMulti final : public TBase {
public:
	typedef typename TBase::ResultType Hit;

private:
	JPH::Array<Hit> hits;
	int max_hits = 0;

public:
	explicit JoltQueryCollectorAnyMulti(int p_max_hits = TDefaultCapacity) :
			max_hits(p_max_hits) {}

	bool had_hit() const {
		return hits.size() > 0;
	}

	int get_hit_count() const {
		return hits.size();
	}

	const Hit &get_hit(int p_index) const {
		return hits[p_index];
	}

	void reset() {
		Reset();
	}

	virtual void Reset() override {
		TBase::Reset();
		hits.clear();
	}

	virtual void AddHit(const Hit &p_hit) override {
		if ((int)hits.size() < max_hits) {
			hits.push_back(p_hit);
		}

		if ((int)hits.size() == max_hits) {
			TBase::ForceEarlyOut();
		}
	}
};

template <typename TBase>
class JoltQueryCollectorClosest final : public TBase {
public:
	typedef typename TBase::ResultType Hit;

private:
	Hit hit;
	bool valid = false;

public:
	bool had_hit() const { return valid; }

	const Hit &get_hit() const { return hit; }

	void reset() {
		Reset();
	}

	virtual void Reset() override {
		TBase::Reset();
		valid = false;
	}

	virtual void AddHit(const Hit &p_hit) override {
		const float early_out = p_hit.GetEarlyOutFraction();

		if (!valid || early_out < hit.GetEarlyOutFraction()) {
			TBase::UpdateEarlyOutFraction(early_out);

			hit = p_hit;
			valid = true;
		}
	}
};

template <typename TBase, int TDefaultCapacity>
class JoltQueryCollectorClosestMulti final : public TBase {
public:
	typedef typename TBase::ResultType Hit;

private:
	JPH::Array<Hit> hits;
	int max_hits = 0;

public:
	explicit JoltQueryCollectorClosestMulti(int p_max_hits = TDefaultCapacity) :
			max_hits(p_max_hits) {}

	bool had_hit() const {
		return hits.size() > 0;
	}

	int get_hit_count() const {
		return hits.size();
	}

	const Hit &get_hit(int p_index) const {
		return hits[p_index];
	}

	void reset() {
		Reset();
	}

	virtual void Reset() override {
		TBase::Reset();
		hits.clear();
	}

	virtual void AddHit(const Hit &p_hit) override {
		typename JPH::Array<Hit>::const_iterator E = hits.cbegin();
		for (; E != hits.cend(); ++E) {
			if (p_hit.GetEarlyOutFraction() < E->GetEarlyOutFraction()) {
				break;
			}
		}

		hits.insert(E, p_hit);

		if ((int)hits.size() > max_hits) {
			hits.resize(max_hits);
		}
	}
};

#endif // JOLT_QUERY_COLLECTORS_H
