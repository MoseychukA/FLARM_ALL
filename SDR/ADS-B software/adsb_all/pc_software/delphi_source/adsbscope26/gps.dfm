object GpsForm: TGpsForm
  Left = 613
  Top = 212
  Width = 557
  Height = 460
  Caption = 'GPS Data'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  OldCreateOrder = False
  Visible = True
  OnActivate = FormActivate
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object GpsImage: TImage
    Left = 24
    Top = 24
    Width = 380
    Height = 380
  end
  object LatLabel: TLabel
    Left = 472
    Top = 24
    Width = 18
    Height = 13
    Caption = 'Lat:'
  end
  object LongLabel: TLabel
    Left = 472
    Top = 48
    Width = 27
    Height = 13
    Caption = 'Long:'
  end
  object FixLabel: TLabel
    Left = 472
    Top = 144
    Width = 16
    Height = 13
    Hint = 'Fix Mode'
    Caption = 'Fix:'
    ParentShowHint = False
    ShowHint = True
  end
  object QualLabel: TLabel
    Left = 472
    Top = 168
    Width = 32
    Height = 13
    Hint = 'fix quality indicator'
    Caption = 'Quality'
    ParentShowHint = False
    ShowHint = True
  end
  object ValidLabel: TLabel
    Left = 472
    Top = 336
    Width = 23
    Height = 13
    Caption = 'Valid'
  end
  object SatLabel: TLabel
    Left = 472
    Top = 192
    Width = 45
    Height = 13
    Hint = 'Number of satellites in use'
    Caption = 'Satellites:'
    ParentShowHint = False
    ShowHint = True
  end
  object TimeLabel: TLabel
    Left = 472
    Top = 288
    Width = 23
    Height = 13
    Hint = 'GPS-time [UTC]'
    Caption = 'Time'
    ParentShowHint = False
    ShowHint = True
  end
  object Label1: TLabel
    Left = 416
    Top = 24
    Width = 41
    Height = 13
    Caption = 'Latitude:'
  end
  object Label2: TLabel
    Left = 416
    Top = 48
    Width = 50
    Height = 13
    Caption = 'Longitude:'
  end
  object Label3: TLabel
    Left = 416
    Top = 144
    Width = 16
    Height = 13
    Hint = 'Fix Mode'
    Caption = 'Fix:'
    ParentShowHint = False
    ShowHint = True
  end
  object Label4: TLabel
    Left = 416
    Top = 168
    Width = 35
    Height = 13
    Hint = 'fix quality indicator'
    Caption = 'Quality:'
    ParentShowHint = False
    ShowHint = True
  end
  object Label5: TLabel
    Left = 416
    Top = 336
    Width = 26
    Height = 13
    Caption = 'Valid:'
  end
  object Label6: TLabel
    Left = 416
    Top = 192
    Width = 45
    Height = 13
    Hint = 'Number of satellites in use'
    Caption = 'Satellites:'
    ParentShowHint = False
    ShowHint = True
  end
  object Label7: TLabel
    Left = 416
    Top = 288
    Width = 26
    Height = 13
    Hint = 'GPS-time [UTC]'
    Caption = 'Time:'
    ParentShowHint = False
    ShowHint = True
  end
  object AltLabel: TLabel
    Left = 472
    Top = 72
    Width = 15
    Height = 13
    Hint = 'altitude over see level'
    Caption = 'Alt:'
    ParentShowHint = False
    ShowHint = True
  end
  object Label8: TLabel
    Left = 416
    Top = 72
    Width = 38
    Height = 13
    Hint = 'altitude over see level'
    Caption = 'Altitude:'
    ParentShowHint = False
    ShowHint = True
  end
  object SpeedLabel: TLabel
    Left = 472
    Top = 96
    Width = 34
    Height = 13
    Hint = 'speed over ground'
    Caption = 'Speed:'
    ParentShowHint = False
    ShowHint = True
  end
  object DirLabel: TLabel
    Left = 472
    Top = 120
    Width = 45
    Height = 13
    Hint = 'course over ground'
    Caption = 'Direction:'
    ParentShowHint = False
    ShowHint = True
  end
  object Label9: TLabel
    Left = 416
    Top = 96
    Width = 34
    Height = 13
    Hint = 'speed over ground'
    Caption = 'Speed:'
    ParentShowHint = False
    ShowHint = True
  end
  object Label10: TLabel
    Left = 416
    Top = 120
    Width = 45
    Height = 13
    Hint = 'course over ground'
    Caption = 'Direction:'
    ParentShowHint = False
    ShowHint = True
  end
  object HDOPLabel: TLabel
    Left = 472
    Top = 240
    Width = 31
    Height = 13
    Hint = 'Horizontal dilution of precision'
    Caption = 'HDOP'
    ParentShowHint = False
    ShowHint = True
  end
  object VDOPLabel: TLabel
    Left = 472
    Top = 264
    Width = 30
    Height = 13
    Hint = 'Vertical dilution of precision'
    Caption = 'VDOP'
    ParentShowHint = False
    ShowHint = True
  end
  object Label11: TLabel
    Left = 416
    Top = 240
    Width = 34
    Height = 13
    Hint = 'Horizontal dilution of precision'
    Caption = 'HDOP:'
    ParentShowHint = False
    ShowHint = True
  end
  object Label12: TLabel
    Left = 416
    Top = 264
    Width = 33
    Height = 13
    Hint = 'Vertical dilution of precision'
    Caption = 'VDOP:'
    ParentShowHint = False
    ShowHint = True
  end
  object PDOPLabel: TLabel
    Left = 472
    Top = 216
    Width = 30
    Height = 13
    Hint = 'Position dilution of precision'
    Caption = 'PDOP'
    ParentShowHint = False
    ShowHint = True
  end
  object Label14: TLabel
    Left = 416
    Top = 216
    Width = 33
    Height = 13
    Hint = 'Position dilution of precision'
    Caption = 'PDOP:'
    ParentShowHint = False
    ShowHint = True
  end
  object dateLabel: TLabel
    Left = 472
    Top = 312
    Width = 23
    Height = 13
    Hint = 'Date'
    Caption = 'Date'
    ParentShowHint = False
    ShowHint = True
  end
  object Label15: TLabel
    Left = 416
    Top = 312
    Width = 26
    Height = 13
    Hint = 'Date'
    Caption = 'Date:'
    ParentShowHint = False
    ShowHint = True
  end
  object Button1: TButton
    Left = 416
    Top = 360
    Width = 105
    Height = 41
    Hint = 'close window'
    Caption = 'close'
    ParentShowHint = False
    ShowHint = False
    TabOrder = 0
    OnClick = Button1Click
  end
  object Timer1: TTimer
    Interval = 3000
    OnTimer = Timer1Timer
    Left = 520
    Top = 8
  end
end
