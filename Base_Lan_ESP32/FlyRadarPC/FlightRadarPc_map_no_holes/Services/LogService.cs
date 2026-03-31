using System;
using System.IO;
using System.Text;

namespace FlightRadarPc.Services
{
    public static class LogService
    {
        private static readonly object Sync = new object();
        public static string LogPath => Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "FlightRadarPc.log");

        public static void Write(string message)
        {
            try
            {
                lock (Sync)
                {
                    File.AppendAllText(LogPath,
                        $"[{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff}] {message}{Environment.NewLine}",
                        Encoding.UTF8);
                }
            }
            catch
            {
            }
        }

        public static void Write(Exception ex, string context)
        {
            if (ex == null) return;
            Write($"{context}: {ex}");
        }
    }
}
