using System;
using System.Linq;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public class TrackHistoryService
    {
        public void Append(AircraftTarget target)
        {
            target.Track.Add(new TargetTrackPoint
            {
                TimestampUtc = target.LastUpdateUtc,
                Latitude = target.Latitude,
                Longitude = target.Longitude,
                DistanceKm = target.DistanceKm,
                BearingDeg = target.BearingDeg,
                RadarPoint = target.RadarPoint
            });
        }

        public void Trim(AircraftTarget target, TimeSpan history)
        {
            var border = DateTime.UtcNow - history;
            var old = target.Track.Where(p => p.TimestampUtc < border).ToList();
            foreach (var item in old)
                target.Track.Remove(item);
        }
    }
}
