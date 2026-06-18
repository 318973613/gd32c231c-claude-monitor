using System;
using System.IO;
using System.Net;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace ClaudeBoardMonitor;

internal sealed class HookServer : IDisposable
{
    private const string Prefix = "http://127.0.0.1:8765/";
    private readonly HttpListener _listener = new();
    private readonly MonitorState _state;
    private CancellationTokenSource? _cts;

    public HookServer(MonitorState state)
    {
        _state = state;
        _listener.Prefixes.Add(Prefix);
    }

    public void Start()
    {
        if (_cts is not null)
        {
            return;
        }

        _cts = new CancellationTokenSource();
        _listener.Start();
        _state.HookServerOnline = true;
        _state.AddEvent("system", "Hook server online", Prefix, "Monitor");
        _ = Task.Run(() => ListenLoopAsync(_cts.Token));
    }

    private async Task ListenLoopAsync(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            try
            {
                var context = await _listener.GetContextAsync();
                _ = Task.Run(() => HandleAsync(context), token);
            }
            catch when (token.IsCancellationRequested)
            {
                break;
            }
            catch (Exception ex)
            {
                _state.AddEvent("error", "Hook server error", ex.Message, "HTTP");
            }
        }
    }

    private async Task HandleAsync(HttpListenerContext context)
    {
        try
        {
            if (context.Request.HttpMethod == "GET" && context.Request.Url?.AbsolutePath == "/health")
            {
                var mode = _state.BoardApprovalEnabled ? "board" : "claude";
                await WriteJsonAsync(context, $"{{\"ok\":true,\"app\":\"ClaudeBoardMonitor\",\"approvalMode\":\"{mode}\"}}");
                return;
            }

            if (context.Request.HttpMethod != "POST")
            {
                context.Response.StatusCode = 405;
                context.Response.Close();
                return;
            }

            using var reader = new StreamReader(context.Request.InputStream, Encoding.UTF8);
            var body = await reader.ReadToEndAsync();
            using var doc = JsonDocument.Parse(string.IsNullOrWhiteSpace(body) ? "{}" : body);
            var path = context.Request.Url?.AbsolutePath ?? "";

            if (path == "/event")
            {
                await HandleEventAsync(context, doc.RootElement);
            }
            else if (path == "/status")
            {
                await HandleStatusAsync(context, doc.RootElement);
            }
            else if (path == "/approval")
            {
                await HandleApprovalAsync(context, doc.RootElement);
            }
            else if (path == "/native-approval")
            {
                await HandleNativeApprovalAsync(context, doc.RootElement);
            }
            else
            {
                context.Response.StatusCode = 404;
                await WriteJsonAsync(context, "{\"ok\":false}");
            }
        }
        catch (Exception ex)
        {
            _state.AddEvent("error", "HTTP request failed", ex.Message, "HTTP");
            context.Response.StatusCode = 500;
            await WriteJsonAsync(context, "{\"ok\":false}");
        }
    }

    private Task HandleEventAsync(HttpListenerContext context, JsonElement payload)
    {
        var hook = ReadString(payload, "hook_event_name", "Unknown");
        var tool = ReadString(payload, "tool_name", "");
        var summary = BuildSummary(hook, tool, payload);

        _state.AddEvent("hook", summary, "", "Claude");
        switch (hook)
        {
            case "UserPromptSubmit":
                _state.SetMode(AiMode.Working, "WORKING", "AI=WORKING");
                break;
            case "PreToolUse":
                _state.SetMode(AiMode.Working, $"TOOL {tool}", $"AI=TOOL:{tool}");
                break;
            case "PostToolUse":
            case "PostToolUseFailure":
                _state.SetMode(AiMode.Working, "WORKING", "AI=WORKING");
                break;
            case "Notification":
                _state.SetMode(AiMode.Working, "NOTICE", "AI=WORKING");
                break;
            case "Stop":
                _state.SetMode(AiMode.Idle, "IDLE", "AI=DONE");
                break;
        }

        return WriteJsonAsync(context, "{\"ok\":true}");
    }

    private Task HandleStatusAsync(HttpListenerContext context, JsonElement payload)
    {
        var model = ReadStringPath(payload, "model.display_name", "");
        if (string.IsNullOrWhiteSpace(model))
        {
            model = ReadStringPath(payload, "model.name", "");
        }
        if (string.IsNullOrWhiteSpace(model))
        {
            model = ReadStringPath(payload, "model.id", "");
        }
        if (string.IsNullOrWhiteSpace(model))
        {
            model = HookSettingsInstaller.GetConfiguredModel();
        }

        var contextUsed = ReadContextPercent(payload);
        var contextRemaining = contextUsed >= 0 ? 100 - contextUsed : -1;
        var limitUsed = ReadPercent(payload,
            "limit.percent_used",
            "rate_limit.percent_used",
            "usage.limit_percent",
            "usage.five_hour_percent",
            "session.limit_percent");
        var cost = ReadDecimalPath(payload, "cost.total_cost_usd", 0M);
        if (cost <= 0M)
        {
            cost = ReadDecimalPath(payload, "total_cost_usd", 0M);
        }

        _state.UpdateRuntimeStatus(model, contextUsed, contextRemaining, limitUsed, cost);
        return WriteJsonAsync(context, "{\"ok\":true}");
    }

    private async Task HandleApprovalAsync(HttpListenerContext context, JsonElement payload)
    {
        var hook = ReadString(payload, "hook_event_name", "Approval");
        var tool = ReadString(payload, "tool_name", "TOOL");
        var request = _state.BeginApproval(tool, BuildSummary(hook, tool, payload));

        var completed = await Task.WhenAny(request.Decision.Task, Task.Delay(TimeSpan.FromSeconds(110)));
        if (completed != request.Decision.Task)
        {
            _state.SetMode(AiMode.Error, "APPROVAL TIMEOUT", "AI=ERROR:APPROVAL_TIMEOUT");
            _state.AddEvent("error", $"Approval timeout #{request.Id}", tool, "Monitor");
            await WriteJsonAsync(context, "{\"decision\":\"deny\",\"reason\":\"approval timeout\"}");
            return;
        }

        var decision = await request.Decision.Task;
        await WriteJsonAsync(context, $"{{\"decision\":\"{decision}\",\"reason\":\"desktop monitor decision\"}}");
    }

    private Task HandleNativeApprovalAsync(HttpListenerContext context, JsonElement payload)
    {
        var hook = ReadString(payload, "hook_event_name", "Approval");
        var tool = ReadString(payload, "tool_name", "TOOL");
        _state.BeginNativeApproval(tool, BuildSummary(hook, tool, payload));
        return WriteJsonAsync(context, "{\"ok\":true}");
    }

    private static string BuildSummary(string hook, string tool, JsonElement payload)
    {
        if (hook is "PreToolUse" or "PermissionRequest" or "PostToolUse" or "PostToolUseFailure")
        {
            var hint = "";
            if (payload.TryGetProperty("tool_input", out var input) && input.ValueKind == JsonValueKind.Object)
            {
                hint = ReadString(input, "file_path", "") != "" ? ReadString(input, "file_path", "") : ReadString(input, "command", "");
            }
            return string.IsNullOrWhiteSpace(hint) ? $"{hook} {tool}" : $"{hook} {tool}: {hint}";
        }

        if (hook == "UserPromptSubmit")
        {
            var prompt = ReadString(payload, "user_prompt", "");
            return prompt.Length > 80 ? $"Prompt: {prompt[..80]}" : $"Prompt: {prompt}";
        }

        return hook;
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

    private static async Task WriteJsonAsync(HttpListenerContext context, string json)
    {
        var bytes = Encoding.UTF8.GetBytes(json);
        context.Response.ContentType = "application/json; charset=utf-8";
        context.Response.ContentLength64 = bytes.Length;
        await context.Response.OutputStream.WriteAsync(bytes);
        context.Response.Close();
    }

    public void Dispose()
    {
        _cts?.Cancel();
        if (_listener.IsListening)
        {
            _listener.Stop();
        }
        _listener.Close();
        _cts?.Dispose();
    }
}
