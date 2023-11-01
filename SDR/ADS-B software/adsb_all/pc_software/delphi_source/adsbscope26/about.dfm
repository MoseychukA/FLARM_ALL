object Form2: TForm2
  Left = 660
  Top = 186
  Width = 567
  Height = 565
  Caption = 'About adsbScope'
  Color = clBtnFace
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -11
  Font.Name = 'MS Sans Serif'
  Font.Style = []
  OldCreateOrder = False
  Visible = True
  PixelsPerInch = 96
  TextHeight = 13
  object Button1: TButton
    Left = 368
    Top = 488
    Width = 81
    Height = 25
    Caption = 'www.sprut.de'
    TabOrder = 0
    OnClick = Button1Click
  end
  object Button2: TButton
    Left = 456
    Top = 488
    Width = 81
    Height = 25
    Caption = 'close'
    TabOrder = 1
    OnClick = Button2Click
  end
  object Memo1: TMemo
    Left = 24
    Top = 16
    Width = 513
    Height = 449
    Font.Charset = DEFAULT_CHARSET
    Font.Color = clWindowText
    Font.Height = -13
    Font.Name = 'MS Sans Serif'
    Font.Style = []
    Lines.Strings = (
      'Tool to list and visualize ADS-B-data.'
      ''
      'homepage:'
      'http://www.sprut.de/electronic/pic/projekte/adsb/adsb_en.html'
      ''
      'Thanks for support:'
      'Ruediger'
      ''
      ''
      'uses data from:'
      ''
      'http://jetvision.de'
      'http://www.libhomeradar.org/download/'
      'http://www.partow.net/miscellaneous/airportdatabase/'
      'http://www.maproom.psu.edu/dcw/'
      'http://aprsworld.net/gisdata/world/'
      'http://dds.cr.usgs.gov/srtm/version2_1/SRTM3/'
      'http://www.maxmind.com/download/worldcities/'
      ''
      'OpenStreetMap & MapQuest:'
      
        '  Data, imagery and map information provided by MapQuest, Open S' +
        'treet Map'
      '  <http://www.openstreetmap.org/> and contributors, '
      '  CC-BY-SA <http://creativecommons.org/licenses/by-sa/2.0/> .'
      ''
      ''
      ''
      'uses the following Delphi Components:'
      ''
      '- GraphicEx from Mike Lischke'
      '- ComPort   from Dejan Crnila'
      '- TZip from Angus Johnson')
    ParentColor = True
    ParentFont = False
    ScrollBars = ssVertical
    TabOrder = 2
  end
end
