#region File Information
//-----------------------------------------------------------------------------
// ConsoleUIControl.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.Text;
using System.Windows.Forms;

namespace Atg.Samples.xbMon
{
    public partial class ConsoleUIControl : UserControl
    {
        private XboxConsole xbox = null;
        private RectangleF PowerButtonRect;
        private bool isSelected = false;
        private SolidBrush selectedBackcolor = new SolidBrush(Color.FromArgb(100, Color.DarkGreen));

        XboxConsole.XboxConnectionEventHandler ConnectionChangeHandler = null;
        XboxConsole.RunningProgramChangedEventHandler RunningProgramChangedHandler = null;

        private delegate void UpdateControlDataDelegate();
        private UpdateControlDataDelegate UpdateMyControlData;

        public delegate void OnConsoleUIResortRequired();
        public event OnConsoleUIResortRequired ConsoleUIResortRequired;

        private bool wasPopulatedByRegistry = false;
        public bool PopulatedByRegistry
        {
            get { return wasPopulatedByRegistry; }
        }

        public ConsoleUIControl(XboxConsole Xbox, bool PopulatedByRegistry)
        {
            InitializeComponent();
            wasPopulatedByRegistry = PopulatedByRegistry;
            xbox = Xbox;
            ConnectionChangeHandler = new XboxConsole.XboxConnectionEventHandler(xbox_OnConnectionChange);
            RunningProgramChangedHandler = new XboxConsole.RunningProgramChangedEventHandler(xbox_OnRunningProgramChange);

            xbox.ConsoleMonitor.VisibleChanged += new EventHandler(xbox_ConsoleMonitorVisibilityChanged);
            UpdateMyControlData = UpdateControlData;
            xbox.ConsoleMonitor.VisibleChanged += new EventHandler(ConsoleMonitor_VisibleChanged);


            //Create the green/red connectivity status region
            //The following values center the power buttons on the UI, but it is rather small: 16,-11,8,8
            PowerButtonRect = new Rectangle(0, ConsoleIcon.Height - 15, 15, 15);
            xbox.OnConnectionChange += new XboxConsole.XboxConnectionEventHandler(xbox_OnConnectionChange);
            xbox.OnRunningProgramChanged += new XboxConsole.RunningProgramChangedEventHandler(xbox_OnRunningProgramChange);

            xbox.Connect();
        }


        void ConsoleMonitor_VisibleChanged(object sender, EventArgs e)
        {
            HandleConsoleMonitorVisibilityChange(true);
        }

        public XboxConsole Xbox
        {
            [System.Diagnostics.DebuggerStepThrough]
            get { return xbox; }
        }
        private void ConsoleUIControl_Load(object sender, EventArgs e)
        {
            UpdateControlData();
        }

        void xbox_ConsoleMonitorVisibilityChanged(object source, EventArgs e)
        {
            UpdateControlData();
        }

        void xbox_OnRunningProgramChange(object source, string ProgramName)
        {
            UpdateControlData();
        }

        void xbox_OnConnectionChange(object source, bool Connected)
        {
            UpdateControlData();
            if (ConsoleUIResortRequired != null)
                ConsoleUIResortRequired();
        }

        private void UpdateControlData()
        {
            try
            {
                //Handle events coming in through seperate threads.
                if (InvokeRequired)
                {
                    //Ignore errors when we're shutting down:
                    try { Invoke(UpdateMyControlData); }
                    catch (ObjectDisposedException) { }
                    return;
                }
                ConsoleName.Text = xbox.ConsoleFriendlyName;
                ConsoleIP.Text = xbox.IP;
                CurrentProgram.Text = xbox.CurrentProgram;

                //Update the monitor status.
                if (xbox.ConsoleMonitor.Visible)
                    MonitorConsole.Image = xbMon.Resources.Resources.MonitorOn;
                else
                    MonitorConsole.Image = xbMon.Resources.Resources.MonitorOff;

                //Update if we're now the default console
                if (xbox.IsDefaultConsole)
                    ConsoleIcon.Image = xbMon.Resources.Resources.DefaultConsole;
                else
                    ConsoleIcon.Image = xbMon.Resources.Resources.Console;

                //Redraw the power status
                ConsoleIcon.Invalidate();
            }
            //When shutting down, objects can be disposed underneath us.
            catch (ObjectDisposedException) { }
            catch (NullReferenceException) { }
        }

        private void ConsoleIcon_Paint(object sender, PaintEventArgs e)
        {
            if (xbox.Locked)
            {
                e.Graphics.DrawImage(xbMon.Resources.Resources.Locked, PowerButtonRect);
            }
            else if (xbox.Connected)
            {
                e.Graphics.DrawImage(xbMon.Resources.Resources.PowerOn, PowerButtonRect);
            }
            else
            {
                e.Graphics.DrawImage(xbMon.Resources.Resources.PowerOff, PowerButtonRect);
            }
        }

        public bool Selected
        {
            [System.Diagnostics.DebuggerStepThrough]
            get { return isSelected; }
            [System.Diagnostics.DebuggerStepThrough]
            set { isSelected = value; Invalidate(); }
        }

        private void ConsoleUIControl_MouseClick(object sender, MouseEventArgs e)
        {
            isSelected = !isSelected;
            Invalidate();
        }
        protected override void OnPaintBackground(PaintEventArgs e)
        {
            base.OnPaintBackground(e);

            if (isSelected)
                e.Graphics.FillRectangle(selectedBackcolor, e.ClipRectangle);
        }

        private void FolderIcon_MouseClick(object sender, MouseEventArgs e)
        {
            if (!xbox.Connected)
                return;

            //Launch the explorer on the selected drive.
            Drives.Items.Clear();
            string[] drives = xbox.Drives;
            if (drives != null)
            {
                foreach (string Drive in drives)
                {
                    string LabelText = Drive.ToUpper().Trim();
                    //Frendly name the various acronyms
                    switch (LabelText)
                    {
                        case "DEVKIT":
                            LabelText = "Game Development Volume (DEVKIT:)";
                            break;
                        case "GAME":
                            LabelText = "Active Title Media (GAME:)";
                            break;
                        case "HDD":
                            LabelText = "Retail Hard Drive Emulaton (HDD:)";
                            break;
                        case "MU0":
                            LabelText = "Memory Unit 1 (MU0:)";
                            break;
                        case "MU1":
                            LabelText = "Memory Unit 2 (MU1:)";
                            break;
                        case "DVD":
                            LabelText = "DVD drive (DVD:)";
                            break;
                        case "ROOT":
                            LabelText = "Xbox Neighborhood for Console";
                            break;
                    }
                    ToolStripMenuItem item = new ToolStripMenuItem(LabelText, null, BrowseDrive_Click);
                    if (string.Compare(Drive, "ROOT", true) == 0)
                        item.Tag = "";
                    else
                        item.Tag = Drive;
                    Drives.Items.Add(item);
                }
                Drives.Show(PointToScreen(new Point(e.X, e.Y)));
            }
        }

        private void BrowseDrive_Click(object sender, EventArgs e)
        {
            //Browse the consoles, using the subdirectory stored
            //in the Tag property of the menu item
            xbox.LaunchExplorer(((ToolStripMenuItem)sender).Tag.ToString());
        }


        private void Reboot_Click(object sender, EventArgs e)
        {
            RebootConsole rc = new RebootConsole(Xbox.Executables);
            if (rc.ShowDialog(this) == DialogResult.OK)
            {
                if (!xbox.Reboot(rc.SelectedExecutable, rc.ColdBootRequested, rc.CommandLineArgs))
                {
                    MessageBox.Show("The console returned an error status attempting to reboto.", "Reboot failure", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
        }



        private void MonitorConsole_MouseClick(object sender, MouseEventArgs e)
        {
            HandleConsoleMonitorVisibilityChange(false);
        }
        private void HandleConsoleMonitorVisibilityChange(bool SenderWasMonitor)
        {
            //Toggle the monitor image, if the user clicked on me
            if (!SenderWasMonitor)
                xbox.ConsoleMonitor.Visible = !xbox.ConsoleMonitor.Visible;

            //Let the list know it needs to resort
            if (ConsoleUIResortRequired != null)
                ConsoleUIResortRequired();

            //And then restart the console timer, if the user clicked me
            if (xbox.ConsoleMonitor.Visible && !SenderWasMonitor)
                xbox.StartScreenshotTimer(0);
        }

        /// <summary> 
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
                Xbox.Dispose();
            }
            base.Dispose(disposing);
        }
        public void Close()
        {
            xbox.MonitorVisible = false;
            Dispose();
        }

        private void CloseUI_MouseClick(object sender, MouseEventArgs e)
        {
            Close();
        }

    }
}
