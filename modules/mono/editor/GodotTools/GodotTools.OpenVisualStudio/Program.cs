using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Text.RegularExpressions;
using System.Threading;
using EnvDTE;

namespace GodotTools.OpenVisualStudio
{
    internal static class Program
    {
        [DllImport("ole32.dll")]
        private static extern int GetRunningObjectTable(int reserved, out IRunningObjectTable pprot);

        [DllImport("ole32.dll")]
        private static extern void CreateBindCtx(int reserved, out IBindCtx ppbc);

        [DllImport("user32.dll")]
        private static extern bool SetForegroundWindow(IntPtr hWnd);

        private static void ShowHelp()
        {
            Console.WriteLine("Opens the file(s) in a Visual Studio instance that is editing the specified solution.");
            Console.WriteLine("If an existing instance for the solution is not found, a new one is created.");
            Console.WriteLine();
            Console.WriteLine("Usage:");
            Console.WriteLine(@"  GodotTools.OpenVisualStudio.exe solution [file[;line[;col]]...]");
            Console.WriteLine(@"  GodotTools.OpenVisualStudio.exe solution [file[;AddMethod[;methodname[;parameter:type;...]]]...]");
            Console.WriteLine();
            Console.WriteLine("Lines and columns begin at one. Zero or lower will result in an error.");
            Console.WriteLine("If a line is specified but a column is not, the line is selected in the text editor.");
        }

        // STAThread needed, otherwise CoRegisterMessageFilter may return CO_E_NOT_SUPPORTED.
        [STAThread]
        private static int Main(string[] args)
        {
            if (args.Length == 0 || args[0] == "--help" || args[0] == "-h")
            {
                ShowHelp();
                return 0;
            }

            string solutionFile = NormalizePath(args[0]);

            Mutex mutex = new Mutex(false, "GodotTools.OpenVisualStudio.exe");
            mutex.WaitOne();

            var dte = FindInstanceEditingSolution(solutionFile);

            if (dte == null)
            {
                // Open a new instance
                dte = TryVisualStudioLaunch("VisualStudio.DTE.17.0");

                if (dte == null)
                {
                    // Launch of VS 2022 failed, fallback to 2019
                    dte = TryVisualStudioLaunch("VisualStudio.DTE.16.0");

                    if (dte == null)
                    {
                        mutex.ReleaseMutex();
                        Console.Error.WriteLine("Visual Studio not found");
                        return 1;
                    }
                }

                dte.UserControl = true;

                try
                {
                    dte.Solution.Open(solutionFile);
                }
                catch (ArgumentException)
                {
                    mutex.ReleaseMutex();
                    Console.Error.WriteLine("Solution.Open: Invalid path or file not found");
                    return 1;
                }

                dte.MainWindow.Visible = true;
            }

            MessageFilter.Register();

            try
            {
                // Open files

                for (int i = 1; i < args.Length; i++)
                {
                    // Both the line number and the column begin at one

                    string[] fileArgumentParts = args[i].Split(';');

                    string filePath = NormalizePath(fileArgumentParts[0]);

                    try
                    {
                        dte.ItemOperations.OpenFile(filePath);
                    }
                    catch (ArgumentException)
                    {
                        mutex.ReleaseMutex();
                        Console.Error.WriteLine("ItemOperations.OpenFile: Invalid path or file not found");
                        return 1;
                    }

                    if (fileArgumentParts.Length > 1)
                    {
                        if (fileArgumentParts[1].ToString() == "AddMethod")
                        {
                            if (fileArgumentParts.Length > 2)
                            {
                                AddMethod(dte, fileArgumentParts);
                            }
                        }
                        else if (int.TryParse(fileArgumentParts[1], out int line))
                        {
                            var textSelection = (TextSelection)dte.ActiveDocument.Selection;

                            if (fileArgumentParts.Length > 2)
                            {
                                if (int.TryParse(fileArgumentParts[2], out int column))
                                {
                                    textSelection.MoveToLineAndOffset(line, column);
                                }
                                else
                                {
                                    mutex.ReleaseMutex();
                                    Console.Error.WriteLine("The column part of the argument must be a valid integer");
                                    return 1;
                                }
                            }
                            else
                            {
                                textSelection.GotoLine(line, Select: true);
                            }
                        }
                        else
                        {
                            mutex.ReleaseMutex();
                            Console.Error.WriteLine("The line part of the argument must be a valid integer");
                            return 1;
                        }
                    }
                }
            }
            finally
            {
                var mainWindow = dte.MainWindow;
                mainWindow.Activate();
                SetForegroundWindow(mainWindow.HWnd);

                MessageFilter.Revoke();
                mutex.ReleaseMutex();
            }

            return 0;
        }

        private static DTE? TryVisualStudioLaunch(string version)
        {
            try
            {
                var visualStudioDteType = Type.GetTypeFromProgID(version, throwOnError: true);
                var dte = (DTE?)Activator.CreateInstance(visualStudioDteType!);

                return dte;
            }
            catch (COMException)
            {
                return null;
            }
        }

        private static DTE? FindInstanceEditingSolution(string solutionPath)
        {
            if (GetRunningObjectTable(0, out IRunningObjectTable pprot) != 0)
                return null;

            try
            {
                pprot.EnumRunning(out IEnumMoniker ppenumMoniker);
                ppenumMoniker.Reset();

                var moniker = new IMoniker[1];

                while (ppenumMoniker.Next(1, moniker, IntPtr.Zero) == 0)
                {
                    string ppszDisplayName;

                    CreateBindCtx(0, out IBindCtx ppbc);

                    try
                    {
                        moniker[0].GetDisplayName(ppbc, null, out ppszDisplayName);
                    }
                    finally
                    {
                        Marshal.ReleaseComObject(ppbc);
                    }

                    if (ppszDisplayName == null)
                        continue;

                    // The digits after the colon are the process ID
                    if (!Regex.IsMatch(ppszDisplayName, "!VisualStudio.DTE.1[6-7].0:[0-9]"))
                        continue;

                    if (pprot.GetObject(moniker[0], out object ppunkObject) == 0)
                    {
                        if (ppunkObject is DTE dte && dte.Solution.FullName.Length > 0)
                        {
                            if (NormalizePath(dte.Solution.FullName) == solutionPath)
                                return dte;
                        }
                    }
                }
            }
            finally
            {
                Marshal.ReleaseComObject(pprot);
            }

            return null;
        }

        private static string NormalizePath(string path)
        {
            return new Uri(Path.GetFullPath(path)).LocalPath
                .TrimEnd(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar)
                .ToUpperInvariant();
        }

        private static void AddMethod(DTE dte, string[] parameters)
        {
            var document = dte.ActiveDocument;
            if (dte.ActiveDocument == null)
                return;

            var projectItem = document.ProjectItem;
            if (projectItem == null)
                return;

            var codeModel = projectItem.FileCodeModel;
            if (codeModel == null)
                return;

            var codeElements = codeModel.CodeElements;

            CodeNamespace codeNamespace = null;
            foreach (CodeElement codeElement in codeElements)
            {
                if (codeElement.Kind == vsCMElement.vsCMElementNamespace)
                {
                    codeNamespace = codeElement as CodeNamespace;
                    break;
                }
            }

            CodeClass codeClass = null;
            if (codeNamespace == null)
            {
                foreach (CodeElement codeElement in codeElements)
                {
                    if (codeElement.Kind == vsCMElement.vsCMElementClass)
                    {
                        codeClass = codeElement as CodeClass;
                        break;
                    }
                }
            }
            else
            {
                foreach (CodeElement codeElement in codeNamespace.Members)
                {
                    if (codeElement.Kind == vsCMElement.vsCMElementClass)
                    {
                        codeClass = codeElement as CodeClass;
                        break;
                    }
                }
            }

            if (codeClass == null)
                return;

            CodeFunction codeFunction = codeClass.AddFunction(parameters[2], vsCMFunction.vsCMFunctionFunction, vsCMTypeRef.vsCMTypeRefVoid, -1, vsCMAccess.vsCMAccessPrivate);
            for (int i = 3; i < parameters.Length; i++)
            {
                string[] parameter = parameters[i].Split(':');
                if (parameter.Length == 2)
                {
                    codeFunction.AddParameter(parameter[0], parameter[1], -1);
                }
            }
            var textSelection = (TextSelection)dte.ActiveDocument.Selection;
            textSelection.MoveToPoint(codeFunction.GetStartPoint(vsCMPart.vsCMPartBody));
        }

        #region MessageFilter. See: http: //msdn.microsoft.com/en-us/library/ms228772.aspx

        private class MessageFilter : IOleMessageFilter
        {
            // Class containing the IOleMessageFilter
            // thread error-handling functions

            private static IOleMessageFilter? _oldFilter;

            // Start the filter
            public static void Register()
            {
                IOleMessageFilter newFilter = new MessageFilter();
                int ret = CoRegisterMessageFilter(newFilter, out _oldFilter);
                if (ret != 0)
                    Console.Error.WriteLine($"CoRegisterMessageFilter failed with error code: {ret}");
            }

            // Done with the filter, close it
            public static void Revoke()
            {
                int ret = CoRegisterMessageFilter(_oldFilter, out _);
                if (ret != 0)
                    Console.Error.WriteLine($"CoRegisterMessageFilter failed with error code: {ret}");
            }

            //
            // IOleMessageFilter functions
            // Handle incoming thread requests
            int IOleMessageFilter.HandleInComingCall(int dwCallType, IntPtr hTaskCaller, int dwTickCount, IntPtr lpInterfaceInfo)
            {
                // Return the flag SERVERCALL_ISHANDLED
                return 0;
            }

            // Thread call was rejected, so try again.
            int IOleMessageFilter.RetryRejectedCall(IntPtr hTaskCallee, int dwTickCount, int dwRejectType)
            {
                // flag = SERVERCALL_RETRYLATER
                if (dwRejectType == 2)
                {
                    // Retry the thread call immediately if return >= 0 & < 100
                    return 99;
                }

                // Too busy; cancel call
                return -1;
            }

            int IOleMessageFilter.MessagePending(IntPtr hTaskCallee, int dwTickCount, int dwPendingType)
            {
                // Return the flag PENDINGMSG_WAITDEFPROCESS
                return 2;
            }

            // Implement the IOleMessageFilter interface
            [DllImport("ole32.dll")]
            private static extern int CoRegisterMessageFilter(IOleMessageFilter? newFilter, out IOleMessageFilter? oldFilter);
        }

        [ComImport(), Guid("00000016-0000-0000-C000-000000000046"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
        private interface IOleMessageFilter
        {
            [PreserveSig]
            int HandleInComingCall(int dwCallType, IntPtr hTaskCaller, int dwTickCount, IntPtr lpInterfaceInfo);

            [PreserveSig]
            int RetryRejectedCall(IntPtr hTaskCallee, int dwTickCount, int dwRejectType);

            [PreserveSig]
            int MessagePending(IntPtr hTaskCallee, int dwTickCount, int dwPendingType);
        }

        #endregion
    }
}
