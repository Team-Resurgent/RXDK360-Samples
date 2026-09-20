#region File Information
//-----------------------------------------------------------------------------
// Log.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace Atg.Samples.xbWatson
{
    /// <summary>
    /// Extension to the RichTextBox control to provide features useful for logging.
    /// </summary>
    public class Log : RichTextBox
    {
        private delegate void VoidDelegate();
    
        private bool scrollToRecentMessages;
    
        public Log()
        {
            // Default properties for a log.
            AutoWordSelection = true;
            DetectUrls = false;
            EnableAutoDragDrop = false;
            Multiline = true;
            ReadOnly = true;
            ScrollBars = RichTextBoxScrollBars.ForcedBoth;
            ShowSelectionMargin = true;
            WordWrap = false;
            
            BackColor = Color.White;
            ForeColor = Color.Black;
        }

        #region Public Methods/Properties

        /// <summary>
        /// Specifies whether the log should scroll to show recent messages.
        /// </summary>
        public bool ScrollToRecentMessages
        {
            get { return scrollToRecentMessages; }
            set { scrollToRecentMessages = value; }
        }

        /// <summary>
        /// Appends text with the specified formatting, including a newline.  (Callable from any thread.)
        /// </summary>
        public void AppendLine(LogFormatting formatting, string text)
        {
            BeginInvoke(new VoidDelegate(delegate
            {
                AppendTextInternal(formatting, text, true);
            }));
        }
        
        /// <summary>
        /// Appends text with the specified formatting.  (Callable from any thread.)
        /// </summary>
        public void AppendText(LogFormatting formatting, string text)
        {
            BeginInvoke(new VoidDelegate(delegate
            {
                AppendTextInternal(formatting, text, false);
            }));
        }

        #endregion

        #region Internal Methods/Properties

        private const int WM_SETREDRAW = 0xb;
        private const int WM_USER = 0x400;
        private const int EM_GETEVENTMASK = (WM_USER + 59);
        private const int EM_SETEVENTMASK = (WM_USER + 69);

        [DllImport("user32", CharSet = CharSet.Auto)]
        private extern static IntPtr SendMessage(IntPtr hWnd, int msg, int wParam, IntPtr lParam);

        /// <summary>
        /// Appends text with the specified formatting.
        /// </summary>
        private void AppendTextInternal(LogFormatting formatting, string text, bool newline)
        {
            // Handle newline.
            if (newline && TextLength > 0)
                text = string.Format("\n{0}", text);
        
            // Format the input text.
            string formatted = formatting.FormatText(text);

            // Update the control.
            IntPtr eventMask = IntPtr.Zero;
            try
            {
                // Stop redrawing or sending events.
                SendMessage(Handle, WM_SETREDRAW, 0, IntPtr.Zero);
                eventMask = SendMessage(Handle, EM_GETEVENTMASK, 0, IntPtr.Zero);

                // Capture original selection.
                int length = SelectionLength;
                int start = SelectionStart;
                int first = GetCharIndexFromPosition(new Point(1, 1));

                // Append the text as RTF.
                Select(TextLength, 0);
                SelectedRtf = formatted;

                if (!scrollToRecentMessages)
                {
                    // Restore original selection.
                    Select(first, 0);
                    ScrollToCaret();

                    Select(start, length);
                }
                else
                {
                    // Scroll to end.
                    Select(TextLength, 0);
                    ScrollToCaret();
                }
            }
            finally
            {
                // Enable events and redrawing.
                SendMessage(Handle, EM_SETEVENTMASK, 0, eventMask);
                SendMessage(Handle, WM_SETREDRAW, 1, IntPtr.Zero);

                // Redraw.
                Refresh();
            }
        }
        
        #endregion
    }
}
