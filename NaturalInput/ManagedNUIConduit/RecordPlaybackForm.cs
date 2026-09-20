#region File Information
//-----------------------------------------------------------------------------
// RecordPlaybackForm.cs
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Threading;
using System.Windows.Forms;
using Microsoft.ATG.NUI;

namespace Microsoft.ATG.ManagedNUIConduit
{
    public partial class RecordPlaybackForm : Form
    {
        private delegate void VoidDelegate();

        /// <summary>
        /// Updates the status text on the form.  Callable from non-UI threads.
        /// </summary>
        private void UpdateStatus(string message)
        {
            if (!statusLabel.IsHandleCreated)
                return;

            statusLabel.BeginInvoke(new VoidDelegate(delegate
            {
                statusLabel.Text = message;
            }));
        }

        public RecordPlaybackForm()
        {
            InitializeComponent();

            recordButton.Enabled = false;
            stopButton.Enabled = false;
            playButton.Enabled = false;

            colorCheckBox.Enabled = false;
            depthCheckBox.Enabled = false;
        }

        private void returnButton_Click(object sender, EventArgs e)
        {
            this.Hide();
        }

        private NotificationCallback notification = null;

        private void RecordPlaybackForm_Load(object sender, EventArgs e)
        {
            // Use a worker thread to avoid stalling the UI thread.
            ThreadPool.QueueUserWorkItem(new WaitCallback(delegate
            {
                try
                {
                    UpdateStatus("Connecting to default console...");

                    XStudio.Connect(null, ConnectionType.Tcp);

                    StreamType streamTypes = StreamType.Color | StreamType.Depth;

                    notification = NotificationCallback.Register(null, streamTypes, Notification.PlaybackStopped);
                    notification.NotificationEvent += new NotificationEventHandler(notification_NotificationEvent);

                    UpdateStatus("Connected.");

                    recordButton.BeginInvoke(new VoidDelegate(delegate
                    {
                        recordButton.Enabled = true;
                        colorCheckBox.Enabled = true;
                        depthCheckBox.Enabled = true;

                        colorCheckBox.Checked = true;
                        depthCheckBox.Checked = true;
                    }));
                }
                catch (NUI.ApiException ex)
                {
                    // Report the error to the user.
                    UpdateStatus(string.Concat("Error: ", ex.Message));
                }
            }));
        }

        private void RecordPlaybackForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                if (file != null)
                {
                    file.Dispose();
                    file = null;
                }

                if (notification != null)
                {
                    notification.Dispose();
                    notification = null;
                }

                if (XStudio.IsConnected)
                    XStudio.Disconnect();
            }
            catch (NUI.ApiException)
            {
                // Ignore errors when shutting down.
            }
        }

        void notification_NotificationEvent(object context, NotificationEventArgs args)
        {
            if (args.Notification == Notification.PlaybackStopped)
            {
                stopButton.BeginInvoke(new VoidDelegate(delegate
                {
                    if (stopButton.Enabled)
                        stopButton_Click(stopButton, EventArgs.Empty);
                }));
            }
        }

        private XStudioFile file = null;
        private bool wasColorRecorded = false;
        private bool wasDepthRecorded = false;

        private void recordButton_Click(object sender, EventArgs e)
        {
            if (!colorCheckBox.Checked && !depthCheckBox.Checked)
            {
                MessageBox.Show(
                    this,
                    "At least one stream type must be selected for recording.",
                    "Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            DialogResult result = saveFileDialog.ShowDialog(this);
            if (result == DialogResult.Cancel)
                return;

            recordButton.Enabled = false;
            stopButton.Enabled = false;
            playButton.Enabled = false;

            colorCheckBox.Enabled = false;
            depthCheckBox.Enabled = false;

            try
            {
                UpdateStatus("Preparing to record...");

                wasColorRecorded = colorCheckBox.Checked;
                wasDepthRecorded = depthCheckBox.Checked;

                StreamType streamTypes = StreamType.None;
                if (colorCheckBox.Checked) streamTypes |= StreamType.Color;
                if (depthCheckBox.Checked) streamTypes |= StreamType.Depth;

                file = XStudioFile.Create(saveFileDialog.FileName, true, streamTypes);

                file.MappedStreams = streamTypes;
                XStudio.Start(file.MappedStreams);

                UpdateStatus("Recording...");

                stopButton.Enabled = true;
            }
            catch (NUI.ApiException ex)
            {
                // Clean up in event of error.
                if (file != null)
                {
                    file.Dispose();
                    file = null;
                }

                // Report the error to the user.
                UpdateStatus(string.Concat("Error: ", ex.Message));
            }
        }

        private void stopButton_Click(object sender, EventArgs e)
        {
            recordButton.Enabled = false;
            stopButton.Enabled = false;
            playButton.Enabled = false;

            colorCheckBox.Enabled = false;
            depthCheckBox.Enabled = false;

            try
            {
                UpdateStatus("Stopping...");

                XStudio.Stop(file.MappedStreams);

                if (file != null)
                {
                    file.Dispose();
                    file = null;
                }

                UpdateStatus("Ready for playback.");

                recordButton.Enabled = true;
                playButton.Enabled = true;

                colorCheckBox.Enabled = true;
                depthCheckBox.Enabled = true;
            }
            catch (NUI.ApiException ex)
            {
                // Report the error to the user.
                UpdateStatus(string.Concat("Error: ", ex.Message));
            }
        }

        private void playButton_Click(object sender, EventArgs e)
        {
            if (!colorCheckBox.Checked && !depthCheckBox.Checked)
            {
                MessageBox.Show(
                    this,
                    "At least one stream type must be selected for play back.",
                    "Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }
            else if (colorCheckBox.Checked && !wasColorRecorded)
            {
                MessageBox.Show(
                    this,
                    "A color stream was not recorded and cannot be selected for play back.",
                    "Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }
            else if (depthCheckBox.Checked && !wasDepthRecorded)
            {
                MessageBox.Show(
                    this,
                    "A depth stream was not recorded and cannot be selected for play back.",
                    "Error",
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
                return;
            }

            recordButton.Enabled = false;
            stopButton.Enabled = false;
            playButton.Enabled = false;

            colorCheckBox.Enabled = false;
            depthCheckBox.Enabled = false;

            try
            {
                UpdateStatus("Preparing to play back...");

                StreamType streamTypes = StreamType.None;
                if (colorCheckBox.Checked) streamTypes |= StreamType.Color;
                if (depthCheckBox.Checked) streamTypes |= StreamType.Depth;

                file = XStudioFile.Open(saveFileDialog.FileName);

                file.MappedStreams = streamTypes;
                XStudio.Start(file.MappedStreams);

                UpdateStatus("Playing back...");

                stopButton.Enabled = true;
            }
            catch (NUI.ApiException ex)
            {
                // Clean up in event of error.
                if (file != null)
                {
                    file.Dispose();
                    file = null;
                }

                // Report the error to the user.
                UpdateStatus(string.Concat("Error: ", ex.Message));
            }
        }
    }
}
