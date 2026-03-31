using System;
using System.Drawing;

namespace FlightRadarPc.Services
{
    public static class GeoHelper
    {
        private const double EarthRadiusKm = 6371.0;

        public static (double distanceKm, double bearingDeg) DistanceAndBearing(double lat1, double lon1, double lat2, double lon2)
        {
            double rLat1 = ToRadians(lat1);
            double rLat2 = ToRadians(lat2);
            double dLat = ToRadians(lat2 - lat1);
            double dLon = ToRadians(lon2 - lon1);

            double a = Math.Sin(dLat / 2) * Math.Sin(dLat / 2) +
                       Math.Cos(rLat1) * Math.Cos(rLat2) * Math.Sin(dLon / 2) * Math.Sin(dLon / 2);
            double c = 2 * Math.Atan2(Math.Sqrt(a), Math.Sqrt(1 - a));
            double distance = EarthRadiusKm * c;

            double y = Math.Sin(dLon) * Math.Cos(rLat2);
            double x = Math.Cos(rLat1) * Math.Sin(rLat2) - Math.Sin(rLat1) * Math.Cos(rLat2) * Math.Cos(dLon);
            double bearing = (ToDegrees(Math.Atan2(y, x)) + 360) % 360;

            return (distance, bearing);
        }

        public static (double latitude, double longitude) OffsetLatLon(double latitude, double longitude, double northKm, double eastKm)
        {
            double dLat = northKm / 111.32;
            double dLon = eastKm / (111.32 * Math.Cos(ToRadians(latitude)));
            return (latitude + dLat, longitude + dLon);
        }

        public static uint StableAddressFromString(string value)
        {
            unchecked
            {
                uint hash = 2166136261;
                foreach (char c in value ?? string.Empty)
                {
                    hash ^= c;
                    hash *= 16777619;
                }
                return hash & 0xFFFFFF;
            }
        }

        public static PointF PolarToPoint(float centerX, float centerY, float radius, double bearingDeg, double normalizedDistance)
        {
            double angle = ToRadians(bearingDeg - 90);
            float r = (float)(radius * normalizedDistance);
            return new PointF(
                centerX + (float)(Math.Cos(angle) * r),
                centerY + (float)(Math.Sin(angle) * r));
        }

        public static PointF LatLonToPoint(double centerLat, double centerLon, double lat, double lon, float centerX, float centerY, float radiusPx, double rangeKm)
        {
            double kmPerDegLat = 111.32;
            double kmPerDegLon = 111.32 * Math.Cos(ToRadians(centerLat));
            double dxKm = (lon - centerLon) * kmPerDegLon;
            double dyKm = (centerLat - lat) * kmPerDegLat;
            double pxPerKm = radiusPx / rangeKm;
            return new PointF(centerX + (float)(dxKm * pxPerKm), centerY + (float)(dyKm * pxPerKm));
        }

        public static string GetCardinalText(double deg)
        {
            string[] dirs = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
            int index = (int)Math.Round(((deg % 360) / 45), MidpointRounding.AwayFromZero) % 8;
            return dirs[index];
        }

        public static string ResolveAircraftTypeName(int type)
        {
            switch (type)
            {
                case 0: return "Неизв.";
                case 1: return "Самолет";
                case 2: return "Вертолет";
                case 3: return "Планер";
                case 4: return "БПЛА";
                case 5: return "Наземный";
                default: return "Тип " + type;
            }
        }

        public static string ResolveSignalSourceName(int source)
        {
            switch (source)
            {
                case 0: return "LoRa";
                case 1: return "DUMP1090";
                case 2: return "GSM/Iridium";
                case 10: return "FLARM NMEA";
                case 11: return "ADS-B SBS";
                case 12: return "ADS-B GDL90";
                default: return "Src " + source;
            }
        }

        private static double ToRadians(double degrees) => Math.PI * degrees / 180.0;
        private static double ToDegrees(double radians) => 180.0 * radians / Math.PI;
    }
}
