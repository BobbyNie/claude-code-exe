using System;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.IO.Compression;
using System.Text;

internal static class TerminalTests
{
    private static void Check(bool condition, string message)
    {
        if (!condition) throw new Exception(message);
    }
    public static void Run()
    {
        RequestRoundTrip();
        ProcessHandoff();
        PortableExtraction();
        TerminalCommand();
        Check(TerminalHost.ShouldLaunch(new string[0], false, false, false, null), "Default interactive CMD must use bundled Terminal");
        Check(!TerminalHost.ShouldLaunch(new string[0], false, false, false, "terminal-session"), "Already in Terminal must not reopen");
        Check(!TerminalHost.ShouldLaunch(new string[0], true, false, false, null), "Piped input must stay direct");
        Check(!TerminalHost.ShouldLaunch(new string[0], false, true, false, null), "Piped output must stay direct");
        Check(!TerminalHost.ShouldLaunch(new string[0], false, false, true, null), "Redirected stderr must stay direct");
        foreach (var args in new[] { new[] { "--help" }, new[] { "--version" }, new[] { "-p", "hello" }, new[] { "hello" }, new[] { "mcp", "list" }, new[] { "--output-format=json" } })
            Check(!TerminalHost.ShouldLaunch(args, false, false, false, null), "Headless command must stay direct: " + string.Join(" ", args));
        foreach (var args in new[] { new[] { "--resume" }, new[] { "--resume", "session-id" }, new[] { "--model", "my-model", "--continue" }, new[] { "-i", "hello ; world" } })
            Check(TerminalHost.ShouldLaunch(args, false, false, false, null), "Interactive command must use bundled Terminal: " + string.Join(" ", args));
    }
    private static void RequestRoundTrip()
    {
        string[] args = { "--resume", "a ; b", "中文\\路径", "a\\\"b", "" };
        var environment = new Dictionary<string, string> { { "QWEN_HOME", "C:\\custom home" }, { "TEST_API_KEY", "secret ; value" } };
        using (var stream = new MemoryStream())
        {
            TerminalHost.WriteRequest(stream, args, "C:\\project ; 中文", environment);
            stream.Position = 0;
            var request = TerminalHost.ReadRequest(stream);
            Check(request.Directory == "C:\\project ; 中文", "Project directory changed in Terminal handoff");
            for (int i = 0; i < args.Length; i++) Check(request.Arguments[i] == args[i], "Terminal handoff changed argument " + i);
            Check(request.Environment["QWEN_HOME"] == environment["QWEN_HOME"], "Explicit portable home lost");
            Check(request.Environment["TEST_API_KEY"] == environment["TEST_API_KEY"], "Caller environment lost");
        }
    }
    private static void TerminalCommand()
    {
        var info = TerminalHost.CreateStartInfo(@"C:\terminal\WindowsTerminal.exe", @"C:\my project ; 中文\qwen.exe", "0123456789abcdef0123456789abcdef");
        Check(info.FileName.EndsWith("WindowsTerminal.exe"), "Must use bundled binary, not system wt alias");
        Check(info.Arguments.StartsWith("-w new new-tab --inheritEnvironment"), "Must use independent window and inherit caller environment");
        Check(info.Arguments.Contains(@"\;"), "Terminal delimiter must be escaped in executable path");
        Check(!info.UseShellExecute, "Do not pass user input through a shell");
        Check(info.Arguments.EndsWith("--qwen-terminal-child 0123456789abcdef0123456789abcdef"), "Must hand off through private pipe token");
    }
    private static void PortableExtraction()
    {
        string root = Path.Combine(Path.GetTempPath(), "qwen terminal tests " + Guid.NewGuid().ToString("N"));
        byte[] archive;
        using (var stream = new MemoryStream())
        {
            using (var zip = new ZipArchive(stream, ZipArchiveMode.Create, true))
                foreach (string name in new[] { "WindowsTerminal.exe", "Microsoft.Terminal.Settings.Model.dll", ".portable", "LICENSE", "NOTICE.html", "settings/settings.json" })
                    using (var writer = new StreamWriter(zip.CreateEntry(name).Open())) writer.Write("fixture");
            archive = stream.ToArray();
        }
        try
        {
            string exe = TerminalHost.EnsureExtracted(root, "1.2.3", delegate { return new MemoryStream(archive); });
            Check(File.Exists(exe), "Bundled Terminal was not extracted");
            string settings = Path.Combine(Path.GetDirectoryName(exe), "settings", "settings.json");
            File.WriteAllText(settings, "custom settings");
            TerminalHost.EnsureExtracted(root, "1.2.3", delegate { throw new Exception("Already extracted Terminal must not be reinstalled"); });
            Check(File.ReadAllText(settings) == "custom settings", "Existing portable settings changed");
            File.Delete(exe);
            TerminalHost.EnsureExtracted(root, "1.2.3", delegate { return new MemoryStream(archive); });
            Check(File.Exists(exe) && File.ReadAllText(settings) == "custom settings", "Repair must preserve Terminal settings");
        }
        finally { if (Directory.Exists(root)) Directory.Delete(root, true); }
    }
    private static void StartPipeChild(string token)
    {
        string exe = Process.GetCurrentProcess().MainModule.FileName;
        string prefix = Path.GetFileNameWithoutExtension(exe) == "dotnet" ? "\"" + Assembly.GetExecutingAssembly().Location + "\" " : "";
        Process.Start(new ProcessStartInfo(exe, prefix + "--qwen-terminal-child " + token) { UseShellExecute = false }).Dispose();
    }
    private static void ProcessHandoff()
    {
        var environment = new Dictionary<string, string> { { "QWEN_HOME", "custom-home" }, { "HANDOFF_TEST", "secret 中文 ;" } };
        int exit = TerminalHost.Handoff(StartPipeChild, new[] { "--resume", "a ; 中文", "" }, Directory.GetCurrentDirectory(), environment, 10000);
        Check(exit == 23, "Parent did not receive actual child exit code");
        bool timedOut = false;
        try { TerminalHost.Handoff(delegate(string token) { }, new string[0], Directory.GetCurrentDirectory(), environment, 100); }
        catch (TimeoutException) { timedOut = true; }
        Check(timedOut, "Failed Terminal startup must have a bounded wait");
        exit = TerminalHost.Handoff(StartPipeChild, new[] { "--disconnect" }, Directory.GetCurrentDirectory(), environment, 10000);
        Check(exit == 130, "Closed Terminal tab must release the waiting parent");
    }
    internal static int RunPipeChild(string token)
    {
        return TerminalHost.RunChild(token, delegate(string[] forwarded) {
                if (forwarded.Length == 1 && forwarded[0] == "--disconnect") Environment.Exit(0);
                Check(forwarded.Length == 3 && forwarded[1] == "a ; 中文" && forwarded[2] == "", "Child arguments corrupted");
                Check(Environment.GetEnvironmentVariable("HANDOFF_TEST") == "secret 中文 ;", "Child environment corrupted");
                Check(Environment.GetEnvironmentVariable("QWEN_HOME") == "custom-home", "Child home corrupted");
                return 23;
            });
    }
    private static int Main(string[] args)
    {
        if (args.Length == 2 && args[0] == "--qwen-terminal-child") return RunPipeChild(args[1]);
        try { Run(); Console.WriteLine("PASS: Terminal routing, handoff, exit code, timeout and portable extraction"); return 0; }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
    }
}
