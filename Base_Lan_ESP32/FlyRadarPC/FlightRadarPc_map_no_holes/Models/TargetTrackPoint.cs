using System;
using System.Drawing;

namespace FlightRadarPc.Models
{
    public class TargetTrackPoint
    {
        public DateTime TimestampUtc { get; set; }
        public double Latitude { get; set; }
        public double Longitude { get; set; }
        public double DistanceKm { get; set; }
        public double BearingDeg { get; set; }
        public PointF RadarPoint { get; set; }
    }
}
