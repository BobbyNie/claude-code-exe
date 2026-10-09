using System;
using System.IO;
using System.Text;
using System.IO.Pipes;
using System.IO.Compression;
using System.Threading;
using System.Text.RegularExpressions;
#if !NET8_0_OR_GREATER
using System.Security.AccessControl;
using System.Security.Principal;
#endif
using System.Collections.Generic;
using System.Collections;
using System.Diagnostics;

internal static class TerminalHost
{
    internal static ProcessStartInfo CreateStartInfo(string terminalExe, string launcherExe, string token)
    {
        return new ProcessStartInfo {
            FileName = terminalExe,
            Arguments = "-w new new-tab --inheritEnvironment --title Qwen " +
                Program.QuoteArgument(launcherExe.Replace(";", @"\;")) + " --qwen-terminal-child " + token,
            UseShellExecute = false,
            CreateNoWindow = true,
        };
    }

    internal static int Launch(string terminalExe, string launcherExe, string[] args, string exeDir)
    {
        var portable = new ProcessStartInfo();
        Program.SetPortableEnvironment(portable, exeDir);
        var environment = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (DictionaryEntry item in portable.EnvironmentVariables)
            environment[(string)item.Key] = (string)item.Value;
        return Handoff(delegate(string token) {
            using (var process = Process.Start(CreateStartInfo(terminalExe, launcherExe, token)))
                if (process == null) throw new InvalidOperationException("Cannot start bundled Windows Terminal.");
        }, args, Directory.GetCurrentDirectory(), environment, 60000);
    }

    internal static string EnsureExtracted(string exeDir, string version, Func<Stream> openArchive)
    {
        if (!Regex.IsMatch(version, @"^\d+(\.\d+){2,3}$")) throw new InvalidDataException("Invalid Terminal version.");
        string root = Path.Combine(exeDir, ".qwen-terminal");
        Directory.CreateDirectory(root);
        using (var installationLock = LockInstallation(Path.Combine(root, "install.lock")))
        {
            string target = Path.Combine(root, "terminal-" + version);
            string exe = Path.Combine(target, "WindowsTerminal.exe");
            string ready = Path.Combine(target, ".ready");
            if (File.Exists(ready) && File.Exists(exe) && File.Exists(Path.Combine(target, ".portable")) &&
                File.Exists(Path.Combine(target, "Microsoft.Terminal.Settings.Model.dll"))) return exe;
            string stage = Path.Combine(root, ".install-" + Guid.NewGuid().ToString("N"));
            try
            {
                Directory.CreateDirectory(stage);
                using (var stream = openArchive())
                {
                    if (stream == null) throw new InvalidDataException("Embedded Windows Terminal archive is missing.");
                    using (var zip = new ZipArchive(stream, ZipArchiveMode.Read)) zip.ExtractToDirectory(stage);
                }
                foreach (string required in new[] { "WindowsTerminal.exe", "Microsoft.Terminal.Settings.Model.dll", ".portable", "LICENSE", "NOTICE.html", "settings/settings.json" })
                    if (!File.Exists(Path.Combine(stage, required))) throw new InvalidDataException("Terminal archive is missing " + required);
                foreach (string file in Directory.GetFiles(stage, "*", SearchOption.AllDirectories))
                {
                    string relative = file.Substring(stage.Length + 1);
                    string destination = Path.Combine(target, relative);
                    // A damaged binary may be repaired. Never overwrite the user's settings.
                    if (relative.StartsWith("settings" + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase) && File.Exists(destination)) continue;
                    Directory.CreateDirectory(Path.GetDirectoryName(destination));
                    File.Copy(file, destination, true);
                }
                File.WriteAllText(ready, version);
                return exe;
            }
            finally { if (Directory.Exists(stage)) Directory.Delete(stage, true); }
        }
    }

    private static FileStream LockInstallation(string path)
    {
        for (int attempt = 0; ; attempt++)
        {
            try { return new FileStream(path, FileMode.OpenOrCreate, FileAccess.ReadWrite, FileShare.None); }
            catch (IOException) { if (attempt >= 600) throw; Thread.Sleep(100); }
        }
    }

    private const string PipePrefix = "qwen-terminal-";

    private static NamedPipeServerStream CreatePipe(string name)
    {
#if NET8_0_OR_GREATER
        return new NamedPipeServerStream(name, PipeDirection.InOut, 1, PipeTransmissionMode.Byte,
            PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
#else
        var security = new PipeSecurity();
        security.SetAccessRuleProtection(true, false);
        security.AddAccessRule(new PipeAccessRule(WindowsIdentity.GetCurrent().User, PipeAccessRights.FullControl, AccessControlType.Allow));
        return new NamedPipeServerStream(name, PipeDirection.InOut, 1, PipeTransmissionMode.Byte,
            PipeOptions.Asynchronous, 0, 0, security);
#endif
    }

    internal static int Handoff(Action<string> start, string[] args, string directory,
        IDictionary<string, string> environment, int connectionTimeout)
    {
        string token = Guid.NewGuid().ToString("N");
        using (var pipe = CreatePipe(PipePrefix + token))
        {
            var connection = pipe.BeginWaitForConnection(null, null);
            using (connection.AsyncWaitHandle)
            {
                start(token);
                if (!connection.AsyncWaitHandle.WaitOne(connectionTimeout))
                    throw new TimeoutException("Windows Terminal did not start Qwen within the startup timeout.");
                pipe.EndWaitForConnection(connection);
            }
            try
            {
                WriteRequest(pipe, args, directory, environment);
                return new BinaryReader(pipe, Encoding.UTF8, true).ReadInt32();
            }
            // Closing the tab terminates its client process. Do not wait forever for an exit report.
            catch (IOException) { return 130; }
        }
    }

    internal static int RunChild(string token, Func<string[], int> run)
    {
        Guid id;
        if (!Guid.TryParseExact(token, "N", out id)) throw new ArgumentException("Invalid Terminal handoff token.");
        using (var pipe = new NamedPipeClientStream(".", PipePrefix + token, PipeDirection.InOut))
        {
            pipe.Connect(10000);
            int exit = 1;
            try
            {
                var request = ReadRequest(pipe);
                foreach (var item in request.Environment)
                {
                    // These values identify the new Terminal, not the launching console.
                    if (item.Key == "WT_SESSION" || item.Key == "WT_PROFILE_ID") continue;
                    System.Environment.SetEnvironmentVariable(item.Key, item.Value);
                }
                Directory.SetCurrentDirectory(request.Directory);
                exit = run(request.Arguments);
            }
            catch (Exception ex) { Console.Error.WriteLine(ex.Message); }
            try
            {
                var writer = new BinaryWriter(pipe, Encoding.UTF8, true);
                writer.Write(exit);
                writer.Flush();
            }
            catch (IOException) { }
            return exit;
        }
    }

    internal sealed class Request
    {
        internal string Directory;
        internal string[] Arguments;
        internal Dictionary<string, string> Environment;
    }

    internal static void WriteRequest(Stream stream, string[] args, string directory, IDictionary<string, string> environment)
    {
        var writer = new BinaryWriter(stream, Encoding.UTF8, true);
        writer.Write(directory);
        writer.Write(args.Length);
        foreach (string arg in args) writer.Write(arg);
        writer.Write(environment.Count);
        foreach (var item in environment) { writer.Write(item.Key); writer.Write(item.Value); }
        writer.Flush();
    }

    internal static Request ReadRequest(Stream stream)
    {
        var reader = new BinaryReader(stream, Encoding.UTF8, true);
        var request = new Request { Directory = reader.ReadString() };
        int count = reader.ReadInt32();
        if (count < 0 || count > 32768) throw new InvalidDataException("Invalid argument count");
        request.Arguments = new string[count];
        for (int i = 0; i < count; i++) request.Arguments[i] = reader.ReadString();
        count = reader.ReadInt32();
        if (count < 0 || count > 32768) throw new InvalidDataException("Invalid environment count");
        request.Environment = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        for (int i = 0; i < count; i++) request.Environment.Add(reader.ReadString(), reader.ReadString());
        return request;
    }

    // Unknown options stay direct: a new CLI automation flag must never open a GUI.
    internal static bool ShouldLaunch(string[] args, bool inputRedirected, bool outputRedirected,
        bool errorRedirected, string terminalSession)
    {
        if (inputRedirected || outputRedirected || errorRedirected || !string.IsNullOrEmpty(terminalSession))
            return false;
        bool positional = false;
        bool interactive = false;
        for (int i = 0; i < args.Length; i++)
        {
            string arg = args[i];
            int equals = arg.IndexOf('=');
            string option = equals < 0 ? arg : arg.Substring(0, equals);
            switch (option)
            {
                case "--help": case "-h": case "--version": case "-v":
                case "--prompt": case "-p": case "--output-format": case "-o":
                case "--list-sessions": case "--delete-session":
                    return false;
                case "--prompt-interactive": case "-i":
                    interactive = true;
                    if (equals < 0 && i + 1 < args.Length) i++;
                    break;
                case "--resume": case "-r":
                    if (equals < 0 && i + 1 < args.Length && !args[i + 1].StartsWith("-")) i++;
                    break;
                case "--continue": case "-c": case "--debug": case "-d":
                case "--yolo": case "-y": case "--screen-reader": case "--bare":
                    break;
                case "--model": case "-m": case "--approval-mode": case "--auth-type":
                case "--system-prompt": case "--append-system-prompt": case "--output-style":
                case "--proxy": case "--api-key": case "--base-url":
                    if (equals < 0) { if (++i >= args.Length) return false; }
                    break;
                default:
                    if (arg.StartsWith("-")) return false;
                    positional = true;
                    break;
            }
        }
        return interactive || !positional;
    }
}
