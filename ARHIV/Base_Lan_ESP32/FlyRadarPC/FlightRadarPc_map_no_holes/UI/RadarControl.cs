using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Linq;
using System.Windows.Forms;
using FlightRadarPc.Models;
using FlightRadarPc.Services;

namespace FlightRadarPc.UI
{
    public class RadarControl : Control
    {
        private IReadOnlyList<AircraftTarget> _targets = Array.Empty<AircraftTarget>();
        private static int _paintCounter;
        private readonly IMapLayerRenderer _mapRenderer = new OpenStreetMapTileRenderer();

        public event Action<float> ZoomChanged;
        public event Action<AircraftTarget> TargetClicked;

        public AppSettings Settings { get; set; }
        public float RadarRangeKm { get; set; } = 20f;
        public bool ShowRangeRings { get; set; } = true;
        public bool ShowTracks { get; set; } = true;
        public bool ShowTargetLabels { get; set; } = true;
        public uint? SelectedTargetAddress { get; set; }

        public RadarControl()
        {
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            DoubleBuffered = true;
            BackColor = Color.Black;
            ForeColor = Color.Lime;
            ResizeRedraw = true;
            MouseWheel += RadarControl_MouseWheel;
            MouseClick += RadarControl_MouseClick;
        }

        private void RadarControl_MouseClick(object sender, MouseEventArgs e)
        {
            try
            {
                AircraftTarget bestTarget = null;
                double bestDistance = 18.0;

                foreach (var target in _targets)
                {
                    if (target == null || target.DistanceKm > RadarRangeKm)
                        continue;

                    double dx = target.RadarPoint.X - e.X;
                    double dy = target.RadarPoint.Y - e.Y;
                    double distance = Math.Sqrt(dx * dx + dy * dy);
                    if (distance < bestDistance)
                    {
                        bestDistance = distance;
                        bestTarget = target;
                    }
                }

                if (bestTarget == null)
                    return;

                SelectedTargetAddress = bestTarget.Address;
                Invalidate();
                TargetClicked?.Invoke(bestTarget);
            }
            catch (Exception ex)
            {
                LogService.Write(ex, "RadarControl.MouseClick");
            }
        }

        public void SetTargets(IReadOnlyList<AircraftTarget> targets)
        {
            _targets = targets ?? Array.Empty<AircraftTarget>();
            LogService.Write($"RadarControl.SetTargets: count={_targets.Count}, selected={(SelectedTargetAddress.HasValue ? SelectedTargetAddress.Value.ToString("X6") : "none")}");
            Invalidate();
        }

        protected override void OnPaint(PaintEventArgs e)
        {
            int paintId = System.Threading.Interlocked.Increment(ref _paintCounter);
            LogService.Write($"RadarControl.OnPaint[{paintId}] ENTER width={Width} height={Height} targets={_targets.Count} range={RadarRangeKm:0.0}");

            try
            {
                base.OnPaint(e);
                var g = e.Graphics;
                g.SmoothingMode = SmoothingMode.AntiAlias;
                g.PixelOffsetMode = PixelOffsetMode.HighQuality;
                g.Clear(Color.Black);

                int size = Math.Min(Width, Height) - 40;
                if (size <= 50)
                {
                    LogService.Write($"RadarControl.OnPaint[{paintId}] EXIT too-small");
                    return;
                }

                float radius = size / 2f;
                float cx = Width / 2f;
                float cy = Height / 2f;
                var radarRect = new RectangleF(cx - radius, cy - radius, size, size);

                using (var path = new GraphicsPath())
                {
                    path.AddEllipse(radarRect);
                    g.SetClip(path);

                    using (var back = new SolidBrush(Color.FromArgb(8, 28, 8)))
                        g.FillEllipse(back, radarRect);

                    GraphicsState state = g.Save();
                    try
                    {
                        if (Settings != null && Settings.OrientationMode == RadarOrientationMode.HeadingUp)
                        {
                            g.TranslateTransform(cx, cy);
                            g.RotateTransform(-Settings.OwnshipHeadingDeg);
                            g.TranslateTransform(-cx, -cy);
                        }

                        if (Settings != null && Settings.ShowMapBackground)
                            _mapRenderer.Draw(g, Rectangle.Round(radarRect), Settings.LocalLatitude, Settings.LocalLongitude, RadarRangeKm, Settings);

                        if (ShowRangeRings)
                        {
                            LogService.Write($"RadarControl.OnPaint[{paintId}] DrawRangeRings");
                            DrawRangeRings(g, cx, cy, radius);
                        }

                        LogService.Write($"RadarControl.OnPaint[{paintId}] DrawGrid");
                        DrawGrid(g, cx, cy, radius, radarRect);

                        if (ShowTracks)
                        {
                            LogService.Write($"RadarControl.OnPaint[{paintId}] DrawTracks");
                            DrawTracks(g, cx, cy, radius);
                        }

                        LogService.Write($"RadarControl.OnPaint[{paintId}] DrawTargets begin");
                        DrawTargets(g, cx, cy, radius);
                        LogService.Write($"RadarControl.OnPaint[{paintId}] DrawTargets end");
                    }
                    finally
                    {
                        g.Restore(state);
                        g.ResetClip();
                    }
                }

                LogService.Write($"RadarControl.OnPaint[{paintId}] DrawCenter");
                DrawCenterAirplane(g, cx, cy);
                DrawCompass(g, cx, cy, radius);
                DrawTopStatus(g);
                LogService.Write($"RadarControl.OnPaint[{paintId}] EXIT ok");
            }
            catch (Exception ex)
            {
                LogService.Write(ex, $"RadarControl.OnPaint[{paintId}]");
                try
                {
                    e.Graphics.Clear(Color.Black);
                    using (var font = new Font("Segoe UI", 10f, FontStyle.Bold))
                    using (var brush = new SolidBrush(Color.OrangeRed))
                    {
                        e.Graphics.DrawString("Ошибка отрисовки радара. См. FlightRadarPc.log", font, brush, new PointF(12, 12));
                    }
                }
                catch
                {
                }
            }
        }

        private void DrawGrid(Graphics g, float cx, float cy, float radius, RectangleF radarRect)
        {
            using (var gridPen = new Pen(Color.FromArgb(0, 120, 0), 1))
            using (var majorPen = new Pen(Color.FromArgb(0, 190, 0), 1.4f))
            {
                for (int deg = 0; deg < 360; deg += 10)
                {
                    bool major = deg % 30 == 0;
                    float tickOuter = radius;
                    float tickInner = radius - (major ? 14 : 8);
                    var p1 = GeoHelper.PolarToPoint(cx, cy, tickOuter, deg, 1);
                    var p2 = GeoHelper.PolarToPoint(cx, cy, tickInner, deg, 1);
                    g.DrawLine(major ? majorPen : gridPen, p1, p2);
                }

                g.DrawLine(gridPen, cx - radius, cy, cx + radius, cy);
                g.DrawLine(gridPen, cx, cy - radius, cx, cy + radius);
                g.DrawEllipse(majorPen, radarRect);
            }
        }

        private void DrawRangeRings(Graphics g, float cx, float cy, float radius)
        {
            using (var ringPen = new Pen(Color.Black, 1.4f))
            using (var font = new Font("Segoe UI", 9f, FontStyle.Bold))
            using (var brush = new SolidBrush(Color.Black))
            using (var back = new SolidBrush(Color.FromArgb(230, 255, 255, 255)))
            {
                ringPen.DashStyle = DashStyle.Solid;

                foreach (var ringKm in RadarRangeHelper.GetRingSteps(RadarRangeKm))
                {
                    float rr = radius * ringKm / RadarRangeKm;
                    g.DrawEllipse(ringPen, cx - rr, cy - rr, rr * 2, rr * 2);

                    string label = $"{ringKm:0.#} км";
                    var sizeF = g.MeasureString(label, font);
                    float lx = cx + 12;
                    float ly = cy - rr - sizeF.Height - 4;
                    g.FillRectangle(back, lx - 3, ly - 1, sizeF.Width + 6, sizeF.Height + 2);
                    g.DrawString(label, font, brush, lx, ly);
                }
            }
        }


        private int GetFadeAlpha(AircraftTarget target)
        {
            if (target == null)
                return 255;

            int holdSeconds = Math.Max(5, Settings != null ? Settings.TargetHoldSeconds : 8);
            double ageSeconds = Math.Max(0, (DateTime.UtcNow - target.LastUpdateUtc).TotalSeconds);
            double fadeSeconds = Math.Max(4.0, Math.Min(8.0, holdSeconds));
            double fadeStart = holdSeconds;
            double fadeEnd = fadeStart + fadeSeconds;
            if (ageSeconds <= fadeStart)
                return 255;
            if (ageSeconds >= fadeEnd)
                return 0;

            double k = 1.0 - ((ageSeconds - fadeStart) / fadeSeconds);
            k = Math.Max(0.0, Math.Min(1.0, k));
            return Math.Max(0, Math.Min(255, (int)Math.Round(255.0 * k)));
        }

        private static Color WithAlpha(Color color, int alpha)
        {
            return Color.FromArgb(Math.Max(0, Math.Min(255, alpha)), color);
        }

        private void DrawTracks(Graphics g, float cx, float cy, float radius)
        {
            foreach (var target in _targets)
            {
                if (target == null || target.Track == null || target.Track.Count < 2)
                    continue;

                int alpha = Math.Max(50, GetFadeAlpha(target) - 20);
                using (var pen = new Pen(Color.FromArgb(alpha, 0, 180, 220), 2f))
                {
                    var points = new List<PointF>();
                    foreach (var trackPoint in target.Track)
                    {
                        if (trackPoint.DistanceKm > RadarRangeKm)
                            continue;
                        double normalized = Math.Min(1.0, trackPoint.DistanceKm / RadarRangeKm);
                        points.Add(GeoHelper.PolarToPoint(cx, cy, radius, trackPoint.BearingDeg, normalized));
                    }

                    if (points.Count >= 2)
                        g.DrawLines(pen, points.ToArray());
                }
            }
        }


        private void DrawTargets(Graphics g, float cx, float cy, float radius)
        {
            var visibleTargets = _targets
                .Where(t => t != null && t.DistanceKm <= RadarRangeKm)
                .OrderBy(t => t.DistanceKm)
                .ToList();

            foreach (var target in visibleTargets)
            {
                double normalized = Math.Min(1.0, target.DistanceKm / RadarRangeKm);
                var pt = GeoHelper.PolarToPoint(cx, cy, radius, target.BearingDeg, normalized);
                target.RadarPoint = pt;

                int alpha = GetFadeAlpha(target);
                float size = GetMarkerSize(target);
                Color markerColor = GetMarkerColor(target);

                using (var markerBrush = new SolidBrush(WithAlpha(markerColor, alpha)))
                using (var pen = new Pen(WithAlpha(Color.Black, alpha), 2f))
                {
                    g.FillEllipse(markerBrush, pt.X - size / 2f, pt.Y - size / 2f, size, size);
                    g.DrawEllipse(pen, pt.X - size / 2f, pt.Y - size / 2f, size, size);
                }

                DrawMotionVector(g, target, pt, size, alpha, markerColor);

                if (SelectedTargetAddress.HasValue && target.Address == SelectedTargetAddress.Value)
                {
                    using (var selPen = new Pen(WithAlpha(Color.Cyan, alpha), 2f))
                        g.DrawEllipse(selPen, pt.X - 12f, pt.Y - 12f, 24f, 24f);
                }
            }

            if (!ShowTargetLabels)
                return;

            using (var font = new Font("Segoe UI", 9f, FontStyle.Bold))
            {
                var occupiedRects = new List<RectangleF>();
                foreach (var target in visibleTargets)
                {
                    int alpha = GetFadeAlpha(target);
                    string name = string.IsNullOrWhiteSpace(target.Callsign) ? target.Address.ToString("X6") : target.Callsign.Trim();
                    string label = $"{name} {target.DistanceKm:0.0}км {target.AltitudeGps}м";
                    var sizeF = g.MeasureString(label, font);
                    var rect = ChooseLabelRectangle(g, target, sizeF, visibleTargets, occupiedRects);
                    occupiedRects.Add(rect);

                    using (var brush = new SolidBrush(WithAlpha(Color.White, alpha)))
                    using (var back = new SolidBrush(Color.FromArgb(Math.Max(80, Math.Min(220, alpha)), 0, 40, 0)))
                    {
                        g.FillRectangle(back, rect);
                        g.DrawString(label, font, brush, rect.X + 2, rect.Y + 1);
                    }
                }
            }
        }



        private float GetMarkerSize(AircraftTarget target)
        {
            return target != null && target.IsTestTarget ? 16f : 10f;
        }

        private Color GetMarkerColor(AircraftTarget target)
        {
            Color markerColor = target != null && target.IsTestTarget ? Color.Yellow : Color.Orange;
            if (target == null)
                return markerColor;
            if (target.AlertLevel == AlertLevel.Warning)
                markerColor = Color.Yellow;
            else if (target.AlertLevel == AlertLevel.Danger)
                markerColor = Color.Red;
            return markerColor;
        }

        private RectangleF ChooseLabelRectangle(Graphics g, AircraftTarget target, SizeF textSize, List<AircraftTarget> visibleTargets, List<RectangleF> occupiedRects)
        {
            float markerSize = GetMarkerSize(target);
            float rectWidth = textSize.Width + 4f;
            float rectHeight = textSize.Height + 2f;
            float gap = 8f;
            var pt = target.RadarPoint;
            var clientRect = new RectangleF(2, 2, Math.Max(0, Width - 4), Math.Max(0, Height - 4));

            var candidates = BuildLabelCandidates(pt, markerSize, rectWidth, rectHeight, gap);

            RectangleF best = candidates[0];
            float bestScore = float.MaxValue;

            foreach (var candidate in candidates)
            {
                float score = ScoreLabelCandidate(g, target, candidate, visibleTargets, occupiedRects, clientRect, markerSize);
                if (score < bestScore)
                {
                    bestScore = score;
                    best = candidate;
                }
            }

            return best;
        }

        private RectangleF[] BuildLabelCandidates(PointF pt, float markerSize, float rectWidth, float rectHeight, float gap)
        {
            float near = markerSize / 2f + gap;
            float far = near + 10f;

            return new[]
            {
                new RectangleF(pt.X + near, pt.Y - rectHeight / 2f, rectWidth, rectHeight),
                new RectangleF(pt.X - near - rectWidth, pt.Y - rectHeight / 2f, rectWidth, rectHeight),
                new RectangleF(pt.X - rectWidth / 2f, pt.Y - near - rectHeight, rectWidth, rectHeight),
                new RectangleF(pt.X - rectWidth / 2f, pt.Y + near, rectWidth, rectHeight),
                new RectangleF(pt.X + far, pt.Y - rectHeight - 2f, rectWidth, rectHeight),
                new RectangleF(pt.X + far, pt.Y + 2f, rectWidth, rectHeight),
                new RectangleF(pt.X - far - rectWidth, pt.Y - rectHeight - 2f, rectWidth, rectHeight),
                new RectangleF(pt.X - far - rectWidth, pt.Y + 2f, rectWidth, rectHeight)
            };
        }

        private float ScoreLabelCandidate(Graphics g, AircraftTarget target, RectangleF candidate, List<AircraftTarget> visibleTargets, List<RectangleF> occupiedRects, RectangleF clientRect, float markerSize)
        {
            float score = 0f;
            var pt = target.RadarPoint;
            var inflatedCandidate = RectangleF.Inflate(candidate, 4f, 3f);
            var leaderBounds = GetLeaderLineBounds(pt, candidate);

            if (!clientRect.Contains(candidate))
                score += 3000f;

            foreach (var occupied in occupiedRects)
            {
                if (inflatedCandidate.IntersectsWith(occupied))
                    score += 2500f;
            }

            foreach (var other in visibleTargets)
            {
                if (other == null || other.Address == target.Address)
                    continue;

                float otherSize = GetMarkerSize(other);
                var markerRect = new RectangleF(other.RadarPoint.X - otherSize / 2f - 4f, other.RadarPoint.Y - otherSize / 2f - 4f, otherSize + 8f, otherSize + 8f);
                if (inflatedCandidate.IntersectsWith(markerRect))
                    score += 2200f;
                if (leaderBounds.IntersectsWith(markerRect))
                    score += 900f;

                var vectorRect = GetMotionVectorBounds(other, other.RadarPoint, otherSize, g.DpiX);
                if (vectorRect.HasValue)
                {
                    var inflatedVector = RectangleF.Inflate(vectorRect.Value, 3f, 3f);
                    if (inflatedCandidate.IntersectsWith(inflatedVector))
                        score += 1700f;
                    if (leaderBounds.IntersectsWith(inflatedVector))
                        score += 700f;
                }

                float distance = DistanceFromRect(other.RadarPoint, candidate);
                if (distance < 18f)
                    score += (18f - distance) * 40f;
            }

            var ownVectorRect = GetMotionVectorBounds(target, target.RadarPoint, markerSize, g.DpiX);
            if (ownVectorRect.HasValue)
            {
                var ownVectorInflated = RectangleF.Inflate(ownVectorRect.Value, 3f, 3f);
                if (inflatedCandidate.IntersectsWith(ownVectorInflated))
                    score += 1800f;
                if (leaderBounds.IntersectsWith(ownVectorInflated))
                    score += 600f;
            }

            score += GetSectorPenalty(pt, candidate, visibleTargets, target.Address);

            float dxCenter = (candidate.Left + candidate.Width / 2f) - pt.X;
            float dyCenter = (candidate.Top + candidate.Height / 2f) - pt.Y;
            score += (float)Math.Sqrt(dxCenter * dxCenter + dyCenter * dyCenter) * 0.8f;

            return score;
        }

        private float GetSectorPenalty(PointF pt, RectangleF candidate, List<AircraftTarget> visibleTargets, uint currentAddress)
        {
            float cx = candidate.Left + candidate.Width / 2f;
            float cy = candidate.Top + candidate.Height / 2f;
            double angle = Math.Atan2(cy - pt.Y, cx - pt.X);
            float penalty = 0f;

            foreach (var other in visibleTargets)
            {
                if (other == null || other.Address == currentAddress)
                    continue;

                double otherAngle = Math.Atan2(other.RadarPoint.Y - pt.Y, other.RadarPoint.X - pt.X);
                double diff = Math.Abs(NormalizeRadians(angle - otherAngle));
                double dist = Math.Sqrt(Math.Pow(other.RadarPoint.X - pt.X, 2) + Math.Pow(other.RadarPoint.Y - pt.Y, 2));

                if (diff < (Math.PI / 6.0))
                    penalty += (float)(Math.Max(0.0, 28.0 - Math.Min(28.0, dist)) * 12.0);
                else if (diff < (Math.PI / 4.0))
                    penalty += (float)(Math.Max(0.0, 20.0 - Math.Min(20.0, dist)) * 6.0);
            }

            return penalty;
        }

        private static double NormalizeRadians(double angle)
        {
            while (angle > Math.PI) angle -= Math.PI * 2.0;
            while (angle < -Math.PI) angle += Math.PI * 2.0;
            return angle;
        }

        private static RectangleF GetLeaderLineBounds(PointF pt, RectangleF rect)
        {
            var anchor = new PointF(rect.Left + rect.Width / 2f, rect.Top + rect.Height / 2f);
            float left = Math.Min(pt.X, anchor.X);
            float top = Math.Min(pt.Y, anchor.Y);
            float right = Math.Max(pt.X, anchor.X);
            float bottom = Math.Max(pt.Y, anchor.Y);
            return RectangleF.FromLTRB(left - 2f, top - 2f, right + 2f, bottom + 2f);
        }

        private static float DistanceFromRect(PointF p, RectangleF rect)
        {
            float dx = Math.Max(rect.Left - p.X, Math.Max(0, p.X - rect.Right));
            float dy = Math.Max(rect.Top - p.Y, Math.Max(0, p.Y - rect.Bottom));
            return (float)Math.Sqrt(dx * dx + dy * dy);
        }

        private RectangleF? GetMotionVectorBounds(AircraftTarget target, PointF pt, float markerSize, float dpiX)
        {
            if (target == null || target.SpeedKmh <= 0)
                return null;

            float maxVectorMm = Settings?.VectorLineMaxMm ?? 12f;
            float maxPixels = (dpiX / 25.4f) * maxVectorMm;
            int maxSpeedKmh = Settings?.VectorLineMaxSpeedKmh ?? 1200;
            float lineLength = maxPixels * Math.Min(1f, Math.Max(0f, target.SpeedKmh / (float)maxSpeedKmh));
            if (lineLength < 6f)
                lineLength = 6f;

            double angleRad = target.CourseDeg * Math.PI / 180.0;
            float startOffset = markerSize * 0.55f;
            var start = new PointF(
                pt.X + (float)(Math.Sin(angleRad) * startOffset),
                pt.Y - (float)(Math.Cos(angleRad) * startOffset));
            var end = new PointF(
                pt.X + (float)(Math.Sin(angleRad) * (startOffset + lineLength)),
                pt.Y - (float)(Math.Cos(angleRad) * (startOffset + lineLength)));

            float left = Math.Min(start.X, end.X);
            float top = Math.Min(start.Y, end.Y);
            float width = Math.Abs(end.X - start.X);
            float height = Math.Abs(end.Y - start.Y);
            return RectangleF.FromLTRB(left, top, left + Math.Max(4f, width), top + Math.Max(4f, height));
        }

        private void DrawMotionVector(Graphics g, AircraftTarget target, PointF pt, float markerSize, int alpha, Color markerColor)
        {
            if (target == null)
                return;
            if (target.SpeedKmh <= 0)
                return;

            float maxVectorMm = Settings?.VectorLineMaxMm ?? 12f;
            float maxPixels = (g.DpiX / 25.4f) * maxVectorMm;
            int maxSpeedKmh = Settings?.VectorLineMaxSpeedKmh ?? 1200;
            float lineLength = maxPixels * Math.Min(1f, Math.Max(0f, target.SpeedKmh / (float)maxSpeedKmh));
            if (lineLength < 6f)
                lineLength = 6f;

            double angleRad = target.CourseDeg * Math.PI / 180.0;
            float startOffset = markerSize * 0.55f;
            var start = new PointF(
                pt.X + (float)(Math.Sin(angleRad) * startOffset),
                pt.Y - (float)(Math.Cos(angleRad) * startOffset));
            var end = new PointF(
                pt.X + (float)(Math.Sin(angleRad) * (startOffset + lineLength)),
                pt.Y - (float)(Math.Cos(angleRad) * (startOffset + lineLength)));

            using (var shadowPen = new Pen(Color.FromArgb(Math.Max(0, alpha - 70), Color.Black), 3.2f))
            using (var vectorPen = new Pen(WithAlpha(markerColor, alpha), 1.8f))
            {
                shadowPen.StartCap = LineCap.Round;
                shadowPen.EndCap = LineCap.Round;
                vectorPen.StartCap = LineCap.Round;
                vectorPen.EndCap = LineCap.Round;
                g.DrawLine(shadowPen, start, end);
                g.DrawLine(vectorPen, start, end);
            }
        }

        private void DrawCenterAirplane(Graphics g, float cx, float cy)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;

            using (var shadow = new SolidBrush(Color.FromArgb(90, 0, 0, 0)))
                g.FillEllipse(shadow, cx - 14, cy - 14, 28, 28);

            using (var outlinePen = new Pen(Color.FromArgb(30, 40, 42), 1.2f))
            using (var bodyBrush = new SolidBrush(Color.FromArgb(220, 245, 248)))
            using (var accentBrush = new SolidBrush(Color.FromArgb(0, 220, 255)))
            using (var canopyBrush = new SolidBrush(Color.FromArgb(55, 120, 180)))
            {
                var fuselage = new RectangleF(cx - 3.2f, cy - 16f, 6.4f, 32f);
                using (var fuselagePath = CreateRoundedRectanglePath(fuselage, 3f))
                {
                    g.FillPath(bodyBrush, fuselagePath);
                    g.DrawPath(outlinePen, fuselagePath);
                }

                PointF[] mainWing =
                {
                    new PointF(cx - 18f, cy - 2f),
                    new PointF(cx - 4.5f, cy + 1.5f),
                    new PointF(cx + 4.5f, cy + 1.5f),
                    new PointF(cx + 18f, cy - 2f),
                    new PointF(cx + 15f, cy - 5.5f),
                    new PointF(cx - 15f, cy - 5.5f)
                };
                g.FillPolygon(bodyBrush, mainWing);
                g.DrawPolygon(outlinePen, mainWing);

                PointF[] tailWing =
                {
                    new PointF(cx - 9f, cy + 10.5f),
                    new PointF(cx - 3.5f, cy + 8f),
                    new PointF(cx + 3.5f, cy + 8f),
                    new PointF(cx + 9f, cy + 10.5f),
                    new PointF(cx + 7f, cy + 13f),
                    new PointF(cx - 7f, cy + 13f)
                };
                g.FillPolygon(bodyBrush, tailWing);
                g.DrawPolygon(outlinePen, tailWing);

                PointF[] nose =
                {
                    new PointF(cx, cy - 20f),
                    new PointF(cx + 3.8f, cy - 13f),
                    new PointF(cx - 3.8f, cy - 13f)
                };
                g.FillPolygon(accentBrush, nose);
                g.DrawPolygon(outlinePen, nose);

                var canopy = new RectangleF(cx - 2.4f, cy - 10f, 4.8f, 8f);
                g.FillEllipse(canopyBrush, canopy);
                g.DrawEllipse(outlinePen, canopy);

                PointF[] fin =
                {
                    new PointF(cx - 0.8f, cy + 13.5f),
                    new PointF(cx, cy + 19f),
                    new PointF(cx + 0.8f, cy + 13.5f)
                };
                g.FillPolygon(accentBrush, fin);
                g.DrawPolygon(outlinePen, fin);
            }
        }

        private void DrawCompass(Graphics g, float cx, float cy, float radius)
        {
            using (var font = new Font("Segoe UI", 10f, FontStyle.Bold))
            using (var brush = new SolidBrush(Color.White))
            using (var back = new SolidBrush(Color.FromArgb(200, 0, 40, 0)))
            {
                DrawCardinal(g, "N", cx, cy - radius + 20, font, brush, back);
                DrawCardinal(g, "E", cx + radius - 20, cy, font, brush, back);
                DrawCardinal(g, "S", cx, cy + radius - 20, font, brush, back);
                DrawCardinal(g, "W", cx - radius + 20, cy, font, brush, back);
            }
        }

        private void DrawCardinal(Graphics g, string text, float x, float y, Font font, Brush brush, Brush back)
        {
            var s = g.MeasureString(text, font);
            var rect = new RectangleF(x - s.Width / 2f - 4, y - s.Height / 2f - 2, s.Width + 8, s.Height + 4);
            g.FillRectangle(back, rect);
            g.DrawString(text, font, brush, rect.X + 4, rect.Y + 2);
        }

        private void DrawTopStatus(Graphics g)
        {
            using (var font = new Font("Segoe UI", 9f, FontStyle.Bold))
            using (var brush = new SolidBrush(Color.White))
            using (var back = new SolidBrush(Color.FromArgb(180, 0, 40, 0)))
            {
                string mapText = Settings != null && Settings.ShowMapBackground ? "карта вкл" : "карта выкл";
                string modeText = Settings != null ? (Settings.OrientationMode == RadarOrientationMode.HeadingUp ? "Heading Up" : "North Up") : "North Up";
                string text = $"Режим {modeText} | {mapText} | Подписи {(ShowTargetLabels ? "вкл" : "выкл")} | Целей: {_targets.Count}";
                var size = g.MeasureString(text, font);
                g.FillRectangle(back, 8, 8, size.Width + 10, size.Height + 4);
                g.DrawString(text, font, brush, 13, 10);
            }
        }

        private void RadarControl_MouseWheel(object sender, MouseEventArgs e)
        {
            RadarRangeKm = e.Delta > 0 ? RadarRangeHelper.ZoomIn(RadarRangeKm) : RadarRangeHelper.ZoomOut(RadarRangeKm);
            ZoomChanged?.Invoke(RadarRangeKm);
            Invalidate();
        }

        private static GraphicsPath CreateRoundedRectanglePath(RectangleF rect, float radius)
        {
            float diameter = radius * 2f;
            var path = new GraphicsPath();
            path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
            path.AddArc(rect.Right - diameter, rect.Y, diameter, diameter, 270, 90);
            path.AddArc(rect.Right - diameter, rect.Bottom - diameter, diameter, diameter, 0, 90);
            path.AddArc(rect.X, rect.Bottom - diameter, diameter, diameter, 90, 90);
            path.CloseFigure();
            return path;
        }

    }
}
