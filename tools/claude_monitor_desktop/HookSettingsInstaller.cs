using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace ClaudeBoardMonitor;

internal static class HookSettingsInstaller
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        WriteIndented = true
    };

    public static string SettingsPath =>
        Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".claude", "settings.json");

    public static bool AreHooksInstalled()
    {
        try
        {
            if (!File.Exists(SettingsPath))
            {
                return false;
            }
            var root = JsonNode.Parse(File.ReadAllText(SettingsPath)) as JsonObject;
            var hooks = root?["hooks"] as JsonObject;
            var statusLine = root?["statusLine"] as JsonObject;
            if (hooks is null || statusLine is null)
            {
                return false;
            }

            var hookText = hooks.ToJsonString();
            var statusText = statusLine.ToJsonString();
            var hasHook = hookText.Contains("ClaudeBoardHook", StringComparison.OrdinalIgnoreCase);
            var hasStatusLine = statusText.Contains("ClaudeBoardHook", StringComparison.OrdinalIgnoreCase) &&
                                statusText.Contains("--statusline", StringComparison.OrdinalIgnoreCase);
            var hasAllPreToolUse = hookText.Contains("\"PreToolUse\"", StringComparison.OrdinalIgnoreCase) &&
                                   HasMatcher(hooks, "PreToolUse", "*");
            return hasHook && hasStatusLine && hasAllPreToolUse;
        }
        catch
        {
            return false;
        }
    }

    public static void Install(MonitorState state)
    {
        Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);
        JsonObject root;
        if (File.Exists(SettingsPath))
        {
            File.Copy(SettingsPath, SettingsPath + ".bak-" + DateTime.Now.ToString("yyyyMMddHHmmss"), overwrite: false);
            root = JsonNode.Parse(File.ReadAllText(SettingsPath)) as JsonObject ?? new JsonObject();
        }
        else
        {
            root = new JsonObject();
        }

        var hooks = root["hooks"] as JsonObject ?? new JsonObject();
        root["hooks"] = hooks;

        var command = BuildHookCommand();
        UpsertEvent(hooks, "UserPromptSubmit", "*", command, 10);
        UpsertEvent(hooks, "PreToolUse", "*", command, 120);
        UpsertEvent(hooks, "PermissionRequest", "*", command, 120);
        UpsertEvent(hooks, "PostToolUse", "*", command, 10);
        UpsertEvent(hooks, "PostToolUseFailure", "*", command, 10);
        UpsertEvent(hooks, "Notification", "*", command, 10);
        UpsertEvent(hooks, "Stop", "*", command, 10);
        root["statusLine"] = BuildStatusLineConfig();

        File.WriteAllText(SettingsPath, root.ToJsonString(JsonOptions));
        state.HooksInstalled = true;
        state.AddEvent("system", "Claude hooks installed", SettingsPath, "Monitor");
    }

    public static string GetConfiguredModel()
    {
        try
        {
            if (!File.Exists(SettingsPath))
            {
                return "";
            }

            var root = JsonNode.Parse(File.ReadAllText(SettingsPath)) as JsonObject;
            var model = root?["model"]?.GetValue<string>() ?? "";
            if (!string.IsNullOrWhiteSpace(model))
            {
                return model;
            }

            var env = root?["env"] as JsonObject;
            return env?["ANTHROPIC_MODEL"]?.GetValue<string>() ?? "";
        }
        catch
        {
            return "";
        }
    }

    private static bool HasMatcher(JsonObject hooks, string eventName, string matcher)
    {
        if (hooks[eventName] is not JsonArray eventHooks)
        {
            return false;
        }

        foreach (var item in eventHooks)
        {
            if (item is JsonObject group &&
                string.Equals(group["matcher"]?.GetValue<string>(), matcher, StringComparison.Ordinal))
            {
                return true;
            }
        }

        return false;
    }

    private static void UpsertEvent(JsonObject hooks, string eventName, string matcher, string command, int timeout)
    {
        var eventHooks = hooks[eventName] as JsonArray ?? new JsonArray();
        RemoveExistingMonitorHooks(eventHooks);
        eventHooks.Add(BuildHookGroup(matcher, command, timeout));
        hooks[eventName] = eventHooks;
    }

    private static JsonObject BuildHookGroup(string matcher, string command, int timeout)
    {
        return new JsonObject
        {
            ["matcher"] = matcher,
            ["hooks"] = new JsonArray
            {
                new JsonObject
                {
                    ["type"] = "command",
                    ["command"] = command,
                    ["timeout"] = timeout
                }
            }
        };
    }

    private static void RemoveExistingMonitorHooks(JsonArray eventHooks)
    {
        for (var i = eventHooks.Count - 1; i >= 0; i--)
        {
            var text = eventHooks[i]?.ToJsonString() ?? "";
            if (text.Contains("ClaudeBoardMonitor", StringComparison.OrdinalIgnoreCase) ||
                text.Contains("ClaudeBoardHook", StringComparison.OrdinalIgnoreCase) ||
                text.Contains("claude_hook.py", StringComparison.OrdinalIgnoreCase))
            {
                eventHooks.RemoveAt(i);
            }
        }
    }

    private static string BuildHookCommand()
    {
        var exe = Path.Combine(AppContext.BaseDirectory, "ClaudeBoardHook.exe");
        return $"\"{exe}\" --hook";
    }

    private static JsonObject BuildStatusLineConfig()
    {
        var exe = Path.Combine(AppContext.BaseDirectory, "ClaudeBoardHook.exe");
        return new JsonObject
        {
            ["type"] = "command",
            ["command"] = $"\"{exe}\" --statusline",
            ["padding"] = 0
        };
    }
}
