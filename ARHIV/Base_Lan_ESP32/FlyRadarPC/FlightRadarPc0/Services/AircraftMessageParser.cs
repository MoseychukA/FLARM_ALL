using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public static class AircraftMessageParser
    {
        public static bool TryParse(string line, AppSettings settings, out AircraftTarget aircraft, out string error)
        {
            aircraft = null;
            error = null;

            switch (settings.InputFormat)
            {
                case InputFormat.FlyRf:
                    return FlyRfParser.TryParse(line, out aircraft, out error);
                case InputFormat.Nmea:
                    return NmeaTrafficParser.TryParse(line, settings, out aircraft, out error);
                case InputFormat.Gdl90:
                    return Gdl90Parser.TryParse(line, out aircraft, out error);
                case InputFormat.Sbs1:
                    return Sbs1Parser.TryParse(line, out aircraft, out error);
                default:
                    if (FlyRfParser.TryParse(line, out aircraft, out error)) return true;
                    if (NmeaTrafficParser.TryParse(line, settings, out aircraft, out error)) return true;
                    if (Sbs1Parser.TryParse(line, out aircraft, out error)) return true;
                    if (Gdl90Parser.TryParse(line, out aircraft, out error)) return true;
                    error = "Формат не распознан";
                    return false;
            }
        }
    }
}
