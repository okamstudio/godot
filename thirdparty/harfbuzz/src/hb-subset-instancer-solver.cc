/*
 * Copyright © 2023  Behdad Esfahbod
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
 */

#include "hb-subset-instancer-solver.hh"

/* This file is a straight port of the following:
 *
 * https://github.com/fonttools/fonttools/blob/f73220816264fc383b8a75f2146e8d69e455d398/Lib/fontTools/varLib/instancer/solver.py
 *
 * Where that file returns None for a triple, we return Triple{}.
 * This should be safe.
 */

constexpr static double EPSILON = 1.0 / (1 << 14);
constexpr static double MAX_F2DOT14 = double (0x7FFF) / (1 << 14);

static inline Triple _reverse_negate(const Triple &v)
{ return {-v.maximum, -v.middle, -v.minimum}; }


static inline double supportScalar (double coord, const Triple &tent)
{
  /* Copied from VarRegionAxis::evaluate() */
  double start = tent.minimum, peak = tent.middle, end = tent.maximum;

  if (unlikely (start > peak || peak > end))
    return 1.;
  if (unlikely (start < 0 && end > 0 && peak != 0))
    return 1.;

  if (peak == 0 || coord == peak)
    return 1.;

  if (coord <= start || end <= coord)
    return 0.;

  /* Interpolate */
  if (coord < peak)
    return (coord - start) / (peak - start);
  else
    return  (end - coord) / (end - peak);
}

static inline rebase_tent_result_t
_solve (Triple tent, Triple axisLimit, bool negative = false)
{
  double axisMin = axisLimit.minimum;
  double axisDef = axisLimit.middle;
  double axisMax = axisLimit.maximum;
  double lower = tent.minimum;
  double peak  = tent.middle;
  double upper = tent.maximum;

  // Mirror the problem such that axisDef <= peak
  if (axisDef > peak)
  {
    rebase_tent_result_t vec = _solve (_reverse_negate (tent),
			   _reverse_negate (axisLimit),
			   !negative);

    for (auto &p : vec)
      p = hb_pair (p.first, _reverse_negate (p.second));

    return vec;
  }
  // axisDef <= peak

  /* case 1: The whole deltaset falls outside the new limit; we can drop it
   *
   *                                          peak
   *  1.........................................o..........
   *                                           / \
   *                                          /   \
   *                                         /     \
   *                                        /       \
   *  0---|-----------|----------|-------- o         o----1
   *    axisMin     axisDef    axisMax   lower     upper
   */
  if (axisMax <= lower && axisMax < peak)
      return rebase_tent_result_t{};  // No overlap

  /* case 2: Only the peak and outermost bound fall outside the new limit;
   * we keep the deltaset, update peak and outermost bound and scale deltas
   * by the scalar value for the restricted axis at the new limit, and solve
   * recursively.
   *
   *                                  |peak
   *  1...............................|.o..........
   *                                  |/ \
   *                                  /   \
   *                                 /|    \
   *                                / |     \
   *  0--------------------------- o  |      o----1
   *                           lower  |      upper
   *                                  |
   *                                axisMax
   *
   * Convert to:
   *
   *  1............................................
   *                                  |
   *                                  o peak
   *                                 /|
   *                                /x|
   *  0--------------------------- o  o upper ----1
   *                           lower  |
   *                                  |
   *                                axisMax
   */
  if (axisMax < peak)
  {
    double mult = supportScalar (axisMax, tent);
    tent = Triple{lower, axisMax, axisMax};

    rebase_tent_result_t vec = _solve (tent, axisLimit);

    for (auto &p : vec)
      p = hb_pair (p.first * mult, p.second);

    return vec;
  }

  // lower <= axisDef <= peak <= axisMax

  double gain = supportScalar (axisDef, tent);
  rebase_tent_result_t out {hb_pair (gain, Triple{})};

  // First, the positive side

  // outGain is the scalar of axisMax at the tent.
  double outGain = supportScalar (axisMax, tent);

  /* Case 3a: Gain is more than outGain. The tent down-slope crosses
   * the axis into negative. We have to split it into multiples.
   *
   *                      | peak  |
   *  1...................|.o.....|..............
   *                      |/x\_   |
   *  gain................+....+_.|..............
   *                     /|    |y\|
   *  ................../.|....|..+_......outGain
   *                   /  |    |  | \
   *  0---|-----------o   |    |  |  o----------1
   *    axisMin    lower  |    |  |   upper
   *                      |    |  |
   *                axisDef    |  axisMax
   *                           |
   *                      crossing
   */
  if (gain >= outGain)
  {
    // Note that this is the branch taken if both gain and outGain are 0.

    // Crossing point on the axis.
    double crossing = peak + (1 - gain) * (upper - peak);

    Triple loc{hb_max (lower, axisDef), peak, crossing};
    double scalar = 1.0;

    // The part before the crossing point.
    out.push (hb_pair (scalar - gain, loc));

    /* The part after the crossing point may use one or two tents,
     * depending on whether upper is before axisMax or not, in one
     * case we need to keep it down to eternity.
     *
     * Case 3a1, similar to case 1neg; just one tent needed, as in
     * the drawing above.
     */
    if (upper >= axisMax)
    {
      Triple loc {crossing, axisMax, axisMax};
      double scalar = outGain;

      out.push (hb_pair (scalar - gain, loc));
    }

    /* Case 3a2: Similar to case 2neg; two tents needed, to keep
     * down to eternity.
     *
     *                      | peak             |
     *  1...................|.o................|...
     *                      |/ \_              |
     *  gain................+....+_............|...
     *                     /|    | \xxxxxxxxxxy|
     *                    / |    |  \_xxxxxyyyy|
     *                   /  |    |    \xxyyyyyy|
     *  0---|-----------o   |    |     o-------|--1
     *    axisMin    lower  |    |      upper  |
     *                      |    |             |
     *                axisDef    |             axisMax
     *                           |
     *                      crossing
     */
    else
    {
      // A tent's peak cannot fall on axis default. Nudge it.
      if (upper == axisDef)
	upper += EPSILON;

      // Downslope.
      Triple loc1 {crossing, upper, axisMax};
      double scalar1 = 0.0;

      // Eternity justify.
      Triple loc2 {upper, axisMax, axisMax};
      double scalar2 = 0.0;

      out.push (hb_pair (scalar1 - gain, loc1));
      out.push (hb_pair (scalar2 - gain, loc2));
    }
  }

  else
  {
    // Special-case if peak is at axisMax.
    if (axisMax == peak)
	upper = peak;

    /* Case 3:
     * we keep deltas as is and only scale the axis upper to achieve
     * the desired new tent if feasible.
     *
     *                        peak
     *  1.....................o....................
     *                       / \_|
     *  ..................../....+_.........outGain
     *                     /     | \
     *  gain..............+......|..+_.............
     *                   /|      |  | \
     *  0---|-----------o |      |  |  o----------1
     *    axisMin    lower|      |  |   upper
     *                    |      |  newUpper
     *              axisDef      axisMax
     */
    double newUpper = peak + (1 - gain) * (upper - peak);
    assert (axisMax <= newUpper);  // Because outGain > gain
    /* Disabled because ots doesn't like us:
     * https://github.com/fonttools/fonttools/issues/3350 */

    if (false && (newUpper <= axisDef + (axisMax - axisDef) * 2))
    {
      upper = newUpper;
      if (!negative && axisDef + (axisMax - axisDef) * MAX_F2DOT14 < upper)
      {
	// we clamp +2.0 to the max F2Dot14 (~1.99994) for convenience
	upper = axisDef + (axisMax - axisDef) * MAX_F2DOT14;
	assert (peak < upper);
      }

      Triple loc {hb_max (axisDef, lower), peak, upper};
      double scalar = 1.0;

      out.push (hb_pair (scalar - gain, loc));
    }

    /* Case 4: New limit doesn't fit; we need to chop into two tents,
     * because the shape of a triangle with part of one side cut off
     * cannot be represented as a triangle itself.
     *
     *            |   peak |
     *  1.........|......o.|....................
     *  ..........|...../x\|.............outGain
     *            |    |xxy|\_
     *            |   /xxxy|  \_
     *            |  |xxxxy|    \_
     *            |  /xxxxy|      \_
     *  0---|-----|-oxxxxxx|        o----------1
     *    axisMin | lower  |        upper
     *            |        |
     *          axisDef  axisMax
     */
    else
    {
      Triple loc1 {hb_max (axisDef, lower), peak, axisMax};
      double scalar1 = 1.0;

      Triple loc2 {peak, axisMax, axisMax};
      double scalar2 = outGain;

      out.push (hb_pair (scalar1 - gain, loc1));
      // Don't add a dirac delta!
      if (peak < axisMax)
	out.push (hb_pair (scalar2 - gain, loc2));
    }
  }

  /* Now, the negative side
   *
   * Case 1neg: Lower extends beyond axisMin: we chop. Simple.
   *
   *                     |   |peak
   *  1..................|...|.o.................
   *                     |   |/ \
   *  gain...............|...+...\...............
   *                     |x_/|    \
   *                     |/  |     \
   *                   _/|   |      \
   *  0---------------o  |   |       o----------1
   *              lower  |   |       upper
   *                     |   |
   *               axisMin   axisDef
   */
  if (lower <= axisMin)
  {
    Triple loc {axisMin, axisMin, axisDef};
    double scalar = supportScalar (axisMin, tent);

    out.push (hb_pair (scalar - gain, loc));
  }

  /* Case 2neg: Lower is betwen axisMin and axisDef: we add two
   * tents to keep it down all the way to eternity.
   *
   *      |               |peak
   *  1...|...............|.o.................
   *      |               |/ \
   *  gain|...............+...\...............
   *      |yxxxxxxxxxxxxx/|    \
   *      |yyyyyyxxxxxxx/ |     \
   *      |yyyyyyyyyyyx/  |      \
   *  0---|-----------o   |       o----------1
   *    axisMin    lower  |       upper
   *                      |
   *                    axisDef
   */
  else
  {
    // A tent's peak cannot fall on axis default. Nudge it.
    if (lower == axisDef)
      lower -= EPSILON;

    // Downslope.
    Triple loc1 {axisMin, lower, axisDef};
    double scalar1 = 0.0;

    // Eternity justify.
    Triple loc2 {axisMin, axisMin, lower};
    double scalar2 = 0.0;

    out.push (hb_pair (scalar1 - gain, loc1));
    out.push (hb_pair (scalar2 - gain, loc2));
  }

  return out;
}

static inline TripleDistances _reverse_triple_distances (const TripleDistances &v)
{ return TripleDistances (v.positive, v.negative); }

double renormalizeValue (double v, const Triple &triple,
                         const TripleDistances &triple_distances, bool extrapolate)
{
  double lower = triple.minimum, def = triple.middle, upper = triple.maximum;
  assert (lower <= def && def <= upper);

  if (!extrapolate)
    v = hb_clamp (v, lower, upper);

  if (v == def)
    return 0.0;

  if (def < 0.0)
    return -renormalizeValue (-v, _reverse_negate (triple),
                              _reverse_triple_distances (triple_distances), extrapolate);

  /* default >= 0 and v != default */
  if (v > def)
    return (v - def) / (upper - def);

  /* v < def */
  if (lower >= 0.0)
    return (v - def) / (def - lower);

  /* lower < 0 and v < default */
  double total_distance = triple_distances.negative * (-lower) + triple_distances.positive * def;

  double v_distance;
  if (v >= 0.0)
    v_distance = (def - v) * triple_distances.positive;
  else
    v_distance = (-v) * triple_distances.negative + triple_distances.positive * def;

  return (-v_distance) /total_distance;
}

rebase_tent_result_t
rebase_tent (Triple tent, Triple axisLimit, TripleDistances axis_triple_distances)
{
  assert (-1.0 <= axisLimit.minimum && axisLimit.minimum <= axisLimit.middle && axisLimit.middle <= axisLimit.maximum && axisLimit.maximum <= +1.0);
  assert (-2.0 <= tent.minimum && tent.minimum <= tent.middle && tent.middle <= tent.maximum && tent.maximum <= +2.0);
  assert (tent.middle != 0.0);

  rebase_tent_result_t sols = _solve (tent, axisLimit);

  auto n = [&axisLimit, &axis_triple_distances] (double v) { return renormalizeValue (v, axisLimit, axis_triple_distances); };

  rebase_tent_result_t out;
  for (auto &p : sols)
  {
    if (!p.first) continue;
    if (p.second == Triple{})
    {
      out.push (p);
      continue;
    }
    Triple t = p.second;
    out.push (hb_pair (p.first,
		       Triple{n (t.minimum), n (t.middle), n (t.maximum)}));
  }

  return out;
}
