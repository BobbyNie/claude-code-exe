using System;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Reflection;

internal static class Program
{
    private const string RuntimeDirName = ".qwen-runtime";
    private const string EmbeddedResourceName = "QwenRuntime";
    private const string VersionFileName = "version.txt";

    private static int Main(string[] args)
    {
        try
        {
            string exeDir = Path.GetDirectoryName(Environment.ProcessPath);
            if (string.IsNullOrEmpty(exeDir))
            {
                exeDir = Directory.GetCurrentDirectory();
            }

            string runtimeDir = Path.Combine(exeDir, RuntimeDirName);
            EnsureRuntimeExtracted(runtimeDir);

            string nodeExe = Path.Combine(runtimeDir, "node", "node.exe");
            string cliJs = Path.Combine(runtimeDir, "lib", "cli.js");

            if (!File.Exists(nodeExe) || !File.Exists(cliJs))
            {
                Console.Error.WriteLine("Qwen Code runtime files are missing.");
                return 1;
            }

            var startInfo = new ProcessStartInfo
            {
                FileName = nodeExe,
                WorkingDirectory = exeDir,
                UseShellExecute = false,
            };

            startInfo.ArgumentList.Add(cliJs);
            foreach (var arg in args)
            {
                startInfo.ArgumentList.Add(arg);
            }

            using var process = Process.Start(startInfo);
            if (process == null)
            {
                return 1;
            }

            process.WaitForExit();
            return process.ExitCode;
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 1;
        }
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
            Directory.Delete(runtimeDir, recursive: true);
        }

        Directory.CreateDirectory(runtimeDir);

        using Stream? resourceStream = Assembly.GetExecutingAssembly().GetManifestResourceStream(EmbeddedResourceName);
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

        File.WriteAllText(Path.Combine(runtimeDir, VersionFileName), embeddedVersion);
    }

    private static string ReadEmbeddedVersion()
    {
        using Stream? versionStream = Assembly.GetExecutingAssembly().GetManifestResourceStream("QwenVersion");
        if (versionStream == null)
        {
            return "unknown";
        }

        using var reader = new StreamReader(versionStream);
        return reader.ReadToEnd().Trim();
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
