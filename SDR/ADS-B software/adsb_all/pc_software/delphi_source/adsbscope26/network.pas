
{*********************************************************************
 * FileName:        network.pas
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
 * this is the network parameter setup form
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

unit network;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls;

type
  TForm4 = class(TForm)
    GroupBox1: TGroupBox;
    GroupBox2: TGroupBox;
    GroupBox3: TGroupBox;
    ServerPortEdit: TEdit;
    Label1: TLabel;
    Label2: TLabel;
    RawServerPortEdit: TEdit;
    Label3: TLabel;
    RawClientPortEdit: TEdit;
    Label4: TLabel;
    RawClientUrlEdit: TEdit;
    Button1: TButton;
    RAWLocalCheckBox: TCheckBox;
    Button2: TButton;
    clientformatRadioGroup: TRadioGroup;
    GroupBox4: TGroupBox;
    adsbscopeButton: TButton;
    beastButton: TButton;
    rtlButton: TButton;
    sharpButton: TButton;
    procedure FormCreate(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure ServerPortEditChange(Sender: TObject);
    procedure RawClientPortEditChange(Sender: TObject);
    procedure RawClientUrlEditChange(Sender: TObject);
    procedure RawServerPortEditChange(Sender: TObject);
    procedure RAWLocalCheckBoxClick(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure clientformatRadioGroupClick(Sender: TObject);
    procedure adsbscopeButtonClick(Sender: TObject);
    procedure beastButtonClick(Sender: TObject);
    procedure rtlButtonClick(Sender: TObject);
    procedure sharpButtonClick(Sender: TObject);
  private
    { Private-Deklarationen }
    donottouchport : boolean;
  public
    { Public-Deklarationen }
  end;

var
  Form4: TForm4;

implementation

uses Unit1;

{$R *.DFM}


procedure TForm4.FormCreate(Sender: TObject);
begin
  //Das Formular bleibt immer im Vordergrund
  if FormStyle <> fsStayOnTop then FormStyle := fsStayOnTop;
  //Programmfenster zentrieren.
  left := (Screen.Width - Width)   div 2;
  top  := (Screen.Height - Height) div 2;
  ServerPortEdit.Text      := inttostr(Form1.ServerSocket.Port);
  RawServerPortEdit.Text   := inttostr(Form1.ServerSocketRAW.Port);
  RawClientPortEdit.Text   := inttostr(Form1.ClientSocket.Port);
  RawClientUrlEdit.Text    := Form1.ClientSocket.Host;
  RawLocalCheckBox.checked := Form1.LocalRawOnly;
  donottouchport := true;
  if NetUseBinFormat then clientformatRadioGroup.itemindex := 1 else clientformatRadioGroup.itemindex := 0;
  donottouchport := false;
end;


// close button
procedure TForm4.Button1Click(Sender: TObject);
begin
  close;
end;


//change port of server
procedure TForm4.ServerPortEditChange(Sender: TObject);
begin
  try
    Form1.ServerSocket.Port := strtoint(ServerPortEdit.Text);
  except
    on EConvertError do ServerPortEdit.Text    := inttostr(Form1.ServerSocket.Port);
  end;
end; //ServerPortEditChange


//change port of raw-client
procedure TForm4.RawClientPortEditChange(Sender: TObject);
begin
  try
    Form1.ClientSocket.Port := strtoint(RawClientPortEdit.Text);
  except
    on EConvertError do RawClientPortEdit.Text := inttostr(Form1.ClientSocket.Port);
  end;
end; //RawClientPortEditChange


// change url of raw-client
procedure TForm4.RawClientUrlEditChange(Sender: TObject);
begin
  Form1.ClientSocket.Host := RawClientUrlEdit.Text;
end;  //RawClientUrlEditChange

//change port of RAW-server
procedure TForm4.RawServerPortEditChange(Sender: TObject);
begin
  try
    Form1.ServerSocketRAW.Port := strtoint(RAWServerPortEdit.Text);
  except
    on EConvertError do RAWServerPortEdit.Text    := inttostr(Form1.ServerSocketRAW.Port);
  end;
end;

procedure TForm4.RAWLocalCheckBoxClick(Sender: TObject);
begin
  Form1.LocalRawOnly := RawLocalCheckBox.checked;
end;

procedure TForm4.Button2Click(Sender: TObject);
begin
  RawClientUrlEdit.Text := '127.0.0.1';
end;

procedure TForm4.clientformatRadioGroupClick(Sender: TObject);
begin
  if not donottouchport then begin
    case clientformatRadioGroup.itemindex of
      0: begin RawClientPortEdit.Text := '7777'; NetUseBinFormat := false; end;
      1: begin RawClientPortEdit.Text :='31001'; NetUseBinFormat := true;  end;
    end;
  end;
end;

procedure TForm4.adsbscopeButtonClick(Sender: TObject);
begin
  clientformatRadioGroup.itemindex := 1;
  clientformatRadioGroup.itemindex := 0;
end;

procedure TForm4.beastButtonClick(Sender: TObject);
begin
  clientformatRadioGroup.itemindex := 0;
  clientformatRadioGroup.itemindex := 1;
end;

procedure TForm4.rtlButtonClick(Sender: TObject);
begin
  clientformatRadioGroup.itemindex := 0;
  clientformatRadioGroup.itemindex := 1;
end;

procedure TForm4.sharpButtonClick(Sender: TObject);
begin
  clientformatRadioGroup.itemindex := 1;
  clientformatRadioGroup.itemindex := 0;
  RawClientPortEdit.Text :='47806';
end;

end.
