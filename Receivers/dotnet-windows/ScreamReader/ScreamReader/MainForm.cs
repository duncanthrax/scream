using System;
using System.ComponentModel;
using System.Windows.Forms;

namespace ScreamReader
{
    public partial class MainForm : Form
    {
        private ScreamReaderTray screamReaderTray;

        public MainForm(ScreamReaderTray screamReaderTray)
        {
            InitializeComponent();

            this.screamReaderTray = screamReaderTray;

            this.FormClosing += (object sender, FormClosingEventArgs e) =>
            {
                if (e.CloseReason == CloseReason.UserClosing)
                {
                    e.Cancel = true;
                    this.Hide();
                }
            };

            this.loudnessFader.ValueChanged += (object sender, EventArgs e) =>
            {
                this.screamReaderTray.udpPlayer.Volume = this.loudnessFader.Value;
                this.volumeLabel.Text = this.loudnessFader.Value.ToString() + "%";
            };

            this.loudnessFader.Value = this.screamReaderTray.udpPlayer.Volume;
        }

        private void FileOnExitClick(object sender, EventArgs e)
        {
            this.screamReaderTray.udpPlayer.Dispose();
            Application.Exit();
        }

        private void AboutToolStripMenuItem_Click(object sender, EventArgs e)
        {
            var about = new About();
            about.ShowDialog(this);
        }
    }
}
