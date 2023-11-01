object HeartBeatForm: THeartBeatForm
  Left = 374
  Top = 97
  Width = 646
  Height = 790
  Caption = 'HeartBeat - decoder status of the last 5 minutes'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnClose = FormClose
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object UImage: TImage
    Left = 16
    Top = 16
    Width = 512
    Height = 200
    Hint = 'Comparator-threshould and mean signal level'
    ParentShowHint = False
    ShowHint = True
  end
  object HImage: TImage
    Left = 16
    Top = 240
    Width = 512
    Height = 100
    Hint = 'frame-header errors'
    ParentShowHint = False
    ShowHint = True
  end
  object DImage: TImage
    Left = 16
    Top = 360
    Width = 512
    Height = 100
    Hint = 'frame-data errors'
    ParentShowHint = False
    ShowHint = True
  end
  object Label1: TLabel
    Left = 536
    Top = 296
    Width = 65
    Height = 13
    Caption = 'Header-Errors'
  end
  object Label2: TLabel
    Left = 536
    Top = 416
    Width = 53
    Height = 13
    Caption = 'Data-Errors'
  end
  object Label3: TLabel
    Left = 536
    Top = 88
    Width = 60
    Height = 13
    Caption = 'U threshould'
  end
  object HScaleLabel: TLabel
    Left = 536
    Top = 240
    Width = 61
    Height = 13
    Caption = 'HScaleLabel'
  end
  object DScaleLabel: TLabel
    Left = 536
    Top = 360
    Width = 61
    Height = 13
    Caption = 'DScaleLabel'
  end
  object FImage: TImage
    Left = 16
    Top = 600
    Width = 512
    Height = 100
    Hint = 'received frames'
    ParentShowHint = False
    ShowHint = True
  end
  object Label4: TLabel
    Left = 536
    Top = 648
    Width = 34
    Height = 13
    Caption = 'Frames'
  end
  object FScaleLabel: TLabel
    Left = 536
    Top = 600
    Width = 59
    Height = 13
    Caption = 'FScaleLabel'
  end
  object Label5: TLabel
    Left = 536
    Top = 120
    Width = 38
    Height = 13
    Caption = 'U signal'
  end
  object Label6: TLabel
    Left = 536
    Top = 208
    Width = 25
    Height = 13
    Caption = '0.6 V'
  end
  object Label7: TLabel
    Left = 536
    Top = 8
    Width = 25
    Height = 13
    Caption = '1.2 V'
  end
  object Label8: TLabel
    Left = 536
    Top = 280
    Width = 6
    Height = 13
    Caption = '0'
  end
  object Label9: TLabel
    Left = 536
    Top = 400
    Width = 6
    Height = 13
    Caption = '0'
  end
  object Label10: TLabel
    Left = 536
    Top = 632
    Width = 6
    Height = 13
    Caption = '0'
  end
  object FImage2: TImage
    Left = 16
    Top = 480
    Width = 512
    Height = 100
    Hint = 'send and lost frames'
    ParentShowHint = False
    ShowHint = True
  end
  object Label11: TLabel
    Left = 536
    Top = 512
    Width = 6
    Height = 13
    Caption = '0'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clGreen
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
  end
  object Label12: TLabel
    Left = 536
    Top = 536
    Width = 6
    Height = 13
    Caption = '0'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clRed
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
  end
  object Button1: TButton
    Left = 552
    Top = 712
    Width = 81
    Height = 41
    Caption = 'Close'
    TabOrder = 0
    OnClick = Button1Click
  end
  object Button2: TButton
    Left = 456
    Top = 712
    Width = 81
    Height = 41
    Caption = 'Erase Data'
    TabOrder = 1
    OnClick = Button2Click
  end
end
