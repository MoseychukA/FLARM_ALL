using System.Collections.Generic;

namespace FlightRadarPc.Services
{
    public static class RadarRangeHelper
    {
        public static IReadOnlyList<float> GetRingSteps(float rangeKm)
        {
            if (rangeKm <= 2) return new[] { 0.5f, 1.0f, 1.5f, 2.0f };
            if (rangeKm <= 5) return new[] { 1f, 2f, 3f, 4f, 5f };
            if (rangeKm <= 10) return new[] { 2f, 4f, 6f, 8f, 10f };
            if (rangeKm <= 20) return new[] { 5f, 10f, 15f, 20f };
            if (rangeKm <= 50) return new[] { 10f, 20f, 30f, 40f, 50f };
            return new[] { rangeKm / 4f, rangeKm / 2f, rangeKm * 0.75f, rangeKm };
        }

        public static float ZoomIn(float current) => Clamp(current / 1.25f);
        public static float ZoomOut(float current) => Clamp(current * 1.25f);
        public static float Clamp(float value) => value < 1f ? 1f : (value > 300f ? 300f : value);
    }
}
