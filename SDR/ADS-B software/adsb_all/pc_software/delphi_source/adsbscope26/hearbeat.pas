{*********************************************************************
 * FileName:        hearbeat.pas
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
 * this is form to display decoder status information
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
unit hearbeat;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls, Unit1;

const
    maxHeart = 255;

type

  THeartBeat = record
           Uref       : word;        // Decoder HeartBeat-daten
           Usig       : word;
           ErrorHead  : word;
           ErrorData  : word;
           Frames     : word;
           FramesSend : word;
           FramesLost : word;
           DebugMode  : byte;
         end;

  THeartBeatForm = class(TForm)
    UImage: TImage;
    HImage: TImage;
    DImage: TImage;
    Button1: TButton;
    Label1: TLabel;
    Label2: TLabel;
    Label3: TLabel;
    HScaleLabel: TLabel;
    DScaleLabel: TLabel;
    FImage: TImage;
    Label4: TLabel;
    FScaleLabel: TLabel;
    Label5: TLabel;
    Label6: TLabel;
    Label7: TLabel;
    Label8: TLabel;
    Label9: TLabel;
    Label10: TLabel;
    FImage2: TImage;
    Label11: TLabel;
    Label12: TLabel;
    Button2: TButton;
    procedure Button1Click(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure eraseheartdata;
    procedure Button2Click(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
  private
    { Private-Deklarationen }
    Heart    : array[0..maxHeart]of THeartBeat;
    HeartPointer  : integer;
    maxErrorHead  : word;
    maxErrorData  : word;
    maxFrames     : word;
    maxFramesSend : word;
    maxFramesLost : word;
  public
    { Public-Deklarationen }
    procedure makevisible(sichtbar : boolean; var position :TFormPos);
    procedure getdata(DecoderHeartBeat : THeartBeat);
    procedure graphic;
  end;

var
  HeartBeatForm: THeartBeatForm;

implementation

{$R *.DFM}

procedure THeartBeatForm.makevisible(sichtbar : boolean; var position :TFormPos);
begin
  if visible and (not sichtbar) then begin
    //ausblenden
    position.top    := top;
    position.left   := left;
    position.width  := width;
    position.height := height;
  end;
  if (not visible) and sichtbar then begin
    //einblenden
    if position.width > 0 then begin
      top    := position.top;
      left   := position.left;
     end;
  end;
  visible := sichtbar;
end;  // makevisible


procedure THeartBeatForm.Button1Click(Sender: TObject);
begin
  Form1.decoderstatus1Click(nil);
end;

procedure THeartBeatForm.FormCreate(Sender: TObject);
begin
  eraseheartdata;
end;


procedure THeartBeatForm.eraseheartdata;
var k : integer;
begin
  for k:=0 to maxHeart do begin
    Heart[k].Uref       := 0;
    Heart[k].Usig       := 0;
    Heart[k].ErrorHead  := 0;
    Heart[k].ErrorData  := 0;
    Heart[k].Frames     := 0;
    Heart[k].FramesSend := 0;
    Heart[k].FramesLost := 0;
    Heart[k].DebugMode  := 0;
  end;
  HeartPointer := 0;
  maxErrorHead := 1000;
  maxErrorData :=  10;
  maxFrames    :=  10;
  graphic;
end;

procedure THeartBeatForm.getdata(DecoderHeartBeat : THeartBeat);
begin
  Heart[HeartPointer] := DecoderHeartBeat;
  inc(HeartPointer);
  if HeartPointer>maxHeart then HeartPointer := 0;
end;

//
// 500 mV = 100
//    1 V = 200
// Uref und Usig :
//  5V=5120
//  1V=1024 -> --
//0.8V= 819 -> 36
//0.6V= 614 -> 77
procedure THeartBeatForm.graphic;
var k, x      : integer;
    pointer   : integer;
    Y_uref    : integer;
    Y_usig    : integer;
    Y_EHead   : integer;
    Y_Edata   : integer;
    Y_Frames  : integer;
    Y_FramesSend : integer;
    Y_FramesLost : integer;
    olp,urp   : TPoint;
    rechteck  : TRect;

  // ermittelt Maximalwert an der Y-Scale der Grafik  
  function getscale(max: word):word;
  begin
    result:=1000;
    if max<100    then result:= ((max div     10)+1)*    10 else
    if max<1000   then result:= ((max div    100)+1)*   100 else
    if max<10000  then result:= ((max div   1000)+1)*  1000 else
    if max<100000 then result:= ((max div  10000)+1)* 10000 else
                       result:= ((max div 100000)+1)*100000;
  end;

  function UtoY(U:word):word;
  begin
    result := 1;
    if U<1024 then
//    result:= 200-(U-512) div 3;       // 0,5V ... 1,086V
      result:= 200-(U-614) div 3;       // 0,6V ... 1,186V
  end;

begin
  // erase images
  olp.x:=0;
  olp.y:=0;
  urp.x:=UImage.Width;
  urp.y:=UImage.Height;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  UImage.canvas.Brush.Color:=clWhite;
  UImage.canvas.FillRect(Rechteck);
  urp.x:=HImage.Width;
  urp.y:=HImage.Height;
  Rechteck.TopLeft:=olp;
  Rechteck.BottomRight:=urp;
  HImage.canvas.Brush.Color:=clWhite;
  HImage.canvas.FillRect(Rechteck);
  DImage.canvas.Brush.Color:=clWhite;
  DImage.canvas.FillRect(Rechteck);
  FImage.canvas.Brush.Color:=clWhite;
  FImage.canvas.FillRect(Rechteck);
  FImage2.canvas.Brush.Color:=clWhite;
  FImage2.canvas.FillRect(Rechteck);

  maxErrorHead := 1000;
  maxErrorData :=  10;
  maxFrames    :=  10;
  maxFramesSend:=  10;
  maxFramesLost:=  10;
  pointer := HeartPointer;
  for k:=0 to 255 do begin
    x := k*2;
    pointer := (HeartPointer+k) mod (maxHeart+1);
    Y_uref  := UtoY(Heart[pointer].Uref);
    if Heart[pointer].ErrorHead>maxErrorHead   then maxErrorHead  := Heart[pointer].ErrorHead;
    if Heart[pointer].ErrorData>maxErrorData   then maxErrorData  := Heart[pointer].ErrorData;
    if Heart[pointer].Frames>maxFrames         then maxFrames     := Heart[pointer].Frames;
    if Heart[pointer].FramesSend>maxFramesSend then maxFramesSend := Heart[pointer].FramesSend;
    //if Heart[pointer].FramesLost>maxFramesLost then maxFramesLost := Heart[pointer].FramesLost;
    if k=0 then begin
      UImage.canvas.MoveTo(x,Y_uref);
    end else begin
      UImage.canvas.LineTo(x,Y_uref);
    end;
  end;
  maxErrorHead  := getscale(maxErrorHead);
  maxErrorData  := getscale(maxErrorData);
  maxFrames     := getscale(maxFrames);
  maxFramesSend := getscale(maxFramesSend);
  maxFramesLost := getscale(maxFramesLost);
  HScaleLabel.caption := inttostr(maxErrorHead);
  DScaleLabel.caption := inttostr(maxErrorData);
  FScaleLabel.caption := inttostr(maxFrames);
  for k:=0 to 255 do begin
    x := k*2;
    pointer := (HeartPointer+k) mod (maxHeart+1);
    Y_usig       := UtoY(Heart[pointer].Usig);
    Y_EHead      := 99-round(Heart[pointer].ErrorHead  / maxErrorHead  * 98);
    Y_Edata      := 99-round(Heart[pointer].ErrorData  / maxErrorData  * 98);
    Y_Frames     := 99-round(Heart[pointer].Frames     / maxFrames     * 98);
    Y_FramesSend := 99-round(Heart[pointer].FramesSend / maxFramesSend * 98);
    Y_FramesLost := 99-round(Heart[pointer].FramesLost / maxFramesSend * 98);

    if K=255 then begin
      label8.caption  := inttostr(Heart[pointer].ErrorHead);
      label9.caption  := inttostr(Heart[pointer].ErrorData);
      label10.caption := inttostr(Heart[pointer].Frames);
      label11.caption := inttostr(Heart[pointer].FramesSend)+' send';
      label12.caption := inttostr(Heart[pointer].FramesLost)+' lost';
    end;

    if Heart[pointer].DebugMode=0 then begin
      UImage.canvas.pen.color:=clBlack;
      HImage.canvas.pen.color:=clBlack;
      DImage.canvas.pen.color:=clBlack;
      FImage.canvas.pen.color:=clBlack;
      FImage2.canvas.pen.color:=clBlack;
    end else begin
      UImage.canvas.pen.color:=clRed;
      HImage.canvas.pen.color:=clRed;
      DImage.canvas.pen.color:=clRed;
      FImage.canvas.pen.color:=clRed;
      FImage2.canvas.pen.color:=clRed;
    end;

    DImage.canvas.MoveTo(x,DImage.height);   DImage.canvas.LineTo(x,  Y_Edata);
    DImage.canvas.MoveTo(x+1,DImage.height); DImage.canvas.LineTo(x+1,Y_Edata);
    FImage.canvas.MoveTo(x  ,FImage.height); FImage.canvas.LineTo(x  ,Y_Frames);
    FImage.canvas.MoveTo(x+1,FImage.height); FImage.canvas.LineTo(x+1,Y_Frames);

    if k=0 then begin
      UImage.canvas.MoveTo(x,Y_usig);
      HImage.canvas.MoveTo(x,Y_EHead);
      //DImage.canvas.MoveTo(x,Y_Edata);
      //FImage.canvas.MoveTo(x,Y_Frames);
      FImage2.canvas.MoveTo(x,Y_FramesSend);
    end else begin
      UImage.canvas.LineTo(x,Y_usig);
      HImage.canvas.LineTo(x,Y_EHead);
      //DImage.canvas.LineTo(x,Y_Edata);
      //FImage.canvas.LineTo(x,Y_Frames);
      FImage2.canvas.LineTo(x,Y_FramesSend);
    end;
    FImage2.canvas.pixels[x,Y_FramesLost] := clRed;
  end;
end; //graphic

procedure THeartBeatForm.Button2Click(Sender: TObject);
begin
  eraseheartdata;
end;

procedure THeartBeatForm.FormClose(Sender: TObject;
  var Action: TCloseAction);
begin
  Action := caHide;
end;

end.
