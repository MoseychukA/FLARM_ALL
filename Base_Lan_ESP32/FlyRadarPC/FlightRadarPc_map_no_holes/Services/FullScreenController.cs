using System.Windows.Forms;

namespace FlightRadarPc.Services
{
    public class FullScreenController
    {
        private FormBorderStyle _savedBorderStyle;
        private FormWindowState _savedState;
        private bool _savedTopMost;

        public bool IsFullScreen { get; private set; }

        public void Toggle(Form form)
        {
            if (IsFullScreen) Exit(form); else Enter(form);
        }

        public void Enter(Form form)
        {
            if (IsFullScreen) return;
            _savedBorderStyle = form.FormBorderStyle;
            _savedState = form.WindowState;
            _savedTopMost = form.TopMost;

            form.FormBorderStyle = FormBorderStyle.None;
            form.WindowState = FormWindowState.Maximized;
            form.TopMost = true;
            IsFullScreen = true;
        }

        public void Exit(Form form)
        {
            if (!IsFullScreen) return;
            form.TopMost = _savedTopMost;
            form.FormBorderStyle = _savedBorderStyle;
            form.WindowState = _savedState;
            IsFullScreen = false;
        }
    }
}
