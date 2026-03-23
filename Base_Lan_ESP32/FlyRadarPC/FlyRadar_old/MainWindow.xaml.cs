using System;
using System.Collections.ObjectModel;
using System.Windows;
using System.Windows.Media;
using System.Windows.Shapes;
using FlyRadar.Models;
using FlyRadar.Services;
using FlyRadar.Utils;
using System.Windows.Threading;

namespace FlyRadar 
{
    public partial class MainWindow : Window
    {
        private readonly ObservableCollection<AircraftInfo> _aircrafts = new();
        private readonly DispatcherTimer _refreshTimer = new();

        // Выбор службы (можно переключать по настройкам)
        private SerialService? _serial;
        private TcpService? _tcp;
        private UdpService? _udp;

        // Параметры карты (центр)
        private double _centerLat = 55.7558;   // Поменяйте в настройках
        private double _centerLon = 37.6173;

        private const double RadarRadius = 300;   // пикселей

        public MainWindow()
        {
            InitializeComponent();
            DrawRadarBackground();

            // Таймер обновления UI (30 FPS достаточно)
            _refreshTimer.Interval = TimeSpan.FromMilliseconds(33);
            _refreshTimer.Tick += (_, __) => RefreshAircrafts();
            _refreshTimer.Start();

            // Пример инициализации (COM‑порт)
            // InitSerial("COM3", 115200);
        }

        #region === Рисование фона ===
        private void DrawRadarBackground()
        {
            RadarCanvas.Children.Clear();

            // Большой круг
            var outer = new Ellipse
            {
                Width = RadarRadius * 2,
                Height = RadarRadius * 2,
                Stroke = Brushes.Green,
                StrokeThickness = 2
            };
            Canvas.SetLeft(outer, RadarCanvas.Width / 2 - RadarRadius);
            Canvas.SetTop(outer, RadarCanvas.Height / 2 - RadarRadius);
            RadarCanvas.Children.Add(outer);

            // Деления каждые 10° и 30°
            for (int deg = 0; deg < 360; deg += 10)
            {
                double rad = deg * Math.PI / 180.0;
                double inner = (deg % 30 == 0) ? RadarRadius - 20 : RadarRadius - 10;
                double x1 = RadarCanvas.Width / 2 + inner * Math.Sin(rad);
                double y1 = RadarCanvas.Height / 2 - inner * Math.Cos(rad);
                double x2 = RadarCanvas.Width / 2 + RadarRadius * Math.Sin(rad);
                double y2 = RadarCanvas.Height / 2 - RadarRadius * Math.Cos(rad);

                var line = new Line
                {
                    X1 = x1, Y1 = y1,
                    X2 = x2, Y2 = y2,
                    Stroke = Brushes.Green,
                    StrokeThickness = (deg % 30 == 0) ? 2 : 1
                };
                RadarCanvas.Children.Add(line);
            }

            // Стороны света
            AddLabel("N", 0);
            AddLabel("E", 90);
            AddLabel("S", 180);
            AddLabel("W", 270);
        }

        private void AddLabel(string txt, double bearingDeg)
        {
            var tb = new System.Windows.Controls.TextBlock
            {
                Text = txt,
                Foreground = Brushes.Lime,
                FontWeight = FontWeights.Bold
            };
            double rad = bearingDeg * Math.PI / 180.0;
            double x = RadarCanvas.Width / 2 + (RadarRadius - 30) * Math.Sin(rad);
            double y = RadarCanvas.Height / 2 - (RadarRadius - 30) * Math.Cos(rad);
            Canvas.SetLeft(tb, x - 8);
            Canvas.SetTop(tb, y - 8);
            RadarCanvas.Children.Add(tb);
        }
        #endregion

        #region === Приём данных ===
        private void InitSerial(string port, int baud = 115200)
        {
            _serial = new SerialService(port, baud);
            _serial.LineReceived += OnLineReceived;
            _serial.Start();
        }

        private void InitTcp(string ip, int port)
        {
            _tcp = new TcpService(ip, port);
            _tcp.LineReceived += OnLineReceived;
            _tcp.Start();
        }

        private void InitUdp(int listenPort)
        {
            _udp = new UdpService(listenPort);
            _udp.LineReceived += OnLineReceived;
            _udp.Start();
        }

        private void OnLineReceived(string line)
        {
            if (NmeaParser.TryParse(line, out var ac) && ac != null)
            {
                // Обновляем или добавляем в коллекцию в UI‑потоке
                Dispatcher.Invoke(() => UpdateOrAddAircraft(ac));
            }
        }

        private void UpdateOrAddAircraft(AircraftInfo incoming)
        {
            var existing = FindAircraft(incoming.Addr);
            if (existing == null)
                _aircrafts.Add(incoming);
            else
                CopyValues(incoming, existing);
        }

        private AircraftInfo? FindAircraft(uint addr) =>
            System.Linq.Enumerable.FirstOrDefault(_aircrafts, a => a.Addr == addr);

        private void CopyValues(AircraftInfo src, AircraftInfo dst)
        {
            dst.Squawk = src.Squawk;
            dst.Callsign = src.Callsign;
            dst.Altitude = src.Altitude;
            dst.PressureAltitude = src.PressureAltitude;
            dst.Speed = src.Speed;
            dst.Course = src.Course;
            dst.VertRate = src.VertRate;
            dst.Latitude = src.Latitude;
            dst.Longitude = src.Longitude;
            dst.AircraftType = src.AircraftType;
            dst.SignalSource = src.SignalSource;
            dst.HourMsg = src.HourMsg;
            dst.MinMsg = src.MinMsg;
        }
        #endregion

        #region === Обновление отображения ===
        private void RefreshAircrafts()
        {
            // Очистим старые графики (исключая фон)
            RadarCanvas.Children.RemoveRange(1, RadarCanvas.Children.Count - 1);

            // Перерисуем фон (если нужно) – можно вынести в отдельный слой
            DrawRadarBackground();

            foreach (var ac in _aircrafts)
            {
                // Преобразуем GPS → локальные координаты в полярную систему
                (double x, double y) = GeoToCanvas(ac.Latitude, ac.Longitude);
                ac.ScreenX = x;
                ac.ScreenY = y;

                // Символ самолёта (можно заменить на Image)
                var plane = new Polygon
                {
                    Points = new PointCollection
                    {
                        new Point(0, -8), new Point(4, 8), new Point(-4, 8)
                    },
                    Fill = Brushes.Yellow,
                    RenderTransform = new RotateTransform(ac.Course, 0, 0)
                };
                Canvas.SetLeft(plane, x);
                Canvas.SetTop(plane, y);
                RadarCanvas.Children.Add(plane);

                // Текстовый индикатор (полётный номер / высота)
                var txt = new System.Windows.Controls.TextBlock
                {
                    Text = $"{ac.Callsign}\n{ac.Altitude} м",
                    Foreground = Brushes.White,
                    FontSize = 10
                };
                Canvas.SetLeft(txt, x + 6);
                Canvas.SetTop(txt, y - 6);
                RadarCanvas.Children.Add(txt);
            }
        }

        /// <summary>
        /// Перевод географических координат в экранные координаты относительно центра.
        /// </summary>
        /// <remarks>
        /// Для простоты используем проекцию «эциентрическую» (маленькие углы).  
        /// $$\Delta\phi = (\text{lat} - \text{centerLat})\times \frac{\pi}{180}$$  
        /// $$\Delta\lambda = (\text{lon} - \text{centerLon})\times \frac{\pi}{180}$$  
        /// $$x = R \cdot \Delta\lambda \cdot \cos(\text{centerLat})$$  
        /// $$y = R \cdot \Delta\phi$$  
        /// где $R$ – масштаб в пикселях/радиан (подбирается опытным путём).
        /// </remarks>
        private (double X, double Y) GeoToCanvas(double lat, double lon)
        {
            const double scale = 3000; // пикселей на радиан (настраивается)
            double dLat = (lat - _centerLat) * Math.PI / 180.0;
            double dLon = (lon - _centerLon) * Math.PI / 180.0;

            double x = RadarCanvas.Width / 2  + scale * dLon * Math.Cos(_centerLat * Math.PI / 180.0);
            double y = RadarCanvas.Height / 2 - scale * dLat;
            return (x, y);
        }
        #endregion

        #region === Меню настроек ===
        private void OpenSettings_Click(object sender, RoutedEventArgs e)
        {
            var dlg = new SettingsWindow
            {
                Owner = this,
                CenterLatitude = _centerLat,
                CenterLongitude = _centerLon
            };
            if (dlg.ShowDialog() == true)
            {
                // Сохраняем новые параметры
                _centerLat = dlg.CenterLatitude;
                _centerLon = dlg.CenterLongitude;

                // Перезапускаем выбранный канал при необходимости
                // (реализация в SettingsWindow)
                DrawRadarBackground(); // перерисовать фон с новым центром
            }
        }
        #endregion
    }
}