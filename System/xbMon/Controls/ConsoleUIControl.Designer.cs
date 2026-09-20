namespace Atg.Samples.xbMon
{
    partial class ConsoleUIControl
    {
        /// <summary> 
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;



        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            this.ConsoleName = new System.Windows.Forms.Label();
            this.ConsoleIP = new System.Windows.Forms.Label();
            this.Drives = new System.Windows.Forms.ContextMenuStrip(this.components);
            this.CurrentProgram = new System.Windows.Forms.Label();
            this.Reboot = new System.Windows.Forms.PictureBox();
            this.FolderIcon = new System.Windows.Forms.PictureBox();
            this.MonitorConsole = new System.Windows.Forms.PictureBox();
            this.ConsoleIcon = new System.Windows.Forms.PictureBox();
            this.CloseUI = new System.Windows.Forms.PictureBox();
            ((System.ComponentModel.ISupportInitialize)(this.Reboot)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.FolderIcon)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.MonitorConsole)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.ConsoleIcon)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.CloseUI)).BeginInit();
            this.SuspendLayout();
            // 
            // ConsoleName
            // 
            this.ConsoleName.AutoEllipsis = true;
            this.ConsoleName.BackColor = System.Drawing.Color.Transparent;
            this.ConsoleName.Location = new System.Drawing.Point(48, 3);
            this.ConsoleName.Name = "ConsoleName";
            this.ConsoleName.Size = new System.Drawing.Size(118, 13);
            this.ConsoleName.TabIndex = 1;
            this.ConsoleName.Text = "Name";
            this.ConsoleName.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ConsoleUIControl_MouseClick);
            // 
            // ConsoleIP
            // 
            this.ConsoleIP.AutoEllipsis = true;
            this.ConsoleIP.BackColor = System.Drawing.Color.Transparent;
            this.ConsoleIP.Location = new System.Drawing.Point(48, 16);
            this.ConsoleIP.Name = "ConsoleIP";
            this.ConsoleIP.Size = new System.Drawing.Size(118, 13);
            this.ConsoleIP.TabIndex = 1;
            this.ConsoleIP.Text = "IP";
            this.ConsoleIP.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ConsoleUIControl_MouseClick);
            // 
            // Drives
            // 
            this.Drives.Name = "Drives";
            this.Drives.Size = new System.Drawing.Size(61, 4);
            // 
            // CurrentProgram
            // 
            this.CurrentProgram.AutoEllipsis = true;
            this.CurrentProgram.BackColor = System.Drawing.Color.Transparent;
            this.CurrentProgram.Location = new System.Drawing.Point(48, 29);
            this.CurrentProgram.Name = "CurrentProgram";
            this.CurrentProgram.Size = new System.Drawing.Size(118, 13);
            this.CurrentProgram.TabIndex = 1;
            this.CurrentProgram.Text = "CurrentProgram";
            this.CurrentProgram.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ConsoleUIControl_MouseClick);
            // 
            // Reboot
            // 
            this.Reboot.Image = global::Atg.Samples.xbMon.Resources.Resources.Restart;
            this.Reboot.Location = new System.Drawing.Point(6, 11);
            this.Reboot.Name = "Reboot";
            this.Reboot.Size = new System.Drawing.Size(10, 12);
            this.Reboot.TabIndex = 0;
            this.Reboot.TabStop = false;
            this.Reboot.Click += new System.EventHandler(this.Reboot_Click);
            // 
            // FolderIcon
            // 
            this.FolderIcon.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.FolderIcon.Image = global::Atg.Samples.xbMon.Resources.Resources.Folders;
            this.FolderIcon.Location = new System.Drawing.Point(174, 18);
            this.FolderIcon.Name = "FolderIcon";
            this.FolderIcon.Size = new System.Drawing.Size(24, 24);
            this.FolderIcon.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.FolderIcon.TabIndex = 0;
            this.FolderIcon.TabStop = false;
            this.FolderIcon.MouseClick += new System.Windows.Forms.MouseEventHandler(this.FolderIcon_MouseClick);
            // 
            // MonitorConsole
            // 
            this.MonitorConsole.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.MonitorConsole.Image = global::Atg.Samples.xbMon.Resources.Resources.MonitorOff;
            this.MonitorConsole.Location = new System.Drawing.Point(204, 18);
            this.MonitorConsole.Name = "MonitorConsole";
            this.MonitorConsole.Size = new System.Drawing.Size(24, 24);
            this.MonitorConsole.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.MonitorConsole.TabIndex = 0;
            this.MonitorConsole.TabStop = false;
            this.MonitorConsole.MouseClick += new System.Windows.Forms.MouseEventHandler(this.MonitorConsole_MouseClick);
            // 
            // ConsoleIcon
            // 
            this.ConsoleIcon.Image = global::Atg.Samples.xbMon.Resources.Resources.Console;
            this.ConsoleIcon.Location = new System.Drawing.Point(3, 3);
            this.ConsoleIcon.Name = "ConsoleIcon";
            this.ConsoleIcon.Size = new System.Drawing.Size(39, 42);
            this.ConsoleIcon.TabIndex = 0;
            this.ConsoleIcon.TabStop = false;
            this.ConsoleIcon.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ConsoleUIControl_MouseClick);
            this.ConsoleIcon.Paint += new System.Windows.Forms.PaintEventHandler(this.ConsoleIcon_Paint);
            // 
            // CloseUI
            // 
            this.CloseUI.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
            this.CloseUI.Image = global::Atg.Samples.xbMon.Resources.Resources.Close;
            this.CloseUI.Location = new System.Drawing.Point(216, 0);
            this.CloseUI.Name = "CloseUI";
            this.CloseUI.Size = new System.Drawing.Size(15, 16);
            this.CloseUI.SizeMode = System.Windows.Forms.PictureBoxSizeMode.StretchImage;
            this.CloseUI.TabIndex = 0;
            this.CloseUI.TabStop = false;
            this.CloseUI.MouseClick += new System.Windows.Forms.MouseEventHandler(this.CloseUI_MouseClick);
            // 
            // ConsoleUIControl
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.BackColor = System.Drawing.Color.Transparent;
            this.Controls.Add(this.FolderIcon);
            this.Controls.Add(this.MonitorConsole);
            this.Controls.Add(this.Reboot);
            this.Controls.Add(this.CurrentProgram);
            this.Controls.Add(this.CloseUI);
            this.Controls.Add(this.ConsoleIP);
            this.Controls.Add(this.ConsoleName);
            this.Controls.Add(this.ConsoleIcon);
            this.Name = "ConsoleUIControl";
            this.Size = new System.Drawing.Size(231, 45);
            this.Load += new System.EventHandler(this.ConsoleUIControl_Load);
            this.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ConsoleUIControl_MouseClick);
            ((System.ComponentModel.ISupportInitialize)(this.Reboot)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.FolderIcon)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.MonitorConsole)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.ConsoleIcon)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.CloseUI)).EndInit();
            this.ResumeLayout(false);

        }

        #endregion

        private System.Windows.Forms.PictureBox ConsoleIcon;
        private System.Windows.Forms.Label ConsoleName;
        private System.Windows.Forms.Label ConsoleIP;
        private System.Windows.Forms.PictureBox FolderIcon;
        private System.Windows.Forms.ContextMenuStrip Drives;
        private System.Windows.Forms.Label CurrentProgram;
        private System.Windows.Forms.PictureBox MonitorConsole;
        private System.Windows.Forms.PictureBox Reboot;
        private System.Windows.Forms.PictureBox CloseUI;



    }
}
