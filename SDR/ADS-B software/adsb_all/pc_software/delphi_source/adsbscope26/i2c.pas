unit i2c;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls;

type
  TForm7 = class(TForm)
    i2cLabel: TLabel;
    i2cEdit: TEdit;
    i2cButton: TButton;
    Button2: TButton;
    Button1: TButton;
    Label1: TLabel;
    procedure Button2Click(Sender: TObject);
    procedure updatei2ceditor;
    procedure FormCreate(Sender: TObject);
    procedure FormActivate(Sender: TObject);
    procedure i2cButtonClick(Sender: TObject);
    procedure Button1Click(Sender: TObject);
  private
    { Private-Deklarationen }
  public
    { Public-Deklarationen }
  end;

var
  Form7: TForm7;

implementation

uses Unit1;

{$R *.DFM}

procedure TForm7.Button2Click(Sender: TObject);
begin
  close;
end;

procedure TForm7.updatei2ceditor;
var st : string;
    k  : integer;
begin
  st :='';
  for k:=1 to i2cdata[0] do st := st + '-' + inttohex(i2cdata[k] ,2);
  i2cEdit.Text := st;
  i2cLabel.caption := 'I2C-Commands: ' + st;
end;

procedure TForm7.FormCreate(Sender: TObject);
begin
  updatei2ceditor;
end;

procedure TForm7.FormActivate(Sender: TObject);
begin
  updatei2ceditor;
end;

procedure TForm7.i2cButtonClick(Sender: TObject);
var st  : string;
    k   : integer;
    i   : integer;
    b   : integer;
    sti : integer;

  function asc2i(z:char):integer;
  begin
    case z of
    '0'..'9': result:= ord(z)-ord('0');
    'A'..'F': result:= ord(z)-ord('A') +10;
    'a'..'f': result:= ord(z)-ord('a') +10;
      else result := -1;
    end;
  end;

  function getnr:integer;
  begin
    result := -1;
    if sti>length(st) then exit;
    while (asc2i(st[sti])<0) and (sti<length(st)) do inc(sti);
    if asc2i(st[sti])>=0 then begin
      result := 0;
      repeat
        if asc2i(st[sti])>=0 then begin
          result := result shl 4;
          result := result + asc2i(st[sti]);
        end else break;
        inc(sti);
      until (sti>length(st));
    end;
  end;

begin
  st  := i2cEdit.Text;
  k   := 0;
  i   := 0;
  sti := 1;
  repeat
    b := getnr;
    if b>=0 then begin
      inc(i);
      i2cdata[i] := b;
    end;
  until (b<0) or (i>14);
  i2cdata[0] := i;
  updatei2ceditor;
end;

procedure TForm7.Button1Click(Sender: TObject);
begin
  i2cButtonClick(nil);
  Form1.other1Click(nil);
  //Form1.i2c2tuner;
end;

end.
