using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;

namespace FlightRadarPc
{
    public enum InputMode { Serial, Tcp, Udp }
    public enum InputFormat { Auto, FlyRf, Nmea, Gdl90, Sbs1 }
    public enum MapProviderMode { None, OpenStreetMap, CustomTiles }
    public enum RadarOrientationMode { NorthUp, HeadingUp }

    public class AppSettings
    {
        public double LocalLatitude { get; set; } = 55.7558;
        public double LocalLongitude { get; set; } = 37.6176;
        public string LocalTimeZoneId { get; set; } = TimeZoneInfo.Local.Id;

        public InputMode InputMode { get; set; } = InputMode.Udp;
        public InputFormat InputFormat { get; set; } = InputFormat.Auto;
        public string SerialPortName { get; set; } = "COM3";
        public int SerialBaudRate { get; set; } = 115200;
        public string IpAddress { get; set; } = "127.0.0.1";
        public int Port { get; set; } = 5000;
        public bool AutoReconnect { get; set; } = true;
        public int ReconnectDelayMs { get; set; } = 1500;

        public float RadarRangeKm { get; set; } = 20f;
        public bool ShowRangeRings { get; set; } = true;
        public bool ShowTracks { get; set; } = true;
        public bool ShowTargetLabels { get; set; } = true;
        public RadarOrientationMode OrientationMode { get; set; } = RadarOrientationMode.NorthUp;
        public int OwnshipHeadingDeg { get; set; } = 0;
        public int TrackHistorySeconds { get; set; } = 180;
        public int TargetHoldSeconds { get; set; } = 8;
        public float VectorLineMaxMm { get; set; } = 12f;
        public int VectorLineMaxSpeedKmh { get; set; } = 1200;
        public bool FullScreenAtStartup { get; set; } = false;
        public bool ShowMapBackground { get; set; } = true;
        public MapProviderMode MapProviderMode { get; set; } = MapProviderMode.OpenStreetMap;
        public string MapTilesPath { get; set; } = "MapCache";
        public string MapUrlTemplate { get; set; } = "https://tile.openstreetmap.org/{z}/{x}/{y}.png";
        public int PreferredMapZoom { get; set; } = 0;

        public bool AlarmEnabled { get; set; } = true;
        public double WarningDistanceKm { get; set; } = 3.0;
        public int WarningAltitudeMeters { get; set; } = 300;
        public double DangerDistanceKm { get; set; } = 1.0;
        public int DangerAltitudeMeters { get; set; } = 150;
        public bool PlayAlarmSound { get; set; } = false;

        public HashSet<int> VisibleAircraftTypes { get; set; } = new HashSet<int>();
        public HashSet<int> VisibleSignalSources { get; set; } = new HashSet<int>();
        public bool HideUnknownType { get; set; } = false;
        public double MaxVisibleDistanceKm { get; set; } = 25;
        public int MinVisibleAltitudeMeters { get; set; } = 0;
        public int MaxVisibleAltitudeMeters { get; set; } = 10000;
        public bool ShowOnlyAlertTargets { get; set; } = false;
        public bool ShowOnlyMovingTargets { get; set; } = false;

        public static string SettingsPath => Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "settings.json");

        public void Normalize()
        {
            MaxVisibleDistanceKm = MaxVisibleDistanceKm <= 0 ? 25 : MaxVisibleDistanceKm;
            MinVisibleAltitudeMeters = MinVisibleAltitudeMeters < 0 ? 0 : MinVisibleAltitudeMeters;
            MaxVisibleAltitudeMeters = MaxVisibleAltitudeMeters <= 0 ? 10000 : MaxVisibleAltitudeMeters;
            RadarRangeKm = RadarRangeKm <= 0 ? 20f : RadarRangeKm;
            TrackHistorySeconds = TrackHistorySeconds < 10 ? 180 : TrackHistorySeconds;
            TargetHoldSeconds = TargetHoldSeconds < 5 ? 5 : TargetHoldSeconds;
            VectorLineMaxMm = VectorLineMaxMm < 2f ? 2f : (VectorLineMaxMm > 30f ? 30f : VectorLineMaxMm);
            VectorLineMaxSpeedKmh = VectorLineMaxSpeedKmh < 50 ? 1200 : (VectorLineMaxSpeedKmh > 5000 ? 5000 : VectorLineMaxSpeedKmh);
            ReconnectDelayMs = ReconnectDelayMs < 250 ? 1500 : ReconnectDelayMs;
            SerialBaudRate = SerialBaudRate <= 0 ? 115200 : SerialBaudRate;
            Port = Port <= 0 ? 5000 : Port;
            SerialPortName = string.IsNullOrWhiteSpace(SerialPortName) ? "COM3" : SerialPortName.Trim();
            IpAddress = string.IsNullOrWhiteSpace(IpAddress) ? "127.0.0.1" : IpAddress.Trim();
            LocalTimeZoneId = string.IsNullOrWhiteSpace(LocalTimeZoneId) ? TimeZoneInfo.Local.Id : LocalTimeZoneId;
            MapTilesPath = string.IsNullOrWhiteSpace(MapTilesPath) ? "MapCache" : MapTilesPath.Trim();
            MapUrlTemplate = string.IsNullOrWhiteSpace(MapUrlTemplate) ? "https://tile.openstreetmap.org/{z}/{x}/{y}.png" : MapUrlTemplate.Trim();
            VisibleAircraftTypes = VisibleAircraftTypes ?? new HashSet<int>();
            VisibleSignalSources = VisibleSignalSources ?? new HashSet<int>();
            if (MapProviderMode == MapProviderMode.None && ShowMapBackground)
                MapProviderMode = MapProviderMode.OpenStreetMap;
        }

        public static AppSettings Load()
        {
            AppSettings settings;
            bool shouldSave = false;
            try
            {
                if (!File.Exists(SettingsPath))
                {
                    settings = new AppSettings();
                    shouldSave = true;
                }
                else
                {
                    var json = File.ReadAllText(SettingsPath);
                    settings = JsonSerializer.Deserialize<AppSettings>(json) ?? new AppSettings();
                    shouldSave = true;
                }
            }
            catch
            {
                settings = new AppSettings();
                shouldSave = true;
            }

            settings.Normalize();
            if (shouldSave)
            {
                try { settings.Save(); } catch { }
            }
            return settings;
        }

        public void Save()
        {
            Normalize();
            var json = JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(SettingsPath, json);
        }

        public DateTime GetLocalTime()
        {
            try
            {
                var tz = TimeZoneInfo.FindSystemTimeZoneById(LocalTimeZoneId);
                return TimeZoneInfo.ConvertTime(DateTime.UtcNow, tz);
            }
            catch
            {
                return DateTime.Now;
            }
        }
    }
}
