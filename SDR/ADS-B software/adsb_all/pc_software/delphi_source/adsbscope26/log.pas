{*********************************************************************
 * FileName:        log.pas
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
unit log;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls;

type
  TForm6 = class(TForm)
    Button1: TButton;
    GroupBox1: TGroupBox;
    debMainCheckBox: TCheckBox;
    debErrorCheckBox: TCheckBox;
    debUsbCheckBox: TCheckBox;
    LogTypRadioGroup: TRadioGroup;
    LogLongRadioGroup: TRadioGroup;
    GroupBox2: TGroupBox;
    logSelCheckBox: TCheckBox;
    logDetCheckBox: TCheckBox;
    procedure Button1Click(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure LogTypRadioGroupClick(Sender: TObject);
    procedure LogLongRadioGroupClick(Sender: TObject);
    procedure logSelCheckBoxClick(Sender: TObject);
    procedure logDetCheckBoxClick(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
  end;

var
  Form6: TForm6;

implementation

uses Unit1;

{$R *.DFM}

procedure TForm6.Button1Click(Sender: TObject);
begin
  close;
end;

procedure TForm6.FormCreate(Sender: TObject);
begin
  debMainCheckBox.checked  := logon;
  debErrorCheckBox.checked := ErrorLogon;
  debUsbCheckBox.checked   := USBLogon;

  logSelCheckBox.checked   := LogSel;
  logDetCheckBox.checked   := LogDet;

  LogTypRadioGroup.itemindex  := LogTyp;
  LogLongRadioGroup.itemindex := LogLong;
end;

procedure TForm6.LogTypRadioGroupClick(Sender: TObject);
begin
  LogTyp := LogTypRadioGroup.itemindex;
end;

procedure TForm6.LogLongRadioGroupClick(Sender: TObject);
begin
  LogLong := LogLongRadioGroup.itemindex;
end;

procedure TForm6.logSelCheckBoxClick(Sender: TObject);
begin
  LogSel := logSelCheckBox.checked;
end;

procedure TForm6.logDetCheckBoxClick(Sender: TObject);
begin
  LogDet := logDetCheckBox.checked;
end;

end.
