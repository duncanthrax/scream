using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace ScreamReader
{
    public partial class About : Form
    {
        public About()
        {
            InitializeComponent();
        }

        private void LinkSebastianClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            Process.Start("https://github.com/mrshoenel");
        }

        private void LinkScreamClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            Process.Start("https://github.com/duncanthrax/scream");
        }
    }
}
