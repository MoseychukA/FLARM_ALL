object WatchListForm: TWatchListForm
  Left = 652
  Top = 180
  Width = 570
  Height = 603
  Caption = 'Watchlist'
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
  object WatchlistStringGrid: TStringGrid
    Left = 16
    Top = 24
    Width = 529
    Height = 409
    TabOrder = 0
    OnMouseDown = WatchlistStringGridMouseDown
    RowHeights = (
      24
      24
      24
      24
      24)
  end
  object Button1: TButton
    Left = 472
    Top = 536
    Width = 73
    Height = 25
    Caption = 'Close'
    TabOrder = 1
    OnClick = Button1Click
  end
  object GroupBox1: TGroupBox
    Left = 16
    Top = 440
    Width = 529
    Height = 81
    Caption = 'Editor'
    TabOrder = 2
    object EditAA: TEdit
      Left = 264
      Top = 28
      Width = 97
      Height = 21
      TabOrder = 1
      Text = 'EditAA'
      OnChange = EditAAChange
      OnEnter = EditAAExit
      OnExit = EditAAExit
    end
    object StaticText1: TStaticText
      Left = 40
      Top = 32
      Width = 29
      Height = 17
      Caption = 'Flight'
      TabOrder = 2
    end
    object StaticText2: TStaticText
      Left = 216
      Top = 32
      Width = 41
      Height = 17
      Caption = 'ICAO24'
      TabOrder = 3
    end
    object EditID: TEdit
      Left = 72
      Top = 28
      Width = 97
      Height = 21
      TabOrder = 0
      Text = 'EditID'
      OnEnter = EditIDExit
      OnExit = EditIDExit
    end
  end
end
