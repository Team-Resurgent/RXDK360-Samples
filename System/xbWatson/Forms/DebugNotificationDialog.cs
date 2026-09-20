#region File Information
//-----------------------------------------------------------------------------
// DebugNotificationDialog.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Threading;
using System.Windows.Forms;

namespace Atg.Samples.xbWatson.Forms
{
    public partial class DebugNotificationDialog : Form
    {
        private delegate void VoidDelegate();
    
        private DebugMonitor monitor;
        
        public DebugNotificationDialog(DebugMonitor monitor)
        {
            this.monitor = monitor;
        
            InitializeComponent();
            
            // Load settings.
            WithHeap = Program.GetSetting<bool>("WithHeap", true);
        }

        #region Public Methods/Properties

        /// <summary>
        /// The detailed information about the debug notification.
        /// </summary>
        public string Details
        {
            set
            {
                detailsText.Lines = value.Split("\n".ToCharArray());
                detailsText.Select(0, 0);
            }
        }

        /// <summary>
        /// The icon for the debug notification.
        /// </summary>
        public NotificationIcon NotificationIcon
        {
            set
            {
                int index = (int)value;
                notificationIcon.Image = notificationIcons.Images[index];
            }
        }

        /// <summary>
        /// The summary of the debug notification.
        /// </summary>
        public string Overview { set { overviewLabel.Text = value; } }

        /// <summary>
        /// Specifies whether the minidump will include heap.
        /// </summary>
        public bool WithHeap
        {
            get { return withHeapCheckBox.Checked; }
            set { withHeapCheckBox.Checked = value; }
        }

        #endregion

        #region Internal Methods/Properties
        
        /// <summary>
        /// Worker thread for saving a minidump.
        /// </summary>
        private void SaveMinidumpThread()
        {
            monitor.SaveMinidump(saveFileDialog.FileName, WithHeap);

            // Update UI.
            try
            {
                BeginInvoke(new VoidDelegate(delegate
                {
                    UseWaitCursor = false;
                    Cursor = Cursors.Default;
                    lowerPanel.Enabled = true;
                    Application.DoEvents();
                }));
            }
            catch (InvalidOperationException) { }
        }
        
        #endregion

        #region Event Handlers

        private void saveMinidumpButton_Click(object sender, EventArgs e)
        {
            DialogResult result = saveFileDialog.ShowDialog(this);
            
            if (result == DialogResult.OK)
            {
                UseWaitCursor = true;
                Cursor = Cursors.WaitCursor;
                lowerPanel.Enabled = false;
                Application.DoEvents();

                // Spawn a thread to save the dump.
                new Thread(new ThreadStart(SaveMinidumpThread)).Start();
            }
        }

        private void DebugNotificationDialog_FormClosed(object sender, FormClosedEventArgs e)
        {
            // Save settings.
            Program.SetSetting<bool>("WithHeap", WithHeap);
        }
        
        #endregion
    }

    /// <summary>
    /// The icons that can be displayed in the notification dialog.
    /// </summary>
    public enum NotificationIcon
    {
        Information,
        Warning,
        Error,
    }
}