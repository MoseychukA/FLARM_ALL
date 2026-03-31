using System;
using System.Globalization;
using System.Text.RegularExpressions;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public static class NmeaTrafficParser
    {
        public static bool TryParse(string line, AppSettings settings, out AircraftTarget aircraft, out string error)
        {
            aircraft = null;
            error = null;

            if (string.IsNullOrWhiteSpace(line))
            {
                error = "Пустая строка";
                return false;
            }

            line = line.Trim();
            if (!line.StartsWith("$PFLAA,", StringComparison.OrdinalIgnoreCase))
            {
                error = "Не PFLAA";
                return false;
            }

            int starIndex = line.IndexOf('*');
            string payload = starIndex >= 0 ? line.Substring(0, starIndex) : line;
            string[] parts = payload.Split(',');
            if (parts.Length < 7)
            {
                error = "Недостаточно полей PFLAA";
                return false;
            }

            try
            {
                int alarmLevel = ParseIntFlexible(GetPart(parts, 1));
                int relNorthMeters = ParseIntFlexible(GetPart(parts, 2));
                int relEastMeters = ParseIntFlexible(GetPart(parts, 3));
                int relVerticalMeters = ParseIntFlexible(GetPart(parts, 4));
                string idType = GetPart(parts, 5).Trim();
                string id = GetPart(parts, 6).Trim();
                int track = ParseIntFlexible(GetPart(parts, 7));
                double turnRate = ParseDoubleFlexible(GetPart(parts, 8));
                int groundSpeed = (int)Math.Round(ParseDoubleFlexible(GetPart(parts, 9)) * 3.6);
                int climb = (int)Math.Round(ParseDoubleFlexible(GetPart(parts, 10)));
                int acType = ParseIntFlexible(GetPart(parts, 11), 3);

                if (string.IsNullOrWhiteSpace(id))
                    id = $"PFLAA_{relNorthMeters}_{relEastMeters}_{relVerticalMeters}";

                var latLon = GeoHelper.OffsetLatLon(settings.LocalLatitude, settings.LocalLongitude, relNorthMeters / 1000.0, relEastMeters / 1000.0);
                var distanceBearing = GeoHelper.DistanceAndBearing(settings.LocalLatitude, settings.LocalLongitude, latLon.latitude, latLon.longitude);

                aircraft = new AircraftTarget
                {
                    Address = GeoHelper.StableAddressFromString(id),
                    Callsign = id,
                    AltitudeGps = relVerticalMeters,
                    PressureAltitude = relVerticalMeters,
                    SpeedKmh = Math.Max(0, groundSpeed),
                    CourseDeg = NormalizeCourse(track),
                    VerticalRate = climb,
                    Latitude = latLon.latitude,
                    Longitude = latLon.longitude,
                    AircraftType = MapPflAaType(acType),
                    SignalSource = 10,
                    LastUpdateUtc = DateTime.UtcNow,
                    ExtraHex = $"alarm={alarmLevel};idType={idType};turn={turnRate.ToString(CultureInfo.InvariantCulture)};raw={line}",
                    DistanceKm = distanceBearing.distanceKm,
                    BearingDeg = distanceBearing.bearingDeg
                };
                return true;
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }
        }

        private static string GetPart(string[] parts, int index)
        {
            return index >= 0 && index < parts.Length ? parts[index] ?? string.Empty : string.Empty;
        }

        private static int NormalizeCourse(int course)
        {
            if (course < 0) course %= 360;
            if (course < 0) course += 360;
            if (course >= 360) course %= 360;
            return course;
        }

        private static int MapPflAaType(int type)
        {
            switch (type)
            {
                case 1: return 3;
                case 2: return 1;
                case 3: return 2;
                case 4: return 1;
                case 5: return 1;
                case 6: return 4;
                case 7: return 5;
                case 8: return 4;
                default: return 0;
            }
        }

        private static int ParseIntFlexible(string value, int defaultValue = 0)
        {
            var number = ExtractNumericToken(value);
            if (string.IsNullOrWhiteSpace(number)) return defaultValue;
            if (int.TryParse(number, NumberStyles.Integer, CultureInfo.InvariantCulture, out var iv)) return iv;
            if (double.TryParse(number, NumberStyles.Float, CultureInfo.InvariantCulture, out var dv)) return (int)Math.Round(dv);
            return defaultValue;
        }

        private static double ParseDoubleFlexible(string value)
        {
            var number = ExtractNumericToken(value);
            if (string.IsNullOrWhiteSpace(number)) return 0;
            return double.TryParse(number, NumberStyles.Float, CultureInfo.InvariantCulture, out var dv) ? dv : 0;
        }

        private static string ExtractNumericToken(string value)
        {
            if (string.IsNullOrWhiteSpace(value)) return string.Empty;
            value = value.Trim().Replace(',', '.');
            var match = Regex.Match(value, @"[-+]?\d+(?:\.\d+)?");
            return match.Success ? match.Value : string.Empty;
        }
    }
}
