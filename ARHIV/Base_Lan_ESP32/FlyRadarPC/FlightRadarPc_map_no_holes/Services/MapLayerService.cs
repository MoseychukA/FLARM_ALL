using System;
using System.Collections.Concurrent;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Net;
using System.Threading.Tasks;

namespace FlightRadarPc.Services
{
    public interface IMapLayerRenderer
    {
        void Draw(Graphics g, Rectangle bounds, double centerLat, double centerLon, float rangeKm, AppSettings settings);
    }

    public class StubMapLayerRenderer : IMapLayerRenderer
    {
        public void Draw(Graphics g, Rectangle bounds, double centerLat, double centerLon, float rangeKm, AppSettings settings)
        {
            using (var brush = new SolidBrush(Color.FromArgb(18, 28, 18)))
            using (var pen = new Pen(Color.FromArgb(28, 55, 28), 1))
            using (var font = new Font("Segoe UI", 8f))
            using (var textBrush = new SolidBrush(Color.FromArgb(90, 140, 90)))
            {
                g.FillRectangle(brush, bounds);
                for (int x = bounds.Left; x < bounds.Right; x += 64)
                    g.DrawLine(pen, x, bounds.Top, x, bounds.Bottom);
                for (int y = bounds.Top; y < bounds.Bottom; y += 64)
                    g.DrawLine(pen, bounds.Left, y, bounds.Right, y);
                g.DrawString("Карта отключена. Включите OSM в настройках.", font, textBrush, bounds.Left + 8, bounds.Bottom - 22);
            }
        }
    }

    public class OpenStreetMapTileRenderer : IMapLayerRenderer
    {
        private readonly ConcurrentDictionary<string, Image> _memoryCache = new ConcurrentDictionary<string, Image>(StringComparer.OrdinalIgnoreCase);
        private readonly ConcurrentDictionary<string, byte> _downloadInProgress = new ConcurrentDictionary<string, byte>(StringComparer.OrdinalIgnoreCase);

        public void Draw(Graphics g, Rectangle bounds, double centerLat, double centerLon, float rangeKm, AppSettings settings)
        {
            if (!settings.ShowMapBackground || settings.MapProviderMode == MapProviderMode.None)
            {
                new StubMapLayerRenderer().Draw(g, bounds, centerLat, centerLon, rangeKm, settings);
                return;
            }

            using (var bg = new SolidBrush(Color.FromArgb(232, 236, 232)))
                g.FillRectangle(bg, bounds);

            int zoom = settings.PreferredMapZoom > 0 ? settings.PreferredMapZoom : EstimateZoom(centerLat, rangeKm, bounds.Width);
            zoom = Math.Max(1, Math.Min(18, zoom));

            double centerPxX = LonToPixelX(centerLon, zoom);
            double centerPxY = LatToPixelY(centerLat, zoom);

            double leftPx = centerPxX - bounds.Width / 2.0;
            double topPx = centerPxY - bounds.Height / 2.0;
            int tileSize = 256;
            int minTileX = (int)Math.Floor(leftPx / tileSize);
            int minTileY = (int)Math.Floor(topPx / tileSize);
            int maxTileX = (int)Math.Floor((leftPx + bounds.Width) / tileSize);
            int maxTileY = (int)Math.Floor((topPx + bounds.Height) / tileSize);
            int maxIndex = (1 << zoom) - 1;

            for (int tileX = minTileX; tileX <= maxTileX; tileX++)
            {
                for (int tileY = minTileY; tileY <= maxTileY; tileY++)
                {
                    int wrappedX = Wrap(tileX, 0, maxIndex);
                    if (tileY < 0 || tileY > maxIndex)
                        continue;

                    float drawX = bounds.Left + (float)(tileX * tileSize - leftPx);
                    float drawY = bounds.Top + (float)(tileY * tileSize - topPx);
                    var dest = new RectangleF(drawX, drawY, tileSize, tileSize);

                    var tileImage = GetTileImage(wrappedX, tileY, zoom, settings, queueDownload: true);
                    if (tileImage != null)
                    {
                        g.DrawImage(tileImage, dest);
                        continue;
                    }

                    if (TryDrawParentFallback(g, dest, wrappedX, tileY, zoom, settings))
                        continue;

                    DrawPlaceholderTile(g, dest, zoom, wrappedX, tileY);
                }
            }

            using (var pen = new Pen(Color.FromArgb(60, 180, 60), 1))
            using (var font = new Font("Segoe UI", 8f))
            using (var brush = new SolidBrush(Color.FromArgb(170, 230, 170)))
            {
                g.DrawString($"OSM z{zoom}", font, brush, bounds.Left + 8, bounds.Top + 8);
                g.DrawRectangle(pen, bounds);
            }
        }

        private bool TryDrawParentFallback(Graphics g, RectangleF dest, int x, int y, int z, AppSettings settings)
        {
            for (int parentZoom = z - 1; parentZoom >= 0; parentZoom--)
            {
                int shift = z - parentZoom;
                int parentX = x >> shift;
                int parentY = y >> shift;
                var parentImage = GetTileImage(parentX, parentY, parentZoom, settings, queueDownload: true);
                if (parentImage == null)
                    continue;

                int quadrantSize = 256 >> shift;
                if (quadrantSize <= 0)
                    break;

                int childOffsetX = x - (parentX << shift);
                int childOffsetY = y - (parentY << shift);
                int srcX = childOffsetX * quadrantSize;
                int srcY = childOffsetY * quadrantSize;
                var src = new Rectangle(srcX, srcY, quadrantSize, quadrantSize);

                var previousMode = g.InterpolationMode;
                var previousPixelOffset = g.PixelOffsetMode;
                g.InterpolationMode = InterpolationMode.HighQualityBicubic;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                try
                {
                    g.DrawImage(parentImage, dest, src, GraphicsUnit.Pixel);
                    using (var hatch = new HatchBrush(HatchStyle.Percent10, Color.FromArgb(28, 255, 255, 255), Color.Transparent))
                        g.FillRectangle(hatch, dest);
                    return true;
                }
                catch
                {
                    return false;
                }
                finally
                {
                    g.InterpolationMode = previousMode;
                    g.PixelOffsetMode = previousPixelOffset;
                }
            }

            return false;
        }

        private Image GetTileImage(int x, int y, int z, AppSettings settings, bool queueDownload)
        {
            string path = GetTilePath(x, y, z, settings);
            if (_memoryCache.TryGetValue(path, out var cached))
                return cached;

            var fileImage = TryLoadTileFromDisk(path);
            if (fileImage != null)
                return fileImage;

            if (queueDownload)
                QueueTileDownload(x, y, z, settings, path);

            return null;
        }

        private static string GetTilePath(int x, int y, int z, AppSettings settings)
        {
            string folder = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, settings.MapTilesPath ?? "MapCache", "osm", z.ToString(), x.ToString());
            return Path.Combine(folder, y + ".png");
        }

        private Image TryLoadTileFromDisk(string path)
        {
            try
            {
                if (!File.Exists(path))
                    return null;

                using (var fs = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                using (var img = Image.FromStream(fs))
                {
                    var clone = new Bitmap(img);
                    _memoryCache[path] = clone;
                    return clone;
                }
            }
            catch
            {
                return null;
            }
        }

        private void QueueTileDownload(int x, int y, int z, AppSettings settings, string path)
        {
            if (!_downloadInProgress.TryAdd(path, 0))
                return;

            Task.Run(() =>
            {
                try
                {
                    DownloadTile(x, y, z, settings, path);
                }
                finally
                {
                    _downloadInProgress.TryRemove(path, out _);
                }
            });
        }

        private void DownloadTile(int x, int y, int z, AppSettings settings, string path)
        {
            try
            {
                if (File.Exists(path))
                {
                    TryLoadTileFromDisk(path);
                    return;
                }

                Directory.CreateDirectory(Path.GetDirectoryName(path) ?? AppDomain.CurrentDomain.BaseDirectory);
                string url = (settings.MapUrlTemplate ?? "https://tile.openstreetmap.org/{z}/{x}/{y}.png")
                    .Replace("{z}", z.ToString())
                    .Replace("{x}", x.ToString())
                    .Replace("{y}", y.ToString());

                using (var client = new WebClient())
                {
                    client.Headers.Add(HttpRequestHeader.UserAgent, "FlightRadarPc/1.0");
                    client.DownloadFile(url, path);
                }

                TryLoadTileFromDisk(path);
            }
            catch
            {
                // Оставляем тихо: на экране будет родительский тайл или мягкая заглушка.
            }
        }

        private static void DrawPlaceholderTile(Graphics g, RectangleF dest, int z, int x, int y)
        {
            using (var brush = new SolidBrush(Color.FromArgb(235, 238, 235)))
            using (var pen = new Pen(Color.FromArgb(180, 188, 180), 1))
            using (var font = new Font("Segoe UI", 7f))
            using (var textBrush = new SolidBrush(Color.FromArgb(110, 120, 110)))
            {
                g.FillRectangle(brush, dest);
                g.DrawRectangle(pen, dest.X, dest.Y, dest.Width, dest.Height);
                g.DrawString($"Загрузка {z}/{x}/{y}", font, textBrush, dest.X + 6, dest.Y + 6);
            }
        }

        private static int EstimateZoom(double lat, double rangeKm, int widthPx)
        {
            double metersPerPixel = (rangeKm * 2000.0) / Math.Max(64, widthPx);
            double zoom = Math.Log(Math.Cos(lat * Math.PI / 180.0) * 156543.03392 / metersPerPixel, 2);
            return (int)Math.Round(zoom, MidpointRounding.AwayFromZero);
        }

        private static int Wrap(int value, int min, int max)
        {
            int range = max - min + 1;
            while (value < min) value += range;
            while (value > max) value -= range;
            return value;
        }

        private static double LonToPixelX(double lon, int zoom)
        {
            double mapSize = 256.0 * (1 << zoom);
            return (lon + 180.0) / 360.0 * mapSize;
        }

        private static double LatToPixelY(double lat, int zoom)
        {
            double mapSize = 256.0 * (1 << zoom);
            double sinLat = Math.Sin(lat * Math.PI / 180.0);
            double y = 0.5 - Math.Log((1 + sinLat) / (1 - sinLat)) / (4 * Math.PI);
            return y * mapSize;
        }
    }
}
