object Form4: TForm4
  Left = 539
  Top = 245
  Width = 431
  Height = 500
  Caption = 'Network setup'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Visible = True
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object GroupBox1: TGroupBox
    Left = 24
    Top = 16
    Width = 377
    Height = 65
    Caption = 'Server (decoded data)'
    TabOrder = 0
    object Label1: TLabel
      Left = 16
      Top = 24
      Width = 54
      Height = 13
      Caption = 'Portnumber'
    end
    object ServerPortEdit: TEdit
      Left = 96
      Top = 24
      Width = 153
      Height = 21
      TabOrder = 0
      Text = 'ServerPortEdit'
      OnChange = ServerPortEditChange
    end
  end
  object GroupBox2: TGroupBox
    Left = 24
    Top = 104
    Width = 377
    Height = 105
    Caption = 'RAW-data-server'
    TabOrder = 1
    object Label2: TLabel
      Left = 16
      Top = 24
      Width = 54
      Height = 13
      Caption = 'Portnumber'
    end
    object RawServerPortEdit: TEdit
      Left = 96
      Top = 24
      Width = 153
      Height = 21
      TabOrder = 0
      Text = '---'
      OnChange = RawServerPortEditChange
    end
    object RAWLocalCheckBox: TCheckBox
      Left = 96
      Top = 64
      Width = 185
      Height = 17
      Caption = 'send data from local decoder only'
      TabOrder = 1
      OnClick = RAWLocalCheckBoxClick
    end
  end
  object GroupBox3: TGroupBox
    Left = 24
    Top = 232
    Width = 377
    Height = 185
    Caption = 'RAW-data-client'
    TabOrder = 2
    object Label3: TLabel
      Left = 16
      Top = 24
      Width = 54
      Height = 13
      Caption = 'Portnumber'
    end
    object Label4: TLabel
      Left = 16
      Top = 64
      Width = 22
      Height = 13
      Caption = 'URL'
    end
    object RawClientPortEdit: TEdit
      Left = 96
      Top = 24
      Width = 153
      Height = 21
      TabOrder = 0
      Text = 'Edit1'
      OnChange = RawClientPortEditChange
    end
    object RawClientUrlEdit: TEdit
      Left = 96
      Top = 64
      Width = 153
      Height = 21
      TabOrder = 1
      Text = 'Edit1'
      OnChange = RawClientUrlEditChange
    end
    object Button2: TButton
      Left = 48
      Top = 64
      Width = 41
      Height = 17
      Hint = 'localhost - receive data from this computer'
      Caption = 'local'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      OnClick = Button2Click
    end
    object clientformatRadioGroup: TRadioGroup
      Left = 272
      Top = 24
      Width = 81
      Height = 73
      Hint = 'binary for BEAST and RTL1090 only'
      Caption = 'dataformat'
      Items.Strings = (
        'normal'
        'binary')
      ParentShowHint = False
      ShowHint = True
      TabOrder = 3
      OnClick = clientformatRadioGroupClick
    end
    object GroupBox4: TGroupBox
      Left = 40
      Top = 104
      Width = 313
      Height = 65
      Caption = 'presets'
      TabOrder = 4
      object adsbscopeButton: TButton
        Left = 24
        Top = 24
        Width = 65
        Height = 25
        Hint = 'data-format and port number for adsbScope-server'
        Caption = 'adsbScope'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 0
        OnClick = adsbscopeButtonClick
      end
      object beastButton: TButton
        Left = 88
        Top = 24
        Width = 65
        Height = 25
        Hint = 'data-format and port number for BEAST-server'
        Caption = 'BEAST'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 1
        OnClick = beastButtonClick
      end
      object rtlButton: TButton
        Left = 152
        Top = 24
        Width = 65
        Height = 25
        Hint = 'data-format and port number for RTL1090-server'
        Caption = 'RTL1090'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 2
        OnClick = rtlButtonClick
      end
      object sharpButton: TButton
        Left = 216
        Top = 24
        Width = 65
        Height = 25
        Hint = 'data-format and port number for ADSB#-server'
        Caption = 'ADSB#'
        ParentShowHint = False
        ShowHint = True
        TabOrder = 3
        OnClick = sharpButtonClick
      end
    end
  end
  object Button1: TButton
    Left = 288
    Top = 432
    Width = 113
    Height = 33
    Hint = 'close window'
    Caption = 'Close'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 3
    OnClick = Button1Click
  end
end
