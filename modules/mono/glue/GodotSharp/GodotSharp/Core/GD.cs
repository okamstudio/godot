#if REAL_T_IS_DOUBLE
using real_t = System.Double;
#else
using real_t = System.Single;
#endif
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Godot
{
    /// <summary>
    /// Godot's global functions.
    /// </summary>
    public static partial class GD
    {
        /// <summary>
        /// Decodes a byte array back to a <c>Variant</c> value.
        /// If <paramref name="allowObjects"/> is <see langword="true"/> decoding objects is allowed.
        ///
        /// WARNING: Deserialized object can contain code which gets executed.
        /// Do not set <paramref name="allowObjects"/> to <see langword="true"/>
        /// if the serialized object comes from untrusted sources to avoid
        /// potential security threats (remote code execution).
        /// </summary>
        /// <param name="bytes">Byte array that will be decoded to a <c>Variant</c>.</param>
        /// <param name="allowObjects">If objects should be decoded.</param>
        /// <returns>The decoded <c>Variant</c>.</returns>
        public static object Bytes2Var(byte[] bytes, bool allowObjects = false)
        {
            return godot_icall_GD_bytes2var(bytes, allowObjects);
        }

        /// <summary>
        /// Converts from a <c>Variant</c> type to another in the best way possible.
        /// The <paramref name="type"/> parameter uses the <see cref="Variant.Type"/> values.
        /// </summary>
        /// <example>
        /// <code>
        /// var a = new Vector2(1, 0);
        /// // Prints 1
        /// GD.Print(a.Length());
        /// var b = GD.Convert(a, Variant.Type.String)
        /// // Prints 6 as "(1, 0)" is 6 characters
        /// GD.Print(b.Length);
        /// </code>
        /// </example>
        /// <returns>The <c>Variant</c> converted to the given <paramref name="type"/>.</returns>
        public static object Convert(object what, Variant.Type type)
        {
            return godot_icall_GD_convert(what, type);
        }

        /// <summary>
        /// Converts from decibels to linear energy (audio).
        /// </summary>
        /// <seealso cref="Linear2Db(real_t)"/>
        /// <param name="db">Decibels to convert.</param>
        /// <returns>Audio volume as linear energy.</returns>
        public static real_t Db2Linear(real_t db)
        {
            return (real_t)Math.Exp(db * 0.11512925464970228420089957273422);
        }

        /// <summary>
        /// Returns the result of <paramref name="value"/> decreased by
        /// <paramref name="step"/> * <paramref name="amount"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// // a = 59;
        /// // float a = GD.DecTime(60, 10, 0.1f);
        /// </code>
        /// </example>
        /// <param name="value">Value that will be decreased.</param>
        /// <param name="amount">
        /// Amount that will be decreased from <paramref name="value"/> for every <paramref name="step"/>.
        /// </param>
        /// <param name="step">Times the <paramref name="value"/> will be decreased by <paramref name="amount"/></param>
        /// <returns>The decreased value.</returns>
        [Obsolete("DecTime has been deprecated and will be removed in Godot 4.0, use Mathf.MoveToward instead.")]
        public static real_t DecTime(real_t value, real_t amount, real_t step)
        {
            real_t sgn = Mathf.Sign(value);
            real_t val = Mathf.Abs(value);
            val -= amount * step;
            if (val < 0)
                val = 0;
            return val * sgn;
        }

        /// <summary>
        /// Get the <see cref="FuncRef"/> that refers to the function
        /// with the given name <paramref name="funcname"/> in the
        /// given object <paramref name="instance"/>.
        /// </summary>
        /// <param name="instance">The object that contains the function.</param>
        /// <param name="funcname">The name of the function.</param>
        /// <returns>A reference to the given object's function.</returns>
        public static FuncRef FuncRef(Object instance, string funcname)
        {
            var ret = new FuncRef();
            ret.SetInstance(instance);
            ret.SetFunction(funcname);
            return ret;
        }

        private static object[] GetPrintParams(object[] parameters)
        {
            if (parameters == null)
            {
                return new[] { "null" };
            }

            return Array.ConvertAll(parameters, x => x?.ToString() ?? "null");
        }

        /// <summary>
        /// Returns the integer hash of the variable passed.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.Print(GD.Hash("a")); // Prints 177670
        /// </code>
        /// </example>
        /// <param name="var">Variable that will be hashed.</param>
        /// <returns>Hash of the variable passed.</returns>
        public static int Hash(object var)
        {
            return godot_icall_GD_hash(var);
        }

        /// <summary>
        /// Returns the <see cref="Object"/> that corresponds to <paramref name="instanceId"/>.
        /// All Objects have a unique instance ID.
        /// </summary>
        /// <example>
        /// <code>
        /// public class MyNode : Node
        /// {
        ///     public string foo = "bar";
        ///
        ///     public override void _Ready()
        ///     {
        ///         ulong id = GetInstanceId();
        ///         var inst = (MyNode)GD.InstanceFromId(Id);
        ///         GD.Print(inst.foo); // Prints bar
        ///     }
        /// }
        /// </code>
        /// </example>
        /// <param name="instanceId">Instance ID of the Object to retrieve.</param>
        /// <returns>The <see cref="Object"/> instance.</returns>
        public static Object InstanceFromId(ulong instanceId)
        {
            return godot_icall_GD_instance_from_id(instanceId);
        }

        /// <summary>
        /// Converts from linear energy to decibels (audio).
        /// This can be used to implement volume sliders that behave as expected (since volume isn't linear).
        /// </summary>
        /// <seealso cref="Db2Linear(real_t)"/>
        /// <example>
        /// <code>
        /// // "slider" refers to a node that inherits Range such as HSlider or VSlider.
        /// // Its range must be configured to go from 0 to 1.
        /// // Change the bus name if you'd like to change the volume of a specific bus only.
        /// AudioServer.SetBusVolumeDb(AudioServer.GetBusIndex("Master"), GD.Linear2Db(slider.value));
        /// </code>
        /// </example>
        /// <param name="linear">The linear energy to convert.</param>
        /// <returns>Audio as decibels.</returns>
        public static real_t Linear2Db(real_t linear)
        {
            return (real_t)(Math.Log(linear) * 8.6858896380650365530225783783321);
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

        /// <summary>
        /// Pushes an error message to Godot's built-in debugger and to the OS terminal.
        ///
        /// Note: Errors printed this way will not pause project execution.
        /// To print an error message and pause project execution in debug builds,
        /// use [code]assert(false, "test error")[/code] instead.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PushError("test_error"); // Prints "test error" to debugger and terminal as error call
        /// </code>
        /// </example>
        /// <param name="message">Error message.</param>
        public static void PushError(string message)
        {
            godot_icall_GD_pusherror(message);
        }

        /// <summary>
        /// Pushes a warning message to Godot's built-in debugger and to the OS terminal.
        /// </summary>
        /// <example>
        /// GD.PushWarning("test warning"); // Prints "test warning" to debugger and terminal as warning call
        /// </example>
        /// <param name="message">Warning message.</param>
        public static void PushWarning(string message)
        {
            godot_icall_GD_pushwarning(message);
        }

        /// <summary>
        /// Converts one or more arguments of any type to string in the best way possible
        /// and prints them to the console.
        ///
        /// Note: Consider using <see cref="PushError(string)"/> and <see cref="PushWarning(string)"/>
        /// to print error and warning messages instead of <see cref="Print(object[])"/>.
        /// This distinguishes them from print messages used for debugging purposes,
        /// while also displaying a stack trace when an error or warning is printed.
        /// </summary>
        /// <example>
        /// <code>
        /// var a = new int[] { 1, 2, 3 };
        /// GD.Print("a", "b", a); // Prints ab[1, 2, 3]
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void Print(params object[] what)
        {
            godot_icall_GD_print(GetPrintParams(what));
        }

        /// <summary>
        /// Prints the current stack trace information to the console.
        /// </summary>
        public static void PrintStack()
        {
            Print(System.Environment.StackTrace);
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
            godot_icall_GD_printerr(GetPrintParams(what));
        }

        /// <summary>
        /// Prints one or more arguments to strings in the best way possible to console.
        /// No newline is added at the end.
        ///
        /// Note: Due to limitations with Godot's built-in console, this only prints to the terminal.
        /// If you need to print in the editor, use another method, such as <see cref="Print(object[])"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PrintRaw("A");
        /// GD.PrintRaw("B");
        /// // Prints AB
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void PrintRaw(params object[] what)
        {
            godot_icall_GD_printraw(GetPrintParams(what));
        }

        /// <summary>
        /// If verbose mode is enabled (<see cref="OS.IsStdoutVerbose"/> returning <see langword="true"/>),
        /// converts one or more arguments of any type to string in the best way possible
        /// and prints them to the console.
        /// </summary>
        /// <example>
        /// <code>
        /// var a = new int[] { 1, 2, 3 };
        /// GD.PrintVerbose("a", "b", a); // Prints ab[1, 2, 3] only if verbose output is enabled
        /// </code>
        /// </example>
        /// <param name="what">Arguments that will be printed.</param>
        public static void PrintVerbose(params object[] what)
        {
            godot_icall_GD_print_verbose(GetPrintParams(what));
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
            godot_icall_GD_prints(GetPrintParams(what));
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
            godot_icall_GD_printt(GetPrintParams(what));
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
            return godot_icall_GD_randf();
        }

        /// <summary>
        /// Returns a random unsigned 32-bit integer.
        /// Use remainder to obtain a random value in the interval <c>[0, N - 1]</c> (where N is smaller than 2^32).
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
            return godot_icall_GD_randi();
        }

        /// <summary>
        /// Randomizes the seed (or the internal state) of the random number generator.
        /// Current implementation reseeds using a number based on time.
        ///
        /// Note: This method is called automatically when the project is run.
        /// If you need to fix the seed to have reproducible results, use <see cref="Seed(ulong)"/>
        /// to initialize the random number generator.
        /// </summary>
        public static void Randomize()
        {
            godot_icall_GD_randomize();
        }

        /// <summary>
        /// Returns a random floating point value on the interval between <paramref name="from"/>
        /// and <paramref name="to"/> (inclusive).
        /// </summary>
        /// <example>
        /// <code>
        /// GD.PrintS(GD.RandRange(-10.0, 10.0), GD.RandRange(-10.0, 10.0)); // Prints e.g. -3.844535 7.45315
        /// </code>
        /// </example>
        /// <returns>A random <see langword="double"/> number inside the given range.</returns>
        public static double RandRange(double from, double to)
        {
            return godot_icall_GD_rand_range(from, to);
        }

        /// <summary>
        /// Returns a random unsigned 32-bit integer, using the given <paramref name="seed"/>.
        /// The <paramref name="newSeed"/> will return the new seed.
        /// </summary>
        /// <param name="seed">Seed to use to generate the random number.</param>
        /// <param name="newSeed">Seed used by the random number generator.</param>
        /// <returns>A random <see langword="uint"/> number.</returns>
        public static uint RandSeed(ulong seed, out ulong newSeed)
        {
            return godot_icall_GD_rand_seed(seed, out newSeed);
        }

        /// <summary>
        /// Returns a <see cref="IEnumerable{T}"/> that iterates from
        /// <c>0</c> to <paramref name="end"/> in steps of <c>1</c>.
        /// </summary>
        /// <param name="end">The last index.</param>
        public static IEnumerable<int> Range(int end)
        {
            return Range(0, end, 1);
        }

        /// <summary>
        /// Returns a <see cref="IEnumerable{T}"/> that iterates from
        /// <paramref name="start"/> to <paramref name="end"/> in steps of <c>1</c>.
        /// </summary>
        /// <param name="start">The first index.</param>
        /// <param name="end">The last index.</param>
        public static IEnumerable<int> Range(int start, int end)
        {
            return Range(start, end, 1);
        }

        /// <summary>
        /// Returns a <see cref="IEnumerable{T}"/> that iterates from
        /// <paramref name="start"/> to <paramref name="end"/> in steps of <paramref name="step"/>.
        /// </summary>
        /// <param name="start">The first index.</param>
        /// <param name="end">The last index.</param>
        /// <param name="step">The amount by which to increment the index on each iteration.</param>
        public static IEnumerable<int> Range(int start, int end, int step)
        {
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
        /// Sets seed for the random number generator.
        /// </summary>
        /// <param name="seed">Seed that will be used.</param>
        public static void Seed(ulong seed)
        {
            godot_icall_GD_seed(seed);
        }

        /// <summary>
        /// Converts one or more arguments of any type to string in the best way possible.
        /// </summary>
        /// <param name="what">Arguments that will converted to string.</param>
        /// <returns>The string formed by the given arguments.</returns>
        public static string Str(params object[] what)
        {
            return godot_icall_GD_str(what);
        }

        /// <summary>
        /// Converts a formatted string that was returned by <see cref="Var2Str(object)"/> to the original value.
        /// </summary>
        /// <example>
        /// <code>
        /// string a = "{\"a\": 1, \"b\": 2 }";
        /// var b = (Godot.Collections.Dictionary)GD.Str2Var(a);
        /// GD.Print(b["a"]); // Prints 1
        /// </code>
        /// </example>
        /// <param name="str">String that will be converted to Variant.</param>
        /// <returns>The decoded <c>Variant</c>.</returns>
        public static object Str2Var(string str)
        {
            return godot_icall_GD_str2var(str);
        }

        /// <summary>
        /// Returns whether the given class exists in <see cref="ClassDB"/>.
        /// </summary>
        /// <returns>If the class exists in <see cref="ClassDB"/>.</returns>
        public static bool TypeExists(string type)
        {
            return godot_icall_GD_type_exists(type);
        }

        /// <summary>
        /// Encodes a <c>Variant</c> value to a byte array.
        /// If <paramref name="fullObjects"/> is <see langword="true"/> encoding objects is allowed
        /// (and can potentially include code).
        /// Deserialization can be done with <see cref="Bytes2Var(byte[], bool)"/>.
        /// </summary>
        /// <param name="var">Variant that will be encoded.</param>
        /// <param name="fullObjects">If objects should be serialized.</param>
        /// <returns>The <c>Variant</c> encoded as an array of bytes.</returns>
        public static byte[] Var2Bytes(object var, bool fullObjects = false)
        {
            return godot_icall_GD_var2bytes(var, fullObjects);
        }

        /// <summary>
        /// Converts a <c>Variant</c> <paramref name="var"/> to a formatted string that
        /// can later be parsed using <see cref="Str2Var(string)"/>.
        /// </summary>
        /// <example>
        /// <code>
        /// var a = new Godot.Collections.Dictionary { ["a"] = 1, ["b"] = 2 };
        /// GD.Print(GD.Var2Str(a));
        /// // Prints
        /// // {
        /// //    "a": 1,
        /// //    "b": 2
        /// // }
        /// </code>
        /// </example>
        /// <param name="var">Variant that will be converted to string.</param>
        /// <returns>The <c>Variant</c> encoded as a string.</returns>
        public static string Var2Str(object var)
        {
            return godot_icall_GD_var2str(var);
        }

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_GD_bytes2var(byte[] bytes, bool allowObjects);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_GD_convert(object what, Variant.Type type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern int godot_icall_GD_hash(object var);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern Object godot_icall_GD_instance_from_id(ulong instanceId);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_print(object[] what);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_printerr(object[] what);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_printraw(object[] what);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_prints(object[] what);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_printt(object[] what);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern float godot_icall_GD_randf();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint godot_icall_GD_randi();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_randomize();

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern double godot_icall_GD_rand_range(double from, double to);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern uint godot_icall_GD_rand_seed(ulong seed, out ulong newSeed);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_seed(ulong seed);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_GD_str(object[] what);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern object godot_icall_GD_str2var(string str);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern bool godot_icall_GD_type_exists(string type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern byte[] godot_icall_GD_var2bytes(object what, bool fullObjects);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern string godot_icall_GD_var2str(object var);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_pusherror(string type);

        [MethodImpl(MethodImplOptions.InternalCall)]
        internal static extern void godot_icall_GD_pushwarning(string type);
    }
}
