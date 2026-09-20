#region File Information
//-----------------------------------------------------------------------------
// Main.cs
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
using XDevkit;
using System.Windows.Forms;
using Microsoft.Win32;
using System.Runtime.InteropServices;

namespace Atg.Samples.xbMon
{
    public partial class Main : Form
    {
        private delegate void SortConsolesDelegate();
        private SortConsolesDelegate SortMyConsoles = null;
        private bool haveRunBefore = false;

        XboxManagerClass XBM = null;
        public Main()
        {
            InitializeComponent();

            SortMyConsoles = SortConsoles;

            XBM = new XboxManagerClass();
            foreach (string Name in XBM.Consoles)
            {
                AddConsoleToList(XBM, Name,false);
            }

            AddRegistryConsoles();

            //Start out hidden.
            Visible = false;
        }

        private void AddRegistryConsoles()
        {
            //Add consoles that have been manually added by the user.
            RegistryKey Consoles = Registry.CurrentUser.CreateSubKey("Software\\Microsoft\\XboxSDK\\xbMon\\Consoles");
            foreach (string Name in Consoles.GetSubKeyNames())
            {
                AddConsoleToList(XBM, Name, true);
            }
            Consoles.Close();
        }

        private void AddConsoleToList(XboxManagerClass XBM, string Name, bool PopulatedByRegistry)
        {
            //Remove duplicates from the list
            foreach (ConsoleUIControl UI in ConsoleList.Controls)
            {
                if (string.Compare(UI.Xbox.ConsoleFriendlyName, Name, true) == 0 ||
                    string.Compare(UI.Xbox.IP, Name, true) == 0)
                {
                    return;
                }
            }

            System.Diagnostics.Debug.WriteLine("Adding " + Name);

            ConsoleUIControl c = new ConsoleUIControl(new XboxConsole(XBM, Name), PopulatedByRegistry);
            
            //Once a control is populated from registry, check to see if it should be visible.
            if (PopulatedByRegistry && !c.Xbox.MonitorVisible)
            {
                c.Dispose();
                return;
            }
            c.ConsoleUIResortRequired += new ConsoleUIControl.OnConsoleUIResortRequired(ConsoleUI_ConsoleUIResortRequired);
            c.Width = ClientRectangle.Width - 30; //30px less to prevent horizontal scrolling
            ConsoleList.Controls.Add(c);
        }

        void ConsoleUI_ConsoleUIResortRequired()
        {
            SortConsoles();
        }

        private void SortConsoles()
        {
            //Handle invocation from other threads.
            if (InvokeRequired)
            {
                //Ignore errors when we're shutting down:
                try { Invoke(SortMyConsoles); }
                catch (ObjectDisposedException) { }
                return;
            }

            //Disable layout, to speed up rendering during sort.
            ConsoleList.SuspendLayout();

            //Create a sorted list of the consoles, sorting by Connected, then by Monitor Visible, then by Console Name.
            SortedList<string, ConsoleUIControl> ListOfConsoles = new SortedList<string, ConsoleUIControl>(ConsoleList.Controls.Count);
            List<ConsoleUIControl> ListOfDuplicates = new List<ConsoleUIControl>();
            foreach (ConsoleUIControl c in ConsoleList.Controls)
            {
                try
                {
                    ListOfConsoles.Add(
                        string.Concat(
                            c.Xbox.Connected ? "A" : "B",
                            c.Xbox.ConsoleFriendlyName), c);
                }
                catch (ArgumentException)
                {
                    //Handle the rare case of a console coming in twice,
                    //Once with name, and once with IP

                    //We need to cycle through again to find which one was 
                    //added through the registry.
                    bool RegistryConsoleFound = false;
                    foreach (ConsoleUIControl c2 in ConsoleList.Controls)
                    {
                        if (
                            (string.Compare(c.Xbox.ConsoleFriendlyName, c2.Xbox.ConsoleFriendlyName, true) == 0 ||
                            string.Compare(c.Xbox.IP, c2.Xbox.IP, true) == 0))
                        {
                            if (c2.PopulatedByRegistry)
                            {
                                ListOfDuplicates.Add(c2);
                                RegistryConsoleFound = true;
                                break;
                            }
                        }
                    }
                    if (!RegistryConsoleFound)
                    {
                        //No registry console was found, remove the current one.
                        ListOfDuplicates.Add(c);
                    }
                }
            }

            //Remove consoles that we now realize are duplicates.
            foreach (ConsoleUIControl c in ListOfDuplicates)
            {
                ConsoleList.Controls.Remove(c);
                c.Close();
            }

            //Perform the sort
            foreach (ConsoleUIControl c in ListOfConsoles.Values)
                c.SendToBack();
            ConsoleList.ResumeLayout();
            Invalidate();
        }

        private void SystemTrayIcon_Click(object sender, EventArgs e)
        {
            if (!haveRunBefore)
                CenterDialogAboveNotificationIcon();

            TopMost = true;
            Visible = !Visible;

            //Bring all the other windows to the front if the main window is visible.
            if (Visible)
            {
                foreach (ConsoleUIControl c in ConsoleList.Controls)
                {
                    if (c.Xbox.ConsoleMonitor.Visible)
                        c.Xbox.ConsoleMonitor.BringToFront();
                }
            }
            WindowState = FormWindowState.Normal;
        }
        private void CenterDialogAboveNotificationIcon()
        {
            Size Monitor = SystemInformation.PrimaryMonitorMaximizedWindowSize;
            Left = System.Windows.Forms.Cursor.Position.X - Width /2;
            if (Right > Monitor.Width)
                Left -= Monitor.Width - Right;
            Top = Monitor.Height - Height;
            Top -= 10;

            Point mouse = PointToClient(new Point(System.Windows.Forms.Cursor.Position.X, System.Windows.Forms.Cursor.Position.Y));
        }

        private void Main_Load(object sender, EventArgs e)
        {
            LoadSettings();
        }

        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            //Exit the application. Note that any children forms Close() 
            //event will not be called.
            Application.Exit();
        }

        private void Main_FormClosing(object sender, FormClosingEventArgs e)
        {
            //If the user is attempting to close the form, minimize instead. 
            //They have to click the Exit menu option to truly close the app.
            if (e.CloseReason == CloseReason.UserClosing)
            {
                Visible = false;
                e.Cancel = true;
            }
            else
            {
                foreach(ConsoleUIControl UI in ConsoleList.Controls)
                {
                    UI.Xbox.Dispose();
                }
                SaveSettings();
            }
        }

        private void add360ToolStripMenuItem_Click(object sender, EventArgs e)
        {
            ConnectionDialog dialog = new ConnectionDialog();
            DialogResult result = dialog.ShowDialog(this);

            if (dialog.ChangesMade)
            {
                XboxManagerClass XBM = new XboxManagerClass();
                foreach (string console in dialog.SelectedConsoles)
                {
                    this.AddConsoleToList(XBM,console,false);
                }
            }
        }

        private void selectAllToolStripMenuItem_Click(object sender, EventArgs e)
        {
            SelectAll();
        }
        private void SelectAll()
        {
            bool SelectAll = selectAllToolStripMenuItem.Text.StartsWith("S");
            if (SelectAll)
                selectAllToolStripMenuItem.Text = "Unselect &All";
            else
                selectAllToolStripMenuItem.Text = "Select &All Active";
            foreach (ConsoleUIControl c in ConsoleList.Controls)
                if (c.Xbox.Connected)
                    c.Selected = SelectAll;
                else
                    c.Selected = false;
        }

        private void Main_KeyDown(object sender, KeyEventArgs e)
        {
            if (e.Control == true && e.KeyCode == Keys.A)
                SelectAll();
        }

        private void BrowseConsoles(string Subdirectory)
        {
            foreach (ConsoleUIControl UI in ConsoleList.Controls)
            {
                if (UI.Selected && UI.Xbox.Connected)
                {
                    UI.Xbox.LaunchExplorer(Subdirectory);
                }
            }
        }

        private void Browse_SubdirToolStripMenuItem_Click(object sender, EventArgs e)
        {
            //Browse the consoles, using the subdirectory stored
            //in the Tag property of the menu item
            BrowseConsoles(((ToolStripMenuItem)sender).Tag.ToString());
        }

        private void Main_Resize(object sender, EventArgs e)
        {
            foreach (ConsoleUIControl c in ConsoleList.Controls)
                c.Width = ClientRectangle.Width - 30; //30px less to prevent horizontal scrolling
        }

        private void rebootSelectedToolStripMenuItem_Click(object sender, EventArgs e)
        {
            List<string> XEXs = null;
            foreach (ConsoleUIControl c in ConsoleList.Controls)
            {
                if (c.Selected && c.Xbox.Connected)
                {
                    // Store the first console's executables.
                    if (XEXs == null)
                    {
                        XEXs = c.Xbox.Executables;
                    }
                    else
                    {
                        // For every other console, only check those files
                        // that exist on the first console.
                        for (int i = 0; i < XEXs.Count; i++)
                        {
                            //If the file doesn't exist, empty it out.
                            if (!string.IsNullOrEmpty(XEXs[i]) &&
                                !c.Xbox.FileExists(XEXs[i]))
                                XEXs[i] = string.Empty;
                        }
                    }
                }
            }

            if (XEXs == null)
            {
                MessageBox.Show("You must first select a console.", "Reboot Console");
                return;
            }

            //Add only those XEXs that are on all consoles
            int j = 0;
            while (j < XEXs.Count)
            {
                if (string.IsNullOrEmpty(XEXs[j]))
                {
                    XEXs.RemoveAt(j);
                }
                else
                {
                    j++;
                }
            }

            RebootConsole rc = new RebootConsole(XEXs);
            if (rc.ShowDialog(this) == DialogResult.OK)
            {
                bool allSuccess = true;
                foreach (ConsoleUIControl c in ConsoleList.Controls)
                {
                    if (c.Selected)
                    {
                        if (!c.Xbox.Reboot(rc.SelectedExecutable, rc.ColdBootRequested, rc.CommandLineArgs))
                            allSuccess = false;
                    }
                }
                if (!allSuccess)
                {
                    MessageBox.Show("At least one console returned an error status attempting to reboto.", "Reboot failure", MessageBoxButtons.OK, MessageBoxIcon.Information);
                }
            }
        }
        private void SaveSettings()
        {
            RegistryKey ConsoleRegistry = Registry.CurrentUser.CreateSubKey("Software\\Microsoft\\XboxSDK\\xbMon\\Consoles");
            ConsoleRegistry.SetValue("Left", Left, RegistryValueKind.DWord);
            ConsoleRegistry.SetValue("Top", Top, RegistryValueKind.DWord);
            ConsoleRegistry.SetValue("Width", Width, RegistryValueKind.DWord);
            ConsoleRegistry.SetValue("Height", Height, RegistryValueKind.DWord);
            ConsoleRegistry.SetValue("HaveRunBefore", true.ToString(), RegistryValueKind.String);
            ConsoleRegistry.Close();
        }
        private void LoadSettings()
        {
            RegistryKey ConsoleRegistry = Registry.CurrentUser.CreateSubKey("Software\\Microsoft\\XboxSDK\\xbMon\\Consoles");
            Left = int.Parse(ConsoleRegistry.GetValue("Left", "0").ToString());
            Top = int.Parse(ConsoleRegistry.GetValue("Top", "0").ToString());
            Width = int.Parse(ConsoleRegistry.GetValue("Width", Width.ToString()).ToString());
            Height = int.Parse(ConsoleRegistry.GetValue("Height", Height.ToString()).ToString());
            haveRunBefore = bool.Parse(ConsoleRegistry.GetValue("HaveRunBefore", false.ToString()).ToString());
            ConsoleRegistry.Close();
        }
    }
}
