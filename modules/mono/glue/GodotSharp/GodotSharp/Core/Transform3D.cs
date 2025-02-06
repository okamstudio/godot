using System;
using System.Diagnostics.CodeAnalysis;
using System.Runtime.InteropServices;
using System.ComponentModel;

#nullable enable

namespace Godot
{
    /// <summary>
    /// 3×4 matrix (3 rows, 4 columns) used for 3D linear transformations.
    /// It can represent transformations such as translation, rotation, or scaling.
    /// It consists of a <see cref="Godot.Basis"/> (first 3 columns) and a
    /// <see cref="Vector3"/> for the origin (last column).
    ///
    /// For more information, read this documentation article:
    /// https://docs.godotengine.org/en/latest/tutorials/math/matrices_and_transforms.html
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Transform3D : IEquatable<Transform3D>
    {
        /// <summary>
        /// The <see cref="Godot.Basis"/> of this transform. Contains the X, Y, and Z basis
        /// vectors (columns 0 to 2) and is responsible for rotation and scale.
        /// </summary>
        public Basis Basis;

        /// <summary>
        /// The origin vector (column 3, the fourth column). Equivalent to array index <c>[3]</c>.
        /// </summary>
        public Vector3 Origin;

        /// <summary>
        /// Access whole columns in the form of <see cref="Vector3"/>.
        /// The fourth column is the <see cref="Origin"/> vector.
        /// </summary>
        /// <param name="column">Which column vector.</param>
        /// <exception cref="ArgumentOutOfRangeException">
        /// <paramref name="column"/> is not 0, 1, 2 or 3.
        /// </exception>
        public Vector3 this[int column]
        {
            readonly get
            {
                switch (column)
                {
                    case 0:
                        return Basis.Column0;
                    case 1:
                        return Basis.Column1;
                    case 2:
                        return Basis.Column2;
                    case 3:
                        return Origin;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(column));
                }
            }
            set
            {
                switch (column)
                {
                    case 0:
                        Basis.Column0 = value;
                        return;
                    case 1:
                        Basis.Column1 = value;
                        return;
                    case 2:
                        Basis.Column2 = value;
                        return;
                    case 3:
                        Origin = value;
                        return;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(column));
                }
            }
        }

        /// <summary>
        /// Access matrix elements in column-major order.
        /// The fourth column is the <see cref="Origin"/> vector.
        /// </summary>
        /// <param name="column">Which column, the matrix horizontal position.</param>
        /// <param name="row">Which row, the matrix vertical position.</param>
        public real_t this[int column, int row]
        {
            readonly get
            {
                if (column == 3)
                {
                    return Origin[row];
                }
                return Basis[column, row];
            }
            set
            {
                if (column == 3)
                {
                    Origin[row] = value;
                    return;
                }
                Basis[column, row] = value;
            }
        }

        /// <summary>
        /// Returns the inverse of the transform, under the assumption that
        /// the basis is invertible (must have non-zero determinant).
        /// </summary>
        /// <seealso cref="Inverse"/>
        /// <returns>The inverse transformation matrix.</returns>
        public readonly Transform3D AffineInverse()
        {
            Basis basisInv = Basis.Inverse();
            return new Transform3D(basisInv, basisInv * -Origin);
        }

        /// <summary>
        /// Returns a transform interpolated between this transform and another
        /// <paramref name="transform"/> by a given <paramref name="weight"/>
        /// (on the range of 0.0 to 1.0).
        /// </summary>
        /// <param name="transform">The other transform.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The interpolated transform.</returns>
        public readonly Transform3D InterpolateWith(Transform3D transform, real_t weight)
        {
            Vector3 sourceScale = Basis.Scale;
            Quaternion sourceRotation = Basis.GetRotationQuaternion();
            Vector3 sourceLocation = Origin;

            Vector3 destinationScale = transform.Basis.Scale;
            Quaternion destinationRotation = transform.Basis.GetRotationQuaternion();
            Vector3 destinationLocation = transform.Origin;

            var interpolated = new Transform3D();
            Quaternion quaternion = sourceRotation.Slerp(destinationRotation, weight).Normalized();
            Vector3 scale = sourceScale.Lerp(destinationScale, weight);
            interpolated.Basis.SetQuaternionScale(quaternion, scale);
            interpolated.Origin = sourceLocation.Lerp(destinationLocation, weight);

            return interpolated;
        }

        /// <summary>
        /// Returns the inverse of the transform, under the assumption that
        /// the transformation basis is orthonormal (i.e. rotation/reflection
        /// is fine, scaling/skew is not). Use <see cref="AffineInverse"/> for
        /// non-orthonormal transforms (e.g. with scaling).
        /// </summary>
        /// <returns>The inverse matrix.</returns>
        public readonly Transform3D Inverse()
        {
            Basis basisTr = Basis.Transposed();
            return new Transform3D(basisTr, basisTr * -Origin);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this transform is finite, by calling
        /// <see cref="Mathf.IsFinite(real_t)"/> on each component.
        /// </summary>
        /// <returns>Whether this vector is finite or not.</returns>
        public readonly bool IsFinite()
        {
            return Basis.IsFinite() && Origin.IsFinite();
        }

        /// <summary>
        /// Returns a copy of the transform rotated such that the forward axis (-Z)
        /// points towards the <paramref name="target"/> position.
        /// The up axis (+Y) points as close to the <paramref name="up"/> vector
        /// as possible while staying perpendicular to the forward axis.
        /// The resulting transform is orthonormalized.
        /// The existing rotation, scale, and skew information from the original transform is discarded.
        /// The <paramref name="target"/> and <paramref name="up"/> vectors cannot be zero,
        /// cannot be parallel to each other, and are defined in global/parent space.
        /// </summary>
        /// <param name="target">The object to look at.</param>
        /// <param name="up">The relative up direction.</param>
        /// <param name="useModelFront">
        /// If true, then the model is oriented in reverse,
        /// towards the model front axis (+Z, Vector3.ModelFront),
        /// which is more useful for orienting 3D models.
        /// </param>
        /// <returns>The resulting transform.</returns>
        public readonly Transform3D LookingAt(Vector3 target, Vector3? up = null, bool useModelFront = false)
        {
            Transform3D t = this;
            t.SetLookAt(Origin, target, up ?? Vector3.Up, useModelFront);
            return t;
        }

        /// <inheritdoc cref="LookingAt(Vector3, Nullable{Vector3}, bool)"/>
        [EditorBrowsable(EditorBrowsableState.Never)]
        public readonly Transform3D LookingAt(Vector3 target, Vector3 up)
        {
            return LookingAt(target, up, false);
        }

        /// <summary>
        /// Returns the transform with the basis orthogonal (90 degrees),
        /// and normalized axis vectors (scale of 1 or -1).
        /// </summary>
        /// <returns>The orthonormalized transform.</returns>
        public readonly Transform3D Orthonormalized()
        {
            return new Transform3D(Basis.Orthonormalized(), Origin);
        }

        /// <summary>
        /// Rotates the transform around the given <paramref name="axis"/> by <paramref name="angle"/> (in radians).
        /// The axis must be a normalized vector.
        /// The operation is done in the parent/global frame, equivalent to
        /// multiplying the matrix from the left.
        /// </summary>
        /// <param name="axis">The axis to rotate around. Must be normalized.</param>
        /// <param name="angle">The angle to rotate, in radians.</param>
        /// <returns>The rotated transformation matrix.</returns>
        public readonly Transform3D Rotated(Vector3 axis, real_t angle)
        {
            return new Transform3D(new Basis(axis, angle), new Vector3()) * this;
        }

        /// <summary>
        /// Rotates the transform around the given <paramref name="axis"/> by <paramref name="angle"/> (in radians).
        /// The axis must be a normalized vector.
        /// The operation is done in the local frame, equivalent to
        /// multiplying the matrix from the right.
        /// </summary>
        /// <param name="axis">The axis to rotate around. Must be normalized.</param>
        /// <param name="angle">The angle to rotate, in radians.</param>
        /// <returns>The rotated transformation matrix.</returns>
        public readonly Transform3D RotatedLocal(Vector3 axis, real_t angle)
        {
            Basis tmpBasis = new Basis(axis, angle);
            return new Transform3D(Basis * tmpBasis, Origin);
        }

        /// <summary>
        /// Scales the transform by the given 3D <paramref name="scale"/> factor.
        /// The operation is done in the parent/global frame, equivalent to
        /// multiplying the matrix from the left.
        /// </summary>
        /// <param name="scale">The scale to introduce.</param>
        /// <returns>The scaled transformation matrix.</returns>
        public readonly Transform3D Scaled(Vector3 scale)
        {
            return new Transform3D(Basis.Scaled(scale), Origin * scale);
        }

        /// <summary>
        /// Scales the transform by the given 3D <paramref name="scale"/> factor.
        /// The operation is done in the local frame, equivalent to
        /// multiplying the matrix from the right.
        /// </summary>
        /// <param name="scale">The scale to introduce.</param>
        /// <returns>The scaled transformation matrix.</returns>
        public readonly Transform3D ScaledLocal(Vector3 scale)
        {
            Basis tmpBasis = Basis.FromScale(scale);
            return new Transform3D(Basis * tmpBasis, Origin);
        }

        private void SetLookAt(Vector3 eye, Vector3 target, Vector3 up, bool useModelFront = false)
        {
            Basis = Basis.LookingAt(target - eye, up, useModelFront);
            Origin = eye;
        }

        /// <summary>
        /// Translates the transform by the given <paramref name="offset"/>.
        /// The operation is done in the parent/global frame, equivalent to
        /// multiplying the matrix from the left.
        /// </summary>
        /// <param name="offset">The offset to translate by.</param>
        /// <returns>The translated matrix.</returns>
        public readonly Transform3D Translated(Vector3 offset)
        {
            return new Transform3D(Basis, Origin + offset);
        }

        /// <summary>
        /// Translates the transform by the given <paramref name="offset"/>.
        /// The operation is done in the local frame, equivalent to
        /// multiplying the matrix from the right.
        /// </summary>
        /// <param name="offset">The offset to translate by.</param>
        /// <returns>The translated matrix.</returns>
        public readonly Transform3D TranslatedLocal(Vector3 offset)
        {
            return new Transform3D(Basis, new Vector3
            (
                Origin[0] + Basis.Row0.Dot(offset),
                Origin[1] + Basis.Row1.Dot(offset),
                Origin[2] + Basis.Row2.Dot(offset)
            ));
        }

        // Constants
        private static readonly Transform3D _identity = new Transform3D(Basis.Identity, Vector3.Zero);
        private static readonly Transform3D _flipX = new Transform3D(new Basis(-1, 0, 0, 0, 1, 0, 0, 0, 1), Vector3.Zero);
        private static readonly Transform3D _flipY = new Transform3D(new Basis(1, 0, 0, 0, -1, 0, 0, 0, 1), Vector3.Zero);
        private static readonly Transform3D _flipZ = new Transform3D(new Basis(1, 0, 0, 0, 1, 0, 0, 0, -1), Vector3.Zero);

        /// <summary>
        /// The identity transform, with no translation, rotation, or scaling applied.
        /// This is used as a replacement for <c>Transform()</c> in GDScript.
        /// </summary>
        /// <value>Equivalent to <c>new Transform(Vector3.Right, Vector3.Up, Vector3.Back, Vector3.Zero)</c>.</value>
        public static Transform3D Identity { get { return _identity; } }
        /// <summary>
        /// The transform that will flip something along the X axis.
        /// </summary>
        /// <value>Equivalent to <c>new Transform(Vector3.Left, Vector3.Up, Vector3.Back, Vector3.Zero)</c>.</value>
        public static Transform3D FlipX { get { return _flipX; } }
        /// <summary>
        /// The transform that will flip something along the Y axis.
        /// </summary>
        /// <value>Equivalent to <c>new Transform(Vector3.Right, Vector3.Down, Vector3.Back, Vector3.Zero)</c>.</value>
        public static Transform3D FlipY { get { return _flipY; } }
        /// <summary>
        /// The transform that will flip something along the Z axis.
        /// </summary>
        /// <value>Equivalent to <c>new Transform(Vector3.Right, Vector3.Up, Vector3.Forward, Vector3.Zero)</c>.</value>
        public static Transform3D FlipZ { get { return _flipZ; } }

        /// <summary>
        /// Constructs a <see cref="Transform3D"/> set to <see langword="default"/>.
        /// </summary>
        /// <remarks>For the GDScript equivalent of <c>Transform()</c>, use <see cref="Identity"/> instead.</remarks>
        public Transform3D() => this = default;

        /// <summary>
        /// Constructs a transformation matrix from 4 vectors (matrix columns).
        /// </summary>
        /// <param name="column0">The X vector, or column index 0.</param>
        /// <param name="column1">The Y vector, or column index 1.</param>
        /// <param name="column2">The Z vector, or column index 2.</param>
        /// <param name="origin">The origin vector, or column index 3.</param>
        public Transform3D(Vector3 column0, Vector3 column1, Vector3 column2, Vector3 origin)
        {
            Basis = new Basis(column0, column1, column2);
            Origin = origin;
        }

        /// <summary>
        /// Constructs a transformation matrix from the given components.
        /// Arguments are named such that xy is equal to calling <c>Basis.X.Y</c>.
        /// </summary>
        /// <param name="xx">The X component of the X column vector, accessed via <c>t.Basis.X.X</c> or <c>[0][0]</c>.</param>
        /// <param name="yx">The X component of the Y column vector, accessed via <c>t.Basis.Y.X</c> or <c>[1][0]</c>.</param>
        /// <param name="zx">The X component of the Z column vector, accessed via <c>t.Basis.Z.X</c> or <c>[2][0]</c>.</param>
        /// <param name="xy">The Y component of the X column vector, accessed via <c>t.Basis.X.Y</c> or <c>[0][1]</c>.</param>
        /// <param name="yy">The Y component of the Y column vector, accessed via <c>t.Basis.Y.Y</c> or <c>[1][1]</c>.</param>
        /// <param name="zy">The Y component of the Z column vector, accessed via <c>t.Basis.Y.Y</c> or <c>[2][1]</c>.</param>
        /// <param name="xz">The Z component of the X column vector, accessed via <c>t.Basis.X.Y</c> or <c>[0][2]</c>.</param>
        /// <param name="yz">The Z component of the Y column vector, accessed via <c>t.Basis.Y.Y</c> or <c>[1][2]</c>.</param>
        /// <param name="zz">The Z component of the Z column vector, accessed via <c>t.Basis.Y.Y</c> or <c>[2][2]</c>.</param>
        /// <param name="ox">The X component of the origin vector, accessed via <c>t.Origin.X</c> or <c>[2][0]</c>.</param>
        /// <param name="oy">The Y component of the origin vector, accessed via <c>t.Origin.Y</c> or <c>[2][1]</c>.</param>
        /// <param name="oz">The Z component of the origin vector, accessed via <c>t.Origin.Z</c> or <c>[2][2]</c>.</param>
        public Transform3D(real_t xx, real_t yx, real_t zx, real_t xy, real_t yy, real_t zy, real_t xz, real_t yz, real_t zz, real_t ox, real_t oy, real_t oz)
        {
            Basis = new Basis(xx, yx, zx, xy, yy, zy, xz, yz, zz);
            Origin = new Vector3(ox, oy, oz);
        }

        /// <summary>
        /// Constructs a transformation matrix from the given <paramref name="basis"/> and
        /// <paramref name="origin"/> vector.
        /// </summary>
        /// <param name="basis">The <see cref="Godot.Basis"/> to create the basis from.</param>
        /// <param name="origin">The origin vector, or column index 3.</param>
        public Transform3D(Basis basis, Vector3 origin)
        {
            Basis = basis;
            Origin = origin;
        }

        /// <summary>
        /// Constructs a transformation matrix from the given <paramref name="projection"/>
        /// by trimming the last row of the projection matrix (<c>projection.X.W</c>,
        /// <c>projection.Y.W</c>, <c>projection.Z.W</c>, and <c>projection.W.W</c>
        /// are not copied over).
        /// </summary>
        /// <param name="projection">The <see cref="Projection"/> to create the transform from.</param>
        public Transform3D(Projection projection)
        {
            Basis = new Basis
            (
                projection.X.X, projection.Y.X, projection.Z.X,
                projection.X.Y, projection.Y.Y, projection.Z.Y,
                projection.X.Z, projection.Y.Z, projection.Z.Z
            );
            Origin = new Vector3
            (
                projection.W.X,
                projection.W.Y,
                projection.W.Z
            );
        }

        /// <summary>
        /// Composes these two transformation matrices by multiplying them
        /// together. This has the effect of transforming the second transform
        /// (the child) by the first transform (the parent).
        /// </summary>
        /// <param name="left">The parent transform.</param>
        /// <param name="right">The child transform.</param>
        /// <returns>The composed transform.</returns>
        public static Transform3D operator *(Transform3D left, Transform3D right)
        {
            left.Origin = left * right.Origin;
            left.Basis *= right.Basis;
            return left;
        }

        /// <summary>
        /// Returns a Vector3 transformed (multiplied) by the transformation matrix.
        /// </summary>
        /// <param name="transform">The transformation to apply.</param>
        /// <param name="vector">A Vector3 to transform.</param>
        /// <returns>The transformed Vector3.</returns>
        public static Vector3 operator *(Transform3D transform, Vector3 vector)
        {
            return new Vector3
            (
                transform.Basis.Row0.Dot(vector) + transform.Origin.X,
                transform.Basis.Row1.Dot(vector) + transform.Origin.Y,
                transform.Basis.Row2.Dot(vector) + transform.Origin.Z
            );
        }

        /// <summary>
        /// Returns a Vector3 transformed (multiplied) by the inverse transformation matrix,
        /// under the assumption that the transformation basis is orthonormal (i.e. rotation/reflection
        /// is fine, scaling/skew is not).
        /// <c>vector * transform</c> is equivalent to <c>transform.Inverse() * vector</c>. See <see cref="Inverse"/>.
        /// For transforming by inverse of an affine transformation (e.g. with scaling) <c>transform.AffineInverse() * vector</c> can be used instead. See <see cref="AffineInverse"/>.
        /// </summary>
        /// <param name="vector">A Vector3 to inversely transform.</param>
        /// <param name="transform">The transformation to apply.</param>
        /// <returns>The inversely transformed Vector3.</returns>
        public static Vector3 operator *(Vector3 vector, Transform3D transform)
        {
            Vector3 vInv = vector - transform.Origin;

            return new Vector3
            (
                (transform.Basis.Row0[0] * vInv.X) + (transform.Basis.Row1[0] * vInv.Y) + (transform.Basis.Row2[0] * vInv.Z),
                (transform.Basis.Row0[1] * vInv.X) + (transform.Basis.Row1[1] * vInv.Y) + (transform.Basis.Row2[1] * vInv.Z),
                (transform.Basis.Row0[2] * vInv.X) + (transform.Basis.Row1[2] * vInv.Y) + (transform.Basis.Row2[2] * vInv.Z)
            );
        }

        /// <summary>
        /// Returns an AABB transformed (multiplied) by the transformation matrix.
        /// </summary>
        /// <param name="transform">The transformation to apply.</param>
        /// <param name="aabb">An AABB to transform.</param>
        /// <returns>The transformed AABB.</returns>
        public static Aabb operator *(Transform3D transform, Aabb aabb)
        {
            Vector3 min = aabb.Position;
            Vector3 max = aabb.Position + aabb.Size;

            Vector3 tmin = transform.Origin;
            Vector3 tmax = transform.Origin;
            for (int i = 0; i < 3; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    real_t e = transform.Basis[j][i] * min[j];
                    real_t f = transform.Basis[j][i] * max[j];
                    if (e < f)
                    {
                        tmin[i] += e;
                        tmax[i] += f;
                    }
                    else
                    {
                        tmin[i] += f;
                        tmax[i] += e;
                    }
                }
            }

            return new Aabb(tmin, tmax - tmin);
        }

        /// <summary>
        /// Returns an AABB transformed (multiplied) by the inverse transformation matrix,
        /// under the assumption that the transformation basis is orthonormal (i.e. rotation/reflection
        /// is fine, scaling/skew is not).
        /// <c>aabb * transform</c> is equivalent to <c>transform.Inverse() * aabb</c>. See <see cref="Inverse"/>.
        /// For transforming by inverse of an affine transformation (e.g. with scaling) <c>transform.AffineInverse() * aabb</c> can be used instead. See <see cref="AffineInverse"/>.
        /// </summary>
        /// <param name="aabb">An AABB to inversely transform.</param>
        /// <param name="transform">The transformation to apply.</param>
        /// <returns>The inversely transformed AABB.</returns>
        public static Aabb operator *(Aabb aabb, Transform3D transform)
        {
            Vector3 pos = new Vector3(aabb.Position.X + aabb.Size.X, aabb.Position.Y + aabb.Size.Y, aabb.Position.Z + aabb.Size.Z) * transform;
            Vector3 to1 = new Vector3(aabb.Position.X + aabb.Size.X, aabb.Position.Y + aabb.Size.Y, aabb.Position.Z) * transform;
            Vector3 to2 = new Vector3(aabb.Position.X + aabb.Size.X, aabb.Position.Y, aabb.Position.Z + aabb.Size.Z) * transform;
            Vector3 to3 = new Vector3(aabb.Position.X + aabb.Size.X, aabb.Position.Y, aabb.Position.Z) * transform;
            Vector3 to4 = new Vector3(aabb.Position.X, aabb.Position.Y + aabb.Size.Y, aabb.Position.Z + aabb.Size.Z) * transform;
            Vector3 to5 = new Vector3(aabb.Position.X, aabb.Position.Y + aabb.Size.Y, aabb.Position.Z) * transform;
            Vector3 to6 = new Vector3(aabb.Position.X, aabb.Position.Y, aabb.Position.Z + aabb.Size.Z) * transform;
            Vector3 to7 = new Vector3(aabb.Position.X, aabb.Position.Y, aabb.Position.Z) * transform;

            return new Aabb(pos, new Vector3()).Expand(to1).Expand(to2).Expand(to3).Expand(to4).Expand(to5).Expand(to6).Expand(to7);
        }

        /// <summary>
        /// Returns a Plane transformed (multiplied) by the transformation matrix.
        /// </summary>
        /// <param name="transform">The transformation to apply.</param>
        /// <param name="plane">A Plane to transform.</param>
        /// <returns>The transformed Plane.</returns>
        public static Plane operator *(Transform3D transform, Plane plane)
        {
            Basis bInvTrans = transform.Basis.Inverse().Transposed();

            // Transform a single point on the plane.
            Vector3 point = transform * (plane.Normal * plane.D);

            // Use inverse transpose for correct normals with non-uniform scaling.
            Vector3 normal = (bInvTrans * plane.Normal).Normalized();

            real_t d = normal.Dot(point);
            return new Plane(normal, d);
        }

        /// <summary>
        /// Returns a Plane transformed (multiplied) by the inverse transformation matrix.
        /// <c>plane * transform</c> is equivalent to <c>transform.AffineInverse() * plane</c>. See <see cref="AffineInverse"/>.
        /// </summary>
        /// <param name="plane">A Plane to inversely transform.</param>
        /// <param name="transform">The transformation to apply.</param>
        /// <returns>The inversely transformed Plane.</returns>
        public static Plane operator *(Plane plane, Transform3D transform)
        {
            Transform3D tInv = transform.AffineInverse();
            Basis bTrans = transform.Basis.Transposed();

            // Transform a single point on the plane.
            Vector3 point = tInv * (plane.Normal * plane.D);

            // Note that instead of precalculating the transpose, an alternative
            // would be to use the transpose for the basis transform.
            // However that would be less SIMD friendly (requiring a swizzle).
            // So the cost is one extra precalced value in the calling code.
            // This is probably worth it, as this could be used in bottleneck areas. And
            // where it is not a bottleneck, the non-fast method is fine.

            // Use transpose for correct normals with non-uniform scaling.
            Vector3 normal = (bTrans * plane.Normal).Normalized();

            real_t d = normal.Dot(point);
            return new Plane(normal, d);
        }

        /// <summary>
        /// Returns a copy of the given Vector3[] transformed (multiplied) by the transformation matrix.
        /// </summary>
        /// <param name="transform">The transformation to apply.</param>
        /// <param name="array">A Vector3[] to transform.</param>
        /// <returns>The transformed copy of the Vector3[].</returns>
        public static Vector3[] operator *(Transform3D transform, Vector3[] array)
        {
            Vector3[] newArray = new Vector3[array.Length];

            for (int i = 0; i < array.Length; i++)
            {
                newArray[i] = transform * array[i];
            }

            return newArray;
        }

        /// <summary>
        /// Returns a copy of the given Vector3[] transformed (multiplied) by the inverse transformation matrix,
        /// under the assumption that the transformation basis is orthonormal (i.e. rotation/reflection
        /// is fine, scaling/skew is not).
        /// <c>array * transform</c> is equivalent to <c>transform.Inverse() * array</c>. See <see cref="Inverse"/>.
        /// For transforming by inverse of an affine transformation (e.g. with scaling) <c>transform.AffineInverse() * array</c> can be used instead. See <see cref="AffineInverse"/>.
        /// </summary>
        /// <param name="array">A Vector3[] to inversely transform.</param>
        /// <param name="transform">The transformation to apply.</param>
        /// <returns>The inversely transformed copy of the Vector3[].</returns>
        public static Vector3[] operator *(Vector3[] array, Transform3D transform)
        {
            Vector3[] newArray = new Vector3[array.Length];

            for (int i = 0; i < array.Length; i++)
            {
                newArray[i] = array[i] * transform;
            }

            return newArray;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the transforms are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left transform.</param>
        /// <param name="right">The right transform.</param>
        /// <returns>Whether or not the transforms are exactly equal.</returns>
        public static bool operator ==(Transform3D left, Transform3D right)
        {
            return left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the transforms are not equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left transform.</param>
        /// <param name="right">The right transform.</param>
        /// <returns>Whether or not the transforms are not equal.</returns>
        public static bool operator !=(Transform3D left, Transform3D right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the transform is exactly equal
        /// to the given object (<paramref name="obj"/>).
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="obj">The object to compare with.</param>
        /// <returns>Whether or not the transform and the object are exactly equal.</returns>
        public override readonly bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is Transform3D other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the transforms are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="other">The other transform to compare.</param>
        /// <returns>Whether or not the matrices are exactly equal.</returns>
        public readonly bool Equals(Transform3D other)
        {
            return Basis.Equals(other.Basis) && Origin.Equals(other.Origin);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this transform and <paramref name="other"/> are approximately equal,
        /// by running <see cref="Vector3.IsEqualApprox(Vector3)"/> on each component.
        /// </summary>
        /// <param name="other">The other transform to compare.</param>
        /// <returns>Whether or not the matrices are approximately equal.</returns>
        public readonly bool IsEqualApprox(Transform3D other)
        {
            return Basis.IsEqualApprox(other.Basis) && Origin.IsEqualApprox(other.Origin);
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Transform3D"/>.
        /// </summary>
        /// <returns>A hash code for this transform.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(Basis, Origin);
        }

        /// <summary>
        /// Converts this <see cref="Transform3D"/> to a string.
        /// </summary>
        /// <returns>A string representation of this transform.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Transform3D"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this transform.</returns>
        public readonly string ToString(string? format)
        {
            return $"[X: {Basis.X.ToString(format)}, Y: {Basis.Y.ToString(format)}, Z: {Basis.Z.ToString(format)}, O: {Origin.ToString(format)}]";
        }
    }
}
