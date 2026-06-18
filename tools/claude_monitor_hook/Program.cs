using ClaudeBoardMonitor;

if (args.Any(arg => arg.Equals("--statusline", StringComparison.OrdinalIgnoreCase)))
{
    return await HookClient.RunStatusLineAsync();
}

return await HookClient.RunAsync();
