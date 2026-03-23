using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace FlyRadar.Services
{
    public class UdpService : IDisposable
    {
        private readonly UdpClient _udp;
        private readonly CancellationTokenSource _cts = new();

        public event Action<string> LineReceived = null!;

        public UdpService(int listenPort)
        {
            _udp = new UdpClient(listenPort);
        }

        public void Start()
        {
            Task.Run(() => ReceiveLoop(_cts.Token));
        }

        private async Task ReceiveLoop(CancellationToken token)
        {
            while (!token.IsCancellationRequested)
            {
                var result = await _udp.ReceiveAsync(token);
                string line = Encoding.ASCII.GetString(result.Buffer);
                LineReceived?.Invoke(line.Trim());
            }
        }

        public void Dispose()
        {
            _cts.Cancel();
            _udp.Close();
            _udp.Dispose();
        }
    }
}