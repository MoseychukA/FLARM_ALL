using System;
using System.ComponentModel;

namespace FlyRadar.Models
{
    public class AircraftInfo : INotifyPropertyChanged
    {
        public uint Addr { get; set; }                     // $FLYRF,%06X
        public int Squawk { get; set; }                   // %d
        public string Callsign { get; set; } = string.Empty;
        public int Altitude { get; set; }                 // геоид, м
        public int PressureAltitude { get; set; }        // датчик, м
        public int Speed { get; set; }                    // km/h
        public int Course { get; set; }                   // градусы
        public int VertRate { get; set; }                 // ft/min
        public double Latitude { get; set; }              // °
        public double Longitude { get; set; }             // °
        public int AircraftType { get; set; }
        public int SignalSource { get; set; }
        public int HourMsg { get; set; }
        public int MinMsg { get; set; }

        // Позиционные свойства для UI (вычисляемые)
        private double _screenX, _screenY;
        public double ScreenX { get => _screenX; set { _screenX = value; OnPropertyChanged(nameof(ScreenX)); } }
        public double ScreenY { get => _screenY; set { _screenY = value; OnPropertyChanged(nameof(ScreenY)); } }

        public event PropertyChangedEventHandler? PropertyChanged;
        protected void OnPropertyChanged(string name) =>
            PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
    }
}