using System;
using System.IO.Ports;
using System.Threading.Tasks;
using FlyRadar.Utils;

namespace FlyRadar.Services
{
    public class SerialService : IDisposable
    {
        private readonly SerialPort _port;

        public event Action<string> LineReceived = null!;

        public SerialService(string portName, int baudRate = 115200)
        {
            _port = new SerialPort(portName, baudRate) { NewLine = "\n" };
            _port.DataReceived += (_, __) => ReadAvailable();
        }

        public void Start()
        {
            if (!_port.IsOpen) _port.Open();
        }

        private void ReadAvailable()
        {
            try
            {
                string line = _port.ReadLine();
                LineReceived?.Invoke(line);
            }
            catch { /* игнорировать ошибки чтения */ }
        }

        public void Dispose()
        {
            if (_port.IsOpen) _port.Close();
            _port.Dispose();
        }
    }
}