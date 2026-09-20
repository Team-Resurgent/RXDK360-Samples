#region File Information
//-----------------------------------------------------------------------------
// ConsoleMonitor.cs
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
    public partial class ConsoleMonitor : Form
    {
        private XboxConsole console = null;

        private delegate void UpdateTitleDelegate();
        private UpdateTitleDelegate UpdateMyTitle =null;

        private delegate void ScreenshotAcquiredDelegate(Image Screenshot);
        private ScreenshotAcquiredDelegate UpdateMyScreenshot = null;

       
        internal ConsoleMonitor(XboxConsole XboxConsole)
        {
            InitializeComponent();

            UpdateMyTitle = UpdateTitle;
            UpdateMyScreenshot = console_OnScreenshotAcquired;

            console = XboxConsole;
            console.OnConnectionChange += new XboxConsole.XboxConnectionEventHandler(console_OnConnectionChange);
            console.OnRunningProgramChanged += new XboxConsole.RunningProgramChangedEventHandler(console_OnRunningProgramChange);
            console.OnScreenshotAcquired += new XboxConsole.ScreenshotAcquiredEventHandler(console_OnScreenshotAcquired);

            if (string.IsNullOrEmpty(console.CurrentProgram))
                Text = console.ConsoleFriendlyName;
            else
                Text = string.Concat(console.ConsoleFriendlyName, " - ", console.CurrentProgram);
        }

        void console_OnScreenshotAcquired(Image Screenshot)
        {
            //Call ourselves on the correct thread if required
            if (InvokeRequired)
            {
                //Ignore errors when we're shutting down:
                try { BeginInvoke(UpdateMyScreenshot, Screenshot); }
                catch (ObjectDisposedException) { }
                return;
            }
            //Update the background image... if we are not shutting down.
            try
            {
                BackgroundImage = Screenshot;
            }
            catch (ObjectDisposedException) { }
        }

         private void ConsoleMonitor_VisibleChanged(object sender, EventArgs e)
         {
             if (Visible)
             {
                 //Reset the background image so we don't show an old screenshot, 
                 //and kick off the timer.
                 BackgroundImage = null;
                 console.StartScreenshotTimer();
             }
             else
             {
                 console.StopScreenshotTimer();
             }
         }

         private void ConsoleMonitor_FormClosing(object sender, FormClosingEventArgs e)
         {
             //If the user is attempting to close the form, minimize instead. 
             //They have to click the Exit menu option to truly close the app.
             if (e.CloseReason == CloseReason.UserClosing)
             {
                 Visible = false;
                 console.StopScreenshotTimer();
                 e.Cancel = true;
             }
         }

         private void ConsoleMonitor_Load(object sender, EventArgs e)
         {
             console.OnConnectionChange += new XboxConsole.XboxConnectionEventHandler(console_OnConnectionChange);
         }

         void console_OnRunningProgramChange(object source, string ProgramName)
         {
             UpdateMyTitle();
         }

         void console_OnConnectionChange(object source, bool Connected)
         {
             UpdateMyTitle();
         }

         private void UpdateTitle()
         {
             try
             {
                 //Handle events coming in through seperate threads.
                 if (InvokeRequired)
                 {
                     //Ignore errors when we're shutting down:
                     try { BeginInvoke(UpdateMyTitle); }
                     catch (ObjectDisposedException) { }
                     return;
                 }

                 //Update our friendly name.
                 if (string.IsNullOrEmpty(console.CurrentProgram))
                     Text = console.ConsoleFriendlyName;
                 else
                     Text = string.Concat(console.ConsoleFriendlyName, " - ", console.CurrentProgram);
             }
             //When shutting down, objects can be disposed underneath us.
             catch (ObjectDisposedException) { }
         }
    }
}

