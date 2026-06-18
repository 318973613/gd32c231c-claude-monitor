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

    public static async Task<int> RunStatusLineAsync()
    {
        string input;
        using (var reader = new StreamReader(Console.OpenStandardInput(), Encoding.UTF8))
        {
            input = await reader.ReadToEndAsync();
        }

        var text = BuildStatusLineText(input);
        await TryPostAsync("/status", BuildMonitorStatusJson(input), 1);
        using var stdout = Console.OpenStandardOutput();
        var bytes = Encoding.UTF8.GetBytes(text + Environment.NewLine);
        stdout.Write(bytes, 0, bytes.Length);
        return 0;
    }

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
            if (!await IsBoardApprovalEnabledAsync())
            {
                await TryPostAsync("/native-approval", input, 3);
                WriteJson(new
                {
                    hookSpecificOutput = new
                    {
                        hookEventName = "PreToolUse",
                        permissionDecision = "ask",
                        permissionDecisionReason = "desktop monitor recorded the request; Claude should ask in the terminal"
                    },
                    suppressOutput = true
                });
                return;
            }

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

    private static string BuildStatusLineText(string input)
    {
        try
        {
            using var doc = JsonDocument.Parse(string.IsNullOrWhiteSpace(input) ? "{}" : input);
            var root = doc.RootElement;
            var model = ReadStringPath(root, "model.display_name", "");
            if (string.IsNullOrWhiteSpace(model))
            {
                model = ReadStringPath(root, "model.name", "");
            }
            if (string.IsNullOrWhiteSpace(model))
            {
                model = ReadStringPath(root, "model.id", "Claude");
            }
            var context = ReadContextPercent(root);
            var cost = ReadDecimalPath(root, "cost.total_cost_usd", 0M);
            if (cost <= 0M)
            {
                cost = ReadDecimalPath(root, "total_cost_usd", 0M);
            }
            var contextText = context >= 0 ? $"CTX {context}%" : "CTX --";
            var costText = cost > 0M ? $"${cost:0.00}" : "$--";
            return $"Board {model} {contextText} {costText}";
        }
        catch
        {
            return "Board Claude CTX -- $--";
        }
    }

    private static string BuildMonitorStatusJson(string input)
    {
        try
        {
            using var doc = JsonDocument.Parse(string.IsNullOrWhiteSpace(input) ? "{}" : input);
            var root = doc.RootElement;
            var model = ReadStringPath(root, "model.display_name", "");
            if (string.IsNullOrWhiteSpace(model))
            {
                model = ReadStringPath(root, "model.name", "");
            }
            if (string.IsNullOrWhiteSpace(model))
            {
                model = ReadStringPath(root, "model.id", "Claude");
            }

            var context = ReadContextPercent(root);
            var limit = ReadPercent(root,
                "limit.percent_used",
                "rate_limit.percent_used",
                "usage.limit_percent",
                "usage.five_hour_percent",
                "session.limit_percent");
            var cost = ReadDecimalPath(root, "cost.total_cost_usd", 0M);
            if (cost <= 0M)
            {
                cost = ReadDecimalPath(root, "total_cost_usd", 0M);
            }

            return JsonSerializer.Serialize(new
            {
                model = new { display_name = model },
                context = new { percentage = context },
                limit = new { percent_used = limit },
                cost = new { total_cost_usd = cost }
            });
        }
        catch
        {
            return "{}";
        }
    }

    private static async Task HandlePermissionRequestAsync(string input)
    {
        try
        {
            if (!await IsBoardApprovalEnabledAsync())
            {
                await TryPostAsync("/native-approval", input, 3);
                WriteJson(new { @continue = true, suppressOutput = true });
                return;
            }

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

    private static async Task<bool> IsBoardApprovalEnabledAsync()
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
        return body.Contains("\"approvalMode\":\"board\"", StringComparison.OrdinalIgnoreCase);
    }

    private static string ReadString(JsonElement element, string name, string fallback)
    {
        return element.TryGetProperty(name, out var value) && value.ValueKind == JsonValueKind.String
            ? value.GetString() ?? fallback
            : fallback;
    }

    private static string ReadStringPath(JsonElement element, string path, string fallback)
    {
        if (!TryGetPath(element, path, out var value))
        {
            return fallback;
        }
        return value.ValueKind == JsonValueKind.String ? value.GetString() ?? fallback : fallback;
    }

    private static int ReadPercent(JsonElement element, params string[] paths)
    {
        foreach (var path in paths)
        {
            var value = ReadDecimalPath(element, path, -1M);
            if (value >= 0M)
            {
                if (value <= 1M)
                {
                    value *= 100M;
                }
                return Math.Clamp((int)Math.Round(value), 0, 100);
            }
        }
        return -1;
    }

    private static int ReadContextPercent(JsonElement element)
    {
        var used = ReadPercent(element,
            "context.percentage",
            "context.percent",
            "context.percent_used",
            "context.used_percent",
            "context.used_percentage",
            "context.usage_percent",
            "context_window.percentage",
            "context_window.percent",
            "context_window.percent_used",
            "context_window.used_percent",
            "context_window.used_percentage",
            "context_window.usage_percent",
            "contextWindow.percentage",
            "contextWindow.percent",
            "contextWindow.percentUsed",
            "contextWindow.usedPercent",
            "contextWindow.usedPercentage",
            "usage.context_percent",
            "usage.context_percentage",
            "usage.context_used_percent",
            "usage.context_usage_percent",
            "usage.context_window_percent",
            "usage.context_window_used_percent",
            "transcript.context_percent",
            "transcript.context_percentage");
        if (used >= 0)
        {
            return used;
        }

        var remaining = ReadPercent(element,
            "context.remaining_percent",
            "context.remaining_percentage",
            "context.percent_remaining",
            "context.percentage_remaining",
            "context_window.remaining_percent",
            "context_window.remaining_percentage",
            "context_window.percent_remaining",
            "context_window.percentage_remaining",
            "contextWindow.remainingPercent",
            "contextWindow.remainingPercentage",
            "usage.context_remaining_percent");
        if (remaining >= 0)
        {
            return Math.Clamp(100 - remaining, 0, 100);
        }

        return FindContextPercent(element, "");
    }

    private static int FindContextPercent(JsonElement element, string path)
    {
        if (element.ValueKind == JsonValueKind.Object)
        {
            foreach (var property in element.EnumerateObject())
            {
                var nextPath = string.IsNullOrEmpty(path) ? property.Name : path + "." + property.Name;
                if (property.Value.ValueKind is JsonValueKind.Number or JsonValueKind.String)
                {
                    var key = nextPath.Replace("_", "", StringComparison.Ordinal).ToLowerInvariant();
                    if (key.Contains("context") && (key.Contains("percent") || key.Contains("percentage")))
                    {
                        var value = ReadDecimalValue(property.Value, -1M);
                        if (value >= 0M)
                        {
                            if (value <= 1M)
                            {
                                value *= 100M;
                            }
                            var percent = Math.Clamp((int)Math.Round(value), 0, 100);
                            return key.Contains("remaining") ? 100 - percent : percent;
                        }
                    }
                }

                var nested = FindContextPercent(property.Value, nextPath);
                if (nested >= 0)
                {
                    return nested;
                }
            }
        }
        else if (element.ValueKind == JsonValueKind.Array)
        {
            foreach (var item in element.EnumerateArray())
            {
                var nested = FindContextPercent(item, path);
                if (nested >= 0)
                {
                    return nested;
                }
            }
        }

        return -1;
    }

    private static decimal ReadDecimalPath(JsonElement element, string path, decimal fallback)
    {
        if (!TryGetPath(element, path, out var value))
        {
            return fallback;
        }
        return ReadDecimalValue(value, fallback);
    }

    private static decimal ReadDecimalValue(JsonElement value, decimal fallback)
    {
        if (value.ValueKind == JsonValueKind.Number && value.TryGetDecimal(out var number))
        {
            return number;
        }
        if (value.ValueKind == JsonValueKind.String && decimal.TryParse(value.GetString(), out number))
        {
            return number;
        }
        return fallback;
    }

    private static bool TryGetPath(JsonElement element, string path, out JsonElement value)
    {
        value = element;
        foreach (var part in path.Split('.'))
        {
            if (value.ValueKind != JsonValueKind.Object || !value.TryGetProperty(part, out value))
            {
                return false;
            }
        }
        return true;
    }

    private static void WriteJson(object payload)
    {
        var json = JsonSerializer.Serialize(payload);
        using var stdout = Console.OpenStandardOutput();
        var bytes = Encoding.UTF8.GetBytes(json);
        stdout.Write(bytes, 0, bytes.Length);
    }
}
