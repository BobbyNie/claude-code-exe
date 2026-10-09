using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

internal static class LauncherTests
{
    [DllImport("kernel32.dll")] private static extern bool AllocConsole();
    [DllImport("kernel32.dll")] private static extern uint GetConsoleProcessList(uint[] list, uint count);
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }
    private static string Encode(string value)
    {
        return Convert.ToBase64String(System.Text.Encoding.UTF8.GetBytes(value));
    }
    private static int Main(string[] args)
    {
        if (args.Length > 0 && args[0] == "--child")
        {
            uint[] processes = new uint[64];
            uint count = GetConsoleProcessList(processes, 64);
            bool shared = false;
            for (int i = 0; i < Math.Min(count, 64); i++)
                if (processes[i] == uint.Parse(args[2])) shared = true;
            File.WriteAllLines(args[1], new[] {
                shared.ToString(), Directory.GetCurrentDirectory(),
                Environment.GetEnvironmentVariable("QWEN_HOME") ?? "",
                Environment.GetEnvironmentVariable("QWEN_RUNTIME_DIR") ?? "",
                Encode(args[3]), Encode(args[4]), Encode(args[5]), Encode(args[6])
            });
            return 23;
        }
        try
        {
            uint[] consoleProcesses = new uint[64];
            if (GetConsoleProcessList(consoleProcesses, 64) == 0)
                Check(AllocConsole(), "Cannot allocate a console for the Windows test");
            var quote = typeof(Program).GetMethod("QuoteArgument", BindingFlags.NonPublic | BindingFlags.Static);
            var self = Assembly.GetExecutingAssembly().Location;
            var output = Path.Combine(Path.GetDirectoryName(self), "child-output.txt");
            string[] values = { @"C:\my project\file.txt", @"C:\my project\", "a\\\"b", "" };
            // First prove the old argument encoder corrupts ordinary Windows paths.
            var quoted = (string)quote.Invoke(null, new object[] { values[0] });
            Check(quoted == "\"" + values[0] + "\"", "Windows paths must not gain backslashes");
            var create = typeof(Program).GetMethod("CreateStartInfo", BindingFlags.NonPublic | BindingFlags.Static);
            Check(create != null, "Missing testable startup boundary");
            Environment.SetEnvironmentVariable("QWEN_HOME", null);
            Environment.SetEnvironmentVariable("QWEN_RUNTIME_DIR", null);
            var info = (ProcessStartInfo)create.Invoke(null, new object[] {
                self, "--child", new[] { output, Process.GetCurrentProcess().Id.ToString(), values[0], values[1], values[2], values[3] }, Path.GetDirectoryName(self)
            });
            Check(!info.CreateNoWindow && !info.UseShellExecute, "Must inherit the existing console");
            Check(!info.RedirectStandardInput && !info.RedirectStandardOutput && !info.RedirectStandardError, "Must preserve terminal handles");
            Check(info.WorkingDirectory == "" || info.WorkingDirectory == Directory.GetCurrentDirectory(), "Must preserve project directory");
            using (var child = Process.Start(info))
            {
                Check(child.WaitForExit(15000), "Child timed out");
                Check(child.ExitCode == 23, "Child exit code changed");
            }
            var lines = File.ReadAllLines(output);
            Check(lines[0] == "True", "Child does not share parent console");
            Check(lines[1] == Directory.GetCurrentDirectory(), "Project directory changed");
            Check(lines[2] == Path.Combine(Path.GetDirectoryName(self), "data", ".qwen"), "Wrong portable configuration path");
            Check(lines[3] == Path.Combine(Path.GetDirectoryName(self), "data", "qwen-runtime"), "Wrong portable history path");
            for (int i = 0; i < values.Length; i++) Check(lines[i + 4] == Encode(values[i]), "Argument round trip failed: " + i);
            Environment.SetEnvironmentVariable("QWEN_HOME", @"C:\custom home");
            Environment.SetEnvironmentVariable("QWEN_RUNTIME_DIR", @"C:\custom history");
            info = (ProcessStartInfo)create.Invoke(null, new object[] { self, "cli.js", new string[0], Path.GetDirectoryName(self) });
            Check(info.EnvironmentVariables["QWEN_HOME"] == @"C:\custom home", "Custom configuration path overwritten");
            Check(info.EnvironmentVariables["QWEN_RUNTIME_DIR"] == @"C:\custom history", "Custom history path overwritten");
            Console.WriteLine("PASS: shared Windows console, project directory, portable paths, overrides, arguments and exit code");
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
