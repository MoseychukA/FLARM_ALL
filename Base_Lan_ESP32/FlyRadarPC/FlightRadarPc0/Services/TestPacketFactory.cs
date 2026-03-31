namespace FlightRadarPc.Services
{
    public static class TestPacketFactory
    {
        public static string CreateVisibleFlyRf()
        {
            return "$FLYRF,ABC123,7000,TEST01,1200,1180,145,92,2,55.7678,37.6376,1,0,12,34,AA55*00";
        }

        public static string CreateFlyRf()
        {
            return "$FLYRF,ABC123,7000,TEST01,1200,1180,145,92,2,55.8000,37.7000,1,0,12,34,AA55*00";
        }

        public static string CreatePflAa()
        {
            return "$PFLAA,0,1500,-800,120,1,FLR01,270,,32.5,1.2,1*00";
        }

        public static string CreateSbs1()
        {
            return "MSG,3,111,11111,4CA123,111111,2026/03/23,12:34:56.789,2026/03/23,12:34:56.789,DLH2AB,35000,430,184,55.8100,37.6200,0,7000,0,0,0,0";
        }

        public static string CreateGdl90Hex()
        {
            // Demo GDL90-like traffic frame for parser testing.
            return "GDL90HEX:7E1400ABC12327AA8E1AC1E233C80064300080205445535430312000A17E";
        }
    }
}
