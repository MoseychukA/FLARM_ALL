
{*********************************************************************
 * FileName:        framerate.pas
 * Dependencies:    See uses section below
 * System:          Win32 (WinXP)
 * Compiler:        Delphi 5
 * Company:         sprut
 * Copyright:       2007-2012 Joerg Bredendiek (sprut)
 * Homepage :       www.sprut.de
 *
 ********************************************************************}

 {*********************************************************************
 * part of adsbScope
 * software to visualize adsb-data
 * this is the framerate-form
 * to visualize the framerate of the decoder over the time
 * for my decoder the comparator-offset-voltage is shown too
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

 unit framerate;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls;

type
  TForm3 = class(TForm)
    ImageR: TImage;
    Button1: TButton;
    Button2: TButton;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    Label4: TLabel;
    procedure Button1Click(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure makegrafik;
    procedure FormActivate(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure ImageRMouseMove(Sender: TObject; Shift: TShiftState; X, Y: Integer);
    procedure showparameters(index : integer);
  private
    { Private-Deklarationen }
  public
    xStep        : real;
    { Public-Deklarationen }
    procedure updatepicture;
  end;

var
  Form3: TForm3;

implementation

uses Unit1;

{$R *.DFM}


// close-button
procedure TForm3.Button1Click(Sender: TObject);
begin
  Form1.framerateactive:=false;
  close;
end;


procedure TForm3.FormCreate(Sender: TObject);
begin
//  inherited Create;
  if FormStyle <> fsStayOnTop then FormStyle := fsStayOnTop;
  //Programmfenster zentrieren.
  left := (Screen.Width - Width)   div 2;
  top  := (Screen.Height - Height) div 2;
  doublebuffered := true;
  Form1.framerateactive := true;
  xStep := 0;
  Label1.caption := '';
  Label2.caption := '';
  Label3.caption := '';
  Label4.caption := '';
  makegrafik;
end;  //FormCreate


procedure TForm3.updatepicture;
begin
  makegrafik;
end;


procedure TForm3.makegrafik;
var
  olp,urp      : TPoint;
  rechteck     : TRect;
  k            : integer;
  FrMax, FrMin : integer;
  Fr           : integer;
  LevelMin,
  LevelMax     : integer;
  Level        : integer;
  yFStep       : real;
  yLStep       : real;
  x, y         : integer;
  stufe        : integer;
  durch        : real;
  Year, Month, Day, Hour1, Hour2, Min, Sec, MSec: Word;

begin
  if Form1.Frameratenindex < 2     then exit;
  if Form1.framerateactive = false then exit;

  // bild loeschen
  olp.x:=0;
  olp.y:=0;
  urp.x:=ImageR.Width;
  urp.y:=ImageR.Height;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  ImageR.canvas.Brush.Color:=clWhite;
  ImageR.canvas.FillRect(Rechteck);

  // beste skalierung ermitteln
  FrMax    := 0;
  FrMin    := 1000000;
  LevelMax := 0;
  LevelMin := 5000;
  for k:=0 to (Form1.Frameratenindex-1) do begin
    if Form1.Frameraten[k].rate>FrMax     then FrMax:=   Form1.Frameraten[k].rate;
    if Form1.Frameraten[k].rate<FrMin     then FrMin:=   Form1.Frameraten[k].rate;
    if Form1.Frameraten[k].level>LevelMax then LevelMax:=Form1.Frameraten[k].level;
    if Form1.Frameraten[k].level<LevelMin then LevelMin:=Form1.Frameraten[k].level;
  end;
  if FrMax<1000 then FrMax := ((FrMax+100 ) div 100 )*100
                else FrMax := ((FrMax+1000) div 1000)*1000;
  if FrMin>1000 then FrMin := ((FrMin-1000) div 1000)*1000 else FrMin := 0;
  LevelMax := LevelMax+20;
  LevelMin := LevelMin-20;
  yFStep := ImageR.height / (FrMax-FrMin);
  yLStep := ImageR.height / (LevelMax-LevelMin);

  // horizontales gitter fuer framerate
  ImageR.canvas.pen.color:=clltBlue;     //Gray;
  ImageR.canvas.pen.style:=psDot;
  Fr    := FrMin;
  stufe := 5000;
  if (FrMax-FrMin)<64000 then stufe :=16000;
  if (FrMax-FrMin)<32000 then stufe := 8000;
  if (FrMax-FrMin)<16000 then stufe := 4000;
  if (FrMax-FrMin)< 8000 then stufe := 2000;
  if (FrMax-FrMin)< 4000 then stufe := 1000;
  if (FrMax-FrMin)< 2000 then stufe :=  500;
  if (FrMax-FrMin)< 1000 then stufe :=  250;
  if (FrMax-FrMin)<  500 then stufe :=  100;
  repeat
    y := ImageR.height - round((Fr-FrMin) * yFStep);
    ImageR.canvas.MoveTo(0,y);
    ImageR.canvas.LineTo(ImageR.Width,y);
    ImageR.canvas.font.color:=clBlue;
    ImageR.canvas.Textout(5, y+1, inttostr(Fr));
    Fr := Fr+stufe;
  until Fr> FrMax;

  // horizontales gitter fuer offset
  if LevelMax>20 then begin
    ImageR.canvas.pen.color:=clltRed;
    ImageR.canvas.pen.style:=psDot;
    Level := LevelMin;
    repeat
      y := ImageR.height - round((Level-LevelMin) * yLStep);
      ImageR.canvas.MoveTo(0,y);
      ImageR.canvas.LineTo(ImageR.Width,y);
      ImageR.canvas.font.color:=clRed;
      ImageR.canvas.Textout(ImageR.Width-40, y+1, inttostr(Level)+'mV');
      Level := Level+20;
    until Level> LevelMax;
  end;

  xStep := (ImageR.Width-1) / (Form1.Frameratenindex-1);

  //vertikales Stundenraster
  ImageR.canvas.pen.color:=clGray;
  ImageR.canvas.pen.style:=psSolid;
  for k:=1 to Form1.Frameratenindex-1 do begin
    x := round(k * xstep);
    DecodeTime(Form1.Frameraten[k-1].zeit, Hour1, Min, Sec, MSec);
    DecodeTime(Form1.Frameraten[k].zeit  , Hour2, Min, Sec, MSec);
    if Hour1<Hour2 then begin
      ImageR.canvas.MoveTo(x,0);
      ImageR.canvas.LineTo(x,ImageR.height);
    end;
  end;

  //frames per aircraft and minute , green
  ImageR.canvas.pen.color:=clGreen;
  ImageR.canvas.pen.style:=psSolid;
  for k:=2 to Form1.Frameratenindex-1 do begin
    x := round(k * xstep);
    durch := (Form1.Frameraten[k-2].fpa + Form1.Frameraten[k-1].fpa + Form1.Frameraten[k].fpa) / 3;
    y := ImageR.height - round(durch * ImageR.height/500);
    if y<1 then y:=1;
    if k=2 then ImageR.canvas.MoveTo(x,y)
           else ImageR.canvas.LineTo(x,y);
  end;

  // Frameratenlinie
  ImageR.canvas.pen.color:=clBlue;
  ImageR.canvas.pen.style:=psSolid;
  for k:=0 to Form1.Frameratenindex-1 do begin
    x := round(k * xstep);
    y := ImageR.height - round((Form1.Frameraten[k].rate-FrMin) * yFStep);
    if k=0 then ImageR.canvas.MoveTo(x,y)
           else ImageR.canvas.LineTo(x,y);
  end;

  //Levellinie
  if LevelMax>20 then begin
    ImageR.canvas.pen.color:=clRed;
    for k:=0 to Form1.Frameratenindex-1 do begin
      x := round(k * xstep);
      y := ImageR.height - round((Form1.Frameraten[k].level-LevelMin) * yLStep);
      if k=0 then ImageR.canvas.MoveTo(x,y)
             else ImageR.canvas.LineTo(x,y);
    end;
  end;

  //letzten datensatz numerisch anzeigen
  showparameters(Form1.Frameratenindex-1);
end;   //makegrafik


procedure TForm3.FormActivate(Sender: TObject);
begin
  makegrafik;
end;


// erase statistics
procedure TForm3.Button2Click(Sender: TObject);
begin
  Form1.Frameratenindex := 0;
  makegrafik;
end;


//wird maus ueber die grafik bewegt, dann darunterliegenden datensatz numerisch anzeigen
procedure TForm3.ImageRMouseMove(Sender: TObject; Shift: TShiftState; X,
  Y: Integer);
var
   index : integer;
begin
  if xStep=0 then exit;
  index := round(X / xStep);
  if index > 1000 then exit;
  if index >= Form1.Frameratenindex then exit;
  showparameters( index );
end;   //ImageRMouseMove


// numerische anzeige eines datensatzes
procedure TForm3.showparameters(index : integer);
var
   avg   : integer;
begin
  LongTimeFormat := 'hh:nn:ss';
  Label1.caption := timetostr(Form1.Frameraten[index].zeit);
  Label2.caption := inttostr( Form1.Frameraten[index].rate) + ' fpm';
  if Form1.Frameraten[index].level<>0 then
    Label3.caption := inttostr( Form1.Frameraten[index].level)    + ' mV';
  if index >= 2 then begin
    avg := round((Form1.Frameraten[index-2].fpa + Form1.Frameraten[index-1].fpa + Form1.Frameraten[index].fpa)/3);
    Label4.caption := inttostr(avg) + ' fpma';
  end else label4.caption:='';
end;  //showparameters

end.
