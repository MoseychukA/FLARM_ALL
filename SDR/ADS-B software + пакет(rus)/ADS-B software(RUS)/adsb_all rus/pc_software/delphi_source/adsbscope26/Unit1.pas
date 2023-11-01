{*********************************************************************
 * FileName:        unit1.pas
 * Dependencies:    See uses section below
 * System:          Win32 (WinXP)
 * Compiler:        Delphi 5
 * Company:         sprut
 * Copyright:       2007-2012 Joerg Bredendiek (sprut)
 * Homepage :       www.sprut.de
 *
 ********************************************************************}

 {*********************************************************************
 * adsbScope
 * software to visualize adsb-data
 * this is the main form
 *********************************************************************}

{*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *}

unit Unit1;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, CPort, StdCtrls, usbdll, Grids, Math, ComCtrls, Menus, ScktComp,
  GraphicEx, URLMon, ShellApi, jpeg, ToolWin, ImgList, FileCtrl,
  about, framerate, network, Zip, CPortCtl, Gauges, Uportmap, comsetup,
  wait4osm, gototown, log, gps;

const
    // decoder control codes
    SET_MODE    = $43;
    RD_ADC      = $37;
    READ_OFFSET = $38;
    SET_OFFSET  = $39;
    SET_DEBUG   = $3B;
    SYS_ADC     = $51;
    SYS_RS232   = $52;
    SYS_I2C     = $54;
    SYS_PWM2    = $58;
    SYS_EEPROM  = $5A;

    Erde      = 60;
    rad       = pi/180;
    ErdRadius = 3424;  // NM
    maxPlane  = 255;
//    maxPlane  = 16383;
    maxTrack  = 511;
    maxFrame  = 1023;            // Framepuffer erhöht in 2.6 T5 & T6
    maxJet    = 200000;          // Flugzeuge der Welt
    maxPort   = 6000;            // Flughaefen der Welt
    maxICAO   = 200;             // Länder der Welt
    maxGPX    = 200000;          // GPX Overlay
    maxATS    = 200000;          // ATS-Routen aus ssb1-file
    maxIFF    = 300;             // Sekundärradare
    maxTown   = 4000;            // Grossstaedte der Welt

    noheading = -10;             // Wert fuer keinen kurs
    maxWatchList = 16;           // maximale Anzahl der ueberwachten airframes
    NoRange      = 100000;       // wenn entfernung unbekannt                                                                 
    NoPlane      = maxPlane+1;   // wenn flugzeug nicht in planes gefunden
    NoAltitude   = -50000;       // ungültige Höhe in feet

    clltRed   = $008080FF;
    clltBlue  = $00FFc080;
    clltBlue1 = $00FFA080;
    clltGreen = $0080FF80;
    clltYellow= $0080FFFF;

    cpr_adsb     = 0;
    cpr_fine     = 3;
    cpr_coarse   = 4;

    cpr_airborne = 1;
    cpr_surface  = 2;

    decoder_adsbPIC   = 1;
    decoder_rxcontrol = 2;
    decoder_beast     = 3;

    maxInterogatoren = 80;
    Sekunde          = 1/24/60/60;
    Tab              = chr(9);

    // symbole für Flugzeugdarstellung
    sym_big      = 0;   // 34 pixel, ab 250 t
    sym_medium   = 1;   // 25 pixel
    sym_small    = 2;   // 22 pixel <300 kts
    sym_fast     = 3;   // >600 kts

type

  datentyp = (singl,int,ch);                          //DM61
  rec = Record                                        //DM61
        CASE datentyp OF                              //DM61
           singl : (s : Single);                      //DM61
           int   : (i : Integer);                     //DM61
           ch    : (c : Array[0..3] OF Char);         //DM61
        END;                                          //DM61

  TMatrix = Array[1..4,1..4] of real;

  TFormPos     = record
           top    : integer;
           left   : integer;
           width  : integer;
           height : integer;
         end;

  Tkoordinaten = record
           laenge : real;         // radiant
           breite : real;         // radiant
           cos_breite : real;
           sin_breite : real;
           x,y    : integer;
           V      : boolean;
           hoehe  : integer;      // in fuss
           time   : TDateTime;
         end;

  TRangePoint  = record
           range  : real;
           koord  : Tkoordinaten;
         end;

   TSatellite = record
           PRN       : integer;  // 01           Satellite PRN number
           Elevation : integer;  // 40           Elevation, degrees
           Azimuth   : integer;  // 083          Azimuth, degrees
           SNR       : integer;  // 46           SNR - higher is better
         end;

   TGps = record
           laengeG   : real;       // grad
           breiteG   : real;       // grad
           hoeheMeer : real;       // meter
           HoeheDelta: real;       // meter
           SpeedKn   : real;       // knoten
           MovingDirection : real; // Grad
           time      : TDateTime;
           date      : TDateTime;
           hoehe     : integer;    // in fuss
           valid     : boolean;
           satnr     : integer;    // anzahl der sats
           fix       : integer;
           quality   : integer;
           SentenceMax : integer;
           Sentence    : integer;
           SatInView   : integer;
           PDOP        : real;     // dilution of precision
           HDOP        : real;     // Horizontal dilution of precision (HDOP)
           VDOP        : real;     // Vertical dilution of precision (VDOP)
           Sat         : array[0..16] of TSatellite;
         end;

   Tkenner = record
           name  : string[16];
           typs  : string[16];
           typl  : string[32];
         end;

  Tairframe = record
           AA     : dword;
           known  : boolean;
           kenner : Tkenner;
         end;

  TFramerate = record
           rate  : integer;   // frames per minute
           level : byte;
           mode  : byte;
           zeit  : TDateTime;
           fpa   : integer;   // frames per aircraft
         end;

  TWatchList = record
           AA      : dword;
           ident   : string[8];
           active  : boolean;
           present : boolean;
           plane   : integer;
           info    : boolean;
         end;

  Tosmtile = record
           z,x,y    : integer;
           ok       : boolean;
           url      : string;
           name     : string;
           bm       : Tbitmap;
           pic      : Tpicture;
           kul, kor,
           kur, kol : Tkoordinaten; // Eckpunkte
         end;

  Tstring255 = string[255];

  Ttrack = Array[0..maxTrack] of Tkoordinaten;

  TFrame = record
         B      : Array[0..14]  of byte;      // 15 byte
         T      : TDateTime;                  // 8 byte
         source : byte;    // 0-local decoder ; 1-raw data received via network
       end;

  TrxString = record
          str      : AnsiString;  //string;
          OldTime  : TDateTime;
          NewTime  : TDateTime;
        end;
  PTrxString = ^TrxString;

  TPlane = record
           active     : boolean;
           missed     : boolean;    // seit 20 sec kein Kontakt mehr
           airborne   : boolean;
           crosslink  : boolean;    // crosslink capability
           alert      : boolean;
           spi        : boolean;
           gesendet   : boolean;    // DM61: Plane wurde gesendet
           AA         : dword;
           time_first : Tdatetime;
           time_last  : Tdatetime;
           time_koord : Tdatetime;
           nr         : integer;
           lat_0      : integer;    // lat even frame
           lat_1      : integer;    // lat odd frame
           long_0     : integer;    // lon even frame
           long_1     : integer;    // lon odd frame
           time_0     : Tdatetime;  // zeit des Empfangs des even frames
           time_1     : Tdatetime;  // zeit des Empfangs des odd frames
           cpr_type_0 : integer;
           cpr_type_1 : integer;
           CPR_quality   : integer;
           CPR_J         : integer;
           CPR_NL        : integer;
           latitude_old  : real;
           longitude_old : real;
           latitude      : real;        // in grad
           longitude     : real;        // in grad
           guess         : TKoordinaten;
           altitude      : integer;
           color         : Tcolor;
           Track         : Ttrack;
           trackindex    : integer;
           airframe      : Tairframe;
           speed         : real;          // in knoten
           heading       : real;          // in radiant
           steigen       : integer;       // feet/min
           ident         : string[8];
           mod3id        : word;          // alter 12Bit Mode3 Identifier
           squawk        : string[4];     // mod3id als oktaler string
           interogator   : integer;
           distance      : real;          // distance from display center to the aircraft
           azimuth       : real;          // direction from display center to the aircraft
           nr_nahe       : integer;       // DM61: Nummerierung Sortiert nach Entfernung
           tabellenzeile : integer;
           mops          : byte;
           nica          : byte;
           nicb          : byte;
           imf           : byte;
           nac           : byte;          // navigation accuracy
           om            : word;          // operational mode
           cc            : word;          // capability class
           symbol        : integer;
         end;

  TForm1 = class(TForm)
    ComPort1: TComPort;
    Image1: TImage;
    Timer1: TTimer;
    TrackBar1: TTrackBar;
    MainMenu1: TMainMenu;
    File1: TMenuItem;
    view1: TMenuItem;
    Grid1: TMenuItem;
    Statenames1: TMenuItem;
    Towns1: TMenuItem;
    Detailedmaps1: TMenuItem;
    Crosshair1: TMenuItem;
    States1: TMenuItem;
    Label2: TLabel;
    End1: TMenuItem;
    Colors1: TMenuItem;
    Borders1: TMenuItem;
    ColorDialog1: TColorDialog;
    Grid2: TMenuItem;
    Crosshair2: TMenuItem;
    Statenames2: TMenuItem;
    Towns2: TMenuItem;
    loadmaps1: TMenuItem;
    Europewest1: TMenuItem;
    EuropeUK1: TMenuItem;
    EuropeNorth1: TMenuItem;
    EuropeSouth1: TMenuItem;
    EuropeEast1: TMenuItem;
    EuropeBalkan1: TMenuItem;
    Russia1: TMenuItem;
    Airports1: TMenuItem;
    Label4: TLabel;
    Airports2: TMenuItem;
    AirportAltitude1: TMenuItem;
    Aircraft1: TMenuItem;
    RangeCircles1: TMenuItem;
    Config1: TMenuItem;
    RangeCircles2: TMenuItem;
    N50NM1: TMenuItem;
    N50km1: TMenuItem;
    N5005ftaltitude1: TMenuItem;
    AirportNames2: TMenuItem;
    normal2: TMenuItem;
    ICAO2: TMenuItem;
    IATA2: TMenuItem;
    none2: TMenuItem;
    Panel1: TPanel;
    StringGrid1: TStringGrid;
    Memo2: TMemo;
    Memo1: TMemo;
    RadioGroup1: TRadioGroup;
    Button1: TButton;
    Button2: TButton;
    dropAircraft1: TMenuItem;
    after20s1: TMenuItem;
    after1min1: TMenuItem;
    after5min1: TMenuItem;
    after30min1: TMenuItem;
    after1hour1: TMenuItem;
    never1: TMenuItem;
    ATSRoutes1: TMenuItem;
    Label5: TLabel;
    ATSRoutes2: TMenuItem;
    removeallMaps1: TMenuItem;
    Label1: TLabel;
    Label3: TLabel;
    Label6: TLabel;
    StatusBar1: TStatusBar;
    goto1: TMenuItem;
    N001: TMenuItem;
    OSMbackground1: TMenuItem;
    OSmap1: TMenuItem;
    fullcolor1: TMenuItem;
    pale1: TMenuItem;
    pale2: TMenuItem;
    gray1: TMenuItem;
    rangering1: TMenuItem;
    Aircrafttrack1: TMenuItem;
    showpredictedposition1: TMenuItem;
    OSMmap1: TMenuItem;
    load1: TMenuItem;
    save1: TMenuItem;
    OpenDialog1: TOpenDialog;
    SaveDialog1: TSaveDialog;
    ToolBar1: TToolBar;
    ToolButton1: TToolButton;
    ImageList1: TImageList;
    ToolButton2: TToolButton;
    ToolButton3: TToolButton;
    ToolButton4: TToolButton;
    ToolButton5: TToolButton;
    ToolButton6: TToolButton;
    ToolButton7: TToolButton;
    savedef: TMenuItem;
    loaddefault1: TMenuItem;
    Afrika1: TMenuItem;
    Asia1: TMenuItem;
    Antarctika1: TMenuItem;
    Northamerica1: TMenuItem;
    Southamerica1: TMenuItem;
    Asiawest1: TMenuItem;
    Asiacentral1: TMenuItem;
    cleanuposm1: TMenuItem;
    about1: TMenuItem;
    AirportILS1: TMenuItem;
    colorbyaltitude1: TMenuItem;
    Coordinates1: TMenuItem;
    N5112341: TMenuItem;
    N5111221: TMenuItem;
    N5111222: TMenuItem;
    GroundRADAR1: TMenuItem;
    GroundRADAR2: TMenuItem;
    adsbPIC1: TMenuItem;
    Urefoffset1: TMenuItem;
    N40mV1: TMenuItem;
    N60mV1: TMenuItem;
    N80mV1: TMenuItem;
    N100mV1: TMenuItem;
    N120mV1: TMenuItem;
    N140mV1: TMenuItem;
    N160mV1: TMenuItem;
    N180mV1: TMenuItem;
    N200mV1: TMenuItem;
    Reset1: TMenuItem;
    activateBootloader1: TMenuItem;
    off1: TMenuItem;
    TestPWM501: TMenuItem;
    TestPWM51: TMenuItem;
    ButtonStart: TButton;
    ButtonStop: TButton;
    showReply1: TMenuItem;
    all1: TMenuItem;
    N11: TMenuItem;
    N12: TMenuItem;
    N21: TMenuItem;
    N31: TMenuItem;
    N41: TMenuItem;
    Internet1: TMenuItem;
    sprut1: TMenuItem;
    Airframesorg1: TMenuItem;
    ServerSocket: TServerSocket;
    ToolButton8: TToolButton;
    ToolButton9: TToolButton;
    ToolButton10: TToolButton;
    ClientSocket: TClientSocket;
    TimeLabel: TLabel;
    filter1: TMenuItem;
    frameratehistory1: TMenuItem;
    ToolButton11: TToolButton;
    ToolButton12: TToolButton;
    usetimetag1: TMenuItem;
    Network1: TMenuItem;
    Networksetting1: TMenuItem;
    Serveractive1: TMenuItem;
    RAWClientactive1: TMenuItem;
    gotoPrague1: TMenuItem;
    ToolButton13: TToolButton;
    ToolButton14: TToolButton;
    TAGfrequency1: TMenuItem;
    N12MHz1: TMenuItem;
    N20MHz1: TMenuItem;
    defaults1: TMenuItem;
    ToolButton15: TToolButton;
    ToolButton16: TToolButton;
    ToolButton17: TToolButton;
    Button3: TButton;
    downloadmaps1: TMenuItem;
    Africa1: TMenuItem;
    Antarctica1: TMenuItem;
    Asia2: TMenuItem;
    Europe2: TMenuItem;
    Gus2: TMenuItem;
    Northamerica2: TMenuItem;
    Southamerica2: TMenuItem;
    Zip1: TZip;
    NorthamericaUSA1: TMenuItem;
    NorthamericaCarib2: TMenuItem;
    Button4: TButton;
    Button5: TButton;
    N1: TMenuItem;
    N2: TMenuItem;
    N3: TMenuItem;
    N4: TMenuItem;
    N6: TMenuItem;
    ComLed3: TComLed;
    ComLed4: TComLed;
    Background1: TMenuItem;
    ServerSocketRAW: TServerSocket;
    RAWServeractive1: TMenuItem;
    ToolButton19: TToolButton;
    RS232speed1: TMenuItem;
    N115kbit1: TMenuItem;
    N1Mbit1: TMenuItem;
    RS232polarity1: TMenuItem;
    withdriver1: TMenuItem;
    withoutdriver1: TMenuItem;
    N19kbit1: TMenuItem;
    Label7: TMenuItem;
    N2lines1: TMenuItem;
    N3lines1: TMenuItem;
    Button6: TButton;
    Button7: TButton;
    Button8: TButton;
    Button9: TButton;
    N4lines1: TMenuItem;
    TransitionAltitudeFL1: TMenuItem;
    N5000ft1: TMenuItem;
    N6000ft1: TMenuItem;
    N7000ft1: TMenuItem;
    N8000ft1: TMenuItem;
    Heading1: TMenuItem;
    N03591: TMenuItem;
    N03601: TMenuItem;
    ConnectatStart1: TMenuItem;
    Watchlist1: TMenuItem;
    ToolButton20: TToolButton;
    TimeGauge: TGauge;
    TimeKorrLabel: TLabel;
    N0Lines: TMenuItem;
    Symbol1: TMenuItem;
    circle1: TMenuItem;
    rectangle1: TMenuItem;
    ToolButton21: TToolButton;
    ToolButton22: TToolButton;
    BigTable1: TMenuItem;
    lockedtomainwindow1: TMenuItem;
    activate1: TMenuItem;
    manageunknownaircraft1: TMenuItem;
    ToolButton23: TToolButton;
    enableheartbeat1: TMenuItem;
    ToolButton24: TToolButton;
    decoderstatus1: TMenuItem;
    allairtargetswithin180NM1: TMenuItem;
    allsurfacetargetswithin45NM1: TMenuItem;
    Button10: TButton;
    Label8: TLabel;
    ToolButton25: TToolButton;
    ToolButton26: TToolButton;
    setReceiverLocation1: TMenuItem;
    gotoReceiverlocation1: TMenuItem;
    Receiver1: TMenuItem;
    Receiver2: TMenuItem;
    ToolButton18: TToolButton;
    maximumrange1: TMenuItem;
    maximumRange2: TMenuItem;
    MaximumRange3: TMenuItem;
    showMaximumonly1: TMenuItem;
    showbyaltitude1: TMenuItem;
    N250mV1: TMenuItem;
    closetargets1: TMenuItem;
    N7: TMenuItem;
    N10000ft1: TMenuItem;
    N10000200001: TMenuItem;
    N20000ft30000ft1: TMenuItem;
    N30000ft1: TMenuItem;
    showMinimumonly1: TMenuItem;
    ImageListL: TImageList;
    aircraft2: TMenuItem;
    ImageListM: TImageList;
    ImageListS: TImageList;
    ImageListF: TImageList;
    Textoverlay1: TMenuItem;
    ToolButton27: TToolButton;
    GPXOverlay1: TMenuItem;
    GPXOverlay2: TMenuItem;
    SRTMBackground1: TMenuItem;
    ProgressBar1: TProgressBar;
    objectN7TMenuItem1: TMenuItem;
    cleanupsrtm1: TMenuItem;
    decoder1: TMenuItem;
    adsbPIC2: TMenuItem;
    GNS58901: TMenuItem;
    rxControl1: TMenuItem;
    Beast1: TMenuItem;
    N8: TMenuItem;
    selcetCOMport1: TMenuItem;
    Beast2: TMenuItem;
    enabletimetag1: TMenuItem;
    onlyDF11171: TMenuItem;
    noCRCcheck1: TMenuItem;
    suppressDF0451: TMenuItem;
    N9: TMenuItem;
    connect1: TMenuItem;
    disconnect1: TMenuItem;
    binaryformat1: TMenuItem;
    binarydataformat1: TMenuItem;
    Debug01: TMenuItem;
    Debug11: TMenuItem;
    AutomaticTest1: TMenuItem;
    N50mV1: TMenuItem;
    N70mV1: TMenuItem;
    N90mV1: TMenuItem;
    N110mV1: TMenuItem;
    N130mV1: TMenuItem;
    N150mV1: TMenuItem;
    N170mV1: TMenuItem;
    N190mV1: TMenuItem;
    automatic1: TMenuItem;
    N10: TMenuItem;
    Textvoverlay1: TMenuItem;
    backgroundnottransparent1: TMenuItem;
    backgroundpicture1: TMenuItem;
    SRTM1: TMenuItem;
    MapQuestOSM1: TMenuItem;
    MapQuestAerial1: TMenuItem;
    OpenStreetMap1: TMenuItem;
    N13: TMenuItem;
    N14: TMenuItem;
    N15: TMenuItem;
    randomcolor1: TMenuItem;
    AircraftLabel1: TMenuItem;
    N16: TMenuItem;
    hideitafter20sec1: TMenuItem;
    Timer2: TTimer;
    Towns3: TMenuItem;
    N1000001: TMenuItem;
    N5000001: TMenuItem;
    N1Million1: TMenuItem;
    N3Million1: TMenuItem;
    gotoTown1: TMenuItem;
    Log1: TMenuItem;
    use1: TMenuItem;
    selectCOMport1: TMenuItem;
    ComPort2: TComPort;
    Timer3: TTimer;
    showData1: TMenuItem;
    positiononstartup1: TMenuItem;
    defaultposition1: TMenuItem;
    lastposition1: TMenuItem;
    DebugFill1: TMenuItem;
    N17: TMenuItem;
    I2C1: TMenuItem;
    donotuse1: TMenuItem;
    C2308C8E001: TMenuItem;
    other1: TMenuItem;
    N18: TMenuItem;
    setupindovidualcode1: TMenuItem;
    Navigation1: TMenuItem;
    N19: TMenuItem;
    procedure LoadAllMaps(dir :string);
    procedure LoadStates(filename: string);
    procedure LoadBigMap(filename: string);
    procedure LoadMap(filename: string);
    procedure sincos(var P:Tkoordinaten);
    function rad2instr(inrad:real; wo:integer):string;
    function asc2int(z : char):byte;
    function koordtostr(x: real):string;
    function koordtostr_Grad(x: real):string;
    procedure makeBM(breite, hoehe : integer; erase :boolean);
    function findairframe(AA: dword):Tairframe;
    procedure reportufo(AA:dword);
    procedure reportseen(AA:dword; RR:string; TS:string; TL:string; IO:char; ID:string);
    procedure reportselect(AA:dword; RR:string; TS:string; TL:string);
    procedure LoadTowns;
    procedure LoadPorts(filename: string);
    function FindAirport(name : string):integer;
    function rwkoord(st : string):Tkoordinaten;
    procedure Load_apt_out(filename: string);
    procedure load_all_apt_out;
    procedure Load_ils_out(filename: string);
    procedure load_all_ils_out;
    procedure Load_ats_out(filename: string; anfang : boolean);
    procedure load_all_ats_out;
    procedure Loadicao24;
    procedure LoadIFF;
    procedure LoadJets(zusatz : boolean);
    procedure movepunkt(var P : TKoordinaten);
    procedure guesposition;
    procedure move;
    procedure MakeLatLong;
    procedure zeichne;
    procedure clrscr(bm: TBitmap);
    procedure fillscr(bm: TBitmap; col :Tcolor);
    procedure Button1Click(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure disconnect;
    procedure connect;
    procedure Sende_Empfange(N,M :byte);
    procedure RadioGroup1Click(Sender: TObject);
    procedure DecoderPause;
    procedure DecoderRestart;
    Function AA2Land(AA : dword):string;
    function GetZeitmarke(Pstr : PTrxString; zstH, zstL : string): TDatetime;
    function TAG2Time(FrameZeit : TDatetime ; Pstr : PTrxString): TDatetime;
    procedure getnextframe(newframe : TFrame);
    procedure Buffer2Frames(nr_rx:integer; datasource:byte; Pstr:PTrxString);
    procedure BinBuffer2Frames(nr_rx:integer; datasource:byte; Pstr:PTrxString);
    procedure Workoff;
    procedure WorkoffNet;
    procedure Timer1Timer(Sender: TObject);
    procedure makedir(name : string);
    procedure Checkcontinents;
    procedure FormCreate(Sender: TObject);
    procedure kommandozeile;
    procedure LoadInit;
    procedure SaveIntStat;
    procedure SaveInit;
    procedure load_Eu_west(Sender: TObject);
    procedure load_Eu_UK(Sender: TObject);
    procedure load_Eu_North(Sender: TObject);
    procedure load_Eu_South(Sender: TObject);
    procedure load_Eu_East(Sender: TObject);
    procedure load_Russia(Sender: TObject);
    procedure load_Eu_balkan(Sender: TObject);
    procedure Decode;
    procedure Airplot(nr:integer);
    function make_Table : integer;
    procedure repaintPPI(Bmove : boolean; newBM : boolean);
    procedure TrackBar1Change(Sender: TObject);
    procedure Grid1Click(Sender: TObject);
    procedure Towns1Click(Sender: TObject);
    procedure Statenames1Click(Sender: TObject);
    procedure Crosshair1Click(Sender: TObject);
    procedure Detailedmaps1Click(Sender: TObject);
    procedure States1Click(Sender: TObject);
    procedure End1Click(Sender: TObject);
    procedure MenuColorBitmapUpdate(MenuePunkt:TMenuItem; MenueFarbe:TColor);
    procedure UpdateAllMenuColors;
    procedure Borders1Click(Sender: TObject);
    procedure Grid2Click(Sender: TObject);
    procedure Crosshair2Click(Sender: TObject);
    procedure Statenames2Click(Sender: TObject);
    procedure Towns2Click(Sender: TObject);
    procedure Airports1Click(Sender: TObject);
    procedure Image1MouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure Image1MouseUp(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure Rotation(var lat, lon : real; mode : integer);
    function xy2koord(x, y : integer):Tkoordinaten;
    procedure Image1MouseMove(Sender: TObject; Shift: TShiftState; X,
      Y: Integer);
    procedure Airports2Click(Sender: TObject);
    procedure AirportAltitude1Click(Sender: TObject);
    procedure Aircraft1Click(Sender: TObject);
    procedure RangeCircles1Click(Sender: TObject);
    procedure normal2Click(Sender: TObject);
    procedure ICAO2Click(Sender: TObject);
    procedure IATA2Click(Sender: TObject);
    procedure none2Click(Sender: TObject);
    procedure N50NM1Click(Sender: TObject);
    procedure N50km1Click(Sender: TObject);
    procedure N5005ftaltitude1Click(Sender: TObject);
    procedure FormResize(Sender: TObject);
    procedure after20s1Click(Sender: TObject);
    procedure after1min1Click(Sender: TObject);
    procedure after5min1Click(Sender: TObject);
    procedure after30min1Click(Sender: TObject);
    procedure after1hour1Click(Sender: TObject);
    procedure never1Click(Sender: TObject);
    procedure ATSRoutes1Click(Sender: TObject);
    procedure StringGrid1SelectCell(Sender: TObject; ACol, ARow: Integer;
      var CanSelect: Boolean);
    procedure KillCommunication;
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure ATSRoutes2Click(Sender: TObject);
    procedure removeallMaps1Click(Sender: TObject);
    procedure Label3Click(Sender: TObject);
    procedure Label6Click(Sender: TObject);
    function osm_CleanUp:integer;
    function osm_koord2tile(lat, long : real; zoom : integer): Tosmtile;
    procedure osm_tile2koord(var tile : Tosmtile);
    procedure N001Click(Sender: TObject);
    procedure loadOSM(zoom : integer);
    procedure OSMbackground1Click(Sender: TObject);
    procedure fullcolor1Click(Sender: TObject);
    procedure pale1Click(Sender: TObject);
    procedure pale2Click(Sender: TObject);
    procedure gray1Click(Sender: TObject);
    procedure rangering1Click(Sender: TObject);
    procedure showpredictedposition1Click(Sender: TObject);
    procedure OSMmap1Click(Sender: TObject);
    procedure save1Click(Sender: TObject);
    procedure saveposition(PosNumber: integer);
    procedure load1Click(Sender: TObject);
    procedure loadposition(PosNumber: integer);
    procedure ToolButton5Click(Sender: TObject);
    procedure ToolButton6Click(Sender: TObject);
    procedure ToolButton1Click(Sender: TObject);
    procedure ToolButton3Click(Sender: TObject);
    procedure ToolButton4Click(Sender: TObject);
    procedure ToolButton2Click(Sender: TObject);
    procedure savedefClick(Sender: TObject);
    procedure loaddefault1Click(Sender: TObject);
    procedure Afrika1Click(Sender: TObject);
    procedure Asia1Click(Sender: TObject);
    procedure Antarctika1Click(Sender: TObject);
    procedure Northamerica1Click(Sender: TObject);
    procedure Southamerica1Click(Sender: TObject);
    procedure Asiawest1Click(Sender: TObject);
    procedure Asiacentral1Click(Sender: TObject);
    procedure cleanuposm1Click(Sender: TObject);
    procedure about1Click(Sender: TObject);
    procedure AirportILS1Click(Sender: TObject);
    procedure colorbyaltitude1Click(Sender: TObject);
    procedure N5111222Click(Sender: TObject);
    procedure N5112341Click(Sender: TObject);
    procedure N5111221Click(Sender: TObject);
    procedure GroundRADAR1Click(Sender: TObject);
    procedure GroundRADAR2Click(Sender: TObject);
    procedure N100mV1Click(Sender: TObject);
    procedure setAgcOffset(offset : byte);
    function readAgcOffset:byte;
    procedure readRS232mode;
    procedure decoderSetPwm(dc:integer);
    procedure decoderReset;
    procedure N40mV1Click(Sender: TObject);
    procedure N60mV1Click(Sender: TObject);
    procedure N80mV1Click(Sender: TObject);
    procedure N120mV1Click(Sender: TObject);
    procedure N140mV1Click(Sender: TObject);
    procedure N160mV1Click(Sender: TObject);
    procedure N180mV1Click(Sender: TObject);
    procedure N200mV1Click(Sender: TObject);
    procedure Reset1Click(Sender: TObject);
    procedure activateBootloader1Click(Sender: TObject);
    procedure DecoderSetRS232Speed(speed:byte);
    procedure DecoderSetRS232Pol(polaritaet:byte);
    procedure off1Click(Sender: TObject);
    procedure TestPWM501Click(Sender: TObject);
    procedure TestPWM51Click(Sender: TObject);
    procedure ButtonStartClick(Sender: TObject);
    procedure ButtonStopClick(Sender: TObject);
    procedure GetDecoderType;
    procedure DecoderDependend(x : integer);
    procedure makeIffMasks;
    procedure N41Click(Sender: TObject);
    procedure N11Click(Sender: TObject);
    procedure all1Click(Sender: TObject);
    procedure N12Click(Sender: TObject);
    procedure N21Click(Sender: TObject);
    procedure N31Click(Sender: TObject);
    procedure sprut1Click(Sender: TObject);
    procedure Airframesorg1Click(Sender: TObject);

    procedure ServerStartStop;
    procedure ServerSocketClientConnect (Sender: TObject; Socket: TCustomWinSocket);
    procedure ServerSocketClientDisconnect(Sender: TObject; Socket: TCustomWinSocket);
    procedure ServerSocketClientRead (Sender: TObject; Socket: TCustomWinSocket);
    procedure ServerSendAir(nr:integer);
    procedure ServerSendId(nr:integer);
    procedure ServerSendMSG(nr:integer; Transtyp:integer);
    procedure ServerSend2All(nachricht:string);
    procedure ToolButton10Click(Sender: TObject);

    procedure RawClientStartStop;
    procedure RawClientReconnect;
    procedure ClientSocketLookup(Sender: TObject; Socket: TCustomWinSocket);
    procedure ClientSocketConnecting(Sender: TObject; Socket: TCustomWinSocket);
    procedure ClientSocketConnect (Sender: TObject; Socket: TCustomWinSocket);
    procedure ClientSocketError (Sender: TObject; Socket: TCustomWinSocket;
                                       ErrorEvent: TErrorEvent; var ErrorCode: Integer);
    procedure ClientSocketDisconnect(Sender: TObject; Socket: TCustomWinSocket);
    procedure ClientSocketRead (Sender: TObject; Socket: TCustomWinSocket);
    procedure ServerSocketClientError(Sender: TObject;
      Socket: TCustomWinSocket; ErrorEvent: TErrorEvent;
      var ErrorCode: Integer);
    procedure Memo1Click(Sender: TObject);
    procedure filter1Click(Sender: TObject);
    procedure frameratehistory1Click(Sender: TObject);
    procedure ToolButton11Click(Sender: TObject);
    procedure Image1DblClick(Sender: TObject);
    procedure ToolButton12Click(Sender: TObject);
    procedure usetimetag1Click(Sender: TObject);
    procedure Networksetting1Click(Sender: TObject);
    procedure Serveractive1Click(Sender: TObject);
    procedure RAWClientactive1Click(Sender: TObject);
    procedure gotoPrague1Click(Sender: TObject);
    procedure gotoOrt(lat, long :real);
    procedure NewReceiverPosition(ZentL,ZentB: real);
    procedure ToolButton13Click(Sender: TObject);
    procedure N12MHz1Click(Sender: TObject);
    procedure N20MHz1Click(Sender: TObject);
    procedure defaults1Click(Sender: TObject);
    procedure ToolButton16Click(Sender: TObject);
    procedure ToolButton17Click(Sender: TObject);
    procedure Button3Click(Sender: TObject);
    function zipexpand(zipfilename, zippfad : string):integer;
    procedure DownloadMaps(was, wohin : string);
    procedure Africa1Click(Sender: TObject);
    procedure Antarctica1Click(Sender: TObject);
    procedure Asia2Click(Sender: TObject);
    procedure Europe2Click(Sender: TObject);
    procedure Gus2Click(Sender: TObject);
    procedure Northamerica2Click(Sender: TObject);
    procedure Southamerica2Click(Sender: TObject);
    procedure NorthamericaUSA1Click(Sender: TObject);
    procedure NorthamericaCarib2Click(Sender: TObject);
    procedure Button4Click(Sender: TObject);
    procedure Mapout;
    procedure Button5Click(Sender: TObject);
    procedure Shrink;
    procedure Background1Click(Sender: TObject);
    procedure ServerSocketRAWClientConnect(Sender: TObject;
      Socket: TCustomWinSocket);
    procedure ServerSocketRAWClientDisconnect(Sender: TObject;
      Socket: TCustomWinSocket);
    procedure ServerSocketRAWClientError(Sender: TObject;
      Socket: TCustomWinSocket; ErrorEvent: TErrorEvent;
      var ErrorCode: Integer);
    procedure ServerSocketRAWClientRead(Sender: TObject;
      Socket: TCustomWinSocket);
    procedure RAWServerStartStop;
    procedure RAWServeractive1Click(Sender: TObject);
    procedure RAWServerSend2All(nachricht: string);
    procedure ToolButton19Click(Sender: TObject);
    procedure N115kbit1Click(Sender: TObject);
    procedure N1Mbit1Click(Sender: TObject);
    procedure withdriver1Click(Sender: TObject);
    procedure withoutdriver1Click(Sender: TObject);
    procedure N19kbit1Click(Sender: TObject);
    procedure N2lines1Click(Sender: TObject);
    procedure N3lines1Click(Sender: TObject);
    procedure N4lines1Click(Sender: TObject);
    procedure N5000ft1Click(Sender: TObject);
    procedure N6000ft1Click(Sender: TObject);
    procedure N7000ft1Click(Sender: TObject);
    procedure N8000ft1Click(Sender: TObject);
    procedure Button6Click(Sender: TObject);
    procedure Button7Click(Sender: TObject);
    procedure Button8Click(Sender: TObject);
    procedure Button9Click(Sender: TObject);
    procedure StringGrid1MouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
    procedure N03601Click(Sender: TObject);
    procedure N03591Click(Sender: TObject);
    procedure ConnectatStart1Click(Sender: TObject);
    procedure Watchlist1Click(Sender: TObject);
    procedure WatchlistCheckAllflights(Sender: TObject);
    function WatchlistCheckAllPlane:string;
    procedure N0LinesClick(Sender: TObject);
    procedure circle1Click(Sender: TObject);
    procedure rectangle1Click(Sender: TObject);
    procedure ToolButton21Click(Sender: TObject);
    procedure activate1Click(Sender: TObject);
    procedure lockedtomainwindow1Click(Sender: TObject);
    procedure manageunknownaircraft1Click(Sender: TObject);
    procedure enableheartbeat1Click(Sender: TObject);
    procedure decoderstatus1Click(Sender: TObject);
    procedure allairtargetswithin180NM1Click(Sender: TObject);
    procedure allsurfacetargetswithin45NM1Click(Sender: TObject);
    procedure Button10Click(Sender: TObject);
    procedure ToolButton25Click(Sender: TObject);
    procedure ToolButton26Click(Sender: TObject);
    procedure setReceiverLocation1Click(Sender: TObject);
    procedure gotoReceiverlocation1Click(Sender: TObject);
    procedure Receiver1Click(Sender: TObject);
    procedure Receiver2Click(Sender: TObject);
    procedure ToolButton18Click(Sender: TObject);
    procedure maximumrange1Click(Sender: TObject);
    procedure maximumRange2Click(Sender: TObject);
    procedure showMaximumonly1Click(Sender: TObject);
    procedure showbyaltitude1Click(Sender: TObject);
    procedure N250mV1Click(Sender: TObject);
    procedure closetargets1Click(Sender: TObject);
    procedure N10000ft1Click(Sender: TObject);
    procedure N10000200001Click(Sender: TObject);
    procedure N20000ft30000ft1Click(Sender: TObject);
    procedure N30000ft1Click(Sender: TObject);
    procedure Memo2Click(Sender: TObject);
    procedure showMinimumonly1Click(Sender: TObject);
    procedure aircraft2Click(Sender: TObject);
    procedure Textoverlay1Click(Sender: TObject);
    procedure ToolButton27Click(Sender: TObject);
    procedure GPXOverlay1Click(Sender: TObject);
    procedure GPXOverlay2Click(Sender: TObject);
    procedure MakeSRTM;
    function srtm_CleanUp:integer;
    procedure SRTMBackground1Click(Sender: TObject);
    procedure cleanupsrtm1Click(Sender: TObject);
    procedure adsbPIC2Click(Sender: TObject);
    procedure GNS58901Click(Sender: TObject);
    procedure rxControl1Click(Sender: TObject);
    procedure Beast1Click(Sender: TObject);
    procedure selcetCOMport1Click(Sender: TObject);
    procedure BeastControl(st : string);
    procedure BeastControlC;
    procedure BeastControlD;
    procedure BeastControlE;
    procedure BeastControlF;
    procedure BeastControlG;
    procedure enabletimetag1Click(Sender: TObject);
    procedure onlyDF11171Click(Sender: TObject);
    procedure noCRCcheck1Click(Sender: TObject);
    procedure suppressDF0451Click(Sender: TObject);
    procedure connect1Click(Sender: TObject);
    procedure disconnect1Click(Sender: TObject);
    procedure binaryformat1Click(Sender: TObject);
    procedure binarydataformat1Click(Sender: TObject);
    procedure Debug01Click(Sender: TObject);
    procedure Debug11Click(Sender: TObject);
    procedure AutomaticTest1Click(Sender: TObject);
    procedure N50mV1Click(Sender: TObject);
    procedure N70mV1Click(Sender: TObject);
    procedure N90mV1Click(Sender: TObject);
    procedure N110mV1Click(Sender: TObject);
    procedure N130mV1Click(Sender: TObject);
    procedure N150mV1Click(Sender: TObject);
    procedure N170mV1Click(Sender: TObject);
    procedure N190mV1Click(Sender: TObject);
    procedure automatic1Click(Sender: TObject);
    procedure backgroundnottransparent1Click(Sender: TObject);
    procedure SRTM1Click(Sender: TObject);
    procedure MapQuestOSM1Click(Sender: TObject);
    procedure MapQuestAerial1Click(Sender: TObject);
    procedure OpenStreetMap1Click(Sender: TObject);
    procedure randomcolor1Click(Sender: TObject);
    procedure AircraftLabel1Click(Sender: TObject);
    procedure hideitafter20sec1Click(Sender: TObject);
    procedure Timer2Timer(Sender: TObject);
    procedure ComPort1BeforeClose(Sender: TObject);
    procedure N1000001Click(Sender: TObject);
    procedure N5000001Click(Sender: TObject);
    procedure N1Million1Click(Sender: TObject);
    procedure N3Million1Click(Sender: TObject);
    procedure gotoTown1Click(Sender: TObject);
    procedure Log1Click(Sender: TObject);

    procedure StartLog;
    procedure StopLog;
    procedure writeLog(st : string);
    procedure writeLnLog(st : string);
    procedure StartErrorLog;
    procedure StopErrorLog;
    procedure Elog(st:string);
    procedure StartUsbLog;
    procedure StopUsbLog;
    procedure use1Click(Sender: TObject);
    procedure selectCOMport1Click(Sender: TObject);
    procedure Timer3Timer(Sender: TObject);
    procedure WorkoffGPS;
    procedure showData1Click(Sender: TObject);
    procedure defaultposition1Click(Sender: TObject);
    procedure lastposition1Click(Sender: TObject);
    procedure TestFill;
    procedure TestFill2;
    procedure DebugFill1Click(Sender: TObject);
    procedure donotuse1Click(Sender: TObject);
    procedure C2308C8E001Click(Sender: TObject);
    procedure other1Click(Sender: TObject);
    procedure i2c2tuner;
    procedure setupindovidualcode1Click(Sender: TObject);
    //procedure BtSendenClick(Sender: TObject);


  private
    { Private-Deklarationen }
    BM    : Tbitmap;
    BM2   : Tbitmap;  // puffert BM
    ClientVerbunden : Boolean;
  public
    { Public-Deklarationen }
    Pfad            : string;
    Planes          : array[0..maxPlane] of TPlane;
    NullZeitmarke   : TDatetime;
    Zeitkorrektur   : integer;
    Frameraten      : array[0..1000] of TFramerate;   //8,h stunden
    Frameratenindex : integer;
    framerateactive : boolean;
    LocalRawOnly    : boolean;    // RAW server sendet nur daten vom lokalen decoder
    asc2intfehler   : boolean;
    Watchlist       : array[0..maxWatchList] of TWatchList;
    procedure memo2_lines_add(st:string);
    procedure DisplayHint(Sender: TObject);
  end;



  Tosm = record
         BM     : Tbitmap;
         ZentB  : real;  //radiant
         ZentL  : real;
         Scale  : real;
         pos    : integer; // position des zoom-reglers
         xoff   : integer;
         yoff   : integer;
         OK     : boolean;
         formwidth  : integer;
         formheight : integer;
         formtop    : integer;
         formleft   : integer;
       end;

  Tinfile = text;    //textfile

  TTownKoordinaten = record
           Koord    : Tkoordinaten;
           Name     : char;        // Anfangsbuchstabe
           pop      : real;        // Bevölkerung in Millionen des Gebiets
           popcore  : real;        // Bevölkerung des Stadtbereichs
           langname : string[32];  // Name der Stadt
         end;

  TLandKoordinaten = record
           Koord    : Tkoordinaten;
           name     : string;       // Name des Landes
           punkte   : integer;
           anstrich : real;
         end;

  PVecarray  = array[0..2000000] of TKoordinaten;

  Trunway = record
           name    : string[4];     // wenn nicht existier =''
           A ,B    : Tkoordinaten;  // anfangs und Endpunkt
           typ     : integer;
         end;

  Tils = record
           name    : string[4];     // wenn nicht existier =''
           A1,B1   : Tkoordinaten;  // anfangs und Endpunkt
           A2,B2   : Tkoordinaten;  // anfangs und Endpunkt
           A3,B3   : Tkoordinaten;  // anfangs und Endpunkt
           typ     : integer;
         end;

  Tairport = record                  // Flughafen
           icao    : string[7];      // XXXX
           iata    : string[7];      // XXX
           name    : string[31];
           town    : string[31];
           country : string[31];
           koord   : Tkoordinaten;
           rw      : array[0..5] of Trunway;  // max 6 runways
           ils     : array[0..9] of Tils;     // max 10 ils
         end;

  TKreuz = record
           X       : integer;
           Y       : integer;
           v       : boolean;
           Nr      : integer;
           counter : integer;
         end;

  Tiff = record
           name    : string[15];     // wenn nicht existier =''
           koord   : Tkoordinaten;
           code    : integer;
         end;

  Ticao = record
          anfang   : integer;
          ende     : integer;
          name     : string[31];
        end;

  TLandMass = array[-180..179, -60..60] of byte;  // 43 kbyte


var
  Form1: TForm1;
  PortMap : TPortMap;    // in unit uportmap

  Root             : boolean = false;      // Warnung mei OSM-Download abschalten
  TableFormPos     : TFormPos;
  FilterFormPos    : TFormPos;
  WatchListFormPos : TFormPos;
  AAmanagerPos     : TFormPos;
  HeartBeatFormPos : TFormPos;
  rxstr            : TrxString;     //AnsiString;
  st_gps           : string;
  Frames           : array[0..maxFrame] of TFrame;  // maxFrame x 24 byte = 24k
  FramePointer     : integer = 0;
  LastFrame        : integer = 0;
  LastPlane        : integer = 1;
  sg_update        : integer = 0;
  framerate_update : integer = 0;
  repaint_update   : integer = 0;
  zeit_update      : integer = 0;
  client_update    : integer = 0;
  atest_update     : integer = 0;
  agc_update       : integer = 0;
  flights_update   : integer = 0;
  watch_update     : integer = 0;
  activeTracks     : integer = 0;
  Framecounter     : integer = 0;
  HBFramecounter   : integer = 0;
  CRCfehler        : integer = 0;
  ATestNeuCounter  : integer = 0;
  ATestAltCounter  : integer = 0;
  ATestActive      : boolean = false;       // neue Version
  Framemesszeit    : TDateTime;
  TagFrequenz      : integer = 12000000;     // Frequenz der genauen Zeitmarken
  HeadingPlusMinus : boolean = false;        // 0 ... 360
  Memo2visible     : boolean = false;

  Logon            : boolean = false;
  ErrorLogon       : boolean = false;
  USBLogon         : boolean = false;
  LogSel           : boolean = false;
  LogDet           : boolean = false;
  Logfile          : Tinfile;
  errorlogfile     : Tinfile;
  WatchLogfile     : Tinfile;
  USBLogFile       : Tinfile;
  LogFileName      : string;
  ErrorLogFileName : string = '';
  USBLogFileName   : string;
  LogTyp           : integer = 0;
  LogLong          : integer = 0;
  LogString        : string = '';
  
  htmlpath         : string;
  ListAllFrames    : boolean = false;        //memo2 per default off
  ListDecodedData  : boolean = true;
  MS               : TMatrix;
  GPSdata1         : TGps;
  i2cdata          : array[0..16] of byte;

  mapmode          : boolean = false;        // Bearbeiten von karten, wirkt auf osm-loeschen nach Verschiebung in Bild

  M        : PVecarray;                            //high res
  G        : array[0..200000] of TKoordinaten;     //lat long
  W        : array[0..20000]  of TKoordinaten;     //grobe Welt
  T        : array[0..maxTown]of TTownKoordinaten; //Städte
  L        : array[0..300]    of TLandKoordinaten; //Ländermittelpunkte
  Port     : array[0..maxPort]of Tairport;         //flughaefen
  Jet      : array[0..maxJet] of Tairframe;        //flugzeuge der welt :-)     100000
  ATS      : array[0..maxATS] of TKoordinaten;     //ssb1  31000 im eddk-file
  IFF      : array[0..maxIFF] of Tiff;             //sekundaerradare
  ICAO     : array[0..maxICAO]of Ticao;            //Länder                     200
  GPX      : array[0..maxGPX] of TKoordinaten;     //GPX-overlay
  rangering,
  rangeringmin: array[0..71, 0..3] of TRangePoint;    //maxi-/minimale Reichweite in km
  pcounter   : integer = 0;
  Gcounter   : integer = 0;
  wcounter   : integer = 0;
  tcounter   : integer = 0;  // Towncounter
  Lcounter   : integer = 0;
  Portcounter: integer = 0;     // flughaefen
  Jetcounter : integer = 0;
  JetSortcounter : integer = 0;
  ATScounter : integer = 0;
  GPXcounter : integer = 0;
  IFFcounter : integer = 0;
  icaocounter: integer = 0;
  TransitAlt : integer = 6000;	// uebergangshoehe zwischen Hoehenangabe in ft oder FL  #*#*#
  Status_HB  : integer = 0;
  Status_TAG : integer = 0;

  kennerdb      : array[0..200000] of Tkenner;
  interogatoren : array[0..maxInterogatoren] of integer;        //0000000 .. 1001111    0..79


  X, Y     : real;
  rusN,rusC, b1, b2    : real;
  LandL, LandB : real;
  Lmin     : real = -pi;        //8*rad;
  Lmax     : real =  pi;        //9*rad;
  Bmin     : real = -pi/2;      //53*rad;
  Bmax     : real = +pi/2;      //54*rad;
  ZentB    : real = 51*rad;     //53.5*rad;
  ZentL    : real = 6*rad;      //8.5*rad;
  Receiver : Tkoordinaten;      // position des ADS-B-Receivers
  sin_ZentB: real;
  cos_ZentB: real;
  Scale    : real = 100;
  Stretch      : integer = 1;     // Verhältnis von BM und Image1
  Pdreh        : Tkoordinaten;    // fuer Erddrehung mit der Maus
  hastomove    : boolean;         // koordinaten vor der anzeige berechnen
  maxtotzeit   : integer = 5*60;  // Sekunden bis zum Löschen nach Kontaktverlust
  Kreuz        : TKreuz;
  nearplane    : integer = -1;    // track dessen Daten in Grafik eingeblendet werden sollen

  borderColor  : TColor = clltGray;
  latlongColor : TColor = clltGray;
  townColor    : TColor = clltBlue;
  chairColor   : TColor = clltGreen;
  cooColor     : TColor = clBlue;
  ovlColor     : TColor = clBlue;
  stateColor   : TColor = clltBlue;
  portColor    : TColor = clltBlue1;
  AtsColor     : TColor = clltYellow;
  GpxColor     : TColor = clltRed;
  rrColor      : TColor = clltGreen;
  rxColor      : TColor = clltRed;
  rriColor     : TColor = clBlack;
  IffColor     : TColor = clltRed;
  BackColor    : TColor = clWhite;
  LabelColor   : TColor = clBlack;

//  osmserver    : string = 'http://tah.openstreetmap.org/Tiles/tile/';               //alt
  osmserver    : string = 'http://a.tile.openstreetmap.org/';                  //neu im test
  mpqserver    : string = 'http://otile1.mqcdn.com/tiles/1.0.0/osm/';
  airserver    : string = 'http://oatile1.mqcdn.com/naip/';

  mapserver    : string = 'ftp://maproom.dlt.psu.edu/alabama2pts.txt';
  osmOK        : boolean = false;
  OsmCacheMiss : boolean = false;
  osm          : Tosm;
  srtm         : Tosm;
  srtmOK       : boolean = false;
  srtmLandMass : TLandMass;           //maske existierender srtm-Tiles
  BsrtmServerOk: boolean = true;      //server ist erreichbar

  DecoderNr1   : integer = decoder_adsbPIC;   // angeschlossen laut Nutzer
  Decoder      : integer = decoder_adsbPIC;   // gefundener decoder
  agc_mode     : byte = 2;                    // aus dem decoder ausgelesener mode,  1= agc-on   0=agc-off
  agc_offset   : byte = 0;                    // aus dem decoder ausgelesener offset in mV
  agc_mitte    : byte = 0;
  AGCon        : boolean = false;             // Offset automatisch optimieren
  AgcCounter   : integer = 0;                 // Zyklenzähler fuer AGC
  AgcUpCounter : integer = 0;
  AgcDownCounter:integer = 0;
  AgcResult    : integer = 100;               // AGC-verbesserung
  AgcFrameCounter:integer = 0;

  dontchangeoffset : boolean = false;         // deaktiviert offsetaenderungen ueber das menue
  dontchangers232  : boolean = false;         // deaktiviert oRS232-aenderungen ueber das menue

  IffFilter        : integer = -1;
  IffMasks         : array[0..5] of integer;

  jmax, selAA      : integer;                               //DM61

  ClientNachricht  : string = '';
  clientstring     : TrxString;

  FullScreen       : boolean = false;   // maximiere Groesse der grafischen Ausgabe
  WinMaximized     : boolean = false;   // Programmfenster maximieren
  UseTimeTag       : boolean = false;   // aktiviere den 48-Bit Zeitcode beim adsbPIC-Decoder
  UseFrameNr       : boolean = false;   // aktiviere der Frame-Nummerierung beim adsbPIC-Decoder
  UseHeartBeat     : boolean = false;   // aktiviere des Decoder-Heartbeats beim adsbPIC-Decoder
  UseBinFormat     : boolean = false;   // verwende binary formnat anstelle von Text beim adsbPIC-Decoder
  NetUseBinFormat  : boolean = false;   // verwende binary formnat anstelle von Text beim network client

  bigmapfilename   : string = 'somewhere_borders.map';

  Sortierung       : integer = 0;      // spalte nach der in der Tasbelle sortiert wird   ACol;
  Sortierrichtung  : boolean = false;  // false=aufsteigend; true=absteigend
  AutoConnect      : boolean = false;  // beim start mit decoder verbinden
  Debugging        : boolean = false;  // Debuggingfeatures aktivieren

  BM2valid         : boolean = false;
  MakeNewBM        : boolean = true;
{
  DSU_count        : integer = 0;
  SSU_count        : integer = 0;
  Fail1_Count      : integer = 0;
  Fail2_Count      : integer = 0;
  Fail3_Count      : integer = 0;
  Zero_Count       : integer = 0;
}
implementation

uses filter, watchlist , table, amanager, hearbeat, iff1, ugpx, i2c;

{$R *.DFM}


//http://www.swissdelphicenter.ch/torry/showcode.php?id=412
function DownloadFile(SourceFile, DestFile: string): Boolean;
begin
  try
    Result := UrlDownloadToFile(nil, PChar(SourceFile), PChar(DestFile), 0, nil) = 0;
  except
    form1.elog('DownloadFile '+ SourceFile + ' -> ' + DestFile);
    Result := False;
  end;
end; //DownloadFile


// liefert Bezeichnung des interrogators
function ifftostr(iff:integer):string;
begin
  //SSS IIII
  //0..15 = II-Code0..15
  if iff<16 then result := 'II-'+inttostr(iff)
            else result := 'SI-'+inttostr(iff-16);
end;   //ifftostr


//wandelt positiven integer in oktalzahl
// stellen ist die Mindestlänge des strings (vorn mit nullen aufgefuellt)
function inttookt(zahl : integer; stellen : integer):string;
var maske : integer;
begin
  result:='';
  if zahl<0 then exit;
  repeat
    maske := zahl and 7;
    zahl  := zahl shr 3;
    result:= inttostr(maske)+result;
    dec(stellen);
  until (stellen<1) and (zahl=0);
end;


//wandelt Hoehe in Farbe
// input : fuss 0 .. 40000
//     0  rot      $0000FF
// 20000  gruen    $00FF00
// 40000  blau     $FF0000
function altitudecolor(hoehe : integer):Tcolor;
var r,g,b,step : integer;
begin
  if hoehe < 20000 then begin
    step := round(hoehe/20000*255);
    r := 255-step;
    g := step;
    b := 0;
  end else begin //  if hoehe >= 20000 then begin
    step := round((hoehe-20000)/20000*255);
    g := 255-step;
    b := step;
    r := 0;
  end;
  if hoehe < 0     then result := clRed  else
  if hoehe > 40000 then result := clBlue else
  result:=  r or (g shl 8) or (b shl 16);
end;  //  altitudecolor


procedure TForm1.memo2_lines_add(st:string);
begin
  if Memo2visible then memo2.lines.add(st);
end;


// Hilfeanzeige fuer Menue
procedure TForm1.DisplayHint(Sender: TObject);
begin
  if Application.Hint <> '' then
  begin
    StatusBar1.SimplePanel := True;
    StatusBar1.SimpleText  := Application.Hint;
  end
  else
    StatusBar1.SimplePanel := False;
end;


procedure TForm1.sincos(var P:Tkoordinaten);
begin
  P.cos_breite := cos(P.breite);
  P.sin_breite := sin(P.breite);
end;


// wandelt einen radiant wert in einen integer-grad-string
function TForm1.rad2instr(inrad:real; wo:integer):string;
var deg : integer;
begin
  // wo: 0-Server / 1-Bild / 2-Tabelle
  // input ist immer +- pi
  deg := round(inrad/rad + 360) mod 360;                  // 0 .. 360
  if HeadingPlusMinus and (deg>180) then deg := deg -360; // +- 180
  result := inttostr(deg);
end;


//wandelt ein hex-char in einen Wert 0..15
function TForm1.asc2int(z : char):byte;
begin
  result := 0;
//  if (z>='0') and (z<='9') then result:= ord(z)-ord('0');
//  if (z>='A') and (z<='F') then result:= ord(z)-ord('A') +10;
//  if (z>='a') and (z<='f') then result:= ord(z)-ord('a') +10;

  case z of
    '0'..'9': result:= ord(z)-ord('0');
    'A'..'F': result:= ord(z)-ord('A') +10;
    'a'..'f': result:= ord(z)-ord('a') +10;
     else asc2intfehler := true;
  end;
end;   //asc2int


// wandelt einen koordinatenwert (radiant) in einen String
function TForm1.koordtostr(x: real):string;
begin
  result := koordtostr_Grad(x/rad);
end;


// wandelt einen koordinatenwert (grad) in einen String
function TForm1.koordtostr_Grad(x: real):string;
begin
  if N5112341.checked then result := floattostrf(x,ffFixed,7,4)+'°';  // 51,1234

  if N5111222.checked then begin                                        //51 11,22'
    if x<0 then result:='-' else result := '';
    x := abs(x);
    result := result + inttostr(trunc(x))+'° '+  floattostrf((x-trunc(x))*60,ffFixed,4,2)+'''';
  end;

  if N5111221.checked then begin                                        // 51:11'22''
    if x<0 then result:='-' else result := '';
    x := abs(x);
    result := result + inttostr(trunc(x))+'° ';
    x:= (x-trunc(x)) * 60;
    result := result +  inttostr(trunc(x))+''' ';
    x:= (x-trunc(x)) * 60;
    result := result +  inttostr(trunc(x))+'''''';
  end;
end;  //koordtostr


// das Bitmap zum Zeichnen sowie fuer OSM erzeugen
// das erase wird nie benutzt
procedure TForm1.makeBM(breite, hoehe : integer; erase :boolean);
begin
  if erase then begin BM.Free; BM2.Free; end;
  BM := TBitmap.Create;
  BM.width  := breite;
  BM.height := hoehe;
  Stretch   := round(BM.width / image1.width);
  BM.canvas.pen.width := Stretch;
  BM.canvas.Font.size := round(8*Stretch);
  BM2 := TBitmap.Create;
  BM2.width  := BM.width;
  BM2.height := BM.height;
  //osm
  if erase then osm.BM.Free;
  osm.BM        := TBitmap.Create;
  osm.BM.width  := breite;
  osm.BM.height := hoehe;
  osmOK         := false;
  //srtm
  srtm.BM        := TBitmap.Create;
  srtm.BM.width  := breite;
  srtm.BM.height := hoehe;
  srtmOK         := false;
end; // makeBM


// identifizieren des Flugzeugs anhand des AA-Kenners
// file icao24.txt von  http://www.libhomeradar.org/download/index.php
// braucht bis zu 200 us bei 60000 AA
{
function TForm1.findairframe(AA: dword):Tairframe;
var k         : integer;
begin
  result.known := false;
  k:=-1 ;
  repeat
    inc(k);
    if jet[k].AA=AA then begin
      result:=jet[k];
      result.known:=true;
    end;
  until (result.known) or (k>=Jetcounter);
end;  //findairframe
}

// identifizieren des Flugzeugs anhand des AA-Kenners
// schnelle Version
// erfordert sortierte icao24plus.txt
function TForm1.findairframe(AA: dword):Tairframe;
var k, L, step  : integer;
begin
  result.known := false;
  //erst die unsortieren am Ende
  if (Jetcounter>JetSortCounter+1) then begin
    k:=JetSortCounter;
    repeat
      inc(k);
      if jet[k].AA=AA then begin
        result:=jet[k];
        result.known:=true;
      end;
    until (result.known) or (k>=Jetcounter);
  end;
  //nun die sortierten am Anfang
  if (not result.known) and (JetSortCounter>0) then begin
    step := JetSortCounter div 2;
    k    := 0;
    repeat
      k := k + step;
      // es kann passieren, dass man etwas oben oder unter herausrutscht !
      if k<0 then begin
        k    := 0;
        step := 0;
      end;
      if k>JetSortCounter then begin
        k    := JetSortCounter;
        step := 0;
      end;
      
      if jet[k].AA=AA then begin
        result:=jet[k];
        result.known:=true;
      end else begin
        if jet[k].AA<AA then step :=   abs(step div 2)+1
                        else step := - abs(step div 2)-1;
      end;
    until (result.known) or (abs(step) < 10);

    if (not result.known) then begin
      L:=0;
      repeat
        if (jet[k].AA<AA) and (k<Jetcounter) then inc(k);
        if (jet[k].AA>AA) and (k>0)          then dec(k);
        inc(L);
        if jet[k].AA=AA then begin
          result:=jet[k];
          result.known:=true;
        end;
      until (result.known) or (L>=30);
    end;
  end;
end;  //findairframe


//unbekanntes flugzeug eintragen
// in \extra\airframesunknown.txt
procedure TForm1.reportufo(AA:dword);
var ufofile    : Tinfile;
    filename   : string;
    a,b,c,d,e,f: char;
    AAA        : integer;
    gefunden   : boolean;
begin
  gefunden := false;
  filename := Pfad+'extra\airframesunknown.txt';
  assignfile(ufofile,filename);
  {$I-}
      reset(ufofile);
  {$I+}
  if IOResult <> 0 then begin
    FileMode := 2;        // Lesen/Schreiben
    rewrite(ufofile);
    closefile(ufofile);
  end else begin
    while not (eof(ufofile) or gefunden) do begin
      readln(ufofile, a,b,c,d,e,f);
      AAA := (asc2int(a) shl 20) or (asc2int(b) shl 16) or (asc2int(c) shl 12) or (asc2int(d) shl 8) or (asc2int(e) shl 4) or asc2int(f);
      if AA=AAA then gefunden:=true;
    end;
    closefile(ufofile);
  end;

  if not gefunden then begin
    Append(ufofile);
    write(ufofile, inttohex(((AA and $FF0000) shr 16),2), inttohex((AA and $00FFFF),4), ':');
    write(ufofile,AA2Land(AA), ':::');
    writeln(ufofile);
    closefile(ufofile);
  end;
end; //reportufo



// gesehene flugzeuge in \log\airframesseen.txt 
procedure TForm1.reportseen(AA:dword; RR:string; TS:string; TL:string; IO:char; ID:string);
var seenfile   : Tinfile;
    filename   : string;
    a,b,c,d,e,f: char;
    AAA        : integer;
    gefunden   : boolean;
begin
//  if pos(MarkerBS,RR)=0 then exit;
  if not LogDet then exit;
  gefunden := false;
  filename := Pfad+'log\airframesseen'+FormatDateTime('-yymmdd',Now)+'.txt';
  assignfile(seenfile,filename);
  {$I-}
      reset(seenfile);
  {$I+}
  if IOResult <> 0 then begin
    FileMode := 2;        // Lesen/Schreiben
    rewrite(seenfile);
    closefile(seenfile);
  end else closefile(seenfile);
  Append(seenfile);
  //3C4E09	D-ACPI	CRJ7	CL-600-2D15 CRJ-700	>18:26:21
  write(seenfile, inttohex(AA,6), Tab, RR, Tab, TS, Tab, TL, Tab, IO, ' ', FormatDateTime('hh:nn:ss',Now), Tab, ID);
  writeln(seenfile);
  closefile(seenfile);
end; //reportseen



// selektierte flugzeuge in \log\airframesselect.txt eintragen
procedure TForm1.reportselect(AA:dword; RR:string; TS:string; TL:string);
var selfile : Tinfile;
    filename   : string;
    a,b,c,d,e,f: char;
    AAA        : integer;
    gefunden   : boolean;
begin
//  if pos(MarkerBS,RR)=0 then exit;
  if not LogSel then exit;
  gefunden := false;
  filename := Pfad+'log\airframesselect'+FormatDateTime('-yymmdd',Now)+'.txt';
  assignfile(selfile,filename);
  {$I-}
      reset(selfile);
  {$I+}
  if IOResult <> 0 then begin
    FileMode := 2;        // Lesen/Schreiben
    rewrite(selfile);
    closefile(selfile);
  end else begin
    while not (eof(selfile) or gefunden) do begin
      readln(selfile, a,b,c,d,e,f);
      AAA := (asc2int(a) shl 20) or (asc2int(b) shl 16) or (asc2int(c) shl 12) or (asc2int(d) shl 8) or (asc2int(e) shl 4) or asc2int(f);
      if AA=AAA then gefunden:=true;
    end;
    closefile(selfile);
  end;

  if not gefunden then begin
    Append(selfile);
    write(selfile, inttohex(AA,6), Tab, RR, Tab, TS, Tab, TL, Tab, FormatDateTime('hh:nn:ss',Now));
    writeln(selfile);
    closefile(selfile);
  end;
end; //reportsel


// alle Maps eines Verzeichnisses einlesen
//http://www.maproom.psu.edu/dcw/
procedure TForm1.LoadAllMaps(dir :string);
var
    search   : TSearchRec;
    filename : string;
begin
  // wenn im Directory einzelkarten vorhanden sind
  // dann werdn nur diese geladen
  if FindFirst(dir+'\*pts.txt', faAnyFile, search)=0 then begin
    filename := dir+'\'+search.name ;
    LoadMap(filename);
    while FindNext(search) = 0 do begin
      filename := dir+'\'+search.name ;
      LoadMap(filename);
    end;
  end else begin
    // wenn keine einzelkarten, dann gesamtkarte
    // zuerst die Umrisse
    if FindFirst(dir+'\*_borders.map', faAnyFile, search)=0 then begin
      bigmapfilename := search.name;
      filename := dir+'\'+search.name ;
      LoadBigMap(filename);
    end;
    // und dann die Namen
    if FindFirst(dir+'\*_states.txt', faAnyFile, search)=0 then begin
      filename := dir+'\'+search.name ;
      LoadStates(filename);
    end;
  end;
  FindClose(search);
  repaintPPI(true, true);
end;  // LoadAllMaps


function wordtorad(w : word):real;
begin
  result:= w / 65535 * 360;
  while result>180  do result:=result-360;
  while result<-180 do result:=result+360;
  result := result*rad;
end;


procedure TForm1.LoadStates(filename: string);
var landnamefile : Tinfile;
    wL, wB       : word;
    landname     : string;
    k            : integer;
begin
  AssignFile(landnamefile, filename);
  {$I-}
  reset(landnamefile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;
  readln(landnamefile);

  repeat
    read(landnamefile,   wL,wB);
    readln(landnamefile, landname);
    L[Lcounter].Koord.laenge := wordtorad(wL);
    L[Lcounter].Koord.breite := wordtorad(wB);
    sincos(L[Lcounter].Koord);
    for k:=1 to length(landname) do if landname[k]='_' then landname[k]:=' ';
    L[Lcounter].name   := landname;
    inc(Lcounter);
  until eof(landnamefile);
  closefile(landnamefile);
end;  //LoadStates


//lade grosse Karte eines kontinents *_borders.map
procedure TForm1.LoadBigMap(filename: string);
var bigmapfile : file of word;
    wL, wB     : word;
begin
  AssignFile(bigmapfile, filename);
  {$I-}
  reset(bigmapfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;

  repeat
    read(bigmapfile,wL,wB);
    M[pcounter].laenge := wordtorad(wL);
    M[pcounter].breite := wordtorad(wB);
    sincos(M[pcounter]);
    inc(pcounter)
  until EOF(bigmapfile);
  closefile(bigmapfile);
  Memo1.Lines.Add(inttostr(pcounter)+' points loaded');
end; //LoadBigMap


//Map einlesen
//http://www.maproom.psu.edu/dcw/
procedure TForm1.LoadMap(filename: string);
var mapfile    : Tinfile;
    landname   : string;
    zeichen    : char;
    polygon    : boolean;
    ende       : boolean;
    polyanfang : boolean;
    antarctica : boolean;
    welt       : boolean;
    neuesland  : boolean;
    laenge, breite, tempwert : real;
    punktecounter            : integer;
    distance                 : real;

begin
  laenge        := 0;
  breite        := 0;
  neuesland     := true;
  antarctica    := false;
  welt          := false;
  punktecounter := 0;
  if FileExists(filename) then assignfile(mapfile,filename)
                          else assignfile(mapfile,Pfad+filename);
  {$I-}
  reset(mapfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;

  landname := '';
  repeat
    read(mapfile,zeichen);
    if ((zeichen<>' ') and (zeichen<>chr($0A)) and (zeichen<>chr($0D))) then landname:=landname+zeichen;
  until zeichen=chr($0A);
  Memo1.Lines.Add('load '+filename+' : '+landname);
  if landname='antarctica'       then antarctica := true;
  if landname='heard_island'     then antarctica := true;
  if landname='world_coastlines' then welt       := true;
  polygon    := false;
  polyanfang := false;
  ende       := false;

  repeat
    read(mapfile,zeichen);
    case zeichen of
       'E':begin
             // END
             if polygon then polygon:=false else ende:=true;
             laenge:=0; breite:=0;
           end;
       ' ','/':begin
             // normale Koordinatenzeile
             read(mapfile,laenge, breite);
             inc(Punktecounter);

             if antarctica then begin
               laenge:=laenge/1852/60; //Meter in Grad
               breite:=breite/1852/60;
               if breite=0 then tempwert:=90
                           else tempwert:=arctan2(laenge,breite)/pi*180;
               breite:=-90+sqrt(sqr(laenge)+sqr(breite));
               laenge:=tempwert;
             end;

             //world1.txt basiert auf einem anderen Bezugssystem
             // diese Korrektur geht gut fuer europa und australien
             // aber nicht so gut fuer feuerland
             if welt then begin
               breite:=breite*1.01;
             end;

             // erster Punkt eines Polygons liegt in seiner Mitte
             //Aufwendigstes Polygon eines Landes bekommt die Landesbeschriftung
             if polyanfang then begin
               if neuesland then begin
                 //erstes Polygon eines landes
                 inc(Lcounter);
                 L[Lcounter].Koord.laenge := laenge*rad;
                 L[Lcounter].Koord.breite := breite*rad;
                 sincos(L[Lcounter].Koord);
                 L[Lcounter].name     := uppercase(landname);
                 L[Lcounter].punkte   := 0;
                 L[Lcounter].anstrich := 0;
                 neuesland:=false;
               end; // if neuesland
             end;  //if polyanfang

             if polyanfang then begin
               punktecounter := 0;
               LandL  := laenge;
               LandB  := breite;
               laenge := 0;
               breite := 0;
               polyanfang := false;
             end;

             //Länge des an-strichs messen
             //Polygon mit längstem Anstrich bekommt den Landesnamen
             if punktecounter=1 then begin
               distance:=(sin(breite*rad)*sin(LandB*rad)+cos(breite*rad)*cos(LandB*rad)*cos((laenge-LandL)*rad));
               distance:=arccos(distance);
               if distance>L[Lcounter].anstrich then begin
                 L[Lcounter].Koord.laenge := LandL*rad;
                 L[Lcounter].Koord.breite := LandB*rad;
                 sincos(L[Lcounter].Koord);
                 L[Lcounter].punkte       := 10000000;
                 L[Lcounter].anstrich     := distance;
               end;
             end;

           end;
       '1'..'9': begin
             // neues Polygon beginnt
             polygon    := true;
             polyanfang := true;
             laenge:=0; breite:=0;
           end;
    end;
    while laenge>180  do laenge:=laenge-360;
    while laenge<-180 do laenge:=laenge+360;
    while breite>90   do breite:=breite-180;
    while breite<-90  do breite:=breite+180;

    laenge:=laenge*rad;  // in Radiant umrechnen
    breite:=breite*rad;

    M[pcounter].laenge := laenge;
    M[pcounter].breite := breite;
    sincos(M[pcounter]);
    inc(pcounter);
    if ((laenge<>0) and (laenge<Lmin)) then Lmin:=laenge;
    if ((laenge<>0) and (laenge>Lmax)) then Lmax:=laenge;
    if ((breite<>0) and (breite<Bmin)) then Bmin:=breite;
    if ((breite<>0) and (breite>Bmax)) then Bmax:=breite;
    while (zeichen<>chr($0A)) and not EOF(mapfile) do read(mapfile,zeichen);
  until ende or EOF(mapfile);
  closefile(mapfile);
  Memo1.Lines.Add(inttostr(pcounter)+' points loaded');
end;  //LoadMap


//laden der Städtedaten
procedure TForm1.LoadTowns;
var mapfile        : Tinfile;
    zeichen        : char;
    ende           : boolean;
    laenge, breite : real;
    population     : real;
    popdowntown    : real;
    cityname       : string;
    minpop         : real;
    filename       : string;
begin
  filename := 'extra\citys.txt';
  if FileExists(filename) then assignfile(mapfile,filename)
                          else assignfile(mapfile,Pfad+filename);
  {$I-}
  reset(mapfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;

  // es werden nur Staedte mit mindestens minpop-Millionen Einwohnern geladen
  minpop := 0.1;
  if N5000001.checked   then minpop := 0.3;
  if N1Million1.checked then minpop := 1;
  if N3Million1.checked then minpop := 3;

  ende:=false;
  tcounter:=0;
  repeat
    read(mapfile,zeichen);
    case zeichen of
       'A'..'Z':begin
            read(mapfile,laenge, breite, population, popdowntown);
            if popdowntown>=minpop then begin
              if population=0  then population  := 0.1; // mindestens 100tausend Einwohner
              if popdowntown=0 then popdowntown := 0.1; // mindestens 100tausend Einwohner
              if population<popdowntown then population := popdowntown; //logisch
              laenge := laenge*rad;  // in Radiant umrechnen
              breite := breite*rad;
              T[tcounter].Koord.laenge := laenge;
              T[tcounter].Koord.breite := breite;
              sincos(T[tcounter].Koord);
              T[tcounter].name    := zeichen;
              T[tcounter].pop     := population;
              T[tcounter].popcore := popdowntown;
              // nun der Name der Stadt
              T[tcounter].langname:= '';
              cityname            := '';
              read(mapfile,zeichen);
              while ((zeichen<' ')  or  (Zeichen>'z'))  and not EOF(mapfile)  do read(mapfile,zeichen);
              while ((zeichen>=' ') and (Zeichen <='z') and not EOF(mapfile)) do begin
                cityname := cityname + zeichen;
                read(mapfile,zeichen);
              end;
              T[tcounter].langname := copy(cityname,1,32); // darf max 32 Zeichen lang sein
              //fertig
              inc(tcounter);
            end;
          end;
       '%': ende:=true;
    end;
    while (zeichen<>chr($0A)) and not EOF(mapfile) do read(mapfile,zeichen);
  until ende or eof(mapfile) or (tcounter>=maxTown);
  dec(tcounter);
  closefile(mapfile);
  Memo1.Lines.Add(inttostr(tcounter)+' Towns loaded');
end;  //LoadTowns


//laden der Flugplaetze
procedure TForm1.LoadPorts(filename: string);
var mapfile               : Tinfile;
    zeichen               : char;
    grad, minute, sekunde : integer;
    k                     : integer;
    koo                   : real;
    richtung              : string[8];

  function getfield: string;
  begin
    result :='';
    repeat
      read(mapfile,zeichen);
      if (zeichen<>':') then result:=result+zeichen;
    until (zeichen = ':') or eoln(mapfile);
  end;

begin  //LoadPorts
  PortMap.Init;
  //9625 airports in database  aber nur 4188 haben koordinaten
  if FileExists(filename) then assignfile(mapfile,filename)
                          else assignfile(mapfile,Pfad+filename);
  {$I-}
  reset(mapfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;

  Portcounter:=0;
  repeat
    port[Portcounter].icao := getfield;
    port[Portcounter].iata := getfield;
    port[Portcounter].name := getfield;
    port[Portcounter].town := getfield;
    port[Portcounter].country := getfield;
    for k:=0 to 5 do port[Portcounter].rw[k].name:='';   // noch keine runways
    for k:=0 to 9 do port[Portcounter].ils[k].name:='';  // noch keine ils
    grad    := strtoint(getfield);
    minute  := strtoint(getfield);
    sekunde := strtoint(getfield);
    koo     := grad + minute/60 + sekunde/3600;

    if koo<>0 then begin  // er hat koordinaten
      richtung := getfield;
      if richtung='S' then port[Portcounter].koord.breite := -koo *rad
                      else port[Portcounter].koord.breite :=  koo *rad;
      grad     := strtoint(getfield);
      minute   := strtoint(getfield);
      sekunde  := strtoint(getfield);
      koo      := grad + minute/60 + sekunde/3600;
      richtung := getfield;
      if richtung='E' then port[Portcounter].koord.laenge :=  koo *rad
                      else port[Portcounter].koord.laenge := -koo *rad;
      sincos(port[Portcounter].koord);
      try port[Portcounter].koord.hoehe := strtoint(getfield); except port[Portcounter].koord.hoehe := 0; end;
      if not PortMap.PutIn(port[Portcounter].koord.breite/rad, port[Portcounter].koord.laenge/rad) then begin
        memo1.lines.add('##Airport'+port[Portcounter].name);
      end;
      inc(Portcounter);
    end;
    readln(mapfile);
  until eof(mapfile) or (Portcounter>=maxPort);
  if Portcounter>0 then dec(Portcounter);    // zeigt auf letzten Airport
  closefile(mapfile);
  memo1.Lines.add(inttostr(Portcounter+1) + ' Airports loaded');
end;  // LoadPorts


//***SSB1*****************************************************//

// gibt es einen Airport mit diesem icao-namen?
function TForm1.FindAirport(name : string):integer;
var k : integer;
begin
  result:=-1;
  for k:=0 to portcounter do
    if port[k].icao=name then begin result:=k; break; end;
end;   // FindAirport


// liest aus einem String der Form  "49.216111+7.1197222"
// die koordinaten aus
function TForm1.rwkoord(st : string):Tkoordinaten;
var sta : string;
    k   : integer;
begin
  DecimalSeparator:='.'; // auf deutschen PCs nötig, da im Files '.' steht und warscheinlich ',' erwartet wird.
  result.hoehe:=0;
  result.V := true;
  try
    k:=2;
    while (st[k]<>'+') and (st[k]<>'-') do inc(k);
    sta:=copy(st,1,k-1);
    result.breite := StrToFloat(sta)*rad;
    sta:=copy(st,k,50);
    if (sta[1]='+') and (sta[2]='-') then sta:=copy(sta,2,50);  // '+-177.1234
    result.laenge := StrToFloat(sta)*rad;
  except
    result.breite := 0;
    result.laenge := 0;
    result.V      := false; // Fehlerkennzeichen
  end;
  sincos(result);
end; // rwkoord


// laden von runways fuer SSB1
// \ssb1\eddk_apt.out
procedure TForm1.Load_apt_out(filename: string);
var rwfile                  : Tinfile;
    rwcounter, nr, rwnr     : integer;
    st1, st2, st3, st4, st5 : string;
    typnr                   : integer;
begin
  if FileExists(filename) then assignfile(rwfile,filename)
                          else assignfile(rwfile,Pfad+filename);
  {$I-}
  reset(rwfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;
  rwcounter:=0;
  st1:='';
  st2:='';
{
;EDDR - 27R
$TYPE=1
49.216111+7.1197222
49.215833+7.1122222
-1
}
  repeat
    st1:=st2;            // ;ICAO richtung
    if not eof(rwfile) then readln(rwfile, st2);
    st4:= copy(st2,1,6);
    if copy(st2,1,6)='$TYPE=' then begin
      try typnr:=strtoint(copy(st2,7,2)); except typnr:=0; end;
      readln(rwfile, st3); // A
      readln(rwfile, st4); // B
      readln(rwfile, st5); // -1
      nr := FindAirport(copy(st1,2,4));
      if nr>=0 then begin
        for rwnr :=0 to 5 do
          if port[nr].rw[rwnr].name='' then begin
            port[nr].rw[rwnr].name:= copy(st1,9,3);
            port[nr].rw[rwnr].A   := rwkoord(st3);
            port[nr].rw[rwnr].B   := rwkoord(st4);
            port[nr].rw[rwnr].typ := typnr;
            inc(rwcounter);
            break; //for Schleife verlassen
          end;
      end;
    end;
  until eof(rwfile);

  closefile(rwfile);
  memo1.Lines.add(inttostr(rwcounter) + ' runways loaded');
end;  // Load_apt_out


// alle Flugplatzlandebahnen fuer SSB1
procedure TForm1.load_all_apt_out;
var
    search     : TSearchRec;
    filename   : string;
begin
  if FindFirst(Pfad+'ssb1\*apt.out', faAnyFile, search)=0 then begin
    filename :=pfad+'ssb1\'+search.name ;
    Load_apt_out(filename);
    while FindNext(search) = 0 do begin
      filename :=pfad+'ssb1\'+search.name ;
      Load_apt_out(filename);
    end;
  end;
  FindClose(search);
end;  // load_all_apt_out


// laden von ILS fuer SSB1
procedure TForm1.Load_ils_out(filename: string);
var ilsfile                 : Tinfile;
    ilscounter, nr, ilsnr   : integer;
    st1, st2, st3, st4, st5 : string;
    typnr                   : integer;
begin
  if FileExists(filename) then assignfile(ilsfile,filename)
                          else assignfile(ilsfile,Pfad+filename);
  {$I-}
  reset(ilsfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;
  ilscounter:=0;
  st1:='';
  st2:='';
{
;EBLG - 23R
$TYPE=20
50.646111+5.4536111
50.703888+5.5480555
-1
50.715555+5.5669444
50.761944+5.6427777
-1
50.773333+5.6616666
50.819722+5.7372222
-1
}
  repeat
    st1:=st2;            // ;ICAO richtung
    if not eof(ilsfile) then readln(ilsfile, st2);
    st4:= copy(st2,1,6);
    if copy(st2,1,6)='$TYPE=' then begin
      try typnr:=strtoint(copy(st2,7,2)); except typnr:=0; end;
      nr := FindAirport(copy(st1,2,4));
      if nr>=0 then begin
        for ilsnr :=0 to 9 do
          if port[nr].ils[ilsnr].name='' then begin
            port[nr].ils[ilsnr].name:= copy(st1,9,3);
            port[nr].ils[ilsnr].typ := typnr;

            readln(ilsfile, st3); // A
            readln(ilsfile, st4); // B
            readln(ilsfile, st5); // -1
            port[nr].ils[ilsnr].A1  := rwkoord(st3);
            port[nr].ils[ilsnr].B1  := rwkoord(st4);

            readln(ilsfile, st3); // A
            readln(ilsfile, st4); // B
            readln(ilsfile, st5); // -1
            port[nr].ils[ilsnr].A2  := rwkoord(st3);
            port[nr].ils[ilsnr].B2  := rwkoord(st4);

            readln(ilsfile, st3); // A
            readln(ilsfile, st4); // B
            readln(ilsfile, st5); // -1
            port[nr].ils[ilsnr].A3  := rwkoord(st3);
            port[nr].ils[ilsnr].B3  := rwkoord(st4);

            inc(ilscounter);
            break; //for Schleife verlassen
          end;
      end;
    end;
  until eof(ilsfile);

  closefile(ilsfile);
  memo1.Lines.add(inttostr(ilscounter) + ' ILS loaded');
end;  // Load_ils_out


// alle FlugplatzILS fuer SSB1
procedure TForm1.load_all_ils_out;
var
    search     : TSearchRec;
    filename   : string;
begin
  if FindFirst(Pfad+'ssb1\*ils.out', faAnyFile, search)=0 then begin
    filename :=pfad+'ssb1\'+search.name ;
    Load_ils_out(filename);
    while FindNext(search) = 0 do begin
      filename :=pfad+'ssb1\'+search.name ;
      Load_ils_out(filename);
    end;
  end;
  FindClose(search);
end;  // load_all_ils_out


// laden von irgentetwas fuer SSB1
procedure TForm1.Load_ats_out(filename: string; anfang : boolean);
var atsfile       : Tinfile;
    st1, st2, st3 : string;
    ats_type      : integer;

  procedure readoneats;
  begin
    repeat
      repeat
        readln(atsfile, st3); // A
        if (copy(st2,1,6)='$TYPE=') then try ats_type := strtoint(copy(st2,7,1)); except ; end;
      until ((st3[1]<>';') and (st3[1]<>'$') and (st3[1]<>'{')) or eof(atsfile);
      if not eof(atsfile) then begin
        if st3='-1' then begin
          ATS[ATScounter].laenge:=0;
          ATS[ATScounter].breite:=0;
          sincos(ATS[ATScounter]);
        end else begin
          ATS[ATScounter] := rwkoord(st3);
        end;
        inc(ATScounter);
      end;
     until eof(atsfile) or (ATScounter>=(maxATS-30)); // behelfsmaessiger Ueberlaufschutz
  end;  //readoneats

begin
  if FileExists(filename) then assignfile(atsfile,filename)
                          else assignfile(atsfile,Pfad+filename);
  {$I-}
  reset(atsfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;
  if Anfang then ATScounter:=0; //geht nur bei einmaligen laden einer einzigen datei
  st1:='';
  st2:='';
  ats_type := 6;

{
;A1   
$TYPE=6
48.406944+3.2947222
48.345347+3.3947222
48.283750+3.4947222
48.222153+3.5947222
48.160555+3.6947222
48.098958+3.7947222
48.037361+3.8947222
48.025555+3.9138888
-1


$TYPE=7
{AFR-A451-B-=
18.106667+37.803333
16.853334+38.296667
-1
{AFR-B21-B-=
17.673333+26.743333
16.1+28.026667
14.666806+29.112128
13.11125+30.226472
9.563167+31.65317
-1
{MES-B407-B->
21.040833+38.364722
20.954722+38.263056
-1
}

  readoneats;
  closefile(atsfile);
  memo1.Lines.add(inttostr(ATScounter) + ' ATS-points loaded');
end;  // Load_ats_out


procedure TForm1.load_all_ats_out;
var
    search     : TSearchRec;
    filename   : string;
begin
  if FindFirst(Pfad+'ssb1\*ats.out', faAnyFile, search)=0 then begin
//  if FindFirst(Pfad+'ssb1\*fir.out', faAnyFile, search)=0 then begin        // ATC-Zohnen
//  if FindFirst(Pfad+'ssb1\*ils.out', faAnyFile, search)=0 then begin        // ILS-Richtungen
    filename :=pfad+'ssb1\'+search.name ;
    Load_ats_out(filename, true);
    while FindNext(search) = 0 do begin
      filename :=pfad+'ssb1\'+search.name ;
      Load_ats_out(filename, false);
    end;
  end;
  FindClose(search);
end;  // load_all_ats_out


// laden der textdatei state_icao24.txt
// das ist ein kopiertes
// erst der name bis zum ersten TAB
// dann Muell bis zur ersen 0 oder 1
// dann nullen und einsen mit space dazwischen
// ca 80 zeilen lang
procedure TForm1.Loadicao24;
var filename  : string;
    icaofile  : Tinfile;
    st1       : string;
    maske, maskeA, maskeB, variable : integer;
    a         : char;
    bits      : integer;
begin
  icaocounter:=0;
  filename := 'extra\state_icao24.txt';
  if FileExists(filename) then assignfile(icaofile,filename)
                          else assignfile(icaofile,Pfad+filename);
  {$I-}
  reset(icaofile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;

  repeat
    // namen des landes
    st1:='';
    read(icaofile, a);
    while a<>chr($09) do begin
      st1:=st1+a;
      read(icaofile, a);
    end;

    //bitmaske
    maske := 0;
    bits  := 0;
    while (not eoln(icaofile)) and (a <> '-') do begin
      read(icaofile, a);
      if a='0' then begin
        maske := maske shl 1;
        inc(bits);
      end;
      if a='1' then begin
        maske := (maske shl 1) or 1;
        inc(bits);
      end;
    end;
    maskeA   := maske shl (24-bits); //Anfang
    variable := round(power(2,24-bits)-1);
    maskeB   := maskeA or variable;

    inc(icaocounter);
    ICAO[icaocounter].anfang := maskeA;
    ICAO[icaocounter].ende   := maskeB;
    ICAO[icaocounter].name   := st1;
    readln(icaofile);
  until eof(icaofile) or (icaocounter>=maxICAO);

  closefile(icaofile);
  memo1.Lines.add(inttostr(icaocounter) + ' AA24-areas loaded');
end;   //Loadicao24



// laden bekannter interrogatoren am boden
//ID  	Comment  	Latitude  	Longitude  	Updated
procedure TForm1.LoadIFF;
var filename  : string;
    ifffile   : Tinfile;
    st1       : string;
    k         : integer;

  function getstring3:string;
  var a : char;
  begin
    result:='';
    repeat
      read(ifffile,a);
      if a<>chr($09) then result:=result+a;
    until (a=chr($09)) or eoln(ifffile);
  end;

begin
  filename := 'ssb1\radarsites.txt';
  if FileExists(filename) then assignfile(ifffile,filename)
                          else assignfile(ifffile,Pfad+filename);
  {$I-}
  reset(ifffile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;

  for k:=0 to maxIFF do IFF[K].name := '';

  //ID  Comment   Latitude  Longitude  Updated
  IFFcounter:=0;
  readln(ifffile);  // ueberschrift
  repeat
    inc(IFFcounter);
    IFF[IFFcounter].name := getstring3;
    st1 := getstring3;
    IFF[IFFcounter].koord.breite := strtofloat(getstring3)*rad;
    IFF[IFFcounter].koord.laenge := strtofloat(getstring3)*rad;
    sincos(IFF[IFFcounter].koord);
    readln(ifffile);
  until eof(ifffile) or (IFFcounter>=maxIFF);
  closefile(ifffile);
  memo1.Lines.add(inttostr(IFFcounter) + ' Groundsites loaded');
end; //LoadIFF


//laden der bekannten Flugzeuge
// (http://www.libhomeradar.org/download/)
// aircrafts.txt ca 170000 eintraege  kenner-typ
// icao24.txt    ca. 8000  eintraege  aa-kenner
// daraus entstand icao24plus.txt
procedure TForm1.LoadJets(zusatz : boolean);
var filename1            : string;
    aafile               : Tinfile;
    typfile              : Tinfile;
    ende                 : boolean;
    aa                   : dword;
    a,b,c,d,e,f,g        : char;
    maxkenner            : integer;

  // list string aus 'extra\aircrafts.txt'
  function getstring:string;
  var a : char;
  begin
    result:='';
    repeat
      read(typfile,a);
      if a<>chr($09) then result:=result+a;
    until (a=chr($09)) or eoln(typfile);
  end;

  // string aus icao24plus.txt oder icao24plus1.txt lesen
  function getstring2:string;
  var a : char;
  begin
    result:='';
    if not eoln(aafile) then
    repeat
      read(aafile,a);
      if a<>chr($09) then result:=result+a;
    until (a=chr($09)) or eoln(aafile);
  end;

  procedure gettyp(var kenner:tkenner);
  var ende   : boolean;
      k      : integer;
  begin
    k    := 0;
    ende := false;
    // kenner - hersteller - typ - typgenau - ...
    repeat
      if kennerdb[k].name = kenner.name then begin
        ende        := true;
        kenner.typs := kennerdb[k].typs;
        kenner.typl := kennerdb[k].typl;
      end;
      inc(k);
    until ende or (k>maxkenner);
  end;

  // laden aller daten aus 'extra\aircrafts.txt'
  // wenn icai24plus.txt neu erstellt werden muss
  function loadkenner:integer;
  begin
    result:=-1;
    reset(typfile);
    ende := false;
    // kenner - hersteller - typ - typgenau - ...
    repeat
      inc(result);
      kennerdb[result].name := getstring;
      getstring;
      kennerdb[result].typs := getstring;
      kennerdb[result].typl := getstring;
      readln(typfile);
    until eof(typfile);
  end;

begin    //LoadJets
  if zusatz then  filename1 := 'extra\icao24plus1.txt'
            else  filename1 := 'extra\icao24plus.txt';
  if FileExists(filename1) then assignfile(aafile,filename1)
                           else assignfile(aafile,Pfad+filename1);
  {$I-}
  reset(aafile);
  {$I+}
  if IOResult <> 0 then begin
    if not zusatz then begin
      MessageDlg('Datei '+filename1+' nicht auffindbar', mtWarning	,[mbOk], 0);
      Memo1.Lines.Add('###load error: '+filename1);
    end;  
    exit;
  end;

  // icao24plus.txt oder icao24plus1.txt laden
  // aa - kenner - typs - typl
  ende:=false;
  if not zusatz then Jetcounter:=0; //erstes file laden
  repeat
    read(aafile, a,b,c,d,e,f, g);
    AA:= (asc2int(a) shl 20) or (asc2int(b) shl 16) or (asc2int(c) shl 12) or (asc2int(d) shl 8) or (asc2int(e) shl 4) or asc2int(f);
    jet[Jetcounter].aa := AA;
    if aa<>0 then begin
      jet[Jetcounter].kenner.name := getstring2;
      jet[Jetcounter].kenner.typs := getstring2;
      jet[Jetcounter].kenner.typl := getstring2;
      inc(Jetcounter);
    end;
    readln(aafile);
  until eof(aafile) or ende or (Jetcounter>=maxjet);
  closefile(aafile);

  JetSortcounter := 0;
  if JetCounter>1 then begin
    JetSortcounter := 1;
    while ( jet[JetSortcounter-1].aa <= jet[JetSortcounter].aa) and (JetSortcounter<JetCounter) do inc(JetSortcounter);
  end;
  memo1.Lines.add(inttostr(Jetcounter) + ' Aircrafts loaded');
end; //LoadJets



// laengen und Breitengrad (radiant) in XY fuer das bitmap umrechnen
procedure TForm1.movepunkt(var P : TKoordinaten);
  var  distance          : real;
       cos_P_breite      : real;
       sin_P_breite      : real;
       cos_Plaenge_ZentL : real;
  begin
  //auf geschwindigkeit optimieren
//    cos_P_breite := cos(P.breite);
//    sin_P_breite := sin(P.breite);
    cos_P_breite      := P.cos_breite;
    sin_P_breite      := P.sin_breite;
    cos_Plaenge_ZentL := cos(P.laenge-ZentL);

    distance := sin_P_breite * sin_ZentB + cos_P_breite * cos_ZentB * cos_Plaenge_ZentL;
    if distance> 1 then distance := 1;       //failsave
    if distance<-1 then distance :=-1;       //failsave
    distance := arccos(distance);
    if ((distance>pi/2)  or (distance>12*pi/2/Scale*Stretch)  ) then     // abhaenging von Monitorgroesse !!
      P.V := false
    else begin
      P.V :=  true;
      P.X :=  round( cos_P_breite * sin((P.laenge-ZentL)) * Erde * Scale);
      P.Y := -round((sin_P_breite * cos_ZentB - cos_P_breite * sin_ZentB * cos_Plaenge_ZentL ) * Erde * Scale);
      // X und Y: Erdradius = Erde * Scale = 60 x Scale
    end;
{
    // das gleiche, nur langsamer
    distance:=(sin(P.breite)*sin(ZentB)+cos(P.breite)*cos(ZentB)*cos(P.laenge-ZentL));
    distance:=arccos(distance);
    if ((distance>pi/2) or (distance>6*pi/2/Scale*Stretch)) then  P.V:=false else begin
      P.V:=true;
      P.X:=round( cos(P.breite)*sin((P.laenge-ZentL)) * Erde * Scale);
      P.Y:=-round((sin(P.breite)*cos(ZentB)-cos(P.breite)*sin(ZentB)*cos(P.laenge-ZentL) ) * Erde * Scale);
      // X und Y: Erdradius = Erde * Scale = 60 x Scale
    end;
}
end; //movepunkt


// momentane position aller flugzeuge schaetzen
procedure TForm1.guesposition;
var k         : integer;
    losttime  : TDatetime;
    d, dB, dL : real;
    dH        : integer;
    az_int    : integer;
    dd1, dd2  : real;     // DM61
    //min       : real;     // DM61
    //i, j      : integer;  // DM61
    level     : integer;

begin
  for k:= 0 to lastPlane do begin
    if planes[k].active then begin
      // schaetzen geht nur, wenn position , kurs und speed bekannt sind
      //if ((planes[k].speed<>0) and (planes[k].heading<>noheading) and (planes[k].latitude<>0) and (planes[k].longitude<>0)) then begin
      if ((planes[k].speed<>0) and (planes[k].heading<>noheading) and (planes[k].time_koord<>0)) then begin
        losttime := (now - planes[k].time_koord) * 24; // stunden
        d  := losttime * planes[k].speed;    // NM
        dB := d * cos( planes[k].heading );  // NM
        dL := d * sin( planes[k].heading );  // NM
        dB := dB / 60 * rad;                 // rad
        dL := dL / 60 * rad;                 // rad
        dL := dL / cos(planes[k].latitude*rad);
        dH := round(planes[k].steigen * losttime*60);
        if planes[k].altitude=NoAltitude then dH:=0;
        planes[k].guess.breite := planes[k].latitude*rad  + dB;   // rad
        planes[k].guess.laenge := planes[k].longitude*rad + dL;   // rad
        sincos(planes[k].guess);
        planes[k].guess.hoehe  := planes[k].altitude      + dH;   // in fuss
        if planes[k].guess.hoehe<0 then planes[k].guess.hoehe:=0;
        movepunkt(planes[k].guess);
        planes[k].guess.time := now;
      end else begin
        planes[k].guess.breite := 0;
        planes[k].guess.laenge := 0;
        sincos(planes[k].guess);
        planes[k].guess.hoehe  := 0;
        planes[k].guess.v      := false;
        planes[k].guess.time   := 0;
      end;

      // Entfernung und richtung vom Receiver zum plane in km und rad
      if (planes[k].latitude<>0) and (planes[k].longitude<>0) then begin
        //dd1 :=  planes[k].latitude -  ZentB /rad;
        //dd2 := (planes[k].longitude - ZentL /rad) * cos(planes[k].latitude*rad);
        dd1 :=  planes[k].latitude -  Receiver.breite /rad;
        dd2 := (planes[k].longitude - Receiver.laenge /rad) * cos(planes[k].latitude*rad);
        planes[k].distance := sqrt(dd1 * dd1 + dd2 * dd2)*111;
        planes[k].azimuth  := ArcTan2(dd2, dd1);
        // pruefen auf rangering
        if maximumrange1.checked and (planes[k].CPR_quality>1) then begin
          az_int := round(planes[k].azimuth/rad);
          if az_int<0 then az_int := az_int+360;
          az_int := az_int div 5;
          level := planes[k].altitude div 10000;
          if level>3 then level:=3;
          if level<0 then level := 0;
          if planes[k].distance>rangering[az_int, level].range then begin
            rangering[az_int, level].range        := planes[k].distance;
            rangering[az_int, level].koord.breite := planes[k].latitude*rad;
            rangering[az_int, level].koord.laenge := planes[k].longitude*rad;
            rangering[az_int, level].koord.hoehe  := planes[k].altitude;
            sincos(rangering[az_int, level].koord);
            movepunkt(rangering[az_int, level].koord);
          end;
          if planes[k].distance<rangeringmin[az_int, level].range then begin
            rangeringmin[az_int, level].range        := planes[k].distance;
            rangeringmin[az_int, level].koord.breite := planes[k].latitude*rad;
            rangeringmin[az_int, level].koord.laenge := planes[k].longitude*rad;
            rangeringmin[az_int, level].koord.hoehe  := planes[k].altitude;
            sincos(rangeringmin[az_int, level].koord);
            movepunkt(rangeringmin[az_int, level].koord);
          end;
        end;
      end;
      {
      // ab hier DM61   ca 10 Zeilen
    end;
  end;

  for i := 0 to lastPlane do if planes[i].active then planes[i].nr_nahe := lastPlane;     //Nach Entfernung sortieren bzw. nummerieren
  for i := 0 to lastPlane do if planes[i].active then begin
    min := 90000;
    for j:= 0 to lastPlane do if planes[j].active then begin
      if (planes[j].distance < min) and (planes[j].nr_nahe > i) then begin
        min:= planes[j].distance;
        planes[j].nr_nahe:= i;
        for k:=0 to j-1 do if (planes[k].nr_nahe = i) then planes[k].nr_nahe := lastPlane;
      end;
      // Ende von DM61
      }

    end;
  end;
end;  //guesposition


//Koordinatentransformation von laenge/breite nach x/y
// in dem Umfang eigentlich nur noetig, wenn Kartenausschnitt
// Scale oder Darstellungsumfang geändert wurden
procedure TForm1.move;
var k, kk  : integer;
begin
  sin_ZentB := sin(ZentB);
  cos_ZentB := cos(ZentB);

  //Empfängerposition
  movepunkt(Receiver);

  //Entfernungskreis
  if maximumrange1.checked then
    for k:=0 to 71 do
      for kk:=0 to 3 do begin
        movepunkt(rangering[k, kk].koord);
        movepunkt(rangeringmin[k, kk].koord);
      end;

  //Aircrafttracks
  if Aircraft1.checked then begin
    for k:=0 to lastPlane do begin
      if planes[k].active then begin
        for kk:=0 to planes[k].trackindex do movepunkt(planes[k].Track[kk]);
      end;
    end;
  end;

  //Ländergrenzen
  if States1.checked then begin
    if ((Scale<20) and not detailedmaps1.checked) or (Pcounter=0) then begin
      //grobe Darstellung
      for k:= 1 to wcounter do movepunkt(W[k]);
    end else begin
      //genaue Darstellung
      for k:= 1 to pcounter do movepunkt(M[k]);
    end;
  end;

  //Lat/Long
  if Grid1.Checked then begin
    MakeLatLong;
    for k:= 1 to Gcounter do movepunkt(G[k]);
  end else Gcounter:=0;

  //ländernamen
  if Statenames1.checked then
    for k:=0 to Lcounter do movepunkt(L[k].Koord);

  //Towns
  if Towns1.checked then
    for k:=0 to Tcounter do movepunkt(T[k].Koord);

  //Airports
  if Airports1.checked then begin
    for k:=0 to Portcounter do begin
      movepunkt(port[k].koord);
      for kk:=0 to 5 do if port[k].rw[kk].name<>'' then begin
        movepunkt(port[k].rw[kk].A);
        movepunkt(port[k].rw[kk].B);
      end;
      for kk:=0 to 9 do if port[k].ils[kk].name<>'' then begin
        movepunkt(port[k].ils[kk].A1);
        movepunkt(port[k].ils[kk].B1);
        movepunkt(port[k].ils[kk].A2);
        movepunkt(port[k].ils[kk].B2);
        movepunkt(port[k].ils[kk].A3);
        movepunkt(port[k].ils[kk].B3);
      end;
    end;
  end;

  //ATS-Routes
  if ATSRoutes1.checked then
    for k:= 1 to ATScounter do movepunkt(ATS[k]);

  //IFF-sites
  if GroundRADAR1.checked then
    for k:= 1 to IFFcounter do movepunkt(IFF[k].koord);

  //GPX-Routes
  if GPXOverlay1.checked then
    for k:= 1 to GPXcounter do movepunkt(GPX[k]);

end;   //move



//Koordinatengitter erstellen
procedure TForm1.MakeLatLong;
var L, B : integer;
begin
  Gcounter:=0;
  for L:=round(Lmin/rad) to round(Lmax/rad) do begin  // pro 1 grad laenge
    G[Gcounter].laenge:=0;
    G[Gcounter].breite:=0;
    sincos(G[Gcounter]);
    inc(Gcounter);
    for B:=round(Bmin/rad) to round(Bmax/rad) do
    if ((scale>30) or ((L mod 10) =0)) then begin     // oder pro 10 grad laenge
      G[Gcounter].laenge:=L*rad;
      G[Gcounter].breite:=B*rad;
      sincos(G[Gcounter]);
      inc(Gcounter);
    end;
    G[Gcounter].laenge:=0;
    G[Gcounter].breite:=0;
    sincos(G[Gcounter]);
    inc(Gcounter);
  end;

  for B:=round(Bmin/rad) to round(Bmax/rad) do begin
    G[Gcounter].laenge:=0;
    G[Gcounter].breite:=0;
    sincos(G[Gcounter]);
    inc(Gcounter);
    for L:=round(Lmin/rad) to round(Lmax/rad) do
    if ((scale>30) or ((B mod 10) =0)) then begin
      G[Gcounter].laenge:=L*rad;
      G[Gcounter].breite:=B*rad;
      sincos(G[Gcounter]);
      inc(Gcounter);
    end;
    G[Gcounter].laenge:=0;
    G[Gcounter].breite:=0;
    sincos(G[Gcounter]);
    inc(Gcounter);
  end;
end;  // MakeLatLong


//darstellen transformierter Punkte
procedure TForm1.zeichne;
var  olp,urp     : TPoint;
     ToRechteck  : TRect;
     FromRechteck:TRect;
     k, kk       : integer;
     pendown     : boolean;
     aatext      : string;
     aaaltitude  : integer;
     xx, yy      : integer;
     x1, x2      : integer;
     vv          : boolean;
     range       : real;
     level       : integer;
     headindex   : integer;
     textoffset  : integer;

  // punkte der polygone zeichnen, wobei der erste strich immer unterdrueckt wird
  procedure zeichnepunkt(P : TKoordinaten);
  begin
    if P.V=true then begin
      if (P.laenge=0) and (P.breite=0) then pendown:=false;
      if pendown then BM.canvas.lineto((BM.width div 2)+P.X,(BM.height div 2)+P.Y)
                 else BM.canvas.moveto((BM.width div 2)+P.X,(BM.height div 2)+P.Y);
      if (P.laenge<>0) or (P.breite<>0) then pendown:=true;
    end else pendown:=false;
  end; //zeichnepunkt

  //stadtkreise zeichnen
  procedure zeichneTown(P : TTownKoordinaten);
  var tr   : integer;  //townradius in pixel
      name : string;
  begin
    tr := round(20*Scale/200*sqrt(P.popcore));
    if tr<BM.width div 120 then tr:=BM.width div 120;
    if P.Koord.V=true then begin
      BM.canvas.Ellipse((BM.width div 2)+P.Koord.X-tr, (BM.height div 2)+P.Koord.Y-tr,
                        (BM.width div 2)+P.Koord.X+tr, (BM.height div 2)+P.Koord.Y+tr);
      if scale>50 then begin // Beschriften nur wenn auch lesbar
        if scale>300 then name := P.langname else name := P.name; // lange Beschriftung nur beim Hereinzoomen
        BM.canvas.textout((BM.width div 2)+P.Koord.X-4*Stretch,(BM.height div 2)+P.Koord.Y-6*Stretch, name);
      end;
    end;
  end;  //zeichneTown

  //Flughafen zeichnen
  procedure zeichnePort(P : Tairport);
  var tr  : integer;  //radius in pixel
      bez : string;
      k   : integer;
  begin
    tr:=round(2*Scale/200);
    if tr<2 then tr:=2;  // Mindestkreisdurchmesser beim herauszoomen
    if P.Koord.V=true then begin
      BM.canvas.Ellipse((BM.width div 2)+P.Koord.X-tr, (BM.height div 2)+P.Koord.Y-tr,
                        (BM.width div 2)+P.Koord.X+tr, (BM.height div 2)+P.Koord.Y+tr);
      //runways
      for k:=0 to 5 do if P.rw[k].name<>'' then begin
        case P.rw[k].typ of
          // es gibt Type=1 und Type=2
          // alle großen sind Type=1
          // vielleicht sind die kleinen militaer oder anderweitig nur eingeschraenkt nutzbar
          1: BM.canvas.pen.style   := psSolid;
          2: BM.canvas.pen.style   := psDot;
        end;
        BM.canvas.moveto((BM.width div 2)+ P.rw[k].A.x, (BM.height div 2)+ P.rw[k].A.y );
        BM.canvas.lineto((BM.width div 2)+ P.rw[k].B.x, (BM.height div 2)+ P.rw[k].B.y );
        BM.canvas.pen.style   := psSolid;
      end;
      //ils     gepunktet zeichnen
      if AirportILS1.checked then
      for k:=0 to 9 do if P.ils[k].name<>'' then begin
        BM.canvas.pen.style   := psDot;
        BM.canvas.moveto((BM.width div 2)+ P.ils[k].A1.x, (BM.height div 2)+ P.ils[k].A1.y );
        BM.canvas.lineto((BM.width div 2)+ P.ils[k].B1.x, (BM.height div 2)+ P.ils[k].B1.y );
        BM.canvas.moveto((BM.width div 2)+ P.ils[k].A2.x, (BM.height div 2)+ P.ils[k].A2.y );
        BM.canvas.lineto((BM.width div 2)+ P.ils[k].B2.x, (BM.height div 2)+ P.ils[k].B2.y );
        BM.canvas.moveto((BM.width div 2)+ P.ils[k].A3.x, (BM.height div 2)+ P.ils[k].A3.y );
        BM.canvas.lineto((BM.width div 2)+ P.ils[k].B3.x, (BM.height div 2)+ P.ils[k].B3.y );
        BM.canvas.pen.style   := psSolid;
        if scale>300 then  // Beschriftung nur beim Hereinzoomen
          BM.canvas.textout((BM.width div 2)+P.ils[k].B3.x, (BM.height div 2)+P.ils[k].B3.y-6*Stretch, P.ils[k].name);
      end;
      // beschriften des Flughafens
      bez:='';
      if normal2.checked then bez := P.name;;
      if ICAO2.checked   then bez := P.ICAO;
      if IATA2.checked   then begin bez := P.IATA; if bez='N/A' then bez:='-'; end;
      // beschriftung nur beim Hereinzoomen
      if scale>300 then begin
        BM.canvas.textout((BM.width div 2)+P.Koord.X+tr, (BM.height div 2)+P.Koord.Y-6*Stretch, Bez);
        if AirportAltitude1.checked and (P.Koord.hoehe<>0) then
          BM.canvas.textout((BM.width div 2)+P.Koord.X+tr, (BM.height div 2)+P.Koord.Y+6*Stretch, inttostr(P.Koord.hoehe));
      end;
    end;
  end; //zeichnePort

begin
  // momentane position aller flugzeuge schaetzen
  guesposition;

  if MakeNewBM or (not BM2valid) then begin
    clrscr(BM);

    //Hintergrundbild von open streat map einbinden
    if osmOK and OSMbackground1.checked then begin
      // verschoben und skaliert in BM kopieren
      FromRechteck := Rect(0, 0, osm.BM.width, osm.BM.height);
      ToRechteck   := Rect( round((BM.width  div 2) - (-osm.xoff+(osm.BM.width  div 2))* Scale/osm.Scale),
                            round((BM.height div 2) - (-osm.yoff+(osm.BM.height div 2))* Scale/osm.Scale),
                            round((BM.width  div 2) + ( osm.xoff+(osm.BM.width  div 2))* Scale/osm.Scale),
                            round((BM.height div 2) + ( osm.yoff+(osm.BM.height div 2))* Scale/osm.Scale) );
      BM.canvas.StretchDraw(ToRechteck, osm.BM);
    end; //osm

    //Hintergrundbild von srtm einbinden
    if srtmOK and SRTMbackground1.checked then begin
      // verschoben und skaliert in BM kopieren
      FromRechteck := Rect(0, 0, srtm.BM.width, srtm.BM.height);
      ToRechteck   := Rect( round((BM.width  div 2) - (-srtm.xoff+(srtm.BM.width  div 2))* Scale/srtm.Scale),
                            round((BM.height div 2) - (-srtm.yoff+(srtm.BM.height div 2))* Scale/srtm.Scale),
                            round((BM.width  div 2) + ( srtm.xoff+(srtm.BM.width  div 2))* Scale/srtm.Scale),
                            round((BM.height div 2) + ( srtm.yoff+(srtm.BM.height div 2))* Scale/srtm.Scale) );
      BM.canvas.StretchDraw(ToRechteck, srtm.BM);
    end;  //srtm

    //LatLong Koordinatengitter
    if Grid1.Checked then begin
      BM.canvas.pen.color:=latlongColor;
      pendown:=false;
      for k:= 1 to Gcounter do zeichnepunkt(G[K]);
    end;
    //erdumrandung wird immer angezeigt
    BM.canvas.pen.color:=latlongColor;
    BM.canvas.brush.style := bsClear;     // kreis nicht fuellen
    kk := round(ErdRadius * Scale/57);
    BM.canvas.ellipse((BM.width div 2)+kk,(BM.height div 2)+kk ,(BM.width div 2)-kk ,(BM.height div 2)-kk);

    //range cirles
    if RangeCircles1.checked then begin
      // 1 Pixel = 57 NM / Scale
      BM.canvas.pen.color   := rrColor;
      BM.canvas.font.color  := rrColor;
      BM.canvas.brush.style := bsClear; // kreis nicht fuellen
      if N50NM1.checked then begin
        for k:=1 to 5 do begin
          kk := round(50*k * Scale/57);                 // 50 NM increments
          BM.canvas.ellipse((BM.width div 2)+kk,(BM.height div 2)+kk ,(BM.width div 2)-kk ,(BM.height div 2)-kk);
          BM.canvas.textout((BM.width div 2),(BM.height div 2)-kk+5*Stretch, inttostr(k*50)+'NM');
        end;
      end else
      if N50km1.checked then begin
        for k:=1 to 8 do begin
          kk := round(50*k * Scale/57/1.8);              // 50 km increments
          BM.canvas.ellipse((BM.width div 2)+kk,(BM.height div 2)+kk ,(BM.width div 2)-kk ,(BM.height div 2)-kk);
          BM.canvas.textout((BM.width div 2),(BM.height div 2)-kk+5*Stretch, inttostr(k*50)+'km');
        end;
      end else
      if N5005ftaltitude1.checked then begin
        for k:=1 to 8 do begin
          kk := round(1.23 * sqrt(k*5000));             // sichtweite in NM
          kk := round(kk * Scale/57);                   // wandeln in pixel
          BM.canvas.ellipse((BM.width div 2)+kk,(BM.height div 2)+kk ,(BM.width div 2)-kk ,(BM.height div 2)-kk);
          BM.canvas.textout((BM.width div 2),(BM.height div 2)-kk+5*Stretch, 'FL: '+inttostr(k*5));
        end;
      end;
      BM.canvas.brush.style := bsSolid;  // alten Zustand wieder herstellen
    end;

    //ATS-Routes
    if ATSRoutes1.checked then begin
      BM.canvas.pen.color := AtsColor;
      for k:= 1 to ATScounter do zeichnepunkt(ATS[K]);
    end;

    //GPX-Overlay
    if GPXOverlay1.checked then begin
      BM.canvas.pen.color := GpxColor;
      for k:= 1 to GPXcounter do zeichnepunkt(GPX[K]);
    end;

    //IFF-Sites
    if GroundRADAR1.checked then begin
      BM.canvas.pen.color   := IffColor;
      BM.canvas.font.color  := IffColor;
      BM.canvas.brush.style := bsClear;
      for k:= 1 to IFFcounter do  if IFF[k].Koord.V=true then begin
        BM.canvas.Ellipse((BM.width div 2)+IFF[k].Koord.X-5, (BM.height div 2)+IFF[k].Koord.Y-5,
                          (BM.width div 2)+IFF[k].Koord.X+5, (BM.height div 2)+IFF[k].Koord.Y+5);
        if scale>300 then  // Beschriftung nur beim Hereinzoomen
          BM.canvas.textout((BM.width div 2)+IFF[k].Koord.X+7, (BM.height div 2)+IFF[k].Koord.y-6*Stretch, IFF[k].name);
      end;
    end;

    //Ländergrenzen
    if States1.checked then begin
      BM.canvas.pen.color := borderColor;
      pendown := false;
      If ((Scale<20) and not detailedmaps1.checked)or (pcounter=0) then begin
        for k:= 1 to wcounter do zeichnepunkt(W[K]);
      end else begin
        for k:= 1 to pcounter do zeichnepunkt(M[K]);
      end;
    end;

    //ländernamen
    if Statenames1.checked then begin
      BM.canvas.pen.color   := stateColor;
      BM.canvas.Font.Color  := stateColor;
      BM.canvas.brush.style := bsClear;
      for k:=1 to Lcounter do if L[k].Koord.V=true then
         BM.canvas.textout((BM.width div 2)+L[k].Koord.X-4*Stretch,(BM.height div 2)+L[k].Koord.Y-6*Stretch,L[k].name);
    end;

    //Towns
    if Towns1.checked then begin
      BM.canvas.brush.color := clWhite;
      BM.canvas.brush.style := bsClear; // kreis nicht fuellen
      BM.canvas.pen.color   := townColor;
      BM.canvas.Font.Color  := townColor;
      for k:=0 to Tcounter do zeichneTown(T[K]);
    end;

    //Mittelkreuz
    if crosshair1.checked then begin
      BM.canvas.pen.color := chairColor;
      BM.canvas.moveto((BM.width div 2),0);  BM.canvas.lineto((BM.width div 2),BM.height);
      BM.canvas.moveto(0,(BM.height div 2)); BM.canvas.lineto(BM.width,(BM.height div 2));
    end;
    //Mittelkoordinaten
    BM.canvas.Font.Color := cooColor;
    BM.canvas.textout(5*Stretch,(BM.height div 2)+Font.Size, koordtostr(ZentB));
    BM.canvas.textout((BM.width div 2)+5,Font.Size+5*Stretch, koordtostr(ZentL));

    //max range of the receiver/antenna
    if maximumrange1.checked then begin
      if showMaximumonly1.checked then begin
        //max of all altitudes
        BM.canvas.pen.color := rriColor;
        pendown:=false;
        for k:= 0 to 71 do begin
          range := rangering[k,3].range;
          level := 3;
          for kk:= 2 downto 0 do if rangering[k,kk].range>range then level:=kk;
          zeichnepunkt(rangering[k,level].koord);
        end;
        range := rangering[0,3].range;
        level := 3;
        for kk:= 2 downto 0 do if rangering[0,kk].range>range then level:=kk;
        zeichnepunkt(rangering[0,level].koord);
        pendown:=false;
      end else begin
        if showbyaltitude1.checked or N10000ft1.checked then begin
          // ...9 999
          BM.canvas.pen.color := altitudecolor(5000);
          for k:= 0 to 71 do zeichnepunkt(rangering[k,0].koord);
          zeichnepunkt(rangering[0,0].koord);
          pendown:=false;
        end;
        if showbyaltitude1.checked or N10000200001.checked then begin
          // 10 000 ... 19 999
          BM.canvas.pen.color := altitudecolor(10000);
          for k:= 0 to 71 do zeichnepunkt(rangering[k,1].koord);
          zeichnepunkt(rangering[0,1].koord);
          pendown:=false;
        end;
        if showbyaltitude1.checked or N20000ft30000ft1.checked then begin
          // 20 000 ... 29 999
          BM.canvas.pen.color := altitudecolor(25000);
          for k:= 0 to 71 do zeichnepunkt(rangering[k,2].koord);
          zeichnepunkt(rangering[0,2].koord);
          pendown:=false;
        end;
        if showbyaltitude1.checked or N30000ft1.checked then begin
          // 30 000 ...
          BM.canvas.pen.color := altitudecolor(35000);
          for k:= 0 to 71 do zeichnepunkt(rangering[k,3].koord);
          zeichnepunkt(rangering[0,3].koord);
          pendown:=false;
        end;
      end;
      // minimum range
      if showMinimumonly1.checked then begin
        //min of all altitudes
        BM.canvas.pen.color := clRed;//rriColor;
        for k:= 0 to 71 do begin
          range := rangeringmin[k,3].range;
          level := 3;
          for kk:= 2 downto 0 do if rangeringmin[k,kk].range<range then level:=kk;
          zeichnepunkt(rangeringmin[k,level].koord);
        end;
        range := rangeringmin[0,3].range;
        level := 3;
        for kk:= 2 downto 0 do if rangeringmin[0,kk].range<range then level:=kk;
        zeichnepunkt(rangeringmin[0,level].koord);
        pendown:=false;
      end;
    end;

    //Airports
    if Airports1.checked then begin
      BM.canvas.brush.style := bsClear; // kreis nicht fuellen
      BM.canvas.pen.color   := portColor;
      BM.canvas.Font.Color  := portColor;
      for k:=0 to Portcounter do zeichnePort(port[k]);
    end;

    //Empfängerposition
    if Receiver1.checked and Receiver.V then begin
      BM.Canvas.pen.color := rxColor;
      BM.canvas.Ellipse((BM.width div 2)+Receiver.X-5, (BM.height div 2)+Receiver.Y-5,
                        (BM.width div 2)+Receiver.X+5, (BM.height div 2)+Receiver.Y+5);
      BM.Canvas.MoveTo((BM.width div 2)+Receiver.X-10,(BM.height div 2)+Receiver.Y);
      BM.Canvas.LineTo((BM.width div 2)+Receiver.X+10,(BM.height div 2)+Receiver.Y);
      BM.Canvas.MoveTo((BM.width div 2)+Receiver.X,(BM.height div 2)+Receiver.Y-10);
      BM.Canvas.LineTo((BM.width div 2)+Receiver.X,(BM.height div 2)+Receiver.Y+10);
    end;

    // alles bis zu diesem punkt aendert sich sehr selten
    // ich sollte das BM so abspeichern und wiederverwenden
    // als Pufferspeicher verwende ich BM2
    BM2.canvas.Draw(0,0, BM);
    BM2valid  := true;
    MakeNewBM := false;
  end else begin
    BM.canvas.Draw(0,0, BM2);
  end; //if MakeNewBM or (not BM2valid)

  // aircraft
  if Aircraft1.checked then begin
    for k:=0 to lastPlane do if (planes[k].active) and not (planes[k].missed and hideitafter20sec1.checked) then begin
      BM.canvas.pen.color  := planes[k].Color;
      if randomcolor1.checked then BM.canvas.font.color := planes[k].Color
                              else BM.canvas.font.color := LabelColor;
      BM.canvas.pen.Style  := psSolid;
      if planes[k].trackindex >= 0 then begin

        //Start-Kringel
        xx := (BM.width div 2) + planes[k].Track[0].x;
        yy := (BM.height div 2)+ planes[k].Track[0].y;
        vv :=                    planes[k].Track[0].v;
        if vv then BM.canvas.Ellipse(xx-1, yy-1, xx+1, yy+1);

        //Track
        pendown := false;
        for kk:=0 to planes[k].trackindex do begin
          if colorbyaltitude1.checked then BM.canvas.pen.color := altitudecolor(planes[k].Track[kk].hoehe);
          zeichnepunkt(planes[k].Track[kk]);
        end;
        //Endpunktkoordinaten merken
        xx := (BM.width div 2) + planes[k].Track[planes[k].trackindex].x;
        yy := (BM.height div 2)+ planes[k].Track[planes[k].trackindex].y;
        vv :=                    planes[k].Track[planes[k].trackindex].v;
        aaaltitude := planes[k].altitude;

        //geschätzter Ort eines Fluzeugs, dessen Squitter gerade nicht kommt
        if vv and showpredictedposition1.checked and planes[k].airborne then
        if planes[k].guess.v then begin
          BM.canvas.pen.Style := psDot;
          BM.canvas.moveto(xx,yy);
          xx := (BM.width div 2) + planes[k].guess.x;
          yy := (BM.height div 2)+ planes[k].guess.y;
          vv :=                    planes[k].guess.v;
          aaaltitude := planes[k].guess.hoehe;
          if vv then BM.canvas.lineto(xx, yy);
          BM.canvas.pen.Style := psSolid;
        end;

        //darstellen nur dann, wenn auch sichtbar
        if vv then begin

          textoffset:=0;
          //Endp-Kringel
          if circle1.checked    then BM.canvas.Ellipse(xx-3, yy-3, xx+3, yy+3) else
          if rectangle1.checked then BM.canvas.Rectangle(xx-3, yy+3, xx+3, yy-3) else
          if aircraft2.checked  then begin
            if planes[k].heading = noheading then
              BM.canvas.Ellipse(xx-3, yy-3, xx+3, yy+3)
            else begin
              headindex := round( planes[k].heading/rad);
              headindex := round((headindex+360-5) / 10 ) mod 36;
              case planes[k].symbol of
                 sym_big:    begin ImageListL.Draw(BM.Canvas, xx-16, yy-16, headindex); textoffset := 15; end;   // >250 to
                 sym_medium: begin ImageListM.Draw(BM.Canvas, xx-12, yy-12, headindex); textoffset := 10; end;   // alle anderen
                 sym_small:  begin ImageListS.Draw(BM.Canvas, xx-12, yy-12, headindex); textoffset := 8;  end;   // < 300 kts
                 sym_fast:   begin ImageListF.Draw(BM.Canvas, xx-12, yy-12, headindex); textoffset := 8;  end;   // > 600 kts
              end; //case
            end;
          end;

          //DF11-antwort
          if planes[k].interogator>=0 then begin
            if (IffFilter < 0) or (IffFilter = planes[k].interogator) then begin
              BM.canvas.Ellipse(xx-5, yy-5, xx+5, yy+5);
              //in welche Zeile muss das nun geschrieben werden?
              if N0lines.checked  then BM.canvas.textout(xx+3+textoffset, yy- 6*Stretch, 'INT='+ifftostr(planes[k].interogator)) else
              if N2lines1.checked then BM.canvas.textout(xx+3+textoffset, yy+18*Stretch, 'INT='+ifftostr(planes[k].interogator)) else
              if N3lines1.checked then BM.canvas.textout(xx+3+textoffset, yy+30*Stretch, 'INT='+ifftostr(planes[k].interogator)) else
                                       BM.canvas.textout(xx+3+textoffset, yy+42*Stretch, 'INT='+ifftostr(planes[k].interogator));
            end;
            planes[k].interogator := -1;
          end;

          // missed-Kreuz
          if (planes[k].missed) and (planes[k].Track[planes[k].trackindex].V) then begin
            BM.canvas.moveto(xx-5,yy-5); BM.canvas.lineto(xx+5,yy+5);
            BM.canvas.moveto(xx-5,yy+5); BM.canvas.lineto(xx+5,yy-5);
          end;

          // Zielkreis
          if k=nearplane then BM.canvas.Ellipse(xx-15, yy-15, xx+15, yy+15);

          // Beschriftung
          if not N0Lines.checked and (Scale>10) then
          if N2lines1.checked then begin
            //Beschriftung 2-zeilig
            //1. Zeile
            if planes[k].airframe.known
              then aatext:=inttostr(k)+'  '+planes[k].airframe.kenner.name+'  '+planes[k].airframe.kenner.typs
              else aatext:=inttostr(k);
            BM.canvas.textout(xx+3+textoffset, yy-6*Stretch, aatext);
            // 2. Zeile
            //aatext:='F'+inttostr(round(planes[k].altitude / 100));                              //altitude   aaaltitude
            if aaaltitude=NoAltitude then aatext:='F??'
                                     else aatext:='F'+inttostr(round(aaaltitude / 100));         //altitude
            if not planes[k].airborne then aatext:='ground';
            if planes[k].steigen < 0  then aatext := aatext+'-' else                             // descend
            if planes[k].steigen > 0  then aatext := aatext+'+' else                             // climb
                                           aatext := aatext+' ';                                 // hoehe konstant
            if planes[k].speed<>0 then aatext:=aatext+'  '+inttostr(round(planes[k].speed));     //speed
            BM.canvas.textout(xx+3+textoffset, yy+6*Stretch, aatext);
          end else begin
            //Beschriftung 3-zeilig nach Rüdiger
            // 1. Zeile
            aatext:=inttostr(k)+'  '+planes[k].ident;                                            // flightnr.-callsign
            BM.canvas.textout(xx+5+textoffset, yy-6*Stretch, aatext);
            // 2. Zeile
            if planes[k].airframe.known
              then aatext := planes[k].airframe.kenner.name+'  '+planes[k].airframe.kenner.typs  // registration  or..
              else aatext := '<' + inttohex(planes[k].AA ,6) + '>';                              // ..ICAO24-adr
            BM.canvas.textout(xx+5+textoffset, yy+6*Stretch, aatext);
            // 3. Zeile
            if aaaltitude=NoAltitude then aatext:='F??' else begin
              if aaaltitude > TransitAlt
                then aatext := 'F'+inttostr(round(aaaltitude / 100))                             // altitude > 6000ft
                else aatext := inttostr(aaaltitude);                                             // altitude < 6000ft
            end;
            if not planes[k].airborne then aatext := 'ground';
            if planes[k].steigen < 0  then aatext := aatext+'-' else                             // descend
            if planes[k].steigen > 0  then aatext := aatext+'+' else                             // climb
                                           aatext := aatext+' ';                                 // hoehe konstant
            BM.canvas.textout(xx+5+textoffset, yy+18*Stretch, aatext);

            if planes[k].speed<>0 then aatext := inttostr(round(planes[k].speed));               // speed
            if planes[k].heading<>noheading
              then aatext := aatext+'  H'+rad2instr(planes[k].heading,1)                         // heading
              else aatext := '';
            BM.canvas.textout(xx+41+textoffset, yy+18*Stretch, aatext);
            //Beschriftung 4-zeilig
            // 4. Zeile
            if N4lines1.checked then begin
              if planes[k].mod3id <>0 then aatext := 'Sqk: '+ planes[k].squawk
                                      else aatext:='';                                           // squawk
              BM.canvas.textout(xx+5+textoffset, yy+30*Stretch, aatext);
            end; // if N4lines1.checked (4. Zeile; squawk-anzeige)
          end; // beschriftung

          //Zielkreuz
          if (Kreuz.counter>0) and (Kreuz.nr=k) then begin
            BM.Canvas.pen.color := clRed;   // Kreuz immer in rot
            BM.Canvas.MoveTo(xx-15, yy);
            BM.Canvas.Lineto(xx+15, yy);
            BM.Canvas.MoveTo(xx, yy-15);
            BM.Canvas.Lineto(xx, yy+15);
          end;

        end;//if vv

      end; //if planes[k].trackindex >= 0
    end;   //for k:=0 to lastPlane do if planes[k].active then
  end;     //if Aircraft1.checked


  //overlaydaten
  // Einblendung der Daten eines flugzeuges in der oberen linken Ecke der Grafik
  if (nearplane<>-1) and planes[nearplane].active then begin
    BM.canvas.Font.Color := ovlColor;
    if backgroundnottransparent1.checked then begin
      BM.canvas.brush.style := bsSolid;
      BM.canvas.brush.Color := BackColor;
    end else
      BM.canvas.brush.style := bsClear;
    x1 := 5*Stretch;
    x2 := 50*Stretch;
    yy := Font.Size+5*Stretch;
    BM.canvas.textout(x1, yy, 'Nr.');
    BM.canvas.textout(x2, yy, inttostr(nearplane));
    yy:=yy + 2*Font.Size ;
    //BM.canvas.textout(x1, yy, 'AA');
    BM.canvas.textout(x1, yy, 'ICAO24');
    BM.canvas.textout(x2, yy, inttohex(planes[nearplane].aa,6) );
    if Form1.planes[nearplane].airframe.known then begin
      yy:=yy + 2*Font.Size ;
      BM.canvas.textout(x1, yy, 'Regist.');
      BM.canvas.textout(x2, yy, planes[nearplane].airframe.kenner.name);
    end;
    if Form1.planes[nearplane].ident<>'' then begin
      yy:=yy + 2*Font.Size ;
      BM.canvas.textout(x1, yy, 'Ident.');
      BM.canvas.textout(x2, yy, planes[nearplane].ident);
    end;
    yy:=yy + 2*Font.Size ;
    BM.canvas.textout(x1, yy, 'Nation');
    BM.canvas.textout(x2, yy, AA2Land(planes[nearplane].aa) );
    //Höhe & Variometer
    yy:=yy + 2*Font.Size ;
    BM.canvas.textout(x1, yy, 'Alt.');
    BM.canvas.textout(x2, yy, inttostr(planes[nearplane].altitude)+' ft' );
    if planes[nearplane].steigen<>0 then begin
      yy:=yy + 2*Font.Size ;
      //BM.canvas.textout(x1, yy, 'Climb.');
      if planes[nearplane].steigen<0 then BM.canvas.textout(x2, yy, '-'+inttostr(planes[nearplane].steigen)+' ft/min' )
                                     else BM.canvas.textout(x2, yy, '+'+inttostr(planes[nearplane].steigen)+' ft/min' );
    end;

    yy:=yy + 2*Font.Size ;
    BM.canvas.textout(x1, yy, 'Lat.');
    BM.canvas.textout(x2, yy, koordtostr_Grad(planes[nearplane].latitude) );
    yy:=yy + 2*Font.Size ;
    BM.canvas.textout(x1, yy, 'Long.');
    BM.canvas.textout(x2, yy, koordtostr_Grad(planes[nearplane].longitude) );
    if planes[nearplane].speed<>0 then begin
      yy:=yy + 2*Font.Size ;
      BM.canvas.textout(x1, yy, 'Speed');
      BM.canvas.textout(x2, yy, inttostr(round(planes[nearplane].speed))+' kts' );
    end;
    if planes[nearplane].heading<>noheading then begin
      yy:=yy + 2*Font.Size ;
      BM.canvas.textout(x1, yy, 'Head.');
      BM.canvas.textout(x2, yy, rad2instr(planes[nearplane].heading, 2)+'°' );
    end;
    if planes[nearplane].airframe.known then begin
      yy:=yy + 2*Font.Size ;
      BM.canvas.textout(x1, yy, 'Type');
      BM.canvas.textout(x2, yy, planes[nearplane].airframe.kenner.typs);
      yy:=yy + 2*Font.Size ;
      //BM.canvas.textout(x1, yy, 'SubType.');
      BM.canvas.textout(x2, yy, planes[nearplane].airframe.kenner.typl);
    end;
    if planes[nearplane].mod3id<>0 then begin
      yy:=yy + 2*Font.Size ;
      BM.canvas.textout(x1, yy, 'Squawk.');
      BM.canvas.textout(x2, yy, planes[nearplane].squawk);
    end;
    BM.canvas.brush.style := bsClear;
  end;  //overlaydaten

  // bitmap in das bild kopieren
  olp.x := 0;
  olp.y := 0;
  urp.x := image1.Width;
  urp.y := image1.Height;
  ToRechteck.TopLeft     := olp;
  ToRechteck.BottomRight := urp;
  image1.canvas.StretchDraw(ToRechteck, BM);
  update;
end;  // zeichne


// loeschen eines Bitmap
procedure TForm1.clrscr(bm: TBitmap);
var
    olp,urp   : TPoint;
    rechteck  : TRect;
begin
  olp.x:=0;
  olp.y:=0;
  urp.x:=bm.Width;
  urp.y:=bm.Height;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  bm.canvas.Brush.Color:=BackColor;
  bm.canvas.FillRect(Rechteck);
end; // clrscr


// füllen eines Bitmap mit einer Farbe
procedure TForm1.fillscr(bm: TBitmap; col :Tcolor);
var
    olp,urp   : TPoint;
    rechteck  : TRect;
begin
  olp.x:=0;
  olp.y:=0;
  urp.x:=bm.Width;
  urp.y:=bm.Height;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  bm.canvas.Brush.Color:=col;
  bm.canvas.Brush.Style:=bsSolid;
  bm.canvas.FillRect(Rechteck);
end; // fillscr


// Einstellen de Com-Port Nummer und  -Parameter
procedure TForm1.Button1Click(Sender: TObject);
begin
  ComSetup1.ShowModal;
end;


// verbinden mit Com-Port
procedure TForm1.Button2Click(Sender: TObject);
begin
  connect;
end;


// disconnect from decoder
procedure TForm1.disconnect;
begin
  if not ComPort1.Connected then exit;
  RadioGroup1.itemindex:=0;
  KillCommunication;

  ToolButton16.indeterminate := false;
  ToolButton16.enabled    := true;
  ToolButton16.imageindex := 24;

  // Menu - decoder
  selcetCOMport1.enabled  := true;
  connect1.enabled        := true;

  ButtonStart.visible  := false;
  ButtonStop.visible   := false;
  progressbar1.visible := false;
  adsbPIC1.visible     := false;
  Beast2.visible       := false;

  clientstring.str     := '';
  clientstring.OldTime := 0;
  clientstring.NewTime := 0;
  rxstr.str     := '';
  rxstr.OldTime := 0;
  rxstr.NewTime := 0;
end;


// connect to decoder
procedure TForm1.connect;
begin
  if ComPort1.Connected then begin
    if decoder=decoder_adsbPIC then RadioGroup1.itemindex:=0;
    ComPort1.Close;
    sleep(1000);
  end;

  //Handshake Grundeinstellung "kein"
  ComPort1.FlowControl.ControlRTS := rtsDisable;
  ComPort1.FlowControl.OutCTSFlow := False;
  ComPort1.FlowControl.XonXoffIn  := False;
  ComPort1.FlowControl.XonXoffOut := False;
  // das Folgende wäre für "Software", aber ich will ja "keine"
  //ComPort1.FlowControl.XonXoffIn := True;
  //ComPort1.FlowControl.XonXoffOut := True;
  case DecoderNr1 of
    decoder_adsbPIC: ;
    decoder_rxcontrol: ;
    decoder_beast:begin
        // das Folgende für "Hardware"
        ComPort1.FlowControl.ControlRTS := rtsHandshake;
        ComPort1.FlowControl.OutCTSFlow := True;
      end;
  end; //case
  if not ComPort1.Connected then
  try
    ComPort1.Open;
  except
    Application.MessageBox(
      'Can not open the selected COM-port'+ chr($0D)+
      'Please check your COM-port-selection',
      'OOPS',  //kopfzeile
      MB_OK
      + MB_ICONWARNING            // gelbes warndreieck
      + MB_APPLMODAL              // user muss ok clicken um weiterzuarbeiten
      + MB_SETFOREGROUND);
  end;
  GetDecoderType;   // decoder erkennen und starten

  if ComPort1.Connected then begin
    ToolButton16.indeterminate    := true;
    ToolButton16.enabled          := false;
    ToolButton16.imageindex       := 25;
    // Menu - decoder
    selcetCOMport1.enabled        := false;
    connect1.enabled              := false;
  end else begin
    ToolButton16.indeterminate    := false;
    ToolButton16.enabled          := true;
    ToolButton16.imageindex       := 24;
    // Menu - decoder
    selcetCOMport1.enabled        := true;
    connect1.enabled              := true;
  end;
  Button1.enabled := ToolButton16.enabled;
end;  //connect


//N Bytes senden und M Bytes empfangen
//timeout ist jeweils 100 ms
// N ist maximal 4
// M wird ignoriert
procedure TForm1.Sende_Empfange(N,M :byte);
var
   asciistr    : string[63];
   rxxstr      : AnsiString;
   k           : integer;
   nrrx        : integer;
begin
  if N>0 then begin
    //senden über comport
    ComPort1.ClearBuffer(true, true);
    sleep(100);
    ComPort1.ClearBuffer(true, true);
    asciistr :='#';
    for k:=0 to (N-1) do asciistr := asciistr + inttohex(send_buf[k],2)+'-';
    asciistr := asciistr + chr($0d);
    Comport1.WriteStr(asciistr);
  end;

  // failsave falls nichts zurueckkommt, es wird maximal receive_buf[5] verwendet
  for k:=0 to 10 do receive_buf[k]:= 0;

  //empfangen über comport
  nrrx:=Comport1.ReadStr(rxxstr, 50);
  for k:=0 to (nrrx div 3)-1 do begin
    receive_buf[k]:=asc2int(rxxstr[3*k+2])*16 + asc2int(rxxstr[3*k+3]);
  end;
end; // sende_empfange


// auswahl der PIC-Decoder -Betriebsart
procedure TForm1.RadioGroup1Click(Sender: TObject);
begin
  if (RadioGroup1.Itemindex = 1) and (not Debugging) then exit;
  if not ComPort1.Connected then exit;
  // das geht nur für den adsbPIC
  if Decoder = decoder_adsbPIC then begin
    DecoderRestart;
    memo1.lines.add('Mode: '+ inttostr(receive_buf[1]));
  end;
  Timer1.enabled := (RadioGroup1.Itemindex>1) or ClientVerbunden or
                   ((RadioGroup1.Itemindex>0) and Debugging);
end;  //RadioGroup1Click


procedure TForm1.DecoderPause;
begin
  if not ComPort1.Connected then exit;
  send_buf[0]:= SET_MODE;           // Mode 0
  send_buf[1]:= 0;
  Sende_Empfange(2, 2);
end;  //DecoderPause


procedure TForm1.DecoderRestart;
begin
  if not ComPort1.Connected then exit;
  send_buf[0]:= SET_MODE;           // vorheriger Mode
  send_buf[1]:= RadioGroup1.Itemindex;
  if UseTimeTag   then send_buf[1] := send_buf[1] or $10;
  if UseFrameNr   then send_buf[1] := send_buf[1] or $20;
  if UseHeartBeat then send_buf[1] := send_buf[1] or $40;
  if UseBinFormat then send_buf[1] := send_buf[1] or $80;
  Sende_Empfange(2, 2);
end; //DecoderRestart


// genaue Zeitmarke auswerten
// Der ganzzahlige Teil eines TDateTime-Wertes entspricht der Anzahl der Tage
// seit dem 30.12.1899. Der fraktionale Teil TDateTime-Wertes gibt die Tageszeit an.
// Buffertinme       : Systemzeit beim Auslesen des Com-Port-Puffers
// zstH (hex-string) : obere  24 Bit der Decoder-Zeitmarke
// zstL (hex-string) : untere 24 Bit der Decoder-Zeitmarke
function TForm1.GetZeitmarke(Pstr : PTrxString; zstH, zstL : string): TDatetime;
var zeitmarkeH : dword;
    zeitmarkeL : dword;
    FrameZeit  : TDatetime;
    Buffertime : TDatetime;
begin
  Buffertime := Pstr^.NewTime;
  result := Buffertime;           // unsauberes failback, dadurch werden TAG-Fehler nicht erkannt
  try zeitmarkeH := strtoint('$'+zstH); except elog('GetZeitmarke H:>>'+zstH+'<<'); exit; end;
  try zeitmarkeL := strtoint('$'+zstL); except elog('GetZeitmarke L:>>'+zstL+'<<'); exit; end;
  if zeitmarkeH<0 then exit;
  if zeitmarkeL<0 then exit;

  //TagFrequenz= 12000000  oder  20000000
  //FrameZeit ist die seit dem Einschalten des Decoders vergangene Zeit
  FrameZeit := (zeitmarkeH/TagFrequenz * Sekunde * $1000000) + (zeitmarkeL/TagFrequenz * Sekunde);    // lokale Zeit des decoders
  //FrameZeit := ((zeitmarkeH * $1000000) + zeitmarkeL) / TagFrequenz * Sekunde;    // lokale Zeit des decoders , schneller

  result := TAG2Time(FrameZeit, Pstr);
end;


function TForm1.TAG2Time(FrameZeit : TDatetime ; Pstr : PTrxString): TDatetime;
var    Buffertime : TDatetime;
begin
  Buffertime := Pstr^.NewTime;

  // NullZeitmarke ist die Systemzeit zu der der Decoder eingeschaltet wurde
  if NullZeitmarke=0 then begin
    NullZeitmarke := Buffertime - FrameZeit;
    ShortDateFormat := 'yyyy/mm/dd';
    DateSeparator:='/';
    LongTimeFormat := 'hh:nn:ss.zzz';
    memo1.lines.add('initiate 0-time to '+DateToStr(NullZeitmarke)+','+TimeToStr(NullZeitmarke));
  end;

  // Pruefen, ob Systemzeit und Decoderzeit parallel laufen
  // Sekunde   = 1/24/60/60;
  // Zeitdifferenz anzeigen
  if TimeGauge.visible then TimeGauge.progress := round((Buffertime - (FrameZeit + NullZeitmarke))/Sekunde*50); // +-3 Sekunden

  if (Buffertime + 2*Sekunde)  < (FrameZeit + NullZeitmarke) then begin
    NullZeitmarke := Buffertime - FrameZeit;
    inc(Zeitkorrektur);
    if TimeGauge.visible then TimeKorrLabel.caption := inttostr(Zeitkorrektur);
    LongTimeFormat := 'hh:nn:ss.zzz';
    memo1.lines.add('adjust 0-time to '+TimeToStr(NullZeitmarke));
  end;
  if (Pstr^.OldTime - 2*Sekunde) > (FrameZeit + NullZeitmarke) then begin
    NullZeitmarke := Buffertime - FrameZeit;
    dec(Zeitkorrektur);
    if TimeGauge.visible then TimeKorrLabel.caption := inttostr(Zeitkorrektur);
    memo1.lines.add('time out of range, fixed');
  end;

  result := FrameZeit + NullZeitmarke;
end;   //GetZeitmarke


// frame eintragen und wenn gewuenscht in memo1 anzeigen
procedure TForm1.getnextframe(newframe : TFrame);
var st : string;
    k : integer;
begin
  inc(Lastframe);
  if Lastframe > maxFrame then Lastframe:=0;
  if LastFrame=framepointer then memo1.lines.add('### RAW-data buffer overflow');
  Frames[Lastframe] := newframe;
  inc(Framecounter);
  if (newframe.source = 0) then begin
    inc(HBFramecounter); // nur fuer lokalen decoder
    // Automatischer Test
    if AutomaticTest1.Checked then begin
      if AtestActive then inc(ATestAltCounter)  // alte version
                     else inc(ATestNeuCounter); // neue version     ATest0Counter
    end;
    // Automatische Offseteinstellung
    if AGCon then inc(AgcFrameCounter);
  end;
  //RAW daten im memo1 anzeigen
  if ListAllFrames and (not FullScreen) then begin
    st:='*';
    for k:=0 to 13 do st:=st+inttohex(newframe.B[k],2);
    st:=st+';';
    memo1.lines.add(st);
  end;
end; //getnextframe


//Frame finden, vom String abschneiden und von getnextframe eintragen lassen
//datasource=0 fuer lokalen decoder  ;  datasource=1 fuer Daten vom RAW-data-client
procedure TForm1.Buffer2Frames(nr_rx:integer; datasource:byte; Pstr:PTrxString);
var k, l       : integer;
    newframe   : TFrame;
    zstH, zstL : string;
begin
    k:=1;
    newframe.source := datasource;
    //Anfang des frames suchen
    while (Pstr^.str[k]<>'*') and (Pstr^.str[k]<>'@') and (k<nr_rx) and (k<length(Pstr^.str)) do inc(k);
    if ((Pstr^.str[k]='*') and (length(Pstr^.str)-k > 28)) or
       ((Pstr^.str[k]='@') and (length(Pstr^.str)-k > 40)) then begin

      newframe.T     := Pstr^.NewTime;
      newframe.B[14] := 1;

      // 48-Bit-Zeitmarke einlesen
      if (Pstr^.str[k]='@') then begin
        zstH:='';
        for l:=1 to 6 do begin
          inc(k);
          zstH:=zstH+Pstr^.str[k];
        end;
        zstL:='';
        for l:=7 to 12 do begin
          inc(k);
          zstL:=zstL+Pstr^.str[k];
        end;
        newframe.T := GetZeitmarke(Pstr, zstH,zstL);
        if Status_TAG < 3 then inc(Status_TAG);
      end;

      // 56 Bit-String
      // @01ACB664F36D 5D773600ADBBBF;
      if Pstr^.str[k+1 +6*2 +2] = ';' then  begin
        for l:=0 to 6 do newframe.B[l]:=asc2int(Pstr^.str[k+1 +l*2])*16 + asc2int(Pstr^.str[k+1 +l*2 +1]);
        for l:=7 to 13 do newframe.B[l]:=0;
        getnextframe(newframe);
        k:=k+1 +6*2 +3;
        Delete(Pstr^.str, 1, k-1);
      end else

      // 112 Bit String
      // @01ACB664B6D5 8D3C660B99455B93F00400C4CE87;
      if Pstr^.str[k+1 +13*2 +2] = ';' then  begin
        for l:=0 to 13 do newframe.B[l]:=asc2int(Pstr^.str[k+1 +l*2])*16 + asc2int(Pstr^.str[k+1 +l*2 +1]);
        getnextframe(newframe);
        k:=k+1 +13*2 +3;
        Delete(Pstr^.str, 1, k-1);
      end else begin
        inc(k);
        Delete(Pstr^.str, 1, k-1);
      end;
    end else

    Delete(Pstr^.str, 1, k-1);
end; //Buffer2Frames


//Frame finden, vom String abschneiden und von getnextframe eintragen lassen
//datasource=0 fuer lokalen decoder  ;  datasource=1 fuer Daten vom RAW-data-client
// beast binary output mit <esc>0x1A anstelle x1B
// <esc> "1" : 6 byte MLAT, 1 byte signal level, 2 byte Mode-AC
// <esc> "2" : 6 byte MLAT, 1 byte signal level, 7 byte Mode-S short frame
// <esc> "3" : 6 byte MLAT, 1 byte signal level, 14 byte Mode-S long frame    = 23 bytes
// <esc><esc>: true 0x1a
// <esc> is 0x1a, and "1", "2" and "3" are 0x31, 0x32 and 0x33
procedure TForm1.BinBuffer2Frames(nr_rx:integer; datasource:byte; Pstr:PTrxString);
var k, l       : integer;
    newframe   : TFrame;
    escape     : char;
    frameTyp   : char;
    FrameZeit  : TDatetime;
begin
    k:=1;
    escape := chr ($1A);
    newframe.source := datasource;
    //Anfang des frames suchen
    while (Pstr^.str[k]<>escape) and (k<nr_rx) and (k<length(Pstr^.str)) do inc(k);
    inc(k);
    frameTyp := Pstr^.str[k];
    if ((frameTyp='2') and (length(Pstr^.str)-k > 16)) or
       ((frameTyp='3') and (length(Pstr^.str)-k > 23)) then begin

      newframe.T     := Pstr^.NewTime;
      newframe.B[14] := 1;

      // 6 byte TAG
      FrameZeit := 0;
      for l:=1 to 6 do FrameZeit := (FrameZeit*$100) + ord(Pstr^.str[k+l]);
      FrameZeit := FrameZeit/TagFrequenz * Sekunde ;    // lokale Zeit des decoders
      newframe.T := TAG2Time(FrameZeit, Pstr);
      if Status_TAG < 3 then inc(Status_TAG);
      k:=k+7;

      inc(k); //1 byte signal level ueberspringen

      case frameTyp of
        '2': begin        // 56 Bit-String  //short
            for l:=0 to 6  do newframe.B[l]:=ord(Pstr^.str[k+l]);
            for l:=7 to 13 do newframe.B[l]:=0;
            getnextframe(newframe);
            k:=k+7;
            Delete(Pstr^.str, 1, k-1);
        end;
        '3': begin       // 112 Bit String // long
            for l:=0 to 13 do newframe.B[l]:=ord(Pstr^.str[k+l]);
            getnextframe(newframe);
            k:=k+14;
            Delete(Pstr^.str, 1, k-1);
        end;
        else begin
            inc(k);
            Delete(Pstr^.str, 1, k-1);
        end;
      end; //case

    end else Delete(Pstr^.str, 1, k-1);
end; //BinBuffer2Frames


// empfangene Daten von rs232 und wandelt sie in Frames
procedure TForm1.Workoff;
var
   nr_rx         : integer;
   NewRxstr      : AnsiString;

begin   //Workoff;
 //short-frame = 16+2 bytes
 //long-frame  = 30+2 bytes
 //5000 f/m = 5000 * 25 bytes/min = 2100 bytes/sekunde

 // @01ACB664F36D 5D773600ADBBBF;                     28 Zeichen
 // *5D773600ADBBBF;                                  16 Zeichen
 // @01ACB664B6D5 8D3C660B99455B93F00400C4CE87;       42 Zeichen
 // *8D3C660B99455B93F00400C4CE87;                    30 Zeichen

 // beast AVR-format
 // *02E99619FACDAE;
 // *8D3C5EE69901BD9540078D37335F;
 // *7700;
 // @016CE3671C74 23FFE7AB7BFCAB;                     MLAT
 // @016CE3671AA8 A800199A8BB80030A8000628F400;
 // @016CE3671C74 7700;
 //
 // beast binary output mit <esc>0x1A anstelle x1B
 // <esc> "1" : 6 byte MLAT, 1 byte signal level, 2 byte Mode-AC
 // <esc> "2" : 6 byte MLAT, 1 byte signal level, 7 byte Mode-S short frame
 // <esc> "3" : 6 byte MLAT, 1 byte signal level, 14 byte Mode-S long frame    = 23 bytes
 // <esc><esc>: true 0x1a
 // <esc> is 0x1a, and "1", "2" and "3" are 0x31, 0x32 and 0x33


 //daten vom lokalen decoder
  rxstr.NewTime := now;
  if rxstr.OldTime = 0 then rxstr.OldTime := rxstr.NewTime;
  nr_rx := Comport1.InputCount;
  if nr_rx>0 then begin
      nr_rx := Comport1.ReadStr(NewRxstr, nr_rx);
      rxstr.NewTime := now;
      if USBLogOn then writeln(USBlogfile,'>>',NewRxstr,'<<');
      rxstr.str:= rxstr.str + NewRxstr;
  end;

  if (binaryformat1.checked and (Decoder = decoder_beast)  ) or
     (UseBinFormat          and (Decoder = decoder_adsbPIC)) then
    while length(rxstr.str)>23 do BinBuffer2frames(length(rxstr.str), 0, @rxstr)
  else
    while length(rxstr.str)>42 do Buffer2frames(length(rxstr.str), 0, @rxstr);

end; // Workoff;


// empfangene Daten von Network und wandelt sie in Frames
// passiert alle 20 ms
// in der Zwischenzeit sammelt ClientSocketRead die Daten vom RAW client im String ClientNachricht zusammen
procedure TForm1.WorkoffNet;
var
   nr_rx        : integer;
begin   // WorkoffNet;
 //short-frame = 16+2 bytes
 //long-frame  = 30+2 bytes
 //5000 f/m = 5000 * 25 bytes/min = 2100 bytes/sekunde

 // @01ACB664F36D 5D773600ADBBBF;                     28 Zeichen
 // *5D773600ADBBBF;                                  16 Zeichen
 // @01ACB664B6D5 8D3C660B99455B93F00400C4CE87;       42 Zeichen
 // *8D3C660B99455B93F00400C4CE87;                    30 Zeichen

  // datem vom RAW-data-server
  clientstring.NewTime := now;
  if clientstring.OldTime = 0 then clientstring.OldTime := clientstring.NewTime;
  nr_rx := length(ClientNachricht);
  if nr_rx>10000 then begin
    nr_rx:=0;
  end else begin
    if ClientNachricht<>'' then begin
      clientstring.str := clientstring.str + ClientNachricht;
      clientstring.NewTime := now;
    end;
  end;
  ClientNachricht := '';

  if NetUseBinFormat then
    while length(clientstring.str)>23 do BinBuffer2frames(nr_rx, 1, @clientstring)
  else
    while length(clientstring.str)>42 do Buffer2frames(nr_rx, 1, @clientstring);

  clientstring.OldTime := clientstring.NewTime;

end; // WorkoffNet;


// alle 20ms Daten vom Com-port holen und abarbeiten
// 1 sekunde =   50 zyklen
// 1 minute  = 3000 zyklen
// Puffer x Abfragefrequenz = 1024 x 50 = 50kByte/s = 400kBit/s <<< Full-Speed = 12 Mbit/s = 1,5 MByte/s
procedure TForm1.Timer1Timer(Sender: TObject);
var minuten : real;
    st      : string;
    dq      : integer;
{
    j       : integer;                                    //DM61

  // suche in planes[] ein flugzeug mit der 24Bit-ID AA   //DM61
  function findPlane(AA :dword):integer;
  var k : integer;
  begin
    result := lastPlane+1;
    for k:=0 to lastPlane do
    if  planes[k].active and (planes[k].AA=AA) then begin
      result := k;
  //    planes[k].time_last := now;
      break;
    end;
  end; //findPlane
}

  // falls Frameraten komplett voll ist
  //dann jeden zweiten punkt wegwerfen und die restlichen punkte zusammenschieben
  procedure CompressFrameraten;
  var k    : integer;
      last : integer;
  begin
    last :=  Frameratenindex div 2;
    for k:=1 to last do Frameraten[k] := Frameraten[2*k];
    Frameratenindex := last+1;
    memo1.lines.add('Compress Frameraten');
  end; //CompressFrameraten


begin  //Timer1Timer alle 20 ms (=50 Hz = 3000 /min)
       // 100*15 = 1500 Aufrufe mit Interval=16 ergibt gemessene 38 sekunden anstelle berechneter 24 Sekunden
  if (not ComPort1.Connected) and (not ClientVerbunden)  then exit;

  if ComPort1.Connected then try
    Workoff;     // Frames vom Com-Port holen und in Frames[] eintragen
  except elog('Workoff'); end;

  if ClientVerbunden    then try
    WorkoffNet;  // Frames vom Server holen und in Frames[] eintragen
  except elog('WorkoffNet'); end;

  try
    Decode;                                 // Frames decodieren
  except elog('Decode'); end;

  //increment the counters
  inc(sg_update);
  inc(framerate_update);
  inc(repaint_update);
  inc(zeit_update);
  inc(flights_update);
  inc(watch_update);
  if ClientVerbunden        then inc(client_update);
  if AutomaticTest1.Checked then inc(atest_update);
  if AGCon                  then inc(agc_update);

  if sg_update>100 then begin  // 2 sekunden
    sg_update:=0;
    // status anzeigen
    Label8.caption := 'Status: ';
    if Status_HB  > 2  then Label8.caption := Label8.caption+'Heartbeat ';
    if Status_TAG > 2  then Label8.caption := Label8.caption+'Time-TAG ';
    if Status_HB  > 1  then dec(Status_HB);
    if Status_TAG > 1  then dec(Status_TAG);
    if Kreuz.counter>0 then Kreuz.counter := Kreuz.counter-2;   //                 Zielkreuz

    try
      activeTracks := make_Table;                               // alle 2 Sekunden Tabelle aktualisieren
      if TableForm.visible then Tableform.make_Table;
    except
      elog('make_Table');
    end;
//    Statusbar1.panels[5].text := 'DSU: '+inttostr(DSU_count)+' SSU: '+ inttostr(SSU_count)+
//    ' Fail1: '+ inttostr(Fail1_count)+' Fail2: '+ inttostr(Fail2_count)+' Fail3: '+ inttostr(Fail3_count)+
//    ' Zero: '+ inttostr(Zero_count);
  end;
  if repaint_update>50 then begin
    repaint_update:=0;
    repaintPPI(false, false);                                // alle 1 Sekunden Grafik aktualisieren
  end;
  if flights_update>250 then begin
    flights_update:=0;
    WatchListCheckAllFlights(nil);                           // alle 5 Sekunden Fluege suchen
  end;
  if watch_update>250 then begin
    watch_update:=0;
    Statusbar1.panels[5].text := WatchListCheckAllPlane;     // alle 5 Sekunden Watchliste bearbeiten
  end;

  if atest_update>250 then begin                             // alle 5 Sekunden automatischer Test
    atest_update := 0;
    if ATestActive then begin
      Debug01Click(nil);  //Test aus
      if ATestAltCounter>0 then
        HeartBeatForm.Label4.caption := 'Frames' + inttostr(round(100*ATestNeuCounter/ATestAltCounter))+' %';   // neu/alt
    end else begin
      Debug11Click(nil);  //Test an
    end;
  end;

  if agc_update>125 then begin                             // alle 2,5 Sekunden AGC
    agc_update := 0;
    inc(AgcCounter);
    case (AgcCounter mod 5) of
    1: begin
         if AgcDownCounter>0 then AgcResult := round(100*AgcUpCounter/AgcDownCounter);
         if debugging then HeartBeatForm.Label4.caption := 'Frames' + inttostr(AgcResult)+' %';

         if AgcCounter>24 then begin     //nach 2 Minuten
           if (AgcUpCounter>300) and (AgcDownCounter>300) then begin
             if AgcResult>110 then agc_mitte := agc_mitte + 5;
             if AgcResult<90  then agc_mitte := agc_mitte - 5;
             if debugging then memo1.lines.add(inttostr(AgcResult)+'% -> '+inttostr(agc_mitte)+' mV');
           end;
           AgcResult      := 100;
           AgcCounter     := 0;
           AgcUpCounter   := 0;
           AgcDownCounter := 0;
         end;
         setAgcOffset(agc_mitte+5);
       end;
    2: AgcFrameCounter := 0;
    3: begin
         AgcUpCounter := AgcUpCounter + AgcFrameCounter;
         setAgcOffset(agc_mitte-5);
       end;
    4: AgcFrameCounter := 0;
    0: AgcDownCounter := AgcDownCounter + AgcFrameCounter;
    end; //case
    if debugging then memo1.lines.add(inttostr(AgcCounter)+' up:'+inttostr(AgcUpCounter)+' down:'+inttostr(AgcDownCounter)+' = '+inttostr(AgcResult)+'%' );
  end;

  if zeit_update>50 then begin
    zeit_update := 0;
    LongTimeFormat := 'hh:nn:ss';
    TimeLabel.caption := 'Time: '+ TimeToStr(now);            // alle 1 Sekunden Zeit anzeigen
    Statusbar1.panels[4].text := TimeLabel.caption;
  end;

  if framerate_update>1500 then begin                         // 30 sec framerate aktualisieren
    framerate_update := 0;
    minuten := (now - Framemesszeit)*24*60;
    Frameraten[Frameratenindex].rate := round(Framecounter/minuten);
    if ListAllFrames and (Frameraten[Frameratenindex].rate > 10000) then Memo1Click(nil);  // bei hohen Frameraten RAW-Listing abschalten
    st := 'Framerate:  ' + inttostr(Frameraten[Frameratenindex].rate)+' Frames/min';
    if activeTracks>0 then begin
      Frameraten[Frameratenindex].fpa := round(Frameraten[Frameratenindex].rate/activeTracks);
      st := st + ' ('+ inttostr(Frameraten[Frameratenindex].fpa) +')';
    end else Frameraten[Frameratenindex].fpa := 0;
    label5.caption            := st;
    Statusbar1.panels[3].text := st;   
    Frameraten[Frameratenindex].zeit := now;
    if decoder<>decoder_adsbPIC then Frameraten[Frameratenindex].level := 0 else begin
      if AGCon then Frameraten[Frameratenindex].level := agc_mitte
               else Frameraten[Frameratenindex].level := agc_offset;
    end;

    Frameraten[Frameratenindex].mode := RadioGroup1.Itemindex;
    inc(Frameratenindex);
    if Frameratenindex>1000 then CompressFrameraten;
    Form3.updatepicture;

    if Framecounter>0 then begin
      //label1.caption:= 'CRC-Errors: ' + inttostr(round(CRCfehler/Framecounter*100))+' %';
      //label1.caption:= 'Data-Quality: ' + inttostr(100 -round(CRCfehler/Framecounter*100))+' %';
      dq := 100 -round(CRCfehler/Framecounter*100);
      if not debugging then begin
        if dq<100 then dq := ((dq div 10) +1 ) *10;
      end;
      label1.caption:= 'Data-Quality: ' + inttostr(dq)+' %';
    end;
    Framecounter  := 0;
    if HBFramecounter>1000000 then HBFramecounter:=0; //failsave
    CRCfehler     := 0;
    Framemesszeit := now;

    makeIffMasks;                                             // die IFFMasken neu belegen
    if memo1.lines.count>20000 then memo1.lines.clear;        // Textfenster loeschen
    if memo2.lines.count>20000 then memo2.lines.clear;
  end;

  if ClientSocket.host='84.42.164.101' then
  if client_update>6000 then begin                    // verbindung zum Prag-Server nach 2 min trennen
    client_update := 0;
    if ClientVerbunden then RawClientStartStop;
  end;
  
end; //Timer1Timer


// ein directory erzeugen, falls es nicht existiert
procedure TForm1.makedir(name : string);
begin
  if not DirectoryExists(name) then if not CreateDir(name) then
    raise Exception.Create('Verzeichnis '+name+' kann nicht erstellt werden');
end;  //makedir


//pruefe, welche karten existieren, aktiviere dementsprechend die Menuepunkte
procedure TForm1.Checkcontinents;

  function checkmapfiles(subpfad :string): boolean;
  var weg : string;
  begin
    weg := pfad+'maps\'+subpfad+'\';
    // einzelne Karten ?
    result := FileExists(weg+ '*pts.txt');
    // Gesamtkarte ?
    if  (FileExists(weg+ '*_borders.map') and FileExists(weg+ '*_states.txt')) then result := true;
  end;

begin
  Europewest1.enabled        := checkmapfiles('europe\west');
  EuropeUK1.enabled          := checkmapfiles('europe\uk');
  EuropeNorth1.enabled       := checkmapfiles('europe\north');
  EuropeSouth1.enabled       := checkmapfiles('europe\south');
  EuropeEast1.enabled        := checkmapfiles('europe\east');
  EuropeBalkan1.enabled      := checkmapfiles('europe\balkan');
  Russia1.enabled            := checkmapfiles('gus');
  Afrika1.enabled            := checkmapfiles('africa');
  Asiawest1.enabled          := checkmapfiles('asia\west');
  Asiacentral1.enabled       := checkmapfiles('asia\central');
  Asia1.enabled              := checkmapfiles('asia\east');
  Antarctika1.enabled        := checkmapfiles('antarctica');
  Northamerica1.enabled      := checkmapfiles('northamerica\canada');
  NorthamericaUSA1.enabled   := checkmapfiles('northamerica\usa');
  NorthamericaCarib2.enabled := checkmapfiles('northamerica\carib');
  Southamerica1.enabled      := checkmapfiles('southamerica');
end;   //Checkcontinents


procedure TForm1.FormCreate(Sender: TObject);
var Leerframe : TFrame;
    k         : integer;
begin
  PortMap := TPortMap.Create;
  Vgpx    := Tgpx.Create;

  //DEBUG
  Button3.visible := false;
  Button4.visible := false;
  Button5.visible := false;
  Button6.visible := false;
  Button7.visible := false;
  Button8.visible := false;
  Button9.visible := false;
  Button10.visible:= false;
  DebugFill1.visible:= false;

 // Logon      := true;      // DEBUG
 // Debugging := true;

  Application.OnHint := DisplayHint;

  mapmode := Button6.visible;    // Bearbeiten von karten, wirkt auf osm-loeschen nach Verschiebung in Bild

  //Pfad:=getcurrentdir+'\';
  // Falls der Programmstart mit Pfadangabe und Kommandozeilenparametern erfolgt
  // dann ergibt getcurrentdir den Pfad des rufenden Verzeichnisses
  // deshalb sollte man den Kommandozeilenparameter 0 benutzen
  // ParamStr(0) ist der Name der eigenen exe mit vollem Pfad
  Pfad:=ExtractFilePath(ParamStr(0));

  //Kommandozeilenparameter
  kommandozeile;

  // alles experimentelle verstecken
  Debug01.visible        := Debugging;
  Debug11.visible        := Debugging;
  AutomaticTest1.visible := Debugging;
  ToolButton18.visible   := Debugging;
  DebugFill1.visible     := Debugging;

  ConnectatStart1.checked := AutoConnect;

  DecimalSeparator      := '.';         // auf deutschen PCs nötig, da in Files '.' steht und warscheinlich ',' erwartet wird.
//  ThousandSeparator :=' ';//                LOCALE_STHOUSAND
  downloadmaps1.enabled := true;

  // eventuell fehlende directories erstellen
  makedir(pfad+'maps');
  makedir(pfad+'maps\europe');
  makedir(pfad+'maps\europe\west');
  makedir(pfad+'maps\europe\uk');
  makedir(pfad+'maps\europe\north');
  makedir(pfad+'maps\europe\east');
  makedir(pfad+'maps\europe\south');
  makedir(pfad+'maps\europe\balkan');
  makedir(pfad+'maps\gus');
  makedir(pfad+'maps\africa');
  makedir(pfad+'maps\antarctica');
  makedir(pfad+'maps\asia');
  makedir(pfad+'maps\asia\west');
  makedir(pfad+'maps\asia\central');
  makedir(pfad+'maps\asia\east');
  makedir(pfad+'maps\northamerica');
  makedir(pfad+'maps\northamerica\canada');
  makedir(pfad+'maps\northamerica\usa');
  makedir(pfad+'maps\northamerica\carib');
  makedir(pfad+'maps\southamerica');
  makedir(pfad+'log');
  makedir(pfad+'osm');
  makedir(pfad+'mpq');
  makedir(pfad+'aer');
  makedir(pfad+'save');
  makedir(pfad+'ssb1');
  makedir(pfad+'html');
  makedir(pfad+'gpx');
  makedir(pfad+'srtm');
  makedir(pfad+'srtm\tile');
  makedir(pfad+'srtm\lupe');

  //pruefe, welche karten existieren, aktiviere dementsprechen die Menuepunkte
  //braucht mehrere Sekunden beim Programmstart
  Checkcontinents;

  // fehlerfahte osm-Kacheln loeschen
  osm_CleanUp;

  StartLog;       //neue logfiles erstellen
  StartErrorLog;
  StartUsbLog;

  ButtonStart.visible       := false;
  ButtonStop.visible        := false;
  progressbar1.visible      := false;
  binarydataformat1.visible := false;
  adsbPIC1.visible          := false;
  Beast2.visible            := false;

  //Zoomregler unsichtbar
  TrackBar1.visible    := false;
  Label4.visible       := false;

  // 255 oder 16k tracks ?
  if maxPlane<300 then caption:=caption+'  (small)' else caption:=caption+'  (big)';

  //Programmfenster zentrieren.
  Form1.left := (Screen.Width - Form1.Width) div 2;
  Form1.top  := (Screen.Height - Form1.Height) div 2;

  makeBM(image1.width,image1.height,false);

  //Welt
  LoadMap('extra\world1.txt');
  Lcounter := 0;
  for k:=0 to pcounter do W[k] := M[k];
  wcounter := pcounter-1;
  pcounter := 0;

  // Flughaefen und flugzeuge  http://www.partow.net/miscellaneous/airportdatabase/
  LoadPorts('extra\GlobalAirportDatabase_dos.txt');

  load_all_apt_out;   // flughafenlandebahnen
  load_all_ils_out;   // flughafen -ILS
  load_all_ats_out;   // Flurrouten

  LoadJets(false);    // Flugzeuge, standard
  LoadJets(true);     // Flugzeuge, eigenes file

  LoadIFF;            // sekundaerradare
  Loadicao24;         // icao-zuordnung

  //GPX-files
  try
    Vgpx.load_all_gpx;
  except
    gpxCounter := 0;
  end;

  //Grundeinstellung
  for k:=0 to 13       do Leerframe.B[k]    := 0;
  for k:=0 to maxFrame do Frames[K]         := Leerframe;
  for k:=0 to 4        do IffMasks[k]       := k;
  for k:=0 to 16       do GPSData1.sat[k].PRN :=-1;

  NullZeitmarke   := 0;
  Zeitkorrektur   := 0;
  Frameratenindex := 0;
  framerateactive := false;
  LocalRawOnly    := true;
  framepointer    := 0;
  LastFrame       := 0;
  Kreuz.counter   := 0;
  NewReceiverPosition(ZentL,ZentB);
  for k:=0 to maxPlane do begin
    Planes[K].active   := false;
    planes[k].distance := NoRange;
  end;
  Framemesszeit   := now;
  for k:=0 to maxInterogatoren do interogatoren[k] := 0;
  for k:=0 to maxWatchList do begin
    Watchlist[k].AA      := 0;
    Watchlist[k].active  := false;
    Watchlist[k].present := false;
    Watchlist[k].plane   := 0;    // nummer in Planes
  end;
  clientstring.str     := '';
  clientstring.OldTime := 0;
  clientstring.NewTime := 0;
  rxstr.str     := '';
  rxstr.OldTime := 0;
  rxstr.NewTime := 0;
  ComPort1.CustomBaudRate :=115200;
  i2cdata[0]    := 0;

  //vorbereiten der Projektion
  sin_ZentB := sin(ZentB);
  cos_ZentB := cos(ZentB);
  b1:=(90-70)*rad;
  b2:=(90-50)*rad;
  rusN :=(ln(sin(b1))-ln(sin(b2))) / (ln(tan(b1))-ln(tan(b2)));
  rusC := sin(b1) / (rusN*power(tan(b1/b2), rusN));

  // fuer Erddrehung mit der Maus, Maus nicht gedrueckt
  Pdreh.v := false;

  // koordinatenberechnung ist noetig vor der naechsten anzeige
  hastomove := true;

  //ca. 150 km Radius
  Trackbar1.Position:= 56;                         // scale einstellen und zeichen

  rxstr.str := '';

  TableFormPos.left   := Form1.left;
  TableFormPos.top    := Form1.top;
  TableFormPos.width  := Form1.width;
  TableFormPos.height := Form1.height;
  AAmanagerPos.left   := 0;
  AAmanagerPos.top    := 0;
  AAmanagerPos.width  := 0;
  AAmanagerPos.height := 0;
  FilterFormPos       := AAmanagerPos;
  WatchListFormPos    := AAmanagerPos;
  AAmanagerPos        := AAmanagerPos;
  HeartBeatFormPos    := AAmanagerPos;
  htmlpath            := pfad+'html\';

  LoadInit;         // initfile laden
  LoadTowns;        //Staedte laden
  UpdateAllMenuColors;

  //decoderabhängige funktionen
  RS232speed1.visible      := false;               // RS232-speed aendern is moeglich
  RS232polarity1.visible   := false;               // RS232-polaritaet aendern is moeglich
  enableheartbeat1.visible := false;               // heartbeat
  ToolButton24.visible     := false;
  decoderstatus1.visible   := false;
  closetargets1.visible    := false;
  I2C1.visible             := false;

  //default position and osm map, steuert auch die Bildschirmgroesse !!
  if lastposition1.Checked then loadposition(1) else loadposition(0);

  // Vollbild
  if WinMaximized then WindowState := wsMaximized;

  if ConnectatStart1.checked then  AutoConnect:= true;
  if AutoConnect then connect;     // mit decoder verbinden

{
  //DM61
  RangeCircles1.checked    := true;                    // DM61 configuration
  AirportAltitude1.checked := false;
  after1min1.checked       := true;
  maxtotzeit               := 60;
  colorbyaltitude1.checked := true;
  ICAO2.checked            := true;
  ComPort1.Port            := 'COM7';
  for k:=0 to maxPlane do planes[k].distance:=60000;        // !!!Vorsicht, ist schon belegt
} 
end;    //FormCreate


// Kommandozeilenparameter
procedure TForm1.kommandozeile;
var k       : integer;
    ar_str  : string;
begin
  // Auswerten der Komandozeilenparameter
  for k := 0 to (ParamCount) do begin
    // ParamStr(0) ist der name der eigenen exe mit vollem pfad
    ar_str:=ParamStr(k);
    if ParamStr(k)[1] = '/' then begin
      case ParamStr(k)[2] of     // im Handbuch sind L S R A
       // official
       'L': Logon      := true;
       'S': if not ServerSocket.Active    then ServerStartStop;
       'R': if not ServerSocketRAW.Active then RAWServerStartStop;
       'A': AutoConnect := true;
       // private
       'E': ErrorLogon := true;
       'U': USBLogon   := true;
       'T': begin TimeGauge.visible := true; TimeKorrLabel.visible := true; end;
//       'D': Debugging   := true; // das schaltet zu viel frei , nicht mehr per kommandozeile erlauben !!
       'M': Root := true;   // Warnung bei OSM-Download abschalten
      end; //case
    end;
  end;
end;   //kommandozeile



//** I N I T F I L E ***********************************************************

procedure TForm1.LoadInit;
var initfile : Tinfile;
    st       : string;
    name     : string;
    stwert   : string;
    filename : string;
    k        : integer;

  function RemoveWhiteSpace:integer;
  var st1   : string;
      k     : integer;
      blabla: boolean;
  begin
    result := 0;
    blabla := false;
    st1:='';
    for k :=1 to length(st) do begin
      if st[k]='"' then blabla := not blabla;
      if ((st[k]<>' ') and (ord(st[k])<> 9)) or blabla then begin
        st1 := st1 + st[k];
        if st[k]= '=' then inc(result);
      end;
    end;
    st := st1;
  end; // RemoveWhiteSpace

  function getname: string;
  var k : integer;
  begin
    result := '';
    for k:=1 to length(st) do begin
      if st[k] = '=' then break;
      result:= result + st[k];
    end;
  end;

  function getstwert: string;
  var k : integer;
  begin
    result := '';
    k:=0;
    repeat inc(k) until st[k] = '=';
    result := copy(st, k+1, length(st)-k);
  end;

  function ReadBoolean:boolean;
  begin
    result := (stwert='1');
  end;

  function ReadString:string;
  begin
    // das ist noch unsauber
    result := copy(stwert,2,length(stwert)-2);
  end;

  procedure ReadColor(var wert :TColor);
  begin
    try
      wert := strtoint(stwert);
    except
      memo1.lines.add('##error init file:'+ st);
    end;
  end;

  procedure ReadHex(var wert :dword);
  begin
    try
      wert := strtoint(stwert);
    except
      memo1.lines.add('##error init file:'+ st);
    end;
  end;

  procedure ReadHex8(var wert :byte);
  begin
    try
      wert := strtoint(stwert);
    except
      memo1.lines.add('##error init file:'+ st);
    end;
  end;

  function ReadInteger(wert :integer):integer;
  begin
    try
      result := strtoint(stwert);
    except
      memo1.lines.add('##error init file:'+ st);
      result := wert;
    end;
  end;

begin   //LoadInit
  filename := Pfad+'initfile.txt';
  if not FileExists(filename) then begin
    Memo1.lines.add('## no init file');
    exit;
  end;

  assignfile(initfile,filename);
  Reset(initfile);
  while not eof(initfile) do begin
    Readln(initfile, st);                          { Erste Zeile der Datei lesen }
    if length(st) > 32 then begin
      if (st[1] <>'#') and (st[1]<>' ') then begin
        if RemoveWhiteSpace=1 then begin
          name   := getname;
          stwert := getstwert;
          // position of forms
          if name = 'FormLeft'            then begin Form1.left  := ReadInteger(left);  TableFormPos.left   := Form1.left;  end;
          if name = 'FormTop'             then begin Form1.top   := ReadInteger(top);   TableFormPos.top    := Form1.top;   end;
          if name = 'FormWidth'           then begin Form1.width := ReadInteger(width); TableFormPos.width  := Form1.width; end;
          if name = 'FormHeight'          then begin Form1.height:= ReadInteger(height);TableFormPos.height := Form1.height;end;
          if name = 'FullScreen'          then begin FullScreen  := ReadBoolean;   FormResize(nil); end;
          if name = 'WinMaximized'        then      WinMaximized := ReadBoolean;

          if name = 'TableFormLeft'       then TableFormPos.left       := ReadInteger(left);
          if name = 'TableFormTop'        then TableFormPos.top        := ReadInteger(top);
          if name = 'TableFormWidth'      then TableFormPos.width      := ReadInteger(width);
          if name = 'TableFormHeight'     then TableFormPos.height     := ReadInteger(height);
          if name = 'TableFormLocked'     then lockedtomainwindow1.checked := ReadBoolean;

          if name = 'FilterFormLeft'      then FilterFormPos.left      := ReadInteger(left);
          if name = 'FilterFormTop'       then FilterFormPos.top       := ReadInteger(top);
          if name = 'FilterFormWidth'     then FilterFormPos.width     := ReadInteger(width);
          if name = 'FilterFormHeight'    then FilterFormPos.height    := ReadInteger(height);

          if name = 'WatchListFormLeft'   then WatchListFormPos.left   := ReadInteger(left);
          if name = 'WatchListFormTop'    then WatchListFormPos.top    := ReadInteger(top);
          if name = 'WatchListFormWidth'  then WatchListFormPos.width  := ReadInteger(width);
          if name = 'WatchListFormHeight' then WatchListFormPos.height := ReadInteger(height);

          if name = 'AAmanagerLeft'       then AAmanagerPos.left       := ReadInteger(left);
          if name = 'AAmanagerTop'        then AAmanagerPos.top        := ReadInteger(top);
          if name = 'AAmanagerWidth'      then AAmanagerPos.width      := ReadInteger(width);
          if name = 'AAmanagerHeight'     then AAmanagerPos.height     := ReadInteger(height);

          if name = 'HeartBeatFormLeft'   then HeartBeatFormPos.left   := ReadInteger(left);
          if name = 'HeartBeatFormTop'    then HeartBeatFormPos.top    := ReadInteger(top);
          if name = 'HeartBeatFormWidth'  then HeartBeatFormPos.width  := ReadInteger(width);
          if name = 'HeartBeatFormHeight' then HeartBeatFormPos.height := ReadInteger(height);

          // what we see in the graphic display
          if name = 'ViewAircraft'        then Aircraft1.checked         := ReadBoolean;
          if name = 'ViewGrid'            then grid1.checked             := ReadBoolean;
          if name = 'ViewCrosshair'       then Crosshair1.checked        := ReadBoolean;
          if name = 'ViewStates'          then States1.checked           := ReadBoolean;
          if name = 'ViewStatenames'      then Statenames1.checked       := ReadBoolean;
          if name = 'ViewTowns'           then Towns1.checked            := ReadBoolean;
          if name = 'ViewDetailedmaps'    then detailedmaps1.checked     := ReadBoolean;
          if name = 'ViewAirports'        then Airports1.checked         := ReadBoolean;
          if name = 'ViewAirportILS'      then AirportILS1.checked       := ReadBoolean;
          if name = 'ViewAirportAltitude' then AirportAltitude1.checked  := ReadBoolean;
          if name = 'ViewRangeCircles'    then RangeCircles1.checked     := ReadBoolean;
          if name = 'ViewReceiver'        then Receiver1.checked         := ReadBoolean;
          if name = 'ViewRangeMax'        then maximumrange1.checked     := ReadBoolean;
          if name = 'ViewGroundRADAR'     then GroundRADAR1.checked      := ReadBoolean;
          if name = 'ViewATSRoutes'       then ATSRoutes1.checked        := ReadBoolean;
          if name = 'ViewGPX'             then GPXOverlay1.checked       := ReadBoolean;
          if name = 'ViewOSMbackground'   then OSMbackground1.checked    := ReadBoolean;
          if name = 'ViewSRTMbackground'  then SRTMbackground1.checked   := ReadBoolean;
          // colors used in the graphic display
          if name = 'ColorGrid'           then ReadColor(latlongColor);
          if name = 'ColorCrosshair'      then ReadColor(chairColor);
          if name = 'ColorBorder'         then ReadColor(borderColor);
          if name = 'ColorStatename'      then ReadColor(stateColor);
          if name = 'ColorTowns'          then ReadColor(townColor);
          if name = 'ColorAirport'        then ReadColor(portColor);
          if name = 'ColorATS'            then ReadColor(AtsColor);
          if name = 'ColorGPX'            then ReadColor(GpxColor);
          if name = 'ColorRangeCircles'   then ReadColor(rrColor);
          if name = 'ColorReceiver'       then ReadColor(rxColor);
          if name = 'ColorRangemax'       then ReadColor(rriColor);
          if name = 'ColorIFF'            then ReadColor(IffColor);
          if name = 'ColorBack'           then ReadColor(BackColor);
          if name = 'ColorLabel'          then ReadColor(LabelColor);
          if name = 'ColorOverlay'        then ReadColor(ovlColor);

          //Aircraft Track
          if name = 'ShowPredictedPosition'then showpredictedposition1.checked := ReadBoolean;
          if name = 'ColorByAltitude'      then colorbyaltitude1.checked := ReadBoolean;
          if name = 'SingleFrameDetectAir' then allairtargetswithin180NM1.checked := ReadBoolean;
          if name = 'SingleFrameDetectSurface' then allsurfacetargetswithin45NM1.checked := ReadBoolean;

          //Text-Overlay
          if name = 'OverlayBackTrans'     then backgroundnottransparent1.checked := ReadBoolean;

          //radiobuttons
          //Transition Altitude
          if name = 'TransAlt5000ft'       then begin N5000ft1.checked := ReadBoolean; TransitAlt := 5000; end;
          if name = 'TransAlt6000ft'       then begin N6000ft1.checked := ReadBoolean; TransitAlt := 6000; end;
          if name = 'TransAlt7000ft'       then begin N7000ft1.checked := ReadBoolean; TransitAlt := 7000; end;
          if name = 'TransAlt8000ft'       then begin N8000ft1.checked := ReadBoolean; TransitAlt := 8000; end;
          //Tracklabels
          if name = 'TrackLabel2Lines'     then N2lines1.checked    := ReadBoolean;
          if name = 'TrackLabel3Lines'     then N3lines1.checked    := ReadBoolean;
          if name = 'TrackLabel4Lines'     then N4lines1.checked    := ReadBoolean;  //für Squawk
          if name = 'TrackLabel0Lines'     then N0lines.checked     := ReadBoolean;
          if name = 'TrackLabeColorRandom' then randomcolor1.checked:= ReadBoolean;
          // Aircraft Symbol
          if name = 'SymbolCircle'         then circle1.checked    := ReadBoolean;
          if name = 'SymbolRectangle'      then rectangle1.checked := ReadBoolean;
          if name = 'SymbolAircraft'       then aircraft2.checked  := ReadBoolean;
          // osm
          if name = 'ColorOsmFull'         then fullcolor1.checked := ReadBoolean;
          if name = 'ColorOsmPale1'        then pale1.checked      := ReadBoolean;
          if name = 'ColorOsmPale2'        then pale2.checked      := ReadBoolean;
          if name = 'ColorOsmGray'         then gray1.checked      := ReadBoolean;
          //RR
          if name = 'Rr50km'               then N50NM1.checked     := ReadBoolean;
          if name = 'Rr50NM'               then N50km1.checked     := ReadBoolean;
          if name = 'Rr5000f'              then N5005ftaltitude1.checked := ReadBoolean;
          //airportname
          if name = 'AirportNameNormal'    then normal2.checked    := ReadBoolean;
          if name = 'AirportNameICAO'      then ICAO2.checked      := ReadBoolean;
          if name = 'AirportNameIATA'      then IATA2.checked      := ReadBoolean;
          if name = 'AirportNameNone'      then none2.checked      := ReadBoolean;
          //drop aircraft
          if name = 'AircraftDrop20s'      then if ReadBoolean then after20s1Click(nil);
          if name = 'AircraftDrop1min'     then if ReadBoolean then after1min1Click(nil);
          if name = 'AircraftDrop5min'     then if ReadBoolean then after5min1Click(nil);
          if name = 'AircraftDrop30min'    then if ReadBoolean then after30min1Click(nil);
          if name = 'AircraftDrop1hour'    then if ReadBoolean then after1hour1Click(nil);
          if name = 'AircraftDrop10houre'  then if ReadBoolean then never1Click(nil);
          if name = 'AircraftHide20s'      then hideitafter20sec1.checked := ReadBoolean;
          //koordinates
          if name = 'Coordinates1'         then N5112341.checked   := ReadBoolean;
          if name = 'Coordinates2'         then N5111221.checked   := ReadBoolean;
          if name = 'Coordinates3'         then N5111222.checked   := ReadBoolean;
          //tag frequency
          if name = 'Tag12MHz'             then if ReadBoolean then N12MHz1Click(nil);
          if name = 'Tag20MHz'             then if ReadBoolean then N20MHz1Click(nil);
          if name = 'TagActive'            then if ReadBoolean then usetimetag1Click(nil);
          if name = 'TagActiveBeast'       then if ReadBoolean then enabletimetag1Click(nil);
          //heart beat
          if name = 'HeartBeatActive'      then if ReadBoolean then enableheartbeat1Click(nil);
          //AutoConnect to decoder
          if name = 'AutoConnect'          then ConnectatStart1.checked := ReadBoolean;
          //memofelder
          if name = 'Memo1ListAllFrames'   then ListAllFrames           := ReadBoolean;
          if name = 'Memo2visible'         then Memo2visible            := ReadBoolean;
          //HeadingPlusMinus
          if name = 'Heading360'           then if ReadBoolean then N03601Click(nil);
          if name = 'Heading180'           then if ReadBoolean then N03591Click(nil);
          //Windrose
          if name = 'RangeMaxMax'          then showMaximumonly1.checked := ReadBoolean;
          if name = 'RangeMaxAltitude'     then showbyaltitude1.checked  := ReadBoolean;
          if name = 'Range00000'           then N10000ft1.checked        := ReadBoolean;
          if name = 'Range10000'           then N10000200001.checked     := ReadBoolean;
          if name = 'Range20000'           then N20000ft30000ft1.checked := ReadBoolean;
          if name = 'Range30000'           then N30000ft1.checked        := ReadBoolean;

          // Background picture
          if name = 'Background_SRTM'      then SRTM1.checked            := ReadBoolean;
          if name = 'Background_MPQ'       then MapQuestOSM1.checked     := ReadBoolean;
          if name = 'Background_MPQA'      then MapQuestAerial1.checked  := ReadBoolean;
          if name = 'Background_OSM'       then OpenStreetMap1.checked   := ReadBoolean;

          // Town sizes
          if name = 'Town_100k'            then N1000001.checked         := ReadBoolean;
          if name = 'Town_300k'            then N5000001.checked         := ReadBoolean;
          if name = 'Town_1M'              then N1Million1.checked       := ReadBoolean;
          if name = 'Town_3M'              then N3Million1.checked       := ReadBoolean;

          // decodertype
          if name = 'DecoderAdsbPIC'       then if ReadBoolean then adsbPIC2Click(nil);
          if name = 'DecoderGNS5890'       then if ReadBoolean then GNS58901Click(nil);
          if name = 'DecoderRxcontrol'     then if ReadBoolean then rxControl1Click(nil);
          if name = 'DecoderBeast'         then if ReadBoolean then Beast1Click(nil);

          //BeastControl
          if name = 'BeastMLAT'            then enabletimetag1.checked   := ReadBoolean;
          if name = 'BeastDF11u17'         then onlyDF11171.checked      := ReadBoolean;
          if name = 'BeastNoCRC'           then noCRCcheck1.checked      := ReadBoolean;
          if name = 'BeastNoDF045'         then suppressDF0451.checked   := ReadBoolean;
          if name = 'BeastBinary'          then binaryformat1.checked    := ReadBoolean;

          //PositionMode
          if name = 'DefaultPosition'      then defaultposition1.Checked := ReadBoolean;
          if name = 'LastPosition'         then lastposition1.Checked    := ReadBoolean;

          //network  and RS232
          if name = 'ComPort'              then ComPort1.Port            := ReadString;
          //if name = 'BaudRate'             then ComPort1.BaudRate        := TBaudRate(ReadInteger(0));    //unnötig
          if name = 'CustomBaudRate'       then ComPort1.CustomBaudRate  := ReadInteger(115200);
          if name = 'ComPort2'             then ComPort2.Port            := ReadString;
          if name = 'BaudRate2'            then ComPort2.BaudRate        := TBaudRate(ReadInteger(0));
          if name = 'ServerPort'           then ServerSocket.Port   := ReadInteger(ServerSocket.Port);
          if name = 'RAWServerPort'        then ServerSocketRAW.Port:= ReadInteger(ServerSocketRAW.Port);
          if name = 'ClientPort'           then ClientSocket.Port   := ReadInteger(ClientSocket.Port);
          if name = 'ClientHost'           then ClientSocket.Host   := ReadString;
          if name = 'ServerActive'         then if ReadBoolean then if not ServerSocket.Active    then ServerStartStop;
          if name = 'RAWServerActive'      then if ReadBoolean then if not ServerSocketRAW.Active then RAWServerStartStop;
          if name = 'LocalRawOnly'         then LocalRawOnly        := ReadBoolean;
          if name = 'NetUseBinFormat'      then NetUseBinFormat     := ReadBoolean;

          //I2C
          if name = 'I2C_off'              then donotuse1.Checked   := ReadBoolean;
          if name = 'I2C_fix'              then C2308C8E001.Checked := ReadBoolean;
          if name = 'I2C_flex'             then other1.Checked      := ReadBoolean;
          for k:=0 to 16 do begin
            if name = 'I2C_Data'+inttostr(k) then ReadHex8(i2cdata[k]);
          end;

          //maps
          if name = 'MapsEuWest'           then if ReadBoolean and Europewest1.enabled   then load_Eu_west(nil);
          if name = 'MapsEuUK'             then if ReadBoolean and EuropeUK1.enabled     then load_Eu_UK(nil);
          if name = 'MapsEuNorth'          then if ReadBoolean and EuropeNorth1.enabled  then load_Eu_North(nil);
          if name = 'MapsEuSouth'          then if ReadBoolean and EuropeSouth1.enabled  then load_Eu_South(nil);
          if name = 'MapsEuEast'           then if ReadBoolean and EuropeEast1.enabled   then load_Eu_East(nil);
          if name = 'MapsEuBalkan'         then if ReadBoolean and EuropeBalkan1.enabled then load_Eu_balkan(nil);
          if name = 'MapsRussia'           then if ReadBoolean and Russia1.enabled       then load_Russia(nil);
          if name = 'MapsAfrica'           then if ReadBoolean and Afrika1.enabled       then Afrika1Click(nil);
          if name = 'MapsAsiaWest'         then if ReadBoolean and Asiawest1.enabled     then Asiawest1Click(nil);
          if name = 'MapsAsiaCentre'       then if ReadBoolean and Asiacentral1.enabled  then Asiacentral1Click(nil);
          if name = 'MapsAsiaEast'         then if ReadBoolean and Asia1.enabled         then Asia1Click(nil);
          if name = 'MapsAntarctica'       then if ReadBoolean and Antarctika1.enabled   then Antarctika1Click(nil);
          if name = 'MapsAmericaNorthCan'  then if ReadBoolean and Northamerica1.enabled then Northamerica1Click(nil);
          if name = 'MapsAmericaNorthUsa'  then if ReadBoolean and NorthamericaUSA1.enabled   then NorthamericaUSA1Click(nil);
          if name = 'MapsAmericaNorthCar'  then if ReadBoolean and NorthamericaCarib2.enabled then NorthamericaCarib2Click(nil);
          if name = 'MapsAmericaSouth'     then if ReadBoolean and Southamerica1.enabled      then Southamerica1Click(nil);

          //Log
          if name = 'LogSelect'            then LogSel  := ReadBoolean;
          if name = 'LogDetected'          then LogDet  := ReadBoolean;

          //watchlist
          for k:=1 to maxWatchList do begin
            if name = 'Watchlist'+inttostr(k)+'ident' then Watchlist[k].ident := ReadString;
            if name = 'Watchlist'+inttostr(k)+'AA'    then ReadHex(Watchlist[k].AA);
            if name = 'Watchlist'+inttostr(k)+'activ' then Watchlist[k].active:=ReadBoolean;
          end;
          //html
          if name = 'HtmlPath'             then htmlpath := ReadString;
        end;
      end;
    end;
  end;  //while
  closefile(initfile);
end;  //LoadInit


// statistik ueber interrogatoren in ein file schreiben
procedure TForm1.SaveIntStat;
var k        : integer;
    statfile : Tinfile;
begin
  {$I-}
  assignfile(statfile,Pfad+'log/interogatoren.txt');
  rewrite(statfile);  //aus der Hilfe:  Bei einer Textdatei ist F nach dem Öffnen schreibgeschützt.
  {$I+}
  if IOResult <> 0 then exit;

  for k:=1 to maxInterogatoren do begin
    if interogatoren[k] > 0 then writeln(statfile, ifftostr(k) ,' : ',inttostr(interogatoren[k] ));
  end;
  closefile(statfile);
end;  //SaveIntStat



procedure TForm1.SaveInit;
var k        : integer;
    initfile : Tinfile;

  procedure saveBoolean(wert : boolean; bla : string);
  begin
    while length(bla)< 30 do bla := bla + ' ';
    if wert then bla := bla + '= 1'
            else bla := bla + '= 0';
    writeln(initfile,bla);
  end;

  procedure saveInteger(wert : Integer; bla : string);
  begin
    while length(bla)< 30 do bla := bla + ' ';
    bla := bla + '= ' + inttostr(wert);
    writeln(initfile,bla);
  end;

  procedure saveHex(wert : DWord; bla : string);
  begin
    while length(bla)< 30 do bla := bla + ' ';
    bla := bla + '= $' + inttohex(wert,8);
    writeln(initfile,bla);
  end;

  procedure saveHex8(wert : byte; bla : string);
  begin
    while length(bla)< 30 do bla := bla + ' ';
    bla := bla + '= $' + inttohex(wert,2);
    writeln(initfile,bla);
  end;

  procedure saveString(wert : String; bla : string);
  begin
    while length(bla)< 30 do bla := bla + ' ';
    bla := bla + '= "' + wert +'"';
    writeln(initfile,bla);
  end;

begin    //SaveInit
  k:=0;
  {$I-}
  assignfile(initfile,Pfad+'initfile.txt');
  rewrite(initfile);  //aus der Hilfe:  Bei einer Textdatei ist F nach dem Öffnen schreibgeschützt.
  {$I+}
  if IOResult <> 0 then exit;

  writeln(initfile,'#adsbScope-InitFile    ',DateToStr(Date));
  writeln(initfile,'#software-generated, do not edit !');

  saveInteger(left   ,'FormLeft');
  saveInteger(top    ,'FormTop');
  saveInteger(width  ,'FormWidth');
  saveInteger(height ,'FormHeight');
  saveBoolean(FullScreen ,'FullScreen');
  saveBoolean(WindowState=wsMaximized ,'WinMaximized');

  saveInteger(TableForm.left   ,'TableFormLeft');
  saveInteger(TableForm.top    ,'TableFormTop');
  saveInteger(TableForm.width  ,'TableFormWidth');
  saveInteger(TableForm.height ,'TableFormHeight');
  saveBoolean(lockedtomainwindow1.checked ,'TableFormLocked');

  saveInteger(FilterForm.left   ,'FilterFormLeft');
  saveInteger(FilterForm.top    ,'FilterFormTop');
  saveInteger(FilterForm.width  ,'FilterFormWidth');
  saveInteger(FilterForm.height ,'FilterFormHeight');

  saveInteger(WatchListForm.left   ,'WatchListFormLeft');
  saveInteger(WatchListForm.top    ,'WatchListFormTop');
  saveInteger(WatchListForm.width  ,'WatchListFormWidth');
  saveInteger(WatchListForm.height ,'WatchListFormHeight');

  saveInteger(AAmanager.left       ,'AAmanagerLeft');
  saveInteger(AAmanager.top        ,'AAmanagerTop');
  saveInteger(AAmanager.width      ,'AAmanagerWidth');
  saveInteger(AAmanager.height     ,'AAmanagerHeight');

  saveInteger(HeartBeatForm.left       ,'HeartBeatFormLeft');
  saveInteger(HeartBeatForm.top        ,'HeartBeatFormTop');
  saveInteger(HeartBeatForm.width      ,'HeartBeatFormWidth');
  saveInteger(HeartBeatForm.height     ,'HeartBeatFormHeight');

  saveBoolean(Aircraft1.checked        ,'ViewAircraft');
  saveBoolean(grid1.checked            ,'ViewGrid');
  saveBoolean(Crosshair1.checked       ,'ViewCrosshair');
  saveBoolean(States1.checked          ,'ViewStates');
  saveBoolean(Statenames1.checked      ,'ViewStatenames');
  saveBoolean(Towns1.checked           ,'ViewTowns');
  saveBoolean(detailedmaps1.checked    ,'ViewDetailedmaps');
  saveBoolean(Airports1.checked        ,'ViewAirports');
  saveBoolean(AirportILS1.checked      ,'ViewAirportILS');
  saveBoolean(AirportAltitude1.checked ,'ViewAirportAltitude');
  saveBoolean(RangeCircles1.checked    ,'ViewRangeCircles');
  saveBoolean(Receiver1.checked        ,'ViewReceiver');
  saveBoolean(maximumrange1.checked    ,'ViewRangeMax');
  saveBoolean(GroundRADAR1.checked     ,'ViewGroundRADAR');
  saveBoolean(ATSRoutes1.checked       ,'ViewATSRoutes');
  saveBoolean(GPXOverlay1.checked      ,'ViewGPX');
  saveBoolean(OSMbackground1.checked   ,'ViewOSMbackground');
  saveBoolean(SRTMbackground1.checked  ,'ViewSRTMbackground');

  saveHex(latlongColor                 ,'ColorGrid');
  saveHex(chairColor                   ,'ColorCrosshair');
  saveHex(borderColor                  ,'ColorBorder');
  saveHex(stateColor                   ,'ColorStatename');
  saveHex(townColor                    ,'ColorTowns');
  saveHex(rxColor                      ,'ColorReceiver');
  saveHex(rriColor                     ,'ColorRangemax');
  saveHex(portColor                    ,'ColorAirport');
  saveHex(AtsColor                     ,'ColorATS');
  saveHex(GpxColor                     ,'ColorGPX');
  saveHex(rrColor                      ,'ColorRangeCircles');
  saveHex(IffColor                     ,'ColorIFF');
  saveHex(BackColor                    ,'ColorBack');
  saveHex(LabelColor                   ,'ColorLabel');
  saveHex(ovlColor                     ,'ColorOverlay');

  //Text-Overlay
  saveBoolean(backgroundnottransparent1.checked    ,'OverlayBackTrans');

  //Aircraft Track
  saveBoolean(showpredictedposition1.checked       ,'ShowPredictedPosition');
  saveBoolean(colorbyaltitude1.checked             ,'ColorByAltitude');
  saveBoolean(allairtargetswithin180NM1.checked    ,'SingleFrameDetectAir');
  saveBoolean(allsurfacetargetswithin45NM1.checked ,'SingleFrameDetectSurface');

  //radiobuttons
  //Transition Altitude    #*#*#
  if N5000ft1.checked     then saveBoolean(N5000ft1.checked    , 'TransAlt5000ft');
  if N6000ft1.checked     then saveBoolean(N6000ft1.checked    , 'TransAlt6000ft');
  if N7000ft1.checked     then saveBoolean(N7000ft1.checked    , 'TransAlt7000ft');
  if N8000ft1.checked     then saveBoolean(N8000ft1.checked    , 'TransAlt8000ft');
  //tracklabel
  if N2lines1.checked     then saveBoolean(N2lines1.checked    , 'TrackLabel2Lines');
  if N3lines1.checked     then saveBoolean(N3lines1.checked    , 'TrackLabel3Lines');
  if N4lines1.checked     then saveBoolean(N4lines1.checked    , 'TrackLabel4Lines');  //für Squawk
  if N0lines.checked      then saveBoolean(N0lines.checked     , 'TrackLabel0Lines');
  saveBoolean(randomcolor1.checked, 'TrackLabeColorRandom');
  // aircraft symbol
  if circle1.checked      then saveBoolean(circle1.checked     , 'SymbolCircle');
  if rectangle1.checked   then saveBoolean(rectangle1.checked  , 'SymbolRectangle');
  if aircraft2.checked    then saveBoolean(aircraft2.checked   , 'SymbolAircraft');
  //osm
  if fullcolor1.checked   then saveBoolean(fullcolor1.checked  , 'ColorOsmFull');
  if pale1.checked        then saveBoolean(pale1.checked       , 'ColorOsmPale1');
  if pale2.checked        then saveBoolean(pale2.checked       , 'ColorOsmPale2');
  if gray1.checked        then saveBoolean(gray1.checked       , 'ColorOsmGray');
  //RR
  if N50NM1.checked       then saveBoolean(N50NM1.checked      , 'Rr50km');
  if N50km1.checked       then saveBoolean(N50km1.checked      , 'Rr50NM');
  if N5005ftaltitude1.checked then saveBoolean(N5005ftaltitude1.checked , 'Rr5000f');
  //airportname
  if normal2.checked      then saveBoolean(normal2.checked     , 'AirportNameNormal');
  if ICAO2.checked        then saveBoolean(ICAO2.checked       , 'AirportNameICAO');
  if IATA2.checked        then saveBoolean(IATA2.checked       , 'AirportNameIATA');
  if none2.checked        then saveBoolean(none2.checked       , 'AirportNameNone');
  //drop/hide aircraft
  if after20s1.checked    then saveBoolean(after20s1.checked   , 'AircraftDrop20s');
  if after1min1.checked   then saveBoolean(after1min1.checked  , 'AircraftDrop1min');
  if after5min1.checked   then saveBoolean(after5min1.checked  , 'AircraftDrop5min');
  if after30min1.checked  then saveBoolean(after30min1.checked , 'AircraftDrop30min');
  if after1hour1.checked  then saveBoolean(after1hour1.checked , 'AircraftDrop1hour');
  if never1.checked       then saveBoolean(never1.checked      , 'AircraftDrop10houre');
  if hideitafter20sec1.checked then saveBoolean(hideitafter20sec1.checked, 'AircraftHide20s');
  //koordinates
  if N5112341.checked         then saveBoolean(N5112341.checked    , 'Coordinates1');
  if N5111221.checked         then saveBoolean(N5111221.checked    , 'Coordinates2');
  if N5111222.checked         then saveBoolean(N5111222.checked    , 'Coordinates3');
  // tag frequency
  if N12MHz1.checked          then saveBoolean(N12MHz1.checked     , 'Tag12MHz');
  if N20MHz1.checked          then saveBoolean(N20MHz1.checked     , 'Tag20MHz');
  if usetimetag1.checked      then saveBoolean(usetimetag1.checked , 'TagActive');
  if enabletimetag1.checked   then saveBoolean(enabletimetag1.checked , 'TagActiveBeast');
  //memofelder
  if ListAllFrames            then saveBoolean(ListAllFrames       , 'Memo1ListAllFrames');
  if Memo2visible             then saveBoolean(Memo2visible        , 'Memo2visible');
  //heart beat
  if enableheartbeat1.checked then saveBoolean(enableheartbeat1.checked , 'HeartBeatActive');
  //HeadingPlusMinus
  if N03601.checked           then saveBoolean(N03601.checked      , 'Heading360');
  if N03591.checked           then saveBoolean(N03591.checked      , 'Heading180');
  // windrose
  if showMaximumonly1.checked then saveBoolean(showMaximumonly1.checked , 'RangeMaxMax');
  if showbyaltitude1.checked  then saveBoolean(showbyaltitude1.checked  , 'RangeMaxAltitude');
  if N10000ft1.checked        then saveBoolean(N10000ft1.checked        , 'Range00000');
  if N10000200001.checked     then saveBoolean(N10000200001.checked     , 'Range10000');
  if N20000ft30000ft1.checked then saveBoolean(N20000ft30000ft1.checked , 'Range20000');
  if N30000ft1.checked        then saveBoolean(N30000ft1.checked        , 'Range30000');
  // Background picture
  if SRTM1.checked            then saveBoolean(SRTM1.checked            , 'Background_SRTM');
  if MapQuestOSM1.checked     then saveBoolean(MapQuestOSM1.checked     , 'Background_MPQ');
  if MapQuestAerial1.checked  then saveBoolean(MapQuestAerial1.checked  , 'Background_MPQA');
  if OpenStreetMap1.checked   then saveBoolean(OpenStreetMap1.checked   , 'Background_OSM');
  // Town sizes
  if N1000001.checked         then saveBoolean(N1000001.checked         , 'Town_100k');
  if N5000001.checked         then saveBoolean(N5000001.checked         , 'Town_300k');
  if N1Million1.checked       then saveBoolean(N1Million1.checked       , 'Town_1M');
  if N3Million1.checked       then saveBoolean(N3Million1.checked       , 'Town_3M');
  // decodertype
  if adsbPIC2.checked         then saveBoolean(adsbPIC2.checked         , 'DecoderAdsbPIC');
  if GNS58901.checked         then saveBoolean(GNS58901.checked         , 'DecoderGNS5890');
  if rxControl1.checked       then saveBoolean(rxControl1.checked       , 'DecoderRxcontrol');
  if Beast1.checked           then saveBoolean(Beast1.checked           , 'DecoderBeast');
  //Beast control
  if enabletimetag1.checked   then saveBoolean(enabletimetag1.checked   , 'BeastMLAT');
  if onlyDF11171.checked      then saveBoolean(onlyDF11171.checked      , 'BeastDF11u17');
  if noCRCcheck1.checked      then saveBoolean(noCRCcheck1.checked      , 'BeastNoCRC');
  if suppressDF0451.checked   then saveBoolean(suppressDF0451.checked   , 'BeastNoDF045');
  if binaryformat1.checked    then saveBoolean(binaryformat1.checked    , 'BeastBinary');

  //maps
  if not Europewest1.enabled        then saveBoolean(not Europewest1.enabled  , 'MapsEuWest');
  if not EuropeUK1.enabled          then saveBoolean(not EuropeUK1.enabled    , 'MapsEuUK');
  if not EuropeNorth1.enabled       then saveBoolean(not EuropeNorth1.enabled , 'MapsEuNorth');
  if not EuropeSouth1.enabled       then saveBoolean(not EuropeSouth1.enabled , 'MapsEuSouth');
  if not EuropeEast1.enabled        then saveBoolean(not EuropeEast1.enabled  , 'MapsEuEast');
  if not EuropeBalkan1.enabled      then saveBoolean(not EuropeBalkan1.enabled, 'MapsEuBalkan');
  if not Russia1.enabled            then saveBoolean(not Russia1.enabled,       'MapsRussia');
  if not Afrika1.enabled            then saveBoolean(not Afrika1.enabled      , 'MapsAfrica');
  if not Asiawest1.enabled          then saveBoolean(not Asiawest1.enabled    , 'MapsAsiaWest');
  if not Asiacentral1.enabled       then saveBoolean(not Asiacentral1.enabled , 'MapsAsiaCentre');
  if not Asia1.enabled              then saveBoolean(not Asia1.enabled        , 'MapsAsiaEast');
  if not Antarctika1.enabled        then saveBoolean(not Antarctika1.enabled  , 'MapsAntarctica');
  if not Northamerica1.enabled      then saveBoolean(not Northamerica1.enabled, 'MapsAmericaNorthCan');
  if not NorthamericaUSA1.enabled   then saveBoolean(not NorthamericaUSA1.enabled, 'MapsAmericaNorthUsa');
  if not NorthamericaCarib2.enabled then saveBoolean(not NorthamericaCarib2.enabled, 'MapsAmericaNorthCar');
  if not Southamerica1.enabled      then saveBoolean(not Southamerica1.enabled, 'MapsAmericaSouth');

  //Log
  saveBoolean(LogSel  ,'LogSelect');
  saveBoolean(LogDet  ,'LogDetected');

  if ConnectatStart1.checked  then saveBoolean(ConnectatStart1.checked , 'AutoConnect');

  //Position Mode
  saveBoolean(defaultposition1.Checked, 'DefaultPosition');
  saveBoolean(lastposition1.Checked, 'LastPosition');      

  //networking
  saveString(ComPort1.Port,            'ComPort');
  //saveInteger(ord(ComPort1.BaudRate),  'BaudRate');  // unnötig
  saveInteger(ComPort1.CustomBaudRate, 'CustomBaudRate');
  saveString(ComPort2.Port,            'ComPort2');
  saveInteger(ord(ComPort2.BaudRate),  'BaudRate2');
  saveInteger(ServerSocket.Port,       'ServerPort');
  saveInteger(ServerSocketRAW.Port,    'RAWServerPort');
  saveBoolean(NetUseBinFormat,         'NetUseBinFormat'); // muss vor clientport stehen
  saveInteger(ClientSocket.Port,       'ClientPort');
  saveString(ClientSocket.Host,        'ClientHost');
  if Serveractive1.checked    then saveBoolean(Serveractive1.checked       , 'ServerActive');
  if RAWServeractive1.checked then saveBoolean(RAWServeractive1.checked    , 'RAWServerActive');
  saveBoolean(LocalRawOnly,            'LocalRawOnly');

  //I2C
  if donotuse1.checked        then saveBoolean(donotuse1.checked   , 'I2C_off');
  if C2308C8E001.checked      then saveBoolean(C2308C8E001.checked , 'I2C_fix');
  if other1.checked           then saveBoolean(other1.checked      , 'I2C_flex');
  if i2cdata[0]>0 then begin
    for k:=0 to i2cdata[0] do saveHex8(i2cdata[k], 'I2C_Data'+inttostr(k));
  end;
  //watchlist
  for k:=1 to maxWatchList do if (Watchlist[k].AA<>0) or (Watchlist[k].ident<>'') then begin
    saveString(Watchlist[k].ident, 'Watchlist'+inttostr(k)+'ident');
    saveHex(Watchlist[k].AA, 'Watchlist'+inttostr(k)+'AA');
    saveBoolean(Watchlist[k].active, 'Watchlist'+inttostr(k)+'activ');
  end;

  //html
  saveString(htmlpath,         'HtmlPath');

  writeln(initfile,'#end of ini-file');
  closefile(initfile);
end;   //SaveInit


//**** M A P S   L A D E N **************************************************

//Westeuropa
procedure TForm1.load_Eu_west(Sender: TObject);
begin
  LoadAllMaps(pfad+'maps\europe\west');
  Europewest1.enabled:=false;
  Europewest1.checked:=true;
end;  //load_Eu_west

//GB  Irland
procedure TForm1.load_Eu_UK(Sender: TObject);
begin
  LoadAllMaps(pfad+'maps\europe\uk');
  EuropeUK1.enabled:=false;
  EuropeUK1.checked:=true;
end;   //load_Eu_UK

//Scandinavien
procedure TForm1.load_Eu_North(Sender: TObject);
begin
  LoadAllMaps(pfad+'maps\europe\north');
  EuropeNorth1.enabled:=false;
  EuropeNorth1.checked:=true;
end; //load_Eu_North

//suedeuropa
procedure TForm1.load_Eu_South(Sender: TObject);
begin
  LoadAllMaps(pfad+'maps\europe\south');
  EuropeSouth1.enabled:=false;
  EuropeSouth1.checked:=true;
end; //load_Eu_South

//Osteuropa
procedure TForm1.load_Eu_East(Sender: TObject);
begin
  LoadAllMaps(pfad+'maps\europe\east');
  EuropeEast1.enabled:=false;
  EuropeEast1.checked:=true;
end;  // load_Eu_East

// russland
procedure TForm1.load_Russia(Sender: TObject);
begin
  LoadAllMaps(pfad+'maps\gus');
  Russia1.enabled:=false;
  Russia1.checked:=true;
end;  //load_Russia

//balkan
procedure TForm1.load_Eu_balkan(Sender: TObject);
begin
  EuropeBalkan1.enabled:=false;
  EuropeBalkan1.checked:=true;
  LoadAllMaps(pfad+'maps\europe\balkan');
end;   //load_Eu_balkan

procedure TForm1.Afrika1Click(Sender: TObject);
begin
  Afrika1.enabled:= false;
  Afrika1.checked:= true;
  LoadAllMaps(pfad+'maps\africa');
end;

procedure TForm1.Asiawest1Click(Sender: TObject);
begin
  Asiawest1.enabled:= false;
  Asiawest1.checked:= true;
  LoadAllMaps(pfad+'maps\asia\west');
end;

procedure TForm1.Asiacentral1Click(Sender: TObject);
begin
  Asiacentral1.enabled:= false;
  Asiacentral1.checked:= true;
  LoadAllMaps(pfad+'maps\asia\central');
end;

procedure TForm1.Asia1Click(Sender: TObject);
begin
  Asia1.enabled:= false;
  Asia1.checked:= true;
  LoadAllMaps(pfad+'maps\asia\east');
end;

procedure TForm1.Antarctika1Click(Sender: TObject);
begin
  Antarctika1.enabled:= false;
  Antarctika1.checked:= true;
  LoadAllMaps(pfad+'maps\antarctica');
end;

procedure TForm1.Northamerica1Click(Sender: TObject);
begin
  Northamerica1.enabled:= false;
  Northamerica1.checked:= true;
  LoadAllMaps(pfad+'maps\northamerica\canada');
end;

procedure TForm1.NorthamericaUSA1Click(Sender: TObject);
begin
  NorthamericaUSA1.enabled:= false;
  NorthamericaUSA1.checked:= true;
  LoadAllMaps(pfad+'maps\northamerica\usa');
end;

procedure TForm1.NorthamericaCarib2Click(Sender: TObject);
begin
  NorthamericaCarib2.enabled:= false;
  NorthamericaCarib2.checked:= true;
  LoadAllMaps(pfad+'maps\northamerica\carib');
end;

procedure TForm1.Southamerica1Click(Sender: TObject);
begin
  Southamerica1.enabled:= false;
  Southamerica1.checked:= true;
  LoadAllMaps(pfad+'maps\southamerica');
end;

procedure TForm1.removeallMaps1Click(Sender: TObject);
begin
  pcounter              := 0;
  Lcounter              := 0;

  Europewest1.checked        := false;
  EuropeUK1.checked          := false;
  EuropeNorth1.checked       := false;
  EuropeSouth1.checked       := false;
  EuropeEast1.checked        := false;
  EuropeBalkan1.checked      := false;
  Russia1.checked            := false;
  Afrika1.checked            := false;
  Asiawest1.checked          := false;
  Asiacentral1.checked       := false;
  Asia1.checked              := false;
  Antarctika1.checked        := false;
  Northamerica1.checked      := false;
  NorthamericaUSA1.checked   := false;
  NorthamericaCarib2.checked := false;
  Southamerica1.checked      := false;     

  checkcontinents;
  repaintPPI(true, true); //die Wekltkarte muss neu berechnet werden
end;


// aus dem 24-Bit-code das land ermitteln
Function TForm1.AA2Land(AA : dword):string;
var
  Land : string;
  k    : integer;
{
http://www.kloth.net/radio/icao-id.php
http://www.kloth.net/radio/icao24alloc.php   ist detaillierter
State             Block (binary)                  (range in hex)    (range in octal)
South Africa      0000 00 001 ... .. ..........   008000 - 00FFFF     100000 -   177777
Egypt             0000 00 010 ... .. ..........   010000 - 017FFF     200000 -   277777
Morocco           0000 00 100 ... .. ..........   020000 - 027FFF     400000 -   477777
Ethiopia          0000 01 000 000 .. ..........   040000 - 040FFF    1000000 -  1007777
Mauritius         0000 01 100 000 00 ..........   060000 - 0603FF    1400000 -  1401777
Qatar             0000 01 101 010 00 ..........   06A000 - 06A3FF    1520000 -  1521777
Algeria           0000 10 100 ... .. ..........   0A0000 - 0A7FFF    2400000 -  2477777
Panama            0000 11 000 010 .. ..........   0C2000 - 0C2FFF    3020000 -  3027777
Russia            0001 .. ... ... .. ..........   100000 - 1FFFFF    4000000 -  7777777
Italy             0011 00 ... ... .. ..........   300000 - 33FFFF   14000000 - 14777777
Spain             0011 01 ... ... .. ..........   340000 - 37FFFF                        by sprut
France            0011 10 ... ... .. ..........   380000 - 3BFFFF   16000000 - 16777777
Germany           0011 11 ... ... .. ..........   3C0000 - 3FFFFF   17000000 - 17777777
UK Great Britain  0100 00 ... ... .. ..........   400000 - 43FFFF   20000000 - 20777777
Finland           0100 01 100 ... .. ..........   460000 - 467FFF   21400000 - 21477777
Netherlands       0100 10 000 ... .. ..........   480000 - 487FFF   22000000 - 22077777
Switzerland       0100 10 110 ... .. ..........   4B0000 - 4B7FFF   22600000 - 22677777
Luxembourg        0100 11 010 000 00 ..........   4D0000 - 4D03FF   23200000 - 23201777
Turkmenistan      0110 00 000 001 10 .........    601800 - 601BFF   30014000 - 30015777
Saudi Arabia      0111 00 010 ... .. ..........   710000 - 717FFF   34200000 - 34277777
Korea Rep.        0111 00 011 ... .. ..........   718000 - 71FFFF   34300000 - 34377777
Singapore         0111 01 101 ... .. ..........   768000 - 76FFFF   35500000 - 35577777
China             0111 10 ... ... .. ..........   780000 - 7BFFFF   36000000 - 36777777
Japan             1000 01 ... ... .. ..........   840000 - 87FFFF   41000000 - 41777777
Thailand          1000 10 000 ... .. ..........   880000 - 887FFF   42000000 - 42077777
U.A. Emirates     1000 10 010 110 .. ..........   896000 - 896FFF   42260000 - 42267777
Taiwan            1000 10 011 001 00 ..........   899000 - 8993FF   42310000 - 42311777
USA               1010 .. ... ... .. ..........   A00000 - AFFFFF   50000000 - 53777777
}
begin
  Land := 'ufo';
  if icaocounter=0 then begin
    // das ist nur noch fallback-loesung
    case AA of
      $008000 .. $00FFFF: Land:='South Africa';
      $010000 .. $017FFF: Land:='Egypt';
      $020000 .. $027FFF: Land:='Morocco';
      $040000 .. $040FFF: Land:='Ethiopia';
      $060000 .. $0603FF: Land:='Mauritius';
      $06A000 .. $06A3FF: Land:='Qatar';
      $0A0000 .. $0A7FFF: Land:='Algeria';
      $0C2000 .. $0C2FFF: Land:='Panama';
      $100000 .. $1FFFFF: Land:='Russia';
      $300000 .. $33FFFF: Land:='Italy';
      $340000 .. $37FFFF: Land:='Spain';
      $380000 .. $3BFFFF: Land:='France';
      $3C0000 .. $3FFFFF: Land:='Germany';
      $400000 .. $43FFFF: Land:='UK'; // Great Britain';
      $460000 .. $467FFF: Land:='Finland';
      $480000 .. $487FFF: Land:='Netherlands';
      $4B0000 .. $4B7FFF: Land:='Switzerland';
      $4D0000 .. $4D03FF: Land:='Luxembourg';
      $601800 .. $601BFF: Land:='Turkmenistan';
      $710000 .. $717FFF: Land:='Saudi Arabia';
      $718000 .. $71FFFF: Land:='Korea Rep.';
      $768000 .. $76FFFF: Land:='Singapore';
      $780000 .. $7BFFFF: Land:='China';
      $840000 .. $87FFFF: Land:='Japan';
      $880000 .. $887FFF: Land:='Thailand';
      $896000 .. $896FFF: Land:='Emirates'; //U.A. Emirates';
      $899000 .. $8993FF: Land:='Taiwan';
      $A00000 .. $AFFFFF: Land:='USA';
    end;
    result:=Land;
  end else begin
    // das ist die richtige routine
    for k:=0 to icaocounter do begin
      if (AA >= ICAO[k].anfang) and (AA <= ICAO[k].ende) then begin
        Land:=ICAO[k].name;
        break;
      end;
    end;
    result:=Land;
  end;
  // zu lange namen kuerzen
  if result='Democratic People''s Republic of Korea ' then result :='DPR Korea'     else
  if result='Democratic Republic of the Congo '       then result :='Congo'         else
  if result='United Republic of Tanzania '            then result :='Tanzania'      else
  if result='Republic of Macedonia '                  then result :='Macedonia'     else
  if result='Netherlands, Kingdom of the '            then result :='Netherlands'   else
  if result='United Arab Emirates '                   then result :='U.A. Emirates' else
  if result='United Kingdom '                         then result :='UK'            else
  if result='Republic of Korea '                      then result :='Korea'         else
  if result='Republic of Moldova '                    then result :='Moldova'       else
  if result='United Republic of Tanzania '            then result :='Tanzania'      else
  if result='United States '                          then result :='USA';
  //ich beschraenke die Laenge auf 14 Zeichen
  if length(result)>14 then result := copy(result, 1, 14);
  // mindestlänge auf 10 setzen
  while length(result)<10 do result:=result+' ';
end; //AA2Land


// Inhalt der Frames aus Frames[] decodieren
// das ist eine monsterfunktion mit diversen unterfunktionen
procedure TForm1.Decode;
var AFrame   : TFrame;
    DF       : byte;      // DF nummer Bit 1..5
    CA_CF_AF : byte;      // Bit 6..8
    AA       : DWORD;     // Bit 9..32
    AC       : integer;
    ID       : integer;
    CRC      : dword;
    CRC1     : dword;
    nr       : integer;   // Flugzeugnummer in planes
    k        : integer;
    Rc       : real;
    memoline : string;   //das wird dann in memo2 geschrieben
    typstr   : string;
    AAstr    : string;
    alt_st   : string;
    koord_st : string;
    speed_st : string;
    ident_st : string;
    AC_st    : string;
    ID_st    : string;
    hoehenart_st : string;
    fst          : string;          // fuer filter

    TISB_exists : boolean;  
    chricao_fail : boolean;

    ME_type     : integer;
    ME_subtype  : integer;
    ME_status   : integer;
    ME_altitude : integer;
    ME_time     : integer;
    ME_cprForm  : integer;
    ME_cprLat   : integer;
    ME_cprLong  : integer;
    ME_movement : integer;
    ME_hdStatus : integer;
    ME_hdTrack  : integer;
    ME_ident    : string[8];

    ME_direction_West : integer;
    ME_speed_West     : integer;
    ME_direction_South: integer;
    ME_speed_South    : integer;
    ME_heading_status : integer;
    ME_heading        : integer;
    ME_airspeed_type  : integer;
    ME_airspeed       : integer;
    ME_vert_sign      : integer;
    ME_vert_rate      : integer;

    altitude_ft     : integer;
    up_alt          : boolean;
    up_koord_surf   : boolean;
    up_koord_air    : boolean;
    up_speed_ground : boolean;
    up_speed_air    : boolean;
    up_ident        : boolean;
    up_airborne     : boolean;
    up_ground       : boolean;


    interogatorID   : dword;
    ServerMsg       : integer;
    RAWnachricht    : string[255];

    DecoderHeartBeat : THeartBeat;


  // suche in planes[] ein flugzeug mit der 24Bit-ID AA
  function findPlane(AA :dword):integer;
  var k : integer;
  begin
    result := maxPlane+1;
    for k:=0 to lastPlane do
      if  planes[k].active and (planes[k].AA=AA) then begin
        result := k;
        planes[k].time_last := now;
        break;
      end;
  end; //findPlane


  // neues Flugzeug in planes[] eintragen
  function newPlane(AA :dword):integer;
  var styp : string;
  begin
    result := -1;
    repeat
      inc(result);
    until (planes[result].active=false) or (result>=maxPlane);
    //das geht nicht schief! im schlimmsten fall ist result=maxplane
    //also existiert planes[result] immer
    //falls planes voll ist, dann ist planes[result].active=true, und nichts wird getan
    if (planes[result].active=false) then begin
      if result > lastplane then lastplane := result;
      planes[result].active     := true;
      planes[result].missed     := false;
      planes[result].AA         := AA;
      planes[result].lat_0      := 0;
      planes[result].lat_1      := 0;
      planes[result].long_0     := 0;
      planes[result].long_1     := 0;
      planes[result].time_0     := 0;
      planes[result].time_1     := 0;
      planes[result].latitude   := 0;     // in grad
      planes[result].longitude  := 0;     // in grad
      planes[result].CPR_quality:=0;
      planes[result].CPR_J      := -1;    // ungueltige Breitengradzohne
      planes[result].CPR_NL     := -1;    // ungueltige Laengengradzohne
      planes[result].altitude   := 0;
      planes[result].speed      := 0;
      planes[result].heading    := noheading;
      planes[result].time_first := now;
      planes[result].time_last  := planes[result].time_first;
      planes[result].time_koord := 0; //planes[result].time_first;
      planes[result].ident      := '';
      planes[result].interogator:= -1;
      planes[result].airborne   := true;
      planes[result].crosslink  := false;
      planes[result].alert      := false;
      planes[result].spi        := false;
      planes[result].steigen    := 0;
      planes[result].mod3id     := 0;
      planes[result].squawk     := '';
      planes[result].distance   := NoRange;
      planes[result].gesendet   := false;
      planes[result].mops       := 0;
      planes[result].nica       := 0;
      planes[result].nicb       := 0;
      planes[result].imf        := 0;
      planes[result].nac        := 0;
      planes[result].om         := 0;
      planes[result].cc         := 0;
      planes[result].symbol     := sym_medium;

//      planes[result].color := random(150)*$10000+random(150)*$100+random(150);
      case (result mod 8) of
        0: planes[result].color := clBlack;
        1: planes[result].color := clRed;
        2: planes[result].color := clGreen;
        3: planes[result].color := clBlue;
        4: planes[result].color := clDkGray;
        5: planes[result].color := clPurple;
        6: planes[result].color := clOlive;
        7: planes[result].color := clNavy;
      end;
      planes[result].trackindex := -1;                 // noch keine Koordinaten
      planes[result].airframe   := findairframe(AA);   // bekanntes flugzeug?
      if planes[result].airframe.known = false then reportufo(AA) else begin
        styp := copy(planes[result].airframe.kenner.typs,1,3);
        if styp='A38' then planes[result].symbol := sym_big;
        if styp='A35' then planes[result].symbol := sym_big;
        if styp='A34' then planes[result].symbol := sym_big;
        if styp='B74' then planes[result].symbol := sym_big;
        if styp='B77' then planes[result].symbol := sym_big;
        if styp='DC1' then planes[result].symbol := sym_big; // DC-10
        if styp='MD1' then planes[result].symbol := sym_big; // MD-11
        if styp='C17' then planes[result].symbol := sym_big;
        if styp='A12' then planes[result].symbol := sym_big; // Antonow 124
      end;
      WatchListForm.CheckNewPlane(AA, result);
      ServerSendAir(result);
      if planes[result].airframe.known then     
        reportseen(planes[result].aa, planes[result].airframe.kenner.name, planes[result].airframe.kenner.typs, planes[result].airframe.kenner.typl, '>', '')
      else
        reportseen(planes[result].aa, '#'+AA2Land(planes[result].aa), '', '', '>', '');
    end else result := maxPlane+1;
  end;   //newPlane


  // wandelt grey-code in binärcode
  function decode_grey(wert : word):word;
  var maske   : word;
      k       : integer;
      copybit : boolean;
  begin
    copybit := false;
    result  := 0;
    maske   := $80;
    for k:= 1 to 16 do begin
      if (wert and maske<>0) then copybit:=not copybit;
      if copybit then result := result or maske;
      maske := maske shr 1;
    end;
  end;  // decode_grey


  // bestimmt die Höhe aus dem alten grey-code
  // Ergebnis ist in feet
  // bei offensichtlich fehlerhaftem grey-code wird NoAltitude zurueckgegeben
  function decode_grey_altitude(Altitude_code :integer):integer;
  var
      altitude_grey : word;
      subcode       : word;
      altitude_grey_sub : word;
      valid             : boolean;
  begin
      valid  := true;
      result := NoAltitude;
      altitude_grey := decode_grey(Altitude_code shr 3);   // mit 500 ft aufloesung
      subcode := Altitude_code and $07;
      altitude_grey_sub := 0;
      if altitude_grey mod 2 =0 then begin
        case subcode of
          1: altitude_grey_sub := 0;
          3: altitude_grey_sub := 1;
          2: altitude_grey_sub := 2;
          6: altitude_grey_sub := 3;
          4: altitude_grey_sub := 4;
        else
          valid := false;
        end
      end else begin
        case subcode of
          4: altitude_grey_sub := 0;
          6: altitude_grey_sub := 1;
          2: altitude_grey_sub := 2;
          3: altitude_grey_sub := 3;
          1: altitude_grey_sub := 4;
        else
          valid := false;
        end
      end;
      if valid then result := altitude_grey*500 +  altitude_grey_sub*100 -1200;
  end;  //decode_grey_altitude



  //wandelt Mode-3A Code in Integer mit 4 Ziffern (eigentlich octal)
  //führende Nullen werden bei der Anzeige leider ausgeblendet z.B. 33 ist eigentlich 0033
  function M3ACode(AFr2,AFr3 : byte):integer;           // AFrame.B[2],AFrame.B[3]
  var
    A1,A2,A4,B1,B2,B4,C1,C2,C4,D1,D2,D4 :byte;
  begin
    // -- -- -- C1 A1 C2 A2 C4 / A4 xx B1 D1 B2 D2 B4 D4
    A4:= (AFr3 shr 7) and $01;
    A2:= (AFr2 shr 1) and $01;
    A1:= (AFr2 shr 3) and $01;

    B4:= (AFr3 shr 1) and $01;
    B2:= (AFr3 shr 3) and $01;
    B1:= (AFr3 shr 5) and $01;

    C4:= (AFr2)       and $01;
    C2:= (AFr2 shr 2) and $01;
    C1:= (AFr2 shr 4) and $01;

    D4:= (AFr3)       and $01;
    D2:= (AFr3 shr 2) and $01;
    D1:= (AFr3 shr 4) and $01;

    result := (A4*4+A2*2+A1)*1000 +  (B4*4+B2*2+B1)*100 + (C4*4+C2*2+C1)*10 +(D4*4+D2*2+D1);
  end;


  // sortiert die 12Bit des alten Oktalcodes in binärer reihenfolge für ID
  //3.1.1.6. Starting with bit 20, the sequence shall be
  //input: C1, A1, C2, A2, C4, // A4, ZERO, B1, D1, B2, D2, B4, D4.
  //output:    A4  A2  A1  B4  // B2  B1    C4  C2  C1  D4  D2  D1
  // Order : A B C D
  function sort_olde_code_bits_ID(oktalcode :word):word;
  begin
    // zero entfernen
    result:= ((oktalcode and $080) shl  4) or       //A4
             ((oktalcode and $200) shl  1) or       //A2
             ((oktalcode and $800) shr  2) or       //A1

             ((oktalcode and $002) shl  7) or       //B4
             ((oktalcode and $008) shl  4) or       //B2
             ((oktalcode and $020) shl  1) or       //B1

             ((oktalcode and $100) shr  3) or       //C4
             ((oktalcode and $400) shr  6) or       //C2
             ((oktalcode and $1000)shr  9) or       //C1

             ((oktalcode and $001) shl  2) or       //D4
             ((oktalcode and $004) shr  1) or       //D2
             ((oktalcode and $010) shr  4) ;        //D1
  end; //sort_olde_code_bits_ID


  // das ist die Order für die alte hoehe, und diese Funktion wird
  // dafür verwendet, wenn ME decodiert wird
  //bitte nicht für ID-Dekodierung verwenden oder verändern
  function sort_olde_code_bits(oktalcode :integer):integer;
  begin
    result:= ((oktalcode and $004) shl  8) or       //D2
             ((oktalcode and $001) shl  9) or       //D4
             ((oktalcode and $400) shr  2) or       //A1
             ((oktalcode and $100) shr  1) or       //A2
             ((oktalcode and $040)       ) or       //A4
             ((oktalcode and $020)       ) or       //B1
             ((oktalcode and $008) shl  1) or       //B2
             ((oktalcode and $002) shl  2) or       //B4
             ((oktalcode and $800) shr  9) or       //C1
             ((oktalcode and $200) shr  8) or       //C2
             ((oktalcode and $080) shr  7) ;        //C4
  end; //sort_olde_code_bits


  // hoehe decodieren
  function get_Hoehe_ft(ME_altitude:integer):integer;
  var altitude_code : word;
      altitude_ft   : integer;
  begin
    //****** Hoehe ***********
    hoehenart_st := '';
    if (ME_altitude and $010) = 0 then begin
      altitude_code := sort_olde_code_bits (ME_altitude);
      altitude_ft   := decode_grey_altitude(Altitude_code);
      hoehenart_st := '(old)  ';
    end else begin
      altitude_code := ((ME_altitude and $FE0) shr  1) or (ME_altitude and $00F);
      altitude_ft   := Altitude_code * 25 - 1000;
    end;
    result := altitude_ft;
  end; //get_Hoehe_ft


  //wandelt Mode-C Altitude Code in Integer (Alt in ft)
  function MC_Code(AFr2,AFr3 : byte):integer;
  var
    A1,A2,A4,B1,B2,B4,C1,C2,C4,D2,D4,M1,Q1 :byte;
    Alt1:integer;
  begin
    A4:= (AFr3 shr 7) and $01;
    A2:= (AFr2 shr 1) and $01;
    A1:= (AFr2 shr 3) and $01;

    B4:= (AFr3 shr 1) and $01;
    B2:= (AFr3 shr 3) and $01;
    B1:= (AFr3 shr 5) and $01;

    C4:= (AFr2 )      and $01;
    C2:= (AFr2 shr 2) and $01;
    C1:= (AFr2 shr 4) and $01;

    D4:= (AFr3 )      and $01;
    D2:= (AFr3 shr 2) and $01;
    M1:= (AFr3 shr 6) and $01;
    Q1:= (AFr3 shr 4) and $01;

    hoehenart_st := '';
    if (Q1=1) then begin
      Alt1 := (C1*1024 + A1*512 + C2*256 + A2*128 + C4*64 + A4*32 //das funktioniert bereits
              + B1*16 + B2*8 + D2*4 +B4*2+ D4) * 25 -1000;
      if (M1=1) then begin
        Alt1 := round( (Alt1+1000) /25 );  // wieder den binärwert ermitteln;
        Alt1 := Alt1 *10 -300;             // hoehe in metern
        Alt1 := round (Alt1 *3.281);       // meter in fuß umrechnen
      end;
    end else begin
      Alt1 := (D2*1024 + D4*512 + A1*256
               + A2*128 + A4*64 + B1*32 +
               B2*16 + B4*8 + C1*4 + C2*2 + C4) ; //nur zum Test später noch umrechnen !!!
      Alt1 := decode_grey_altitude(Alt1);
      hoehenart_st := '(old)  ';
    end;
    result:=Alt1;
  end;  //MC_Code


  //The number of even longitude zones at a latitude is called NL.
  //number of longitude zones (NL) as a function of latitude
  // darf nur mit -90 ... +90 aufgerufen werden
  function NL(rlat : real):integer;
  var lat   : real;
      NLlat : integer;
  begin
    lat    := abs(rlat);
    // Tabelle aus 1090-WP-9-14
    If      lat <  10.47047130 Then NLlat :=  59
    Else if lat <  14.82817437 Then NLlat :=  58
    Else if lat <  18.18626357 Then NLlat :=  57
    Else if lat <  21.02939493 Then NLlat :=  56
    Else if lat <  23.54504487 Then NLlat :=  55
    Else if lat <  25.82924707 Then NLlat :=  54
    Else if lat <  27.93898710 Then NLlat :=  53
    Else if lat <  29.91135686 Then NLlat :=  52
    Else if lat <  31.77209708 Then NLlat :=  51
    Else if lat <  33.53993436 Then NLlat :=  50
    Else if lat <  35.22899598 Then NLlat :=  49
    Else if lat <  36.85025108 Then NLlat :=  48
    Else if lat <  38.41241892 Then NLlat :=  47
    Else if lat <  39.92256684 Then NLlat :=  46
    Else if lat <  41.38651832 Then NLlat :=  45
    Else if lat <  42.80914012 Then NLlat :=  44
    Else if lat <  44.19454951 Then NLlat :=  43
    Else if lat <  45.54626723 Then NLlat :=  42
    Else if lat <  46.86733252 Then NLlat :=  41
    Else if lat <  48.16039128 Then NLlat :=  40
    Else if lat <  49.42776439 Then NLlat :=  39
    Else if lat <  50.67150166 Then NLlat :=  38
    Else if lat <  51.89342469 Then NLlat :=  37
    Else if lat <  53.09516153 Then NLlat :=  36
    Else if lat <  54.27817472 Then NLlat :=  35
    Else if lat <  55.44378444 Then NLlat :=  34
    Else if lat <  56.59318756 Then NLlat :=  33
    Else if lat <  57.72747354 Then NLlat :=  32
    Else if lat <  58.84763776 Then NLlat :=  31
    Else if lat <  59.95459277 Then NLlat :=  30
    Else if lat <  61.04917774 Then NLlat :=  29
    Else if lat <  62.13216659 Then NLlat :=  28
    Else if lat <  63.20427479 Then NLlat :=  27
    Else if lat <  64.26616523 Then NLlat :=  26
    Else if lat <  65.31845310 Then NLlat :=  25
    Else if lat <  66.36171008 Then NLlat :=  24
    Else if lat <  67.39646774 Then NLlat :=  23
    Else if lat <  68.42322022 Then NLlat :=  22
    Else if lat <  69.44242631 Then NLlat :=  21
    Else if lat <  70.45451075 Then NLlat :=  20
    Else if lat <  71.45986473 Then NLlat :=  19
    Else if lat <  72.45884545 Then NLlat :=  18
    Else if lat <  73.45177442 Then NLlat :=  17
    Else if lat <  74.43893416 Then NLlat :=  16
    Else if lat <  75.42056257 Then NLlat :=  15
    Else if lat <  76.39684391 Then NLlat :=  14
    Else if lat <  77.36789461 Then NLlat :=  13
    Else if lat <  78.33374083 Then NLlat :=  12
    Else if lat <  79.29428225 Then NLlat :=  11
    Else if lat <  80.24923213 Then NLlat :=  10
    Else if lat <  81.19801349 Then NLlat :=  9
    Else if lat <  82.13956981 Then NLlat :=  8
    Else if lat <  83.07199445 Then NLlat :=  7
    Else if lat <  83.99173563 Then NLlat :=  6
    Else if lat <  84.89166191 Then NLlat :=  5
    Else if lat <  85.75541621 Then NLlat :=  4
    Else if lat <  86.53536998 Then NLlat :=  3
    Else if lat <  87.00000000 Then NLlat :=  2
    Else                            NLlat :=  1;
    result :=NLlat;
  end;   // function NL


  // stets positiver modulo
  function Pmod(a,b : integer):integer;
  begin
    result := a mod b;
    if a<0 then result:= result + b;
  end;


  // floor of 2.3 is 2.0
  // floor of 3.8 is 3.0
  // floor of -2.3 is -3.0
  // floor of -3.8 is -4.0
  // result := trunc(x+10000)-10000
  //
  // y := round(abs(x+2))
  // result :=  trunc(x+y)-y
//  function floor(x : real):integer;
//  begin
//    if x>=0 then result:= trunc(x)
//    else begin
//      result := trunc(x-0.999999999);
//    end;
//  end;  // floor


  // Breitengrad ist zwischen -90 und +90
  function correct_lat(lat:real):real;
  begin
    result:=lat;
    if lat> 90 then result:=lat-360;
    if lat<-90 then result:=lat+360;
    if abs(result)>90 then memo2_lines_add('### CPR-decoding-error');
  end;  // correct_lat


  // falls der Track komplett voll ist
  //dann jeden zweiten punkt wegwerfen und die restlichen punkte zusammenschieben
  procedure CompressTrack(nr: integer);
  var k    : integer;
      last : integer;
  begin
    //Planes[nr].trackindex:=0;
    last :=  Planes[nr].trackindex div 2;
    for k:=1 to last do Planes[nr].Track[k] := Planes[nr].Track[2*k];
    Planes[nr].trackindex := last+1;
    memo1.lines.add('Compress Track '+inttostr(nr));
  end; //CompressTrack


  // findet zu den hochaufloesenden longitude-koordinaten koor, die passenden globalen
  // koordinaten, die am dichtesten an ref liegen.
  // ref ist radiant!
  function cpr2ref(ref, koord :real):real;
  begin
    result := koord + 90 * round((ref/rad-koord)/(90));
  end; //cpr2ref

  // modulo mit real, lol
  function cpr_mod(x,y:real):real;
  begin
    if (x<0) then x:= x+360;
    result := x-y*floor(x/y);
  end; //cpr_mod


  // single frame update of position
  // siehe "1090-WP30-21-Appendix_A Mods.pdf"  A.1.7.5 Locally Unambiguous CPR Decoding for Airborne, TIS-B and Intent Lat/Lon
  // input:  last known position of aircraft in grad, cprLat, cprLong, cprForm
  // output: new position
  // Werte in Tkoordinaten hier ausnamsweise in grad !!
  function SFU (yz, xz, i :integer; old_breite, old_laenge : real; cprWorld :integer):Tkoordinaten;
  var
    j, m : integer;
    dlat, dlon : real;
  begin
    //Breite
    dlat := cprWorld / (60-i);
    j := floor(old_breite/dlat) + floor(0.5 + cpr_mod(old_breite, dlat)/dlat - yz/131072);
    result.breite := dlat * (j + yz/131072);
    //Laenge
    if NL(result.breite)=1 then dlon := cprWorld else dlon := cprWorld/(NL(result.breite)-I);
    m := floor(old_laenge/dlon) + floor(0.5 + cpr_mod(old_laenge, dlon)/dlon - xz/131072);
    result.laenge := dlon * (m + xz/131072);
  end;  // SFU


  // position des Flugzeugs decodieren
  // und in Track eintragen
  // 1 : airborne position, global
  // 2 : surface position, 0..90
  function Get_Koord(cpr_type : integer):string;
  const
      TwoPowerNb = 131072;      // 2^17 = 131072
  var
      J        : integer;
      rlat_0   : real;
      rlat_1   : real;
      Latitude : real;
      NL0      : integer;
      NL1      : integer;
      nlt      : integer;
      I        : integer;
      ni       : integer;
      dlon     : real;
      M        : integer;
      Lon      : real;
      Lont     : integer;
      zeitdifferenz : integer;
      AirDlat0 : real;
      AirDlat1 : real;
      SFUKoord : Tkoordinaten;
      koord_ok : byte;
      maxtimegap : integer;
      RefLang    : real;
      RefBreit   : real;
      SfuDist    : real;
      maxSfsuDist: real;
      cprWorld   : integer;
  begin
    koord_ok := 0;
    result   := '*';
    I        := ME_cprForm; // even or odd
    Latitude := 0;
    Lon      := 0;
    NL0      := 0;

    if ME_cprForm=0 then begin
      Planes[nr].lat_0  :=  ME_cprLat;
      Planes[nr].long_0 :=  ME_cprLong;
      Planes[nr].time_0 :=  now;
      Planes[nr].cpr_type_0 := cpr_type;
    end else begin
      Planes[nr].lat_1  :=  ME_cprLat;
      Planes[nr].long_1 :=  ME_cprLong;
      Planes[nr].time_1 :=  now;
      Planes[nr].cpr_type_1 := cpr_type;
    end;

    // wenn daten da sind und zwischen den frames maximal 10 Sekunden liegen , dann global berechnen
    // fuer surface position sind bis max 25 knoten 50 sekunden zulaessig, darueber 25 sekunden
    maxtimegap  := 10;
    maxSfsuDist := 180;
    cprWorld    := 360;        // innerhalb dieses Bereichs wird gearbeitet
    case cpr_type of
    cpr_airborne: begin    // airborne
         cprWorld   := 360;              // global
         maxtimegap := 10;
         maxSfsuDist:= 180;
       end;
    cpr_surface: begin    // surface
         cprWorld   := 90;              // local
         maxtimegap := 25;
         maxSfsuDist:= 45;
       end;
    end; //case
    AirDlat0   := cprWorld/60;
    AirDlat1   := cprWorld/59;

    planes[nr].time_last := now;

    // Berechnung der Zeitdifferenz zwischen even und odd frame in Sekunden
    zeitdifferenz := 100;
    if (Planes[nr].time_0 <> 0) and (Planes[nr].time_1 <>0) then
      zeitdifferenz := round( abs(Planes[nr].time_0 - Planes[nr].time_1)*24*60*60); // Sekunden zwischen den frames

    if (Planes[nr].cpr_type_0 = Planes[nr].cpr_type_1) and
       (Planes[nr].lat_0<>0) and (Planes[nr].lat_1<>0) and (Planes[nr].long_0<>0) and (Planes[nr].long_1<>0) and
       (zeitdifferenz <= maxtimegap) then begin
      // nach http://www.lll.lu/~edward/edward/adsb/Decoding%20ADSB%20position.html
      // das ist ein c&p von "WP B5-07 TIS-B SARPs CPR Attach 2.rtf"
      // siehe auch "1090-WP30-21-Appendix_A Mods.pdf"  A.1.7.7 Globally Unambiguous Airborne Position Decoding

      // erst mal Latitude ,  Compute the latitude index J
      // the zone index is related to the offset between the southern edges of the even and odd zones
      J := floor(((59 * Planes[nr].lat_0 - 60 * Planes[nr].lat_1) / TwoPowerNb) + 0.5);
      // now Compute the latitude values of Rlat(0) and Rlat(1):
      rlat_0 := correct_lat(AirDlat0 * ((J mod 60) + Planes[nr].lat_0 / TwoPowerNb));
      rlat_1 := correct_lat(AirDlat1 * ((J mod 59) + Planes[nr].lat_1 / TwoPowerNb));
      // both results should be +- 90
      if (abs(rlat_0)<=90) and (abs(rlat_1)<=90) then begin
        // both should be in the same Longitude-zones
        NL0 := NL(rlat_0);
        NL1 := NL(rlat_1);
        if NL0=NL1 then begin    // even und odd frame passen zusammen
          // ok, we have a valid latitude; we use the value from the last frame
          if I=0 then latitude := rlat_0 else latitude := rlat_1;

          // und nun Längengrad
          if (I=0)  then ni := NL0 else ni:=NL1-1;           // Anzahl der Zohnen pro Erdumfang
          if (ni<1) then ni := 1;
          case cpr_type of
            cpr_airborne: dlon := 360 / ni;                             // Breite einer Zohne in Grad
            cpr_surface:  dlon :=  90 / ni;
          end;
          // nummer der Zohne bestimmen
          if (I=1) then nlt :=  NL1 else nlt := NL0;
          M := floor( ( Planes[nr].long_0*(nlt-1) - Planes[nr].long_1*nlt ) / TwoPowerNb + 0.5);
          // und nun berechnen
          if (I=1) then Lont := Planes[nr].long_1 else Lont := Planes[nr].long_0;
          Lon := dlon * (pmod(M, ni) +  Lont/TwoPowerNb);
          koord_ok := 1;

          if cpr_type=cpr_surface then begin
            // surface-position decoding has 8 possible results
            // first test for existing airport
            case PortMap.Test(latitude, Lon) of
              $FE: begin Lon := Lon+0;   end;
              $FD: begin Lon := Lon+90;  end;
              $FB: begin Lon := Lon+180; end;
              $F7: begin Lon := Lon+270; end;
              $EF: begin latitude := latitude-90; end;
              $DF: begin latitude := latitude-90; Lon := Lon+90;  end;
              $BF: begin latitude := latitude-90; Lon := Lon+180; end;
              $7F: begin latitude := latitude-90; Lon := Lon+270; end;
            else begin
                // if no or multiple airport, then
                // select the location closest to the Receiver center
                // latitude and lon:  0..90
                Lon := cpr2ref(Receiver.laenge, Lon);
                if (Receiver.breite/rad)<(latitude-45) then latitude := latitude-90;
              end;
            end; //case
            koord_ok := 2;
          end;

          if Lon>=180 then Lon := Lon - 360;
          
          //inc(DSU_Count);
        end;  // NL0=NL1
      end;  // <=90
    end;   // < 10 sekunden , global


    //single-frame-update allways enabled
    //single-frame-detection has to be enabled manually
    if (planes[nr].time_koord<>0) or
       ((cpr_type=cpr_airborne) and allairtargetswithin180NM1.checked) or
       ((cpr_type=cpr_surface)  and allsurfacetargetswithin45NM1.checked) then
    if (koord_ok = 0) then begin //and (cpr_type=cpr_airborne) then begin
      if planes[nr].time_koord <>0 then begin
        if  (Planes[nr].guess.time<>0) then begin            // position geschaetzt
          RefBreit := Planes[nr].guess.breite/rad;
          RefLang  := Planes[nr].guess.laenge/rad;
        end else begin
          RefBreit := Planes[nr].latitude;                  // letzte bekannte position
          RefLang  := Planes[nr].longitude;
        end;
      end else begin                                        // Empfängerposition
        RefBreit := Receiver.breite/rad;
        RefLang  := Receiver.laenge/rad;
      end;
      SFUKoord := SFU(ME_cprLat, ME_cprLong, I,  RefBreit,  RefLang, cprWorld);
      SfuDist  := 60 * sqrt( sqr(SFUKoord.breite-RefBreit) + sqr((SFUKoord.laenge-RefLang)*cos(RefBreit*rad))); //NM
      // SfuDist  := 60 * ( abs(SFUKoord.breite-RefBreit) + abs((SFUKoord.laenge-RefLang)*cos(RefBreit*rad))); //NM  fast
      if SfuDist<maxSfsuDist then begin
        latitude := SFUKoord.breite;
        Lon      := SFUKoord.laenge;
      end;

      if (Latitude<>0) or (Lon<>0) then begin
//        inc(SSU_Count);
        koord_ok := 3;
      end;
    end; // local


    if (koord_ok<>0) then begin    // Koordinaten sind ermittelt
      if  (Planes[nr].guess.time<>0) then begin             // position geschaetzt
        RefBreit := Planes[nr].guess.breite/rad;
        RefLang  := Planes[nr].guess.laenge/rad;
      end else begin
        RefBreit := Planes[nr].latitude;                    // letzte bekannte position
        RefLang  := Planes[nr].longitude;
      end;
      //zu große Fehler erkennen, da ist noch was faul
      if ((abs(RefBreit-latitude) + abs(RefLang-Lon))<1) or            // stimmt zum track
         ( Planes[nr].time_koord=0) or                                 // erste koordinaten
         ((planes[nr].CPR_quality<0) and (koord_ok=1)) then begin      // neubestimmung wegen fehler
        Planes[nr].latitude_old  := Planes[nr].latitude;
        Planes[nr].latitude      := latitude;
        Planes[nr].longitude_old := Planes[nr].longitude;
        Planes[nr].longitude     := Lon;
        planes[nr].time_koord    := now;
        if (planes[nr].CPR_quality < 5) and (koord_ok=1) then inc(planes[nr].CPR_quality);
        planes[nr].CPR_J         := J;        // latitude zone index
        planes[nr].CPR_NL        := NL0;      // Longitude-zone
        // Eintragen in den Track
        if Planes[nr].trackindex<maxTrack then inc(Planes[nr].trackindex) else CompressTrack(nr);
        Planes[nr].Track[Planes[nr].trackindex].laenge := Lon*rad;
        Planes[nr].Track[Planes[nr].trackindex].breite := latitude*rad;
        sincos(Planes[nr].Track[Planes[nr].trackindex]);
        Planes[nr].Track[Planes[nr].trackindex].time   := AFrame.T;
        Planes[nr].Track[Planes[nr].trackindex].hoehe  := planes[nr].altitude;
        // voraussetzung fuer movepunkt
        sin_ZentB := sin(ZentB);
        cos_ZentB := cos(ZentB);
        movepunkt(Planes[nr].Track[Planes[nr].trackindex]); // und koordinaten wandeln
        result := 'B:'+floattostrF(planes[nr].latitude, ffFixed, 7, 4);
        result := result + '  L:'+floattostrF(Planes[nr].longitude, ffFixed, 7, 4);
        //horizontaler Positionsfehler
        if Rc<>0 then result := result + ' Rc<'+inttostr(round(Rc))+'m';
      end else begin
        if (planes[nr].CPR_quality > -5) then dec(planes[nr].CPR_quality);   //abweichende koordinaten
//        case koord_ok of
//          1: inc(Fail1_Count);
//          2: inc(Fail2_Count);
//          3: inc(Fail3_Count);
//        end; // case
      end;
    end else
//    inc(Zero_Count);

  end;   // Get_Koord

  // CRC 112 bit
  function parity112bit:dword;
  const poly             : cardinal = $FFFA0480;
  var i,data,data1,data2 : cardinal;
  begin
    data :=(AFrame.B[0] shl 24) or (AFrame.B[1] shl 16) or (AFrame.B[2]  shl 8) or AFrame.B[3];
    data1:=(AFrame.B[4] shl 24) or (AFrame.B[5] shl 16) or (AFrame.B[6]  shl 8) or AFrame.B[7];
    data2:=(AFrame.B[8] shl 24) or (AFrame.B[9] shl 16) or (AFrame.B[10] shl 8);
    for i:=1 to 88 do
    begin
      if (data and $80000000) <> 0 then data:=data xor poly;
      data:=data shl 1;
      if (data1 and $80000000) <>0 then data:=data or 1;
      data1:=data1 shl 1;
      if (data2 and $80000000) <>0 then data1:=data1 or 1;
      data2:=data2 shl 1;
    end;
    result:=data shr 8;
  end;  // parity112bit

  // CRC 56 bit
  function parity56bit:dword;
  const poly  : cardinal = $FFFA0480;
  var i,data  : cardinal;
  begin
    data :=(AFrame.B[0] shl 24) or (AFrame.B[1] shl 16) or (AFrame.B[2] shl 8) or AFrame.B[3];
    for i:=1 to 32 do
    begin
      if (data and $80000000) <> 0 then data:=data xor poly;
      data:=data shl 1;
    end;
    result:=data shr 8;
  end;  // parity56bit

  //CRC
  function getCRC:dword;
  begin
    if DF<16 then result:=parity56bit
             else result:=parity112bit;
  end; // getCRC


  //ist das abfragende Radar bekannt?
  // ID 0 ..79
  function interogatorIDknown(interogatorID:dword) : boolean;
  begin
    result:=false;
    if interogatorID >  maxInterogatoren then  begin
      elog('interogatorID : '+inttostr(interogatorID));
      exit;
    end else begin   // soll nur 8 bit lang sein ??  mal sehen
      if  interogatoren[interogatorID]<1000000000 then inc(interogatoren[interogatorID]);  //ueberlauf vermeiden
      if  interogatoren[interogatorID]>3          then begin
        result := true;
        if iffForm.visible then iffForm.iffrx(interogatorID, nr, planes[nr].color);
      end;
    end;  
  end;  // interogatorIDknown


  // chr() fuer ICAO-Identifier
  function chricao(wert:integer):char;
  begin
    case wert of
      1..26   : result := chr(wert-1+ord('A'));  // letters
      $20     : result := ' ';                   // space
      $30..$39: result := chr(wert);             // numbers
    else begin
        result := '?';
        chricao_fail := true;
      end;
    end;
  end; // chricao


  // AP ist xor von CRC und AA
  function AP_decode(DF:integer):integer;
  var AP_wert    : dword;
      aircraftID : dword;
  begin
    CRC := getCRC;
    if DF<16 then AP_wert := (AFrame.B[4]  shl 16) or (AFrame.B[5]  shl 8) or AFrame.B[6]
             else AP_wert := (AFrame.B[11] shl 16) or (AFrame.B[12] shl 8) or AFrame.B[13];
    aircraftID := CRC xor AP_wert;
    nr := findPlane(aircraftID);
    result := nr;
    if result>maxPlane then begin
      // dieses Flugzeug wird gerade nicht getrackt, entweder ein neues (unwarscheinlich) oder ein CRC-Fehler (warscheinlich)
      result:=-1;
      inc(CRCfehler);
      writeLog('CRC-fail or unknown    ');
    end else begin
      AA    := aircraftID;
      AAstr := inttohex(aircraftID,6);
      writeLog(AAstr+'  '+AA2Land(aircraftID)+'  ');
    end;
    memoline := memoline + AAstr+' '+typstr+' '+AA2Land(AA)+'  ';
  end; //AP_decode


  // 3.1.2.6.6.1 MB: Message, Comm-B. This 56-bit (33-88) downlink field shall be used
  // to transmit data link messages to the ground.
  {
  siehe  ICAO Document „Manual on Mode S Specific Services” (Doc 9688)
  Register	Inhalt
  BDS 01h	Data Link Capability Report
  BDS 02h	Aircraft Identity
  BDS 03h	ACAS Resolution Advisory
  BDS 04h	Selected Vertical Intent parameters (Bit 28…40: Barometric Pressure Setting)
  BDS 05h	Extended Squitter Airborne Position
  BDS 06h	Extended Squitter Surface Position
  BDS 07h	Extended Squitter Status (transmitted only in reply to interrogation)
  BDS 08h	Extended Squitter A/C Id & Category
  BDS 09h	Extended Squitter Airborne Velocity
  BDS 0Ah	Extended Squitter Event Report
  BDS 61h	Extended Squitter Emergency/Priority Status (transmitted once per second during an emergency)
  BDS 65h	Aircraft Operational Status
  ? bei der GICB-Register nummer sind die beiden Ziffern getauscht ? stimmt nur selten
  }
  function MB_decode:string;
  var k            : integer;
      MB_ident     : string;
      //MB_str       : string;
      //MB_str2      : string;
      //MB_str3      : string;
      TYPE_CODE    : byte;     //FORMAT_TYPE_CODE
      subtype_CODE : byte;
  begin
    result := 'MB:';
    for k :=4 to 10 do result:= result + inttohex(AFrame.B[k],2);
    result := result + ' ';

    // Der nachfolgende code ist ein Versuch die MB-Daten eindeutig zuzuordnen.
    // Das scheint aber noch ein unlösbares Problem zu sein.
    // selbst das Callsign hat sich in der Praxis als nicht eindeutig erwiesen.
    // deshalb ist dieser Bereich z.Z. deaktiviert
//    memoline := memoline+result;
//    writelog(result);
//    exit;

    if planes[nr].ident = '' then begin
    
    TYPE_CODE    := AFrame.B[4] shr 3;
    subtype_CODE := AFrame.B[4] and $07;
    //result:= result + '  [';
    //result:= result + inttostr(TYPE_CODE) + ':' + inttostr(subtype_CODE) + ']  ';

    // ModeS-WP03-09-DO-181D-Appendix-B+C_rework v3_.pdf
    //TYPE_CODE = 28  BDS Code 6,1 – Aircraft Status
    //    Subtype 1: Emergency/Priority Status
    //    Subtype 2: Extended Squitter TCAS RA Broadcast
    //TYPE_CODE = 29  BDS Code 6,2 – Target State and Status Information
    //TYPE_CODE = 31  BDS Code 6,5 – Extended Squitter Aircraft Operational Status
    //    Subtype 0 = Airborne Status Message
    //    Subtype 1 = Surface Status Message

    //0x10 beginnt BDS Code 1,0 – Data Link Capability Report
    //0x20 beginnt kommt die ID-message (BDS 2,0)
    //0x30 beginnt kommt TCAS/ACAS Active Resolution Advisory (BDS 3,0)
    //0xE0 .. 0xE7    BDS Code 6,1 – Aircraft Status
    //0xE8 .. 0xEF    BDS Code 6,2 – Target State and Status Information
    case AFrame.B[4] of
      //$10: result:= result + 'BDS 1,0  '; // Data Link Capability Report
      $20: begin  // aircraft identifier
             chricao_fail := false;
             MB_ident :=
             chricao(  (AFrame.B[5] and $FC) shr 2                          ) +
             chricao( ((AFrame.B[5] and $03) shl 4) or (AFrame.B[6] shr 4)  ) +
             chricao( ((AFrame.B[6] and $0F) shl 2) or (AFrame.B[7] shr 6)  ) +
             chricao(  (AFrame.B[7] and $3F)                                ) +
             chricao(  (AFrame.B[8] and $FC) shr 2                          ) +
             chricao( ((AFrame.B[8] and $03) shl 4) or (AFrame.B[9] shr 4)  ) +
             chricao( ((AFrame.B[9] and $0F) shl 2) or (AFrame.B[10] shr 6) ) +
             chricao(  (AFrame.B[10] and $3F)                               );
             if not chricao_fail then begin
               result:= result + MB_ident + '  ';
               // das folgende if/else sieht dumm aus, aber muss so sein
               if planes[nr].ident<>MB_ident then begin
                 planes[nr].ident := MB_ident;
                 ServerSendId(nr);
               end else planes[nr].ident := MB_ident;
               ident_st := 'Id:'+MB_ident+'  ';
               ServerMsg := 1;
             end;
           end;
      //$30: result:= result + 'BDS 3,0  ';     //TCAS/ACAS Active Resolution Advisory
      
{     // Versuch der weitergehenden Bestimmung des MB-Feldes, vorübergehend abgeschaltet, zur Zeitersparnis
      else
      begin
        if (TYPE_CODE=28) then
              if (subtype_CODE=1) or (subtype_CODE=2) then result:= result + 'BDS 6,1  ';  //Aircraft Status

        if (TYPE_CODE=29) then
              if (subtype_CODE=2) then result:= result + 'BDS 6,2  ';  //Target State and Status Information

        if (TYPE_CODE=31) then
              if (subtype_CODE<2) then result:= result + 'BDS 6,5  ';  //Extended Squitter Aircraft Operational Status

        // test auf BDS 2,1      Aircraft and Airline Registration Markings
        chricao_fail := false;
        MB_ident :=
             chricao(  (AFrame.B[4] and $7E) shr 1                          ) +
             chricao( ((AFrame.B[4] and $01) shl 5) or (AFrame.B[5] shr 3)  ) +
             chricao( ((AFrame.B[5] and $07) shl 3) or (AFrame.B[6] shr 5)  ) +
             chricao( ((AFrame.B[6] and $1F) shl 1) or (AFrame.B[7] shr 7)  ) +
             chricao(  (AFrame.B[7] and $7E) shr 1                          ) +
             chricao( ((AFrame.B[7] and $01) shl 5) or (AFrame.B[8] shr 3)  ) +
             chricao( ((AFrame.B[8] and $07) shl 3) or (AFrame.B[9] shr 5)  );
        MB_str:=
             chricao( ((AFrame.B[9] and $0F) shl 2) or (AFrame.B[10] shr 6) ) +
             chricao(  (AFrame.B[10]and $3F)                               );
        if not chricao_fail then
          result:= result + 'BDS 2,1 ' + MB_ident + '  ' + MB_str + '  ';

        // test auf BDS 4,1    Next Waypoint Details
        chricao_fail := false;
        MB_str :=
             chricao(  (AFrame.B[4] and $7E) shr 1                          ) +
             chricao( ((AFrame.B[4] and $01) shl 5) or (AFrame.B[5] shr 3)  ) +
             chricao( ((AFrame.B[5] and $07) shl 3) or (AFrame.B[6] shr 5)  ) +
             chricao( ((AFrame.B[6] and $1F) shl 1) or (AFrame.B[7] shr 7)  ) +
             chricao(  (AFrame.B[7] and $7E) shr 1                          ) +
             chricao( ((AFrame.B[7] and $01) shl 5) or (AFrame.B[8] shr 3)  ) +
             chricao( ((AFrame.B[8] and $07) shl 3) or (AFrame.B[9] shr 5)  ) +
             chricao( ((AFrame.B[9] and $1F) shl 1) or (AFrame.B[10]shr 7)  ) +
             chricao(  (AFrame.B[10]and $7E) shr 1)    ;
        if not chricao_fail then
          result:= result + 'BDS 4,1 ' + MB_str + '  ';

        // test auf BDS 2,5
        chricao_fail := false;
        MB_str :=     // model designation
             chricao( ((AFrame.B[5] and $01) shl 5) or (AFrame.B[6] shr 3)  ) +
             chricao( ((AFrame.B[6] and $07) shl 3) or (AFrame.B[7] shr 5)  ) +
             chricao( ((AFrame.B[7] and $1F) shl 1) or (AFrame.B[8] shr 7)  ) +
             chricao(  (AFrame.B[8] and $7E) shr 1                          ) +
             chricao( ((AFrame.B[8] and $01) shl 5) or (AFrame.B[9] shr 3)  ) ;
        MB_str2 :=    // aircraft type (L/H/A) + nr engines (0..7) + engine type (P/T/J)
             chricao(  (AFrame.B[4] and $FC) shr 2) +
             inttostr(((AFrame.B[4] and $03) shl 1) or (AFrame.B[5] and $80) shr 7) +
             chricao(  (AFrame.B[5] and $7E) shr 1);
        MB_str3 :=   // wake turbulence category   M/L/H
             chricao( ((AFrame.B[9] and $07) shl 3) or (AFrame.B[10] and $E0) shr 5) ;
        if not chricao_fail and  (MB_str2[1] in ['L','H','A']) and
           (MB_str2[3] in ['P','T','J']) and  (MB_str3[1] in ['M','L','H']) then
           result:= result + 'BDS 2,5 ' + MB_str + '  ';
      end;

}

    end;  //case

    end; //if

    memoline := memoline+result;
    writeLog(result);
  end; //MB_decode


  // 3.1.2.8.3.1 MV: Message, ACAS. This 56-bit (33-88) downlink field shall contain GICB information as requested
  // in the DS field of the UF 0 interrogation that elicited the reply.
  // Note.— The MV field is also used by ACAS for air-air coordination (4.3.8.4.2.4).
  function MV_decode:string;
  var k    : integer;
      VDS1 : word;
      VDS2 : word;
      ARA  : word;
      RAC  : word;
      RAT  : word;
      MTE  : word;
  begin
    result := 'MV:';
    for k :=4 to 10 do result:= result + inttohex(AFrame.B[k],2);
    result:= result + '  ';

    // 4.3.8.4.2.4  MV field. This 56-bit (33-88) field shall be used to transmit air-air coordination reply messages.
    // VDS (V-definition subfield). This 8-bit (33-40) subfield shall define the remainder of MV.
    // Note.— For convenience in coding, VDS is expressed in two groups of four bits each, VDS1 and VDS2.
    // Subfields in MV for a coordination reply. When VDS1 = 3 and VDS2 = 0, the following subfields shall be contained in MV:
    // ARA (active RAs). This 14-bit (41-54) subfield   4.3.8.4.2.2.1.1.
    // RAC (RACs record). This 4-bit (55-58) subfield   4.3.8.4.2.2.1.2.
    // RAT (RA terminated indicator). This 1-bit (59) subfield   4.3.8.4.2.2.1.3.
    // MTE (multiple threat encounter). This 1-bit (60) subfield 4.3.8.4.2.2.1.4.

    VDS1 := (AFrame.B[4] and $F0) shr 4;
    VDS2 :=  AFrame.B[4] and $0F;
    if (VDS1=3) and (VDS2=0) then begin
      //MV for a coordination reply
      ARA  :=  (AFrame.B[5] shl 6) or ((AFrame.B[6] and $FC) shr 2);
      RAC  := ((AFrame.B[6] and $03) shl 2) or ((AFrame.B[7] and $C0) shr 6);
      RAT  :=  (AFrame.B[7] and $20) shr 5;
      MTE  :=  (AFrame.B[7] and $10) shr 4;
      // daraus text zu basteln ergibt einen zu langen string, besser die Zahlen lassen
      result:='MV:VDS1=3/VDS2=0/ARA='+inttohex(ARA,2)+'/RAC='+inttostr(RAC)+'/RAT='+inttostr(RAT)+'/MTE='+inttostr(MTE)+'  ';
    end;

    memoline := memoline+result;
    writelog(result);
  end; //MV_decode


  // CA capability                       3.1.2.5.2.2.1
  // CA: Capability. This 3-bit (6-8) downlink field shall convey information on the transponder level,
  // the additional information below, and shall be used in formats DF = 11 and DF = 17.
  function CA_decode:string;
  var CA : word;
  begin
    result := '';
    CA  := (AFrame.B[0] and $07);
    //if ca=5 then exit;
    // fast alle haben CA:L1-air
    //nur falls da wirklich was bedeutendes steht:
    case CA of
      0: result := 'CA:L1  '; // Level 1 transponder (surveillance only), and no ability to set CA code 7 and either airborne or on the ground
      1: result := ''; //reserved
      2: result := ''; //reserved
      3: result := ''; //reserved
      4: result := 'CA:L2-ground  '; // Level 2 or above transponder and ability to set CA code 7 and on the ground
      5: result := 'CA:L2-air  ';    // Level 2 or above transponder and ability to set CA code 7 and airborne
      6: result := 'CA:L2  ';        // Level 2 or above transponder and ability to set CA code 7 and either airborne or on the ground
      7: result := 'CA:7  '; //  the DR field is not equal to 0 or the FS field equals 2, 3, 4 or 5, and either airborne or on the ground
    end;
    memoline := memoline+result;
    writelog(result);
  end; //CA_decode


  // UM utility message                  3.1.2.6.5.3     6 bit = IIS(4bit) + IDS(2bit)
  function UM_decode:string;
  var UM, IIS, IDS : word;
  begin
    result := '';
    UM  := ((AFrame.B[1] and $07) shl 3) or ((AFrame.B[2] and $E0) shr 5);
    if UM=0 then exit;
    //nur falls da wirklich was bedeutendes steht:
    IIS := (UM and $3C) shr 2;
    IDS :=  UM and $03;
    case IDS of
      0: result := ''; //signifies no information
      1: result := '/Comm-B'; //signifies IIS contains Comm-B II code
      2: result := '/Comm-C'; //signifies IIS contains Comm-C II code
      3: result := '/Comm-D'; //signifies IIS contains Comm-D II code.
    end;
    result:='UM:Int='+inttostr(IIS)+result+'  ';
    memoline := memoline+result;
    writelog(result);
  end; //UM_decode


  // DR downlink request                 3.1.2.6.5.2     5 bit
  function DR_decode:string;
  var DR : word;
  begin
    result := '';
    DR := (AFrame.B[1] and $F8) shr 3;
    if DR=0 then exit;
    //nur falls da wirklich was bedeutendes steht:
    case DR of
      0: result := '';                      // signifies no downlink request
      1: result := 'DR:CommB  ';            //  signifies request to send Comm-B message
      2: result := 'DR:2  ';                //  reserved for ACAS
      3: result := 'DR:3  ';                //  reserved for ACAS
      4: result := 'DR:CommB-bc1  ';        //  signifies Comm-B broadcast message 1 available
      5: result := 'DR:CommB-bc2  ';        //  signifies Comm-B broadcast message 2 available
      6: result := 'DR:6  ';                //  reserved for ACAS
      7: result := 'DR:7  ';                //  reserved for ACAS
      8..15:  result := '';                 //  not assigned
      16..31: result := '';                 //  see downlink ELM protocol (3.1.2.7.7.1)
      //Codes 1-15 shall take precedence over codes 16-31
    end;
    memoline := memoline+result;
    writelog(result);
  end; //DR_decode


  //3.1.2.6.5.1 FS: Flight status. This 3-bit (6-8) downlink field
  function FS_decode:string;
  var FS : word;
  begin
    result := '';
    FS := AFrame.B[0] and $07;
    if FS=0 then exit;
    //nur weitermachen wenn da was wichtiges steht
    case FS of
      0: begin
         result := 'FS:airborne  ';        // signifies no alert and no SPI, aircraft is airborne
         planes[nr].airborne := true;
         planes[nr].alert    := false;
         planes[nr].spi      := false;
         end;
      1: begin
         result := 'FS:ground  ';          // signifies no alert and no SPI, aircraft is on the ground
         planes[nr].airborne := false;
         planes[nr].alert    := false;
         planes[nr].spi      := false;
         end;
      2: begin
         result := 'FS:alert/airborne  ';  // signifies alert, no SPI, aircraft is airborne
         planes[nr].airborne := true;
         planes[nr].alert    := true;
         planes[nr].spi      := false;
         end;
      3: begin
         result := 'FS:alert/ground  ';    // signifies alert, no SPI, aircraft is on the ground
         planes[nr].airborne := false;
         planes[nr].alert    := true;
         planes[nr].spi      := false;
         end;
      4: begin
         result := 'FS:alert/SPI  ';       // signifies alert and SPI, aircraft is airborne or on the ground
         planes[nr].alert    := true;
         planes[nr].spi      := true;
         end;
      5: begin
         result := 'FS:SPI  ';             // signifies no alert and SPI, aircraft is airborne or on the ground
         planes[nr].alert    := false;
         planes[nr].spi      := true;
         end;
      6: result := ''; // reserved
      7: result := ''; // not assigned
    end;
    memoline := memoline+result;
    writelog(result);
  end; //FS_decode


  //3.1.2.8.2.2 RI: Reply information, air-air. This 4-bit (14-17) downlink field shall report the
  //aircraft’s maximum cruising true airspeed capability and type of reply to interrogating aircraft.
  function RI_decode:string;
  var RI : word;
  begin
    result := '';
    RI := ((AFrame.B[1] and $07) shl 1) or ((AFrame.B[2] and $80) shr 7);
    case RI of
       0: result :='no-ACAS  ';
       8: result :='no-Vmax  ';            // no maximum airspeed data available
       9: result :='Vmax<75kts  ';         // maximum airspeed is .LE. 140 km/h (75 kt)
      10: result :='Vmax:75..150kts  ';    // maximum airspeed is .GT. 140 and .LE. 280 km/h (75 and 150 kt)
      11: result :='Vmax:150..300kts  ';   // maximum airspeed is .GT. 280 and .LE. 560 km/h (150 and 300 kt)
      12: result :='Vmax:300..600kts  ';   // maximum airspeed is .GT. 560 and .LE. 1 110 km/h (300 and 600 kt)
      13: result :='Vmax:600..1200kts  ';  // maximum airspeed is .GT. 1 110 and .LE. 2 220 km/h (600 and 1 200 kt)
      14: result :='Vmax>1200kts  ';       // maximum airspeed is more than 2 220 km/h (1 200 kt)
    end;
    case RI of
      9..11:  planes[nr].symbol := sym_small;
      13..14: planes[nr].symbol := sym_fast;
    end;
    memoline := memoline+result;
    writelog(result);
  end; //RI_decode



  // VS vertical status                  3.1.2.8.2.1     1 bit 0=airborne
  function VS_decode:boolean;
  begin
    planes[nr].airborne := (AFrame.B[0] and $04) = 0;
    if not planes[nr].airborne then begin
      writelog('on-ground  ');
      memoline := memoline+'on-ground  ';
    end;
    result := planes[nr].airborne;
  end; //VS_decode


  // CC  Cross-link capability            3.1.2.8.2.3      1 bit 1=supports the cross-link capability
  function CC_decode:boolean;
  begin
    planes[nr].crosslink := (AFrame.B[0] and $02) <> 0;
    if planes[nr].crosslink then begin
      writelog('crosslink  ');
      memoline := memoline+'crosslink  ';
    end;
    result := planes[nr].crosslink;
  end; //CC_decode


  // AC altitude code          3.1.2.6.5.4
  // altes Höhenverfahren
  // liegert im Fehlerfall NoAltitude zurück
  function AC_decode:integer;
  begin
    AC := MC_Code(AFrame.B[2],AFrame.B[3]);
    if AC<>NoAltitude then begin
      if (nr>=0) and (nr<=lastPlane) then planes[nr].altitude := AC;
      AC_st:= 'AC:'+inttostr(AC)+'ft  '+hoehenart_st;
    end else AC_st:= 'AC: invalid ft  '+hoehenart_st;
    writelog(AC_st);
    memoline := memoline+AC_st;
    result := AC;
  end; //AC_decode


  // ID wie in mode A          3.1.2.6.7.1
  //Starting with bit 20, the sequence shall be C1, A1, C2, A2, C4, A4, ZERO, B1, D1, B2, D2, B4, D4
  function ID_decode:word;
  begin
{
    ID := M3ACode(AFrame.B[2],AFrame.B[3]);                 // DM61
    ID_st := ' ID:' + inttostr(ID)+ '  ';                   // DM61
}
    ID := sort_olde_code_bits_ID((AFrame.B[2] shl 8) or AFrame.B[3]);
    ID_st:= 'ID:' + inttostr((ID and $E00) shr 9) + inttostr((ID and $1C0) shr 6)
                  + inttostr((ID and $038) shr 3) + inttostr((ID and $007)) + '  ';
    if (nr>=0) and (nr<=lastPlane) then begin
      planes[nr].mod3id := ID;
      planes[nr].squawk := inttookt(ID,4);
    end;
    writelog(ID_st);
    memoline := memoline+ID_st;
    result := ID;
  end; //ID_decode


  // AA  **********************************************************************
  // Adresse im Klartext enthalten                        3.1.2.5.2.2    /   2.2.3.2.1.5
  // liefert nr des flugzeuges zurueck
  // und schreibt sie auch in nr
  function AA_decode:integer;
  begin
    // Adresse im Klartext enthalten
    AA := (AFrame.B[1] shl 16) or (AFrame.B[2] shl 8) or AFrame.B[3];
    nr := findPlane(AA);
{ // nach unten verlegt, wegen DF11-Fehler des Beast
    if nr>maxPlane then nr:=newplane(AA);
    result := nr;
    if nr>maxPlane then begin
      result := -1;
      memo2_lines_add('###### to many planes ');
      writelnlog('###### to many planes ');
      exit;
    end;
}
    // Vorsicht!
    // bei DF11 ist das Ende der CRC mit der Interrogator-ID ge-xor-d
    // die Interrogator-ID ist 0..63 (6 bit)
    CRC := getCRC;
    if DF<16 then CRC1 := (AFrame.B[4]  shl 16) or (AFrame.B[5]  shl 8) or AFrame.B[6]
             else CRC1 := (AFrame.B[11] shl 16) or (AFrame.B[12] shl 8) or AFrame.B[13];
    if DF=11 then begin
      interogatorID := CRC xor CRC1;
      if interogatorIDknown(interogatorID) then begin
        if (nr>=0) and (nr<=lastPlane) and GroundRADAR1.checked then planes[nr].interogator := interogatorID;
        // interrogator-id entfernen
        CRC  := CRC  or $FF;
        CRC1 := CRC1 or $FF;
      end else interogatorID := 0;
    end;
    if CRC <> CRC1 then begin
      writelog('?'+AAstr+'  '+AA2Land(AA));
      writelog('  ### CRC fail:'+inttohex(CRC,6)+' <> '+inttohex(CRC1,6));
      inc(CRCfehler);
      result:=-1;
      exit;
    end;

    // CRC ist ok
    if nr>maxPlane then nr:=newplane(AA);
    result := nr;
    if nr>maxPlane then begin
      result := -1;
      memo2_lines_add('###### to many planes ');
      writelnlog('###### to many planes ');
      exit;
    end;       

    AAstr := inttohex(AA,6);
    writelog(AAstr+'  '+AA2Land(AA)+'  ');
    if (interogatorID<>0) then writelog('Inter:'+ifftostr(interogatorID)+'  ');
    memoline := memoline+AAstr+' '+typstr+' '+AA2Land(AA)+'  ';
    if (interogatorID<>0) then memoline := memoline +'Inter:'+ifftostr(interogatorID)+'  ';
  end; // AA_decode



  //** ME ********************************************************************
  // ADS-B - Message-field decodieren                3.1.2.8.6.2
  // true: ads-B     false: trisb
  //0: ADS-B
  //3: fine TIS-B
  //4: coarse TIS-B
  procedure ME_decode(CPR_res : integer);
  begin
    if (CPR_res<>cpr_adsb) and (CPR_res<>cpr_fine) then exit;

    if (nr>=0) and (nr<=lastPlane) then begin
      ME_type    := AFrame.B[4] shr 3;
      ME_subtype := AFrame.B[4] and $07;
      
      ME_heading_status := 0;

      typstr := '??'+inttostr(ME_type)+'-'+inttostr(ME_subtype);
      case ME_type of
        0:     typstr := 'Position        ';
        1..4:  typstr := 'Identification  ';
        5..8:  typstr := 'Surf. Position  ';
        9..18: typstr := 'Airb. Position  ';
        19:    case ME_subtype of
               1..4: typstr := 'Airb. Velocity  ';
               end;
        20..22:typstr := 'Airb. Position  ';
        23:    case ME_subtype of
                0: typstr := 'Test            ';
               end;
        24:    ;
        25..26:;
        27:    ;
        28:    case ME_subtype of
                1: typstr := 'ES Airc. Status ';
               end;
        29:    ;
        30:    ;
        31:    case ME_subtype of
               0..1: typstr := 'Airc. Status    ';
               end;
      end;

      case ME_type of
        1..4:begin ; // identification and category message                           2.2.3.2.5 S. 82
          // normally the airline flight number
          // 8 characters, of 6 bit ascii
          ME_ident :=
          chricao(  (AFrame.B[5] and $FC) shr 2                          ) +
          chricao( ((AFrame.B[5] and $03) shl 4) or (AFrame.B[6] shr 4)  ) +
          chricao( ((AFrame.B[6] and $0F) shl 2) or (AFrame.B[7] shr 6)  ) +
          chricao(  (AFrame.B[7] and $3F)                                ) +
          chricao(  (AFrame.B[8] and $FC) shr 2                          ) +
          chricao( ((AFrame.B[8] and $03) shl 4) or (AFrame.B[9] shr 4)  ) +
          chricao( ((AFrame.B[9] and $0F) shl 2) or (AFrame.B[10] shr 6) ) +
          chricao(  (AFrame.B[10] and $3F)                               );
          up_ident   := true;
        end;  // identification and category message

        0:begin // no-position message
          ME_status   := (AFrame.B[4] shr 1) or $03;
          ME_altitude := (AFrame.B[5] shl 4) or ((AFrame.B[6] and $F0) shr 4);   //baro or no altitude
          //horizontale Genauigkeit in Metern
          Rc := 0; // unknown

          up_alt      := true;
        end; // no-position message

        5..8:begin ; //surface position message
          ME_movement :=((AFrame.B[4] and $07) shl 4) or ((AFrame.B[5] and $F0) shr 4);    //2.2.3.2.4.2   S.71
          ME_hdStatus := (AFrame.B[5] and $08) shr 3;                                      //2.2.3.2.4.3   S.71
          ME_hdTrack  :=((AFrame.B[5] and $07) shl 4) or ((AFrame.B[6] and $F0) shr 4);    //2.2.3.2.4.4   S.72    ME_hdTrack * 2.8125 grad

          if CPR_res=cpr_adsb then  ME_time := (AFrame.B[6] and $08) shr 3;
          ME_cprForm  := (AFrame.B[6] and $04) shr 2;
          ME_cprLat   :=((AFrame.B[6] and $03) shl 15) or (AFrame.B[7] shl 7) or (AFrame.B[8] shr 1);
          ME_cprLong  :=((AFrame.B[8] and $01) shl 16) or (AFrame.B[9] shl 8) or AFrame.B[10];

          //horizontale Genauigkeit in Metern
          Rc := 0; // unknown
          case ME_type of
               5: Rc := 7.5; //m
               6: Rc := 25; //m
               7:    Rc := 185.2; //m
               8:    Rc := 0; //unknown
          end;

          up_koord_surf := true;
          up_ground     := true;
        end;   //surface position message

        9..18,20..22:begin //airborne position message
          ME_status   := (AFrame.B[4] shr 1) or $03;
          //9..18 baro altitude   20..22 GNSS height (HAE)
          ME_altitude := (AFrame.B[5] shl 4) or ((AFrame.B[6] and $F0) shr 4);
          if CPR_res=cpr_adsb then begin
            planes[nr].nicb := AFrame.B[4] and 1;
            ME_time         := (AFrame.B[6] and $08) shr 3;
          end else planes[nr].imf := AFrame.B[4] and 1;

          ME_cprForm  := (AFrame.B[6] and $04) shr 2;   // odd or even
          ME_cprLat   :=((AFrame.B[6] and $03) shl 15) or (AFrame.B[7] shl 7) or (AFrame.B[8] shr 1);
          ME_cprLong  :=((AFrame.B[8] and $01) shl 16) or (AFrame.B[9] shl 8) or AFrame.B[10];

          //horizontale Genauigkeit in Metern
          Rc := 0; // unknown
          case ME_type of
               9 : Rc := 7.5; //m
               10: Rc := 25; //m
               11:    Rc := 185.2; //m
               12: Rc := 370.4; //m
               13:    Rc := 1111.2; //m
               14: Rc := 1852; //m
               15: Rc := 3704; //m
               16:    Rc := 14816; //m
               17: Rc := 37040; //m
               18: Rc := 0; //unknown
               20: Rc := 7.5; //m
               21: Rc := 25; //m
               22: Rc := 0; //unknown
          end;

          up_alt      := true;
          up_koord_air:= true;
          up_airborne := true;
        end; // airborne position message

        19: // airborne velocity message                                               2.2.3.2.6  S 84 ff
            //subtype 0  -
            //subtype 1  groundspeed unterschall
            //subtype 2  groundspeed ueberschall
            //subtype 3  airspeed/heading unterschall
            //subtype 4  airspeed/heading ueberschall
            //subtype 5,6,7 -
          begin
            case ME_subtype of
               1,2: begin
                 ME_direction_West := (AFrame.B[5] and $04) shr 2;
                 ME_speed_West     :=((AFrame.B[5] and $03) shl 8) or AFrame.B[6];    // 0=off, 1=0, 2=1, 3=1
                 if (ME_subtype=2) and (ME_speed_West>0) then
                   ME_speed_West   := ((ME_speed_West-1)*4)+1;                        // 0=off, 1=0, 2=4, 3=8
                 ME_direction_South:= (AFrame.B[7] and $80) shr 7;
                 ME_speed_South    :=((AFrame.B[7] and $7F) shl 3) or ((AFrame.B[8] and $E0) shr 5);
                 if (ME_subtype=2) and (ME_speed_South>0) then
                   ME_speed_South  := ((ME_speed_South-1)*4)+1;                       // 0=off, 1=0, 2=4, 3=8
                 if (ME_speed_West)+(ME_speed_South)>0 then up_speed_ground := true;
               end;
               3,4: begin
                 ME_heading_status := (AFrame.B[5] and $04) shr 2;
                 ME_heading        :=((AFrame.B[5] and $03) shl 8) or AFrame.B[6];    // 2.2.3.2.6.3.7   s.98
                 ME_airspeed_type  := (AFrame.B[7] and $80) shr 7;
                 ME_airspeed       :=((AFrame.B[7] and $7F) shl 3) or ((AFrame.B[8] and $E0) shr 5);
                 if (ME_subtype=4) and (ME_airspeed>0) then
                   ME_airspeed := ((ME_airspeed-1)*4)+1;                              // 0=off, 1=0, 2=4, 3=8
                 if ME_heading_status=1 then up_speed_air := true;
               end;
           end;
          // variometer
          ME_vert_sign :=  (AFrame.B[8] and $08) shr 3;
          ME_vert_rate := ((AFrame.B[8] and $07) shl 6) or ((AFrame.B[9] and $FC) shr 2);
        end;

        31: if (ME_subtype>=0) and (ME_subtype<=1) then begin    // operational status message
          planes[nr].mops :=  AFrame.B[9] shr 5;                 // ADS-B Version number
          planes[nr].nica := (AFrame.B[9] shr 4) and 1;          // NIC susp-A
          planes[nr].nac  :=  AFrame.B[9] and $0F;               // navigation accuracy
          planes[nr].om   := (AFrame.B[7] shl 8) or AFrame.B[8]; // operational modes
          planes[nr].cc   := (AFrame.B[5] shl 8) or AFrame.B[6]; // capability class
          case ME_subtype of
            0: ;
            1: ;
          end;
        end;

      end; // case  ME_type

      //aircraft identifier
      if up_ident then begin
        // das folgende if/else sieht dumm aus, aber muss so sein
        if planes[nr].ident<>ME_ident then begin
          planes[nr].ident := ME_ident;
          ServerSendId(nr);
        end else planes[nr].ident := ME_ident;
        ident_st := 'Id:'+ME_ident+'  ';
        ServerMsg := 1;
      end else ident_st := '';

      // hoehe updaten
      if up_alt then begin
        altitude_ft := get_Hoehe_ft(ME_altitude);
        if altitude_ft<>NoAltitude then begin
          planes[nr].altitude := altitude_ft;
          alt_st := inttostr(altitude_ft)+'ft  '+hoehenart_st;
        end else alt_st := 'invalid ft  '+hoehenart_st;
      end else alt_st := '';

      //koordinaten updaten
      if up_koord_air or up_koord_surf then begin
        //***CPR****
        if up_koord_air then koord_st := get_Koord(cpr_airborne) else koord_st := get_Koord(cpr_surface);
        if koord_st<>'*' then ServerMsg := 3;
        koord_st := koord_st+'  ';

        if ME_heading_status=1 then begin
          planes[nr].heading := (ME_heading * 0.3515625) * rad;    // 360/1024                   // 0 .. 2 pi
          if planes[nr].heading > pi then  planes[nr].heading := planes[nr].heading - (2*pi);    // +- pi
        end;
      end else koord_st := '';

      speed_st:='';
      //variometer
      if (up_speed_ground or up_speed_air) and (ME_vert_rate<>0) then begin
        planes[nr].steigen := (ME_vert_rate-1)*64;                             //  ft/min
        if ME_vert_sign=1 then planes[nr].steigen := -planes[nr].steigen;      //  sinken
        ServerMsg := 4;
      end;
      // groundspeed updaten
      if up_speed_ground then begin
        if ME_direction_West =1 then ME_speed_West  := -(ME_speed_West-1)  else ME_speed_West  := (ME_speed_West-1);
        if ME_direction_South=1 then ME_speed_South := -(ME_speed_South-1) else ME_speed_South := (ME_speed_South-1);
        planes[nr].speed   := sqrt(sqr(ME_speed_West) + sqr(ME_speed_South));
        planes[nr].heading := arctan2(ME_speed_West,ME_speed_South);   // +- pi;
        speed_st:='V='+inttostr(round(planes[nr].speed))+' HD='+inttostr(round(planes[nr].heading/rad))+' Var='+inttostr(planes[nr].steigen)+'  ';
        ServerMsg := 4;
      end;
      // airspeed updaten
      if up_speed_air then begin
        if ME_airspeed>0 then planes[nr].speed := ME_airspeed-1;      //knoten
        planes[nr].heading := arctan2(ME_speed_West,ME_speed_South);  // +- pi
        if ME_heading_status=1 then begin
          planes[nr].heading := (ME_heading * 0.3515625) * rad;      // 360/1024                 // 0 .. 2 pi
          if planes[nr].heading > pi then  planes[nr].heading := planes[nr].heading - (2*pi);    // +- pi
        end;
        speed_st:='V='+inttostr(round(planes[nr].speed))+' HD='+inttostr(round(planes[nr].heading/rad))+' Var='+inttostr(planes[nr].steigen)+'  ';
        ServerMsg := 4;
      end;
      //airborne, ground
      if up_airborne then  planes[nr].airborne := true;
      if up_ground   then  planes[nr].airborne := false;

      memoline := memoline+ident_st+alt_St + koord_st + speed_st;
      writelog(typstr+' '+ident_st+alt_St+ koord_st+ speed_st);
      if up_koord_air or up_koord_surf then airplot(nr); // grafische Ausgabe des gesamten bildschirms !!!!!
    end; 
  end; // ME_decode


  // einen Frame als RAW versenden
  //  RAWnachricht    : string[255];
  procedure RAWServerSendFrame;
  var k   : integer;
      //TAG : integer;
  begin
    // AFrame.T enthält die Zeit
    //TAG := round(AFrame.T- trunc(AFrame.T) * TagFrequenz *24*60*60);
    //RAWnachricht := RAWnachricht + '@' + inttohex(TAG,12);
    RAWnachricht := '*';
    for k:=0 to 6 do RAWnachricht := RAWnachricht + inttohex(AFrame.B[k],2);
    if DF>15 then begin
       for k:=7 to 13 do RAWnachricht := RAWnachricht + inttohex(AFrame.B[k],2);
    end;
    RAWnachricht := RAWnachricht + ';'+chr($0A)+chr($0D);
    //if length(RAWnachricht) > 200 then begin
      RAWServerSend2All(RAWnachricht);
      RAWnachricht := '';
    //end;
  end;  //RAWServerSendFrame


  // noch vorhandene RAW daten senden
  procedure RAWServerFlush;
  begin
    if ServerSocketRAW.active then begin
      if RAWnachricht <> '' then begin
        RAWServerSend2All(RAWnachricht);
        RAWnachricht := '';
      end;
    end;
  end;  //RAWServerFlush

begin    // decode
  while framepointer <> LastFrame do begin
    alt_st := '';
    fst    := '';

    inc(framepointer);
    if framepointer>maxFrame then framepointer := 0;
    AFrame := Frames[framepointer];

    Frames[framepointer].B[14] := 0;       // stelle in frames wieder frei
    DF       := AFrame.B[0] shr 3;         // Download-Format
    if DF>=24 then DF:=24;                 // 11*** = DF24
    CA_CF_AF := AFrame.B[0] and 7;

    if logon then begin
      writelog('*');
      for k:=0 to 13 do writelog(inttohex(AFrame.B[k],2));
      LongTimeFormat := 'hh:nn:ss (zzz)';
      writelog('; '+ timetostr(AFrame.T) +' DF'+inttostr(DF)+':' );
      if DF<10 then writelog(' ' );
    end;

    // versenden der RAW-daten , max 255 zeichen lang
    // es fehlt der Zeitstempel
    // es wird jeder Frame einzeln gesendet
    // mit RAWServerSendFrame kann das gebündelt werden
    if ServerSocketRAW.active then begin
      if (AFrame.source=0) or (not LocalRawOnly) then begin
        RAWServerSendFrame;
      end;
    end;

    nr              := -1;
    Rc              := 0;
    up_alt          := false;
    up_koord_air    := false;
    up_koord_surf   := false;
    up_speed_ground := false;
    up_speed_air    := false;
    up_ident        := false;
    up_airborne     := false;
    up_ground       := false;

    TISB_exists     := false; // TIS-B    §2.2.17
    interogatorID   := 0;
    ServerMsg       := 0;
    AC_st           := '';
    ID_st           := '';
    memoline        := '';
    
    // 56 bit
    // DF  0        short ait to air surveilance (ACAS)
    // DF  4        surveilance, altitude reply
    // DF  5        surveilance, identify reply
    // DF 11    *   all call reply
    // 112 bit
    // DF 16        long air to air surveilance (ACAS)
    // DF 17    *   extended squitter
    // DF 18    *   extended squitter, no transponder (TIS)
    // DF 19    *   military extended squitter
    // DF 20        Comm-B, altitude reply
    // DF 21        Comm-B, identify reply
    // DF 24        Comm-D, extended length message (ELM)

    //Comm-B. A 112-bit reply containing the 56-bit MB message field.
    // This field is used by the downlink SLM, groundinitiated and broadcast protocols.
    //Comm-D. A 112-bit reply containing the 80-bit MD message field.
    // This field is used by the downlink ELM protocol.


    // DF downlink format                X 3.1.2.3.2.1.2
    // VS vertical status                X 3.1.2.8.2.1     1 bit 0=airborne
    // CC cross-link capability            3.1.2.8.2.3
    // RI reply information              X 3.1.2.8.2.2
    // AC altitude code                  X 3.1.2.6.5.4
    // AP address/parity                 X 3.1.2.3.2.1.3  (3.1.2.3.3.2)
    // MV message, ACAS                  X 3.1.2.8.3.1
    // CA capability                     X 3.1.2.5.2.2.1
    // AA address, announced             X 3.1.2.5.2.2.2
    // ME message, extended squitter     X 3.1.2.8.6.2
    // PI parity/interrogator identifier X 3.1.2.3.2.1.4   (3.1.2.3.3.2)
    // CF control field                    3.1.2.8.7.2
    // FS flight status                  X 3.1.2.6.5.1     3 bit
    // DR downlink request               X 3.1.2.6.5.2     5 bit
    // UM utility message                X 3.1.2.6.5.3     6 bit = IIS(4bit) + IDS(2bit)
    // MB message, Comm-B                X 3.1.2.6.6.1
    // ID identity                       X 3.1.2.6.7.1

    // KE control, ELM                     3.1.2.7.3.1     1 bit  0=downlink ELM transmission; 1=uplink ELM acknowledgement
    // ND number of D-segment              3.1.2.7.3.2     4 bit
    // MD message, Comm-D                  3.1.2.7.3.3    80 bit

    
    // KE Control ELM; signalisiert, ob in MD ein Downlink ELM Segment oder ein Acknowledgment auf ein Uplink ELM enthalten ist
    // ND Number of D-Segments; enthält die Nummer des Segments der Comm C ELM
    // MD Message Comm D; enthält die Nutzdaten der Downlink ELM
    // AP Address/Parity; enthält die 24Bit Adresse des Flugzeuges überlagert mit den Parity-Bits


    try

    if DF=0 then begin        // ACAS
      //VS,  CC,  RI,  AC,  AP
      typstr :='Short-air-sourv.';
      // weiter nur, wenn flugzeug bereit getracked
      if AP_decode(DF)>=0 then begin
        AC_decode;
        VS_decode;
        CC_decode;
        RI_decode;
      end else memoline:='';
    end;

    if DF=4 then begin
      //FS,  DR,  UM,  AC,  AP
      typstr :='Sourv.-alt-reply';
      if AP_decode(DF)>=0 then begin
        AC_decode;
        FS_decode;
        DR_decode;
        UM_decode;
        ServerMsg := 5;
      end else memoline:='';
    end;

    except elog('Decode DF0..DF4'); end;

    try

    if DF=5 then begin
      //FS,  DR,  UM,  ID,  AP
      typstr :='Sourv-ident-repl';
      if AP_decode(DF)>=0 then begin
        ID_decode;
        FS_decode;
        DR_decode;
        UM_decode;
        ServerMsg := 6;
      end else memoline:='';
    end;

    if DF=11 then begin
      // kann Squitter wie auch Antwort sein
      //CA,  AA,  PI
      typstr :='All-call-reply  ';
      if AA_decode>=1 then begin       // prueft auch PI
        CA_decode;
        ServerMsg := 8;
      end else memoline:='';
    end;

    except elog('Decode DF5..DF11'); end;

    try

    if DF=16 then begin        // ACAS
      //VS,  RI,  AC,  MV,  AP
      typstr :='Long-air-sourv. ';
      if AP_decode(DF)>=0 then begin
        AC_decode;
        VS_decode;
        RI_decode;
        MV_decode;
      end else memoline:='';
    end;

    //(DF=17 or DF=18 and CF=0, 1 or 6, or DF=19 and AF=0)
    if DF=17 then begin
      //CA,  AA,  ME,  PI
      typstr :='Extended-squitt.';
      if AA_decode>=0 then begin          // prueft auch PI
        ME_decode(cpr_adsb);                  // ADS-B - Message-field
        CA_decode;
      end  else memoline:='';
    end;

    except elog('Decode DF16..DF17'); end;

    try

    if DF=18 then begin
      //CF, AA, ME,  PI
      //CF:
      //Code 0 = ADS-B ES/NT devices that report the ICAO 24-bit address in the AA field (3.1.2.8.7)
      //Code 1 = Reserved for ADS-B for ES/NT devices that use other addressing techniques in the AA field (3.1.2.8.7.3)
      //Code 2 = Fine format TIS-B message
      //Code 3 = Coarse format TIS-B message
      //Code 4 = Reserved for TIS-B management messages
      //Code 5 = TIS-B messages that relay ADS-B messages that use other addressing techniques in the AA field
      //Code 6 = ADS-B rebroadcast using the same type codes and message formats as defined for DF = 17 ADS-B messages
      //Code 7 = Reserved
      typstr :='Extended-squitt.';
      case CA_CF_AF of
       0: begin if AA_decode>=0 then ME_decode(cpr_adsb)   else memoline:=''; end;          // prueft auch PI
       1: ;  // kann flugzeug nicht bestimmen
       2: begin if AA_decode>=0 then ME_decode(cpr_fine)   else memoline:=''; end;          // prueft auch PI
       3: begin if AA_decode>=0 then ME_decode(cpr_coarse) else memoline:=''; end;          // prueft auch PI
       4:;  //reserved
       5:;  // kann flugzeug nicht bestimmen
       6: begin if AA_decode>=0 then ME_decode(cpr_adsb)   else memoline:=''; end;
       7:;  // reserved
      end;
    end;

    if DF=19 then begin
      //AF, ...
      typstr :='Mil-ext.-squitt.';
      case CA_CF_AF of
       0:  begin if AA_decode>=0 then ME_decode(cpr_adsb) else memoline:=''; end;   // prueft auch PI
       //1..7 military
      end;
    end;

    if DF=20 then begin  // wie DF4 mit MB
      //FS,  DR,  UM,  AC,  MB,  AP
      typstr :='ComB-alt-reply  ';
      if AP_decode(DF)>=0 then begin
        AC_decode;
        FS_decode;
        DR_decode;
        UM_decode;
        MB_decode;
        ServerMsg := 5;
      end else memoline:='';
    end;

    except elog('Decode DF18..DF20'); end;

    try

    if DF=21 then begin   // wie DF5 mit MB
      //FS,  DR,  UM,  ID,  MB,  AP
      typstr :='ComB-ident-reply';
      if AP_decode(DF)>=0 then begin
        ID_decode;
        FS_decode;
        DR_decode;
        UM_decode;
        MB_decode;
        ServerMsg := 6;
      end else memoline:='';
    end;

    //DF23 - this is consistend with official standard
    //2.2.3.2.7.3 TYPE Code “23” ADS-B Messages for “TEST”
    //Subtype=0 TEST;   Messages of Subtypes 1 through 7 are reserved.
    //heartbeat des decoders
    // kommt alle 1,3 sekunden
    // als DF23 mit AA=000000
    // 0 = DF=23 Subtype=0
    // 1-2   Uref  = ADC * 5   5V = 5120  (+2.4%)
    // 3-4   Usig
    // 5-6   Error_Header
    // 7-8   Error_Data
    // 9-10
    // 11 - 13 = 00-00-00
    if DF=23 then
    //if (AFrame.B[11] or AFrame.B[12] or AFrame.B[13]) = 0 then
    begin
      dec(Framecounter);  //heartbeat zaehlt nicht
      dec(HBFramecounter);  //heartbeat zaehlt nicht
      if AFrame.B[9]>1 then exit; // Fehler, darf nur 0 oder 1 sein
      if Status_HB  < 3 then inc(Status_HB);
      DecoderHeartBeat.Uref       := (AFrame.B[2] shl 8) + AFrame.B[1];
      DecoderHeartBeat.Usig       := (AFrame.B[4] shl 8) + AFrame.B[3];
      DecoderHeartBeat.ErrorHead  := (AFrame.B[6] shl 8) + AFrame.B[5];
      DecoderHeartBeat.ErrorData  := (AFrame.B[8] shl 8) + AFrame.B[7];
      DecoderHeartBeat.DebugMode  :=  AFrame.B[9];
      DecoderHeartBeat.FramesSend := (AFrame.B[11] shl 8) + AFrame.B[10];
      DecoderHeartBeat.FramesLost := (AFrame.B[13] shl 8) + AFrame.B[12];

      if HBFramecounter<4095 then DecoderHeartBeat.Frames := HBFramecounter else DecoderHeartBeat.Frames:=0;
      HBFramecounter := 0;
      Label6.caption := 'U-ref = '   +inttostr(round(DecoderHeartBeat.Uref*0.9765625))+'mV';
      Label3.caption := 'U-signal = '+inttostr(round(DecoderHeartBeat.Usig*0.9765625))+'mV';
      HeartBeatForm.getdata(DecoderHeartBeat);
      if HeartBeatForm.visible then begin
        HeartBeatForm.graphic;
        writelog(Label6.caption+ '  '+ Label3.caption+
                 '  EHd: '+ inttostr(DecoderHeartBeat.ErrorHead)+
                 '  EDa: '+ inttostr(DecoderHeartBeat.ErrorData) );
      end;
    end;  // DF23 - Heartbeat


    if DF=24 then begin
      //KE,  ND,  MD,  AP
      typstr :='ComD-ELM        ';
      if AP_decode(DF)>=0 then begin
        df:=df // platzhalter
      end else  memoline:='';
    end;

    except elog('Decode DF16..DF24'); end;

    // Daten ueber Server ausgeben
    if ServerMsg<>0 then ServerSendMSG(nr, ServerMsg);

    //daten an filter-Fenster ausgeben
    if nr<=lastPlane then
      if (filter1.checked and (FilterForm.FilterAA = planes[nr].AA)) then FilterForm.updatedata(nr, fst);

    //Ausgabe der decodierten Daten in memo2
    //falls Kreuz aktiviert ist, dann nur Daten des ausgewaehlen flugzeugs ausgeben
    if (memoline<>'') and (not FullScreen) and ListDecodedData then 
      if (Kreuz.nr = nr) or (Kreuz.counter < 1) then memo2_lines_add(memoline);

{ DM61
    if checkboxAll.checked then begin                                           //DM61
      if memoline<>'' then memo2_lines_add(memoline);                           //DM61
    end else if (AA = SelAA) and (memoline<>'') then memo2_lines_add(memoline); //DM61
}

    writelnlog('');
  end; // while
  RAWServerFlush;
end;  // decode


// aktualisiertes Flugzeug grafisch darstellen
// nr ist nummer des zu aktualisierenden flugzeuges
// ich bau aber das ganze bild auf
// darf aber nicht zu oft repaintPPI aufrufen
// ansonsten geht der rechner in die knie
procedure TForm1.Airplot(nr:integer);
begin
  //13*20ms = 260 ms
  // dadurch maximal 4 Bildaufbauten pro sekunde
  if repaint_update>13 then begin
    repaint_update:=0;
    repaintPPI(false, false);
  end;
end;  //Airplot


// Tabelle aller Flugzeuge im Tracker erstellen
//und aufräumen von "verlorenen" flugzeugen
// liefert anzahl der Traks
//
// sortierung entsprechend StringGrid1MouseDown
//  Sortierung       : integer = 0;      // spalte nach der in der Tasbelle sortiert wird   ACol;
//  Sortierrichtung  : boolean = false;  // false=aufsteigend; true=absteigend
function TForm1.make_Table : integer;
var anzahl  : integer;
    k, kk   : integer;
    zeile   : integer;
    totseit : integer;
    reihe   : array[0..maxPlane] of integer;
    a       : integer;
    inorder : boolean;

  // Vergleichsfunktion fuer bubble sort  
  function kleiner(a,b:integer):boolean;
  begin
    result:=false; //failsave;
    case Sortierung of
      0: result := a < b;
      1: result := planes[a].aa        < planes[b].aa;
      2: result := AA2Land(planes[a].aa) < AA2Land(planes[b].aa); // behelf
      3: result := planes[a].ident     < planes[b].ident;
      4: result := planes[a].altitude  < planes[b].altitude;
      5: result := planes[a].latitude  < planes[b].latitude;
      6: result := planes[a].longitude < planes[b].longitude;
      7: result := planes[a].speed     < planes[b].speed;
      8: result := planes[a].heading   < planes[b].heading;
      9: result := planes[a].steigen   < planes[b].steigen;
     10: result := planes[a].airframe.kenner.typs < planes[b].airframe.kenner.typs;
     11: result := planes[a].time_last < planes[b].time_last;
    end; //case
  end;

begin
  result := 0;
  anzahl := 0;
  // aktive flugzeuge zaehlen und Anzeigereihenfolge entsprechen dem Arrayindex festlegen
  for k:=lastPlane downto 0 do if planes[k].active then begin
    inc(anzahl);
    planes[k].Tabellenzeile := anzahl;   //neueste nach oben
    reihe[anzahl] :=k;
  end;

  // Umsortieren  (Bubblesort) je nach gewähltem Kriterium
  // sollte noch optimiert werden
  if anzahl>=2 then begin
    repeat
      inorder := true;
      for k:=1 to anzahl-1 do if kleiner(reihe[k], reihe[k+1]) then begin
        inorder := false;
        a := reihe[k]; reihe[k] := reihe[k+1]; reihe[k+1]:= a;  // swap
      end;
    until inorder;
  end;
  for k:=1 to anzahl do planes[reihe[k]].Tabellenzeile := k;

  // Sortierung umkehren fals gefordert
  if Sortierrichtung then
    for k:=0 to lastPlane do if planes[k].active then
      planes[k].Tabellenzeile := anzahl-planes[k].Tabellenzeile+1;

  StringGrid1.rowcount := anzahl+1;
  if StringGrid1.rowcount>1 then StringGrid1.fixedrows:=1;
  //  col, row  spalte, zeile
  StringGrid1.cells[0,0]:='Nr.';      StringGrid1.ColWidths[0]:=40;  // Platz fuer 5 Stellen
  StringGrid1.cells[1,0]:='ICAO24';   StringGrid1.ColWidths[1]:=50;
  StringGrid1.cells[2,0]:='Regist.';  StringGrid1.ColWidths[2]:=68;
  StringGrid1.cells[3,0]:='Ident';    StringGrid1.ColWidths[3]:=63;  //callsign

  StringGrid1.cells[4,0]:='Alt';      StringGrid1.ColWidths[4]:=50;
  StringGrid1.cells[5,0]:='Lat';      StringGrid1.ColWidths[5]:=50;
  StringGrid1.cells[6,0]:='Long';     StringGrid1.ColWidths[6]:=50;
  StringGrid1.cells[7,0]:='Speed';    StringGrid1.ColWidths[7]:=40;
  StringGrid1.cells[8,0]:='Head.';    StringGrid1.ColWidths[8]:=40;
  StringGrid1.cells[9,0]:='Climb';    StringGrid1.ColWidths[9]:=45;
  StringGrid1.cells[10,0]:='Type';    StringGrid1.ColWidths[10]:=50;

  StringGrid1.cells[11,0]:='T-out';   StringGrid1.ColWidths[11]:=40;
{
  StringGrid1.cells[12,0]:='Squawk';   //DM61
  StringGrid1.cells[13,0]:='Dist' ;    //DM61
  StringGrid1.cells[14,0]:='Nr2' ;     //DM61
}

  for k:=lastPlane downto 0 do if planes[k].active then begin
    //inc(zeile);
    zeile:=planes[k].Tabellenzeile;
    StringGrid1.cells[0,zeile]:=inttostr(k);
    StringGrid1.cells[1,zeile]:=inttohex(planes[k].aa,6);
    for kk:=2 to 10 do StringGrid1.cells[kk,zeile]:='';
    if planes[k].airframe.known
      then StringGrid1.cells[2,zeile]:=planes[k].airframe.kenner.name
      else StringGrid1.cells[2,zeile]:=AA2Land(planes[k].aa);
    if planes[k].ident<>''          then StringGrid1.cells[3,zeile]:=planes[k].ident;
    if planes[k].altitude<>0        then StringGrid1.cells[4,zeile]:=inttostr(planes[k].altitude);
    if not planes[k].airborne       then StringGrid1.cells[4,zeile]:='ground';
    if planes[k].latitude<>0        then StringGrid1.cells[5,zeile]:=floattostrF(planes[k].latitude, ffFixed, 5, 2);
    if planes[k].longitude<>0       then StringGrid1.cells[6,zeile]:=floattostrF(planes[k].longitude, ffFixed, 5, 2);
    if planes[k].speed<>0           then StringGrid1.cells[7,zeile]:=inttostr(round(planes[k].speed));  // in knoten
    //if planes[k].heading<>noheading then StringGrid1.cells[8,zeile]:=inttostr(round(planes[k].heading/rad +360) mod 360);
    if planes[k].heading<>noheading then StringGrid1.cells[8,zeile]:=rad2instr(planes[k].heading, 2);
    if planes[k].steigen<>0         then StringGrid1.cells[9,zeile]:=inttostr(planes[k].steigen);
    if planes[k].airframe.known     then StringGrid1.cells[10,zeile]:=planes[k].airframe.kenner.typs
                                    else StringGrid1.cells[10,zeile]:='';
{  DM61
    if planes[k].mod3id<>0          then StringGrid1.cells[12,zeile]:=inttostr(planes[k].mod3id);            //DM61
    if (planes[k].distance<>0) and (planes[k].latitude<>0)
                            then StringGrid1.cells[13,zeile]:=floattostrF(planes[k].distance,ffFixed, 5, 1); //DM61
    StringGrid1.cells[14,zeile]:=inttostr(planes[k].nr_nahe);                                                //DM61
}

    //und aufräumen von "verlorenen" flugzeugen
    totseit := round( (now - planes[k].time_last)*24*60*60); // Sekunden seit letztem frame
    if not planes[k].missed then StringGrid1.cells[11,zeile]:= inttostr(totseit)
                            else StringGrid1.cells[11,zeile]:= inttostr(totseit)+' M';
    if totseit > 10 then begin  // vermeidet falsche Koordinaten von unzusammenhängenden Frames (odd/even)
      planes[k].lat_0  := 0;
      planes[k].lat_1  := 0;
      planes[k].long_0 := 0;
      planes[k].long_1 := 0;
      planes[k].missed := true;
    end else begin
      planes[k].missed := false;
      inc(result);
    end;
    if maxtotzeit > 0 then if totseit > maxtotzeit then begin
      planes[k].active:=false; // der kommt nicht mehr wieder
      if planes[k].airframe.known
        then reportseen(planes[k].aa, planes[k].airframe.kenner.name, planes[k].airframe.kenner.typs, planes[k].airframe.kenner.typl, '<', planes[k].ident)
        else reportseen(planes[k].aa, '#'+AA2Land(planes[k].aa), '', '','<', planes[k].ident);
      // ich hoffe mal, das sich das Folgende nicht auf die for-schleife auswirkt
      if k=lastplane then repeat dec(lastplane); until (planes[lastplane].active=true) or (lastplane=0);
      if Kreuz.nr = k  then Kreuz.counter :=  0;
      if nearplane = k then nearplane     := -1;
{
      planes[k].distance := 8000;        //DM61
   // if (ComPort2.Connected) and (planes[k].nr_nahe<jmax) then DM61send(k,0);  //DM61 Lösch-kdo senden
      if (ComPort2.Connected) then DM61send(k,0);  //DM61 Lösch-kdo senden
}
    end;
  end;
end;  //make_Table


// grafik neu aufbauen
// BMove = true  erneuere die Projektion aller Koordinaten auf den Bildschirm
// newBM = true  alles neu Zeichnen
// newBM = false nur aircraft und overlay neu zeichenen
procedure TForm1.repaintPPI(Bmove : boolean; newBM : boolean);
begin
  if Bmove then begin
    Kreuz.counter := 0;
    hastomove     := true;
    MakeNewBM     := true;
    Statusbar1.panels[0].text := 'Center: Lat='+ koordtostr(ZentB)+' Long= '+koordtostr(ZentL);
    move;
  end;
  if newBM then MakeNewBM := true;
  repaint_update:= 0;
  try Zeichne; except; elog('Zeichne'); end;   // einfacher bugfix fuer den manchmal auftretenden Fehler
end; // repaintPPI


// scale aendern
// 16   :  5    : ganzer globus
// 85   : 3300  : 11 NM
// Stretch=1
procedure TForm1.TrackBar1Change(Sender: TObject);
begin
  //M[K].Y:=round((ZentB-M[k].breite)*Scale);
  // 1 Pixel = 60 NM / Scale
  // Bildbreite [NM] = Bildbreite x 60 / Scale
  Scale:=power(1.1,Trackbar1.Position)*Stretch;  //16..76
  // Breite des bildes in NM
  Label2.caption:='E-W: '+inttostr(round(BM.width/Scale*57))+' NM';
  Label4.caption:='ZOOM '+inttostr(round(Scale));
  // osm-Karten unter zoom 12 weisen einen Offset zu der globalen Weltkarte auf
  // deshalb sollte globale Weltkarte und OSM nicht gleichzeitig
  // angezeigt werden können
  // Als Schaltschwelle dient Scale=20
  OSMmap1.enabled  := Scale>20;   // osm und srtm nur, wenn die Erde dicht genug ist
  ToolButton7.enabled  := OSMmap1.enabled;
  if OSMmap1.enabled then  ToolButton7.imageindex:=12 else ToolButton7.imageindex:=13;
  hastomove := true;
  Update;
  repaintPPI(true, true);
end; //TrackBar1Change


//****MAUSBEDIENUNG IM BILD************************************//

procedure TForm1.Image1MouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
var
  k, d   : integer;
  xx, yy : integer;
  dx, dy : integer;
begin
  //ein nahes Flugzeug finden
  nearplane := -1;
  d := 20;
  for k:=lastPlane downto 0 do if planes[k].active then if planes[k].trackindex >= 0 then begin
    if showpredictedposition1.checked and planes[k].airborne then begin
      xx := (BM.width div 2) + planes[k].guess.x;
      yy := (BM.height div 2)+ planes[k].guess.y;
    end else begin
      xx := (BM.width div 2) + planes[k].Track[planes[k].trackindex].x;
      yy := (BM.height div 2)+ planes[k].Track[planes[k].trackindex].y;
    end;
    dx := abs(X-xx);
    dy := abs(Y-yy);
    if (dx+dy)<d then begin
      d := dx+dy;
      nearplane := k;
      if planes[k].airframe.known then
        reportselect(planes[k].aa, planes[k].airframe.kenner.name, planes[k].airframe.kenner.typs, planes[k].airframe.kenner.typl)
      else
        reportselect(planes[k].aa, '#'+AA2Land(planes[k].aa),'','');
    end;
  end;

  if nearplane=-1 then begin  // test verhindert, dass beim Flugzeug-anklicken versehentlich OSM gelöscht wird
    Pdreh.x := X;
    Pdreh.y := Y;
    Pdreh.V := true;
  end;

end; // Image1MouseDown


procedure TForm1.Image1MouseUp(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
  Pdreh.V := false;
  if (osm.xoff<>0) or (osm.yoff<>0) then begin
    if not mapmode then begin
      clrscr(osm.BM);
      osmOK := false;
      OSMbackground1.checked := false;
    end;
    repaintPPI(false, true);
  end;
  if (srtm.xoff<>0) or (srtm.yoff<>0) then begin
    if not mapmode then begin
      clrscr(srtm.BM);
      SRTMbackground1.checked := false;
      srtmOK := false;
    end;
    repaintPPI(false, true);
  end;
end; // Image1MouseUp


//Drehung der sphaerischen Koordinaten
//zur ermittlung der Erdkoordinaten aus "Draufsichtkoordinaten"
//mode 0 normale berechnung
//     1 vorbereitung zum schnellen rechnen
//     2 schneller mode
procedure TForm1.Rotation(var lat, lon : real; mode : integer);
var
  MT            : TMatrix;
  MrZ, MrX, MrY : TMatrix;
  i, k : integer;

  //Multiplikation zweier Matrizen       k-Spalte, i-Zeile
  function MulMatrix(a,b:TMatrix):TMatrix;
  var l, i2, k2 : integer;
  begin
    for i2:= 1 to 4 do for k2:=1 to 4 do begin
      result[i2,k2]:=0;
      for l:=1 to 4 do result[i2,k2]:=result[i2,k2]+a[i2,l]*b[l,k2];
    end;
  end;

  //Addition zweier Matrizen
  function AddMatrix(a,b:TMatrix):TMatrix;
  var i,k:integer;
  begin
    for i:=1 to 4 do for k:=1 to 4 do result[i,k]:=a[i,k]+b[i,k];
  end;

  //Rotation um die Z-Achse
  // drehung rechts herum
  function RotZ(M:TMatrix; w: real):TMatrix;
  begin
    //Azimutmatrix - Drehung um Z-Achse
    MrZ[1,1]:=cos(w); MrZ[2,2]:= MrZ[1,1];
    MrZ[1,2]:=sin(w); MrZ[2,1]:=-MrZ[1,2];
    result := MulMatrix(M, MrZ);
  end;

  //Rotation um die X-Achse
  // drehung um die vertikale achse nach rechts
  function RotX(M:TMatrix; w: real):TMatrix;
  begin
    //Elevationsmatrix - Drehung um x-Achse
    MrX[2,2]:=cos(w); MrX[3,3]:= MrX[2,2];
    MrX[2,3]:=sin(w); MrX[3,2]:=-MrX[2,3];
    result := MulMatrix(M, MrX);
  end;

  //Rotation um die Y-Achse
  // drehung um die horizontale achse nach oben
  function RotY(M:TMatrix; w: real):TMatrix;
  begin
    //Elevationsmatrix - Drehung um y-Achse
    MrY[1,1]:=cos(w); MrY[3,3]:= MrY[1,1];
    MrY[3,1]:=sin(w); MrY[1,3]:=-MrY[3,1];
    result := MulMatrix(M, MrY);
  end;

  // sphaerisch in karthesisch
  function sk2kk(lat, lon : real):TMatrix;
  var x,y,z : real;
      pw    : real;
  begin
    pw := pi/2 - lat;
    x := sin(pw) * cos(lon);
    y := sin(pw) * sin(lon);
    z := cos(pw);
    result[1,1]:=X;
    result[1,2]:=Y;
    result[1,3]:=Z;
    result[1,4]:=1;
    result[2,1]:=0;
    result[2,2]:=0;
    result[2,3]:=0;
    result[2,4]:=0;
    result[3]:=result[2];
    result[4]:=result[2];
  end;

  // karthesisch in sphaerisch
  procedure kk2sk(M:TMatrix);
  var x, y, z : real;
      pw : real;    //polarwinkel 0..pi
  begin
    x := M[1,1]/M[1,4];
    y := M[1,2]/M[1,4];
    z := M[1,3]/M[1,4];
    lon := arctan2(y,x); //-Pi .. Pi
    pw := arccos(z);     // 0..pi
    lat := pi/2 - pw;    //Pi/2 .. -Pi/2
  end;

begin  //Rotation

  if mode<2 then begin
    //Matrizen löschen
    for i:=1 to 4 do for k:=1 to 4 do MT[i,k]:=0;
    MT[1,1]:=1; MT[2,2]:=1; MT[3,3]:=1; MT[4,4]:=1;
    MrZ:=MT;
    MrX:=MT;
    MrY:=MT;
    MS :=MT;
  end;

  if Mode=0 then begin
    MT := sk2kk(lat, lon);          // sphaerisch -> karthesisch
    MT := RotY(MT, 90*rad - ZentB); // drehung um die horizontale achse nach oben
    MT := RotZ(MT, ZentL);          // drehung um die vertikale achse nach rechts
    kk2sk(MT);                      // karthesisch -> sphaerisch
  end;

  if Mode=1 then begin
    //Elevationsmatrix - Drehung um y-Achse
    MrY[1,1]:=cos(90*rad - ZentB); MrY[3,3]:= MrY[1,1];
    MrY[3,1]:=sin(90*rad - ZentB); MrY[1,3]:=-MrY[3,1];
    //Azimutmatrix - Drehung um Z-Achse
    MrZ[1,1]:=cos(ZentL); MrZ[2,2]:= MrZ[1,1];
    MrZ[1,2]:=sin(ZentL); MrZ[2,1]:=-MrZ[1,2];
    MS := MulMatrix(MrY, MrZ);
  end;

  if Mode=2 then begin
    MT := sk2kk(lat, lon);          // sphaerisch -> karthesisch
    MT := MulMatrix(MT, MS);
    kk2sk(MT);                      // karthesisch -> sphaerisch
  end;

end; //Rotation


//wandelt Bildkorrdinaten in Erdkoordinaten um
function TForm1.xy2koord(x, y : integer):Tkoordinaten;
var
  xx     : real;
  radius : real;
  dx, dy : real;
  xlat, xlon : real;
begin
  //Entfernung zur Mitte
  // 1 Pixel = 57 NM / Scale
  radius := erdradius;
  dx := x-image1.width/2 ;
  dy := y-image1.height/2;
  xx := sqrt( sqr(dx) + sqr(dy) ) * 57 / Scale;  // Abstand in der Projektion in NM
  if xx<radius then begin
    // errechnen der Koordinaten bei Blick auf den Nordpol:   ZentB = 90; ZentL = 0;
    dx := dx *57 / scale / radius;
    dy := dy *57 / scale / radius;
    xx := xx / radius;
    xlat := arccos(xx);        //breite    0 .. Pi
    xlon := arctan2(dx, dy);   //laenge    -Pi .. +Pi
    // nun Drehung der Erde
    rotation(xlat, xlon, 0);
    result.laenge := xlon;
    result.breite := xlat;
    result.v      := true;
  end else begin
    result.laenge := 0;
    result.breite := 0;
    result.v      := false;
  end;
end;  // xy2koord


// Karte mit der Maus bewegen und zoomen
procedure TForm1.Image1MouseMove(Sender: TObject; Shift: TShiftState; X,
  Y: Integer);
var
  r      : integer;
  xx     : real;
  radius : real;
  dx, dy : real;
  xlat, xlon : real;
begin
  //Entfernung zur Mitte
  // 1 Pixel = 57 NM / Scale
  radius := ErdRadius;  // NM
  dx := x-image1.width/2 ;
  dy := y-image1.height/2;
  xx := sqrt( sqr(dx) + sqr(dy) ) * 57 / Scale;  // Abstand in der Projektion in NM
  if xx<radius then begin
    r := round( radius * arcsin(xx/radius));
    Statusbar1.panels[1].text := 'Range: '+ inttostr(r)+' NM = '+inttostr(round(r*1.8))+' km';

    // errechnen der Koordinaten bei blick auf den Nordpol:   ZentB = 90; ZentL = 0;
    dx := dx *57 / scale / radius;
    dy := dy *57 / scale / radius;
    xx := xx / radius;
    xlat := arccos(xx);        //breite
    xlon := arctan2(dx, dy);   //laenge
    rotation(xlat, xlon, 0);
    Statusbar1.panels[2].text := 'Koord: '+ koordtostr(xlat) +' N / '+koordtostr(xlon)+' E';
  end else begin
    Statusbar1.panels[1].text := 'Range: --';
    Statusbar1.panels[2].text := 'Koord: --';
  end;

  //koordinaten verschieben
  if Pdreh.V then begin
  //Erde drehen mit linker Maustaste
    if ssLeft in Shift then begin
      if Pdreh.x<>X then begin
        osm.xoff  := osm.xoff  - round(( Pdreh.x - X )* osm.Scale/Scale);
        srtm.xoff := srtm.xoff - round(( Pdreh.x - X )* srtm.Scale/Scale);
        ZentL := ZentL + (Pdreh.x-X)*57/Scale/60*rad  / cos_ZentB;         // in radiant
        if ZentL> PI then ZentL := ZentL-(2*PI);
        if ZentL<-PI then ZentL := ZentL+(2*PI);
        Pdreh.x := X;
        hastomove:= true;
      end;
      if Pdreh.y<>Y then begin
        osm.yoff  := osm.yoff  - round(( Pdreh.y - y )* osm.Scale/Scale);
        srtm.yoff := srtm.yoff - round(( Pdreh.y - y )* srtm.Scale/Scale);
        ZentB := ZentB - (Pdreh.y-Y)*57/Scale/60*rad;
        if ZentB> PI then ZentB := ZentB-(2*PI);
        if ZentB<-PI then ZentB := ZentB+(2*PI);
        sin_ZentB := sin(ZentB);
        cos_ZentB := cos(ZentB);
        Pdreh.y := Y;
        hastomove:= true;
      end;
      //Button10Click(nil);        //DEBUG
    end;
    // zoom mit rechter Maustaste
    if ssright in Shift then begin
      if abs(Pdreh.y-Y)>6 then begin
        TrackBar1.position := TrackBar1.position +  (Pdreh.y-Y) div 6;
        Pdreh.y := Y;
        hastomove:= true;
      end;
    end;
    repaintPPI(true, true);
  end;

end;  // Image1MouseMove


//*** MENUE***************************************************//

procedure TForm1.Grid1Click(Sender: TObject);
begin
  grid1.checked := not grid1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.Towns1Click(Sender: TObject);
begin
  Towns1.checked := not Towns1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.Aircraft1Click(Sender: TObject);
begin
  Aircraft1.checked := not Aircraft1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.Statenames1Click(Sender: TObject);
begin
  Statenames1.checked := not Statenames1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.Crosshair1Click(Sender: TObject);
begin
  Crosshair1.checked := not Crosshair1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.Detailedmaps1Click(Sender: TObject);
begin
  detailedmaps1.checked := not detailedmaps1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.States1Click(Sender: TObject);
begin
  States1.checked := not States1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.Airports1Click(Sender: TObject);
begin
  Airports1.checked := not Airports1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.AirportILS1Click(Sender: TObject);
begin
  AirportILS1.checked := not AirportILS1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.AirportAltitude1Click(Sender: TObject);
begin
  AirportAltitude1.checked := not AirportAltitude1.checked;
  repaintPPI(false, true);
end;

procedure TForm1.RangeCircles1Click(Sender: TObject);
begin
  RangeCircles1.checked := not RangeCircles1.checked;
  repaintPPI(false, true);
end;

procedure TForm1.Receiver1Click(Sender: TObject);
begin
  Receiver1.checked := not Receiver1.checked;
  repaintPPI(false, true);
end;

procedure TForm1.GroundRADAR1Click(Sender: TObject);
begin
  GroundRADAR1.checked := not GroundRADAR1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.ATSRoutes1Click(Sender: TObject);
begin
  ATSRoutes1.checked := not ATSRoutes1.checked;
  repaintPPI(true, true);    // da die nicht per default schon an und berechnet sind
end;

procedure TForm1.GPXOverlay1Click(Sender: TObject);
begin
  GPXOverlay1.checked := not GPXOverlay1.checked;
  repaintPPI(true, true);    // da die nicht per default schon an und berechnet sind
end;

procedure TForm1.maximumrange1Click(Sender: TObject);
begin
  maximumrange1.checked := not maximumrange1.checked;
  repaintPPI(true, true);
end;

procedure TForm1.OSMbackground1Click(Sender: TObject);
begin
  if SRTM1.checked then begin
    if not osmOK then OSMbackground1.checked := false
                 else OSMbackground1.checked := not OSMbackground1.checked;
    if OSMbackground1.checked then SRTMBackground1.checked := false;
  end else begin
    if not srtmOK then OSMbackground1.checked := false
                  else OSMbackground1.checked := not OSMbackground1.checked;
    if OSMbackground1.checked then OSMbackground1.checked := false;
  end;
  repaintPPI(false, true);
end;

procedure TForm1.SRTMBackground1Click(Sender: TObject);
begin
  if not srtmOK then SRTMBackground1.checked := false
                else SRTMBackground1.checked := not SRTMBackground1.checked;
  if SRTMBackground1.checked then OSMbackground1.checked := false;
  repaintPPI(false, true);
end;

procedure TForm1.End1Click(Sender: TObject);
begin
  disconnect;
  close;
end;


// versieht einen Menuepunkt mit einem Bitmap einer Farbe
procedure TForm1.MenuColorBitmapUpdate(MenuePunkt : TMenuItem; MenueFarbe : TColor);
var
    olp,urp   : TPoint;
    rechteck  : TRect;
begin
  if (MenuePunkt.Bitmap=nil) then MenuePunkt.Bitmap := TBitmap.Create;
  MenuePunkt.Bitmap.Height := 10;
  MenuePunkt.Bitmap.Width  := 10;

  olp.x:=1;
  olp.y:=1;
  urp.x:=MenuePunkt.Bitmap.Width-1;
  urp.y:=MenuePunkt.Bitmap.Height-1;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  MenuePunkt.Bitmap.canvas.Brush.Color:=MenueFarbe;
  MenuePunkt.Bitmap.canvas.Brush.Style:=bsSolid;
  MenuePunkt.Bitmap.canvas.FillRect(Rechteck);
end; //MenuColorBitmapUpdate


procedure TForm1.UpdateAllMenuColors;
begin
  MenuColorBitmapUpdate(Borders1, borderColor);
  MenuColorBitmapUpdate(Background1, BackColor);
  MenuColorBitmapUpdate(AircraftLabel1, LabelColor);
  MenuColorBitmapUpdate(Grid2, latlongColor);
  MenuColorBitmapUpdate(Crosshair2, chairColor);
  MenuColorBitmapUpdate(Statenames2, stateColor);
  MenuColorBitmapUpdate(Towns2, townColor);
  MenuColorBitmapUpdate(Airports2, portColor);
  MenuColorBitmapUpdate(ATSRoutes2, atsColor);
  MenuColorBitmapUpdate(GPXOverlay2, GpxColor);
  MenuColorBitmapUpdate(Textoverlay1, ovlColor);
  MenuColorBitmapUpdate(GroundRADAR2, iffColor);
  MenuColorBitmapUpdate(rangering1, rrColor);
  MenuColorBitmapUpdate(Receiver2, rxColor);
  MenuColorBitmapUpdate(maximumRange2, rriColor);
end;

procedure TForm1.Borders1Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    borderColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Borders1, borderColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Background1Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    BackColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Background1, BackColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.AircraftLabel1Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    LabelColor := ColorDialog1.Color;
    if randomcolor1.checked then randomcolor1click(nil);
    MenuColorBitmapUpdate(AircraftLabel1, LabelColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Grid2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    latlongColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Grid2, latlongColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Crosshair2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    chairColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Crosshair2, chairColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Statenames2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    stateColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Statenames2, stateColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Towns2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    townColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Towns2, townColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Airports2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    portColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Airports2, portColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.ATSRoutes2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    atsColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(ATSRoutes2, atsColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.GPXOverlay2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    GpxColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(GPXOverlay2, GpxColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Textoverlay1Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    ovlColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Textoverlay1, ovlColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.GroundRADAR2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    iffColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(GroundRADAR2, iffColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.rangering1Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    rrColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(rangering1, rrColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.Receiver2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    rxColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(Receiver2, rxColor);
    repaintPPI(false, true);
  end;
end;

procedure TForm1.maximumRange2Click(Sender: TObject);
begin
  if ColorDialog1.Execute then  begin
    rriColor := ColorDialog1.Color;
    MenuColorBitmapUpdate(maximumRange2, rriColor);
    repaintPPI(false, true);
  end;
end;


// set colors to default values
procedure TForm1.defaults1Click(Sender: TObject);
begin
  borderColor   := clltGray;
  latlongColor  := clltGray;
  townColor     := clltBlue;
  chairColor    := clltGreen;
  cooColor      := clBlue;
  ovlColor      := clBlue;
  stateColor    := clltBlue;
  portColor     := clltBlue1;
  AtsColor      := clltYellow;
  GpxColor      := clltRed;
  rrColor       := clltGreen;
  rxColor       := clltRed;
  rriColor      := clBlack;
  IffColor      := clltRed;
  BackColor     := clWhite;
  Labelcolor    := clBlack;
  pale2.checked := true;        //osm
  randomcolor1.checked := true; // label
  UpdateAllMenuColors;
  repaintPPI(false, true);
end;


procedure TForm1.normal2Click(Sender: TObject);
begin
  normal2.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.ICAO2Click(Sender: TObject);
begin
  ICAO2.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.IATA2Click(Sender: TObject);
begin
  IATA2.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.none2Click(Sender: TObject);
begin
  none2.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.N50NM1Click(Sender: TObject);
begin
  N50NM1.checked := true;
  RangeCircles1.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.N50km1Click(Sender: TObject);
begin
  N50km1.checked        := true;
  RangeCircles1.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.N5005ftaltitude1Click(Sender: TObject);
begin
  N5005ftaltitude1.checked := true;
  RangeCircles1.checked    := true;  
  repaintPPI(false, true);
end;

procedure TForm1.fullcolor1Click(Sender: TObject);
begin
  fullcolor1.checked := true;
end;

procedure TForm1.pale1Click(Sender: TObject);
begin
  pale1.checked := true;
end;

procedure TForm1.pale2Click(Sender: TObject);
begin
  pale2.checked := true;
end;

procedure TForm1.gray1Click(Sender: TObject);
begin
  gray1.checked := true;
end;


procedure TForm1.N5111222Click(Sender: TObject);
begin
  //51 11,22'
  N5111222.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.N5112341Click(Sender: TObject);
begin
  // 51,1234
  N5112341.checked := true;
  repaintPPI(false, true);
end;

procedure TForm1.N5111221Click(Sender: TObject);
begin
  // 51:11'22''
  N5111221.checked := true;
  repaintPPI(false, true);
end;


procedure TForm1.after20s1Click(Sender: TObject);
begin
  after20s1.checked := true;
  maxtotzeit := 20;
end;

procedure TForm1.after1min1Click(Sender: TObject);
begin
  after1min1.checked := true;
  maxtotzeit := 60;
end;

procedure TForm1.after5min1Click(Sender: TObject);
begin
  after5min1.checked := true;
  maxtotzeit := 5*60;
end;

procedure TForm1.after30min1Click(Sender: TObject);
begin
  after30min1.checked := true;
  maxtotzeit := 30*60;
end;

procedure TForm1.after1hour1Click(Sender: TObject);
begin
  after1hour1.checked := true;
  maxtotzeit := 60*60;
end;

procedure TForm1.never1Click(Sender: TObject);
begin
  never1.checked := true;
  maxtotzeit := 10*60*60;
end;

procedure TForm1.hideitafter20sec1Click(Sender: TObject);
begin
  hideitafter20sec1.checked := not hideitafter20sec1.checked;
end;

procedure TForm1.showpredictedposition1Click(Sender: TObject);
begin
  showpredictedposition1.checked := not showpredictedposition1.checked;
end;

procedure TForm1.colorbyaltitude1Click(Sender: TObject);
begin
  colorbyaltitude1.checked := not colorbyaltitude1.checked;
end;

procedure TForm1.allairtargetswithin180NM1Click(Sender: TObject);
begin
  allairtargetswithin180NM1.checked := not allairtargetswithin180NM1.checked;
end;

procedure TForm1.allsurfacetargetswithin45NM1Click(Sender: TObject);
begin
  allsurfacetargetswithin45NM1.checked := not allsurfacetargetswithin45NM1.checked;
end;


// ************ Menuepunkt label 1 ... 4 lines
procedure TForm1.N2lines1Click(Sender: TObject);
begin
  N2lines1.checked := true;
end;

procedure TForm1.N3lines1Click(Sender: TObject);
begin
  N3lines1.checked := true;
end;

procedure TForm1.N4lines1Click(Sender: TObject); 
begin
  N4lines1.checked := true;
end;

procedure TForm1.N0LinesClick(Sender: TObject);
begin
  N0lines.checked := true;
end;

// ************ Menuepunkt  label color
procedure TForm1.randomcolor1Click(Sender: TObject);
begin
  randomcolor1.checked := not randomcolor1.checked;
  if randomcolor1.checked
    then AircraftLabel1.caption := 'Aircraft Label: random'
    else AircraftLabel1.caption := 'Aircraft Label';
end;

// *************** Menue Symbol
procedure TForm1.circle1Click(Sender: TObject);
begin
  circle1.checked := true;
end;

procedure TForm1.rectangle1Click(Sender: TObject);
begin
  rectangle1.checked := true;
end;

procedure TForm1.aircraft2Click(Sender: TObject);
begin
  aircraft2.checked := true;
end;

// *************** Menue Transition
procedure TForm1.N5000ft1Click(Sender: TObject);
begin
  N5000ft1.checked := true;
  TransitAlt := 5000;
end;

procedure TForm1.N6000ft1Click(Sender: TObject);
begin
  N6000ft1.checked := true;
  TransitAlt := 6000;
end;

procedure TForm1.N7000ft1Click(Sender: TObject);
begin
  N7000ft1.checked := true;
  TransitAlt := 7000;
end;

procedure TForm1.N8000ft1Click(Sender: TObject);
begin
  N8000ft1.checked := true;
  TransitAlt := 8000;
end;


//*************  PROGRAMMFENSTER SKALIEREN************************//

//skalieren der Grafik
procedure TForm1.FormResize(Sender: TObject);
begin
  // Mindestmaß
  if Form1.width<1280 then Form1.width  := 1280;
  if Form1.height<800 then Form1.height := 800;

  //position und groesse der graphic
  if not FullScreen then begin
    Image1.width := Form1.width  - 647;
    Image1.height:= Form1.height - 137;
  end else begin
    Image1.width := Form1.width  - 24;
    Image1.height:= Form1.height - 137;
  end;
  Image1.picture.Bitmap.width := Image1.width;
  Image1.picture.Bitmap.height:= Image1.height;
  BM.width     := Image1.width;
  BM.height    := Image1.height;
  BM2.width    := Image1.width;
  BM2.height   := Image1.height;

  //positionen von sichtbaren objekten festlegen
  panel1.left    := Form1.width  - 15 - panel1.width;    // das enthält alles rechts vom Bild
  panel1.height  := Form1.height - 137;                  // das enthält alles rechts vom Bild  800-159 = 641
  Label3.top     := panel1.height - 49;            // die Label im unteren Teil des Panel1
  Label5.top     := Label3.top;
  Label6.top     := Label3.top + 16;
  Label8.top     := Label3.top + 32;
  Label1.top     := Label6.top;
  TimeLabel.top  := Label3.top + 32;
  Memo2.top      := panel1.height - 185;           // decoded data

  if Memo2visible then begin                       // Tabellengröße anpassen und memo2 ein/aus
    Memo2.Visible := true;
    StringGrid1.Height := panel1.height - 336;
    ToolButton27.imageindex := 52;
    ToolButton27.hint := 'hide decoded data';
  end else begin
    Memo2.Visible := false;
    StringGrid1.Height := Panel1.Height - 192;
    ToolButton27.imageindex := 53;
    ToolButton27.hint := 'show decoded data';
  end;

  trackbar1.top  := Form1.height - 102;                  // Zoom
  Label2.top     := Form1.height - 102;                  // zoom
  Label4.top     := Form1.height -  95;                  // e-W-Ausdehnung
  panel1.visible := not FullScreen;
  progressbar1.top  := Form1.height - 90;
  progressbar1.left := Form1.width  - 340;

  Label2.caption := 'E-W: '+inttostr(round(BM.width/Scale*57))+' NM';
  repaintPPI(true, true);
end; //FormResize


// click in die Tabelle bewirkt Kreuz auf Flugzeug in Grafik
procedure TForm1.StringGrid1SelectCell(Sender: TObject; ACol,
  ARow: Integer; var CanSelect: Boolean);
var st             : string;
    nr             : integer;
begin
  if ARow=0 then exit; // erledigt StringGrid1MouseDown schon
  st := StringGrid1.Cells[0,ARow];
  if st<> '' then nr:=strtoint(st) else exit;
  //E_Tele3.text := inttohex(planes[nr].AA,6);   //DM61
  if planes[nr].trackindex >= 0 then begin
    Kreuz.counter := 30;    // 30 sekunden timeout
    Kreuz.nr      := nr;
    nearplane     := nr;
  end;
  FilterForm.getAA(planes[nr].AA);
  repaintPPI(false, false);
end; //StringGrid1SelectCell


//bei klick in oberste Zeile der Tabelle wird das sortierkriterium
// und die Sortierreichenfolge (aufsteigend absteigend) festgelegt
procedure TForm1.StringGrid1MouseDown(Sender: TObject;
  Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
var
  i, breite, spalte : Integer;
begin
  //War klick in oberste Zeile?
  if y<=StringGrid1.RowHeights[0] then begin
    // Klick war in Titelleiste
    //nun spalte bestimmen
    breite:=0;
    spalte:=0;
    for i:=0 to StringGrid1.ColCount-1 do begin
      breite:=breite+StringGrid1.ColWidths[i];
      if breite>x then begin
        spalte:=i;
        Break;
      end;
    end;
    // Klick war in Titelleiste
    // Zur Aenderung der Sortierreihenfolge
    Kreuz.counter := 0;
    // an dieser Stelle lassen sich Sortierreihenfolgen fuer die tabelle festlegen
    if Sortierung = spalte then Sortierrichtung:=not(Sortierrichtung)
    else begin
      Sortierung := spalte;
      Sortierrichtung := false; //aufsteigend
    end;
    activeTracks := make_Table; // Tabelle mit neuer sortierung zeichnen
  end;
end;   //StringGrid1MouseDown


procedure TForm1.KillCommunication;
var  asciistr : string;
begin
  //com-port
  try
    if ComPort1.Connected then begin
      if decoder=decoder_adsbPIC then begin
        send_buf[0] := SET_MODE;  // decoder abschalten
        send_buf[1] := 0;
        Sende_Empfange(2, 2);
        Timer1.enabled := false;
        decoderReset;
        sleep(250);
      end;
      if decoder=decoder_rxcontrol then begin
        asciistr := chr($13);       // datenstrom abschalten  mit CRTL-S = XOFF ; http://rxcontrol.free.fr/PicADSB/index.html
        Comport1.WriteStr(asciistr);
        Timer1.enabled := false;
        sleep(250);
      end;
      ComPort1.ClearBuffer(true, true);
      ComPort1.Close;  // das bleibt hängen
      ComPort1.ClearBuffer(true, true);
    end;
  except
    ;
  end;
  Timer1.enabled := false;
end;  // KillCommunication


// this is the end - my friend
procedure TForm1.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  KillCommunication;

  // gps
  if use1.checked then use1click(nil);

  // close log-files
  StopLog;
  StopErrorLog;
  StopUsbLog;

  try closefile(Watchlogfile); except ;end;

  //Netzwerk
  if ClientVerbunden        then ClientSocket.Close;
  if ServerSocket.Active    then ServerSocket.Close;
  if ServerSocketRAW.Active then ServerSocketRAW.Close;

  //initfile schreiben
  SaveInit;

  //statistik ueber empfangene IFF-Interrogatoren
  saveIntStat;

  //letzte Position in lastpos - Files schreiben
  saveposition(1);

  Action := caFree;

end;    //FormClose


// messe den analogen Signalpegel
procedure TForm1.Label3Click(Sender: TObject);
var wert      : integer;
    millivolt : integer;
begin
  if not ComPort1.Connected then exit;
  DecoderPause;

  send_buf[0] := SYS_ADC;           // ADC
  send_buf[1] := 1;                 // set AN1
  send_buf[2] := 0;
  Sende_Empfange(3, 16);

  send_buf[0] := RD_ADC;            // ADC
  send_buf[1] := 3;                 // read
  Sende_Empfange(2, 16);
  wert := receive_buf[3];           // high
  wert := wert*256+ receive_buf[2]; // low

  DecoderRestart;

  if wert>1023 then millivolt := 0
               else millivolt := round(wert*5000/1024);
  Label3.caption := 'U-signal = '+inttostr(millivolt)+'mV';
  writelnlog('Decoder: U-signal = '+inttostr(millivolt)+' mV');
end;  //Label3Click


// messe den Komparator-Referenzpegel
procedure TForm1.Label6Click(Sender: TObject);
var wert      : integer;
    millivolt : integer;
begin
  if not ComPort1.Connected then exit;
  DecoderPause;

  send_buf[0] := SYS_ADC;           // ADC
  send_buf[1] := 0;                 // set AN0
  send_buf[2] := 0;
  Sende_Empfange(3, 16);

  send_buf[0] := RD_ADC;            // ADC
  send_buf[1] := 3;                 // read
  Sende_Empfange(2, 16);
  wert := receive_buf[3];           // high
  wert := wert*256+ receive_buf[2]; // low

  DecoderRestart;

  if wert>1023 then millivolt := 0
               else millivolt := round(wert*5000/1024);
  Label6.caption := 'U-ref = '+inttostr(millivolt)+'mV';
  writelnlog('Decoder: U-ref = '+inttostr(millivolt)+' mV');
end;  //Label6Click



//************* O S M ***************************************//
//http://wiki.openstreetmap.org/wiki/Slippy_map_tilenames

// loeschen aller leeren tiles
// die sind nur wenige 100 Bytes groß
function TForm1.osm_CleanUp:integer;
var
    search     : TSearchRec;
    filename   : string;
begin
  result := 0;
  if FindFirst(pfad+'osm\*.png', faAnyFile, search)=0 then begin
    filename :=pfad+'osm\'+search.name ;
    if search.size<1024 then DeleteFile(filename);
    while FindNext(search) = 0 do begin
      filename :=pfad+'osm\'+search.name ;
      if search.size<1024 then DeleteFile(filename);
    end;
  end;
  FindClose(search);
end;  //osm_CleanUp


// coordinates to tile numbers
// n = 2 ^ zoom
// xtile = ((lon_deg + 180) / 360) * n
// ytile = (1 - (log(tan(lat_rad) + sec(lat_rad)) / ?)) / 2 * n

// Koordinaten = ecke unten links

// Abmessungen eines tile bei Zoom
// zoom  0 :    1 x   1 tile     360     x 180
// zoom  1 :    2 x   2 tiles    180     x  90
// zoom  2 :    4 x   4 tiles     90     x  45
// zoom  3 :    8 x   8 tiles     45     x  22.5
// zoom  4 :   16 x  16 tiles     22.5   x  11.25
// zoom  5 :   32 x  32 tiles     11.25  x  5.625
// zoom  6 :   64 x  64 tiles     11.25  x  5.625
// zoom  7 :  128 x 128 tiles      5.625 x  2.8125
// zoom  8 :  256 x 256 tiles     2.8125 x  1.40625
// zoom  9 :  512 x 512 tiles     2.8125 x  0.7031
// zoom 10 : 1024 x1024 tiles     0.7031 x  0.3515

// zoom 14 :  16k x 16k tiles

// fuer diese Koordinaten und diesen zoom die richtige Kachel finden und
// auf osm.BM projezieren
function TForm1.osm_koord2tile(lat, long : real; zoom : integer): Tosmtile;

  // Farbe aufhellen
  function bleach(dunkel:Tcolor):Tcolor;
  var r,b,g:integer;
  begin
    result := dunkel;
    if fullcolor1.checked then result:= dunkel else
    if pale1.checked then result:= ((dunkel and $FEFEFE) shr 1) or $808080 else
    if pale2.checked then result:= ((dunkel and $FCFCFC) shr 2) or $C0C0C0 else
    if gray1.checked then begin
      r := (dunkel and $FF0000) shr 16;
      g := (dunkel and $00FF00) shr  8;
      b :=  dunkel and $0000FF;
      g:=((r+g+b) div 8) +159;     // das Grau ausbleichen
      result:= g * $010101;
    end;
  end;

var
  lat_rad, lon_deg, n: Real;
  olp,urp   : TPoint;
  rechteck  : TRect;
  punkt     : Tkoordinaten;
  db, dl    : real;
  z, s      : integer;
  farbe     : Tcolor;
  x1, y1    : integer;
  x2, y2    : integer;
  xx, yy    : integer;
  deg       : integer;
  JPG       : TJpegImage;
begin
  result.ok := false;

  // in polnähe gibt es noch Probleme
  if abs(lat/rad) >85 then exit;

  // Rueckseite der Erde ist unsichtbar
  deg := round((ZentB-lat)/rad);
  if abs(deg)>90 then exit;
  deg := round((ZentL-long)/rad);
  if abs(deg)>90 then exit;

  // laenge -180 ... +180   breite -90 ... +90
  if (lat  >  (pi/2)) then lat  :=  pi - lat;    // Breite >  90
  if (lat  < -(pi/2)) then lat  := -pi - lat;    // Breite < -90
  if (long >  pi)     then long := long -2*pi;   // laenge > 180
  if (long < -pi)     then long := long +2*pi;   // laenge < 180

  lon_deg := long/rad;
  lat_rad := lat;
  n := Power(2, zoom);
  result.x := Trunc(((lon_deg + 180) / 360) * n);

  // name des Tile ermitteln und bei bedarf downloaden
  result.y := Trunc((1 - (ln(Tan(lat_rad) + (1 /Cos(lat_rad))) / Pi)) / 2 * n);
  result.z := zoom;

  // http://wiki.openstreetmap.org/wiki/Mapquest#MapQuest-hosted_map_tiles
  //Tile URLs
  //The tile URLs are very similar to regular OSM tiles, with only the front of the URL being different.
  //OpenStreetMap tile URL 	    http://a.tile.openstreetmap.org/8/126/87.png
  //MapQuest tile URL 	            http://otile1.mqcdn.com/tiles/1.0.0/osm/8/126/87.png
  //MapQuest Open Aerial tile URL   http://oatile1.mqcdn.com/naip/15/5240/12661.jpg
  //Just replace the "http://a.tile.openstreetmap.org" bit with "http://otile1.mqcdn.com/tiles/1.0.0/osm".
  //Note: MapQuest has 4 subdomains set up, otile1 to otile4, all pointing to the same CDN.

  if OpenStreetMap1.checked then begin
    // PNG
    result.url  := osmserver + inttostr(result.z) +'/' + inttostr(result.x) + '/' + inttostr(result.y) + '.png';
    result.name := Pfad+'osm\'+inttostr(result.z) +'_' + inttostr(result.x) + '_' + inttostr(result.y) + '.png';
    if not fileexists(result.name) then begin
      if (not OsmCacheMiss) and (not Root) then Wait4osm1.ShowModal;    // beim ersten download warnen
      OsmCacheMiss := true;
      downloadfile(result.url, result.name);
    end;
    result.pic := Tpicture.create;
    if fileexists(result.name) then result.pic.loadfromfile(result.name) else fillscr(result.pic.bitmap, clred);
  end else begin

    // JPG
    if MapQuestOSM1.checked then begin
      result.url  := mpqserver +inttostr(result.z) +'/' + inttostr(result.x) + '/' + inttostr(result.y) + '.png';
      result.name := Pfad+'mpq\'+inttostr(result.z) +'_' + inttostr(result.x) + '_' + inttostr(result.y) + '.jpg';  // ist wirklich so
    end;
    if MapQuestAerial1.checked then begin
      result.url  := airserver +inttostr(result.z) +'/' + inttostr(result.x) + '/' + inttostr(result.y) + '.jpg';
      result.name := Pfad+'aer\'+inttostr(result.z) +'_' + inttostr(result.x) + '_' + inttostr(result.y) + '.jpg';
    end;
    if not fileexists(result.name) then downloadfile(result.url, result.name);
    result.pic := Tpicture.create;

    //vorhandenes Bild laden
    if fileexists(result.name) then begin
      JPG := TJPEGImage.Create;
      try
        //srtmtile := TBitmap.create;
        JPG.LoadFromFile(result.name);
        result.pic.bitmap.Assign(JPG);
      finally
        JPG.Free
      end;
    end else fillscr(result.pic.bitmap, clred);
  end;


  osm_tile2koord(result);     // kordinaten eines tile

  // nun mühsam alle Punkte des Tile auf das osm.BM übertragen
  db:=(result.kol.breite-result.kul.breite)/255;
  dl:=(result.kor.laenge-result.kol.laenge)/255;
  for z:=0 to 255 do for s:=0 to 255 do begin
    punkt.breite := result.kul.breite + z*db;
    punkt.laenge := result.kol.laenge + s*dl;
    sincos(punkt);
    movepunkt(punkt);
    Farbe :=bleach(result.pic.bitmap.canvas.pixels[s,255-z]);
    x1 := (image1.width  div 2)+punkt.x;
    y1 := (image1.height div 2)+punkt.y;

    punkt.breite := punkt.breite + db;
    punkt.laenge := punkt.laenge + dl;
    sincos(punkt);
    movepunkt(punkt);
    x2 := (image1.width  div 2)+punkt.x;
    y2 := (image1.height div 2)+punkt.y;

    for xx:=x1 to x2 do for yy:=y1 downto y2 do osm.BM.canvas.pixels[xx, yy ] := Farbe;
  end;

  result.ok:= true;
  
  // den Fortschritt anzeigen
  olp.x:=0;
  olp.y:=0;
  urp.x:=image1.Width;
  urp.y:=image1.Height;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  image1.canvas.StretchDraw(Rechteck, osm.BM);
  update;
end;  //osm_koord2tile


// kordinaten eines tile
// tile numbers to coordinates
// n = 2 ^ zoom
// lon_deg = xtile / n * 360.0 - 180.0
// lat_rad = arctan(sinh(PI * (1 - 2 * ytile / n)))
// lat_deg = lat_rad * 180.0 / PI
procedure TForm1.osm_tile2koord(var tile : Tosmtile);
var n        : real;
    lon_deg  : real;
    lon_rad  : real;
    lat_rad  : real;
begin
  n       := Power(2, tile.z);
  lon_deg := tile.x / n * 360.0 - 180.0;
  lon_rad := lon_deg * rad;
  lat_rad := arctan(sinh(pi * (1 - 2 * tile.y / n)));

  tile.kol.breite := lat_rad;
  tile.kol.laenge := lon_rad;
  sincos(tile.kol);

  tile.kur.breite := arctan(sinh(pi * (1 - 2 * (tile.y+1) / n)));
  tile.kur.laenge := tile.kol.laenge + 2*pi/n;
  sincos(tile.kur);

  tile.kor.breite := tile.kol.breite;
  tile.kor.laenge := tile.kur.laenge;
  sincos(tile.kor);

  tile.kul.breite := tile.kur.breite;
  tile.kul.laenge := tile.kol.laenge;
  sincos(tile.kul);

  sin_ZentB := sin(ZentB);
  cos_ZentB := cos(ZentB);

  movepunkt(tile.kul);
  movepunkt(tile.kor);
  movepunkt(tile.kur);
  movepunkt(tile.kol);

end;  //osm_tile2koord


// Bildmitte als Receiver-Standort uebernehmen
procedure TForm1.setReceiverLocation1Click(Sender: TObject);
begin
  ToolButton25Click(nil);
end;

//Sprung zum Receiver standort
procedure TForm1.gotoReceiverlocation1Click(Sender: TObject);
begin
  ToolButton26Click(nil);
end;


// Sprung zu Koordinaten
// die mitte der welt
procedure TForm1.N001Click(Sender: TObject);
begin
  ZentB := 0; ZentL:=0; osmOK:=false; srtmOK:=false; repaintPPI(true, true);
end;  //N001Click


// zum receiver in prag
procedure TForm1.gotoPrague1Click(Sender: TObject);
begin
  ZentB := 50.04755*rad; ZentL:=14.344917*rad; osmOK:=false; srtmOK:=false; repaintPPI(true, true);
end;


// zum ort gehen (koordinaten in Grad)
procedure TForm1.gotoOrt(lat, long :real);
begin
  if (abs(ZentB-lat*rad) + abs(ZentL - long*rad)) < 0.0001 then exit;
  ZentB := lat*rad; ZentL:=long*rad; osmOK:=false; srtmOK:=false; repaintPPI(true, true);
end;


procedure TForm1.NewReceiverPosition(ZentL,ZentB: real);
var k, kk : integer;
begin
  Receiver.Laenge := ZentL;
  Receiver.Breite := ZentB;
  sincos(Receiver);
  for k:=0 to 71 do for kk:=0 to 3 do begin
    rangering[k,kk].range := 0;
    rangering[k,kk].koord := Receiver;
    rangeringmin[k,kk].range := 200;
    rangeringmin[k,kk].koord := Receiver;
  end;
  hastomove := true;
end;


// hier ist der Receiver
procedure TForm1.ToolButton25Click(Sender: TObject);
begin
  if Application.MessageBox(
        'This will change the position of the ADS-B receiver !'+ chr($0D)+
        'Are you shure ?',
        'WARNING',  //kopfzeile
        MB_OKCANCEL + MB_DEFBUTTON1
        //+ MB_ICONWARNING            // gelbes warndreieck
        //+ MB_ICONHAND             // roter kreis mit kreuz
        + MB_ICONQUESTION         // blaues Fragezeichen
        + MB_APPLMODAL + MB_SETFOREGROUND 	) <> IDOK then exit;
  NewReceiverPosition(ZentL,ZentB);
  repaintPPI(true, true);
end;


// zum Receiver
procedure TForm1.ToolButton26Click(Sender: TObject);
begin
  ZentB := Receiver.Breite;
  ZentL := Receiver.Laenge;
//  if osm.ZentB = ZentB then osm.yoff:=0;  //radiant
//  if osm.ZentL = ZentL then osm.xoff:=0;  //radiant
//  if (osm.yoff<>0) or (osm.xoff <>0) then osmOK :=false else osmOK :=true;
  osmOK  :=false;
  srtmOK :=false;     
  repaintPPI(true, true);
end;


// Abmessungen eines tile bei Zoom
// zoom  0 :    1 x   1 tile     360     x 180
// zoom  1 :    2 x   2 tiles    180     x  90
// zoom  2 :    4 x   4 tiles     90     x  45
// zoom  3 :    8 x   8 tiles     45     x  22.5
// zoom  4 :   16 x  16 tiles     22.5   x  11.25
// zoom  5 :   32 x  32 tiles     11.25  x  5.625
// zoom  6 :   64 x  64 tiles     11.25  x  5.625
// zoom  7 :  128 x 128 tiles      5.625 x  2.8125
// zoom  8 :  256 x 256 tiles     2.8125 x  1.40625
// zoom  9 :  512 x 512 tiles     2.8125 x  0.7031
// zoom 10 : 1024 x1024 tiles     0.7031 x  0.3515
//
// laden von MapQuest oder OSM
procedure TForm1.loadOSM(zoom : integer);
var x, y      : integer;
    olp,urp   : TPoint;
    rechteck  : TRect;
    dL, dB    : real;
    n         : real;
    kachel    : Tosmtile;
    go        : Boolean;
begin
  if zoom=0 then begin
    // automatische Wahl des geeigneten Zoom-levels / Tilegröße
    zoom := 5;                         // kontinentale Übersicht
    if scale> 50    then zoom :=  6;           // 25 * 2
    if scale>100    then zoom :=  7;           // 25 * 4
    if scale>200    then zoom :=  8;           // 25 * 8
    if scale>400    then zoom :=  9;           // 25 * 16
    if scale>800    then zoom := 10;
    if scale>1600   then zoom := 11;
    if scale>3200   then zoom := 12;
    if scale>6400   then zoom := 13;     // ganz dicht drann
    if scale>12800  then zoom := 14;     // ganz dicht drann
    if scale>25600  then zoom := 15;     // ganz dicht drann
    if scale>51200  then zoom := 16;     // ganz dicht drann
    if scale>102400 then zoom := 17;     // ganz dicht drann
  end;

  clrscr(osm.BM);
  osm.bm.width := image1.width;
  osm.bm.height := image1.height;
  n   := power(2,zoom);
  dB := 180 / n;
  dL := 360 / n;

  // das sollte unbedingt in mehreren Threds erledigt werden.

  for x:=0 to 10 do begin              //nach rechts
    go :=false;
    for y := 0 to 10 do begin   //nach oben
      kachel := osm_koord2tile(ZentB+(y*dB*rad), ZentL+(x*dL*rad), zoom);
      if kachel.ok then begin
        if (kachel.kor.x <  (image1.Width div 2))  or   (kachel.kur.x <  (image1.Width div 2))  then go := true;
        if (kachel.kol.y < -(image1.Height div 2)) and  (kachel.kor.y < -(image1.Height div 2)) then break;
      end else break;
    end;
    for y := -1 downto -10 do begin  // nach unten
      kachel := osm_koord2tile(ZentB+(y*dB*rad), ZentL+(x*dL*rad), zoom);
      if kachel.ok then begin
        if (kachel.kor.x < (image1.Width div 2))  or  (kachel.kur.x < (image1.Width div 2))  then go := true;
        if (kachel.kul.y > (image1.Height div 2)) and (kachel.kur.y > (image1.Height div 2)) then break;
      end else break;
    end;
    if not go then break;
  end;

  for x:=-1 downto -10 do begin              //nach links
    go :=false;
    for y := 0 to 10 do begin   //nach oben
      kachel := osm_koord2tile(ZentB+(y*dB*rad), ZentL+(x*dL*rad), zoom);
      if kachel.ok then begin
        if (kachel.kol.x > -(image1.Width div 2))  or   (kachel.kul.x > -(image1.Width div 2))  then go := true;
        if (kachel.kol.y < -(image1.Height div 2)) and  (kachel.kor.y < -(image1.Height div 2)) then break;
      end else break;
    end;
    for y := -1 downto -10 do begin  // nach unten
      kachel := osm_koord2tile(ZentB+(y*dB*rad), ZentL+(x*dL*rad), zoom);
      if kachel.ok then begin      
        if (kachel.kol.x > -(image1.Width div 2))  or  (kachel.kul.x > -(image1.Width div 2))  then go := true;
        if (kachel.kul.y >  (image1.Height div 2)) and (kachel.kur.y >  (image1.Height div 2)) then break;
      end else break;
    end;
    if not go then break;
  end;

  osm.ZentB := ZentB;
  osm.ZentL := ZentL;
  osm.Scale := Scale;

  osm.xoff := 0;
  osm.yoff := 0;

  olp.x:=0;
  olp.y:=0;
  urp.x:=image1.Width;
  urp.y:=image1.Height;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  image1.canvas.StretchDraw(Rechteck, osm.BM);
  osmOK := true;
  OSMbackground1.checked  := true;
  SRTMbackground1.checked := false;
  repaintPPI(false, true);
end;  // loadOSM


// erzeugen einer SRTM oder OSM-Karte
procedure TForm1.OSMmap1Click(Sender: TObject);
var
  Save_Cursor:TCursor;
begin
  OsmCacheMiss := false;

  ToolButton7.down := true;
  ToolButton7.imageindex := 13;
  Save_Cursor   := Screen.Cursor;
  Screen.Cursor := crHourglass;    { Cursor als Sanduhr }

  try
    if SRTM1.checked then begin
      progressbar1.position := 0;
      progressbar1.visible  := true;
      MakeSRTM;
    end else loadOSM(0);
  finally
    progressbar1.visible := false;   // falls SRTM
    Screen.Cursor := Save_Cursor;  { Alten Zustand wiederherstellen }
  end;
  ToolButton7.imageindex := 12;
  ToolButton7.down := false;
end; //OSMmap1Click


// speichert osm-karte mit koordinaten und scale ...
procedure TForm1.save1Click(Sender: TObject);
begin
  saveposition(9);
end;

// speichere position und OSM als default
procedure TForm1.savedefClick(Sender: TObject);
begin
  saveposition(0);
end;


procedure TForm1.saveposition(PosNumber: integer);
var sfile    : file of Tosm;
    rfile    : file of TRangePoint;
    filename : string;
    jp       : TJpegImage;
    jpfile   : string;
    k        : integer;
begin
  filename := Pfad+'save\default.pos';
  case PosNumber of
    0: filename := Pfad+'save\default.pos';
    1: filename := Pfad+'save\lastpos.pos';
    9: begin
         SaveDialog1.Filter       := 'position (*.pos)|*.POS';
         SaveDialog1.DefaultExt   := 'pos';
         SaveDialog1.InitialDir   := Pfad+'save\';
         if not SaveDialog1.Execute then exit;
         filename := SaveDialog1.FileName;
       end;
  end;

  // osm vorbereiten
  // falls keine osm-karte geladen wurde
  if (osm.scale=0) or (not osmOK) then begin
    osm.ZentB := ZentB;
    osm.ZentL := ZentL;
    osm.Scale := Scale;
    osm.OK    := false;
  end else osm.OK := true;
  osm.pos := TrackBar1.position;
  osm.formtop    := Form1.top;
  osm.formleft   := Form1.left;
  osm.formwidth  := Form1.Width;
  osm.formheight := Form1.Height;

  // srtm vorbereiten
  // falls keine srtm-karte geladen wurde
  if (srtm.scale=0) or (not srtmOK) then begin
    srtm.ZentB := ZentB;
    srtm.ZentL := ZentL;
    srtm.Scale := Scale;
    srtm.OK    := false;
  end else srtm.OK := true;
  srtm.pos := TrackBar1.position;
  srtm.formtop    := Form1.top;
  srtm.formleft   := Form1.left;
  srtm.formwidth  := Form1.Width;
  srtm.formheight := Form1.Height;

  // savefile anlegen
  assignfile(sfile,filename);
  {$I-}
  rewrite(sfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('can not create '+filename, mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###save error: '+filename);
    exit;
  end;
  write(sfile, osm, srtm);
  closefile(sfile);

  //osm speichern
  jpfile:='osm.jpg';
  jpfile := copy(filename, 1, length(filename)-4) +'.jpg';
  jp := TJpegImage.Create;
  try
    with jp do
    begin
      Assign(osm.BM);
      CompressionQuality := 80;
      SaveToFile(jpfile)
    end;
  finally
    jp.Free;
  end;

  //srtm speichern
  jpfile:='srtm.jpg';
  jpfile := copy(filename, 1, length(filename)-4) +'_srtm.jpg';
  jp := TJpegImage.Create;
  try
    with jp do
    begin
      Assign(srtm.BM);
      CompressionQuality := 80;
      SaveToFile(jpfile)
    end;
  finally
    jp.Free;
  end;

  //maxrange speichern
  filename := copy(filename, 1, length(filename)-4) +'.rri';
  assignfile(rfile,filename);
  {$I-}
  rewrite(rfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('can not create '+filename, mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###save error: '+filename);
    exit;
  end;
  for k:=0 to 71 do write(rfile, rangering[k,0],rangering[k,1],rangering[k,2],rangering[k,3]);
  closefile(rfile);

  //minrange speichern
  filename := copy(filename, 1, length(filename)-4) +'.rrj';
  assignfile(rfile,filename);
  {$I-}
  rewrite(rfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('can not create '+filename, mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###save error: '+filename);
    exit;
  end;
  for k:=0 to 71 do write(rfile, rangeringmin[k,0],rangeringmin[k,1],rangeringmin[k,2],rangeringmin[k,3]);
  closefile(rfile);

end; //saveposition


//laden einer position
procedure TForm1.load1Click(Sender: TObject);
begin
  loadposition(9);
end;  //load1Click


//laden der default-position
procedure TForm1.loaddefault1Click(Sender: TObject);
begin
  loadposition(0);
end;


//laden einer position
procedure TForm1.loadposition(PosNumber: integer);
var sfile    : file of Tosm;
    rfile    : file of TRangePoint;
    filename : string;
    JPG      : TJpegImage;
    jpfile   : string;
    k        : integer;
    sizeoffile : integer;
begin
  case PosNumber of
    0: filename := Pfad+'save\default.pos';
    1: filename := Pfad+'save\lastpos.pos'; 
    9: begin
         OpenDialog1.Filter     := 'position (*.pos)|*.POS';
         OpenDialog1.DefaultExt := 'pos';
         OpenDialog1.InitialDir :=  Pfad+'save\';
         if not OpenDialog1.Execute then exit;          { Dialog zum Dateiöffnen anzeigen }
         filename := OpenDialog1.FileName;
       end;
  end;

  assignfile(sfile,filename);
  {$I-}
  reset(sfile);
  {$I+}
  if IOResult <> 0 then begin
    if PosNumber=0 then exit;
    MessageDlg('can not open '+filename, mtWarning	,[mbOk], 0);
    Memo1.Lines.Add('###load error: '+filename);
    exit;
  end;

  sizeoffile := FileSize(sfile);

  //** osm *****************************************************
  osm.BM.Free;
  read(sfile, osm);      //lesen
  osm.BM        := TBitmap.Create;
  osm.BM.width  := image1.width;
  osm.BM.height := image1.height;
  Form1.top     := osm.formtop;
  Form1.left    := osm.formleft;
  Form1.Width   := osm.formwidth;
  Form1.Height  := osm.formheight;
  osmOK         := osm.OK;
  ZentB         := osm.ZentB;
  ZentL         := osm.ZentL;
  sin_ZentB     := sin(ZentB);
  cos_ZentB     := cos(ZentB);
  NewReceiverPosition(ZentL,ZentB);
  hastomove     := true;

  jpfile := copy(filename, 1, length(filename)-4) +'.jpg';
  //vorhandenes Bild laden
  if fileexists(jpfile) then begin
    JPG := TJPEGImage.Create;
    try
      JPG.LoadFromFile(jpfile);
      osm.BM.Assign(JPG);
    finally
      JPG.Free
    end;
  end;

  TrackBar1.position := osm.pos;

//** srtm *****************************************************
  if sizeoffile>1 then begin
    srtm.BM.Free;
    read(sfile, srtm);      //lesen
    srtm.BM        := TBitmap.Create;
    srtm.BM.width  := image1.width;
    srtm.BM.height := image1.height;
    srtmOK         := srtm.OK;
    hastomove      := true;

    jpfile := copy(filename, 1, length(filename)-4) +'_srtm.jpg';
    //vorhandenes Bild laden
    if fileexists(jpfile) then begin
      JPG := TJPEGImage.Create;
      try
        JPG.LoadFromFile(jpfile);
        srtm.BM.Assign(JPG);
      finally
        JPG.Free
      end;
    end;
  end;

  closefile(sfile);
  repaintPPI(true, true);

//** range *****************************************************
  //maxrange
  filename := copy(filename, 1, length(filename)-4) +'.rri';
  assignfile(rfile,filename);
  {$I-}
  reset(rfile);
  {$I+}
  if IOResult <> 0 then exit;
  for k:=0 to 71 do read(rfile, rangering[k,0], rangering[k,1], rangering[k,2], rangering[k,3]);
  closefile(rfile);

  //minrage
  filename := copy(filename, 1, length(filename)-4) +'.rrj';
  assignfile(rfile,filename);
  {$I-}
  reset(rfile);
  {$I+}
  if IOResult <> 0 then exit;
  for k:=0 to 71 do read(rfile, rangeringmin[k,0], rangeringmin[k,1], rangeringmin[k,2], rangeringmin[k,3]);
  closefile(rfile);

end;    // loadposition



//***** TOOLBAR  **********************************************************//

// zoom in and out
procedure TForm1.ToolButton5Click(Sender: TObject);
begin
  Trackbar1.Position := Trackbar1.Position + 1;
end;

procedure TForm1.ToolButton6Click(Sender: TObject);
begin
  Trackbar1.Position := Trackbar1.Position - 1;
end;

// bild verschieben mit pfeil-buttons
//nach unten
procedure TForm1.ToolButton1Click(Sender: TObject);
begin
  osm.yoff := osm.yoff + round(5 * osm.Scale/Scale);
  ZentB := ZentB + 5*57/Scale/60*rad;
  sin_ZentB := sin(ZentB);
  cos_ZentB := cos(ZentB);
  hastomove:= true;
  repaintPPI(true, true);
end;

//nach links
procedure TForm1.ToolButton3Click(Sender: TObject);
begin
  osm.xoff  := osm.xoff  - round(5 * osm.Scale/Scale); ;
  srtm.xoff := srtm.xoff - round(5 * srtm.Scale/Scale); ;
  ZentL := ZentL + (5)*57/Scale/60*rad  / cos_ZentB;         // in radiant
  hastomove:= true;
  repaintPPI(true, true);
end;

//nach rechts
procedure TForm1.ToolButton4Click(Sender: TObject);
begin
  osm.xoff  := osm.xoff  + round(5 * osm.Scale/Scale); ;
  srtm.xoff := srtm.xoff + round(5 * srtm.Scale/Scale); ;
  ZentL := ZentL - (5)*57/Scale/60*rad  / cos_ZentB;         // in radiant
  hastomove:= true;
  repaintPPI(true, true);
end;

//nach oben
procedure TForm1.ToolButton2Click(Sender: TObject);
begin
  osm.yoff  := osm.yoff  - round(5 * osm.Scale/Scale); ;
  srtm.yoff := srtm.yoff - round(5 * srtm.Scale/Scale); ;
  ZentB := ZentB - 5*57/Scale/60*rad;
  sin_ZentB := sin(ZentB);
  cos_ZentB := cos(ZentB);
  hastomove:= true;
  repaintPPI(true, true);
end;


procedure TForm1.cleanuposm1Click(Sender: TObject);
var Save_Cursor : TCursor;
begin
  Save_Cursor   := Screen.Cursor;
  Screen.Cursor := crHourglass;    { Cursor als Sanduhr }
  try
    osm_CleanUp;
  finally
    Screen.Cursor := Save_Cursor;  { Alten Zustand wiederherstellen }
  end;
end;

procedure TForm1.cleanupsrtm1Click(Sender: TObject);
var Save_Cursor : TCursor;
begin
  Save_Cursor   := Screen.Cursor;
  Screen.Cursor := crHourglass;    { Cursor als Sanduhr }
  try
    srtm_CleanUp;
  finally
    Screen.Cursor := Save_Cursor;  { Alten Zustand wiederherstellen }
  end;
end;

procedure TForm1.about1Click(Sender: TObject);
begin
  Application.CreateForm(TForm2,Form2);
end;


// im moment unnötige funktion zum einstellen des dc der pwm
// würde durch die AGC sofort wieder verstellt werden
procedure TForm1.decoderSetPwm(dc:integer);
begin
  if not ComPort1.Connected then exit;
  send_buf[0]:=SYS_PWM2;    // PWM2
  send_buf[1]:=2;           // DC
  send_buf[2]:=dc mod $100; // low
  send_buf[3]:=dc div $100; // high
  Sende_Empfange(4, 16);
end;


// adsbPIC-Decoder
// Abstand zwischen pegel und referenz einstellen
// hoechster Pegel des AD8313 ist etwa 1,7 V bei 0dBm
// fuer near-tarhets ein threshould von 1,4 .. 1,5 V scheint brauchbar
// das waere ein pwm von 286 .. 307
procedure TForm1.setAgcOffset(offset : byte);
begin
  if not ComPort1.Connected then exit;
  if dontchangeoffset then exit;
  if offset=255 then offset := agc_offset;   // alten Wert beibehalten
  send_buf[0]:=SET_OFFSET;
  send_buf[1]:=2;           // unbenutzt
  send_buf[2]:=1;           // agc on
  if closetargets1.checked then
    send_buf[2] := send_buf[2] or $04; // near_far_mode on
  if AGCon then
    send_buf[2] := send_buf[2] or $10; // nicht im EEPROM speichern
  send_buf[3]:=offset;      // ofset in mV
  if offset=0 then
    send_buf[2]:=0;         // agc off
  Sende_Empfange(4, 2);
  agc_offset := offset;
end;


// auslesen den AGC-Offsets aus dem decoder
// geht erst ab firmware version 3
// AGC-DISABLED geht ab Fw 7
function TForm1.readAgcOffset:byte;
begin
  result := 0;
  if not ComPort1.Connected then exit;
  send_buf[0] := READ_OFFSET;
  Sende_Empfange(1, 16);
  agc_mode   := receive_buf[2];           // agc_mode
  agc_offset := receive_buf[3];           // agc_offset
  result     := agc_offset;

  dontchangeoffset := true;
  N100mV1.checked  := false;
  case  agc_offset of
    30..45:   N40mV1.checked  := true;
    46..55:   N50mV1.checked  := true;
    56..65:   N60mV1.checked  := true;
    66..75:   N70mV1.checked  := true;
    76..85:   N80mV1.checked  := true;
    86..95:   N90mV1.checked  := true;
    96..105:  N100mV1.checked := true;
    106..115: N110mV1.checked := true;
    116..125: N120mV1.checked := true;
    126..135: N130mV1.checked := true;
    136..145: N140mV1.checked := true;
    146..155: N150mV1.checked := true;
    156..165: N160mV1.checked := true;
    166..175: N170mV1.checked := true;
    176..185: N180mV1.checked := true;
    186..195: N190mV1.checked := true;
    196..210: N200mV1.checked := true;
    211..255: N250mV1.checked := true;
  end;

  if agc_mode=0 then off1.checked := true;

  dontchangeoffset := false;
  writelnlog( 'Decoder: AGC-Offset = '+inttostr(agc_offset)+' mV');
  if  agc_mode=0 then begin
    writelnlog('Decoder: AGC-DISABLED !');
    memo1.lines.add('Decoder: AGC-DISABLED !');
  end;
end; // readAgcOffset


// auslesen der RS232-Einstellungen
// geht erst ab firmware version 7
procedure TForm1.readRS232mode;
var speed, polaritaet : byte;
begin
  if not ComPort1.Connected then exit;
  send_buf[0] := SYS_EEPROM;
  send_buf[1] := 5;           //block lesen
  send_buf[2] := 1;           //startadresse
  send_buf[3] := 3;           //bytezahl

  Sende_Empfange(4, 16);
  speed      := receive_buf[4];           // speed
  polaritaet := receive_buf[5];           // polaritaet
  if speed>2 then speed:=0;
  if polaritaet>1 then polaritaet:=0;

  dontchangers232 := true;
  N115kbit1.checked := true;
  if speed=1 then begin
    N1Mbit1.checked := true;
    writelnlog('Decoder: RS232-bitrate = 1 MBit');
  end;
  if speed=2 then begin
    N19kbit1.checked := true;
    writelnlog('Decoder: RS232-bitrate = 19.2 KBit');
  end;

  withdriver1.checked := true;
  if polaritaet=1 then begin
    withoutdriver1.checked := true;
    writelnlog('Decoder: RS232-polarity : no drivers');
  end;
  dontchangers232 := false;
end; // readRS232mode



// adsbPIC-Decoder reset
procedure TForm1.decoderReset;
begin
  if not ComPort1.Connected then ComPort1.Open;
  send_buf[0]:= RESET_DEVICE;  //bRESET_DEVICE;
  Sende_Empfange(1,0);
  NullZeitmarke := 0;
  memo1.lines.add('');
  Memo1.lines.add(' ');
  Memo1.lines.add('finished');
  if ComPort1.Connected then ComPort1.close;
end;  //RESET für USB-Device


//*** ABSTAND von REFERENZ zu PEGEL einstellen**********************************

procedure TForm1.N40mV1Click(Sender: TObject);
begin
  N40mV1.checked := true;
  setAgcOffset(40);
end;

procedure TForm1.N50mV1Click(Sender: TObject);
begin
  N50mV1.checked := true;
  setAgcOffset(50);
end;

procedure TForm1.N60mV1Click(Sender: TObject);
begin
  N60mV1.checked := true;
  setAgcOffset(60);
end;

procedure TForm1.N70mV1Click(Sender: TObject);
begin
  N70mV1.checked := true;
  setAgcOffset(70);
end;


procedure TForm1.N80mV1Click(Sender: TObject);
begin
  N80mV1.checked := true;
  setAgcOffset(80);
end;

procedure TForm1.N90mV1Click(Sender: TObject);
begin
  N90mV1.checked := true;
  setAgcOffset(90);
end;

procedure TForm1.N100mV1Click(Sender: TObject);
begin
  N100mV1.checked := true;
  setAgcOffset(100);
end;

procedure TForm1.N110mV1Click(Sender: TObject);
begin
  N110mV1.checked := true;
  setAgcOffset(110);
end;

procedure TForm1.N120mV1Click(Sender: TObject);
begin
  N120mV1.checked := true;
  setAgcOffset(120);
end;

procedure TForm1.N130mV1Click(Sender: TObject);
begin
  N130mV1.checked := true;
  setAgcOffset(130);
end;

procedure TForm1.N140mV1Click(Sender: TObject);
begin
  N140mV1.checked := true;
  setAgcOffset(140);
end;

procedure TForm1.N150mV1Click(Sender: TObject);
begin
  N150mV1.checked := true;
  setAgcOffset(150);
end;

procedure TForm1.N160mV1Click(Sender: TObject);
begin
  N160mV1.checked := true;
  setAgcOffset(160);
end;

procedure TForm1.N170mV1Click(Sender: TObject);
begin
  N170mV1.checked := true;
  setAgcOffset(170);
end;

procedure TForm1.N180mV1Click(Sender: TObject);
begin
  N180mV1.checked := true;
  setAgcOffset(180);
end;

procedure TForm1.N190mV1Click(Sender: TObject);
begin
  N100mV1.checked := true;
  setAgcOffset(100);
end;

procedure TForm1.N200mV1Click(Sender: TObject);
begin
  N200mV1.checked := true;
  setAgcOffset(200);
end;

procedure TForm1.N250mV1Click(Sender: TObject);
begin
  N250mV1.checked := true;
  setAgcOffset(250);
end;

procedure TForm1.off1Click(Sender: TObject);
begin
  off1.checked := true;
  setAgcOffset(0);
end;

// automatische offsetkorrektur
procedure TForm1.automatic1Click(Sender: TObject);
begin
  automatic1.checked := not automatic1.checked;
  AGCon := automatic1.checked;
  if AGCon then begin
    agc_update     := 0;
    AgcCounter     := 0;                 // Zyklenzähler fuer AGC
    AgcResult      := 100;               // AGC-verbesserung
    agc_mitte      := agc_offset;
    AgcUpCounter   := 0;
    AgcDownCounter := 0;
  end;
end;


// optimierung fue nahe targets
// dafuer wird gelegentlich der Threshould fuer 1,3 sekunden angehoben
procedure TForm1.closetargets1Click(Sender: TObject);
begin
  closetargets1.checked := not closetargets1.checked;
  setAgcOffset(255); // an aus bei gleichem offset
end;

procedure TForm1.binarydataformat1Click(Sender: TObject);
begin
  binarydataformat1.checked := not binarydataformat1.checked;
  UseBinFormat := binarydataformat1.checked;
  if not ComPort1.Connected then exit;
  DecoderPause;
  DecoderRestart;
end;


//erlaube Zeitmarken im Decoder
procedure TForm1.usetimetag1Click(Sender: TObject);
begin
  usetimetag1.checked := not usetimetag1.checked;
  UseTimeTag := usetimetag1.checked;
  if usetimetag1.checked then usetimetag1.imageindex:=34
                         else usetimetag1.imageindex:=35;
  if usetimetag1.checked then usetimetag1.caption:='disable time tag'
                         else usetimetag1.caption:='enable time tag';
  if not ComPort1.Connected then exit;
  DecoderPause;
  DecoderRestart;
end;


// erlaube das Senden von Zustandsinformationen
// als DF23 mit AA=000000
procedure TForm1.enableheartbeat1Click(Sender: TObject);
begin
  enableheartbeat1.checked := not enableheartbeat1.checked;
  UseHeartBeat := enableheartbeat1.checked;
  if enableheartbeat1.checked then enableheartbeat1.imageindex:=58
                              else enableheartbeat1.imageindex:=59;
  if enableheartbeat1.checked then enableheartbeat1.caption:='disable heart beat'
                              else enableheartbeat1.caption:='enable heart beat';
  if not ComPort1.Connected then exit;
  DecoderPause;
  DecoderRestart;
end;


//connect to decoder at program start
procedure TForm1.ConnectatStart1Click(Sender: TObject);
begin
  ConnectatStart1.checked := not ConnectatStart1.checked;
  AutoConnect := ConnectatStart1.checked;
end;


procedure TForm1.connect1Click(Sender: TObject);
begin
  connect;
end;

procedure TForm1.disconnect1Click(Sender: TObject);
begin
  disconnect;
end;


//RESET für USB-Device
procedure TForm1.Reset1Click(Sender: TObject);
begin
  decoderReset;
end;


// Bootloader aktivieren
procedure TForm1.activateBootloader1Click(Sender: TObject);
var
  Save_Cursor:TCursor;
begin
  if not ComPort1.Connected then exit;
  if Application.MessageBox(
        'This will activate the Bootloader and disable the decoder-function !'+ chr($0D)+
        'To use the decoder again, you will have to flash new firmware ! '+ chr($0D)+ chr($0D)+
        'Are you really shure ?',
        'WARNING   ---   WARNING',  //kopfzeile
        MB_OKCANCEL + MB_DEFBUTTON1
        + MB_ICONWARNING            // gelbes warndreieck
        //+ MB_ICONHAND             // roter kreis mit kreuz
        //+ MB_ICONQUESTION         // blaues Fragezeichen
        + MB_APPLMODAL + MB_SETFOREGROUND 	) <> IDOK then exit;

  Save_Cursor   := Screen.Cursor;
  Screen.Cursor := crHourglass;
  DecoderPause;

  // EEPROM-Zelle 0xFE mit FF beschreiben
  send_buf[0] := SYS_EEPROM;
  send_buf[1] := 2;                    // schreiben
  send_buf[2] := $FE;                  // Adresse
  send_buf[3] := $FF;                  // Datenbyte
  Sende_Empfange(4, 16);

  // RESET
  send_buf[0]:= RESET_DEVICE;
  Sende_Empfange(1,0);
  sleep(3000);

  Screen.Cursor := Save_Cursor;
  ComPort1.Close;
end;  // Bootloader aktivieren


// einstellen der RS232-Geschwindigkeit
procedure TForm1.DecoderSetRS232Speed(speed:byte);
begin
  if dontchangers232 then exit;
  if not ComPort1.Connected then exit;
  DecoderPause;

  send_buf[0]:=SYS_EEPROM;   // EEPROM
  send_buf[1]:=2;            // byte schreiben
  send_buf[2]:=1;            // adresse
  send_buf[3]:=speed;        // daten
  Sende_Empfange(4, 16);

  readRS232mode;    // Test
  DecoderRestart;
end;  //DecoderSetRS232Speed

procedure TForm1.N115kbit1Click(Sender: TObject);
begin
  DecoderSetRS232Speed(0);
end;

procedure TForm1.N1Mbit1Click(Sender: TObject);
begin
  DecoderSetRS232Speed(1);
end;

procedure TForm1.N19kbit1Click(Sender: TObject);
begin
  DecoderSetRS232Speed(2);
end;


// einstellen der RS232-Polaritaet
procedure TForm1.DecoderSetRS232Pol(polaritaet:byte);
begin
  if dontchangers232 then exit;
  if not ComPort1.Connected then exit;
  DecoderPause;

  send_buf[0]:=SYS_EEPROM;   // EEPROM
  send_buf[1]:=2;            // byte schreiben
  send_buf[2]:=2;            // adresse
  send_buf[3]:=polaritaet;   // daten
  Sende_Empfange(4, 16);

  readRS232mode;    // Test
  DecoderRestart;
end;

procedure TForm1.withdriver1Click(Sender: TObject);
begin
  DecoderSetRS232Pol(0);
end;

procedure TForm1.withoutdriver1Click(Sender: TObject);
begin
  DecoderSetRS232Pol(1);
end;

procedure TForm1.TestPWM501Click(Sender: TObject);
begin
  decoderSetPwm(511);
end;

procedure TForm1.TestPWM51Click(Sender: TObject);
begin
  decoderSetPwm(51);
end;

procedure TForm1.ButtonStartClick(Sender: TObject);
begin
  Timer1.enabled := true;
end;

procedure TForm1.ButtonStopClick(Sender: TObject);
begin
  Timer1.enabled := false;
end;


// Decodertyp erkennen und aktivieren
// der Soll-Typ steht ja in DecoderNr1 drinn
//    decoder_adsbPIC   = 1;
//    decoder_rxcontrol = 2;
//    decoder_beast     = 3;
procedure TForm1.GetDecoderType;
var
  asciistr : string;
  rxstr    : string;
  nrrx     : integer;
  hard, soft : byte;
begin
  if not ComPort1.Connected then exit;
  Decoder := 0;
  ComPort1.ClearBuffer(true, true);

  if DecoderNr1=decoder_rxcontrol then begin
    // rxcontrol gibt sich auf '?' zu erkennen
    asciistr := '?';
    Comport1.WriteStr(asciistr);
    //empfangen über comport
    nrrx:=Comport1.ReadStr(rxstr, 50);
    if (nrrx>5) and (copy(rxstr, 1, 5) = '=ADSB') then begin
      // ja, das ist ein rxcontrol
      memo1.lines.add('Decoder: rxcontrol');
      memo1.lines.add(rxstr);
      DecoderDependend(decoder_rxcontrol);
      asciistr := '3';         // *.....;
      Comport1.WriteStr(asciistr);
      writelnlog('Decoder: '+ rxstr+' detected and activated');
      exit;
    end;
  end;   //decoder_rxcontrol

  if DecoderNr1=decoder_adsbPIC then begin
    // wenn es kein rxcontrol war, dann mal auf adsbPIC testen
    ComPort1.ClearBuffer(true, true);
    send_buf[0] := READ_VERSION;      // wer bist denn du?
    Sende_Empfange(1, 16);
    hard := receive_buf[3];           // Gerät
    soft := receive_buf[2];           // Version
    //  0-Brenner8; 1=Bootloader 2=USB4A 3=Brenner9 4= adsbPIC
    // nur adsbPIC und USB4A benutzen CDC-Treiber
    if hard=4 then begin
      //ja, das ist ein adsbPIC
      memo1.lines.add('Decoder: adsbPIC Version: '+inttostr(soft));
      writelnlog('Decoder: adsbPIC Version: '+inttostr(soft)+' detected and activated');
      DecoderDependend(decoder_adsbPIC);
      N12MHz1Click(nil);
      Label3Click(nil);
      Label6Click(nil);
      if soft>=3 then readAgcOffset;                // spannungspegel auslesen
      if (soft>=7) and (not GNS58901.checked) then begin
        RS232speed1.visible    := true;             // RS232-speed aendern is moeglich
        RS232polarity1.visible := true;             // RS232-polaritaet aendern is moeglich
        readRS232mode;
      end;
      if soft>=9 then begin
        enableheartbeat1.visible  := true;
        ToolButton24.visible      := true;
        decoderstatus1.visible    := true;
        closetargets1.visible     := true;
      end;
      if soft>=10 then begin
        binarydataformat1.visible := true;
        automatic1.visible        := true;
      end;
      if soft>=11 then begin
        I2C1.visible              := true;
        if not donotuse1.checked then i2c2tuner;
      end;
      if soft>=12 then
        if not binarydataformat1.checked then binarydataformat1Click(nil);

      //gns oder adsbpic
      if adsbPIC2.checked then adsbPIC1.Caption:='adsbPIC'
                          else adsbPIC1.Caption:='GNS5890';
      activateBootloader1.visible := adsbPIC2.checked;

      RadioGroup1.itemindex:=2;                     // und datenfluss einschalten (alle frames)
      exit;
    end;
  end; //decoder_adsbPIC

  if DecoderNr1=decoder_beast then begin
    memo1.lines.add('### maybe a BEAST !');
    writelnlog('Decoder: Beast');
    DecoderDependend(decoder_beast);
    N12MHz1Click(nil);
    // initialisierungsstring senden
    // DIP 	Function 	       OFF, open = lower case         ON, closed = upper case characters
    // 1,2 	Baudrate 	       not supported                  not supported
    // 3 	Output Format 	       "c" 	                      "C"
    // 4 	DF-11/17 only filter   "d" 	                      "D"
    // 5 	MLAT info 	       "e" 	                      "E"
    // 6 	CRC check disable      "f" 	                      "F"
    // 7 	DF0/4/5 masking filter "g" 	                      "G"
    // 8 	RTS handshake 	       "h" 	                      "H"
    // 9 	FEC disable 	       "i" 	                      "I"
    // 10 	Mode-A/C enable        "j" 	                      "J"
    //
    // switches 1, 2, 5, 8 ON = cdEfgHij
    //BeastControl('cdEfgHij');
    BeastControl('Hij');
    BeastControlC;
    BeastControlD;
    BeastControlE;
    BeastControlF;
    BeastControlG;
    ComPort1.ClearBuffer(true, true);
    if enabletimetag1.checked then BeastControl('E') else BeastControl('e');
    RadioGroup1.itemindex:=2;
    exit;
  end;

  // da ist kein decoder
  memo1.lines.add('### No Decoder detected !');
  ComPort1.Close;
end;  //GetDecoderType


// je nach Decoder muss einiges abgeschaltet werden
procedure TForm1.DecoderDependend(x : integer);
begin
  Decoder := x;
  adsbPIC1.visible    := (Decoder = decoder_adsbPIC);
  Label3.visible      := (Decoder = decoder_adsbPIC);    //U-signal =
  Label6.visible      := (Decoder = decoder_adsbPIC);    //U-ref =
  RadioGroup1.visible := (Decoder = decoder_adsbPIC);

  ButtonStart.visible := (Decoder = decoder_rxcontrol);
  ButtonStop.visible  := (Decoder = decoder_rxcontrol);

  Beast2.visible      := (Decoder = decoder_beast);
end;  // DecoderDependend



//**** I N T E R O G A T O R E N ******************************************************

// show reply: all
procedure TForm1.all1Click(Sender: TObject);
begin
  all1.checked := true;
  GroundRADAR1.checked := true;
  repaintPPI(true, true);
  IffFilter := -1;
end;

procedure TForm1.N11Click(Sender: TObject);
begin
  N11.checked := true;
  GroundRADAR1.checked := true;
  repaintPPI(true, true);
  IffFilter := IffMasks[0];
end;

procedure TForm1.N12Click(Sender: TObject);
begin
  N12.checked := true;
  GroundRADAR1.checked := true;
  repaintPPI(true, true);
  IffFilter := IffMasks[1];
end;

procedure TForm1.N21Click(Sender: TObject);
begin
  N21.checked := true;
  GroundRADAR1.checked := true;
  repaintPPI(true, true);
  IffFilter := IffMasks[2];
end;

procedure TForm1.N31Click(Sender: TObject);
begin
  N31.checked := true;
  GroundRADAR1.checked := true;
  repaintPPI(true, true);
  IffFilter := IffMasks[3];
end;

procedure TForm1.N41Click(Sender: TObject);
begin
  N41.checked := true;
  GroundRADAR1.checked := true;
  repaintPPI(true, true);
  IffFilter := IffMasks[4];
end;


//dir reply-menue-punkte mit den haeufigsten iff-interrogatoren belegen
procedure TForm1.makeIffMasks;
var k,M,N : integer;
    iffnr : array[0..5] of integer;
begin
  for k:=0 to 4 do begin
    IffMasks[k] := k;
    iffnr[k] := 1;
  end;
  for k:=0 to maxInterogatoren do begin
    for M:=0 to 4 do begin
      if interogatoren[k]>iffnr[M] then begin
        for N:=3 downto M do begin
          iffnr[N+1] := iffnr[N];
          IffMasks[N+1] := IffMasks[N];
        end;
        iffnr[M] := interogatoren[k];
        IffMasks[M] := k;
        break;
      end;
    end;
  end;

  N41.caption := ifftostr(IffMasks[4]);
  N31.caption := ifftostr(IffMasks[3]);
  N21.caption := ifftostr(IffMasks[2]);
  N12.caption := ifftostr(IffMasks[1]);
  N11.caption := ifftostr(IffMasks[0]);
end; //makeIffMasks


//**** T I M E T A G  ******************************************************

// takt fuer die hochaufloesende Zeitmarke des Decoders
procedure TForm1.N12MHz1Click(Sender: TObject);
begin
  N12MHz1.checked := true;
  TagFrequenz := 12000000;
end;

procedure TForm1.N20MHz1Click(Sender: TObject);
begin
  N20MHz1.checked := true;
  TagFrequenz := 20000000;
end;


//**** V O R Z E I C H E N   B E I   H E A D I N G *****************************
// 0.. 360
procedure TForm1.N03601Click(Sender: TObject);
begin
  N03601.checked   := true;
  HeadingPlusMinus := false;
end;

// -180 .. +180
procedure TForm1.N03591Click(Sender: TObject);
begin
  N03591.checked   := true;
  HeadingPlusMinus := true;
end;


// Darstellung der Reichweitenwindrose
procedure TForm1.showMaximumonly1Click(Sender: TObject);
begin
  showMaximumonly1.checked := true;
  maximumrange1.checked    := true;
  repaintPPI(true, true);
end;

procedure TForm1.showbyaltitude1Click(Sender: TObject);
begin
  showbyaltitude1.checked := true;
  maximumrange1.checked   := true;
  repaintPPI(true, true);
end;

procedure TForm1.N10000ft1Click(Sender: TObject);
begin
  N10000ft1.checked     := true;
  maximumrange1.checked := true;
  repaintPPI(true, true);
end;

procedure TForm1.N10000200001Click(Sender: TObject);
begin
  N10000200001.checked  := true;
  maximumrange1.checked := true;
  repaintPPI(true, true);
end;

procedure TForm1.N20000ft30000ft1Click(Sender: TObject);
begin
  N20000ft30000ft1.checked := true;
  maximumrange1.checked    := true;
  repaintPPI(true, true);
end;

procedure TForm1.N30000ft1Click(Sender: TObject);
begin
  N30000ft1.checked     := true;
  maximumrange1.checked := true;
  repaintPPI(true, true);
end;

procedure TForm1.showMinimumonly1Click(Sender: TObject);
begin
  showMinimumonly1.checked := not showMinimumonly1.checked;
  if showMinimumonly1.checked then maximumrange1.checked := true;
  repaintPPI(true, true);
end;


//****   T E X T _ O V E R L A Y ***********************************************

procedure TForm1.backgroundnottransparent1Click(Sender: TObject);
begin
  backgroundnottransparent1.Checked := not backgroundnottransparent1.checked;
end;


//****  B A C K G R O U N D P I C T U R E  *************************************

procedure TForm1.SRTM1Click(Sender: TObject);
begin
  SRTM1.checked := true;
end;

procedure TForm1.MapQuestOSM1Click(Sender: TObject);
begin
  if not MapQuestOSM1.checked then osmOK := false;
  MapQuestOSM1.checked := true;
end;

procedure TForm1.MapQuestAerial1Click(Sender: TObject);
begin
  if not MapQuestAerial1.checked then osmOK := false;
  MapQuestAerial1.checked := true;
end;

procedure TForm1.OpenStreetMap1Click(Sender: TObject);
begin
  if not OpenStreetMap1.checked then osmOK := false;
  OpenStreetMap1.checked := true;
end;


//****  T O W N S **************************************************************

procedure TForm1.N1000001Click(Sender: TObject);
begin
  N1000001.checked := true;
  LoadTowns;
  repaintPPI(true, true);
end;

procedure TForm1.N5000001Click(Sender: TObject);
begin
  N5000001.checked := true;
  LoadTowns;
  repaintPPI(true, true);
end;

procedure TForm1.N1Million1Click(Sender: TObject);
begin
  N1Million1.checked := true;
  LoadTowns;
  repaintPPI(true, true);
end;

procedure TForm1.N3Million1Click(Sender: TObject);
begin
  N3Million1.checked := true;
  LoadTowns;
  repaintPPI(true, true);
end;


//****  W E B S E I T E N ******************************************************

// webseiten
procedure TForm1.sprut1Click(Sender: TObject);
begin
  shellexecute( 0,'open','http://www.sprut.de',nil,nil,SW_SHOW);
end;

procedure TForm1.Airframesorg1Click(Sender: TObject);
begin
  shellexecute( 0,'open','http://www.airframes.org/',nil,nil,SW_SHOW);
end;


//****N E T W O R K  *********************************************************

procedure TForm1.Networksetting1Click(Sender: TObject);
begin
  Application.CreateForm(TForm4,Form4);
end;


//****S E R V E R *********************************************************
// server on-off button
procedure TForm1.ToolButton10Click(Sender: TObject);
begin
  ServerStartStop;
end; //ToolButton10Click


// server on-off via menu
procedure TForm1.Serveractive1Click(Sender: TObject);
begin
  ServerStartStop;
end;


procedure TForm1.ServerStartStop;
begin
  if ServerSocket.Active
    then begin
      ServerSocket.Close;
      Memo1.Lines.Add ('Server: Server stoped');
      ToolButton10.imageindex := 15;
      ToolButton10.Hint       := 'start SERVER (decoded data)';
      Serveractive1.checked   := false;
    end
    else begin
      ServerSocket.Open;
      Memo1.Lines.Add ('Server: Server started');
      ToolButton10.imageindex := 14;
      ToolButton10.Hint       := 'stop SERVER (decoded data)';
      Serveractive1.checked   := true;
    end
end;  //ServerStartStop

{******************* Server: Ereignis-Methoden: Verbindung  ****************************}

procedure TForm1.ServerSocketClientConnect (Sender: TObject; Socket: TCustomWinSocket);
var Nachricht: string;
begin
  Memo1.Lines.Add ('Server: new Client connected');
  Nachricht := 'Server: Welcome to the adsb-server'+chr($0a)+chr($0d);
  Socket.SendText (Nachricht);
end;


{Beachten Sie: Das Ereignis OnDisconnect wird ausgelöst, bevor die Socket-Verbindung
               getrennt wird. Die Anzahl der offenen Sockets, die hier im Status-Feld
               angezeigt wird, enthält also noch den Socket, der unmittelbar danach
               gelöscht wird}
procedure TForm1.ServerSocketClientDisconnect(Sender: TObject; Socket: TCustomWinSocket);
begin
  Memo1.Lines.Add ('Server: client disconnected' + ' - Sockets: ' + IntToStr(ServerSocket.Socket.ActiveConnections));
end;


{****************** Server: Ereignis-Methoden: Kommunikation  **************************}

// das sollte entfernt werden
procedure TForm1.ServerSocketClientRead (Sender: TObject; Socket: TCustomWinSocket);
var nachricht : string[255];
begin
   exit;

  {Nachricht empfangen}
  Socket.ReceiveBuf (Nachricht, SizeOf(Nachricht));
  {Protokoll am Server}
  Memo1.Lines.Add ('Von client: ' + Nachricht);
end;


// einen String mit der aktuellen Zeit für serverausgaben erzeugen
function ServerDateTimetostr(zeit:Tdatetime):string;
begin
  ShortDateFormat := 'yyyy/mm/dd';
  DateSeparator:='/';
  LongTimeFormat := 'hh:nn:ss.zzz';
  result := DateToStr(zeit)+','+TimeToStr(zeit);
end;  //ServerDateTimetostr


// nachricht fuer neues aircraft
procedure TForm1.ServerSendAir(nr:integer);
var nachricht:string[255];
begin
  nachricht:='AIR,0,0,'+inttostr(nr)+','+inttohex(planes[nr].AA,6)+','+inttostr(nr)+',';
  nachricht:=nachricht + ServerDateTimetostr(now)+',';
  nachricht:=nachricht + ServerDateTimetostr(now);
  ServerSend2All(nachricht);
end; //ServerSendAir


// nachricht fuer aircraft-ID
//Field 1:	 Message type 	    (MSG, STA, ID, AIR, SEL or CLK)
//Field 2:  	 Transmission Type  MSG sub types 1 to 8. Not used by other message types.
//Field 3:  	 Session ID 	    Database Session record number
//Field 4:  	 AircraftID 	    Database Aircraft record number
//Field 5:  	 HexIdent 	    Aircraft Mode S hexadecimal code
//Field 6:  	 FlightID 	    Database Flight record number
//Field 7:  	 Date message generated 	  As it says
//Field 8:  	 Time message generated 	  As it says
//Field 9:  	 Date message logged 	          As it says
//Field 10: 	 Time message logged 	          As it says
procedure TForm1.ServerSendId(nr:integer);
var nachricht:string[255];
begin
  nachricht:='ID,0,0,'+inttostr(nr)+','+inttohex(planes[nr].AA,6)+','+inttostr(nr)+',';
  nachricht:=nachricht + ServerDateTimetostr(now)+',';
  nachricht:=nachricht + ServerDateTimetostr(now);
  nachricht:=nachricht + ',' + planes[nr].ident;
  ServerSend2All(nachricht);
end;  //ServerSendId


// nachricht fuer MSG
procedure TForm1.ServerSendMSG(nr:integer; Transtyp:integer);
var nachricht:string[255];
begin
// Transtyp
//1 = ID Message (1090ES DF17 or ELS DAP)
//2 = Surface Position Message (1090ES DF17)
//3 = Airborne Position Message (1090ES DF17)
//4 = Airborne Velocity Message (1090ES DF17)
//5 = Surveillance Altitude Message (DF4, DF20)
//6 = Surveillance ID (Squawk) Message (DF5, DF21)
//8 = All-Call Reply/TCAS Acquisition Squitter (DF11)

//1 IDMessage: Callsign
//2 Surface Position Message: Altitude, GroundSpeed, Track, Lat, Long  ; ist track = heading ?
//3 Airborne Position Message: Altitude, Lat, Long, Alert, Emergency, SPI
//4 Airborne Velocity Message: GroundSpeed, Track, VerticalRate
//5 Surveillance Altitude Message: Altitude, Alert, SPI
//6 Surveillance ID (Squawk) Message: Altitude, Squawk, Alert, Emergency, SPI
//7 All-Call Reply: None at the moment

  //http://homepages.mcb.net/bones/SBS/Article/Barebones42_Socket_Data.htm
  //Field 1:	 Message type 	    (MSG, STA, ID, AIR, SEL or CLK)
  //Field 2:  	 Transmission Type  MSG sub types 1 to 8. Not used by other message types.
  //Field 3:  	 Session ID 	    Database Session record number
  //Field 4:  	 AircraftID 	    Database Aircraft record number
  //Field 5:  	 HexIdent 	    Aircraft Mode S hexadecimal code
  //Field 6:  	 FlightID 	    Database Flight record number
  nachricht:='MSG,'+inttostr(Transtyp)+',0,'+inttostr(nr)+','+inttohex(planes[nr].AA,6)+','+inttostr(nr)+',';

  //Field 7:  	 Date message generated 	  As it says
  //Field 8:  	 Time message generated 	  As it says
  //Field 9:  	 Date message logged 	          As it says
  //Field 10: 	 Time message logged 	          As it says
  nachricht:=nachricht + ServerDateTimetostr(now)+',';                     //Field 9  dt generated
  nachricht:=nachricht + ServerDateTimetostr(now);                         //Field 10 dt logged
  nachricht:=nachricht + ',';

  //Field 11:	 Callsign 	 An eight digit flight ID - can be flight number or registration (or even nothing).
  //Field 12:	 Altitude 	 Mode C altitude. Height relative to 1013.2mb (Flight Level). Not height AMSL..
  //Field 13:	 GroundSpeed 	 Speed over ground (not indicated airspeed)
  //Field 14:	 Track 	 Track of aircraft (not heading). Derived from the velocity E/W and velocity N/S
  //Field 15:	 Latitude 	 North and East positive. South and West negative.
  //Field 16:	 Longitude 	 North and East positive. South and West negative.
  //Field 17:	 VerticalRate 	 64ft resolution
  //Field 18:	 Squawk 	 Assigned Mode A squawk code.
  //Field 19:	 Alert (Squawk change) 	 Flag to indicate squawk has changed.
  //Field 20:	 Emergency 	 Flag to indicate emergency code has been set
  //Field 21:	 SPI (Ident) 	 Flag to indicate transponder Ident has been activated.
  //Field 22:	 IsOnGround 	 Flag to indicate ground squat switch is active
  if Transtyp=1 then
    nachricht:=nachricht + planes[nr].ident;                               //Field 11: Callsign
  nachricht:=nachricht + ',';
  if Transtyp in [2,3,5,6] then
    nachricht:=nachricht + inttostr(planes[nr].altitude);                  //Field 12: Altitude
  nachricht:=nachricht + ',';
  if Transtyp in [2,4] then
    nachricht:=nachricht + inttostr(round(planes[nr].speed));              //Field 13: GroundSpeed knoten
  nachricht:=nachricht + ',';
  if Transtyp in [2,4] then
    //nachricht:=nachricht + inttostr(round(planes[nr].heading/rad + 360) mod 360);        //Field 14: Track  (ist das heading ??)
    nachricht:=nachricht + rad2instr(planes[nr].heading,0);                //Field 14: Track  (heading)
  nachricht:=nachricht + ',';
  if Transtyp in [2,3] then
    nachricht:=nachricht + floattostrF(planes[nr].latitude, ffFixed, 7, 4);//Field 15: Lat
  nachricht:=nachricht + ',';
  if Transtyp in [2,3] then
    nachricht:=nachricht + floattostrF(planes[nr].longitude, ffFixed, 7, 4);//Field 16: Long
  nachricht:=nachricht + ',';
  if Transtyp in [4] then
    nachricht:=nachricht + inttostr(planes[nr].steigen);                   //Field 17: VerticalRate    ft/min
  nachricht:=nachricht + ',';
  if Transtyp in [6] then
    nachricht:=nachricht + planes[nr].squawk;                               //Field 18: Squawk oktal
  nachricht:=nachricht + ',';
  nachricht:=nachricht + '0';//Field 19: Flag: Alert, Squak has changed
  nachricht:=nachricht + ',';
  nachricht:=nachricht + '0';//Field 20: Flag: Emergency
  nachricht:=nachricht + ',';
  nachricht:=nachricht + '0';//Field 21: Flag: SPI      transponder ident has been activated
  nachricht:=nachricht + ',';
  nachricht:=nachricht + '0';//Field 22: Flag: IsOnGround

  ServerSend2All(nachricht);
end; //ServerSendMSG


// einen string an alle clients senden
procedure TForm1.ServerSend2All(nachricht:string);
var i : integer;
begin
  if not ServerSocket.Active then exit;
  nachricht := nachricht+chr($0d)+chr($0a);    //<CR><LF>
  {Weiter an alle Teilnehmer}
  with ServerSocket.Socket do begin
    for i := 0 to ActiveConnections-1 do
      Connections[I].SendText (Nachricht);
  end; {with}
end; //ServerSend2All


//****CLIENT ********************************************************************//
{
http://84.42.164.101:7777/

Frame starts with the @ character prefix,
followed by 26 (48bits tag + 56 bits frames) or 40 bytes (48 bits tag + 112bits frames),
a terminal ';' and ends with a CRLF sequence.
@TAGRAW;<CR><LF>
TAG 48-Bit mit 1/12MHz increment (0.0833 us) ?

@01ACB664B6D5 8D3C660B99455B93F00400C4CE87;
@01ACB664F36D 5D773600ADBBBF;
@01ACB6652B4B02E1999868A7E1;
@01ACB665A7BD5D06A0448CF1E5;
@01ACB667B7D15D4BA992280E2E;
@01ACB667C0F55D4BCD6C5704BC;
@01ACB66849FD02E199105EF823;
@01ACB6689AB48D45AC4F58C907C879B5B8DBACBA;
@01ACB668AE4D8D45AC4F9910E4AC20041718CFFB;
@01ACB66A38918D4248FA909B8260C8ACBC96813E;
@01ACB66B63115D4BCD6C5704BC;
@01ACB66BEE735D4CA50B991B91;
@01ACB66C0EA402E19137AAFCD1;
@01ACB66C2D0C02E61410CD9AA2;
@01ACB66D5F408D40624F587D80FC3B65C1A4F96F;
@01ACB66D7DEF02C60A1FCCC8B8;
@01ACB66DF62A02E197B04CC179;
@01ACB66E56A002E1911E54B916;
@01ACB66FD5785D06A0448CF1E5;
@01ACB670BD6502E1971804412F;
}

//start via Menu
procedure TForm1.RAWClientactive1Click(Sender: TObject);
begin
  RawClientStartStop;
end;

//start via button
procedure TForm1.ToolButton13Click(Sender: TObject);
begin
  RawClientStartStop;
end;

procedure TForm1.RawClientStartStop;
begin
  if ClientVerbunden then begin
    ClientVerbunden := False;
    ClientSocket.Close;
    ToolButton13.imageindex := 21;
    ToolButton13.Hint       := 'start RAW-data CLIENT';
    RAWClientactive1.checked := false;
    Timer1.enabled := (RadioGroup1.Itemindex>1);
    Timer2.enabled := false;
  end else begin
    ClientVerbunden := True;
    ClientSocket.Open;
    ToolButton13.imageindex := 20;
    ToolButton13.Hint       := 'stop RAW-data CLIENT';
    RAWClientactive1.checked := true;
    Timer1.enabled := true;
    writelnlog('RAW-data from '+ClientSocket.Host+'   port: '+inttostr(ClientSocket.port));
    // ein paar extras fuer Prag
    if ClientSocket.Host = '84.42.164.101' then begin
      load_Eu_East(nil);             
      load_Eu_South(nil);
      gotoPrague1Click(nil);
    end;
  end;
end;

//geht die Verbindung zum Server verloren, dann wird timer 2 aktiviert
// dieser versucht jede Sekunde ein Reconnect zum Server
procedure TForm1.RawClientReconnect;
begin
  ClientSocket.Open;
end;

procedure TForm1.Timer2Timer(Sender: TObject);
begin
  RawClientReconnect;
end;

{***************  Client Ereignis-Methoden: Verbindung  ***********************************}

procedure TForm1.ClientSocketLookup(Sender: TObject; Socket: TCustomWinSocket);
begin
  // MemStatus.Lines.Add ('Client: Server wird gesucht')
end;

procedure TForm1.ClientSocketConnecting(Sender: TObject; Socket: TCustomWinSocket);
begin
  // MemStatus.Lines.Add ('Client: Verbindung wird aufgebaut')
end;

procedure TForm1.ClientSocketConnect (Sender: TObject; Socket: TCustomWinSocket);
begin
  if timer2.enabled then begin
    Memo1.Lines.Add ('Client: reconnected');
    timer2.enabled := false;
    ToolButton13.imageindex := 20;
  end else
    Memo1.Lines.Add ('Client: connected');
end;

procedure TForm1.ClientSocketError (Sender: TObject; Socket: TCustomWinSocket;
                                       ErrorEvent: TErrorEvent; var ErrorCode: Integer);
begin
  // falls timer 2 aktiv sein sollte, dann ist der Fehler ein missglückter reconnect
  if not Timer2.enabled then Memo1.Lines.Add ('Client: Fehler ' + IntToStr(ErrorCode));
  // Error 10061: kein Server gefunden
  ErrorCode := 0;
end;

procedure TForm1.ClientSocketDisconnect(Sender: TObject;
  Socket: TCustomWinSocket);
begin
  if ClientVerbunden then begin
    Memo1.Lines.Add ('Client: lost connection');
    Timer2.enabled := true;
    ToolButton13.imageindex := 67;
    //RawClientStartStop;
  end else
  Memo1.Lines.Add ('Client: disconnect');
end;


{********************  Client Ereignis-Methoden: Kommunikation  *************************}

// tritt automatisch auf, wenn daten aus dem socket gelesen werden können
procedure TForm1.ClientSocketRead (Sender: TObject; Socket: TCustomWinSocket);
var Nachricht: string;
begin
  //Socket.ReceiveBuf (Nachricht, SizeOf(Nachricht));
  Nachricht := Socket.ReceiveText;
  //Memo2_Lines_Add (Nachricht);
  ClientNachricht := ClientNachricht + Nachricht;
end;

// client-fehler abfangen
procedure TForm1.ServerSocketClientError(Sender: TObject;
  Socket: TCustomWinSocket; ErrorEvent: TErrorEvent;
  var ErrorCode: Integer);
begin
  Memo1.Lines.Add ('Server: Client-error ' + IntToStr(ErrorCode));
  ErrorCode := 0;
end;

procedure TForm1.Memo1Click(Sender: TObject);
begin
  ListAllFrames := not ListAllFrames;
  if not ListAllFrames then begin
    memo1.lines.add('--RAW-data listing disabled');
    memo1.lines.add('--click to see frames RAW data');
  end else  memo1.lines.add('++RAW-data listing enabled');
end;

procedure TForm1.Memo2Click(Sender: TObject);
begin
  ListDecodedData := not ListDecodedData;
  if not ListDecodedData then begin
    memo2.lines.add('--data listing disabled');
    memo2.lines.add('--click again to see decoded data');
  end else  memo2.lines.add('++data listing enabled');
end;


//***RAW-Server****************************************************************************

procedure TForm1.ServerSocketRAWClientConnect(Sender: TObject;
  Socket: TCustomWinSocket);
begin
  Memo1.Lines.Add ('RAW-Server: new Client connected');
end;

procedure TForm1.ServerSocketRAWClientDisconnect(Sender: TObject;
  Socket: TCustomWinSocket);
begin
  Memo1.Lines.Add ('RAW-Server: client disconnected' + ' - Sockets: ' + IntToStr(ServerSocket.Socket.ActiveConnections));
end;

procedure TForm1.ServerSocketRAWClientError(Sender: TObject;
  Socket: TCustomWinSocket; ErrorEvent: TErrorEvent;
  var ErrorCode: Integer);
begin
  Memo1.Lines.Add ('RAW-Server: Client-error ' + IntToStr(ErrorCode));
  ErrorCode := 0;
end;

procedure TForm1.ServerSocketRAWClientRead(Sender: TObject;
  Socket: TCustomWinSocket);
var nachricht : string[255];
begin
   exit;

  {Nachricht empfangen}
  Socket.ReceiveBuf (Nachricht, SizeOf(Nachricht));
  {Protokoll am Server}
  Memo1.Lines.Add ('Von client: ' + Nachricht);
end;


procedure TForm1.RAWServerStartStop;
begin
  if ServerSocketRAW.Active
    then begin
      ServerSocketRAW.Close;
      Memo1.Lines.Add ('RAW-Server: Server stoped');
      ToolButton19.imageindex := 15;
      ToolButton19.Hint       := 'start RAW-data SERVER';
      RAWServeractive1.checked:= false;
    end
    else begin
      ServerSocketRAW.Open;
      Memo1.Lines.Add ('RAW-Server: Server started');
      ToolButton19.imageindex := 14;
      ToolButton19.Hint       := 'stop RAW-data SERVER';
      RAWServeractive1.checked:= true;
    end
end;  //RAWServerStartStop

procedure TForm1.RAWServeractive1Click(Sender: TObject);
begin
     RAWServerStartStop;
end;


// einen string an alle clients senden
procedure TForm1.RAWServerSend2All(nachricht: string);
var i : integer;
begin
  if not ServerSocketRAW.Active then exit;
  nachricht := nachricht+chr($0d)+chr($0a);    //<CR><LF>
  {Weiter an alle Teilnehmer}
  with ServerSocketRAW.Socket do begin
    for i := 0 to ActiveConnections-1 do
      Connections[I].SendText (Nachricht);
  end; {with}
end; //RAWServerSend2All

procedure TForm1.ToolButton19Click(Sender: TObject);
begin
  RAWServerStartStop;
end;

//***RAW-Server***ENDE*************************************************************************

//**** F I L T E R *************************************************************

//gefiltertes spezielles Logfile ermoeglichen
procedure TForm1.filter1Click(Sender: TObject);
begin
  filter1.checked    := not filter1.checked;
  //FilterForm.visible :=     filter1.checked;
  FilterForm.makevisible(filter1.checked, FilterFormPos);

  FilterForm.updatetime := now;
  FilterForm.ProgressBar1.Position := 0;
  FilterForm.Timer1.enabled := FilterForm.visible;
end;

//**** W A T C H L I S T *******************************************************

//ueberwachte Flugzeuge auswaehlen
procedure TForm1.Watchlist1Click(Sender: TObject);
begin
  Watchlist1.checked    := not Watchlist1.checked;
  WatchListForm.makevisible(Watchlist1.checked, WatchListFormPos);
end;

//**** A I R C R A F T   M A N A G E R *****************************************
procedure TForm1.manageunknownaircraft1Click(Sender: TObject);
begin
  manageunknownaircraft1.checked := not manageunknownaircraft1.checked;
  AAmanager.makevisible(manageunknownaircraft1.checked, AAmanagerPos);
end;


//**** G O T O *****************************************************************
// goto towns or airports
procedure TForm1.gotoTown1Click(Sender: TObject);
begin
  Application.CreateForm(TForm5,Form5);
end;


//**** L O G *******************************************************************
//log
procedure TForm1.Log1Click(Sender: TObject);
begin
  Application.CreateForm(TForm6,Form6);
end;


//**** H E A R T   B E A T *****************************************************
procedure TForm1.decoderstatus1Click(Sender: TObject);
begin
  decoderstatus1.checked := not decoderstatus1.checked;
  HeartBeatForm.makevisible(decoderstatus1.checked, HeartBeatFormPos);
end;


//**** W A T C H L I S T *******************************************************
// prüfen, ob gesuchte Fluege da sind
procedure TForm1.WatchlistCheckAllflights(Sender: TObject);
var k, l : integer;
begin
  for k:=1 to maxWatchList do begin
    for l:=lastPlane downto 0 do begin
      if {(planes[l].active) and} (planes[l].ident<>'') and (planes[l].ident=Watchlist[k].ident) then begin
        // AA des gefundenen Flugzeuges dem Flug zuordnen
        Watchlist[k].AA     := planes[l].AA;
        Watchlist[k].active := true;
        if Watchlist[k].info then beep;
      end;
    end;
  end;
end; //WatchlistCheckAllflights


// prüfen, ob gesuchte Flugzeuge da sind
function TForm1.WatchlistCheckAllPlane : string;
var liststring : string;

  // 0 neu
  // 1 fliegt
  // 2 weg
  procedure WatchLog(k,Mode:integer);
  var filename : string;
      nr       : integer;
  begin
    nr := Watchlist[k].plane;
    liststring := liststring + inttostr(k) + '=' + inttostr(nr) + '  ';
    filename := Pfad+'log\watchlog'+FormatDateTime('-yymmdd',Now)+'.txt';  // Filenamen mit Datum
    assignfile(WatchLogfile,filename);
    {$I-}
    FileMode := 2;        // Lesen/Schreiben
    Append(WatchLogfile);
    {$I+}
    if IOResult <> 0 then begin
      FileMode := 2;        // Lesen/Schreiben
      {$I-}
      rewrite(WatchLogfile);
      {$I+}
      if IOResult <> 0 then exit;
      writeln(WatchLogfile,'adsbScope-WatchLog    ',DateToStr(Date));
    //                      --     2  19:11:15  406131           36975    00.0000    00.0000    523     0      0        UK
      writeln(WatchLogfile,'    Watch     Time      AA   Flight    Alt        Lat       Long  Speed  Head  Vario       Reg      Type'); 

    end;

    LongTimeFormat := 'hh:nn:ss';
    case mode of
     0:write(WatchLogfile,'>> ');  //neu
     1:write(WatchLogfile,'-- ');
     2:write(WatchLogfile,'<< ');  //gegangen
    end;

    write(WatchLogfile, k:5,
                        TimeToStr(Time):10,
                        inttohex(Watchlist[k].AA,6):8);
    if mode<>2 then begin
      write(WatchLogfile, planes[nr].ident:10,
                          planes[nr].altitude:6,
                          koordtostr_Grad(planes[nr].latitude):11,
                          koordtostr_Grad(planes[nr].longitude):11, 
                          round(planes[nr].speed):7,    // knoten
                          rad2instr(planes[nr].heading, 2):6,
                          planes[nr].steigen:7);
      if planes[nr].airframe.known then write(WatchLogfile, planes[nr].airframe.kenner.name:10)
                                   else write(WatchLogfile, trim(AA2Land(planes[nr].aa)):10);
      if planes[nr].airframe.known then write(WatchLogfile, planes[nr].airframe.kenner.typs:10)
                                   else write(WatchLogfile, '':10);
    end;
    writeln(WatchLogfile);
    closefile(WatchLogfile);
  end;  //WatchLog

  procedure WatchKill(k:integer);
  begin
    Watchlist[k].plane   := 0;
    Watchlist[k].present := false;
    WatchLog(k,2);
  end;  //WatchKill

var k, l : integer;
begin    //WatchlistCheckAllPlane
  result     := '';
  liststring := '';
  //Watchlist aktualisieren
  for k:=1 to maxWatchList do if Watchlist[k].active then begin
    if Watchlist[k].present then begin
      if planes[Watchlist[k].plane].active and (Watchlist[k].AA=planes[Watchlist[k].plane].AA) then watchlog(k,1) else watchkill(k);
    end else begin
      for l:=lastPlane downto 0 do if (planes[l].active) and (planes[l].AA=Watchlist[k].AA) then begin
        Watchlist[k].present := true;
        Watchlist[k].plane   := l;     // nummer
        WatchLog(k,0);
      end;
    end;
  end;
  try WatchListForm.updateWatchlistStringGrid; except; end;
  if liststring<>'' then   result := 'Watch=Table: '+liststring;
end;  //WatchlistCheckAllPlane


//**** F R A M E R A T E H I S T O R Y *****************************************

procedure TForm1.frameratehistory1Click(Sender: TObject);
begin
  Application.CreateForm(TForm3,Form3);
end;

procedure TForm1.ToolButton11Click(Sender: TObject);
begin
  Application.CreateForm(TForm3,Form3);
end;


//***** F U L L S C R E E N ****************************************************

procedure TForm1.Image1DblClick(Sender: TObject);
begin
  FullScreen := not FullScreen;
  FormResize(nil);
end;

procedure TForm1.ToolButton12Click(Sender: TObject);
begin
  FullScreen := not FullScreen;
  FormResize(nil);
end;


//***** C O N N E C T   T O   D E C O D E R  ***********************************

procedure TForm1.ToolButton16Click(Sender: TObject);
begin
  ComSetup1.ShowModal;
//  Comport1.ShowSetupDialog;
end;

procedure TForm1.ToolButton17Click(Sender: TObject);
begin
  connect;
end;

procedure TForm1.Button3Click(Sender: TObject);
begin
  Timer1.enabled := true;
end;

//***** T H E   B I G   T A B L E **********************************************

procedure TForm1.ToolButton21Click(Sender: TObject);
begin
  if TableForm.visible then begin
    TableForm.visible := false;
    TableForm.visible := true;
  end else TableForm.makevisible(TableFormPos);
end;


procedure TForm1.activate1Click(Sender: TObject);
begin
  ToolButton21Click(nil);
end;


procedure TForm1.lockedtomainwindow1Click(Sender: TObject);
begin
  lockedtomainwindow1.checked := not lockedtomainwindow1.checked;
end;

//*** M E M O ******************************************************************

procedure TForm1.ToolButton27Click(Sender: TObject);
begin
  Memo2Visible := not Memo2Visible;
  FormResize(nil);
end;


//******************************************************************************
// das zip-file entpacken
function TForm1.zipexpand(zipfilename, zippfad : string):integer;
begin
  result := 0;
  if FileExists(zipfilename) then
  with Zip1 do
  begin
    Filename       := zipfilename;
    ExtractPath    := zippfad;
    ExtractOptions := [eoWithPaths];
    result         := Extract;
  end;
end;   //zipexpand


procedure TForm1.DownloadMaps(was, wohin : string);
var
   url       : string;
   name      : string;
   mapserver : string;
   zielpfad  : string;
   Save_Cursor : TCursor;
begin
  Save_Cursor := Screen.Cursor;
  Screen.Cursor := crHourglass;    { Cursor als Sanduhr }
  try
    mapserver := 'http://www.sprut.de/img/maps/';
    url  := mapserver + was;
    zielpfad :=Pfad+'maps\'+wohin+'\';
    name := zielpfad + was;
    if not fileexists(name) then downloadfile(url, name);
  finally
    Screen.Cursor := Save_Cursor;  { Alten Zustand wiederherstellen }
  end;

  if fileexists(name) then begin
    memo1.lines.add(inttostr(zipexpand(name, zielpfad))+' files downloaded for '+wohin);
    DeleteFile(name);
  end else
    memo1.lines.add('## download failed');
  Checkcontinents;
end; //DownloadMaps


procedure TForm1.Africa1Click(Sender: TObject);
begin
  DownloadMaps('africa.zip' ,'africa');
end;

procedure TForm1.Antarctica1Click(Sender: TObject);
begin
  DownloadMaps('antarctica.zip' ,'antarctica');
end;

procedure TForm1.Asia2Click(Sender: TObject);
begin
  DownloadMaps('asia.zip' ,'asia');
end;

procedure TForm1.Europe2Click(Sender: TObject);
begin
  DownloadMaps('europe.zip' ,'europe');
end;

procedure TForm1.Gus2Click(Sender: TObject);
begin
  DownloadMaps('gus.zip' ,'gus');
end;

procedure TForm1.Northamerica2Click(Sender: TObject);
begin
  DownloadMaps('northamerica.zip', 'northamerica');
end;

procedure TForm1.Southamerica2Click(Sender: TObject);
begin
  DownloadMaps('southamerica.zip', 'southamerica');
end;


procedure TForm1.Button4Click(Sender: TObject);
begin
  mapout;
end;

procedure TForm1.Mapout;
var   outfile          : Tinfile;
      wordfile         : file of word;
      landnamefile     : Tinfile;
      k                : integer;
      wL, wB           : word;
      prefix           : string;
      gap              : integer;
      d                : integer;

  function gradtoword(grad:real):word;
  begin
    result := 0;
    while grad<0    do grad := grad+360;
    while grad>=360 do grad := grad-360;
    try result:=round(grad/360*65535) except memo1.lines.add(floattostr(grad)) end;
  end;

begin
  Button4.enabled := false;

  prefix :='northamerica_usa';
  prefix :='test';
  prefix := bigmapfilename;

  assignfile(landnamefile,Pfad+'maps\'+prefix+'_states.txt');
  rewrite(landnamefile);
  writeln(landnamefile,'#adsbScope-landnamefile ');
  for k:=0 to Lcounter do begin
    wL := gradtoword(L[k].Koord.laenge/rad);
    wB := gradtoword(L[k].Koord.breite/rad);
    if L[k].name<>'' then writeln(landnamefile, wL,' ',wB,' ',L[k].name);
  end;
  closefile(landnamefile);


//  assignfile(outfile,Pfad+'maps\'+prefix+'_borders.txt');
  assignfile(outfile,Pfad+'maps\'+bigmapfilename+'.txt');
  rewrite(outfile);
  gap := 0;
  for k:=0 to (pcounter-1) do begin
    if (M[k].laenge=0) and (M[k].breite=0) then inc(gap) else gap:=0;
    if gap<3 then begin
      wL := gradtoword(M[k].laenge/rad);
      wB := gradtoword(M[k].breite/rad);
      if M[k].V then writeln(outfile, wL, '  ' , wB ,' V')
                else writeln(outfile, wL, '  ' , wB );
    end;
  end;
  closefile(outfile);


//  assignfile(wordfile,Pfad+'maps\'+prefix+'_borders.map');
  assignfile(wordfile,Pfad+'maps\'+bigmapfilename);
  rewrite(wordfile);
  gap := 0;
  d   := 0;
  for k:=0 to (pcounter-1) do begin
    if (M[k].laenge=0) and (M[k].breite=0) then inc(gap) else gap:=0;
    if gap<3 then begin
      wL := gradtoword(M[k].laenge/rad);
      wB := gradtoword(M[k].breite/rad);
      write(wordfile, wL, wB );
    end else inc(d);
  end;
  closefile(wordfile);
  memo1.lines.add(inttostr(d)+' nullzeilen entfernt');

  Button4.enabled := true;
end; //Mapout



procedure TForm1.Button5Click(Sender: TObject);
begin
  shrink;
end;

procedure TForm1.Shrink;
var k, l, d, dd            : integer;
    //n                      : integer;
    startk, stopk          : integer;
    Lmin, Lmax, Bmin, Bmax : real;
    Ldelta, Bdelta         : real;

  function isnull(K: TKoordinaten):boolean;
  begin
    result := (K.laenge=0) and (K.breite=0);
  end;

begin  //Shrink
  Button5.enabled := false;
  memo1.lines.add(inttostr(pcounter)+' Punkte');

  //flecken entfernen
  LDelta := 0.0003*2; // 1NM
  BDelta := 0.0003*2;

  k      := 0;
  startk := 0;
  stopk  := 0;
  d      := 0;
  dd     := 0;
  repeat
    inc(k);
    if isnull(M[k-1]) and (not isnull(M[k])) then begin
      startk := k;
      stopk  := startk;
    end;
    if isnull(M[k+1]) and (not isnull(M[k])) then stopk:=k;
    if (startk <> stopk) and (M[startk].laenge = M[stopk].laenge) and (M[startk].breite = M[stopk].breite) then begin
      Lmin:=M[startk].laenge;
      Lmax:=M[startk].laenge;
      Bmin:=M[startk].breite;
      Bmax:=M[startk].breite;
      for l:=startk to stopk do begin
        if M[l].laenge<Lmin then Lmin := M[l].laenge;
        if M[l].laenge>Lmax then Lmax := M[l].laenge;
        if M[l].breite<Bmin then Bmin := M[l].breite;
        if M[l].breite>Bmax then Bmax := M[l].breite;
      end;
      if (Lmax-Lmin < Ldelta) and (Bmax-Bmin < Bdelta) then begin
        inc(dd);
        for l:=startk to stopk do begin
          M[l].laenge := 0;
          M[l].breite := 0;
          M[l].V      := false;
          inc(d);
        end;
      end;
      startk := stopk;
    end;
  until k >= (pcounter-1);
  memo1.lines.add(inttostr(dd)+' flecken gefunden');
  memo1.lines.add(inttostr(d)+' fleckenpunkte entfernt');

{
  //doppellinien entfernen
  // alle punkte abarbeiten
  for k := 0 to (pcounter-1) do if not isnull(M[k]) then begin
    if (k mod 1000 = 0) then begin memo1.lines.add(inttostr(k)); update; end;
    //gibt es diesen punkt noch einmal?
    for l:=(pcounter-1) downto (k+1) do  if not isnull(M[l]) then
    if (M[l].laenge=M[k].laenge) and (M[l].breite=M[k].breite) then begin
      n:=2;
      while (not isnull(M[l-n])) and (M[l-n].laenge=M[k+n].laenge) and (M[l-n].breite=M[k+n].breite)
             and ((k+n+10) < (l-n)) do begin
        M[l-n+1].laenge:=0;
        M[l-n+1].breite:=0;
        M[l-n+1].V:=false;
        inc(n);
        inc(d);
      end;
    end;
  end;
  memo1.lines.add(inttostr(d)+' Doubletten gefunden');
}

{
  // einzelpunkte entfernen
  d := 0;
  for k := 1 to (pcounter-2) do
  if isnull(M[k-1]) and isnull(M[k+1]) then begin
    M[k].laenge:=0;
    M[k].breite:=0;
    inc(d);
  end;
  memo1.lines.add(inttostr(d)+' Einzelpunkte gefunden');
}

{
  //flecken entfernen
  LDelta := 0.0003; // 1NM
  BDelta := 0.0003;
  k      := 0;
  startk := 0;
  stopk  := 0;
  d      := 0;
  dd     := 0;
  repeat
    inc(k);
    if isnull(M[k-1]) and (not isnull(M[k])) then begin
      startk := k;
      stopk  := startk;
    end;
    if isnull(M[k+1]) and (not isnull(M[k])) then stopk:=k;
    if (startk <> stopk) and (M[startk].laenge = M[stopk].laenge) and (M[startk].breite = M[stopk].breite) then begin
      Lmin:=M[startk].laenge;
      Lmax:=M[startk].laenge;
      Bmin:=M[startk].breite;
      Bmax:=M[startk].breite;
      for l:=startk to stopk do begin
        if M[l].laenge<Lmin then Lmin := M[l].laenge;
        if M[l].laenge>Lmax then Lmax := M[l].laenge;
        if M[l].breite<Bmin then Bmin := M[l].breite;
        if M[l].breite>Bmax then Bmax := M[l].breite;
      end;
      if (Lmax-Lmin < Ldelta) and (Bmax-Bmin < Bdelta) then begin
        inc(dd);
        for l:=startk to stopk do begin
          M[l].laenge := 0;
          M[l].breite := 0;
          M[l].V      := false;
          inc(d);
        end;
      end;
      startk := stopk;
    end;
  until k >= (pcounter-1);
  memo1.lines.add(inttostr(dd)+' flecken gefunden');
  memo1.lines.add(inttostr(d)+' fleckenpunkte entfernt');
}
{
  //spruenge suchen
  d := 0;
  for k := 0 to (pcounter-2) do
  if (not isnull(M[k])) and (not isnull(M[k+1]))
      and (abs(M[k].breite - M[k+1].breite) > 0.01 )   then begin //5grad
    M[k].laenge := 0;
    M[k].breite := 0;
    M[k].V      := false;
    M[k+1].laenge := 0;
    M[k+1].breite := 0;
    M[k+1].V      := false;
    inc(d);
  end;
  memo1.lines.add(inttostr(d)+' Breiten-Spruenge gefunden');

  //spruenge suchen
  d := 0;
  for k := 0 to (pcounter-2) do
  if (not isnull(M[k])) and (not isnull(M[k+1]))
      and (abs(M[k].laenge - M[k+1].laenge) > 0.1 )   then begin //5grad
    M[k].laenge := 0;
    M[k].breite := 0;
    M[k].V      := false;
    M[k+1].laenge := 0;
    M[k+1].breite := 0;
    M[k+1].V      := false;
    inc(d);
  end;
  memo1.lines.add(inttostr(d)+' Laengen-Spruenge gefunden');
}
{
  //180 grd suchen
  d := 0;
  for k := 0 to (pcounter-2) do
  if (not isnull(M[k]))
      and (abs( abs(M[k].laenge/rad) - 180 ) < 0.01 )  then begin
    M[k].laenge := 0;
    M[k].breite := 0;
    M[k].V      := false;
    inc(d);
  end;
  memo1.lines.add(inttostr(d)+' 180 grad punkte gefunden');

  // 90 grd suchen
  d := 0;
  for k := 0 to (pcounter-2) do
  if (not isnull(M[k]))
      and (abs( abs(M[k].laenge/rad) - 90 ) < 0.01 )  then begin
    M[k].laenge := 0;
    M[k].breite := 0;
    M[k].V      := false;
    inc(d);
  end;
  memo1.lines.add(inttostr(d)+' 90 grad punkte gefunden');
}

{
  //pole suchen
  d := 0;
  for k := 0 to (pcounter-2) do
  if (not isnull(M[k])) and (not isnull(M[k+1]))
      and (M[k+1].breite < -1.5 )   then begin //-87
    M[k+1].laenge := 0;
    M[k+1].breite := 0;
    M[k+1].V      := false;
    inc(d);
  end;
  memo1.lines.add(inttostr(d)+' minusse gefunden');
}

  Button5.enabled := true;
end;  //Shrink


//start new border
// pcounter zeigt auf naechste freie Stelle in M
// diese belegen
// und 00 anhängen
{
  Tkoordinaten = record
           laenge : real;
           breite : real;
           cos_breite : real;
           sin_breite : real;
           x,y    : integer;
           V      : boolean;
           hoehe  : integer;   // in fuss
           time   : TDateTime;
         end;
}
//ZentB  ZentL     sind in radiant
procedure TForm1.Button6Click(Sender: TObject);
begin

  M[pcounter].laenge := ZentL;
  M[pcounter].breite := ZentB;
  M[pcounter].hoehe  := 0;
  sincos(M[pcounter]);

  inc(pcounter);
  M[pcounter].laenge := 0;
  M[pcounter].breite := 0;
  M[pcounter].hoehe  := 0;
  inc(pcounter);
  M[pcounter].laenge := 0;
  M[pcounter].breite := 0;
  M[pcounter].hoehe  := 0;
  inc(pcounter);

end; //start new border

// punkt an border anhaengen :
// 00 am Ende abschneiden
// koordinatenpunk anhaengen
// und wieder 00 anhaengen
procedure TForm1.Button7Click(Sender: TObject);
begin
  repeat
    dec(pcounter);
  until (M[pcounter].laenge<>0) and (M[pcounter].breite<>0);
  inc(pcounter);
  Button6Click(nil);
  repaintPPI(true, true);
end;

// punkt an border anhaenegn & osm neu aufbauen
procedure TForm1.Button8Click(Sender: TObject);
begin
  Button7Click(nil);
  OSMmap1Click(nil);
end;

// letzten punkt loeschen
procedure TForm1.Button9Click(Sender: TObject);
begin
  repeat
    dec(pcounter);
  until (M[pcounter].laenge<>0) and (M[pcounter].breite<>0);
  
  M[pcounter].laenge := 0;
  M[pcounter].breite := 0;
  M[pcounter].hoehe  := 0;
  inc(pcounter);
  M[pcounter].laenge := 0;
  M[pcounter].breite := 0;
  M[pcounter].hoehe  := 0;
  inc(pcounter);

  repaintPPI(true, true);
end;


procedure TForm1.Button10Click(Sender: TObject);
var b, l : real;
    z    : byte;
begin
  b := ZentB/rad;
  l := ZentL/rad;
  z := PortMap.Test(b, l);
  //if z<>$FF then
  memo2_lines_add(inttostr(z));
end;


procedure TForm1.ToolButton18Click(Sender: TObject);
begin
  iffForm.visible := true;
  iffForm.init(0);
end;


//************* S R T M ***************************************//
//Urheber- und Nutzungsrechte
//Die vom USGS veröffentlichten Daten sind als Public Domain zum Herunterladen
//kostenfrei verfügbar und dürfen uneingeschränkt verwendet werden:
//USGS-authored or produced data and information are in the public domain.
//Die nicht veröffentlichten Daten mit einer Bogensekunde Auflösung von Gebieten
//außerhalb der USA unterliegen der Kontrolle des US-Verteidigungsministeriums.

//Die Höhenangaben der Oberflächenvermessung beziehen sich auf das weltweit
//einheitliche Referenzsystem WGS84 EGM96 Geoid, die horizontale Georeferenzierung
//erfolgt mit WGS84 als geodätisches Datum.

//generate overlay picture from SRTM-Tiles
procedure TForm1.MakeSRTM;
var
   Koo                        : Tkoordinaten;
   LminR, LmaxR, BminR, BmaxR : real;
   valid                      : boolean;
   links, rechts, oben, unten : integer;
   BMsrtm                     : TBitmap;
   B, L, LL                   : integer;
   srtmfilename               : string;
   srtmpath                   : string;
   srtmurl                    : string;
   srtmurl_base               : string;
   ToRect, FromRect           : TRect;
   srtmtile                   : TBitmap;
   xx, yy, xxs, yys           : integer;
   scalexx, Scaleyy           : integer;
   JPG                        : TJpegImage;
   bufColor                   : TColor;
   non1, non2                 : real;
   srtmServerTested           : boolean;
   lmFile                     : file of TLandMass;
   lmfilename                 : string;
   testfile                   : string;

  procedure testkminmax(x, y: integer);
  begin
    Koo   := xy2koord(x,y);
    if not Koo.v then valid := false else begin
      if abs(Koo.laenge - LminR) > (1.8*Pi) then begin
        if (Koo.laenge < LminR) then Koo.laenge :=Koo.laenge + 2*Pi
                                else Koo.laenge :=Koo.laenge - 2*Pi;
      end;
      if abs(Koo.laenge - LmaxR) > (1.8*Pi) then begin
        if (Koo.laenge < LmaxR) then Koo.laenge :=Koo.laenge + 2*Pi
                                else Koo.laenge :=Koo.laenge - 2*Pi;
      end;
      if Koo.laenge < LminR then LminR := Koo.laenge;
      if Koo.laenge > LmaxR then LmaxR := Koo.laenge;
      if Koo.breite < BminR then BminR := Koo.breite;
      if Koo.breite > BmaxR then BmaxR := Koo.breite;
    end;
  end;

  // Farbe aufhellen
  function bleach(dunkel:Tcolor):Tcolor;
  var r,b,g:integer;
  begin
    result := dunkel;
    if fullcolor1.checked then result:= dunkel else
    if pale1.checked then result:= ((dunkel and $FEFEFE) shr 1) or $808080 else
    if pale2.checked then result:= ((dunkel and $FCFCFC) shr 2) or $C0C0C0 else
    if gray1.checked then begin
      r := (dunkel and $FF0000) shr 16;
      g := (dunkel and $00FF00) shr  8;
      b :=  dunkel and $0000FF;
      g:=((r+g+b) div 8) +159;     // das Grau ausbleichen
      result:= g * $010101;
    end;
  end;

  //vor dem ersten Herunterladen pruefen, ob server erreicht werden kann
  function srtmServerOk:boolean;
  var  testfile : string;
  begin
    result:= BsrtmServerOk;
    if srtmServerTested then exit
    else begin
      testfile := srtmpath+'test.jpg';
      DeleteFile(testfile);
      downloadfile(srtmurl_base+'tiles/N/E/N30E000.jpg', testfile);
      BsrtmServerOk := fileexists(testfile);
      if BsrtmServerOk then Memo1.Lines.Add('SRTM-server: OK')
                       else Memo1.Lines.Add('SRTM-server: FAIL');
      srtmServerTested := true;
      result := BsrtmServerOk;
    end;
  end; //testserver

  // master-landmass-file laden
  function LoadMaster:boolean;
  var lmfilename, srtmurl : string;
  begin
    result := false;
    if not srtmServerOK then exit;
    lmfilename := pfad+'srtm\landmass_master.srtm';
    srtmurl :=srtmurl_base+'landmass_master.srtm';
    if downloadfile(srtmurl, lmfilename) then begin
      Memo1.Lines.Add('SRTM-masterfile: OK');
      result:= true;
    end else Memo1.Lines.Add('SRTM-masterfile: FAIL');
  end;  //LoadMaster

  // load file, that contains mask of existing tiles
  procedure loadLandMass;
  var  lll, bbb : integer;
  begin
    // ueberall land, failback
    for lll:=-180 to 179 do for bbb:=-60 to 60 do srtmLandmass[lll,bbb]:=1;
    // master-file nutzen wenn vorhanden
    lmfilename := pfad+'srtm\landmass_master.srtm';
    if not fileexists(lmfilename) then LoadMaster; //wenn fehlt, dann download
    if fileexists(lmfilename) then begin
      AssignFile(lmFile, lmfilename);
      Reset(lmFile);
      Read(lmFile, srtmLandmass);
      CloseFile(lmFile);
    end else begin
      // wenn nicht dann privates file nutzen
      lmfilename := pfad+'srtm\landmass.srtm';
      if fileexists(lmfilename) then begin
        AssignFile(lmFile, lmfilename);
        Reset(lmFile);
        Read(lmFile, srtmLandmass);
        CloseFile(lmFile);
      end;
    end; // master hat gefehlt
    // neuer pfad fuer save noetig, wenn master vorhanden
    lmfilename := pfad+'srtm\landmass.srtm';
  end; //loadLandMass

  // speichert private land-maske
  procedure saveLandMass;
  begin
    AssignFile(lmFile, lmfilename);
    Rewrite(lmFile);
    Write(lmFile, srtmLandmass);
    CloseFile(lmFile);
  end; //saveLandMass

  // kennzeichnet tile als nicht existent auf dem server
  procedure markLandMass(L, B : integer);
  begin
    // 0= wasser
    srtmLandmass[L,B] := 0;
  end;

  // liefer 0, wenn tile auf dem server nicht existiert
  function testLandMass(L, B : integer):boolean;
  begin
    result:= (srtmLandmass[L,B]<>0);
  end;

begin    //MakeSRTM
  valid            := true;
  srtmok           := false;
  BsrtmServerOk    := true;
  srtmServerTested := false;
  srtmpath := pfad+'srtm\tile\';
  srtmurl_base     := 'http://www.sprut.de/img/srtm/';
  rotation(non1, non2, 1);

  //ausdehnung des bildausschnittes bestimmen
  Koo   := xy2koord(image1.width div 2, image1.height div 2);
  LminR := Koo.laenge;
  LmaxR := LminR;
  BminR := Koo.breite;
  BmaxR := BminR;
  testkminmax(0,0);
  testkminmax(image1.width div 2,0);
  testkminmax(image1.width,0);
  testkminmax(0,image1.height);
  testkminmax(image1.width div 2,image1.height);
  testkminmax(image1.width,image1.height);
  links  := round( LminR/rad -0.5);
  rechts := round( LmaxR/rad +0.5);
  if (rechts<links) then rechts := rechts+360; // Datumsgrenze
  unten  := round( BminR/rad -0.5);
  oben   := round( BmaxR/rad +0.5);
  //Statusbar1.panels[5].text := inttostr(links)+'->'+inttostr(rechts)+' / '+inttostr(unten)+'->'+inttostr(oben);
  if ((rechts-links) > 40) or ((oben-unten) > 20) then begin
    Memo1.Lines.Add('SRTM: visible area to large');
    Memo1.Lines.Add('      please zoom in and retry');
    exit;
  end;

  // Begrenzung auf 2000 x 1500 pixel
  Scalexx := 600;
  Scaleyy := 600;
  case (rechts-links) of
    7..9   : Scalexx := 300;
    10..19 : Scalexx := 200;
    20..26 : Scalexx := 100;
    27..39 : Scalexx :=  75;
    40     : Scalexx :=  50;
  end;
  case (oben-unten) of
    5..9   : Scaleyy := 300;
    10..14 : Scaleyy := 150;
    15..20 : Scaleyy := 100;
  end;

  // bitmap korrekter groesse erstellen und mit tiles fuellen
  BMsrtm := TBitmap.create;
  BMsrtm.width  := Scalexx * (rechts-links);
  BMsrtm.height := Scaleyy * (oben-unten);
  bufColor := BackColor;
  BackColor := clNavy;
  clrscr(BMsrtm);
  BackColor := bufColor;

  loadLandMass;

  for LL:=links to rechts do begin
    for B:=unten to oben do if ((B>-60) and (B<60)) then begin
      L :=LL;
      //korrektur fuer Datumsgrenze
      if L>179  then L:=L-360;
      if L<-180 then L:=L+360;
      // name des tile bestimmen
      srtmfilename := '';
      if B>=0 then srtmfilename:=srtmfilename+'N' else srtmfilename:=srtmfilename+'S';
      if abs(B)<10 then srtmfilename:=srtmfilename+'0';
      srtmfilename:=srtmfilename+inttostr(abs(B));
      if L>=0 then srtmfilename:=srtmfilename+'E' else srtmfilename:=srtmfilename+'W';
      if abs(L)<10 then srtmfilename:=srtmfilename+'0';
      if abs(L)<100 then srtmfilename:=srtmfilename+'0';
      srtmfilename:=srtmfilename+inttostr(abs(L))+'.jpg';

      // herunterladen
      srtmurl := srtmurl_base+'tiles/';
      if B>=0 then srtmurl:=srtmurl+'N/' else srtmurl:=srtmurl+'S/';
      if L>=0 then srtmurl:=srtmurl+'E/' else srtmurl:=srtmurl+'W/';
      if not fileexists(srtmpath+srtmfilename) then
        if testLandMass(L, B) then
          if srtmServerOk then downloadfile(srtmurl+srtmfilename, srtmpath+srtmfilename);

      //vorhandenes Bild laden
      if fileexists(srtmpath+srtmfilename) then begin
        JPG := TJPEGImage.Create;
        try
          srtmtile := TBitmap.create;
          JPG.LoadFromFile(srtmpath+srtmfilename);
          srtmtile.Assign(JPG);
          xx := Scalexx * (LL-links);
          yy := BMsrtm.height - (Scaleyy * (B-unten));
          ToRect := Rect(xx, yy-Scaleyy, xx+Scalexx, yy);      //dest
          FromRect := Rect(0,0,srtmtile.width, srtmtile.height);   //source
          BMsrtm.Canvas.StretchDraw(ToRect, srtmtile);
          srtmtile.Free;
        finally
          JPG.Free
        end;
      end else if BsrtmServerOk then markLandMass(L,B); // liegt im wasser
    end;
    progressbar1.position := round(50 * (LL-links) / (rechts-links));
  end;

  saveLandMass;
  progressbar1.position := 50;

  // bitmap punktweise in overlaybitmap kopieren
  srtm.bm.width := image1.width;
  srtm.bm.height := image1.height;
  for xx:=0 to srtm.bm.width do begin
    for yy := 0 to srtm.bm.height do begin
      Koo := xy2koord(xx, yy);       // punktkoordinaten in rad
      //umrechnen in Grad relativ zur linken unteren Ecke von srtm.bm
      Koo.laenge := Koo.laenge/rad - links;
      Koo.breite := Koo.breite/rad - unten;
      //korrektur fuer Datumsgrenze
      if Koo.laenge<0   then Koo.laenge := Koo.laenge + 360;
      if Koo.laenge>180 then Koo.laenge := Koo.laenge - 360;
      xxs := round(Koo.laenge * Scalexx);
      yys := BMsrtm.height - round(Koo.breite * Scaleyy);
      srtm.bm.canvas.pixels[xx, yy] := bleach( BMsrtm.canvas.pixels[xxs, yys] );
    end;
    progressbar1.position := 50 + round(50 * xx / srtm.bm.width);
  end;
  BMsrtm.free;

  srtm.ZentB := ZentB;
  srtm.ZentL := ZentL;
  srtm.Scale := Scale;
  srtm.xoff := 0;
  srtm.yoff := 0;

  srtmok := true;
  SRTMbackground1.checked := true;
  OSMbackground1.checked  := false;
  repaintPPI(false, true);
end;  // MakeSRTM


// loeschen aller leeren tiles
// die sind nur wenige 100 Bytes groß
//loeschen des masterfiles
function TForm1.srtm_CleanUp:integer;
var
    search     : TSearchRec;
    filename   : string;
begin
  result := 0;
  if FindFirst(pfad+'srtm\tile\*.jpg', faAnyFile, search)=0 then begin
    filename :=pfad+'srtm\tile\'+search.name ;
    if search.size<1024 then DeleteFile(filename);
    while FindNext(search) = 0 do begin
      filename :=pfad+'srtm\tile\'+search.name ;
      if search.size<1024 then DeleteFile(filename);
    end;
  end;
  FindClose(search);
  filename :=pfad+'srtm\landmass_master.srtm' ;
  DeleteFile(filename);
end;  //srtm_CleanUp    


//** D E C O D E R C O N T R O L ***********************************************

// Decoderauswahl
procedure TForm1.adsbPIC2Click(Sender: TObject);
begin
  adsbPIC2.checked := true;
  DecoderNr1       := decoder_adsbPIC;
end;

procedure TForm1.GNS58901Click(Sender: TObject);
begin
  GNS58901.checked := true;
  DecoderNr1       := decoder_adsbPIC;
end;

procedure TForm1.rxControl1Click(Sender: TObject);
begin
  rxControl1.checked := true;
  DecoderNr1         := decoder_rxcontrol;
end;

procedure TForm1.Beast1Click(Sender: TObject);
begin
  Beast1.checked := true;
  DecoderNr1     := decoder_beast;
  N12MHz1Click(nil);  //TAG mit 12MHz clock
end;

procedure TForm1.selcetCOMport1Click(Sender: TObject);
begin
  //if ComPort1.Connected then exit;  //DEBUG
  ComSetup1.ShowModal;
//  Comport1.ShowSetupDialog;
end;



//
//
// DIP 	Function 	       OFF, open = lower case         ON, closed = upper case characters
// 1,2 	Baudrate 	       not supported                  not supported
// 3 	Output Format 	       "c" 	                      "C"
// 4 	DF-11/17 only filter   "d" 	                      "D"
// 5 	MLAT info 	       "e" 	                      "E"
// 6 	CRC check disable      "f" 	                      "F"
// 7 	DF0/4/5 masking filter "g" 	                      "G"
// 8 	RTS handshake 	       "h" 	                      "H"
// 9 	FEC disable 	       "i" 	                      "I"
// 10 	Mode-A/C enable        "j" 	                      "J"
//
//sendet alle Zeichen des String einzeln zum Beast
procedure TForm1.BeastControl(st : string);
var k , nr : integer;
    asciistr : string[63];
begin
  if st='' then exit;
  if not ComPort1.Connected then exit;
  ComPort1.ClearBuffer(true, true);
  for k:=1 to length(st) do begin
    asciistr := chr($1A)+'1'+st[k];
//    asciistr := chr($1B)+'1'+st[k];
    nr := Comport1.WriteStr(asciistr);
    if nr<>3 then memo1.lines.add('##Beast comm. failure:'+inttostr(nr));
  end;
end;  //BeastControl


//binary data format
procedure TForm1.binaryformat1Click(Sender: TObject);
begin
  binaryformat1.checked := not binaryformat1.checked;
  BeastControlC;
end;

procedure TForm1.BeastControlC;
begin
  if binaryformat1.checked then begin
    BeastControl('C');
    writelnlog('Decoder: Beast: binary data format');
  end else begin
    BeastControl('c');
    writelnlog('Decoder: Beast: normal data format');
  end;
end;

//Beast nur DF11/17 ausgeben
procedure TForm1.onlyDF11171Click(Sender: TObject);
begin
  onlyDF11171.checked := not onlyDF11171.checked;
  BeastControlD;
end;

procedure TForm1.BeastControlD;
begin
  if onlyDF11171.checked then begin
    BeastControl('D');
    writelnlog('Decoder: Beast: only DF11,17 on');
  end else begin
    BeastControl('d');
    writelnlog('Decoder: Beast: only DF11,17 off');
  end;
end;


//Beast MLAT on off, ueberschreibt Switch 5
procedure TForm1.enabletimetag1Click(Sender: TObject);
begin
  enabletimetag1.checked := not enabletimetag1.checked;
  BeastControlE;
end;

procedure TForm1.BeastControlE;
begin
  if enabletimetag1.checked then begin
    BeastControl('E');
    enabletimetag1.imageindex:=34;
    enabletimetag1.caption:='disable time tag';
    writelnlog('Decoder: Beast: disable time tag');
  end else begin
    BeastControl('e');
    enabletimetag1.imageindex:=35;
    enabletimetag1.caption:='enable time tag';
    writelnlog('Decoder: Beast: enable time tag');
  end;
end;


//Beast crc check fuer FF11/17 abschalten
procedure TForm1.noCRCcheck1Click(Sender: TObject);
begin
  noCRCcheck1.checked := not noCRCcheck1.checked;
  BeastControlF;
end;

procedure TForm1.BeastControlF;
begin
  if noCRCcheck1.checked then begin
    BeastControl('F');
    writelnlog('Decoder: Beast: CRC check off');
  end else begin
    BeastControl('f');
    writelnlog('Decoder: Beast: CRC check on');
  end;
end;

//Beast unterdruecken der DF0/4/5
procedure TForm1.suppressDF0451Click(Sender: TObject);
begin
  suppressDF0451.checked := not suppressDF0451.checked;
  BeastControlG;
end;

procedure TForm1.BeastControlG;
begin
  if suppressDF0451.checked then begin
    BeastControl('G');
    writelnlog('Decoder: Beast: DF0,4,5 off');
  end else begin
    BeastControl('g');
    writelnlog('Decoder: Beast: DF0,4,5 on');
  end;
end;

// aktiviert neue Version
procedure TForm1.Debug01Click(Sender: TObject);
begin
  if not ComPort1.Connected then exit;
  send_buf[0]:=SET_DEBUG;   //
  send_buf[1]:=0;           //
  Sende_Empfange(2, 16);
  ATestActive := false;
end;

// aktiviert alte Version
procedure TForm1.Debug11Click(Sender: TObject);
begin
  if not ComPort1.Connected then exit;
  send_buf[0]:=SET_DEBUG;   //
  send_buf[1]:=1;           //
  Sende_Empfange(2, 16);
  ATestActive := true;
end;


procedure TForm1.AutomaticTest1Click(Sender: TObject);
begin
  AutomaticTest1.Checked := not AutomaticTest1.Checked;
  if AutomaticTest1.Checked then begin
    ATestNeuCounter := 0;
    ATestAltCounter := 0;
    AtestActive     := false;
  end else Debug01Click(nil);
end;


procedure TForm1.ComPort1BeforeClose(Sender: TObject);
begin
  ComPort1.ClearBuffer(true, true);
end;


//**** S Q U A W K **************************************************************
{
7500 – Hijack
7600 – Lost Comm (radio failure)
7700 – Emergency
}

{
Special Transponder Codes
In addition to being used as a method of identification, your transponder can also be used to transmit a signal for help to ATC. I have listed some of the codes below:
    * 7700 - Emergency, this will be an indication on the controller?s screen that your aircraft has experienced an emergency. Just as a reminder, it is OK to declare an emergency on VATSIM just as long as you ask the controller for permission first, if they decline the emergency, you must disconnect..
    * 7600 - Radio Failure, this code is not used very often on VATSIM because we have both Text and Voice communications. If you would like to simulate lost comms procedures, make sure to check with your controller first.
    * 7777 - Military Interceptor Operations, consult the VATSIM Special operation regulations before engaging in any Military Interceptor Operations.
    * 7500 - Hijack, this code is a part of the tutorial for informational purposes only. Simulating hijackings is ABSOLUTELY NOT ALLOWED on the VATSIM Network. Should you set your transponder to this code you will automatically be removed from the network. Please do not ?Try to see if it works?, Users simulating a hijacking are subject to disciplinary action up to and including permanent suspension of their PID.

VFR Squawk Codes of the world
    * 1200- VFR flight, this is the standard squawk code used in North American airspace when no other has been assigned.
    * 7000- VFR standard squawk code for most of European airspace.
}


{
0000
    * Shall not be used — is a non-discrete mode A code (Europe)[11]
    * Mode C or other SSR failure (UK)[12]
    * Should never be assigned (USA)[13]
    * Military intercept code (USA)[14]
    * Internal ARTCC subsets assigned by En Route Safety and Operations Support (Discrete codes only except for first primary block to be used as non-discrete if all discrete codes are assigned) (USA)[13]

0033
    * Parachute dropping in progress (UK)[12]

0041 to 0057
    * Assigned for VFR traffic under Flight Information Services (BXL FIC) (Belgium)

0100
    * Flights operating at aerodromes (in lieu of codes 1200, 2000 or 3000 when assigned by ATC or noted in the Enroute Supplement Australia) (Australia)[16]

0100 to 0400
    * Allocated to Service Area Operations for assignment for use by Terminal/CERAP/Industry/Unique Purpose/Experimental Activities (USA)[13]

0100 to 0700
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]

0500, 0600, 0700
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

1000
    * Instrument Flight Rules (IFR) flight below 18,000' when no other code has been assigned (Canada)[6]
    * Non-discrete mode A code reserved use in Mode S radar/ADS-B environment where the aircraft identification will be used to correlate the flight plan instead of the mode A code (ICAO)[11]
    * Used exclusively by ADS-B aircraft to inhibit Mode 3A transmit (USA)[13]
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

1100
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

1200
    * Civil VFR flights in class E or G airspace (Australia )[16]
    * Visual flight rules (VFR) flight, this is the standard squawk code used in North American airspace when no other has been assigned (Canada and USA)[6][13]

1201
    * Visual flight rules (VFR) glider operations for gliders not in contact with ATC, through February 2012 (USA)[17]
    * Assigned via FAR 93.95 for use by VFR aircraft in the immediate vicinity of LAX (USA)[13]

1202
    * Visual flight rules (VFR) glider operations for gliders not in contact with ATC; effective February 2012 (USA)[13][17]

1203 to 1272
    * Discrete 1200 series codes, unless otherwise allocated (for example, 1255), designated for DVFR aircraft and only assigned by FSS (USA)[13]

1255
    * Aircraft not in contact with an ATC facility while en route to/from or within the designated fire fighting area(s) (USA)[13][18]

1273 to 1275
    * Calibration Performance Monitoring Equipment (CPME) “Parrot” transponders (USA)[13]

1276
    * Air Defense Identification Zone (ADIZ) penetration when unable to establish communication with ATC or aeronautical facility (USA)[13]

1277
    * VFR aircraft which fly authorized SAR missions for the USAF or USCG while en route to/from or within the designated search area (USA)[13][18]

1300
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

1400
    * VFR flight above 12,500'ASL when no other code has been assigned (Canada)[6]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

1500
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

1600, 1700
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

2000
    * Civil IFR flights in Class G airspace (Australia)[16]
    * Uncontrolled IFR at or above 18,000' (Canada)[6]
    * The code to be squawked when entering a secondary surveillance radar (SSR) area from a non-SSR area used as Uncontrolled IFR flight squawk code(ICAO countries)[11]
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

2100
    * Ground testing by aircraft maintenance staff (Australia)[16]
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

2200, 2300, 2400
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

2500, 2600, 2700
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

3000
    * Civil flights in classes A, C and D airspace, or IFR flights in Class E airspace (Australia)[16]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

3100, 3200, 3300, 3400, 3500, 3600, 3700
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

4000
    * Civil flights not involved in special operations or SAR, operating in Class G airspace in excess of 15NM offshore (Australia)[16]
    * Aircraft on a VFR Military Training Route or requiring frequent or rapid changes in altitude (USA)[19]
    * Non-discrete code assignments in accordance with FAA Order JO 7110.65, 5-2 *Also for use in oceanic airspace, unless another code is assigned by ATC (USA)[13]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

4100
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

4200, 4300
    * Internal ARTCC subsets assigned by En Route Safety and Operations Support (Discrete codes only except for first primary block to be used as non-discrete if all discrete codes are assigned) (USA)[13]

4400 to 4477
    * Reserved for use by SR-71, YF-12, U-2 and B-57, pressure suit flights, and aircraft operations above FL600 (USA)[13][19]

4401 to 4433
    * Reserved in accordance with FAA Order JO 7110.67 (Fed Law Enforcement) (USA)[13]

4434 to 4437
    * Weather reconnaissance, as appropriate (USA)[13]

4440 to 4441
    * Operations above FL600 for Lockheed/NASA from Moffett Field (USA)[13]

4442 to 4446
    * Operations above FL600 for Lockheed from Air Force Plant 42 (USA)[13]

4447 to 4452
    * Operations above FL600 for SR-71/U-2 operations from Edwards AFB (USA)[13]

4453
    * High balloon operations – National Scientific Balloon Facility, Palestine TX, and other providers, some in international operations (USA)[13]

4454 to 4465
    * Air Force operations above FL600 as designated in FAA Order 7610.4 (USA)[13]

4466 to 4477
    * Reserved in accordance with FAA Order JO 7110.67 (Fed Law Enforcement) (USA)[13]

4500, 4600, 4700
    * Internal ARTCC subsets assigned by En Route Safety and Operations Support (Discrete codes only except for first primary block to be used as non-discrete if all discrete codes are assigned) (USA)[13]

5000
    * Aircraft flying on military operations (Australia)[16]

5000
    * Reserved for use by NORAD (USA and Canada)[13]

5061 to 5062, 5100, 5200
    * Reserved for special use by Potomac TRACON (USA)[13]

5100, 5200, 5300, 5500
    * Internal ARTCC subsets assigned by En Route Safety and Operations Support (Discrete codes only except for first primary block to be used as non-discrete if all discrete codes are assigned) (USA)[13]

5100 to 5300
    * May be used by DOD aircraft beyond radar coverage but inside US controlled airspace with coordination as appropriate with applicable Area Operations Directorate (USA)[13]

5400
    * Reserved for use by NORAD (USA and Canada)[13]

5600, 5700
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

6000
    * Military flights in Class G airspace (Australia)[16]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

6100
    * Reserved for use by NORAD (USA and Canada)[13]

6200, 6300
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

6400
    * Reserved for use by NORAD(USA and Canada)[13]

6500, 6600, 6700
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

7000
    * VFR standard squawk code when no other code has been assigned (ICAO)[11]
    * This code does not imply VFR; 7000 is used as a general conspicuity squawk (UK)[12]
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

7001
    * Used in some countries to identify VFR traffic (France)
    * Sudden military climb out from low-level operations (UK)[12]

7004
    * Aerobatic and display code in some countries (UK)[12]

7010
    * VFR circuit traffic code in the UK

7070 to 079
    * Paradrop activities (France)

7100, 7200, 7300, 7400
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

7500
    * Aircraft hijacking (ICAO, worldwide)[6][13]

7501 to 7577

    * Reserved for use by Continental NORAD Region (CONR) (USA)[13]

7600
    * Radio Failure (Lost Communications) (ICAO, worldwide)[6][13]

7601 to 7607
    * Reserved for special use by FAA (USA)[13]

7610 to 7676
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

7615
    * Civil flights engaged in littoral surveillance (Australia)[16]

7700
    * Emergency (ICAO, worldwide)[6][13]

7701 to 7707
    * Reserved for special use by FAA (USA)[13]

7710 to 7776
    * External ARTCC subsets (Discrete codes of blocks only except for first primary block, which is used as the ARTCC’s non-discrete code if all discrete codes are assigned) (USA)[13]

7777
    * Non-discrete code used by fixed test transponders (RABMs) to check correctness of radar stations (BITE) (Belgium, Germany, Netherlands, USA)
    * DOD interceptor aircraft on active air defense missions and operating without ATC clearance in accordance with FAA Order 7610.4 (USA)[13][20]
}

//**** L O G *******************************************************************

procedure TForm1.StartLog;
begin
  //neues logfile erstellen
  if Logon then begin
    LogFileName := Pfad+'log\logfile'+FormatDateTime('-yymmdd-hhnn',Now)+'.txt';  // Filenamen mit Datum+Uhrzeit
    assignfile(logfile, LogFileName);
    rewrite(logfile);  //aus der Hilfe:  Bei einer Textdatei ist F nach dem Öffnen schreibgeschützt.
    writeln(logfile,'adsbScope-LogFile    ',DateToStr(Date));
    writeln(logfile, caption); // damit die Softwareversion festgehalten ist ist
    logstring := '';
  end;
end;

procedure TForm1.StopLog;
begin
  if logon then begin
    if length(logstring)>0 then writeLnLog('');
    reset(logfile);
    readln(logfile);
     if not EOF(logfile) then readln(logfile);             // zwei Zeilen sind immer da
    if EOF(logfile) then begin
      closefile(logfile);
      DeleteFile(LogFileName);   // stand nichts drinn
    end else closefile(logfile);
  end;
end;

procedure TForm1.writeLog(st : string);
begin
  if LogOn then logstring := logstring+st;
end;

procedure TForm1.writeLnLog(st : string);
begin
  if LogOn then begin
    logstring := logstring+st;
    writeln(logfile, logstring);
    logstring := '';
  end;
end;


//****  E R R O R - L O G ******************************************************
procedure TForm1.StartErrorLog;
begin
  //neues Error-logfile erstellen
  if ErrorLogon then begin
    ErrorLogFileName := Pfad+'log\errorlog'+FormatDateTime('-yymmdd-hhnn',Now)+'.txt'; //Filenamen mit Datum+Uhrzeit
    assignfile(errorlogfile, ErrorLogFileName);
    rewrite(errorlogfile);  //aus der Hilfe:  Bei einer Textdatei ist F nach dem Öffnen schreibgeschützt.
    writeln(errorlogfile,'adsbScope-Error-LogFile    ',DateToStr(Date));
    writeln(logfile, caption); // damit die Softwareversion festgehalten ist ist
  end;
end;


procedure TForm1.StopErrorLog;
begin
  if ErrorLogon then begin
    reset(errorlogfile);
    readln(errorlogfile);
    if not EOF(errorlogfile) then readln(errorlogfile);
    if EOF(errorlogfile) then begin
      closefile(errorlogfile);
      DeleteFile(ErrorLogFileName);   // stand nichts drinn
    end else closefile(errorlogfile);
  end;
end;


// Fehler ins error-log schreiben
procedure TForm1.Elog(st:string);
begin
  if not ErrorLogon then exit;
  LongTimeFormat := 'hh:nn:ss';
  writeln(errorlogfile, TimeToStr(Time) + ' : ' + st);
end;


//**** U S B - L O G ***********************************************************
procedure TForm1.StartUsbLog;
begin
  //neues USBlogfile erstellen
  if USBLogon then begin
    USBLogFileName := Pfad+'log\UsbLog'+FormatDateTime('-yymmdd-hhnn',Now)+'.txt';  // Filenamen mit Datum+Uhrzeit
    assignfile(UsbLogfile, USBLogFileName);
    rewrite(UsbLogfile);  //aus der Hilfe:  Bei einer Textdatei ist F nach dem Öffnen schreibgeschützt.
    writeln(UsbLogfile,'adsbScope-USB-LogFile    ',DateToStr(Date));
    writeln(logfile, caption); // damit die Softwareversion festgehalten ist ist
  end;
end;


procedure TForm1.StopUsbLog;
begin
  if USBLogon then begin
    reset(USBlogfile);
    readln(USBlogfile);
    if not EOF(USBlogfile) then readln(USBlogfile);
    if EOF(USBlogfile) then begin
      closefile(USBlogfile);
      DeleteFile(USBLogFileName);   // stand nichts drinn
    end else closefile(USBlogfile);
  end;
end;


//**** G P S - N M E A *********************************************************
{
$GPGGA,151137.036,5017.2096,N,02444.0265,W,0,00,50.0,0.0,M,,,,0000*25
$GPGLL,5017.2096,N,02444.0265,W,151137.036,V*3D
$GPGSA,A,1,,,,,,,,,,,,,50.0,50.0,50.0*05
$GPGSV,3,1,12,01,90,000,,06,32,000,,20,31,000,,23,27,000,*73
$GPGSV,3,2,12,08,26,000,,30,24,000,,02,20,000,,11,19,000,*7A
$GPGSV,3,3,12,10,18,000,,28,17,000,,24,09,000,,25,08,000,*7E
$GPRMC,151137.036,V,5017.2096,N,02444.0265,W,,,041012,,*0C
$GPVTG,,T,,M,,N,,K*4E

$GPGGA,103934.296,5103.9867,N,00610.8920,E,0,00,50.0,114.0,M,,,,0000*3D
$GPGLL,5103.9867,N,00610.8920,E,103934.296,V*21
$GPGSA,A,1,,,,,,,,,,,,,50.0,50.0,50.0*05
$GPGSV,3,1,10,30,58,294,,21,55,163,,29,49,069,47,31,41,217,32*74
$GPGSV,3,2,10,16,28,300,,25,25,127,,05,17,051,39,06,16,259,*71
$GPGSV,3,3,10,13,07,336,33,23,06,304,*7B
$GPRMC,103934.296,V,5103.9867,N,00610.8920,E,,,061012,,*12
$GPVTG,,T,,M,,N,,K*4E
}


procedure TForm1.use1Click(Sender: TObject);
var k : integer;
begin
  use1.checked   := not use1.checked;
  Timer3.enabled := use1.checked;
  if not (Timer3.enabled) and  ComPort2.Connected then ComPort2.Close;

  st_gps := '';
  GPSdata1.valid := false;
  GPSdata1.date  := 0;
  GPSdata1.time  := 0;
  for k:=0 to 16 do GPSData1.sat[k].PRN :=-1;
end;


procedure TForm1.selectCOMport1Click(Sender: TObject);
begin
  ComPort2.ShowSetupDialog;
end;


procedure TForm1.Timer3Timer(Sender: TObject);
begin
  WorkoffGPS;
end;


// NMEA-Messages con Comport2 abholen und auswerten
procedure TForm1.WorkoffGPS;
var st               : string;
    stSender, stSatz : string;
    nr_rx_gps        : integer;

  // einen string zwischen zwei kommas abschneiden von st  
  function getGpsString:string;
  var ch : char;
  begin
    result:='';
    if st='' then exit;
    while (st[1]<>',') and (st[1]<>'*') and (length(st)>1) do begin
      ch := st[1];
      result := result+ch;
      st := copy(st, 2, length(st)-1);
    end;
    if length(st)>0 then st := copy(st,2, length(st)-1);  // komma entfernen
  end; //getGpsString

  //input  'xxx.yyy'
  //output 'xxx'
  function vor_punkt(st: string):string;
  var pos : integer;
  begin
    result := '';
    pos := AnsiPos('.', st);
    if pos=0 then result:= st else
      if pos>1 then result := copy(st,1,pos-1);
  end;  //vor_punkt

  //input  'xxx.yyy'
  //output 'yyy'
  function nach_punkt(st: string):string;
  var pos : integer;
  begin
    result := '';
    pos := AnsiPos('.', st);
    if pos<>0 then result:= copy(st,pos+1,length(st));
  end; //nach_punkt

  // pruefsumme
  procedure GpsCrc;
  var crc   : byte;
      k     : integer;
      stcrc : string;
  begin
    crc := 0;
    for k:=2 to length(st)-3 do crc := crc xor ord(st[k]);
    stcrc := inttohex(crc, 2);
    if (uppercase(stcrc[1])<>uppercase(st[length(st)-1])) or (uppercase(stcrc[2])<>uppercase(st[length(st)])) then st := '';
  end; //GpsCrc

  // von st_gps eine message abschneiden in in st ablegen
  //$....*xx0A0D
  procedure getGpsMessage;
  var laenge : integer;
      anfang : integer;
      ende   : integer;
  begin
    repeat
      st := '';
      laenge := length(st_gps);
      if laenge<9 then exit;

      //Anfang suchen
      anfang := 0;
      repeat
        inc(anfang);
      until (st_gps[anfang]='$') or (anfang=laenge);
      if (anfang=laenge) then exit;

      //ende suchen
      ende := anfang;
      repeat
        inc(ende);
      until (st_gps[ende]=chr($0A)) or (st_gps[ende]=chr($0D)) or (ende=laenge);
      if (ende=laenge) then exit;

      //nach st kopieren
      st     := copy(st_gps, anfang, ende-anfang);
      st_gps := copy(st_gps, ende, laenge);
      if length(st)>6 then if st[length(st)-2]='*' then GpsCrc;
    until length(st)>6;
  end; //getGpsMessage


  //UTC Time
  //092204.999    hhmmss.sss
  //123519        12:35:19 UTC
  procedure gpsUhrzeit;
  var sta, stb             : string;
      Hour, Min, Sec, MSec : Word;
  begin
    sta := getGpsString;
    if length(sta)<6 then exit;
    stb := vor_punkt(sta);
    if length(stb)<>6 then exit;
    // TDateTime
    // Der ganzzahlige Teil eines TDateTime-Wertes entspricht der Anzahl der Tage seit dem 30.12.1899.
    // Der fraktionale Teil TDateTime-Wertes gibt die Tageszeit an.
    try
      Hour := strtoint(copy(stb,1,2));
      Min  := strtoint(copy(stb,3,2));
      Sec  := strtoint(copy(stb,5,2));
      MSec := 0;
      GPSdata1.time := EncodeTime(Hour, Min, Sec, MSec) + Date;
    except
      exit;
    end;
    //memo1.lines.add('GPS-Time: '+stb[1]+stb[2]+':'+stb[3]+stb[4]+':'+stb[5]+stb[6]);
  end; //gpsUhrzeit

  //Date (dd/mm/yy) xxxxxx 160496
  procedure gpsDatum;
  var stb              : string;
      Day, Month, Year : Word;
  begin
    stb := getGpsString;
    if length(stb)<>6 then exit;
    // TDateTime
    // Der ganzzahlige Teil eines TDateTime-Wertes entspricht der Anzahl der Tage seit dem 30.12.1899.
    // Der fraktionale Teil TDateTime-Wertes gibt die Tageszeit an.
    try
      Day   := strtoint(copy(stb,1,2));
      Month := strtoint(copy(stb,3,2));
      Year  := strtoint(copy(stb,5,2))+2000;
      GPSdata1.date := EncodeDate(Year, Month, Day);
    except
      exit;
    end;
    //memo1.lines.add('GPS-Date: '+stb[1]+stb[2]+':'+stb[3]+stb[4]+':'+stb[5]+stb[6]);
  end; //gpsDatum

  //liest naechsten Parameter aus Message und wandelt in integer
  //fail = -1
  function gpsInteger:integer;
  var sta : string;
  begin
    result := -1;
    sta := getGpsString;
    try
      result := strtoint(sta);
    except
      result := -1
    end;
  end; //gpsInteger

  //liest naechsten Parameter aus Message und wandelt in real
  //fail = 0
  function gpsReal:real;
  var sta, sti, stf : string;
  begin
    result := 0;
    sta := getGpsString;
    sti := vor_punkt(sta);
    stf := nach_punkt(sta);
    try
      if stf='' then stf := '0';
      result := strtoint(stf)/power(10, length(stf) ) + abs(strtoint(sti));
      if strtoint(sti)<0 then result := 0-result;
    except
      result := 0;
    end;
  end; //gpsReal

  //A            Status A=active or V=Void.
  //Status der Bestimmung: A=Active (gültig); V=void (ungültig)
  procedure gpsStatus;
  var sta : string;
  begin
    sta := getGpsString;
    if sta ='' then exit;
    if (sta <>'A') and (sta <>'V') then exit;
    GpsData1.valid := sta='A';
    //memo1.lines.add('Status: '+sta);
  end; //gpsStatus

  //Fix quality
  //0 = invalid, 1 = GPS fix (SPS), 2 = DGPS fix
  //3 = PPS fix, 4 = Real Time Kinematic, 5 = Float RTK
  //6 = estimated (dead reckoning) (2.3 feature)
  //7 = Manual input mode, 8 = Simulation mode
  procedure gpsQualitaet;
  var sta : string;
  begin
    sta := getGpsString;
    if sta ='' then exit;
    try
      GPSdata1.quality := strtoint(sta);
    except;
      exit;
    end;
    //GpsData1.valid := sta='1';
    //memo1.lines.add('Qualitaet: '+sta);
  end; //gpsQualitaet

  //Auto-Auswahl 2D oder 3D Bestimmung
  //A auto,  M manual
  procedure gpsAutoSelection;
  var sta : string;
  begin
    sta := getGpsString;
    if sta ='' then exit;
    if (sta <>'A') and (sta <>'M') then exit;
    //memo1.lines.add('AutoSelection: '+sta);
  end; //gpsAutoSelection

  //1 = no fix, 2 = 2D fix, 3 = 3D fix
  procedure gpsFix;
  var sta : string;
  begin
    sta := getGpsString;
    if sta ='' then exit;
    try
      GpsData1.fix := strtoint(sta);
    except
      exit;
    end;
    //memo1.lines.add('Fix: '+sta+'D fix');
  end; //gpsFix

  //04         0..12
  procedure gpsSatellitenAnzahl;
  var sta : string;
  begin
    sta := getGpsString;
    if sta ='' then exit;
    try
      GpsData1.satnr := strtoint(sta);
    except
      exit;
    end;
    //memo1.lines.add('Satelliten: '+sta);
  end; //gpsSatellitenAnzahl

  //4807.038,N   Latitude 48 deg 07.038' N
  procedure gpsBreitengrad;
  var sta, stb, sti, stf : string;
      breite : real;
  begin
    sta := getGpsString;
    stb := getGpsString;
    if sta ='' then exit;
    if (stb <>'N') and (stb<>'S') then exit;
    sti := vor_punkt(sta);
    stf := nach_punkt(sta);
    try
      breite := strtoint(stf)/power(10, length(stf) ) + strtoint(copy(sti,3,2));
      breite := strtoint(copy(sti,1,2)) + breite/60;
      if stb='S' then breite := 0-breite;
    except
      breite := 0;
      exit;
    end;
    // hier breite an software uebergeben.
    GPSdata1.breiteG := breite;
    //memo1.lines.add('Breitengrad: '+koordtostr_Grad(breite));
  end; //gpsBreitengrad

  //01131.000,E  Longitude 11 deg 31.000' E
  procedure gpsLaengengrad;
  var sta, stb, sti, stf : string;
      laenge : real;
  begin
    sta := getGpsString;
    stb := getGpsString;
    if sta ='' then exit;
    if (stb <>'E') and (stb<>'W') then exit;
    sti := vor_punkt(sta);
    stf := nach_punkt(sta);
    try
      laenge := strtoint(stf)/power(10, length(stf) ) + strtoint(copy(sti,4,2));
      laenge := strtoint(copy(sti,1,3)) + laenge/60;
      if stb='W' then laenge := 0-laenge;
    except
      laenge := 0;
      exit;
    end;
    // hier laenge an software uebergeben.
    GPSdata1.laengeG := laenge;
    //memo1.lines.add('Laengengrad: '+koordtostr_Grad(laenge));
  end; //gpsLaengengrad

  // Magnetic variation (degrees) x.x 13.8
  // MAG_REF Magnetic variation (E = east, W = west) (Note 2) a E
//  procedure gpsMissweisung;
//  begin

//  end; //gpsMissweisung

  procedure gpsHoeheMeer;
  var hoehe : real;
      sta   : string;
  begin
    hoehe := gpsReal;
    sta   := getGpsString;
    if (sta='M') and (hoehe<>0) then GPSData1.hoeheMeer := hoehe;
  end; //gpsHoeheMeer

  procedure gpsHoeheDelta;
  var hoehe : real;
      sta   : string;
  begin
    hoehe := gpsReal;
    sta   := getGpsString;
    if (sta='M') and (hoehe<>0) then GPSData1.HoeheDelta := hoehe;
  end; //gpsHoeheDelta

  //PRNs of satellites used for fix (space for 12)
  // anzeigen welchemaximal 12 Satelliten benutzt werden
  procedure gpsSatx12;
  var k , nr   : integer;
      sta, stb : string;
  begin
    stb := '';
    for k:=1 to 12 do begin
      sta := getGpsString;
      try
        if sta<>'' then begin
          nr := strtoint(sta);
          stb:=stb+inttostr(nr)+'-';
        end;  
      except
        exit;
      end;
    end;
    //memo1.lines.add('Sat: '+stb);
  end; //gpsSatx12

  //data about the satellites that the unit might be able to find
  //based on its viewing mask and almanac data
  procedure SatData(offset : integer);
  var nr : integer;
  begin
    if GPSData1.Sentence<=0 then exit;
    nr := (GPSData1.Sentence-1)*4 + Offset;
    if nr>12 then exit;
    GPSData1.Sat[nr].PRN       := gpsInteger;  //01           Satellite PRN number
    GPSData1.Sat[nr].Elevation := gpsInteger;  //40           Elevation, degrees
    GPSData1.Sat[nr].Azimuth   := gpsInteger;  //083          Azimuth, degrees
    GPSData1.Sat[nr].SNR       := gpsInteger;  //46           SNR - higher is better
  end; //SatData

begin   //WorkoffGPS
  // wenn noetig com-port oeffnen
  if not ComPort2.Connected then
  try
    ComPort2.Open;
    ComPort2.ClearBuffer(true, true);
  except
    Application.MessageBox(
      'Can not open the selected GPS-COM-port'+ chr($0D)+
      'Please check your COM-port-selection',
      'OOPS',  //kopfzeile
      MB_OK
      + MB_ICONWARNING            // gelbes warndreieck
      + MB_APPLMODAL              // user muss ok clicken um weiterzuarbeiten
      + MB_SETFOREGROUND);
    if use1.checked then use1Click(nil);  // timer abschalten
  end;
  if not ComPort2.Connected then exit;

  //lesen vom com-port
  nr_rx_gps := Comport2.InputCount;
  if nr_rx_gps>0 then begin
    nr_rx_gps := Comport2.ReadStr(st, nr_rx_gps);
  end;
  //$ ... *xx0D0A
  //memo2.lines.add(inttostr(nr_rx_gps)+'>>'+st+'<<');
  st_gps := st_gps + st;

  GpsData1.valid := false;

  // messages auswerten
  repeat
    getGpsMessage;
    //memo1.lines.add('Message: '+st);

    if length(st)>9 then if st[1]='$' then begin
      stSender := copy(st,2,2);
      stSatz   := copy(st,4,3);
      getGpsString;
      if stSender ='GP' then begin
        //$GPGGA,151137.036,5017.2096,N,02444.0265,W,0,00,50.0,0.0,M,,,,0000*25
        if stSatz ='GGA' then begin    // fix data
          gpsUhrzeit;
          gpsBreitengrad;
          gpsLaengengrad;
          gpsQualitaet;
          gpsSatellitenAnzahl;
          GPSData1.HDOP := gpsReal;
          gpsHoeheMeer;
          gpsHoeheDelta;
        end else
        //$GPRMC,151137.036,V,5017.2096,N,02444.0265,W,,,041012,,*0C
        if stSatz ='RMC' then begin    // mindestdaten
          gpsUhrzeit;
          gpsStatus;
          gpsBreitengrad;
          gpsLaengengrad;
          GPSData1.SpeedKn         := gpsReal;   //Speed over the ground in knots (real)
          GPSData1.MovingDirection := gpsReal;   //Track angle in degrees True (real)
          gpsDatum;
          //gpsMissweisung;           //Magnetic Variation
        end else

        if stSatz ='GLL' then begin    // wie mit loran-c
          gpsBreitengrad;
          gpsLaengengrad;
          gpsUhrzeit;
          gpsStatus;
        end else

        if stSatz ='GSA' then begin    // Overall Satellite data
          gpsAutoSelection;            //Auto-Auswahl 2D oder 3D Bestimmung
          gpsFix;                      //Art der Positionsbestimmung  no-fix /2D-fix/ 3D-fix
          gpsSatx12;                   //PRNs of satellites used for fix (space for 12)
          GPSData1.PDOP := gpsReal;    //2.5      PDOP (dilution of precision)
          GPSData1.HDOP := gpsReal;    //1.3      Horizontal dilution of precision (HDOP)
          GPSData1.VDOP := gpsReal;    //2.1      Vertical dilution of precision (VDOP)
        end else
        //$GPGSV,3,3,12,10,18,000,,28,17,000,,24,09,000,,25,08,000,*7E
        if stSatz ='GSV' then begin    //Satellites in view
          GPSData1.SentenceMax := gpsInteger; //Number of sentences for full data
          GPSData1.Sentence    := gpsInteger; //sentence 1, 2 or 3
          GPSData1.SatInView   := gpsInteger; //Number of satellites in view
          SatData(0);                         // 0  4  8 12
          SatData(1);                         // 1  5  9 13
          SatData(2);                         // 2  6 10 14
          SatData(3);                         // 3  7 11 15
        end;
      end;  // GP
    end; // if
  until st ='';
  st :='';

  if (GpsData1.valid) then gotoOrt( GPSdata1.breiteG, GPSdata1.laengeG);
  
end; //WorkoffGPS

procedure TForm1.showData1Click(Sender: TObject);
begin
  Application.CreateForm(TGpsForm, GpsForm);
end;

procedure TForm1.defaultposition1Click(Sender: TObject);
begin
  defaultposition1.Checked := true;
end;

procedure TForm1.lastposition1Click(Sender: TObject);
begin
  lastposition1.Checked := true;
end;


procedure TForm1.TestFill;
var
  page    : integer;
  map     : integer;
  nummer  : integer;
  st      : string;
begin
  if not ComPort1.Connected then exit;
  for page := 0 to $1F do begin // $7F do begin
    for map := 0 to $1F do begin
      send_buf[0] := SYS_EEPROM;
      send_buf[1] := 5;           //block lesen
      send_buf[2] := map*8;       //startadresse
      send_buf[3] := 8+$80;       //bytezahl
      send_buf[4] := page;
      Sende_Empfange(5, 16);
      st := '';
      st := inttohex((page shl 8) + (map shl 3), 4)+' : ';
      for nummer:=0 to 7 do begin st := st+' '+inttohex(receive_buf[4+nummer],2); end;
      memo1.lines.add(st);
    end;
  end;
end;

procedure TForm1.TestFill2;
var
  adr    : integer;
  nummer : integer;
  st     : string;
begin
  if not ComPort1.Connected then exit;
  for adr := 0 to $10 do begin
    st := inttohex(adr shl 3, 4)+' : ';
    for nummer := 0 to 7 do begin
      send_buf[0] := SET_MODE;
      send_buf[1] := 0;
      Sende_Empfange(2, 2);
      st := st+' '+inttohex(receive_buf[1],2);
    end;
    memo1.lines.add(st);
  end;
end;

procedure TForm1.DebugFill1Click(Sender: TObject);
begin
  TestFill2;
  memo1.lines.add('---------------');
  TestFill;
end;

//**** I 2 C  ******************************************************************

procedure TForm1.donotuse1Click(Sender: TObject);
begin
  donotuse1.checked := true;
end;

procedure TForm1.C2308C8E001Click(Sender: TObject);
begin
  C2308C8E001.checked := true;
  i2c2tuner;
end;

procedure TForm1.other1Click(Sender: TObject);
begin
  other1.checked := true;
  i2c2tuner;
end;

procedure TForm1.i2c2tuner;
var k : integer;
begin
  if not ComPort1.Connected then exit;
  if donotuse1.checked then exit;

  DecoderPause;
  send_buf[0] := SYS_I2C;     // I2C
  send_buf[1] := 0;           // BSJE3-159A
  if other1.checked then begin
    if (i2cdata[0]>0) and (i2cdata[0]<15) then for k:=0 to i2cdata[0] do send_buf[k+1] := i2cdata[k];
    Sende_Empfange(i2cdata[0]+2, 16);
  end else
    Sende_Empfange(2, 16);
  case receive_buf[0] of
    0 : memo1.lines.add('I2C: failed to claim bus');
    1 : memo1.lines.add('I2C: bus-error');
    2 : memo1.lines.add('I2C: ACK-error');
    3 : memo1.lines.add('I2C: ACK- & bus-error');
  end;
  DecoderRestart;
end;

procedure TForm1.setupindovidualcode1Click(Sender: TObject);
begin
  Form7.ShowModal;
end;

end.
