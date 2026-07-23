using System;
using FlightRadarPc.Models;

namespace FlightRadarPc.Services
{
    public static class Gdl90Parser
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

            string hex = NormalizeHex(line);
            if (string.IsNullOrWhiteSpace(hex))
            {
                error = "Не GDL90 hex";
                return false;
            }

            byte[] frame;
            try
            {
                frame = HexToBytes(hex);
            }
            catch (Exception ex)
            {
                error = ex.Message;
                return false;
            }

            if (frame.Length < 2 || frame[0] != 0x7E || frame[frame.Length - 1] != 0x7E)
            {
                error = "Кадр GDL90 должен начинаться и заканчиваться 7E";
                return false;
            }

            var payload = Unescape(frame, 1, frame.Length - 2);
            if (payload.Length < 28)
            {
                error = "Слишком короткий GDL90 кадр";
                return false;
            }

            byte messageId = payload[0];
            if (messageId != 0x14 && messageId != 0x1E)
            {
                error = "Это не Traffic Report GDL90";
                return false;
            }

            try
            {
                uint address = (uint)((payload[2] << 16) | (payload[3] << 8) | payload[4]);
                double lat = Decode24BitCoord(payload[5], payload[6], payload[7]);
                double lon = Decode24BitCoord(payload[8], payload[9], payload[10]);
                int altitudeFt = ((payload[11] << 4) | (payload[12] >> 4)) * 25 - 1000;
                int heading = (int)Math.Round(payload[17] * 360.0 / 256.0) % 360;
                int hVel = ((payload[15] & 0x0F) << 8) | payload[16];
                string callsign = DecodeCallsign(payload, 19, 8);
                int type = payload.Length > 27 ? payload[27] : 0;

                aircraft = new AircraftTarget
                {
                    Address = address,
                    Callsign = string.IsNullOrWhiteSpace(callsign) ? address.ToString("X6") : callsign,
                    AltitudeGps = (int)Math.Round(altitudeFt * 0.3048),
                    PressureAltitude = (int)Math.Round(altitudeFt * 0.3048),
                    SpeedKmh = (int)Math.Round(hVel * 1.852),
                    CourseDeg = heading,
                    VerticalRate = 0,
                    Latitude = lat,
                    Longitude = lon,
                    AircraftType = MapEmitter(type),
                    SignalSource = 12,
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

        private static int MapEmitter(int type)
        {
            if (type == 9 || type == 10) return 2;
            if (type == 1 || type == 2 || type == 6) return 1;
            if (type == 11) return 3;
            return 0;
        }

        private static string NormalizeHex(string line)
        {
            line = line.Trim();
            if (line.StartsWith("GDL90HEX:", StringComparison.OrdinalIgnoreCase))
                line = line.Substring(9);
            line = line.Replace("-", string.Empty).Replace(" ", string.Empty);
            return line.StartsWith("7E", StringComparison.OrdinalIgnoreCase) ? line : null;
        }

        private static byte[] HexToBytes(string hex)
        {
            if (hex.Length % 2 != 0)
                throw new ArgumentException("Нечетная длина hex строки");
            var data = new byte[hex.Length / 2];
            for (int i = 0; i < data.Length; i++)
                data[i] = Convert.ToByte(hex.Substring(i * 2, 2), 16);
            return data;
        }

        private static byte[] Unescape(byte[] frame, int offset, int count)
        {
            var output = new System.Collections.Generic.List<byte>(count);
            int end = offset + count;
            for (int i = offset; i < end; i++)
            {
                if (frame[i] == 0x7D && i + 1 < end)
                {
                    i++;
                    output.Add((byte)(frame[i] ^ 0x20));
                }
                else
                {
                    output.Add(frame[i]);
                }
            }
            return output.ToArray();
        }

        private static double Decode24BitCoord(byte b1, byte b2, byte b3)
        {
            int raw = (b1 << 16) | (b2 << 8) | b3;
            if ((raw & 0x800000) != 0)
                raw -= 0x1000000;
            return raw * (180.0 / 8388608.0);
        }

        private static string DecodeCallsign(byte[] payload, int offset, int count)
        {
            int length = Math.Min(count, payload.Length - offset);
            if (length <= 0) return string.Empty;
            var chars = new char[length];
            for (int i = 0; i < length; i++)
            {
                char c = (char)payload[offset + i];
                chars[i] = char.IsControl(c) ? ' ' : c;
            }
            return new string(chars).Trim();
        }
    }
}
