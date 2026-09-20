//--------------------------------------------------------------------------------------
// XedFileTagger.cs
//
// This sample shows how to use the Xed APIs from within c# to add title specific data
// to .xed files. In particular, each depth event can be tagged with a string value that
// can be used for helping with development of gesture detection or joint filtering.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Imaging;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using Nui;
using XStudio;

namespace XedFileTagger
{
    public partial class XedFileTagger : Form
    {
        // Identification string for tags in the title data stream
        private const String m_TagID = "TAG:";

        // We cannot read/write to the same stream, so we need a seperate reader and writer
        private XedFileReader m_XedFileReader;
        private XedFileWriter m_XedFileWriter;

        // For each depth event in the xed file, we can add a string as a tag
        private List<String> m_Tags;

        // Keep track of the currently selected depth event(s)
        private int m_iCurrentDepthEvent;
        private int m_iStartDepthEvent;

        // Is the file dirty
        private bool m_bFileEditedButNotSaved;

        // Display a bitmap with the tagged and selected depth events
        private Bitmap m_TitleDataBitmap;

        // Display the currently selected depth event as a bitmap
        private Bitmap m_DepthMapBitmap;
        private Color[] m_DepthColorTable;

        //--------------------------------------------------------------------------------------
        // Name: XedFileTagger()
        // Desc: Constructor
        //--------------------------------------------------------------------------------------
        public XedFileTagger()
        {
            InitializeComponent();
            m_iCurrentDepthEvent = 0;
            m_iStartDepthEvent = 0;
            m_bFileEditedButNotSaved = false;
            UpdateStatusLabel(null);
            m_DepthMapBitmap = new Bitmap(XedFile.m_uDepthWidth, XedFile.m_uDepthHeight);
            InitializeDepthColorTable();
            m_XedFileReader = new XedFileReader();
            m_XedFileWriter = new XedFileWriter();
        }

        //--------------------------------------------------------------------------------------
        // Name: openToolStripMenuItem_Click()
        // Desc: Menu item: File->Open
        //--------------------------------------------------------------------------------------
        private void openToolStripMenuItem_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog() { Filter = "Xbox Event Data file (*.xed)|*.xed|All files (*.*)|*.*", Multiselect = false };

            // if the current file has been edited but not saved, ask the user if he wants to save it
            if (m_bFileEditedButNotSaved == true)
            {
                string message = string.Format("Would you like to save the current file?");
                if (MessageBox.Show(message, "Save?", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                {
                    StartSaveDialog();
                }
            }

            if (ofd.ShowDialog() == DialogResult.OK)
            {
                // Update the UI
                Cursor = Cursors.WaitCursor;
                UpdateStatusLabel("Loading " + ofd.FileName + " ...");
                UpdateStatusProgress(0);
                toolStripProgressBar.Visible = true;

                // Load the file
                XedFile.ErrorCode errorCode = LoadFile(ofd.FileName);

                // Update the UI
                toolStripProgressBar.Visible = false;
                UpdateUI();
                labelPath.Text = "Path: " + ofd.FileName;
                labelFileNumDepthEvents.Text = "Number of Depth Events: " + m_Tags.Count().ToString();
                Cursor = Cursors.Default;

                // Handle errors from loading
                switch (errorCode)
                {
                    case XedFile.ErrorCode.ERROR_NONE:
                        m_bFileEditedButNotSaved = false;
                        break;

                    default:
                        String message = "Failed to load " + ofd.FileName + "\nError message: " + XedFile.ErrorCodeText[(int)errorCode];
                        MessageBox.Show(message, "Load error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        break;
                }
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: saveToolStripMenuItem_Click()
        // Desc: Menu item: File->Save
        //--------------------------------------------------------------------------------------
        private void saveToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // Save the current file
            Save(m_XedFileReader.m_FileName);
        }

        //--------------------------------------------------------------------------------------
        // Name: saveAsToolStripMenuItem_Click()
        // Desc: Menu item: File->SaveAs
        //--------------------------------------------------------------------------------------
        private void saveAsToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // Choose the file name and then save
            StartSaveDialog();
        }

        //--------------------------------------------------------------------------------------
        // Name: exitToolStripMenuItem_Click()
        // Desc: Menu item: File->Exit
        //--------------------------------------------------------------------------------------
        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            // if the file was edited but not saved, ask if the user wants to save it first
            if (m_bFileEditedButNotSaved == true)
            {
                string message = string.Format("Would you like to save the current file?");
                if (MessageBox.Show(message, "Save?", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes)
                {
                    StartSaveDialog();
                }
            }

            Application.Exit();
        }

        //--------------------------------------------------------------------------------------
        // Name: XedFileTagger_FormClosed()
        // Desc: Window close button was clicked
        //--------------------------------------------------------------------------------------
        private void XedFileTagger_FormClosed(object sender, FormClosedEventArgs e)
        {
            exitToolStripMenuItem_Click(null, null);
        }

        //--------------------------------------------------------------------------------------
        // Name: aboutToolStripMenuItem_Click()
        // Desc: Menu item: Help->About
        //--------------------------------------------------------------------------------------
        private void aboutToolStripMenuItem_Click(object sender, EventArgs e)
        {
            AboutBox aboutBox = new AboutBox();
            aboutBox.ShowDialog();
        }

        //--------------------------------------------------------------------------------------
        // Name: depthBufferFrame_Scroll()
        // Desc: Updates the UI when scroll bar is scrolling through depth events
        //--------------------------------------------------------------------------------------
        private void depthBufferFrame_Scroll(object sender, EventArgs e)
        {
            m_iCurrentDepthEvent = depthBufferFrame.Value;

            // Check if Ctrl key was pressed for multiple frame selection
            if (Control.ModifierKeys != Keys.Control)
            {
                m_iStartDepthEvent = m_iCurrentDepthEvent;
            }

            UpdateUI();
        }

        //--------------------------------------------------------------------------------------
        // Name: buttonAdd_Click()
        // Desc: Adds a tag to the selected depth events
        //--------------------------------------------------------------------------------------
        private void buttonAdd_Click(object sender, EventArgs e)
        {
            if (!String.IsNullOrEmpty(comboBoxTags.Text))
            {
                // Add the tag to the combo box's list items for easy selection later on
                if (!comboBoxTags.Items.Contains(comboBoxTags.Text))
                {
                    comboBoxTags.Items.Add(comboBoxTags.Text);
                }

                if (m_Tags != null)
                {
                    // Set the tag for the range of selected depth events
                    for (int i = m_iStartDepthEvent; i <= m_iCurrentDepthEvent; i++)
                    {
                        m_Tags[i] = comboBoxTags.Text;
                    }
                }

                // Mark the file as dirty
                m_bFileEditedButNotSaved = true;
            }

            UpdateUI();
        }

        //--------------------------------------------------------------------------------------
        // Name: buttonRemove_Click()
        // Desc: Removes a tag from the selected depth events
        //--------------------------------------------------------------------------------------
        private void buttonRemove_Click(object sender, EventArgs e)
        {
            // Reset the tag for the range of selected depth events
            for (int i = m_iStartDepthEvent; i <= m_iCurrentDepthEvent; i++)
            {
                m_Tags[i] = "";
            }

            // Mark the file as dirty
            m_bFileEditedButNotSaved = true;

            UpdateUI();
        }

        //--------------------------------------------------------------------------------------
        // Name: comboBoxTags_KeyPress()
        // Desc: Handle the Enter key press from within the text box of the combo box
        //--------------------------------------------------------------------------------------
        private void comboBoxTags_KeyPress(object sender, KeyPressEventArgs e)
        {
            // if we find the Enter key was pressed, simulate a AddButton click
            if (e.KeyChar == '\r')
            {
                buttonAdd_Click(null, null);
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: LoadFile()
        // Desc: Loads a xed file and setup the tags from the title data
        //--------------------------------------------------------------------------------------
        private XedFile.ErrorCode LoadFile(String fileName)
        {
            if (String.IsNullOrEmpty(fileName))
            {
                return XedFile.ErrorCode.ERROR_FILE_DONT_EXIST;
            }

            int iNumDepthEvents = 0;
            String message;

            // Open the file
            XedFile.ErrorCode errorCode = m_XedFileReader.Open(fileName);

            switch (errorCode)
            {
                case XedFile.ErrorCode.ERROR_NONE:
                    iNumDepthEvents = (int)Xed.XedGetNuiDepthEventCount(m_XedFileReader.m_pContext);

                    if (iNumDepthEvents == 0)
                    {
                        message = "Failed to load " + fileName + "\nThe file has no depth frame events.";
                        MessageBox.Show(message, "Load error", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                        m_XedFileReader.Close();
                        return XedFile.ErrorCode.ERROR_OPENING_FILE;
                    }
                    break;

                default:
                    message = "Failed to load " + fileName + "\nError message: " + XedFile.ErrorCodeText[(int)errorCode];
                    MessageBox.Show(message, "Load error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    m_XedFileReader.Close();
                    return XedFile.ErrorCode.ERROR_OPENING_FILE;
            }

            m_iCurrentDepthEvent = 0;
            m_iStartDepthEvent = 0;
            m_bFileEditedButNotSaved = false;

            m_Tags = new List<String>(iNumDepthEvents);
            for (uint i = 0; i < iNumDepthEvents; i++)
            {
                m_Tags.Add("");
                UpdateStatusProgress((int)(100 * i / iNumDepthEvents));
            }

            depthBufferFrame.Maximum = iNumDepthEvents - 1;
            depthBufferFrame.Value = m_iCurrentDepthEvent;
            m_TitleDataBitmap = new Bitmap(iNumDepthEvents, 1);

            XSTUDIO_TITLE_INDEX_DATA titleDataIndex = new XSTUDIO_TITLE_INDEX_DATA();
            titleDataIndex.IndexData = new UInt32[4];

            // Read in tags
            UInt32 uNumTitleDataEvents = Xed.XedGetTitleDataEventCount(m_XedFileReader.m_pContext);

            // Read all title data events
            for (UInt32 uEventIndex = 0; uEventIndex < uNumTitleDataEvents; uEventIndex++)
            {
                UpdateStatusProgress((int)(100 * uEventIndex / uNumTitleDataEvents));

                // Read the title data
                IntPtr pTitleData = IntPtr.Zero;
                if (!m_XedFileReader.GetTitleData(uEventIndex, ref pTitleData, ref titleDataIndex))
                {
                    continue;
                }

                // The title data is saved as an ansi string, but c# strings are unicode
                String titleData = Marshal.PtrToStringAnsi(pTitleData);

                // If the title data starts with "TAG:", then it's a tag
                if (titleData.StartsWith(m_TagID))
                {
                    // We could have used the XedGetNuiDepthEventIndexFromMicroseconds() API, but it's much
                    // simpler to save the corresponding depth event index in the titleDataIndex.
                    // This does however mean that you cannot concatenate or trim the files after they
                    // have been tagged.
                    UInt32 uDepthEventIndex = titleDataIndex.IndexData[0];

                    // If the title data read is a tag, then add it to our tag array
                    if (uDepthEventIndex < iNumDepthEvents)
                    {
                        // Get the tag string and add it to the tag array
                        String tag = titleData.Substring(m_TagID.Length);
                        m_Tags[(int)uDepthEventIndex] = tag;

                        // Also add the tag string to the combo box
                        if (!comboBoxTags.Items.Contains(tag))
                        {
                            comboBoxTags.Items.Add(tag);
                        }
                    }
                }
            }

            return XedFile.ErrorCode.ERROR_NONE;
        }

        //--------------------------------------------------------------------------------------
        // Name: StartSaveDialog()
        // Desc: Opens a SaveFile dialog box
        //--------------------------------------------------------------------------------------
        public void StartSaveDialog()
        {
            if (m_XedFileReader != null &&
                m_XedFileReader.IsOpen())
            {
                SaveFileDialog sfd = new SaveFileDialog() { Filter = "Xbox Event Data file (*.xed)|*.xed|All files (*.*)|*.*" };

                if (sfd.ShowDialog() == DialogResult.OK)
                {
                    Save(sfd.FileName);
                }
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: Save()
        // Desc: Save the xed file
        //--------------------------------------------------------------------------------------
        void Save(string fileName)
        {
            XedFile.ErrorCode errorCode;

            // Update UI
            Cursor = Cursors.WaitCursor;
            UpdateStatusLabel("Saving " + fileName + " ...");
            UpdateStatusProgress(0);
            toolStripProgressBar.Visible = true;

            // Write the data events to the file
            errorCode = WriteEventsToFile(fileName);

            // Update UI
            toolStripProgressBar.Visible = false;
            UpdateUI();
            Cursor = Cursors.Default;

            // Handle error codes
            switch (errorCode)
            {
                case XedFile.ErrorCode.ERROR_NONE:
                    m_bFileEditedButNotSaved = false;
                    labelPath.Text = "Path: " + fileName;
                    labelFileNumDepthEvents.Text = "Number of Depth Events: " + m_Tags.Count().ToString();
                    m_XedFileReader.Open(fileName);
                    break;

                default:
                    String message = "Failed to save " + fileName + "\nError message: " + XedFile.ErrorCodeText[(int)errorCode];
                    MessageBox.Show(message, "Save error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    break;
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: WriteEventsToFile()
        // Desc: Write data events to the xed file in chronological order using the timestamps
        //--------------------------------------------------------------------------------------
        private XedFile.ErrorCode WriteEventsToFile(String fileName)
        {
            // Open the file for writing
            XedFile.ErrorCode errorCode = m_XedFileWriter.Open(fileName, m_XedFileReader.m_pContext);
            if (errorCode != XedFile.ErrorCode.ERROR_NONE)
            {
                return errorCode;
            }

            UInt32 hr = 0;
            UInt32 uFrameNumber;
            UInt64 uTimeStamp;
            UInt32 uRequiredSize;
            NUI_SKELETON_FRAME skeletonFrame = new NUI_SKELETON_FRAME(0);
            NUI_IDENTITY_MESSAGE identityMessage = new NUI_IDENTITY_MESSAGE();
            XSTUDIO_TITLE_INDEX_DATA titleIndexData = new XSTUDIO_TITLE_INDEX_DATA();
            titleIndexData.IndexData = new UInt32[4];

            UInt32 uNumColorEvents = Xed.XedGetNuiColorEventCount(m_XedFileReader.m_pContext);
            UInt32 uNumDepthEvents = Xed.XedGetNuiDepthEventCount(m_XedFileReader.m_pContext);
            UInt32 uNumSkeletonEvents = Xed.XedGetNuiSkeletonEventCount(m_XedFileReader.m_pContext);
            UInt32 uNumIdentityEvents = Xed.XedGetEventCount(m_XedFileReader.m_pContext, XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_IDENTITY);
            UInt32 uNumTitleDataEvents = Xed.XedGetTitleDataEventCount(m_XedFileReader.m_pContext);
            UInt32 uNumTagEvents = uNumDepthEvents;

            UInt64 uTotalEvents = uNumColorEvents + uNumDepthEvents + uNumSkeletonEvents + uNumIdentityEvents + uNumTitleDataEvents + uNumTagEvents;
            UInt64 uCurrentEventCnt = 0;

            // Read all timestamps into arrays
            List<UInt64> uColorTimeStamps = new List<UInt64>((int)uNumColorEvents);
            List<UInt64> uDepthTimeStamps = new List<UInt64>((int)uNumDepthEvents);
            List<UInt64> uSkeletonTimeStamps = new List<UInt64>((int)uNumSkeletonEvents);
            List<UInt64> uIdentityTimeStamps = new List<UInt64>((int)uNumIdentityEvents);
            List<UInt64> uTitleDataTimeStamps = new List<UInt64>((int)uNumTitleDataEvents);
            List<UInt64> uTagTimeStamps = new List<UInt64>((int)uNumTagEvents);

            for (UInt32 i = 0; i < uNumColorEvents; i++)
            {
                hr = Xed.XedReadNuiColorFrame(m_XedFileReader.m_pContext, i, out uFrameNumber, out uTimeStamp, null, 0, false);
                uColorTimeStamps.Add(uTimeStamp);
            }

            for (UInt32 i = 0; i < uNumDepthEvents; i++)
            {
                hr = Xed.XedReadNuiDepthFrame(m_XedFileReader.m_pContext, i, out uFrameNumber, out uTimeStamp, null, 0, false);
                uDepthTimeStamps.Add(uTimeStamp);
            }

            for (UInt32 i = 0; i < uNumSkeletonEvents; i++)
            {
                hr = Xed.XedReadNuiSkeletonFrame(m_XedFileReader.m_pContext, i, out uFrameNumber, out uTimeStamp, out skeletonFrame, Xed.m_uSkeletonFrameSize);
                uSkeletonTimeStamps.Add(uTimeStamp);
            }

            for (UInt32 i = 0; i < uNumIdentityEvents; i++)
            {
                hr = Xed.XedReadNuiIdentityMessage(m_XedFileReader.m_pContext, i, out uTimeStamp, out identityMessage, 0);
                uIdentityTimeStamps.Add(uTimeStamp);
            }

            for (UInt32 i = 0; i < uNumTitleDataEvents; i++)
            {
                hr = Xed.XedReadTitleData(m_XedFileReader.m_pContext, i, out uTimeStamp, out titleIndexData, IntPtr.Zero, 0, out uRequiredSize);
                uTitleDataTimeStamps.Add(uTimeStamp);
            }

            for (UInt32 i = 0; i < uNumTagEvents; i++)
            {
                // events cannot have duplicate timestamps, so generate timestamps offset from the depth timestamp
                uTagTimeStamps.Add(uDepthTimeStamps[(int)i] + 1);
            }

            // Add an invalid timestamp at the end of each array
            uColorTimeStamps.Add(XStudio.Constants.XED_TIMESTAMP_INVALID);
            uDepthTimeStamps.Add(XStudio.Constants.XED_TIMESTAMP_INVALID);
            uSkeletonTimeStamps.Add(XStudio.Constants.XED_TIMESTAMP_INVALID);
            uIdentityTimeStamps.Add(XStudio.Constants.XED_TIMESTAMP_INVALID);
            uTitleDataTimeStamps.Add(XStudio.Constants.XED_TIMESTAMP_INVALID);
            uTagTimeStamps.Add(XStudio.Constants.XED_TIMESTAMP_INVALID);

            UInt32 uColorEventIndex = 0;
            UInt32 uDepthEventIndex = 0;
            UInt32 uSkeletonEventIndex = 0;
            UInt32 uIdentityEventIndex = 0;
            UInt32 uTitleDataEventIndex = 0;
            UInt32 uTagEventIndex = 0;
                       
            do
            {
                // find the earliest time stamp
                uTimeStamp = Math.Min(uColorTimeStamps[(int)uColorEventIndex], uDepthTimeStamps[(int)uDepthEventIndex]);
                uTimeStamp = Math.Min(uSkeletonTimeStamps[(int)uSkeletonEventIndex], uTimeStamp);
                uTimeStamp = Math.Min(uIdentityTimeStamps[(int)uIdentityEventIndex], uTimeStamp);
                uTimeStamp = Math.Min(uTitleDataTimeStamps[(int)uTitleDataEventIndex], uTimeStamp);
                uTimeStamp = Math.Min(uTagTimeStamps[(int)uTagEventIndex], uTimeStamp);

                // Now find from which stream this time stamp came
                if (uTimeStamp == uColorTimeStamps[(int)uColorEventIndex])
                {
                    // Copy the color from the input to the output context
                    hr = Xed.XedCopyNuiColorFrame(m_XedFileReader.m_pContext, uColorEventIndex, m_XedFileWriter.m_pContext, XStudio.Constants.XED_FRAMENUMBER_INVALID, XStudio.Constants.XED_TIMESTAMP_INVALID);
                    uColorEventIndex++;
                }
                else if (uTimeStamp == uDepthTimeStamps[(int)uDepthEventIndex])
                {
                    // Copy the depth from the input to the output context
                    hr = Xed.XedCopyNuiDepthFrame(m_XedFileReader.m_pContext, uDepthEventIndex, m_XedFileWriter.m_pContext, XStudio.Constants.XED_FRAMENUMBER_INVALID, XStudio.Constants.XED_TIMESTAMP_INVALID);
                    uDepthEventIndex++;
                }
                else if (uTimeStamp == uSkeletonTimeStamps[(int)uSkeletonEventIndex])
                {
                    // Copy the skeleton from the input to the output context
                    hr = Xed.XedCopyNuiSkeletonFrame(m_XedFileReader.m_pContext, uSkeletonEventIndex, m_XedFileWriter.m_pContext, XStudio.Constants.XED_FRAMENUMBER_INVALID, XStudio.Constants.XED_TIMESTAMP_INVALID);
                    uSkeletonEventIndex++;
                }
                else if (uTimeStamp == uIdentityTimeStamps[(int)uIdentityEventIndex])
                {
                    // Copy the identity data from the input to the output context
                    hr = Xed.XedCopyNuiIdentityMessage(m_XedFileReader.m_pContext, uIdentityEventIndex, m_XedFileWriter.m_pContext, 0, XStudio.Constants.XED_TIMESTAMP_INVALID);
                    uIdentityEventIndex++;
                }
                else if (uTimeStamp == uTitleDataTimeStamps[(int)uTitleDataEventIndex])
                {
                    // Copy all title data that is not tagging data. Tagging data is a string starting with "TAG:". This
                    // means we first have to read the data from the input stream and then decide if we need to copy it or later
                    // write it
                    IntPtr pTitleData = IntPtr.Zero;
                    if (m_XedFileReader.GetTitleData(uTitleDataEventIndex, ref pTitleData, ref titleIndexData))
                    {
                        // Only copy the data if it's not a tag, since we want to preserve other title data. The title data will
                        // be in the form of an ansi string, but all c# strings are unicode
                        String titleData = Marshal.PtrToStringAnsi(pTitleData);
                        if (!titleData.StartsWith(m_TagID))
                        {
                            // Copy the title data from the input to the output context
                            hr = Xed.XedCopyTitleData(m_XedFileReader.m_pContext, uTitleDataEventIndex, m_XedFileWriter.m_pContext, XStudio.Constants.XED_TIMESTAMP_INVALID);
                        }
                    }
                    uTitleDataEventIndex++;
                }
                else
                {
                    // Only write out the valid tags
                    if (!String.IsNullOrEmpty(m_Tags[(int)uTagEventIndex]))
                    {
                        String tag = m_TagID + m_Tags[(int)uTagEventIndex];
                        UInt32 uTitleDataSize = (UInt32)tag.Length + 1;
                        IntPtr pTitleData = IntPtr.Zero;

                        // Copies the unicode string to a ansi string and return the pointer
                        pTitleData = Marshal.StringToHGlobalAnsi(tag);

                        // We cannot 100% rely on syncing up the title data and depth data using microseconds, since each frame
                        // can have multiple events at the same microseconds. We therefore write the depth event index into this
                        // structure so that we can easily sychronize the data when we load it in
                        titleIndexData.IndexData[0] = (UInt32)uTagEventIndex;
                        hr = Xed.XedWriteTitleData(m_XedFileWriter.m_pContext, uTagTimeStamps[(int)uTagEventIndex], ref titleIndexData, pTitleData, uTitleDataSize);

                        // free the unmanaged copy
                        Marshal.FreeHGlobal(pTitleData);
                    }
                    uTagEventIndex++;
                }
                
                // This should really be on a seperate thread, but we keep the sample simple
                UpdateStatusProgress((int)(100 * uCurrentEventCnt / uTotalEvents));
                uCurrentEventCnt++;
               
            } while (uCurrentEventCnt < uTotalEvents);


            Refresh();

            // Close the writer
            return m_XedFileWriter.Close();
        }

        //--------------------------------------------------------------------------------------
        // Name: UpdateUI()
        // Desc: Update the UI from current state
        //--------------------------------------------------------------------------------------
        private void UpdateUI()
        {
            UpdateDepthFrame();
            UpdateStatusLabel(null);
            UpdateTitleDataImage();
            UpdateRemoveButton();
            Refresh();
        }

        //--------------------------------------------------------------------------------------
        // Name: UpdateDepthFrame()
        // Desc: Update the image of the depth frame
        //--------------------------------------------------------------------------------------
        private void UpdateDepthFrame()
        {
            ushort[] depthData = GetDepthBuffer(m_iCurrentDepthEvent);

            if (depthData != null)
            {
                // To colorize the depth values, normalize the depth value against a maximum depth
                // value to be colorized and lookup into the color table
                float fMaxDepth = 3500.0f; // use 3.5 meters as max depth to colorize
                uint uNormalize = (uint)Math.Ceiling(fMaxDepth / 511.0f);

                lock (this)
                {
                    for (int y = 0; y < m_DepthMapBitmap.Height; y++)
                    {
                        for (int x = 0; x < m_DepthMapBitmap.Width; x++)
                        {
                            uint uIndex = Math.Min(GetDepthValue(depthData, x, y) / uNormalize, 511);
                            m_DepthMapBitmap.SetPixel(x, y, m_DepthColorTable[uIndex]);
                        }
                    }
                }

                // Draw the bitmap
                depthBufferImage.Image = m_DepthMapBitmap;
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: UpdateStatusLabel()
        // Desc: Update the status label
        //--------------------------------------------------------------------------------------
        private void UpdateStatusLabel(String text)
        {
            if (text != null)
            {
                toolStripStatusLabel.Text = text;
            }
            else
            {
                string tag = "";
                if (m_Tags != null)
                {
                    tag = m_Tags[m_iCurrentDepthEvent];
                }

                if (m_iStartDepthEvent == m_iCurrentDepthEvent)
                {
                    toolStripStatusLabel.Text = string.Format("Event: {0}  Tag: {1}", m_iCurrentDepthEvent, tag);
                }
                else
                {
                    toolStripStatusLabel.Text = string.Format("Events: {0}-{1}  Tag: {2}", m_iStartDepthEvent, m_iCurrentDepthEvent, tag);
                }
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: UpdateTitleDataImage()
        // Desc: Update title data image which shows the graph where tags were added
        //--------------------------------------------------------------------------------------
        private void UpdateTitleDataImage()
        {
            if (m_TitleDataBitmap != null)
            {
                int iNumDepthEvents = m_Tags.Count();

                for (int i = 0; i < iNumDepthEvents; i++)
                {
                    Color color = Color.White;

                    // dark gray for selected
                    if (i >= m_iStartDepthEvent &&
                        i <= m_iCurrentDepthEvent)
                    {
                        color = Color.DarkGray;
                    }

                    // Green for added
                    if (m_Tags[i] != "")
                    {
                        color = Color.Green;
                    }

                    m_TitleDataBitmap.SetPixel(i, 0, color);
                }

                // Draw the bitmap
                titleDataImage.Image = m_TitleDataBitmap;
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: UpdateRemoveButton()
        // Desc: Enable or disable the remove button based on if a tag can be removed
        //--------------------------------------------------------------------------------------
        private void UpdateRemoveButton()
        {
            bool bFoundTagData = false;

            if (m_Tags != null)
            {
                for (int i = m_iStartDepthEvent; i <= m_iCurrentDepthEvent; i++)
                {
                    if (m_Tags[i] != "")
                    {
                        bFoundTagData = true;
                        break;
                    }
                }
            }

            buttonRemove.Enabled = bFoundTagData;
        }

        //--------------------------------------------------------------------------------------
        // Name: UpdateStatusProgress()
        // Desc: Update the progress bars when loading/saving
        //--------------------------------------------------------------------------------------
        private void UpdateStatusProgress(int iProgress)
        {
            toolStripProgressBar.Invalidate();
            toolStripProgressBar.Value = iProgress;
            statusStrip.Refresh();
        }

        //--------------------------------------------------------------------------------------
        // Name: GetDepthValue()
        // Desc: Get the depth value in the depth buffer
        //--------------------------------------------------------------------------------------
        private static ushort GetDepthValue(ushort[] data, int x, int y)
        {
            // make sure we're not reading out of bounds
            x = Math.Max(0, Math.Min(XedFile.m_uDepthWidth - 1, x));
            y = Math.Max(0, Math.Min(XedFile.m_uDepthHeight - 1, y));
            int index = y * XedFile.m_uDepthWidth + x;

            // lower 3 bits are used for segmentation
            return (ushort)(data[index] >> 3);
        }
        
        //--------------------------------------------------------------------------------------
        // Name: GetDepthBuffer()
        // Desc: Get the depth buffer from the depth event
        //--------------------------------------------------------------------------------------
        private ushort[] GetDepthBuffer(int iDepthEvent)
        {
            lock (this)
            {
                return m_XedFileReader.GetDepthBuffer((uint)iDepthEvent);
            }
        }

        //--------------------------------------------------------------------------------------
        // Name: InitializeDepthColorTable()
        // Desc: Initialize the color table for displaying the depth data
        //--------------------------------------------------------------------------------------
        private void InitializeDepthColorTable()
        {
            m_DepthColorTable = new Color[512];

            // Build depth map visualization color table. Rainbow linear gradient mirrored at
            // center point with gradual dimming starting at center point to start of table.

            const float fPi = 3.141592654f;
            const int iHalfTableSize = 512 / 2;
            float fGutter = 0.2f;
            int iTableIndex = iHalfTableSize;
            float fStep = (1.0f - (fGutter * 2.0f)) / iHalfTableSize;

            for (float t = fGutter; t < (1.0f - fGutter); t += fStep)
            {
                float[] fColor = new float[3];
                const float fBand = 0.7f;
                const float fCurveExp = 2.0f;
                const float fBandGap = 1.0f - fBand;

                for (int i = 0; i < 3; i++)
                {
                    float s = (t - fBandGap * 0.5f * i) / fBand;
                    if ((s >= 0.0f) && (s <= 1.0f))
                    {
                        fColor[i] = (float)Math.Pow(Math.Sin(s * fPi * 2.0f - fPi * 0.5f) * 0.5f + 0.5f, fCurveExp);
                    }
                }

                m_DepthColorTable[iTableIndex++] = Color.FromArgb((byte)(fColor[0] * 255.0f),
                                                                     (byte)(fColor[1] * 255.0f),
                                                                     (byte)(fColor[2] * 255.0f));
            }

            for (int i = 0; i < iHalfTableSize; i++)
            {
                Color s = m_DepthColorTable[512 - 1 - i];

                float fDim = (float)i / (float)iHalfTableSize;

                m_DepthColorTable[i] = Color.FromArgb((byte)((float)s.R * (0.25f + (fDim * 0.75f))),
                                                         (byte)((float)s.G * (0.25f + (fDim * 0.75f))),
                                                         (byte)((float)s.B * (0.25f + (fDim * 0.75f))));
            }
        }

        private void depthBufferFrame_KeyPress(object sender, KeyPressEventArgs e)
        {
            char test = e.KeyChar;
            switch (e.KeyChar)
            {
                // Enter - add the currently selected tag
                case '\r':
                    buttonAdd_Click(sender, e);
                    e.Handled = true;
                    break;
            }
        }

        private void batchConvertToVGBToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Atg.Samples.XedFileTagger.BatchConvertForm batchConvertForm = new Atg.Samples.XedFileTagger.BatchConvertForm();
            batchConvertForm.Show();
        }
    }
}
