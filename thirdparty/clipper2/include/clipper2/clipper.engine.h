/*******************************************************************************
* Author    :  Angus Johnson                                                   *
* Date      :  5 July 2024                                                     *
* Website   :  http://www.angusj.com                                           *
* Copyright :  Angus Johnson 2010-2024                                         *
* Purpose   :  This is the main polygon clipping module                        *
* License   :  http://www.boost.org/LICENSE_1_0.txt                            *
*******************************************************************************/

#ifndef CLIPPER_ENGINE_H
#define CLIPPER_ENGINE_H

#include <cstdlib>
#include <stdint.h> //#541
#include <iostream>
#include <queue>
#include <vector>
#include <functional>
#include <numeric>
#include <memory>

#include "clipper2/clipper.core.h"

namespace Clipper2Lib {

	struct Scanline;
	struct IntersectNode;
	struct Active;
	struct Vertex;
	struct LocalMinima;
	struct OutRec;
	struct HorzSegment;

	//Note: all clipping operations except for Difference are commutative.
	enum class ClipType { None, Intersection, Union, Difference, Xor };

	enum class PathType { Subject, Clip };
	enum class JoinWith { None, Left, Right };

	enum class VertexFlags : uint32_t {
		None = 0, OpenStart = 1, OpenEnd = 2, LocalMax = 4, LocalMin = 8
	};

	constexpr enum VertexFlags operator &(enum VertexFlags a, enum VertexFlags b)
	{
		return (enum VertexFlags)(uint32_t(a) & uint32_t(b));
	}

	constexpr enum VertexFlags operator |(enum VertexFlags a, enum VertexFlags b)
	{
		return (enum VertexFlags)(uint32_t(a) | uint32_t(b));
	}

	struct Vertex {
		Point64 pt;
		Vertex* next = nullptr;
		Vertex* prev = nullptr;
		VertexFlags flags = VertexFlags::None;
	};

	struct OutPt {
		Point64 pt;
		OutPt*	next = nullptr;
		OutPt*	prev = nullptr;
		OutRec* outrec;
		HorzSegment* horz = nullptr;

		OutPt(const Point64& pt_, OutRec* outrec_): pt(pt_), outrec(outrec_) {
			next = this;
			prev = this;
		}
	};

	class PolyPath;
	class PolyPath64;
	class PolyPathD;
	using PolyTree64 = PolyPath64;
	using PolyTreeD = PolyPathD;

	struct OutRec;
	typedef std::vector<OutRec*> OutRecList;

	//OutRec: contains a path in the clipping solution. Edges in the AEL will
	//have OutRec pointers assigned when they form part of the clipping solution.
	struct OutRec {
		size_t idx = 0;
		OutRec* owner = nullptr;
		Active* front_edge = nullptr;
		Active* back_edge = nullptr;
		OutPt* pts = nullptr;
		PolyPath* polypath = nullptr;
		OutRecList* splits = nullptr;
		OutRec* recursive_split = nullptr;
		Rect64 bounds = {};
		Path64 path;
		bool is_open = false;

		~OutRec() {
			if (splits) delete splits;
			// nb: don't delete the split pointers
			// as these are owned by ClipperBase's outrec_list_
		};
	};

	///////////////////////////////////////////////////////////////////
	//Important: UP and DOWN here are premised on Y-axis positive down
	//displays, which is the orientation used in Clipper's development.
	///////////////////////////////////////////////////////////////////

	struct Active {
		Point64 bot;
		Point64 top;
		int64_t curr_x = 0;		//current (updated at every new scanline)
		double dx = 0.0;
		int wind_dx = 1;			//1 or -1 depending on winding direction
		int wind_cnt = 0;
		int wind_cnt2 = 0;		//winding count of the opposite polytype
		OutRec* outrec = nullptr;
		//AEL: 'active edge list' (Vatti's AET - active edge table)
		//     a linked list of all edges (from left to right) that are present
		//     (or 'active') within the current scanbeam (a horizontal 'beam' that
		//     sweeps from bottom to top over the paths in the clipping operation).
		Active* prev_in_ael = nullptr;
		Active* next_in_ael = nullptr;
		//SEL: 'sorted edge list' (Vatti's ST - sorted table)
		//     linked list used when sorting edges into their new positions at the
		//     top of scanbeams, but also (re)used to process horizontals.
		Active* prev_in_sel = nullptr;
		Active* next_in_sel = nullptr;
		Active* jump = nullptr;
		Vertex* vertex_top = nullptr;
		LocalMinima* local_min = nullptr;  // the bottom of an edge 'bound' (also Vatti)
		bool is_left_bound = false;
		JoinWith join_with = JoinWith::None;
	};

	struct LocalMinima {
		Vertex* vertex;
		PathType polytype;
		bool is_open;
		LocalMinima(Vertex* v, PathType pt, bool open) :
			vertex(v), polytype(pt), is_open(open){}
	};

	struct IntersectNode {
		Point64 pt;
		Active* edge1;
		Active* edge2;
		IntersectNode() : pt(Point64(0,0)), edge1(NULL), edge2(NULL) {}
			IntersectNode(Active* e1, Active* e2, Point64& pt_) :
			pt(pt_), edge1(e1), edge2(e2) {}
	};

	struct HorzSegment {
		OutPt* left_op;
		OutPt* right_op = nullptr;
		bool left_to_right = true;
		HorzSegment() : left_op(nullptr) { }
		explicit HorzSegment(OutPt* op) : left_op(op) { }
	};

	struct HorzJoin {
		OutPt* op1 = nullptr;
		OutPt* op2 = nullptr;
		HorzJoin() {};
		explicit HorzJoin(OutPt* ltr, OutPt* rtl) : op1(ltr), op2(rtl) { }
	};

#ifdef USINGZ
		typedef std::function<void(const Point64& e1bot, const Point64& e1top,
		const Point64& e2bot, const Point64& e2top, Point64& pt)> ZCallback64;

	typedef std::function<void(const PointD& e1bot, const PointD& e1top,
		const PointD& e2bot, const PointD& e2top, PointD& pt)> ZCallbackD;
#endif

	typedef std::vector<HorzSegment> HorzSegmentList;
	typedef std::unique_ptr<LocalMinima> LocalMinima_ptr;
	typedef std::vector<LocalMinima_ptr> LocalMinimaList;
	typedef std::vector<IntersectNode> IntersectNodeList;

	// ReuseableDataContainer64 ------------------------------------------------

	class ReuseableDataContainer64 {
	private:
		friend class ClipperBase;
		LocalMinimaList minima_list_;
		std::vector<Vertex*> vertex_lists_;
		void AddLocMin(Vertex& vert, PathType polytype, bool is_open);
	public:
		virtual ~ReuseableDataContainer64();
		void Clear();
		void AddPaths(const Paths64& paths, PathType polytype, bool is_open);
	};

	// ClipperBase -------------------------------------------------------------

	class ClipperBase {
	private:
		ClipType cliptype_ = ClipType::None;
		FillRule fillrule_ = FillRule::EvenOdd;
		FillRule fillpos = FillRule::Positive;
		int64_t bot_y_ = 0;
		bool minima_list_sorted_ = false;
		bool using_polytree_ = false;
		Active* actives_ = nullptr;
		Active *sel_ = nullptr;
		LocalMinimaList minima_list_;		//pointers in case of memory reallocs
		LocalMinimaList::iterator current_locmin_iter_;
		std::vector<Vertex*> vertex_lists_;
		std::priority_queue<int64_t> scanline_list_;
		IntersectNodeList intersect_nodes_;
    HorzSegmentList horz_seg_list_;
		std::vector<HorzJoin> horz_join_list_;
		void Reset();
		inline void InsertScanline(int64_t y);
		inline bool PopScanline(int64_t &y);
		inline bool PopLocalMinima(int64_t y, LocalMinima*& local_minima);
		void DisposeAllOutRecs();
		void DisposeVerticesAndLocalMinima();
		void DeleteEdges(Active*& e);
		inline void AddLocMin(Vertex &vert, PathType polytype, bool is_open);
		bool IsContributingClosed(const Active &e) const;
		inline bool IsContributingOpen(const Active &e) const;
		void SetWindCountForClosedPathEdge(Active &edge);
		void SetWindCountForOpenPathEdge(Active &e);
		void InsertLocalMinimaIntoAEL(int64_t bot_y);
		void InsertLeftEdge(Active &e);
		inline void PushHorz(Active &e);
		inline bool PopHorz(Active *&e);
		inline OutPt* StartOpenPath(Active &e, const Point64& pt);
		inline void UpdateEdgeIntoAEL(Active *e);
		void IntersectEdges(Active &e1, Active &e2, const Point64& pt);
		inline void DeleteFromAEL(Active &e);
		inline void AdjustCurrXAndCopyToSEL(const int64_t top_y);
		void DoIntersections(const int64_t top_y);
		void AddNewIntersectNode(Active &e1, Active &e2, const int64_t top_y);
		bool BuildIntersectList(const int64_t top_y);
		void ProcessIntersectList();
		void SwapPositionsInAEL(Active& edge1, Active& edge2);
		OutRec* NewOutRec();
		OutPt* AddOutPt(const Active &e, const Point64& pt);
		OutPt* AddLocalMinPoly(Active &e1, Active &e2,
			const Point64& pt, bool is_new = false);
		OutPt* AddLocalMaxPoly(Active &e1, Active &e2, const Point64& pt);
		void DoHorizontal(Active &horz);
		bool ResetHorzDirection(const Active &horz, const Vertex* max_vertex,
			int64_t &horz_left, int64_t &horz_right);
		void DoTopOfScanbeam(const int64_t top_y);
		Active *DoMaxima(Active &e);
		void JoinOutrecPaths(Active &e1, Active &e2);
		void FixSelfIntersects(OutRec* outrec);
		void DoSplitOp(OutRec* outRec, OutPt* splitOp);

		inline void AddTrialHorzJoin(OutPt* op);
		void ConvertHorzSegsToJoins();
		void ProcessHorzJoins();

		void Split(Active& e, const Point64& pt);
		inline void CheckJoinLeft(Active& e,
			const Point64& pt, bool check_curr_x = false);
		inline void CheckJoinRight(Active& e,
			const Point64& pt, bool check_curr_x = false);
	protected:
		bool preserve_collinear_ = true;
		bool reverse_solution_ = false;
		int error_code_ = 0;
		bool has_open_paths_ = false;
		bool succeeded_ = true;
		OutRecList outrec_list_; //pointers in case list memory reallocated
		bool ExecuteInternal(ClipType ct, FillRule ft, bool use_polytrees);
		void CleanCollinear(OutRec* outrec);
		bool CheckBounds(OutRec* outrec);
		bool CheckSplitOwner(OutRec* outrec, OutRecList* splits);
		void RecursiveCheckOwners(OutRec* outrec, PolyPath* polypath);
#ifdef USINGZ
		ZCallback64 zCallback_ = nullptr;
		void SetZ(const Active& e1, const Active& e2, Point64& pt);
#endif
		void CleanUp();  // unlike Clear, CleanUp preserves added paths
		void AddPath(const Path64& path, PathType polytype, bool is_open);
		void AddPaths(const Paths64& paths, PathType polytype, bool is_open);
	public:
		virtual ~ClipperBase();
		int ErrorCode() const { return error_code_; };
		void PreserveCollinear(bool val) { preserve_collinear_ = val; };
		bool PreserveCollinear() const { return preserve_collinear_;};
		void ReverseSolution(bool val) { reverse_solution_ = val; };
		bool ReverseSolution() const { return reverse_solution_; };
		void Clear();
		void AddReuseableData(const ReuseableDataContainer64& reuseable_data);
#ifdef USINGZ
		int64_t DefaultZ = 0;
#endif
	};

	// PolyPath / PolyTree --------------------------------------------------------

	//PolyTree: is intended as a READ-ONLY data structure for CLOSED paths returned
	//by clipping operations. While this structure is more complex than the
	//alternative Paths structure, it does preserve path 'ownership' - ie those
	//paths that contain (or own) other paths. This will be useful to some users.

	class PolyPath {
	protected:
		PolyPath* parent_;
	public:
		PolyPath(PolyPath* parent = nullptr): parent_(parent){}
		virtual ~PolyPath() {};
		//https://en.cppreference.com/w/cpp/language/rule_of_three
		PolyPath(const PolyPath&) = delete;
		PolyPath& operator=(const PolyPath&) = delete;

		unsigned Level() const
		{
			unsigned result = 0;
			const PolyPath* p = parent_;
			while (p) { ++result; p = p->parent_; }
			return result;
		}

		virtual PolyPath* AddChild(const Path64& path) = 0;

		virtual void Clear() = 0;
		virtual size_t Count() const { return 0; }

		const PolyPath* Parent() const { return parent_; }

		bool IsHole() const
		{
			unsigned lvl = Level();
			//Even levels except level 0
			return lvl && !(lvl & 1);
		}
	};

	typedef typename std::vector<std::unique_ptr<PolyPath64>> PolyPath64List;
	typedef typename std::vector<std::unique_ptr<PolyPathD>>  PolyPathDList;

	class PolyPath64 : public PolyPath {
	private:
		PolyPath64List childs_;
		Path64 polygon_;
	public:
		explicit PolyPath64(PolyPath64* parent = nullptr) : PolyPath(parent) {}
		explicit PolyPath64(PolyPath64* parent, const Path64& path) : PolyPath(parent) { polygon_ = path; }

		~PolyPath64() {
			childs_.resize(0);
		}

		PolyPath64* operator [] (size_t index) const
		{
			return childs_[index].get(); //std::unique_ptr
		}

		PolyPath64* Child(size_t index) const
		{
			return childs_[index].get();
		}

		PolyPath64List::const_iterator begin() const { return childs_.cbegin(); }
		PolyPath64List::const_iterator end() const { return childs_.cend(); }

		PolyPath64* AddChild(const Path64& path) override
		{
			return childs_.emplace_back(std::make_unique<PolyPath64>(this, path)).get();
		}

		void Clear() override
		{
			childs_.resize(0);
		}

		size_t Count() const override
		{
			return childs_.size();
		}

		const Path64& Polygon() const { return polygon_; };

		double Area() const
		{
			return std::accumulate(childs_.cbegin(), childs_.cend(),
				Clipper2Lib::Area<int64_t>(polygon_),
				[](double a, const auto& child) {return a + child->Area(); });
		}

	};

	class PolyPathD : public PolyPath {
	private:
		PolyPathDList childs_;
		double scale_;
		PathD polygon_;
	public:
		explicit PolyPathD(PolyPathD* parent = nullptr) : PolyPath(parent)
		{
			scale_ = parent ? parent->scale_ : 1.0;
		}

		explicit PolyPathD(PolyPathD* parent, const Path64& path) : PolyPath(parent)
		{
			scale_ = parent ? parent->scale_ : 1.0;
			int error_code = 0;
			polygon_ = ScalePath<double, int64_t>(path, scale_, error_code);
		}

		explicit PolyPathD(PolyPathD* parent, const PathD& path) : PolyPath(parent)
		{
			scale_ = parent ? parent->scale_ : 1.0;
			polygon_ = path;
		}

		~PolyPathD() {
			childs_.resize(0);
		}

		PolyPathD* operator [] (size_t index) const
		{
			return childs_[index].get();
		}

		PolyPathD* Child(size_t index) const
		{
			return childs_[index].get();
		}

		PolyPathDList::const_iterator begin() const { return childs_.cbegin(); }
		PolyPathDList::const_iterator end() const { return childs_.cend(); }

		void SetScale(double value) { scale_ = value; }
		double Scale() const { return scale_; }

		PolyPathD* AddChild(const Path64& path) override
		{
			return childs_.emplace_back(std::make_unique<PolyPathD>(this, path)).get();
		}

		PolyPathD* AddChild(const PathD& path)
		{
			return childs_.emplace_back(std::make_unique<PolyPathD>(this, path)).get();
		}

		void Clear() override
		{
			childs_.resize(0);
		}

		size_t Count() const override
		{
			return childs_.size();
		}

		const PathD& Polygon() const { return polygon_; };

		double Area() const
		{
			return std::accumulate(childs_.begin(), childs_.end(),
				Clipper2Lib::Area<double>(polygon_),
				[](double a, const auto& child) {return a + child->Area(); });
		}
	};

	class Clipper64 : public ClipperBase
	{
	private:
		void BuildPaths64(Paths64& solutionClosed, Paths64* solutionOpen);
		void BuildTree64(PolyPath64& polytree, Paths64& open_paths);
	public:
#ifdef USINGZ
		void SetZCallback(ZCallback64 cb) { zCallback_ = cb; }
#endif

		void AddSubject(const Paths64& subjects)
		{
			AddPaths(subjects, PathType::Subject, false);
		}
		void AddOpenSubject(const Paths64& open_subjects)
		{
			AddPaths(open_subjects, PathType::Subject, true);
		}
		void AddClip(const Paths64& clips)
		{
			AddPaths(clips, PathType::Clip, false);
		}

		bool Execute(ClipType clip_type,
			FillRule fill_rule, Paths64& closed_paths)
		{
			Paths64 dummy;
			return Execute(clip_type, fill_rule, closed_paths, dummy);
		}

		bool Execute(ClipType clip_type, FillRule fill_rule,
			Paths64& closed_paths, Paths64& open_paths)
		{
			closed_paths.clear();
			open_paths.clear();
			if (ExecuteInternal(clip_type, fill_rule, false))
					BuildPaths64(closed_paths, &open_paths);
			CleanUp();
			return succeeded_;
		}

		bool Execute(ClipType clip_type, FillRule fill_rule, PolyTree64& polytree)
		{
			Paths64 dummy;
			return Execute(clip_type, fill_rule, polytree, dummy);
		}

		bool Execute(ClipType clip_type,
			FillRule fill_rule, PolyTree64& polytree, Paths64& open_paths)
		{
			if (ExecuteInternal(clip_type, fill_rule, true))
			{
				open_paths.clear();
				polytree.Clear();
				BuildTree64(polytree, open_paths);
			}
			CleanUp();
			return succeeded_;
		}
	};

	class ClipperD : public ClipperBase {
	private:
		double scale_ = 1.0, invScale_ = 1.0;
#ifdef USINGZ
		ZCallbackD zCallbackD_ = nullptr;
#endif
		void BuildPathsD(PathsD& solutionClosed, PathsD* solutionOpen);
		void BuildTreeD(PolyPathD& polytree, PathsD& open_paths);
	public:
		explicit ClipperD(int precision = 2) : ClipperBase()
		{
			CheckPrecisionRange(precision, error_code_);
			// to optimize scaling / descaling precision
			// set the scale to a power of double's radix (2) (#25)
			scale_ = std::pow(std::numeric_limits<double>::radix,
				std::ilogb(std::pow(10, precision)) + 1);
			invScale_ = 1 / scale_;
		}

#ifdef USINGZ
		void SetZCallback(ZCallbackD cb) { zCallbackD_ = cb; };

		void ZCB(const Point64& e1bot, const Point64& e1top,
			const Point64& e2bot, const Point64& e2top, Point64& pt)
		{
			// de-scale (x & y)
			// temporarily convert integers to their initial float values
			// this will slow clipping marginally but will make it much easier
			// to understand the coordinates passed to the callback function
			PointD tmp = PointD(pt) * invScale_;
			PointD e1b = PointD(e1bot) * invScale_;
			PointD e1t = PointD(e1top) * invScale_;
			PointD e2b = PointD(e2bot) * invScale_;
			PointD e2t = PointD(e2top) * invScale_;
			zCallbackD_(e1b,e1t, e2b, e2t, tmp);
			pt.z = tmp.z; // only update 'z'
		};

		void CheckCallback()
		{
			if(zCallbackD_)
				// if the user defined float point callback has been assigned
				// then assign the proxy callback function
				ClipperBase::zCallback_ =
					std::bind(&ClipperD::ZCB, this, std::placeholders::_1,
					std::placeholders::_2, std::placeholders::_3,
					std::placeholders::_4, std::placeholders::_5);
			else
				ClipperBase::zCallback_ = nullptr;
		}

#endif

		void AddSubject(const PathsD& subjects)
		{
			AddPaths(ScalePaths<int64_t, double>(subjects, scale_, error_code_), PathType::Subject, false);
		}

		void AddOpenSubject(const PathsD& open_subjects)
		{
			AddPaths(ScalePaths<int64_t, double>(open_subjects, scale_, error_code_), PathType::Subject, true);
		}

		void AddClip(const PathsD& clips)
		{
			AddPaths(ScalePaths<int64_t, double>(clips, scale_, error_code_), PathType::Clip, false);
		}

		bool Execute(ClipType clip_type, FillRule fill_rule, PathsD& closed_paths)
		{
			PathsD dummy;
			return Execute(clip_type, fill_rule, closed_paths, dummy);
		}

		bool Execute(ClipType clip_type,
			FillRule fill_rule, PathsD& closed_paths, PathsD& open_paths)
		{
#ifdef USINGZ
			CheckCallback();
#endif
			if (ExecuteInternal(clip_type, fill_rule, false))
			{
				BuildPathsD(closed_paths, &open_paths);
			}
			CleanUp();
			return succeeded_;
		}

		bool Execute(ClipType clip_type, FillRule fill_rule, PolyTreeD& polytree)
		{
			PathsD dummy;
			return Execute(clip_type, fill_rule, polytree, dummy);
		}

		bool Execute(ClipType clip_type,
			FillRule fill_rule, PolyTreeD& polytree, PathsD& open_paths)
		{
#ifdef USINGZ
			CheckCallback();
#endif
			if (ExecuteInternal(clip_type, fill_rule, true))
			{
				polytree.Clear();
				polytree.SetScale(invScale_);
				open_paths.clear();
				BuildTreeD(polytree, open_paths);
			}
			CleanUp();
			return succeeded_;
		}

	};

}  // namespace

#endif  // CLIPPER_ENGINE_H
