using System;
using System.IO;
using System.Net.Http;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace ClaudeBoardMonitor;

internal static class HookClient
{
    private static readonly string[] ApprovalTools =
    {
        "Bash",
        "Write",
        "Edit",
        "MultiEdit",
        "NotebookEdit"
    };

    public static async Task<int> RunAsync()
    {
        string input;
        using (var reader = new StreamReader(Console.OpenStandardInput(), Encoding.UTF8))
        {
            input = await reader.ReadToEndAsync();
        }

        JsonDocument doc;
        try
        {
            doc = JsonDocument.Parse(string.IsNullOrWhiteSpace(input) ? "{}" : input);
        }
        catch (Exception ex)
        {
            WriteJson(new { @continue = true, suppressOutput = true, systemMessage = "Hook JSON parse failed: " + ex.Message });
            return 0;
        }

        using (doc)
        {
            var root = doc.RootElement;
            var hook = ReadString(root, "hook_event_name", "");
            if (hook == "PreToolUse")
            {
                await HandlePreToolUseAsync(input, root);
            }
            else if (hook == "PermissionRequest")
            {
                await HandlePermissionRequestAsync(input);
            }
            else
            {
                await HandleEventAsync(input, hook);
            }
        }

        return 0;
    }

    private static async Task HandlePreToolUseAsync(string input, JsonElement root)
    {
        var tool = ReadString(root, "tool_name", "");
        if (Array.IndexOf(ApprovalTools, tool) < 0)
        {
            await TryPostAsync("/event", input, 3);
            WriteJson(new { @continue = true, suppressOutput = true });
            return;
        }

        try
        {
            await EnsureMonitorAsync();
            var response = await PostAsync("/approval", input, 115);
            using var doc = JsonDocument.Parse(response);
            var decision = ReadString(doc.RootElement, "decision", "deny") == "allow" ? "allow" : "deny";
            var reason = ReadString(doc.RootElement, "reason", "desktop monitor decision");
            WriteJson(new
            {
                hookSpecificOutput = new
                {
                    hookEventName = "PreToolUse",
                    permissionDecision = decision,
                    permissionDecisionReason = reason
                },
                systemMessage = "Board monitor decision: " + decision
            });
        }
        catch (Exception ex)
        {
            WriteJson(new
            {
                hookSpecificOutput = new
                {
                    hookEventName = "PreToolUse",
                    permissionDecision = "ask",
                    permissionDecisionReason = "board monitor unavailable: " + ex.Message
                },
                systemMessage = "Board monitor unavailable; falling back to Claude permission prompt."
            });
        }
    }

    private static async Task HandlePermissionRequestAsync(string input)
    {
        try
        {
            await EnsureMonitorAsync();
            var response = await PostAsync("/approval", input, 115);
            using var doc = JsonDocument.Parse(response);
            var behavior = ReadString(doc.RootElement, "decision", "deny") == "allow" ? "allow" : "deny";
            var reason = ReadString(doc.RootElement, "reason", "desktop monitor decision");
            WriteJson(new
            {
                hookSpecificOutput = new
                {
                    hookEventName = "PermissionRequest",
                    decision = new
                    {
                        behavior,
                        message = reason
                    }
                },
                systemMessage = "Board monitor permission decision: " + behavior
            });
        }
        catch (Exception ex)
        {
            WriteJson(new
            {
                @continue = true,
                suppressOutput = true,
                systemMessage = "Board monitor unavailable for PermissionRequest: " + ex.Message
            });
        }
    }

    private static async Task HandleEventAsync(string input, string hook)
    {
        await TryPostAsync("/event", input, 3);
        if (hook == "Stop")
        {
            WriteJson(new { decision = "approve", reason = "monitor event recorded", suppressOutput = true });
        }
        else
        {
            WriteJson(new { @continue = true, suppressOutput = true });
        }
    }

    private static async Task<bool> TryPostAsync(string path, string input, int timeoutSeconds)
    {
        try
        {
            await PostAsync(path, input, timeoutSeconds);
            return true;
        }
        catch
        {
            return false;
        }
    }

    private static async Task<string> PostAsync(string path, string input, int timeoutSeconds)
    {
        using var handler = new HttpClientHandler { UseProxy = false };
        using var client = new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(timeoutSeconds) };
        using var content = new StringContent(input, Encoding.UTF8, "application/json");
        using var response = await client.PostAsync("http://127.0.0.1:8765" + path, content);
        response.EnsureSuccessStatusCode();
        return await response.Content.ReadAsStringAsync();
    }

    private static async Task EnsureMonitorAsync()
    {
        using var handler = new HttpClientHandler { UseProxy = false };
        using var client = new HttpClient(handler) { Timeout = TimeSpan.FromSeconds(2) };
        using var response = await client.GetAsync("http://127.0.0.1:8765/health");
        response.EnsureSuccessStatusCode();
        var body = await response.Content.ReadAsStringAsync();
        if (!body.Contains("ClaudeBoardMonitor", StringComparison.OrdinalIgnoreCase))
        {
            throw new InvalidOperationException("desktop monitor health check failed");
        }
    }

    private static string ReadString(JsonElement element, string name, string fallback)
    {
        return element.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? fallback
            : fallback;
    }

    private static void WriteJson(object payload)
    {
        var json = JsonSerializer.Serialize(payload);
        using var stdout = Console.OpenStandardOutput();
        var bytes = Encoding.UTF8.GetBytes(json);
        stdout.Write(bytes, 0, bytes.Length);
    }
}
