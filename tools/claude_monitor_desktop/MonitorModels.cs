using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace ClaudeBoardMonitor;

internal enum AiMode
{
    Idle,
    Working,
    NativeApproval,
    WaitingApproval,
    Approved,
    Denied,
    Error
}

internal sealed record MonitorEvent(
    DateTime Time,
    string Kind,
    string Title,
    string Detail,
    string Source
);

internal sealed class ApprovalRequest
{
    public int Id { get; init; }
    public string Tool { get; init; } = "";
    public string Summary { get; init; } = "";
    public DateTime CreatedAt { get; init; } = DateTime.Now;
    public TaskCompletionSource<string> Decision { get; } =
        new(TaskCreationOptions.RunContinuationsAsynchronously);
}

internal sealed class MonitorState
{
    private readonly object _gate = new();
    private readonly List<MonitorEvent> _events = new();
    private int _approvalSeq;
    private string _queuedBoardDecision = "";
    private string _queuedBoardSource = "";
    private DateTime _queuedBoardDecisionAt = DateTime.MinValue;

    public event Action? Changed;
    public event Action<string>? BoardLineRequested;

    public AiMode Mode { get; private set; } = AiMode.Idle;
    public string StatusText { get; private set; } = "IDLE";
    public bool SerialConnected { get; set; }
    public string SerialPortName { get; set; } = "";
    public bool HookServerOnline { get; set; }
    public bool HooksInstalled { get; set; }
    public bool BoardApprovalEnabled { get; set; } = true;
    public string ModelName { get; private set; } = "UNKNOWN";
    public int ContextUsedPercent { get; private set; } = -1;
    public int ContextRemainingPercent { get; private set; } = -1;
    public int LimitUsedPercent { get; private set; } = -1;
    public decimal TotalCostUsd { get; private set; }
    public ApprovalRequest? PendingApproval { get; private set; }
    public IReadOnlyList<MonitorEvent> Events => _events;

    public void AddEvent(string kind, string title, string detail, string source)
    {
        lock (_gate)
        {
            _events.Add(new MonitorEvent(DateTime.Now, kind, title, detail, source));
            if (_events.Count > 400)
            {
                _events.RemoveRange(0, _events.Count - 400);
            }
        }
        Changed?.Invoke();
    }

    public void SetMode(AiMode mode, string status, string? boardLine = null)
    {
        Mode = mode;
        StatusText = status;
        Changed?.Invoke();
        if (!string.IsNullOrWhiteSpace(boardLine))
        {
            BoardLineRequested?.Invoke(boardLine);
        }
    }

    public void UpdateRuntimeStatus(string modelName, int contextUsedPercent, int contextRemainingPercent, int limitUsedPercent, decimal totalCostUsd)
    {
        ModelName = string.IsNullOrWhiteSpace(modelName) ? "UNKNOWN" : modelName;
        ContextUsedPercent = ClampPercentOrUnknown(contextUsedPercent);
        ContextRemainingPercent = ClampPercentOrUnknown(contextRemainingPercent);
        LimitUsedPercent = ClampPercentOrUnknown(limitUsedPercent);
        TotalCostUsd = totalCostUsd < 0 ? 0 : totalCostUsd;
        Changed?.Invoke();

        EmitRuntimeStatusToBoard();
    }

    public void EmitRuntimeStatusToBoard()
    {
        BoardLineRequested?.Invoke("MODEL=" + ToBoardText(ModelName, 14));
        if (ContextUsedPercent >= 0)
        {
            var remaining = ContextRemainingPercent >= 0 ? ContextRemainingPercent : 100 - ContextUsedPercent;
            BoardLineRequested?.Invoke($"CTX={ContextUsedPercent},{remaining}");
        }
        if (LimitUsedPercent >= 0)
        {
            BoardLineRequested?.Invoke($"RATE={LimitUsedPercent},{100 - LimitUsedPercent}");
        }
        if (TotalCostUsd > 0)
        {
            var cents = (int)Math.Clamp(Math.Round(TotalCostUsd * 100M), 0M, 9999M);
            BoardLineRequested?.Invoke($"COST={cents}");
        }
    }

    public ApprovalRequest BeginApproval(string tool, string summary)
    {
        var request = new ApprovalRequest
        {
            Id = ++_approvalSeq,
            Tool = tool,
            Summary = summary.Length > 120 ? summary[..120] : summary
        };
        PendingApproval = request;
        AddEvent("approval", $"WAIT #{request.Id} {tool}", summary, "Claude");
        SetMode(AiMode.WaitingApproval, "WAITING APPROVAL", $"AI=WAIT_APPROVE:{tool}");
        if (TryConsumeQueuedBoardDecision(out var queuedDecision, out var queuedSource))
        {
            AddEvent("decision", $"BUFFERED {queuedDecision.ToUpperInvariant()} #{request.Id}", request.Tool, queuedSource);
            Decide(queuedDecision, queuedSource);
        }
        return request;
    }

    public void BeginNativeApproval(string tool, string summary)
    {
        PendingApproval = null;
        AddEvent("approval", $"TERMINAL CONFIRM {tool}", summary, "Claude");
        SetMode(AiMode.NativeApproval, "TERMINAL APPROVAL", $"AI=NATIVE_APPROVE:{tool}");
    }

    public bool Decide(string decision, string source)
    {
        var request = PendingApproval;
        if (request is null)
        {
            QueueBoardDecisionIfUseful(decision, source);
            AddEvent("button", $"{source}: no pending approval", "", "Board");
            return false;
        }

        PendingApproval = null;
        request.Decision.TrySetResult(decision);
        if (decision == "allow")
        {
            SetMode(AiMode.Approved, "APPROVED", "AI=APPROVED");
        }
        else
        {
            SetMode(AiMode.Denied, "DENIED", "AI=DENIED");
        }
        AddEvent("decision", $"{decision.ToUpperInvariant()} #{request.Id}", request.Tool, source);
        return true;
    }

    private void QueueBoardDecisionIfUseful(string decision, string source)
    {
        if (Mode != AiMode.Working && Mode != AiMode.WaitingApproval)
        {
            return;
        }
        if (!source.StartsWith("PA", StringComparison.OrdinalIgnoreCase))
        {
            return;
        }

        _queuedBoardDecision = decision;
        _queuedBoardSource = source;
        _queuedBoardDecisionAt = DateTime.Now;
        AddEvent("button", $"{source}: buffered {decision}", "valid for 2.5s", "Board");
    }

    private bool TryConsumeQueuedBoardDecision(out string decision, out string source)
    {
        decision = "";
        source = "";
        if (string.IsNullOrWhiteSpace(_queuedBoardDecision))
        {
            return false;
        }
        if ((DateTime.Now - _queuedBoardDecisionAt) > TimeSpan.FromMilliseconds(2500))
        {
            _queuedBoardDecision = "";
            _queuedBoardSource = "";
            return false;
        }

        decision = _queuedBoardDecision;
        source = _queuedBoardSource;
        _queuedBoardDecision = "";
        _queuedBoardSource = "";
        return true;
    }

    private static int ClampPercentOrUnknown(int value)
    {
        if (value < 0)
        {
            return -1;
        }
        if (value > 100)
        {
            return 100;
        }
        return value;
    }

    private static string ToBoardText(string text, int maxLength)
    {
        var chars = text.ToUpperInvariant()
            .Where(ch => (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' || ch == '.')
            .Take(maxLength)
            .ToArray();
        return chars.Length == 0 ? "UNKNOWN" : new string(chars);
    }
}
