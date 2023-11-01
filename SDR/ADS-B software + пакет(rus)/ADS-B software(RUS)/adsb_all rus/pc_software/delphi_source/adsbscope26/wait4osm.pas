
{*********************************************************************
 * FileName:        wait4osm.pas
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
 * this is a delay for the use of OSM-tile servers
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
 unit wait4osm;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, ExtCtrls;

type
  Twait4osm = class(TForm)
    Timer1: TTimer;
    Button1: TButton;
    Label1: TLabel;
    Memo1: TMemo;
    procedure FormActivate(Sender: TObject);
    procedure Timer1Timer(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure Button1Click(Sender: TObject);
    procedure FormClose(Sender: TObject; var Action: TCloseAction);
  private
    { Private-Deklarationen }
    Countdown : integer;
  public
    { Public-Deklarationen }
  end;

var
  Wait4osm1 : Twait4osm;

implementation

{$R *.DFM}

procedure Twait4osm.FormActivate(Sender: TObject);
begin
  Countdown       := 10;
  label1.caption  := 'wait '+inttostr(Countdown)+' seconds';
  Button1.enabled := false;
  Timer1.enabled  := true;
end;

procedure Twait4osm.Timer1Timer(Sender: TObject);
begin
  dec(Countdown);
  if Countdown>0 then label1.caption := 'wait '+inttostr(Countdown)+' seconds'
  else begin
    label1.caption := '';
    Button1.enabled := true;
    Timer1.enabled  := false;
  end;
end;

procedure Twait4osm.FormCreate(Sender: TObject);
begin
  Countdown       := 10;
  Timer1.enabled  := false;
end;

procedure Twait4osm.Button1Click(Sender: TObject);
begin
  close;
end;

procedure Twait4osm.FormClose(Sender: TObject; var Action: TCloseAction);
begin
  label1.caption := '';
  Timer1.enabled := false;
end;

end.
