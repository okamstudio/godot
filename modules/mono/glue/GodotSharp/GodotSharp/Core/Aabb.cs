using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;

#nullable enable

namespace Godot
{
    /// <summary>
    /// Axis-Aligned Bounding Box. AABB consists of a position, a size, and
    /// several utility functions. It is typically used for fast overlap tests.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Aabb : IEquatable<Aabb>
    {
        private Vector3 _position;
        private Vector3 _size;

        /// <summary>
        /// Beginning corner. Typically has values lower than <see cref="End"/>.
        /// </summary>
        /// <value>Directly uses a private field.</value>
        public Vector3 Position
        {
            readonly get { return _position; }
            set { _position = value; }
        }

        /// <summary>
        /// Size from <see cref="Position"/> to <see cref="End"/>. Typically all components are positive.
        /// If the size is negative, you can use <see cref="Abs"/> to fix it.
        /// </summary>
        /// <value>Directly uses a private field.</value>
        public Vector3 Size
        {
            readonly get { return _size; }
            set { _size = value; }
        }

        /// <summary>
        /// Ending corner. This is calculated as <see cref="Position"/> plus
        /// <see cref="Size"/>. Setting this value will change the size.
        /// </summary>
        /// <value>
        /// Getting is equivalent to <paramref name="value"/> = <see cref="Position"/> + <see cref="Size"/>,
        /// setting is equivalent to <see cref="Size"/> = <paramref name="value"/> - <see cref="Position"/>
        /// </value>
        public Vector3 End
        {
            readonly get { return _position + _size; }
            set { _size = value - _position; }
        }

        /// <summary>
        /// The volume of this <see cref="Aabb"/>.
        /// See also <see cref="HasVolume"/>.
        /// </summary>
        public readonly real_t Volume
        {
            get { return _size.X * _size.Y * _size.Z; }
        }

        /// <summary>
        /// Returns an <see cref="Aabb"/> with equivalent position and size, modified so that
        /// the most-negative corner is the origin and the size is positive.
        /// </summary>
        /// <returns>The modified <see cref="Aabb"/>.</returns>
        public readonly Aabb Abs()
        {
            Vector3 end = End;
            Vector3 topLeft = end.Min(_position);
            return new Aabb(topLeft, _size.Abs());
        }

        /// <summary>
        /// Returns the center of the <see cref="Aabb"/>, which is equal
        /// to <see cref="Position"/> + (<see cref="Size"/> / 2).
        /// </summary>
        /// <returns>The center.</returns>
        public readonly Vector3 GetCenter()
        {
            return _position + (_size * 0.5f);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this <see cref="Aabb"/> completely encloses another one.
        /// </summary>
        /// <param name="with">The other <see cref="Aabb"/> that may be enclosed.</param>
        /// <returns>
        /// A <see langword="bool"/> for whether or not this <see cref="Aabb"/> encloses <paramref name="with"/>.
        /// </returns>
        public readonly bool Encloses(Aabb with)
        {
            Vector3 srcMin = _position;
            Vector3 srcMax = _position + _size;
            Vector3 dstMin = with._position;
            Vector3 dstMax = with._position + with._size;

            return srcMin.X <= dstMin.X &&
                   srcMax.X >= dstMax.X &&
                   srcMin.Y <= dstMin.Y &&
                   srcMax.Y >= dstMax.Y &&
                   srcMin.Z <= dstMin.Z &&
                   srcMax.Z >= dstMax.Z;
        }

        /// <summary>
        /// Returns this <see cref="Aabb"/> expanded to include a given point.
        /// </summary>
        /// <param name="point">The point to include.</param>
        /// <returns>The expanded <see cref="Aabb"/>.</returns>
        public readonly Aabb Expand(Vector3 point)
        {
            Vector3 begin = _position;
            Vector3 end = _position + _size;

            if (point.X < begin.X)
            {
                begin.X = point.X;
            }
            if (point.Y < begin.Y)
            {
                begin.Y = point.Y;
            }
            if (point.Z < begin.Z)
            {
                begin.Z = point.Z;
            }

            if (point.X > end.X)
            {
                end.X = point.X;
            }
            if (point.Y > end.Y)
            {
                end.Y = point.Y;
            }
            if (point.Z > end.Z)
            {
                end.Z = point.Z;
            }

            return new Aabb(begin, end - begin);
        }

        /// <summary>
        /// Gets the position of one of the 8 endpoints of the <see cref="Aabb"/>.
        /// </summary>
        /// <param name="idx">Which endpoint to get.</param>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="idx"/> is less than 0 or greater than 7.
        /// </exception>
        /// <returns>An endpoint of the <see cref="Aabb"/>.</returns>
        public readonly Vector3 GetEndpoint(int idx)
        {
            switch (idx)
            {
                case 0:
                    return new Vector3(_position.X, _position.Y, _position.Z);
                case 1:
                    return new Vector3(_position.X, _position.Y, _position.Z + _size.Z);
                case 2:
                    return new Vector3(_position.X, _position.Y + _size.Y, _position.Z);
                case 3:
                    return new Vector3(_position.X, _position.Y + _size.Y, _position.Z + _size.Z);
                case 4:
                    return new Vector3(_position.X + _size.X, _position.Y, _position.Z);
                case 5:
                    return new Vector3(_position.X + _size.X, _position.Y, _position.Z + _size.Z);
                case 6:
                    return new Vector3(_position.X + _size.X, _position.Y + _size.Y, _position.Z);
                case 7:
                    return new Vector3(_position.X + _size.X, _position.Y + _size.Y, _position.Z + _size.Z);
                default:
                    {
                        throw new ArgumentOutOfRangeException(nameof(idx),
                            $"Index is {idx}, but a value from 0 to 7 is expected.");
                    }
            }
        }

        /// <summary>
        /// Returns the normalized longest axis of the <see cref="Aabb"/>.
        /// </summary>
        /// <returns>A vector representing the normalized longest axis of the <see cref="Aabb"/>.</returns>
        public readonly Vector3 GetLongestAxis()
        {
            var axis = new Vector3(1f, 0f, 0f);
            real_t maxSize = _size.X;

            if (_size.Y > maxSize)
            {
                axis = new Vector3(0f, 1f, 0f);
                maxSize = _size.Y;
            }

            if (_size.Z > maxSize)
            {
                axis = new Vector3(0f, 0f, 1f);
            }

            return axis;
        }

        /// <summary>
        /// Returns the <see cref="Vector3.Axis"/> index of the longest axis of the <see cref="Aabb"/>.
        /// </summary>
        /// <returns>A <see cref="Vector3.Axis"/> index for which axis is longest.</returns>
        public readonly Vector3.Axis GetLongestAxisIndex()
        {
            var axis = Vector3.Axis.X;
            real_t maxSize = _size.X;

            if (_size.Y > maxSize)
            {
                axis = Vector3.Axis.Y;
                maxSize = _size.Y;
            }

            if (_size.Z > maxSize)
            {
                axis = Vector3.Axis.Z;
            }

            return axis;
        }

        /// <summary>
        /// Returns the scalar length of the longest axis of the <see cref="Aabb"/>.
        /// </summary>
        /// <returns>The scalar length of the longest axis of the <see cref="Aabb"/>.</returns>
        public readonly real_t GetLongestAxisSize()
        {
            real_t maxSize = _size.X;

            if (_size.Y > maxSize)
                maxSize = _size.Y;

            if (_size.Z > maxSize)
                maxSize = _size.Z;

            return maxSize;
        }

        /// <summary>
        /// Returns the normalized shortest axis of the <see cref="Aabb"/>.
        /// </summary>
        /// <returns>A vector representing the normalized shortest axis of the <see cref="Aabb"/>.</returns>
        public readonly Vector3 GetShortestAxis()
        {
            var axis = new Vector3(1f, 0f, 0f);
            real_t maxSize = _size.X;

            if (_size.Y < maxSize)
            {
                axis = new Vector3(0f, 1f, 0f);
                maxSize = _size.Y;
            }

            if (_size.Z < maxSize)
            {
                axis = new Vector3(0f, 0f, 1f);
            }

            return axis;
        }

        /// <summary>
        /// Returns the <see cref="Vector3.Axis"/> index of the shortest axis of the <see cref="Aabb"/>.
        /// </summary>
        /// <returns>A <see cref="Vector3.Axis"/> index for which axis is shortest.</returns>
        public readonly Vector3.Axis GetShortestAxisIndex()
        {
            var axis = Vector3.Axis.X;
            real_t maxSize = _size.X;

            if (_size.Y < maxSize)
            {
                axis = Vector3.Axis.Y;
                maxSize = _size.Y;
            }

            if (_size.Z < maxSize)
            {
                axis = Vector3.Axis.Z;
            }

            return axis;
        }

        /// <summary>
        /// Returns the scalar length of the shortest axis of the <see cref="Aabb"/>.
        /// </summary>
        /// <returns>The scalar length of the shortest axis of the <see cref="Aabb"/>.</returns>
        public readonly real_t GetShortestAxisSize()
        {
            real_t maxSize = _size.X;

            if (_size.Y < maxSize)
                maxSize = _size.Y;

            if (_size.Z < maxSize)
                maxSize = _size.Z;

            return maxSize;
        }

        /// <summary>
        /// Returns the support point in a given direction.
        /// This is useful for collision detection algorithms.
        /// </summary>
        /// <param name="dir">The direction to find support for.</param>
        /// <returns>A vector representing the support.</returns>
        public readonly Vector3 GetSupport(Vector3 dir)
        {
            Vector3 support = _position;
            if (dir.X > 0.0f)
            {
                support.X += _size.X;
            }
            if (dir.Y > 0.0f)
            {
                support.Y += _size.Y;
            }
            if (dir.Z > 0.0f)
            {
                support.Z += _size.Z;
            }
            return support;
        }

        /// <summary>
        /// Returns a copy of the <see cref="Aabb"/> grown a given amount of units towards all the sides.
        /// </summary>
        /// <param name="by">The amount to grow by.</param>
        /// <returns>The grown <see cref="Aabb"/>.</returns>
        public readonly Aabb Grow(real_t by)
        {
            Aabb res = this;

            res._position.X -= by;
            res._position.Y -= by;
            res._position.Z -= by;
            res._size.X += 2.0f * by;
            res._size.Y += 2.0f * by;
            res._size.Z += 2.0f * by;

            return res;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the <see cref="Aabb"/> contains a point,
        /// or <see langword="false"/> otherwise.
        /// </summary>
        /// <param name="point">The point to check.</param>
        /// <returns>
        /// A <see langword="bool"/> for whether or not the <see cref="Aabb"/> contains <paramref name="point"/>.
        /// </returns>
        public readonly bool HasPoint(Vector3 point)
        {
            if (point.X < _position.X)
                return false;
            if (point.Y < _position.Y)
                return false;
            if (point.Z < _position.Z)
                return false;
            if (point.X > _position.X + _size.X)
                return false;
            if (point.Y > _position.Y + _size.Y)
                return false;
            if (point.Z > _position.Z + _size.Z)
                return false;

            return true;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the <see cref="Aabb"/>
        /// has a surface or a length, and <see langword="false"/>
        /// if the <see cref="Aabb"/> is empty (all components
        /// of <see cref="Size"/> are zero or negative).
        /// </summary>
        /// <returns>
        /// A <see langword="bool"/> for whether or not the <see cref="Aabb"/> has surface.
        /// </returns>
        public readonly bool HasSurface()
        {
            return _size.X > 0.0f || _size.Y > 0.0f || _size.Z > 0.0f;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the <see cref="Aabb"/> has
        /// area, and <see langword="false"/> if the <see cref="Aabb"/>
        /// is linear, empty, or has a negative <see cref="Size"/>.
        /// See also <see cref="Volume"/>.
        /// </summary>
        /// <returns>
        /// A <see langword="bool"/> for whether or not the <see cref="Aabb"/> has volume.
        /// </returns>
        public readonly bool HasVolume()
        {
            return _size.X > 0.0f && _size.Y > 0.0f && _size.Z > 0.0f;
        }

        /// <summary>
        /// Returns the intersection of this <see cref="Aabb"/> and <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other <see cref="Aabb"/>.</param>
        /// <returns>The clipped <see cref="Aabb"/>.</returns>
        public readonly Aabb Intersection(Aabb with)
        {
            Vector3 srcMin = _position;
            Vector3 srcMax = _position + _size;
            Vector3 dstMin = with._position;
            Vector3 dstMax = with._position + with._size;

            Vector3 min, max;

            if (srcMin.X > dstMax.X || srcMax.X < dstMin.X)
            {
                return new Aabb();
            }

            min.X = srcMin.X > dstMin.X ? srcMin.X : dstMin.X;
            max.X = srcMax.X < dstMax.X ? srcMax.X : dstMax.X;

            if (srcMin.Y > dstMax.Y || srcMax.Y < dstMin.Y)
            {
                return new Aabb();
            }

            min.Y = srcMin.Y > dstMin.Y ? srcMin.Y : dstMin.Y;
            max.Y = srcMax.Y < dstMax.Y ? srcMax.Y : dstMax.Y;

            if (srcMin.Z > dstMax.Z || srcMax.Z < dstMin.Z)
            {
                return new Aabb();
            }

            min.Z = srcMin.Z > dstMin.Z ? srcMin.Z : dstMin.Z;
            max.Z = srcMax.Z < dstMax.Z ? srcMax.Z : dstMax.Z;

            return new Aabb(min, max - min);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the <see cref="Aabb"/> overlaps with <paramref name="with"/>
        /// (i.e. they have at least one point in common).
        /// </summary>
        /// <param name="with">The other <see cref="Aabb"/> to check for intersections with.</param>
        /// <returns>
        /// A <see langword="bool"/> for whether or not they are intersecting.
        /// </returns>
        public readonly bool Intersects(Aabb with)
        {
            if (_position.X >= with._position.X + with._size.X)
                return false;
            if (_position.X + _size.X <= with._position.X)
                return false;
            if (_position.Y >= with._position.Y + with._size.Y)
                return false;
            if (_position.Y + _size.Y <= with._position.Y)
                return false;
            if (_position.Z >= with._position.Z + with._size.Z)
                return false;
            if (_position.Z + _size.Z <= with._position.Z)
                return false;

            return true;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the <see cref="Aabb"/> is on both sides of <paramref name="plane"/>.
        /// </summary>
        /// <param name="plane">The <see cref="Plane"/> to check for intersection.</param>
        /// <returns>
        /// A <see langword="bool"/> for whether or not the <see cref="Aabb"/> intersects the <see cref="Plane"/>.
        /// </returns>
        public readonly bool IntersectsPlane(Plane plane)
        {
            Vector3[] points =
            {
                new Vector3(_position.X, _position.Y, _position.Z),
                new Vector3(_position.X, _position.Y, _position.Z + _size.Z),
                new Vector3(_position.X, _position.Y + _size.Y, _position.Z),
                new Vector3(_position.X, _position.Y + _size.Y, _position.Z + _size.Z),
                new Vector3(_position.X + _size.X, _position.Y, _position.Z),
                new Vector3(_position.X + _size.X, _position.Y, _position.Z + _size.Z),
                new Vector3(_position.X + _size.X, _position.Y + _size.Y, _position.Z),
                new Vector3(_position.X + _size.X, _position.Y + _size.Y, _position.Z + _size.Z)
            };

            bool over = false;
            bool under = false;

            for (int i = 0; i < 8; i++)
            {
                if (plane.DistanceTo(points[i]) > 0)
                {
                    over = true;
                }
                else
                {
                    under = true;
                }
            }

            return under && over;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the <see cref="Aabb"/> intersects
        /// the ray along <paramref name="dir"/> positioned at <paramref name="from"/>.
        /// </summary>
        /// <param name="origin">The origin of the ray.</param>
        /// <param name="dir">The direction of the ray.</param>
        /// <returns>
        /// A <see langword="bool"/> for whether or not the <see cref="Aabb"/> intersects the ray.
        /// </returns>
        public readonly bool IntersectsRay(Vector3 from, Vector3 dir)
        {
            if (HasPoint(from)) return true;

            real_t tmin = real_t.MinValue;
            real_t tmax = real_t.MaxValue;

            Vector3 end = _position + _size;

            for (int i = 0; i < 3; i++)
            {
                if (dir[i] == 0)
                {
                    if ((from[i] < _position[i]) || (from[i] > end[i]))
                    {
                        return false;
                    }
                }
                else
                {
                    // Ray is not parallel to planes in this direction.
                    real_t t1 = (_position[i] - from[i]) / dir[i];
                    real_t t2 = (end[i] - from[i]) / dir[i];

                    if (t1 > t2)
                    {
                        (t2, t1) = (t1, t2);
                    }
                    if (t1 >= tmin)
                    {
                        tmin = t1;
                    }
                    if (t2 < tmax)
                    {
                        if (t2 < 0)
                        {
                            return false;
                        }
                        tmax = t2;
                    }
                    if (tmin > tmax)
                    {
                        return false;
                    }
                }
            }

            return true;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the <see cref="Aabb"/> intersects
        /// the line segment between <paramref name="from"/> and <paramref name="to"/>.
        /// </summary>
        /// <param name="from">The start of the line segment.</param>
        /// <param name="to">The end of the line segment.</param>
        /// <returns>
        /// A <see langword="bool"/> for whether or not the <see cref="Aabb"/> intersects the line segment.
        /// </returns>
        public readonly bool IntersectsSegment(Vector3 from, Vector3 to)
        {
            real_t min = 0f;
            real_t max = 1f;

            for (int i = 0; i < 3; i++)
            {
                real_t segFrom = from[i];
                real_t segTo = to[i];
                real_t boxBegin = _position[i];
                real_t boxEnd = boxBegin + _size[i];
                real_t cmin, cmax;

                if (segFrom < segTo)
                {
                    if (segFrom > boxEnd || segTo < boxBegin)
                    {
                        return false;
                    }

                    real_t length = segTo - segFrom;
                    cmin = segFrom < boxBegin ? (boxBegin - segFrom) / length : 0f;
                    cmax = segTo > boxEnd ? (boxEnd - segFrom) / length : 1f;
                }
                else
                {
                    if (segTo > boxEnd || segFrom < boxBegin)
                    {
                        return false;
                    }

                    real_t length = segTo - segFrom;
                    cmin = segFrom > boxEnd ? (boxEnd - segFrom) / length : 0f;
                    cmax = segTo < boxBegin ? (boxBegin - segFrom) / length : 1f;
                }

                if (cmin > min)
                {
                    min = cmin;
                }

                if (cmax < max)
                {
                    max = cmax;
                }
                if (max < min)
                {
                    return false;
                }
            }

            return true;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this <see cref="Aabb"/> is finite, by calling
        /// <see cref="Mathf.IsFinite(real_t)"/> on each component.
        /// </summary>
        /// <returns>Whether this vector is finite or not.</returns>
        public readonly bool IsFinite()
        {
            return _position.IsFinite() && _size.IsFinite();
        }

        /// <summary>
        /// Returns a larger <see cref="Aabb"/> that contains this <see cref="Aabb"/> and <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other <see cref="Aabb"/>.</param>
        /// <returns>The merged <see cref="Aabb"/>.</returns>
        public readonly Aabb Merge(Aabb with)
        {
            Vector3 beg1 = _position;
            Vector3 beg2 = with._position;
            var end1 = new Vector3(_size.X, _size.Y, _size.Z) + beg1;
            var end2 = new Vector3(with._size.X, with._size.Y, with._size.Z) + beg2;

            var min = new Vector3(
                beg1.X < beg2.X ? beg1.X : beg2.X,
                beg1.Y < beg2.Y ? beg1.Y : beg2.Y,
                beg1.Z < beg2.Z ? beg1.Z : beg2.Z
            );

            var max = new Vector3(
                end1.X > end2.X ? end1.X : end2.X,
                end1.Y > end2.Y ? end1.Y : end2.Y,
                end1.Z > end2.Z ? end1.Z : end2.Z
            );

            return new Aabb(min, max - min);
        }

        /// <summary>
        /// Constructs an <see cref="Aabb"/> from a position and size.
        /// </summary>
        /// <param name="position">The position.</param>
        /// <param name="size">The size, typically positive.</param>
        public Aabb(Vector3 position, Vector3 size)
        {
            _position = position;
            _size = size;
        }

        /// <summary>
        /// Constructs an <see cref="Aabb"/> from a <paramref name="position"/>,
        /// <paramref name="width"/>, <paramref name="height"/>, and <paramref name="depth"/>.
        /// </summary>
        /// <param name="position">The position.</param>
        /// <param name="width">The width, typically positive.</param>
        /// <param name="height">The height, typically positive.</param>
        /// <param name="depth">The depth, typically positive.</param>
        public Aabb(Vector3 position, real_t width, real_t height, real_t depth)
        {
            _position = position;
            _size = new Vector3(width, height, depth);
        }

        /// <summary>
        /// Constructs an <see cref="Aabb"/> from <paramref name="x"/>,
        /// <paramref name="y"/>, <paramref name="z"/>, and <paramref name="size"/>.
        /// </summary>
        /// <param name="x">The position's X coordinate.</param>
        /// <param name="y">The position's Y coordinate.</param>
        /// <param name="z">The position's Z coordinate.</param>
        /// <param name="size">The size, typically positive.</param>
        public Aabb(real_t x, real_t y, real_t z, Vector3 size)
        {
            _position = new Vector3(x, y, z);
            _size = size;
        }

        /// <summary>
        /// Constructs an <see cref="Aabb"/> from <paramref name="x"/>,
        /// <paramref name="y"/>, <paramref name="z"/>, <paramref name="width"/>,
        /// <paramref name="height"/>, and <paramref name="depth"/>.
        /// </summary>
        /// <param name="x">The position's X coordinate.</param>
        /// <param name="y">The position's Y coordinate.</param>
        /// <param name="z">The position's Z coordinate.</param>
        /// <param name="width">The width, typically positive.</param>
        /// <param name="height">The height, typically positive.</param>
        /// <param name="depth">The depth, typically positive.</param>
        public Aabb(real_t x, real_t y, real_t z, real_t width, real_t height, real_t depth)
        {
            _position = new Vector3(x, y, z);
            _size = new Vector3(width, height, depth);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the AABBs are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left AABB.</param>
        /// <param name="right">The right AABB.</param>
        /// <returns>Whether or not the AABBs are exactly equal.</returns>
        public static bool operator ==(Aabb left, Aabb right)
        {
            return left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the AABBs are not equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left AABB.</param>
        /// <param name="right">The right AABB.</param>
        /// <returns>Whether or not the AABBs are not equal.</returns>
        public static bool operator !=(Aabb left, Aabb right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the AABB is exactly equal
        /// to the given object (<paramref name="obj"/>).
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="obj">The object to compare with.</param>
        /// <returns>Whether or not the AABB and the object are equal.</returns>
        public override readonly bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is Aabb other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the AABBs are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="other">The other AABB.</param>
        /// <returns>Whether or not the AABBs are exactly equal.</returns>
        public readonly bool Equals(Aabb other)
        {
            return _position == other._position && _size == other._size;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this AABB and <paramref name="other"/> are approximately equal,
        /// by running <see cref="Vector3.IsEqualApprox(Vector3)"/> on each component.
        /// </summary>
        /// <param name="other">The other AABB to compare.</param>
        /// <returns>Whether or not the AABBs structures are approximately equal.</returns>
        public readonly bool IsEqualApprox(Aabb other)
        {
            return _position.IsEqualApprox(other._position) && _size.IsEqualApprox(other._size);
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Aabb"/>.
        /// </summary>
        /// <returns>A hash code for this AABB.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(_position, _size);
        }

        /// <summary>
        /// Converts this <see cref="Aabb"/> to a string.
        /// </summary>
        /// <returns>A string representation of this AABB.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Aabb"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this AABB.</returns>
        public readonly string ToString(string? format)
        {
            return $"{_position.ToString(format)}, {_size.ToString(format)}";
        }
    }
}
