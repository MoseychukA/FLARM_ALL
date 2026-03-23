using System;
using System.Globalization;
using System.Text.RegularExpressions;
using FlyRadar.Models;

namespace FlyRadar.Utils
{
    public static class NmeaParser
    {
        // Регекс соответствует строке $FLYRF,addr,squawk,callsign,alt,pressAlt,speed,course,vertRate,lat,lon,atype,src,hour,minute*
        private static readonly Regex FlyRfxRegex =
            new Regex(@"\$FLYRF,([0-9A-F]{6}),(\d+),([^,]*),(\d+),(\d+),(\d+),(\d+),(-?\d+),(-?\d+\.?\d*),(-?\d+\.?\d*),(\d+),(\d+),(\d+),(\d+)\*",
                      RegexOptions.Compiled);

        public static bool TryParse(string line, out AircraftInfo? info)
        {
            info = null;
            var m = FlyRfxRegex.Match(line.Trim());
            if (!m.Success) return false;

            try
            {
                info = new AircraftInfo
                {
                    Addr = Convert.ToUInt32(m.Groups[1].Value, 16),
                    Squawk = int.Parse(m.Groups[2].Value),
                    Callsign = m.Groups[3].Value.Trim(),
                    Altitude = int.Parse(m.Groups[4].Value),
                    PressureAltitude = int.Parse(m.Groups[5].Value),
                    Speed = int.Parse(m.Groups[6].Value),
                    Course = int.Parse(m.Groups[7].Value),
                    VertRate = int.Parse(m.Groups[8].Value),
                    Latitude = double.Parse(m.Groups[9].Value, CultureInfo.InvariantCulture),
                    Longitude = double.Parse(m.Groups[10].Value, CultureInfo.InvariantCulture),
                    AircraftType = int.Parse(m.Groups[11].Value),
                    SignalSource = int.Parse(m.Groups[12].Value),
                    HourMsg = int.Parse(m.Groups[13].Value),
                    MinMsg = int.Parse(m.Groups[14].Value)
                };
                return true;
            }
            catch
            {
                return false;
            }
        }
    }
}