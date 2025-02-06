using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using Godot.NativeInterop;

namespace Godot
{
    /// <summary>
    /// Godot's global functions.
    /// </summary>
    public static partial class GD
    {
        /// <summary>
        /// Decodes a byte array back to a <see cref="Variant"/> value, without decoding objects.
        /// Note: If you need object deserialization, see <see cref="BytesToVarWithObjects"/>.
        /// </summary>
        /// <param name="bytes">Byte array that will be decoded to a <see cref="Variant"/>.</param>
        /// <returns>The decoded <see cref="Variant"/>.</returns>
        public static Variant BytesToVar(Span<byte> bytes)
        {
            using var varBytes = Marshaling.ConvertSystemArrayToNativePackedByteArray(bytes);
            NativeFuncs.godotsharp_bytes_to_var(varBytes, godot_bool.False, out godot_variant ret);
            return Variant.CreateTakingOwnershipOfDisposableValue(ret);
        }

        /// <summary>
        /// Decodes a byte array back to a <see cref="Variant"/> value. Decoding objects is allowed.
        /// Warning: Deserialized object can contain code which gets executed. Do not use this
        /// option if the serialized object comes from untrusted sources to avoid potential security
        /// threats (remote code execution).
        /// </summary>
        /// <param name="bytes">Byte array that will be decoded to a <see cref="Variant"/>.</param>
        /// <returns>The decoded <see cref="Variant"/>.</returns>
        public static Variant BytesToVarWithObjects(Span<byte> bytes)
        {
            using var varBytes = Marshaling.ConvertSystemArrayToNativePackedByteArray(bytes);
            NativeFuncs.godotsharp_bytes_to_var(varBytes, godot_bool.True, out godot_variant ret);
            return Variant.CreateTakingOwnershipOfDisposableValue(ret);
        }

        /// <summary>
        /// Converts <paramref name="what"/> to <paramref name="type"/> in the best way possible.
        /// The <paramref name="type"/> parameter uses the <see cref="Variant.Type"/> values.
        /// </summary>
        /// <example>
        /// <code>
        /// Variant a = new Godot.Collections.Array { 4, 2.5, 1.2 };
        /// GD.Print(a.VariantType == Variant.Type.Array); // Prints true
        ///
        /// var b = GD.Convert(a, Variant.Type.PackedByteArray);
        /// GD.Print(b); // Prints [4, 2, 1]
        /// GD.Print(b.VariantType == Variant.Type.Array); // Prints false
        /// </code>
        /// </example>
        /// <returns>The <c>Variant</c> converted to the given <paramref name="type"/>.</returns>
        public static Variant Convert(Variant what, Variant.Type type)
        {
            NativeFuncs.godotsharp_convert((godot_variant)what.NativeVar, (int)type, out godot_variant ret);
            return Variant.CreateTakingOwnershipOfDisposableValue(ret);
        }

        /// <summary>
        /// Returns the integer hash of the passed <paramref name="var"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(GD.Hash("a")); // Prints 177670
        /// </code>
        /// </example>
        /// <param name="var">Variable that will be hashed.</param>
        /// <returns>Hash of the variable passed.</returns>
        public static int Hash(Variant var)
        {
            return NativeFuncs.godotsharp_hash((godot_variant)var.NativeVar);
        }

        /// <summary>
        /// Loads a resource from the filesystem located at <paramref name="path"/>.
        /// The resource is loaded on the method call (unless it's referenced already
        /// elsewhere, e.g. in another script or in the scene), which might cause slight delay,
        /// especially when loading scenes. To avoid unnecessary delays when loading something
        /// multiple times, either store the resource in a variable.
        ///
        /// Note: Resource paths can be obtained by right-clicking on a resource in the FileSystem
        /// dock and choosing "Copy Path" or by dragging the file from the FileSystem dock into the script.
        ///
        /// Important: The path must be absolute, a local path will just return <see langword="null"/>.
        /// This method is a simplified version of <see cref="ResourceLoader.Load"/>, which can be used
        /// for more advanced scenarios.
        /// </summary>
        /// <example>
        /// <code>
        /// // Load a scene called main located in the root of the project directory and cache it in a variable.
        /// var main = GD.Load("res://main.tscn"); // main will contain a PackedScene resource.
        /// </code>
        /// </example>
        /// <param name="path">Path of the <see cref="Resource"/> to load.</param>
        /// <returns>The loaded <see cref="Resource"/>.</returns>
        public static Resource Load(string path)
        {
            return ResourceLoader.Load(path);
        }

        /// <summary>
        /// Loads a resource from the filesystem located at <paramref name="path"/>.
        /// The resource is loaded on the method call (unless it's referenced already
        /// elsewhere, e.g. in another script or in the scene), which might cause slight delay,
        /// especially when loading scenes. To avoid unnecessary delays when loading something
        /// multiple times, either store the resource in a variable.
        ///
        /// Note: Resource paths can be obtained by right-clicking on a resource in the FileSystem
        /// dock and choosing "Copy Path" or by dragging the file from the FileSystem dock into the script.
        ///
        /// Important: The path must be absolute, a local path will just return <see langword="null"/>.
        /// This method is a simplified version of <see cref="ResourceLoader.Load"/>, which can be used
        /// for more advanced scenarios.
        /// </summary>
        /// <example>
        /// <code>
        /// // Load a scene called main located in the root of the project directory and cache it in a variable.
        /// var main = GD.Load&lt;PackedScene&gt;("res://main.tscn"); // main will contain a PackedScene resource.
        /// </code>
        /// </example>
        /// <param name="path">Path of the <see cref="Resource"/> to load.</param>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Resource"/>.</typeparam>
        public static T Load<T>(string path) where T : class
        {
            return ResourceLoader.Load<T>(path);
        }

        private static string AppendPrintParams(object[] parameters)
        {
            if (parameters == null)
            {
                return "null";
            }

            var sb = new StringBuilder();
            for (int i = 0; i < parameters.Length; i++)
            {
                sb.Append(parameters[i]?.ToString() ?? "null");
            }
            return sb.ToString();
        }

        private static string AppendPrintParams(char separator, object[] parameters)
        {
            if (parameters == null)
            {
                return "null";
            }

            var sb = new StringBuilder();
            for (int i = 0; i < parameters.Length; i++)
            {
                if (i != 0)
                    sb.Append(separator);
                sb.Append(parameters[i]?.ToString() ?? "null");
            }
            return sb.ToString();
        }

        /// <summary>
        /// Prints a message to the console.
        ///
        /// Note: Consider using <see cref="PushError(string)"/> and <see cref="PushWarning(string)"/>
        /// to print error and warning messages instead of <see cref="Print(string)"/>.
        /// This distinguishes them from print messages used for debugging purposes,
        /// while also displaying a stack trace when an error or warning is printed.
        /// </summary>
        /// <param name="what">Message that will be printed.</param>
        public static void Print(string what)
        {
            using var godotStr = Marshaling.ConvertStringToNative(what);
            NativeFuncs.godotsharp_print(godotStr);
        }

        /// <summary>
        /// Converts one or more arguments of any type to string in the best way possible
        /// and prints them to the console.
        ///
        /// Note: Consider using <see cref="PushError(object[])"/> and <see cref="PushWarning(object[])"/>
        /// to print error and warning messages instead of <see cref="Print(object[])"/>.
        /// This distinguishes them from print messages used for debugging purposes,
        /// while also displaying a stack trace when an error or warning is printed.
        /// </summary>
        /// <example>
        /// <code>
        /// var a = new Godot.Collections.Array { 1, 2, 3 };
        /// GD.Print("a", "b", a); // Prints ab[1, 2, 3]
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void Print(params object[] what)
        {
            Print(AppendPrintParams(what));
        }

        /// <summary>
        /// Prints a message to the console.
        /// The following BBCode tags are supported: b, i, u, s, indent, code, url, center,
        /// right, color, bgcolor, fgcolor.
        /// Color tags only support named colors such as <c>red</c>, not hexadecimal color codes.
        /// Unsupported tags will be left as-is in standard output.
        /// When printing to standard output, the supported subset of BBCode is converted to
        /// ANSI escape codes for the terminal emulator to display. Displaying ANSI escape codes
        /// is currently only supported on Linux and macOS. Support for ANSI escape codes may vary
        /// across terminal emulators, especially for italic and strikethrough.
        ///
        /// Note: Consider using <see cref="PushError(string)"/> and <see cref="PushWarning(string)"/>
        /// to print error and warning messages instead of <see cref="Print(string)"/> or
        /// <see cref="PrintRich(string)"/>.
        /// This distinguishes them from print messages used for debugging purposes,
        /// while also displaying a stack trace when an error or warning is printed.
        /// </summary>
        /// <param name="what">Message that will be printed.</param>
        public static void PrintRich(string what)
        {
            using var godotStr = Marshaling.ConvertStringToNative(what);
            NativeFuncs.godotsharp_print_rich(godotStr);
        }

        /// <summary>
        /// Converts one or more arguments of any type to string in the best way possible
        /// and prints them to the console.
        /// The following BBCode tags are supported: b, i, u, s, indent, code, url, center,
        /// right, color, bgcolor, fgcolor.
        /// Color tags only support named colors such as <c>red</c>, not hexadecimal color codes.
        /// Unsupported tags will be left as-is in standard output.
        /// When printing to standard output, the supported subset of BBCode is converted to
        /// ANSI escape codes for the terminal emulator to display. Displaying ANSI escape codes
        /// is currently only supported on Linux and macOS. Support for ANSI escape codes may vary
        /// across terminal emulators, especially for italic and strikethrough.
        ///
        /// Note: Consider using <see cref="PushError(object[])"/> and <see cref="PushWarning(object[])"/>
        /// to print error and warning messages instead of <see cref="Print(object[])"/> or
        /// <see cref="PrintRich(object[])"/>.
        /// This distinguishes them from print messages used for debugging purposes,
        /// while also displaying a stack trace when an error or warning is printed.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PrintRich("[code][b]Hello world![/b][/code]"); // Prints out: [b]Hello world![/b]
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void PrintRich(params object[] what)
        {
            PrintRich(AppendPrintParams(what));
        }

        /// <summary>
        /// Prints a message to standard error line.
        /// </summary>
        /// <param name="what">Message that will be printed.</param>
        public static void PrintErr(string what)
        {
            using var godotStr = Marshaling.ConvertStringToNative(what);
            NativeFuncs.godotsharp_printerr(godotStr);
        }

        /// <summary>
        /// Prints one or more arguments to strings in the best way possible to standard error line.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PrintErr("prints to stderr");
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void PrintErr(params object[] what)
        {
            PrintErr(AppendPrintParams(what));
        }

        /// <summary>
        /// Prints a message to the OS terminal.
        /// Unlike <see cref="Print(string)"/>, no newline is added at the end.
        /// </summary>
        /// <param name="what">Message that will be printed.</param>
        public static void PrintRaw(string what)
        {
            using var godotStr = Marshaling.ConvertStringToNative(what);
            NativeFuncs.godotsharp_printraw(godotStr);
        }

        /// <summary>
        /// Prints one or more arguments to strings in the best way possible to the OS terminal.
        /// Unlike <see cref="Print(object[])"/>, no newline is added at the end.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PrintRaw("A");
        /// GD.PrintRaw("B");
        /// GD.PrintRaw("C");
        /// // Prints ABC to terminal
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void PrintRaw(params object[] what)
        {
            PrintRaw(AppendPrintParams(what));
        }

        /// <summary>
        /// Prints one or more arguments to the console with a space between each argument.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PrintS("A", "B", "C"); // Prints A B C
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void PrintS(params object[] what)
        {
            string message = AppendPrintParams(' ', what);
            using var godotStr = Marshaling.ConvertStringToNative(message);
            NativeFuncs.godotsharp_prints(godotStr);
        }

        /// <summary>
        /// Prints one or more arguments to the console with a tab between each argument.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PrintT("A", "B", "C"); // Prints A       B       C
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void PrintT(params object[] what)
        {
            string message = AppendPrintParams('\t', what);
            using var godotStr = Marshaling.ConvertStringToNative(message);
            NativeFuncs.godotsharp_printt(godotStr);
        }

        [StackTraceHidden]
        private static void ErrPrintError(string message, godot_error_handler_type type = godot_error_handler_type.ERR_HANDLER_ERROR)
        {
            // Skip 1 frame to avoid current method.
            var stackFrame = DebuggingUtils.GetCurrentStackFrame(skipFrames: 1);
            string callerFilePath = ProjectSettings.LocalizePath(stackFrame.GetFileName());
            DebuggingUtils.GetStackFrameMethodDecl(stackFrame, out string callerName);
            int callerLineNumber = stackFrame.GetFileLineNumber();

            using godot_string messageStr = Marshaling.ConvertStringToNative(message);
            using godot_string callerNameStr = Marshaling.ConvertStringToNative(callerName);
            using godot_string callerFilePathStr = Marshaling.ConvertStringToNative(callerFilePath);
            NativeFuncs.godotsharp_err_print_error(callerNameStr, callerFilePathStr, callerLineNumber, messageStr, p_type: type);
        }

        /// <summary>
        /// Pushes an error message to Godot's built-in debugger and to the OS terminal.
        ///
        /// Note: Errors printed this way will not pause project execution.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PushError("test error"); // Prints "test error" to debugger and terminal as error call
        /// </code>
        /// </example>
        /// <param name="message">Error message.</param>
        public static void PushError(string message)
        {
            ErrPrintError(message);
        }

        /// <summary>
        /// Pushes an error message to Godot's built-in debugger and to the OS terminal.
        ///
        /// Note: Errors printed this way will not pause project execution.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PushError("test_error"); // Prints "test error" to debugger and terminal as error call
        /// </code>
        /// </example>
        /// <param name="what">Arguments that form the error message.</param>
        public static void PushError(params object[] what)
        {
            ErrPrintError(AppendPrintParams(what));
        }

        /// <summary>
        /// Pushes a warning message to Godot's built-in debugger and to the OS terminal.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PushWarning("test warning"); // Prints "test warning" to debugger and terminal as warning call
        /// </code>
        /// </example>
        /// <param name="message">Warning message.</param>
        public static void PushWarning(string message)
        {
            ErrPrintError(message, type: godot_error_handler_type.ERR_HANDLER_WARNING);
        }

        /// <summary>
        /// Pushes a warning message to Godot's built-in debugger and to the OS terminal.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PushWarning("test warning"); // Prints "test warning" to debugger and terminal as warning call
        /// </code>
        /// </example>
        /// <param name="what">Arguments that form the warning message.</param>
        public static void PushWarning(params object[] what)
        {
            ErrPrintError(AppendPrintParams(what), type: godot_error_handler_type.ERR_HANDLER_WARNING);
        }

        /// <summary>
        /// Returns a random floating point value between <c>0.0</c> and <c>1.0</c> (inclusive).
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Randf(); // Returns e.g. 0.375671
        /// </code>
        /// </example>
        /// <returns>A random <see langword="float"/> number.</returns>
        public static float Randf()
        {
            return NativeFuncs.godotsharp_randf();
        }

        /// <summary>
        /// Returns a normally-distributed pseudo-random floating point value
        /// using Box-Muller transform with the specified <pararmref name="mean"/>
        /// and a standard <paramref name="deviation"/>.
        /// This is also called Gaussian distribution.
        /// </summary>
        /// <returns>A random normally-distributed <see langword="float"/> number.</returns>
        public static double Randfn(double mean, double deviation)
        {
            return NativeFuncs.godotsharp_randfn(mean, deviation);
        }

        /// <summary>
        /// Returns a random unsigned 32-bit integer.
        /// Use remainder to obtain a random value in the interval <c>[0, N - 1]</c>
        /// (where N is smaller than 2^32).
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Randi();           // Returns random integer between 0 and 2^32 - 1
        /// GD.Randi() % 20;      // Returns random integer between 0 and 19
        /// GD.Randi() % 100;     // Returns random integer between 0 and 99
        /// GD.Randi() % 100 + 1; // Returns random integer between 1 and 100
        /// </code>
        /// </example>
        /// <returns>A random <see langword="uint"/> number.</returns>
        public static uint Randi()
        {
            return NativeFuncs.godotsharp_randi();
        }

        /// <summary>
        /// Returns a random boolean with a bias of <paramref name="true_chance"/> to be <see langword="true"/> and <see langword="false"/> otherwise.
        /// </summary>
        /// <returns>A random <see langword="bool"/>.</returns>
        public static bool RandBool(double true_chance = 0.5)
        {
            return NativeFuncs.godotsharp_rand_bool(true_chance);
        }

        /// <summary>
        /// Randomizes the seed (or the internal state) of the random number generator.
        /// The current implementation uses a number based on the device's time.
        ///
        /// Note: This method is called automatically when the project is run.
        /// If you need to fix the seed to have consistent, reproducible results,
        /// use <see cref="Seed(ulong)"/> to initialize the random number generator.
        /// </summary>
        public static void Randomize()
        {
            NativeFuncs.godotsharp_randomize();
        }

        /// <summary>
        /// Returns a random floating point value between <paramref name="from"/>
        /// and <paramref name="to"/> (inclusive).
        /// </summary>
        /// <example>
        /// <code>
        /// GD.RandRange(0.0, 20.5);   // Returns e.g. 7.45315
        /// GD.RandRange(-10.0, 10.0); // Returns e.g. -3.844535
        /// </code>
        /// </example>
        /// <returns>A random <see langword="double"/> number inside the given range.</returns>
        public static double RandRange(double from, double to)
        {
            return NativeFuncs.godotsharp_randf_range(from, to);
        }

        /// <summary>
        /// Returns a random signed 32-bit integer between <paramref name="from"/>
        /// and <paramref name="to"/> (inclusive). If <paramref name="to"/> is lesser than
        /// <paramref name="from"/>, they are swapped.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.RandRange(0, 1);      // Returns either 0 or 1
        /// GD.RandRange(-10, 1000); // Returns random integer between -10 and 1000
        /// </code>
        /// </example>
        /// <returns>A random <see langword="int"/> number inside the given range.</returns>
        public static int RandRange(int from, int to)
        {
            return NativeFuncs.godotsharp_randi_range(from, to);
        }

        /// <summary>
        /// Given a <paramref name="seed"/>, returns a randomized <see langword="uint"/>
        /// value. The <paramref name="seed"/> may be modified.
        /// Passing the same <paramref name="seed"/> consistently returns the same value.
        ///
        /// Note: "Seed" here refers to the internal state of the pseudo random number
        /// generator, currently implemented as a 64 bit integer.
        /// </summary>
        /// <example>
        /// <code>
        /// var a = GD.RandFromSeed(4);
        /// </code>
        /// </example>
        /// <param name="seed">
        /// Seed to use to generate the random number.
        /// If a different seed is used, its value will be modified.
        /// </param>
        /// <returns>A random <see langword="uint"/> number.</returns>
        public static uint RandFromSeed(ref ulong seed)
        {
            return NativeFuncs.godotsharp_rand_from_seed(seed, out seed);
        }

        /// <summary>
        /// Returns a <see cref="IEnumerable{T}"/> that iterates from
        /// <c>0</c> (inclusive) to <paramref name="end"/> (exclusive)
        /// in steps of <c>1</c>.
        /// </summary>
        /// <param name="end">The last index.</param>
        public static IEnumerable<int> Range(int end)
        {
            return Range(0, end, 1);
        }

        /// <summary>
        /// Returns a <see cref="IEnumerable{T}"/> that iterates from
        /// <paramref name="start"/> (inclusive) to <paramref name="end"/> (exclusive)
        /// in steps of <c>1</c>.
        /// </summary>
        /// <param name="start">The first index.</param>
        /// <param name="end">The last index.</param>
        public static IEnumerable<int> Range(int start, int end)
        {
            return Range(start, end, 1);
        }

        /// <summary>
        /// Returns a <see cref="IEnumerable{T}"/> that iterates from
        /// <paramref name="start"/> (inclusive) to <paramref name="end"/> (exclusive)
        /// in steps of <paramref name="step"/>.
        /// The argument <paramref name="step"/> can be negative, but not <c>0</c>.
        /// </summary>
        /// <exception cref="ArgumentException">
        /// <paramref name="step"/> is 0.
        /// </exception>
        /// <param name="start">The first index.</param>
        /// <param name="end">The last index.</param>
        /// <param name="step">The amount by which to increment the index on each iteration.</param>
        public static IEnumerable<int> Range(int start, int end, int step)
        {
            if (step == 0)
                throw new ArgumentException("step cannot be 0.", nameof(step));

            if (end < start && step > 0)
                yield break;

            if (end > start && step < 0)
                yield break;

            if (step > 0)
            {
                for (int i = start; i < end; i += step)
                    yield return i;
            }
            else
            {
                for (int i = start; i > end; i += step)
                    yield return i;
            }
        }

        /// <summary>
        /// Sets seed for the random number generator to <paramref name="seed"/>.
        /// Setting the seed manually can ensure consistent, repeatable results for
        /// most random functions.
        /// </summary>
        /// <example>
        /// <code>
        /// ulong mySeed = (ulong)GD.Hash("Godot Rocks");
        /// GD.Seed(mySeed);
        /// var a = GD.Randf() + GD.Randi();
        /// GD.Seed(mySeed);
        /// var b = GD.Randf() + GD.Randi();
        /// // a and b are now identical
        /// </code>
        /// </example>
        /// <param name="seed">Seed that will be used.</param>
        public static void Seed(ulong seed)
        {
            NativeFuncs.godotsharp_seed(seed);
        }

        /// <summary>
        /// Converts a formatted string that was returned by <see cref="VarToStr(Variant)"/>
        /// to the original value.
        /// </summary>
        /// <example>
        /// <code>
        /// string a = "{ \"a\": 1, \"b\": 2 }";        // a is a string
        /// var b = GD.StrToVar(a).AsGodotDictionary(); // b is a Dictionary
        /// GD.Print(b["a"]);                           // Prints 1
        /// </code>
        /// </example>
        /// <param name="str">String that will be converted to Variant.</param>
        /// <returns>The decoded <c>Variant</c>.</returns>
        public static Variant StrToVar(string str)
        {
            using var godotStr = Marshaling.ConvertStringToNative(str);
            NativeFuncs.godotsharp_str_to_var(godotStr, out godot_variant ret);
            return Variant.CreateTakingOwnershipOfDisposableValue(ret);
        }

        /// <summary>
        /// Encodes a <see cref="Variant"/> value to a byte array, without encoding objects.
        /// Deserialization can be done with <see cref="BytesToVar"/>.
        /// Note: If you need object serialization, see <see cref="VarToBytesWithObjects"/>.
        /// </summary>
        /// <param name="var"><see cref="Variant"/> that will be encoded.</param>
        /// <returns>The <see cref="Variant"/> encoded as an array of bytes.</returns>
        public static byte[] VarToBytes(Variant var)
        {
            NativeFuncs.godotsharp_var_to_bytes((godot_variant)var.NativeVar, godot_bool.False, out var varBytes);
            using (varBytes)
                return Marshaling.ConvertNativePackedByteArrayToSystemArray(varBytes);
        }

        /// <summary>
        /// Encodes a <see cref="Variant"/>. Encoding objects is allowed (and can potentially
        /// include executable code). Deserialization can be done with <see cref="BytesToVarWithObjects"/>.
        /// </summary>
        /// <param name="var"><see cref="Variant"/> that will be encoded.</param>
        /// <returns>The <see cref="Variant"/> encoded as an array of bytes.</returns>
        public static byte[] VarToBytesWithObjects(Variant var)
        {
            NativeFuncs.godotsharp_var_to_bytes((godot_variant)var.NativeVar, godot_bool.True, out var varBytes);
            using (varBytes)
                return Marshaling.ConvertNativePackedByteArrayToSystemArray(varBytes);
        }

        /// <summary>
        /// Converts a <see cref="Variant"/> <paramref name="var"/> to a formatted string that
        /// can later be parsed using <see cref="StrToVar(string)"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// var a = new Godot.Collections.Dictionary { ["a"] = 1, ["b"] = 2 };
        /// GD.Print(GD.VarToStr(a));
        /// // Prints:
        /// // {
        /// //     "a": 1,
        /// //     "b": 2
        /// // }
        /// </code>
        /// </example>
        /// <param name="var">Variant that will be converted to string.</param>
        /// <returns>The <see cref="Variant"/> encoded as a string.</returns>
        public static string VarToStr(Variant var)
        {
            NativeFuncs.godotsharp_var_to_str((godot_variant)var.NativeVar, out godot_string ret);
            using (ret)
                return Marshaling.ConvertStringToManaged(ret);
        }

        /// <summary>
        /// Get the <see cref="Variant.Type"/> that corresponds for the given <see cref="Type"/>.
        /// </summary>
        /// <returns>The <see cref="Variant.Type"/> for the given <paramref name="type"/>.</returns>
        public static Variant.Type TypeToVariantType(Type type)
        {
            return Marshaling.ConvertManagedTypeToVariantType(type, out bool _);
        }
    }
}
