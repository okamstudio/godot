using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

#nullable enable

namespace Godot
{
    /// <summary>
    /// 3-element structure that can be used to represent positions in 3D space or any other pair of numeric values.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3 : IEquatable<Vector3>
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
            Y,
            /// <summary>
            /// The vector's Z axis.
            /// </summary>
            Z
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
        /// The vector's Z component. Also accessible by using the index position <c>[2]</c>.
        /// </summary>
        public real_t Z;

        /// <summary>
        /// Access vector components using their index.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is not 0, 1 or 2.
        /// </exception>
        /// <value>
        /// <c>[0]</c> is equivalent to <see cref="X"/>,
        /// <c>[1]</c> is equivalent to <see cref="Y"/>,
        /// <c>[2]</c> is equivalent to <see cref="Z"/>.
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
                    case 2:
                        return Z;
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
                    case 2:
                        Z = value;
                        return;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
        }

        /// <summary>
        /// Helper method for deconstruction into a tuple.
        /// </summary>
        public readonly void Deconstruct(out real_t x, out real_t y, out real_t z)
        {
            x = X;
            y = Y;
            z = Z;
        }

        internal void Normalize()
        {
            real_t lengthsq = LengthSquared();

            if (lengthsq == 0)
            {
                X = Y = Z = 0f;
            }
            else
            {
                real_t length = Mathf.Sqrt(lengthsq);
                X /= length;
                Y /= length;
                Z /= length;
            }
        }

        /// <summary>
        /// Returns a new vector with all components in absolute values (i.e. positive).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Abs(real_t)"/> called on each component.</returns>
        public readonly Vector3 Abs()
        {
            return new Vector3(Mathf.Abs(X), Mathf.Abs(Y), Mathf.Abs(Z));
        }

        /// <summary>
        /// Returns the unsigned minimum angle to the given vector, in radians.
        /// </summary>
        /// <param name="to">The other vector to compare this vector to.</param>
        /// <returns>The unsigned angle between the two vectors, in radians.</returns>
        public readonly real_t AngleTo(Vector3 to)
        {
            return Mathf.Atan2(Cross(to).Length(), Dot(to));
        }

        /// <summary>
        /// Returns this vector "bounced off" from a plane defined by the given normal.
        /// </summary>
        /// <param name="normal">The normal vector defining the plane to bounce off. Must be normalized.</param>
        /// <returns>The bounced vector.</returns>
        public readonly Vector3 Bounce(Vector3 normal)
        {
            return -Reflect(normal);
        }

        /// <summary>
        /// Returns a new vector with all components rounded up (towards positive infinity).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Ceil(real_t)"/> called on each component.</returns>
        public readonly Vector3 Ceil()
        {
            return new Vector3(Mathf.Ceil(X), Mathf.Ceil(Y), Mathf.Ceil(Z));
        }

        /// <summary>
        /// Returns a new vector with all components clamped between the
        /// components of <paramref name="min"/> and <paramref name="max"/> using
        /// <see cref="Mathf.Clamp(real_t, real_t, real_t)"/>.
        /// </summary>
        /// <param name="min">The vector with minimum allowed values.</param>
        /// <param name="max">The vector with maximum allowed values.</param>
        /// <returns>The vector with all components clamped.</returns>
        public readonly Vector3 Clamp(Vector3 min, Vector3 max)
        {
            return new Vector3
            (
                Mathf.Clamp(X, min.X, max.X),
                Mathf.Clamp(Y, min.Y, max.Y),
                Mathf.Clamp(Z, min.Z, max.Z)
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
        public readonly Vector3 Clamp(real_t min, real_t max)
        {
            return new Vector3
            (
                Mathf.Clamp(X, min, max),
                Mathf.Clamp(Y, min, max),
                Mathf.Clamp(Z, min, max)
            );
        }

        /// <summary>
        /// Returns the cross product of this vector and <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other vector.</param>
        /// <returns>The cross product vector.</returns>
        public readonly Vector3 Cross(Vector3 with)
        {
            return new Vector3
            (
                (Y * with.Z) - (Z * with.Y),
                (Z * with.X) - (X * with.Z),
                (X * with.Y) - (Y * with.X)
            );
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
        public readonly Vector3 CubicInterpolate(Vector3 b, Vector3 preA, Vector3 postB, real_t weight)
        {
            return new Vector3
            (
                Mathf.CubicInterpolate(X, b.X, preA.X, postB.X, weight),
                Mathf.CubicInterpolate(Y, b.Y, preA.Y, postB.Y, weight),
                Mathf.CubicInterpolate(Z, b.Z, preA.Z, postB.Z, weight)
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
        public readonly Vector3 CubicInterpolateInTime(Vector3 b, Vector3 preA, Vector3 postB, real_t weight, real_t t, real_t preAT, real_t postBT)
        {
            return new Vector3
            (
                Mathf.CubicInterpolateInTime(X, b.X, preA.X, postB.X, weight, t, preAT, postBT),
                Mathf.CubicInterpolateInTime(Y, b.Y, preA.Y, postB.Y, weight, t, preAT, postBT),
                Mathf.CubicInterpolateInTime(Z, b.Z, preA.Z, postB.Z, weight, t, preAT, postBT)
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
        public readonly Vector3 BezierInterpolate(Vector3 control1, Vector3 control2, Vector3 end, real_t t)
        {
            return new Vector3
            (
                Mathf.BezierInterpolate(X, control1.X, control2.X, end.X, t),
                Mathf.BezierInterpolate(Y, control1.Y, control2.Y, end.Y, t),
                Mathf.BezierInterpolate(Z, control1.Z, control2.Z, end.Z, t)
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
        public readonly Vector3 BezierDerivative(Vector3 control1, Vector3 control2, Vector3 end, real_t t)
        {
            return new Vector3(
                Mathf.BezierDerivative(X, control1.X, control2.X, end.X, t),
                Mathf.BezierDerivative(Y, control1.Y, control2.Y, end.Y, t),
                Mathf.BezierDerivative(Z, control1.Z, control2.Z, end.Z, t)
            );
        }

        /// <summary>
        /// Returns the normalized vector pointing from this vector to <paramref name="to"/>.
        /// </summary>
        /// <param name="to">The other vector to point towards.</param>
        /// <returns>The direction from this vector to <paramref name="to"/>.</returns>
        public readonly Vector3 DirectionTo(Vector3 to)
        {
            return new Vector3(to.X - X, to.Y - Y, to.Z - Z).Normalized();
        }

        /// <summary>
        /// Returns the squared distance between this vector and <paramref name="to"/>.
        /// This method runs faster than <see cref="DistanceTo"/>, so prefer it if
        /// you need to compare vectors or need the squared distance for some formula.
        /// </summary>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The squared distance between the two vectors.</returns>
        public readonly real_t DistanceSquaredTo(Vector3 to)
        {
            return (to - this).LengthSquared();
        }

        /// <summary>
        /// Returns the distance between this vector and <paramref name="to"/>.
        /// </summary>
        /// <seealso cref="DistanceSquaredTo(Vector3)"/>
        /// <param name="to">The other vector to use.</param>
        /// <returns>The distance between the two vectors.</returns>
        public readonly real_t DistanceTo(Vector3 to)
        {
            return (to - this).Length();
        }

        /// <summary>
        /// Returns the dot product of this vector and <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The dot product of the two vectors.</returns>
        public readonly real_t Dot(Vector3 with)
        {
            return (X * with.X) + (Y * with.Y) + (Z * with.Z);
        }

        /// <summary>
        /// Returns a new vector with all components rounded down (towards negative infinity).
        /// </summary>
        /// <returns>A vector with <see cref="Mathf.Floor(real_t)"/> called on each component.</returns>
        public readonly Vector3 Floor()
        {
            return new Vector3(Mathf.Floor(X), Mathf.Floor(Y), Mathf.Floor(Z));
        }

        /// <summary>
        /// Returns the inverse of this vector. This is the same as <c>new Vector3(1 / v.X, 1 / v.Y, 1 / v.Z)</c>.
        /// </summary>
        /// <returns>The inverse of this vector.</returns>
        public readonly Vector3 Inverse()
        {
            return new Vector3(1 / X, 1 / Y, 1 / Z);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector is finite, by calling
        /// <see cref="Mathf.IsFinite(real_t)"/> on each component.
        /// </summary>
        /// <returns>Whether this vector is finite or not.</returns>
        public readonly bool IsFinite()
        {
            return Mathf.IsFinite(X) && Mathf.IsFinite(Y) && Mathf.IsFinite(Z);
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
            real_t x2 = X * X;
            real_t y2 = Y * Y;
            real_t z2 = Z * Z;

            return Mathf.Sqrt(x2 + y2 + z2);
        }

        /// <summary>
        /// Returns the squared length (squared magnitude) of this vector.
        /// This method runs faster than <see cref="Length"/>, so prefer it if
        /// you need to compare vectors or need the squared length for some formula.
        /// </summary>
        /// <returns>The squared length of this vector.</returns>
        public readonly real_t LengthSquared()
        {
            real_t x2 = X * X;
            real_t y2 = Y * Y;
            real_t z2 = Z * Z;

            return x2 + y2 + z2;
        }

        /// <summary>
        /// Returns the result of the linear interpolation between
        /// this vector and <paramref name="to"/> by amount <paramref name="weight"/>.
        /// </summary>
        /// <param name="to">The destination vector for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting vector of the interpolation.</returns>
        public readonly Vector3 Lerp(Vector3 to, real_t weight)
        {
            return new Vector3
            (
                Mathf.Lerp(X, to.X, weight),
                Mathf.Lerp(Y, to.Y, weight),
                Mathf.Lerp(Z, to.Z, weight)
            );
        }

        /// <summary>
        /// Returns the vector with a maximum length by limiting its length to <paramref name="length"/>.
        /// </summary>
        /// <param name="length">The length to limit to.</param>
        /// <returns>The vector with its length limited.</returns>
        public readonly Vector3 LimitLength(real_t length = 1.0f)
        {
            Vector3 v = this;
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
        /// Equivalent to <c>new Vector3(Mathf.Max(X, with.X), Mathf.Max(Y, with.Y), Mathf.Max(Z, with.Z))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector3 Max(Vector3 with)
        {
            return new Vector3
            (
                Mathf.Max(X, with.X),
                Mathf.Max(Y, with.Y),
                Mathf.Max(Z, with.Z)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise maximum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector3(Mathf.Max(X, with), Mathf.Max(Y, with), Mathf.Max(Z, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting maximum vector.</returns>
        public readonly Vector3 Max(real_t with)
        {
            return new Vector3
            (
                Mathf.Max(X, with),
                Mathf.Max(Y, with),
                Mathf.Max(Z, with)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise minimum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector3(Mathf.Min(X, with.X), Mathf.Min(Y, with.Y), Mathf.Min(Z, with.Z))</c>.
        /// </summary>
        /// <param name="with">The other vector to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector3 Min(Vector3 with)
        {
            return new Vector3
            (
                Mathf.Min(X, with.X),
                Mathf.Min(Y, with.Y),
                Mathf.Min(Z, with.Z)
            );
        }

        /// <summary>
        /// Returns the result of the component-wise minimum between
        /// this vector and <paramref name="with"/>.
        /// Equivalent to <c>new Vector3(Mathf.Min(X, with), Mathf.Min(Y, with), Mathf.Min(Z, with))</c>.
        /// </summary>
        /// <param name="with">The other value to use.</param>
        /// <returns>The resulting minimum vector.</returns>
        public readonly Vector3 Min(real_t with)
        {
            return new Vector3
            (
                Mathf.Min(X, with),
                Mathf.Min(Y, with),
                Mathf.Min(Z, with)
            );
        }

        /// <summary>
        /// Returns the axis of the vector's highest value. See <see cref="Axis"/>.
        /// If all components are equal, this method returns <see cref="Axis.X"/>.
        /// </summary>
        /// <returns>The index of the highest axis.</returns>
        public readonly Axis MaxAxisIndex()
        {
            return X < Y ? (Y < Z ? Axis.Z : Axis.Y) : (X < Z ? Axis.Z : Axis.X);
        }

        /// <summary>
        /// Returns the axis of the vector's lowest value. See <see cref="Axis"/>.
        /// If all components are equal, this method returns <see cref="Axis.Z"/>.
        /// </summary>
        /// <returns>The index of the lowest axis.</returns>
        public readonly Axis MinAxisIndex()
        {
            return X < Y ? (X < Z ? Axis.X : Axis.Z) : (Y < Z ? Axis.Y : Axis.Z);
        }

        /// <summary>
        /// Moves this vector toward <paramref name="to"/> by the fixed <paramref name="delta"/> amount.
        /// </summary>
        /// <param name="to">The vector to move towards.</param>
        /// <param name="delta">The amount to move towards by.</param>
        /// <returns>The resulting vector.</returns>
        public readonly Vector3 MoveToward(Vector3 to, real_t delta)
        {
            Vector3 v = this;
            Vector3 vd = to - v;
            real_t len = vd.Length();
            if (len <= delta || len < Mathf.Epsilon)
                return to;

            return v + (vd / len * delta);
        }

        /// <summary>
        /// Returns the vector scaled to unit length. Equivalent to <c>v / v.Length()</c>.
        /// </summary>
        /// <returns>A normalized version of the vector.</returns>
        public readonly Vector3 Normalized()
        {
            Vector3 v = this;
            v.Normalize();
            return v;
        }

        /// <summary>
        /// Returns the outer product with <paramref name="with"/>.
        /// </summary>
        /// <param name="with">The other vector.</param>
        /// <returns>A <see cref="Basis"/> representing the outer product matrix.</returns>
        public readonly Basis Outer(Vector3 with)
        {
            return new Basis(
                X * with.X, X * with.Y, X * with.Z,
                Y * with.X, Y * with.Y, Y * with.Z,
                Z * with.X, Z * with.Y, Z * with.Z
            );
        }

        /// <summary>
        /// Returns a vector composed of the <see cref="Mathf.PosMod(real_t, real_t)"/> of this vector's components
        /// and <paramref name="mod"/>.
        /// </summary>
        /// <param name="mod">A value representing the divisor of the operation.</param>
        /// <returns>
        /// A vector with each component <see cref="Mathf.PosMod(real_t, real_t)"/> by <paramref name="mod"/>.
        /// </returns>
        public readonly Vector3 PosMod(real_t mod)
        {
            Vector3 v;
            v.X = Mathf.PosMod(X, mod);
            v.Y = Mathf.PosMod(Y, mod);
            v.Z = Mathf.PosMod(Z, mod);
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
        public readonly Vector3 PosMod(Vector3 modv)
        {
            Vector3 v;
            v.X = Mathf.PosMod(X, modv.X);
            v.Y = Mathf.PosMod(Y, modv.Y);
            v.Z = Mathf.PosMod(Z, modv.Z);
            return v;
        }

        /// <summary>
        /// Returns a new vector resulting from projecting this vector onto the given vector <paramref name="onNormal"/>.
        /// The resulting new vector is parallel to <paramref name="onNormal"/>.
        /// See also <see cref="Slide(Vector3)"/>.
        /// Note: If the vector <paramref name="onNormal"/> is a zero vector, the components of the resulting new vector will be <see cref="real_t.NaN"/>.
        /// </summary>
        /// <param name="onNormal">The vector to project onto.</param>
        /// <returns>The projected vector.</returns>
        public readonly Vector3 Project(Vector3 onNormal)
        {
            return onNormal * (Dot(onNormal) / onNormal.LengthSquared());
        }

        /// <summary>
        /// Returns this vector reflected from a plane defined by the given <paramref name="normal"/>.
        /// </summary>
        /// <param name="normal">The normal vector defining the plane to reflect from. Must be normalized.</param>
        /// <returns>The reflected vector.</returns>
        public readonly Vector3 Reflect(Vector3 normal)
        {
#if DEBUG
            if (!normal.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized.", nameof(normal));
            }
#endif
            return (2.0f * Dot(normal) * normal) - this;
        }

        /// <summary>
        /// Rotates this vector around a given <paramref name="axis"/> vector by <paramref name="angle"/> (in radians).
        /// The <paramref name="axis"/> vector must be a normalized vector.
        /// </summary>
        /// <param name="axis">The vector to rotate around. Must be normalized.</param>
        /// <param name="angle">The angle to rotate by, in radians.</param>
        /// <returns>The rotated vector.</returns>
        public readonly Vector3 Rotated(Vector3 axis, real_t angle)
        {
#if DEBUG
            if (!axis.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized.", nameof(axis));
            }
#endif
            return new Basis(axis, angle) * this;
        }

        /// <summary>
        /// Returns this vector with all components rounded to the nearest integer,
        /// with halfway cases rounded towards the nearest multiple of two.
        /// </summary>
        /// <returns>The rounded vector.</returns>
        public readonly Vector3 Round()
        {
            return new Vector3(Mathf.Round(X), Mathf.Round(Y), Mathf.Round(Z));
        }

        /// <summary>
        /// Returns a vector with each component set to one or negative one, depending
        /// on the signs of this vector's components, or zero if the component is zero,
        /// by calling <see cref="Mathf.Sign(real_t)"/> on each component.
        /// </summary>
        /// <returns>A vector with all components as either <c>1</c>, <c>-1</c>, or <c>0</c>.</returns>
        public readonly Vector3 Sign()
        {
            Vector3 v;
            v.X = Mathf.Sign(X);
            v.Y = Mathf.Sign(Y);
            v.Z = Mathf.Sign(Z);
            return v;
        }

        /// <summary>
        /// Returns the signed angle to the given vector, in radians.
        /// The sign of the angle is positive in a counter-clockwise
        /// direction and negative in a clockwise direction when viewed
        /// from the side specified by the <paramref name="axis"/>.
        /// </summary>
        /// <param name="to">The other vector to compare this vector to.</param>
        /// <param name="axis">The reference axis to use for the angle sign.</param>
        /// <returns>The signed angle between the two vectors, in radians.</returns>
        public readonly real_t SignedAngleTo(Vector3 to, Vector3 axis)
        {
            Vector3 crossTo = Cross(to);
            real_t unsignedAngle = Mathf.Atan2(crossTo.Length(), Dot(to));
            real_t sign = crossTo.Dot(axis);
            return (sign < 0) ? -unsignedAngle : unsignedAngle;
        }

        /// <summary>
        /// Returns the result of the spherical linear interpolation between
        /// this vector and <paramref name="to"/> by amount <paramref name="weight"/>.
        ///
        /// This method also handles interpolating the lengths if the input vectors
        /// have different lengths. For the special case of one or both input vectors
        /// having zero length, this method behaves like <see cref="Lerp(Vector3, real_t)"/>.
        /// </summary>
        /// <param name="to">The destination vector for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting vector of the interpolation.</returns>
        public readonly Vector3 Slerp(Vector3 to, real_t weight)
        {
            real_t startLengthSquared = LengthSquared();
            real_t endLengthSquared = to.LengthSquared();
            if (startLengthSquared == 0.0 || endLengthSquared == 0.0)
            {
                // Zero length vectors have no angle, so the best we can do is either lerp or throw an error.
                return Lerp(to, weight);
            }
            Vector3 axis = Cross(to);
            real_t axisLengthSquared = axis.LengthSquared();
            if (axisLengthSquared == 0.0)
            {
                // Colinear vectors have no rotation axis or angle between them, so the best we can do is lerp.
                return Lerp(to, weight);
            }
            axis /= Mathf.Sqrt(axisLengthSquared);
            real_t startLength = Mathf.Sqrt(startLengthSquared);
            real_t resultLength = Mathf.Lerp(startLength, Mathf.Sqrt(endLengthSquared), weight);
            real_t angle = AngleTo(to);
            return Rotated(axis, angle * weight) * (resultLength / startLength);
        }

        /// <summary>
        /// Returns a new vector resulting from sliding this vector along a plane with normal <paramref name="normal"/>.
        /// The resulting new vector is perpendicular to <paramref name="normal"/>, and is equivalent to this vector minus its projection on <paramref name="normal"/>.
        /// See also <see cref="Project(Vector3)"/>.
        /// Note: The vector <paramref name="normal"/> must be normalized. See also <see cref="Normalized()"/>.
        /// </summary>
        /// <param name="normal">The normal vector of the plane to slide on.</param>
        /// <returns>The slid vector.</returns>
        public readonly Vector3 Slide(Vector3 normal)
        {
            return this - (normal * Dot(normal));
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the nearest multiple of the corresponding component in <paramref name="step"/>.
        /// This can also be used to round to an arbitrary number of decimals.
        /// </summary>
        /// <param name="step">A vector value representing the step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector3 Snapped(Vector3 step)
        {
            return new Vector3
            (
                Mathf.Snapped(X, step.X),
                Mathf.Snapped(Y, step.Y),
                Mathf.Snapped(Z, step.Z)
            );
        }

        /// <summary>
        /// Returns a new vector with each component snapped to the nearest multiple of <paramref name="step"/>.
        /// This can also be used to round to an arbitrary number of decimals.
        /// </summary>
        /// <param name="step">The step size to snap to.</param>
        /// <returns>The snapped vector.</returns>
        public readonly Vector3 Snapped(real_t step)
        {
            return new Vector3
            (
                Mathf.Snapped(X, step),
                Mathf.Snapped(Y, step),
                Mathf.Snapped(Z, step)
            );
        }

        /// <summary>
        /// Returns the octahedral-encoded (oct32) form of this Vector3 as a Vector2. Since a Vector2 occupies 1/3 less memory compared to Vector3,
        /// this form of compression can be used to pass greater amounts of normalized Vector3s without increasing storage or memory requirements.
        /// See also <see cref="Normalized()"/>, <see cref="OctahedronDecode(Vector2)"/>.
        /// Note: OctahedronEncode can only be used for normalized vectors. OctahedronEncode does not check whether this Vector3 is normalized,
        /// and will return a value that does not decompress to the original value if the Vector3 is not normalized.
		/// Note: Octahedral compression is lossy, although visual differences are rarely perceptible in real world scenarios.
        /// </summary>
        /// <returns>The encoded Vector2.</returns>
        public readonly Vector2 OctahedronEncode()
        {
            Vector3 n = this;
            n /= Mathf.Abs(n.X) + Mathf.Abs(n.Y) + Mathf.Abs(n.Z);
            Vector2 o;
            if (n.Z >= 0.0f)
            {
                o.X = n.X;
                o.Y = n.Y;
            }
            else
            {
                o.X = (1.0f - Mathf.Abs(n.Y)) * (n.X >= 0.0f ? 1.0f : -1.0f);
                o.Y = (1.0f - Mathf.Abs(n.X)) * (n.Y >= 0.0f ? 1.0f : -1.0f);
            }
            o.X = o.X * 0.5f + 0.5f;
            o.Y = o.Y * 0.5f + 0.5f;
            return o;
        }

        /// <summary>
        /// Returns the Vector3 from an octahedral-compressed form created using <see cref="OctahedronEncode()"/> (stored as a Vector2).
        /// </summary>
        /// <param name="oct">Encoded Vector2</param>
        /// <returns>The decoded normalized Vector3.</returns>
        public static Vector3 OctahedronDecode(Vector2 oct)
        {
            var f = new Vector2(oct.X * 2.0f - 1.0f, oct.Y * 2.0f - 1.0f);
            var n = new Vector3(f.X, f.Y, 1.0f - Mathf.Abs(f.X) - Mathf.Abs(f.Y));
            real_t t = Mathf.Clamp(-n.Z, 0.0f, 1.0f);
            n.X += n.X >= 0 ? -t : t;
            n.Y += n.Y >= 0 ? -t : t;
            return n.Normalized();
        }

        // Constants
        private static readonly Vector3 _zero = new Vector3(0, 0, 0);
        private static readonly Vector3 _one = new Vector3(1, 1, 1);
        private static readonly Vector3 _inf = new Vector3(Mathf.Inf, Mathf.Inf, Mathf.Inf);

        private static readonly Vector3 _up = new Vector3(0, 1, 0);
        private static readonly Vector3 _down = new Vector3(0, -1, 0);
        private static readonly Vector3 _right = new Vector3(1, 0, 0);
        private static readonly Vector3 _left = new Vector3(-1, 0, 0);
        private static readonly Vector3 _forward = new Vector3(0, 0, -1);
        private static readonly Vector3 _back = new Vector3(0, 0, 1);

        private static readonly Vector3 _modelLeft = new Vector3(1, 0, 0);
        private static readonly Vector3 _modelRight = new Vector3(-1, 0, 0);
        private static readonly Vector3 _modelTop = new Vector3(0, 1, 0);
        private static readonly Vector3 _modelBottom = new Vector3(0, -1, 0);
        private static readonly Vector3 _modelFront = new Vector3(0, 0, 1);
        private static readonly Vector3 _modelRear = new Vector3(0, 0, -1);

        /// <summary>
        /// Zero vector, a vector with all components set to <c>0</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(0, 0, 0)</c>.</value>
        public static Vector3 Zero { get { return _zero; } }
        /// <summary>
        /// One vector, a vector with all components set to <c>1</c>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(1, 1, 1)</c>.</value>
        public static Vector3 One { get { return _one; } }
        /// <summary>
        /// Infinity vector, a vector with all components set to <see cref="Mathf.Inf"/>.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(Mathf.Inf, Mathf.Inf, Mathf.Inf)</c>.</value>
        public static Vector3 Inf { get { return _inf; } }

        /// <summary>
        /// Up unit vector.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(0, 1, 0)</c>.</value>
        public static Vector3 Up { get { return _up; } }
        /// <summary>
        /// Down unit vector.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(0, -1, 0)</c>.</value>
        public static Vector3 Down { get { return _down; } }
        /// <summary>
        /// Right unit vector. Represents the local direction of right,
        /// and the global direction of east.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(1, 0, 0)</c>.</value>
        public static Vector3 Right { get { return _right; } }
        /// <summary>
        /// Left unit vector. Represents the local direction of left,
        /// and the global direction of west.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(-1, 0, 0)</c>.</value>
        public static Vector3 Left { get { return _left; } }
        /// <summary>
        /// Forward unit vector. Represents the local direction of forward,
        /// and the global direction of north.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(0, 0, -1)</c>.</value>
        public static Vector3 Forward { get { return _forward; } }
        /// <summary>
        /// Back unit vector. Represents the local direction of back,
        /// and the global direction of south.
        /// </summary>
        /// <value>Equivalent to <c>new Vector3(0, 0, 1)</c>.</value>
        public static Vector3 Back { get { return _back; } }

        /// <summary>
        /// Unit vector pointing towards the left side of imported 3D assets.
        /// </summary>
        public static Vector3 ModelLeft { get { return _modelLeft; } }
        /// <summary>
        /// Unit vector pointing towards the right side of imported 3D assets.
        /// </summary>
        public static Vector3 ModelRight { get { return _modelRight; } }
        /// <summary>
        /// Unit vector pointing towards the top side (up) of imported 3D assets.
        /// </summary>
        public static Vector3 ModelTop { get { return _modelTop; } }
        /// <summary>
        /// Unit vector pointing towards the bottom side (down) of imported 3D assets.
        /// </summary>
        public static Vector3 ModelBottom { get { return _modelBottom; } }
        /// <summary>
        /// Unit vector pointing towards the front side (facing forward) of imported 3D assets.
        /// </summary>
        public static Vector3 ModelFront { get { return _modelFront; } }
        /// <summary>
        /// Unit vector pointing towards the rear side (back) of imported 3D assets.
        /// </summary>
        public static Vector3 ModelRear { get { return _modelRear; } }

        /// <summary>
        /// Constructs a new <see cref="Vector3"/> with the given components.
        /// </summary>
        /// <param name="x">The vector's X component.</param>
        /// <param name="y">The vector's Y component.</param>
        /// <param name="z">The vector's Z component.</param>
        public Vector3(real_t x, real_t y, real_t z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        /// <summary>
        /// Adds each component of the <see cref="Vector3"/>
        /// with the components of the given <see cref="Vector3"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The added vector.</returns>
        public static Vector3 operator +(Vector3 left, Vector3 right)
        {
            left.X += right.X;
            left.Y += right.Y;
            left.Z += right.Z;
            return left;
        }

        /// <summary>
        /// Subtracts each component of the <see cref="Vector3"/>
        /// by the components of the given <see cref="Vector3"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The subtracted vector.</returns>
        public static Vector3 operator -(Vector3 left, Vector3 right)
        {
            left.X -= right.X;
            left.Y -= right.Y;
            left.Z -= right.Z;
            return left;
        }

        /// <summary>
        /// Returns the negative value of the <see cref="Vector3"/>.
        /// This is the same as writing <c>new Vector3(-v.X, -v.Y, -v.Z)</c>.
        /// This operation flips the direction of the vector while
        /// keeping the same magnitude.
        /// With floats, the number zero can be either positive or negative.
        /// </summary>
        /// <param name="vec">The vector to negate/flip.</param>
        /// <returns>The negated/flipped vector.</returns>
        public static Vector3 operator -(Vector3 vec)
        {
            vec.X = -vec.X;
            vec.Y = -vec.Y;
            vec.Z = -vec.Z;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector3"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="vec">The vector to multiply.</param>
        /// <param name="scale">The scale to multiply by.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector3 operator *(Vector3 vec, real_t scale)
        {
            vec.X *= scale;
            vec.Y *= scale;
            vec.Z *= scale;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector3"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="scale">The scale to multiply by.</param>
        /// <param name="vec">The vector to multiply.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector3 operator *(real_t scale, Vector3 vec)
        {
            vec.X *= scale;
            vec.Y *= scale;
            vec.Z *= scale;
            return vec;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Vector3"/>
        /// by the components of the given <see cref="Vector3"/>.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>The multiplied vector.</returns>
        public static Vector3 operator *(Vector3 left, Vector3 right)
        {
            left.X *= right.X;
            left.Y *= right.Y;
            left.Z *= right.Z;
            return left;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector3"/>
        /// by the given <see cref="real_t"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The divided vector.</returns>
        public static Vector3 operator /(Vector3 vec, real_t divisor)
        {
            vec.X /= divisor;
            vec.Y /= divisor;
            vec.Z /= divisor;
            return vec;
        }

        /// <summary>
        /// Divides each component of the <see cref="Vector3"/>
        /// by the components of the given <see cref="Vector3"/>.
        /// </summary>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The divided vector.</returns>
        public static Vector3 operator /(Vector3 vec, Vector3 divisorv)
        {
            vec.X /= divisorv.X;
            vec.Y /= divisorv.Y;
            vec.Z /= divisorv.Z;
            return vec;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector3"/>
        /// with the components of the given <see cref="real_t"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="PosMod(real_t)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector3(10, -20, 30) % 7); // Prints "(3, -6, 2)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisor">The divisor value.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector3 operator %(Vector3 vec, real_t divisor)
        {
            vec.X %= divisor;
            vec.Y %= divisor;
            vec.Z %= divisor;
            return vec;
        }

        /// <summary>
        /// Gets the remainder of each component of the <see cref="Vector3"/>
        /// with the components of the given <see cref="Vector3"/>.
        /// This operation uses truncated division, which is often not desired
        /// as it does not work well with negative numbers.
        /// Consider using <see cref="PosMod(Vector3)"/> instead
        /// if you want to handle negative numbers.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(new Vector3(10, -20, 30) % new Vector3(7, 8, 9)); // Prints "(3, -4, 3)"
        /// </code>
        /// </example>
        /// <param name="vec">The dividend vector.</param>
        /// <param name="divisorv">The divisor vector.</param>
        /// <returns>The remainder vector.</returns>
        public static Vector3 operator %(Vector3 vec, Vector3 divisorv)
        {
            vec.X %= divisorv.X;
            vec.Y %= divisorv.Y;
            vec.Z %= divisorv.Z;
            return vec;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the vectors are exactly equal.</returns>
        public static bool operator ==(Vector3 left, Vector3 right)
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
        public static bool operator !=(Vector3 left, Vector3 right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Compares two <see cref="Vector3"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors, and then with the Z values.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than the right.</returns>
        public static bool operator <(Vector3 left, Vector3 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    return left.Z < right.Z;
                }
                return left.Y < right.Y;
            }
            return left.X < right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector3"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors, and then with the Z values.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than the right.</returns>
        public static bool operator >(Vector3 left, Vector3 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    return left.Z > right.Z;
                }
                return left.Y > right.Y;
            }
            return left.X > right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector3"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is less than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors, and then with the Z values.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is less than or equal to the right.</returns>
        public static bool operator <=(Vector3 left, Vector3 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    return left.Z <= right.Z;
                }
                return left.Y < right.Y;
            }
            return left.X < right.X;
        }

        /// <summary>
        /// Compares two <see cref="Vector3"/> vectors by first checking if
        /// the X value of the <paramref name="left"/> vector is greater than
        /// or equal to the X value of the <paramref name="right"/> vector.
        /// If the X values are exactly equal, then it repeats this check
        /// with the Y values of the two vectors, and then with the Z values.
        /// This operator is useful for sorting vectors.
        /// </summary>
        /// <param name="left">The left vector.</param>
        /// <param name="right">The right vector.</param>
        /// <returns>Whether or not the left is greater than or equal to the right.</returns>
        public static bool operator >=(Vector3 left, Vector3 right)
        {
            if (left.X == right.X)
            {
                if (left.Y == right.Y)
                {
                    return left.Z >= right.Z;
                }
                return left.Y > right.Y;
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
            return obj is Vector3 other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the vectors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="other">The other vector.</param>
        /// <returns>Whether or not the vectors are exactly equal.</returns>
        public readonly bool Equals(Vector3 other)
        {
            return X == other.X && Y == other.Y && Z == other.Z;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this vector and <paramref name="other"/> are approximately equal,
        /// by running <see cref="Mathf.IsEqualApprox(real_t, real_t)"/> on each component.
        /// </summary>
        /// <param name="other">The other vector to compare.</param>
        /// <returns>Whether or not the vectors are approximately equal.</returns>
        public readonly bool IsEqualApprox(Vector3 other)
        {
            return Mathf.IsEqualApprox(X, other.X) && Mathf.IsEqualApprox(Y, other.Y) && Mathf.IsEqualApprox(Z, other.Z);
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
            return Mathf.IsZeroApprox(X) && Mathf.IsZeroApprox(Y) && Mathf.IsZeroApprox(Z);
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Vector3"/>.
        /// </summary>
        /// <returns>A hash code for this vector.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(X, Y, Z);
        }

        /// <summary>
        /// Converts this <see cref="Vector3"/> to a string.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Vector3"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this vector.</returns>
        public readonly string ToString(string? format)
        {
            return $"({X.ToString(format, CultureInfo.InvariantCulture)}, {Y.ToString(format, CultureInfo.InvariantCulture)}, {Z.ToString(format, CultureInfo.InvariantCulture)})";
        }
    }
}
