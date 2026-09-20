namespace Atg.Samples.xbMon
{
    partial class Main
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
            this.components = new System.ComponentModel.Container();
            this.SystemTrayIcon = new System.Windows.Forms.NotifyIcon(this.components);
            this.mnu360List = new System.Windows.Forms.MenuStrip();
            this.add360ToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem2 = new System.Windows.Forms.ToolStripMenuItem();
            this.browseToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.Browse_DEVKITToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.Browse_HDDToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.Browse_GAMEToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.rebootSelectedToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.selectAllToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.exitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.ConsoleList = new System.Windows.Forms.FlowLayoutPanel();
            this.mnu360List.SuspendLayout();
            this.SuspendLayout();
            // 
            // SystemTrayIcon
            // 
            this.SystemTrayIcon.Icon = global::Atg.Samples.xbMon.Resources.Resources.SystemTrayIcon;
            this.SystemTrayIcon.Text = "xbMon";
            this.SystemTrayIcon.Visible = true;
            this.SystemTrayIcon.Click += new System.EventHandler(this.SystemTrayIcon_Click);
            // 
            // mnu360List
            // 
            this.mnu360List.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.add360ToolStripMenuItem,
            this.toolStripMenuItem2,
            this.selectAllToolStripMenuItem,
            this.exitToolStripMenuItem});
            this.mnu360List.LayoutStyle = System.Windows.Forms.ToolStripLayoutStyle.Flow;
            this.mnu360List.Location = new System.Drawing.Point(0, 0);
            this.mnu360List.Name = "mnu360List";
            this.mnu360List.Size = new System.Drawing.Size(254, 23);
            this.mnu360List.TabIndex = 0;
            this.mnu360List.Text = "menuStrip1";
            // 
            // add360ToolStripMenuItem
            // 
            this.add360ToolStripMenuItem.Name = "add360ToolStripMenuItem";
            this.add360ToolStripMenuItem.Padding = new System.Windows.Forms.Padding(0);
            this.add360ToolStripMenuItem.Size = new System.Drawing.Size(81, 19);
            this.add360ToolStripMenuItem.Text = "A&dd/Remove";
            this.add360ToolStripMenuItem.Click += new System.EventHandler(this.add360ToolStripMenuItem_Click);
            // 
            // toolStripMenuItem2
            // 
            this.toolStripMenuItem2.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.browseToolStripMenuItem,
            this.rebootSelectedToolStripMenuItem});
            this.toolStripMenuItem2.Name = "toolStripMenuItem2";
            this.toolStripMenuItem2.Padding = new System.Windows.Forms.Padding(0);
            this.toolStripMenuItem2.Size = new System.Drawing.Size(41, 19);
            this.toolStripMenuItem2.Text = "&Batch";
            // 
            // browseToolStripMenuItem
            // 
            this.browseToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.Browse_DEVKITToolStripMenuItem,
            this.Browse_HDDToolStripMenuItem,
            this.Browse_GAMEToolStripMenuItem});
            this.browseToolStripMenuItem.Name = "browseToolStripMenuItem";
            this.browseToolStripMenuItem.Size = new System.Drawing.Size(159, 22);
            this.browseToolStripMenuItem.Text = "Browse Selected";
            // 
            // Browse_DEVKITToolStripMenuItem
            // 
            this.Browse_DEVKITToolStripMenuItem.Name = "Browse_DEVKITToolStripMenuItem";
            this.Browse_DEVKITToolStripMenuItem.Size = new System.Drawing.Size(275, 22);
            this.Browse_DEVKITToolStripMenuItem.Tag = "DEVKIT";
            this.Browse_DEVKITToolStripMenuItem.Text = "Game Development Volume (DEVKIT:)";
            this.Browse_DEVKITToolStripMenuItem.Click += new System.EventHandler(this.Browse_SubdirToolStripMenuItem_Click);
            // 
            // Browse_HDDToolStripMenuItem
            // 
            this.Browse_HDDToolStripMenuItem.Name = "Browse_HDDToolStripMenuItem";
            this.Browse_HDDToolStripMenuItem.Size = new System.Drawing.Size(275, 22);
            this.Browse_HDDToolStripMenuItem.Tag = "HDD";
            this.Browse_HDDToolStripMenuItem.Text = "Retail Hard Drive Emulation (HDD:)";
            this.Browse_HDDToolStripMenuItem.Click += new System.EventHandler(this.Browse_SubdirToolStripMenuItem_Click);
            // 
            // Browse_GAMEToolStripMenuItem
            // 
            this.Browse_GAMEToolStripMenuItem.Name = "Browse_GAMEToolStripMenuItem";
            this.Browse_GAMEToolStripMenuItem.Size = new System.Drawing.Size(275, 22);
            this.Browse_GAMEToolStripMenuItem.Tag = "GAME";
            this.Browse_GAMEToolStripMenuItem.Text = "Active Title Media (GAME:)";
            this.Browse_GAMEToolStripMenuItem.Click += new System.EventHandler(this.Browse_SubdirToolStripMenuItem_Click);
            // 
            // rebootSelectedToolStripMenuItem
            // 
            this.rebootSelectedToolStripMenuItem.Name = "rebootSelectedToolStripMenuItem";
            this.rebootSelectedToolStripMenuItem.Size = new System.Drawing.Size(159, 22);
            this.rebootSelectedToolStripMenuItem.Text = "Reboot Selected";
            this.rebootSelectedToolStripMenuItem.Click += new System.EventHandler(this.rebootSelectedToolStripMenuItem_Click);
            // 
            // selectAllToolStripMenuItem
            // 
            this.selectAllToolStripMenuItem.Name = "selectAllToolStripMenuItem";
            this.selectAllToolStripMenuItem.Padding = new System.Windows.Forms.Padding(0);
            this.selectAllToolStripMenuItem.Size = new System.Drawing.Size(95, 19);
            this.selectAllToolStripMenuItem.Text = "Select &All Active";
            this.selectAllToolStripMenuItem.Click += new System.EventHandler(this.selectAllToolStripMenuItem_Click);
            // 
            // exitToolStripMenuItem
            // 
            this.exitToolStripMenuItem.Name = "exitToolStripMenuItem";
            this.exitToolStripMenuItem.Padding = new System.Windows.Forms.Padding(0);
            this.exitToolStripMenuItem.Size = new System.Drawing.Size(29, 19);
            this.exitToolStripMenuItem.Text = "E&xit";
            this.exitToolStripMenuItem.Click += new System.EventHandler(this.exitToolStripMenuItem_Click);
            // 
            // ConsoleList
            // 
            this.ConsoleList.AutoScroll = true;
            this.ConsoleList.Dock = System.Windows.Forms.DockStyle.Fill;
            this.ConsoleList.FlowDirection = System.Windows.Forms.FlowDirection.TopDown;
            this.ConsoleList.Location = new System.Drawing.Point(0, 23);
            this.ConsoleList.Name = "ConsoleList";
            this.ConsoleList.Size = new System.Drawing.Size(254, 241);
            this.ConsoleList.TabIndex = 2;
            this.ConsoleList.WrapContents = false;
            // 
            // Main
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(254, 264);
            this.Controls.Add(this.ConsoleList);
            this.Controls.Add(this.mnu360List);
            this.DoubleBuffered = true;
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
            this.KeyPreview = true;
            this.MainMenuStrip = this.mnu360List;
            this.Name = "Main";
            this.ShowInTaskbar = false;
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Show;
            this.StartPosition = System.Windows.Forms.FormStartPosition.Manual;
            this.TopMost = true;
            this.Load += new System.EventHandler(this.Main_Load);
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.Main_FormClosing);
            this.Resize += new System.EventHandler(this.Main_Resize);
            this.KeyDown += new System.Windows.Forms.KeyEventHandler(this.Main_KeyDown);
            this.mnu360List.ResumeLayout(false);
            this.mnu360List.PerformLayout();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.NotifyIcon SystemTrayIcon;
        private System.Windows.Forms.MenuStrip mnu360List;
        private System.Windows.Forms.ToolStripMenuItem add360ToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem exitToolStripMenuItem;
        private System.Windows.Forms.FlowLayoutPanel ConsoleList;
        private System.Windows.Forms.ToolStripMenuItem selectAllToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem toolStripMenuItem2;
        private System.Windows.Forms.ToolStripMenuItem browseToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem Browse_DEVKITToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem Browse_HDDToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem Browse_GAMEToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem rebootSelectedToolStripMenuItem;
    }
}

