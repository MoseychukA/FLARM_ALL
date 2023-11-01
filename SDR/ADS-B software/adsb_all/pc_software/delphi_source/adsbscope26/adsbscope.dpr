program adsbscope;

uses
  Forms,
  Unit1 in 'Unit1.pas' {Form1},
  usbdll in 'usbdll.pas',
  about in 'about.pas' {Form2},
  filter in 'filter.pas' {FilterForm},
  framerate in 'framerate.pas' {Form3},
  network in 'network.pas' {Form4},
  watchlist in 'watchlist.pas' {WatchListForm},
  table in 'table.pas' {TableForm},
  amanager in 'amanager.pas' {AAmanager},
  hearbeat in 'hearbeat.pas' {HeartBeatForm},
  iff1 in 'iff1.pas' {iffForm},
  Uportmap in 'Uportmap.pas',
  Ugpx in 'Ugpx.pas',
  comsetup in 'comsetup.pas' {ComSetup},
  wait4osm in 'wait4osm.pas' {Wait4osm},
  splashscr in 'splashscr.pas' {SplashScreenForm},
  gototown in 'gototown.pas' {Form5},
  log in 'log.pas' {Form6},
  gps in 'gps.pas' {GpsForm},
  i2c in 'i2c.pas' {Form7};

{$R *.RES}

begin
  // SplashScreen Fenster erstellen
  SplashScreenForm := TSplashScreenForm.Create(Application);

  Try
    // SplashScreen Fenster anzeigen
    SplashScreenForm.Show;

    Application.Initialize;
    SplashScreenForm.Update;
    Application.Title := 'adsbScope';
    Application.CreateForm(TForm1, Form1);
  Application.CreateForm(TFilterForm, FilterForm);
  Application.CreateForm(TWatchListForm, WatchListForm);
  Application.CreateForm(TTableForm, TableForm);
  Application.CreateForm(TAAmanager, AAmanager);
  Application.CreateForm(THeartBeatForm, HeartBeatForm);
  Application.CreateForm(TiffForm, iffForm);
  Application.CreateForm(TComSetup, ComSetup1);
  Application.CreateForm(TWait4osm, Wait4osm1);
  Application.CreateForm(TForm7, Form7);
  Application.showhint := true;

    // SplashScreen Fenster ausblenden
    SplashScreenForm.Hide;

  Finally
    // SplashScreen Fenster freigeben
    SplashScreenForm.Free;
  End;
  Application.Run;
end.
