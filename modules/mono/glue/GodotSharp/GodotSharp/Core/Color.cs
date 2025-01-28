using System;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Runtime.InteropServices;
using Godot.NativeInterop;

#nullable enable

namespace Godot
{
    /// <summary>
    /// A color represented by red, green, blue, and alpha (RGBA) components.
    /// The alpha component is often used for transparency.
    /// Values are in floating-point and usually range from 0 to 1.
    /// Some properties (such as <see cref="CanvasItem.Modulate"/>) may accept values
    /// greater than 1 (overbright or HDR colors).
    ///
    /// If you want to supply values in a range of 0 to 255, you should use
    /// <see cref="Color8"/> and the <c>r8</c>/<c>g8</c>/<c>b8</c>/<c>a8</c> properties.
    /// </summary>
    [Serializable]
    [StructLayout(LayoutKind.Sequential)]
    public struct Color : IEquatable<Color>
    {
        /// <summary>
        /// The color's red component, typically on the range of 0 to 1.
        /// </summary>
        public float R;

        /// <summary>
        /// The color's green component, typically on the range of 0 to 1.
        /// </summary>
        public float G;

        /// <summary>
        /// The color's blue component, typically on the range of 0 to 1.
        /// </summary>
        public float B;

        /// <summary>
        /// The color's alpha component, typically on the range of 0 to 1.
		/// A value of 0 means that the color is fully transparent.
		/// A value of 1 means that the color is fully opaque.
        /// </summary>
        public float A;

        /// <summary>
        /// Wrapper for <see cref="R"/> that uses the range 0 to 255 instead of 0 to 1.
        /// </summary>
        /// <value>Getting is equivalent to multiplying by 255 and rounding. Setting is equivalent to dividing by 255.</value>
        public int R8
        {
            readonly get
            {
                return (int)Math.Round(R * 255.0f);
            }
            set
            {
                R = value / 255.0f;
            }
        }

        /// <summary>
        /// Wrapper for <see cref="G"/> that uses the range 0 to 255 instead of 0 to 1.
        /// </summary>
        /// <value>Getting is equivalent to multiplying by 255 and rounding. Setting is equivalent to dividing by 255.</value>
        public int G8
        {
            readonly get
            {
                return (int)Math.Round(G * 255.0f);
            }
            set
            {
                G = value / 255.0f;
            }
        }

        /// <summary>
        /// Wrapper for <see cref="B"/> that uses the range 0 to 255 instead of 0 to 1.
        /// </summary>
        /// <value>Getting is equivalent to multiplying by 255 and rounding. Setting is equivalent to dividing by 255.</value>
        public int B8
        {
            readonly get
            {
                return (int)Math.Round(B * 255.0f);
            }
            set
            {
                B = value / 255.0f;
            }
        }

        /// <summary>
        /// Wrapper for <see cref="A"/> that uses the range 0 to 255 instead of 0 to 1.
        /// </summary>
        /// <value>Getting is equivalent to multiplying by 255 and rounding. Setting is equivalent to dividing by 255.</value>
        public int A8
        {
            readonly get
            {
                return (int)Math.Round(A * 255.0f);
            }
            set
            {
                A = value / 255.0f;
            }
        }

        /// <summary>
        /// The HSV hue of this color, on the range 0 to 1.
        /// </summary>
        /// <value>Getting is a long process, refer to the source code for details. Setting uses <see cref="FromHsv"/>.</value>
        public float H
        {
            readonly get
            {
                float max = Math.Max(R, Math.Max(G, B));
                float min = Math.Min(R, Math.Min(G, B));

                float delta = max - min;

                if (delta == 0)
                {
                    return 0;
                }

                float h;

                if (R == max)
                {
                    h = (G - B) / delta; // Between yellow & magenta
                }
                else if (G == max)
                {
                    h = 2 + ((B - R) / delta); // Between cyan & yellow
                }
                else
                {
                    h = 4 + ((R - G) / delta); // Between magenta & cyan
                }

                h /= 6.0f;

                if (h < 0)
                {
                    h += 1.0f;
                }

                return h;
            }
            set
            {
                this = FromHsv(value, S, V, A);
            }
        }

        /// <summary>
        /// The HSV saturation of this color, on the range 0 to 1.
        /// </summary>
        /// <value>Getting is equivalent to the ratio between the min and max RGB value. Setting uses <see cref="FromHsv"/>.</value>
        public float S
        {
            readonly get
            {
                float max = Math.Max(R, Math.Max(G, B));
                float min = Math.Min(R, Math.Min(G, B));

                float delta = max - min;

                return max == 0 ? 0 : delta / max;
            }
            set
            {
                this = FromHsv(H, value, V, A);
            }
        }

        /// <summary>
        /// The HSV value (brightness) of this color, on the range 0 to 1.
        /// </summary>
        /// <value>Getting is equivalent to using <see cref="Math.Max(float, float)"/> on the RGB components. Setting uses <see cref="FromHsv"/>.</value>
        public float V
        {
            readonly get
            {
                return Math.Max(R, Math.Max(G, B));
            }
            set
            {
                this = FromHsv(H, S, value, A);
            }
        }

        /// <summary>
        /// The OKHSL hue of this color, on the range 0 to 1.
        /// </summary>
        public float OkHslH
        {
            readonly get
            {
                return NativeFuncs.godotsharp_color_get_ok_hsl_h(this);
            }
            set
            {
                this = FromOkHsl(value, OkHslS, OkHslL, A);
            }
        }

        /// <summary>
        /// The OKHSL saturation of this color, on the range 0 to 1.
        /// </summary>
        public float OkHslS
        {
            readonly get
            {
                return NativeFuncs.godotsharp_color_get_ok_hsl_s(this);
            }
            set
            {
                this = FromOkHsl(OkHslH, value, OkHslL, A);
            }
        }

        /// <summary>
        /// The OKHSL lightness of this color, on the range 0 to 1.
        /// </summary>
        public float OkHslL
        {
            readonly get
            {
                return NativeFuncs.godotsharp_color_get_ok_hsl_l(this);
            }
            set
            {
                this = FromOkHsl(OkHslH, OkHslS, value, A);
            }
        }

        /// <summary>
        /// Returns the light intensity of the color, as a value between 0.0 and 1.0 (inclusive).
        /// This is useful when determining light or dark color. Colors with a luminance smaller
        /// than 0.5 can be generally considered dark.
        /// Note: <see cref="Luminance"/> relies on the color being in the linear color space to
        /// return an accurate relative luminance value. If the color is in the sRGB color space
        /// use <see cref="SrgbToLinear"/> to convert it to the linear color space first.
        /// </summary>
        public readonly float Luminance
        {
            get { return 0.2126f * R + 0.7152f * G + 0.0722f * B; }
        }

        /// <summary>
        /// Access color components using their index.
        /// </summary>
        /// <value>
        /// <c>[0]</c> is equivalent to <see cref="R"/>,
        /// <c>[1]</c> is equivalent to <see cref="G"/>,
        /// <c>[2]</c> is equivalent to <see cref="B"/>,
        /// <c>[3]</c> is equivalent to <see cref="A"/>.
        /// </value>
        public float this[int index]
        {
            readonly get
            {
                switch (index)
                {
                    case 0:
                        return R;
                    case 1:
                        return G;
                    case 2:
                        return B;
                    case 3:
                        return A;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
            set
            {
                switch (index)
                {
                    case 0:
                        R = value;
                        return;
                    case 1:
                        G = value;
                        return;
                    case 2:
                        B = value;
                        return;
                    case 3:
                        A = value;
                        return;
                    default:
                        throw new ArgumentOutOfRangeException(nameof(index));
                }
            }
        }

        /// <summary>
        /// Returns a new color resulting from blending this color over another.
        /// If the color is opaque, the result is also opaque.
        /// The second color may have a range of alpha values.
        /// </summary>
        /// <param name="over">The color to blend over.</param>
        /// <returns>This color blended over <paramref name="over"/>.</returns>
        public readonly Color Blend(Color over)
        {
            Color res;

            float sa = 1.0f - over.A;
            res.A = (A * sa) + over.A;

            if (res.A == 0)
            {
                return new Color(0, 0, 0, 0);
            }

            res.R = ((R * A * sa) + (over.R * over.A)) / res.A;
            res.G = ((G * A * sa) + (over.G * over.A)) / res.A;
            res.B = ((B * A * sa) + (over.B * over.A)) / res.A;

            return res;
        }

        /// <summary>
        /// Returns a new color with all components clamped between the
        /// components of <paramref name="min"/> and <paramref name="max"/>
        /// using <see cref="Mathf.Clamp(float, float, float)"/>.
        /// </summary>
        /// <param name="min">The color with minimum allowed values.</param>
        /// <param name="max">The color with maximum allowed values.</param>
        /// <returns>The color with all components clamped.</returns>
        public readonly Color Clamp(Color? min = null, Color? max = null)
        {
            Color minimum = min ?? new Color(0, 0, 0, 0);
            Color maximum = max ?? new Color(1, 1, 1, 1);
            return new Color
            (
                (float)Mathf.Clamp(R, minimum.R, maximum.R),
                (float)Mathf.Clamp(G, minimum.G, maximum.G),
                (float)Mathf.Clamp(B, minimum.B, maximum.B),
                (float)Mathf.Clamp(A, minimum.A, maximum.A)
            );
        }

        /// <summary>
        /// Returns a new color resulting from making this color darker
        /// by the specified ratio (on the range of 0 to 1).
        /// </summary>
        /// <param name="amount">The ratio to darken by.</param>
        /// <returns>The darkened color.</returns>
        public readonly Color Darkened(float amount)
        {
            Color res = this;
            res.R *= 1.0f - amount;
            res.G *= 1.0f - amount;
            res.B *= 1.0f - amount;
            return res;
        }

        /// <summary>
        /// Returns the inverted color: <c>(1 - r, 1 - g, 1 - b, a)</c>.
        /// </summary>
        /// <returns>The inverted color.</returns>
        public readonly Color Inverted()
        {
            return new Color(
                1.0f - R,
                1.0f - G,
                1.0f - B,
                A
            );
        }

        /// <summary>
        /// Returns a new color resulting from making this color lighter
        /// by the specified ratio (on the range of 0 to 1).
        /// </summary>
        /// <param name="amount">The ratio to lighten by.</param>
        /// <returns>The lightened color.</returns>
        public readonly Color Lightened(float amount)
        {
            Color res = this;
            res.R += (1.0f - res.R) * amount;
            res.G += (1.0f - res.G) * amount;
            res.B += (1.0f - res.B) * amount;
            return res;
        }

        /// <summary>
        /// Returns the result of the linear interpolation between
        /// this color and <paramref name="to"/> by amount <paramref name="weight"/>.
        /// </summary>
        /// <param name="to">The destination color for interpolation.</param>
        /// <param name="weight">A value on the range of 0.0 to 1.0, representing the amount of interpolation.</param>
        /// <returns>The resulting color of the interpolation.</returns>
        public readonly Color Lerp(Color to, real_t weight)
        {
            return new Color
            (
                (float)Mathf.Lerp(R, to.R, weight),
                (float)Mathf.Lerp(G, to.G, weight),
                (float)Mathf.Lerp(B, to.B, weight),
                (float)Mathf.Lerp(A, to.A, weight)
            );
        }

        /// <summary>
        /// Returns the color converted to the sRGB color space.
        /// This method assumes the original color is in the linear color space.
        /// See also <see cref="SrgbToLinear"/> which performs the opposite operation.
        /// </summary>
        /// <returns>The sRGB color.</returns>
        public readonly Color LinearToSrgb()
        {
            return new Color(
                R < 0.0031308f ? 12.92f * R : (1.0f + 0.055f) * (float)Mathf.Pow(R, 1.0f / 2.4f) - 0.055f,
                G < 0.0031308f ? 12.92f * G : (1.0f + 0.055f) * (float)Mathf.Pow(G, 1.0f / 2.4f) - 0.055f,
                B < 0.0031308f ? 12.92f * B : (1.0f + 0.055f) * (float)Mathf.Pow(B, 1.0f / 2.4f) - 0.055f, A);
        }

        /// <summary>
        /// Returns the color converted to linear color space.
        /// This method assumes the original color already is in sRGB color space.
        /// See also <see cref="LinearToSrgb"/> which performs the opposite operation.
        /// </summary>
        /// <returns>The color in linear color space.</returns>
        public readonly Color SrgbToLinear()
        {
            return new Color(
                R < 0.04045f ? R * (1.0f / 12.92f) : (float)Mathf.Pow((R + 0.055f) * (float)(1.0 / (1.0 + 0.055)), 2.4f),
                G < 0.04045f ? G * (1.0f / 12.92f) : (float)Mathf.Pow((G + 0.055f) * (float)(1.0 / (1.0 + 0.055)), 2.4f),
                B < 0.04045f ? B * (1.0f / 12.92f) : (float)Mathf.Pow((B + 0.055f) * (float)(1.0 / (1.0 + 0.055)), 2.4f),
                A);
        }

        /// <summary>
        /// Returns the color converted to an unsigned 32-bit integer in ABGR
        /// format (each byte represents a color channel).
        /// ABGR is the reversed version of the default format.
        /// </summary>
        /// <returns>A <see langword="uint"/> representing this color in ABGR32 format.</returns>
        public readonly uint ToAbgr32()
        {
            uint c = (byte)Math.Round(A * 255);
            c <<= 8;
            c |= (byte)Math.Round(B * 255);
            c <<= 8;
            c |= (byte)Math.Round(G * 255);
            c <<= 8;
            c |= (byte)Math.Round(R * 255);

            return c;
        }

        /// <summary>
        /// Returns the color converted to an unsigned 64-bit integer in ABGR
        /// format (each word represents a color channel).
        /// ABGR is the reversed version of the default format.
        /// </summary>
        /// <returns>A <see langword="ulong"/> representing this color in ABGR64 format.</returns>
        public readonly ulong ToAbgr64()
        {
            ulong c = (ushort)Math.Round(A * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(B * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(G * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(R * 65535);

            return c;
        }

        /// <summary>
        /// Returns the color converted to an unsigned 32-bit integer in ARGB
        /// format (each byte represents a color channel).
        /// ARGB is more compatible with DirectX, but not used much in Godot.
        /// </summary>
        /// <returns>A <see langword="uint"/> representing this color in ARGB32 format.</returns>
        public readonly uint ToArgb32()
        {
            uint c = (byte)Math.Round(A * 255);
            c <<= 8;
            c |= (byte)Math.Round(R * 255);
            c <<= 8;
            c |= (byte)Math.Round(G * 255);
            c <<= 8;
            c |= (byte)Math.Round(B * 255);

            return c;
        }

        /// <summary>
        /// Returns the color converted to an unsigned 64-bit integer in ARGB
        /// format (each word represents a color channel).
        /// ARGB is more compatible with DirectX, but not used much in Godot.
        /// </summary>
        /// <returns>A <see langword="ulong"/> representing this color in ARGB64 format.</returns>
        public readonly ulong ToArgb64()
        {
            ulong c = (ushort)Math.Round(A * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(R * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(G * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(B * 65535);

            return c;
        }

        /// <summary>
        /// Returns the color converted to an unsigned 32-bit integer in RGBA
        /// format (each byte represents a color channel).
        /// RGBA is Godot's default and recommended format.
        /// </summary>
        /// <returns>A <see langword="uint"/> representing this color in RGBA32 format.</returns>
        public readonly uint ToRgba32()
        {
            uint c = (byte)Math.Round(R * 255);
            c <<= 8;
            c |= (byte)Math.Round(G * 255);
            c <<= 8;
            c |= (byte)Math.Round(B * 255);
            c <<= 8;
            c |= (byte)Math.Round(A * 255);

            return c;
        }

        /// <summary>
        /// Returns the color converted to an unsigned 64-bit integer in RGBA
        /// format (each word represents a color channel).
        /// RGBA is Godot's default and recommended format.
        /// </summary>
        /// <returns>A <see langword="ulong"/> representing this color in RGBA64 format.</returns>
        public readonly ulong ToRgba64()
        {
            ulong c = (ushort)Math.Round(R * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(G * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(B * 65535);
            c <<= 16;
            c |= (ushort)Math.Round(A * 65535);

            return c;
        }

        /// <summary>
        /// Returns the color's HTML hexadecimal color string in RGBA format.
        /// </summary>
        /// <param name="includeAlpha">
        /// Whether or not to include alpha. If <see langword="false"/>, the color is RGB instead of RGBA.
        /// </param>
        /// <returns>A string for the HTML hexadecimal representation of this color.</returns>
        public readonly string ToHtml(bool includeAlpha = true)
        {
            string txt = string.Empty;

            txt += ToHex32(R);
            txt += ToHex32(G);
            txt += ToHex32(B);

            if (includeAlpha)
            {
                txt += ToHex32(A);
            }

            return txt;
        }

        /// <summary>
        /// Constructs a <see cref="Color"/> from RGBA values, typically on the range of 0 to 1.
        /// </summary>
        /// <param name="r">The color's red component, typically on the range of 0 to 1.</param>
        /// <param name="g">The color's green component, typically on the range of 0 to 1.</param>
        /// <param name="b">The color's blue component, typically on the range of 0 to 1.</param>
        /// <param name="a">
		/// The color's alpha value, typically on the range of 0 to 1.
		/// A value of 0 means that the color is fully transparent.
		/// A value of 1 means that the color is fully opaque.
		/// </param>
        public Color(float r, float g, float b, float a = 1.0f)
        {
            R = r;
            G = g;
            B = b;
            A = a;
        }

        /// <summary>
        /// Constructs a <see cref="Color"/> from an existing color and an alpha value.
        /// </summary>
        /// <param name="c">The color to construct from. Only its RGB values are used.</param>
        /// <param name="a">
		/// The color's alpha value, typically on the range of 0 to 1.
		/// A value of 0 means that the color is fully transparent.
		/// A value of 1 means that the color is fully opaque.
		/// </param>
        public Color(Color c, float a = 1.0f)
        {
            R = c.R;
            G = c.G;
            B = c.B;
            A = a;
        }

        /// <summary>
        /// Constructs a <see cref="Color"/> from an unsigned 32-bit integer in RGBA format
        /// (each byte represents a color channel).
        /// </summary>
        /// <param name="rgba">The <see langword="uint"/> representing the color as 0xRRGGBBAA.</param>
        public Color(uint rgba)
        {
            A = (rgba & 0xFF) / 255.0f;
            rgba >>= 8;
            B = (rgba & 0xFF) / 255.0f;
            rgba >>= 8;
            G = (rgba & 0xFF) / 255.0f;
            rgba >>= 8;
            R = (rgba & 0xFF) / 255.0f;
        }

        /// <summary>
        /// Constructs a <see cref="Color"/> from an unsigned 64-bit integer in RGBA format
        /// (each word represents a color channel).
        /// </summary>
        /// <param name="rgba">The <see langword="ulong"/> representing the color as 0xRRRRGGGGBBBBAAAA.</param>
        public Color(ulong rgba)
        {
            A = (rgba & 0xFFFF) / 65535.0f;
            rgba >>= 16;
            B = (rgba & 0xFFFF) / 65535.0f;
            rgba >>= 16;
            G = (rgba & 0xFFFF) / 65535.0f;
            rgba >>= 16;
            R = (rgba & 0xFFFF) / 65535.0f;
        }

        /// <summary>
        /// Constructs a <see cref="Color"/> either from an HTML color code or from a
        /// standardized color name. Supported color names are the same as the
        /// <see cref="Colors"/> constants.
        /// </summary>
        /// <param name="code">The HTML color code or color name to construct from.</param>
        /// <exception cref="ArgumentOutOfRangeException">
        /// A color cannot be inferred from the given <paramref name="code"/>.
        /// It was invalid HTML and a color with that name was not found.
        /// </exception>
        public Color(string code)
        {
            if (HtmlIsValid(code))
            {
                this = FromHtml(code);
            }
            else
            {
                this = Named(code);
            }
        }

        /// <summary>
        /// Constructs a <see cref="Color"/> either from an HTML color code or from a
        /// standardized color name, with <paramref name="alpha"/> on the range of 0 to 1. Supported
        /// color names are the same as the <see cref="Colors"/> constants.
        /// </summary>
        /// <param name="code">The HTML color code or color name to construct from.</param>
        /// <param name="alpha">The alpha (transparency) value, typically on the range of 0 to 1.</param>
        public Color(string code, float alpha)
        {
            this = new Color(code);
            A = alpha;
        }

        /// <summary>
        /// Constructs a <see cref="Color"/> from the HTML hexadecimal color string in RGBA format.
        /// </summary>
        /// <param name="rgba">A string for the HTML hexadecimal representation of this color.</param>
        /// <exception name="ArgumentOutOfRangeException">
        /// <paramref name="rgba"/> color code is invalid.
        /// </exception>
        public static Color FromHtml(ReadOnlySpan<char> rgba)
        {
            Color c;
            if (rgba.Length == 0)
            {
                c.R = 0f;
                c.G = 0f;
                c.B = 0f;
                c.A = 1.0f;
                return c;
            }

            if (rgba[0] == '#')
            {
                rgba = rgba.Slice(1);
            }

            // If enabled, use 1 hex digit per channel instead of 2.
            // Other sizes aren't in the HTML/CSS spec but we could add them if desired.
            bool isShorthand = rgba.Length < 5;
            bool alpha;

            if (rgba.Length == 8)
            {
                alpha = true;
            }
            else if (rgba.Length == 6)
            {
                alpha = false;
            }
            else if (rgba.Length == 4)
            {
                alpha = true;
            }
            else if (rgba.Length == 3)
            {
                alpha = false;
            }
            else
            {
                throw new ArgumentOutOfRangeException(
                    $"Invalid color code. Length is {rgba.Length}, but a length of 6 or 8 is expected: {rgba}");
            }

            c.A = 1.0f;
            if (isShorthand)
            {
                c.R = ParseCol4(rgba, 0) / 15f;
                c.G = ParseCol4(rgba, 1) / 15f;
                c.B = ParseCol4(rgba, 2) / 15f;
                if (alpha)
                {
                    c.A = ParseCol4(rgba, 3) / 15f;
                }
            }
            else
            {
                c.R = ParseCol8(rgba, 0) / 255f;
                c.G = ParseCol8(rgba, 2) / 255f;
                c.B = ParseCol8(rgba, 4) / 255f;
                if (alpha)
                {
                    c.A = ParseCol8(rgba, 6) / 255f;
                }
            }

            if (c.R < 0)
            {
                throw new ArgumentOutOfRangeException($"Invalid color code. Red part is not valid hexadecimal: {rgba}");
            }

            if (c.G < 0)
            {
                throw new ArgumentOutOfRangeException($"Invalid color code. Green part is not valid hexadecimal: {rgba}");
            }

            if (c.B < 0)
            {
                throw new ArgumentOutOfRangeException($"Invalid color code. Blue part is not valid hexadecimal: {rgba}");
            }

            if (c.A < 0)
            {
                throw new ArgumentOutOfRangeException($"Invalid color code. Alpha part is not valid hexadecimal: {rgba}");
            }
            return c;
        }

        /// <summary>
        /// Returns a color constructed from integer red, green, blue, and alpha channels.
        /// Each channel should have 8 bits of information ranging from 0 to 255.
        /// </summary>
        /// <param name="r8">The red component represented on the range of 0 to 255.</param>
        /// <param name="g8">The green component represented on the range of 0 to 255.</param>
        /// <param name="b8">The blue component represented on the range of 0 to 255.</param>
        /// <param name="a8">The alpha (transparency) component represented on the range of 0 to 255.</param>
        /// <returns>The constructed color.</returns>
        public static Color Color8(byte r8, byte g8, byte b8, byte a8 = 255)
        {
            return new Color(r8 / 255f, g8 / 255f, b8 / 255f, a8 / 255f);
        }

        /// <summary>
        /// Returns a color according to the standardized name, with the
        /// specified alpha value. Supported color names are the same as
        /// the constants defined in <see cref="Colors"/>.
        /// </summary>
        /// <param name="name">The name of the color.</param>
        /// <exception cref="ArgumentOutOfRangeException">
        /// A color with the given name is not found.
        /// </exception>
        /// <returns>The constructed color.</returns>
        private static Color Named(string name)
        {
            if (!FindNamedColor(name, out Color color))
            {
                throw new ArgumentOutOfRangeException($"Invalid Color Name: {name}");
            }

            return color;
        }

        /// <summary>
        /// Returns a color according to the standardized name, with the
        /// specified alpha value. Supported color names are the same as
        /// the constants defined in <see cref="Colors"/>.
        /// If a color with the given name is not found, it returns
        /// <paramref name="default"/>.
        /// </summary>
        /// <param name="name">The name of the color.</param>
        /// <param name="default">
        /// The default color to return when a color with the given name
        /// is not found.
        /// </param>
        /// <returns>The constructed color.</returns>
        private static Color Named(string name, Color @default)
        {
            if (!FindNamedColor(name, out Color color))
            {
                return @default;
            }

            return color;
        }

        private static bool FindNamedColor(string name, out Color color)
        {
            name = name.Replace(" ", string.Empty, StringComparison.Ordinal);
            name = name.Replace("-", string.Empty, StringComparison.Ordinal);
            name = name.Replace("_", string.Empty, StringComparison.Ordinal);
            name = name.Replace("'", string.Empty, StringComparison.Ordinal);
            name = name.Replace(".", string.Empty, StringComparison.Ordinal);
            name = name.ToUpperInvariant();

            return Colors.NamedColors.TryGetValue(name, out color);
        }

        /// <summary>
        /// Constructs a color from an HSV profile. The <paramref name="hue"/>,
        /// <paramref name="saturation"/>, and <paramref name="value"/> are typically
        /// between 0.0 and 1.0.
        /// </summary>
        /// <param name="hue">The HSV hue, typically on the range of 0 to 1.</param>
        /// <param name="saturation">The HSV saturation, typically on the range of 0 to 1.</param>
        /// <param name="value">The HSV value (brightness), typically on the range of 0 to 1.</param>
        /// <param name="alpha">The alpha (transparency) value, typically on the range of 0 to 1.</param>
        /// <returns>The constructed color.</returns>
        public static Color FromHsv(float hue, float saturation, float value, float alpha = 1.0f)
        {
            if (saturation == 0)
            {
                // Achromatic (gray)
                return new Color(value, value, value, alpha);
            }

            int i;
            float f, p, q, t;

            hue *= 6.0f;
            hue %= 6f;
            i = (int)hue;

            f = hue - i;
            p = value * (1 - saturation);
            q = value * (1 - (saturation * f));
            t = value * (1 - (saturation * (1 - f)));

            switch (i)
            {
                case 0: // Red is the dominant color
                    return new Color(value, t, p, alpha);
                case 1: // Green is the dominant color
                    return new Color(q, value, p, alpha);
                case 2:
                    return new Color(p, value, t, alpha);
                case 3: // Blue is the dominant color
                    return new Color(p, q, value, alpha);
                case 4:
                    return new Color(t, p, value, alpha);
                default: // (5) Red is the dominant color
                    return new Color(value, p, q, alpha);
            }
        }

        /// <summary>
        /// Converts a color to HSV values. This is equivalent to using each of
        /// the <c>h</c>/<c>s</c>/<c>v</c> properties, but much more efficient.
        /// </summary>
        /// <param name="hue">Output parameter for the HSV hue.</param>
        /// <param name="saturation">Output parameter for the HSV saturation.</param>
        /// <param name="value">Output parameter for the HSV value.</param>
        public readonly void ToHsv(out float hue, out float saturation, out float value)
        {
            float max = (float)Mathf.Max(R, Mathf.Max(G, B));
            float min = (float)Mathf.Min(R, Mathf.Min(G, B));

            float delta = max - min;

            if (delta == 0)
            {
                hue = 0;
            }
            else
            {
                if (R == max)
                {
                    hue = (G - B) / delta; // Between yellow & magenta
                }
                else if (G == max)
                {
                    hue = 2 + ((B - R) / delta); // Between cyan & yellow
                }
                else
                {
                    hue = 4 + ((R - G) / delta); // Between magenta & cyan
                }

                hue /= 6.0f;

                if (hue < 0)
                    hue += 1.0f;
            }

            if (max == 0)
                saturation = 0;
            else
                saturation = 1 - (min / max);

            value = max;
        }

        private static int ParseCol4(ReadOnlySpan<char> str, int index)
        {
            char character = str[index];

            if (character >= '0' && character <= '9')
            {
                return character - '0';
            }
            else if (character >= 'a' && character <= 'f')
            {
                return character + (10 - 'a');
            }
            else if (character >= 'A' && character <= 'F')
            {
                return character + (10 - 'A');
            }
            return -1;
        }

        private static int ParseCol8(ReadOnlySpan<char> str, int index)
        {
            return ParseCol4(str, index) * 16 + ParseCol4(str, index + 1);
        }

        /// <summary>
        /// Constructs a color from an OK HSL profile. The <paramref name="hue"/>,
        /// <paramref name="saturation"/>, and <paramref name="lightness"/> are typically
        /// between 0.0 and 1.0.
        /// </summary>
        /// <param name="hue">The OK HSL hue, typically on the range of 0 to 1.</param>
        /// <param name="saturation">The OK HSL saturation, typically on the range of 0 to 1.</param>
        /// <param name="lightness">The OK HSL lightness, typically on the range of 0 to 1.</param>
        /// <param name="alpha">The alpha (transparency) value, typically on the range of 0 to 1.</param>
        /// <returns>The constructed color.</returns>
        public static Color FromOkHsl(float hue, float saturation, float lightness, float alpha = 1.0f)
        {
            return NativeFuncs.godotsharp_color_from_ok_hsl(hue, saturation, lightness, alpha);
        }

        /// <summary>
        /// Encodes a <see cref="Color"/> from a RGBE9995 format integer.
        /// See <see cref="Image.Format.Rgbe9995"/>.
        /// </summary>
        /// <param name="rgbe">The RGBE9995 encoded color.</param>
        /// <returns>The constructed color.</returns>
        public static Color FromRgbe9995(uint rgbe)
        {
            float r = rgbe & 0x1ff;
            float g = (rgbe >> 9) & 0x1ff;
            float b = (rgbe >> 18) & 0x1ff;
            float e = rgbe >> 27;
            float m = (float)Mathf.Pow(2.0f, e - 15.0f - 9.0f);

            float rd = r * m;
            float gd = g * m;
            float bd = b * m;

            return new Color(rd, gd, bd, 1.0f);
        }

        /// <summary>
        /// Constructs a color from the given string, which can be either an HTML color
        /// code or a named color. Returns <paramref name="default"/> if the color cannot
        /// be inferred from the string. Supported color names are the same as the
        /// <see cref="Colors"/> constants.
        /// </summary>
        /// <param name="str">The HTML color code or color name.</param>
        /// <param name="default">The fallback color to return if the color cannot be inferred.</param>
        /// <returns>The constructed color.</returns>
        public static Color FromString(string str, Color @default)
        {
            if (HtmlIsValid(str))
            {
                return FromHtml(str);
            }
            else
            {
                return Named(str, @default);
            }
        }

        private static string ToHex32(float val)
        {
            byte b = (byte)Mathf.RoundToInt(Mathf.Clamp(val * 255, 0, 255));
            return b.HexEncode();
        }

        /// <summary>
        /// Returns <see langword="true"/> if <paramref name="color"/> is a valid HTML hexadecimal
        /// color string. The string must be a hexadecimal value (case-insensitive) of either 3,
        /// 4, 6 or 8 digits, and may be prefixed by a hash sign (<c>#</c>). This method is
        /// identical to <see cref="StringExtensions.IsValidHtmlColor(string)"/>.
        /// </summary>
        /// <param name="color">The HTML hexadecimal color string.</param>
        /// <returns>Whether or not the string was a valid HTML hexadecimal color string.</returns>
        public static bool HtmlIsValid(ReadOnlySpan<char> color)
        {
            if (color.IsEmpty)
            {
                return false;
            }

            if (color[0] == '#')
            {
                color = color.Slice(1);
            }

            // Check if the amount of hex digits is valid.
            int len = color.Length;
            if (!(len == 3 || len == 4 || len == 6 || len == 8))
            {
                return false;
            }

            // Check if each hex digit is valid.
            for (int i = 0; i < len; i++)
            {
                if (ParseCol4(color, i) == -1)
                {
                    return false;
                }
            }

            return true;
        }

        /// <summary>
        /// Adds each component of the <see cref="Color"/>
        /// with the components of the given <see cref="Color"/>.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>The added color.</returns>
        public static Color operator +(Color left, Color right)
        {
            left.R += right.R;
            left.G += right.G;
            left.B += right.B;
            left.A += right.A;
            return left;
        }

        /// <summary>
        /// Subtracts each component of the <see cref="Color"/>
        /// by the components of the given <see cref="Color"/>.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>The subtracted color.</returns>
        public static Color operator -(Color left, Color right)
        {
            left.R -= right.R;
            left.G -= right.G;
            left.B -= right.B;
            left.A -= right.A;
            return left;
        }

        /// <summary>
        /// Inverts the given color. This is equivalent to
        /// <c>Colors.White - c</c> or
        /// <c>new Color(1 - c.R, 1 - c.G, 1 - c.B, 1 - c.A)</c>.
        /// </summary>
        /// <param name="color">The color to invert.</param>
        /// <returns>The inverted color.</returns>
        public static Color operator -(Color color)
        {
            return Colors.White - color;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Color"/>
        /// by the given <see langword="float"/>.
        /// </summary>
        /// <param name="color">The color to multiply.</param>
        /// <param name="scale">The value to multiply by.</param>
        /// <returns>The multiplied color.</returns>
        public static Color operator *(Color color, float scale)
        {
            color.R *= scale;
            color.G *= scale;
            color.B *= scale;
            color.A *= scale;
            return color;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Color"/>
        /// by the given <see langword="float"/>.
        /// </summary>
        /// <param name="scale">The value to multiply by.</param>
        /// <param name="color">The color to multiply.</param>
        /// <returns>The multiplied color.</returns>
        public static Color operator *(float scale, Color color)
        {
            color.R *= scale;
            color.G *= scale;
            color.B *= scale;
            color.A *= scale;
            return color;
        }

        /// <summary>
        /// Multiplies each component of the <see cref="Color"/>
        /// by the components of the given <see cref="Color"/>.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>The multiplied color.</returns>
        public static Color operator *(Color left, Color right)
        {
            left.R *= right.R;
            left.G *= right.G;
            left.B *= right.B;
            left.A *= right.A;
            return left;
        }

        /// <summary>
        /// Divides each component of the <see cref="Color"/>
        /// by the given <see langword="float"/>.
        /// </summary>
        /// <param name="color">The dividend vector.</param>
        /// <param name="scale">The divisor value.</param>
        /// <returns>The divided color.</returns>
        public static Color operator /(Color color, float scale)
        {
            color.R /= scale;
            color.G /= scale;
            color.B /= scale;
            color.A /= scale;
            return color;
        }

        /// <summary>
        /// Divides each component of the <see cref="Color"/>
        /// by the components of the given <see cref="Color"/>.
        /// </summary>
        /// <param name="left">The dividend color.</param>
        /// <param name="right">The divisor color.</param>
        /// <returns>The divided color.</returns>
        public static Color operator /(Color left, Color right)
        {
            left.R /= right.R;
            left.G /= right.G;
            left.B /= right.B;
            left.A /= right.A;
            return left;
        }

        /// <summary>
        /// Returns <see langword="true"/> if the colors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>Whether or not the colors are equal.</returns>
        public static bool operator ==(Color left, Color right)
        {
            return left.Equals(right);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the colors are not equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>Whether or not the colors are equal.</returns>
        public static bool operator !=(Color left, Color right)
        {
            return !left.Equals(right);
        }

        /// <summary>
        /// Compares two <see cref="Color"/>s by first checking if
        /// the red value of the <paramref name="left"/> color is less than
        /// the red value of the <paramref name="right"/> color.
        /// If the red values are exactly equal, then it repeats this check
        /// with the green values of the two colors, then with the blue values,
        /// and then with the alpha value.
        /// This operator is useful for sorting colors.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>Whether or not the left is less than the right.</returns>
        public static bool operator <(Color left, Color right)
        {
            if (left.R == right.R)
            {
                if (left.G == right.G)
                {
                    if (left.B == right.B)
                    {
                        return left.A < right.A;
                    }
                    return left.B < right.B;
                }
                return left.G < right.G;
            }
            return left.R < right.R;
        }

        /// <summary>
        /// Compares two <see cref="Color"/>s by first checking if
        /// the red value of the <paramref name="left"/> color is greater than
        /// the red value of the <paramref name="right"/> color.
        /// If the red values are exactly equal, then it repeats this check
        /// with the green values of the two colors, then with the blue values,
        /// and then with the alpha value.
        /// This operator is useful for sorting colors.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>Whether or not the left is greater than the right.</returns>
        public static bool operator >(Color left, Color right)
        {
            if (left.R == right.R)
            {
                if (left.G == right.G)
                {
                    if (left.B == right.B)
                    {
                        return left.A > right.A;
                    }
                    return left.B > right.B;
                }
                return left.G > right.G;
            }
            return left.R > right.R;
        }

        /// <summary>
        /// Compares two <see cref="Color"/>s by first checking if
        /// the red value of the <paramref name="left"/> color is less than
        /// or equal to the red value of the <paramref name="right"/> color.
        /// If the red values are exactly equal, then it repeats this check
        /// with the green values of the two colors, then with the blue values,
        /// and then with the alpha value.
        /// This operator is useful for sorting colors.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>Whether or not the left is less than or equal to the right.</returns>
        public static bool operator <=(Color left, Color right)
        {
            if (left.R == right.R)
            {
                if (left.G == right.G)
                {
                    if (left.B == right.B)
                    {
                        return left.A <= right.A;
                    }
                    return left.B < right.B;
                }
                return left.G < right.G;
            }
            return left.R < right.R;
        }

        /// <summary>
        /// Compares two <see cref="Color"/>s by first checking if
        /// the red value of the <paramref name="left"/> color is greater than
        /// or equal to the red value of the <paramref name="right"/> color.
        /// If the red values are exactly equal, then it repeats this check
        /// with the green values of the two colors, then with the blue values,
        /// and then with the alpha value.
        /// This operator is useful for sorting colors.
        /// </summary>
        /// <param name="left">The left color.</param>
        /// <param name="right">The right color.</param>
        /// <returns>Whether or not the left is greater than or equal to the right.</returns>
        public static bool operator >=(Color left, Color right)
        {
            if (left.R == right.R)
            {
                if (left.G == right.G)
                {
                    if (left.B == right.B)
                    {
                        return left.A >= right.A;
                    }
                    return left.B > right.B;
                }
                return left.G > right.G;
            }
            return left.R > right.R;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this color and <paramref name="obj"/> are equal.
        /// </summary>
        /// <param name="obj">The other object to compare.</param>
        /// <returns>Whether or not the color and the other object are equal.</returns>
        public override readonly bool Equals([NotNullWhen(true)] object? obj)
        {
            return obj is Color other && Equals(other);
        }

        /// <summary>
        /// Returns <see langword="true"/> if the colors are exactly equal.
        /// Note: Due to floating-point precision errors, consider using
        /// <see cref="IsEqualApprox"/> instead, which is more reliable.
        /// </summary>
        /// <param name="other">The other color.</param>
        /// <returns>Whether or not the colors are equal.</returns>
        public readonly bool Equals(Color other)
        {
            return R == other.R && G == other.G && B == other.B && A == other.A;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this color and <paramref name="other"/> are approximately equal,
        /// by running <see cref="Mathf.IsEqualApprox(float, float)"/> on each component.
        /// </summary>
        /// <param name="other">The other color to compare.</param>
        /// <returns>Whether or not the colors are approximately equal.</returns>
        public readonly bool IsEqualApprox(Color other)
        {
            return Mathf.IsEqualApprox(R, other.R) && Mathf.IsEqualApprox(G, other.G) && Mathf.IsEqualApprox(B, other.B) && Mathf.IsEqualApprox(A, other.A);
        }

        /// <summary>
        /// Serves as the hash function for <see cref="Color"/>.
        /// </summary>
        /// <returns>A hash code for this color.</returns>
        public override readonly int GetHashCode()
        {
            return HashCode.Combine(R, G, B, A);
        }

        /// <summary>
        /// Converts this <see cref="Color"/> to a string.
        /// </summary>
        /// <returns>A string representation of this color.</returns>
        public override readonly string ToString() => ToString(null);

        /// <summary>
        /// Converts this <see cref="Color"/> to a string with the given <paramref name="format"/>.
        /// </summary>
        /// <returns>A string representation of this color.</returns>
        public readonly string ToString(string? format)
        {
            return $"({R.ToString(format, CultureInfo.InvariantCulture)}, {G.ToString(format, CultureInfo.InvariantCulture)}, {B.ToString(format, CultureInfo.InvariantCulture)}, {A.ToString(format, CultureInfo.InvariantCulture)})";
        }
    }
}
