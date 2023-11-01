object Form3: TForm3
  Left = 253
  Top = 321
  Width = 846
  Height = 307
  Caption = 'Framerate'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Visible = True
  OnActivate = FormActivate
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object ImageR: TImage
    Left = 16
    Top = 16
    Width = 800
    Height = 200
    OnMouseMove = ImageRMouseMove
  end
  object Label1: TLabel
    Left = 200
    Top = 232
    Width = 32
    Height = 13
    Hint = 'time of last values'
    Caption = 'Label1'
    ParentShowHint = False
    ShowHint = True
  end
  object Label2: TLabel
    Left = 200
    Top = 256
    Width = 32
    Height = 13
    Hint = 'frames per minute'
    Caption = 'Label2'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clBlue
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
    ParentShowHint = False
    ShowHint = True
  end
  object Label3: TLabel
    Left = 392
    Top = 232
    Width = 32
    Height = 13
    Hint = 'comparator offset voltage'
    Caption = 'Label3'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clRed
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
    ParentShowHint = False
    ShowHint = True
  end
  object Label4: TLabel
    Left = 392
    Top = 256
    Width = 32
    Height = 13
    Hint = 'avg. frames from each aircraft per minute'
    Caption = 'Label4'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clGreen
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    ParentFont = False
    ParentShowHint = False
    ShowHint = True
  end
  object Button1: TButton
    Left = 680
    Top = 232
    Width = 137
    Height = 33
    Hint = 'close this window'
    Caption = 'Close'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    OnClick = Button1Click
  end
  object Button2: TButton
    Left = 16
    Top = 232
    Width = 137
    Height = 33
    Hint = 'erase allthis data and start from scratch'
    Caption = 'erase stats.'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 1
    OnClick = Button2Click
  end
end
