using System.Collections.Generic;
using System.Linq;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public class TargetFilterService
    {
        public IReadOnlyList<AircraftTarget> Apply(IEnumerable<AircraftTarget> targets, AppSettings settings)
        {
            var source = (targets ?? Enumerable.Empty<AircraftTarget>()).Where(t => t != null).ToList();
            if (settings == null)
                return source.OrderBy(t => t.DistanceKm).ToList();

            int minAltitude = settings.MinVisibleAltitudeMeters;
            int maxAltitude = settings.MaxVisibleAltitudeMeters;
            if (maxAltitude < minAltitude)
            {
                var temp = minAltitude;
                minAltitude = maxAltitude;
                maxAltitude = temp;
            }

            double maxDistance = settings.MaxVisibleDistanceKm <= 0 ? double.MaxValue : settings.MaxVisibleDistanceKm;
            var result = new List<AircraftTarget>();

            foreach (var t in source)
            {
                if (t.IsTestTarget || string.Equals(t.Callsign, "TEST01"))
                {
                    result.Add(t);
                    continue;
                }

                if (settings.HideUnknownType && t.AircraftType == 0)
                    continue;

                if (settings.VisibleAircraftTypes != null && settings.VisibleAircraftTypes.Count > 0 && !settings.VisibleAircraftTypes.Contains(t.AircraftType))
                    continue;

                if (settings.VisibleSignalSources != null && settings.VisibleSignalSources.Count > 0 && !settings.VisibleSignalSources.Contains(t.SignalSource))
                    continue;

                if (t.DistanceKm > maxDistance)
                    continue;

                if (t.AltitudeGps < minAltitude || t.AltitudeGps > maxAltitude)
                    continue;

                if (settings.ShowOnlyAlertTargets && t.AlertLevel == AlertLevel.None)
                    continue;

                if (settings.ShowOnlyMovingTargets && t.SpeedKmh <= 5)
                    continue;

                result.Add(t);
            }

            return result.OrderBy(t => t.DistanceKm).ToList();
        }
    }
}
