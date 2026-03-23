using System;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace FlyRadar.Services
{
    public class TcpService : IDisposable
    {
        private readonly TcpClient _client;
        private readonly CancellationTokenSource _cts = new();

        public event Action<string> LineReceived = null!;

        public TcpService(string ip, int port)
        {
            _client = new TcpClient();
            _client.Connect(ip, port);
        }

        public void Start()
        {
            Task.Run(() => ReceiveLoop(_cts.Token));
        }

        private async Task ReceiveLoop(CancellationToken token)
        {
            var stream = _client.GetStream();
            var sb = new StringBuilder();
            var buffer = new byte[1024];

            while (!token.IsCancellationRequested)
            {
                int read = await stream.ReadAsync(buffer, 0, buffer.Length, token);
                if (read <= 0) break;

                sb.Append(Encoding.ASCII.GetString(buffer, 0, read));
                string all = sb.ToString();

                // разбиваем по символу перевода строки
                var lines = all.Split('\n');
                for (int i = 0; i < lines.Length - 1; i++)
                    LineReceived?.Invoke(lines[i].Trim());

                sb.Clear();
                sb.Append(lines[^1]); // оставляем неполную строку
            }
        }

        public void Dispose()
        {
            _cts.Cancel();
            _client.Close();
            _client.Dispose();
        }
    }
}