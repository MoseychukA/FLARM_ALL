object iffForm: TiffForm
  Left = 424
  Top = 506
  Width = 1242
  Height = 325
  Caption = 'IFF-Timeline'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  PixelsPerInch = 96
  TextHeight = 13
  object iffImage: TImage
    Left = 16
    Top = 16
    Width = 1200
    Height = 225
    OnMouseDown = iffImageMouseDown
    OnMouseMove = iffImageMouseMove
  end
  object startLabel: TLabel
    Left = 72
    Top = 256
    Width = 46
    Height = 13
    Caption = 'startLabel'
  end
  object jetztLabel: TLabel
    Left = 248
    Top = 256
    Width = 45
    Height = 13
    Caption = 'jetztLabel'
  end
  object rpmLabel: TLabel
    Left = 432
    Top = 256
    Width = 43
    Height = 13
    Caption = 'rpmLabel'
  end
end
