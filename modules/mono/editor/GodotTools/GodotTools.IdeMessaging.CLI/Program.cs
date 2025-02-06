using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using GodotTools.IdeMessaging.Requests;
using Newtonsoft.Json;

namespace GodotTools.IdeMessaging.CLI
{
    internal static class Program
    {
        private static readonly ILogger Logger = new CustomLogger();

        public static int Main(string[] args)
        {
            try
            {
                var mainTask = StartAsync(args, Console.OpenStandardInput(), Console.OpenStandardOutput());
                mainTask.Wait();
                return mainTask.Result;
            }
            catch (Exception ex)
            {
                Logger.LogError("Unhandled exception: ", ex);
                return 1;
            }
        }

        private static async Task<int> StartAsync(string[] args, Stream inputStream, Stream outputStream)
        {
            var inputReader = new StreamReader(inputStream, Encoding.UTF8);
            var outputWriter = new StreamWriter(outputStream, Encoding.UTF8);

            try
            {
                if (args.Length == 0)
                {
                    Logger.LogError("Expected at least 1 argument");
                    return 1;
                }

                string godotProjectDir = Path.GetFullPath(args[0]);

                // Validate the project directory to prevent path traversal
                string trustedBaseDir = Directory.GetCurrentDirectory(); // Use a trusted base directory
                if (!godotProjectDir.StartsWith(trustedBaseDir, StringComparison.OrdinalIgnoreCase))
                {
                    Logger.LogError("Path traversal detected in the specified Godot project directory.");
                    return 1;
                }

                if (!Directory.Exists(godotProjectDir))
                {
                    Logger.LogError($"The specified Godot project directory does not exist: {godotProjectDir}");
                    return 1;
                }

                var forwarder = new ForwarderMessageHandler(outputWriter);

                using (var fwdClient = new Client("VisualStudioCode", godotProjectDir, forwarder, Logger))
                {
                    fwdClient.Start();

                    // ReSharper disable AccessToDisposedClosure
                    fwdClient.Connected += async () => await forwarder.WriteLineToOutput("Event=Connected");
                    fwdClient.Disconnected += async () => await forwarder.WriteLineToOutput("Event=Disconnected");
                    // ReSharper restore AccessToDisposedClosure

                    // TODO: Await connected with timeout

                    while (!fwdClient.IsDisposed)
                    {
                        string? firstLine = await inputReader.ReadLineAsync();

                        if (firstLine == null || firstLine == "QUIT")
                            goto ExitMainLoop;

                        string messageId = firstLine;

                        string? messageArgcLine = await inputReader.ReadLineAsync();

                        if (messageArgcLine == null)
                        {
                            Logger.LogInfo("EOF when expecting argument count");
                            goto ExitMainLoop;
                        }

                        if (!int.TryParse(messageArgcLine, out int messageArgc))
                        {
                            Logger.LogError("Received invalid line for argument count: " + firstLine);
                            continue;
                        }

                        var body = new StringBuilder();

                        for (int i = 0; i < messageArgc; i++)
                        {
                            string? bodyLine = await inputReader.ReadLineAsync();

                            if (bodyLine == null)
                            {
                                Logger.LogInfo($"EOF when expecting body line #{i + 1}");
                                goto ExitMainLoop;
                            }

                            body.AppendLine(bodyLine);
                        }

                        var response = await SendRequest(fwdClient, messageId, new MessageContent(MessageStatus.Ok, body.ToString()));

                        if (response == null)
                        {
                            Logger.LogError($"Failed to write message to the server: {messageId}");
                        }
                        else
                        {
                            var content = new MessageContent(response.Status, JsonConvert.SerializeObject(response));
                            await forwarder.WriteResponseToOutput(messageId, content);
                        }
                    }

                ExitMainLoop:
                    await forwarder.WriteLineToOutput("Event=Quit");
                }

                return 0;
            }
            catch (Exception e)
            {
                Logger.LogError("Unhandled exception", e);
                return 1;
            }
        }

        private static async Task<Response?> SendRequest(Client client, string id, MessageContent content)
        {
            var handlers = new Dictionary<string, Func<Task<Response?>>>
            {
                [PlayRequest.Id] = async () =>
                {
                    var request = JsonConvert.DeserializeObject<PlayRequest>(content.Body);
                    return await client.SendRequest<PlayResponse>(request!);
                },
                [DebugPlayRequest.Id] = async () =>
                {
                    var request = JsonConvert.DeserializeObject<DebugPlayRequest>(content.Body);
                    return await client.SendRequest<DebugPlayResponse>(request!);
                },
                [ReloadScriptsRequest.Id] = async () =>
                {
                    var request = JsonConvert.DeserializeObject<ReloadScriptsRequest>(content.Body);
                    return await client.SendRequest<ReloadScriptsResponse>(request!);
                },
                [CodeCompletionRequest.Id] = async () =>
                {
                    var request = JsonConvert.DeserializeObject<CodeCompletionRequest>(content.Body);
                    return await client.SendRequest<CodeCompletionResponse>(request!);
                }
            };

            if (handlers.TryGetValue(id, out var handler))
                return await handler();

            Console.WriteLine("INVALID REQUEST");
            return null;
        }

        private class CustomLogger : ILogger
        {
            private static string ThisAppPath => Assembly.GetExecutingAssembly().Location;
            private static string ThisAppPathWithoutExtension => Path.ChangeExtension(ThisAppPath, null);

            private static readonly string LogPath = $"{ThisAppPathWithoutExtension}.log";

            private static StreamWriter NewWriter() => new StreamWriter(LogPath, append: true, encoding: Encoding.UTF8);

            private static void Log(StreamWriter writer, string message)
            {
                writer.WriteLine($"{DateTime.Now:HH:mm:ss.ffffff}: {message}");
            }

            public void LogDebug(string message)
            {
                using (var writer = NewWriter())
                {
                    Log(writer, "DEBUG: " + message);
                }
            }

            public void LogInfo(string message)
            {
                using (var writer = NewWriter())
                {
                    Log(writer, "INFO: " + message);
                }
            }

            public void LogWarning(string message)
            {
                using (var writer = NewWriter())
                {
                    Log(writer, "WARN: " + message);
                }
            }

            public void LogError(string message)
            {
                using (var writer = NewWriter())
                {
                    Log(writer, "ERROR: " + message);
                }
            }

            public void LogError(string message, Exception e)
            {
                using (var writer = NewWriter())
                {
                    Log(writer, "EXCEPTION: " + message + '\n' + e);
                }
            }
        }
    }
}
