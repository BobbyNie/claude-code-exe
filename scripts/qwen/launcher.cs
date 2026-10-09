using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;
using System.Text;

internal static class Program
{
    private const string RuntimeDirName = ".qwen-runtime";
    private const string EmbeddedResourceName = "QwenRuntime";
    private const string VersionFileName = "version.txt";

    private static int Main(string[] args)
    {
        try
        {
            if (args.Length == 2 && args[0] == "--qwen-terminal-child")
                return TerminalHost.RunChild(args[1], RunQwen);

            string exePath = Assembly.GetExecutingAssembly().Location;
            string exeDir = Path.GetDirectoryName(exePath);
            if (TerminalHost.ShouldLaunch(args, Console.IsInputRedirected, Console.IsOutputRedirected,
                Console.IsErrorRedirected, Environment.GetEnvironmentVariable("WT_SESSION")))
            {
                try
                {
                    string terminalExe = EnsureTerminalExtracted(exeDir);
                    return WaitForCleanup(delegate { return TerminalHost.Launch(terminalExe, exePath, args, exeDir); });
                }
                catch (Exception ex)
                {
                    throw new InvalidOperationException("Bundled Windows Terminal startup failed. Requires Windows 10 build 19041 or later. " + ex.Message, ex);
                }
            }
            return RunQwen(args);
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
    }

    internal static string EnsureTerminalExtracted(string exeDir)
    {
        string version;
        using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("QwenTerminalVersion"))
        {
            if (stream == null) throw new InvalidOperationException("Embedded Terminal version is missing.");
            using (var reader = new StreamReader(stream)) version = reader.ReadToEnd().Trim();
        }
        return TerminalHost.EnsureExtracted(exeDir, version, delegate {
            return Assembly.GetExecutingAssembly().GetManifestResourceStream("QwenTerminal");
        });
    }

    private static int RunQwen(string[] args)
    {
        string exeDir = Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location);
        string runtimeDir = Path.Combine(exeDir, RuntimeDirName);
        EnsureRuntimeExtracted(runtimeDir);
        string nodeExe = Path.Combine(runtimeDir, "node", "node.exe");
        string cliJs = Path.Combine(runtimeDir, "lib", "cli.js");
        if (!File.Exists(nodeExe) || !File.Exists(cliJs))
            throw new InvalidOperationException("Qwen Code runtime files are missing.");
        var startInfo = CreateStartInfo(nodeExe, cliJs, args, exeDir);
        using (var process = Process.Start(startInfo))
        {
            if (process == null) return 1;
            return WaitForCleanup(delegate { process.WaitForExit(); return process.ExitCode; });
        }
    }

    private static int WaitForCleanup(Func<int> wait)
    {
        // The terminal child owns Ctrl+C and cleanup. Keep its waiting parent alive.
        ConsoleCancelEventHandler handler = delegate(object sender, ConsoleCancelEventArgs e) { e.Cancel = true; };
        Console.CancelKeyPress += handler;
        try { return wait(); }
        finally { Console.CancelKeyPress -= handler; }
    }

    private static ProcessStartInfo CreateStartInfo(string nodeExe, string cliJs, string[] args, string exeDir)
    {
        var info = new ProcessStartInfo
        {
            FileName = nodeExe,
            Arguments = BuildArguments(cliJs, args),
            UseShellExecute = false,
            // Keep the caller's console and terminal handles. Do not create a second window.
            CreateNoWindow = false,
        };
        SetPortableEnvironment(info, exeDir);
        // Do not change WorkingDirectory: Qwen associates sessions with the project directory.
        return info;
    }

    internal static void SetPortableEnvironment(ProcessStartInfo info, string exeDir)
    {
        if (string.IsNullOrEmpty(info.EnvironmentVariables["QWEN_HOME"]))
            info.EnvironmentVariables["QWEN_HOME"] = Path.Combine(exeDir, "data", ".qwen");
        if (string.IsNullOrEmpty(info.EnvironmentVariables["QWEN_RUNTIME_DIR"]))
            info.EnvironmentVariables["QWEN_RUNTIME_DIR"] = Path.Combine(exeDir, "data", "qwen-runtime");
    }

    private static string BuildArguments(string cliJs, string[] args)
    {
        var builder = new StringBuilder();
        builder.Append(QuoteArgument(cliJs));

        foreach (var arg in args)
        {
            builder.Append(' ');
            builder.Append(QuoteArgument(arg));
        }

        return builder.ToString();
    }

    internal static string QuoteArgument(string value)
    {
        if (string.IsNullOrEmpty(value))
        {
            return "\"\"";
        }

        if (value.IndexOfAny(new[] { ' ', '\t', '"', '\n', '\r' }) < 0)
        {
            return value;
        }

        var quoted = new StringBuilder();
        quoted.Append('"');
        int backslashes = 0;
        foreach (var ch in value)
        {
            if (ch == '\\')
            {
                backslashes++;
                continue;
            }
            // Windows only doubles backslashes before a quote or the closing quote.
            quoted.Append('\\', ch == '"' ? backslashes * 2 + 1 : backslashes);
            quoted.Append(ch);
            backslashes = 0;
        }
        quoted.Append('\\', backslashes * 2);

        quoted.Append('"');
        return quoted.ToString();
    }

    private static void EnsureRuntimeExtracted(string runtimeDir)
    {
        string embeddedVersion = ReadEmbeddedVersion();
        string installedVersion = ReadInstalledVersion(runtimeDir);
        string marker = Path.Combine(runtimeDir, "node", "node.exe");

        if (File.Exists(marker) && string.Equals(installedVersion, embeddedVersion, StringComparison.Ordinal))
        {
            return;
        }

        if (Directory.Exists(runtimeDir))
        {
            Directory.Delete(runtimeDir, true);
        }

        Directory.CreateDirectory(runtimeDir);

        using (Stream resourceStream = Assembly.GetExecutingAssembly().GetManifestResourceStream(EmbeddedResourceName))
        {
            if (resourceStream == null)
            {
                throw new InvalidOperationException("Embedded Qwen Code runtime archive not found.");
            }

            string tempZip = Path.Combine(Path.GetTempPath(), "qwen-runtime-" + Guid.NewGuid().ToString("N") + ".zip");
            try
            {
                using (var fileStream = File.Create(tempZip))
                {
                    resourceStream.CopyTo(fileStream);
                }

                ZipFile.ExtractToDirectory(tempZip, runtimeDir);
            }
            finally
            {
                if (File.Exists(tempZip))
                {
                    File.Delete(tempZip);
                }
            }
        }

        File.WriteAllText(Path.Combine(runtimeDir, VersionFileName), embeddedVersion);
    }

    private static string ReadEmbeddedVersion()
    {
        using (Stream versionStream = Assembly.GetExecutingAssembly().GetManifestResourceStream("QwenVersion"))
        {
            if (versionStream == null)
            {
                return "unknown";
            }

            using (var reader = new StreamReader(versionStream))
            {
                return reader.ReadToEnd().Trim();
            }
        }
    }

    private static string ReadInstalledVersion(string runtimeDir)
    {
        string versionPath = Path.Combine(runtimeDir, VersionFileName);
        if (!File.Exists(versionPath))
        {
            return string.Empty;
        }

        return File.ReadAllText(versionPath).Trim();
    }
}
