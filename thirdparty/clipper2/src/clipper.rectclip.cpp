/*******************************************************************************
* Author    :  Angus Johnson                                                   *
* Date      :  5 July 2024                                                     *
* Website   :  http://www.angusj.com                                           *
* Copyright :  Angus Johnson 2010-2024                                         *
* Purpose   :  FAST rectangular clipping                                       *
* License   :  http://www.boost.org/LICENSE_1_0.txt                            *
*******************************************************************************/

#include <cmath>
#include "clipper2/clipper.h"
#include "clipper2/clipper.rectclip.h"

namespace Clipper2Lib {

  //------------------------------------------------------------------------------
  // Miscellaneous methods
  //------------------------------------------------------------------------------

  inline bool Path1ContainsPath2(const Path64& path1, const Path64& path2)
  {
    int io_count = 0;
    // precondition: no (significant) overlap
    for (const Point64& pt : path2)
    {
      PointInPolygonResult pip = PointInPolygon(pt, path1);
      switch (pip)
      {
      case PointInPolygonResult::IsOutside: ++io_count; break;
      case PointInPolygonResult::IsInside: --io_count; break;
      default: continue;
      }
      if (std::abs(io_count) > 1) break;
    }
    return io_count <= 0;
  }

  inline bool GetLocation(const Rect64& rec,
    const Point64& pt, Location& loc)
  {
    if (pt.x == rec.left && pt.y >= rec.top && pt.y <= rec.bottom)
    {
      loc = Location::Left;
      return false;
    }
    else if (pt.x == rec.right && pt.y >= rec.top && pt.y <= rec.bottom)
    {
      loc = Location::Right;
      return false;
    }
    else if (pt.y == rec.top && pt.x >= rec.left && pt.x <= rec.right)
    {
      loc = Location::Top;
      return false;
    }
    else if (pt.y == rec.bottom && pt.x >= rec.left && pt.x <= rec.right)
    {
      loc = Location::Bottom;
      return false;
    }
    else if (pt.x < rec.left) loc = Location::Left;
    else if (pt.x > rec.right) loc = Location::Right;
    else if (pt.y < rec.top) loc = Location::Top;
    else if (pt.y > rec.bottom) loc = Location::Bottom;
    else loc = Location::Inside;
    return true;
  }

  inline bool IsHorizontal(const Point64& pt1, const Point64& pt2)
  {
    return pt1.y == pt2.y;
  }

  bool GetSegmentIntersection(const Point64& p1,
    const Point64& p2, const Point64& p3, const Point64& p4, Point64& ip)
  {
    double res1 = CrossProduct(p1, p3, p4);
    double res2 = CrossProduct(p2, p3, p4);
    if (res1 == 0)
    {
      ip = p1;
      if (res2 == 0) return false; // segments are collinear
      else if (p1 == p3 || p1 == p4) return true;
      //else if (p2 == p3 || p2 == p4) { ip = p2; return true; }
      else if (IsHorizontal(p3, p4)) return ((p1.x > p3.x) == (p1.x < p4.x));
      else return ((p1.y > p3.y) == (p1.y < p4.y));
    }
    else if (res2 == 0)
    {
      ip = p2;
      if (p2 == p3 || p2 == p4) return true;
      else if (IsHorizontal(p3, p4)) return ((p2.x > p3.x) == (p2.x < p4.x));
      else return ((p2.y > p3.y) == (p2.y < p4.y));
    }
    if ((res1 > 0) == (res2 > 0)) return false;

    double res3 = CrossProduct(p3, p1, p2);
    double res4 = CrossProduct(p4, p1, p2);
    if (res3 == 0)
    {
      ip = p3;
      if (p3 == p1 || p3 == p2) return true;
      else if (IsHorizontal(p1, p2)) return ((p3.x > p1.x) == (p3.x < p2.x));
      else return ((p3.y > p1.y) == (p3.y < p2.y));
    }
    else if (res4 == 0)
    {
      ip = p4;
      if (p4 == p1 || p4 == p2) return true;
      else if (IsHorizontal(p1, p2)) return ((p4.x > p1.x) == (p4.x < p2.x));
      else return ((p4.y > p1.y) == (p4.y < p2.y));
    }
    if ((res3 > 0) == (res4 > 0)) return false;

    // segments must intersect to get here
    return GetSegmentIntersectPt(p1, p2, p3, p4, ip);
  }

  inline bool GetIntersection(const Path64& rectPath,
    const Point64& p, const Point64& p2, Location& loc, Point64& ip)
  {
    // gets the intersection closest to 'p'
    // when Result = false, loc will remain unchanged
    switch (loc)
    {
    case Location::Left:
      if (GetSegmentIntersection(p, p2, rectPath[0], rectPath[3], ip)) return true;
      else if ((p.y < rectPath[0].y) && GetSegmentIntersection(p, p2, rectPath[0], rectPath[1], ip))
      {
        loc = Location::Top;
        return true;
      }
      else if (GetSegmentIntersection(p, p2, rectPath[2], rectPath[3], ip))
      {
        loc = Location::Bottom;
        return true;
      }
      else return false;

    case Location::Top:
      if (GetSegmentIntersection(p, p2, rectPath[0], rectPath[1], ip)) return true;
      else if ((p.x < rectPath[0].x) && GetSegmentIntersection(p, p2, rectPath[0], rectPath[3], ip))
      {
        loc = Location::Left;
        return true;
      }
      else if (GetSegmentIntersection(p, p2, rectPath[1], rectPath[2], ip))
      {
        loc = Location::Right;
        return true;
      }
      else return false;

    case Location::Right:
      if (GetSegmentIntersection(p, p2, rectPath[1], rectPath[2], ip)) return true;
      else if ((p.y < rectPath[1].y) && GetSegmentIntersection(p, p2, rectPath[0], rectPath[1], ip))
      {
        loc = Location::Top;
        return true;
      }
      else if (GetSegmentIntersection(p, p2, rectPath[2], rectPath[3], ip))
      {
        loc = Location::Bottom;
        return true;
      }
      else return false;

    case Location::Bottom:
      if (GetSegmentIntersection(p, p2, rectPath[2], rectPath[3], ip)) return true;
      else if ((p.x < rectPath[3].x) && GetSegmentIntersection(p, p2, rectPath[0], rectPath[3], ip))
      {
        loc = Location::Left;
        return true;
      }
      else if (GetSegmentIntersection(p, p2, rectPath[1], rectPath[2], ip))
      {
        loc = Location::Right;
        return true;
      }
      else return false;

    default: // loc == rInside
      if (GetSegmentIntersection(p, p2, rectPath[0], rectPath[3], ip))
      {
        loc = Location::Left;
        return true;
      }
      else if (GetSegmentIntersection(p, p2, rectPath[0], rectPath[1], ip))
      {
        loc = Location::Top;
        return true;
      }
      else if (GetSegmentIntersection(p, p2, rectPath[1], rectPath[2], ip))
      {
        loc = Location::Right;
        return true;
      }
      else if (GetSegmentIntersection(p, p2, rectPath[2], rectPath[3], ip))
      {
        loc = Location::Bottom;
        return true;
      }
      else return false;
    }
  }

  inline Location GetAdjacentLocation(Location loc, bool isClockwise)
  {
    int delta = (isClockwise) ? 1 : 3;
    return static_cast<Location>((static_cast<int>(loc) + delta) % 4);
  }

  inline bool HeadingClockwise(Location prev, Location curr)
  {
    return (static_cast<int>(prev) + 1) % 4 == static_cast<int>(curr);
  }

  inline bool AreOpposites(Location prev, Location curr)
  {
    return abs(static_cast<int>(prev) - static_cast<int>(curr)) == 2;
  }

  inline bool IsClockwise(Location prev, Location curr,
    const Point64& prev_pt, const Point64& curr_pt, const Point64& rect_mp)
  {
    if (AreOpposites(prev, curr))
      return CrossProduct(prev_pt, rect_mp, curr_pt) < 0;
    else
      return HeadingClockwise(prev, curr);
  }

  inline OutPt2* UnlinkOp(OutPt2* op)
  {
    if (op->next == op) return nullptr;
    op->prev->next = op->next;
    op->next->prev = op->prev;
    return op->next;
  }

  inline OutPt2* UnlinkOpBack(OutPt2* op)
  {
    if (op->next == op) return nullptr;
    op->prev->next = op->next;
    op->next->prev = op->prev;
    return op->prev;
  }

  inline uint32_t GetEdgesForPt(const Point64& pt, const Rect64& rec)
  {
    uint32_t result = 0;
    if (pt.x == rec.left) result = 1;
    else if (pt.x == rec.right) result = 4;
    if (pt.y == rec.top) result += 2;
    else if (pt.y == rec.bottom) result += 8;
    return result;
  }

  inline bool IsHeadingClockwise(const Point64& pt1, const Point64& pt2, int edgeIdx)
  {
    switch (edgeIdx)
    {
    case 0: return pt2.y < pt1.y;
    case 1: return pt2.x > pt1.x;
    case 2: return pt2.y > pt1.y;
    default: return pt2.x < pt1.x;
    }
  }

  inline bool HasHorzOverlap(const Point64& left1, const Point64& right1,
    const Point64& left2, const Point64& right2)
  {
    return (left1.x < right2.x) && (right1.x > left2.x);
  }

  inline bool HasVertOverlap(const Point64& top1, const Point64& bottom1,
    const Point64& top2, const Point64& bottom2)
  {
    return (top1.y < bottom2.y) && (bottom1.y > top2.y);
  }

  inline void AddToEdge(OutPt2List& edge, OutPt2* op)
  {
    if (op->edge) return;
    op->edge = &edge;
    edge.push_back(op);
  }

  inline void UncoupleEdge(OutPt2* op)
  {
    if (!op->edge) return;
    for (size_t i = 0; i < op->edge->size(); ++i)
    {
      OutPt2* op2 = (*op->edge)[i];
      if (op2 == op)
      {
        (*op->edge)[i] = nullptr;
        break;
      }
    }
    op->edge = nullptr;
  }

  inline void SetNewOwner(OutPt2* op, size_t new_idx)
  {
    op->owner_idx = new_idx;
    OutPt2* op2 = op->next;
    while (op2 != op)
    {
      op2->owner_idx = new_idx;
      op2 = op2->next;
    }
  }

  //----------------------------------------------------------------------------
  // RectClip64
  //----------------------------------------------------------------------------

  OutPt2* RectClip64::Add(Point64 pt, bool start_new)
  {
    // this method is only called by InternalExecute.
    // Later splitting & rejoining won't create additional op's,
    // though they will change the (non-storage) results_ count.
    size_t curr_idx = results_.size();
    OutPt2* result;
    if (curr_idx == 0 || start_new)
    {
      result = &op_container_.emplace_back(OutPt2());
      result->pt = pt;
      result->next = result;
      result->prev = result;
      results_.push_back(result);
    }
    else
    {
      --curr_idx;
      OutPt2* prevOp = results_[curr_idx];
      if (prevOp->pt == pt)  return prevOp;
      result = &op_container_.emplace_back(OutPt2());
      result->owner_idx = curr_idx;
      result->pt = pt;
      result->next = prevOp->next;
      prevOp->next->prev = result;
      prevOp->next = result;
      result->prev = prevOp;
      results_[curr_idx] = result;
    }
    return result;
  }

  void RectClip64::AddCorner(Location prev, Location curr)
  {
    if (HeadingClockwise(prev, curr))
      Add(rect_as_path_[static_cast<size_t>(prev)]);
    else
      Add(rect_as_path_[static_cast<size_t>(curr)]);
  }

  void RectClip64::AddCorner(Location& loc, bool isClockwise)
  {
    if (isClockwise)
    {
      Add(rect_as_path_[static_cast<size_t>(loc)]);
      loc = GetAdjacentLocation(loc, true);
    }
    else
    {
      loc = GetAdjacentLocation(loc, false);
      Add(rect_as_path_[static_cast<size_t>(loc)]);
    }
  }

  void RectClip64::GetNextLocation(const Path64& path,
    Location& loc, size_t& i, size_t highI)
  {
    switch (loc)
    {
    case Location::Left:
      while (i <= highI && path[i].x <= rect_.left) ++i;
      if (i > highI) break;
      else if (path[i].x >= rect_.right) loc = Location::Right;
      else if (path[i].y <= rect_.top) loc = Location::Top;
      else if (path[i].y >= rect_.bottom) loc = Location::Bottom;
      else loc = Location::Inside;
      break;

    case Location::Top:
      while (i <= highI && path[i].y <= rect_.top) ++i;
      if (i > highI) break;
      else if (path[i].y >= rect_.bottom) loc = Location::Bottom;
      else if (path[i].x <= rect_.left) loc = Location::Left;
      else if (path[i].x >= rect_.right) loc = Location::Right;
      else loc = Location::Inside;
      break;

    case Location::Right:
      while (i <= highI && path[i].x >= rect_.right) ++i;
      if (i > highI) break;
      else if (path[i].x <= rect_.left) loc = Location::Left;
      else if (path[i].y <= rect_.top) loc = Location::Top;
      else if (path[i].y >= rect_.bottom) loc = Location::Bottom;
      else loc = Location::Inside;
      break;

    case Location::Bottom:
      while (i <= highI && path[i].y >= rect_.bottom) ++i;
      if (i > highI) break;
      else if (path[i].y <= rect_.top) loc = Location::Top;
      else if (path[i].x <= rect_.left) loc = Location::Left;
      else if (path[i].x >= rect_.right) loc = Location::Right;
      else loc = Location::Inside;
      break;

    case Location::Inside:
      while (i <= highI)
      {
        if (path[i].x < rect_.left) loc = Location::Left;
        else if (path[i].x > rect_.right) loc = Location::Right;
        else if (path[i].y > rect_.bottom) loc = Location::Bottom;
        else if (path[i].y < rect_.top) loc = Location::Top;
        else { Add(path[i]); ++i; continue; }
        break; //inner loop
      }
      break;
    } //switch
  }

  bool StartLocsAreClockwise(const std::vector<Location>& startlocs)
  {
    int result = 0;
    for (size_t i = 1; i < startlocs.size(); ++i)
    {
      int d = static_cast<int>(startlocs[i]) - static_cast<int>(startlocs[i - 1]);
      switch (d)
      {
        case -1: result -= 1; break;
        case 1: result += 1; break;
        case -3: result += 1; break;
        case 3: result -= 1; break;
      }
    }
    return result > 0;
  }

  void RectClip64::ExecuteInternal(const Path64& path)
  {
    if (path.size() < 1)
      return;

    size_t highI = path.size() - 1;
    Location prev = Location::Inside, loc;
    Location crossing_loc = Location::Inside;
    Location first_cross_ = Location::Inside;
    if (!GetLocation(rect_, path[highI], loc))
    {
      size_t i = highI;
      while (i > 0 && !GetLocation(rect_, path[i - 1], prev))
        --i;
      if (i == 0)
      {
        // all of path must be inside fRect
        for (const auto& pt : path) Add(pt);
        return;
      }
      if (prev == Location::Inside) loc = Location::Inside;
    }
    Location starting_loc = loc;

    ///////////////////////////////////////////////////
    size_t i = 0;
    while (i <= highI)
    {
      prev = loc;
      Location crossing_prev = crossing_loc;

      GetNextLocation(path, loc, i, highI);

      if (i > highI) break;
      Point64 ip, ip2;
      Point64 prev_pt = (i) ?
        path[static_cast<size_t>(i - 1)] :
        path[highI];

      crossing_loc = loc;
      if (!GetIntersection(rect_as_path_,
        path[i], prev_pt, crossing_loc, ip))
      {
        // ie remaining outside
        if (crossing_prev == Location::Inside)
        {
          bool isClockw = IsClockwise(prev, loc, prev_pt, path[i], rect_mp_);
          do {
            start_locs_.push_back(prev);
            prev = GetAdjacentLocation(prev, isClockw);
          } while (prev != loc);
          crossing_loc = crossing_prev; // still not crossed
        }
        else if (prev != Location::Inside && prev != loc)
        {
          bool isClockw = IsClockwise(prev, loc, prev_pt, path[i], rect_mp_);
          do {
            AddCorner(prev, isClockw);
          } while (prev != loc);
        }
        ++i;
        continue;
      }

      ////////////////////////////////////////////////////
      // we must be crossing the rect boundary to get here
      ////////////////////////////////////////////////////

      if (loc == Location::Inside) // path must be entering rect
      {
        if (first_cross_ == Location::Inside)
        {
          first_cross_ = crossing_loc;
          start_locs_.push_back(prev);
        }
        else if (prev != crossing_loc)
        {
          bool isClockw = IsClockwise(prev, crossing_loc, prev_pt, path[i], rect_mp_);
          do {
            AddCorner(prev, isClockw);
          } while (prev != crossing_loc);
        }
      }
      else if (prev != Location::Inside)
      {
        // passing right through rect. 'ip' here will be the second
        // intersect pt but we'll also need the first intersect pt (ip2)
        loc = prev;
        GetIntersection(rect_as_path_, prev_pt, path[i], loc, ip2);
        if (crossing_prev != Location::Inside && crossing_prev != loc) //579
          AddCorner(crossing_prev, loc);

        if (first_cross_ == Location::Inside)
        {
          first_cross_ = loc;
          start_locs_.push_back(prev);
        }

        loc = crossing_loc;
        Add(ip2);
        if (ip == ip2)
        {
          // it's very likely that path[i] is on rect
          GetLocation(rect_, path[i], loc);
          AddCorner(crossing_loc, loc);
          crossing_loc = loc;
          continue;
        }
      }
      else // path must be exiting rect
      {
        loc = crossing_loc;
        if (first_cross_ == Location::Inside)
          first_cross_ = crossing_loc;
      }

      Add(ip);

    } //while i <= highI
    ///////////////////////////////////////////////////

    if (first_cross_ == Location::Inside)
    {
      // path never intersects
      if (starting_loc != Location::Inside)
      {
        // path is outside rect
        // but being outside, it still may not contain rect
        if (path_bounds_.Contains(rect_) &&
          Path1ContainsPath2(path, rect_as_path_))
        {
          // yep, the path does fully contain rect
          // so add rect to the solution
          bool is_clockwise_path = StartLocsAreClockwise(start_locs_);
          for (size_t j = 0; j < 4; ++j)
          {
            size_t k = is_clockwise_path ? j : 3 - j; // reverses result path
            Add(rect_as_path_[k]);
            // we may well need to do some splitting later, so
            AddToEdge(edges_[k * 2], results_[0]);
          }
        }
      }
    }
    else if (loc != Location::Inside &&
      (loc != first_cross_ || start_locs_.size() > 2))
    {
      if (start_locs_.size() > 0)
      {
        prev = loc;
        for (auto loc2 : start_locs_)
        {
          if (prev == loc2) continue;
          AddCorner(prev, HeadingClockwise(prev, loc2));
          prev = loc2;
        }
        loc = prev;
      }
      if (loc != first_cross_)
        AddCorner(loc, HeadingClockwise(loc, first_cross_));
    }
  }

  void RectClip64::CheckEdges()
  {
    for (size_t i = 0; i < results_.size(); ++i)
    {
      OutPt2* op = results_[i];
      if (!op) continue;
      OutPt2* op2 = op;
      do
      {
        if (IsCollinear(op2->prev->pt, op2->pt, op2->next->pt))
        {
          if (op2 == op)
          {
            op2 = UnlinkOpBack(op2);
            if (!op2) break;
            op = op2->prev;
          }
          else
          {
            op2 = UnlinkOpBack(op2);
            if (!op2) break;
          }
        }
        else
          op2 = op2->next;
      } while (op2 != op);

      if (!op2)
      {
        results_[i] = nullptr;
        continue;
      }
      results_[i] = op; // safety first

      uint32_t edgeSet1 = GetEdgesForPt(op->prev->pt, rect_);
      op2 = op;
      do
      {
        uint32_t edgeSet2 = GetEdgesForPt(op2->pt, rect_);
        if (edgeSet2 && !op2->edge)
        {
          uint32_t combinedSet = (edgeSet1 & edgeSet2);
          for (int j = 0; j < 4; ++j)
          {
            if (combinedSet & (1 << j))
            {
              if (IsHeadingClockwise(op2->prev->pt, op2->pt, j))
                AddToEdge(edges_[j * 2], op2);
              else
                AddToEdge(edges_[j * 2 + 1], op2);
            }
          }
        }
        edgeSet1 = edgeSet2;
        op2 = op2->next;
      } while (op2 != op);
    }
  }

  void RectClip64::TidyEdges(size_t idx, OutPt2List& cw, OutPt2List& ccw)
  {
    if (ccw.empty()) return;
    bool isHorz = ((idx == 1) || (idx == 3));
    bool cwIsTowardLarger = ((idx == 1) || (idx == 2));
    size_t i = 0, j = 0;
    OutPt2* p1, * p2, * p1a, * p2a, * op, * op2;

    while (i < cw.size())
    {
      p1 = cw[i];
      if (!p1 || p1->next == p1->prev)
      {
        cw[i++] = nullptr;
        j = 0;
        continue;
      }

      size_t jLim = ccw.size();
      while (j < jLim &&
        (!ccw[j] || ccw[j]->next == ccw[j]->prev)) ++j;

      if (j == jLim)
      {
        ++i;
        j = 0;
        continue;
      }

      if (cwIsTowardLarger)
      {
        // p1 >>>> p1a;
        // p2 <<<< p2a;
        p1 = cw[i]->prev;
        p1a = cw[i];
        p2 = ccw[j];
        p2a = ccw[j]->prev;
      }
      else
      {
        // p1 <<<< p1a;
        // p2 >>>> p2a;
        p1 = cw[i];
        p1a = cw[i]->prev;
        p2 = ccw[j]->prev;
        p2a = ccw[j];
      }

      if ((isHorz && !HasHorzOverlap(p1->pt, p1a->pt, p2->pt, p2a->pt)) ||
        (!isHorz && !HasVertOverlap(p1->pt, p1a->pt, p2->pt, p2a->pt)))
      {
        ++j;
        continue;
      }

      // to get here we're either splitting or rejoining
      bool isRejoining = cw[i]->owner_idx != ccw[j]->owner_idx;

      if (isRejoining)
      {
        results_[p2->owner_idx] = nullptr;
        SetNewOwner(p2, p1->owner_idx);
      }

      // do the split or re-join
      if (cwIsTowardLarger)
      {
        // p1 >> | >> p1a;
        // p2 << | << p2a;
        p1->next = p2;
        p2->prev = p1;
        p1a->prev = p2a;
        p2a->next = p1a;
      }
      else
      {
        // p1 << | << p1a;
        // p2 >> | >> p2a;
        p1->prev = p2;
        p2->next = p1;
        p1a->next = p2a;
        p2a->prev = p1a;
      }

      if (!isRejoining)
      {
        size_t new_idx = results_.size();
        results_.push_back(p1a);
        SetNewOwner(p1a, new_idx);
      }

      if (cwIsTowardLarger)
      {
        op = p2;
        op2 = p1a;
      }
      else
      {
        op = p1;
        op2 = p2a;
      }
      results_[op->owner_idx] = op;
      results_[op2->owner_idx] = op2;

      // and now lots of work to get ready for the next loop

      bool opIsLarger, op2IsLarger;
      if (isHorz) // X
      {
        opIsLarger = op->pt.x > op->prev->pt.x;
        op2IsLarger = op2->pt.x > op2->prev->pt.x;
      }
      else       // Y
      {
        opIsLarger = op->pt.y > op->prev->pt.y;
        op2IsLarger = op2->pt.y > op2->prev->pt.y;
      }

      if ((op->next == op->prev) ||
        (op->pt == op->prev->pt))
      {
        if (op2IsLarger == cwIsTowardLarger)
        {
          cw[i] = op2;
          ccw[j++] = nullptr;
        }
        else
        {
          ccw[j] = op2;
          cw[i++] = nullptr;
        }
      }
      else if ((op2->next == op2->prev) ||
        (op2->pt == op2->prev->pt))
      {
        if (opIsLarger == cwIsTowardLarger)
        {
          cw[i] = op;
          ccw[j++] = nullptr;
        }
        else
        {
          ccw[j] = op;
          cw[i++] = nullptr;
        }
      }
      else if (opIsLarger == op2IsLarger)
      {
        if (opIsLarger == cwIsTowardLarger)
        {
          cw[i] = op;
          UncoupleEdge(op2);
          AddToEdge(cw, op2);
          ccw[j++] = nullptr;
        }
        else
        {
          cw[i++] = nullptr;
          ccw[j] = op2;
          UncoupleEdge(op);
          AddToEdge(ccw, op);
          j = 0;
        }
      }
      else
      {
        if (opIsLarger == cwIsTowardLarger)
          cw[i] = op;
        else
          ccw[j] = op;
        if (op2IsLarger == cwIsTowardLarger)
          cw[i] = op2;
        else
          ccw[j] = op2;
      }
    }
  }

  Path64 RectClip64::GetPath(OutPt2*& op)
  {
    if (!op || op->next == op->prev) return Path64();

    OutPt2* op2 = op->next;
    while (op2 && op2 != op)
    {
      if (IsCollinear(op2->prev->pt,
        op2->pt, op2->next->pt))
      {
        op = op2->prev;
        op2 = UnlinkOp(op2);
      }
      else
        op2 = op2->next;
    }
    op = op2; // needed for op cleanup
    if (!op2) return Path64();

    Path64 result;
    result.push_back(op->pt);
    op2 = op->next;
    while (op2 != op)
    {
      result.push_back(op2->pt);
      op2 = op2->next;
    }
    return result;
  }

  Paths64 RectClip64::Execute(const Paths64& paths)
  {
    Paths64 result;
    if (rect_.IsEmpty()) return result;

    for (const Path64& path : paths)
    {
      if (path.size() < 3) continue;
      path_bounds_ = GetBounds(path);
      if (!rect_.Intersects(path_bounds_))
        continue; // the path must be completely outside rect_
      else if (rect_.Contains(path_bounds_))
      {
        // the path must be completely inside rect_
        result.push_back(path);
        continue;
      }

      ExecuteInternal(path);
      CheckEdges();
      for (size_t i = 0; i < 4; ++i)
        TidyEdges(i, edges_[i * 2], edges_[i * 2 + 1]);

      for (OutPt2*& op :  results_)
      {
        Path64 tmp = GetPath(op);
        if (!tmp.empty())
          result.emplace_back(tmp);
      }

      //clean up after every loop
      op_container_ = std::deque<OutPt2>();
      results_.clear();
      for (OutPt2List &edge : edges_) edge.clear();
      start_locs_.clear();
    }
    return result;
  }

  //------------------------------------------------------------------------------
  // RectClipLines64
  //------------------------------------------------------------------------------

  Paths64 RectClipLines64::Execute(const Paths64& paths)
  {
    Paths64 result;
    if (rect_.IsEmpty()) return result;

    for (const auto& path : paths)
    {
      Rect64 pathrec = GetBounds(path);
      if (!rect_.Intersects(pathrec)) continue;

      ExecuteInternal(path);

      for (OutPt2*& op : results_)
      {
        Path64 tmp = GetPath(op);
        if (!tmp.empty())
          result.emplace_back(tmp);
      }
      results_.clear();

      op_container_ = std::deque<OutPt2>();
      start_locs_.clear();
    }
    return result;
  }

  void RectClipLines64::ExecuteInternal(const Path64& path)
  {
    if (rect_.IsEmpty() || path.size() < 2) return;

    results_.clear();
    op_container_ = std::deque<OutPt2>();
    start_locs_.clear();

    size_t i = 1, highI = path.size() - 1;

    Location prev = Location::Inside, loc;
    Location crossing_loc;
    if (!GetLocation(rect_, path[0], loc))
    {
      while (i <= highI && !GetLocation(rect_, path[i], prev)) ++i;
      if (i > highI)
      {
        // all of path must be inside fRect
        for (const auto& pt : path) Add(pt);
        return;
      }
      if (prev == Location::Inside) loc = Location::Inside;
      i = 1;
    }
    if (loc == Location::Inside) Add(path[0]);

    ///////////////////////////////////////////////////
    while (i <= highI)
    {
      prev = loc;
      GetNextLocation(path, loc, i, highI);
      if (i > highI) break;
      Point64 ip, ip2;
      Point64 prev_pt = path[static_cast<size_t>(i - 1)];

      crossing_loc = loc;
      if (!GetIntersection(rect_as_path_,
        path[i], prev_pt, crossing_loc, ip))
      {
        // ie remaining outside
        ++i;
        continue;
      }

      ////////////////////////////////////////////////////
      // we must be crossing the rect boundary to get here
      ////////////////////////////////////////////////////

      if (loc == Location::Inside) // path must be entering rect
      {
        Add(ip, true);
      }
      else if (prev != Location::Inside)
      {
        // passing right through rect. 'ip' here will be the second
        // intersect pt but we'll also need the first intersect pt (ip2)
        crossing_loc = prev;
        GetIntersection(rect_as_path_,
          prev_pt, path[i], crossing_loc, ip2);
        Add(ip2, true);
        Add(ip);
      }
      else // path must be exiting rect
      {
        Add(ip);
      }
    } //while i <= highI
    ///////////////////////////////////////////////////
  }

  Path64 RectClipLines64::GetPath(OutPt2*& op)
  {
    Path64 result;
    if (!op || op == op->next) return result;
    op = op->next; // starting at path beginning
    result.push_back(op->pt);
    OutPt2 *op2 = op->next;
    while (op2 != op)
    {
      result.push_back(op2->pt);
      op2 = op2->next;
    }
    return result;
  }

} // namespace
