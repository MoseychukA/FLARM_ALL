object wait4osm: Twait4osm
  Left = 588
  Top = 282
  Width = 505
  Height = 324
  Caption = 'Information'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnActivate = FormActivate
  OnClose = FormClose
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object Label1: TLabel
    Left = 56
    Top = 240
    Width = 196
    Height = 32
    Caption = 'wait 10 seconds'
    Font.Charset = ANSI_CHARSET
    Font.Color = clWindowText
    Font.Height = -27
    Font.Name = 'Arial'
    Font.Style = []
    ParentFont = False
  end
  object Button1: TButton
    Left = 320
    Top = 232
    Width = 161
    Height = 41
    Caption = 'Continue'
    TabOrder = 0
    OnClick = Button1Click
  end
  object Memo1: TMemo
    Left = 16
    Top = 16
    Width = 465
    Height = 201
    Color = clBtnFace
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    Lines.Strings = (
      'You should not use the option'
      '  Config -> Background Picture -> OpenStreetMap'
      'on regular base.'
      ''
      'Please prefere '
      '  Config -> Background Picture -> MapQuest/OSM'
      '  Config -> Background Picture -> MapQuest/Aerial'
      '  Config -> Background Picture -> SRTM'
      ''
      'to reduce the load at OpenStreatMap Servers.')
    ParentFont = False
    TabOrder = 1
  end
  object Timer1: TTimer
    Enabled = False
    OnTimer = Timer1Timer
    Left = 280
    Top = 240
  end
end
