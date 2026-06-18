using System;
using System.Windows.Forms;

namespace ClaudeBoardMonitor;

internal static class Program
{
    [STAThread]
    private static void Main()
    {
        if (Environment.GetCommandLineArgs().Any(arg => arg.Equals("--hook", StringComparison.OrdinalIgnoreCase)))
        {
            Environment.Exit(HookClient.RunAsync().GetAwaiter().GetResult());
            return;
        }
        if (Environment.GetCommandLineArgs().Any(arg => arg.Equals("--statusline", StringComparison.OrdinalIgnoreCase)))
        {
            Environment.Exit(HookClient.RunStatusLineAsync().GetAwaiter().GetResult());
            return;
        }
        if (Environment.GetCommandLineArgs().Any(arg => arg.Equals("--install-hooks", StringComparison.OrdinalIgnoreCase)))
        {
            var state = new MonitorState();
            HookSettingsInstaller.Install(state);
            Environment.Exit(0);
            return;
        }

        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
