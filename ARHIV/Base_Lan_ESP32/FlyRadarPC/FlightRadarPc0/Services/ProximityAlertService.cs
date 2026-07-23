using System.Collections.Generic;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public class ProximityAlertService
    {
        public IReadOnlyList<AircraftTarget> Evaluate(IEnumerable<AircraftTarget> targets, AppSettings settings)
        {
            var result = new List<AircraftTarget>();
            foreach (var target in targets)
            {
                if (!settings.AlarmEnabled)
                {
                    target.AlertLevel = AlertLevel.None;
                    continue;
                }

                int altitudeDelta = System.Math.Abs(target.AltitudeGps);
                if (target.DistanceKm <= settings.DangerDistanceKm && altitudeDelta <= settings.DangerAltitudeMeters)
                    target.AlertLevel = AlertLevel.Danger;
                else if (target.DistanceKm <= settings.WarningDistanceKm && altitudeDelta <= settings.WarningAltitudeMeters)
                    target.AlertLevel = AlertLevel.Warning;
                else
                    target.AlertLevel = AlertLevel.None;

                if (target.AlertLevel != AlertLevel.None)
                    result.Add(target);
            }
            return result;
        }
    }
}
