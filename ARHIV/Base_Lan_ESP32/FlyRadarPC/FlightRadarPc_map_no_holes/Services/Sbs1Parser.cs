using System;
using System.Globalization;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public static class Sbs1Parser
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
            if (!line.StartsWith("MSG,"))
            {
                error = "Не SBS-1";
                return false;
            }

            string[] parts = line.Split(',');
            if (parts.Length < 22)
            {
                error = "Недостаточно полей SBS-1";
                return false;
            }

            try
            {
                string addrHex = parts[4].Trim();
                double lat = ParseDouble(parts[14]);
                double lon = ParseDouble(parts[15]);
                if (lat == 0 && lon == 0)
                {
                    error = "В SBS-1 нет координат";
                    return false;
                }

                aircraft = new AircraftTarget
                {
                    Address = ParseHexOrStable(addrHex),
                    Callsign = parts[10].Trim(),
                    AltitudeGps = ParseInt(parts[11]),
                    PressureAltitude = ParseInt(parts[11]),
                    SpeedKmh = (int)Math.Round(ParseDouble(parts[12]) * 1.852),
                    CourseDeg = ParseInt(parts[13]),
                    VerticalRate = ParseInt(parts[16]),
                    Latitude = lat,
                    Longitude = lon,
                    Squawk = ParseInt(parts[17]),
                    AircraftType = 1,
                    SignalSource = 11,
                    LastUpdateUtc = DateTime.UtcNow,
                    ExtraHex = line
                };
                return true;
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }

        private static uint ParseHexOrStable(string value)
        {
            uint parsed;
            if (uint.TryParse(value, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out parsed))
                return parsed;
            return GeoHelper.StableAddressFromString(value);
        }

        private static int ParseInt(string value)
        {
            if (string.IsNullOrWhiteSpace(value)) return 0;
            int v;
            return int.TryParse(value.Trim(), NumberStyles.Integer, CultureInfo.InvariantCulture, out v) ? v : 0;
        }

        private static double ParseDouble(string value)
        {
            if (string.IsNullOrWhiteSpace(value)) return 0;
            double v;
            return double.TryParse(value.Trim(), NumberStyles.Float, CultureInfo.InvariantCulture, out v) ? v : 0;
        }
    }
}
