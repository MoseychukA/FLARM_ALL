 {*********************************************************************
 * FileName:        gototown.pas
 * Dependencies:    See uses section below
 * System:          Win32 (WinXP)
 * Compiler:        Delphi 5
 * Company:         sprut
 * Copyright:       2007-2012 Joerg Bredendiek (sprut)
 * Homepage :       www.sprut.de
 *
 ********************************************************************}


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
unit gototown;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls;

type
  TForm5 = class(TForm)
    TownComboBox: TComboBox;
    Button1: TButton;
    PortComboBox: TComboBox;
    Label1: TLabel;
    Label2: TLabel;
    Button2: TButton;
    procedure Button1Click(Sender: TObject);
    procedure updatedropdownlisteTowns(st:string);
    procedure updatedropdownlistePorts(st:string);
    procedure FormCreate(Sender: TObject);
    procedure TownComboBoxChange(Sender: TObject);
    procedure gotothetown;
    procedure gototheport;
    procedure PortComboBoxChange(Sender: TObject);
    procedure Button2Click(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
    breite_old : real;
    laenge_old : real;
  end;

var
  Form5: TForm5;

implementation

uses Unit1;

{$R *.DFM}

procedure TForm5.Button1Click(Sender: TObject);
begin
  close;
end;

procedure TForm5.updatedropdownlisteTowns(st:string);
var k    : integer;
    lang : integer;
begin
  TownComboBox.Text := st;
  TownComboBox.SelStart := length(TownComboBox.Text);
  TownComboBox.droppeddown := true;
  st   := uppercase(st);
  TownComboBox.Items.clear;
  lang := length(st);
  for k:= 0 to tcounter do begin
    if uppercase(copy(T[k].langname,1,lang))=st then TownComboBox.Items.Add( T[k].langname );
  end;
end;  //updatedropdownlisteTowns


function portbez(k : integer):string;
var bez : string;
begin
  bez:='';
  if Form1.normal2.checked then bez := Port[k].name;
  if Form1.ICAO2.checked   then bez := Port[k].ICAO;
  if Form1.IATA2.checked   then begin bez := Port[k].IATA; if bez='N/A' then bez:='-'; end;
  result := bez;
end;


procedure TForm5.updatedropdownlistePorts(st:string);
var k    : integer;
    lang : integer;
begin
  PortComboBox.Text := st;
  PortComboBox.SelStart := length(PortComboBox.Text);
  PortComboBox.droppeddown := true;
  st   := uppercase(st);
  PortComboBox.Items.clear;
  lang := length(st);
  for k:= 0 to Portcounter do begin
    if portbez(k)<>'-' then if uppercase(copy(portbez(k),1,lang))=st then PortComboBox.Items.Add( portbez(k) );
  end;
end;  //updatedropdownlistePorts


procedure TForm5.FormCreate(Sender: TObject);
begin
  updatedropdownlisteTowns('');
  updatedropdownlistePorts('');
  breite_old := ZentB;  // radiant
  laenge_old := ZentL;
end; //FormCreate


procedure TForm5.TownComboBoxChange(Sender: TObject);
begin
  gotothetown;
end;


procedure TForm5.gotothetown;
var st : string;
    k  : integer;
    lang : integer;
begin
  st := uppercase(TownComboBox.Text);
  for k:= 0 to tcounter do begin
    if uppercase(T[k].langname)=st then begin
      Form1.gotoOrt(T[k].koord.breite/rad, T[k].koord.laenge/rad);
      break;
    end;
  end;
end; //gotothetown


procedure TForm5.gototheport;
var st : string;
    k  : integer;
    lang : integer;
begin
  st := uppercase(PortComboBox.Text);
  for k:= 0 to Portcounter do begin
    if portbez(k)<>'-' then if uppercase(portbez(k))=st then begin
      Form1.gotoOrt(Port[k].koord.breite/rad, Port[k].koord.laenge/rad);
      break;
    end;
  end;
end;   //gototheport


procedure TForm5.PortComboBoxChange(Sender: TObject);
begin
  gototheport;
end;


procedure TForm5.Button2Click(Sender: TObject);
begin
  Form1.gotoOrt(breite_old/rad, laenge_old/rad);
end;

end.
