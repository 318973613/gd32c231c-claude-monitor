using System;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Windows.Forms;

namespace ClaudeBoardMonitor;

internal sealed class MainForm : Form
{
    private readonly MonitorState _state = new();
    private readonly SerialPort _serial = new();
    private readonly HookServer _server;

    private ComboBox _portBox = null!;
    private Label _status = null!;
    private Label _hookDot = null!;
    private Label _serialDot = null!;
    private Label _hooksDot = null!;
    private ListBox _events = null!;
    private TextBox _serialLog = null!;
    private Label _approvalTitle = null!;
    private Label _approvalBody = null!;

    private readonly Color _bg = Color.FromArgb(8, 12, 19);
    private readonly Color _panel = Color.FromArgb(15, 23, 36);
    private readonly Color _panel2 = Color.FromArgb(21, 31, 47);
    private readonly Color _text = Color.FromArgb(236, 244, 255);
    private readonly Color _muted = Color.FromArgb(134, 151, 176);
    private readonly Color _green = Color.FromArgb(34, 197, 94);
    private readonly Color _red = Color.FromArgb(239, 68, 68);
    private readonly Color _amber = Color.FromArgb(245, 158, 11);

    public MainForm()
    {
        Text = "Claude Board Monitor";
        Width = 1180;
        Height = 740;
        MinimumSize = new Size(1040, 660);
        BackColor = _bg;
        Font = new Font("Segoe UI", 10);

        _server = new HookServer(_state);
        _state.Changed += SafeRefresh;
        _state.BoardLineRequested += SendBoardLine;
        _serial.DataReceived += SerialDataReceived;

        BuildUi();
        RefreshPorts();
        try
        {
            _server.Start();
            _state.HooksInstalled = HookSettingsInstaller.AreHooksInstalled();
        }
        catch (Exception ex)
        {
            _state.AddEvent("error", "Hook server failed", ex.Message, "Monitor");
        }
        RefreshUi();
    }

    private void BuildUi()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            Padding = new Padding(18),
            RowCount = 2,
            ColumnCount = 1,
            BackColor = _bg
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 88));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Controls.Add(root);

        root.Controls.Add(BuildHeader(), 0, 0);

        var body = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, BackColor = _bg };
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 270));
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 360));
        root.Controls.Add(body, 0, 1);

        body.Controls.Add(BuildLeftPanel(), 0, 0);
        body.Controls.Add(BuildEventPanel(), 1, 0);
        body.Controls.Add(BuildRightPanel(), 2, 0);
    }

    private Control BuildHeader()
    {
        var header = Panel(_bg);
        var title = Label("Claude Board Monitor", 28, FontStyle.Bold, _text);
        title.Location = new Point(4, 5);
        title.AutoSize = true;
        header.Controls.Add(title);

        var sub = Label("Claude hooks -> Windows monitor -> USART0 -> GD32C231C", 10, FontStyle.Regular, _muted);
        sub.Location = new Point(8, 52);
        sub.AutoSize = true;
        header.Controls.Add(sub);

        _status = Label("IDLE", 24, FontStyle.Bold, _green);
        _status.TextAlign = ContentAlignment.MiddleRight;
        _status.Anchor = AnchorStyles.Top | AnchorStyles.Right;
        _status.Location = new Point(760, 20);
        _status.Size = new Size(360, 42);
        header.Controls.Add(_status);
        return header;
    }

    private Control BuildLeftPanel()
    {
        var panel = Panel(_panel);
        panel.Padding = new Padding(16);
        panel.Margin = new Padding(0, 0, 12, 0);

        AddSectionTitle(panel, "Connection", 14);
        _portBox = new ComboBox { Location = new Point(18, 56), Size = new Size(136, 30), DropDownStyle = ComboBoxStyle.DropDownList };
        panel.Controls.Add(_portBox);
        AddButton(panel, "Refresh", 164, 55, 82, 32, RefreshPorts, _panel2, _text);
        AddButton(panel, "Connect", 18, 98, 108, 40, ConnectSerial, Color.FromArgb(28, 93, 57), _text);
        AddButton(panel, "Disconnect", 138, 98, 108, 40, DisconnectSerial, Color.FromArgb(64, 45, 45), _text);

        AddSectionTitle(panel, "Automation", 170);
        AddButton(panel, "Install Claude Hooks", 18, 212, 228, 44, InstallHooks, Color.FromArgb(34, 88, 151), _text);
        AddButton(panel, "Open settings.json", 18, 266, 228, 38, OpenSettings, _panel2, _text);
        AddButton(panel, "Test: Working", 18, 330, 108, 38, () => _state.SetMode(AiMode.Working, "WORKING", "AI=WORKING"), _panel2, _text);
        AddButton(panel, "Test: Idle", 138, 330, 108, 38, () => _state.SetMode(AiMode.Idle, "IDLE", "AI=IDLE"), _panel2, _text);

        AddSectionTitle(panel, "Health", 402);
        _hookDot = Dot("Hook server", 18, 444);
        _serialDot = Dot("Board serial", 18, 478);
        _hooksDot = Dot("Claude hooks", 18, 512);
        panel.Controls.Add(_hookDot);
        panel.Controls.Add(_serialDot);
        panel.Controls.Add(_hooksDot);
        return panel;
    }

    private Control BuildEventPanel()
    {
        var panel = Panel(_panel);
        panel.Padding = new Padding(16);
        panel.Margin = new Padding(0, 0, 12, 0);
        AddSectionTitle(panel, "Live Claude Events", 14);
        _events = new ListBox
        {
            Location = new Point(18, 56),
            Size = new Size(470, 560),
            Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            BackColor = Color.FromArgb(5, 9, 14),
            ForeColor = _text,
            BorderStyle = BorderStyle.None,
            Font = new Font("Cascadia Mono", 10)
        };
        panel.Controls.Add(_events);
        return panel;
    }

    private Control BuildRightPanel()
    {
        var panel = Panel(_panel);
        panel.Padding = new Padding(16);

        AddSectionTitle(panel, "Board Approval", 14);
        var approval = Panel(_panel2);
        approval.Location = new Point(18, 56);
        approval.Size = new Size(320, 176);
        panel.Controls.Add(approval);

        _approvalTitle = Label("No pending request", 13, FontStyle.Bold, _text);
        _approvalTitle.Location = new Point(14, 14);
        _approvalTitle.Size = new Size(292, 32);
        approval.Controls.Add(_approvalTitle);
        _approvalBody = Label("PA0 approve / PA4 deny. GUI buttons are available for testing.", 10, FontStyle.Regular, _muted);
        _approvalBody.Location = new Point(14, 52);
        _approvalBody.Size = new Size(292, 52);
        approval.Controls.Add(_approvalBody);
        AddButton(approval, "Deny", 14, 118, 138, 42, () => _state.Decide("deny", "GUI"), Color.FromArgb(111, 42, 48), _text);
        AddButton(approval, "Approve", 166, 118, 138, 42, () => _state.Decide("allow", "GUI"), Color.FromArgb(26, 122, 69), _text);

        AddSectionTitle(panel, "Serial Log", 260);
        _serialLog = new TextBox
        {
            Location = new Point(18, 302),
            Size = new Size(320, 314),
            Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            Multiline = true,
            ReadOnly = true,
            ScrollBars = ScrollBars.Vertical,
            BackColor = Color.FromArgb(5, 9, 14),
            ForeColor = Color.FromArgb(222, 234, 248),
            BorderStyle = BorderStyle.None,
            Font = new Font("Cascadia Mono", 9)
        };
        panel.Controls.Add(_serialLog);
        return panel;
    }

    private void ConnectSerial()
    {
        try
        {
            if (_portBox.SelectedItem is null)
            {
                return;
            }
            _serial.PortName = _portBox.SelectedItem.ToString() ?? "";
            _serial.BaudRate = 115200;
            _serial.NewLine = "\n";
            _serial.Open();
            _state.SerialConnected = true;
            _state.SerialPortName = _serial.PortName;
            _state.AddEvent("system", $"Serial connected {_serial.PortName}", "115200 8N1", "Monitor");
            SendBoardLine("AI=IDLE");
            RefreshUi();
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Serial connect failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void DisconnectSerial()
    {
        if (_serial.IsOpen)
        {
            _serial.Close();
        }
        _state.SerialConnected = false;
        _state.AddEvent("system", "Serial disconnected", "", "Monitor");
        RefreshUi();
    }

    private void SerialDataReceived(object? sender, SerialDataReceivedEventArgs e)
    {
        try
        {
            var line = _serial.ReadLine().Trim();
            BeginInvoke(() => HandleBoardLine(line));
        }
        catch
        {
        }
    }

    private void HandleBoardLine(string line)
    {
        AddSerialLog("Board -> PC  " + line);
        if (line.Equals("BTN=APPROVE", StringComparison.OrdinalIgnoreCase))
        {
            _state.Decide("allow", "PA0");
        }
        else if (line.Equals("BTN=DENY", StringComparison.OrdinalIgnoreCase))
        {
            _state.Decide("deny", "PA4");
        }
    }

    private void SendBoardLine(string line)
    {
        try
        {
            if (!_serial.IsOpen)
            {
                AddSerialLog("Skipped, serial offline: " + line);
                return;
            }
            _serial.WriteLine(line);
            AddSerialLog("PC -> Board  " + line);
        }
        catch (Exception ex)
        {
            AddSerialLog("Serial write failed: " + ex.Message);
        }
    }

    private void InstallHooks()
    {
        try
        {
            HookSettingsInstaller.Install(_state);
            MessageBox.Show("Hooks installed. Restart Claude Code to load them.", "Claude hooks", MessageBoxButtons.OK, MessageBoxIcon.Information);
            RefreshUi();
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Hook install failed", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void OpenSettings()
    {
        System.Diagnostics.Process.Start("explorer.exe", "/select,\"" + HookSettingsInstaller.SettingsPath + "\"");
    }

    private void RefreshPorts()
    {
        var box = _portBox;
        if (box is null)
        {
            return;
        }
        var selected = box.SelectedItem?.ToString();
        box.Items.Clear();
        foreach (var port in SerialPort.GetPortNames().OrderBy(x => x))
        {
            box.Items.Add(port);
        }
        if (selected is not null && box.Items.Contains(selected))
        {
            box.SelectedItem = selected;
        }
        else if (box.Items.Count > 0)
        {
            box.SelectedIndex = 0;
        }
    }

    private void SafeRefresh()
    {
        if (IsHandleCreated)
        {
            BeginInvoke(RefreshUi);
        }
    }

    private void RefreshUi()
    {
        _status.Text = _state.StatusText;
        _status.ForeColor = _state.Mode switch
        {
            AiMode.Working => _amber,
            AiMode.WaitingApproval => _amber,
            AiMode.Approved => _green,
            AiMode.Denied => _red,
            AiMode.Error => _red,
            _ => _green
        };

        SetDot(_hookDot, _state.HookServerOnline, "Hook server");
        SetDot(_serialDot, _serial.IsOpen, _serial.IsOpen ? $"Board serial  {_serial.PortName}" : "Board serial");
        SetDot(_hooksDot, _state.HooksInstalled || HookSettingsInstaller.AreHooksInstalled(), "Claude hooks");

        _events.BeginUpdate();
        _events.Items.Clear();
        foreach (var item in _state.Events.Reverse().Take(120).Reverse())
        {
            _events.Items.Add($"[{item.Time:HH:mm:ss}] {item.Source,-7} {item.Title}");
        }
        _events.EndUpdate();
        if (_events.Items.Count > 0)
        {
            _events.TopIndex = _events.Items.Count - 1;
        }

        if (_state.PendingApproval is { } req)
        {
            _approvalTitle.Text = $"#{req.Id} {req.Tool}";
            _approvalBody.Text = req.Summary;
        }
        else
        {
            _approvalTitle.Text = "No pending request";
            _approvalBody.Text = "PA0 approve / PA4 deny. GUI buttons are available for testing.";
        }
    }

    private void AddSerialLog(string text)
    {
        if (!IsHandleCreated)
        {
            return;
        }
        if (InvokeRequired)
        {
            BeginInvoke(() => AddSerialLog(text));
            return;
        }
        _serialLog.AppendText($"[{DateTime.Now:HH:mm:ss}] {text}\r\n");
    }

    private Panel Panel(Color color) => new()
    {
        Dock = DockStyle.Fill,
        BackColor = color
    };

    private Label Label(string text, int size, FontStyle style, Color color) => new()
    {
        Text = text,
        ForeColor = color,
        BackColor = Color.Transparent,
        Font = new Font("Segoe UI", size, style)
    };

    private void AddSectionTitle(Control parent, string text, int y)
    {
        var label = Label(text, 12, FontStyle.Bold, _text);
        label.Location = new Point(18, y);
        label.AutoSize = true;
        parent.Controls.Add(label);
    }

    private void AddButton(Control parent, string text, int x, int y, int w, int h, Action action, Color bg, Color fg)
    {
        var btn = new Button
        {
            Text = text,
            Location = new Point(x, y),
            Size = new Size(w, h),
            FlatStyle = FlatStyle.Flat,
            BackColor = bg,
            ForeColor = fg,
            Font = new Font("Segoe UI", 10, FontStyle.Bold)
        };
        btn.FlatAppearance.BorderSize = 0;
        btn.Click += (_, _) => action();
        parent.Controls.Add(btn);
    }

    private Label Dot(string label, int x, int y)
    {
        var dot = Label("●  " + label, 10, FontStyle.Bold, _muted);
        dot.Location = new Point(x, y);
        dot.AutoSize = true;
        return dot;
    }

    private void SetDot(Label dot, bool ok, string label)
    {
        dot.Text = "●  " + label;
        dot.ForeColor = ok ? _green : _muted;
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        _server.Dispose();
        if (_serial.IsOpen)
        {
            _serial.Close();
        }
        base.OnFormClosing(e);
    }
}
