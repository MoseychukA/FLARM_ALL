
{*********************************************************************
 * FileName:        comsetup.pas
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
 * this is the setup for com-port
 *  uses allways the custom baud rate to reach 3 Mbaud
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

 unit comsetup;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, CPort;

type
  TComSetup = class(TForm)
    Button1: TButton;
    GroupBox1: TGroupBox;
    PortComboBox: TComboBox;
    BaudrateComboBox: TComboBox;
    Label1: TLabel;
    Label2: TLabel;
    Button2: TButton;
    procedure FormCreate(Sender: TObject);
    procedure updateLists;
    procedure Button2Click(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure FormActivate(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
  end;

var
  ComSetup1: TComSetup;

implementation

uses Unit1;

{$R *.DFM}


procedure TComSetup.FormCreate(Sender: TObject);
begin
  updateLists;
end;


// check for existing com ports
// show last used com-port und last used baudrate
procedure TComSetup.updateLists;
var
 ComPortsStringList:TStringList;
 I ,k : integer;
begin
  ComPortsStringList:=TStringList.Create;
  try
    EnumComPorts(ComPortsStringList); // Gibt mir die vorhandenen Comports zurück
    k := -1;
    PortComboBox.clear;
    for I := 0 to ComPortsStringList.Count - 1 do begin
      PortComboBox.Items.Add(ComPortsStringList[I]);
      if trim(ComPortsStringList[I]) = Form1.ComPort1.Port then k := I;
    end;
    PortComboBox.Itemindex := k;
  finally
    ComPortsStringList.Free;
  end;

  BaudrateComboBox.Itemindex := 0;
  k := -1;
  for I := 0 to BaudrateComboBox.Items.Count - 1 do begin
    if strToint(trim(BaudrateComboBox.Items[I] )) = Form1.ComPort1.CustomBaudRate then k := I;
  end;
  BaudrateComboBox.Itemindex := k;
end; //updateLists


//cancel
procedure TComSetup.Button2Click(Sender: TObject);
begin
  ModalResult := mrCancel;
end;


// OK
procedure TComSetup.Button1Click(Sender: TObject);
begin
  //com port uebernehmen
  Form1.ComPort1.Port := PortComboBox.Items[PortComboBox.Itemindex];
  //baud rate uebernehmen
  Form1.ComPort1.CustomBaudRate := strToint(trim(BaudrateComboBox.Items[BaudrateComboBox.Itemindex]));
  Form1.ComPort1.BaudRate := brCustom;
  ModalResult := mrOK;
end;


procedure TComSetup.FormActivate(Sender: TObject);
begin
  updateLists;
end;

end.
