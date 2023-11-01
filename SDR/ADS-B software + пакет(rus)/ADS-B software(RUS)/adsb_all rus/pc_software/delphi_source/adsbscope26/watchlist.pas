
{*********************************************************************
 * FileName:        watchlist.pas
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
 * this is the watchlist form
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

unit watchlist;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  Grids, StdCtrls, Unit1;

type
  TWatchListForm = class(TForm)
    WatchlistStringGrid: TStringGrid;
    Button1: TButton;
    GroupBox1: TGroupBox;
    EditAA: TEdit;
    StaticText1: TStaticText;
    StaticText2: TStaticText;
    EditID: TEdit;  

    procedure CheckNewPlane(AA : dword; plane:integer);
    procedure Button1Click(Sender: TObject);
    procedure updateWatchlistStringGrid;
    procedure FormCreate(Sender: TObject);
    procedure WatchlistStringGridMouseDown(Sender: TObject;
      Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
    procedure EditAAChange(Sender: TObject);
    procedure EditAAExit(Sender: TObject);
    procedure EditIDChange(Sender: TObject);
    procedure EditIDExit(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
    procedure makevisible(sichtbar : boolean; var position :TFormPos);
  end;

var
  WatchListForm : TWatchListForm;
  SelectedWatch : integer;
  ActiveCol     : integer;

implementation

{$R *.DFM}

procedure TWatchListForm.makevisible(sichtbar : boolean; var position :TFormPos);
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


// Tabelle erstellen
procedure TWatchListForm.updateWatchlistStringGrid;
var k, l : integer;
    af   : Tairframe;
begin
  WatchlistStringGrid.RowCount := maxWatchList;
  WatchlistStringGrid.ColCount := 8;
  with WatchlistStringGrid do
    for k := 0 to ColCount - 1 do
      for l:= 0 to RowCount - 1 do
        Cells[k,l] := '';

  WatchlistStringGrid.Cells[0, 0] := 'No';
  WatchlistStringGrid.Cells[1, 0] := 'Flight';
  WatchlistStringGrid.Cells[2, 0] := 'ICAO24';
  WatchlistStringGrid.Cells[3, 0] := 'Reg';
  WatchlistStringGrid.Cells[4, 0] := 'active';   ActiveCol:=3;
  WatchlistStringGrid.Cells[5, 0] := 'present';
  WatchlistStringGrid.Cells[6, 0] := 'plane-No';
  WatchlistStringGrid.Cells[7, 0] := 'info';
  for k:=1 to maxWatchList do begin
    af := Form1.findairframe(Form1.Watchlist[k].AA);
    WatchlistStringGrid.Cells[0, k] := inttostr(k);
    WatchlistStringGrid.Cells[1, k] := Form1.Watchlist[k].ident;
    WatchlistStringGrid.Cells[2, k] := inttohex(Form1.Watchlist[k].AA, 6);
    if af.known then
      WatchlistStringGrid.Cells[3, k] := af.Kenner.name;
    if Form1.Watchlist[k].active  then
      WatchlistStringGrid.Cells[4, k] := 'active';
    if Form1.Watchlist[k].present then begin
      WatchlistStringGrid.Cells[5, k] := 'present';
      WatchlistStringGrid.Cells[6, k] := inttostr(Form1.Watchlist[k].plane);
    end;
    if Form1.Watchlist[k].info  then
      WatchlistStringGrid.Cells[7, k] := 'on' else
      WatchlistStringGrid.Cells[7, k] := 'off';
  end;
end; //updateWatchlistStringGrid


// ein AA wird in die plane-Tabelle aufgenommen
procedure TWatchListForm.CheckNewPlane(AA : dword; plane:integer);
var k : integer;
begin
  if AA=0 then exit;
  for k:=1 to maxWatchList do if Form1.Watchlist[k].AA=AA then begin
    Form1.Watchlist[k].present := true;
    Form1.Watchlist[k].plane   := plane;     // nummer
    updateWatchlistStringGrid;
  end;
end; //CheckNewPlane


//unsichtbar machen
procedure TWatchListForm.Button1Click(Sender: TObject);
begin
  Form1.Watchlist1Click(nil);
end; //Button1Click


// program start
procedure TWatchListForm.FormCreate(Sender: TObject);
begin
  SelectedWatch := 0;
  ActiveCol     := 3;
  updateWatchlistStringGrid;
  EditAA.Text   := '--';
  EditID.Text   := '';
end; //FormCreate


// Mausklick in die Tabelle
procedure TWatchListForm.WatchlistStringGridMouseDown(Sender: TObject;
  Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
var
  Column, Row: Longint;
begin
  WatchlistStringGrid.MouseToCell(X, Y, Column, Row);                     // kpl. procedure neu
  if row<1 then exit;
  if row>maxWatchList then exit;
  SelectedWatch := Row;
  case column of
    1 : begin   // ID
          updateWatchlistStringGrid;
          EditID.Text := Form1.Watchlist[SelectedWatch].ident ;
          EditID.Text := StringReplace(EditID.Text, ' ', '',[rfReplaceAll]);
          EditID.Setfocus;
        end;
    2 : begin   // ICAO24
          if Form1.Watchlist[SelectedWatch].AA<>0 then begin
            updateWatchlistStringGrid;
            EditAA.Text := inttohex(Form1.Watchlist[SelectedWatch].AA, 6);
          end;
          EditAA.Setfocus;
        end;
    4 : begin   // active
          updateWatchlistStringGrid;
          if Form1.Watchlist[SelectedWatch].AA<>0 then
             Form1.Watchlist[SelectedWatch].active := not Form1.Watchlist[SelectedWatch].active
          else Form1.Watchlist[SelectedWatch].active := false;
          updateWatchlistStringGrid;
        end;
    7 : begin   // info
          updateWatchlistStringGrid;
          Form1.Watchlist[SelectedWatch].info := not Form1.Watchlist[SelectedWatch].info;
          updateWatchlistStringGrid;
        end;
  end;
end;  //WatchlistStringGridMouseDown


// Aenderung der ICAO24 im EditAA-Editorfeld
procedure TWatchListForm.EditAAChange(Sender: TObject);
begin
  if SelectedWatch=0 then exit;
  try
    Form1.Watchlist[SelectedWatch].AA := strtoint('$'+EditAA.text);
    EditAA.font.color := clWindowText;
  except
    on EConvertError do EditAA.font.color := clRed;
  end;
  updateWatchlistStringGrid;
end;  //EditAAChange


// falls was geaendert wurde, dann noch mal sauber reinschreiben
// Aenderung der ICAO24 im EditAA-Editorfeld
procedure TWatchListForm.EditAAExit(Sender: TObject);
begin
  if SelectedWatch=0 then exit;
  EditAA.text := inttohex(Form1.Watchlist[SelectedWatch].AA, 6);
  if Form1.Watchlist[SelectedWatch].AA=0 then Form1.Watchlist[SelectedWatch].active  := false;
  if Form1.Watchlist[SelectedWatch].AA=0 then Form1.Watchlist[SelectedWatch].present := false;
  updateWatchlistStringGrid;
end;  //EditAAExit


// Aenderung der Flight-ID im EditID-Editorfeld
procedure TWatchListForm.EditIDChange(Sender: TObject);
begin
  if SelectedWatch=0 then exit;
  try
    Form1.Watchlist[SelectedWatch].ident := UpperCase(EditID.text);
    EditID.font.color := clWindowText;
  except
    on EConvertError do EditID.font.color := clRed;
  end;
  updateWatchlistStringGrid;
end;  //EditIDChange


// falls was geaendert wurde, dann noch mal sauber reinschreiben
// Aenderung der Flight-ID im EditID-Editorfeld
procedure TWatchListForm.EditIDExit(Sender: TObject);
var k : integer;
begin
  if SelectedWatch=0 then exit;
  Form1.Watchlist[SelectedWatch].ident := UpperCase(EditID.text);
  EditID.text := Form1.Watchlist[SelectedWatch].ident;
  for k:= length(Form1.Watchlist[SelectedWatch].ident) to 8 do Form1.Watchlist[SelectedWatch].ident := Form1.Watchlist[SelectedWatch].ident+' ';
  if Form1.Watchlist[SelectedWatch].AA=0 then Form1.Watchlist[SelectedWatch].active := false;
  updateWatchlistStringGrid;
end;  //EditIDExit


procedure TWatchListForm.FormClose(Sender: TObject;
  var Action: TCloseAction);
begin
  Form1.Watchlist1Click(nil);
end;

end.
