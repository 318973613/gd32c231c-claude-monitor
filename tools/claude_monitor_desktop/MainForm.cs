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
    private Label _statusText = null!;
    private Label _statusHint = null!;
    private Label _hookMetric = null!;
    private Label _serialMetric = null!;
    private Label _hooksMetric = null!;
    private Panel _attentionPanel = null!;
    private Label _attentionTitle = null!;
    private Label _attentionBody = null!;
    private Panel _approvalPanel = null!;
    private Label _approvalTitle = null!;
    private Label _approvalBody = null!;
    private CheckBox _boardApprovalBox = null!;
    private ListView _events = null!;
    private TextBox _serialLog = null!;

    private readonly Color _bg = Color.FromArgb(246, 247, 242);
    private readonly Color _card = Color.FromArgb(255, 252, 246);
    private readonly Color _cardAlt = Color.FromArgb(245, 240, 232);
    private readonly Color _line = Color.FromArgb(224, 216, 203);
    private readonly Color _ink = Color.FromArgb(34, 34, 31);
    private readonly Color _muted = Color.FromArgb(105, 97, 87);
    private readonly Color _dark = Color.FromArgb(39, 37, 33);
    private readonly Color _orange = Color.FromArgb(203, 101, 45);
    private readonly Color _yellow = Color.FromArgb(255, 196, 58);
    private readonly Color _yellowPale = Color.FromArgb(255, 243, 194);
    private readonly Color _green = Color.FromArgb(32, 133, 83);
    private readonly Color _red = Color.FromArgb(173, 56, 62);
    private readonly Color _redPale = Color.FromArgb(255, 230, 226);
    private readonly Color _bluePale = Color.FromArgb(229, 238, 252);
    private readonly Color _blue = Color.FromArgb(56, 94, 145);
    private readonly Font _font = new("Microsoft YaHei UI", 10F, FontStyle.Regular);
    private readonly Font _mono = new("Cascadia Mono", 9.5F, FontStyle.Regular);

    public MainForm()
    {
        Text = "Claude 开发板监控";
        var workingArea = Screen.PrimaryScreen?.WorkingArea ?? new Rectangle(0, 0, 1280, 800);
        Width = Math.Min(1120, Math.Max(1040, workingArea.Width - 80));
        Height = Math.Min(760, Math.Max(660, workingArea.Height - 80));
        MinimumSize = new Size(Math.Min(1040, workingArea.Width - 80), Math.Min(660, workingArea.Height - 80));
        BackColor = _bg;
        Font = _font;
        StartPosition = FormStartPosition.Manual;
        Location = new Point(workingArea.Left + 40, workingArea.Top + 40);

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
            _state.AddEvent("error", "Hook 服务启动失败", ex.Message, "Monitor");
        }

        RefreshUi();
    }

    private void BuildUi()
    {
        var root = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            BackColor = _bg,
            Padding = new Padding(16),
            RowCount = 4,
            ColumnCount = 1
        };
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 78));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 58));
        root.RowStyles.Add(new RowStyle(SizeType.Absolute, 82));
        root.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        Controls.Add(root);

        root.Controls.Add(BuildHeader(), 0, 0);
        root.Controls.Add(BuildAttentionBanner(), 0, 1);
        root.Controls.Add(BuildMetrics(), 0, 2);
        root.Controls.Add(BuildBody(), 0, 3);
    }

    private Control BuildHeader()
    {
        var header = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            BackColor = _bg,
            ColumnCount = 2,
            RowCount = 1
        };
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        header.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 360));

        var left = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            BackColor = _bg,
            RowCount = 2,
            ColumnCount = 1,
            Padding = new Padding(4, 4, 0, 0)
        };
        left.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
        left.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));

        var title = MakeLabel("Claude 开发板监控台", 20, FontStyle.Bold, _ink);
        title.Dock = DockStyle.Fill;
        title.AutoSize = false;
        title.AutoEllipsis = true;
        left.Controls.Add(title, 0, 0);

        var subtitle = MakeLabel("Claude Code Hooks / 串口链路 / GD32C231C 状态监控", 10, FontStyle.Regular, _muted);
        subtitle.Dock = DockStyle.Fill;
        subtitle.AutoSize = false;
        subtitle.AutoEllipsis = true;
        left.Controls.Add(subtitle, 0, 1);

        var right = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            BackColor = _bg,
            RowCount = 2,
            ColumnCount = 1,
            Padding = new Padding(0, 8, 4, 0)
        };
        right.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
        right.RowStyles.Add(new RowStyle(SizeType.Absolute, 24));

        _statusText = MakeLabel("空闲", 18, FontStyle.Bold, _green);
        _statusText.Dock = DockStyle.Fill;
        _statusText.AutoSize = false;
        _statusText.TextAlign = ContentAlignment.MiddleRight;
        right.Controls.Add(_statusText, 0, 0);

        _statusHint = MakeLabel("等待 Claude 事件", 10, FontStyle.Regular, _muted);
        _statusHint.Dock = DockStyle.Fill;
        _statusHint.AutoSize = false;
        _statusHint.TextAlign = ContentAlignment.MiddleRight;
        _statusHint.AutoEllipsis = true;
        _statusHint.Visible = false;
        right.Controls.Add(_statusHint, 0, 1);

        header.Controls.Add(left, 0, 0);
        header.Controls.Add(right, 1, 0);

        return header;
    }

    private Control BuildAttentionBanner()
    {
        _attentionPanel = new Panel
        {
            Dock = DockStyle.Fill,
            BackColor = _green,
            Padding = new Padding(18, 8, 18, 8)
        };

        _attentionTitle = MakeLabel("外部审批模式已开启", 13, FontStyle.Bold, Color.White);
        _attentionTitle.Location = new Point(18, 7);
        _attentionTitle.Size = new Size(420, 22);
        _attentionTitle.AutoSize = false;
        _attentionTitle.AutoEllipsis = true;
        _attentionPanel.Controls.Add(_attentionTitle);

        _attentionBody = MakeLabel("有权限请求时会停在这里，按 PA0 同意 / PA4 拒绝，也可以点击右侧按钮。", 10, FontStyle.Regular, Color.White);
        _attentionBody.Location = new Point(18, 31);
        _attentionBody.Size = new Size(980, 20);
        _attentionBody.Anchor = AnchorStyles.Left | AnchorStyles.Top | AnchorStyles.Right;
        _attentionBody.AutoSize = false;
        _attentionBody.AutoEllipsis = true;
        _attentionPanel.Controls.Add(_attentionBody);

        return _attentionPanel;
    }

    private Control BuildMetrics()
    {
        var grid = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            BackColor = _bg,
            ColumnCount = 3,
            RowCount = 1,
            Padding = new Padding(0, 0, 0, 12)
        };
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.33F));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.33F));
        grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 33.34F));

        _hookMetric = AddMetricCard(grid, 0, "Hook 服务", "本机 127.0.0.1:8765");
        _serialMetric = AddMetricCard(grid, 1, "开发板串口", "未连接");
        _hooksMetric = AddMetricCard(grid, 2, "Claude 接入", "未安装");
        return grid;
    }

    private Label AddMetricCard(TableLayoutPanel parent, int column, string title, string detail)
    {
        var card = MakeCard();
        card.Margin = column == 1 ? new Padding(8, 0, 8, 0) : new Padding(0, 0, 0, 0);

        var label = MakeLabel(title, 11, FontStyle.Bold, _ink);
        label.Location = new Point(18, 14);
        label.AutoSize = true;
        card.Controls.Add(label);

        var value = MakeLabel(detail, 10, FontStyle.Regular, _muted);
        value.Location = new Point(18, 42);
        value.Size = new Size(330, 24);
        card.Controls.Add(value);

        parent.Controls.Add(card, column, 0);
        return value;
    }

    private Control BuildBody()
    {
        var body = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            BackColor = _bg,
            ColumnCount = 3,
            RowCount = 1
        };
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 320));
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
        body.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 390));

        body.Controls.Add(BuildLeftPanel(), 0, 0);
        body.Controls.Add(BuildEventPanel(), 1, 0);
        body.Controls.Add(BuildRightPanel(), 2, 0);
        return body;
    }

    private Control BuildLeftPanel()
    {
        var panel = MakeCard();
        panel.Margin = new Padding(0, 0, 12, 0);
        panel.Padding = new Padding(18);
        panel.AutoScroll = true;

        AddSection(panel, "设备连接", "连接 GD32C231C USART0", 18);
        _portBox = new ComboBox
        {
            Location = new Point(20, 78),
            Size = new Size(164, 34),
            DropDownStyle = ComboBoxStyle.DropDownList,
            Font = _font
        };
        panel.Controls.Add(_portBox);
        AddButton(panel, "刷新", 194, 77, 76, 34, RefreshPorts, _cardAlt, _ink);
        AddButton(panel, "连接", 20, 124, 124, 42, ConnectSerial, _green, Color.White);
        AddButton(panel, "断开", 154, 124, 116, 42, DisconnectSerial, _dark, Color.White);
        AddButton(panel, "同步电脑时间", 20, 174, 250, 36, SyncBoardTime, _cardAlt, _ink);

        AddSection(panel, "Claude 接入", "Hooks + statusLine 配置", 226);
        AddButton(panel, "安装 / 更新 Hooks", 20, 286, 250, 44, InstallHooks, _blue, Color.White);
        AddButton(panel, "打开 settings.json", 20, 340, 250, 38, OpenSettings, _cardAlt, _ink);
        _boardApprovalBox = new CheckBox
        {
            Text = "外部审批模式",
            Location = new Point(20, 392),
            Size = new Size(250, 30),
            BackColor = _card,
            ForeColor = _ink,
            Font = new Font("Microsoft YaHei UI", 10, FontStyle.Bold),
            Checked = _state.BoardApprovalEnabled
        };
        _boardApprovalBox.CheckedChanged += (_, _) =>
        {
            _state.BoardApprovalEnabled = _boardApprovalBox.Checked;
            _state.AddEvent("system", _boardApprovalBox.Checked ? "外部审批已启用" : "已切回 Claude 原生确认", "", "Monitor");
            RefreshUi();
        };
        panel.Controls.Add(_boardApprovalBox);

        AddSection(panel, "快速测试", "直接测试开发板屏幕状态", 452);
        AddButton(panel, "工作中", 20, 512, 124, 38, () => _state.SetMode(AiMode.Working, "WORKING", "AI=WORKING"), _orange, Color.White);
        AddButton(panel, "空闲", 154, 512, 116, 38, () => _state.SetMode(AiMode.Idle, "IDLE", "AI=IDLE"), _cardAlt, _ink);
        return panel;
    }

    private Control BuildEventPanel()
    {
        var panel = MakeCard();
        panel.Margin = new Padding(0, 0, 12, 0);
        panel.Padding = new Padding(18);

        AddSection(panel, "实时事件", "Claude 工具调用、权限请求、停止事件", 18);
        _events = new ListView
        {
            Location = new Point(20, 76),
            Size = new Size(420, 500),
            Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            View = View.Details,
            FullRowSelect = true,
            GridLines = false,
            BorderStyle = BorderStyle.None,
            BackColor = Color.FromArgb(255, 250, 241),
            ForeColor = _ink,
            Font = _mono
        };
        _events.Columns.Add("时间", 76);
        _events.Columns.Add("来源", 84);
        _events.Columns.Add("事件", 260);
        panel.Controls.Add(_events);
        return panel;
    }

    private Control BuildRightPanel()
    {
        var right = new TableLayoutPanel
        {
            Dock = DockStyle.Fill,
            BackColor = _bg,
            RowCount = 2,
            ColumnCount = 1
        };
        right.RowStyles.Add(new RowStyle(SizeType.Absolute, 260));
        right.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
        right.Controls.Add(BuildApprovalPanel(), 0, 0);
        right.Controls.Add(BuildSerialPanel(), 0, 1);
        return right;
    }

    private Control BuildApprovalPanel()
    {
        var panel = MakeCard();
        _approvalPanel = panel;
        panel.Margin = new Padding(0, 0, 0, 12);
        panel.Padding = new Padding(18);
        AddSection(panel, "权限请求", "外部审批时可用 PA0 / PA4 或按钮控制", 18);

        _approvalTitle = MakeLabel("暂无待审批请求", 12, FontStyle.Bold, _ink);
        _approvalTitle.Location = new Point(20, 78);
        _approvalTitle.Size = new Size(340, 30);
        _approvalTitle.AutoSize = false;
        _approvalTitle.AutoEllipsis = true;
        panel.Controls.Add(_approvalTitle);

        _approvalBody = MakeLabel("这里显示 Claude 权限请求。", 10, FontStyle.Regular, _muted);
        _approvalBody.Location = new Point(20, 116);
        _approvalBody.Size = new Size(340, 68);
        _approvalBody.AutoSize = false;
        panel.Controls.Add(_approvalBody);

        AddButton(panel, "拒绝", 20, 202, 164, 42, () => DecideFromGui("deny"), _red, Color.White);
        AddButton(panel, "同意", 196, 202, 164, 42, () => DecideFromGui("allow"), _green, Color.White);
        return panel;
    }

    private Control BuildSerialPanel()
    {
        var panel = MakeCard();
        panel.Padding = new Padding(18);
        AddSection(panel, "串口日志", "PC 与开发板的 ASCII 行协议", 18);

        _serialLog = new TextBox
        {
            Location = new Point(20, 76),
            Size = new Size(340, 300),
            Anchor = AnchorStyles.Top | AnchorStyles.Bottom | AnchorStyles.Left | AnchorStyles.Right,
            Multiline = true,
            ReadOnly = true,
            ScrollBars = ScrollBars.Vertical,
            BorderStyle = BorderStyle.None,
            BackColor = Color.FromArgb(38, 36, 32),
            ForeColor = Color.FromArgb(244, 237, 225),
            Font = _mono
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
            _state.AddEvent("system", $"串口已连接 {_serial.PortName}", "115200 8N1", "Monitor");
            SendBoardLine("AI=IDLE");
            SyncBoardTime();
            _state.EmitRuntimeStatusToBoard();
            RefreshUi();
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "串口连接失败", MessageBoxButtons.OK, MessageBoxIcon.Error);
        }
    }

    private void DisconnectSerial()
    {
        if (_serial.IsOpen)
        {
            _serial.Close();
        }
        _state.SerialConnected = false;
        _state.AddEvent("system", "串口已断开", "", "Monitor");
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
        AddSerialLog("开发板 -> PC  " + line);
        if (line.Equals("BTN=APPROVE", StringComparison.OrdinalIgnoreCase))
        {
            if (!_state.Decide("allow", "PA0"))
            {
                AddSerialLog("当前没有待审批请求，PA0 不会影响终端原生提示。");
            }
        }
        else if (line.Equals("BTN=DENY", StringComparison.OrdinalIgnoreCase))
        {
            if (!_state.Decide("deny", "PA4"))
            {
                AddSerialLog("当前没有待审批请求，PA4 不会影响终端原生提示。");
            }
        }
    }

    private void DecideFromGui(string decision)
    {
        if (_state.Decide(decision, "GUI"))
        {
            return;
        }

        MessageBox.Show(
            "当前没有上位机接管的待审批请求。\r\n\r\n如果终端里已经出现 Claude 原生确认提示，只能在终端里按 1/2/3。\r\n要用 PA0/PA4 或这里的按钮控制，请保持“外部审批模式”开启，并重新触发一次权限请求。",
            "没有待审批请求",
            MessageBoxButtons.OK,
            MessageBoxIcon.Information);
    }

    private void SendBoardLine(string line)
    {
        try
        {
            if (!_serial.IsOpen)
            {
                AddSerialLog("串口未连接，跳过：" + line);
                return;
            }
            _serial.WriteLine(line);
            AddSerialLog("PC -> 开发板  " + line);
        }
        catch (Exception ex)
        {
            AddSerialLog("串口写入失败：" + ex.Message);
        }
    }

    private void SyncBoardTime()
    {
        var line = $"TIME={DateTime.Now:HH:mm:ss}";
        SendBoardLine(line);
        if (_serial.IsOpen)
        {
            _state.AddEvent("system", "已同步电脑时间", line, "Monitor");
        }
    }

    private void InstallHooks()
    {
        try
        {
            HookSettingsInstaller.Install(_state);
            MessageBox.Show("Claude Hooks 已安装。请重启 Claude Code 后生效。", "Claude 接入", MessageBoxButtons.OK, MessageBoxIcon.Information);
            RefreshUi();
        }
        catch (Exception ex)
        {
            MessageBox.Show(ex.Message, "Hooks 安装失败", MessageBoxButtons.OK, MessageBoxIcon.Error);
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
        var chineseStatus = _state.Mode switch
        {
            AiMode.Working => "工作中",
            AiMode.NativeApproval => "终端确认",
            AiMode.WaitingApproval => "等待审批",
            AiMode.Approved => "已同意",
            AiMode.Denied => "已拒绝",
            AiMode.Error => "异常",
            _ => "空闲"
        };
        _statusText.Text = chineseStatus;
        _statusText.ForeColor = _state.Mode switch
        {
            AiMode.WaitingApproval => _red,
            AiMode.NativeApproval => _blue,
            AiMode.Working => _orange,
            AiMode.Approved => _green,
            AiMode.Denied or AiMode.Error => _red,
            _ => _green
        };
        _statusHint.Visible = true;
        _statusHint.Text = _state.Mode switch
        {
            AiMode.WaitingApproval => "按 PA0 同意 / PA4 拒绝",
            AiMode.NativeApproval => "请在 Claude 终端按 1/2/3",
            _ => "等待 Claude 事件"
        };

        Text = _state.Mode switch
        {
            AiMode.WaitingApproval => "Claude 开发板监控 [等待审批]",
            AiMode.NativeApproval => "Claude 开发板监控 [终端确认]",
            _ => "Claude 开发板监控"
        };

        _hookMetric.Text = _state.HookServerOnline ? "已启动 · 127.0.0.1:8765" : "未启动";
        _hookMetric.ForeColor = _state.HookServerOnline ? _green : _red;
        _serialMetric.Text = _serial.IsOpen ? $"已连接 · {_serial.PortName}" : "未连接";
        _serialMetric.ForeColor = _serial.IsOpen ? _green : _muted;
        _hooksMetric.Text = (_state.HooksInstalled || HookSettingsInstaller.AreHooksInstalled())
            ? RuntimeSummary()
            : "未安装";
        _hooksMetric.ForeColor = (_state.HooksInstalled || HookSettingsInstaller.AreHooksInstalled()) ? _green : _muted;

        _events.BeginUpdate();
        _events.Items.Clear();
        foreach (var item in _state.Events.Reverse().Take(160).Reverse())
        {
            var row = new ListViewItem(item.Time.ToString("HH:mm:ss"));
            row.SubItems.Add(item.Source);
            row.SubItems.Add(item.Title);
            _events.Items.Add(row);
        }
        _events.EndUpdate();
        if (_events.Items.Count > 0)
        {
            _events.EnsureVisible(_events.Items.Count - 1);
        }

        if (_state.PendingApproval is { } req)
        {
            _attentionPanel.BackColor = _red;
            _attentionTitle.Text = "等待审批：Claude 已暂停";
            _attentionBody.Text = "现在需要你决定：按 PA0 同意 / PA4 拒绝，或点击右侧“同意 / 拒绝”。";
            _approvalPanel.BackColor = _redPale;
            _approvalTitle.Text = $"等待审批 #{req.Id} · {req.Tool}";
            _approvalTitle.ForeColor = _red;
            _approvalTitle.Font = new Font("Microsoft YaHei UI", 15, FontStyle.Bold);
            _approvalBody.ForeColor = _ink;
            _approvalBody.Text = $"Claude 正在等待这个决定。\r\n{req.Summary}";
        }
        else
        {
            _approvalTitle.Font = new Font("Microsoft YaHei UI", 12, FontStyle.Bold);
            if (_state.Mode == AiMode.NativeApproval)
            {
                _attentionPanel.BackColor = _blue;
                _attentionTitle.Text = "Claude 原生终端确认";
                _attentionBody.Text = "当前请求不由板子接管：请到 Claude 终端按 1/2/3。PA0 / PA4 / GUI 按钮不会控制这次提示。";
                _approvalPanel.BackColor = _bluePale;
                _approvalTitle.Text = "请看 Claude 终端";
                _approvalTitle.ForeColor = _blue;
                _approvalBody.ForeColor = _ink;
                _approvalBody.Text = "这是 Claude 原生确认模式。已有终端提示不能被上位机接管，只能在终端输入 1/2/3。";
            }
            else
            {
                _approvalPanel.BackColor = _card;
                _approvalTitle.Text = "暂无待审批请求";
                _approvalTitle.ForeColor = _ink;
                _approvalBody.ForeColor = _muted;
                if (_state.BoardApprovalEnabled)
                {
                    _attentionPanel.BackColor = _green;
                    _attentionTitle.Text = "外部审批模式已开启";
                    _attentionBody.Text = "有权限请求时会停在这里，按 PA0 同意 / PA4 拒绝，也可以点击右侧按钮。";
                    _approvalBody.Text = "下一次权限请求会等待 PA0、PA4 或这里的按钮。终端不会弹出原生确认。";
                }
                else
                {
                    _attentionPanel.BackColor = _blue;
                    _attentionTitle.Text = "Claude 原生确认模式";
                    _attentionBody.Text = "如果出现权限请求，请在 Claude 终端按 1/2/3；PA0 / PA4 只会记录按键，不会审批。";
                    _approvalBody.Text = "终端提示只能在终端按 1/2/3。要让 PA0/PA4 控制审批，请打开左侧“外部审批模式”。";
                }
            }
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

    private string RuntimeSummary()
    {
        var context = _state.ContextUsedPercent >= 0 ? $"CTX {_state.ContextUsedPercent}%" : "CTX --";
        return $"{ShortText(_state.ModelName, 16)} · {context}";
    }

    private static string ShortText(string text, int maxLength)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return "UNKNOWN";
        }
        return text.Length <= maxLength ? text : text[..maxLength];
    }

    private Panel MakeCard() => new()
    {
        Dock = DockStyle.Fill,
        BackColor = _card
    };

    private Label MakeLabel(string text, float size, FontStyle style, Color color) => new()
    {
        Text = text,
        ForeColor = color,
        BackColor = Color.Transparent,
        Font = new Font("Microsoft YaHei UI", size, style)
    };

    private void AddSection(Control parent, string title, string subtitle, int y)
    {
        var titleLabel = MakeLabel(title, 12, FontStyle.Bold, _ink);
        titleLabel.Location = new Point(20, y);
        titleLabel.AutoSize = true;
        parent.Controls.Add(titleLabel);

        var subtitleLabel = MakeLabel(subtitle, 9, FontStyle.Regular, _muted);
        subtitleLabel.Location = new Point(22, y + 32);
        subtitleLabel.AutoSize = false;
        subtitleLabel.AutoEllipsis = true;
        subtitleLabel.Size = new Size(240, 22);
        parent.Controls.Add(subtitleLabel);
    }

    private void AddButton(Control parent, string text, int x, int y, int w, int h, Action action, Color bg, Color fg)
    {
        var button = new Button
        {
            Text = text,
            Location = new Point(x, y),
            Size = new Size(w, h),
            FlatStyle = FlatStyle.Flat,
            BackColor = bg,
            ForeColor = fg,
            Font = new Font("Microsoft YaHei UI", 10, FontStyle.Bold),
            Cursor = Cursors.Hand
        };
        button.FlatAppearance.BorderSize = 0;
        button.Click += (_, _) => action();
        parent.Controls.Add(button);
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
