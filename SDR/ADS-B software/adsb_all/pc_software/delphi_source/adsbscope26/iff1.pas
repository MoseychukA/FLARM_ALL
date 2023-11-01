
{*********************************************************************
 * FileName:        iff1.pas
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
 * this shows a timeline with IFF replies
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
 unit iff1;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  ExtCtrls, StdCtrls;

const
    pixelprosekunde = 20;         // 50 ms pro pixel
    linkerRand      = 30;
    maxiffrx        = 1200;
    noiff           = $100;

type
  TiffForm = class(TForm)
    iffImage: TImage;
    startLabel: TLabel;
    jetztLabel: TLabel;
    rpmLabel: TLabel;
    procedure iffImageMouseMove(Sender: TObject; Shift: TShiftState; X,
      Y: Integer);
    procedure iffImageMouseDown(Sender: TObject; Button: TMouseButton;
      Shift: TShiftState; X, Y: Integer);
  private
    { Private-Deklarationen }
    actiff  : array[0..14] of integer;
    iffrpm  : array[0..14] of integer;
    iffdata : array[0..14,0..maxiffrx] of word;
    ifftimezero : TDatetime;
    startzeit   : TDatetime;
    jetztzeit   : TDatetime;
    function timetox(zeit : TDatetime):integer;
    function xtotime(x : integer): TDatetime;
    function getRpm(nr:integer):integer;
    function getzeile(nr:integer):integer;
  public
    { Public-Deklarationen }
    procedure init(mode : integer);
    procedure corelate;
    procedure iffrx(iffnr:integer; tracknr:integer; farbe:Tcolor);
  end;

var
  iffForm: TiffForm;

implementation

{$R *.DFM}

// liefert Bezeichnung des interrogators
// copypasta aus dem main-form
function ifftostr(iff:integer):string;
begin
  //SSS IIII
  //0..15 = II-Code0..15
  if iff<16 then result := 'II-'+inttostr(iff)
            else result := 'SI-'+inttostr(iff-16);
end;   //ifftostr


function TiffForm.timetox(zeit : TDatetime):integer;
begin
  result := round( (zeit-startzeit) *24 *60 *60 *pixelprosekunde) + linkerRand; // 100ms aufloesung
end;


function TiffForm.xtotime(x : integer): TDatetime;
begin
  result := (x-linkerRand) /pixelprosekunde/60/60/24 + ifftimezero
end;


// zum IFF passende Zeile in der Grafik suchen bzw. festlegen
function TiffForm.getzeile(nr:integer):integer;
var k : integer;
begin
  result:=-1;
  for k:=0 to 14 do if actiff[k]=nr then result:=k;
  if result<0 then begin
    k:=-1;
    repeat
      inc(k);
      if actiff[k] = -1 then begin
        actiff[k] := nr;
        result    := k;
        iffimage.canvas.textout(5,k*15, ifftostr(nr));
      end;
    until (k>14) or (result>=0);
  end;  
end;  //getzeile


procedure TiffForm.init(mode : integer);
var
    olp,urp   : TPoint;
    rechteck  : TRect;
    k, l      : integer;
begin
  // grafik vorbereiten
  iffimage.canvas.Brush.Color:= clWhite;
  iffimage.Canvas.FillRect(ClientRect);
  olp.x:=0;
  urp.x:=iffimage.Width;
  iffimage.canvas.Brush.Color:= $00F0F0F0;
  for k:=0 to 6 do begin
    olp.y:=k*30+15;
    urp.y:=k*30+30;
    Rechteck.TopLeft:=olp;
    Rechteck.BottomRight:=urp;
    iffimage.canvas.FillRect(Rechteck);
  end;

  for k:=0 to 14 do begin
    if mode=0 then actiff[k] := -1;      //nur bei Erstinitialisierung
    for l:=0 to maxiffrx do iffdata[k,l] := noiff;
  end;

  ifftimezero := now;
  startzeit   := ifftimezero;
  LongTimeFormat := 'hh:nn:ss.zzz';
  startLabel.caption := timetostr(startzeit);
end;  //init


procedure TiffForm.corelate;
var k, l    : integer;
    rpm     : integer;
    periode : real;
begin
  k:=1;
  //cleanup, rangewalk-function
  for k:=0 to maxiffrx-3 do begin
    for l := 0 to 14 do
      if iffdata[l,k]<> noiff then
        if iffdata[l,k]=iffdata[l,k+1] then iffdata[l,k]:=noiff;
        if iffdata[l,k]=iffdata[l,k+2] then iffdata[l,k]:=noiff;
        if iffdata[l,k]=iffdata[l,k+3] then iffdata[l,k]:=noiff;
  end;

  for l := 0 to 14 do iffrpm[l] := getRpm(l);

end;


function TiffForm.getRpm(nr:integer):integer;
var rpm     : integer;
    periode : real;
begin
  result := 0;
  if nr>14 then exit;
  if nr<0  then exit;
  for rpm:=4 to 16 do begin
    periode := 60/rpm * pixelprosekunde;  //300 .. 75


  end;

end;




// aufruf vom hauptprogramm
// iffnr   = interrogatorID
// tracknr =  nr des flugzeuges/Tracks
procedure TiffForm.iffrx(iffnr:integer; tracknr:integer; farbe:Tcolor);
var k     : integer;
    x, y  : integer;
    Zeile : integer;
    s     : integer;
    maske : word;
begin
  zeile:= getzeile(iffnr);
  if zeile<0 then exit;
  maske := 1 shl zeile;
  x:= timetox(now);
  if x> 1200 then begin
    corelate;
    init(1);
  end else begin
    s:=x-linkerrand;
    if (s<=maxiffrx) and (s>=0) then iffdata[zeile,x] := tracknr mod $10000;
    y:=zeile*15;
    iffimage.canvas.pen.color := farbe;
    iffimage.canvas.moveto(x,y);
    iffimage.canvas.lineto(x,y+10);
  end;  
end; //iffrx


procedure TiffForm.iffImageMouseMove(Sender: TObject; Shift: TShiftState;
  X, Y: Integer);
var
  periode, rpm : real;
begin
  jetztzeit := xtotime(x);
  LongTimeFormat := 'hh:nn:ss.zzz';
  jetztLabel.caption := timetostr( jetztzeit );
  if jetztzeit=startzeit then exit;
  periode := 1/ (jetztzeit-startzeit);
  rpm := periode /24/60;
  rpmLabel.caption := FloatToStrF(rpm, ffFixed, 6, 1);
end;  //iffImageMouseMove


procedure TiffForm.iffImageMouseDown(Sender: TObject; Button: TMouseButton;
  Shift: TShiftState; X, Y: Integer);
begin
  startzeit      := xtotime(x);
  LongTimeFormat := 'hh:nn:ss.zzz';
  startLabel.caption := timetostr(startzeit);
end;  //iffImageMouseDown

end.
