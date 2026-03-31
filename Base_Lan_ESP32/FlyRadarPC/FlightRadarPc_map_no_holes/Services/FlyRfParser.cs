using System;
using System.Globalization;
using System.Linq;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public static class FlyRfParser
    {
        public static bool TryParse(string line, out AircraftTarget aircraft, out string error)
        {
            aircraft = null;
            error = null;

            if (string.IsNullOrWhiteSpace(line))
            {
                error = "Пустая строка";
                return false;
            }

            line = line.Trim();
            if (!line.StartsWith("$FLYRF,"))
            {
                error = "Неизвестный тип сообщения";
                return false;
            }

            int starIndex = line.IndexOf('*');
            string payload = starIndex >= 0 ? line.Substring(0, starIndex) : line;
            var parts = payload.Split(',');

            if (parts.Length < 16)
            {
                error = $"Недостаточно полей: {parts.Length}";
                return false;
            }

            try
            {
                aircraft = new AircraftTarget
                {
                    Address = Convert.ToUInt32(parts[1], 16),
                    Squawk = ParseInt(parts[2]),
                    Callsign = parts[3].Trim(),
                    AltitudeGps = ParseInt(parts[4]),
                    PressureAltitude = ParseInt(parts[5]),
                    SpeedKmh = ParseInt(parts[6]),
                    CourseDeg = ParseInt(parts[7]),
                    VerticalRate = ParseInt(parts[8]),
                    Latitude = ParseDouble(parts[9]),
                    Longitude = ParseDouble(parts[10]),
                    AircraftType = ParseInt(parts[11]),
                    SignalSource = ParseInt(parts[12]),
                    HourMsg = ParseInt(parts[13]),
                    MinMsg = ParseInt(parts[14]),
                    ExtraHex = parts.ElementAtOrDefault(15)?.Trim(),
                    LastUpdateUtc = DateTime.UtcNow
                };
                return true;
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }

        private static int ParseInt(string value) => int.Parse(value.Trim(), CultureInfo.InvariantCulture);
        private static double ParseDouble(string value) => double.Parse(value.Trim(), CultureInfo.InvariantCulture);
    }
}
