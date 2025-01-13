/*******************************************************************************
* Author    :  Angus Johnson                                                   *
* Date      :  27 April 2024                                                   *
* Website   :  http://www.angusj.com                                           *
* Copyright :  Angus Johnson 2010-2024                                         *
* Purpose   :  This is the main polygon clipping module                        *
* License   :  http://www.boost.org/LICENSE_1_0.txt                            *
*******************************************************************************/

#include <cstdlib>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <numeric>
#include <algorithm>

#include "clipper2/clipper.engine.h"
#include "clipper2/clipper.h"

// https://github.com/AngusJohnson/Clipper2/discussions/334
// #discussioncomment-4248602
#if defined(_MSC_VER) && ( defined(_M_AMD64) || defined(_M_X64) )
#include <xmmintrin.h>
#include <emmintrin.h>
#define fmin(a,b) _mm_cvtsd_f64(_mm_min_sd(_mm_set_sd(a),_mm_set_sd(b)))
#define fmax(a,b) _mm_cvtsd_f64(_mm_max_sd(_mm_set_sd(a),_mm_set_sd(b)))
#define nearbyint(a) _mm_cvtsd_si64(_mm_set_sd(a)) /* Note: expression type is (int64_t) */
#endif

namespace Clipper2Lib {

  static const Rect64 invalid_rect = Rect64(false);

  // Every closed path (ie polygon) is made up of a series of vertices forming edge 
  // 'bounds' that alternate between ascending bounds (containing edges going up 
  // relative to the Y-axis) and descending bounds. 'Local Minima' refers to
  // vertices where ascending and descending bounds join at the bottom, and
  // 'Local Maxima' are where ascending and descending bounds join at the top.

  struct Scanline {
    int64_t y = 0;
    Scanline* next = nullptr;

    explicit Scanline(int64_t y_) : y(y_) {}
  };

  struct HorzSegSorter {
    inline bool operator()(const HorzSegment& hs1, const HorzSegment& hs2)
    {
      if (!hs1.right_op || !hs2.right_op) return (hs1.right_op);
      return  hs2.left_op->pt.x > hs1.left_op->pt.x;
    }
  };

  struct LocMinSorter {
    inline bool operator()(const LocalMinima_ptr& locMin1,
      const LocalMinima_ptr& locMin2)
    {
      if (locMin2->vertex->pt.y != locMin1->vertex->pt.y)
        return locMin2->vertex->pt.y < locMin1->vertex->pt.y;
      else
        return locMin2->vertex->pt.x > locMin1->vertex->pt.x;
    }
  };


  inline bool IsOdd(int val)
  {
    return (val & 1) ? true : false;
  }


  inline bool IsHotEdge(const Active& e)
  {
    return (e.outrec);
  }


  inline bool IsOpen(const Active& e)
  {
    return (e.local_min->is_open);
  }


  inline bool IsOpenEnd(const Vertex& v)
  {
    return (v.flags & (VertexFlags::OpenStart | VertexFlags::OpenEnd)) !=
      VertexFlags::None;
  }


  inline bool IsOpenEnd(const Active& ae)
  {
    return IsOpenEnd(*ae.vertex_top);
  }


  inline Active* GetPrevHotEdge(const Active& e)
  {
    Active* prev = e.prev_in_ael;
    while (prev && (IsOpen(*prev) || !IsHotEdge(*prev)))
      prev = prev->prev_in_ael;
    return prev;
  }

  inline bool IsFront(const Active& e)
  {
    return (&e == e.outrec->front_edge);
  }

  inline bool IsInvalidPath(OutPt* op)
  {
    return (!op || op->next == op);
  }

  /*******************************************************************************
    *  Dx:                             0(90deg)                                    *
    *                                  |                                           *
    *               +inf (180deg) <--- o ---> -inf (0deg)                          *
    *******************************************************************************/

  inline double GetDx(const Point64& pt1, const Point64& pt2)
  {
    double dy = double(pt2.y - pt1.y);
    if (dy != 0)
      return double(pt2.x - pt1.x) / dy;
    else if (pt2.x > pt1.x)
      return -std::numeric_limits<double>::max();
    else
      return std::numeric_limits<double>::max();
  }

  inline int64_t TopX(const Active& ae, const int64_t currentY)
  {
    if ((currentY == ae.top.y) || (ae.top.x == ae.bot.x)) return ae.top.x;
    else if (currentY == ae.bot.y) return ae.bot.x;
    else return ae.bot.x + static_cast<int64_t>(nearbyint(ae.dx * (currentY - ae.bot.y)));
    // nb: std::nearbyint (or std::round) substantially *improves* performance here
    // as it greatly improves the likelihood of edge adjacency in ProcessIntersectList().
  }


  inline bool IsHorizontal(const Active& e)
  {
    return (e.top.y == e.bot.y);
  }


  inline bool IsHeadingRightHorz(const Active& e)
  {
    return e.dx == -std::numeric_limits<double>::max();
  }


  inline bool IsHeadingLeftHorz(const Active& e)
  {
    return e.dx == std::numeric_limits<double>::max();
  }


  inline void SwapActives(Active*& e1, Active*& e2)
  {
    Active* e = e1;
    e1 = e2;
    e2 = e;
  }

  inline PathType GetPolyType(const Active& e)
  {
    return e.local_min->polytype;
  }

  inline bool IsSamePolyType(const Active& e1, const Active& e2)
  {
    return e1.local_min->polytype == e2.local_min->polytype;
  }

  inline void SetDx(Active& e)
  {
    e.dx = GetDx(e.bot, e.top);
  }

  inline Vertex* NextVertex(const Active& e)
  {
    if (e.wind_dx > 0)
      return e.vertex_top->next;
    else
      return e.vertex_top->prev;
  }

  //PrevPrevVertex: useful to get the (inverted Y-axis) top of the
  //alternate edge (ie left or right bound) during edge insertion.
  inline Vertex* PrevPrevVertex(const Active& ae)
  {
    if (ae.wind_dx > 0)
      return ae.vertex_top->prev->prev;
    else
      return ae.vertex_top->next->next;
  }


  inline Active* ExtractFromSEL(Active* ae)
  {
    Active* res = ae->next_in_sel;
    if (res)
      res->prev_in_sel = ae->prev_in_sel;
    ae->prev_in_sel->next_in_sel = res;
    return res;
  }


  inline void Insert1Before2InSEL(Active* ae1, Active* ae2)
  {
    ae1->prev_in_sel = ae2->prev_in_sel;
    if (ae1->prev_in_sel)
      ae1->prev_in_sel->next_in_sel = ae1;
    ae1->next_in_sel = ae2;
    ae2->prev_in_sel = ae1;
  }

  inline bool IsMaxima(const Vertex& v)
  {
    return ((v.flags & VertexFlags::LocalMax) != VertexFlags::None);
  }


  inline bool IsMaxima(const Active& e)
  {
    return IsMaxima(*e.vertex_top);
  }

  inline Vertex* GetCurrYMaximaVertex_Open(const Active& e)
  {
    Vertex* result = e.vertex_top;
    if (e.wind_dx > 0)
      while ((result->next->pt.y == result->pt.y) &&
        ((result->flags & (VertexFlags::OpenEnd |
          VertexFlags::LocalMax)) == VertexFlags::None))
            result = result->next;
    else
      while (result->prev->pt.y == result->pt.y &&
        ((result->flags & (VertexFlags::OpenEnd |
          VertexFlags::LocalMax)) == VertexFlags::None))
          result = result->prev;
    if (!IsMaxima(*result)) result = nullptr; // not a maxima
    return result;
  }

    inline Vertex* GetCurrYMaximaVertex(const Active& e)
  {
    Vertex* result = e.vertex_top;
    if (e.wind_dx > 0)
      while (result->next->pt.y == result->pt.y) result = result->next;
    else
      while (result->prev->pt.y == result->pt.y) result = result->prev;
    if (!IsMaxima(*result)) result = nullptr; // not a maxima
    return result;
  }

  Active* GetMaximaPair(const Active& e)
  {
    Active* e2;
    e2 = e.next_in_ael;
    while (e2)
    {
      if (e2->vertex_top == e.vertex_top) return e2;  // Found!
      e2 = e2->next_in_ael;
    }
    return nullptr;
  }

  inline int PointCount(OutPt* op)
  {
    OutPt* op2 = op;
    int cnt = 0;
    do
    {
      op2 = op2->next;
      ++cnt;
    } while (op2 != op);
    return cnt;
  }

  inline OutPt* DuplicateOp(OutPt* op, bool insert_after)
  {
    OutPt* result = new OutPt(op->pt, op->outrec);
    if (insert_after)
    {
      result->next = op->next;
      result->next->prev = result;
      result->prev = op;
      op->next = result;
    }
    else
    {
      result->prev = op->prev;
      result->prev->next = result;
      result->next = op;
      op->prev = result;
    }
    return result;
  }

  inline OutPt* DisposeOutPt(OutPt* op)
  {
    OutPt* result = op->next;
    op->prev->next = op->next;
    op->next->prev = op->prev;
    delete op;
    return result;
  }


  inline void DisposeOutPts(OutRec* outrec)
  {
    OutPt* op = outrec->pts;
    op->prev->next = nullptr;
    while (op)
    {
      OutPt* tmp = op;
      op = op->next;
      delete tmp;
    };
    outrec->pts = nullptr;
  }


  bool IntersectListSort(const IntersectNode& a, const IntersectNode& b)
  {
    //note different inequality tests ...
    return (a.pt.y == b.pt.y) ? (a.pt.x < b.pt.x) : (a.pt.y > b.pt.y);
  }


  inline void SetSides(OutRec& outrec, Active& start_edge, Active& end_edge)
  {
    outrec.front_edge = &start_edge;
    outrec.back_edge = &end_edge;
  }


  void SwapOutrecs(Active& e1, Active& e2)
  {
    OutRec* or1 = e1.outrec;
    OutRec* or2 = e2.outrec;
    if (or1 == or2)
    {
      Active* e = or1->front_edge;
      or1->front_edge = or1->back_edge;
      or1->back_edge = e;
      return;
    }
    if (or1)
    {
      if (&e1 == or1->front_edge)
        or1->front_edge = &e2;
      else
        or1->back_edge = &e2;
    }
    if (or2)
    {
      if (&e2 == or2->front_edge)
        or2->front_edge = &e1;
      else
        or2->back_edge = &e1;
    }
    e1.outrec = or2;
    e2.outrec = or1;
  }


  double Area(OutPt* op)
  {
    //https://en.wikipedia.org/wiki/Shoelace_formula
    double result = 0.0;
    OutPt* op2 = op;
    do
    {
      result += static_cast<double>(op2->prev->pt.y + op2->pt.y) *
        static_cast<double>(op2->prev->pt.x - op2->pt.x);
      op2 = op2->next;
    } while (op2 != op);
    return result * 0.5;
  }

  inline double AreaTriangle(const Point64& pt1,
    const Point64& pt2, const Point64& pt3)
  {
    return (static_cast<double>(pt3.y + pt1.y) * static_cast<double>(pt3.x - pt1.x) +
      static_cast<double>(pt1.y + pt2.y) * static_cast<double>(pt1.x - pt2.x) +
      static_cast<double>(pt2.y + pt3.y) * static_cast<double>(pt2.x - pt3.x));
  }

  void ReverseOutPts(OutPt* op)
  {
    if (!op) return;

    OutPt* op1 = op;
    OutPt* op2;

    do
    {
      op2 = op1->next;
      op1->next = op1->prev;
      op1->prev = op2;
      op1 = op2;
    } while (op1 != op);
  }

  inline void SwapSides(OutRec& outrec)
  {
    Active* e2 = outrec.front_edge;
    outrec.front_edge = outrec.back_edge;
    outrec.back_edge = e2;
    outrec.pts = outrec.pts->next;
  }

  inline OutRec* GetRealOutRec(OutRec* outrec)
  {
    while (outrec && !outrec->pts) outrec = outrec->owner;
    return outrec;
  }

  inline bool IsValidOwner(OutRec* outrec, OutRec* testOwner)
  {
    // prevent outrec owning itself either directly or indirectly
    while (testOwner && testOwner != outrec) testOwner = testOwner->owner;
    return !testOwner;
  }

  inline void UncoupleOutRec(Active ae)
  {
    OutRec* outrec = ae.outrec;
    if (!outrec) return;
    outrec->front_edge->outrec = nullptr;
    outrec->back_edge->outrec = nullptr;
    outrec->front_edge = nullptr;
    outrec->back_edge = nullptr;
  }


  inline bool PtsReallyClose(const Point64& pt1, const Point64& pt2)
  {
    return (std::llabs(pt1.x - pt2.x) < 2) && (std::llabs(pt1.y - pt2.y) < 2);
  }

  inline bool IsVerySmallTriangle(const OutPt& op)
  {
    return op.next->next == op.prev &&
      (PtsReallyClose(op.prev->pt, op.next->pt) ||
        PtsReallyClose(op.pt, op.next->pt) ||
        PtsReallyClose(op.pt, op.prev->pt));
  }

  inline bool IsValidClosedPath(const OutPt* op)
  {
    return op && (op->next != op) && (op->next != op->prev) &&
      !IsVerySmallTriangle(*op);
  }

  inline bool OutrecIsAscending(const Active* hotEdge)
  {
    return (hotEdge == hotEdge->outrec->front_edge);
  }

  inline void SwapFrontBackSides(OutRec& outrec)
  {
    Active* tmp = outrec.front_edge;
    outrec.front_edge = outrec.back_edge;
    outrec.back_edge = tmp;
    outrec.pts = outrec.pts->next;
  }

  inline bool EdgesAdjacentInAEL(const IntersectNode& inode)
  {
    return (inode.edge1->next_in_ael == inode.edge2) || (inode.edge1->prev_in_ael == inode.edge2);
  }

  inline bool IsJoined(const Active& e)
  {
    return e.join_with != JoinWith::None;
  }

  inline void SetOwner(OutRec* outrec, OutRec* new_owner)
  {
    //precondition1: new_owner is never null
    while (new_owner->owner && !new_owner->owner->pts)
      new_owner->owner = new_owner->owner->owner;
    OutRec* tmp = new_owner;
    while (tmp && tmp != outrec) tmp = tmp->owner;
    if (tmp) new_owner->owner = outrec->owner;
    outrec->owner = new_owner;
  }

  static PointInPolygonResult PointInOpPolygon(const Point64& pt, OutPt* op)
  {
    if (op == op->next || op->prev == op->next)
      return PointInPolygonResult::IsOutside;

    OutPt* op2 = op;
    do
    {
      if (op->pt.y != pt.y) break;
      op = op->next;
    } while (op != op2);
    if (op->pt.y == pt.y) // not a proper polygon
      return PointInPolygonResult::IsOutside;

    bool is_above = op->pt.y < pt.y, starting_above = is_above;
    int val = 0;
    op2 = op->next;
    while (op2 != op)
    {
      if (is_above)
        while (op2 != op && op2->pt.y < pt.y) op2 = op2->next;
      else
        while (op2 != op && op2->pt.y > pt.y) op2 = op2->next;
      if (op2 == op) break;

      // must have touched or crossed the pt.Y horizonal
      // and this must happen an even number of times

      if (op2->pt.y == pt.y) // touching the horizontal
      {
        if (op2->pt.x == pt.x || (op2->pt.y == op2->prev->pt.y &&
          (pt.x < op2->prev->pt.x) != (pt.x < op2->pt.x)))
          return PointInPolygonResult::IsOn;

        op2 = op2->next;
        if (op2 == op) break;
        continue;
      }

      if (pt.x < op2->pt.x && pt.x < op2->prev->pt.x);
      // do nothing because
      // we're only interested in edges crossing on the left
      else if ((pt.x > op2->prev->pt.x && pt.x > op2->pt.x))
        val = 1 - val; // toggle val
      else
      {
        double d = CrossProduct(op2->prev->pt, op2->pt, pt);
        if (d == 0) return PointInPolygonResult::IsOn;
        if ((d < 0) == is_above) val = 1 - val;
      }
      is_above = !is_above;
      op2 = op2->next;
    }

    if (is_above != starting_above)
    {
      double d = CrossProduct(op2->prev->pt, op2->pt, pt);
      if (d == 0) return PointInPolygonResult::IsOn;
      if ((d < 0) == is_above) val = 1 - val;
    }

    if (val == 0) return PointInPolygonResult::IsOutside;
    else return PointInPolygonResult::IsInside;
  }

  inline Path64 GetCleanPath(OutPt* op)
  {
    Path64 result;
    OutPt* op2 = op;
    while (op2->next != op &&
      ((op2->pt.x == op2->next->pt.x && op2->pt.x == op2->prev->pt.x) ||
        (op2->pt.y == op2->next->pt.y && op2->pt.y == op2->prev->pt.y))) op2 = op2->next;
    result.push_back(op2->pt);
    OutPt* prevOp = op2;
    op2 = op2->next;
    while (op2 != op)
    {
      if ((op2->pt.x != op2->next->pt.x || op2->pt.x != prevOp->pt.x) &&
        (op2->pt.y != op2->next->pt.y || op2->pt.y != prevOp->pt.y))
      {
        result.push_back(op2->pt);
        prevOp = op2;
      }
      op2 = op2->next;
    }
    return result;
  }

  inline bool Path1InsidePath2(OutPt* op1, OutPt* op2)
  {
    // we need to make some accommodation for rounding errors
    // so we won't jump if the first vertex is found outside
    PointInPolygonResult result;
    int outside_cnt = 0;
    OutPt* op = op1;
    do
    {
      result = PointInOpPolygon(op->pt, op2);
      if (result == PointInPolygonResult::IsOutside) ++outside_cnt;
      else if (result == PointInPolygonResult::IsInside) --outside_cnt;
      op = op->next;
    } while (op != op1 && std::abs(outside_cnt) < 2);
    if (std::abs(outside_cnt) > 1) return (outside_cnt < 0);
    // since path1's location is still equivocal, check its midpoint
    Point64 mp = GetBounds(GetCleanPath(op1)).MidPoint();
    Path64 path2 = GetCleanPath(op2);
    return PointInPolygon(mp, path2) != PointInPolygonResult::IsOutside;
  }

  //------------------------------------------------------------------------------
  //------------------------------------------------------------------------------

  void AddLocMin(LocalMinimaList& list,
    Vertex& vert, PathType polytype, bool is_open)
  {
    //make sure the vertex is added only once ...
    if ((VertexFlags::LocalMin & vert.flags) != VertexFlags::None) return;

    vert.flags = (vert.flags | VertexFlags::LocalMin);
    list.push_back(std::make_unique <LocalMinima>(&vert, polytype, is_open));
  }

  void AddPaths_(const Paths64& paths, PathType polytype, bool is_open,
    std::vector<Vertex*>& vertexLists, LocalMinimaList& locMinList)
  {
    const auto total_vertex_count =
      std::accumulate(paths.begin(), paths.end(), size_t(0),
        [](const auto& a, const Path64& path)
        {return a + path.size(); });
    if (total_vertex_count == 0) return;

    Vertex* vertices = new Vertex[total_vertex_count], * v = vertices;
    for (const Path64& path : paths)
    {
      //for each path create a circular double linked list of vertices
      Vertex* v0 = v, * curr_v = v, * prev_v = nullptr;

      if (path.empty())
        continue;

      v->prev = nullptr;
      int cnt = 0;
      for (const Point64& pt : path)
      {
        if (prev_v)
        {
          if (prev_v->pt == pt) continue; // ie skips duplicates
          prev_v->next = curr_v;
        }
        curr_v->prev = prev_v;
        curr_v->pt = pt;
        curr_v->flags = VertexFlags::None;
        prev_v = curr_v++;
        cnt++;
      }
      if (!prev_v || !prev_v->prev) continue;
      if (!is_open && prev_v->pt == v0->pt)
        prev_v = prev_v->prev;
      prev_v->next = v0;
      v0->prev = prev_v;
      v = curr_v; // ie get ready for next path
      if (cnt < 2 || (cnt == 2 && !is_open)) continue;

      //now find and assign local minima
      bool going_up, going_up0;
      if (is_open)
      {
        curr_v = v0->next;
        while (curr_v != v0 && curr_v->pt.y == v0->pt.y)
          curr_v = curr_v->next;
        going_up = curr_v->pt.y <= v0->pt.y;
        if (going_up)
        {
          v0->flags = VertexFlags::OpenStart;
          AddLocMin(locMinList , *v0, polytype, true);
        }
        else
          v0->flags = VertexFlags::OpenStart | VertexFlags::LocalMax;
      }
      else // closed path
      {
        prev_v = v0->prev;
        while (prev_v != v0 && prev_v->pt.y == v0->pt.y)
          prev_v = prev_v->prev;
        if (prev_v == v0)
          continue; // only open paths can be completely flat
        going_up = prev_v->pt.y > v0->pt.y;
      }

      going_up0 = going_up;
      prev_v = v0;
      curr_v = v0->next;
      while (curr_v != v0)
      {
        if (curr_v->pt.y > prev_v->pt.y && going_up)
        {
          prev_v->flags = (prev_v->flags | VertexFlags::LocalMax);
          going_up = false;
        }
        else if (curr_v->pt.y < prev_v->pt.y && !going_up)
        {
          going_up = true;
          AddLocMin(locMinList, *prev_v, polytype, is_open);
        }
        prev_v = curr_v;
        curr_v = curr_v->next;
      }

      if (is_open)
      {
        prev_v->flags = prev_v->flags | VertexFlags::OpenEnd;
        if (going_up)
          prev_v->flags = prev_v->flags | VertexFlags::LocalMax;
        else
          AddLocMin(locMinList, *prev_v, polytype, is_open);
      }
      else if (going_up != going_up0)
      {
        if (going_up0) AddLocMin(locMinList, *prev_v, polytype, false);
        else prev_v->flags = prev_v->flags | VertexFlags::LocalMax;
      }
    } // end processing current path

    vertexLists.emplace_back(vertices);
  }

  //------------------------------------------------------------------------------
  // ReuseableDataContainer64 methods ...
  //------------------------------------------------------------------------------

  void ReuseableDataContainer64::AddLocMin(Vertex& vert, PathType polytype, bool is_open)
  {
    //make sure the vertex is added only once ...
    if ((VertexFlags::LocalMin & vert.flags) != VertexFlags::None) return;

    vert.flags = (vert.flags | VertexFlags::LocalMin);
    minima_list_.push_back(std::make_unique <LocalMinima>(&vert, polytype, is_open));
  }

  void ReuseableDataContainer64::AddPaths(const Paths64& paths,
    PathType polytype, bool is_open)
  {
    AddPaths_(paths, polytype, is_open, vertex_lists_, minima_list_);
  }

  ReuseableDataContainer64::~ReuseableDataContainer64()
  {
    Clear();
  }

  void ReuseableDataContainer64::Clear()
  {
    minima_list_.clear();
    for (auto v : vertex_lists_) delete[] v;
    vertex_lists_.clear();
  }

  //------------------------------------------------------------------------------
  // ClipperBase methods ...
  //------------------------------------------------------------------------------

  ClipperBase::~ClipperBase()
  {
    Clear();
  }

  void ClipperBase::DeleteEdges(Active*& e)
  {
    while (e)
    {
      Active* e2 = e;
      e = e->next_in_ael;
      delete e2;
    }
  }

  void ClipperBase::CleanUp()
  {
    DeleteEdges(actives_);
    scanline_list_ = std::priority_queue<int64_t>();
    intersect_nodes_.clear();
    DisposeAllOutRecs();
    horz_seg_list_.clear();
    horz_join_list_.clear();
  }


  void ClipperBase::Clear()
  {
    CleanUp();
    DisposeVerticesAndLocalMinima();
    current_locmin_iter_ = minima_list_.begin();
    minima_list_sorted_ = false;
    has_open_paths_ = false;
  }


  void ClipperBase::Reset()
  {
    if (!minima_list_sorted_)
    {
      std::stable_sort(minima_list_.begin(), minima_list_.end(), LocMinSorter()); //#594
      minima_list_sorted_ = true;
    }
    LocalMinimaList::const_reverse_iterator i;
    for (i = minima_list_.rbegin(); i != minima_list_.rend(); ++i)
      InsertScanline((*i)->vertex->pt.y);

    current_locmin_iter_ = minima_list_.begin();
    actives_ = nullptr;
    sel_ = nullptr;
    succeeded_ = true;
  }


#ifdef USINGZ
  void ClipperBase::SetZ(const Active& e1, const Active& e2, Point64& ip)
  {
    if (!zCallback_) return;
    // prioritize subject over clip vertices by passing
    // subject vertices before clip vertices in the callback
    if (GetPolyType(e1) == PathType::Subject)
    {
      if (ip == e1.bot) ip.z = e1.bot.z;
      else if (ip == e1.top) ip.z = e1.top.z;
      else if (ip == e2.bot) ip.z = e2.bot.z;
      else if (ip == e2.top) ip.z = e2.top.z;
      else ip.z = DefaultZ;
      zCallback_(e1.bot, e1.top, e2.bot, e2.top, ip);
    }
    else
    {
      if (ip == e2.bot) ip.z = e2.bot.z;
      else if (ip == e2.top) ip.z = e2.top.z;
      else if (ip == e1.bot) ip.z = e1.bot.z;
      else if (ip == e1.top) ip.z = e1.top.z;
      else ip.z = DefaultZ;
      zCallback_(e2.bot, e2.top, e1.bot, e1.top, ip);
    }
  }
#endif

  void ClipperBase::AddPath(const Path64& path, PathType polytype, bool is_open)
  {
    Paths64 tmp;
    tmp.push_back(path);
    AddPaths(tmp, polytype, is_open);
  }

  void ClipperBase::AddPaths(const Paths64& paths, PathType polytype, bool is_open)
  {
    if (is_open) has_open_paths_ = true;
    minima_list_sorted_ = false;
    AddPaths_(paths, polytype, is_open, vertex_lists_, minima_list_);
  }

  void ClipperBase::AddReuseableData(const ReuseableDataContainer64& reuseable_data)
  {
    // nb: reuseable_data will continue to own the vertices
    // and remains responsible for their clean up.
    succeeded_ = false;
    minima_list_sorted_ = false;
    LocalMinimaList::const_iterator i;
    for (i = reuseable_data.minima_list_.cbegin(); i != reuseable_data.minima_list_.cend(); ++i)
    {
      minima_list_.push_back(std::make_unique <LocalMinima>((*i)->vertex, (*i)->polytype, (*i)->is_open));
      if ((*i)->is_open) has_open_paths_ = true;
    }
  }

  void ClipperBase::InsertScanline(int64_t y)
  {
    scanline_list_.push(y);
  }


  bool ClipperBase::PopScanline(int64_t& y)
  {
    if (scanline_list_.empty()) return false;
    y = scanline_list_.top();
    scanline_list_.pop();
    while (!scanline_list_.empty() && y == scanline_list_.top())
      scanline_list_.pop();  // Pop duplicates.
    return true;
  }


  bool ClipperBase::PopLocalMinima(int64_t y, LocalMinima*& local_minima)
  {
    if (current_locmin_iter_ == minima_list_.end() || (*current_locmin_iter_)->vertex->pt.y != y) return false;
    local_minima = (current_locmin_iter_++)->get();
    return true;
  }

  void ClipperBase::DisposeAllOutRecs()
  {
    for (auto outrec : outrec_list_)
    {
      if (outrec->pts) DisposeOutPts(outrec);
      delete outrec;
    }
    outrec_list_.resize(0);
  }

  void ClipperBase::DisposeVerticesAndLocalMinima()
  {
    minima_list_.clear();
    for (auto v : vertex_lists_) delete[] v;
    vertex_lists_.clear();
  }


  void ClipperBase::AddLocMin(Vertex& vert, PathType polytype, bool is_open)
  {
    //make sure the vertex is added only once ...
    if ((VertexFlags::LocalMin & vert.flags) != VertexFlags::None) return;

    vert.flags = (vert.flags | VertexFlags::LocalMin);
    minima_list_.push_back(std::make_unique <LocalMinima>(&vert, polytype, is_open));
  }

  bool ClipperBase::IsContributingClosed(const Active& e) const
  {
    switch (fillrule_)
    {
    case FillRule::EvenOdd:
      break;
    case FillRule::NonZero:
      if (abs(e.wind_cnt) != 1) return false;
      break;
    case FillRule::Positive:
      if (e.wind_cnt != 1) return false;
      break;
    case FillRule::Negative:
      if (e.wind_cnt != -1) return false;
      break;
    }

    switch (cliptype_)
    {
    case ClipType::None:
      return false;
    case ClipType::Intersection:
      switch (fillrule_)
      {
      case FillRule::Positive:
        return (e.wind_cnt2 > 0);
      case FillRule::Negative:
        return (e.wind_cnt2 < 0);
      default:
        return (e.wind_cnt2 != 0);
      }
      break;

    case ClipType::Union:
      switch (fillrule_)
      {
      case FillRule::Positive:
        return (e.wind_cnt2 <= 0);
      case FillRule::Negative:
        return (e.wind_cnt2 >= 0);
      default:
        return (e.wind_cnt2 == 0);
      }
      break;

    case ClipType::Difference:
      bool result;
      switch (fillrule_)
      {
      case FillRule::Positive:
        result = (e.wind_cnt2 <= 0);
        break;
      case FillRule::Negative:
        result = (e.wind_cnt2 >= 0);
        break;
      default:
        result = (e.wind_cnt2 == 0);
      }
      if (GetPolyType(e) == PathType::Subject)
        return result;
      else
        return !result;
      break;

    case ClipType::Xor: return true;  break;
    }
    return false;  // we should never get here
  }


  inline bool ClipperBase::IsContributingOpen(const Active& e) const
  {
    bool is_in_clip, is_in_subj;
    switch (fillrule_)
    {
    case FillRule::Positive:
      is_in_clip = e.wind_cnt2 > 0;
      is_in_subj = e.wind_cnt > 0;
      break;
    case FillRule::Negative:
      is_in_clip = e.wind_cnt2 < 0;
      is_in_subj = e.wind_cnt < 0;
      break;
    default:
      is_in_clip = e.wind_cnt2 != 0;
      is_in_subj = e.wind_cnt != 0;
    }

    switch (cliptype_)
    {
    case ClipType::Intersection: return is_in_clip;
    case ClipType::Union: return (!is_in_subj && !is_in_clip);
    default: return !is_in_clip;
    }
  }


  void ClipperBase::SetWindCountForClosedPathEdge(Active& e)
  {
    //Wind counts refer to polygon regions not edges, so here an edge's WindCnt
    //indicates the higher of the wind counts for the two regions touching the
    //edge. (NB Adjacent regions can only ever have their wind counts differ by
    //one. Also, open paths have no meaningful wind directions or counts.)

    Active* e2 = e.prev_in_ael;
    //find the nearest closed path edge of the same PolyType in AEL (heading left)
    PathType pt = GetPolyType(e);
    while (e2 && (GetPolyType(*e2) != pt || IsOpen(*e2))) e2 = e2->prev_in_ael;

    if (!e2)
    {
      e.wind_cnt = e.wind_dx;
      e2 = actives_;
    }
    else if (fillrule_ == FillRule::EvenOdd)
    {
      e.wind_cnt = e.wind_dx;
      e.wind_cnt2 = e2->wind_cnt2;
      e2 = e2->next_in_ael;
    }
    else
    {
      //NonZero, positive, or negative filling here ...
      //if e's WindCnt is in the SAME direction as its WindDx, then polygon
      //filling will be on the right of 'e'.
      //NB neither e2.WindCnt nor e2.WindDx should ever be 0.
      if (e2->wind_cnt * e2->wind_dx < 0)
      {
        //opposite directions so 'e' is outside 'e2' ...
        if (abs(e2->wind_cnt) > 1)
        {
          //outside prev poly but still inside another.
          if (e2->wind_dx * e.wind_dx < 0)
            //reversing direction so use the same WC
            e.wind_cnt = e2->wind_cnt;
          else
            //otherwise keep 'reducing' the WC by 1 (ie towards 0) ...
            e.wind_cnt = e2->wind_cnt + e.wind_dx;
        }
        else
          //now outside all polys of same polytype so set own WC ...
          e.wind_cnt = (IsOpen(e) ? 1 : e.wind_dx);
      }
      else
      {
        //'e' must be inside 'e2'
        if (e2->wind_dx * e.wind_dx < 0)
          //reversing direction so use the same WC
          e.wind_cnt = e2->wind_cnt;
        else
          //otherwise keep 'increasing' the WC by 1 (ie away from 0) ...
          e.wind_cnt = e2->wind_cnt + e.wind_dx;
      }
      e.wind_cnt2 = e2->wind_cnt2;
      e2 = e2->next_in_ael;  // ie get ready to calc WindCnt2
    }

    //update wind_cnt2 ...
    if (fillrule_ == FillRule::EvenOdd)
      while (e2 != &e)
      {
        if (GetPolyType(*e2) != pt && !IsOpen(*e2))
          e.wind_cnt2 = (e.wind_cnt2 == 0 ? 1 : 0);
        e2 = e2->next_in_ael;
      }
    else
      while (e2 != &e)
      {
        if (GetPolyType(*e2) != pt && !IsOpen(*e2))
          e.wind_cnt2 += e2->wind_dx;
        e2 = e2->next_in_ael;
      }
  }


  void ClipperBase::SetWindCountForOpenPathEdge(Active& e)
  {
    Active* e2 = actives_;
    if (fillrule_ == FillRule::EvenOdd)
    {
      int cnt1 = 0, cnt2 = 0;
      while (e2 != &e)
      {
        if (GetPolyType(*e2) == PathType::Clip)
          cnt2++;
        else if (!IsOpen(*e2))
          cnt1++;
        e2 = e2->next_in_ael;
      }
      e.wind_cnt = (IsOdd(cnt1) ? 1 : 0);
      e.wind_cnt2 = (IsOdd(cnt2) ? 1 : 0);
    }
    else
    {
      while (e2 != &e)
      {
        if (GetPolyType(*e2) == PathType::Clip)
          e.wind_cnt2 += e2->wind_dx;
        else if (!IsOpen(*e2))
          e.wind_cnt += e2->wind_dx;
        e2 = e2->next_in_ael;
      }
    }
  }

  bool IsValidAelOrder(const Active& resident, const Active& newcomer)
  {
    if (newcomer.curr_x != resident.curr_x)
        return newcomer.curr_x > resident.curr_x;

    //get the turning direction  a1.top, a2.bot, a2.top
    double d = CrossProduct(resident.top, newcomer.bot, newcomer.top);
    if (d != 0) return d < 0;

    //edges must be collinear to get here
    //for starting open paths, place them according to
    //the direction they're about to turn
    if (!IsMaxima(resident) && (resident.top.y > newcomer.top.y))
    {
      return CrossProduct(newcomer.bot,
        resident.top, NextVertex(resident)->pt) <= 0;
    }
    else if (!IsMaxima(newcomer) && (newcomer.top.y > resident.top.y))
    {
      return CrossProduct(newcomer.bot,
        newcomer.top, NextVertex(newcomer)->pt) >= 0;
    }

    int64_t y = newcomer.bot.y;
    bool newcomerIsLeft = newcomer.is_left_bound;

    if (resident.bot.y != y || resident.local_min->vertex->pt.y != y)
      return newcomer.is_left_bound;
    //resident must also have just been inserted
    else if (resident.is_left_bound != newcomerIsLeft)
      return newcomerIsLeft;
    else if (IsCollinear(PrevPrevVertex(resident)->pt,
      resident.bot, resident.top)) return true;
    else
      //compare turning direction of the alternate bound
      return (CrossProduct(PrevPrevVertex(resident)->pt,
        newcomer.bot, PrevPrevVertex(newcomer)->pt) > 0) == newcomerIsLeft;
  }


  void ClipperBase::InsertLeftEdge(Active& e)
  {
    Active* e2;
    if (!actives_)
    {
      e.prev_in_ael = nullptr;
      e.next_in_ael = nullptr;
      actives_ = &e;
    }
    else if (!IsValidAelOrder(*actives_, e))
    {
      e.prev_in_ael = nullptr;
      e.next_in_ael = actives_;
      actives_->prev_in_ael = &e;
      actives_ = &e;
    }
    else
    {
      e2 = actives_;
      while (e2->next_in_ael && IsValidAelOrder(*e2->next_in_ael, e))
        e2 = e2->next_in_ael;
      if (e2->join_with == JoinWith::Right)
        e2 = e2->next_in_ael;
      if (!e2) return; // should never happen and stops compiler warning :)
      e.next_in_ael = e2->next_in_ael;
      if (e2->next_in_ael) e2->next_in_ael->prev_in_ael = &e;
      e.prev_in_ael = e2;
      e2->next_in_ael = &e;
    }
  }


  void InsertRightEdge(Active& e, Active& e2)
  {
    e2.next_in_ael = e.next_in_ael;
    if (e.next_in_ael) e.next_in_ael->prev_in_ael = &e2;
    e2.prev_in_ael = &e;
    e.next_in_ael = &e2;
  }


  void ClipperBase::InsertLocalMinimaIntoAEL(int64_t bot_y)
  {
    LocalMinima* local_minima;
    Active* left_bound, * right_bound;
    //Add any local minima (if any) at BotY ...
    //nb: horizontal local minima edges should contain locMin.vertex.prev

    while (PopLocalMinima(bot_y, local_minima))
    {
      if ((local_minima->vertex->flags & VertexFlags::OpenStart) != VertexFlags::None)
      {
        left_bound = nullptr;
      }
      else
      {
        left_bound = new Active();
        left_bound->bot = local_minima->vertex->pt;
        left_bound->curr_x = left_bound->bot.x;
        left_bound->wind_dx = -1;
        left_bound->vertex_top = local_minima->vertex->prev;  // ie descending
        left_bound->top = left_bound->vertex_top->pt;
        left_bound->local_min = local_minima;
        SetDx(*left_bound);
      }

      if ((local_minima->vertex->flags & VertexFlags::OpenEnd) != VertexFlags::None)
      {
        right_bound = nullptr;
      }
      else
      {
        right_bound = new Active();
        right_bound->bot = local_minima->vertex->pt;
        right_bound->curr_x = right_bound->bot.x;
        right_bound->wind_dx = 1;
        right_bound->vertex_top = local_minima->vertex->next;  // ie ascending
        right_bound->top = right_bound->vertex_top->pt;
        right_bound->local_min = local_minima;
        SetDx(*right_bound);
      }

      //Currently LeftB is just the descending bound and RightB is the ascending.
      //Now if the LeftB isn't on the left of RightB then we need swap them.
      if (left_bound && right_bound)
      {
        if (IsHorizontal(*left_bound))
        {
          if (IsHeadingRightHorz(*left_bound)) SwapActives(left_bound, right_bound);
        }
        else if (IsHorizontal(*right_bound))
        {
          if (IsHeadingLeftHorz(*right_bound)) SwapActives(left_bound, right_bound);
        }
        else if (left_bound->dx < right_bound->dx)
          SwapActives(left_bound, right_bound);
      }
      else if (!left_bound)
      {
        left_bound = right_bound;
        right_bound = nullptr;
      }

      bool contributing;
      left_bound->is_left_bound = true;
      InsertLeftEdge(*left_bound);

      if (IsOpen(*left_bound))
      {
        SetWindCountForOpenPathEdge(*left_bound);
        contributing = IsContributingOpen(*left_bound);
      }
      else
      {
        SetWindCountForClosedPathEdge(*left_bound);
        contributing = IsContributingClosed(*left_bound);
      }

      if (right_bound)
      {
        right_bound->is_left_bound = false;
        right_bound->wind_cnt = left_bound->wind_cnt;
        right_bound->wind_cnt2 = left_bound->wind_cnt2;
        InsertRightEdge(*left_bound, *right_bound);  ///////
        if (contributing)
        {
          AddLocalMinPoly(*left_bound, *right_bound, left_bound->bot, true);
          if (!IsHorizontal(*left_bound))
            CheckJoinLeft(*left_bound, left_bound->bot);
        }

        while (right_bound->next_in_ael &&
          IsValidAelOrder(*right_bound->next_in_ael, *right_bound))
        {
          IntersectEdges(*right_bound, *right_bound->next_in_ael, right_bound->bot);
          SwapPositionsInAEL(*right_bound, *right_bound->next_in_ael);
        }

        if (IsHorizontal(*right_bound))
          PushHorz(*right_bound);
        else
        {
          CheckJoinRight(*right_bound, right_bound->bot);
          InsertScanline(right_bound->top.y);
        }
      }
      else if (contributing)
      {
        StartOpenPath(*left_bound, left_bound->bot);
      }

      if (IsHorizontal(*left_bound))
        PushHorz(*left_bound);
      else
        InsertScanline(left_bound->top.y);
    }  // while (PopLocalMinima())
  }


  inline void ClipperBase::PushHorz(Active& e)
  {
    e.next_in_sel = (sel_ ? sel_ : nullptr);
    sel_ = &e;
  }


  inline bool ClipperBase::PopHorz(Active*& e)
  {
    e = sel_;
    if (!e) return false;
    sel_ = sel_->next_in_sel;
    return true;
  }


  OutPt* ClipperBase::AddLocalMinPoly(Active& e1, Active& e2,
    const Point64& pt, bool is_new)
  {
    OutRec* outrec = NewOutRec();
    e1.outrec = outrec;
    e2.outrec = outrec;

    if (IsOpen(e1))
    {
      outrec->owner = nullptr;
      outrec->is_open = true;
      if (e1.wind_dx > 0)
        SetSides(*outrec, e1, e2);
      else
        SetSides(*outrec, e2, e1);
    }
    else
    {
      Active* prevHotEdge = GetPrevHotEdge(e1);
      //e.windDx is the winding direction of the **input** paths
      //and unrelated to the winding direction of output polygons.
      //Output orientation is determined by e.outrec.frontE which is
      //the ascending edge (see AddLocalMinPoly).
      if (prevHotEdge)
      {
        if (using_polytree_)
          SetOwner(outrec, prevHotEdge->outrec);
        if (OutrecIsAscending(prevHotEdge) == is_new)
          SetSides(*outrec, e2, e1);
        else
          SetSides(*outrec, e1, e2);
      }
      else
      {
        outrec->owner = nullptr;
        if (is_new)
          SetSides(*outrec, e1, e2);
        else
          SetSides(*outrec, e2, e1);
      }
    }

    OutPt* op = new OutPt(pt, outrec);
    outrec->pts = op;
    return op;
  }


  OutPt* ClipperBase::AddLocalMaxPoly(Active& e1, Active& e2, const Point64& pt)
  {
    if (IsJoined(e1)) Split(e1, pt);
    if (IsJoined(e2)) Split(e2, pt);

    if (IsFront(e1) == IsFront(e2))
    {
      if (IsOpenEnd(e1))
        SwapFrontBackSides(*e1.outrec);
      else if (IsOpenEnd(e2))
        SwapFrontBackSides(*e2.outrec);
      else
      {
        succeeded_ = false;
        return nullptr;
      }
    }

    OutPt* result = AddOutPt(e1, pt);
    if (e1.outrec == e2.outrec)
    {
      OutRec& outrec = *e1.outrec;
      outrec.pts = result;

      if (using_polytree_)
      {
        Active* e = GetPrevHotEdge(e1);
        if (!e)
          outrec.owner = nullptr;
        else
          SetOwner(&outrec, e->outrec);
        // nb: outRec.owner here is likely NOT the real
        // owner but this will be checked in RecursiveCheckOwners()
      }

      UncoupleOutRec(e1);
      result = outrec.pts;
      if (outrec.owner && !outrec.owner->front_edge)
        outrec.owner = GetRealOutRec(outrec.owner);
    }
    //and to preserve the winding orientation of outrec ...
    else if (IsOpen(e1))
    {
      if (e1.wind_dx < 0)
        JoinOutrecPaths(e1, e2);
      else
        JoinOutrecPaths(e2, e1);
    }
    else if (e1.outrec->idx < e2.outrec->idx)
      JoinOutrecPaths(e1, e2);
    else
      JoinOutrecPaths(e2, e1);
    return result;
  }

  void ClipperBase::JoinOutrecPaths(Active& e1, Active& e2)
  {
    //join e2 outrec path onto e1 outrec path and then delete e2 outrec path
    //pointers. (NB Only very rarely do the joining ends share the same coords.)
    OutPt* p1_st = e1.outrec->pts;
    OutPt* p2_st = e2.outrec->pts;
    OutPt* p1_end = p1_st->next;
    OutPt* p2_end = p2_st->next;
    if (IsFront(e1))
    {
      p2_end->prev = p1_st;
      p1_st->next = p2_end;
      p2_st->next = p1_end;
      p1_end->prev = p2_st;
      e1.outrec->pts = p2_st;
      e1.outrec->front_edge = e2.outrec->front_edge;
      if (e1.outrec->front_edge)
        e1.outrec->front_edge->outrec = e1.outrec;
    }
    else
    {
      p1_end->prev = p2_st;
      p2_st->next = p1_end;
      p1_st->next = p2_end;
      p2_end->prev = p1_st;
      e1.outrec->back_edge = e2.outrec->back_edge;
      if (e1.outrec->back_edge)
        e1.outrec->back_edge->outrec = e1.outrec;
    }

    //after joining, the e2.OutRec must contains no vertices ...
    e2.outrec->front_edge = nullptr;
    e2.outrec->back_edge = nullptr;
    e2.outrec->pts = nullptr;

    if (IsOpenEnd(e1))
    {
      e2.outrec->pts = e1.outrec->pts;
      e1.outrec->pts = nullptr;
    }
    else
      SetOwner(e2.outrec, e1.outrec);

    //and e1 and e2 are maxima and are about to be dropped from the Actives list.
    e1.outrec = nullptr;
    e2.outrec = nullptr;
  }

  OutRec* ClipperBase::NewOutRec()
  {
    OutRec* result = new OutRec();
    result->idx = outrec_list_.size();
    outrec_list_.push_back(result);
    result->pts = nullptr;
    result->owner = nullptr;
    result->polypath = nullptr;
    result->is_open = false;
    result->splits = nullptr;
    return result;
  }


  OutPt* ClipperBase::AddOutPt(const Active& e, const Point64& pt)
  {
    OutPt* new_op = nullptr;

    //Outrec.OutPts: a circular doubly-linked-list of POutPt where ...
    //op_front[.Prev]* ~~~> op_back & op_back == op_front.Next
    OutRec* outrec = e.outrec;
    bool to_front = IsFront(e);
    OutPt* op_front = outrec->pts;
    OutPt* op_back = op_front->next;

    if (to_front)
    {
      if (pt == op_front->pt)
        return op_front;
    }
    else if (pt == op_back->pt)
      return op_back;

    new_op = new OutPt(pt, outrec);
    op_back->prev = new_op;
    new_op->prev = op_front;
    new_op->next = op_back;
    op_front->next = new_op;
    if (to_front) outrec->pts = new_op;
    return new_op;
  }

  void ClipperBase::CleanCollinear(OutRec* outrec)
  {
    outrec = GetRealOutRec(outrec);
    if (!outrec || outrec->is_open) return;
    if (!IsValidClosedPath(outrec->pts))
    {
      DisposeOutPts(outrec);
      return;
    }

    OutPt* startOp = outrec->pts, * op2 = startOp;
    for (; ; )
    {
      //NB if preserveCollinear == true, then only remove 180 deg. spikes
      if (IsCollinear(op2->prev->pt, op2->pt, op2->next->pt) &&
        (op2->pt == op2->prev->pt ||
          op2->pt == op2->next->pt || !preserve_collinear_ ||
          DotProduct(op2->prev->pt, op2->pt, op2->next->pt) < 0))
      {

        if (op2 == outrec->pts) outrec->pts = op2->prev;

        op2 = DisposeOutPt(op2);
        if (!IsValidClosedPath(op2))
        {
          DisposeOutPts(outrec);
          return;
        }
        startOp = op2;
        continue;
      }
      op2 = op2->next;
      if (op2 == startOp) break;
    }
    FixSelfIntersects(outrec);
  }

  void ClipperBase::DoSplitOp(OutRec* outrec, OutPt* splitOp)
  {
    // splitOp.prev -> splitOp &&
    // splitOp.next -> splitOp.next.next are intersecting
    OutPt* prevOp = splitOp->prev;
    OutPt* nextNextOp = splitOp->next->next;
    outrec->pts = prevOp;

    Point64 ip;
    GetSegmentIntersectPt(prevOp->pt, splitOp->pt,
      splitOp->next->pt, nextNextOp->pt, ip);

#ifdef USINGZ
    if (zCallback_) zCallback_(prevOp->pt, splitOp->pt,
      splitOp->next->pt, nextNextOp->pt, ip);
#endif
    double area1 = Area(outrec->pts);
    double absArea1 = std::fabs(area1);
    if (absArea1 < 2)
    {
      DisposeOutPts(outrec);
      return;
    }

    double area2 = AreaTriangle(ip, splitOp->pt, splitOp->next->pt);
    double absArea2 = std::fabs(area2);

    // de-link splitOp and splitOp.next from the path
    // while inserting the intersection point
    if (ip == prevOp->pt || ip == nextNextOp->pt)
    {
      nextNextOp->prev = prevOp;
      prevOp->next = nextNextOp;
    }
    else
    {
      OutPt* newOp2 = new OutPt(ip, prevOp->outrec);
      newOp2->prev = prevOp;
      newOp2->next = nextNextOp;
      nextNextOp->prev = newOp2;
      prevOp->next = newOp2;
    }

    // area1 is the path's area *before* splitting, whereas area2 is
    // the area of the triangle containing splitOp & splitOp.next.
    // So the only way for these areas to have the same sign is if
    // the split triangle is larger than the path containing prevOp or
    // if there's more than one self-intersection.
    if (absArea2 >= 1 &&
      (absArea2 > absArea1 || (area2 > 0) == (area1 > 0)))
    {
      OutRec* newOr = NewOutRec();
      newOr->owner = outrec->owner;

      splitOp->outrec = newOr;
      splitOp->next->outrec = newOr;
      OutPt* newOp = new OutPt(ip, newOr);
      newOp->prev = splitOp->next;
      newOp->next = splitOp;
      newOr->pts = newOp;
      splitOp->prev = newOp;
      splitOp->next->next = newOp;

      if (using_polytree_)
      {
        if (Path1InsidePath2(prevOp, newOp))
        {
          newOr->splits = new OutRecList();
          newOr->splits->push_back(outrec);
        }
        else
        {
          if (!outrec->splits) outrec->splits = new OutRecList();
          outrec->splits->push_back(newOr);
        }
      }
    }
    else
    {
      delete splitOp->next;
      delete splitOp;
    }
  }

  void ClipperBase::FixSelfIntersects(OutRec* outrec)
  {
    OutPt* op2 = outrec->pts;
    for (; ; )
    {
      // triangles can't self-intersect
      if (op2->prev == op2->next->next) break;
      if (SegmentsIntersect(op2->prev->pt,
        op2->pt, op2->next->pt, op2->next->next->pt))
      {
        if (op2 == outrec->pts || op2->next == outrec->pts)
          outrec->pts = outrec->pts->prev;
        DoSplitOp(outrec, op2);
        if (!outrec->pts) break;
        op2 = outrec->pts;
        continue;
      }
      else
        op2 = op2->next;

      if (op2 == outrec->pts) break;
    }
  }


  inline void UpdateOutrecOwner(OutRec* outrec)
  {
    OutPt* opCurr = outrec->pts;
    for (; ; )
    {
      opCurr->outrec = outrec;
      opCurr = opCurr->next;
      if (opCurr == outrec->pts) return;
    }
  }


  OutPt* ClipperBase::StartOpenPath(Active& e, const Point64& pt)
  {
    OutRec* outrec = NewOutRec();
    outrec->is_open = true;

    if (e.wind_dx > 0)
    {
      outrec->front_edge = &e;
      outrec->back_edge = nullptr;
    }
    else
    {
      outrec->front_edge = nullptr;
      outrec->back_edge = &e;
    }

    e.outrec = outrec;

    OutPt* op = new OutPt(pt, outrec);
    outrec->pts = op;
    return op;
  }

  inline void TrimHorz(Active& horzEdge, bool preserveCollinear)
  {
    bool wasTrimmed = false;
    Point64 pt = NextVertex(horzEdge)->pt;
    while (pt.y == horzEdge.top.y)
    {
      //always trim 180 deg. spikes (in closed paths)
      //but otherwise break if preserveCollinear = true
      if (preserveCollinear &&
        ((pt.x < horzEdge.top.x) != (horzEdge.bot.x < horzEdge.top.x)))
        break;

      horzEdge.vertex_top = NextVertex(horzEdge);
      horzEdge.top = pt;
      wasTrimmed = true;
      if (IsMaxima(horzEdge)) break;
      pt = NextVertex(horzEdge)->pt;
    }

    if (wasTrimmed) SetDx(horzEdge); // +/-infinity
  }


  inline void ClipperBase::UpdateEdgeIntoAEL(Active* e)
  {
    e->bot = e->top;
    e->vertex_top = NextVertex(*e);
    e->top = e->vertex_top->pt;
    e->curr_x = e->bot.x;
    SetDx(*e);

    if (IsJoined(*e)) Split(*e, e->bot);

    if (IsHorizontal(*e))
    {
      if (!IsOpen(*e)) TrimHorz(*e, preserve_collinear_);
      return;
    }

    InsertScanline(e->top.y);
    CheckJoinLeft(*e, e->bot);
    CheckJoinRight(*e, e->bot, true); // (#500)
  }

  Active* FindEdgeWithMatchingLocMin(Active* e)
  {
    Active* result = e->next_in_ael;
    while (result)
    {
      if (result->local_min == e->local_min) return result;
      else if (!IsHorizontal(*result) && e->bot != result->bot) result = nullptr;
      else result = result->next_in_ael;
    }
    result = e->prev_in_ael;
    while (result)
    {
      if (result->local_min == e->local_min) return result;
      else if (!IsHorizontal(*result) && e->bot != result->bot) return nullptr;
      else result = result->prev_in_ael;
    }
    return result;
  }


  void ClipperBase::IntersectEdges(Active& e1, Active& e2, const Point64& pt)
  {
    //MANAGE OPEN PATH INTERSECTIONS SEPARATELY ...
    if (has_open_paths_ && (IsOpen(e1) || IsOpen(e2)))
    {
      if (IsOpen(e1) && IsOpen(e2)) return;
      Active* edge_o, * edge_c;
      if (IsOpen(e1))
      {
        edge_o = &e1;
        edge_c = &e2;
      }
      else
      {
        edge_o = &e2;
        edge_c = &e1;
      }
      if (IsJoined(*edge_c)) Split(*edge_c, pt); // needed for safety

      if (abs(edge_c->wind_cnt) != 1) return;
      switch (cliptype_)
      {
      case ClipType::Union:
        if (!IsHotEdge(*edge_c)) return;
        break;
      default:
        if (edge_c->local_min->polytype == PathType::Subject)
          return;
      }

      switch (fillrule_)
      {
      case FillRule::Positive: 
        if (edge_c->wind_cnt != 1) return; 
        break;
      case FillRule::Negative: 
        if (edge_c->wind_cnt != -1) return; 
        break;
      default: 
        if (std::abs(edge_c->wind_cnt) != 1) return; 
      }

#ifdef USINGZ
      OutPt* resultOp;
#endif
      //toggle contribution ...
      if (IsHotEdge(*edge_o))
      {
#ifdef USINGZ
        resultOp = AddOutPt(*edge_o, pt);
#else
        AddOutPt(*edge_o, pt);
#endif
        if (IsFront(*edge_o)) edge_o->outrec->front_edge = nullptr;
        else edge_o->outrec->back_edge = nullptr;
        edge_o->outrec = nullptr;
      }

      //horizontal edges can pass under open paths at a LocMins
      else if (pt == edge_o->local_min->vertex->pt &&
        !IsOpenEnd(*edge_o->local_min->vertex))
      {
        //find the other side of the LocMin and
        //if it's 'hot' join up with it ...
        Active* e3 = FindEdgeWithMatchingLocMin(edge_o);
        if (e3 && IsHotEdge(*e3))
        {
          edge_o->outrec = e3->outrec;
          if (edge_o->wind_dx > 0)
            SetSides(*e3->outrec, *edge_o, *e3);
          else
            SetSides(*e3->outrec, *e3, *edge_o);
          return;
        }
        else
#ifdef USINGZ
          resultOp = StartOpenPath(*edge_o, pt);
#else
          StartOpenPath(*edge_o, pt);
#endif
      }
      else
#ifdef USINGZ
        resultOp = StartOpenPath(*edge_o, pt);
#else
        StartOpenPath(*edge_o, pt);
#endif

#ifdef USINGZ
      if (zCallback_) SetZ(*edge_o, *edge_c, resultOp->pt);
#endif
      return;
    } // end of an open path intersection

    //MANAGING CLOSED PATHS FROM HERE ON

    if (IsJoined(e1)) Split(e1, pt);
    if (IsJoined(e2)) Split(e2, pt);

    //UPDATE WINDING COUNTS...

    int old_e1_windcnt, old_e2_windcnt;
    if (e1.local_min->polytype == e2.local_min->polytype)
    {
      if (fillrule_ == FillRule::EvenOdd)
      {
        old_e1_windcnt = e1.wind_cnt;
        e1.wind_cnt = e2.wind_cnt;
        e2.wind_cnt = old_e1_windcnt;
      }
      else
      {
        if (e1.wind_cnt + e2.wind_dx == 0)
          e1.wind_cnt = -e1.wind_cnt;
        else
          e1.wind_cnt += e2.wind_dx;
        if (e2.wind_cnt - e1.wind_dx == 0)
          e2.wind_cnt = -e2.wind_cnt;
        else
          e2.wind_cnt -= e1.wind_dx;
      }
    }
    else
    {
      if (fillrule_ != FillRule::EvenOdd)
      {
        e1.wind_cnt2 += e2.wind_dx;
        e2.wind_cnt2 -= e1.wind_dx;
      }
      else
      {
        e1.wind_cnt2 = (e1.wind_cnt2 == 0 ? 1 : 0);
        e2.wind_cnt2 = (e2.wind_cnt2 == 0 ? 1 : 0);
      }
    }

    switch (fillrule_)
    {
    case FillRule::EvenOdd:
    case FillRule::NonZero:
      old_e1_windcnt = abs(e1.wind_cnt);
      old_e2_windcnt = abs(e2.wind_cnt);
      break;
    default:
      if (fillrule_ == fillpos)
      {
        old_e1_windcnt = e1.wind_cnt;
        old_e2_windcnt = e2.wind_cnt;
      }
      else
      {
        old_e1_windcnt = -e1.wind_cnt;
        old_e2_windcnt = -e2.wind_cnt;
      }
      break;
    }

    const bool e1_windcnt_in_01 = old_e1_windcnt == 0 || old_e1_windcnt == 1;
    const bool e2_windcnt_in_01 = old_e2_windcnt == 0 || old_e2_windcnt == 1;

    if ((!IsHotEdge(e1) && !e1_windcnt_in_01) || 
      (!IsHotEdge(e2) && !e2_windcnt_in_01))
        return;

    //NOW PROCESS THE INTERSECTION ...
#ifdef USINGZ
    OutPt* resultOp = nullptr;
#endif
    //if both edges are 'hot' ...
    if (IsHotEdge(e1) && IsHotEdge(e2))
    {
      if ((old_e1_windcnt != 0 && old_e1_windcnt != 1) || (old_e2_windcnt != 0 && old_e2_windcnt != 1) ||
        (e1.local_min->polytype != e2.local_min->polytype && cliptype_ != ClipType::Xor))
      {
#ifdef USINGZ
        resultOp = AddLocalMaxPoly(e1, e2, pt);
        if (zCallback_ && resultOp) SetZ(e1, e2, resultOp->pt);
#else
        AddLocalMaxPoly(e1, e2, pt);
#endif
      }
      else if (IsFront(e1) || (e1.outrec == e2.outrec))
      {
        //this 'else if' condition isn't strictly needed but
        //it's sensible to split polygons that ony touch at
        //a common vertex (not at common edges).

#ifdef USINGZ
        resultOp = AddLocalMaxPoly(e1, e2, pt);
        OutPt* op2 = AddLocalMinPoly(e1, e2, pt);
        if (zCallback_ && resultOp) SetZ(e1, e2, resultOp->pt);
        if (zCallback_) SetZ(e1, e2, op2->pt);
#else
        AddLocalMaxPoly(e1, e2, pt);
        AddLocalMinPoly(e1, e2, pt);
#endif
      }
      else
      {
#ifdef USINGZ
        resultOp = AddOutPt(e1, pt);
        OutPt* op2 = AddOutPt(e2, pt);
        if (zCallback_)
        {
          SetZ(e1, e2, resultOp->pt);
          SetZ(e1, e2, op2->pt);
        }
#else
        AddOutPt(e1, pt);
        AddOutPt(e2, pt);
#endif
        SwapOutrecs(e1, e2);
      }
    }
    else if (IsHotEdge(e1))
    {
#ifdef USINGZ
      resultOp = AddOutPt(e1, pt);
      if (zCallback_) SetZ(e1, e2, resultOp->pt);
#else
      AddOutPt(e1, pt);
#endif
      SwapOutrecs(e1, e2);
    }
    else if (IsHotEdge(e2))
    {
#ifdef USINGZ
      resultOp = AddOutPt(e2, pt);
      if (zCallback_) SetZ(e1, e2, resultOp->pt);
#else
      AddOutPt(e2, pt);
#endif
      SwapOutrecs(e1, e2);
    }
    else
    {
      int64_t e1Wc2, e2Wc2;
      switch (fillrule_)
      {
      case FillRule::EvenOdd:
      case FillRule::NonZero:
        e1Wc2 = abs(e1.wind_cnt2);
        e2Wc2 = abs(e2.wind_cnt2);
        break;
      default:
        if (fillrule_ == fillpos)
        {
          e1Wc2 = e1.wind_cnt2;
          e2Wc2 = e2.wind_cnt2;
        }
        else
        {
          e1Wc2 = -e1.wind_cnt2;
          e2Wc2 = -e2.wind_cnt2;
        }
        break;
      }

      if (!IsSamePolyType(e1, e2))
      {
#ifdef USINGZ
        resultOp = AddLocalMinPoly(e1, e2, pt, false);
        if (zCallback_) SetZ(e1, e2, resultOp->pt);
#else
        AddLocalMinPoly(e1, e2, pt, false);
#endif
      }
      else if (old_e1_windcnt == 1 && old_e2_windcnt == 1)
      {
#ifdef USINGZ
        resultOp = nullptr;
#endif
        switch (cliptype_)
        {
        case ClipType::Union:
          if (e1Wc2 <= 0 && e2Wc2 <= 0)
#ifdef USINGZ
            resultOp = AddLocalMinPoly(e1, e2, pt, false);
#else
            AddLocalMinPoly(e1, e2, pt, false);
#endif
          break;
        case ClipType::Difference:
          if (((GetPolyType(e1) == PathType::Clip) && (e1Wc2 > 0) && (e2Wc2 > 0)) ||
            ((GetPolyType(e1) == PathType::Subject) && (e1Wc2 <= 0) && (e2Wc2 <= 0)))
          {
#ifdef USINGZ
            resultOp = AddLocalMinPoly(e1, e2, pt, false);
#else
            AddLocalMinPoly(e1, e2, pt, false);
#endif
          }
          break;
        case ClipType::Xor:
#ifdef USINGZ
          resultOp = AddLocalMinPoly(e1, e2, pt, false);
#else
          AddLocalMinPoly(e1, e2, pt, false);
#endif
          break;
        default:
          if (e1Wc2 > 0 && e2Wc2 > 0)
#ifdef USINGZ
            resultOp = AddLocalMinPoly(e1, e2, pt, false);
#else
            AddLocalMinPoly(e1, e2, pt, false);
#endif
          break;
        }
#ifdef USINGZ
        if (resultOp && zCallback_) SetZ(e1, e2, resultOp->pt);
#endif
      }
    }
  }

  inline void ClipperBase::DeleteFromAEL(Active& e)
  {
    Active* prev = e.prev_in_ael;
    Active* next = e.next_in_ael;
    if (!prev && !next && (&e != actives_)) return;  // already deleted
    if (prev)
      prev->next_in_ael = next;
    else
      actives_ = next;
    if (next) next->prev_in_ael = prev;
    delete& e;
  }


  inline void ClipperBase::AdjustCurrXAndCopyToSEL(const int64_t top_y)
  {
    Active* e = actives_;
    sel_ = e;
    while (e)
    {
      e->prev_in_sel = e->prev_in_ael;
      e->next_in_sel = e->next_in_ael;
      e->jump = e->next_in_sel;
      if (e->join_with == JoinWith::Left)
        e->curr_x = e->prev_in_ael->curr_x; // also avoids complications
      else
        e->curr_x = TopX(*e, top_y);
      e = e->next_in_ael;
    }
  }

  bool ClipperBase::ExecuteInternal(ClipType ct, FillRule fillrule, bool use_polytrees)
  {
    cliptype_ = ct;
    fillrule_ = fillrule;
    using_polytree_ = use_polytrees;
    Reset();
    int64_t y;
    if (ct == ClipType::None || !PopScanline(y)) return true;

    while (succeeded_)
    {
      InsertLocalMinimaIntoAEL(y);
      Active* e;
      while (PopHorz(e)) DoHorizontal(*e);
      if (horz_seg_list_.size() > 0)
      {
        ConvertHorzSegsToJoins();
        horz_seg_list_.clear();
      }
      bot_y_ = y;  // bot_y_ == bottom of scanbeam
      if (!PopScanline(y)) break;  // y new top of scanbeam
      DoIntersections(y);
      DoTopOfScanbeam(y);
      while (PopHorz(e)) DoHorizontal(*e);
    }
    if (succeeded_) ProcessHorzJoins();
    return succeeded_;
  }

  inline void FixOutRecPts(OutRec* outrec)
  {
    OutPt* op = outrec->pts;
    do {
      op->outrec = outrec;
      op = op->next;
    } while (op != outrec->pts);
  }

  inline bool SetHorzSegHeadingForward(HorzSegment& hs, OutPt* opP, OutPt* opN)
  {
    if (opP->pt.x == opN->pt.x) return false;
    if (opP->pt.x < opN->pt.x)
    {
      hs.left_op = opP;
      hs.right_op = opN;
      hs.left_to_right = true;
    }
    else
    {
      hs.left_op = opN;
      hs.right_op = opP;
      hs.left_to_right = false;
    }
    return true;
  }

  inline bool UpdateHorzSegment(HorzSegment& hs)
  {
    OutPt* op = hs.left_op;
    OutRec* outrec = GetRealOutRec(op->outrec);
    bool outrecHasEdges = outrec->front_edge;
    int64_t curr_y = op->pt.y;
    OutPt* opP = op, * opN = op;
    if (outrecHasEdges)
    {
      OutPt* opA = outrec->pts, * opZ = opA->next;
      while (opP != opZ && opP->prev->pt.y == curr_y)
        opP = opP->prev;
      while (opN != opA && opN->next->pt.y == curr_y)
        opN = opN->next;
    }
    else
    {
      while (opP->prev != opN && opP->prev->pt.y == curr_y)
        opP = opP->prev;
      while (opN->next != opP && opN->next->pt.y == curr_y)
        opN = opN->next;
    }
    bool result =
      SetHorzSegHeadingForward(hs, opP, opN) &&
      !hs.left_op->horz;

    if (result)
      hs.left_op->horz = &hs;
    else
      hs.right_op = nullptr; // (for sorting)
    return result;
  }

  void ClipperBase::ConvertHorzSegsToJoins()
  {
    auto j = std::count_if(horz_seg_list_.begin(),
      horz_seg_list_.end(),
      [](HorzSegment& hs) { return UpdateHorzSegment(hs); });
    if (j < 2) return;

    std::stable_sort(horz_seg_list_.begin(), horz_seg_list_.end(), HorzSegSorter());

    HorzSegmentList::iterator hs1 = horz_seg_list_.begin(), hs2;
    HorzSegmentList::iterator hs_end = hs1 +j;
    HorzSegmentList::iterator hs_end1 = hs_end - 1;

    for (; hs1 != hs_end1; ++hs1)
    {
      for (hs2 = hs1 + 1; hs2 != hs_end; ++hs2)
      {
        if ((hs2->left_op->pt.x >= hs1->right_op->pt.x) ||
          (hs2->left_to_right == hs1->left_to_right) ||
          (hs2->right_op->pt.x <= hs1->left_op->pt.x)) continue;
        int64_t curr_y = hs1->left_op->pt.y;
        if (hs1->left_to_right)
        {
          while (hs1->left_op->next->pt.y == curr_y &&
            hs1->left_op->next->pt.x <= hs2->left_op->pt.x)
            hs1->left_op = hs1->left_op->next;
          while (hs2->left_op->prev->pt.y == curr_y &&
            hs2->left_op->prev->pt.x <= hs1->left_op->pt.x)
            hs2->left_op = hs2->left_op->prev;
          HorzJoin join = HorzJoin(
            DuplicateOp(hs1->left_op, true),
            DuplicateOp(hs2->left_op, false));
          horz_join_list_.push_back(join);
        }
        else
        {
          while (hs1->left_op->prev->pt.y == curr_y &&
            hs1->left_op->prev->pt.x <= hs2->left_op->pt.x)
            hs1->left_op = hs1->left_op->prev;
          while (hs2->left_op->next->pt.y == curr_y &&
            hs2->left_op->next->pt.x <= hs1->left_op->pt.x)
            hs2->left_op = hs2->left_op->next;
          HorzJoin join = HorzJoin(
            DuplicateOp(hs2->left_op, true),
            DuplicateOp(hs1->left_op, false));
          horz_join_list_.push_back(join);
        }
      }
    }
  }

  void MoveSplits(OutRec* fromOr, OutRec* toOr)
  {
    if (!fromOr->splits) return;
    if (!toOr->splits) toOr->splits = new OutRecList();
    OutRecList::iterator orIter = fromOr->splits->begin();
    for (; orIter != fromOr->splits->end(); ++orIter)
      toOr->splits->push_back(*orIter);
    fromOr->splits->clear();
  }


  void ClipperBase::ProcessHorzJoins()
  {
    for (const HorzJoin& j : horz_join_list_)
    {
      OutRec* or1 = GetRealOutRec(j.op1->outrec);
      OutRec* or2 = GetRealOutRec(j.op2->outrec);

      OutPt* op1b = j.op1->next;
      OutPt* op2b = j.op2->prev;
      j.op1->next = j.op2;
      j.op2->prev = j.op1;
      op1b->prev = op2b;
      op2b->next = op1b;

      if (or1 == or2) // 'join' is really a split
      {
        or2 = NewOutRec();
        or2->pts = op1b;
        FixOutRecPts(or2);

        //if or1->pts has moved to or2 then update or1->pts!!
        if (or1->pts->outrec == or2)
        {
          or1->pts = j.op1;
          or1->pts->outrec = or1;
        }

        if (using_polytree_) //#498, #520, #584, D#576, #618
        {
          if (Path1InsidePath2(or1->pts, or2->pts))
          {
            //swap or1's & or2's pts
            OutPt* tmp = or1->pts;
            or1->pts = or2->pts;
            or2->pts = tmp;
            FixOutRecPts(or1);
            FixOutRecPts(or2);
            //or2 is now inside or1
            or2->owner = or1;
          }
          else if (Path1InsidePath2(or2->pts, or1->pts))
          {
            or2->owner = or1;
          }
          else
            or2->owner = or1->owner;

          if (!or1->splits) or1->splits = new OutRecList();
          or1->splits->push_back(or2);
        }
        else
          or2->owner = or1;
      }
      else
      {
        or2->pts = nullptr;
        if (using_polytree_)
        {
          SetOwner(or2, or1);
          MoveSplits(or2, or1); //#618
        }
        else
          or2->owner = or1;
      }
    }
  }

  void ClipperBase::DoIntersections(const int64_t top_y)
  {
    if (BuildIntersectList(top_y))
    {
      ProcessIntersectList();
      intersect_nodes_.clear();
    }
  }

  void ClipperBase::AddNewIntersectNode(Active& e1, Active& e2, int64_t top_y)
  {
    Point64 ip;
    if (!GetSegmentIntersectPt(e1.bot, e1.top, e2.bot, e2.top, ip))
      ip = Point64(e1.curr_x, top_y); //parallel edges

    //rounding errors can occasionally place the calculated intersection
    //point either below or above the scanbeam, so check and correct ...
    if (ip.y > bot_y_ || ip.y < top_y)
    {
      double abs_dx1 = std::fabs(e1.dx);
      double abs_dx2 = std::fabs(e2.dx);
      if (abs_dx1 > 100 && abs_dx2 > 100)
      {
        if (abs_dx1 > abs_dx2)
          ip = GetClosestPointOnSegment(ip, e1.bot, e1.top);
        else
          ip = GetClosestPointOnSegment(ip, e2.bot, e2.top);
      }
      else if (abs_dx1 > 100)
        ip = GetClosestPointOnSegment(ip, e1.bot, e1.top);
      else if (abs_dx2 > 100)
        ip = GetClosestPointOnSegment(ip, e2.bot, e2.top);
      else
      {
        if (ip.y < top_y) ip.y = top_y;
        else ip.y = bot_y_;
        if (abs_dx1 < abs_dx2) ip.x = TopX(e1, ip.y);
        else ip.x = TopX(e2, ip.y);
      }
    }
    intersect_nodes_.push_back(IntersectNode(&e1, &e2, ip));
  }

  bool ClipperBase::BuildIntersectList(const int64_t top_y)
  {
    if (!actives_ || !actives_->next_in_ael) return false;

    //Calculate edge positions at the top of the current scanbeam, and from this
    //we will determine the intersections required to reach these new positions.
    AdjustCurrXAndCopyToSEL(top_y);
    //Find all edge intersections in the current scanbeam using a stable merge
    //sort that ensures only adjacent edges are intersecting. Intersect info is
    //stored in FIntersectList ready to be processed in ProcessIntersectList.
    //Re merge sorts see https://stackoverflow.com/a/46319131/359538

    Active* left = sel_, * right, * l_end, * r_end, * curr_base, * tmp;

    while (left && left->jump)
    {
      Active* prev_base = nullptr;
      while (left && left->jump)
      {
        curr_base = left;
        right = left->jump;
        l_end = right;
        r_end = right->jump;
        left->jump = r_end;
        while (left != l_end && right != r_end)
        {
          if (right->curr_x < left->curr_x)
          {
            tmp = right->prev_in_sel;
            for (; ; )
            {
              AddNewIntersectNode(*tmp, *right, top_y);
              if (tmp == left) break;
              tmp = tmp->prev_in_sel;
            }

            tmp = right;
            right = ExtractFromSEL(tmp);
            l_end = right;
            Insert1Before2InSEL(tmp, left);
            if (left == curr_base)
            {
              curr_base = tmp;
              curr_base->jump = r_end;
              if (!prev_base) sel_ = curr_base;
              else prev_base->jump = curr_base;
            }
          }
          else left = left->next_in_sel;
        }
        prev_base = curr_base;
        left = r_end;
      }
      left = sel_;
    }
    return intersect_nodes_.size() > 0;
  }

  void ClipperBase::ProcessIntersectList()
  {
    //We now have a list of intersections required so that edges will be
    //correctly positioned at the top of the scanbeam. However, it's important
    //that edge intersections are processed from the bottom up, but it's also
    //crucial that intersections only occur between adjacent edges.

    //First we do a quicksort so intersections proceed in a bottom up order ...
    std::sort(intersect_nodes_.begin(), intersect_nodes_.end(), IntersectListSort);
    //Now as we process these intersections, we must sometimes adjust the order
    //to ensure that intersecting edges are always adjacent ...

    IntersectNodeList::iterator node_iter, node_iter2;
    for (node_iter = intersect_nodes_.begin();
      node_iter != intersect_nodes_.end();  ++node_iter)
    {
      if (!EdgesAdjacentInAEL(*node_iter))
      {
        node_iter2 = node_iter + 1;
        while (!EdgesAdjacentInAEL(*node_iter2)) ++node_iter2;
        std::swap(*node_iter, *node_iter2);
      }

      IntersectNode& node = *node_iter;
      IntersectEdges(*node.edge1, *node.edge2, node.pt);
      SwapPositionsInAEL(*node.edge1, *node.edge2);

      node.edge1->curr_x = node.pt.x;
      node.edge2->curr_x = node.pt.x;
      CheckJoinLeft(*node.edge2, node.pt, true);
      CheckJoinRight(*node.edge1, node.pt, true);
    }
  }

  void ClipperBase::SwapPositionsInAEL(Active& e1, Active& e2)
  {
    //preconditon: e1 must be immediately to the left of e2
    Active* next = e2.next_in_ael;
    if (next) next->prev_in_ael = &e1;
    Active* prev = e1.prev_in_ael;
    if (prev) prev->next_in_ael = &e2;
    e2.prev_in_ael = prev;
    e2.next_in_ael = &e1;
    e1.prev_in_ael = &e2;
    e1.next_in_ael = next;
    if (!e2.prev_in_ael) actives_ = &e2;
  }

  inline OutPt* GetLastOp(const Active& hot_edge)
  {
    OutRec* outrec = hot_edge.outrec;
    OutPt* result = outrec->pts;
    if (&hot_edge != outrec->front_edge)
      result = result->next;
    return result;
  }

  void ClipperBase::AddTrialHorzJoin(OutPt* op)
  {
    if (op->outrec->is_open) return;
    horz_seg_list_.push_back(HorzSegment(op));
  }

  bool ClipperBase::ResetHorzDirection(const Active& horz,
    const Vertex* max_vertex, int64_t& horz_left, int64_t& horz_right)
  {
    if (horz.bot.x == horz.top.x)
    {
      //the horizontal edge is going nowhere ...
      horz_left = horz.curr_x;
      horz_right = horz.curr_x;
      Active* e = horz.next_in_ael;
      while (e && e->vertex_top != max_vertex) e = e->next_in_ael;
      return e != nullptr;
    }
    else if (horz.curr_x < horz.top.x)
    {
      horz_left = horz.curr_x;
      horz_right = horz.top.x;
      return true;
    }
    else
    {
      horz_left = horz.top.x;
      horz_right = horz.curr_x;
      return false;  // right to left
    }
  }

  void ClipperBase::DoHorizontal(Active& horz)
    /*******************************************************************************
        * Notes: Horizontal edges (HEs) at scanline intersections (ie at the top or    *
        * bottom of a scanbeam) are processed as if layered.The order in which HEs     *
        * are processed doesn't matter. HEs intersect with the bottom vertices of      *
        * other HEs[#] and with non-horizontal edges [*]. Once these intersections     *
        * are completed, intermediate HEs are 'promoted' to the next edge in their     *
        * bounds, and they in turn may be intersected[%] by other HEs.                 *
        *                                                                              *
        * eg: 3 horizontals at a scanline:    /   |                     /           /  *
        *              |                     /    |     (HE3)o ========%========== o   *
        *              o ======= o(HE2)     /     |         /         /                *
        *          o ============#=========*======*========#=========o (HE1)           *
        *         /              |        /       |       /                            *
        *******************************************************************************/
  {
    Point64 pt;
    bool horzIsOpen = IsOpen(horz);
    int64_t y = horz.bot.y;
    Vertex* vertex_max;
    if (horzIsOpen)
      vertex_max = GetCurrYMaximaVertex_Open(horz);
    else
      vertex_max = GetCurrYMaximaVertex(horz);

    //// remove 180 deg.spikes and also simplify
    //// consecutive horizontals when PreserveCollinear = true
    //if (!horzIsOpen && vertex_max != horz.vertex_top)
    //  TrimHorz(horz, PreserveCollinear);

    int64_t horz_left, horz_right;
    bool is_left_to_right =
      ResetHorzDirection(horz, vertex_max, horz_left, horz_right);

    if (IsHotEdge(horz))
    {
#ifdef USINGZ
      OutPt* op = AddOutPt(horz, Point64(horz.curr_x, y, horz.bot.z));
#else
      OutPt* op = AddOutPt(horz, Point64(horz.curr_x, y));
#endif
      AddTrialHorzJoin(op);
    }

    while (true) // loop through consec. horizontal edges
    {
      Active* e;
      if (is_left_to_right) e = horz.next_in_ael;
      else e = horz.prev_in_ael;

      while (e)
      {
        if (e->vertex_top == vertex_max)
        {
          if (IsHotEdge(horz) && IsJoined(*e))
            Split(*e, e->top);

          //if (IsHotEdge(horz) != IsHotEdge(*e))
          //    DoError(undefined_error_i);

          if (IsHotEdge(horz))
          {
            while (horz.vertex_top != vertex_max)
            {
              AddOutPt(horz, horz.top);
              UpdateEdgeIntoAEL(&horz);
            }
            if (is_left_to_right)
              AddLocalMaxPoly(horz, *e, horz.top);
            else
              AddLocalMaxPoly(*e, horz, horz.top);
          }
          DeleteFromAEL(*e);
          DeleteFromAEL(horz);
          return;
        }

        //if horzEdge is a maxima, keep going until we reach
        //its maxima pair, otherwise check for break conditions
        if (vertex_max != horz.vertex_top || IsOpenEnd(horz))
        {
          //otherwise stop when 'ae' is beyond the end of the horizontal line
          if ((is_left_to_right && e->curr_x > horz_right) ||
            (!is_left_to_right && e->curr_x < horz_left)) break;

          if (e->curr_x == horz.top.x && !IsHorizontal(*e))
          {
            pt = NextVertex(horz)->pt;
            if (is_left_to_right)
            {
              //with open paths we'll only break once past horz's end
              if (IsOpen(*e) && !IsSamePolyType(*e, horz) && !IsHotEdge(*e))
              {
                if (TopX(*e, pt.y) > pt.x) break;
              }
              //otherwise we'll only break when horz's outslope is greater than e's
              else if (TopX(*e, pt.y) >= pt.x) break;
            }
            else
            {
              if (IsOpen(*e) && !IsSamePolyType(*e, horz) && !IsHotEdge(*e))
              {
                if (TopX(*e, pt.y) < pt.x) break;
              }
              else if (TopX(*e, pt.y) <= pt.x) break;
            }
          }
        }

        pt = Point64(e->curr_x, horz.bot.y);
        if (is_left_to_right)
        {
          IntersectEdges(horz, *e, pt);
          SwapPositionsInAEL(horz, *e);
          CheckJoinLeft(*e, pt);
          horz.curr_x = e->curr_x;
          e = horz.next_in_ael;
        }
        else
        {
          IntersectEdges(*e, horz, pt);
          SwapPositionsInAEL(*e, horz);
          CheckJoinRight(*e, pt);
          horz.curr_x = e->curr_x;
          e = horz.prev_in_ael;
        }

        if (horz.outrec)
        {
          //nb: The outrec containining the op returned by IntersectEdges
          //above may no longer be associated with horzEdge.
          AddTrialHorzJoin(GetLastOp(horz));
        }
      }

      //check if we've finished with (consecutive) horizontals ...
      if (horzIsOpen && IsOpenEnd(horz)) // ie open at top
      {
        if (IsHotEdge(horz))
        {
          AddOutPt(horz, horz.top);
          if (IsFront(horz))
            horz.outrec->front_edge = nullptr;
          else
            horz.outrec->back_edge = nullptr;
          horz.outrec = nullptr;
        }
        DeleteFromAEL(horz);
        return;
      }
      else if (NextVertex(horz)->pt.y != horz.top.y)
        break;

      //still more horizontals in bound to process ...
      if (IsHotEdge(horz))
        AddOutPt(horz, horz.top);
      UpdateEdgeIntoAEL(&horz);

      is_left_to_right =
        ResetHorzDirection(horz, vertex_max, horz_left, horz_right);
    }

    if (IsHotEdge(horz))
    {
      OutPt* op = AddOutPt(horz, horz.top);
      AddTrialHorzJoin(op);
    }

    UpdateEdgeIntoAEL(&horz); // end of an intermediate horiz.
  }

  void ClipperBase::DoTopOfScanbeam(const int64_t y)
  {
    sel_ = nullptr;  // sel_ is reused to flag horizontals (see PushHorz below)
    Active* e = actives_;
    while (e)
    {
      //nb: 'e' will never be horizontal here
      if (e->top.y == y)
      {
        e->curr_x = e->top.x;
        if (IsMaxima(*e))
        {
          e = DoMaxima(*e);  // TOP OF BOUND (MAXIMA)
          continue;
        }
        else
        {
          //INTERMEDIATE VERTEX ...
          if (IsHotEdge(*e)) AddOutPt(*e, e->top);
          UpdateEdgeIntoAEL(e);
          if (IsHorizontal(*e))
            PushHorz(*e);  // horizontals are processed later
        }
      }
      else // i.e. not the top of the edge
        e->curr_x = TopX(*e, y);

      e = e->next_in_ael;
    }
  }


  Active* ClipperBase::DoMaxima(Active& e)
  {
    Active* next_e, * prev_e, * max_pair;
    prev_e = e.prev_in_ael;
    next_e = e.next_in_ael;
    if (IsOpenEnd(e))
    {
      if (IsHotEdge(e)) AddOutPt(e, e.top);
      if (!IsHorizontal(e))
      {
        if (IsHotEdge(e))
        {
          if (IsFront(e))
            e.outrec->front_edge = nullptr;
          else
            e.outrec->back_edge = nullptr;
          e.outrec = nullptr;
        }
        DeleteFromAEL(e);
      }
      return next_e;
    }

    max_pair = GetMaximaPair(e);
    if (!max_pair) return next_e;  // eMaxPair is horizontal

    if (IsJoined(e)) Split(e, e.top);
    if (IsJoined(*max_pair)) Split(*max_pair, max_pair->top);

    //only non-horizontal maxima here.
    //process any edges between maxima pair ...
    while (next_e != max_pair)
    {
      IntersectEdges(e, *next_e, e.top);
      SwapPositionsInAEL(e, *next_e);
      next_e = e.next_in_ael;
    }

    if (IsOpen(e))
    {
      if (IsHotEdge(e))
        AddLocalMaxPoly(e, *max_pair, e.top);
      DeleteFromAEL(*max_pair);
      DeleteFromAEL(e);
      return (prev_e ? prev_e->next_in_ael : actives_);
    }

    // e.next_in_ael== max_pair ...
    if (IsHotEdge(e))
      AddLocalMaxPoly(e, *max_pair, e.top);

    DeleteFromAEL(e);
    DeleteFromAEL(*max_pair);
    return (prev_e ? prev_e->next_in_ael : actives_);
  }

  void ClipperBase::Split(Active& e, const Point64& pt)
  {
    if (e.join_with == JoinWith::Right)
    {
      e.join_with = JoinWith::None;
      e.next_in_ael->join_with = JoinWith::None;
      AddLocalMinPoly(e, *e.next_in_ael, pt, true);
    }
    else
    {
      e.join_with = JoinWith::None;
      e.prev_in_ael->join_with = JoinWith::None;
      AddLocalMinPoly(*e.prev_in_ael, e, pt, true);
    }
  }

  void ClipperBase::CheckJoinLeft(Active& e,
    const Point64& pt, bool check_curr_x)
  {
    Active* prev = e.prev_in_ael;
    if (!prev ||
      !IsHotEdge(e) || !IsHotEdge(*prev) ||
      IsHorizontal(e) || IsHorizontal(*prev) ||
      IsOpen(e) || IsOpen(*prev) ) return;
    if ((pt.y < e.top.y + 2 || pt.y < prev->top.y + 2) &&
      ((e.bot.y > pt.y) || (prev->bot.y > pt.y))) return; // avoid trivial joins

    if (check_curr_x)
    {
      if (PerpendicDistFromLineSqrd(pt, prev->bot, prev->top) > 0.25) return;
    }
    else if (e.curr_x != prev->curr_x) return;
    if (!IsCollinear(e.top, pt, prev->top)) return;

    if (e.outrec->idx == prev->outrec->idx)
      AddLocalMaxPoly(*prev, e, pt);
    else if (e.outrec->idx < prev->outrec->idx)
      JoinOutrecPaths(e, *prev);
    else
      JoinOutrecPaths(*prev, e);
    prev->join_with = JoinWith::Right;
    e.join_with = JoinWith::Left;
  }

  void ClipperBase::CheckJoinRight(Active& e,
    const Point64& pt, bool check_curr_x)
  {
    Active* next = e.next_in_ael;
    if (!next ||
      !IsHotEdge(e) || !IsHotEdge(*next) ||
      IsHorizontal(e) || IsHorizontal(*next) ||
      IsOpen(e) || IsOpen(*next)) return;
    if ((pt.y < e.top.y +2 || pt.y < next->top.y +2) &&
      ((e.bot.y > pt.y) || (next->bot.y > pt.y))) return; // avoid trivial joins

    if (check_curr_x)
    {
      if (PerpendicDistFromLineSqrd(pt, next->bot, next->top) > 0.35) return;
    }
    else if (e.curr_x != next->curr_x) return;
    if (!IsCollinear(e.top, pt, next->top)) return;

    if (e.outrec->idx == next->outrec->idx)
      AddLocalMaxPoly(e, *next, pt);
    else if (e.outrec->idx < next->outrec->idx)
      JoinOutrecPaths(e, *next);
    else
      JoinOutrecPaths(*next, e);

    e.join_with = JoinWith::Right;
    next->join_with = JoinWith::Left;
  }

  inline bool GetHorzExtendedHorzSeg(OutPt*& op, OutPt*& op2)
  {
    OutRec* outrec = GetRealOutRec(op->outrec);
    op2 = op;
    if (outrec->front_edge)
    {
      while (op->prev != outrec->pts &&
        op->prev->pt.y == op->pt.y) op = op->prev;
      while (op2 != outrec->pts &&
        op2->next->pt.y == op2->pt.y) op2 = op2->next;
      return op2 != op;
    }
    else
    {
      while (op->prev != op2 && op->prev->pt.y == op->pt.y)
        op = op->prev;
      while (op2->next != op && op2->next->pt.y == op2->pt.y)
        op2 = op2->next;
      return op2 != op && op2->next != op;
    }
  }

  bool BuildPath64(OutPt* op, bool reverse, bool isOpen, Path64& path)
  {
    if (!op || op->next == op || (!isOpen && op->next == op->prev))
      return false;

    path.resize(0);
    Point64 lastPt;
    OutPt* op2;
    if (reverse)
    {
      lastPt = op->pt;
      op2 = op->prev;
    }
    else
    {
      op = op->next;
      lastPt = op->pt;
      op2 = op->next;
    }
    path.push_back(lastPt);

    while (op2 != op)
    {
      if (op2->pt != lastPt)
      {
        lastPt = op2->pt;
        path.push_back(lastPt);
      }
      if (reverse)
        op2 = op2->prev;
      else
        op2 = op2->next;
    }

    if (!isOpen && path.size() == 3 && IsVerySmallTriangle(*op2)) return false;
    else return true;
  }

  bool ClipperBase::CheckBounds(OutRec* outrec)
  {
    if (!outrec->pts) return false;
    if (!outrec->bounds.IsEmpty()) return true;
    CleanCollinear(outrec);
    if (!outrec->pts ||
      !BuildPath64(outrec->pts, reverse_solution_, false, outrec->path)){
        return false;}
    outrec->bounds = GetBounds(outrec->path);
    return true;
  }

  bool ClipperBase::CheckSplitOwner(OutRec* outrec, OutRecList* splits)
  {
    for (auto split : *splits)
    {
      split = GetRealOutRec(split);
      if(!split || split == outrec || split->recursive_split == outrec) continue;
      split->recursive_split = outrec; // prevent infinite loops

      if (split->splits && CheckSplitOwner(outrec, split->splits))
          return true;
      else if (CheckBounds(split) &&
        IsValidOwner(outrec, split) &&
        split->bounds.Contains(outrec->bounds) &&
        Path1InsidePath2(outrec->pts, split->pts))
      {
        outrec->owner = split; //found in split
        return true;
      }
    }
    return false;
  }

  void ClipperBase::RecursiveCheckOwners(OutRec* outrec, PolyPath* polypath)
  {
    // pre-condition: outrec will have valid bounds
    // post-condition: if a valid path, outrec will have a polypath

    if (outrec->polypath || outrec->bounds.IsEmpty()) return;

    while (outrec->owner)
    {
      if (outrec->owner->splits && CheckSplitOwner(outrec, outrec->owner->splits)) break;
      if (outrec->owner->pts && CheckBounds(outrec->owner) &&
        outrec->owner->bounds.Contains(outrec->bounds) &&
        Path1InsidePath2(outrec->pts, outrec->owner->pts)) break;
      outrec->owner = outrec->owner->owner;
    }

    if (outrec->owner)
    {
      if (!outrec->owner->polypath)
        RecursiveCheckOwners(outrec->owner, polypath);
      outrec->polypath = outrec->owner->polypath->AddChild(outrec->path);
    }
    else
      outrec->polypath = polypath->AddChild(outrec->path);
  }

  void Clipper64::BuildPaths64(Paths64& solutionClosed, Paths64* solutionOpen)
  {
    solutionClosed.resize(0);
    solutionClosed.reserve(outrec_list_.size());
    if (solutionOpen)
    {
      solutionOpen->resize(0);
      solutionOpen->reserve(outrec_list_.size());
    }

    // nb: outrec_list_.size() may change in the following
    // while loop because polygons may be split during
    // calls to CleanCollinear which calls FixSelfIntersects
    for (size_t i = 0; i < outrec_list_.size(); ++i)
    {
      OutRec* outrec = outrec_list_[i];
      if (outrec->pts == nullptr) continue;

      Path64 path;
      if (solutionOpen && outrec->is_open)
      {
        if (BuildPath64(outrec->pts, reverse_solution_, true, path))
          solutionOpen->emplace_back(std::move(path));
      }
      else
      {
        // nb: CleanCollinear can add to outrec_list_
        CleanCollinear(outrec);
        //closed paths should always return a Positive orientation
        if (BuildPath64(outrec->pts, reverse_solution_, false, path))
          solutionClosed.emplace_back(std::move(path));
      }
    }
  }

  void Clipper64::BuildTree64(PolyPath64& polytree, Paths64& open_paths)
  {
    polytree.Clear();
    open_paths.resize(0);
    if (has_open_paths_)
      open_paths.reserve(outrec_list_.size());

    // outrec_list_.size() is not static here because
    // CheckBounds below can indirectly add additional
    // OutRec (via FixOutRecPts & CleanCollinear)
    for (size_t i = 0; i < outrec_list_.size(); ++i)
    {
      OutRec* outrec = outrec_list_[i];
      if (!outrec || !outrec->pts) continue;
      if (outrec->is_open)
      {
        Path64 path;
        if (BuildPath64(outrec->pts, reverse_solution_, true, path))
          open_paths.push_back(path);
        continue;
      }

      if (CheckBounds(outrec))
        RecursiveCheckOwners(outrec, &polytree);
    }
  }

  bool BuildPathD(OutPt* op, bool reverse, bool isOpen, PathD& path, double inv_scale)
  {
    if (!op || op->next == op || (!isOpen && op->next == op->prev))
      return false;

    path.resize(0);
    Point64 lastPt;
    OutPt* op2;
    if (reverse)
    {
      lastPt = op->pt;
      op2 = op->prev;
    }
    else
    {
      op = op->next;
      lastPt = op->pt;
      op2 = op->next;
    }
#ifdef USINGZ
    path.push_back(PointD(lastPt.x * inv_scale, lastPt.y * inv_scale, lastPt.z));
#else
    path.push_back(PointD(lastPt.x * inv_scale, lastPt.y * inv_scale));
#endif

    while (op2 != op)
    {
      if (op2->pt != lastPt)
      {
        lastPt = op2->pt;
#ifdef USINGZ
        path.push_back(PointD(lastPt.x * inv_scale, lastPt.y * inv_scale, lastPt.z));
#else
        path.push_back(PointD(lastPt.x * inv_scale, lastPt.y * inv_scale));
#endif

      }
      if (reverse)
        op2 = op2->prev;
      else
        op2 = op2->next;
    }
    if (path.size() == 3 && IsVerySmallTriangle(*op2)) return false;
    return true;
  }

  void ClipperD::BuildPathsD(PathsD& solutionClosed, PathsD* solutionOpen)
  {
    solutionClosed.resize(0);
    solutionClosed.reserve(outrec_list_.size());
    if (solutionOpen)
    {
      solutionOpen->resize(0);
      solutionOpen->reserve(outrec_list_.size());
    }

    // outrec_list_.size() is not static here because
    // CleanCollinear below can indirectly add additional
    // OutRec (via FixOutRecPts)
    for (std::size_t i = 0; i < outrec_list_.size(); ++i)
    {
      OutRec* outrec = outrec_list_[i];
      if (outrec->pts == nullptr) continue;

      PathD path;
      if (solutionOpen && outrec->is_open)
      {
        if (BuildPathD(outrec->pts, reverse_solution_, true, path, invScale_))
          solutionOpen->emplace_back(std::move(path));
      }
      else
      {
        CleanCollinear(outrec);
        //closed paths should always return a Positive orientation
        if (BuildPathD(outrec->pts, reverse_solution_, false, path, invScale_))
          solutionClosed.emplace_back(std::move(path));
      }
    }
  }

  void ClipperD::BuildTreeD(PolyPathD& polytree, PathsD& open_paths)
  {
    polytree.Clear();
    open_paths.resize(0);
    if (has_open_paths_)
      open_paths.reserve(outrec_list_.size());

    // outrec_list_.size() is not static here because
    // BuildPathD below can indirectly add additional OutRec //#607
    for (size_t i = 0; i < outrec_list_.size(); ++i)
    {
      OutRec* outrec = outrec_list_[i];
      if (!outrec || !outrec->pts) continue;
      if (outrec->is_open)
      {
        PathD path;
        if (BuildPathD(outrec->pts, reverse_solution_, true, path, invScale_))
          open_paths.push_back(path);
        continue;
      }

      if (CheckBounds(outrec))
        RecursiveCheckOwners(outrec, &polytree);
    }
  }

}  // namespace clipper2lib
