/*******************************************************************************
* Author    :  Angus Johnson                                                   *
* Date      :  1 November 2023                                                 *
* Website   :  http://www.angusj.com                                           *
* Copyright :  Angus Johnson 2010-2023                                         *
* Purpose   :  Minkowski Sum and Difference                                    *
* License   :  http://www.boost.org/LICENSE_1_0.txt                            *
*******************************************************************************/

#ifndef CLIPPER_MINKOWSKI_H
#define CLIPPER_MINKOWSKI_H

#include <cstdlib>
#include <vector>
#include <string>
#include "clipper2/clipper.core.h"

namespace Clipper2Lib
{

  namespace detail
  {
    inline Paths64 Minkowski(const Path64& pattern, const Path64& path, bool isSum, bool isClosed)
    {
      size_t delta = isClosed ? 0 : 1;
      size_t patLen = pattern.size(), pathLen = path.size();
      if (patLen == 0 || pathLen == 0) return Paths64();
      Paths64 tmp;
      tmp.reserve(pathLen);

      if (isSum)
      {
        for (const Point64& p : path)
        {
          Path64 path2(pattern.size());
          std::transform(pattern.cbegin(), pattern.cend(),
            path2.begin(), [p](const Point64& pt2) {return p + pt2; });
          tmp.push_back(path2);
        }
      }
      else
      {
        for (const Point64& p : path)
        {
          Path64 path2(pattern.size());
          std::transform(pattern.cbegin(), pattern.cend(),
            path2.begin(), [p](const Point64& pt2) {return p - pt2; });
          tmp.push_back(path2);
        }
      }

      Paths64 result;
      result.reserve((pathLen - delta) * patLen);
      size_t g = isClosed ? pathLen - 1 : 0;
      for (size_t h = patLen - 1, i = delta; i < pathLen; ++i)
      {
        for (size_t j = 0; j < patLen; j++)
        {
          Path64 quad;
          quad.reserve(4);
          {
            quad.push_back(tmp[g][h]);
            quad.push_back(tmp[i][h]);
            quad.push_back(tmp[i][j]);
            quad.push_back(tmp[g][j]);
          };
          if (!IsPositive(quad))
            std::reverse(quad.begin(), quad.end());
          result.push_back(quad);
          h = j;
        }
        g = i;
      }
      return result;
    }

    inline Paths64 Union(const Paths64& subjects, FillRule fillrule)
    {
      Paths64 result;
      Clipper64 clipper;
      clipper.AddSubject(subjects);
      clipper.Execute(ClipType::Union, fillrule, result);
      return result;
    }

  } // namespace internal

  inline Paths64 MinkowskiSum(const Path64& pattern, const Path64& path, bool isClosed)
  {
    return detail::Union(detail::Minkowski(pattern, path, true, isClosed), FillRule::NonZero);
  }

  inline PathsD MinkowskiSum(const PathD& pattern, const PathD& path, bool isClosed, int decimalPlaces = 2)
  {
    int error_code = 0;
    double scale = pow(10, decimalPlaces);
    Path64 pat64 = ScalePath<int64_t, double>(pattern, scale, error_code);
    Path64 path64 = ScalePath<int64_t, double>(path, scale, error_code);
    Paths64 tmp = detail::Union(detail::Minkowski(pat64, path64, true, isClosed), FillRule::NonZero);
    return ScalePaths<double, int64_t>(tmp, 1 / scale, error_code);
  }

  inline Paths64 MinkowskiDiff(const Path64& pattern, const Path64& path, bool isClosed)
  {
    return detail::Union(detail::Minkowski(pattern, path, false, isClosed), FillRule::NonZero);
  }

  inline PathsD MinkowskiDiff(const PathD& pattern, const PathD& path, bool isClosed, int decimalPlaces = 2)
  {
    int error_code = 0;
    double scale = pow(10, decimalPlaces);
    Path64 pat64 = ScalePath<int64_t, double>(pattern, scale, error_code);
    Path64 path64 = ScalePath<int64_t, double>(path, scale, error_code);
    Paths64 tmp = detail::Union(detail::Minkowski(pat64, path64, false, isClosed), FillRule::NonZero);
    return ScalePaths<double, int64_t>(tmp, 1 / scale, error_code);
  }

} // Clipper2Lib namespace

#endif  // CLIPPER_MINKOWSKI_H
