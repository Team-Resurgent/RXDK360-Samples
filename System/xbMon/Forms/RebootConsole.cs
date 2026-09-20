#region File Information
//-----------------------------------------------------------------------------
// RebootConsole.cs
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
    public partial class RebootConsole : Form
    {
        public RebootConsole(List<string> RebootTargetOptions)
        {
            InitializeComponent();
            RebootTargets.Items.Add("Reboot to Launcher");
            foreach (string Options in RebootTargetOptions)
            {
                RebootTargets.Items.Add(Options);
            }
            RebootTargets.SelectedIndex = 0;
        }

        private void RebootConsole_Load(object sender, EventArgs e)
        {

        }

        public string SelectedExecutable
        {
            get
            {
                if (RebootTargets.SelectedIndex == 0)
                    return null; //Null will cause a reboot to the launcher.
                else if (RebootTargets.SelectedIndex > 0)
                    return RebootTargets.SelectedItem.ToString();
                else
                    return string.Empty;
            }
        }
        public bool ColdBootRequested
        {
            [System.Diagnostics.DebuggerStepThrough]
            get
            {
                return ColdReboot.Checked;
            }
        }
        public string CommandLineArgs
        {
            [System.Diagnostics.DebuggerStepThrough]
            get
            {
                return CommandLineArguments.Text;
            }
        }
    }
}
