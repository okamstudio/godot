using System;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using Godot.NativeInterop;

#nullable enable

namespace Godot
{
    internal static class DebuggingUtils
    {
        private static void AppendTypeName(this StringBuilder sb, Type type)
        {
            // Use the C# type keyword for built-in types.
            // https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/builtin-types/built-in-types
            if (type == typeof(void))
                sb.Append("void");
            else if (type == typeof(bool))
                sb.Append("bool");
            else if (type == typeof(byte))
                sb.Append("byte");
            else if (type == typeof(sbyte))
                sb.Append("sbyte");
            else if (type == typeof(char))
                sb.Append("char");
            else if (type == typeof(decimal))
                sb.Append("decimal");
            else if (type == typeof(double))
                sb.Append("double");
            else if (type == typeof(float))
                sb.Append("float");
            else if (type == typeof(int))
                sb.Append("int");
            else if (type == typeof(uint))
                sb.Append("uint");
            else if (type == typeof(nint))
                sb.Append("nint");
            else if (type == typeof(nuint))
                sb.Append("nuint");
            else if (type == typeof(long))
                sb.Append("long");
            else if (type == typeof(ulong))
                sb.Append("ulong");
            else if (type == typeof(short))
                sb.Append("short");
            else if (type == typeof(ushort))
                sb.Append("ushort");
            else if (type == typeof(object))
                sb.Append("object");
            else if (type == typeof(string))
                sb.Append("string");
            else
                sb.Append(type);
        }

        internal static void InstallTraceListener()
        {
            Trace.Listeners.Clear();
            Trace.Listeners.Add(new GodotTraceListener());
        }

#pragma warning disable IDE1006 // Naming rule violation
        // ReSharper disable once InconsistentNaming
        [StructLayout(LayoutKind.Sequential)]
        internal ref struct godot_stack_info
        {
            public godot_string File;
            public godot_string Func;
            public int Line;
        }

        // ReSharper disable once InconsistentNaming
        [StructLayout(LayoutKind.Sequential)]
        internal ref struct godot_stack_info_vector
        {
            private IntPtr _writeProxy;
            private unsafe godot_stack_info* _ptr;

            public readonly unsafe godot_stack_info* Elements
            {
                [MethodImpl(MethodImplOptions.AggressiveInlining)]
                get => _ptr;
            }

            public void Resize(int size)
            {
                ArgumentOutOfRangeException.ThrowIfNegative(size);

                var err = NativeFuncs.godotsharp_stack_info_vector_resize(ref this, size);
                if (err != Error.Ok)
                    throw new InvalidOperationException("Failed to resize vector. Error code is: " + err.ToString());
            }

            public unsafe void Dispose()
            {
                if (_ptr == null)
                    return;
                NativeFuncs.godotsharp_stack_info_vector_destroy(ref this);
                _ptr = null;
            }
        }
#pragma warning restore IDE1006

        internal static unsafe StackFrame? GetCurrentStackFrame(int skipFrames = 0)
        {
            // We skip 2 frames:
            // The first skipped frame is the current method.
            // The second skipped frame is a method in NativeInterop.NativeFuncs.
            var stackTrace = new StackTrace(skipFrames: 2 + skipFrames, fNeedFileInfo: true);
            return stackTrace.GetFrame(0);
        }

        [UnmanagedCallersOnly]
        internal static unsafe void GetCurrentStackInfo(void* destVector)
        {
            try
            {
                var vector = (godot_stack_info_vector*)destVector;

                // We skip 2 frames:
                // The first skipped frame is the current method.
                // The second skipped frame is a method in NativeInterop.NativeFuncs.
                var stackTrace = new StackTrace(skipFrames: 2, fNeedFileInfo: true);
                int frameCount = stackTrace.FrameCount;

                if (frameCount == 0)
                    return;

                vector->Resize(frameCount);

                int i = 0;
                foreach (StackFrame frame in stackTrace.GetFrames())
                {
                    var method = frame.GetMethod();

                    if (method is MethodInfo methodInfo && methodInfo.IsDefined(typeof(StackTraceHiddenAttribute)))
                    {
                        // Skip methods marked hidden from the stack trace.
                        continue;
                    }

                    string? fileName = frame.GetFileName();
                    int fileLineNumber = frame.GetFileLineNumber();

                    GetStackFrameMethodDecl(frame, out string methodDecl);

                    godot_stack_info* stackInfo = &vector->Elements[i];

                    // Assign directly to element in Vector. This way we don't need to worry
                    // about disposal if an exception is thrown. The Vector takes care of it.
                    stackInfo->File = Marshaling.ConvertStringToNative(fileName);
                    stackInfo->Func = Marshaling.ConvertStringToNative(methodDecl);
                    stackInfo->Line = fileLineNumber;

                    i++;
                }

                // Resize the vector again in case we skipped some frames.
                vector->Resize(i);
            }
            catch (Exception e)
            {
                ExceptionUtils.LogException(e);
            }
        }

        internal static void GetStackFrameMethodDecl(StackFrame frame, out string methodDecl)
        {
            MethodBase? methodBase = frame.GetMethod();

            if (methodBase == null)
            {
                methodDecl = string.Empty;
                return;
            }

            var sb = new StringBuilder();

            if (methodBase is MethodInfo methodInfo)
            {
                sb.AppendTypeName(methodInfo.ReturnType);
                sb.Append(' ');
            }

            sb.Append(methodBase.DeclaringType?.FullName ?? "<unknown>");
            sb.Append('.');
            sb.Append(methodBase.Name);

            if (methodBase.IsGenericMethod)
            {
                Type[] genericParams = methodBase.GetGenericArguments();

                sb.Append('<');

                for (int j = 0; j < genericParams.Length; j++)
                {
                    if (j > 0)
                        sb.Append(", ");

                    sb.AppendTypeName(genericParams[j]);
                }

                sb.Append('>');
            }

            sb.Append('(');

            bool varArgs = (methodBase.CallingConvention & CallingConventions.VarArgs) != 0;

            ParameterInfo[] parameter = methodBase.GetParameters();

            for (int i = 0; i < parameter.Length; i++)
            {
                if (i > 0)
                    sb.Append(", ");

                if (i == parameter.Length - 1 && varArgs)
                    sb.Append("params ");

                sb.AppendTypeName(parameter[i].ParameterType);
            }

            sb.Append(')');

            methodDecl = sb.ToString();
        }
    }
}
