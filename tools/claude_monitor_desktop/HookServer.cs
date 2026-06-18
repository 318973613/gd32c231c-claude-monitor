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
                await WriteJsonAsync(context, "{\"ok\":true,\"app\":\"ClaudeBoardMonitor\"}");
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
            else if (path == "/approval")
            {
                await HandleApprovalAsync(context, doc.RootElement);
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
