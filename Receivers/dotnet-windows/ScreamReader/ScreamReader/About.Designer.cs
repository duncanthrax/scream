namespace ScreamReader
{
    partial class About
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.linkSebastian = new System.Windows.Forms.LinkLabel();
            this.label3 = new System.Windows.Forms.Label();
            this.linkScream = new System.Windows.Forms.LinkLabel();
            this.SuspendLayout();
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(13, 13);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(409, 13);
            this.label1.TabIndex = 0;
            this.label1.Text = "ScreamReader is a .Net-based application to read Wave from UDP multicast streams." +
    "";
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(12, 30);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(122, 13);
            this.label2.TabIndex = 1;
            this.label2.Text = "Author: Sebastian Hönel";
            // 
            // linkSebastian
            // 
            this.linkSebastian.AutoSize = true;
            this.linkSebastian.Location = new System.Drawing.Point(131, 30);
            this.linkSebastian.Name = "linkSebastian";
            this.linkSebastian.Size = new System.Drawing.Size(148, 13);
            this.linkSebastian.TabIndex = 2;
            this.linkSebastian.TabStop = true;
            this.linkSebastian.Text = "https://github.com/mrshoenel";
            this.linkSebastian.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.LinkSebastianClicked);
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(13, 60);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(103, 13);
            this.label3.TabIndex = 3;
            this.label3.Text = "Credits go to scream";
            // 
            // linkScream
            // 
            this.linkScream.AutoSize = true;
            this.linkScream.Location = new System.Drawing.Point(113, 60);
            this.linkScream.Name = "linkScream";
            this.linkScream.Size = new System.Drawing.Size(198, 13);
            this.linkScream.TabIndex = 4;
            this.linkScream.TabStop = true;
            this.linkScream.Text = "https://github.com/duncanthrax/scream";
            this.linkScream.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.LinkScreamClicked);
            // 
            // About
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(437, 88);
            this.Controls.Add(this.linkScream);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.linkSebastian);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.label1);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Name = "About";
            this.Text = "About ScreamReader";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.LinkLabel linkSebastian;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.LinkLabel linkScream;
    }
}