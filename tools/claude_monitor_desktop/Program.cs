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

        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
