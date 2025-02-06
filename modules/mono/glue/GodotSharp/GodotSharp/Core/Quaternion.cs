using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;

#nullable enable

namespace Godot
{
    /// <summary>
    /// A unit quaternion used for representing 3D rotations.
    /// Quaternions need to be normalized to be used for rotation.
    ///
    /// It is similar to <see cref="Basis"/>, which implements matrix
    /// representation of rotations, and can be parametrized using both
    /// an axis-angle pair or Euler angles. Basis stores rotation, scale,
    /// and shearing, while Quaternion only stores rotation.
    ///
    /// Due to its compactness and the way it is stored in memory, certain
    /// operations (obtaining axis-angle and performing SLERP, in particular)
    /// are more efficient and robust against floating-point errors.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Quaternion : IEquatable<Quaternion>
    {
        /// <summary>
        /// X component of the quaternion (imaginary <c>i</c> axis part).
        /// Quaternion components should usually not be manipulated directly.
        /// </summary>
        public real_t X;

        /// <summary>
        /// Y component of the quaternion (imaginary <c>j</c> axis part).
        /// Quaternion components should usually not be manipulated directly.
        /// </summary>
        public real_t Y;

        /// <summary>
        /// Z component of the quaternion (imaginary <c>k</c> axis part).
        /// Quaternion components should usually not be manipulated directly.
        /// </summary>
        public real_t Z;

        /// <summary>
        /// W component of the quaternion (real part).
        /// Quaternion components should usually not be manipulated directly.
        /// </summary>
        public real_t W;

        /// <summary>
        /// Access quaternion components using their index.
        /// </summary>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="index"/> is not 0, 1, 2 or 3.
        /// </exception>
        /// <value>
        /// <c>[0]</c> is equivalent to <see cref="X"/>,
        /// <c>[1]</c> is equivalent to <see cref="Y"/>,
        /// <c>[2]</c> is equivalent to <see cref="Z"/>,
        /// <c>[3]</c> is equivalent to <see cref="W"/>.
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
                    case 3:
                        return W;
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
                        break;
                    case 1:
                        Y = value;
                        break;
                    case 2:
                        Z = value;
                        break;
                    case 3:
                        W = value;
                        break;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
        }

        /// <summary>
        /// Returns the angle between this quaternion and <paramref name="to"/>.
        /// This is the magnitude of the angle you would need to rotate
        /// by to get from one to the other.
        ///
        /// Note: This method has an abnormally high amount
        /// of floating-point error, so methods such as
        /// <see cref="Mathf.IsZeroApprox(real_t)"/> will not work reliably.
        /// </summary>
        /// <param name="to">The other quaternion.</param>
        /// <returns>The angle between the quaternions.</returns>
        public readonly real_t AngleTo(Quaternion to)
        {
            real_t dot = Dot(to);
            return Mathf.Acos(Mathf.Clamp(dot * dot * 2 - 1, -1, 1));
        }

        /// <summary>
        /// Performs a spherical cubic interpolation between quaternions <paramref name="preA"/>, this quaternion,
        /// <paramref name="b"/>, and <paramref name="postB"/>, by the given amount <paramref name="weight"/>.
        /// </summary>
        /// <param name="b">The destination quaternion.</param>
        /// <param name="preA">A quaternion before this quaternion.</param>
        /// <param name="postB">A quaternion after <paramref name="b"/>.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The interpolated quaternion.</returns>
        public readonly Quaternion SphericalCubicInterpolate(Quaternion b, Quaternion preA, Quaternion postB, real_t weight)
        {
#if DEBUG
            if (!IsNormalized())
            {
                throw new InvalidOperationException("Quaternion is not normalized");
            }
            if (!b.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized", nameof(b));
            }
#endif

            // Align flip phases.
            Quaternion fromQ = new Basis(this).GetRotationQuaternion();
            Quaternion preQ = new Basis(preA).GetRotationQuaternion();
            Quaternion toQ = new Basis(b).GetRotationQuaternion();
            Quaternion postQ = new Basis(postB).GetRotationQuaternion();

            // Flip quaternions to shortest path if necessary.
            bool flip1 = Math.Sign(fromQ.Dot(preQ)) < 0;
            preQ = flip1 ? -preQ : preQ;
            bool flip2 = Math.Sign(fromQ.Dot(toQ)) < 0;
            toQ = flip2 ? -toQ : toQ;
            bool flip3 = flip2 ? toQ.Dot(postQ) <= 0 : Math.Sign(toQ.Dot(postQ)) < 0;
            postQ = flip3 ? -postQ : postQ;

            // Calc by Expmap in fromQ space.
            Quaternion lnFrom = new Quaternion(0, 0, 0, 0);
            Quaternion lnTo = (fromQ.Inverse() * toQ).Log();
            Quaternion lnPre = (fromQ.Inverse() * preQ).Log();
            Quaternion lnPost = (fromQ.Inverse() * postQ).Log();
            Quaternion ln = new Quaternion(
                Mathf.CubicInterpolate(lnFrom.X, lnTo.X, lnPre.X, lnPost.X, weight),
                Mathf.CubicInterpolate(lnFrom.Y, lnTo.Y, lnPre.Y, lnPost.Y, weight),
                Mathf.CubicInterpolate(lnFrom.Z, lnTo.Z, lnPre.Z, lnPost.Z, weight),
                0);
            Quaternion q1 = fromQ * ln.Exp();

            // Calc by Expmap in toQ space.
            lnFrom = (toQ.Inverse() * fromQ).Log();
            lnTo = new Quaternion(0, 0, 0, 0);
            lnPre = (toQ.Inverse() * preQ).Log();
            lnPost = (toQ.Inverse() * postQ).Log();
            ln = new Quaternion(
                Mathf.CubicInterpolate(lnFrom.X, lnTo.X, lnPre.X, lnPost.X, weight),
                Mathf.CubicInterpolate(lnFrom.Y, lnTo.Y, lnPre.Y, lnPost.Y, weight),
                Mathf.CubicInterpolate(lnFrom.Z, lnTo.Z, lnPre.Z, lnPost.Z, weight),
                0);
            Quaternion q2 = toQ * ln.Exp();

            // To cancel error made by Expmap ambiguity, do blending.
            return q1.Slerp(q2, weight);
        }

        /// <summary>
        /// Performs a spherical cubic interpolation between quaternions <paramref name="preA"/>, this quaternion,
        /// <paramref name="b"/>, and <paramref name="postB"/>, by the given amount <paramref name="weight"/>.
        /// It can perform smoother interpolation than <see cref="SphericalCubicInterpolate"/>
        /// by the time values.
        /// </summary>
        /// <param name="b">The destination quaternion.</param>
        /// <param name="preA">A quaternion before this quaternion.</param>
        /// <param name="postB">A quaternion after <paramref name="b"/>.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <param name="bT"></param>
        /// <param name="preAT"></param>
        /// <param name="postBT"></param>
        /// <returns>The interpolated quaternion.</returns>
        public readonly Quaternion SphericalCubicInterpolateInTime(Quaternion b, Quaternion preA, Quaternion postB, real_t weight, real_t bT, real_t preAT, real_t postBT)
        {
#if DEBUG
            if (!IsNormalized())
            {
                throw new InvalidOperationException("Quaternion is not normalized");
            }
            if (!b.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized", nameof(b));
            }
#endif

            // Align flip phases.
            Quaternion fromQ = new Basis(this).GetRotationQuaternion();
            Quaternion preQ = new Basis(preA).GetRotationQuaternion();
            Quaternion toQ = new Basis(b).GetRotationQuaternion();
            Quaternion postQ = new Basis(postB).GetRotationQuaternion();

            // Flip quaternions to shortest path if necessary.
            bool flip1 = Math.Sign(fromQ.Dot(preQ)) < 0;
            preQ = flip1 ? -preQ : preQ;
            bool flip2 = Math.Sign(fromQ.Dot(toQ)) < 0;
            toQ = flip2 ? -toQ : toQ;
            bool flip3 = flip2 ? toQ.Dot(postQ) <= 0 : Math.Sign(toQ.Dot(postQ)) < 0;
            postQ = flip3 ? -postQ : postQ;

            // Calc by Expmap in fromQ space.
            Quaternion lnFrom = new Quaternion(0, 0, 0, 0);
            Quaternion lnTo = (fromQ.Inverse() * toQ).Log();
            Quaternion lnPre = (fromQ.Inverse() * preQ).Log();
            Quaternion lnPost = (fromQ.Inverse() * postQ).Log();
            Quaternion ln = new Quaternion(
                Mathf.CubicInterpolateInTime(lnFrom.X, lnTo.X, lnPre.X, lnPost.X, weight, bT, preAT, postBT),
                Mathf.CubicInterpolateInTime(lnFrom.Y, lnTo.Y, lnPre.Y, lnPost.Y, weight, bT, preAT, postBT),
                Mathf.CubicInterpolateInTime(lnFrom.Z, lnTo.Z, lnPre.Z, lnPost.Z, weight, bT, preAT, postBT),
                0);
            Quaternion q1 = fromQ * ln.Exp();

            // Calc by Expmap in toQ space.
            lnFrom = (toQ.Inverse() * fromQ).Log();
            lnTo = new Quaternion(0, 0, 0, 0);
            lnPre = (toQ.Inverse() * preQ).Log();
            lnPost = (toQ.Inverse() * postQ).Log();
            ln = new Quaternion(
                Mathf.CubicInterpolateInTime(lnFrom.X, lnTo.X, lnPre.X, lnPost.X, weight, bT, preAT, postBT),
                Mathf.CubicInterpolateInTime(lnFrom.Y, lnTo.Y, lnPre.Y, lnPost.Y, weight, bT, preAT, postBT),
                Mathf.CubicInterpolateInTime(lnFrom.Z, lnTo.Z, lnPre.Z, lnPost.Z, weight, bT, preAT, postBT),
                0);
            Quaternion q2 = toQ * ln.Exp();

            // To cancel error made by Expmap ambiguity, do blending.
            return q1.Slerp(q2, weight);
        }

        /// <summary>
        /// Returns the dot product of two quaternions.
        /// </summary>
        /// <param name="b">The other quaternion.</param>
        /// <returns>The dot product.</returns>
        public readonly real_t Dot(Quaternion b)
        {
            return (X * b.X) + (Y * b.Y) + (Z * b.Z) + (W * b.W);
        }

        public readonly Quaternion Exp()
        {
            Vector3 v = new Vector3(X, Y, Z);
            real_t theta = v.Length();
            v = v.Normalized();
            if (theta < Mathf.Epsilon || !v.IsNormalized())
            {
                return new Quaternion(0, 0, 0, 1);
            }
            return new Quaternion(v, theta);
        }

        public readonly real_t GetAngle()
        {
            return 2 * Mathf.Acos(W);
        }

        public readonly Vector3 GetAxis()
        {
            if (Mathf.Abs(W) > 1 - Mathf.Epsilon)
            {
                return new Vector3(X, Y, Z);
            }

            real_t r = 1 / Mathf.Sqrt(1 - W * W);
            return new Vector3(X * r, Y * r, Z * r);
        }

        /// <summary>
        /// Returns Euler angles (in the YXZ convention: when decomposing,
        /// first Z, then X, and Y last) corresponding to the rotation
        /// represented by the unit quaternion. Returned vector contains
        /// the rotation angles in the format (X angle, Y angle, Z angle).
        /// </summary>
        /// <returns>The Euler angle representation of this quaternion.</returns>
        public readonly Vector3 GetEuler(EulerOrder order = EulerOrder.Yxz)
        {
#if DEBUG
            if (!IsNormalized())
            {
                throw new InvalidOperationException("Quaternion is not normalized.");
            }
#endif
            var basis = new Basis(this);
            return basis.GetEuler(order);
        }

        /// <summary>
        /// Returns the inverse of the quaternion.
        /// </summary>
        /// <returns>The inverse quaternion.</returns>
        public readonly Quaternion Inverse()
        {
#if DEBUG
            if (!IsNormalized())
            {
                throw new InvalidOperationException("Quaternion is not normalized.");
            }
#endif
            return new Quaternion(-X, -Y, -Z, W);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this quaternion is finite, by calling
        /// <see cref="Mathf.IsFinite(real_t)"/> on each component.
        /// </summary>
        /// <returns>Whether this vector is finite or not.</returns>
        public readonly bool IsFinite()
        {
            return Mathf.IsFinite(X) && Mathf.IsFinite(Y) && Mathf.IsFinite(Z) && Mathf.IsFinite(W);
        }

        /// <summary>
        /// Returns whether the quaternion is normalized or not.
        /// </summary>
        /// <returns>A <see langword="bool"/> for whether the quaternion is normalized or not.</returns>
        public readonly bool IsNormalized()
        {
            return Mathf.Abs(LengthSquared() - 1) <= Mathf.Epsilon;
        }

        public readonly Quaternion Log()
        {
            Vector3 v = GetAxis() * GetAngle();
            return new Quaternion(v.X, v.Y, v.Z, 0);
        }

        /// <summary>
        /// Returns the length (magnitude) of the quaternion.
        /// </summary>
        /// <seealso cref="LengthSquared"/>
        /// <value>Equivalent to <c>Mathf.Sqrt(LengthSquared)</c>.</value>
        public readonly real_t Length()
        {
            return Mathf.Sqrt(LengthSquared());
        }

        /// <summary>
        /// Returns the squared length (squared magnitude) of the quaternion.
        /// This method runs faster than <see cref="Length"/>, so prefer it if
        /// you need to compare quaternions or need the squared length for some formula.
        /// </summary>
        /// <value>Equivalent to <c>Dot(this)</c>.</value>
        public readonly real_t LengthSquared()
        {
            return Dot(this);
        }

        /// <summary>
        /// Returns a copy of the quaternion, normalized to unit length.
        /// </summary>
        /// <returns>The normalized quaternion.</returns>
        public readonly Quaternion Normalized()
        {
            return this / Length();
        }

        /// <summary>
        /// Returns the result of the spherical linear interpolation between
        /// this quaternion and <paramref name="to"/> by amount <paramref name="weight"/>.
        ///
        /// Note: Both quaternions must be normalized.
        /// </summary>
        /// <param name="to">The destination quaternion for interpolation. Must be normalized.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting quaternion of the interpolation.</returns>
        public readonly Quaternion Slerp(Quaternion to, real_t weight)
        {
#if DEBUG
            if (!IsNormalized())
            {
                throw new InvalidOperationException("Quaternion is not normalized.");
            }
            if (!to.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized.", nameof(to));
            }
#endif

            // Calculate cosine.
            real_t cosom = Dot(to);

            var to1 = new Quaternion();

            // Adjust signs if necessary.
            if (cosom < 0.0)
            {
                cosom = -cosom;
                to1 = -to;
            }
            else
            {
                to1 = to;
            }

            real_t sinom, scale0, scale1;

            // Calculate coefficients.
            if (1.0 - cosom > Mathf.Epsilon)
            {
                // Standard case (Slerp).
                real_t omega = Mathf.Acos(cosom);
                sinom = Mathf.Sin(omega);
                scale0 = Mathf.Sin((1.0f - weight) * omega) / sinom;
                scale1 = Mathf.Sin(weight * omega) / sinom;
            }
            else
            {
                // Quaternions are very close so we can do a linear interpolation.
                scale0 = 1.0f - weight;
                scale1 = weight;
            }

            // Calculate final values.
            return new Quaternion
            (
                (scale0 * X) + (scale1 * to1.X),
                (scale0 * Y) + (scale1 * to1.Y),
                (scale0 * Z) + (scale1 * to1.Z),
                (scale0 * W) + (scale1 * to1.W)
            );
        }

        /// <summary>
        /// Returns the result of the spherical linear interpolation between
        /// this quaternion and <paramref name="to"/> by amount <paramref name="weight"/>, but without
        /// checking if the rotation path is not bigger than 90 degrees.
        /// </summary>
        /// <param name="to">The destination quaternion for interpolation. Must be normalized.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting quaternion of the interpolation.</returns>
        public readonly Quaternion Slerpni(Quaternion to, real_t weight)
        {
#if DEBUG
            if (!IsNormalized())
            {
                throw new InvalidOperationException("Quaternion is not normalized");
            }
            if (!to.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized", nameof(to));
            }
#endif

            real_t dot = Dot(to);

            if (Mathf.Abs(dot) > 0.9999f)
            {
                return this;
            }

            real_t theta = Mathf.Acos(dot);
            real_t sinT = 1.0f / Mathf.Sin(theta);
            real_t newFactor = Mathf.Sin(weight * theta) * sinT;
            real_t invFactor = Mathf.Sin((1.0f - weight) * theta) * sinT;

            return new Quaternion
            (
                (invFactor * X) + (newFactor * to.X),
                (invFactor * Y) + (newFactor * to.Y),
                (invFactor * Z) + (newFactor * to.Z),
                (invFactor * W) + (newFactor * to.W)
            );
        }

        // Constants
        private static readonly Quaternion _identity = new Quaternion(0, 0, 0, 1);

        /// <summary>
        /// The identity quaternion, representing no rotation.
        /// Equivalent to an identity <see cref="Basis"/> matrix. If a vector is transformed by
        /// an identity quaternion, it will not change.
        /// This is used as a replacement for <c>Quaternion()</c> in GDScript.
        /// </summary>
        /// <value>Equivalent to <c>new Quaternion(0, 0, 0, 1)</c>.</value>
        public static Quaternion Identity { get { return _identity; } }

        /// <summary>
        /// Constructs a <see cref="Quaternion"/> set to <see langword="default"/>.
        /// </summary>
        /// <remarks>For the GDScript equivalent of <c>Quaternion()</c>, use <see cref="Identity"/> instead.</remarks>
        public Quaternion() => this = default;

        /// <summary>
        /// Constructs a <see cref="Quaternion"/> defined by the given values.
        /// </summary>
        /// <param name="x">X component of the quaternion (imaginary <c>i</c> axis part).</param>
        /// <param name="y">Y component of the quaternion (imaginary <c>j</c> axis part).</param>
        /// <param name="z">Z component of the quaternion (imaginary <c>k</c> axis part).</param>
        /// <param name="w">W component of the quaternion (real part).</param>
        public Quaternion(real_t x, real_t y, real_t z, real_t w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        /// <summary>
        /// Constructs a <see cref="Quaternion"/> from the given <see cref="Basis"/>.
        /// </summary>
        /// <param name="basis">The <see cref="Basis"/> to construct from.</param>
        public Quaternion(Basis basis)
        {
            this = basis.GetQuaternion();
        }

        /// <summary>
        /// Constructs a <see cref="Quaternion"/> that will rotate around the given axis
        /// by the specified angle. The axis must be a normalized vector.
        /// </summary>
        /// <param name="axis">The axis to rotate around. Must be normalized.</param>
        /// <param name="angle">The angle to rotate, in radians.</param>
        public Quaternion(Vector3 axis, real_t angle)
        {
#if DEBUG
            if (!axis.IsNormalized())
            {
                throw new ArgumentException("Argument is not normalized.", nameof(axis));
            }
#endif

            real_t d = axis.Length();

            if (d == 0f)
            {
                X = 0f;
                Y = 0f;
                Z = 0f;
                W = 0f;
            }
            else
            {
                (real_t sin, real_t cos) = Mathf.SinCos(angle * 0.5f);
                real_t s = sin / d;

                X = axis.X * s;
                Y = axis.Y * s;
                Z = axis.Z * s;
                W = cos;
            }
        }

        public Quaternion(Vector3 arcFrom, Vector3 arcTo)
        {
            Vector3 c = arcFrom.Cross(arcTo);
            real_t d = arcFrom.Dot(arcTo);

            if (d < -1.0f + Mathf.Epsilon)
            {
                X = 0f;
                Y = 1f;
                Z = 0f;
                W = 0f;
            }
            else
            {
                real_t s = Mathf.Sqrt((1.0f + d) * 2.0f);
                real_t rs = 1.0f / s;

                X = c.X * rs;
                Y = c.Y * rs;
                Z = c.Z * rs;
                W = s * 0.5f;
            }
        }

        /// <summary>
        /// Constructs a <see cref="Quaternion"/> that will perform a rotation specified by
        /// Euler angles (in the YXZ convention: when decomposing, first Z, then X, and Y last),
        /// given in the vector format as (X angle, Y angle, Z angle).
        /// </summary>
        /// <param name="eulerYXZ">Euler angles that the quaternion will be rotated by.</param>
        public static Quaternion FromEuler(Vector3 eulerYXZ)
        {
            real_t halfA1 = eulerYXZ.Y * 0.5f;
            real_t halfA2 = eulerYXZ.X * 0.5f;
            real_t halfA3 = eulerYXZ.Z * 0.5f;

            // R = Y(a1).X(a2).Z(a3) convention for Euler angles.
            // Conversion to quaternion as listed in https://ntrs.nasa.gov/archive/nasa/casi.ntrs.nasa.gov/19770024290.pdf (page A-6)
            // a3 is the angle of the first rotation, following the notation in this reference.

            (real_t sinA1, real_t cosA1) = Mathf.SinCos(halfA1);
            (real_t sinA2, real_t cosA2) = Mathf.SinCos(halfA2);
            (real_t sinA3, real_t cosA3) = Mathf.SinCos(halfA3);

            return new Quaternion(
                (sinA1 * cosA2 * sinA3) + (cosA1 * sinA2 * cosA3),
                (sinA1 * cosA2 * cosA3) - (cosA1 * sinA2 * sinA3),
                (cosA1 * cosA2 * sinA3) - (sinA1 * sinA2 * cosA3),
                (sinA1 * sinA2 * sinA3) + (cosA1 * cosA2 * cosA3)
            );
        }

        /// <summary>
        /// Composes these two quaternions by multiplying them together.
        /// This has the effect of rotating the second quaternion
        /// (the child) by the first quaternion (the parent).
        /// </summary>
        /// <param name="left">The parent quaternion.</param>
        /// <param name="right">The child quaternion.</param>
        /// <returns>The composed quaternion.</returns>
        public static Quaternion operator *(Quaternion left, Quaternion right)
        {
            return new Quaternion
            (
                (left.W * right.X) + (left.X * right.W) + (left.Y * right.Z) - (left.Z * right.Y),
                (left.W * right.Y) + (left.Y * right.W) + (left.Z * right.X) - (left.X * right.Z),
                (left.W * right.Z) + (left.Z * right.W) + (left.X * right.Y) - (left.Y * right.X),
                (left.W * right.W) - (left.X * right.X) - (left.Y * right.Y) - (left.Z * right.Z)
            );
        }

        /// <summary>
        /// Returns a Vector3 rotated (multiplied) by the quaternion.
        /// </summary>
        /// <param name="quaternion">The quaternion to rotate by.</param>
        /// <param name="vector">A Vector3 to transform.</param>
        /// <returns>The rotated Vector3.</returns>
        public static Vector3 operator *(Quaternion quaternion, Vector3 vector)
        {
#if DEBUG
            if (!quaternion.IsNormalized())
            {
                throw new InvalidOperationException("Quaternion is not normalized.");
            }
#endif
            var u = new Vector3(quaternion.X, quaternion.Y, quaternion.Z);
            Vector3 uv = u.Cross(vector);
            return vector + (((uv * quaternion.W) + u.Cross(uv)) * 2);
        }

        /// <summary>
        /// Returns a Vector3 rotated (multiplied) by the inverse quaternion.
        /// <c>vector * quaternion</c> is equivalent to <c>quaternion.Inverse() * vector</c>. See <see cref="Inverse"/>.
        /// </summary>
        /// <param name="vector">A Vector3 to inversely rotate.</param>
        /// <param name="quaternion">The quaternion to rotate by.</param>
        /// <returns>The inversely rotated Vector3.</returns>
        public static Vector3 operator *(Vector3 vector, Quaternion quaternion)
        {
            return quaternion.Inverse() * vector;
        }

        /// <summary>
        /// Adds each component of the left <see cref="Quaternion"/>
        /// to the right <see cref="Quaternion"/>. This operation is not
        /// meaningful on its own, but it can be used as a part of a
        /// larger expression, such as approximating an intermediate
        /// rotation between two nearby rotations.
        /// </summary>
        /// <param name="left">The left quaternion to add.</param>
        /// <param name="right">The right quaternion to add.</param>
        /// <returns>The added quaternion.</returns>
        public static Quaternion operator +(Quaternion left, Quaternion right)
        {
            return new Quaternion(left.X + right.X, left.Y + right.Y, left.Z + right.Z, left.W + right.W);
        }

        /// <summary>
        /// Subtracts each component of the left <see cref="Quaternion"/>
        /// by the right <see cref="Quaternion"/>. This operation is not
        /// meaningful on its own, but it can be used as a part of a
        /// larger expression.
        /// </summary>
        /// <param name="left">The left quaternion to subtract.</param>
        /// <param name="right">The right quaternion to subtract.</param>
        /// <returns>The subtracted quaternion.</returns>
        public static Quaternion operator -(Quaternion left, Quaternion right)
        {
            return new Quaternion(left.X - right.X, left.Y - right.Y, left.Z - right.Z, left.W - right.W);
        }

        /// <summary>
        /// Returns the negative value of the <see cref="Quaternion"/>.
        /// This is the same as writing
        /// <c>new Quaternion(-q.X, -q.Y, -q.Z, -q.W)</c>. This operation
        /// results in a quaternion that represents the same rotation.
        /// </summary>
        /// <param name="quat">The quaternion to negate.</param>
        /// <returns>The negated quaternion.</returns>
        public static Quaternion operator -(Quaternion quat)
        {
            return new Quaternion(-quat.X, -quat.Y, -quat.Z, -quat.W);
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Quaternion"/>
        /// by the given <see cref="real_t"/>. This operation is not
        /// meaningful on its own, but it can be used as a part of a
        /// larger expression.
        /// </summary>
        /// <param name="left">The quaternion to multiply.</param>
        /// <param name="right">The value to multiply by.</param>
        /// <returns>The multiplied quaternion.</returns>
        public static Quaternion operator *(Quaternion left, real_t right)
        {
            return new Quaternion(left.X * right, left.Y * right, left.Z * right, left.W * right);
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Quaternion"/>
        /// by the given <see cref="real_t"/>. This operation is not
        /// meaningful on its own, but it can be used as a part of a
        /// larger expression.
        /// </summary>
        /// <param name="left">The value to multiply by.</param>
        /// <param name="right">The quaternion to multiply.</param>
        /// <returns>The multiplied quaternion.</returns>
        public static Quaternion operator *(real_t left, Quaternion right)
        {
            return new Quaternion(right.X * left, right.Y * left, right.Z * left, right.W * left);
        }

        /// <summary>
        /// Divides each component of the <see cref="Quaternion"/>
        /// by the given <see cref="real_t"/>. This operation is not
        /// meaningful on its own, but it can be used as a part of a
        /// larger expression.
        /// </summary>
        /// <param name="left">The quaternion to divide.</param>
        /// <param name="right">The value to divide by.</param>
        /// <returns>The divided quaternion.</returns>
        public static Quaternion operator /(Quaternion left, real_t right)
        {
            return left * (1.0f / right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the quaternions are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left quaternion.</param>
        /// <param name="right">The right quaternion.</param>
        /// <returns>Whether or not the quaternions are exactly equal.</returns>
        public static bool operator ==(Quaternion left, Quaternion right)
        {
            return left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the quaternions are not equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left quaternion.</param>
        /// <param name="right">The right quaternion.</param>
        /// <returns>Whether or not the quaternions are not equal.</returns>
        public static bool operator !=(Quaternion left, Quaternion right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this quaternion and <paramref name="obj"/> are equal.
        /// </summary>
        /// <param name="obj">The other object to compare.</param>
        /// <returns>Whether or not the quaternion and the other object are exactly equal.</returns>
        public override readonly bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is Quaternion other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this quaternion and <paramref name="other"/> are equal.
        /// </summary>
        /// <param name="other">The other quaternion to compare.</param>
        /// <returns>Whether or not the quaternions are exactly equal.</returns>
        public readonly bool Equals(Quaternion other)
        {
            return X == other.X && Y == other.Y && Z == other.Z && W == other.W;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this quaternion and <paramref name="other"/> are approximately equal,
        /// by running <see cref="Mathf.IsEqualApprox(real_t, real_t)"/> on each component.
        /// </summary>
        /// <param name="other">The other quaternion to compare.</param>
        /// <returns>Whether or not the quaternions are approximately equal.</returns>
        public readonly bool IsEqualApprox(Quaternion other)
        {
            return Mathf.IsEqualApprox(X, other.X) && Mathf.IsEqualApprox(Y, other.Y) && Mathf.IsEqualApprox(Z, other.Z) && Mathf.IsEqualApprox(W, other.W);
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Quaternion"/>.
        /// </summary>
        /// <returns>A hash code for this quaternion.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(X, Y, Z, W);
        }

        /// <summary>
        /// Converts this <see cref="Quaternion"/> to a string.
        /// </summary>
        /// <returns>A string representation of this quaternion.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Quaternion"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this quaternion.</returns>
        public readonly string ToString(string? format)
        {
            return $"({X.ToString(format, CultureInfo.InvariantCulture)}, {Y.ToString(format, CultureInfo.InvariantCulture)}, {Z.ToString(format, CultureInfo.InvariantCulture)}, {W.ToString(format, CultureInfo.InvariantCulture)})";
        }
    }
}
