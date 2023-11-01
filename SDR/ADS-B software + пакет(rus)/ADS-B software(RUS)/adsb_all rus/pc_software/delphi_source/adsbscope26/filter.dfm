object FilterForm: TFilterForm
  Left = 708
  Top = 105
  Width = 501
  Height = 548
  Caption = 'Filter'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  OldCreateOrder = False
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object MemoF: TMemo
    Left = 24
    Top = 48
    Width = 449
    Height = 313
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -19
    Font.Name = 'Courier New'
    Font.Style = []
    Lines.Strings = (
      'filtered data')
    ParentFont = False
    TabOrder = 0
  end
  object FilterLogButton: TButton
    Left = 24
    Top = 480
    Width = 129
    Height = 33
    Caption = 'start log to file'
    TabOrder = 1
    OnClick = FilterLogButtonClick
  end
  object FilterCloseButton: TButton
    Left = 344
    Top = 480
    Width = 129
    Height = 33
    Caption = 'Close'
    TabOrder = 2
    OnClick = FilterCloseButtonClick
  end
  object Edit1: TEdit
    Left = 184
    Top = 480
    Width = 129
    Height = 21
    TabOrder = 3
    Text = '000000'
    OnChange = Edit1Change
    OnExit = Edit1Exit
  end
  object MemoFL: TMemo
    Left = 24
    Top = 384
    Width = 449
    Height = 81
    Lines.Strings = (
      'Logfile-data')
    TabOrder = 4
  end
  object ProgressBar1: TProgressBar
    Left = 24
    Top = 16
    Width = 449
    Height = 25
    Min = 0
    Max = 20
    Smooth = True
    TabOrder = 5
  end
  object Timer1: TTimer
    Enabled = False
    OnTimer = Timer1Timer
    Top = 336
  end
end
