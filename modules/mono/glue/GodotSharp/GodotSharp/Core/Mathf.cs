using System;
using System.Runtime.CompilerServices;

namespace Godot
{
    /// <summary>
    /// Provides constants and static methods for common mathematical functions.
    /// </summary>
    public static partial class Mathf
    {
        // Define constants with Decimal precision and cast down to double or float.

        /// <summary>
        /// The circle constant, the circumference of the unit circle in radians.
        /// </summary>
        // 6.2831855f and 6.28318530717959
        public const real_t Tau = (real_t)6.2831853071795864769252867666M;

        /// <summary>
        /// Constant that represents how many times the diameter of a circle
        /// fits around its perimeter. This is equivalent to <c>Mathf.Tau / 2</c>.
        /// </summary>
        // 3.1415927f and 3.14159265358979
        public const real_t Pi = (real_t)3.1415926535897932384626433833M;

        /// <summary>
        /// Positive infinity. For negative infinity, use <c>-Mathf.Inf</c>.
        /// </summary>
        public const real_t Inf = real_t.PositiveInfinity;

        /// <summary>
        /// "Not a Number", an invalid value. <c>NaN</c> has special properties, including
        /// that it is not equal to itself. It is output by some invalid operations,
        /// such as dividing zero by zero.
        /// </summary>
        public const real_t NaN = real_t.NaN;

        // 0.0174532924f and 0.0174532925199433
        private const float DegToRadConstF = (float)0.0174532925199432957692369077M;
        private const double DegToRadConstD = (double)0.0174532925199432957692369077M;
        // 57.29578f and 57.2957795130823
        private const float RadToDegConstF = (float)57.295779513082320876798154814M;
        private const double RadToDegConstD = (double)57.295779513082320876798154814M;

        /// <summary>
        /// Returns the absolute value of <paramref name="s"/> (i.e. positive value).
        /// </summary>
        /// <param name="s">The input number.</param>
        /// <returns>The absolute value of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Abs(int s)
        {
            return Math.Abs(s);
        }

        /// <summary>
        /// Returns the absolute value of <paramref name="s"/> (i.e. positive value).
        /// </summary>
        /// <param name="s">The input number.</param>
        /// <returns>The absolute value of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Abs(float s)
        {
            return Math.Abs(s);
        }

        /// <summary>
        /// Returns the absolute value of <paramref name="s"/> (i.e. positive value).
        /// </summary>
        /// <param name="s">The input number.</param>
        /// <returns>The absolute value of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Abs(double s)
        {
            return Math.Abs(s);
        }

        /// <summary>
        /// Returns the arc cosine of <paramref name="s"/> in radians.
        /// Use to get the angle of cosine <paramref name="s"/>.
        /// </summary>
        /// <param name="s">The input cosine value. Must be on the range of -1.0 to 1.0.</param>
        /// <returns>
        /// An angle that would result in the given cosine value. On the range <c>0</c> to <c>Tau/2</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Acos(float s)
        {
            return MathF.Acos(s);
        }

        /// <summary>
        /// Returns the arc cosine of <paramref name="s"/> in radians.
        /// Use to get the angle of cosine <paramref name="s"/>.
        /// </summary>
        /// <param name="s">The input cosine value. Must be on the range of -1.0 to 1.0.</param>
        /// <returns>
        /// An angle that would result in the given cosine value. On the range <c>0</c> to <c>Tau/2</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Acos(double s)
        {
            return Math.Acos(s);
        }

        /// <summary>
        /// Returns the hyperbolic arc (also called inverse) cosine of <paramref name="s"/> in radians.
        /// Use it to get the angle from an angle's cosine in hyperbolic space if
        /// <paramref name="s"/> is larger or equal to 1.
        /// </summary>
        /// <param name="s">The input hyperbolic cosine value.</param>
        /// <returns>
        /// An angle that would result in the given hyperbolic cosine value.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Acosh(float s)
        {
            return MathF.Acosh(s);
        }

        /// <summary>
        /// Returns the hyperbolic arc (also called inverse) cosine of <paramref name="s"/> in radians.
        /// Use it to get the angle from an angle's cosine in hyperbolic space if
        /// <paramref name="s"/> is larger or equal to 1.
        /// </summary>
        /// <param name="s">The input hyperbolic cosine value.</param>
        /// <returns>
        /// An angle that would result in the given hyperbolic cosine value.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Acosh(double s)
        {
            return Math.Acosh(s);
        }

        /// <summary>
        /// Returns the difference between the two angles,
        /// in range of -<see cref="Pi"/>, <see cref="Pi"/>.
        /// When <paramref name="from"/> and <paramref name="to"/> are opposite,
        /// returns -<see cref="Pi"/> if <paramref name="from"/> is smaller than <paramref name="to"/>,
        /// or <see cref="Pi"/> otherwise.
        /// </summary>
        /// <param name="from">The start angle.</param>
        /// <param name="to">The destination angle.</param>
        /// <returns>The difference between the two angles.</returns>
        public static float AngleDifference(float from, float to)
        {
            float difference = (to - from) % MathF.Tau;
            return ((2.0f * difference) % MathF.Tau) - difference;
        }

        /// <summary>
        /// Returns the difference between the two angles,
        /// in range of -<see cref="Pi"/>, <see cref="Pi"/>.
        /// When <paramref name="from"/> and <paramref name="to"/> are opposite,
        /// returns -<see cref="Pi"/> if <paramref name="from"/> is smaller than <paramref name="to"/>,
        /// or <see cref="Pi"/> otherwise.
        /// </summary>
        /// <param name="from">The start angle.</param>
        /// <param name="to">The destination angle.</param>
        /// <returns>The difference between the two angles.</returns>
        public static double AngleDifference(double from, double to)
        {
            double difference = (to - from) % Math.Tau;
            return ((2.0 * difference) % Math.Tau) - difference;
        }

        /// <summary>
        /// Returns the arc sine of <paramref name="s"/> in radians.
        /// Use to get the angle of sine <paramref name="s"/>.
        /// </summary>
        /// <param name="s">The input sine value. Must be on the range of -1.0 to 1.0.</param>
        /// <returns>
        /// An angle that would result in the given sine value. On the range <c>-Tau/4</c> to <c>Tau/4</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Asin(float s)
        {
            return MathF.Asin(s);
        }

        /// <summary>
        /// Returns the arc sine of <paramref name="s"/> in radians.
        /// Use to get the angle of sine <paramref name="s"/>.
        /// </summary>
        /// <param name="s">The input sine value. Must be on the range of -1.0 to 1.0.</param>
        /// <returns>
        /// An angle that would result in the given sine value. On the range <c>-Tau/4</c> to <c>Tau/4</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Asin(double s)
        {
            return Math.Asin(s);
        }

        /// <summary>
        /// Returns the hyperbolic arc (also called inverse) sine of <paramref name="s"/> in radians.
        /// Use it to get the angle from an angle's sine in hyperbolic space if
        /// <paramref name="s"/> is larger or equal to 1.
        /// </summary>
        /// <param name="s">The input hyperbolic sine value.</param>
        /// <returns>
        /// An angle that would result in the given hyperbolic sine value.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Asinh(float s)
        {
            return MathF.Asinh(s);
        }

        /// <summary>
        /// Returns the hyperbolic arc (also called inverse) sine of <paramref name="s"/> in radians.
        /// Use it to get the angle from an angle's sine in hyperbolic space if
        /// <paramref name="s"/> is larger or equal to 1.
        /// </summary>
        /// <param name="s">The input hyperbolic sine value.</param>
        /// <returns>
        /// An angle that would result in the given hyperbolic sine value.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Asinh(double s)
        {
            return Math.Asinh(s);
        }

        /// <summary>
        /// Returns the arc tangent of <paramref name="s"/> in radians.
        /// Use to get the angle of tangent <paramref name="s"/>.
        ///
        /// The method cannot know in which quadrant the angle should fall.
        /// See <see cref="Atan2(float, float)"/> if you have both <c>y</c> and <c>x</c>.
        /// </summary>
        /// <param name="s">The input tangent value.</param>
        /// <returns>
        /// An angle that would result in the given tangent value. On the range <c>-Tau/4</c> to <c>Tau/4</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Atan(float s)
        {
            return MathF.Atan(s);
        }

        /// <summary>
        /// Returns the arc tangent of <paramref name="s"/> in radians.
        /// Use to get the angle of tangent <paramref name="s"/>.
        ///
        /// The method cannot know in which quadrant the angle should fall.
        /// See <see cref="Atan2(double, double)"/> if you have both <c>y</c> and <c>x</c>.
        /// </summary>
        /// <param name="s">The input tangent value.</param>
        /// <returns>
        /// An angle that would result in the given tangent value. On the range <c>-Tau/4</c> to <c>Tau/4</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Atan(double s)
        {
            return Math.Atan(s);
        }

        /// <summary>
        /// Returns the arc tangent of <paramref name="y"/> and <paramref name="x"/> in radians.
        /// Use to get the angle of the tangent of <c>y/x</c>. To compute the value, the method takes into
        /// account the sign of both arguments in order to determine the quadrant.
        ///
        /// Important note: The Y coordinate comes first, by convention.
        /// </summary>
        /// <param name="y">The Y coordinate of the point to find the angle to.</param>
        /// <param name="x">The X coordinate of the point to find the angle to.</param>
        /// <returns>
        /// An angle that would result in the given tangent value. On the range <c>-Tau/2</c> to <c>Tau/2</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Atan2(float y, float x)
        {
            return MathF.Atan2(y, x);
        }

        /// <summary>
        /// Returns the arc tangent of <paramref name="y"/> and <paramref name="x"/> in radians.
        /// Use to get the angle of the tangent of <c>y/x</c>. To compute the value, the method takes into
        /// account the sign of both arguments in order to determine the quadrant.
        ///
        /// Important note: The Y coordinate comes first, by convention.
        /// </summary>
        /// <param name="y">The Y coordinate of the point to find the angle to.</param>
        /// <param name="x">The X coordinate of the point to find the angle to.</param>
        /// <returns>
        /// An angle that would result in the given tangent value. On the range <c>-Tau/2</c> to <c>Tau/2</c>.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Atan2(double y, double x)
        {
            return Math.Atan2(y, x);
        }

        /// <summary>
        /// Returns the hyperbolic arc (also called inverse) tangent of <paramref name="s"/> in radians.
        /// Use it to get the angle from an angle's tangent in hyperbolic space if
        /// <paramref name="s"/> is between -1 and 1 (non-inclusive).
        /// </summary>
        /// <param name="s">The input hyperbolic tangent value.</param>
        /// <returns>
        /// An angle that would result in the given hyperbolic tangent value.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Atanh(float s)
        {
            return MathF.Atanh(s);
        }

        /// <summary>
        /// Returns the hyperbolic arc (also called inverse) tangent of <paramref name="s"/> in radians.
        /// Use it to get the angle from an angle's tangent in hyperbolic space if
        /// <paramref name="s"/> is between -1 and 1 (non-inclusive).
        /// </summary>
        /// <param name="s">The input hyperbolic tangent value.</param>
        /// <returns>
        /// An angle that would result in the given hyperbolic tangent value.
        /// </returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Atanh(double s)
        {
            return Math.Atanh(s);
        }

        /// <summary>
        /// Rounds <paramref name="s"/> upward (towards positive infinity).
        /// </summary>
        /// <param name="s">The number to ceil.</param>
        /// <returns>The smallest whole number that is not less than <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Ceil(float s)
        {
            return MathF.Ceiling(s);
        }

        /// <summary>
        /// Rounds <paramref name="s"/> upward (towards positive infinity).
        /// </summary>
        /// <param name="s">The number to ceil.</param>
        /// <returns>The smallest whole number that is not less than <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Ceil(double s)
        {
            return Math.Ceiling(s);
        }

        /// <summary>
        /// Clamps a <paramref name="value"/> so that it is not less than <paramref name="min"/>
        /// and not more than <paramref name="max"/>.
        /// </summary>
        /// <param name="value">The value to clamp.</param>
        /// <param name="min">The minimum allowed value.</param>
        /// <param name="max">The maximum allowed value.</param>
        /// <returns>The clamped value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Clamp(int value, int min, int max)
        {
            return Math.Clamp(value, min, max);
        }

        /// <summary>
        /// Clamps a <paramref name="value"/> so that it is not less than <paramref name="min"/>
        /// and not more than <paramref name="max"/>.
        /// </summary>
        /// <param name="value">The value to clamp.</param>
        /// <param name="min">The minimum allowed value.</param>
        /// <param name="max">The maximum allowed value.</param>
        /// <returns>The clamped value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Clamp(float value, float min, float max)
        {
            return Math.Clamp(value, min, max);
        }

        /// <summary>
        /// Clamps a <paramref name="value"/> so that it is not less than <paramref name="min"/>
        /// and not more than <paramref name="max"/>.
        /// </summary>
        /// <param name="value">The value to clamp.</param>
        /// <param name="min">The minimum allowed value.</param>
        /// <param name="max">The maximum allowed value.</param>
        /// <returns>The clamped value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Clamp(double value, double min, double max)
        {
            return Math.Clamp(value, min, max);
        }

        /// <summary>
        /// Returns the cosine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The cosine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Cos(float s)
        {
            return MathF.Cos(s);
        }

        /// <summary>
        /// Returns the cosine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The cosine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Cos(double s)
        {
            return Math.Cos(s);
        }

        /// <summary>
        /// Returns the hyperbolic cosine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The hyperbolic cosine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Cosh(float s)
        {
            return MathF.Cosh(s);
        }

        /// <summary>
        /// Returns the hyperbolic cosine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The hyperbolic cosine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Cosh(double s)
        {
            return Math.Cosh(s);
        }

        /// <summary>
        /// Cubic interpolates between two values by the factor defined in <paramref name="weight"/>
        /// with pre and post values.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float CubicInterpolate(float from, float to, float pre, float post, float weight)
        {
            return 0.5f *
                    ((from * 2.0f) +
                            (-pre + to) * weight +
                            (2.0f * pre - 5.0f * from + 4.0f * to - post) * (weight * weight) +
                            (-pre + 3.0f * from - 3.0f * to + post) * (weight * weight * weight));
        }

        /// <summary>
        /// Cubic interpolates between two values by the factor defined in <paramref name="weight"/>
        /// with pre and post values.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double CubicInterpolate(double from, double to, double pre, double post, double weight)
        {
            return 0.5 *
                    ((from * 2.0) +
                            (-pre + to) * weight +
                            (2.0 * pre - 5.0 * from + 4.0 * to - post) * (weight * weight) +
                            (-pre + 3.0 * from - 3.0 * to + post) * (weight * weight * weight));
        }

        /// <summary>
        /// Cubic interpolates between two rotation values with shortest path
        /// by the factor defined in <paramref name="weight"/> with pre and post values.
        /// See also <see cref="LerpAngle(float, float, float)"/>.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float CubicInterpolateAngle(float from, float to, float pre, float post, float weight)
        {
            float fromRot = from % MathF.Tau;

            float preDiff = (pre - fromRot) % MathF.Tau;
            float preRot = fromRot + (2.0f * preDiff) % MathF.Tau - preDiff;

            float toDiff = (to - fromRot) % MathF.Tau;
            float toRot = fromRot + (2.0f * toDiff) % MathF.Tau - toDiff;

            float postDiff = (post - toRot) % MathF.Tau;
            float postRot = toRot + (2.0f * postDiff) % MathF.Tau - postDiff;

            return CubicInterpolate(fromRot, toRot, preRot, postRot, weight);
        }

        /// <summary>
        /// Cubic interpolates between two rotation values with shortest path
        /// by the factor defined in <paramref name="weight"/> with pre and post values.
        /// See also <see cref="LerpAngle(double, double, double)"/>.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double CubicInterpolateAngle(double from, double to, double pre, double post, double weight)
        {
            double fromRot = from % Math.Tau;

            double preDiff = (pre - fromRot) % Math.Tau;
            double preRot = fromRot + (2.0 * preDiff) % Math.Tau - preDiff;

            double toDiff = (to - fromRot) % Math.Tau;
            double toRot = fromRot + (2.0 * toDiff) % Math.Tau - toDiff;

            double postDiff = (post - toRot) % Math.Tau;
            double postRot = toRot + (2.0 * postDiff) % Math.Tau - postDiff;

            return CubicInterpolate(fromRot, toRot, preRot, postRot, weight);
        }

        /// <summary>
        /// Cubic interpolates between two values by the factor defined in <paramref name="weight"/>
        /// with pre and post values.
        /// It can perform smoother interpolation than
        /// <see cref="CubicInterpolate(float, float, float, float, float)"/>
        /// by the time values.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <param name="toT"></param>
        /// <param name="preT"></param>
        /// <param name="postT"></param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float CubicInterpolateInTime(float from, float to, float pre, float post, float weight, float toT, float preT, float postT)
        {
            /* Barry-Goldman method */
            float t = Lerp(0.0f, toT, weight);
            float a1 = Lerp(pre, from, preT == 0 ? 0.0f : (t - preT) / -preT);
            float a2 = Lerp(from, to, toT == 0 ? 0.5f : t / toT);
            float a3 = Lerp(to, post, postT - toT == 0 ? 1.0f : (t - toT) / (postT - toT));
            float b1 = Lerp(a1, a2, toT - preT == 0 ? 0.0f : (t - preT) / (toT - preT));
            float b2 = Lerp(a2, a3, postT == 0 ? 1.0f : t / postT);
            return Lerp(b1, b2, toT == 0 ? 0.5f : t / toT);
        }

        /// <summary>
        /// Cubic interpolates between two values by the factor defined in <paramref name="weight"/>
        /// with pre and post values.
        /// It can perform smoother interpolation than
        /// <see cref="CubicInterpolate(double, double, double, double, double)"/>
        /// by the time values.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <param name="toT"></param>
        /// <param name="preT"></param>
        /// <param name="postT"></param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double CubicInterpolateInTime(double from, double to, double pre, double post, double weight, double toT, double preT, double postT)
        {
            /* Barry-Goldman method */
            double t = Lerp(0.0, toT, weight);
            double a1 = Lerp(pre, from, preT == 0 ? 0.0 : (t - preT) / -preT);
            double a2 = Lerp(from, to, toT == 0 ? 0.5 : t / toT);
            double a3 = Lerp(to, post, postT - toT == 0 ? 1.0 : (t - toT) / (postT - toT));
            double b1 = Lerp(a1, a2, toT - preT == 0 ? 0.0 : (t - preT) / (toT - preT));
            double b2 = Lerp(a2, a3, postT == 0 ? 1.0 : t / postT);
            return Lerp(b1, b2, toT == 0 ? 0.5 : t / toT);
        }

        /// <summary>
        /// Cubic interpolates between two rotation values with shortest path
        /// by the factor defined in <paramref name="weight"/> with pre and post values.
        /// See also <see cref="LerpAngle(float, float, float)"/>.
        /// It can perform smoother interpolation than
        /// <see cref="CubicInterpolateAngle(float, float, float, float, float)"/>
        /// by the time values.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <param name="toT"></param>
        /// <param name="preT"></param>
        /// <param name="postT"></param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float CubicInterpolateAngleInTime(float from, float to, float pre, float post, float weight, float toT, float preT, float postT)
        {
            float fromRot = from % MathF.Tau;

            float preDiff = (pre - fromRot) % MathF.Tau;
            float preRot = fromRot + (2.0f * preDiff) % MathF.Tau - preDiff;

            float toDiff = (to - fromRot) % MathF.Tau;
            float toRot = fromRot + (2.0f * toDiff) % MathF.Tau - toDiff;

            float postDiff = (post - toRot) % MathF.Tau;
            float postRot = toRot + (2.0f * postDiff) % MathF.Tau - postDiff;

            return CubicInterpolateInTime(fromRot, toRot, preRot, postRot, weight, toT, preT, postT);
        }

        /// <summary>
        /// Cubic interpolates between two rotation values with shortest path
        /// by the factor defined in <paramref name="weight"/> with pre and post values.
        /// See also <see cref="LerpAngle(double, double, double)"/>.
        /// It can perform smoother interpolation than
        /// <see cref="CubicInterpolateAngle(double, double, double, double, double)"/>
        /// by the time values.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="pre">The value which before "from" value for interpolation.</param>
        /// <param name="post">The value which after "to" value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <param name="toT"></param>
        /// <param name="preT"></param>
        /// <param name="postT"></param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double CubicInterpolateAngleInTime(double from, double to, double pre, double post, double weight, double toT, double preT, double postT)
        {
            double fromRot = from % Math.Tau;

            double preDiff = (pre - fromRot) % Math.Tau;
            double preRot = fromRot + (2.0 * preDiff) % Math.Tau - preDiff;

            double toDiff = (to - fromRot) % Math.Tau;
            double toRot = fromRot + (2.0 * toDiff) % Math.Tau - toDiff;

            double postDiff = (post - toRot) % Math.Tau;
            double postRot = toRot + (2.0 * postDiff) % Math.Tau - postDiff;

            return CubicInterpolateInTime(fromRot, toRot, preRot, postRot, weight, toT, preT, postT);
        }

        /// <summary>
        /// Returns the point at the given <paramref name="t"/> on a one-dimensional Bezier curve defined by
        /// the given <paramref name="control1"/>, <paramref name="control2"/>, and <paramref name="end"/> points.
        /// </summary>
        /// <param name="start">The start value for the interpolation.</param>
        /// <param name="control1">Control point that defines the bezier curve.</param>
        /// <param name="control2">Control point that defines the bezier curve.</param>
        /// <param name="end">The destination value for the interpolation.</param>
        /// <param name="t">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float BezierInterpolate(float start, float control1, float control2, float end, float t)
        {
            // Formula from Wikipedia article on Bezier curves
            float omt = 1.0f - t;
            float omt2 = omt * omt;
            float omt3 = omt2 * omt;
            float t2 = t * t;
            float t3 = t2 * t;

            return start * omt3 + control1 * omt2 * t * 3.0f + control2 * omt * t2 * 3.0f + end * t3;
        }

        /// <summary>
        /// Returns the point at the given <paramref name="t"/> on a one-dimensional Bezier curve defined by
        /// the given <paramref name="control1"/>, <paramref name="control2"/>, and <paramref name="end"/> points.
        /// </summary>
        /// <param name="start">The start value for the interpolation.</param>
        /// <param name="control1">Control point that defines the bezier curve.</param>
        /// <param name="control2">Control point that defines the bezier curve.</param>
        /// <param name="end">The destination value for the interpolation.</param>
        /// <param name="t">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double BezierInterpolate(double start, double control1, double control2, double end, double t)
        {
            // Formula from Wikipedia article on Bezier curves
            double omt = 1.0 - t;
            double omt2 = omt * omt;
            double omt3 = omt2 * omt;
            double t2 = t * t;
            double t3 = t2 * t;

            return start * omt3 + control1 * omt2 * t * 3.0 + control2 * omt * t2 * 3.0 + end * t3;
        }

        /// <summary>
        /// Returns the derivative at the given <paramref name="t"/> on a one dimensional Bezier curve defined by
        /// the given <paramref name="control1"/>, <paramref name="control2"/>, and <paramref name="end"/> points.
        /// </summary>
        /// <param name="start">The start value for the interpolation.</param>
        /// <param name="control1">Control point that defines the bezier curve.</param>
        /// <param name="control2">Control point that defines the bezier curve.</param>
        /// <param name="end">The destination value for the interpolation.</param>
        /// <param name="t">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float BezierDerivative(float start, float control1, float control2, float end, float t)
        {
            // Formula from Wikipedia article on Bezier curves
            float omt = 1.0f - t;
            float omt2 = omt * omt;
            float t2 = t * t;

            float d = (control1 - start) * 3.0f * omt2 + (control2 - control1) * 6.0f * omt * t + (end - control2) * 3.0f * t2;
            return d;
        }

        /// <summary>
        /// Returns the derivative at the given <paramref name="t"/> on a one dimensional Bezier curve defined by
        /// the given <paramref name="control1"/>, <paramref name="control2"/>, and <paramref name="end"/> points.
        /// </summary>
        /// <param name="start">The start value for the interpolation.</param>
        /// <param name="control1">Control point that defines the bezier curve.</param>
        /// <param name="control2">Control point that defines the bezier curve.</param>
        /// <param name="end">The destination value for the interpolation.</param>
        /// <param name="t">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double BezierDerivative(double start, double control1, double control2, double end, double t)
        {
            // Formula from Wikipedia article on Bezier curves
            double omt = 1.0 - t;
            double omt2 = omt * omt;
            double t2 = t * t;

            double d = (control1 - start) * 3.0 * omt2 + (control2 - control1) * 6.0 * omt * t + (end - control2) * 3.0 * t2;
            return d;
        }

        /// <summary>
        /// Converts from decibels to linear energy (audio).
        /// </summary>
        /// <seealso cref="LinearToDb(float)"/>
        /// <param name="db">Decibels to convert.</param>
        /// <returns>Audio volume as linear energy.</returns>
        public static float DbToLinear(float db)
        {
            return MathF.Exp(db * 0.11512925464970228420089957273422f);
        }

        /// <summary>
        /// Converts from decibels to linear energy (audio).
        /// </summary>
        /// <seealso cref="LinearToDb(double)"/>
        /// <param name="db">Decibels to convert.</param>
        /// <returns>Audio volume as linear energy.</returns>
        public static double DbToLinear(double db)
        {
            return Math.Exp(db * 0.11512925464970228420089957273422);
        }

        /// <summary>
        /// Converts an angle expressed in degrees to radians.
        /// </summary>
        /// <param name="deg">An angle expressed in degrees.</param>
        /// <returns>The same angle expressed in radians.</returns>
        public static float DegToRad(float deg)
        {
            return deg * DegToRadConstF;
        }

        /// <summary>
        /// Converts an angle expressed in degrees to radians.
        /// </summary>
        /// <param name="deg">An angle expressed in degrees.</param>
        /// <returns>The same angle expressed in radians.</returns>
        public static double DegToRad(double deg)
        {
            return deg * DegToRadConstD;
        }

        /// <summary>
        /// Easing function, based on exponent. The <paramref name="curve"/> values are:
        /// <c>0</c> is constant, <c>1</c> is linear, <c>0</c> to <c>1</c> is ease-in, <c>1</c> or more is ease-out.
        /// Negative values are in-out/out-in.
        /// </summary>
        /// <param name="s">The value to ease.</param>
        /// <param name="curve">
        /// <c>0</c> is constant, <c>1</c> is linear, <c>0</c> to <c>1</c> is ease-in, <c>1</c> or more is ease-out.
        /// </param>
        /// <returns>The eased value.</returns>
        public static float Ease(float s, float curve)
        {
            if (s < 0.0f)
            {
                s = 0.0f;
            }
            else if (s > 1.0f)
            {
                s = 1.0f;
            }

            if (curve > 0.0f)
            {
                if (curve < 1.0f)
                {
                    return 1.0f - MathF.Pow(1.0f - s, 1.0f / curve);
                }

                return MathF.Pow(s, curve);
            }

            if (curve < 0.0f)
            {
                if (s < 0.5f)
                {
                    return MathF.Pow(s * 2.0f, -curve) * 0.5f;
                }

                return ((1.0f - MathF.Pow(1.0f - ((s - 0.5f) * 2.0f), -curve)) * 0.5f) + 0.5f;
            }

            return 0.0f;
        }

        /// <summary>
        /// Easing function, based on exponent. The <paramref name="curve"/> values are:
        /// <c>0</c> is constant, <c>1</c> is linear, <c>0</c> to <c>1</c> is ease-in, <c>1</c> or more is ease-out.
        /// Negative values are in-out/out-in.
        /// </summary>
        /// <param name="s">The value to ease.</param>
        /// <param name="curve">
        /// <c>0</c> is constant, <c>1</c> is linear, <c>0</c> to <c>1</c> is ease-in, <c>1</c> or more is ease-out.
        /// </param>
        /// <returns>The eased value.</returns>
        public static double Ease(double s, double curve)
        {
            if (s < 0.0)
            {
                s = 0.0;
            }
            else if (s > 1.0)
            {
                s = 1.0;
            }

            if (curve > 0)
            {
                if (curve < 1.0)
                {
                    return 1.0 - Math.Pow(1.0 - s, 1.0 / curve);
                }

                return Math.Pow(s, curve);
            }

            if (curve < 0.0)
            {
                if (s < 0.5)
                {
                    return Math.Pow(s * 2.0, -curve) * 0.5;
                }

                return ((1.0 - Math.Pow(1.0 - ((s - 0.5) * 2.0), -curve)) * 0.5) + 0.5;
            }

            return 0.0;
        }

        /// <summary>
        /// The natural exponential function. It raises the mathematical
        /// constant <c>e</c> to the power of <paramref name="s"/> and returns it.
        /// </summary>
        /// <param name="s">The exponent to raise <c>e</c> to.</param>
        /// <returns><c>e</c> raised to the power of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Exp(float s)
        {
            return MathF.Exp(s);
        }

        /// <summary>
        /// The natural exponential function. It raises the mathematical
        /// constant <c>e</c> to the power of <paramref name="s"/> and returns it.
        /// </summary>
        /// <param name="s">The exponent to raise <c>e</c> to.</param>
        /// <returns><c>e</c> raised to the power of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Exp(double s)
        {
            return Math.Exp(s);
        }

        /// <summary>
        /// Rounds <paramref name="s"/> downward (towards negative infinity).
        /// </summary>
        /// <param name="s">The number to floor.</param>
        /// <returns>The largest whole number that is not more than <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Floor(float s)
        {
            return MathF.Floor(s);
        }

        /// <summary>
        /// Rounds <paramref name="s"/> downward (towards negative infinity).
        /// </summary>
        /// <param name="s">The number to floor.</param>
        /// <returns>The largest whole number that is not more than <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Floor(double s)
        {
            return Math.Floor(s);
        }

        /// <summary>
        /// Returns a normalized value considering the given range.
        /// This is the opposite of <see cref="Lerp(float, float, float)"/>.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="weight">The interpolated value.</param>
        /// <returns>
        /// The resulting value of the inverse interpolation.
        /// The returned value will be between 0.0 and 1.0 if <paramref name="weight"/> is
        /// between <paramref name="from"/> and <paramref name="to"/> (inclusive).
        /// </returns>
        public static float InverseLerp(float from, float to, float weight)
        {
            return (weight - from) / (to - from);
        }

        /// <summary>
        /// Returns a normalized value considering the given range.
        /// This is the opposite of <see cref="Lerp(double, double, double)"/>.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="weight">The interpolated value.</param>
        /// <returns>
        /// The resulting value of the inverse interpolation.
        /// The returned value will be between 0.0 and 1.0 if <paramref name="weight"/> is
        /// between <paramref name="from"/> and <paramref name="to"/> (inclusive).
        /// </returns>
        public static double InverseLerp(double from, double to, double weight)
        {
            return (weight - from) / (to - from);
        }

        /// <summary>
        /// Returns <see langword="true"/> if <paramref name="a"/> and <paramref name="b"/> are approximately equal
        /// to each other.
        /// The comparison is done using a tolerance calculation with <see cref="Epsilon"/>.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>A <see langword="bool"/> for whether or not the two values are approximately equal.</returns>
        public static bool IsEqualApprox(float a, float b)
        {
            // Check for exact equality first, required to handle "infinity" values.
            if (a == b)
            {
                return true;
            }
            // Then check for approximate equality.
            float tolerance = EpsilonF * Math.Abs(a);
            if (tolerance < EpsilonF)
            {
                tolerance = EpsilonF;
            }
            return Math.Abs(a - b) < tolerance;
        }

        /// <summary>
        /// Returns <see langword="true"/> if <paramref name="a"/> and <paramref name="b"/> are approximately equal
        /// to each other.
        /// The comparison is done using a tolerance calculation with <see cref="Epsilon"/>.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>A <see langword="bool"/> for whether or not the two values are approximately equal.</returns>
        public static bool IsEqualApprox(double a, double b)
        {
            // Check for exact equality first, required to handle "infinity" values.
            if (a == b)
            {
                return true;
            }
            // Then check for approximate equality.
            double tolerance = EpsilonD * Math.Abs(a);
            if (tolerance < EpsilonD)
            {
                tolerance = EpsilonD;
            }
            return Math.Abs(a - b) < tolerance;
        }

        /// <summary>
        /// Returns whether <paramref name="s"/> is a finite value, i.e. it is not
        /// <see cref="NaN"/>, positive infinite, or negative infinity.
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is a finite value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsFinite(float s)
        {
            return float.IsFinite(s);
        }

        /// <summary>
        /// Returns whether <paramref name="s"/> is a finite value, i.e. it is not
        /// <see cref="NaN"/>, positive infinite, or negative infinity.
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is a finite value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsFinite(double s)
        {
            return double.IsFinite(s);
        }

        /// <summary>
        /// Returns whether <paramref name="s"/> is an infinity value (either positive infinity or negative infinity).
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is an infinity value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsInf(float s)
        {
            return float.IsInfinity(s);
        }

        /// <summary>
        /// Returns whether <paramref name="s"/> is an infinity value (either positive infinity or negative infinity).
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is an infinity value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsInf(double s)
        {
            return double.IsInfinity(s);
        }

        /// <summary>
        /// Returns whether <paramref name="s"/> is a <c>NaN</c> ("Not a Number" or invalid) value.
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is a <c>NaN</c> value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsNaN(float s)
        {
            return float.IsNaN(s);
        }

        /// <summary>
        /// Returns whether <paramref name="s"/> is a <c>NaN</c> ("Not a Number" or invalid) value.
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is a <c>NaN</c> value.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsNaN(double s)
        {
            return double.IsNaN(s);
        }

        /// <summary>
        /// Returns <see langword="true"/> if <paramref name="s"/> is zero or almost zero.
        /// The comparison is done using a tolerance calculation with <see cref="Epsilon"/>.
        ///
        /// This method is faster than using <see cref="IsEqualApprox(float, float)"/> with
        /// one value as zero.
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is nearly zero.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsZeroApprox(float s)
        {
            return Math.Abs(s) < EpsilonF;
        }

        /// <summary>
        /// Returns <see langword="true"/> if <paramref name="s"/> is zero or almost zero.
        /// The comparison is done using a tolerance calculation with <see cref="Epsilon"/>.
        ///
        /// This method is faster than using <see cref="IsEqualApprox(double, double)"/> with
        /// one value as zero.
        /// </summary>
        /// <param name="s">The value to check.</param>
        /// <returns>A <see langword="bool"/> for whether or not the value is nearly zero.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static bool IsZeroApprox(double s)
        {
            return Math.Abs(s) < EpsilonD;
        }

        /// <summary>
        /// Linearly interpolates between two values by a normalized value.
        /// This is the opposite <see cref="InverseLerp(float, float, float)"/>.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float Lerp(float from, float to, float weight)
        {
            return from + ((to - from) * weight);
        }

        /// <summary>
        /// Linearly interpolates between two values by a normalized value.
        /// This is the opposite <see cref="InverseLerp(double, double, double)"/>.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double Lerp(double from, double to, double weight)
        {
            return from + ((to - from) * weight);
        }

        /// <summary>
        /// Linearly interpolates between two angles (in radians) by a normalized value.
        ///
        /// Similar to <see cref="Lerp(float, float, float)"/>,
        /// but interpolates correctly when the angles wrap around <see cref="Tau"/>.
        /// </summary>
        /// <param name="from">The start angle for interpolation.</param>
        /// <param name="to">The destination angle for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting angle of the interpolation.</returns>
        public static float LerpAngle(float from, float to, float weight)
        {
            return from + AngleDifference(from, to) * weight;
        }

        /// <summary>
        /// Linearly interpolates between two angles (in radians) by a normalized value.
        ///
        /// Similar to <see cref="Lerp(double, double, double)"/>,
        /// but interpolates correctly when the angles wrap around <see cref="Tau"/>.
        /// </summary>
        /// <param name="from">The start angle for interpolation.</param>
        /// <param name="to">The destination angle for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting angle of the interpolation.</returns>
        public static double LerpAngle(double from, double to, double weight)
        {
            return from + AngleDifference(from, to) * weight;
        }

        /// <summary>
        /// Converts from linear energy to decibels (audio).
        /// This can be used to implement volume sliders that behave as expected (since volume isn't linear).
        /// </summary>
        /// <seealso cref="DbToLinear(float)"/>
        /// <example>
        /// <code>
        /// // "slider" refers to a node that inherits Range such as HSlider or VSlider.
        /// // Its range must be configured to go from 0 to 1.
        /// // Change the bus name if you'd like to change the volume of a specific bus only.
        /// AudioServer.SetBusVolumeDb(AudioServer.GetBusIndex("Master"), GD.LinearToDb(slider.value));
        /// </code>
        /// </example>
        /// <param name="linear">The linear energy to convert.</param>
        /// <returns>Audio as decibels.</returns>
        public static float LinearToDb(float linear)
        {
            return MathF.Log(linear) * 8.6858896380650365530225783783321f;
        }

        /// <summary>
        /// Converts from linear energy to decibels (audio).
        /// This can be used to implement volume sliders that behave as expected (since volume isn't linear).
        /// </summary>
        /// <seealso cref="DbToLinear(double)"/>
        /// <example>
        /// <code>
        /// // "slider" refers to a node that inherits Range such as HSlider or VSlider.
        /// // Its range must be configured to go from 0 to 1.
        /// // Change the bus name if you'd like to change the volume of a specific bus only.
        /// AudioServer.SetBusVolumeDb(AudioServer.GetBusIndex("Master"), GD.LinearToDb(slider.value));
        /// </code>
        /// </example>
        /// <param name="linear">The linear energy to convert.</param>
        /// <returns>Audio as decibels.</returns>
        public static double LinearToDb(double linear)
        {
            return Math.Log(linear) * 8.6858896380650365530225783783321;
        }

        /// <summary>
        /// Natural logarithm. The amount of time needed to reach a certain level of continuous growth.
        ///
        /// Note: This is not the same as the "log" function on most calculators, which uses a base 10 logarithm.
        /// </summary>
        /// <param name="s">The input value.</param>
        /// <returns>The natural log of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Log(float s)
        {
            return MathF.Log(s);
        }

        /// <summary>
        /// Natural logarithm. The amount of time needed to reach a certain level of continuous growth.
        ///
        /// Note: This is not the same as the "log" function on most calculators, which uses a base 10 logarithm.
        /// </summary>
        /// <param name="s">The input value.</param>
        /// <returns>The natural log of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Log(double s)
        {
            return Math.Log(s);
        }

        /// <summary>
        /// Returns the maximum of two values.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>Whichever of the two values is higher.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Max(int a, int b)
        {
            return Math.Max(a, b);
        }

        /// <summary>
        /// Returns the maximum of two values.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>Whichever of the two values is higher.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Max(float a, float b)
        {
            return Math.Max(a, b);
        }

        /// <summary>
        /// Returns the maximum of two values.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>Whichever of the two values is higher.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Max(double a, double b)
        {
            return Math.Max(a, b);
        }

        /// <summary>
        /// Returns the minimum of two values.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>Whichever of the two values is lower.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Min(int a, int b)
        {
            return Math.Min(a, b);
        }

        /// <summary>
        /// Returns the minimum of two values.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>Whichever of the two values is lower.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Min(float a, float b)
        {
            return Math.Min(a, b);
        }

        /// <summary>
        /// Returns the minimum of two values.
        /// </summary>
        /// <param name="a">One of the values.</param>
        /// <param name="b">The other value.</param>
        /// <returns>Whichever of the two values is lower.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Min(double a, double b)
        {
            return Math.Min(a, b);
        }

        /// <summary>
        /// Moves <paramref name="from"/> toward <paramref name="to"/> by the <paramref name="delta"/> value.
        ///
        /// Use a negative <paramref name="delta"/> value to move away.
        /// </summary>
        /// <param name="from">The start value.</param>
        /// <param name="to">The value to move towards.</param>
        /// <param name="delta">The amount to move by.</param>
        /// <returns>The value after moving.</returns>
        public static float MoveToward(float from, float to, float delta)
        {
            if (Math.Abs(to - from) <= delta)
                return to;

            return from + (Math.Sign(to - from) * delta);
        }

        /// <summary>
        /// Moves <paramref name="from"/> toward <paramref name="to"/> smoothly (fast to slow) by the <paramref name="delta"/> amount. The function is defined as <code>lerp(from, to, 1 - exp(-delta))</code>.
        /// Note that due to the nature of the exponential function, the return value will quickly get close to the target <paramref name="delta"/> but may never reach it. It is recommended to rely on <see cref="IsEqualApprox(float, float)"/> to test whether the target is reached.
        /// </summary>
        /// <param name="from">The start value.</param>
        /// <param name="to">The value to move towards.</param>
        /// <param name="delta">The amount to move by.</param>
        /// <returns>The value after moving smoothly.</returns>
        public static float MoveTowardSmooth(float from, float to, float delta)
        {
            return Lerp(from, to, -ExpM1(-delta));
        }

        /// <summary>
        /// Moves <paramref name="from"/> toward <paramref name="to"/> by the <paramref name="delta"/> value.
        ///
        /// Use a negative <paramref name="delta"/> value to move away.
        /// </summary>
        /// <param name="from">The start value.</param>
        /// <param name="to">The value to move towards.</param>
        /// <param name="delta">The amount to move by.</param>
        /// <returns>The value after moving.</returns>
        public static double MoveToward(double from, double to, double delta)
        {
            if (Math.Abs(to - from) <= delta)
                return to;

            return from + (Math.Sign(to - from) * delta);
        }

        /// <summary>
        /// Moves <paramref name="from"/> toward <paramref name="to"/> smoothly (fast to slow) by the <paramref name="delta"/> amount. The function is defined as <code>lerp(from, to, 1 - exp(-delta))</code>.
        /// Note that due to the nature of the exponential function, the return value will quickly get close to the target <paramref name="delta"/> but may never reach it. It is recommended to rely on <see cref="IsEqualApprox(double, double)"/> to test whether the target is reached.
        /// </summary>
        /// <param name="from">The start value.</param>
        /// <param name="to">The value to move towards.</param>
        /// <param name="delta">The amount to move by.</param>
        /// <returns>The value after moving smoothly.</returns>
        public static double MoveTowardSmooth(double from, double to, double delta)
        {
            return Lerp(from, to, -ExpM1(-delta));
        }


        /// <summary>
        /// Returns the nearest larger power of 2 for the integer <paramref name="value"/>.
        /// </summary>
        /// <param name="value">The input value.</param>
        /// <returns>The nearest larger power of 2.</returns>
        public static int NearestPo2(int value)
        {
            value--;
            value |= value >> 1;
            value |= value >> 2;
            value |= value >> 4;
            value |= value >> 8;
            value |= value >> 16;
            value++;
            return value;
        }

        /// <summary>
        /// Performs a canonical Modulus operation, where the output is on the range [0, <paramref name="b"/>).
        /// </summary>
        /// <param name="a">The dividend, the primary input.</param>
        /// <param name="b">The divisor. The output is on the range [0, <paramref name="b"/>).</param>
        /// <returns>The resulting output.</returns>
        public static int PosMod(int a, int b)
        {
            int c = a % b;
            if ((c < 0 && b > 0) || (c > 0 && b < 0))
            {
                c += b;
            }
            return c;
        }

        /// <summary>
        /// Performs a canonical Modulus operation, where the output is on the range [0, <paramref name="b"/>).
        /// </summary>
        /// <param name="a">The dividend, the primary input.</param>
        /// <param name="b">The divisor. The output is on the range [0, <paramref name="b"/>).</param>
        /// <returns>The resulting output.</returns>
        public static float PosMod(float a, float b)
        {
            float c = a % b;
            if ((c < 0 && b > 0) || (c > 0 && b < 0))
            {
                c += b;
            }
            return c;
        }

        /// <summary>
        /// Performs a canonical Modulus operation, where the output is on the range [0, <paramref name="b"/>).
        /// </summary>
        /// <param name="a">The dividend, the primary input.</param>
        /// <param name="b">The divisor. The output is on the range [0, <paramref name="b"/>).</param>
        /// <returns>The resulting output.</returns>
        public static double PosMod(double a, double b)
        {
            double c = a % b;
            if ((c < 0 && b > 0) || (c > 0 && b < 0))
            {
                c += b;
            }
            return c;
        }

        /// <summary>
        /// Returns the result of <paramref name="x"/> raised to the power of <paramref name="y"/>.
        /// </summary>
        /// <param name="x">The base.</param>
        /// <param name="y">The exponent.</param>
        /// <returns><paramref name="x"/> raised to the power of <paramref name="y"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Pow(float x, float y)
        {
            return MathF.Pow(x, y);
        }

        /// <summary>
        /// Returns the result of <paramref name="x"/> raised to the power of <paramref name="y"/>.
        /// </summary>
        /// <param name="x">The base.</param>
        /// <param name="y">The exponent.</param>
        /// <returns><paramref name="x"/> raised to the power of <paramref name="y"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Pow(double x, double y)
        {
            return Math.Pow(x, y);
        }

        /// <summary>
        /// Converts an angle expressed in radians to degrees.
        /// </summary>
        /// <param name="rad">An angle expressed in radians.</param>
        /// <returns>The same angle expressed in degrees.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float RadToDeg(float rad)
        {
            return rad * RadToDegConstF;
        }

        /// <summary>
        /// Converts an angle expressed in radians to degrees.
        /// </summary>
        /// <param name="rad">An angle expressed in radians.</param>
        /// <returns>The same angle expressed in degrees.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double RadToDeg(double rad)
        {
            return rad * RadToDegConstD;
        }

        /// <summary>
        /// Maps a <paramref name="value"/> from [<paramref name="inFrom"/>, <paramref name="inTo"/>]
        /// to [<paramref name="outFrom"/>, <paramref name="outTo"/>].
        /// </summary>
        /// <param name="value">The value to map.</param>
        /// <param name="inFrom">The start value for the input interpolation.</param>
        /// <param name="inTo">The destination value for the input interpolation.</param>
        /// <param name="outFrom">The start value for the output interpolation.</param>
        /// <param name="outTo">The destination value for the output interpolation.</param>
        /// <returns>The resulting mapped value mapped.</returns>
        public static float Remap(float value, float inFrom, float inTo, float outFrom, float outTo)
        {
            return Lerp(outFrom, outTo, InverseLerp(inFrom, inTo, value));
        }

        /// <summary>
        /// Maps a <paramref name="value"/> from [<paramref name="inFrom"/>, <paramref name="inTo"/>]
        /// to [<paramref name="outFrom"/>, <paramref name="outTo"/>].
        /// </summary>
        /// <param name="value">The value to map.</param>
        /// <param name="inFrom">The start value for the input interpolation.</param>
        /// <param name="inTo">The destination value for the input interpolation.</param>
        /// <param name="outFrom">The start value for the output interpolation.</param>
        /// <param name="outTo">The destination value for the output interpolation.</param>
        /// <returns>The resulting mapped value mapped.</returns>
        public static double Remap(double value, double inFrom, double inTo, double outFrom, double outTo)
        {
            return Lerp(outFrom, outTo, InverseLerp(inFrom, inTo, value));
        }

        /// <summary>
        /// Rotates <paramref name="from"/> toward <paramref name="to"/> by the <paramref name="delta"/> amount. Will not go past <paramref name="to"/>.
        /// Similar to <see cref="MoveToward(float, float, float)"/> but interpolates correctly when the angles wrap around <see cref="Tau"/>.
        /// If <paramref name="delta"/> is negative, this function will rotate away from <paramref name="to"/>, toward the opposite angle, and will not go past the opposite angle.
        /// </summary>
        /// <param name="from">The start angle.</param>
        /// <param name="to">The angle to move towards.</param>
        /// <param name="delta">The amount to move by.</param>
        /// <returns>The angle after moving.</returns>
        public static float RotateToward(float from, float to, float delta)
        {
            float difference = AngleDifference(from, to);
            float absDifference = Math.Abs(difference);
            return from + Math.Clamp(delta, absDifference - MathF.PI, absDifference) * (difference >= 0.0f ? 1.0f : -1.0f);
        }

        /// <summary>
        /// Rotates <paramref name="from"/> toward <paramref name="to"/> by the <paramref name="delta"/> amount. Will not go past <paramref name="to"/>.
        /// Similar to <see cref="MoveToward(double, double, double)"/> but interpolates correctly when the angles wrap around <see cref="Tau"/>.
        /// If <paramref name="delta"/> is negative, this function will rotate away from <paramref name="to"/>, toward the opposite angle, and will not go past the opposite angle.
        /// </summary>
        /// <param name="from">The start angle.</param>
        /// <param name="to">The angle to move towards.</param>
        /// <param name="delta">The amount to move by.</param>
        /// <returns>The angle after moving.</returns>
        public static double RotateToward(double from, double to, double delta)
        {
            double difference = AngleDifference(from, to);
            double absDifference = Math.Abs(difference);
            return from + Math.Clamp(delta, absDifference - Math.PI, absDifference) * (difference >= 0.0 ? 1.0 : -1.0);
        }

        /// <summary>
        /// Rounds <paramref name="s"/> to the nearest whole number,
        /// with halfway cases rounded towards the nearest multiple of two.
        /// </summary>
        /// <param name="s">The number to round.</param>
        /// <returns>The rounded number.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Round(float s)
        {
            return MathF.Round(s);
        }

        /// <summary>
        /// Rounds <paramref name="s"/> to the nearest whole number,
        /// with halfway cases rounded towards the nearest multiple of two.
        /// </summary>
        /// <param name="s">The number to round.</param>
        /// <returns>The rounded number.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Round(double s)
        {
            return Math.Round(s);
        }

        /// <summary>
        /// Returns the sign of <paramref name="s"/>: <c>-1</c> or <c>1</c>.
        /// Returns <c>0</c> if <paramref name="s"/> is <c>0</c>.
        /// </summary>
        /// <param name="s">The input number.</param>
        /// <returns>One of three possible values: <c>1</c>, <c>-1</c>, or <c>0</c>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Sign(int s)
        {
            return Math.Sign(s);
        }

        /// <summary>
        /// Returns the sign of <paramref name="s"/>: <c>-1</c> or <c>1</c>.
        /// Returns <c>0</c> if <paramref name="s"/> is <c>0</c>.
        /// </summary>
        /// <param name="s">The input number.</param>
        /// <returns>One of three possible values: <c>1</c>, <c>-1</c>, or <c>0</c>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Sign(float s)
        {
            return Math.Sign(s);
        }

        /// <summary>
        /// Returns the sign of <paramref name="s"/>: <c>-1</c> or <c>1</c>.
        /// Returns <c>0</c> if <paramref name="s"/> is <c>0</c>.
        /// </summary>
        /// <param name="s">The input number.</param>
        /// <returns>One of three possible values: <c>1</c>, <c>-1</c>, or <c>0</c>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static int Sign(double s)
        {
            return Math.Sign(s);
        }

        /// <summary>
        /// Returns the sine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The sine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Sin(float s)
        {
            return MathF.Sin(s);
        }

        /// <summary>
        /// Returns the sine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The sine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Sin(double s)
        {
            return Math.Sin(s);
        }

        /// <summary>
        /// Returns the hyperbolic sine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The hyperbolic sine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Sinh(float s)
        {
            return MathF.Sinh(s);
        }

        /// <summary>
        /// Returns the hyperbolic sine of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The hyperbolic sine of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Sinh(double s)
        {
            return Math.Sinh(s);
        }

        /// <summary>
        /// Returns a number smoothly interpolated between <paramref name="from"/> and <paramref name="to"/>,
        /// based on the <paramref name="weight"/>. Similar to <see cref="Lerp(float, float, float)"/>,
        /// but interpolates faster at the beginning and slower at the end.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="weight">A value representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static float SmoothStep(float from, float to, float weight)
        {
            if (IsEqualApprox(from, to))
            {
                return from;
            }
            float x = Math.Clamp((weight - from) / (to - from), 0.0f, 1.0f);
            return x * x * (3 - (2 * x));
        }

        /// <summary>
        /// Returns a number smoothly interpolated between <paramref name="from"/> and <paramref name="to"/>,
        /// based on the <paramref name="weight"/>. Similar to <see cref="Lerp(double, double, double)"/>,
        /// but interpolates faster at the beginning and slower at the end.
        /// </summary>
        /// <param name="from">The start value for interpolation.</param>
        /// <param name="to">The destination value for interpolation.</param>
        /// <param name="weight">A value representing the amount of interpolation.</param>
        /// <returns>The resulting value of the interpolation.</returns>
        public static double SmoothStep(double from, double to, double weight)
        {
            if (IsEqualApprox(from, to))
            {
                return from;
            }
            double x = Math.Clamp((weight - from) / (to - from), 0.0, 1.0);
            return x * x * (3 - (2 * x));
        }

        /// <summary>
        /// Returns the square root of <paramref name="s"/>, where <paramref name="s"/> is a non-negative number.
        ///
        /// If you need negative inputs, use <see cref="System.Numerics.Complex"/>.
        /// </summary>
        /// <param name="s">The input number. Must not be negative.</param>
        /// <returns>The square root of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Sqrt(float s)
        {
            return MathF.Sqrt(s);
        }

        /// <summary>
        /// Returns the square root of <paramref name="s"/>, where <paramref name="s"/> is a non-negative number.
        ///
        /// If you need negative inputs, use <see cref="System.Numerics.Complex"/>.
        /// </summary>
        /// <param name="s">The input number. Must not be negative.</param>
        /// <returns>The square root of <paramref name="s"/>.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Sqrt(double s)
        {
            return Math.Sqrt(s);
        }

        /// <summary>
        /// Returns the position of the first non-zero digit, after the
        /// decimal point. Note that the maximum return value is 10,
        /// which is a design decision in the implementation.
        /// </summary>
        /// <param name="step">The input value.</param>
        /// <returns>The position of the first non-zero digit.</returns>
        public static int StepDecimals(double step)
        {
            double[] sd = new double[]
            {
                0.9999,
                0.09999,
                0.009999,
                0.0009999,
                0.00009999,
                0.000009999,
                0.0000009999,
                0.00000009999,
                0.000000009999,
            };
            double abs = Math.Abs(step);
            double decs = abs - (int)abs; // Strip away integer part
            for (int i = 0; i < sd.Length; i++)
            {
                if (decs >= sd[i])
                {
                    return i;
                }
            }
            return 0;
        }

        /// <summary>
        /// Snaps float value <paramref name="s"/> to a given <paramref name="step"/>.
        /// This can also be used to round a floating point number to an arbitrary number of decimals.
        /// </summary>
        /// <param name="s">The value to snap.</param>
        /// <param name="step">The step size to snap to.</param>
        /// <returns>The snapped value.</returns>
        public static float Snapped(float s, float step)
        {
            if (step != 0f)
            {
                return MathF.Floor((s / step) + 0.5f) * step;
            }

            return s;
        }

        /// <summary>
        /// Snaps float value <paramref name="s"/> to a given <paramref name="step"/>.
        /// This can also be used to round a floating point number to an arbitrary number of decimals.
        /// </summary>
        /// <param name="s">The value to snap.</param>
        /// <param name="step">The step size to snap to.</param>
        /// <returns>The snapped value.</returns>
        public static double Snapped(double s, double step)
        {
            if (step != 0f)
            {
                return Math.Floor((s / step) + 0.5f) * step;
            }

            return s;
        }

        /// <summary>
        /// Returns the tangent of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The tangent of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Tan(float s)
        {
            return MathF.Tan(s);
        }

        /// <summary>
        /// Returns the tangent of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The tangent of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Tan(double s)
        {
            return Math.Tan(s);
        }

        /// <summary>
        /// Returns the hyperbolic tangent of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The hyperbolic tangent of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static float Tanh(float s)
        {
            return MathF.Tanh(s);
        }

        /// <summary>
        /// Returns the hyperbolic tangent of angle <paramref name="s"/> in radians.
        /// </summary>
        /// <param name="s">The angle in radians.</param>
        /// <returns>The hyperbolic tangent of that angle.</returns>
        [MethodImpl(MethodImplOptions.AggressiveInlining)]
        public static double Tanh(double s)
        {
            return Math.Tanh(s);
        }

        /// <summary>
        /// Wraps <paramref name="value"/> between <paramref name="min"/> and <paramref name="max"/>.
        /// Usable for creating loop-alike behavior or infinite surfaces.
        /// If <paramref name="min"/> is <c>0</c>, this is equivalent
        /// to <see cref="PosMod(int, int)"/>, so prefer using that instead.
        /// </summary>
        /// <param name="value">The value to wrap.</param>
        /// <param name="min">The minimum allowed value and lower bound of the range.</param>
        /// <param name="max">The maximum allowed value and upper bound of the range.</param>
        /// <returns>The wrapped value.</returns>
        public static int Wrap(int value, int min, int max)
        {
            int range = max - min;
            if (range == 0)
                return min;

            return min + ((((value - min) % range) + range) % range);
        }

        /// <summary>
        /// Wraps <paramref name="value"/> between <paramref name="min"/> and <paramref name="max"/>.
        /// Usable for creating loop-alike behavior or infinite surfaces.
        /// If <paramref name="min"/> is <c>0</c>, this is equivalent
        /// to <see cref="PosMod(float, float)"/>, so prefer using that instead.
        /// </summary>
        /// <param name="value">The value to wrap.</param>
        /// <param name="min">The minimum allowed value and lower bound of the range.</param>
        /// <param name="max">The maximum allowed value and upper bound of the range.</param>
        /// <returns>The wrapped value.</returns>
        public static float Wrap(float value, float min, float max)
        {
            float range = max - min;
            if (IsZeroApprox(range))
            {
                return min;
            }
            return min + ((((value - min) % range) + range) % range);
        }

        /// <summary>
        /// Wraps <paramref name="value"/> between <paramref name="min"/> and <paramref name="max"/>.
        /// Usable for creating loop-alike behavior or infinite surfaces.
        /// If <paramref name="min"/> is <c>0</c>, this is equivalent
        /// to <see cref="PosMod(double, double)"/>, so prefer using that instead.
        /// </summary>
        /// <param name="value">The value to wrap.</param>
        /// <param name="min">The minimum allowed value and lower bound of the range.</param>
        /// <param name="max">The maximum allowed value and upper bound of the range.</param>
        /// <returns>The wrapped value.</returns>
        public static double Wrap(double value, double min, double max)
        {
            double range = max - min;
            if (IsZeroApprox(range))
            {
                return min;
            }
            return min + ((((value - min) % range) + range) % range);
        }

        /// <summary>
        /// Returns the <paramref name="value"/> wrapped between <c>0</c> and the <paramref name="length"/>.
        /// If the limit is reached, the next value the function returned is decreased to the <c>0</c> side
        /// or increased to the <paramref name="length"/> side (like a triangle wave).
        /// If <paramref name="length"/> is less than zero, it becomes positive.
        /// </summary>
        /// <param name="value">The value to pingpong.</param>
        /// <param name="length">The maximum value of the function.</param>
        /// <returns>The ping-ponged value.</returns>
        public static float PingPong(float value, float length)
        {
            return (length != 0.0f) ? Math.Abs(Fract((value - length) / (length * 2.0f)) * length * 2.0f - length) : 0.0f;

            static float Fract(float value)
            {
                return value - MathF.Floor(value);
            }
        }

        /// <summary>
        /// Returns the <paramref name="value"/> wrapped between <c>0</c> and the <paramref name="length"/>.
        /// If the limit is reached, the next value the function returned is decreased to the <c>0</c> side
        /// or increased to the <paramref name="length"/> side (like a triangle wave).
        /// If <paramref name="length"/> is less than zero, it becomes positive.
        /// </summary>
        /// <param name="value">The value to pingpong.</param>
        /// <param name="length">The maximum value of the function.</param>
        /// <returns>The ping-ponged value.</returns>
        public static double PingPong(double value, double length)
        {
            return (length != 0.0) ? Math.Abs(Fract((value - length) / (length * 2.0)) * length * 2.0 - length) : 0.0;

            static double Fract(double value)
            {
                return value - Math.Floor(value);
            }
        }
    }
}
