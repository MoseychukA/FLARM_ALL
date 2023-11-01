
{*********************************************************************
 * FileName:        table.pas
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
 * this is the big table with all detected aircraft
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

 unit table;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, Grids, ComCtrls, ToolWin, ImgList, Menus, Unit1;

type
  TTableForm = class(TForm)
    StringGrid1: TStringGrid;
    ToolBar1: TToolBar;
    ToolButton1: TToolButton;
    MainMenu1: TMainMenu;
    hallo1: TMenuItem;
    ImageList1: TImageList;
    Close1: TMenuItem;
    procedure StringGrid1MouseDown(Sender: TObject;
      Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
    procedure StringGrid1SelectCell(Sender: TObject; ACol,
      ARow: Integer; var CanSelect: Boolean);
    procedure FormResize(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure ToolButton1Click(Sender: TObject);
    procedure Back2Mainwindow;
    procedure hallo1Click(Sender: TObject);
    procedure Close1Click(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    procedure makevisible(pos:TFormPos);
    function make_Table : integer;
    procedure setFormLeft(newleft:integer);
    { Public-Deklarationen }
  end;

var
  TableForm: TTableForm;

implementation

uses filter;

{$R *.DFM}

// Behelf: Kopie aus Hauptfile
// liefert Bezeichnung des interrogators
function ifftostr(iff:integer):string;
begin
  //SSS IIII
  //0..15 = II-Code0..15
  if iff<16 then result := 'II-'+inttostr(iff)
            else result := 'SI-'+inttostr(iff-16);
end;   //ifftostr


procedure TTableForm.makevisible(pos:TFormPos);
begin
  if Form1.lockedtomainwindow1.checked then begin
    left    := Form1.left;
    top     := Form1.top;
    width   := Form1.width;
    height  := Form1.height;
  end else begin
    left    := pos.left;
    top     := pos.top;
    width   := pos.width;
    height  := pos.height;
  end;
  visible := true;
  make_Table;
end;

procedure  TTableForm.setFormLeft(newleft:integer);
begin
  left := newleft;
end;


//bei klick in oberste Zeile der Tabelle wird das sortierkriterium
// und die Sortierreichenfolge (aufsteigend absteigend) festgelegt
procedure TTableForm.StringGrid1MouseDown(Sender: TObject;
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



// click in die Tabelle bewirkt Kreuz auf Flugzeug in Grafik
procedure TTableForm.StringGrid1SelectCell(Sender: TObject; ACol,
  ARow: Integer; var CanSelect: Boolean);
var st             : string;
    nr             : integer;
begin
  if ARow=0 then exit; // erledigt StringGrid1MouseDown schon
  st := StringGrid1.Cells[0,ARow];
  if st<> '' then nr:=strtoint(st) else exit;
  //E_Tele3.text := inttohex(planes[nr].AA,6);   //DM61
  if Form1.planes[nr].trackindex >= 0 then begin
    Kreuz.counter := 30;
    Kreuz.nr      := nr;
    nearplane     := nr;
  end;
  FilterForm.getAA(Form1.planes[nr].AA);
  Form1.repaintPPI(false, false);
end; //StringGrid1SelectCell



// Tabelle aller Flugzeuge im Tracker erstellen
//und aufräumen von "verlorenen" flugzeugen
// liefert anzahl der Traks
//
// sortierung entsprechend StringGrid1MouseDown
//  Sortierung       : integer = 0;      // spalte nach der in der Tasbelle sortiert wird   ACol;
//  Sortierrichtung  : boolean = false;  // false=aufsteigend; true=absteigend
function TTableForm.make_Table : integer;
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
      1: result := Form1.planes[a].aa        < Form1.planes[b].aa;
      2: result := Form1.AA2Land(Form1.planes[a].aa) < Form1.AA2Land(Form1.planes[b].aa); // behelf
      3: result := Form1.planes[a].ident     < Form1.planes[b].ident;
      4: result := Form1.planes[a].altitude  < Form1.planes[b].altitude;
      5: result := Form1.planes[a].latitude  < Form1.planes[b].latitude;
      6: result := Form1.planes[a].longitude < Form1.planes[b].longitude;
      7: result := Form1.planes[a].speed     < Form1.planes[b].speed;
      8: result := Form1.planes[a].heading   < Form1.planes[b].heading;
      9: result := Form1.planes[a].steigen   < Form1.planes[b].steigen;
     10: result := Form1.planes[a].airframe.kenner.typs < Form1.planes[b].airframe.kenner.typs;
     11: result := Form1.planes[a].airframe.kenner.typl < Form1.planes[b].airframe.kenner.typl;
     12: result := Form1.planes[a].time_last < Form1.planes[b].time_last;
     13: result := Form1.planes[a].mod3id    < Form1.planes[b].mod3id;
     14: result := Form1.planes[a].distance  < Form1.planes[b].distance;
    end; //case
  end;

begin
  result := 0;
  anzahl := 0;
  // aktive flugzeuge zaehlen und Anzeigereihenfolge entsprechen dem Arrayindex festlegen
  for k:=lastPlane downto 0 do if Form1.planes[k].active then begin
    inc(anzahl);
    Form1.planes[k].Tabellenzeile := anzahl;   //neueste nach oben
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
  for k:=1 to anzahl do Form1.planes[reihe[k]].Tabellenzeile := k;

  // Sortierung umkehren fals gefordert
  if Sortierrichtung then
    for k:=0 to lastPlane do if Form1.planes[k].active then
      Form1.planes[k].Tabellenzeile := anzahl-Form1.planes[k].Tabellenzeile+1;

  StringGrid1.rowcount := anzahl+1;
  if StringGrid1.rowcount>1 then StringGrid1.fixedrows:=1;
  StringGrid1.colcount := 20;
  // default colwidth = 64
  //  col, row  spalte, zeile
  StringGrid1.cells[0,0]  := 'Nr.';      StringGrid1.ColWidths[0] := 40; // Platz fuer 5 Stellen
  //StringGrid1.cells[1,0]  := 'AA';
  StringGrid1.cells[1,0]  := 'ICAO24';
  StringGrid1.cells[2,0]  := 'Regist.';  StringGrid1.ColWidths[2] := 70;
  StringGrid1.cells[3,0]  := 'Ident';    StringGrid1.ColWidths[3] := 65;     //callsign
  StringGrid1.cells[4,0]  := 'Nation';   StringGrid1.ColWidths[4] := 65;     //nation
  StringGrid1.cells[5,0]  := 'Alt';      StringGrid1.ColWidths[5] := 50;
  StringGrid1.cells[6,0]  := 'Lat';
  StringGrid1.cells[7,0]  := 'Long';
  StringGrid1.cells[8,0]  := 'Speed';    StringGrid1.ColWidths[8] := 40;
  StringGrid1.cells[9,0]  := 'Head.';    StringGrid1.ColWidths[9] := 40;
  StringGrid1.cells[10,0] := 'Climb';    StringGrid1.ColWidths[10]:= 50;
  StringGrid1.cells[11,0] := 'Type';
  StringGrid1.cells[12,0] := 'Sub-Type'; StringGrid1.ColWidths[12]:= 90;
  StringGrid1.cells[13,0] := 'T-out';    StringGrid1.ColWidths[13]:= 40;
  StringGrid1.cells[14,0] := 'Squawk';
  StringGrid1.cells[15,0] := 'Dist [km]' ;
  StringGrid1.cells[16,0] := 'Direction' ;
  StringGrid1.cells[17,0] := 'detected at' ;
  StringGrid1.cells[18,0] := 'Interrogator' ;
  StringGrid1.cells[19,0] := 'CPR-Quality' ;

  //StringGrid1.cells[14,0]:='Nr2' ;


  for k:=lastPlane downto 0 do if Form1.planes[k].active then begin
    //inc(zeile);
    zeile:=Form1.planes[k].Tabellenzeile;
    StringGrid1.cells[0,zeile]:=inttostr(k);
    StringGrid1.cells[1,zeile]:=inttohex(Form1.planes[k].aa,6);
    for kk:=2 to StringGrid1.colcount  do StringGrid1.cells[kk,zeile]:='';
    if Form1.planes[k].airframe.known     then StringGrid1.cells[2,zeile]  := Form1.planes[k].airframe.kenner.name
                                          else StringGrid1.cells[2,zeile]  := Form1.AA2Land(Form1.planes[k].aa);
    if Form1.planes[k].ident<>''          then StringGrid1.cells[3,zeile]  := Form1.planes[k].ident;
                                               StringGrid1.cells[4,zeile]  := Form1.AA2Land(Form1.planes[k].aa);
    if Form1.planes[k].altitude<>0        then StringGrid1.cells[5,zeile]  := inttostr(Form1.planes[k].altitude);
    if not Form1.planes[k].airborne       then StringGrid1.cells[5,zeile]  := 'ground';
    if Form1.planes[k].latitude<>0        then StringGrid1.cells[6,zeile]  := Form1.koordtostr_Grad(Form1.planes[k].latitude);
    if Form1.planes[k].longitude<>0       then StringGrid1.cells[7,zeile]  := Form1.koordtostr_Grad(Form1.planes[k].longitude);
    if Form1.planes[k].speed<>0           then StringGrid1.cells[8,zeile]  := inttostr(round(Form1.planes[k].speed));  // in knoten
    if Form1.planes[k].heading<>noheading then StringGrid1.cells[9,zeile]  := Form1.rad2instr(Form1.planes[k].heading, 2);
    if Form1.planes[k].steigen<>0         then StringGrid1.cells[10,zeile] := inttostr(Form1.planes[k].steigen);
    if Form1.planes[k].airframe.known     then StringGrid1.cells[11,zeile] := Form1.planes[k].airframe.kenner.typs;
    if Form1.planes[k].airframe.known     then StringGrid1.cells[12,zeile] := Form1.planes[k].airframe.kenner.typl;
    totseit := round( (now - Form1.planes[k].time_last)*24*60*60);            // Sekunden seit letztem frame
    if not Form1.planes[k].missed         then StringGrid1.cells[13,zeile] := inttostr(totseit)
                                          else StringGrid1.cells[13,zeile] := inttostr(totseit)+' M';
    if Form1.planes[k].mod3id<>0          then StringGrid1.cells[14,zeile] := Form1.planes[k].squawk;
    if (Form1.planes[k].distance<>NoRange)then begin
                                               StringGrid1.cells[15,zeile] := inttostr(round(Form1.planes[k].distance));
                                               StringGrid1.cells[16,zeile] := Form1.rad2instr(Form1.planes[k].azimuth, 2);
                                               end;
    LongTimeFormat := 'hh:nn:ss';              StringGrid1.cells[17,zeile] := TimeToStr(Form1.planes[k].time_first);
    if Form1.planes[k].interogator>=0     then StringGrid1.cells[18,zeile] := ifftostr(Form1.planes[k].interogator);
                                               StringGrid1.cells[19,zeile] := inttostr(Form1.planes[k].CPR_quality);

    //StringGrid1.cells[14,zeile]:=inttostr(Form1.planes[k].nr_nahe);                                               // DM61
  end;
end;  //make_Table


procedure TTableForm.FormResize(Sender: TObject);
begin
  StringGrid1.left  := 24;
  StringGrid1.width := TableForm.width - (2 * StringGrid1.left);
  StringGrid1.top   := 40;
  StringGrid1.height:= TableForm.height-StringGrid1.top-70;
end; //


procedure TTableForm.FormCreate(Sender: TObject);
begin
  FormResize(nil);
end;


procedure TTableForm.ToolButton1Click(Sender: TObject);
begin
  Back2Mainwindow;
end;


procedure TTableForm.Back2Mainwindow;
begin
  Form1.visible := false;
  Form1.visible := true;
end;


procedure TTableForm.hallo1Click(Sender: TObject);
begin
  Back2Mainwindow;
end;


procedure TTableForm.Close1Click(Sender: TObject);
begin
  visible := false;
end;

end.
