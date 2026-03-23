using System;
using System.Windows;

namespace FlyRadar
{
    public partial class SettingsWindow : Window
    {
        public double CenterLatitude { get; set; }
        public double CenterLongitude { get; set; }

        public SettingsWindow()
        {
            InitializeComponent();
            Loaded += SettingsWindow_Loaded;
        }

        private void SettingsWindow_Loaded(object? sender, RoutedEventArgs e)
        {
            TxtLat.Text = CenterLatitude.ToString("F6");
            TxtLon.Text = CenterLongitude.ToString("F6");
            TxtNow.Text = DateTime.Now.ToString("HH:mm:ss");
        }

        private void Ok_Click(object sender, RoutedEventArgs e)
        {
            // Проверка вводимых данных (можно расширить)
            if (!double.TryParse(TxtLat.Text, out var lat) ||
                !double.TryParse(TxtLon.Text, out var lon))
            {
                MessageBox.Show("Неверный формат координат", "Ошибка", MessageBoxButton.OK, MessageBoxImage.Error);
                return;
            }

            CenterLatitude = lat;
            CenterLongitude = lon;

            // Здесь можно сохранить выбранный канал (COM/TCP/UDP) в свойствах окна
            // и передать их в MainWindow при закрытии.

            DialogResult = true;
            Close();
        }
    }
}