object ComSetup: TComSetup
  Left = 806
  Top = 271
  Width = 228
  Height = 196
  Caption = 'Setup Com-Port'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  OnActivate = FormActivate
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object Button1: TButton
    Left = 72
    Top = 136
    Width = 65
    Height = 25
    Caption = 'OK'
    TabOrder = 0
    OnClick = Button1Click
  end
  object GroupBox1: TGroupBox
    Left = 8
    Top = 8
    Width = 201
    Height = 121
    Caption = 'Settings'
    TabOrder = 1
    object Label1: TLabel
      Left = 8
      Top = 32
      Width = 19
      Height = 13
      Caption = 'Port'
    end
    object Label2: TLabel
      Left = 8
      Top = 64
      Width = 46
      Height = 13
      Caption = 'Baud rate'
    end
    object PortComboBox: TComboBox
      Left = 64
      Top = 24
      Width = 120
      Height = 21
      ItemHeight = 13
      TabOrder = 0
      Text = 'PortComboBox'
    end
    object BaudrateComboBox: TComboBox
      Left = 64
      Top = 64
      Width = 120
      Height = 21
      ItemHeight = 13
      TabOrder = 1
      Text = 'BaudrateComboBox'
      Items.Strings = (
        '3000000'
        '1000000'
        '   921600'
        '   230400'
        '   115200'
        '    19200')
    end
  end
  object Button2: TButton
    Left = 144
    Top = 136
    Width = 65
    Height = 25
    Caption = 'Cancel'
    TabOrder = 2
    OnClick = Button2Click
  end
end
