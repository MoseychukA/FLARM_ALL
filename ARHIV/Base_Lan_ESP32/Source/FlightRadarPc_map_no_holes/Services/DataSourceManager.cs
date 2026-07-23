using System;
using System.IO;
using System.IO.Ports;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace FlightRadarPc.Services
{
    public class DataSourceManager : IDisposable
    {
        private CancellationTokenSource _serialCts;
        private CancellationTokenSource _tcpCts;
        private CancellationTokenSource _udpCts;
        private SerialPort _serialPort;
        private TcpClient _tcpClient;
        private UdpClient _udpClient;
        private SynchronizationContext _syncContext;

        public event Action<string> LineReceived;
        public event Action<string, string> PacketReceived;
        public event Action<string> StatusChanged;
        public event Action<string> MonitorMessage;
        public bool IsRunning => IsSerialRunning || IsTcpRunning || IsUdpRunning;
        public bool IsSerialRunning => _serialCts != null && !_serialCts.IsCancellationRequested;
        public bool IsTcpRunning => _tcpCts != null && !_tcpCts.IsCancellationRequested;
        public bool IsUdpRunning => _udpCts != null && !_udpCts.IsCancellationRequested;

        public void Start(AppSettings settings)
        {
            _syncContext = SynchronizationContext.Current;
            StartSerialSource(settings);
            StartTcpSource(settings);
            StartUdpSource(settings);
            RaiseStatus($"Прием активен: COM+TCP+UDP / формат {settings.InputFormat}");
            RaiseMonitor($"[START] Одновременный запуск COM {settings.SerialPortName}, TCP {settings.IpAddress}:{settings.Port}, UDP {settings.IpAddress}:{settings.Port}");
        }

        public void StartSerialSource(AppSettings settings)
        {
            _syncContext = SynchronizationContext.Current;
            StopSerialSource();
            _serialCts = new CancellationTokenSource();
            RaiseMonitor($"[COM {settings.SerialPortName}] Старт");
            StartSerial(settings, _serialCts.Token);
            RaiseCompositeStatus(settings);
        }

        public void StartTcpSource(AppSettings settings)
        {
            _syncContext = SynchronizationContext.Current;
            StopTcpSource();
            _tcpCts = new CancellationTokenSource();
            RaiseMonitor($"[TCP {settings.IpAddress}:{settings.Port}] Старт");
            StartTcpClient(settings, _tcpCts.Token);
            RaiseCompositeStatus(settings);
        }

        public void StartUdpSource(AppSettings settings)
        {
            _syncContext = SynchronizationContext.Current;
            StopUdpSource();
            _udpCts = new CancellationTokenSource();
            RaiseMonitor($"[UDP {settings.IpAddress}:{settings.Port}] Старт");
            StartUdpListener(settings, _udpCts.Token);
            RaiseCompositeStatus(settings);
        }

        public void StopSerialSource()
        {
            try { _serialCts?.Cancel(); } catch { }
            try { _serialPort?.Close(); } catch { }
            try { _serialPort?.Dispose(); } catch { }
            _serialPort = null;
            _serialCts = null;
        }

        public void StopTcpSource()
        {
            try { _tcpCts?.Cancel(); } catch { }
            try { _tcpClient?.Close(); } catch { }
            try { _tcpClient?.Dispose(); } catch { }
            _tcpClient = null;
            _tcpCts = null;
        }

        public void StopUdpSource()
        {
            try { _udpCts?.Cancel(); } catch { }
            try { _udpClient?.Close(); } catch { }
            try { _udpClient?.Dispose(); } catch { }
            _udpClient = null;
            _udpCts = null;
        }

        public void Stop()
        {
            StopSerialSource();
            StopTcpSource();
            StopUdpSource();
            RaiseStatus("Источник остановлен");
        }


        private void RaiseCompositeStatus(AppSettings settings)
        {
            var parts = new StringBuilder();
            if (IsSerialRunning) parts.Append($"COM {settings.SerialPortName} ");
            if (IsTcpRunning) parts.Append($"TCP {settings.IpAddress}:{settings.Port} ");
            if (IsUdpRunning) parts.Append($"UDP {settings.IpAddress}:{settings.Port} ");
            var body = parts.ToString().Trim();
            RaiseStatus(string.IsNullOrWhiteSpace(body) ? "Источник остановлен" : $"Прием активен: {body} / формат {settings.InputFormat}");
        }

        private void StartSerial(AppSettings settings, CancellationToken token)
        {
            Task.Run(() =>
            {
                string sourceTag = $"COM {settings.SerialPortName}";
                while (!token.IsCancellationRequested)
                {
                    try
                    {
                        try { _serialPort?.Dispose(); } catch { }
                        _serialPort = new SerialPort(settings.SerialPortName, settings.SerialBaudRate)
                        {
                            NewLine = "\n",
                            Encoding = Encoding.ASCII,
                            ReadTimeout = 250,
                            DtrEnable = false,
                            RtsEnable = false,
                            Handshake = Handshake.None
                        };
                        _serialPort.Open();
                        _serialPort.DiscardInBuffer();
                        RaiseMonitor($"[{sourceTag}] Открыт прием @ {settings.SerialBaudRate}, формат {settings.InputFormat}");

                        var lineBuffer = new StringBuilder(4096);
                        while (!token.IsCancellationRequested && _serialPort != null && _serialPort.IsOpen)
                        {
                            try
                            {
                                int bytesToRead = _serialPort.BytesToRead;
                                if (bytesToRead <= 0)
                                {
                                    Thread.Sleep(15);
                                    continue;
                                }

                                string chunk = _serialPort.ReadExisting();
                                if (string.IsNullOrEmpty(chunk))
                                {
                                    Thread.Sleep(5);
                                    continue;
                                }

                                lineBuffer.Append(chunk);
                                DrainTextBuffer(lineBuffer, sourceTag);
                            }
                            catch (TimeoutException)
                            {
                            }
                        }

                        if (lineBuffer.Length > 0)
                        {
                            string tail = lineBuffer.ToString().Trim();
                            if (!string.IsNullOrWhiteSpace(tail))
                                RaiseLine(tail, sourceTag);
                        }
                    }
                    catch (UnauthorizedAccessException ex)
                    {
                        string ports = string.Join(", ", SerialPort.GetPortNames());
                        RaiseMonitor($"[{sourceTag}] ОШИБКА: COM-порт занят или закрыт другим приложением. Доступные порты: {(string.IsNullOrWhiteSpace(ports) ? "не найдены" : ports)}. {ex.Message}");
                        SleepReconnect(settings, token);
                    }
                    catch (IOException ex)
                    {
                        RaiseMonitor($"[{sourceTag}] ОШИБКА чтения: {ex.Message}");
                        SleepReconnect(settings, token);
                    }
                    catch (InvalidOperationException ex)
                    {
                        RaiseMonitor($"[{sourceTag}] ОШИБКА состояния: {ex.Message}");
                        SleepReconnect(settings, token);
                    }
                    catch (Exception ex)
                    {
                        RaiseMonitor($"[{sourceTag}] ОШИБКА: {ex.Message}");
                        SleepReconnect(settings, token);
                    }
                    finally
                    {
                        try { _serialPort?.Close(); } catch { }
                    }
                }
            }, token);
        }

        private void StartTcpClient(AppSettings settings, CancellationToken token)
        {
            Task.Run(async () =>
            {
                string sourceTag = $"TCP {settings.IpAddress}:{settings.Port}";
                while (!token.IsCancellationRequested)
                {
                    try
                    {
                        _tcpClient?.Dispose();
                        _tcpClient = new TcpClient { NoDelay = true };
                        RaiseMonitor($"[{sourceTag}] Подключение...");
                        await _tcpClient.ConnectAsync(settings.IpAddress, settings.Port).ConfigureAwait(false);
                        RaiseMonitor($"[{sourceTag}] Подключено, формат {settings.InputFormat}");

                        using (var stream = _tcpClient.GetStream())
                        {
                            if (settings.InputFormat == InputFormat.Gdl90)
                                await ReadGdl90FramesAsync(stream, token, sourceTag).ConfigureAwait(false);
                            else
                                await ReadTextLinesAsync(stream, token, sourceTag).ConfigureAwait(false);
                        }
                    }
                    catch (ObjectDisposedException)
                    {
                    }
                    catch (Exception ex)
                    {
                        RaiseMonitor($"[{sourceTag}] ОШИБКА: {ex.Message}");
                        await DelayReconnect(settings, token).ConfigureAwait(false);
                    }
                }
            }, token);
        }

        private void StartUdpListener(AppSettings settings, CancellationToken token)
        {
            Task.Run(async () =>
            {
                string sourceTag = $"UDP {settings.IpAddress}:{settings.Port}";
                try
                {
                    var ip = string.IsNullOrWhiteSpace(settings.IpAddress) || settings.IpAddress == "0.0.0.0"
                        ? IPAddress.Any
                        : IPAddress.Parse(settings.IpAddress);
                    _udpClient = new UdpClient();
                    _udpClient.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                    _udpClient.ExclusiveAddressUse = false;
                    _udpClient.Client.Bind(new IPEndPoint(ip, settings.Port));
                    sourceTag = $"UDP {ip}:{settings.Port}";
                    RaiseMonitor($"[{sourceTag}] Ожидание пакетов, формат {settings.InputFormat}");
                }
                catch (Exception ex)
                {
                    RaiseMonitor($"[{sourceTag}] ОШИБКА запуска: {ex.Message}");
                    return;
                }

                while (!token.IsCancellationRequested)
                {
                    try
                    {
                        var result = await _udpClient.ReceiveAsync().ConfigureAwait(false);
                        string resultTag = $"UDP {result.RemoteEndPoint.Address}:{result.RemoteEndPoint.Port}";
                        if (settings.InputFormat == InputFormat.Gdl90)
                        {
                            EmitGdl90Frames(result.Buffer, resultTag);
                        }
                        else
                        {
                            string data = Encoding.ASCII.GetString(result.Buffer);
                            using (var sr = new StringReader(data))
                            {
                                string line;
                                while ((line = sr.ReadLine()) != null)
                                    RaiseLine(line, resultTag);
                            }
                        }
                    }
                    catch (ObjectDisposedException)
                    {
                    }
                    catch (Exception ex)
                    {
                        RaiseMonitor($"[{sourceTag}] ОШИБКА: {ex.Message}");
                        await DelayReconnect(settings, token).ConfigureAwait(false);
                    }
                }
            }, token);
        }

        private void DrainTextBuffer(StringBuilder lineBuffer, string sourceTag)
        {
            int lineStart = 0;
            for (int i = 0; i < lineBuffer.Length; i++)
            {
                char ch = lineBuffer[i];
                if (ch != '\r' && ch != '\n')
                    continue;

                if (i > lineStart)
                {
                    string line = lineBuffer.ToString(lineStart, i - lineStart).Trim();
                    if (!string.IsNullOrWhiteSpace(line))
                        RaiseLine(line, sourceTag);
                }

                while (i + 1 < lineBuffer.Length && (lineBuffer[i + 1] == '\r' || lineBuffer[i + 1] == '\n'))
                    i++;
                lineStart = i + 1;
            }

            if (lineStart > 0)
                lineBuffer.Remove(0, lineStart);
        }

        private async Task ReadTextLinesAsync(Stream stream, CancellationToken token, string sourceTag)
        {
            using (var reader = new StreamReader(stream, Encoding.ASCII, false, 4096, true))
            {
                while (!token.IsCancellationRequested)
                {
                    var line = await reader.ReadLineAsync().ConfigureAwait(false);
                    if (line == null)
                        break;
                    RaiseLine(line, sourceTag);
                }
            }
        }

        private async Task ReadGdl90FramesAsync(Stream stream, CancellationToken token, string sourceTag)
        {
            var buffer = new byte[4096];
            using (var ms = new MemoryStream())
            {
                while (!token.IsCancellationRequested)
                {
                    int read = await stream.ReadAsync(buffer, 0, buffer.Length, token).ConfigureAwait(false);
                    if (read <= 0)
                        break;

                    ms.Write(buffer, 0, read);
                    EmitGdl90Frames(ms.ToArray(), sourceTag);
                    ms.SetLength(0);
                }
            }
        }

        private void EmitGdl90Frames(byte[] data, string sourceTag)
        {
            if (data == null || data.Length == 0)
                return;

            int start = -1;
            for (int i = 0; i < data.Length; i++)
            {
                if (data[i] == 0x7E)
                {
                    if (start >= 0 && i > start + 1)
                    {
                        var frame = new byte[i - start + 1];
                        Buffer.BlockCopy(data, start, frame, 0, frame.Length);
                        RaiseLine("GDL90HEX:" + BitConverter.ToString(frame).Replace("-", string.Empty), sourceTag);
                    }
                    start = i;
                }
            }
        }

        private void RaiseLine(string line, string sourceTag)
        {
            if (string.IsNullOrWhiteSpace(line))
                return;

            var trimmed = line.Trim();
            PostToContext(() =>
            {
                PacketReceived?.Invoke(sourceTag, trimmed);
                LineReceived?.Invoke(trimmed);
            });
        }

        private void RaiseStatus(string message)
        {
            PostToContext(() => StatusChanged?.Invoke(message));
        }

        private void RaiseMonitor(string message)
        {
            PostToContext(() => MonitorMessage?.Invoke(message));
        }

        private void PostToContext(Action action)
        {
            var context = _syncContext;
            if (context != null)
                context.Post(_ => action(), null);
            else
                action();
        }

        private static void SleepReconnect(AppSettings settings, CancellationToken token)
        {
            if (!settings.AutoReconnect)
                return;
            try { Task.Delay(settings.ReconnectDelayMs, token).Wait(token); } catch { }
        }

        private static async Task DelayReconnect(AppSettings settings, CancellationToken token)
        {
            if (!settings.AutoReconnect)
                return;
            try { await Task.Delay(settings.ReconnectDelayMs, token).ConfigureAwait(false); } catch { }
        }

        public void Dispose() => Stop();
    }
}
