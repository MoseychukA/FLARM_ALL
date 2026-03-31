using System;
using System.Threading;
using System.Windows.Forms;
using FlightRadarPc.Services;

namespace FlightRadarPc
{
    static class Program
    {
        [STAThread]
        static void Main()
        {
            LogService.Write("========== APP START ==========");
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);

            Application.ThreadException += (s, e) =>
            {
                LogService.Write(e.Exception, "UI ThreadException");
                try
                {
                    MessageBox.Show(
                        e.Exception.Message + "\r\n\r\nПодробности записаны в FlightRadarPc.log",
                        "Ошибка программы",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Error);
                }
                catch { }
            };

            AppDomain.CurrentDomain.UnhandledException += (s, e) =>
            {
                LogService.Write(e.ExceptionObject as Exception, "UnhandledException");
            };

            try
            {
                LogService.Write("Program.Main: before Application.Run");
                Application.Run(new UI.MainForm());
                LogService.Write("Program.Main: Application.Run returned normally");
            }
            catch (Exception ex)
            {
                LogService.Write(ex, "Main");
                MessageBox.Show(
                    ex.Message + "\r\n\r\nПодробности записаны в FlightRadarPc.log",
                    "Критическая ошибка",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }
    }
}
