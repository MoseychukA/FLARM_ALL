using System;
using System.Drawing;
using System.IO.Ports;
using System.Linq;
using System.Windows.Forms;

namespace FlightRadarPc.UI
{
    public class SettingsForm : Form
    {
        private readonly AppSettings _settings;
        private readonly TextBox _tbLat = new TextBox();
        private readonly TextBox _tbLon = new TextBox();
        private readonly ComboBox _cbMode = new ComboBox();
        private readonly ComboBox _cbFormat = new ComboBox();
        private readonly ComboBox _cbCom = new ComboBox();
        private readonly NumericUpDown _numBaud = new NumericUpDown();
        private readonly TextBox _tbIp = new TextBox();
        private readonly NumericUpDown _numPort = new NumericUpDown();
        private readonly ComboBox _cbTimezone = new ComboBox();
        private readonly NumericUpDown _numRange = new NumericUpDown();
        private readonly CheckBox _chkRings = new CheckBox { Text = "Показывать кольца дальности" };
        private readonly CheckBox _chkTracks = new CheckBox { Text = "Показывать треки" };
        private readonly CheckBox _chkMap = new CheckBox { Text = "Показывать карту-подложку" };
        private readonly CheckBox _chkLabels = new CheckBox { Text = "Показывать подписи целей" };
        private readonly CheckBox _chkAlarm = new CheckBox { Text = "Тревоги по сближению" };
        private readonly CheckBox _chkHideUnknown = new CheckBox { Text = "Скрыть неизвестные типы" };
        private readonly CheckBox _chkReconnect = new CheckBox { Text = "Автопереподключение" };
        private readonly NumericUpDown _numReconnectMs = new NumericUpDown();
        private readonly NumericUpDown _numTrackSeconds = new NumericUpDown();
        private readonly NumericUpDown _numHoldSeconds = new NumericUpDown();
        private readonly NumericUpDown _numVectorLineMm = new NumericUpDown();
        private readonly NumericUpDown _numVectorLineMaxSpeed = new NumericUpDown();
        private readonly NumericUpDown _numWarnDist = new NumericUpDown();
        private readonly NumericUpDown _numWarnAlt = new NumericUpDown();
        private readonly NumericUpDown _numDangerDist = new NumericUpDown();
        private readonly NumericUpDown _numDangerAlt = new NumericUpDown();
        private readonly ComboBox _cbMapProvider = new ComboBox();
        private readonly ComboBox _cbOrientation = new ComboBox();
        private readonly NumericUpDown _numOwnHeading = new NumericUpDown();
        private readonly TextBox _tbCachePath = new TextBox();
        private readonly Label _lblTime = new Label();
        private readonly Label _lblFormatHelp = new Label();
        private readonly Timer _timer = new Timer();

        public AppSettings ResultSettings { get; private set; }

        public SettingsForm(AppSettings settings)
        {
            _settings = settings;
            Text = "Настройки источника и радара";
            Width = 710;
            Height = 840;
            FormBorderStyle = FormBorderStyle.FixedDialog;
            MaximizeBox = false;
            MinimizeBox = false;
            StartPosition = FormStartPosition.CenterParent;

            var buttons = new FlowLayoutPanel
            {
                Dock = DockStyle.Bottom,
                FlowDirection = FlowDirection.RightToLeft,
                Height = 52,
                Padding = new Padding(12, 8, 12, 8)
            };
            var btnOk = new Button { Text = "OK", DialogResult = DialogResult.OK, Width = 90 };
            var btnCancel = new Button { Text = "Отмена", DialogResult = DialogResult.Cancel, Width = 90 };
            btnOk.Click += (s, e) => SaveResult();
            buttons.Controls.Add(btnOk);
            buttons.Controls.Add(btnCancel);
            Controls.Add(buttons);

            var contentPanel = new Panel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(0, 0, 0, 12)
            };
            Controls.Add(contentPanel);

            var table = new TableLayoutPanel
            {
                Dock = DockStyle.Fill,
                Padding = new Padding(12, 12, 12, 18),
                ColumnCount = 2,
                AutoScroll = true,
                AutoSize = false
            };
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 42));
            table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 58));
            contentPanel.Controls.Add(table);

            _cbMode.DropDownStyle = ComboBoxStyle.DropDownList;
            _cbMode.Items.AddRange(Enum.GetNames(typeof(InputMode)));
            _cbFormat.DropDownStyle = ComboBoxStyle.DropDownList;
            _cbFormat.Items.AddRange(Enum.GetNames(typeof(InputFormat)));
            _cbFormat.SelectedIndexChanged += (s, e) => UpdateFormatHelp();
            _cbCom.DropDownStyle = ComboBoxStyle.DropDownList;
            _cbCom.Items.AddRange(SerialPort.GetPortNames().OrderBy(x => x).Cast<object>().ToArray());
            _numBaud.Maximum = 3000000;
            _numBaud.Minimum = 1200;
            _numBaud.Increment = 1200;
            _numPort.Maximum = 65535;
            _numPort.Minimum = 1;
            _numRange.Maximum = 300;
            _numRange.Minimum = 1;
            _numTrackSeconds.Maximum = 1800;
            _numTrackSeconds.Minimum = 10;
            _numHoldSeconds.Maximum = 120;
            _numHoldSeconds.Minimum = 2;
            _numVectorLineMm.Maximum = 30;
            _numVectorLineMm.Minimum = 2;
            _numVectorLineMm.DecimalPlaces = 1;
            _numVectorLineMm.Increment = 0.5M;
            _numVectorLineMaxSpeed.Maximum = 5000;
            _numVectorLineMaxSpeed.Minimum = 50;
            _numVectorLineMaxSpeed.Increment = 50;
            _numWarnDist.DecimalPlaces = 1;
            _numWarnDist.Maximum = 50;
            _numWarnDist.Minimum = 1;
            _numWarnAlt.Maximum = 5000;
            _numWarnAlt.Minimum = 10;
            _numDangerDist.DecimalPlaces = 1;
            _numDangerDist.Maximum = 20;
            _numDangerDist.Minimum = 1;
            _numDangerAlt.Maximum = 3000;
            _numDangerAlt.Minimum = 10;
            _numReconnectMs.Maximum = 60000;
            _numReconnectMs.Minimum = 250;
            _numReconnectMs.Increment = 250;
            _cbTimezone.DropDownStyle = ComboBoxStyle.DropDownList;
            _cbTimezone.Items.AddRange(TimeZoneInfo.GetSystemTimeZones().Select(z => z.Id).Cast<object>().ToArray());
            _cbMapProvider.DropDownStyle = ComboBoxStyle.DropDownList;
            _cbMapProvider.Items.AddRange(Enum.GetNames(typeof(MapProviderMode)));
            _cbOrientation.DropDownStyle = ComboBoxStyle.DropDownList;
            _cbOrientation.Items.AddRange(Enum.GetNames(typeof(RadarOrientationMode)));
            _numOwnHeading.Minimum = 0;
            _numOwnHeading.Maximum = 359;
            _lblFormatHelp.AutoSize = true;
            _lblFormatHelp.MaximumSize = new Size(380, 0);
            _lblFormatHelp.ForeColor = Color.DarkGreen;

            int row = 0;
            AddRow(table, row++, "Локальная широта:", _tbLat);
            AddRow(table, row++, "Локальная долгота:", _tbLon);
            AddRow(table, row++, "Источник данных:", _cbMode);
            AddRow(table, row++, "Формат данных:", _cbFormat);
            AddRow(table, row++, "Подсказка по формату:", _lblFormatHelp);
            AddRow(table, row++, "COM порт:", _cbCom);
            AddRow(table, row++, "Скорость COM:", _numBaud);
            AddRow(table, row++, "IP адрес:", _tbIp);
            AddRow(table, row++, "Порт TCP/UDP:", _numPort);
            AddRow(table, row++, string.Empty, _chkReconnect);
            AddRow(table, row++, "Задержка переподкл., мс:", _numReconnectMs);
            AddRow(table, row++, "Часовой пояс:", _cbTimezone);
            AddRow(table, row++, "Радиус обзора, км:", _numRange);
            AddRow(table, row++, "Треки, сек:", _numTrackSeconds);
            AddRow(table, row++, "Удержание цели, сек:", _numHoldSeconds);
            AddRow(table, row++, "Длина линии скорости, мм:", _numVectorLineMm);
            AddRow(table, row++, "Макс. скорость для линии, км/ч:", _numVectorLineMaxSpeed);
            AddRow(table, row++, "Провайдер карты:", _cbMapProvider);
            AddRow(table, row++, "Папка кэша карт:", _tbCachePath);
            AddRow(table, row++, "Ориентация радара:", _cbOrientation);
            AddRow(table, row++, "Курс для Heading Up, °:", _numOwnHeading);
            AddRow(table, row++, "Текущее время:", _lblTime);
            AddRow(table, row++, string.Empty, _chkRings);
            AddRow(table, row++, string.Empty, _chkTracks);
            AddRow(table, row++, string.Empty, _chkMap);
            AddRow(table, row++, string.Empty, _chkLabels);
            AddRow(table, row++, string.Empty, _chkAlarm);
            AddRow(table, row++, string.Empty, _chkHideUnknown);
            AddRow(table, row++, "Предупреждение, км:", _numWarnDist);
            AddRow(table, row++, "Предупреждение, м:", _numWarnAlt);
            AddRow(table, row++, "Опасно, км:", _numDangerDist);
            AddRow(table, row++, "Опасно, м:", _numDangerAlt);

            AcceptButton = btnOk;
            CancelButton = btnCancel;

            LoadValues();

            _timer.Interval = 1000;
            _timer.Tick += (s, e) => _lblTime.Text = GetDisplayedTime();
            _timer.Start();
        }

        private void AddRow(TableLayoutPanel table, int row, string label, Control control)
        {
            while (table.RowStyles.Count <= row)
                table.RowStyles.Add(new RowStyle(SizeType.AutoSize));

            var lbl = new Label { Text = label, AutoSize = true, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft };
            control.Dock = DockStyle.Fill;
            table.Controls.Add(lbl, 0, row);
            table.Controls.Add(control, 1, row);
        }

        private void LoadValues()
        {
            _tbLat.Text = _settings.LocalLatitude.ToString(System.Globalization.CultureInfo.InvariantCulture);
            _tbLon.Text = _settings.LocalLongitude.ToString(System.Globalization.CultureInfo.InvariantCulture);
            _cbMode.SelectedItem = _settings.InputMode.ToString();
            _cbFormat.SelectedItem = _settings.InputFormat.ToString();
            if (_cbCom.Items.Count == 0)
                _cbCom.Items.Add(_settings.SerialPortName);
            _cbCom.SelectedItem = _cbCom.Items.Cast<object>().FirstOrDefault(x => x.ToString() == _settings.SerialPortName) ?? _cbCom.Items[0];
            _numBaud.Value = _settings.SerialBaudRate;
            _tbIp.Text = _settings.IpAddress;
            _numPort.Value = _settings.Port;
            _chkReconnect.Checked = _settings.AutoReconnect;
            _numReconnectMs.Value = _settings.ReconnectDelayMs;
            _numRange.Value = (decimal)_settings.RadarRangeKm;
            _numTrackSeconds.Value = _settings.TrackHistorySeconds;
            _numHoldSeconds.Value = _settings.TargetHoldSeconds;
            _numVectorLineMm.Value = (decimal)_settings.VectorLineMaxMm;
            _numVectorLineMaxSpeed.Value = _settings.VectorLineMaxSpeedKmh;
            _chkRings.Checked = _settings.ShowRangeRings;
            _chkTracks.Checked = _settings.ShowTracks;
            _chkMap.Checked = _settings.ShowMapBackground;
            _chkLabels.Checked = _settings.ShowTargetLabels;
            _chkAlarm.Checked = _settings.AlarmEnabled;
            _chkHideUnknown.Checked = _settings.HideUnknownType;
            _numWarnDist.Value = (decimal)_settings.WarningDistanceKm;
            _numWarnAlt.Value = _settings.WarningAltitudeMeters;
            _numDangerDist.Value = (decimal)_settings.DangerDistanceKm;
            _numDangerAlt.Value = _settings.DangerAltitudeMeters;
            _cbTimezone.SelectedItem = _cbTimezone.Items.Cast<object>().FirstOrDefault(x => x.ToString() == _settings.LocalTimeZoneId) ?? TimeZoneInfo.Local.Id;
            _cbMapProvider.SelectedItem = _settings.MapProviderMode.ToString();
            _cbOrientation.SelectedItem = _settings.OrientationMode.ToString();
            _numOwnHeading.Value = _settings.OwnshipHeadingDeg;
            _tbCachePath.Text = _settings.MapTilesPath;
            _lblTime.Text = GetDisplayedTime();
            UpdateFormatHelp();
        }

        private void UpdateFormatHelp()
        {
            switch (_cbFormat.SelectedItem?.ToString())
            {
                case "FlyRf":
                    _lblFormatHelp.Text = "FLYRF: ваши пакеты $FLYRF,...";
                    break;
                case "Nmea":
                    _lblFormatHelp.Text = "NMEA/FLARM: ожидается PFLAA для воздушных целей.";
                    break;
                case "Gdl90":
                    _lblFormatHelp.Text = "GDL90: для TCP/UDP ожидаются кадры 0x7E ... 0x7E. Для теста можно подавать строку GDL90HEX:7E...7E.";
                    break;
                case "Sbs1":
                    _lblFormatHelp.Text = "SBS-1 / BaseStation: ожидается строка MSG,... от dump1090 и совместимых источников.";
                    break;
                default:
                    _lblFormatHelp.Text = "Auto: программа пытается распознать FLYRF, NMEA PFLAA, SBS-1 и GDL90 автоматически.";
                    break;
            }
        }

        private string GetDisplayedTime()
        {
            try
            {
                var tzId = _cbTimezone.SelectedItem?.ToString() ?? _settings.LocalTimeZoneId;
                var tz = TimeZoneInfo.FindSystemTimeZoneById(tzId);
                return TimeZoneInfo.ConvertTime(DateTime.UtcNow, tz).ToString("dd.MM.yyyy HH:mm:ss");
            }
            catch
            {
                return DateTime.Now.ToString("dd.MM.yyyy HH:mm:ss");
            }
        }

        private void SaveResult()
        {
            ResultSettings = new AppSettings
            {
                LocalLatitude = double.Parse(_tbLat.Text, System.Globalization.CultureInfo.InvariantCulture),
                LocalLongitude = double.Parse(_tbLon.Text, System.Globalization.CultureInfo.InvariantCulture),
                InputMode = (InputMode)Enum.Parse(typeof(InputMode), _cbMode.SelectedItem?.ToString() ?? nameof(InputMode.Udp)),
                InputFormat = (InputFormat)Enum.Parse(typeof(InputFormat), _cbFormat.SelectedItem?.ToString() ?? nameof(InputFormat.Auto)),
                SerialPortName = _cbCom.SelectedItem?.ToString() ?? "COM3",
                SerialBaudRate = (int)_numBaud.Value,
                IpAddress = _tbIp.Text.Trim(),
                Port = (int)_numPort.Value,
                AutoReconnect = _chkReconnect.Checked,
                ReconnectDelayMs = (int)_numReconnectMs.Value,
                LocalTimeZoneId = _cbTimezone.SelectedItem?.ToString() ?? TimeZoneInfo.Local.Id,
                RadarRangeKm = (float)_numRange.Value,
                ShowRangeRings = _chkRings.Checked,
                ShowTracks = _chkTracks.Checked,
                ShowMapBackground = _chkMap.Checked,
                ShowTargetLabels = _chkLabels.Checked,
                MapProviderMode = (MapProviderMode)Enum.Parse(typeof(MapProviderMode), _cbMapProvider.SelectedItem?.ToString() ?? nameof(MapProviderMode.OpenStreetMap)),
                MapTilesPath = _tbCachePath.Text.Trim(),
                OrientationMode = (RadarOrientationMode)Enum.Parse(typeof(RadarOrientationMode), _cbOrientation.SelectedItem?.ToString() ?? nameof(RadarOrientationMode.NorthUp)),
                OwnshipHeadingDeg = (int)_numOwnHeading.Value,
                AlarmEnabled = _chkAlarm.Checked,
                HideUnknownType = _chkHideUnknown.Checked,
                TrackHistorySeconds = (int)_numTrackSeconds.Value,
                TargetHoldSeconds = (int)_numHoldSeconds.Value,
                VectorLineMaxMm = (float)_numVectorLineMm.Value,
                VectorLineMaxSpeedKmh = (int)_numVectorLineMaxSpeed.Value,
                WarningDistanceKm = (double)_numWarnDist.Value,
                WarningAltitudeMeters = (int)_numWarnAlt.Value,
                DangerDistanceKm = (double)_numDangerDist.Value,
                DangerAltitudeMeters = (int)_numDangerAlt.Value,
                VisibleAircraftTypes = _settings.VisibleAircraftTypes,
                VisibleSignalSources = _settings.VisibleSignalSources,
                MaxVisibleDistanceKm = _settings.MaxVisibleDistanceKm,
                MinVisibleAltitudeMeters = _settings.MinVisibleAltitudeMeters,
                MaxVisibleAltitudeMeters = _settings.MaxVisibleAltitudeMeters,
                ShowOnlyAlertTargets = _settings.ShowOnlyAlertTargets,
                ShowOnlyMovingTargets = _settings.ShowOnlyMovingTargets
            };
        }

        protected override void OnFormClosed(FormClosedEventArgs e)
        {
            _timer.Stop();
            _timer.Dispose();
            base.OnFormClosed(e);
        }
    }
}
