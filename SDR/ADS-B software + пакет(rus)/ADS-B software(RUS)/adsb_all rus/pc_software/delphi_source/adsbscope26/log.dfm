object Form6: TForm6
  Left = 726
  Top = 212
  Width = 351
  Height = 332
  Caption = 'Log-Setup'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  FormStyle = fsStayOnTop
  Icon.Data = {
    0000010001002020100000000000E80200001600000028000000200000004000
    0000010004000000000080020000000000000000000000000000000000000000
    000000008000008000000080800080000000800080008080000080808000C0C0
    C0000000FF0000FF000000FFFF00FF000000FF00FF00FFFF0000FFFFFF009999
    999999999999999999999999999994444F00F40F444444444444444444499C44
    4F00F40F444444444444444444499CC44F0F0F0F444444440000004444499CCC
    4F0F0F0F444440008888880044499CCCCF04F00F444008888888888804499CCC
    CF00000F4008F8F8F8F8888804499CCCCF04F00F0F8F88888888800004499CCC
    CF0F0040F8F8F8F8F800078804499CCCCCF0040F8F888F880077787804499CCC
    CCCFC0F8F8F8F8F00787878044499CCCCCCC0F8F8F8F80070878788044499CCC
    CCC0F8F8F8F807770787880444499CCCCCC0FFFF8F8077780878780444499CCC
    CC08F8F8F80F77870787804444499CCCCC0FFF8F80F0F7780878044444499CCC
    C0F8F8F8078F0F870787044444499CCCC0FF8FF07777F0F80880444444499CCC
    C0F8F8F077878F0F0804444444499CCC0FFFFF07777878F00044444444499CCC
    0FF8F000000000000F4F444444499CCC0FFFF07778787880F0F0F44444499CCC
    0FF807878787870CCF00F44444499CCC0FFF0778787870CCF000F44444499CCC
    0FF8078787800CCCCFFF0F4444499CCC0FF07878780CCCCCCCCCFF4444499CCC
    C0F0777700CCCCCCCCCCCC4444499CCCC0F07700CCCCCCCCCCCCCCC444499CCC
    CC0000CCCCCCCCCCCCCCCCCC44499CCCCCCCCCCCCCCCCCCCCCCCCCCCC4499CCC
    CCCCCCCCCCCCCCCCCCCCCCCCCC49999999999999999999999999999999990000
    0000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    0000000000000000000000000000000000000000000000000000000000000000
    000000000000000000000000000000000000000000000000000000000000}
  OldCreateOrder = False
  Visible = True
  OnCreate = FormCreate
  PixelsPerInch = 96
  TextHeight = 13
  object Button1: TButton
    Left = 184
    Top = 248
    Width = 137
    Height = 33
    Hint = 'close window'
    Caption = 'OK'
    ParentShowHint = False
    ShowHint = True
    TabOrder = 0
    OnClick = Button1Click
  end
  object GroupBox1: TGroupBox
    Left = 184
    Top = 16
    Width = 137
    Height = 129
    Hint = 'logfunction for debugging only'
    Caption = 'Debug only'
    Enabled = False
    ParentShowHint = False
    ShowHint = True
    TabOrder = 1
    object debMainCheckBox: TCheckBox
      Left = 16
      Top = 24
      Width = 105
      Height = 25
      Hint = 'log all received and decoded data'
      Caption = 'Main Debug-Log'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
    end
    object debErrorCheckBox: TCheckBox
      Left = 16
      Top = 56
      Width = 105
      Height = 25
      Hint = 'log errors'
      Caption = 'Error-Debug-Log'
      TabOrder = 1
    end
    object debUsbCheckBox: TCheckBox
      Left = 16
      Top = 88
      Width = 105
      Height = 25
      Hint = 'log all USB-data'
      Caption = 'USB-Debug-Log'
      TabOrder = 2
    end
  end
  object LogTypRadioGroup: TRadioGroup
    Left = 24
    Top = 16
    Width = 145
    Height = 129
    Hint = 'type of the log file'
    Caption = 'Type of Log'
    Enabled = False
    ItemIndex = 0
    Items.Strings = (
      'none'
      'DF17-Log'
      'Track-Log'
      'Monitor-Log')
    ParentShowHint = False
    ShowHint = False
    TabOrder = 2
    OnClick = LogTypRadioGroupClick
  end
  object LogLongRadioGroup: TRadioGroup
    Left = 24
    Top = 152
    Width = 145
    Height = 129
    Hint = 'create a new log file every ...'
    Caption = 'Length of each Log'
    Enabled = False
    ItemIndex = 0
    Items.Strings = (
      'unlimited'
      '1 Day'
      '6 Houres'
      '1 Houre'
      '15 Minutes')
    ParentShowHint = False
    ShowHint = True
    TabOrder = 3
    OnClick = LogLongRadioGroupClick
  end
  object GroupBox2: TGroupBox
    Left = 184
    Top = 152
    Width = 137
    Height = 81
    Caption = 'other'
    TabOrder = 4
    object logSelCheckBox: TCheckBox
      Left = 16
      Top = 24
      Width = 113
      Height = 17
      Hint = 'log aircraft if selected by user'
      Caption = 'selected aircraft'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      OnClick = logSelCheckBoxClick
    end
    object logDetCheckBox: TCheckBox
      Left = 16
      Top = 48
      Width = 113
      Height = 17
      Hint = 'log aircraft at detection and if discarted'
      Caption = 'detected aircraft'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnClick = logDetCheckBoxClick
    end
  end
end
