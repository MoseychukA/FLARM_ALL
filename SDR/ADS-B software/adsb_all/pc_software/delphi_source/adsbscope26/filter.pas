
{*********************************************************************
 * FileName:        filter.pas
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
 * this is the filter-form
 * to display the data of only one selected aircraft
 * and to write a log-file with all received data from this aircraft
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
 
unit filter;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ComCtrls, ExtCtrls, Unit1;

type
  TFilterForm = class(TForm)
    MemoF: TMemo;
    FilterLogButton: TButton;
    FilterCloseButton: TButton;
    Edit1: TEdit;
    MemoFL: TMemo;
    ProgressBar1: TProgressBar;
    Timer1: TTimer;
    procedure FormCreate(Sender: TObject);
    procedure FilterCloseButtonClick(Sender: TObject);
    procedure Edit1Change(Sender: TObject);
    procedure Edit1Exit(Sender: TObject);
    procedure FilterLogButtonClick(Sender: TObject);
    procedure Timer1Timer(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
    FilterAA : dword;
    updatetime : TDateTime;
    procedure makevisible(sichtbar : boolean; var position :TFormPos);
    procedure updatedata(nr:integer; fst : string);
    procedure GetAA(AA:dword);
  end;

var
  FilterForm: TFilterForm;
  flogfile : text;
  FilterLogon : boolean = false;

implementation


{$R *.DFM}


procedure TFilterForm.makevisible(sichtbar : boolean; var position :TFormPos);
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
      //width  := position.width;
      //height := position.height;
    end;
  end;
  visible := sichtbar;
end;  // makevisible


// daten vom Hauptprogramm uebernehmen
procedure TFilterForm.updatedata(nr:integer; fst : string);
var st     : string;
    AFrame : TFrame;
    trenn  : char;
    DF, k  : integer;
begin
  if Form1.planes[nr].AA <> FilterAA then exit;
  updatetime := now;

  // Ausgaben in das Textfenster
  MemoF.lines.clear;
  LongTimeFormat := 'hh:nn:ss:zzz';
  MemoF.lines.add('Time             '+timetostr(updatetime));
  MemoF.lines.add('Track            '+inttostr(nr));
  MemoF.lines.add('ICAO24           '+inttohex(Form1.planes[nr].AA,6) );
  MemoF.lines.add('Latitude         '+Form1.koordtostr(Form1.planes[nr].latitude*rad));
  MemoF.lines.add('Longitude        '+Form1.koordtostr(Form1.planes[nr].longitude*rad));
  MemoF.lines.add('Altitude         '+inttostr(Form1.planes[nr].altitude)+' ft');
  MemoF.lines.add('Speed            '+inttostr(round(Form1.planes[nr].speed))+' ktn');
  MemoF.lines.add('Heading          '+inttostr(round(Form1.planes[nr].heading/rad +360) mod 360)+'°');
  MemoF.lines.add('Variometer       '+inttostr(Form1.planes[nr].steigen)+' ft/min');
  MemoF.lines.add('Ident            '+Form1.planes[nr].ident);

  //falls logfile geschrieben werden soll
  if FilterLogon then begin
    trenn :=',';
    st:='';
    // hier hinein alle Ausgaben in das logfile

    //frame raw daten
    AFrame := Frames[framepointer];
    DF       := AFrame.B[0] shr 3;         // Download-Format
    write(flogfile, '*');
    for k:=0 to 13 do write(flogfile, inttohex(AFrame.B[k],2));
    write(flogfile,'; ');
    write(flogfile,trenn);

    //Zeit
    LongTimeFormat := 'hh:nn:ss (zzz)';
    write(flogfile,timetostr(AFrame.T) + trenn);

    //DF
    write(flogfile,'DF',inttostr(DF),':' );
    if DF<10 then write(flogfile,' ' );
    write(flogfile,trenn);

    writeln(flogfile, st, fst);
    MemoFL.lines.add(st + fst);
  end;
end;  //updatedata


procedure TFilterForm.FormCreate(Sender: TObject);
begin
  GetAA(0);
  updatetime := now;
end;  //FormCreate


//belegen des filters mit einer 24-Bit-Adresse
procedure TFilterForm.GetAA(AA:dword);
begin
  if FilterAA <> AA then begin
    MemoF.lines.clear;
    FilterAA   := AA;
    Edit1.text := inttohex(AA,6);
    updatetime := now;
  end;
end;    //GetAA


//filter-Form schliessen
procedure TFilterForm.FilterCloseButtonClick(Sender: TObject);
begin
  GetAA(0);
  Form1.filter1Click(nil);
end;  //FilterCloseButtonClick


// manuelle Eingabe  der Adresse in den editor
procedure TFilterForm.Edit1Change(Sender: TObject);
begin
  try
    FilterAA := strtoint('$'+Edit1.text);
  except
    on EConvertError do Edit1.text := inttohex(FilterAA,6);
  end;
  updatetime := now;
end;  //Edit1Change


// falls was geaendert wurde, dann noch mal sauber reinschreiben
procedure TFilterForm.Edit1Exit(Sender: TObject);
begin
  Edit1.text := inttohex(FilterAA,6);
end;  //Edit1Exit


// start log to file
procedure TFilterForm.FilterLogButtonClick(Sender: TObject);
var st : string;
begin
  //neues logfile erstellen
  if not FilterLogon then begin
    FilterLogon := true;
    FilterLogButton.caption := 'stop log to file';
    ShortDateFormat := 'yyyy_mm_dd';
//    DateSeparator:='/';
    LongTimeFormat := 'hh_nn_ss';
    st := inttohex(FilterAA,6)+'_'+DateToStr(now)+'_'+TimeToStr (now)+'.txt';
    assignfile(flogfile,Form1.Pfad+'log\'+st);
    MemoFL.lines.add('open logfile '+st);
    rewrite(flogfile);  //aus der Hilfe:  Bei einer Textdatei ist F nach dem Öffnen schreibgeschützt.
    st := 'adsbScope-LogFile  for  '+inttohex(FilterAA,6)+'  '+DateToStr(Date);
    writeln(flogfile,st);
    MemoFL.lines.add(st);
  end else begin
    FilterLogon := false;
    FilterLogButton.caption := 'start log to file';
    closefile(flogfile);
    MemoFL.lines.add('logfile closed');
  end;
end;   //FilterLogButtonClick


procedure TFilterForm.Timer1Timer(Sender: TObject);
begin
  ProgressBar1.Position := round((now - updatetime)*24*60*60);  //sekunden
end;   //Timer1Timer

end.
