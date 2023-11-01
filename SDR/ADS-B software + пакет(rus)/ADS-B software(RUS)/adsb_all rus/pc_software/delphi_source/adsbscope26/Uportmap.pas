{*********************************************************************
 * FileName:        Uportmap.pas
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
 * this is to idetify a specific airport from ambiguous surface coordinates
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

unit Uportmap;

interface

const
    maxPortMap   = 90 * 30;   //2700 = 7 MByte

type
  // a low resolution map of the airports of the worlf
  // foldet into a 90x90 degree area
  // used to decide in which 1/8 of the world a surface track is
  //  really located - it should be on an aircraft
  TPortMap = class(Tobject)
  private
    B, L : integer;
    Z    : byte;
    Res  : real; // punkte pro Grad
    Map  : array[-1..maxPortMap,-1..maxPortMap] of byte;       // 900x900 = 810 kbyte
    function makeLBZ(breite, laenge:real):byte;
  public
    procedure Init;
    function PutIn(breite, laenge:real):boolean;
    function Test(breite,  laenge:real):byte;
  end;


implementation


//****  P O R T M A P **********************************************************

procedure TPortMap.Init;
var L, B : integer;
begin
  for L:=-1 to maxPortMap do for B:=-1 to maxPortMap do Map[L,B]:=$FF;
  Res := maxPortMap / 90;
end;

function TPortMap.makeLBZ(breite, laenge:real):byte;
begin
  if laenge< 0   then laenge := laenge+360;
  if laenge>=360 then laenge := laenge-360;    // 0..360
  L := round(laenge*Res);
  if breite< -90 then breite := breite+180;    // -90..+90
  if breite>= 90 then breite := breite-180;
  B := round(breite*Res);
  case (L div maxPortMap) of
    0: Z:= $FE;
    1: Z:= $FD;
    2: Z:= $FB;
    3: Z:= $F7;
  end;
  if B<0 then Z := ((Z and $0F) shl 4) or $0F;
  L:= L mod maxPortMap;
  B:= (B+maxPortMap) mod maxPortMap;
  result := Map[L,B];
end;  //makeLBZ


// Eintragen
//False: in anderm Sektor ist platz an gleicher stelle
function TPortMap.PutIn(breite, laenge:real):boolean;
var sektor : byte;
begin
  sektor := Test(breite, laenge); //makeLBZ(breite, laenge);
  result:= (sektor=$FF) or (sektor=Z);
  Map[L,B] := Map[L,B] and Z;
end;  //PutIn


// abfrage nach dem Sektor, in dem ein platz vorhanden ist
// input in grad
function TPortMap.Test(breite, laenge:real):byte;
begin
  result := makeLBZ(breite, laenge) and Map[L+1,B] and Map[L-1,B] and Map[L,B+1] and Map[L,B-1]
  //and Map[L+1,B+1] and Map[L-1,B-1] and Map[L-1,B+1] and Map[L+1,B-1]
  ;
end;


end.
