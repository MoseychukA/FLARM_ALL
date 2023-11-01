unit gps;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, StdCtrls;

type
  TGpsForm = class(TForm)
    GpsImage: TImage;
    Button1: TButton;
    Timer1: TTimer;
    LatLabel: TLabel;
    LongLabel: TLabel;
    FixLabel: TLabel;
    QualLabel: TLabel;
    ValidLabel: TLabel;
    SatLabel: TLabel;
    TimeLabel: TLabel;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    Label5: TLabel;
    Label6: TLabel;
    Label7: TLabel;
    AltLabel: TLabel;
    Label8: TLabel;
    SpeedLabel: TLabel;
    DirLabel: TLabel;
    Label9: TLabel;
    Label10: TLabel;
    HDOPLabel: TLabel;
    VDOPLabel: TLabel;
    Label11: TLabel;
    Label12: TLabel;
    PDOPLabel: TLabel;
    Label14: TLabel;
    dateLabel: TLabel;
    Label15: TLabel;
    procedure clrscr(bm: TImage);
    procedure Button1Click(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure Timer1Timer(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
    procedure GpsUpdate;
  end;

var
  GpsForm: TGpsForm;

implementation

uses Unit1;

{$R *.DFM}


// loeschen eines Bitmap
procedure TGpsForm.clrscr(bm: TImage);
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
  bm.canvas.Brush.Color:=clWhite; //BackColor;
  bm.canvas.FillRect(Rechteck);
end; // clrscr


// alle ausgaben erneuern
procedure TGpsForm.GpsUpdate;
var k , r: integer;
    CX, CY : integer;
    x,  y  : integer;
    sc  : real;
begin
  sc := 2;  // pixel pro grad
  CX := GpsImage.width div 2;
  CY := GpsImage.height div 2;
  clrscr(GpsImage);

  //grid
  GpsImage.canvas.pen.color := clBlack;
  GpsImage.canvas.pen.style := psSolid;
  GpsImage.canvas.brush.style := bsClear;
  GpsImage.canvas.moveto(0,CY);
  GpsImage.canvas.lineto(2*CX,CY);
  GpsImage.canvas.moveto(CX,0);
  GpsImage.canvas.lineto(CX,2*CY);
  for k:=1 to 4 do begin
    r := round(k*22.5*sc);
    GpsImage.canvas.ellipse(cx-r, CY-r, CX+r, CY+r);
  end;
  GpsImage.canvas.pen.color   := clBlack;
  GpsImage.canvas.brush.style := bsClear;
  GpsImage.canvas.textout(CX+10, 15, 'N');
  GpsImage.canvas.textout(CX+10, 2*cy-25, 'S');
  GpsImage.canvas.textout(15, CY-15, 'W');
  GpsImage.canvas.textout(2*CX-20, CY-15, 'E');

  //satellites
  r := 5;
  GpsImage.canvas.pen.color   := clBlack;
  GpsImage.canvas.brush.style := bsSolid;
  for k:=0 to 15 do begin
    if GPSData1.sat[k].prn>=0 then begin
      GpsImage.canvas.brush.color := round(GPSData1.Sat[k].SNR/100*$0000FF00) or round((100-GPSData1.Sat[k].SNR)/100 * $000000FF);
      x := CX+ round(sin(GPSData1.Sat[k].Azimuth) * (90-GPSData1.Sat[k].Elevation) * sc);
      y := CY- round(cos(GPSData1.Sat[k].Azimuth) * (90-GPSData1.Sat[k].Elevation) * sc);
      if GPSData1.Sat[k].SNR >=0 then GpsImage.canvas.brush.style := bsSolid
                                 else GpsImage.canvas.brush.style := bsClear;
      GpsImage.canvas.ellipse(x-r, y-r, x+r, y+r);
      GpsImage.canvas.brush.style := bsClear;
      GpsImage.canvas.textout(x+10, y,   inttostr(GPSData1.sat[k].prn));
      if GPSData1.sat[k].snr>=0 then
        GpsImage.canvas.textout(x+10, y+10,inttostr(GPSData1.sat[k].snr)+'%');
    end;
  end;

  //labels
  if GpsData1.valid then begin
    LatLabel.font.color  := clBlack;
    LongLabel.font.color := clBlack;
    AltLabel.font.color  := clBlack;
  end else begin
    LatLabel.font.color  := clGray;
    LongLabel.font.color := clGray;
    AltLabel.font.color  := clGray;
  end;
  LatLabel.Caption  := Form1.koordtostr_Grad(GPSdata1.breiteG);
  LongLabel.Caption := Form1.koordtostr_Grad(GPSdata1.laengeG);
  AltLabel.Caption  := inttostr(round(GPSData1.hoeheMeer))+' m';
  //AltLabel.Caption  := inttostr(round(GPSData1.hoehe*0.3048))+' m';

  case GPSdata1.fix of
    0: FixLabel.font.color := clRed;
    1: FixLabel.font.color := clRed;
    2: FixLabel.font.color := clPurple;
    else FixLabel.font.color := clBlack;
  end;
  case GPSdata1.fix of
    0: FixLabel.Caption  := 'no fix';
    1: FixLabel.Caption  := '2D';
    2: FixLabel.Caption  := '3D';
    else FixLabel.Caption  := inttostr(GPSdata1.fix);
  end;
  case GPSdata1.quality of
    0: QualLabel.caption := 'no fix';
    1: QualLabel.caption := 'GPS fix';
    2: QualLabel.caption := 'DGPS fix';
    3: QualLabel.caption := 'PPS fix';
    4: QualLabel.caption := 'Real Time Kinematic';
    5: QualLabel.caption := 'Float RTK';
    6: QualLabel.caption := 'estimated';
    7: QualLabel.caption := 'Manual input';
    8: QualLabel.caption := 'Simulation';
    else QualLabel.caption := inttostr(GPSdata1.quality);
  end;
  SatLabel.caption  := inttostr(GpsData1.satnr);

  // Bewegung nur bei > 1 knoten anzeigen
  if GPSdata1.SpeedKn>1 then begin
    //SpeedLabel.caption:= floattostrf(GPSdata1.SpeedKn, ffFixed, 4, 2)+ ' kn';
    //DirLabel.caption  := Form1.koordtostr_Grad(GPSdata1.MovingDirection);
    SpeedLabel.caption:= inttostr(round(GPSdata1.SpeedKn))+ ' kn';
    DirLabel.caption  := inttostr(round(GPSdata1.MovingDirection))+'°';
  end else begin
    SpeedLabel.caption:= '--';
    DirLabel.caption  := '--';
  end;

  PDOPLabel.font.color:=clBlack;
  if GPSdata1.PDOP>10 then PDOPLabel.font.color:=clRed;
  if GPSdata1.PDOP<6  then PDOPLabel.font.color:=clGreen;
  PDOPLabel.Caption := inttostr(round(GPSdata1.PDOP))+'  (3D)';

  HDOPLabel.font.color:=clBlack;
  if GPSdata1.HDOP>10 then HDOPLabel.font.color:=clRed;
  if GPSdata1.HDOP<6  then HDOPLabel.font.color:=clGreen;
  HDOPLabel.Caption := inttostr(round(GPSdata1.HDOP))+'  (2D)';

  VDOPLabel.font.color:=clBlack;
  if GPSdata1.VDOP>10 then VDOPLabel.font.color:=clRed;
  if GPSdata1.VDOP<6  then VDOPLabel.font.color:=clGreen;
  VDOPLabel.Caption := inttostr(round(GPSdata1.VDOP))+'  (1D)';

  TimeLabel.font.color := clBlack;
  LongTimeFormat    := 'hh:nn:ss';
  if GPSdata1.time=0 then TimeLabel.Caption  := '--'
                     else TimeLabel.Caption := timetostr(GPSdata1.time);

  DateLabel.font.color := clBlack;
  ShortDateFormat    := 'dd:mm:yyyy';
  if GPSdata1.date=0 then DateLabel.Caption  := '--'
                     else DateLabel.Caption  := DateToStr(GPSdata1.date);

  if GpsData1.valid then begin
    ValidLabel.font.color := clBlack;
    ValidLabel.caption    :='VALID'
  end else begin
    ValidLabel.font.color := clRed;
    ValidLabel.caption    :='INVALID';
  end;
end;  //GpsUpdate


procedure TGpsForm.Button1Click(Sender: TObject);
begin
  close;
end;

procedure TGpsForm.FormCreate(Sender: TObject);
begin
  GpsUpdate;
end;

procedure TGpsForm.FormActivate(Sender: TObject);
begin
  GpsUpdate;
end;

procedure TGpsForm.Timer1Timer(Sender: TObject);
begin
  GpsUpdate;
end;

end.
