using System;
using System.Windows.Forms;

namespace ScreamReader
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new ScreamReaderTray());
        }
    }


    public class ScreamReaderTray : Form
    {
        protected internal NotifyIcon trayIcon;

        protected ContextMenu trayMenu;

        internal UdpWaveStreamPlayer udpPlayer;

        private MainForm mainForm;

        public ScreamReaderTray()
        {
            this.udpPlayer = new UdpWaveStreamPlayer();
            this.udpPlayer.Start();
            this.mainForm = new MainForm(this);
            this.trayMenu = new ContextMenu();

            this.trayIcon = new NotifyIcon();
            this.trayIcon.Text = "ScreamReader";
            this.trayIcon.Icon = Properties.Resources.speaker_ico;

            // Add menu to tray icon and show it.
            this.trayIcon.ContextMenu = trayMenu;
            this.trayIcon.Visible = true;
            this.trayIcon.DoubleClick += (object sender, EventArgs e) =>
            {
                if (this.mainForm.Visible)
                {
                    this.mainForm.Focus();
                    return;
                }
                this.mainForm.ShowDialog(this);
            };

            trayMenu.MenuItems.Add("Exit", this.OnExit);
        }

        private void OnExit(object sender, EventArgs e)
        {
            this.udpPlayer.Dispose();
            trayIcon.Visible = false;
            Application.Exit();
        }

        protected override void OnLoad(EventArgs e)
        {
            this.Visible = false;
            this.ShowInTaskbar = false;

            base.OnLoad(e);
        }
    }
}
