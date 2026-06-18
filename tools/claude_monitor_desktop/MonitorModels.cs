using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace ClaudeBoardMonitor;

internal enum AiMode
{
    Idle,
    Working,
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

    public event Action? Changed;
    public event Action<string>? BoardLineRequested;

    public AiMode Mode { get; private set; } = AiMode.Idle;
    public string StatusText { get; private set; } = "IDLE";
    public bool SerialConnected { get; set; }
    public string SerialPortName { get; set; } = "";
    public bool HookServerOnline { get; set; }
    public bool HooksInstalled { get; set; }
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
        return request;
    }

    public bool Decide(string decision, string source)
    {
        var request = PendingApproval;
        if (request is null)
        {
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
}
