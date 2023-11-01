object Form7: TForm7
  Left = 396
  Top = 313
  Width = 572
  Height = 177
  Caption = 'I2C-code setup'
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
  object i2cLabel: TLabel
    Left = 8
    Top = 72
    Width = 82
    Height = 13
    Caption = 'I2C-Command:'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object Label1: TLabel
    Left = 8
    Top = 37
    Width = 93
    Height = 13
    Caption = 'type in new cmd'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object i2cEdit: TEdit
    Left = 112
    Top = 32
    Width = 297
    Height = 21
    TabOrder = 0
    Text = 'i2cEdit'
  end
  object i2cButton: TButton
    Left = 424
    Top = 24
    Width = 129
    Height = 33
    Caption = 'change'
    TabOrder = 1
    OnClick = i2cButtonClick
  end
  object Button2: TButton
    Left = 424
    Top = 104
    Width = 129
    Height = 33
    Caption = 'close'
    TabOrder = 2
    OnClick = Button2Click
  end
  object Button1: TButton
    Left = 424
    Top = 64
    Width = 129
    Height = 33
    Caption = 'change and send via I2C'
    TabOrder = 3
    OnClick = Button1Click
  end
end
