object AAmanager: TAAmanager
  Left = 363
  Top = 127
  Width = 944
  Height = 661
  Caption = 'Aircraftmanager'
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
  object Label1: TLabel
    Left = 418
    Top = 240
    Width = 31
    Height = 32
    Caption = '>>'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -27
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
  end
  object Button3: TButton
    Left = 736
    Top = 576
    Width = 185
    Height = 33
    Hint = 'close window'
    Caption = 'Close'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    OnClick = Button3Click
  end
  object InfoMemo: TMemo
    Left = 32
    Top = 496
    Width = 369
    Height = 113
    Lines.Strings = (
      'Log:')
    ScrollBars = ssVertical
    TabOrder = 1
  end
  object Button5: TButton
    Left = 464
    Top = 496
    Width = 169
    Height = 33
    Hint = 'open the webpage in the browser'
    Caption = 'open Airframes.org'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 2
    OnClick = Button5Click
  end
  object Button6: TButton
    Left = 464
    Top = 576
    Width = 169
    Height = 33
    Hint = 'read the downloaded webpages'
    Caption = 'read HTML-File'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -11
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 3
    OnClick = Button6Click
  end
  object UfoGroupBox: TGroupBox
    Left = 16
    Top = 24
    Width = 400
    Height = 440
    Caption = 'my list of unknown aircraft'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -16
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
    TabOrder = 4
    object UfoMemo: TMemo
      Left = 16
      Top = 32
      Width = 369
      Height = 337
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
      Lines.Strings = (
        'UfoMemo')
      ParentFont = False
      ScrollBars = ssVertical
      TabOrder = 0
    end
    object Button1: TButton
      Left = 16
      Top = 392
      Width = 169
      Height = 33
      Hint = 'erase all aircraft of unknown (not existing) origin'
      Caption = 'Erase all UFOs'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnClick = Button1Click
    end
    object Button2: TButton
      Left = 216
      Top = 392
      Width = 169
      Height = 33
      Hint = 'erase the whole list of unknown aircraft'
      Caption = 'Erase ALL unknown aircraft'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      OnClick = Button2Click
    end
  end
  object ExtraGroupBox: TGroupBox
    Left = 448
    Top = 24
    Width = 473
    Height = 440
    Caption = 'my list of known aircraft'
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -16
    Font.Name = 'MS Sans Serif'
    Font.Style = [fsBold]
    ParentFont = False
    TabOrder = 5
    object ExtraMemo: TMemo
      Left = 15
      Top = 39
      Width = 442
      Height = 337
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -13
      Font.Name = 'Courier New'
      Font.Style = []
      Lines.Strings = (
        'ExtraMemo')
      ParentFont = False
      ScrollBars = ssVertical
      TabOrder = 0
    end
    object Button7: TButton
      Left = 15
      Top = 391
      Width = 169
      Height = 33
      Hint = 'check the list of known aircraft for errors'
      Caption = 'check list of known aircraft'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = clWindowText
      Font.Height = -11
      Font.Name = 'MS Sans Serif'
      Font.Style = []
      ParentFont = False
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnClick = Button7Click
    end
  end
  object BitBtn1: TBitBtn
    Left = 736
    Top = 496
    Width = 185
    Height = 33
    Hint = 'download latest icao24plus.txt file '
    Caption = 'download new aircraft-database'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 6
    OnClick = BitBtn1Click
    Glyph.Data = {
      76010000424D7601000000000000760000002800000020000000100000000100
      04000000000000010000120B0000120B00001000000000000000000000000000
      800000800000008080008000000080008000808000007F7F7F00BFBFBF000000
      FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF003FFFFFFFFFFF
      FFFF33333333333FFFFF3FFFFFFFFF00000F333333333377777F33FFFFFFFF09
      990F33333333337F337F333FFFFFFF09990F33333333337F337F3333FFFFFF09
      990F33333333337FFF7F33333FFFFF00000F3333333333777773333333FFFFFF
      FFFF3333333333333F333333333FFFFF0FFF3333333333337FF333333333FFF0
      00FF33333333333777FF333333333F00000F33FFFFF33777777F300000333000
      0000377777F33777777730EEE033333000FF37F337F3333777F330EEE0333330
      00FF37F337F3333777F330EEE033333000FF37FFF7F333F77733300000333000
      03FF3777773337777333333333333333333F3333333333333333}
    NumGlyphs = 2
  end
  object BitBtn2: TBitBtn
    Left = 464
    Top = 536
    Width = 169
    Height = 33
    Hint = 'select the directory with the saved HTML-files'
    Caption = 'select HTML-directory'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 7
    OnClick = BitBtn2Click
    Glyph.Data = {
      76010000424D7601000000000000760000002800000020000000100000000100
      04000000000000010000120B0000120B00001000000000000000000000000000
      800000800000008080008000000080008000808000007F7F7F00BFBFBF000000
      FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF00303333333333
      333337F3333333333333303333333333333337F33FFFFF3FF3FF303300000300
      300337FF77777F77377330000BBB0333333337777F337F33333330330BB00333
      333337F373F773333333303330033333333337F3377333333333303333333333
      333337F33FFFFF3FF3FF303300000300300337FF77777F77377330000BBB0333
      333337777F337F33333330330BB00333333337F373F773333333303330033333
      333337F3377333333333303333333333333337FFFF3FF3FFF333000003003000
      333377777F77377733330BBB0333333333337F337F33333333330BB003333333
      333373F773333333333330033333333333333773333333333333}
    NumGlyphs = 2
  end
  object HtmlOpenDialog: TOpenDialog
    Left = 816
    Top = 8
  end
  object AATimer: TTimer
    Enabled = False
    OnTimer = AATimerTimer
    Left = 640
    Top = 576
  end
end
