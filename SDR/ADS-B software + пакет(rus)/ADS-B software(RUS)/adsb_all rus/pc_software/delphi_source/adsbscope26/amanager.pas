{*********************************************************************
 * FileName:        amanager.pas
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

unit amanager;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, unit1, ExtCtrls, URLMon, Buttons;

type
  TAAmanager = class(TForm)
    Button3: TButton;
    InfoMemo: TMemo;
    Button5: TButton;
    Button6: TButton;
    Label1: TLabel;
    HtmlOpenDialog: TOpenDialog;
    UfoGroupBox: TGroupBox;
    UfoMemo: TMemo;
    Button1: TButton;
    Button2: TButton;
    ExtraGroupBox: TGroupBox;
    ExtraMemo: TMemo;
    Button7: TButton;
    AATimer: TTimer;
    BitBtn1: TBitBtn;
    BitBtn2: TBitBtn;
    procedure Loadufofile;
    procedure LoadExtrafile;
    procedure CleanExtrafile;
    procedure FormCreate(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure eraseAllUfos;
    procedure eraseufo(aast : string);
    function getaafromhtml(filename:string):Tairframe;
    function readhtml:Tairframe;
    procedure Button3Click(Sender: TObject);
    procedure Button5Click(Sender: TObject);
    procedure Button6Click(Sender: TObject);
    procedure HtmlToExtra(quitet : boolean);
    procedure Button7Click(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure Downloadicao24plus;
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
    procedure AATimerTimer(Sender: TObject);
    procedure Button8Click(Sender: TObject);
    procedure BitBtn1Click(Sender: TObject);
    procedure BitBtn2Click(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
    procedure makevisible(sichtbar : boolean; var position :TFormPos);
  end;

var
  AAmanager: TAAmanager;

implementation

{$R *.DFM}


// steht auch genauso im hauptformular
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


procedure TAAmanager.makevisible(sichtbar : boolean; var position :TFormPos);
begin
  if visible and (not sichtbar) then begin
    //ausblenden
    AATimer.enabled := false;
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


//Anzeige des UFO-Files
procedure TAAmanager.Loadufofile;
var filename   : string;
begin
  filename := Form1.Pfad+'extra\airframesunknown.txt';
  UfoMemo.Lines.Clear;
  if FileExists(filename) then begin
    try
      UfoMemo.Lines.LoadFromFile(filename);
      UfoGroupBox.Caption := 'my list of '+inttostr(UfoMemo.lines.count)+' unknown aircraft';
    except
      UfoMemo.Lines.add('## File corrupt ##');
      UfoGroupBox.Caption := 'my list of unknown aircraft';
    end;
  end else UfoMemo.Lines.Add('##  no File ##');
end;  //Loadufofile


//Anzeige des Extra-Files
procedure TAAmanager.LoadExtrafile;
var filename   : string;
begin
  filename := Form1.Pfad+'extra\icao24plus1.txt';
  ExtraMemo.Lines.Clear;
  if FileExists(filename) then begin
    try
      ExtraMemo.Lines.LoadFromFile(filename);
      ExtraGroupBox.Caption := 'my list of '+inttostr(ExtraMemo.lines.count)+' known aircraft';
    except
      ExtraMemo.Lines.add('## File corrupt ##');
      ExtraGroupBox.Caption := 'my list of known aircraft';
    end;
  end else ExtraMemo.Lines.Add('##  no File ##');
end; //LoadExtrafile


// löscht doppelte Einträge aus icao24plus1.txt
procedure TAAmanager.CleanExtrafile;
var infile, outfile : Tinfile;
    inname, outname : string;
    st              : string;
    k, l, m         : integer;
    AA              : dword;
    AAtable         : array [0..100000] of dword;
    neu             : boolean;
begin
  inname  := Form1.Pfad+'extra\icao24plus1.txt';
  outname := Form1.Pfad+'extra\icao24plus1.tmp';
  assignfile(infile, inname);
  assignfile(outfile,outname);
  try
    rewrite(outfile);
  except
    infoMemo.lines.add('can not create temp-file');
    exit;
  end;
  try
    reset(infile);
  except
    closefile(outfile);
    infoMemo.lines.add('can not open file of known aircraft');
    exit;
  end;
  k:=0;
  l:=0;
  //3C493A	D-ABIZ	B735	737-530
  while not eof(infile) do begin
    readln(infile, st);
    try
      AA := strtoint('$'+copy(st, 1,6));
    except
      AA :=0;
    end;
    neu := true;
    if l>0 then for m:=0 to l-1 do if aatable[m]=AA then neu := false;
    if neu then begin
      AAtable[l] := AA;
      inc(l);
      writeln(outfile, st);
    end else inc(k);
  end;
  infoMemo.lines.add(inttostr(k)+' entry(s) removed from list of known aircraft');
  closefile(infile);
  closefile(outfile);
  erase(infile);
  rename(outfile, inname);
  LoadExtrafile;
end; //CleanExtrafile


procedure TAAmanager.FormCreate(Sender: TObject);
begin
  Loadufofile;
  LoadExtrafile;
end;


procedure TAAmanager.Button1Click(Sender: TObject);
begin
  eraseAllUfos;
end;


//loesche alle UFOs
procedure TAAmanager.eraseAllUfos;
var infile, outfile : Tinfile;
    inname, outname : string;
    st              : string;
    k               : integer;
begin
  inname  := Form1.Pfad+'extra\airframesunknown.txt';
  outname := Form1.Pfad+'extra\airframesunknown.tmp';
  assignfile(infile, inname);
  assignfile(outfile,outname);
  try
    rewrite(outfile);
  except
    infoMemo.lines.add('can not create temp-file');
    exit;
  end;
  try
    reset(infile);
  except
    closefile(outfile);
    infoMemo.lines.add('can not open file of unknown aircraft');
    exit;
  end;
  k:=0;
  //471EAA:ufo:::
  while not eof(infile) do begin
    readln(infile, st);
    if copy(st, 8,3)<>'ufo' then writeln(outfile, st) else inc(k);
  end;
  closefile(infile);
  closefile(outfile);
  erase(infile);
  rename(outfile, inname);
  infoMemo.lines.add(inttostr(k)+' ufos removed');
  Loadufofile;
end; //eraseAllUfos


//loescht ein bestimmtes UFO
procedure TAAmanager.eraseufo(aast : string);
var infile, outfile : Tinfile;
    inname, outname : string;
    st              : string;
    k               : integer;
begin
  inname  := Form1.Pfad+'extra\airframesunknown.txt';
  outname := Form1.Pfad+'extra\airframesunknown.tmp';
  assignfile(infile, inname);
  assignfile(outfile,outname);
  rewrite(outfile);
  reset(infile);
  k:=0;
  //471EAA:ufo:::
  while not eof(infile) do begin
    readln(infile, st);
    if copy(st, 1,6)<>copy(aast, 1,6) then writeln(outfile, st) else inc(k);
  end;
  infoMemo.lines.add(inttostr(k)+' entry removed from list of unknown aircraft');
  closefile(infile);
  closefile(outfile);
  erase(infile);
  rename(outfile, inname);
  Loadufofile;
end;   //eraseufo


//lese die daten aus diesem html-file
function TAAmanager.getaafromhtml(filename:string):Tairframe;
var htmlfile  : Tinfile;
    f,a,b,c   : char;
    st        : string;
    head, body: boolean;
    AATabelle : array[0..20,0..1] of string;
    T0, T1, k : integer;

  function getstring:string;
  begin
    result:='';
    //suchen nach <td
    b:=' ';
    c:=' ';
    repeat
      a:=b;
      b:=c;
      read(htmlfile, c);
    until ((a='<') and (b='t') and ((c='h') or ((c='d') and (not head)))) or eof(htmlfile);
    if eof(htmlfile) then exit;
    //suchen nach >
    if c='h' then head:=false;
    if (not head) and (c='d') then body:= true;
    repeat read(htmlfile, c); until (c='>') or eof(htmlfile);
    if eof(htmlfile) then exit;
    //alles bis </td einlesen dabei <...> ueberspringen
    repeat
      read(htmlfile, c);
      if c<>'<' then result := result+c else begin
        a:=' ';
        b:=' ';
        repeat
          f:=a;
          a:=b;
          b:=c;
          read(htmlfile, c);
        until (c='>') or ((f='<') and (a='/') and (b='t') and ((c='h') or ((c='d') and (not head))) ) or eof(htmlfile);
      end;
    until ((f='<') and (a='/') and (b='t') and ((c='h') or ((c='d') and (not head))) ) or eof(htmlfile);
  end; //getstring

begin  //getaafromhtml
  T0 := 0;
  T1 := 0;
  head := true; // erst mal nur nach tabellen-header suchen
  body := false;
  result.known:=false;
  {$I-}
  assignfile(htmlfile, filename);
  reset(htmlfile);
  {$I+}
  if (IOResult <> 0) then begin
    try closefile(htmlfile); except ; end;
    exit;
  end;
  repeat
    st := getstring;
    if not body then begin
      AATabelle[T0,0] := trim(st);
      inc(T0);
    end else begin
      AATabelle[T1,1] := trim(st);
      inc(T1);
    end;
//    infomemo.lines.add(st);
  until eof(htmlfile) or (T0>20) or (T1>20);
//  for k:=0 to T0-1 do infomemo.lines.add( AATabelle[k,0]+'  :  '+AATabelle[k,1]);

  for k:=0 to T0-1 do begin
    if AATabelle[k,0]='Registration' then result.kenner.name := AATabelle[k,1];
    if AATabelle[k,0]='Type'         then result.kenner.typs := AATabelle[k,1];
    if AATabelle[k,0]='Model'        then result.kenner.typl := AATabelle[k,1];
    if AATabelle[k,0]='ICAO24'       then
    try
      result.AA := strtoint('$'+AATabelle[k,1]);
    except
      result.AA := 0;
    end;
  end;
  result.known:=result.AA<>0;
  closefile(htmlfile);
end; //getaafromhtml

{
Registration  :  PH-HRK
Manuf.  :  Piaggio
Model  :  P-180
Type  :  P180
c/n  :  1120
i/t  :  L2T
Selcal  :  
ICAO24  :  4844E2
Reg&nbsp;/&nbsp;Opr  :  Rotterdam Private Air BV
built  :  2006
test reg  :  
delivery  :  2006
prev.reg  :  
until  :  
next reg  :  
status  :  active
&nbsp;  :  edit
}

//finde ein html-File und lese sie daten ein
function TAAmanager.readhtml:Tairframe;
var search     : TSearchRec;
    filename   : string;
begin
  result.known := false;
  if FindFirst(htmlpath+'AIRFRAMES*.htm*', faAnyFile, search)=0 then begin
    filename :=htmlpath+search.name;
    result   := getaafromhtml(filename);
    if result.known then DeleteFile(filename);
  end;
  FindClose(search);
end; //readhtml


// close
procedure TAAmanager.Button3Click(Sender: TObject);
begin
  Form1.manageunknownaircraft1Click(nil);
end;


//open webbrowser
procedure TAAmanager.Button5Click(Sender: TObject);
begin
  Form1.Airframesorg1Click(nil);
end;


// check html
procedure TAAmanager.Button6Click(Sender: TObject);
begin
  HtmlToExtra(false);
  AATimer.enabled:= true;
end;

//teste alle 5 Sekunden auf html file
procedure TAAmanager.AATimerTimer(Sender: TObject);
begin
  HtmlToExtra(true);
end;


// check html
procedure TAAmanager.HtmlToExtra(quitet : boolean);
var af : Tairframe;
    filename : string;
    aafile   : Tinfile;
begin
  filename:= Form1.Pfad+'extra\icao24plus1.txt';
  af := readhtml;

  if not af.known then begin
    if not quitet then infomemo.lines.add('no usabele HTML-file');
    exit;
  end;

  assignfile(aafile, filename);
  if not FileExists(filename) then try rewrite(aafile);
  except
    infomemo.lines.add('can not create file of known aircraft');
    exit;
  end;
  try
    append(aafile);
  except
    infomemo.lines.add('can not open file of known aircraft');
    exit;
  end;
  write(aafile, inttohex(af.aa,6), chr($09));
  write(aafile, af.kenner.name, chr($09));
  write(aafile, af.kenner.typs, chr($09));
  write(aafile, af.kenner.typl);
  writeln(aafile);
  closefile(aafile);
  LoadExtrafile;
  eraseufo(inttohex(af.aa, 6));
end;  //HtmlToExtra


procedure TAAmanager.Button7Click(Sender: TObject);
begin
  CleanExtrafile;
end;


//pfad zu den html-Dateien festlegen
procedure TAAmanager.BitBtn2Click(Sender: TObject);
var filename : string;
begin
    HtmlOpenDialog.Filter     := 'AIRFRAMES|*.htm;*.html';
    HtmlOpenDialog.DefaultExt := 'htm';
    HtmlOpenDialog.InitialDir :=  htmlpath;
    if not HtmlOpenDialog.Execute then exit;          { Dialog zum Dateiöffnen anzeigen }
    filename := HtmlOpenDialog.FileName;
    htmlpath := ExtractFileDir(filename)+'\';
    infoMemo.lines.add('Path: '+htmlpath);
end;


// ufo-file loeschen
procedure TAAmanager.Button2Click(Sender: TObject);
var ufofile : Tinfile;
    ufoname : string;
begin
  if Application.MessageBox('Do you really want to erase the list of unknown aircraft?',
        'Warning', MB_OKCANCEL + MB_DEFBUTTON1) = IDOK then begin
    ufoname  := Form1.Pfad+'extra\airframesunknown.txt';
    assignfile(ufofile, ufoname);
    try
      erase(ufofile);
    except
      infoMemo.lines.add('can not erase file of unknown aircraft');
      exit;
    end;  
    Loadufofile;
  end;
end;  //Button2Click


// download icao24plus.txt from my server
procedure TAAmanager.Downloadicao24plus;
var
   url       : string;
   zipname   : string;
   mapserver : string;
   zielpfad  : string;
   was       : string;
   version   : string;
   k         : integer;
   Save_Cursor : TCursor;
   tf          : Tinfile;
begin
  Save_Cursor   := Screen.Cursor;
  Screen.Cursor := crHourglass;    { Cursor als Sanduhr }
  was       := 'icao24plus.zip';
  mapserver := 'http://www.sprut.de/img/files/';
  url       := mapserver + was;
  zielpfad  := Form1.Pfad +'extra\';
  zipname   := zielpfad + was;
  try
    if not fileexists(zipname) then downloadfile(url, zipname);
  finally
    Screen.Cursor := Save_Cursor;  { Alten Zustand wiederherstellen }
  end;

  if fileexists(zipname) then begin
    infomemo.lines.add(inttostr(Form1.zipexpand(zipname, zielpfad))+' file downloaded');
    DeleteFile(zipname);
    // Anzeige der Version
    assignfile(tf,zielpfad+'icao24plus.txt');
    reset(tf);
    readln(tf,version);
    if length(version)>6 then begin
      for k:=1 to 6 do version[k]:=' ';
      infomemo.lines.add(version);
    end;
    closefile(tf);

    Form1.LoadJets(false); // standard
    Form1.LoadJets(true);  // eigenes file
  end else
    infomemo.lines.add('## download failed');
end; //Downloadicao24plus


procedure TAAmanager.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  Form1.manageunknownaircraft1Click(nil);
end;


procedure TAAmanager.Button8Click(Sender: TObject);
begin
  Downloadicao24plus;
end;


procedure TAAmanager.BitBtn1Click(Sender: TObject);
begin
  Downloadicao24plus;
end;  


end.

//Ausgabe:
//010012	SU-GBB	A320	A320-231
//      write(aafile, inttohex((jet[k].aa and $FF0000) shr 16, 2), inttohex((jet[k].aa and $00FFFF), 4), chr($09));
//      write(aafile, jet[k].kenner.name, chr($09));
//      write(aafile, jet[k].kenner.typs, chr($09));
//      write(aafile, jet[k].kenner.typl);
//      writeln(aafile);

