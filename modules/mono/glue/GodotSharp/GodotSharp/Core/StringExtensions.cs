using System;
using System.Collections.Generic;
using System.Globalization;
using System.Security;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using Godot.NativeInterop;

#nullable enable

namespace Godot
{
    /// <summary>
    /// Extension methods to manipulate strings.
    /// </summary>
    public static class StringExtensions
    {
        private static int GetSliceCount(this string instance, string splitter)
        {
            if (string.IsNullOrEmpty(instance) || string.IsNullOrEmpty(splitter))
                return 0;

            int pos = 0;
            int slices = 1;

            while ((pos = instance.Find(splitter, pos, caseSensitive: true)) >= 0)
            {
                slices++;
                pos += splitter.Length;
            }

            return slices;
        }

        private static string GetSliceCharacter(this string instance, char splitter, int slice)
        {
            if (!string.IsNullOrEmpty(instance) && slice >= 0)
            {
                int i = 0;
                int prev = 0;
                int count = 0;

                while (true)
                {
                    bool end = instance.Length <= i;

                    if (end || instance[i] == splitter)
                    {
                        if (slice == count)
                        {
                            return instance.Substring(prev, i - prev);
                        }
                        else if (end)
                        {
                            return string.Empty;
                        }

                        count++;
                        prev = i + 1;
                    }

                    i++;
                }
            }

            return string.Empty;
        }

        /// <summary>
        /// Returns the bigrams (pairs of consecutive letters) of this string.
        /// </summary>
        /// <param name="instance">The string that will be used.</param>
        /// <returns>The bigrams of this string.</returns>
        public static string[] Bigrams(this string instance)
        {
            string[] b = new string[instance.Length - 1];

            for (int i = 0; i < b.Length; i++)
            {
                b[i] = instance.Substring(i, 2);
            }

            return b;
        }

        /// <summary>
        /// Converts a string containing a binary number into an integer.
        /// Binary strings can either be prefixed with <c>0b</c> or not,
        /// and they can also start with a <c>-</c> before the optional prefix.
        /// </summary>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The converted string.</returns>
        public static int BinToInt(this string instance)
        {
            if (instance.Length == 0)
            {
                return 0;
            }

            int sign = 1;

            if (instance[0] == '-')
            {
                sign = -1;
                instance = instance.Substring(1);
            }

            if (instance.StartsWith("0b", StringComparison.OrdinalIgnoreCase))
            {
                instance = instance.Substring(2);
            }

            return sign * Convert.ToInt32(instance, 2);
        }

        /// <summary>
        /// Returns the number of occurrences of substring <paramref name="what"/> in the string.
        /// </summary>
        /// <param name="instance">The string where the substring will be searched.</param>
        /// <param name="what">The substring that will be counted.</param>
        /// <param name="from">Index to start searching from.</param>
        /// <param name="to">Index to stop searching at.</param>
        /// <param name="caseSensitive">If the search is case sensitive.</param>
        /// <returns>Number of occurrences of the substring in the string.</returns>
        public static int Count(this string instance, string what, int from = 0, int to = 0, bool caseSensitive = true)
        {
            if (what.Length == 0)
            {
                return 0;
            }

            int len = instance.Length;
            int slen = what.Length;

            if (len < slen)
            {
                return 0;
            }

            string str;

            if (from >= 0 && to >= 0)
            {
                if (to == 0)
                {
                    to = len;
                }
                else if (from >= to)
                {
                    return 0;
                }

                if (from == 0 && to == len)
                {
                    str = instance;
                }
                else
                {
                    str = instance.Substring(from, to - from);
                }
            }
            else
            {
                return 0;
            }

            int c = 0;
            int idx;

            do
            {
                idx = str.IndexOf(what, caseSensitive ? StringComparison.Ordinal : StringComparison.OrdinalIgnoreCase);
                if (idx != -1)
                {
                    str = str.Substring(idx + slen);
                    ++c;
                }
            } while (idx != -1);

            return c;
        }

        /// <summary>
        /// Returns the number of occurrences of substring <paramref name="what"/> (ignoring case)
        /// between <paramref name="from"/> and <paramref name="to"/> positions. If <paramref name="from"/>
        /// and <paramref name="to"/> equals 0 the whole string will be used. If only <paramref name="to"/>
        /// equals 0 the remained substring will be used.
        /// </summary>
        /// <param name="instance">The string where the substring will be searched.</param>
        /// <param name="what">The substring that will be counted.</param>
        /// <param name="from">Index to start searching from.</param>
        /// <param name="to">Index to stop searching at.</param>
        /// <returns>Number of occurrences of the substring in the string.</returns>
        public static int CountN(this string instance, string what, int from = 0, int to = 0)
        {
            return instance.Count(what, from, to, caseSensitive: false);
        }

        /// <summary>
        /// Returns a copy of the string with indentation (leading tabs and spaces) removed.
        /// See also <see cref="Indent"/> to add indentation.
        /// </summary>
        /// <param name="instance">The string to remove the indentation from.</param>
        /// <returns>The string with the indentation removed.</returns>
        public static string Dedent(this string instance)
        {
            var sb = new StringBuilder();
            string indent = "";
            bool hasIndent = false;
            bool hasText = false;
            int lineStart = 0;
            int indentStop = -1;

            for (int i = 0; i < instance.Length; i++)
            {
                char c = instance[i];
                if (c == '\n')
                {
                    if (hasText)
                    {
                        sb.Append(instance.AsSpan(indentStop, i - indentStop));
                    }
                    sb.Append('\n');
                    hasText = false;
                    lineStart = i + 1;
                    indentStop = -1;
                }
                else if (!hasText)
                {
                    if (c > 32)
                    {
                        hasText = true;
                        if (!hasIndent)
                        {
                            hasIndent = true;
                            indent = instance.Substring(lineStart, i - lineStart);
                            indentStop = i;
                        }
                    }
                    if (hasIndent && indentStop < 0)
                    {
                        int j = i - lineStart;
                        if (j >= indent.Length || c != indent[j])
                        {
                            indentStop = i;
                        }
                    }
                }
            }

            if (hasText)
            {
                sb.Append(instance.AsSpan(indentStop, instance.Length - indentStop));
            }

            return sb.ToString();
        }

        /// <summary>
        /// Returns a copy of the string with special characters escaped using the C language standard.
        /// </summary>
        /// <param name="instance">The string to escape.</param>
        /// <returns>The escaped string.</returns>
        public static string CEscape(this string instance)
        {
            var sb = new StringBuilder(instance);

            sb.Replace("\\", "\\\\");
            sb.Replace("\a", "\\a");
            sb.Replace("\b", "\\b");
            sb.Replace("\f", "\\f");
            sb.Replace("\n", "\\n");
            sb.Replace("\r", "\\r");
            sb.Replace("\t", "\\t");
            sb.Replace("\v", "\\v");
            sb.Replace("\'", "\\'");
            sb.Replace("\"", "\\\"");

            return sb.ToString();
        }

        /// <summary>
        /// Returns a copy of the string with escaped characters replaced by their meanings
        /// according to the C language standard.
        /// </summary>
        /// <param name="instance">The string to unescape.</param>
        /// <returns>The unescaped string.</returns>
        public static string CUnescape(this string instance)
        {
            var sb = new StringBuilder(instance);

            sb.Replace("\\a", "\a");
            sb.Replace("\\b", "\b");
            sb.Replace("\\f", "\f");
            sb.Replace("\\n", "\n");
            sb.Replace("\\r", "\r");
            sb.Replace("\\t", "\t");
            sb.Replace("\\v", "\v");
            sb.Replace("\\'", "\'");
            sb.Replace("\\\"", "\"");
            sb.Replace("\\\\", "\\");

            return sb.ToString();
        }

        /// <summary>
        /// Changes the case of some letters. Replace underscores with spaces, convert all letters
        /// to lowercase then capitalize first and every letter following the space character.
        /// For <c>capitalize camelCase mixed_with_underscores</c> it will return
        /// <c>Capitalize Camelcase Mixed With Underscores</c>.
        /// </summary>
        /// <param name="instance">The string to capitalize.</param>
        /// <returns>The capitalized string.</returns>
        public static string Capitalize(this string instance)
        {
            string aux = instance.CamelcaseToUnderscore(true).Replace("_", " ", StringComparison.Ordinal).Trim();
            string cap = string.Empty;

            for (int i = 0; i < aux.GetSliceCount(" "); i++)
            {
                string slice = aux.GetSliceCharacter(' ', i);
                if (slice.Length > 0)
                {
                    slice = char.ToUpperInvariant(slice[0]) + slice.Substring(1);
                    if (i > 0)
                        cap += " ";
                    cap += slice;
                }
            }

            return cap;
        }

        /// <summary>
        /// Returns the string converted to <c>camelCase</c>.
        /// </summary>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The converted string.</returns>
        public static string ToCamelCase(this string instance)
        {
            using godot_string instanceStr = Marshaling.ConvertStringToNative(instance);
            NativeFuncs.godotsharp_string_to_camel_case(instanceStr, out godot_string camelCase);
            using (camelCase)
                return Marshaling.ConvertStringToManaged(camelCase);
        }

        /// <summary>
        /// Returns the string converted to <c>PascalCase</c>.
        /// </summary>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The converted string.</returns>
        public static string ToPascalCase(this string instance)
        {
            using godot_string instanceStr = Marshaling.ConvertStringToNative(instance);
            NativeFuncs.godotsharp_string_to_pascal_case(instanceStr, out godot_string pascalCase);
            using (pascalCase)
                return Marshaling.ConvertStringToManaged(pascalCase);
        }

        /// <summary>
        /// Returns the string converted to <c>snake_case</c>.
        /// </summary>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The converted string.</returns>
        public static string ToSnakeCase(this string instance)
        {
            using godot_string instanceStr = Marshaling.ConvertStringToNative(instance);
            NativeFuncs.godotsharp_string_to_snake_case(instanceStr, out godot_string snakeCase);
            using (snakeCase)
                return Marshaling.ConvertStringToManaged(snakeCase);
        }

        private static string CamelcaseToUnderscore(this string instance, bool lowerCase)
        {
            string newString = string.Empty;
            int startIndex = 0;

            for (int i = 1; i < instance.Length; i++)
            {
                bool isUpper = char.IsUpper(instance[i]);
                bool isNumber = char.IsDigit(instance[i]);

                bool areNext2Lower = false;
                bool isNextLower = false;
                bool isNextNumber = false;
                bool wasPrecedentUpper = char.IsUpper(instance[i - 1]);
                bool wasPrecedentNumber = char.IsDigit(instance[i - 1]);

                if (i + 2 < instance.Length)
                {
                    areNext2Lower = char.IsLower(instance[i + 1]) && char.IsLower(instance[i + 2]);
                }

                if (i + 1 < instance.Length)
                {
                    isNextLower = char.IsLower(instance[i + 1]);
                    isNextNumber = char.IsDigit(instance[i + 1]);
                }

                bool condA = isUpper && !wasPrecedentUpper && !wasPrecedentNumber;
                bool condB = wasPrecedentUpper && isUpper && areNext2Lower;
                bool condC = isNumber && !wasPrecedentNumber;
                bool canBreakNumberLetter = isNumber && !wasPrecedentNumber && isNextLower;
                bool canBreakLetterNumber = !isNumber && wasPrecedentNumber && (isNextLower || isNextNumber);

                bool shouldSplit = condA || condB || condC || canBreakNumberLetter || canBreakLetterNumber;
                if (shouldSplit)
                {
                    newString += string.Concat(instance.AsSpan(startIndex, i - startIndex), "_");
                    startIndex = i;
                }
            }

            newString += instance.Substring(startIndex, instance.Length - startIndex);
            return lowerCase ? newString.ToLowerInvariant() : newString;
        }

        /// <summary>
        /// Performs a case-sensitive comparison to another string and returns an integer that indicates their relative position in the sort order.
        /// </summary>
        /// <seealso cref="NocasecmpTo(string, string)"/>
        /// <seealso cref="CompareTo(string, string, bool)"/>
        /// <param name="instance">The string to compare.</param>
        /// <param name="to">The other string to compare.</param>
        /// <returns>An integer that indicates the lexical relationship between the two comparands.</returns>
        public static int CasecmpTo(this string instance, string to)
        {
#pragma warning disable CA1309 // Use ordinal string comparison
            return string.Compare(instance, to, ignoreCase: false, null);
#pragma warning restore CA1309
        }

        /// <summary>
        /// Performs a comparison to another string and returns an integer that indicates their relative position in the sort order.
        /// </summary>
        /// <param name="instance">The string to compare.</param>
        /// <param name="to">The other string to compare.</param>
        /// <param name="caseSensitive">
        /// If <see langword="true"/>, the comparison will be case sensitive.
        /// </param>
        /// <returns>An integer that indicates the lexical relationship between the two comparands.</returns>
        [Obsolete("Use string.Compare instead.")]
        public static int CompareTo(this string instance, string to, bool caseSensitive = true)
        {
#pragma warning disable CA1309 // Use ordinal string comparison
            return string.Compare(instance, to, ignoreCase: !caseSensitive, null);
#pragma warning restore CA1309
        }

        /// <summary>
        /// Returns the extension without the leading period character (<c>.</c>)
        /// if the string is a valid file name or path. If the string does not contain
        /// an extension, returns an empty string instead.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print("/path/to/file.txt".GetExtension())  // "txt"
        /// GD.Print("file.txt".GetExtension())  // "txt"
        /// GD.Print("file.sample.txt".GetExtension())  // "txt"
        /// GD.Print(".txt".GetExtension())  // "txt"
        /// GD.Print("file.txt.".GetExtension())  // "" (empty string)
        /// GD.Print("file.txt..".GetExtension())  // "" (empty string)
        /// GD.Print("txt".GetExtension())  // "" (empty string)
        /// GD.Print("".GetExtension())  // "" (empty string)
        /// </code>
        /// </example>
        /// <seealso cref="GetBaseName(string)"/>
        /// <seealso cref="GetBaseDir(string)"/>
        /// <seealso cref="GetFile(string)"/>
        /// <param name="instance">The path to a file.</param>
        /// <returns>The extension of the file or an empty string.</returns>
        public static string GetExtension(this string instance)
        {
            int pos = instance.RFind(".");

            if (pos < 0)
                return instance;

            return instance.Substring(pos + 1);
        }

        /// <summary>
        /// Returns the index of the first occurrence of the specified string in this instance,
        /// or <c>-1</c>. Optionally, the starting search index can be specified, continuing
        /// to the end of the string.
        /// Note: If you just want to know whether a string contains a substring, use the
        /// <see cref="string.Contains(string)"/> method.
        /// </summary>
        /// <seealso cref="Find(string, char, int, bool)"/>
        /// <seealso cref="FindN(string, string, int)"/>
        /// <seealso cref="RFind(string, string, int, bool)"/>
        /// <seealso cref="RFindN(string, string, int)"/>
        /// <param name="instance">The string that will be searched.</param>
        /// <param name="what">The substring to find.</param>
        /// <param name="from">The search starting position.</param>
        /// <param name="caseSensitive">If <see langword="true"/>, the search is case sensitive.</param>
        /// <returns>The starting position of the substring, or -1 if not found.</returns>
        public static int Find(this string instance, string what, int from = 0, bool caseSensitive = true)
        {
            return instance.IndexOf(what, from,
                caseSensitive ? StringComparison.Ordinal : StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Find the first occurrence of a char. Optionally, the search starting position can be passed.
        /// </summary>
        /// <seealso cref="Find(string, string, int, bool)"/>
        /// <seealso cref="FindN(string, string, int)"/>
        /// <seealso cref="RFind(string, string, int, bool)"/>
        /// <seealso cref="RFindN(string, string, int)"/>
        /// <param name="instance">The string that will be searched.</param>
        /// <param name="what">The substring to find.</param>
        /// <param name="from">The search starting position.</param>
        /// <param name="caseSensitive">If <see langword="true"/>, the search is case sensitive.</param>
        /// <returns>The first instance of the char, or -1 if not found.</returns>
        public static int Find(this string instance, char what, int from = 0, bool caseSensitive = true)
        {
            if (caseSensitive)
                return instance.IndexOf(what, from);

            return CultureInfo.InvariantCulture.CompareInfo.IndexOf(instance, what, from, CompareOptions.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Returns the index of the first case-insensitive occurrence of the specified string in this instance,
        /// or <c>-1</c>. Optionally, the starting search index can be specified, continuing
        /// to the end of the string.
        /// </summary>
        /// <seealso cref="Find(string, string, int, bool)"/>
        /// <seealso cref="Find(string, char, int, bool)"/>
        /// <seealso cref="RFind(string, string, int, bool)"/>
        /// <seealso cref="RFindN(string, string, int)"/>
        /// <param name="instance">The string that will be searched.</param>
        /// <param name="what">The substring to find.</param>
        /// <param name="from">The search starting position.</param>
        /// <returns>The starting position of the substring, or -1 if not found.</returns>
        public static int FindN(this string instance, string what, int from = 0)
        {
            return instance.IndexOf(what, from, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// If the string is a path to a file, return the base directory.
        /// </summary>
        /// <seealso cref="GetBaseName(string)"/>
        /// <seealso cref="GetExtension(string)"/>
        /// <seealso cref="GetFile(string)"/>
        /// <param name="instance">The path to a file.</param>
        /// <returns>The base directory.</returns>
        public static string GetBaseDir(this string instance)
        {
            int basepos = instance.Find("://");

            string rs;
            string directory = string.Empty;

            if (basepos != -1)
            {
                int end = basepos + 3;
                rs = instance.Substring(end);
                directory = instance.Substring(0, end);
            }
            else
            {
                if (instance.StartsWith('/'))
                {
                    rs = instance.Substring(1);
                    directory = "/";
                }
                else
                {
                    rs = instance;
                }
            }

            int sep = Mathf.Max(rs.RFind("/"), rs.RFind("\\"));

            if (sep == -1)
                return directory;

            return directory + rs.Substr(0, sep);
        }

        /// <summary>
        /// If the string is a path to a file, return the path to the file without the extension.
        /// </summary>
        /// <seealso cref="GetExtension(string)"/>
        /// <seealso cref="GetBaseDir(string)"/>
        /// <seealso cref="GetFile(string)"/>
        /// <param name="instance">The path to a file.</param>
        /// <returns>The path to the file without the extension.</returns>
        public static string GetBaseName(this string instance)
        {
            int index = instance.RFind(".");

            if (index > 0)
                return instance.Substring(0, index);

            return instance;
        }

        /// <summary>
        /// If the string is a path to a file, return the file and ignore the base directory.
        /// </summary>
        /// <seealso cref="GetBaseName(string)"/>
        /// <seealso cref="GetExtension(string)"/>
        /// <seealso cref="GetBaseDir(string)"/>
        /// <param name="instance">The path to a file.</param>
        /// <returns>The file name.</returns>
        public static string GetFile(this string instance)
        {
            int sep = Mathf.Max(instance.RFind("/"), instance.RFind("\\"));

            if (sep == -1)
                return instance;

            return instance.Substring(sep + 1);
        }

        /// <summary>
        /// Converts ASCII encoded array to string.
        /// Fast alternative to <see cref="GetStringFromUtf8"/> if the
        /// content is ASCII-only. Unlike the UTF-8 function this function
        /// maps every byte to a character in the array. Multibyte sequences
        /// will not be interpreted correctly. For parsing user input always
        /// use <see cref="GetStringFromUtf8"/>.
        /// </summary>
        /// <param name="bytes">A byte array of ASCII characters (on the range of 0-127).</param>
        /// <returns>A string created from the bytes.</returns>
        public static string GetStringFromAscii(this byte[] bytes)
        {
            return Encoding.ASCII.GetString(bytes);
        }

        /// <summary>
        /// Converts UTF-16 encoded array to string using the little endian byte order.
        /// </summary>
        /// <param name="bytes">A byte array of UTF-16 characters.</param>
        /// <returns>A string created from the bytes.</returns>
        public static string GetStringFromUtf16(this byte[] bytes)
        {
            return Encoding.Unicode.GetString(bytes);
        }

        /// <summary>
        /// Converts UTF-32 encoded array to string using the little endian byte order.
        /// </summary>
        /// <param name="bytes">A byte array of UTF-32 characters.</param>
        /// <returns>A string created from the bytes.</returns>
        public static string GetStringFromUtf32(this byte[] bytes)
        {
            return Encoding.UTF32.GetString(bytes);
        }

        /// <summary>
        /// Converts UTF-8 encoded array to string.
        /// Slower than <see cref="GetStringFromAscii"/> but supports UTF-8
        /// encoded data. Use this function if you are unsure about the
        /// source of the data. For user input this function
        /// should always be preferred.
        /// </summary>
        /// <param name="bytes">
        /// A byte array of UTF-8 characters (a character may take up multiple bytes).
        /// </param>
        /// <returns>A string created from the bytes.</returns>
        public static string GetStringFromUtf8(this byte[] bytes)
        {
            return Encoding.UTF8.GetString(bytes);
        }

        /// <summary>
        /// Hash the string and return a 32 bits unsigned integer.
        /// </summary>
        /// <param name="instance">The string to hash.</param>
        /// <returns>The calculated hash of the string.</returns>
        public static uint Hash(this string instance)
        {
            uint hash = 5381;

            foreach (uint c in instance)
            {
                hash = (hash << 5) + hash + c; // hash * 33 + c
            }

            return hash;
        }

        /// <summary>
        /// Decodes a hexadecimal string.
        /// </summary>
        /// <param name="instance">The hexadecimal string.</param>
        /// <returns>The byte array representation of this string.</returns>
        public static byte[] HexDecode(this string instance)
        {
            if (instance.Length % 2 != 0)
            {
                throw new ArgumentException("Hexadecimal string of uneven length.", nameof(instance));
            }
            int len = instance.Length / 2;
            byte[] ret = new byte[len];
            for (int i = 0; i < len; i++)
            {
                ret[i] = (byte)int.Parse(instance.AsSpan(i * 2, 2), NumberStyles.AllowHexSpecifier, CultureInfo.InvariantCulture);
            }
            return ret;
        }

        /// <summary>
        /// Returns a hexadecimal representation of this byte as a string.
        /// </summary>
        /// <param name="b">The byte to encode.</param>
        /// <returns>The hexadecimal representation of this byte.</returns>
        internal static string HexEncode(this byte b)
        {
            string ret = string.Empty;

            for (int i = 0; i < 2; i++)
            {
                char c;
                int lv = b & 0xF;

                if (lv < 10)
                {
                    c = (char)('0' + lv);
                }
                else
                {
                    c = (char)('a' + lv - 10);
                }

                b >>= 4;
                ret = c + ret;
            }

            return ret;
        }

        /// <summary>
        /// Returns a hexadecimal representation of this byte array as a string.
        /// </summary>
        /// <param name="bytes">The byte array to encode.</param>
        /// <returns>The hexadecimal representation of this byte array.</returns>
        public static string HexEncode(this byte[] bytes)
        {
            string ret = string.Empty;

            foreach (byte b in bytes)
            {
                ret += b.HexEncode();
            }

            return ret;
        }

        /// <summary>
        /// Converts a string containing a hexadecimal number into an integer.
        /// Hexadecimal strings can either be prefixed with <c>0x</c> or not,
        /// and they can also start with a <c>-</c> before the optional prefix.
        /// </summary>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The converted string.</returns>
        public static int HexToInt(this string instance)
        {
            if (instance.Length == 0)
            {
                return 0;
            }

            int sign = 1;

            if (instance[0] == '-')
            {
                sign = -1;
                instance = instance.Substring(1);
            }

            if (instance.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            {
                instance = instance.Substring(2);
            }

            return sign * int.Parse(instance, NumberStyles.HexNumber, CultureInfo.InvariantCulture);
        }

        /// <summary>
        /// Returns a copy of the string with lines indented with <paramref name="prefix"/>.
        /// For example, the string can be indented with two tabs using <c>"\t\t"</c>,
        /// or four spaces using <c>"    "</c>. The prefix can be any string so it can
        /// also be used to comment out strings with e.g. <c>"// </c>.
        /// See also <see cref="Dedent"/> to remove indentation.
        /// Note: Empty lines are kept empty.
        /// </summary>
        /// <param name="instance">The string to add indentation to.</param>
        /// <param name="prefix">The string to use as indentation.</param>
        /// <returns>The string with indentation added.</returns>
        public static string Indent(this string instance, string prefix)
        {
            var sb = new StringBuilder();
            int lineStart = 0;

            for (int i = 0; i < instance.Length; i++)
            {
                char c = instance[i];
                if (c == '\n')
                {
                    if (i == lineStart)
                    {
                        sb.Append(c); // Leave empty lines empty.
                    }
                    else
                    {
                        sb.Append(prefix);
                        sb.Append(instance.AsSpan(lineStart, i - lineStart + 1));
                    }
                    lineStart = i + 1;
                }
            }
            if (lineStart != instance.Length)
            {
                sb.Append(prefix);
                sb.Append(instance.AsSpan(lineStart));
            }
            return sb.ToString();
        }

        /// <summary>
        /// Returns <see langword="true"/> if the string is a path to a file or
        /// directory and its starting point is explicitly defined. This includes
        /// <c>res://</c>, <c>user://</c>, <c>C:\</c>, <c>/</c>, etc.
        /// </summary>
        /// <seealso cref="IsRelativePath(string)"/>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string is an absolute path.</returns>
        public static bool IsAbsolutePath(this string instance)
        {
            if (string.IsNullOrEmpty(instance))
                return false;
            else if (instance.Length > 1)
                return instance[0] == '/' || instance[0] == '\\' || instance.Contains(":/", StringComparison.Ordinal) || instance.Contains(":\\", StringComparison.Ordinal);
            else
                return instance[0] == '/' || instance[0] == '\\';
        }

        /// <summary>
        /// Returns <see langword="true"/> if the string is a path to a file or
        /// directory and its starting point is implicitly defined within the
        /// context it is being used. The starting point may refer to the current
        /// directory (<c>./</c>), or the current <see cref="Node"/>.
        /// </summary>
        /// <seealso cref="IsAbsolutePath(string)"/>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string is a relative path.</returns>
        public static bool IsRelativePath(this string instance)
        {
            return !IsAbsolutePath(instance);
        }

        /// <summary>
        /// Check whether this string is a subsequence of the given string.
        /// </summary>
        /// <seealso cref="IsSubsequenceOfN(string, string)"/>
        /// <param name="instance">The subsequence to search.</param>
        /// <param name="text">The string that contains the subsequence.</param>
        /// <param name="caseSensitive">If <see langword="true"/>, the check is case sensitive.</param>
        /// <returns>If the string is a subsequence of the given string.</returns>
        public static bool IsSubsequenceOf(this string instance, string text, bool caseSensitive = true)
        {
            int len = instance.Length;

            if (len == 0)
                return true; // Technically an empty string is subsequence of any string

            if (len > text.Length)
                return false;

            int source = 0;
            int target = 0;

            while (source < len && target < text.Length)
            {
                bool match;

                if (!caseSensitive)
                {
                    char sourcec = char.ToLowerInvariant(instance[source]);
                    char targetc = char.ToLowerInvariant(text[target]);
                    match = sourcec == targetc;
                }
                else
                {
                    match = instance[source] == text[target];
                }

                if (match)
                {
                    source++;
                    if (source >= len)
                        return true;
                }

                target++;
            }

            return false;
        }

        /// <summary>
        /// Check whether this string is a subsequence of the given string, ignoring case differences.
        /// </summary>
        /// <seealso cref="IsSubsequenceOf(string, string, bool)"/>
        /// <param name="instance">The subsequence to search.</param>
        /// <param name="text">The string that contains the subsequence.</param>
        /// <returns>If the string is a subsequence of the given string.</returns>
        public static bool IsSubsequenceOfN(this string instance, string text)
        {
            return instance.IsSubsequenceOf(text, caseSensitive: false);
        }

        private static readonly char[] _invalidFileNameCharacters = { ':', '/', '\\', '?', '*', '"', '|', '%', '<', '>' };

        /// <summary>
        /// Returns <see langword="true"/> if this string is free from characters that
        /// aren't allowed in file names.
        /// </summary>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string contains a valid file name.</returns>
        public static bool IsValidFileName(this string instance)
        {
            var stripped = instance.Trim();
            if (instance != stripped)
                return false;

            if (string.IsNullOrEmpty(stripped))
                return false;

            return instance.IndexOfAny(_invalidFileNameCharacters) == -1;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this string contains a valid <see langword="float"/>.
        /// This is inclusive of integers, and also supports exponents.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print("1.7".IsValidFloat())  // Prints "True"
        /// GD.Print("24".IsValidFloat())  // Prints "True"
        /// GD.Print("7e3".IsValidFloat())  // Prints "True"
        /// GD.Print("Hello".IsValidFloat())  // Prints "False"
        /// </code>
        /// </example>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string contains a valid floating point number.</returns>
        public static bool IsValidFloat(this string instance)
        {
            return float.TryParse(instance, out _);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this string contains a valid hexadecimal number.
        /// If <paramref name="withPrefix"/> is <see langword="true"/>, then a validity of the
        /// hexadecimal number is determined by <c>0x</c> prefix, for instance: <c>0xDEADC0DE</c>.
        /// </summary>
        /// <param name="instance">The string to check.</param>
        /// <param name="withPrefix">If the string must contain the <c>0x</c> prefix to be valid.</param>
        /// <returns>If the string contains a valid hexadecimal number.</returns>
        public static bool IsValidHexNumber(this string instance, bool withPrefix = false)
        {
            if (string.IsNullOrEmpty(instance))
                return false;

            int from = 0;
            if (instance.Length != 1 && instance[0] == '+' || instance[0] == '-')
            {
                from++;
            }

            if (withPrefix)
            {
                if (instance.Length < 3)
                    return false;
                if (instance[from] != '0' || instance[from + 1] != 'x')
                    return false;
                from += 2;
            }

            for (int i = from; i < instance.Length; i++)
            {
                char c = instance[i];
                if (IsHexDigit(c))
                    continue;

                return false;
            }

            return true;

            static bool IsHexDigit(char c)
            {
                return char.IsDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
            }
        }

        /// <summary>
        /// Returns <see langword="true"/> if this string contains a valid color in hexadecimal
        /// HTML notation. Other HTML notations such as named colors or <c>hsl()</c> aren't
        /// considered valid by this method and will return <see langword="false"/>.
        /// </summary>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string contains a valid HTML color.</returns>
        public static bool IsValidHtmlColor(this string instance)
        {
            return Color.HtmlIsValid(instance);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this string is a valid identifier.
        /// A valid identifier may contain only letters, digits and underscores (<c>_</c>)
        /// and the first character may not be a digit.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print("good_ident_1".IsValidIdentifier())  // Prints "True"
        /// GD.Print("1st_bad_ident".IsValidIdentifier())  // Prints "False"
        /// GD.Print("bad_ident_#2".IsValidIdentifier())  // Prints "False"
        /// </code>
        /// </example>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string contains a valid identifier.</returns>
        public static bool IsValidIdentifier(this string instance)
        {
            int len = instance.Length;

            if (len == 0)
                return false;

            if (instance[0] >= '0' && instance[0] <= '9')
                return false; // Identifiers cannot start with numbers.

            for (int i = 0; i < len; i++)
            {
                bool validChar = instance[i] == '_' ||
                    (instance[i] >= 'a' && instance[i] <= 'z') ||
                    (instance[i] >= 'A' && instance[i] <= 'Z') ||
                    (instance[i] >= '0' && instance[i] <= '9');

                if (!validChar)
                    return false;
            }

            return true;
        }

        /// <summary>
        /// Returns <see langword="true"/> if this string contains a valid <see langword="int"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print("7".IsValidInt())  // Prints "True"
        /// GD.Print("14.6".IsValidInt())  // Prints "False"
        /// GD.Print("L".IsValidInt())  // Prints "False"
        /// GD.Print("+3".IsValidInt())  // Prints "True"
        /// GD.Print("-12".IsValidInt())  // Prints "True"
        /// </code>
        /// </example>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string contains a valid integer.</returns>
        public static bool IsValidInt(this string instance)
        {
            return int.TryParse(instance, out _);
        }

        /// <summary>
        /// Returns <see langword="true"/> if this string contains only a well-formatted
        /// IPv4 or IPv6 address. This method considers reserved IP addresses such as
        /// <c>0.0.0.0</c> as valid.
        /// </summary>
        /// <param name="instance">The string to check.</param>
        /// <returns>If the string contains a valid IP address.</returns>
        public static bool IsValidIPAddress(this string instance)
        {
            if (instance.Contains(':', StringComparison.Ordinal))
            {
                string[] ip = instance.Split(':');

                for (int i = 0; i < ip.Length; i++)
                {
                    string n = ip[i];
                    if (n.Length == 0)
                        continue;

                    if (n.IsValidHexNumber(withPrefix: false))
                    {
                        long nint = n.HexToInt();
                        if (nint < 0 || nint > 0xffff)
                            return false;

                        continue;
                    }

                    if (!n.IsValidIPAddress())
                        return false;
                }
            }
            else
            {
                string[] ip = instance.Split('.');

                if (ip.Length != 4)
                    return false;

                for (int i = 0; i < ip.Length; i++)
                {
                    string n = ip[i];
                    if (!n.IsValidInt())
                        return false;

                    int val = n.ToInt();
                    if (val < 0 || val > 255)
                        return false;
                }
            }

            return true;
        }

        /// <summary>
        /// Returns a copy of the string with special characters escaped using the JSON standard.
        /// </summary>
        /// <param name="instance">The string to escape.</param>
        /// <returns>The escaped string.</returns>
        public static string JSONEscape(this string instance)
        {
            var sb = new StringBuilder(instance);

            sb.Replace("\\", "\\\\");
            sb.Replace("\b", "\\b");
            sb.Replace("\f", "\\f");
            sb.Replace("\n", "\\n");
            sb.Replace("\r", "\\r");
            sb.Replace("\t", "\\t");
            sb.Replace("\v", "\\v");
            sb.Replace("\"", "\\\"");

            return sb.ToString();
        }

        /// <summary>
        /// Returns an amount of characters from the left of the string.
        /// </summary>
        /// <seealso cref="Right(string, int)"/>
        /// <param name="instance">The original string.</param>
        /// <param name="pos">The position in the string where the left side ends.</param>
        /// <returns>The left side of the string from the given position.</returns>
        public static string Left(this string instance, int pos)
        {
            if (pos <= 0)
                return string.Empty;

            if (pos >= instance.Length)
                return instance;

            return instance.Substring(0, pos);
        }

        /// <summary>
        /// Do a simple expression match, where '*' matches zero or more
        /// arbitrary characters and '?' matches any single character except '.'.
        /// </summary>
        /// <param name="str">The string to check.</param>
        /// <param name="pattern">Expression to check.</param>
        /// <param name="caseSensitive">
        /// If <see langword="true"/>, the check will be case sensitive.
        /// </param>
        /// <returns>If the expression has any matches.</returns>
        private static bool WildcardMatch(ReadOnlySpan<char> str, ReadOnlySpan<char> pattern, bool caseSensitive)
        {
            // case '\0':
            if (pattern.IsEmpty)
                return str.IsEmpty;

            switch (pattern[0])
            {
                case '*':
                    return WildcardMatch(str, pattern.Slice(1), caseSensitive)
                        || (!str.IsEmpty && WildcardMatch(str.Slice(1), pattern, caseSensitive));
                case '?':
                    return !str.IsEmpty && str[0] != '.' &&
                        WildcardMatch(str.Slice(1), pattern.Slice(1), caseSensitive);
                default:
                    if (str.IsEmpty)
                        return false;
                    bool charMatches = caseSensitive ?
                        str[0] == pattern[0] :
                        char.ToUpperInvariant(str[0]) == char.ToUpperInvariant(pattern[0]);
                    return charMatches &&
                        WildcardMatch(str.Slice(1), pattern.Slice(1), caseSensitive);
            }
        }

        /// <summary>
        /// Do a simple case sensitive expression match, using ? and * wildcards.
        /// </summary>
        /// <seealso cref="MatchN(string, string)"/>
        /// <param name="instance">The string to check.</param>
        /// <param name="expr">Expression to check.</param>
        /// <param name="caseSensitive">
        /// If <see langword="true"/>, the check will be case sensitive.
        /// </param>
        /// <returns>If the expression has any matches.</returns>
        public static bool Match(this string instance, string expr, bool caseSensitive = true)
        {
            if (instance.Length == 0 || expr.Length == 0)
                return false;

            return WildcardMatch(instance, expr, caseSensitive);
        }

        /// <summary>
        /// Do a simple case insensitive expression match, using ? and * wildcards.
        /// </summary>
        /// <seealso cref="Match(string, string, bool)"/>
        /// <param name="instance">The string to check.</param>
        /// <param name="expr">Expression to check.</param>
        /// <returns>If the expression has any matches.</returns>
        public static bool MatchN(this string instance, string expr)
        {
            if (instance.Length == 0 || expr.Length == 0)
                return false;

            return WildcardMatch(instance, expr, caseSensitive: false);
        }

        /// <summary>
        /// Returns the MD5 hash of the string as an array of bytes.
        /// </summary>
        /// <seealso cref="Md5Text(string)"/>
        /// <param name="instance">The string to hash.</param>
        /// <returns>The MD5 hash of the string.</returns>
        public static byte[] Md5Buffer(this string instance)
        {
#pragma warning disable CA5351 // Do Not Use Broken Cryptographic Algorithms
            return MD5.HashData(Encoding.UTF8.GetBytes(instance));
#pragma warning restore CA5351
        }

        /// <summary>
        /// Returns the MD5 hash of the string as a string.
        /// </summary>
        /// <seealso cref="Md5Buffer(string)"/>
        /// <param name="instance">The string to hash.</param>
        /// <returns>The MD5 hash of the string.</returns>
        public static string Md5Text(this string instance)
        {
            return instance.Md5Buffer().HexEncode();
        }

        /// <summary>
        /// Performs a case-insensitive comparison to another string and returns an integer that indicates their relative position in the sort order.
        /// </summary>
        /// <seealso cref="CasecmpTo(string, string)"/>
        /// <seealso cref="CompareTo(string, string, bool)"/>
        /// <param name="instance">The string to compare.</param>
        /// <param name="to">The other string to compare.</param>
        /// <returns>An integer that indicates the lexical relationship between the two comparands.</returns>
        public static int NocasecmpTo(this string instance, string to)
        {
#pragma warning disable CA1309 // Use ordinal string comparison
            return string.Compare(instance, to, ignoreCase: true, null);
#pragma warning restore CA1309
        }

        /// <summary>
        /// Format a number to have an exact number of <paramref name="digits"/>
        /// after the decimal point.
        /// </summary>
        /// <seealso cref="PadZeros(string, int)"/>
        /// <param name="instance">The string to pad.</param>
        /// <param name="digits">Amount of digits after the decimal point.</param>
        /// <returns>The string padded with zeroes.</returns>
        public static string PadDecimals(this string instance, int digits)
        {
            int c = instance.Find(".");

            if (c == -1)
            {
                if (digits <= 0)
                    return instance;

                instance += ".";
                c = instance.Length - 1;
            }
            else
            {
                if (digits <= 0)
                    return instance.Substring(0, c);
            }

            if (instance.Length - (c + 1) > digits)
            {
                instance = instance.Substring(0, c + digits + 1);
            }
            else
            {
                while (instance.Length - (c + 1) < digits)
                {
                    instance += "0";
                }
            }

            return instance;
        }

        /// <summary>
        /// Format a number to have an exact number of <paramref name="digits"/>
        /// before the decimal point.
        /// </summary>
        /// <seealso cref="PadDecimals(string, int)"/>
        /// <param name="instance">The string to pad.</param>
        /// <param name="digits">Amount of digits before the decimal point.</param>
        /// <returns>The string padded with zeroes.</returns>
        public static string PadZeros(this string instance, int digits)
        {
            string s = instance;
            int end = s.Find(".");

            if (end == -1)
                end = s.Length;

            if (end == 0)
                return s;

            int begin = 0;

            while (begin < end && (s[begin] < '0' || s[begin] > '9'))
            {
                begin++;
            }

            if (begin >= end)
                return s;

            while (end - begin < digits)
            {
                s = s.Insert(begin, "0");
                end++;
            }

            return s;
        }

        /// <summary>
        /// If the string is a path, this concatenates <paramref name="file"/>
        /// at the end of the string as a subpath.
        /// E.g. <c>"this/is".PathJoin("path") == "this/is/path"</c>.
        /// </summary>
        /// <param name="instance">The path that will be concatenated.</param>
        /// <param name="file">File name to concatenate with the path.</param>
        /// <returns>The concatenated path with the given file name.</returns>
        public static string PathJoin(this string instance, string file)
        {
            if (instance.Length > 0 && instance[instance.Length - 1] == '/')
                return instance + file;
            return instance + "/" + file;
        }

        /// <summary>
        /// Replace occurrences of a substring for different ones inside the string, but search case-insensitive.
        /// </summary>
        /// <seealso cref="string.Replace(string, string, StringComparison)"/>
        /// <param name="instance">The string to modify.</param>
        /// <param name="what">The substring to be replaced in the string.</param>
        /// <param name="forwhat">The substring that replaces <paramref name="what"/>.</param>
        /// <returns>The string with the substring occurrences replaced.</returns>
        public static string ReplaceN(this string instance, string what, string forwhat)
        {
            return Regex.Replace(instance, what, forwhat, RegexOptions.IgnoreCase);
        }

        /// <summary>
        /// Returns the index of the last occurrence of the specified string in this instance,
        /// or <c>-1</c>. Optionally, the starting search index can be specified, continuing to
        /// the beginning of the string.
        /// </summary>
        /// <seealso cref="Find(string, string, int, bool)"/>
        /// <seealso cref="Find(string, char, int, bool)"/>
        /// <seealso cref="FindN(string, string, int)"/>
        /// <seealso cref="RFindN(string, string, int)"/>
        /// <param name="instance">The string that will be searched.</param>
        /// <param name="what">The substring to search in the string.</param>
        /// <param name="from">The position at which to start searching.</param>
        /// <param name="caseSensitive">If <see langword="true"/>, the search is case sensitive.</param>
        /// <returns>The position at which the substring was found, or -1 if not found.</returns>
        public static int RFind(this string instance, string what, int from = -1, bool caseSensitive = true)
        {
            if (from == -1)
                from = instance.Length - 1;

            return instance.LastIndexOf(what, from,
                caseSensitive ? StringComparison.Ordinal : StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Returns the index of the last case-insensitive occurrence of the specified string in this instance,
        /// or <c>-1</c>. Optionally, the starting search index can be specified, continuing to
        /// the beginning of the string.
        /// </summary>
        /// <seealso cref="Find(string, string, int, bool)"/>
        /// <seealso cref="Find(string, char, int, bool)"/>
        /// <seealso cref="FindN(string, string, int)"/>
        /// <seealso cref="RFind(string, string, int, bool)"/>
        /// <param name="instance">The string that will be searched.</param>
        /// <param name="what">The substring to search in the string.</param>
        /// <param name="from">The position at which to start searching.</param>
        /// <returns>The position at which the substring was found, or -1 if not found.</returns>
        public static int RFindN(this string instance, string what, int from = -1)
        {
            if (from == -1)
                from = instance.Length - 1;

            return instance.LastIndexOf(what, from, StringComparison.OrdinalIgnoreCase);
        }

        /// <summary>
        /// Returns the right side of the string from a given position.
        /// </summary>
        /// <seealso cref="Left(string, int)"/>
        /// <param name="instance">The original string.</param>
        /// <param name="pos">The position in the string from which the right side starts.</param>
        /// <returns>The right side of the string from the given position.</returns>
        public static string Right(this string instance, int pos)
        {
            if (pos >= instance.Length)
                return instance;

            if (pos < 0)
                return string.Empty;

            return instance.Substring(pos, instance.Length - pos);
        }

        /// <summary>
        /// Returns the SHA-1 hash of the string as an array of bytes.
        /// </summary>
        /// <seealso cref="Sha1Text(string)"/>
        /// <param name="instance">The string to hash.</param>
        /// <returns>The SHA-1 hash of the string.</returns>
        public static byte[] Sha1Buffer(this string instance)
        {
#pragma warning disable CA5350 // Do Not Use Weak Cryptographic Algorithms
            return SHA1.HashData(Encoding.UTF8.GetBytes(instance));
#pragma warning restore CA5350
        }

        /// <summary>
        /// Returns the SHA-1 hash of the string as a string.
        /// </summary>
        /// <seealso cref="Sha1Buffer(string)"/>
        /// <param name="instance">The string to hash.</param>
        /// <returns>The SHA-1 hash of the string.</returns>
        public static string Sha1Text(this string instance)
        {
            return instance.Sha1Buffer().HexEncode();
        }

        /// <summary>
        /// Returns the SHA-256 hash of the string as an array of bytes.
        /// </summary>
        /// <seealso cref="Sha256Text(string)"/>
        /// <param name="instance">The string to hash.</param>
        /// <returns>The SHA-256 hash of the string.</returns>
        public static byte[] Sha256Buffer(this string instance)
        {
            return SHA256.HashData(Encoding.UTF8.GetBytes(instance));
        }

        /// <summary>
        /// Returns the SHA-256 hash of the string as a string.
        /// </summary>
        /// <seealso cref="Sha256Buffer(string)"/>
        /// <param name="instance">The string to hash.</param>
        /// <returns>The SHA-256 hash of the string.</returns>
        public static string Sha256Text(this string instance)
        {
            return instance.Sha256Buffer().HexEncode();
        }

        /// <summary>
        /// Returns the similarity index of the text compared to this string.
        /// 1 means totally similar and 0 means totally dissimilar.
        /// </summary>
        /// <param name="instance">The string to compare.</param>
        /// <param name="text">The other string to compare.</param>
        /// <returns>The similarity index.</returns>
        public static float Similarity(this string instance, string text)
        {
            if (instance == text)
            {
                // Equal strings are totally similar
                return 1.0f;
            }

            if (instance.Length < 2 || text.Length < 2)
            {
                // No way to calculate similarity without a single bigram
                return 0.0f;
            }

            string[] sourceBigrams = instance.Bigrams();
            string[] targetBigrams = text.Bigrams();

            int sourceSize = sourceBigrams.Length;
            int targetSize = targetBigrams.Length;

            float sum = sourceSize + targetSize;
            float inter = 0;

            for (int i = 0; i < sourceSize; i++)
            {
                for (int j = 0; j < targetSize; j++)
                {
                    if (sourceBigrams[i] == targetBigrams[j])
                    {
                        inter++;
                        break;
                    }
                }
            }

            return 2.0f * inter / sum;
        }

        /// <summary>
        /// Returns a simplified canonical path.
        /// </summary>
        public static string SimplifyPath(this string instance)
        {
            using godot_string instanceStr = Marshaling.ConvertStringToNative(instance);
            NativeFuncs.godotsharp_string_simplify_path(instanceStr, out godot_string simplifiedPath);
            using (simplifiedPath)
                return Marshaling.ConvertStringToManaged(simplifiedPath);
        }

        /// <summary>
        /// Split the string by a divisor string, return an array of the substrings.
        /// Example "One,Two,Three" will return ["One","Two","Three"] if split by ",".
        /// </summary>
        /// <seealso cref="SplitFloats(string, string, bool)"/>
        /// <param name="instance">The string to split.</param>
        /// <param name="divisor">The divisor string that splits the string.</param>
        /// <param name="allowEmpty">
        /// If <see langword="true"/>, the array may include empty strings.
        /// </param>
        /// <returns>The array of strings split from the string.</returns>
        public static string[] Split(this string instance, string divisor, bool allowEmpty = true)
        {
            return instance.Split(divisor,
                allowEmpty ? StringSplitOptions.None : StringSplitOptions.RemoveEmptyEntries);
        }

        /// <summary>
        /// Split the string in floats by using a divisor string, return an array of the substrings.
        /// Example "1,2.5,3" will return [1,2.5,3] if split by ",".
        /// </summary>
        /// <seealso cref="Split(string, string, bool)"/>
        /// <param name="instance">The string to split.</param>
        /// <param name="divisor">The divisor string that splits the string.</param>
        /// <param name="allowEmpty">
        /// If <see langword="true"/>, the array may include empty floats.
        /// </param>
        /// <returns>The array of floats split from the string.</returns>
        public static float[] SplitFloats(this string instance, string divisor, bool allowEmpty = true)
        {
            var ret = new List<float>();
            int from = 0;
            int len = instance.Length;

            while (true)
            {
                int end = instance.Find(divisor, from, caseSensitive: true);
                if (end < 0)
                    end = len;
                if (allowEmpty || end > from)
                    ret.Add(float.Parse(instance.AsSpan(from), CultureInfo.InvariantCulture));
                if (end == len)
                    break;

                from = end + divisor.Length;
            }

            return ret.ToArray();
        }

        private static readonly char[] _nonPrintable =
        {
            (char)00, (char)01, (char)02, (char)03, (char)04, (char)05,
            (char)06, (char)07, (char)08, (char)09, (char)10, (char)11,
            (char)12, (char)13, (char)14, (char)15, (char)16, (char)17,
            (char)18, (char)19, (char)20, (char)21, (char)22, (char)23,
            (char)24, (char)25, (char)26, (char)27, (char)28, (char)29,
            (char)30, (char)31, (char)32
        };

        /// <summary>
        /// Returns a copy of the string stripped of any non-printable character
        /// (including tabulations, spaces and line breaks) at the beginning and the end.
        /// The optional arguments are used to toggle stripping on the left and right
        /// edges respectively.
        /// </summary>
        /// <param name="instance">The string to strip.</param>
        /// <param name="left">If the left side should be stripped.</param>
        /// <param name="right">If the right side should be stripped.</param>
        /// <returns>The string stripped of any non-printable characters.</returns>
        public static string StripEdges(this string instance, bool left = true, bool right = true)
        {
            if (left)
            {
                if (right)
                    return instance.Trim(_nonPrintable);
                return instance.TrimStart(_nonPrintable);
            }

            return instance.TrimEnd(_nonPrintable);
        }


        /// <summary>
        /// Returns a copy of the string stripped of any escape character.
        /// These include all non-printable control characters of the first page
        /// of the ASCII table (&lt; 32), such as tabulation (<c>\t</c>) and
        /// newline (<c>\n</c> and <c>\r</c>) characters, but not spaces.
        /// </summary>
        /// <param name="instance">The string to strip.</param>
        /// <returns>The string stripped of any escape characters.</returns>
        public static string StripEscapes(this string instance)
        {
            var sb = new StringBuilder();
            for (int i = 0; i < instance.Length; i++)
            {
                // Escape characters on first page of the ASCII table, before 32 (Space).
                if (instance[i] < 32)
                    continue;

                sb.Append(instance[i]);
            }

            return sb.ToString();
        }

        /// <summary>
        /// Returns part of the string from the position <paramref name="from"/>, with length <paramref name="len"/>.
        /// </summary>
        /// <param name="instance">The string to slice.</param>
        /// <param name="from">The position in the string that the part starts from.</param>
        /// <param name="len">The length of the returned part.</param>
        /// <returns>
        /// Part of the string from the position <paramref name="from"/>, with length <paramref name="len"/>.
        /// </returns>
        public static string Substr(this string instance, int from, int len)
        {
            int max = instance.Length - from;
            return instance.Substring(from, len > max ? max : len);
        }

        /// <summary>
        /// Converts the String (which is a character array) to PackedByteArray (which is an array of bytes).
        /// The conversion is faster compared to <see cref="ToUtf8Buffer(string)"/>,
        /// as this method assumes that all the characters in the String are ASCII characters.
        /// </summary>
        /// <seealso cref="ToUtf8Buffer(string)"/>
        /// <seealso cref="ToUtf16Buffer(string)"/>
        /// <seealso cref="ToUtf32Buffer(string)"/>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The string as ASCII encoded bytes.</returns>
        public static byte[] ToAsciiBuffer(this string instance)
        {
            return Encoding.ASCII.GetBytes(instance);
        }

        /// <summary>
        /// Converts a string, containing a decimal number, into a <see langword="float" />.
        /// </summary>
        /// <seealso cref="ToInt(string)"/>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The number representation of the string.</returns>
        public static float ToFloat(this string instance)
        {
            return float.Parse(instance, CultureInfo.InvariantCulture);
        }

        /// <summary>
        /// Converts a string, containing an integer number, into an <see langword="int" />.
        /// </summary>
        /// <seealso cref="ToFloat(string)"/>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The number representation of the string.</returns>
        public static int ToInt(this string instance)
        {
            return int.Parse(instance, CultureInfo.InvariantCulture);
        }

        /// <summary>
        /// Converts the string (which is an array of characters) to a UTF-16 encoded array of bytes.
        /// </summary>
        /// <seealso cref="ToAsciiBuffer(string)"/>
        /// <seealso cref="ToUtf32Buffer(string)"/>
        /// <seealso cref="ToUtf8Buffer(string)"/>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The string as UTF-16 encoded bytes.</returns>
        public static byte[] ToUtf16Buffer(this string instance)
        {
            return Encoding.Unicode.GetBytes(instance);
        }

        /// <summary>
        /// Converts the string (which is an array of characters) to a UTF-32 encoded array of bytes.
        /// </summary>
        /// <seealso cref="ToAsciiBuffer(string)"/>
        /// <seealso cref="ToUtf16Buffer(string)"/>
        /// <seealso cref="ToUtf8Buffer(string)"/>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The string as UTF-32 encoded bytes.</returns>
        public static byte[] ToUtf32Buffer(this string instance)
        {
            return Encoding.UTF32.GetBytes(instance);
        }

        /// <summary>
        /// Converts the string (which is an array of characters) to a UTF-8 encoded array of bytes.
        /// The conversion is a bit slower than <see cref="ToAsciiBuffer(string)"/>,
        /// but supports all UTF-8 characters. Therefore, you should prefer this function
        /// over <see cref="ToAsciiBuffer(string)"/>.
        /// </summary>
        /// <seealso cref="ToAsciiBuffer(string)"/>
        /// <seealso cref="ToUtf16Buffer(string)"/>
        /// <seealso cref="ToUtf32Buffer(string)"/>
        /// <param name="instance">The string to convert.</param>
        /// <returns>The string as UTF-8 encoded bytes.</returns>
        public static byte[] ToUtf8Buffer(this string instance)
        {
            return Encoding.UTF8.GetBytes(instance);
        }

        /// <summary>
        /// Removes a given string from the start if it starts with it or leaves the string unchanged.
        /// </summary>
        /// <param name="instance">The string to remove the prefix from.</param>
        /// <param name="prefix">The string to remove from the start.</param>
        /// <returns>A copy of the string with the prefix string removed from the start.</returns>
        public static string TrimPrefix(this string instance, string prefix)
        {
            if (instance.StartsWith(prefix, StringComparison.Ordinal))
                return instance.Substring(prefix.Length);

            return instance;
        }

        /// <summary>
        /// Removes a given string from the end if it ends with it or leaves the string unchanged.
        /// </summary>
        /// <param name="instance">The string to remove the suffix from.</param>
        /// <param name="suffix">The string to remove from the end.</param>
        /// <returns>A copy of the string with the suffix string removed from the end.</returns>
        public static string TrimSuffix(this string instance, string suffix)
        {
            if (instance.EndsWith(suffix, StringComparison.Ordinal))
                return instance.Substring(0, instance.Length - suffix.Length);

            return instance;
        }

        /// <summary>
        /// Decodes a string in URL encoded format. This is meant to
        /// decode parameters in a URL when receiving an HTTP request.
        /// This mostly wraps around <see cref="Uri.UnescapeDataString"/>,
        /// but also handles <c>+</c>.
        /// See <see cref="URIEncode"/> for encoding.
        /// </summary>
        /// <param name="instance">The string to decode.</param>
        /// <returns>The unescaped string.</returns>
        public static string URIDecode(this string instance)
        {
            return Uri.UnescapeDataString(instance.Replace("+", "%20", StringComparison.Ordinal));
        }

        /// <summary>
        /// Encodes a string to URL friendly format. This is meant to
        /// encode parameters in a URL when sending an HTTP request.
        /// This wraps around <see cref="Uri.EscapeDataString"/>.
        /// See <see cref="URIDecode"/> for decoding.
        /// </summary>
        /// <param name="instance">The string to encode.</param>
        /// <returns>The escaped string.</returns>
        public static string URIEncode(this string instance)
        {
            return Uri.EscapeDataString(instance);
        }

        private const string UniqueNodePrefix = "%";
        private static readonly string[] _invalidNodeNameCharacters = { ".", ":", "@", "/", "\"", UniqueNodePrefix };

        /// <summary>
        /// Removes any characters from the string that are prohibited in
        /// <see cref="Node"/> names (<c>.</c> <c>:</c> <c>@</c> <c>/</c> <c>"</c>).
        /// </summary>
        /// <param name="instance">The string to sanitize.</param>
        /// <returns>The string sanitized as a valid node name.</returns>
        public static string ValidateNodeName(this string instance)
        {
            string name = instance.Replace(_invalidNodeNameCharacters[0], "", StringComparison.Ordinal);
            for (int i = 1; i < _invalidNodeNameCharacters.Length; i++)
            {
                name = name.Replace(_invalidNodeNameCharacters[i], "", StringComparison.Ordinal);
            }
            return name;
        }

        /// <summary>
        /// Returns a copy of the string with special characters escaped using the XML standard.
        /// </summary>
        /// <seealso cref="XMLUnescape(string)"/>
        /// <param name="instance">The string to escape.</param>
        /// <returns>The escaped string.</returns>
        public static string XMLEscape(this string instance)
        {
            return SecurityElement.Escape(instance);
        }

        /// <summary>
        /// Returns a copy of the string with escaped characters replaced by their meanings
        /// according to the XML standard.
        /// </summary>
        /// <seealso cref="XMLEscape(string)"/>
        /// <param name="instance">The string to unescape.</param>
        /// <returns>The unescaped string.</returns>
        public static string? XMLUnescape(this string instance)
        {
            return SecurityElement.FromString(instance)?.Text;
        }
    }
}
