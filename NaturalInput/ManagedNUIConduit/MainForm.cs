#region File Information
//-----------------------------------------------------------------------------
// MainForm.cs
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Windows.Forms;

namespace Microsoft.ATG.ManagedNUIConduit
{
    public partial class MainForm : Form
    {
        public MainForm()
        {
            InitializeComponent();
        }

        private void showConsumerButton_Click(object sender, EventArgs e)
        {
            showConsumerButton.Enabled = false;
            showRecordPlaybackButton.Enabled = false;

            using (ConsumerForm consumerForm = new ConsumerForm())
            {
                consumerForm.ShowDialog(this);
            }

            showConsumerButton.Enabled = true;
            showRecordPlaybackButton.Enabled = true;
        }

        private void showRecordPlaybackButton_Click(object sender, EventArgs e)
        {
            showConsumerButton.Enabled = false;
            showRecordPlaybackButton.Enabled = false;

            using (RecordPlaybackForm recordPlaybackForm = new RecordPlaybackForm())
            {
                recordPlaybackForm.ShowDialog(this);
            }

            showConsumerButton.Enabled = true;
            showRecordPlaybackButton.Enabled = true;
        }
    }
}
