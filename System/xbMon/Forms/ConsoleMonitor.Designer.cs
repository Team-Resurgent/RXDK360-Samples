namespace Atg.Samples.xbMon
{
    partial class ConsoleMonitor
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
            this.SuspendLayout();
            // 
            // ConsoleMonitor
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.Black;
            this.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Zoom;
            this.ClientSize = new System.Drawing.Size(478, 352);
            this.DoubleBuffered = true;
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
            this.Icon = global::Atg.Samples.xbMon.Resources.Resources.SystemTrayIcon;
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "ConsoleMonitor";
            this.ShowIcon = false;
            this.ShowInTaskbar = false;
            this.StartPosition = System.Windows.Forms.FormStartPosition.Manual;
            this.Text = "ConsoleMonitor";
            this.TopMost = true;
            this.Load += new System.EventHandler(this.ConsoleMonitor_Load);
            this.VisibleChanged += new System.EventHandler(this.ConsoleMonitor_VisibleChanged);
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.ConsoleMonitor_FormClosing);
            this.ResumeLayout(false);

        }

        #endregion

    }
}