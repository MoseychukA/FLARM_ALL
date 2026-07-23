using System;
using System.Collections.Generic;
using System.Drawing;

namespace FlightRadarPc.Models
{
    public enum AlertLevel { None, Warning, Danger }

    public class AircraftTarget
    {
        public uint Address { get; set; }
        public int Squawk { get; set; }
        public string Callsign { get; set; }
        public int AltitudeGps { get; set; }
        public int PressureAltitude { get; set; }
        public int SpeedKmh { get; set; }
        public int CourseDeg { get; set; }
        public int VerticalRate { get; set; }
        public double Latitude { get; set; }
        public double Longitude { get; set; }
        public int AircraftType { get; set; }
        public int SignalSource { get; set; }
        public int HourMsg { get; set; }
        public int MinMsg { get; set; }
        public string ExtraHex { get; set; }
        public DateTime LastUpdateUtc { get; set; } = DateTime.UtcNow;

        public double DistanceKm { get; set; }
        public double BearingDeg { get; set; }
        public PointF RadarPoint { get; set; }
        public AlertLevel AlertLevel { get; set; }
        public bool IsTestTarget { get; set; }
        public bool IsSynthetic { get; set; }

        public List<TargetTrackPoint> Track { get; } = new List<TargetTrackPoint>();
    }
}
