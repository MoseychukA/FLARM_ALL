{*********************************************************************
 * FileName:        gpx.pas
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
 * this is the import of GPX-data
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

unit Ugpx;

//routinen fuer GPX-overlay
{
Grundstruktur
<?xml version="1.0" encoding="utf-8"?>
<gpx version="1.1" creator="Ersteller der Datei">
  <metadata> <!-- Metadaten --> </metadata>
  <wpt lat="xx.xxx" lon="yy.yyy"> <!-- Attribute des Wegpunkts --> </wpt>
  <!-- weitere Wegpunkte -->
  <rte>
    <!-- Attribute der Route -->
    <rtept lat="xx.xxx" lon="yy.yyy"> <!-- Attribute des Routenpunkts --> </rtept>
    <!-- weitere Routenpunkte -->
  </rte>
  <!-- weitere Routen -->
  <trk>
    <!-- Attribute des Tracks -->
    <trkseg>
      <trkpt lat="xx.xxx" lon="yy.yyy"> <!-- Attribute des Trackpunkts --> </trkpt>
      <!-- weitere Trackpunkte -->
    </trkseg>
    <!-- weitere Track-Segmente -->
  </trk>
  <!-- weitere Tracks -->
</gpx>
}

interface

uses unit1, Sysutils, Dialogs;

type

  TGpx = class(Tobject)
  private
    gpxfile  : Tinfile;
    filename : string;
    Pfad : string;
    Zeile : string;
    function fileread:char;
    procedure gtx_read_line;
    function XmlLookGpx:boolean;
    function XmlGetKey:string;
    function XmlLookAny:boolean;
    procedure info(st:string);
    function XmlLookEndKey:boolean;
    procedure gpx_get_koord;
    procedure end_of_trace;
    function gtxoord:Tkoordinaten;
  public
    procedure Init;
    procedure load_all_gpx;
    procedure readgpxfile(name:string);
  end;


var
  Vgpx    : Tgpx;        // in unit ugpx

implementation

//****  G P X - I M P O R T ****************************************************

function TGpx.fileread:char;
begin
  if not eof(gpxfile) then
  repeat
    read(gpxfile, result);
  until ( (result<>chr($0D)) and (result<>chr($0D)) and (result<>chr($09)) ) or eof(gpxfile);
  if eof(gpxfile) then result:=' ';
end;


procedure TGpx.Init;
begin
  filename := '';
end;


// liest aus einem String der Form   lat="52.520448" lon="13.414690"
// die koordinaten aus
function TGpx.gtxoord:Tkoordinaten;
var sta    : string;
    p1, p2 : integer;
    a, b   : integer;
begin
  DecimalSeparator:='.'; // auf deutschen PCs nötig, da im Files '.' steht und warscheinlich ',' erwartet wird.
  result.breite := 0;
  result.laenge := 0;
  result.hoehe:=0;
  result.V := false;
  try
    p1 := pos('lat=', Zeile);  //breite
    p2 := pos('lon=', Zeile);  //laenge
    if (p1=0) or (p2=0) then exit;

    a:=p1;
    repeat inc(a) until (Zeile[a] = '"') or (a >= length(Zeile));
    if a<length(Zeile) then begin
      inc(a);
      b:=a;
      repeat inc(b) until (Zeile[b] = '"') or (b >= length(Zeile));
      if Zeile[b] = '"' then begin
        dec(b);
        sta := copy(Zeile,a,b-a+1);
        result.breite := StrToFloat(sta)*rad;
      end;
    end;

    a:=p2;
    repeat inc(a) until (Zeile[a] = '"') or (a >= length(Zeile));
    if a<length(Zeile) then begin
      inc(a);
      b:=a;
      repeat inc(b) until (Zeile[b] = '"') or (b >= length(Zeile));
      if Zeile[b] = '"' then begin
        dec(b);
        sta := copy(Zeile,a,b-a+1);
        result.laenge := StrToFloat(sta)*rad;
      end;
    end;
    result.V := true;
  except
    result.breite := 0;
    result.laenge := 0;
    result.V      := false; // Fehlerkennzeichen
  end;
  Form1.sincos(result);
end; // gtxoord


//lese koordinaten aus file und schreibe punkt in GPX
procedure TGpx.gpx_get_koord;
begin
  GPX[GPXcounter]:=gtxoord;
  if GPXcounter>=maxGPX then exit;
  inc(GPXcounter);
end;


//Schreibe Endpunkt in GPX
procedure TGpx.end_of_trace;
begin
  GPX[GPXcounter].laenge := 0;
  GPX[GPXcounter].breite := 0;
  GPX[GPXcounter].v      := false;  //?
  Form1.sincos(GPX[GPXcounter]);
  if GPXcounter>=maxGPX then exit;
  inc(GPXcounter);
end;


// go forward until >
function TGpx.XmlLookEndKey:boolean;
var a : char;
begin
  result:=false;
  repeat a:= fileread; until (a='>') or eof(gpxfile);
  result:= (a='>');
end;


//suche nach
//   <gpx....>
//return true wenn gefunden
function TGpx.XmlLookGpx:boolean;
var exp : string;
begin
  result:= false;
  repeat exp:=XmlGetKey; until (exp='gpx') or (exp='');
  if exp='' then exit
            else result:=XmlLookEndKey;
end;


// alles bis > in string Zeile lesen
procedure TGpx.gtx_read_line;
var a : char;
begin
  Zeile:='';
  repeat
    a := fileread;
    Zeile:=Zeile+a
  until eof(gpxfile) or (a='>');
end;


// go to the next X and read the keyword behind the <
function TGpx.XmlGetKey:string;
var a   : char;
    exp : string;
    ok  : boolean;
begin
  exp:='';
  repeat a:=fileread until (a='<') or eof(gpxfile);
  if eof(gpxfile) then exit;
  repeat
    a:=fileread;
    ok:=((a>='a') and (a<='z')) or ((a>='A') and (a<='Z')) or (a='/') or (a='?') ;
    if ok then exp:=exp+a;
  until not ok;
  result:=exp;
end;


//suche nach
//   <wpt....></wpt>
//   <rte>..</rte>  oder
//   <trk>..</trk>
//return true wenn gefunden
//return false fals nicht, oder fals  </gpx> gefunden
function TGpx.XmlLookAny:boolean;
var
  exp : string;
begin
  result:= false;
  exp:=XmlGetKey;
  if eof(gpxfile) then exit;

  if exp='wpt' then begin
    //info('wpt detected');
    result:= true;
    gtx_read_line;
    gpx_get_koord;
    end_of_trace;
  end;

  if exp='rte' then begin
    //info('rte detected');
    result:= true;
    //suche rtept
    repeat
      exp:=XmlGetKey;
      if eof(gpxfile) then exit;
      if exp='rtept' then begin
        //info('rtept detected');
        gtx_read_line;
        gpx_get_koord;
      end;  
    until exp='/rte';
    end_of_trace;
  end;

  if exp='trk' then begin
    //info('trk detected');
    result:= true;
    //suche trkpt
    repeat
      exp:=XmlGetKey;
      if eof(gpxfile) then exit;
      if exp='trkpt' then begin
        //info('trkpt detected');
        gtx_read_line;
        gpx_get_koord;
      end;
      if exp='/trkseg' then end_of_trace;
    until exp='/rte';
    end_of_trace;
  end;

if not  eof(gpxfile) then result:= true;

end;


// display info string
procedure TGpx.info(st:string);
begin
  Form1.Memo1.Lines.Add(st);
end;


// load all gpx-files from the gpx directory
procedure TGpx.load_all_gpx;
var
    search     : TSearchRec;
    filename   : string;
begin
  Pfad := Form1.Pfad;
  if FindFirst(Pfad+'gpx\*gpx', faAnyFile, search)=0 then begin
    filename :=pfad+'gpx\'+search.name ;
    readgpxfile(filename);
    while FindNext(search) = 0 do begin
      filename :=pfad+'gpx\'+search.name ;
      readgpxfile(filename);
    end;
  end;
  FindClose(search);
end;  // load_all_gpx


// read one pgx-file
procedure TGpx.readgpxfile(name:string);
begin
  filename := name;

  if FileExists(filename) then assignfile(gpxfile,filename) else exit;
  {$I-}
  reset(gpxfile);
  {$I+}
  if IOResult <> 0 then begin
    MessageDlg('Datei '+filename+' nicht auffindbar', mtWarning	,[mbOk], 0);
    info('###load error: '+filename);
    exit;
  end;
  info('load file '+ extractfilename(filename));

  if XmlLookGpx then repeat
  until not XmlLookAny;

  closefile(gpxfile);
  info(inttostr(GPXcounter) + ' GPX-points loaded');
end;

end.
