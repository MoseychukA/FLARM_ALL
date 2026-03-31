using System;
using System.Collections.Generic;
using System.Drawing;
using System.Linq;
using System.Media;
using System.Windows.Forms;
using FlightRadarPc.Models;
using FlightRadarPc.Services;

namespace FlightRadarPc.UI
{
    public class MainForm : Form
    {
        private AppSettings _settings;
        private readonly DataSourceManager _dataSource = new DataSourceManager();
        private readonly TargetRepository _repository = new TargetRepository();
        private readonly TargetFilterService _filterService = new TargetFilterService();
        private readonly TrackHistoryService _trackService = new TrackHistoryService();
        private readonly ProximityAlertService _alertService = new ProximityAlertService();
        private readonly FullScreenController _fullScreen = new FullScreenController();

        private readonly RadarControl _radar = new RadarControl();
        private readonly Panel _radarHost = new Panel();
        private readonly Panel _radarContainer = new Panel();
        private readonly DataGridView _targetGrid = new DataGridView();
        private readonly RichTextBox _decodedList = new RichTextBox();
        private readonly RichTextBox _packetList = new RichTextBox();
        private readonly RichTextBox _monitorList = new RichTextBox();
        private readonly List<PacketViewItem> _visiblePacketItems = new List<PacketViewItem>();
        private readonly List<DecodedViewItem> _visibleDecodedItems = new List<DecodedViewItem>();
        private int _selectedDecodedLineIndex = -1;
        private int _selectedPacketLineIndex = -1;
        private readonly RichTextBox _packetDetails = new RichTextBox();
        private readonly StatusStrip _status = new StatusStrip();
        private readonly ToolStripStatusLabel _statusLabel = new ToolStripStatusLabel("Готово");
        private readonly ToolStripStatusLabel _zoomLabel = new ToolStripStatusLabel();
        private readonly ToolStripStatusLabel _clockLabel = new ToolStripStatusLabel();
        private readonly Timer _uiTimer = new Timer();
        private readonly Timer _alarmTimer = new Timer();
        private bool _testFlightActive;
        private int _testFlightStep;
        private bool _refreshPending = true;
        private readonly ToolStripMenuItem _fullScreenMenuItem = new ToolStripMenuItem("Полный экран (F11)");
        private readonly ToolStripMenuItem _mapMenuItem = new ToolStripMenuItem("Карта-подложка");
        private readonly ToolStripMenuItem _tracksMenuItem = new ToolStripMenuItem("Треки целей");
        private readonly ToolStripMenuItem _ringsMenuItem = new ToolStripMenuItem("Кольца дальности");
        private readonly ToolStripMenuItem _labelsMenuItem = new ToolStripMenuItem("Подписи целей");
        private readonly ToolStripMenuItem _northUpMenuItem = new ToolStripMenuItem("North Up");
        private readonly ToolStripMenuItem _headingUpMenuItem = new ToolStripMenuItem("Heading Up");
        private readonly CheckedListBox _aircraftTypeFilter = new CheckedListBox();
        private readonly CheckedListBox _sourceFilter = new CheckedListBox();
        private readonly NumericUpDown _maxDistanceFilter = new NumericUpDown();
        private readonly NumericUpDown _minAltitudeFilter = new NumericUpDown();
        private readonly NumericUpDown _maxAltitudeFilter = new NumericUpDown();
        private readonly CheckBox _chkOnlyAlerts = new CheckBox { Text = "Только тревоги", AutoSize = true };
        private readonly CheckBox _chkOnlyMoving = new CheckBox { Text = "Только движущиеся", AutoSize = true };
        private readonly Label _summaryLabel = new Label { Dock = DockStyle.Top, Height = 44, Padding = new Padding(6), TextAlign = ContentAlignment.MiddleLeft };
        private readonly SplitContainer _mainSplit = new SplitContainer();
        private readonly Panel _topToolbar = new Panel();
        private readonly Panel _topDockContainer = new Panel();
        private readonly TabControl _rightTabs = new TabControl();
        private readonly TextBox _targetDetails = new TextBox();
        private readonly List<PacketViewItem> _packetHistory = new List<PacketViewItem>();
        private readonly List<DecodedViewItem> _decodedHistory = new List<DecodedViewItem>();
        private readonly HashSet<string> _mutedOutputSources = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        private uint? _selectedTargetAddress;
        private bool _suppressGridSelectionChanged;
        private CheckBox _outputComCheckBox;
        private CheckBox _outputTcpCheckBox;
        private CheckBox _outputUdpCheckBox;
        private CheckBox _outputTestCheckBox;
        private Button _testFlightButton;
        private MenuStrip _menuStrip;
        private string _selectedTargetFallbackDetails;
        private uint? _selectedTargetFallbackAddress;
        private DateTime _selectedTargetFallbackExpiresUtc = DateTime.MinValue;

        private const int MaxPacketHistory = 200;
        private readonly float _startupRadarRangeKm;

        public MainForm()
        {
            Text = "Отображение движения самолетов";
            Width = 1500;
            Height = 900;
            StartPosition = FormStartPosition.CenterScreen;
            KeyPreview = true;
            TryApplyAppIcon();

            _settings = AppSettings.Load();
            if (_settings.MapProviderMode == MapProviderMode.None)
                _settings.MapProviderMode = MapProviderMode.OpenStreetMap;
            NormalizeSettings();
            SaveSettingsQuietly();
            _startupRadarRangeKm = _settings.RadarRangeKm;

            _radar.Dock = DockStyle.Fill;
            _radar.BackColor = Color.Black;
            ConfigureTargetGrid();
            ConfigureCopyContextMenu(_decodedList);
            ConfigureCopyContextMenu(_packetList);
            ConfigureCopyContextMenu(_monitorList);

            BuildMenu();
            BuildLayout();
            BuildStatusBar();
            PopulateFilterLists();
            ApplySettingsToUi();

            _dataSource.LineReceived += line => HandleLineReceived("ACTIVE", line);
            _dataSource.PacketReceived += (source, line) => RegisterPacket(source, line);
            _dataSource.MonitorMessage += AppendMonitorMessage;
            _dataSource.StatusChanged += msg => _statusLabel.Text = msg;
            _radar.ZoomChanged += zoom =>
            {
                _settings.RadarRangeKm = zoom;
                _zoomLabel.Text = $"Масштаб: {zoom:0.0} км";
                RefreshRadarAndList();
            };
            _radar.TargetClicked += target => SelectTarget(target != null ? (uint?)target.Address : null);

            _uiTimer.Interval = 250;
            _uiTimer.Tick += (s, e) => UpdateUiState();
            _uiTimer.Start();

            _alarmTimer.Interval = 1500;
            _alarmTimer.Tick += (s, e) => ProcessAlerts();
            _alarmTimer.Start();

            if (_settings.FullScreenAtStartup)
                _fullScreen.Enter(this);

            Resize += (s, e) => UpdateRadarLayout();
            Shown += (s, e) =>
            {
                AdjustSplitDistance();
                UpdateRadarLayout();
                StartSource();
            };
        }
        private void NormalizeSettings()
        {
            _settings?.Normalize();
        }

        private void BuildMenu()
        {
            _menuStrip = new MenuStrip();
            var menu = _menuStrip;

            var fileMenu = new ToolStripMenuItem("Файл");
            fileMenu.DropDownItems.Add("Настройки", null, (s, e) => ShowSettings());
            fileMenu.DropDownItems.Add("Старт", null, (s, e) => StartSource());
            fileMenu.DropDownItems.Add("Стоп", null, (s, e) => _dataSource.Stop());
            fileMenu.DropDownItems.Add("Сброс масштаба", null, (s, e) => ResetZoom());
            fileMenu.DropDownItems.Add(new ToolStripSeparator());
            fileMenu.DropDownItems.Add("Показать тестовый самолет", null, (s, e) => ShowTestAircraft());
            fileMenu.DropDownItems.Add("Движение тестового самолета", null, (s, e) => StartMovingTestAircraft());
            fileMenu.DropDownItems.Add("Добавить тестовый пакет", null, (s, e) => InjectTestPacket());
            fileMenu.DropDownItems.Add("Очистить список пакетов", null, (s, e) => ClearPacketHistory());
            fileMenu.DropDownItems.Add("Вернуться к стартовому экрану", null, (s, e) => ReturnToStartScreen());
            fileMenu.DropDownItems.Add(new ToolStripSeparator());
            fileMenu.DropDownItems.Add("Тест FLYRF", null, (s, e) => InjectTestPacket(TestPacketFactory.CreateFlyRf()));
            fileMenu.DropDownItems.Add("Тест NMEA PFLAA", null, (s, e) => InjectTestPacket(TestPacketFactory.CreatePflAa()));
            fileMenu.DropDownItems.Add("Тест SBS-1", null, (s, e) => InjectTestPacket(TestPacketFactory.CreateSbs1()));
            fileMenu.DropDownItems.Add("Тест GDL90", null, (s, e) => InjectTestPacket(TestPacketFactory.CreateGdl90Hex()));
            fileMenu.DropDownItems.Add(new ToolStripSeparator());
            fileMenu.DropDownItems.Add("Выход", null, (s, e) => Close());

            var viewMenu = new ToolStripMenuItem("Вид");
            _ringsMenuItem.CheckOnClick = true;
            _ringsMenuItem.Click += (s, e) => { _settings.ShowRangeRings = _ringsMenuItem.Checked; ApplySettingsToUi(); SaveSettingsQuietly(); };
            _tracksMenuItem.CheckOnClick = true;
            _tracksMenuItem.Click += (s, e) => { _settings.ShowTracks = _tracksMenuItem.Checked; ApplySettingsToUi(); SaveSettingsQuietly(); };
            _mapMenuItem.CheckOnClick = true;
            _mapMenuItem.Click += (s, e) => { _settings.ShowMapBackground = _mapMenuItem.Checked; if (_settings.ShowMapBackground && _settings.MapProviderMode == MapProviderMode.None) _settings.MapProviderMode = MapProviderMode.OpenStreetMap; ApplySettingsToUi(); SaveSettingsQuietly(); };
            _labelsMenuItem.CheckOnClick = true;
            _labelsMenuItem.Click += (s, e) => { _settings.ShowTargetLabels = _labelsMenuItem.Checked; ApplySettingsToUi(); SaveSettingsQuietly(); };
            _northUpMenuItem.Click += (s, e) => { _settings.OrientationMode = RadarOrientationMode.NorthUp; ApplySettingsToUi(); SaveSettingsQuietly(); };
            _headingUpMenuItem.Click += (s, e) => { _settings.OrientationMode = RadarOrientationMode.HeadingUp; ApplySettingsToUi(); SaveSettingsQuietly(); };
            _fullScreenMenuItem.Click += (s, e) => ToggleFullScreen();
            viewMenu.DropDownItems.Add(_ringsMenuItem);
            viewMenu.DropDownItems.Add(_tracksMenuItem);
            viewMenu.DropDownItems.Add(_mapMenuItem);
            viewMenu.DropDownItems.Add(_labelsMenuItem);
            viewMenu.DropDownItems.Add(new ToolStripSeparator());
            viewMenu.DropDownItems.Add(_northUpMenuItem);
            viewMenu.DropDownItems.Add(_headingUpMenuItem);
            viewMenu.DropDownItems.Add(_fullScreenMenuItem);

            var filterMenu = new ToolStripMenuItem("Фильтр");
            filterMenu.DropDownItems.Add("Сбросить фильтры", null, (s, e) => ClearFilters());
            filterMenu.DropDownItems.Add("Только тревожные цели", null, (s, e) => { _chkOnlyAlerts.Checked = true; SyncFilterSettingsFromPanel(); });

            var testMenu = new ToolStripMenuItem("Пакеты");
            testMenu.DropDownItems.Add("Показать тестовый самолет", null, (s, e) => ShowTestAircraft());
            testMenu.DropDownItems.Add("Движение тестового самолета", null, (s, e) => StartMovingTestAircraft());
            testMenu.DropDownItems.Add("Тестовый пакет", null, (s, e) => InjectTestPacket());
            testMenu.DropDownItems.Add("Показать разбор последнего", null, (s, e) => ShowLastPacket());
            testMenu.DropDownItems.Add("Очистить журнал", null, (s, e) => ClearPacketHistory());
            testMenu.DropDownItems.Add("Вернуться к стартовому экрану", null, (s, e) => ReturnToStartScreen());

            var helpMenu = new ToolStripMenuItem("Справка");
            helpMenu.DropDownItems.Add("О программе", null, (s, e) =>
                MessageBox.Show(this,
                    "Радар для ПК: квадратный экран по размеру круга, расширенная панель информации и вкладка просмотра/разбора принимаемых пакетов.",
                    "О программе",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Information));

            menu.Items.Add(fileMenu);
            menu.Items.Add(viewMenu);
            menu.Items.Add(filterMenu);
            menu.Items.Add(testMenu);
            menu.Items.Add(helpMenu);
            MainMenuStrip = menu;
        }

        private void BuildLayout()
        {
            _mainSplit.Dock = DockStyle.Fill;
            _mainSplit.BackColor = Color.Black;
            _mainSplit.Panel1MinSize = 560;
            //_mainSplit.Panel2MinSize = 420;

            _radarHost.Dock = DockStyle.Fill;
            _radarHost.BackColor = Color.FromArgb(10, 10, 10);
            _radarHost.Padding = new Padding(10, 4, 10, 10);
            _radarHost.Controls.Add(_radarContainer);

            _radarContainer.BackColor = Color.Black;
            _radarContainer.Controls.Add(_radar);

            ConfigureLogTextBox(_decodedList, 9f);

            _targetDetails.Dock = DockStyle.Fill;
            _targetDetails.Multiline = true;
            _targetDetails.ReadOnly = true;
            _targetDetails.ScrollBars = ScrollBars.Vertical;
            _targetDetails.BorderStyle = BorderStyle.FixedSingle;
            _targetDetails.BackColor = Color.FromArgb(12, 22, 12);
            _targetDetails.ForeColor = Color.White;
            _targetDetails.Font = new Font("Consolas", 10f, FontStyle.Regular);
            _targetDetails.Text = "Выберите цель в списке или щелчком по радару.";

            var monitorTab = new TabPage("Монитор");
            var filterTab = new TabPage("Фильтры");
            monitorTab.Controls.Add(BuildMonitorPanel());
            filterTab.Controls.Add(BuildFilterPanel());

            _rightTabs.Dock = DockStyle.Fill;
            _rightTabs.TabPages.Add(monitorTab);
            _rightTabs.TabPages.Add(filterTab);

            _mainSplit.Panel1.Controls.Add(_radarHost);
            _mainSplit.Panel2.Controls.Add(_rightTabs);

            BuildTopToolbar();

            _topDockContainer.Dock = DockStyle.Top;
            _topDockContainer.Height = 82;
            _topDockContainer.Padding = new Padding(0, 0, 0, 1);
            _topDockContainer.BackColor = SystemColors.Control;
            _topToolbar.Dock = DockStyle.Top;
            if (MainMenuStrip != null)
            {
                MainMenuStrip.Dock = DockStyle.Top;
                MainMenuStrip.Padding = new Padding(6, 2, 0, 2);
                _topDockContainer.Controls.Add(MainMenuStrip);
            }
            _topDockContainer.Controls.Add(_topToolbar);
            if (MainMenuStrip != null)
                _topDockContainer.Controls.SetChildIndex(_topToolbar, 0);

            Controls.Add(_mainSplit);
            Controls.Add(_topDockContainer);

            _mainSplit.SizeChanged += (s, e) => AdjustSplitDistance();
            UpdateRadarLayout();
        }

        private void BuildTopToolbar()
        {
            _topToolbar.Dock = DockStyle.Top;
            _topToolbar.Height = 42;
            _topToolbar.Padding = new Padding(10, 6, 10, 2);
            _topToolbar.BackColor = SystemColors.Control;

            var row1 = new FlowLayoutPanel { Dock = DockStyle.Fill, Height = 34, WrapContents = false, AutoScroll = true, Padding = new Padding(0, 2, 0, 0) };

            Button MakeBtn(string text, EventHandler onClick, int width = 90)
            {
                var b = new Button { Text = text, Width = width, Height = 28, Margin = new Padding(0, 0, 8, 0) };
                b.Click += onClick;
                return b;
            }

            row1.Controls.Add(new Label { Text = "Источники:", AutoSize = true, Margin = new Padding(0, 7, 12, 0) });
            row1.Controls.Add(MakeBtn("COM Start", (s, e) => StartSerialSource(), 84));
            row1.Controls.Add(MakeBtn("COM Stop", (s, e) => StopSerialSource(), 84));
            row1.Controls.Add(MakeBtn("TCP Start", (s, e) => StartTcpSource(), 84));
            row1.Controls.Add(MakeBtn("TCP Stop", (s, e) => StopTcpSource(), 84));
            row1.Controls.Add(MakeBtn("UDP Start", (s, e) => StartUdpSource(), 84));
            row1.Controls.Add(MakeBtn("UDP Stop", (s, e) => StopUdpSource(), 84));
            row1.Controls.Add(MakeBtn("Тестовый пакет", (s, e) => InjectTestPacket(), 122));
            row1.Controls.Add(MakeBtn("Тестовый самолет", (s, e) => ShowTestAircraft(), 130));
            _testFlightButton = MakeBtn("Тестовый полет", (s, e) => ToggleMovingTestAircraft(), 120);
            row1.Controls.Add(_testFlightButton);
            row1.Controls.Add(MakeBtn("Очистить", (s, e) => ClearPacketHistory(), 88));
            row1.Controls.Add(MakeBtn("Стартовый экран", (s, e) => ReturnToStartScreen(), 132));
            row1.Controls.Add(MakeBtn("Настройки", (s, e) => OpenSettings(), 96));

            _topToolbar.Controls.Add(row1);
        }

        private Control BuildMonitorPanel()
        {
            var detailsTabs = new TabControl { Dock = DockStyle.Fill };
            var targetTab = new TabPage("Цель");
            var packetTab = new TabPage("Разбор пакета");
            var monitorTab = new TabPage("События");
            targetTab.Controls.Add(_targetDetails);
            packetTab.Controls.Add(_packetDetails);
            monitorTab.Controls.Add(_monitorList);
            detailsTabs.TabPages.Add(targetTab);
            detailsTabs.TabPages.Add(packetTab);
            detailsTabs.TabPages.Add(monitorTab);

            ConfigureLogTextBox(_monitorList, 8.5f);
            ConfigureLogTextBox(_packetList, 9f);
            ConfigureLogTextBox(_decodedList, 9f);
            _decodedList.MouseUp += (s, e) => ShowSelectedDecodedPacketDetails();
            _decodedList.KeyUp += (s, e) => ShowSelectedDecodedPacketDetails();
            _packetList.MouseUp += (s, e) => ShowSelectedPacketDetails();
            _packetList.KeyUp += (s, e) => ShowSelectedPacketDetails();

            _packetDetails.Dock = DockStyle.Fill;
            _packetDetails.ReadOnly = true;
            _packetDetails.Font = new Font("Consolas", 10f);
            _packetDetails.BackColor = Color.White;

            var outputPanel = new FlowLayoutPanel
            {
                Dock = DockStyle.None,
                Height = 26,
                WrapContents = false,
                AutoScroll = false,
                FlowDirection = FlowDirection.LeftToRight,
                Padding = new Padding(0, 2, 0, 0),
                AutoSize = true
            };

            _outputComCheckBox = new CheckBox { Text = "COM", Checked = true, AutoSize = true, Margin = new Padding(0, 2, 12, 0) };
            _outputTcpCheckBox = new CheckBox { Text = "TCP", Checked = true, AutoSize = true, Margin = new Padding(0, 2, 12, 0) };
            _outputUdpCheckBox = new CheckBox { Text = "UDP", Checked = true, AutoSize = true, Margin = new Padding(0, 2, 12, 0) };
            _outputTestCheckBox = new CheckBox { Text = "TEST", Checked = true, AutoSize = true, Margin = new Padding(0, 2, 12, 0) };
            _outputComCheckBox.CheckedChanged += (s, e) => SetSourceOutputEnabled("COM", _outputComCheckBox.Checked);
            _outputTcpCheckBox.CheckedChanged += (s, e) => SetSourceOutputEnabled("TCP", _outputTcpCheckBox.Checked);
            _outputUdpCheckBox.CheckedChanged += (s, e) => SetSourceOutputEnabled("UDP", _outputUdpCheckBox.Checked);
            _outputTestCheckBox.CheckedChanged += (s, e) => SetSourceOutputEnabled("TEST", _outputTestCheckBox.Checked);

            outputPanel.Controls.Add(new Label { Text = "Вывод:", AutoSize = true, Margin = new Padding(0, 4, 8, 0) });
            outputPanel.Controls.Add(_outputComCheckBox);
            outputPanel.Controls.Add(_outputTcpCheckBox);
            outputPanel.Controls.Add(_outputUdpCheckBox);
            outputPanel.Controls.Add(_outputTestCheckBox);

            var outputHost = new Panel { Dock = DockStyle.Fill, Margin = new Padding(0), Padding = new Padding(0) };
            outputHost.Controls.Add(outputPanel);
            outputHost.Resize += (s, e) =>
            {
                outputPanel.Left = Math.Max(0, outputHost.ClientSize.Width - outputPanel.Width);
                outputPanel.Top = Math.Max(0, (outputHost.ClientSize.Height - outputPanel.Height) / 2);
            };

            _targetGrid.Dock = DockStyle.Fill;
            _targetGrid.Margin = new Padding(0);
            _decodedList.Margin = new Padding(0);
            _packetList.Margin = new Padding(0);
            detailsTabs.Margin = new Padding(0);

            var layout = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 5,
                Margin = new Padding(0),
                Padding = new Padding(0)
            };
            layout.RowStyles.Add(new RowStyle(SizeType.Absolute, 28f));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 32f));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 22.6667f));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 22.6667f));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 22.6667f));

            layout.Controls.Add(outputHost, 0, 0);
            layout.Controls.Add(_targetGrid, 0, 1);
            layout.Controls.Add(_decodedList, 0, 2);
            layout.Controls.Add(_packetList, 0, 3);
            layout.Controls.Add(detailsTabs, 0, 4);

            return layout;
        }


        private void ApplySafeHorizontalSplitLayout(SplitContainer split, int minPanel1, int minPanel2, int desiredDistance)
        {
            try
            {
                if (split == null || split.IsDisposed)
                    return;

                int available = split.Height - split.SplitterWidth;
                if (available <= 40)
                    return;

                int safeMin1 = minPanel1;
                int safeMin2 = minPanel2;

                if (available <= (minPanel1 + minPanel2))
                {
                    safeMin1 = Math.Max(20, available / 3);
                    safeMin2 = Math.Max(20, available / 3);
                }

                int minDistance = Math.Max(20, safeMin1);
                int maxDistance = Math.Max(minDistance, available - safeMin2);
                int safeDistance = Math.Min(Math.Max(minDistance, desiredDistance), maxDistance);

                if (split.SplitterDistance != safeDistance)
                    split.SplitterDistance = safeDistance;

                if (split.Panel1MinSize != safeMin1)
                    split.Panel1MinSize = safeMin1;
                if (split.Panel2MinSize != safeMin2)
                    split.Panel2MinSize = safeMin2;
            }
            catch (Exception ex)
            {
                LogService.Write("ApplySafeHorizontalSplitLayout: " + ex);
            }
        }

        private void ConfigureTargetGrid()
        {
            _targetGrid.Dock = DockStyle.Top;
            _targetGrid.Height = 180;
            _targetGrid.ReadOnly = true;
            _targetGrid.AllowUserToAddRows = false;
            _targetGrid.AllowUserToDeleteRows = false;
            _targetGrid.AllowUserToResizeRows = false;
            _targetGrid.RowHeadersVisible = false;
            _targetGrid.MultiSelect = false;
            _targetGrid.SelectionMode = DataGridViewSelectionMode.FullRowSelect;
            _targetGrid.AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill;
            _targetGrid.BackgroundColor = Color.White;
            _targetGrid.BorderStyle = BorderStyle.FixedSingle;
            _targetGrid.Columns.Clear();
            _targetGrid.Columns.Add("Address", "Адрес");
            _targetGrid.Columns.Add("Callsign", "Позывной");
            _targetGrid.Columns.Add("Type", "Тип");
            _targetGrid.Columns.Add("Altitude", "Высота");
            _targetGrid.Columns.Add("Speed", "Скорость");
            _targetGrid.Columns.Add("Course", "Курс");
            _targetGrid.Columns.Add("Distance", "Дист., км");
            _targetGrid.Columns.Add("Source", "Источник");
            _targetGrid.Columns.Add("Time", "Время");
            _targetGrid.SelectionChanged += (s, e) =>
            {
                if (_suppressGridSelectionChanged || _targetGrid.SelectedRows.Count == 0)
                    return;
                var tag = _targetGrid.SelectedRows[0].Tag;
                if (tag is uint addr)
                    SelectTarget(addr);
            };
        }

        private Control BuildFilterPanel()
        {
            var panel = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                ColumnCount = 1,
                RowCount = 9,
                Padding = new Padding(8),
                AutoScroll = true
            };
            for (int i = 0; i < 9; i++) panel.RowStyles.Add(new RowStyle(SizeType.AutoSize));

            _summaryLabel.Text = "Расширенные фильтры";
            panel.Controls.Add(_summaryLabel, 0, 0);

            panel.Controls.Add(new Label { Text = "Типы целей", AutoSize = true }, 0, 1);
            _aircraftTypeFilter.Height = 120;
            _aircraftTypeFilter.CheckOnClick = true;
            _aircraftTypeFilter.Dock = DockStyle.Top;
            _aircraftTypeFilter.ItemCheck += AircraftTypeFilter_ItemCheck;
            panel.Controls.Add(_aircraftTypeFilter, 0, 2);

            panel.Controls.Add(new Label { Text = "Источники", AutoSize = true }, 0, 3);
            _sourceFilter.Height = 80;
            _sourceFilter.CheckOnClick = true;
            _sourceFilter.Dock = DockStyle.Top;
            _sourceFilter.ItemCheck += SourceFilter_ItemCheck;
            panel.Controls.Add(_sourceFilter, 0, 4);

            var rangePanel = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true };
            _maxDistanceFilter.Minimum = 1; _maxDistanceFilter.Maximum = 500; _maxDistanceFilter.Width = 80;
            _minAltitudeFilter.Minimum = -1000; _minAltitudeFilter.Maximum = 20000; _minAltitudeFilter.Width = 80;
            _maxAltitudeFilter.Minimum = -1000; _maxAltitudeFilter.Maximum = 30000; _maxAltitudeFilter.Width = 80;
            _maxDistanceFilter.ValueChanged += (s, e) => SyncFilterSettingsFromPanel();
            _minAltitudeFilter.ValueChanged += (s, e) => SyncFilterSettingsFromPanel();
            _maxAltitudeFilter.ValueChanged += (s, e) => SyncFilterSettingsFromPanel();
            rangePanel.Controls.Add(new Label { Text = "Макс. дальность, км", AutoSize = true, Margin = new Padding(0, 8, 4, 0) });
            rangePanel.Controls.Add(_maxDistanceFilter);
            rangePanel.Controls.Add(new Label { Text = "Мин. высота, м", AutoSize = true, Margin = new Padding(12, 8, 4, 0) });
            rangePanel.Controls.Add(_minAltitudeFilter);
            rangePanel.Controls.Add(new Label { Text = "Макс. высота, м", AutoSize = true, Margin = new Padding(12, 8, 4, 0) });
            rangePanel.Controls.Add(_maxAltitudeFilter);
            panel.Controls.Add(rangePanel, 0, 5);

            var flagsPanel = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true };
            _chkOnlyAlerts.CheckedChanged += (s, e) => SyncFilterSettingsFromPanel();
            _chkOnlyMoving.CheckedChanged += (s, e) => SyncFilterSettingsFromPanel();
            flagsPanel.Controls.Add(_chkOnlyAlerts);
            flagsPanel.Controls.Add(_chkOnlyMoving);
            panel.Controls.Add(flagsPanel, 0, 6);

            var btnPanel = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true };
            var btnApply = new Button { Text = "Применить", Width = 120 };
            var btnReset = new Button { Text = "Сброс", Width = 120 };
            btnApply.Click += (s, e) => SyncFilterSettingsFromPanel();
            btnReset.Click += (s, e) => ClearFilters();
            btnPanel.Controls.Add(btnApply);
            btnPanel.Controls.Add(btnReset);
            panel.Controls.Add(btnPanel, 0, 7);

            panel.Controls.Add(new Label
            {
                Text = "Подсказка: журнал пакетов открывается на вкладке 'Пакеты'. Можно добавить тестовый пакет и посмотреть разбор всех полей.",
                AutoSize = true,
                MaximumSize = new Size(430, 0)
            }, 0, 8);

            return panel;
        }

        private void BuildStatusBar()
        {
            _status.Items.Add(_statusLabel);
            _status.Items.Add(new ToolStripStatusLabel { Spring = true });
            _status.Items.Add(_zoomLabel);
            _status.Items.Add(_clockLabel);
            Controls.Add(_status);
        }

        private void PopulateFilterLists()
        {
            _aircraftTypeFilter.Items.Clear();
            for (int i = 0; i <= 5; i++)
                _aircraftTypeFilter.Items.Add(new FilterItem(i, GeoHelper.ResolveAircraftTypeName(i)));

            _sourceFilter.Items.Clear();
            foreach (int source in new[] { 0, 1, 2, 10, 11, 12 })
                _sourceFilter.Items.Add(new FilterItem(source, GeoHelper.ResolveSignalSourceName(source)));
        }

        private void ApplySettingsToUi()
        {
            NormalizeSettings();
            _radar.Settings = _settings;
            _radar.RadarRangeKm = _settings.RadarRangeKm;
            _radar.ShowRangeRings = _settings.ShowRangeRings;
            _radar.ShowTracks = _settings.ShowTracks;
            _radar.ShowTargetLabels = _settings.ShowTargetLabels;
            _ringsMenuItem.Checked = _settings.ShowRangeRings;
            _tracksMenuItem.Checked = _settings.ShowTracks;
            _mapMenuItem.Checked = _settings.ShowMapBackground;
            _labelsMenuItem.Checked = _settings.ShowTargetLabels;
            _northUpMenuItem.Checked = _settings.OrientationMode == RadarOrientationMode.NorthUp;
            _headingUpMenuItem.Checked = _settings.OrientationMode == RadarOrientationMode.HeadingUp;
            _zoomLabel.Text = $"Масштаб: {_settings.RadarRangeKm:0.0} км";
            SyncPanelFromSettings();
            RefreshRadarAndList();
            UpdateRadarLayout();
        }

        private void SaveSettingsQuietly()
        {
            try
            {
                _settings?.Save();
            }
            catch (Exception ex)
            {
                LogService.Write(ex, "MainForm.SaveSettingsQuietly");
            }
        }

        private void SyncPanelFromSettings()
        {
            for (int i = 0; i < _aircraftTypeFilter.Items.Count; i++)
            {
                var item = (FilterItem)_aircraftTypeFilter.Items[i];
                _aircraftTypeFilter.SetItemChecked(i, _settings.VisibleAircraftTypes.Contains(item.Value));
            }
            for (int i = 0; i < _sourceFilter.Items.Count; i++)
            {
                var item = (FilterItem)_sourceFilter.Items[i];
                _sourceFilter.SetItemChecked(i, _settings.VisibleSignalSources.Contains(item.Value));
            }
            _maxDistanceFilter.Value = (decimal)Math.Max((double)_maxDistanceFilter.Minimum, Math.Min((double)_maxDistanceFilter.Maximum, _settings.MaxVisibleDistanceKm));
            _minAltitudeFilter.Value = Math.Max(_minAltitudeFilter.Minimum, Math.Min(_minAltitudeFilter.Maximum, _settings.MinVisibleAltitudeMeters));
            _maxAltitudeFilter.Value = Math.Max(_maxAltitudeFilter.Minimum, Math.Min(_maxAltitudeFilter.Maximum, _settings.MaxVisibleAltitudeMeters));
            _chkOnlyAlerts.Checked = _settings.ShowOnlyAlertTargets;
            _chkOnlyMoving.Checked = _settings.ShowOnlyMovingTargets;
        }


        private void AircraftTypeFilter_ItemCheck(object sender, ItemCheckEventArgs e)
        {
            var values = new HashSet<int>();
            for (int i = 0; i < _aircraftTypeFilter.Items.Count; i++)
            {
                bool isChecked = i == e.Index ? e.NewValue == CheckState.Checked : _aircraftTypeFilter.GetItemChecked(i);
                if (isChecked)
                    values.Add(((FilterItem)_aircraftTypeFilter.Items[i]).Value);
            }

            _settings.VisibleAircraftTypes = values;
            SaveSettingsQuietly();
            RefreshRadarAndList();
        }

        private void SourceFilter_ItemCheck(object sender, ItemCheckEventArgs e)
        {
            var values = new HashSet<int>();
            for (int i = 0; i < _sourceFilter.Items.Count; i++)
            {
                bool isChecked = i == e.Index ? e.NewValue == CheckState.Checked : _sourceFilter.GetItemChecked(i);
                if (isChecked)
                    values.Add(((FilterItem)_sourceFilter.Items[i]).Value);
            }

            _settings.VisibleSignalSources = values;
            SaveSettingsQuietly();
            RefreshRadarAndList();
        }

        private void SyncFilterSettingsFromPanelUsingNewValues()
        {
            var selectedAircraft = new HashSet<int>();
            for (int i = 0; i < _aircraftTypeFilter.Items.Count; i++)
                if (_aircraftTypeFilter.GetItemChecked(i))
                    selectedAircraft.Add(((FilterItem)_aircraftTypeFilter.Items[i]).Value);

            var selectedSources = new HashSet<int>();
            for (int i = 0; i < _sourceFilter.Items.Count; i++)
                if (_sourceFilter.GetItemChecked(i))
                    selectedSources.Add(((FilterItem)_sourceFilter.Items[i]).Value);

            _settings.VisibleAircraftTypes = selectedAircraft;
            _settings.VisibleSignalSources = selectedSources;
            _settings.MaxVisibleDistanceKm = (double)_maxDistanceFilter.Value;
            _settings.MinVisibleAltitudeMeters = (int)_minAltitudeFilter.Value;
            _settings.MaxVisibleAltitudeMeters = (int)_maxAltitudeFilter.Value;
            _settings.ShowOnlyAlertTargets = _chkOnlyAlerts.Checked;
            _settings.ShowOnlyMovingTargets = _chkOnlyMoving.Checked;
            SaveSettingsQuietly();
            RefreshRadarAndList();
        }

        private void SyncFilterSettingsFromPanel()
        {
            _settings.VisibleAircraftTypes = new HashSet<int>(_aircraftTypeFilter.CheckedItems.Cast<FilterItem>().Select(x => x.Value));
            _settings.VisibleSignalSources = new HashSet<int>(_sourceFilter.CheckedItems.Cast<FilterItem>().Select(x => x.Value));
            _settings.MaxVisibleDistanceKm = (double)_maxDistanceFilter.Value;
            _settings.MinVisibleAltitudeMeters = (int)_minAltitudeFilter.Value;
            _settings.MaxVisibleAltitudeMeters = (int)_maxAltitudeFilter.Value;
            _settings.ShowOnlyAlertTargets = _chkOnlyAlerts.Checked;
            _settings.ShowOnlyMovingTargets = _chkOnlyMoving.Checked;
            SaveSettingsQuietly();
            RefreshRadarAndList();
        }

        private void ShowSettings()
        {
            OpenSettings();
        }

        private void StartSource()
        {
            try
            {
                _dataSource.Start(_settings);
                _refreshPending = true;
                AppendMonitorMessage($"[START] Запуск одновременного приема COM+TCP+UDP / формат {_settings.InputFormat}");
                AppendMonitorMessage("[START] Верхняя секция: расшифрованные пакеты. Средняя: сырые пакеты.");
                _statusLabel.Text = $"Прием активен: COM+TCP+UDP / {_settings.InputFormat}";
            }
            catch (Exception ex)
            {
                AppendMonitorMessage("[START] ОШИБКА: " + ex.Message);
                MessageBox.Show(this, ex.Message, "Ошибка запуска", MessageBoxButtons.OK, MessageBoxIcon.Error);
                _statusLabel.Text = "Ошибка запуска";
            }
        }

        private void HandleLineReceived(string source, string line)
        {
            AircraftTarget aircraft;
            string error;
            if (!AircraftMessageParser.TryParse(line, _settings, out aircraft, out error))
            {
                _statusLabel.Text = "Ошибка пакета: " + error;
                return;
            }

            var distanceAndBearing = GeoHelper.DistanceAndBearing(_settings.LocalLatitude, _settings.LocalLongitude, aircraft.Latitude, aircraft.Longitude);
            aircraft.DistanceKm = distanceAndBearing.distanceKm;
            aircraft.BearingDeg = distanceAndBearing.bearingDeg;

            var target = _repository.Upsert(aircraft);
            _trackService.Append(target);
            _trackService.Trim(target, TimeSpan.FromSeconds(_settings.TrackHistorySeconds));

            _statusLabel.Text = $"Принят пакет {aircraft.Callsign} ({aircraft.Address:X6})";
            _refreshPending = true;
        }

        private void UpdateUiState()
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action(UpdateUiState));
                return;
            }
            try
            {
                _clockLabel.Text = _settings.GetLocalTime().ToString("dd.MM.yyyy HH:mm:ss");
                UpdateTestAircraftMotion();
                _repository.RemoveStale(TimeSpan.FromSeconds(GetTargetRemovalSeconds()));
                if (_refreshPending)
                {
                    RefreshRadarAndList();
                    _refreshPending = false;
                }
            }
            catch (Exception ex)
            {
                LogService.Write(ex, "MainForm.UpdateUiState");
            }
        }

        private void RefreshRadarAndList()
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action(RefreshRadarAndList));
                return;
            }
            try
            {
                var allTargets = _repository.GetAll();
                _alertService.Evaluate(allTargets, _settings);
                var filtered = _filterService.Apply(allTargets, _settings);
                if (_selectedTargetAddress.HasValue && _repository.FindByAddress(_selectedTargetAddress.Value) == null && !CanUseSelectedFallback())
                    _selectedTargetAddress = null;
                _radar.SelectedTargetAddress = _selectedTargetAddress;
                _radar.SetTargets(filtered);
                RefreshList(filtered);
                UpdateSummary(filtered, allTargets.Count);
                UpdateSelectedTargetDetails();
            }
            catch (Exception ex)
            {
                LogService.Write(ex, "MainForm.RefreshRadarAndList");
                _statusLabel.Text = "Ошибка обновления экрана";
            }
        }

        private void UpdateSummary(IReadOnlyList<AircraftTarget> filtered, int totalCount)
        {
            int warnings = filtered.Count(t => t.AlertLevel == AlertLevel.Warning);
            int dangers = filtered.Count(t => t.AlertLevel == AlertLevel.Danger);
            _summaryLabel.Text = $"Всего: {totalCount} | После фильтра: {filtered.Count} | Внимание: {warnings} | Опасно: {dangers}";
        }

        private void RefreshList(IReadOnlyList<AircraftTarget> targets)
        {
            PopulateTargetGrid(targets);

            var selected = _selectedTargetAddress.HasValue ? _repository.FindByAddress(_selectedTargetAddress.Value) : null;
            if (selected == null)
            {
                if (_selectedTargetAddress.HasValue && _selectedTargetFallbackAddress == _selectedTargetAddress)
                    _summaryLabel.Text = "Выбрана цель из монитора. Детали сохранены до появления новой отметки.";
                else
                    _summaryLabel.Text = $"Всего целей: {targets.Count}. Верхняя секция показывает расшифрованные пакеты, средняя — сырые строки.";
            }
            else
            {
                _summaryLabel.Text = $"Выбрана цель: {selected.Callsign} {selected.DistanceKm:0.0} км, {selected.AltitudeGps} м, {GeoHelper.ResolveSignalSourceName(selected.SignalSource)}";
            }
        }

        private void PopulateTargetGrid(IReadOnlyList<AircraftTarget> targets)
        {
            _suppressGridSelectionChanged = true;
            try
            {
                _targetGrid.Rows.Clear();
                foreach (var target in targets.OrderBy(t => t.DistanceKm))
                {
                    int rowIndex = _targetGrid.Rows.Add(
                        target.Address.ToString("X6"),
                        target.Callsign,
                        GeoHelper.ResolveAircraftTypeName(target.AircraftType),
                        target.AltitudeGps,
                        target.SpeedKmh,
                        target.CourseDeg,
                        target.DistanceKm.ToString("0.0"),
                        GeoHelper.ResolveSignalSourceName(target.SignalSource),
                        target.LastUpdateUtc.ToLocalTime().ToString("HH:mm:ss")
                    );
                    _targetGrid.Rows[rowIndex].Tag = target.Address;
                }
                SyncListSelection();
            }
            finally
            {
                _suppressGridSelectionChanged = false;
            }
        }

        private void SelectTarget(uint? address)
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action<uint?>(SelectTarget), address);
                return;
            }

            bool changed = _selectedTargetAddress != address || _radar.SelectedTargetAddress != address;
            _selectedTargetAddress = address;
            _radar.SelectedTargetAddress = address;
            SyncListSelection();
            UpdateSelectedTargetDetails();
            if (changed)
                _radar.Invalidate();
        }

        private void SyncListSelection()
        {
            if (_targetGrid.Rows.Count == 0)
                return;

            _suppressGridSelectionChanged = true;
            try
            {
                _targetGrid.ClearSelection();
                if (!_selectedTargetAddress.HasValue)
                    return;

                foreach (DataGridViewRow row in _targetGrid.Rows)
                {
                    if (row.Tag is uint addr && addr == _selectedTargetAddress.Value)
                    {
                        row.Selected = true;
                        if (_targetGrid.FirstDisplayedScrollingRowIndex != row.Index)
                            _targetGrid.FirstDisplayedScrollingRowIndex = Math.Max(0, row.Index);
                        break;
                    }
                }
            }
            finally
            {
                _suppressGridSelectionChanged = false;
            }
        }

        private void CacheSelectedTargetFallback(uint address, string details)
        {
            if (address == 0 || string.IsNullOrWhiteSpace(details))
                return;

            _selectedTargetFallbackAddress = address;
            _selectedTargetFallbackDetails = details;
            _selectedTargetFallbackExpiresUtc = DateTime.UtcNow.AddSeconds(Math.Max(5, _settings != null ? _settings.TargetHoldSeconds : 5));
        }

        private bool CanUseSelectedFallback()
        {
            return _selectedTargetAddress.HasValue
                && _selectedTargetFallbackAddress == _selectedTargetAddress
                && !string.IsNullOrWhiteSpace(_selectedTargetFallbackDetails)
                && DateTime.UtcNow <= _selectedTargetFallbackExpiresUtc;
        }

        private int GetTargetRemovalSeconds()
        {
            int holdSeconds = Math.Max(5, _settings != null ? _settings.TargetHoldSeconds : 8);
            int fadeSeconds = Math.Max(4, Math.Min(8, holdSeconds));
            return holdSeconds + fadeSeconds;
        }

        private void UpdateSelectedTargetDetails()
        {
            if (_selectedTargetAddress == null)
            {
                _targetDetails.Text = "Выберите цель в списке или щелчком по радару.";
                return;
            }

            var target = _repository.FindByAddress(_selectedTargetAddress.Value);
            if (target == null)
            {
                if (CanUseSelectedFallback())
                {
                    _targetDetails.Text = _selectedTargetFallbackDetails + Environment.NewLine + Environment.NewLine + "Примечание: живая отметка временно отсутствует, показаны последние данные из монитора.";
                }
                else
                {
                    _targetDetails.Text = "Выбранная цель больше недоступна.";
                }
                return;
            }

            var liveDetails = string.Join(Environment.NewLine, new[]
            {
                "Подробная информация по выбранной цели",
                string.Empty,
                "Адрес         : " + target.Address.ToString("X6"),
                "Позывной      : " + target.Callsign,
                "Тип           : " + GeoHelper.ResolveAircraftTypeName(target.AircraftType),
                "Источник      : " + GeoHelper.ResolveSignalSourceName(target.SignalSource),
                "Высота GPS    : " + target.AltitudeGps + " м",
                "Баро-высота   : " + target.PressureAltitude + " м",
                "Скорость      : " + target.SpeedKmh + " км/ч",
                "Курс          : " + target.CourseDeg + "°",
                "Вертикальная  : " + target.VerticalRate,
                "Дистанция     : " + target.DistanceKm.ToString("0.0") + " км",
                "Пеленг        : " + target.BearingDeg.ToString("0") + "° " + GeoHelper.GetCardinalText(target.BearingDeg),
                "Координаты    : " + target.Latitude.ToString("0.000000") + ", " + target.Longitude.ToString("0.000000"),
                "Тревога       : " + target.AlertLevel
            });
            _targetDetails.Text = liveDetails;
            CacheSelectedTargetFallback(target.Address, liveDetails);
        }

        private void AdjustSplitDistance()
        {
            if (_mainSplit.Width <= 0)
                return;

            int minLeft = _mainSplit.Panel1MinSize;
            int minRight = _mainSplit.Panel2MinSize;
            int maxLeft = _mainSplit.Width - minRight - _mainSplit.SplitterWidth;
            if (maxLeft < minLeft)
                maxLeft = minLeft;

            int desired = (int)(_mainSplit.Width * 0.62);
            if (desired < minLeft)
                desired = minLeft;
            if (desired > maxLeft)
                desired = maxLeft;

            try
            {
                _mainSplit.SplitterDistance = desired;
            }
            catch
            {
            }
        }

        private void RegisterPacket(string source, string line)
        {
            string sourceKind = GetSourceKind(source);
            var item = new PacketViewItem
            {
                Timestamp = DateTime.Now,
                Source = string.IsNullOrWhiteSpace(source) ? "UNKNOWN" : source,
                RawLine = line,
                ParsedDetails = BuildPacketDetails(source, line)
            };

            _packetHistory.Add(item);
            if (!_mutedOutputSources.Contains(sourceKind))
            {
                _visiblePacketItems.Add(item);
                while (_visiblePacketItems.Count > MaxPacketHistory)
                    _visiblePacketItems.RemoveAt(0);
                AppendLine(_packetList, item.ToString());
                if (_visiblePacketItems.Count != _packetHistory.Count(x => !_mutedOutputSources.Contains(GetSourceKind(x.Source))))
                    RebuildVisiblePacketLists();
            }

            while (_packetHistory.Count > MaxPacketHistory)
                _packetHistory.RemoveAt(0);

            AircraftTarget aircraft;
            string error;
            if (AircraftMessageParser.TryParse(line, _settings, out aircraft, out error))
            {
                var decoded = new DecodedViewItem
                {
                    Timestamp = item.Timestamp,
                    Source = item.Source,
                    Address = aircraft.Address,
                    Details = item.ParsedDetails,
                    Summary = BuildDecodedSummary(aircraft, source)
                };
                _decodedHistory.Add(decoded);
                while (_decodedHistory.Count > MaxPacketHistory)
                    _decodedHistory.RemoveAt(0);

                if (!_mutedOutputSources.Contains(sourceKind))
                {
                    _visibleDecodedItems.Add(decoded);
                    while (_visibleDecodedItems.Count > MaxPacketHistory)
                        _visibleDecodedItems.RemoveAt(0);
                    AppendLine(_decodedList, decoded.ToString());
                    if (_visibleDecodedItems.Count != _decodedHistory.Count(x => !_mutedOutputSources.Contains(GetSourceKind(x.Source))))
                        RebuildVisiblePacketLists();
                }
            }

            if (!_mutedOutputSources.Contains(sourceKind))
            {
                _selectedPacketLineIndex = _visiblePacketItems.Count - 1;
                _selectedDecodedLineIndex = _visibleDecodedItems.Count - 1;
            }
        }

        private string BuildPacketDetails(string source, string line)
        {
            AircraftTarget aircraft;
            string error;
            if (!AircraftMessageParser.TryParse(line, _settings, out aircraft, out error))
                return "Источник: " + source + "\r\nОшибка разбора:\r\n" + error + "\r\n\r\nСырой пакет:\r\n" + line;

            var values = GeoHelper.DistanceAndBearing(_settings.LocalLatitude, _settings.LocalLongitude, aircraft.Latitude, aircraft.Longitude);
            string formatName = DetectPacketFormat(line);
            return string.Join(Environment.NewLine, new[]
            {
                "Источник        : " + source,
                "Сырой пакет:",
                line,
                string.Empty,
                "Разбор полей:",
                "Формат         : " + formatName,
                "Address        : " + aircraft.Address.ToString("X6"),
                "Squawk         : " + aircraft.Squawk,
                "Callsign       : " + aircraft.Callsign,
                "Altitude GPS   : " + aircraft.AltitudeGps + " м",
                "Press Alt      : " + aircraft.PressureAltitude + " м",
                "Speed          : " + aircraft.SpeedKmh + " км/ч",
                "Course         : " + aircraft.CourseDeg + "°",
                "Vertical Rate  : " + aircraft.VerticalRate,
                "Latitude       : " + aircraft.Latitude.ToString("0.000000"),
                "Longitude      : " + aircraft.Longitude.ToString("0.000000"),
                "Aircraft Type  : " + aircraft.AircraftType + " (" + GeoHelper.ResolveAircraftTypeName(aircraft.AircraftType) + ")",
                "Source         : " + aircraft.SignalSource + " (" + GeoHelper.ResolveSignalSourceName(aircraft.SignalSource) + ")",
                "Time Msg       : " + aircraft.HourMsg.ToString("00") + ":" + aircraft.MinMsg.ToString("00"),
                "ExtraHex       : " + aircraft.ExtraHex,
                "Distance       : " + values.distanceKm.ToString("0.0") + " км",
                "Bearing        : " + values.bearingDeg.ToString("0") + "° " + GeoHelper.GetCardinalText(values.bearingDeg)
            });
        }



        private static string DetectPacketFormat(string line)
        {
            if (string.IsNullOrWhiteSpace(line))
                return "Unknown";
            line = line.Trim();
            if (line.StartsWith("$PFLAA,")) return "NMEA PFLAA";
            if (line.StartsWith("$FLYRF,")) return "FLYRF";
            if (line.StartsWith("MSG,")) return "SBS-1";
            if (line.StartsWith("GDL90HEX:")) return "GDL90";
            return "Auto";
        }

        private string BuildDecodedSummary(AircraftTarget aircraft, string source)
        {
            return string.Format("{0:HH:mm:ss}  [{1}] {2} {3:0.0}км {4}м {5}° {6}км/ч",
                DateTime.Now, GetSourceKind(source), string.IsNullOrWhiteSpace(aircraft.Callsign) ? aircraft.Address.ToString("X6") : aircraft.Callsign,
                aircraft.DistanceKm, aircraft.AltitudeGps, aircraft.CourseDeg, aircraft.SpeedKmh);
        }

        private void ShowSelectedDecodedPacketDetails()
        {
            int idx = GetSelectedLineIndex(_decodedList);
            if (idx < 0 || idx >= _visibleDecodedItems.Count)
                return;

            _selectedDecodedLineIndex = idx;
            var decoded = _visibleDecodedItems[idx];
            _packetDetails.Text = decoded.Details;
            if (decoded.Address != 0)
            {
                CacheSelectedTargetFallback(decoded.Address, decoded.Details);
                SelectTarget(decoded.Address);
            }
        }

        private static string GetSourceKind(string source)
        {
            if (string.IsNullOrWhiteSpace(source)) return "UNKNOWN";
            var value = source.Trim().TrimStart('[').ToUpperInvariant();
            if (value.StartsWith("COM")) return "COM";
            if (value.StartsWith("TCP")) return "TCP";
            if (value.StartsWith("UDP")) return "UDP";
            if (value.StartsWith("TEST")) return "TEST";
            return value;
        }

        private void SetSourceOutputEnabled(string sourceKind, bool enabled)
        {
            if (enabled)
                _mutedOutputSources.Remove(sourceKind);
            else
                _mutedOutputSources.Add(sourceKind);

            RebuildVisiblePacketLists();
        }

        private void RebuildVisiblePacketLists()
        {
            _visibleDecodedItems.Clear();
            foreach (var item in _decodedHistory.Where(x => !_mutedOutputSources.Contains(GetSourceKind(x.Source))))
                _visibleDecodedItems.Add(item);
            _decodedList.Text = string.Join(Environment.NewLine, _visibleDecodedItems.Select(x => x.ToString()));

            _visiblePacketItems.Clear();
            foreach (var item in _packetHistory.Where(x => !_mutedOutputSources.Contains(GetSourceKind(x.Source))))
                _visiblePacketItems.Add(item);
            _packetList.Text = string.Join(Environment.NewLine, _visiblePacketItems.Select(x => x.ToString()));

            ScrollToBottom(_decodedList);
            ScrollToBottom(_packetList);
        }

        private void ConfigureCopyContextMenu(RichTextBox box)
        {
            var menu = new ContextMenuStrip();
            var miCopy = new ToolStripMenuItem("Копировать");
            var miSelectAll = new ToolStripMenuItem("Выделить все");
            miCopy.Click += (s, e) => { if (!string.IsNullOrEmpty(box.SelectedText)) box.Copy(); };
            miSelectAll.Click += (s, e) => box.SelectAll();
            menu.Items.Add(miCopy);
            menu.Items.Add(miSelectAll);
            menu.Opening += (s, e) => miCopy.Enabled = !string.IsNullOrEmpty(box.SelectedText);
            box.ContextMenuStrip = menu;
            box.ShortcutsEnabled = true;
            box.HideSelection = false;
            box.MouseUp += (s, e) =>
            {
                if (e.Button == MouseButtons.Right)
                {
                    box.Focus();
                    menu.Show(box, e.Location);
                }
            };
        }

        private void OpenSettings()
        {
            using (var form = new SettingsForm(_settings))
            {
                if (form.ShowDialog(this) != DialogResult.OK)
                    return;

                _settings = form.ResultSettings;
                NormalizeSettings();
                _settings.Save();
                ApplySettingsToUi();
                RefreshRadarAndList();
            }
        }

        private void StartSerialSource()
        {
            _dataSource.StartSerialSource(_settings);
        }

        private void StopSerialSource()
        {
            _dataSource.StopSerialSource();
            _statusLabel.Text = "COM остановлен";
        }

        private void StartTcpSource()
        {
            _dataSource.StartTcpSource(_settings);
        }

        private void StopTcpSource()
        {
            _dataSource.StopTcpSource();
            _statusLabel.Text = "TCP остановлен";
        }

        private void StartUdpSource()
        {
            _dataSource.StartUdpSource(_settings);
        }

        private void StopUdpSource()
        {
            _dataSource.StopUdpSource();
            _statusLabel.Text = "UDP остановлен";
        }

        private void ConfigureLogTextBox(RichTextBox box, float fontSize)
        {
            box.Dock = DockStyle.Fill;
            box.ReadOnly = true;
            box.Multiline = true;
            box.WordWrap = false;
            box.HideSelection = false;
            box.BorderStyle = BorderStyle.FixedSingle;
            box.BackColor = Color.White;
            box.ForeColor = Color.Black;
            box.Font = new Font("Consolas", fontSize);
            box.ScrollBars = RichTextBoxScrollBars.Both;
            box.DetectUrls = false;
            box.ShortcutsEnabled = true;
        }

        private void AppendLine(RichTextBox box, string text)
        {
            if (box.TextLength > 0)
                box.AppendText(Environment.NewLine);
            box.AppendText(text ?? string.Empty);
            ScrollToBottom(box);
        }

        private static void ScrollToBottom(RichTextBox box)
        {
            box.SelectionStart = box.TextLength;
            box.ScrollToCaret();
        }

        private static int GetSelectedLineIndex(RichTextBox box)
        {
            int charIndex = box.SelectionStart;
            return box.GetLineFromCharIndex(charIndex);
        }

        private void ShowSelectedPacketDetails()
        {
            int idx = GetSelectedLineIndex(_packetList);
            if (idx >= 0 && idx < _visiblePacketItems.Count)
            {
                _selectedPacketLineIndex = idx;
                var item = _visiblePacketItems[idx];
                _packetDetails.Text = item.ParsedDetails;
                CacheSelectedTargetFromRawPacket(item);
            }
            else
            {
                _packetDetails.Clear();
            }
        }


        private void CacheSelectedTargetFromRawPacket(PacketViewItem item)
        {
            if (item == null || string.IsNullOrWhiteSpace(item.RawLine))
                return;

            AircraftTarget aircraft;
            string error;
            if (!AircraftMessageParser.TryParse(item.RawLine, _settings, out aircraft, out error) || aircraft.Address == 0)
                return;

            CacheSelectedTargetFallback(aircraft.Address, item.ParsedDetails);
            SelectTarget(aircraft.Address);
        }

        private void ShowLastPacket()
        {
            if (_visiblePacketItems.Count <= 0)
                return;
            _rightTabs.SelectedIndex = 0;
            _selectedPacketLineIndex = _visiblePacketItems.Count - 1;
            ShowSelectedPacketDetails();
        }

        private void ClearPacketHistory()
        {
            ClearPacketHistoryInternal(true);
        }

        private void ClearPacketHistoryInternal(bool updateStatus)
        {
            _packetHistory.Clear();
            _decodedHistory.Clear();
            _visibleDecodedItems.Clear();
            _visiblePacketItems.Clear();
            _decodedList.Clear();
            _packetList.Clear();
            _monitorList.Clear();
            _packetDetails.Clear();
            _targetDetails.Text = "Выберите цель в списке или щелчком по радару.";
            if (updateStatus)
                _statusLabel.Text = "Журнал пакетов очищен";
        }

        private void InjectTestPacket()
        {
            string packet;
            switch (_settings.InputFormat)
            {
                case InputFormat.Nmea:
                    packet = TestPacketFactory.CreatePflAa();
                    break;
                case InputFormat.Gdl90:
                    packet = TestPacketFactory.CreateGdl90Hex();
                    break;
                case InputFormat.Sbs1:
                    packet = TestPacketFactory.CreateSbs1();
                    break;
                default:
                    packet = TestPacketFactory.CreateFlyRf();
                    break;
            }
            _rightTabs.SelectedIndex = 0;
            RegisterPacket("TEST", packet);
            HandleLineReceived("TEST", packet);
        }



        private void UpdateTestAircraftMotion()
        {
            if (!_testFlightActive)
                return;

            var existing = _repository.FindByAddress(0xABC123);
            if (existing == null)
            {
                _testFlightActive = false;
                UpdateTestFlightButtonState();
                return;
            }

            _testFlightStep++;
            double angleDeg = (_testFlightStep * 4.0) % 360.0;
            double radiusKm = 6.0 + Math.Sin(_testFlightStep / 10.0) * 1.5;
            double eastKm = Math.Cos(angleDeg * Math.PI / 180.0) * radiusKm;
            double northKm = Math.Sin(angleDeg * Math.PI / 180.0) * radiusKm;
            var position = GeoHelper.OffsetLatLon(_settings.LocalLatitude, _settings.LocalLongitude, northKm, eastKm);

            int course = (int)((360.0 - angleDeg) % 360.0);
            var now = _settings.GetLocalTime();
            var aircraft = new AircraftTarget
            {
                Address = 0xABC123,
                Squawk = 7000,
                Callsign = "TEST01",
                AltitudeGps = 1200 + (int)Math.Round(150.0 * Math.Sin(_testFlightStep / 4.0)),
                PressureAltitude = 1180 + (int)Math.Round(150.0 * Math.Sin(_testFlightStep / 4.0)),
                SpeedKmh = 90,
                CourseDeg = course,
                VerticalRate = (int)Math.Round(2.0 * Math.Cos(_testFlightStep / 4.0)),
                Latitude = position.latitude,
                Longitude = position.longitude,
                AircraftType = 1,
                SignalSource = 0,
                HourMsg = now.Hour,
                MinMsg = now.Minute,
                ExtraHex = "AA55",
                LastUpdateUtc = DateTime.UtcNow,
                IsTestTarget = true,
                IsSynthetic = true
            };

            var distanceAndBearing = GeoHelper.DistanceAndBearing(_settings.LocalLatitude, _settings.LocalLongitude, aircraft.Latitude, aircraft.Longitude);
            aircraft.DistanceKm = distanceAndBearing.distanceKm;
            aircraft.BearingDeg = distanceAndBearing.bearingDeg;

            var target = _repository.Upsert(aircraft);
            _trackService.Append(target);
            _trackService.Trim(target, TimeSpan.FromSeconds(_settings.TrackHistorySeconds));
            _settings.OwnshipHeadingDeg = course;
            _refreshPending = true;
        }

        private AircraftTarget BuildStableTestTarget()
        {
            _testFlightStep = 0;
            var now = _settings.GetLocalTime();
            var position = GeoHelper.OffsetLatLon(_settings.LocalLatitude, _settings.LocalLongitude, 0.0, 3.0);
            var aircraft = new AircraftTarget
            {
                Address = 0xABC123,
                Squawk = 7000,
                Callsign = "TEST01",
                AltitudeGps = 1200,
                PressureAltitude = 1180,
                SpeedKmh = 90,
                CourseDeg = 0,
                VerticalRate = 2,
                Latitude = position.latitude,
                Longitude = position.longitude,
                AircraftType = 1,
                SignalSource = 0,
                HourMsg = now.Hour,
                MinMsg = now.Minute,
                ExtraHex = "AA55",
                LastUpdateUtc = DateTime.UtcNow,
                IsTestTarget = true,
                IsSynthetic = true
            };

            var distanceAndBearing = GeoHelper.DistanceAndBearing(_settings.LocalLatitude, _settings.LocalLongitude, aircraft.Latitude, aircraft.Longitude);
            aircraft.DistanceKm = distanceAndBearing.distanceKm;
            aircraft.BearingDeg = distanceAndBearing.bearingDeg;
            return aircraft;
        }

        private void EnsureTestTargetVisibleInUi()
        {
            if (_settings.RadarRangeKm < 5f)
                _settings.RadarRangeKm = 20f;

            _settings.ShowTargetLabels = true;
            _settings.ShowRangeRings = true;
            _settings.ShowTracks = true;
            _settings.ShowOnlyAlertTargets = false;
            _settings.ShowOnlyMovingTargets = false;

            if (_settings.MaxVisibleDistanceKm < 20)
                _settings.MaxVisibleDistanceKm = 20;
            if (_settings.MinVisibleAltitudeMeters > 1200)
                _settings.MinVisibleAltitudeMeters = 0;
            if (_settings.MaxVisibleAltitudeMeters < 1200)
                _settings.MaxVisibleAltitudeMeters = 10000;

            if (_settings.VisibleAircraftTypes != null && _settings.VisibleAircraftTypes.Count > 0 && !_settings.VisibleAircraftTypes.Contains(1))
                _settings.VisibleAircraftTypes.Add(1);
            if (_settings.VisibleSignalSources != null && _settings.VisibleSignalSources.Count > 0 && !_settings.VisibleSignalSources.Contains(0))
                _settings.VisibleSignalSources.Add(0);

            ApplySettingsToUi();
        }

        private void ShowTestAircraft()
        {
            LogService.Write("MainForm.ShowTestAircraft: begin");
            try
            {
                if (InvokeRequired)
                {
                    BeginInvoke(new Action(ShowTestAircraft));
                    return;
                }

                LogService.Write("MainForm.ShowTestAircraft: switch tab and ensure visible");
                _rightTabs.SelectedIndex = 0;
                EnsureTestTargetVisibleInUi();

                LogService.Write("MainForm.ShowTestAircraft: build target");
                var builtTarget = BuildStableTestTarget();
                LogService.Write($"MainForm.ShowTestAircraft: target built addr={builtTarget.Address:X6} lat={builtTarget.Latitude:0.000000} lon={builtTarget.Longitude:0.000000} dist={builtTarget.DistanceKm:0.0}");
                var target = _repository.Upsert(builtTarget);
                _testFlightActive = false;
                if (_settings.ShowMapBackground && _settings.MapProviderMode == MapProviderMode.None)
                    _settings.MapProviderMode = MapProviderMode.OpenStreetMap;
                LogService.Write($"MainForm.ShowTestAircraft: repository upsert done count={_repository.GetAll().Count}");
                _trackService.Append(target);
                _trackService.Trim(target, TimeSpan.FromSeconds(_settings.TrackHistorySeconds));
                LogService.Write($"MainForm.ShowTestAircraft: track count={target.Track.Count}");

                LogService.Write("MainForm.ShowTestAircraft: before SelectTarget");
                SelectTarget(target.Address);
                LogService.Write("MainForm.ShowTestAircraft: SelectTarget done");
                _refreshPending = true;
                RefreshRadarAndList();
                LogService.Write("MainForm.ShowTestAircraft: RefreshRadarAndList done");
                _radar.Invalidate();
                LogService.Write("MainForm.ShowTestAircraft: radar invalidate done");
                _statusLabel.Text = $"Тестовый самолет TEST01 показан: {target.DistanceKm:0.0} км, {target.BearingDeg:0}°";
                LogService.Write("MainForm.ShowTestAircraft: completed OK");
            }
            catch (Exception ex)
            {
                LogService.Write(ex, "MainForm.ShowTestAircraft");
                MessageBox.Show(this,
                    ex.Message + "\r\n\r\nПодробности записаны в FlightRadarPc.log",
                    "Ошибка показа тестового самолета",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        private void ToggleMovingTestAircraft()
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action(ToggleMovingTestAircraft));
                return;
            }

            if (_testFlightActive)
            {
                StopMovingTestAircraft();
                return;
            }

            StartMovingTestAircraft();
        }

        private void StartMovingTestAircraft()
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action(StartMovingTestAircraft));
                return;
            }

            EnsureTestTargetVisibleInUi();
            var target = _repository.Upsert(BuildStableTestTarget());
            _trackService.Append(target);
            _trackService.Trim(target, TimeSpan.FromSeconds(_settings.TrackHistorySeconds));
            _testFlightActive = true;
            if (_settings.ShowMapBackground && _settings.MapProviderMode == MapProviderMode.None)
                _settings.MapProviderMode = MapProviderMode.OpenStreetMap;
            SelectTarget(target.Address);
            _refreshPending = true;
            RefreshRadarAndList();
            UpdateTestFlightButtonState();
            _statusLabel.Text = "Тестовый полет запущен независимо от источников";
        }

        private void StopMovingTestAircraft()
        {
            _testFlightActive = false;
            UpdateTestFlightButtonState();
            _statusLabel.Text = "Тестовый полет остановлен";
        }

        private void UpdateTestFlightButtonState()
        {
            if (_testFlightButton == null)
                return;

            _testFlightButton.Text = _testFlightActive ? "Стоп тест" : "Тестовый полет";
        }

        private void TryApplyAppIcon()
        {
            try
            {
                string iconPath = System.IO.Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "AppIcon.ico");
                if (System.IO.File.Exists(iconPath))
                    Icon = new Icon(iconPath);
            }
            catch
            {
            }
        }

        private void ReturnToStartScreen()
        {
            _dataSource.Stop();
            _repository.Clear();
            _testFlightActive = false;
            UpdateTestFlightButtonState();
            _testFlightStep = 0;
            _selectedTargetAddress = null;
            _radar.SelectedTargetAddress = null;
            _settings.RadarRangeKm = _startupRadarRangeKm;
            _rightTabs.SelectedIndex = 0;
            ClearPacketHistoryInternal(false);
            _refreshPending = true;
            RefreshRadarAndList();
            UpdateRadarLayout();
            _statusLabel.Text = "Стартовый экран восстановлен";
        }

        private void ProcessAlerts()
        {
            var alerts = _repository.GetAll().Where(x => x.AlertLevel != AlertLevel.None).OrderByDescending(x => x.AlertLevel).ToList();
            var highest = alerts.FirstOrDefault();
            if (highest == null)
                return;

            if (highest.AlertLevel == AlertLevel.Danger)
            {
                _statusLabel.Text = $"ОПАСНО: {highest.Callsign} {highest.DistanceKm:0.0} км, {highest.AltitudeGps} м";
                if (_settings.PlayAlarmSound)
                    SystemSounds.Exclamation.Play();
            }
            else if (highest.AlertLevel == AlertLevel.Warning)
            {
                _statusLabel.Text = $"ВНИМАНИЕ: {highest.Callsign} {highest.DistanceKm:0.0} км, {highest.AltitudeGps} м";
            }
        }

        private void ClearFilters()
        {
            _settings.VisibleAircraftTypes.Clear();
            _settings.VisibleSignalSources.Clear();
            _settings.MaxVisibleDistanceKm = 25;
            _settings.MinVisibleAltitudeMeters = 0;
            _settings.MaxVisibleAltitudeMeters = 10000;
            _settings.ShowOnlyAlertTargets = false;
            _settings.ShowOnlyMovingTargets = false;
            SaveSettingsQuietly();
            ApplySettingsToUi();
        }

        private void ResetZoom()
        {
            _settings.RadarRangeKm = 20f;
            SaveSettingsQuietly();
            ApplySettingsToUi();
        }

        private void ToggleFullScreen()
        {
            _fullScreen.Toggle(this);
            MainMenuStrip.Visible = !_fullScreen.IsFullScreen;
            _status.Visible = true;
            UpdateRadarLayout();
        }

        private void UpdateRadarLayout()
        {
            if (_mainSplit.Width <= 0 || _mainSplit.Height <= 0)
                return;

            int availableWidth = Math.Max(0, _radarHost.ClientSize.Width - _radarHost.Padding.Horizontal);
            int availableHeight = Math.Max(0, _radarHost.ClientSize.Height - _radarHost.Padding.Vertical);
            int side = Math.Min(availableWidth, availableHeight);
            side = Math.Max(100, side);
            int x = _radarHost.Padding.Left + Math.Max(0, (availableWidth - side) / 2);
            int y = _radarHost.Padding.Top + Math.Max(0, (availableHeight - side) / 2);
            _radarContainer.Bounds = new Rectangle(x, y, side, side);
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (keyData == Keys.F11)
            {
                ToggleFullScreen();
                return true;
            }
            if (keyData == Keys.H)
            {
                _settings.OrientationMode = _settings.OrientationMode == RadarOrientationMode.NorthUp
                    ? RadarOrientationMode.HeadingUp
                    : RadarOrientationMode.NorthUp;
                ApplySettingsToUi();
                return true;
            }
            if (keyData == (Keys.Control | Keys.T))
            {
                InjectTestPacket();
                return true;
            }
            if (keyData == (Keys.Control | Keys.R))
            {
                ReturnToStartScreen();
                return true;
            }
            return base.ProcessCmdKey(ref msg, keyData);
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            _uiTimer.Stop();
            _alarmTimer.Stop();
            _uiTimer.Dispose();
            _alarmTimer.Dispose();
            _dataSource.Dispose();
            base.OnFormClosed(e);
        }

        private sealed class FilterItem
        {
            public int Value { get; }
            public string Text { get; }
            public FilterItem(int value, string text) { Value = value; Text = text; }
            public override string ToString() => Text;
        }


        private void InjectTestPacket(string packet)
        {
            AppendMonitorMessage("[TEST] Добавлен тестовый пакет");
            RegisterPacket("TEST", packet);
            HandleLineReceived("TEST", packet);
            _statusLabel.Text = "Добавлен тестовый пакет: " + (_settings.InputFormat == InputFormat.Auto ? "Auto" : _settings.InputFormat.ToString());
            _rightTabs.SelectedIndex = 0;
        }

        private void AppendMonitorMessage(string message)
        {
            if (InvokeRequired)
            {
                BeginInvoke(new Action<string>(AppendMonitorMessage), message);
                return;
            }

            if (string.IsNullOrWhiteSpace(message))
                return;

            if (_mutedOutputSources.Contains(GetSourceKind(message)))
                return;

            var lines = _monitorList.Lines.ToList();
            lines.Add($"{DateTime.Now:HH:mm:ss}  {message}");
            while (lines.Count > MaxPacketHistory)
                lines.RemoveAt(0);
            _monitorList.Lines = lines.ToArray();
            ScrollToBottom(_monitorList);
        }

        private sealed class DecodedViewItem
        {
            public DateTime Timestamp { get; set; }
            public string Source { get; set; }
            public uint Address { get; set; }
            public string Summary { get; set; }
            public string Details { get; set; }
            public override string ToString() => Summary;
        }

        private sealed class PacketViewItem
        {
            public DateTime Timestamp { get; set; }
            public string Source { get; set; }
            public string RawLine { get; set; }
            public string ParsedDetails { get; set; }
            public override string ToString() => $"{Timestamp:HH:mm:ss}  [{Source}] {RawLine}";
        }
    }
}
