using System;
using System.Collections.Generic;
using System.Linq;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public class TargetRepository
    {
        private readonly Dictionary<uint, AircraftTarget> _targets = new Dictionary<uint, AircraftTarget>();
        private readonly object _sync = new object();

        public AircraftTarget Upsert(AircraftTarget incoming)
        {
            lock (_sync)
            {
                AircraftTarget existing;
                if (_targets.TryGetValue(incoming.Address, out existing))
                {
                    CopyValues(incoming, existing);
                    return existing;
                }

                _targets[incoming.Address] = incoming;
                return incoming;
            }
        }

        public void RemoveStale(TimeSpan maxAge)
        {
            lock (_sync)
            {
                var limit = DateTime.UtcNow - maxAge;
                var keys = _targets.Values.Where(t => t.LastUpdateUtc < limit).Select(t => t.Address).ToList();
                foreach (var key in keys)
                    _targets.Remove(key);
            }
        }

        public IReadOnlyList<AircraftTarget> GetAll()
        {
            lock (_sync)
                return _targets.Values.OrderBy(t => t.DistanceKm).ToList();
        }

        public AircraftTarget FindByAddress(uint address)
        {
            lock (_sync)
            {
                AircraftTarget target;
                return _targets.TryGetValue(address, out target) ? target : null;
            }
        }

        public void Clear()
        {
            lock (_sync)
                _targets.Clear();
        }

        private static void CopyValues(AircraftTarget source, AircraftTarget target)
        {
            target.Squawk = source.Squawk;
            target.Callsign = source.Callsign;
            target.AltitudeGps = source.AltitudeGps;
            target.PressureAltitude = source.PressureAltitude;
            target.SpeedKmh = source.SpeedKmh;
            target.CourseDeg = source.CourseDeg;
            target.VerticalRate = source.VerticalRate;
            target.Latitude = source.Latitude;
            target.Longitude = source.Longitude;
            target.AircraftType = source.AircraftType;
            target.SignalSource = source.SignalSource;
            target.HourMsg = source.HourMsg;
            target.MinMsg = source.MinMsg;
            target.ExtraHex = source.ExtraHex;
            target.LastUpdateUtc = source.LastUpdateUtc;
            target.DistanceKm = source.DistanceKm;
            target.BearingDeg = source.BearingDeg;
            target.RadarPoint = source.RadarPoint;
            target.IsTestTarget = source.IsTestTarget;
            target.IsSynthetic = source.IsSynthetic;
        }
    }
}
