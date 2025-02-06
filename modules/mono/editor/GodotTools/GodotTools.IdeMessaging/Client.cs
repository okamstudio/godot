using System;
using System.Diagnostics.CodeAnalysis;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Net.Sockets;
using Newtonsoft.Json;
using System.Threading;
using System.Threading.Tasks;
using GodotTools.IdeMessaging.Requests;
using GodotTools.IdeMessaging.Utils;

namespace GodotTools.IdeMessaging
{
    // ReSharper disable once UnusedType.Global
    public sealed class Client : IDisposable
    {
        private readonly ILogger logger;

        private readonly string identity;

        private string MetaFilePath { get; }
        private DateTime? metaFileModifiedTime;
        private GodotIdeMetadata godotIdeMetadata;
        private readonly FileSystemWatcher fsWatcher;

        public string GodotEditorExecutablePath => godotIdeMetadata.EditorExecutablePath;

        private readonly IMessageHandler messageHandler;

        private Peer? peer;
        private readonly SemaphoreSlim connectionSem = new SemaphoreSlim(1);

        private readonly Queue<NotifyAwaiter<bool>> clientConnectedAwaiters = new Queue<NotifyAwaiter<bool>>();
        private readonly Queue<NotifyAwaiter<bool>> clientDisconnectedAwaiters = new Queue<NotifyAwaiter<bool>>();

        // ReSharper disable once UnusedMember.Global
        public async Task<bool> AwaitConnected()
        {
            var awaiter = new NotifyAwaiter<bool>();
            clientConnectedAwaiters.Enqueue(awaiter);
            return await awaiter;
        }

        // ReSharper disable once UnusedMember.Global
        public async Task<bool> AwaitDisconnected()
        {
            var awaiter = new NotifyAwaiter<bool>();
            clientDisconnectedAwaiters.Enqueue(awaiter);
            return await awaiter;
        }

        // ReSharper disable once MemberCanBePrivate.Global
        public bool IsDisposed { get; private set; }

        // ReSharper disable once MemberCanBePrivate.Global
        [MemberNotNullWhen(true, "peer")]
        public bool IsConnected => peer != null && !peer.IsDisposed && peer.IsTcpClientConnected;

        // ReSharper disable once EventNeverSubscribedTo.Global
        public event Action Connected
        {
            add
            {
                if (peer != null && !peer.IsDisposed)
                    peer.Connected += value;
            }
            remove
            {
                if (peer != null && !peer.IsDisposed)
                    peer.Connected -= value;
            }
        }

        // ReSharper disable once EventNeverSubscribedTo.Global
        public event Action Disconnected
        {
            add
            {
                if (peer != null && !peer.IsDisposed)
                    peer.Disconnected += value;
            }
            remove
            {
                if (peer != null && !peer.IsDisposed)
                    peer.Disconnected -= value;
            }
        }

        ~Client()
        {
            Dispose(disposing: false);
        }

        public async void Dispose()
        {
            if (IsDisposed)
                return;

            using (await connectionSem.UseAsync())
            {
                if (IsDisposed) // lock may not be fair
                    return;
                IsDisposed = true;
            }

            Dispose(disposing: true);
            GC.SuppressFinalize(this);
        }

        private void Dispose(bool disposing)
        {
            if (disposing)
            {
                peer?.Dispose();
                fsWatcher.Dispose();
            }
        }

        public Client(string identity, string godotProjectDir, IMessageHandler messageHandler, ILogger logger)
        {
            this.identity = identity;
            this.messageHandler = messageHandler;
            this.logger = logger;

            // Normalize and validate the godotProjectDir to prevent path traversal
            string trustedBaseDir = Path.GetFullPath(godotProjectDir);

            string projectMetadataDir = Path.GetFullPath(Path.Combine(trustedBaseDir, ".godot", "mono", "metadata"));
            if (!projectMetadataDir.StartsWith(trustedBaseDir, StringComparison.OrdinalIgnoreCase))
            {
                throw new UnauthorizedAccessException("Path traversal detected in godotProjectDir.");
            }

            // FileSystemWatcher requires an existing directory
            if (!Directory.Exists(projectMetadataDir))
            {
                // Check if the non hidden version exists
                string path = Path.Combine(trustedBaseDir, "godot", "mono", "metadata");
                string nonHiddenProjectMetadataDir = Path.GetFullPath(path);

                if (!nonHiddenProjectMetadataDir.StartsWith(trustedBaseDir, StringComparison.OrdinalIgnoreCase))
                {
                    string message = "Path traversal detected in non-hidden project metadata directory.";
                    throw new UnauthorizedAccessException(message);
                }

                if (Directory.Exists(nonHiddenProjectMetadataDir))
                {
                    projectMetadataDir = nonHiddenProjectMetadataDir;
                }
                else
                {
                    Directory.CreateDirectory(projectMetadataDir);
                }
            }

            MetaFilePath = Path.Combine(projectMetadataDir, GodotIdeMetadata.DefaultFileName);

            fsWatcher = new FileSystemWatcher(projectMetadataDir, GodotIdeMetadata.DefaultFileName);
        }

        private async void OnMetaFileChanged(object sender, FileSystemEventArgs e)
        {
            if (IsDisposed)
                return;

            using (await connectionSem.UseAsync())
            {
                if (IsDisposed)
                    return;

                if (!File.Exists(MetaFilePath))
                    return;

                var lastWriteTime = File.GetLastWriteTime(MetaFilePath);

                if (lastWriteTime == metaFileModifiedTime)
                    return;

                metaFileModifiedTime = lastWriteTime;

                var metadata = ReadMetadataFile();

                if (metadata != null && metadata != godotIdeMetadata)
                {
                    godotIdeMetadata = metadata.Value;
                    _ = Task.Run(ConnectToServer);
                }
            }
        }

        private async void OnMetaFileDeleted(object sender, FileSystemEventArgs e)
        {
            if (IsDisposed)
                return;

            if (IsConnected)
            {
                using (await connectionSem.UseAsync())
                    peer?.Dispose();
            }

            // The file may have been re-created

            using (await connectionSem.UseAsync())
            {
                if (IsDisposed)
                    return;

                if (IsConnected || !File.Exists(MetaFilePath))
                    return;

                var lastWriteTime = File.GetLastWriteTime(MetaFilePath);

                if (lastWriteTime == metaFileModifiedTime)
                    return;

                metaFileModifiedTime = lastWriteTime;

                var metadata = ReadMetadataFile();

                if (metadata != null)
                {
                    godotIdeMetadata = metadata.Value;
                    _ = Task.Run(ConnectToServer);
                }
            }
        }

        private GodotIdeMetadata? ReadMetadataFile()
        {
            using (var fileStream = new FileStream(MetaFilePath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (var reader = new StreamReader(fileStream))
            {
                string? portStr = reader.ReadLine();

                if (portStr == null)
                    return null;

                string? editorExecutablePath = reader.ReadLine();

                if (editorExecutablePath == null)
                    return null;

                if (!int.TryParse(portStr, out int port))
                    return null;

                return new GodotIdeMetadata(port, editorExecutablePath);
            }
        }

        private async Task AcceptClient(TcpClient tcpClient)
        {
            logger.LogDebug("Accept client...");

            using (peer = new Peer(tcpClient, new ClientHandshake(), messageHandler, logger))
            {
                // ReSharper disable AccessToDisposedClosure
                peer.Connected += () =>
                {
                    logger.LogInfo("Connection open with Ide Client");

                    while (clientConnectedAwaiters.Count > 0)
                        clientConnectedAwaiters.Dequeue().SetResult(true);
                };

                peer.Disconnected += () =>
                {
                    while (clientDisconnectedAwaiters.Count > 0)
                        clientDisconnectedAwaiters.Dequeue().SetResult(true);
                };
                // ReSharper restore AccessToDisposedClosure

                try
                {
                    if (!await peer.DoHandshake(identity))
                    {
                        logger.LogError("Handshake failed");
                        return;
                    }
                }
                catch (Exception e)
                {
                    logger.LogError("Handshake failed with unhandled exception: ", e);
                    return;
                }

                await peer.Process();

                logger.LogInfo("Connection closed with Ide Client");
            }
        }

        private async Task ConnectToServer()
        {
            var tcpClient = new TcpClient();

            try
            {
                logger.LogInfo("Connecting to Godot Ide Server");

                await tcpClient.ConnectAsync(IPAddress.Loopback, godotIdeMetadata.Port);

                logger.LogInfo("Connection open with Godot Ide Server");

                await AcceptClient(tcpClient);
            }
            catch (SocketException e)
            {
                if (e.SocketErrorCode == SocketError.ConnectionRefused)
                    logger.LogError("The connection to the Godot Ide Server was refused");
                else
                    throw;
            }
        }

        // ReSharper disable once UnusedMember.Global
        public async void Start()
        {
            fsWatcher.Created += OnMetaFileChanged;
            fsWatcher.Changed += OnMetaFileChanged;
            fsWatcher.Deleted += OnMetaFileDeleted;
            fsWatcher.EnableRaisingEvents = true;

            using (await connectionSem.UseAsync())
            {
                if (IsDisposed)
                    return;

                if (IsConnected)
                    return;

                if (!File.Exists(MetaFilePath))
                {
                    logger.LogInfo("There is no Godot Ide Server running");
                    return;
                }

                var metadata = ReadMetadataFile();

                if (metadata != null)
                {
                    godotIdeMetadata = metadata.Value;
                    _ = Task.Run(ConnectToServer);
                }
                else
                {
                    logger.LogError("Failed to read Godot Ide metadata file");
                }
            }
        }

        public async Task<TResponse?> SendRequest<TResponse>(Request request)
            where TResponse : Response, new()
        {
            if (!IsConnected)
            {
                logger.LogError("Cannot write request. Not connected to the Godot Ide Server.");
                return null;
            }

            string body = JsonConvert.SerializeObject(request);
            return await peer.SendRequest<TResponse>(request.Id, body);
        }

        public async Task<TResponse?> SendRequest<TResponse>(string id, string body)
            where TResponse : Response, new()
        {
            if (!IsConnected)
            {
                logger.LogError("Cannot write request. Not connected to the Godot Ide Server.");
                return null;
            }

            return await peer.SendRequest<TResponse>(id, body);
        }
    }
}
