object Form5: TForm5
  Left = 968
  Top = 227
  Width = 438
  Height = 367
  Caption = 'goto Town or Airport'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  OldCreateOrder = False
  Visible = True
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object Label1: TLabel
    Left = 24
    Top = 16
    Width = 38
    Height = 13
    Caption = 'Towns'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object Label2: TLabel
    Left = 248
    Top = 16
    Width = 44
    Height = 13
    Caption = 'Airports'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object TownComboBox: TComboBox
    Left = 24
    Top = 32
    Width = 153
    Height = 289
    Style = csSimple
    ItemHeight = 13
    Sorted = True
    TabOrder = 0
    OnChange = TownComboBoxChange
  end
  object Button1: TButton
    Left = 184
    Top = 288
    Width = 57
    Height = 25
    Caption = 'Close'
    TabOrder = 1
    OnClick = Button1Click
  end
  object PortComboBox: TComboBox
    Left = 248
    Top = 32
    Width = 153
    Height = 289
    Style = csSimple
    ItemHeight = 13
    Sorted = True
    TabOrder = 2
    OnChange = PortComboBoxChange
  end
  object Button2: TButton
    Left = 184
    Top = 256
    Width = 57
    Height = 25
    Caption = 'go back'
    TabOrder = 3
    OnClick = Button2Click
  end
end
