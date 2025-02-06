using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

#nullable enable

namespace Godot
{
    /// <summary>
    /// 2-element structure that can be used to represent positions in 2D space or any other pair of numeric values.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2 : IEquatable<Vector2>
    {
        /// <summary>
        /// Enumerated index values for the axes.
        /// Returned by <see cref="MaxAxisIndex"/> and <see cref="MinAxisIndex"/>.
        /// </summary>
        public enum Axis
        {
            /// <summary>
            /// The vector's X axis.
            /// </summary>
            X = 0,
            /// <summary>
            /// The vector's Y axis.
            /// </summary>
            Y
        }

        /// <summary>
        /// The vector's X component. Also accessible by using the index position <c>[0]</c>.
        /// </summary>
        public real_t X;

        /// <summary>
        /// The vector's Y component. Also accessible by using the index position <c>[1]</c>.
        /// </summary>
        public real_t Y;

        /// <summary>
        /// Access vector components using their index.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is not 0 or 1.
        /// </exception>
        /// <value>
        /// <c>[0]</c> is equivalent to <see cref="X"/>,
        /// <c>[1]</c> is equivalent to <see cref="Y"/>.
        /// </value>
        public real_t this[int index]
        {
            readonly get
            {
                switch (index)
                {
                    case 0:
                        return X;
                    case 1:
                        return Y;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
            set
            {
                switch (index)
                {
                    case 0:
                        X = value;
                        return;
                    case 1:
                        Y = value;
                        return;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
        }

        /// <summary>
        /// Helper method for deconstruction into a tuple.
        /// </summary>
        public readonly void Deconstruct(out real_t x, out real_t y)
        {
            x = X;
            y = Y;
        }

        internal void Normalize()
        {
            real_t lengthsq = LengthSquared();

            if (lengthsq == 0)
            {
                X = Y = 0f;
            }
            else
            {
                real_t length = Mathf.Sqrt(lengthsq);
                X /= length;
                Y /= length;
            }
        }

        /// <summary>
        /// Returns a new vector with all components in absolute values (i.e. positive).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Abs(real_t)"/> called on each component.</returns>
        public readonly Vector2 Abs()
        {
            return new Vector2(Mathf.Abs(X), Mathf.Abs(Y));
        }

        /// <summary>
        /// Returns this vector's angle with respect to the X axis, or (1, 0) vector, in radians.
        ///
        /// Equivalent to the result of <see cref="Mathf.Atan2(real_t, real_t)"/> when
        /// called with the vector's <see cref="Y"/> and <see cref="X"/> as parameters: <c>Mathf.Atan2(v.Y, v.X)</c>.
        /// </summary>
        /// <returns>The angle of this vector, in radians.</returns>
        public readonly real_t Angle()
        {
            return Mathf.Atan2(Y, X);
        }

        /// <summary>
        /// Returns the angle to the given vector, in radians.
        /// </summary>
        /// <param name="to">The other vector to compare this vector to.</param>
        /// <returns>The angle between the two vectors, in radians.</returns>
        public readonly real_t AngleTo(Vector2 to)
        {
            return Mathf.Atan2(Cross(to), Dot(to));
        }

        /// <summary>
        /// Returns the angle between the line connecting the two points and the X axis, in radians.
        /// </summary>
        /// <param name="to">The other vector to compare this vector to.</param>
        /// <returns>The angle between the two vectors, in radians.</returns>
        public readonly real_t AngleToPoint(Vector2 to)
        {
            return Mathf.Atan2(to.Y - Y, to.X - X);
        }

        /// <summary>
        /// Returns the aspect ratio of this vector, the ratio of <see cref="X"/> to <see cref="Y"/>.
        /// </summary>
        /// <returns>The <see cref="X"/> component divided by the <see cref="Y"/> component.</returns>
        public readonly real_t Aspect()
        {
            return X / Y;
        }

        /// <summary>
        /// Returns the vector "bounced off" from a plane defined by the given normal.
        /// </summary>
        /// <param name="normal">The normal vector defining the plane to bounce off. Must be normalized.</param>
        /// <returns>The bounced vector.</returns>
        public readonly Vector2 Bounce(Vector2 normal)
        {
            return -Reflect(normal);
        }

        /// <summary>
        /// Returns a new vector with all components rounded up (towards positive infinity).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Ceil(real_t)"/> called on each component.</returns>
        public readonly Vector2 Ceil()
        {
            return new Vector2(Mathf.Ceil(X), Mathf.Ceil(Y));
        }

        /// <summary>
        /// Returns a new vector with all components clamped between the
        /// components of <paramref name="min"/> and <paramref name="max"/> using
        /// <see cref="Mathf.Clamp(real_t, real_t, real_t)"/>.
        /// </summary>
        /// <param name="min">The vector with minimum allowed values.</param>
        /// <param name="max">The vector with maximum allowed values.</param>
        /// <returns>The vector with all components clamped.</returns>
        public readonly Vector2 Clamp(Vector2 min, Vector2 max)
        {
            return new Vector2
            (
                Mathf.Clamp(X, min.X, max.X),
                Mathf.Clamp(Y, min.Y, max.Y)
            );
        }

        /// <summary>
        /// Returns a new vector with all components clamped between the
        /// <paramref name="min"/> and <paramref name="max"/> using
        /// <see cref="Mathf.Clamp(real_t, real_t, real_t)"/>.
        /// </summary>
        /// <param name="min">The minimum allowed value.</param>
        /// <param name="max">The maximum allowed value.</param>
        /// <returns>The vector with all components clamped.</returns>
        public readonly Vector2 Clamp(real_t min, real_t max)
        {
            return new Vector2
            (
                Mathf.Clamp(X, min, max),
                Mathf.Clamp(Y, min, max)
            );
        }

        /// <summary>
        /// Returns the cross product of this vector and <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other vector.</param>
        /// <returns>The cross product value.</returns>
        public readonly real_t Cross(Vector2 with)
        {
            return (X * with.Y) - (Y * with.X);
        }

        /// <summary>
        /// Performs a cubic interpolation between vectors <paramref name="preA"/>, this vector,
        /// <paramref name="b"/>, and <paramref name="postB"/>, by the given amount <paramref name="weight"/>.
        /// </summary>
        /// <param name="b">The destination vector.</param>
        /// <param name="preA">A vector before this vector.</param>
        /// <param name="postB">A vector after <paramref name="b"/>.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The interpolated vector.</returns>
        public readonly Vector2 CubicInterpolate(Vector2 b, Vector2 preA, Vector2 postB, real_t weight)
        {
            return new Vector2
            (
                Mathf.CubicInterpolate(X, b.X, preA.X, postB.X, weight),
                Mathf.CubicInterpolate(Y, b.Y, preA.Y, postB.Y, weight)
            );
        }

        /// <summary>
        /// Performs a cubic interpolation between vectors <paramref name="preA"/>, this vector,
        /// <paramref name="b"/>, and <paramref name="postB"/>, by the given amount <paramref name="weight"/>.
        /// It can perform smoother interpolation than <see cref="CubicInterpolate"/>
        /// by the time values.
        /// </summary>
        /// <param name="b">The destination vector.</param>
        /// <param name="preA">A vector before this vector.</param>
        /// <param name="postB">A vector after <paramref name="b"/>.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <param name="t"></param>
        /// <param name="preAT"></param>
        /// <param name="postBT"></param>
        /// <returns>The interpolated vector.</returns>
        public readonly Vector2 CubicInterpolateInTime(Vector2 b, Vector2 preA, Vector2 postB, real_t weight, real_t t, real_t preAT, real_t postBT)
        {
            return new Vector2
            (
                Mathf.CubicInterpolateInTime(X, b.X, preA.X, postB.X, weight, t, preAT, postBT),
                Mathf.CubicInterpolateInTime(Y, b.Y, preA.Y, postB.Y, weight, t, preAT, postBT)
            );
        }

        /// <summary>
        /// Returns the point at the given <paramref name="t"/> on a one-dimensional Bezier curve defined by this vector
        /// and the given <paramref name="control1"/>, <paramref name="control2"/>, and <paramref name="end"/> points.
        /// </summary>
        /// <param name="control1">Control point that defines the bezier curve.</param>
        /// <param name="control2">Control point that defines the bezier curve.</param>
        /// <param name="end">The destination vector.</param>
        /// <param name="t">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The interpolated vector.</returns>
        public readonly Vector2 BezierInterpolate(Vector2 control1, Vector2 control2, Vector2 end, real_t t)
        {
            return new Vector2
            (
                Mathf.BezierInterpolate(X, control1.X, control2.X, end.X, t),
                Mathf.BezierInterpolate(Y, control1.Y, control2.Y, end.Y, t)
            );
        }

        /// <summary>
        /// Returns the derivative at the given <paramref name="t"/> on the Bezier curve defined by this vector
        /// and the given <paramref name="control1"/>, <paramref name="control2"/>, and <paramref name="end"/> points.
        /// </summary>
        /// <param name="control1">Control point that defines the bezier curve.</param>
        /// <param name="control2">Control point that defines the bezier curve.</param>
        /// <param name="end">The destination value for the interpolation.</param>
        /// <param name="t">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public readonly Vector2 BezierDerivative(Vector2 control1, Vector2 control2, Vector2 end, real_t t)
        {
            return new Vector2(
                Mathf.BezierDerivative(X, control1.X, control2.X, end.X, t),
                Mathf.BezierDerivative(Y, control1.Y, control2.Y, end.Y, t)
            );
        }

        /// <summary>
        /// Returns the normalized vector pointing from this vector to <paramref name="to"/>.
        /// </summary>
        /// <param name="to">The other vector to point towards.</param>
        /// <returns>The direction from this vector to <paramref name="to"/>.</returns>
        public readonly Vector2 DirectionTo(Vector2 to)
        {
            return new Vector2(to.X - X, to.Y - Y).Normalized();
        }

        /// <summary>
        /// Returns the squared distance between this vector and <paramref name="to"/>.
        /// This method runs faster than <see cref="DistanceTo"/>, so prefer it if
        /// you need to compare vectors or need the squared distance for some formula.
        /// </summary>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The squared distance between the two vectors.</returns>
        public readonly real_t DistanceSquaredTo(Vector2 to)
        {
            return (X - to.X) * (X - to.X) + (Y - to.Y) * (Y - to.Y);
        }

        /// <summary>
        /// Returns the distance between this vector and <paramref name="to"/>.
        /// </summary>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The distance between the two vectors.</returns>
        public readonly real_t DistanceTo(Vector2 to)
        {
            return Mathf.Sqrt((X - to.X) * (X - to.X) + (Y - to.Y) * (Y - to.Y));
        }

        /// <summary>
        /// Returns the dot product of this vector and <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The dot product of the two vectors.</returns>
        public readonly real_t Dot(Vector2 with)
        {
            return (X * with.X) + (Y * with.Y);
        }

        /// <summary>
        /// Returns a new vector with all components rounded down (towards negative infinity).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Floor(real_t)"/> called on each component.</returns>
        public readonly Vector2 Floor()
        {
            return new Vector2(Mathf.Floor(X), Mathf.Floor(Y));
        }

        /// <summary>
        /// Returns the inverse of this vector. This is the same as <c>new Vector2(1 / v.X, 1 / v.Y)</c>.
        /// </summary>
        /// <returns>The inverse of this vector.</returns>
        public readonly Vector2 Inverse()
        {
            return new Vector2(1 / X, 1 / Y);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector is finite, by calling
        /// <see cref="Mathf.IsFinite(real_t)"/> on each component.
        /// </summary>
        /// <returns>Whether this vector is finite or not.</returns>
        public readonly bool IsFinite()
        {
            return Mathf.IsFinite(X) && Mathf.IsFinite(Y);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vector is normalized, and <see langword="false"/> otherwise.
        /// </summary>
        /// <returns>A <see langword="bool"/> indicating whether or not the vector is normalized.</returns>
        public readonly bool IsNormalized()
        {
            return Mathf.Abs(LengthSquared() - 1.0f) < Mathf.Epsilon;
        }

        /// <summary>
        /// Returns the length (magnitude) of this vector.
        /// </summary>
        /// <seealso cref="LengthSquared"/>
        /// <returns>The length of this vector.</returns>
        public readonly real_t Length()
        {
            return Mathf.Sqrt((X * X) + (Y * Y));
        }

        /// <summary>
        /// Returns the squared length (squared magnitude) of this vector.
        /// This method runs faster than <see cref="Length"/>, so prefer it if
        /// you need to compare vectors or need the squared length for some formula.
        /// </summary>
        /// <returns>The squared length of this vector.</returns>
        public readonly real_t LengthSquared()
        {
            return (X * X) + (Y * Y);
        }

        /// <summary>
        /// Returns the result of the linear interpolation between
        /// this vector and <paramref name="to"/> by amount <paramref name="weight"/>.
        /// </summary>
        /// <param name="to">The destination vector for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting vector of the interpolation.</returns>
        public readonly Vector2 Lerp(Vector2 to, real_t weight)
        {
            return new Vector2
            (
                Mathf.Lerp(X, to.X, weight),
                Mathf.Lerp(Y, to.Y, weight)
            );
        }

        /// <summary>
        /// Returns the vector with a maximum length by limiting its length to <paramref name="length"/>.
        /// </summary>
        /// <param name="length">The length to limit to.</param>
        /// <returns>The vector with its length limited.</returns>
        public readonly Vector2 LimitLength(real_t length = 1.0f)
        {
            Vector2 v = this;
            real_t l = Length();

            if (l > 0 && length < l)
            {
                v /= l;
                v *= length;
            }

            return v;
        }

        /// <summary>
        /// Returns the result of the component-wise maximum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector2(Mathf.Max(X, with.X), Mathf.Max(Y, with.Y))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector2 Max(Vector2 with)
        {
            return new Vector2
            (
                Mathf.Max(X, with.X),
                Mathf.Max(Y, with.Y)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise maximum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector2(Mathf.Max(X, with), Mathf.Max(Y, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector2 Max(real_t with)
        {
            return new Vector2
            (
                Mathf.Max(X, with),
                Mathf.Max(Y, with)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise minimum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector2(Mathf.Min(X, with.X), Mathf.Min(Y, with.Y))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector2 Min(Vector2 with)
        {
            return new Vector2
            (
                Mathf.Min(X, with.X),
                Mathf.Min(Y, with.Y)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise minimum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector2(Mathf.Min(X, with), Mathf.Min(Y, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector2 Min(real_t with)
        {
            return new Vector2
            (
                Mathf.Min(X, with),
                Mathf.Min(Y, with)
            );
        }

        /// <summary>
        /// Returns the axis of the vector's highest value. See <see cref="Axis"/>.
        /// If both components are equal, this method returns <see cref="Axis.X"/>.
        /// </summary>
        /// <returns>The index of the highest axis.</returns>
        public readonly Axis MaxAxisIndex()
        {
            return X < Y ? Axis.Y : Axis.X;
        }

        /// <summary>
        /// Returns the axis of the vector's lowest value. See <see cref="Axis"/>.
        /// If both components are equal, this method returns <see cref="Axis.Y"/>.
        /// </summary>
        /// <returns>The index of the lowest axis.</returns>
        public readonly Axis MinAxisIndex()
        {
            return X < Y ? Axis.X : Axis.Y;
        }

        /// <summary>
        /// Moves this vector toward <paramref name="to"/> by the fixed <paramref name="delta"/> amount.
        /// </summary>
        /// <param name="to">The vector to move towards.</param>
        /// <param name="delta">The amount to move towards by.</param>
        /// <returns>The resulting vector.</returns>
        public readonly Vector2 MoveToward(Vector2 to, real_t delta)
        {
            Vector2 v = this;
            Vector2 vd = to - v;
            real_t len = vd.Length();
            if (len <= delta || len < Mathf.Epsilon)
                return to;

            return v + (vd / len * delta);
        }

        /// <summary>
        /// Returns the vector scaled to unit length. Equivalent to <c>v / v.Length()</c>.
        /// </summary>
        /// <returns>A normalized version of the vector.</returns>
        public readonly Vector2 Normalized()
        {
            Vector2 v = this;
            v.Normalize();
            return v;
        }

        /// <summary>
        /// Returns a vector composed of the <see cref="Mathf.PosMod(real_t, real_t)"/> of this vector's components
        /// and <paramref name="mod"/>.
        /// </summary>
        /// <param name="mod">A value representing the divisor of the operation.</param>
        /// <returns>
        /// A vector with each component <see cref="Mathf.PosMod(real_t, real_t)"/> by <paramref name="mod"/>.
        /// </returns>
        public readonly Vector2 PosMod(real_t mod)
        {
            Vector2 v;
            v.X = Mathf.PosMod(X, mod);
            v.Y = Mathf.PosMod(Y, mod);
            return v;
        }

        /// <summary>
        /// Returns a vector composed of the <see cref="Mathf.PosMod(real_t, real_t)"/> of this vector's components
        /// and <paramref name="modv"/>'s components.
        /// </summary>
        /// <param name="modv">A vector representing the divisors of the operation.</param>
        /// <returns>
        /// A vector with each component <see cref="Mathf.PosMod(real_t, real_t)"/> by <paramref name="modv"/>'s components.
        /// </returns>
        public readonly Vector2 PosMod(Vector2 modv)
        {
            Vector2 v;
            v.X = Mathf.PosMod(X, modv.X);
            v.Y = Mathf.PosMod(Y, modv.Y);
            return v;
        }

        /// <summary>
        /// Returns a new vector resulting from projecting this vector onto the given vector <paramref name="onNormal"/>.
        /// The resulting new vector is parallel to <paramref name="onNormal"/>.
        /// See also <see cref="Slide(Vector2)"/>.
        /// Note: If the vector <paramref name="onNormal"/> is a zero vector, the components of the resulting new vector will be <see cref="real_t.NaN"/>.
        /// </summary>
        /// <param name="onNormal">The vector to project onto.</param>
        /// <returns>The projected vector.</returns>
        public readonly Vector2 Project(Vector2 onNormal)
        {
            return onNormal * (Dot(onNormal) / onNormal.LengthSquared());
        }

        /// <summary>
        /// Returns this vector reflected from a plane defined by the given <paramref name="normal"/>.
        /// </summary>
        /// <param name="normal">The normal vector defining the plane to reflect from. Must be normalized.</param>
        /// <returns>The reflected vector.</returns>
        public readonly Vector2 Reflect(Vector2 normal)
        {
#if DEBUG
            if (!normal.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized.", nameof(normal));
            }
#endif
            return (2 * Dot(normal) * normal) - this;
        }

        /// <summary>
        /// Rotates this vector by <paramref name="angle"/> radians.
        /// </summary>
        /// <param name="angle">The angle to rotate by, in radians.</param>
        /// <returns>The rotated vector.</returns>
        public readonly Vector2 Rotated(real_t angle)
        {
            (real_t sin, real_t cos) = Mathf.SinCos(angle);
            return new Vector2
            (
                X * cos - Y * sin,
                X * sin + Y * cos
            );
        }

        /// <summary>
        /// Returns this vector with all components rounded to the nearest integer,
        /// with halfway cases rounded towards the nearest multiple of two.
        /// </summary>
        /// <returns>The rounded vector.</returns>
        public readonly Vector2 Round()
        {
            return new Vector2(Mathf.Round(X), Mathf.Round(Y));
        }

        /// <summary>
        /// Returns a vector with each component set to one or negative one, depending
        /// on the signs of this vector's components, or zero if the component is zero,
        /// by calling <see cref="Mathf.Sign(real_t)"/> on each component.
        /// </summary>
        /// <returns>A vector with all components as either <c>1</c>, <c>-1</c>, or <c>0</c>.</returns>
        public readonly Vector2 Sign()
        {
            Vector2 v;
            v.X = Mathf.Sign(X);
            v.Y = Mathf.Sign(Y);
            return v;
        }

        /// <summary>
        /// Returns the result of the spherical linear interpolation between
        /// this vector and <paramref name="to"/> by amount <paramref name="weight"/>.
        ///
        /// This method also handles interpolating the lengths if the input vectors
        /// have different lengths. For the special case of one or both input vectors
        /// having zero length, this method behaves like <see cref="Lerp(Vector2, real_t)"/>.
        /// </summary>
        /// <param name="to">The destination vector for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting vector of the interpolation.</returns>
        public readonly Vector2 Slerp(Vector2 to, real_t weight)
        {
            real_t startLengthSquared = LengthSquared();
            real_t endLengthSquared = to.LengthSquared();
            if (startLengthSquared == 0.0 || endLengthSquared == 0.0)
            {
                // Zero length vectors have no angle, so the best we can do is either lerp or throw an error.
                return Lerp(to, weight);
            }
            real_t startLength = Mathf.Sqrt(startLengthSquared);
            real_t resultLength = Mathf.Lerp(startLength, Mathf.Sqrt(endLengthSquared), weight);
            real_t angle = AngleTo(to);
            return Rotated(angle * weight) * (resultLength / startLength);
        }

        /// <summary>
        /// Returns a new vector resulting from sliding this vector along a line with normal <paramref name="normal"/>.
        /// The resulting new vector is perpendicular to <paramref name="normal"/>, and is equivalent to this vector minus its projection on <paramref name="normal"/>.
        /// See also <see cref="Project(Vector2)"/>.
        /// Note: The vector <paramref name="normal"/> must be normalized. See also <see cref="Normalized()"/>.
        /// </summary>
        /// <param name="normal">The normal vector of the plane to slide on.</param>
        /// <returns>The slid vector.</returns>
        public readonly Vector2 Slide(Vector2 normal)
        {
            return this - (normal * Dot(normal));
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the nearest multiple of the corresponding component in <paramref name="step"/>.
        /// This can also be used to round to an arbitrary number of decimals.
        /// </summary>
        /// <param name="step">A vector value representing the step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector2 Snapped(Vector2 step)
        {
            return new Vector2(Mathf.Snapped(X, step.X), Mathf.Snapped(Y, step.Y));
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the nearest multiple of <paramref name="step"/>.
        /// This can also be used to round to an arbitrary number of decimals.
        /// </summary>
        /// <param name="step">The step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector2 Snapped(real_t step)
        {
            return new Vector2(Mathf.Snapped(X, step), Mathf.Snapped(Y, step));
        }

        /// <summary>
        /// Returns a perpendicular vector rotated 90 degrees counter-clockwise
        /// compared to the original, with the same length.
        /// </summary>
        /// <returns>The perpendicular vector.</returns>
        public readonly Vector2 Orthogonal()
        {
            return new Vector2(Y, -X);
        }

        // Constants
        private static readonly Vector2 _zero = new Vector2(0, 0);
        private static readonly Vector2 _one = new Vector2(1, 1);
        private static readonly Vector2 _inf = new Vector2(Mathf.Inf, Mathf.Inf);

        private static readonly Vector2 _up = new Vector2(0, -1);
        private static readonly Vector2 _down = new Vector2(0, 1);
        private static readonly Vector2 _right = new Vector2(1, 0);
        private static readonly Vector2 _left = new Vector2(-1, 0);

        /// <summary>
        /// Zero vector, a vector with all components set to <c>0</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector2(0, 0)</c>.</value>
        public static Vector2 Zero { get { return _zero; } }
        /// <summary>
        /// One vector, a vector with all components set to <c>1</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector2(1, 1)</c>.</value>
        public static Vector2 One { get { return _one; } }
        /// <summary>
        /// Infinity vector, a vector with all components set to <see cref="Mathf.Inf"/>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector2(Mathf.Inf, Mathf.Inf)</c>.</value>
        public static Vector2 Inf { get { return _inf; } }

        /// <summary>
        /// Up unit vector. Y is down in 2D, so this vector points -Y.
        /// </summary>
        /// <value>Equivalent to <c>new Vector2(0, -1)</c>.</value>
        public static Vector2 Up { get { return _up; } }
        /// <summary>
        /// Down unit vector. Y is down in 2D, so this vector points +Y.
        /// </summary>
        /// <value>Equivalent to <c>new Vector2(0, 1)</c>.</value>
        public static Vector2 Down { get { return _down; } }
        /// <summary>
        /// Right unit vector. Represents the direction of right.
        /// </summary>
        /// <value>Equivalent to <c>new Vector2(1, 0)</c>.</value>
        public static Vector2 Right { get { return _right; } }
        /// <summary>
        /// Left unit vector. Represents the direction of left.
        /// </summary>
        /// <value>Equivalent to <c>new Vector2(-1, 0)</c>.</value>
        public static Vector2 Left { get { return _left; } }

        /// <summary>
        /// Constructs a new <see cref="Vector2"/> with the given components.
        /// </summary>
        /// <param name="x">The vector's X component.</param>
        /// <param name="y">The vector's Y component.</param>
        public Vector2(real_t x, real_t y)
        {
            X = x;
            Y = y;
        }

        /// <summary>
        /// Creates a unit Vector2 rotated to the given angle. This is equivalent to doing
        /// <c>Vector2(Mathf.Cos(angle), Mathf.Sin(angle))</c> or <c>Vector2.Right.Rotated(angle)</c>.
        /// </summary>
        /// <param name="angle">Angle of the vector, in radians.</param>
        /// <returns>The resulting vector.</returns>
        public static Vector2 FromAngle(real_t angle)
        {
            (real_t sin, real_t cos) = Mathf.SinCos(angle);
            return new Vector2(cos, sin);
        }

        /// <summary>
        /// Adds each component of the <see cref="Vector2"/>
        /// with the components of the given <see cref="Vector2"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The added vector.</returns>
        public static Vector2 operator +(Vector2 left, Vector2 right)
        {
            Vector2 v;
            v.X = left.X + right.X;
            v.Y = left.Y + right.Y;
            return v;
        }

        /// <summary>
        /// Subtracts each component of the <see cref="Vector2"/>
        /// by the components of the given <see cref="Vector2"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The subtracted vector.</returns>
        public static Vector2 operator -(Vector2 left, Vector2 right)
        {
            Vector2 v;
            v.X = left.X - right.X;
            v.Y = left.Y - right.Y;
            return v;
        }

        /// <summary>
        /// Returns the negative value of the <see cref="Vector2"/>.
        /// This is the same as writing <c>new Vector2(-v.X, -v.Y)</c>.
        /// This operation flips the direction of the vector while
        /// keeping the same magnitude.
        /// With floats, the number zero can be either positive or negative.
        /// </summary>
        /// <param name="vec">The vector to negate/flip.</param>
        /// <returns>The negated/flipped vector.</returns>
        public static Vector2 operator -(Vector2 vec)
        {
            Vector2 v;
            v.X = -vec.X;
            v.Y = -vec.Y;
            return v;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector2"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="vec">The vector to multiply.</param>
        /// <param name="scale">The scale to multiply by.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector2 operator *(Vector2 vec, real_t scale)
        {
            Vector2 v;
            v.X = vec.X * scale;
            v.Y = vec.Y * scale;
            return v;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector2"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="scale">The scale to multiply by.</param>
        /// <param name="vec">The vector to multiply.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector2 operator *(real_t scale, Vector2 vec)
        {
            Vector2 v;
            v.X = vec.X * scale;
            v.Y = vec.Y * scale;
            return v;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector2"/>
        /// by the components of the given <see cref="Vector2"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector2 operator *(Vector2 left, Vector2 right)
        {
            Vector2 v;
            v.X = left.X * right.X;
            v.Y = left.Y * right.Y;
            return v;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector2"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The divided vector.</returns>
        public static Vector2 operator /(Vector2 vec, real_t divisor)
        {
            Vector2 v;
            v.X = vec.X / divisor;
            v.Y = vec.Y / divisor;
            return v;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector2"/>
        /// by the components of the given <see cref="Vector2"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The divided vector.</returns>
        public static Vector2 operator /(Vector2 vec, Vector2 divisorv)
        {
            Vector2 v;
            v.X = vec.X / divisorv.X;
            v.Y = vec.Y / divisorv.Y;
            return v;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector2"/>
        /// with the components of the given <see cref="real_t"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="PosMod(real_t)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector2(10, -20) % 7); // Prints "(3, -6)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector2 operator %(Vector2 vec, real_t divisor)
        {
            Vector2 v;
            v.X = vec.X % divisor;
            v.Y = vec.Y % divisor;
            return v;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector2"/>
        /// with the components of the given <see cref="Vector2"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="PosMod(Vector2)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector2(10, -20) % new Vector2(7, 8)); // Prints "(3, -4)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector2 operator %(Vector2 vec, Vector2 divisorv)
        {
            Vector2 v;
            v.X = vec.X % divisorv.X;
            v.Y = vec.Y % divisorv.Y;
            return v;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the vectors are exactly equal.</returns>
        public static bool operator ==(Vector2 left, Vector2 right)
        {
            return left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are not equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the vectors are not equal.</returns>
        public static bool operator !=(Vector2 left, Vector2 right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Compares two <see cref="Vector2"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than the right.</returns>
        public static bool operator <(Vector2 left, Vector2 right)
        {
            if (left.X == right.X)
            {
                return left.Y < right.Y;
            }
            return left.X < right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector2"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than the right.</returns>
        public static bool operator >(Vector2 left, Vector2 right)
        {
            if (left.X == right.X)
            {
                return left.Y > right.Y;
            }
            return left.X > right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector2"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than or equal to the right.</returns>
        public static bool operator <=(Vector2 left, Vector2 right)
        {
            if (left.X == right.X)
            {
                return left.Y <= right.Y;
            }
            return left.X < right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector2"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than or equal to the right.</returns>
        public static bool operator >=(Vector2 left, Vector2 right)
        {
            if (left.X == right.X)
            {
                return left.Y >= right.Y;
            }
            return left.X > right.X;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vector is exactly equal
        /// to the given object (<paramref name="obj"/>).
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="obj">The object to compare with.</param>
        /// <returns>Whether or not the vector and the object are equal.</returns>
        public override readonly bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is Vector2 other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="other">The other vector.</param>
        /// <returns>Whether or not the vectors are exactly equal.</returns>
        public readonly bool Equals(Vector2 other)
        {
            return X == other.X && Y == other.Y;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector and <paramref name="other"/> are approximately equal,
        /// by running <see cref="Mathf.IsEqualApprox(real_t, real_t)"/> on each component.
        /// </summary>
        /// <param name="other">The other vector to compare.</param>
        /// <returns>Whether or not the vectors are approximately equal.</returns>
        public readonly bool IsEqualApprox(Vector2 other)
        {
            return Mathf.IsEqualApprox(X, other.X) && Mathf.IsEqualApprox(Y, other.Y);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector's values are approximately zero,
        /// by running <see cref="Mathf.IsZeroApprox(real_t)"/> on each component.
        /// This method is faster than using <see cref="IsEqualApprox"/> with one value
        /// as a zero vector.
        /// </summary>
        /// <returns>Whether or not the vector is approximately zero.</returns>
        public readonly bool IsZeroApprox()
        {
            return Mathf.IsZeroApprox(X) && Mathf.IsZeroApprox(Y);
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Vector2"/>.
        /// </summary>
        /// <returns>A hash code for this vector.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(X, Y);
        }

        /// <summary>
        /// Converts this <see cref="Vector2"/> to a string.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Vector2"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public readonly string ToString(string? format)
        {
            return $"({X.ToString(format, CultureInfo.InvariantCulture)}, {Y.ToString(format, CultureInfo.InvariantCulture)})";
        }
    }
}
