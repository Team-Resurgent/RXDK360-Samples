//--------------------------------------------------------------------------------------
// Xed.cs
//
// Defines functions and structures used for Xed APIs and a reader/writer class that
// encapsulates some of these APIs
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Collections.Generic;
using Nui;
using XStudio;


//--------------------------------------------------------------------------------------
// Structures and constants for XStudio and Xed APIs
//--------------------------------------------------------------------------------------

namespace XStudio
{
    public static class Constants
    {
        public static readonly UInt32 XED_EVENTINDEX_INVALID = unchecked((UInt32)(-1));
        public static readonly UInt32 XED_FRAMENUMBER_INVALID = unchecked((UInt32)(-1));
        public static readonly UInt64 XED_TIMESTAMP_INVALID = unchecked((UInt64)(-1));
    }

    [StructLayout(LayoutKind.Explicit, Size = 24)]
    public struct XSTUDIO_TITLE_INDEX_DATA
    {
        [FieldOffset(0)]
        public UInt64 Reserved;                                     // 8 Bytes

        [FieldOffset(8)]
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)]
        public UInt32[] IndexData;                                  // 16 Bytes
    }

    public enum XSTUDIO_STREAM_ID : uint
    {
        XSTUDIO_STREAM_ID_NUICAM_BASE = 0,
        XSTUDIO_STREAM_ID_NUICAM_DEPTH = XSTUDIO_STREAM_ID_NUICAM_BASE + 0,            //  0
        XSTUDIO_STREAM_ID_NUICAM_COLOR = XSTUDIO_STREAM_ID_NUICAM_BASE + 1,            //  1

        XSTUDIO_STREAM_ID_NUIAPI_BASE = 4,
        XSTUDIO_STREAM_ID_NUIAPI_SKELETON = XSTUDIO_STREAM_ID_NUIAPI_BASE + 0,	       //  4
        XSTUDIO_STREAM_ID_NUIAPI_IDENTITY = XSTUDIO_STREAM_ID_NUIAPI_BASE + 2,         //  6
        XSTUDIO_STREAM_ID_NUIAPI_PLAYER_INDEX = XSTUDIO_STREAM_ID_NUIAPI_BASE + 3,     //  7

        XSTUDIO_STREAM_ID_TITLE_BASE = 16,
        XSTUDIO_STREAM_ID_TITLE_DATA = XSTUDIO_STREAM_ID_TITLE_BASE + 0,	           // 16

        XSTUDIO_STREAM_ID_MISC_PRIVATE = 31,                                           // 31

        XSTUDIO_STREAM_ID_COUNT = 32,

        XSTUDIO_STREAM_ID_INVALID = 0xFFFFFFFF
    };

}


//--------------------------------------------------------------------------------------
// Import all Xed APIs from native dll
//--------------------------------------------------------------------------------------

public class Xed
{
    // We need to dynamically set the dll directory before using APIs from XedFile.dll
    [DllImport("kernel32.dll", SetLastError = true)]
    static extern Int32 SetDllDirectory(string lpPathName);

    // Import native APIs from XedFile.dll
    private const string DLL_NAME = "XedFile.dll";

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedCloseFile(ref IntPtr ppContext);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedCopyNuiColorFrame(IntPtr pFromContext,
                                                     UInt32 eventIndex,
                                                     IntPtr pToContext,
                                                     UInt32 frameNumber,
                                                     UInt64 frameMicroseconds);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedCopyNuiDepthFrame(IntPtr pFromContext,
                                                     UInt32 eventIndex,
                                                     IntPtr pToContext,
                                                     UInt32 frameNumber,
                                                     UInt64 frameMicroseconds);


    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedCopyNuiIdentityMessage(IntPtr pSrcContext,
                                                          UInt32 srcEventIndex,
                                                          IntPtr pDstContext,
                                                          UInt32 dwDstTrackinID,
                                                          UInt64 dstEventMicroseconds);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedCopyNuiSkeletonFrame(IntPtr pFromContext,
                                                        UInt32 eventIndex,
                                                        IntPtr pToContext,
                                                        UInt32 frameNumber,
                                                        UInt64 frameMicroseconds);


    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedCopyTitleData(IntPtr pFromContext,
                                                 UInt32 eventIndex,
                                                 IntPtr pToContext,
                                                 UInt64 eventMicroseconds);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedCreateFile([MarshalAs(UnmanagedType.LPStr)]String sFilePath,
                                               IntPtr pTemplate,
                                               UInt32 flags,
                                               ref IntPtr ppContext);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetEventCount(IntPtr pContext,
                                                 XSTUDIO_STREAM_ID streamId);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiColorEventCount(IntPtr pContext);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiColorEventIndexFromFrameNumber(IntPtr pContext,
                                                                        UInt32 frameNumber);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiColorEventIndexFromMicroseconds(IntPtr pContext,
                                                                         UInt64 frameMicroseconds);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiDepthEventCount(IntPtr pContext);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiDepthEventIndexFromFrameNumber(IntPtr pContext,
                                                                        UInt32 frameNumber);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiDepthEventIndexFromMicroseconds(IntPtr pContext,
                                                                         UInt64 frameMicroseconds);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiSkeletonEventCount(IntPtr pContext);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiSkeletonEventIndexFromFrameNumber(IntPtr pContext,
                                                                           UInt32 frameNumber);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetNuiSkeletonEventIndexFromMicroseconds(IntPtr pContext,
                                                                            UInt64 frameMicroseconds);
  
    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetTitleDataEventCount(IntPtr pContext);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedGetTitleDataEventIndexFromMicroseconds(IntPtr pContext,
                                                                          UInt64 frameMicroseconds);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedOpenFile([MarshalAs(UnmanagedType.LPStr)]String sFilePath,
                                             UInt32 flags,
                                             out IntPtr ppContext);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedReadNuiColorFrame(IntPtr pContext,
                                                     UInt32 eventIndex,
                                                     out UInt32 pFrameNumber,
                                                     out UInt64 pFrameMicroseconds,
                                                     UInt32[] pBuffer,
                                                     UInt32 bufferSize,
                                                     Boolean registerWithDepth);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedReadNuiDepthFrame(IntPtr pContext,
                                                     UInt32 eventIndex,
                                                     out UInt32 pFrameNumber,
                                                     out UInt64 pFrameMicroseconds,
                                                     UInt16[] pBuffer,
                                                     UInt32 bufferSize,
                                                     Boolean registerWithColor);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedReadNuiIdentityMessage(IntPtr pContext,
                                                          UInt32 eventIndex,
                                                          out UInt64 pFrameMicroseconds,
                                                          out NUI_IDENTITY_MESSAGE pIdentityMessage,
                                                          UInt32 identityMessageSize);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedReadNuiSkeletonFrame(IntPtr pContext,
                                                        UInt32 eventIndex,
                                                        out UInt32 pFrameNumber,
                                                        out UInt64 pFrameMicroseconds,
                                                        out NUI_SKELETON_FRAME pSkeletonFrame,
                                                        UInt32 skeletonFrameSize);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedReadTitleData(IntPtr pContext,
                                                 UInt32 eventIndex,
                                                 out UInt64 pEventMicroseconds,
                                                 out XSTUDIO_TITLE_INDEX_DATA pTitleIndexData,
                                                 IntPtr pTitleDataBuffer,
                                                 UInt32 titleDataBufferSize,
                                                 out UInt32 pRequiredTitleDataBufferSize);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedSetFileNuiColorQuality(IntPtr pContext,
                                                          UInt32 newColorQuality,
                                                          out UInt32 pPreviousColorQuality);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedSetNuiColorBufferType(IntPtr pContext,
                                                         NUI_IMAGE_TYPE newImageType,
                                                         out NUI_IMAGE_TYPE pPreviousImageType);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedSetNuiDepthImageType(IntPtr pContext,
                                                        NUI_IMAGE_TYPE newImageType,
                                                        out NUI_IMAGE_TYPE pPreviousImageType);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedWriteNuiColorFrame(IntPtr pContext,
                                                      UInt32 frameNumber,
                                                      UInt64 frameMicroseconds,
                                                      UInt32[] pBuffer,
                                                      UInt32 bufferSize);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedWriteNuiDepthFrame(IntPtr pContext,
                                                      UInt32 frameNumber,
                                                      UInt64 frameMicroseconds,
                                                      UInt16[] pBuffer,
                                                      UInt32 bufferSize);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedWriteNuiIdentityMessage(IntPtr pContext,
                                                           UInt64 frameMicroseconds,
                                                           ref NUI_IDENTITY_MESSAGE pIdentityMessage,
                                                           UInt32 identityMessageSize);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedWriteNuiSkeletonFrame(IntPtr pContext,
                                                         UInt32 frameNumber,
                                                         UInt64 frameMicroseconds,
                                                         ref NUI_SKELETON_FRAME pSkeletonFrame,
                                                         UInt32 skeletonFrameSize);

    [DllImport(DLL_NAME, SetLastError = true)]
    public extern static UInt32 XedWriteTitleData(IntPtr pContext,
                                                  UInt64 eventMicroseconds,
                                                  ref XSTUDIO_TITLE_INDEX_DATA pTitleIndexData,
                                                  IntPtr pTitleDataBuffer,
                                                  UInt32 titleDataBufferSize);

    static public uint m_uSkeletonFrameSize;


    //--------------------------------------------------------------------------------------
    // Name: Xed()
    // Desc: Constructor
    //--------------------------------------------------------------------------------------

    public Xed()
    {
        // Determine structure size at runtime
        NUI_SKELETON_FRAME SkeletonFrame = new NUI_SKELETON_FRAME(0);
        m_uSkeletonFrameSize = (uint)Marshal.SizeOf(SkeletonFrame);

        // We need to load the XedFile.dll from the XEDK path
        string path = Environment.GetEnvironmentVariable("XEDK") + "\\bin\\win32";
        SetDllDirectory(path);
    }
}


//--------------------------------------------------------------------------------------
// Defines a class for opening and closing Xed files with error codes
//--------------------------------------------------------------------------------------

public class XedFile : Xed
{
    public enum ErrorCode
    {
        ERROR_NONE,
        ERROR_OPENING_FILE,
        ERROR_FILE_DONT_EXIST,
        ERROR_WRITING_DEPTH,
        ERROR_WRITING_COLOR,
        ERROR_WRITING_SKELETON,
        ERROR_WRITING_TITLEDATA,
        ERROR_CREATING_TEMP_FILE,
        ERROR_DELETING_TEMP_FILE,
        ERROR_COPYING_FILE,
        ERROR_COPYING_TEMP_FILE,
        ERROR_TEMP_FILE_DONT_EXIST,
        ERROR_COPYING_TEMP_FILE_READ_ONLY,
    };

    static public String[] ErrorCodeText =
    {
        "Operation succeeded.",
        "XedOpenFile() failed.\nTry to open the file with Xbox Studio to convert it to the latest format.",
        "File does not exist.",
        "XedCopyNuiDepthFrame() failed.",
        "XedCopyNuiCopyFrame() failed.",
        "XedCopyNuiSkeletonFrame() failed.",
        "XedWriteTitleData() failed.",
        "XedCreateFile() failed trying to create " + '%' + "TEMP" + '%' + "\\XedFileWriter.tmp",
        "Failed to delete " + '%' + "TEMP" + '%' + "\\XedFileWriter.tmp\nMake sure another instance of XedFileTagger is not running.",
        "Failed to copy file to " + '%' + "TEMP" + '%' + "\\XedFileReader.tmp\nMake sure another instance of XedFileTagger is not running.",
        "Failed to copy " + '%' + "TEMP" + '%' + "\\XedFileWriter.tmp to file. Data can be recovered by manually copying the temp file.",
        "Data was saved in " + '%' + "TEMP" + '%' + "\\XedFileWriter.tmp, but it does not exists anymore.",
        "Failed to copy " + '%' + "TEMP" + '%' + "\\XedFileWriter.tmp to file because original data file is marked as ReadOnly. Data can be recovered by manually copying the temp file.",
    };

    public IntPtr m_pContext = IntPtr.Zero;
    public String m_FileName = null;

    public const int m_uDepthWidth  = 320;
    public const int m_uDepthHeight = 240;


    //--------------------------------------------------------------------------------------
    // Name: XedFile()
    // Desc: Constructor
    //--------------------------------------------------------------------------------------

    public XedFile()
    {
        Close();
        Reset();
    }


    //--------------------------------------------------------------------------------------
    // Name: ~XedFile()
    // Desc: Destructor
    //--------------------------------------------------------------------------------------

    ~XedFile()
    {
       Close();
       Reset();
    }


    //--------------------------------------------------------------------------------------
    // Name: IsOpen()
    // Desc: Checks to see if the file is open
    //--------------------------------------------------------------------------------------

    public bool IsOpen()
    {
        return m_FileName != null &&
               m_pContext != IntPtr.Zero;
    }


    //--------------------------------------------------------------------------------------
    // Name: Reset()
    // Desc: Resets data
    //--------------------------------------------------------------------------------------
    
    public void Reset()
    {
        m_FileName = null;
    }


    //--------------------------------------------------------------------------------------
    // Name: Close()
    // Desc: Closes the file
    //--------------------------------------------------------------------------------------

    public virtual ErrorCode Close()
    {
        // Close the file
        if (m_pContext != IntPtr.Zero)
        {
            UInt32 hr = XedCloseFile(ref m_pContext);
            m_pContext = IntPtr.Zero;
        }

        return ErrorCode.ERROR_NONE;
    }
}


//--------------------------------------------------------------------------------------
// Defines a class that opens a Xed file for reading
//--------------------------------------------------------------------------------------

public class XedFileReader : XedFile
{
    //----------------------------------------------------------------------------------
    // Name: Open()
    // Desc: Opens a xed file for readering
    //----------------------------------------------------------------------------------

    public ErrorCode Open(String fileName)
    {
        if (fileName == null)
        {
            return ErrorCode.ERROR_FILE_DONT_EXIST;
        }

        // Reset all data and close previous files
        Close();
        Reset();

        // NOTE: The Xed API's cannot read/write to the same file so we always
        // do the following:
        //
        //  1. Remove readonly attrib from %TEMP%\\XedFileReader.tmp 
        //  2. Copy fileName %TEMP%\\XedFileReader.tmp
        //  3. XedFileOpen( %TEMP%\\XedFileReader.tmp )

        String tempPath = Environment.GetEnvironmentVariable("TEMP");
        String tempFileName = tempPath + "\\XedFileReader.tmp";

        // Copy the fileName to the temp file
        FileInfo file = new FileInfo(fileName);
        if (file.Exists)
        {
            try
            {
                FileAttributes fileAttributes;

                // Get temp file attributes and remove ReadOnly
                try
                {
                    fileAttributes = File.GetAttributes(tempFileName);
                    fileAttributes = fileAttributes & ~FileAttributes.ReadOnly;
                    File.SetAttributes(tempFileName, fileAttributes);
                }
                catch (System.Exception)
                {
                    // Do nothing if we get an exception getting the file attributes, this is expected
                    // if the temp file doesn't exist already.
                }

                // Now copy the file to temp
                file.CopyTo(tempFileName, true);

                // The copied file could again be ReadOnly, so make sure tmp file is always not ReadOnly
                fileAttributes = File.GetAttributes(tempFileName);
                fileAttributes = fileAttributes & ~FileAttributes.ReadOnly;
                File.SetAttributes(tempFileName, fileAttributes);
            }
            catch
            {
                return ErrorCode.ERROR_COPYING_FILE;
            }
        }
        else
        {
            return ErrorCode.ERROR_FILE_DONT_EXIST;
        }

        // Open the file temp file which now has the content of the original file
        // This is so that we can do other file operations on the actual file while
        // still editing the content of the original file, but opened from the temp file
        UInt32 hr = XedOpenFile(tempFileName, 0, out m_pContext);
        if (hr != 0)
        {
            return ErrorCode.ERROR_OPENING_FILE;
        }

        m_FileName = fileName;

        return ErrorCode.ERROR_NONE;
    }


    //----------------------------------------------------------------------------------
    // Name: GetDepthBuffer()
    // Desc: Find the depth buffer from the depth event index
    //----------------------------------------------------------------------------------

    public ushort[] GetDepthBuffer(UInt32 uDepthEventIndex)
    {
        UInt32 hr = 0;
        UInt32 uDepthFrame;
        UInt64 uMicroseconds;
        UInt32 uBufferSize = m_uDepthWidth * m_uDepthHeight * sizeof(ushort);  // as per documentation
        ushort[] pDepthBuffer = new ushort[m_uDepthWidth * m_uDepthHeight];

        // This is safe since the Xed API is native, the garbage collector will to the pinning
        hr = XedReadNuiDepthFrame(m_pContext, uDepthEventIndex, out uDepthFrame, out uMicroseconds, pDepthBuffer, uBufferSize, false);
        if (hr != 0)
        {
            return null;
        }

        return pDepthBuffer;
    }


    //----------------------------------------------------------------------------------
    // Name: GetSkeletonFrameFromDepthEventIndex()
    // Desc: Find a skeleton data event assosiated with this depth event
    //----------------------------------------------------------------------------------

    public bool GetSkeletonFrameFromDepthEventIndex(UInt32 uDepthEventIndex, ref NUI_SKELETON_FRAME skeletonFrame)
    {
        // Get frame data from depth event
        UInt32 uDepthFrame;
        UInt64 uMicroseconds;
        UInt32 hr = XedReadNuiDepthFrame(m_pContext, uDepthEventIndex, out uDepthFrame, out uMicroseconds, null, 0, false);
        if (hr != 0)
        {
            return false;
        }

        // Get the skeleton event index from the depth frame
        UInt32 uSkeletonEventIndex = XedGetNuiSkeletonEventIndexFromFrameNumber(m_pContext, uDepthFrame);

        // Check that we have a valid skeleton event at this depth frame
        if (uSkeletonEventIndex != XStudio.Constants.XED_EVENTINDEX_INVALID)
        {
            // Read the skeleton data at the current skeleton event index
            UInt32 uSkeletonFrame = 0;
            hr = XedReadNuiSkeletonFrame(m_pContext, uSkeletonEventIndex, out uSkeletonFrame, out uMicroseconds, out skeletonFrame, m_uSkeletonFrameSize);

            if (hr == 0)
            {
                return true;
            }
        }

        return false;
    }


    //----------------------------------------------------------------------------------
    // Name: GetTitleData()
    // Desc: Find title data from the title data event index. The function returns the
    //       pointer to the title data and the title data index which can be used
    //       to synchronize with other data events
    //----------------------------------------------------------------------------------

    public bool GetTitleData(UInt32 uTitleDataEventIndex, ref IntPtr pTitleData, ref XSTUDIO_TITLE_INDEX_DATA pTitleDataIndex)
    {
        // Check that we have a valid event index
        if (uTitleDataEventIndex != XStudio.Constants.XED_EVENTINDEX_INVALID)
        {
            // Find out how big the title data is
            UInt32 uRequiredSize;
            UInt64 uEventMicroseconds;
            UInt32 hr = XedReadTitleData(m_pContext, uTitleDataEventIndex, out uEventMicroseconds, out pTitleDataIndex, IntPtr.Zero, 0, out uRequiredSize);

            if (hr != 0)
            {
                return false;
            }

            // Allocate buffer to hold the data
            byte[] pByteData = new byte[uRequiredSize];
            unsafe
            {
                fixed (byte* pBytePointer = pByteData)
                {
                    pTitleData = (IntPtr)pBytePointer;

                    // Read the title data at the current event index, this time with the allocated buffer
                    hr = XedReadTitleData(m_pContext, uTitleDataEventIndex, out uEventMicroseconds, out pTitleDataIndex, pTitleData, uRequiredSize, out uRequiredSize);
                }
            }

            if (hr == 0)
            {
                return true;
            }
        }

        return false;
    }   
}


//--------------------------------------------------------------------------------------
// Defines a class that opens a Xed file for writing
//--------------------------------------------------------------------------------------

public class XedFileWriter : XedFile
{
    //----------------------------------------------------------------------------------
    // Name: Open()
    // Desc: Opens a xed file for writing
    //----------------------------------------------------------------------------------

    public ErrorCode Open(String fileName, IntPtr pXedContextTemplate)
    {
        Close();
        Reset();

        // NOTE: The Xed API's cannot read/write to the same file, and to be efficient
        // we need to use the XedCopy API's which only works with XedCreate, so we always
        // do the following:
        //
        //  1. Remove ReadOnly file attrib
        //  2. Delete %TEMP%\\XedFileWriter.tmp
        //  3. XedFileCreateFile( %TEMP%\\XedFileWriter.tmp )
        //  4. XedCopyXXX() or XedWriteXXX()
        //  5. XedClose()
        //  6. Copy %TEMP%\\XedFileWriter.tmp fileName

        // First make sure our temp file is deleted
        String tempPath = Environment.GetEnvironmentVariable("TEMP");
        String tempFileName = tempPath + "\\XedFileWriter.tmp";

        // Delete the temp file
        FileInfo tempFile = new FileInfo(tempFileName);
        if (tempFile.Exists)
        {
            try
            {
                // Get file attributes and remove ReadOnly
                FileAttributes fileAttributes = File.GetAttributes(tempFileName);
                fileAttributes = fileAttributes & ~FileAttributes.ReadOnly;
                File.SetAttributes(tempFileName, fileAttributes);

                File.Delete(tempFileName);
            }
            catch
            {
                return ErrorCode.ERROR_DELETING_TEMP_FILE;
            }
        }

        // Create the temp file
        UInt32 hr = XedCreateFile(tempFileName, pXedContextTemplate, 0, ref m_pContext);
        if (hr != 0)
        {
            return ErrorCode.ERROR_CREATING_TEMP_FILE;
        }

        m_FileName = fileName;

        return ErrorCode.ERROR_NONE;
    }


    //--------------------------------------------------------------------------------------
    // Name: Close()
    // Desc: Closes the file and attemps to copy the tmp file to the actual file
    //--------------------------------------------------------------------------------------

    public override ErrorCode Close()
    {
        // Close the xed file
        if (m_pContext != IntPtr.Zero)
        {
            UInt32 hr = XedCloseFile(ref m_pContext);
            m_pContext = IntPtr.Zero;
        }

        if (m_FileName != null)
        {
            // Copy the temp file to the actual filename
            String tempPath = Environment.GetEnvironmentVariable("TEMP");
            String tempFileName = tempPath + "\\XedFileWriter.tmp";

            FileInfo tempFile = new FileInfo(tempFileName);
            if (tempFile.Exists)
            {
                // Get file attributes from original file. If the original file is ReadOnly, we
                // respect it and return an error message. We don't want to
                // save over valuable orignal data that was marked as ReadOnly
                try
                {
                    FileAttributes fileAttributes = File.GetAttributes(m_FileName);
                    if ((fileAttributes & FileAttributes.ReadOnly) == FileAttributes.ReadOnly)
                    {
                        return ErrorCode.ERROR_COPYING_TEMP_FILE_READ_ONLY;
                    }
                }
                catch (System.Exception)
                {
                	// If we caught here, the file doesn't exist.
                }

                try
                {
                    // Now copy the temp file
                    tempFile.CopyTo(m_FileName, true);
                }
                catch
                {
                    return ErrorCode.ERROR_COPYING_TEMP_FILE;
                }
            }
            else
            {
                // This should never happen since we just created the temp file ourselves, but
                // who knows what other processes might be trying to access the file
                return ErrorCode.ERROR_TEMP_FILE_DONT_EXIST;
            }
        }

        return ErrorCode.ERROR_NONE;
    }
}

