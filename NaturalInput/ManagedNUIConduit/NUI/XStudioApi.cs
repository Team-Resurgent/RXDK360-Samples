#region File Information
//-----------------------------------------------------------------------------
// XboxStudioAPI.cs
//
// P/Invoke-based wrapper around the Xbox Studio API set.
//
// NOTE: Naming conventions in this file do not follow standard .NET style.
//       The intention is to expose, as closely as possible, the interface of
//       the existing Xbox Studio API set.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Runtime.InteropServices;

namespace Microsoft.ATG.NUI
{
    /// <summary>
    /// P/Invoke-based wrapper around the Xbox Studio API set.
    /// </summary>
    internal static class XStudioApi
    {
        #region Private Helper Enumerations

        private enum HResultBase
        {
            E_WIN32 = unchecked((int)0x80070000),
            E_NUI = unchecked((int)0x83010000),
            S_XSTUDIO = unchecked((int)0x03100000),
            E_XSTUDIO = unchecked((int)0x83100000),
        }

        private enum Win32ErrorCode
        {
            ERROR_SUCCESS = 0,
            ERROR_INVALID_FUNCTION = 1,
            ERROR_FILE_NOT_FOUND = 2,
            ERROR_PATH_NOT_FOUND = 3,
            ERROR_TOO_MANY_OPEN_FILES = 4,
            ERROR_ACCESS_DENIED = 5,
            ERROR_INVALID_HANDLE = 6,
            ERROR_INVALID_DATA = 13,
            ERROR_OUTOFMEMORY = 14,
            ERROR_NOT_READY = 21,
            ERROR_SHARING_VIOLATION = 32,
            ERROR_INVALID_PARAMETER = 87,
            ERROR_BUSY = 170,
            ERROR_ALREADY_EXISTS = 183,
            ERROR_MORE_DATA = 234,
            ERROR_NO_MORE_ITEMS = 259,
            ERROR_INVALID_FLAGS = 1004,
            ERROR_DEVICE_NOT_CONNECTED = 1167,
            ERROR_CONNECTION_UNAVAIL = 1201,
            ERROR_ALREADY_INITIALIZED = 1247,
            ERROR_TIMEOUT = 1460,
            ERROR_INVALID_OPERATION = 4317,
        }

        #endregion

        /// <summary>
        /// Success and failure codes are encapsulated in this enumeration.
        /// </summary>
        internal enum HRESULT
        {
            S_OK = 0,

            #region HRESULT codes for Facility NUI (0x301)

            E_NUI_DEVICE_NOT_CONNECTED = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_DEVICE_NOT_CONNECTED,
            E_NUI_DEVICE_NOT_READY = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_NOT_READY,
            E_NUI_ALREADY_INITIALIZED = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_ALREADY_INITIALIZED,
            E_NUI_IDENTITY_BUSY = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_BUSY,
            E_NUI_NO_MORE_ITEMS = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_NO_MORE_ITEMS,

            E_NUI_FRAME_NO_DATA = HResultBase.E_NUI | 1,
            E_NUI_STREAM_NOT_ENABLED = HResultBase.E_NUI | 2,
            E_NUI_IMAGE_STREAM_IN_USE = HResultBase.E_NUI | 3,
            E_NUI_FRAME_LIMIT_EXCEEDED = HResultBase.E_NUI | 4,
            E_NUI_FEATURE_NOT_INITIALIZED = HResultBase.E_NUI | 5,
            E_NUI_IDENTITY_ENROLLMENT_LIMIT = HResultBase.E_NUI | 6,
            E_NUI_IDENTITY_UI_REQUIRED = HResultBase.E_NUI | 7,
            E_NUI_IDENTITY_LOST_TRACK = HResultBase.E_NUI | 8,
            E_NUI_IDENTITY_QUALITY_ISSUE = HResultBase.E_NUI | 9,
            E_NUI_IDENTITY_NO_UNENROLL_SIGN_IN = HResultBase.E_NUI | 10,
            E_NUI_SYSTEM_UI_PRESENT = HResultBase.E_NUI | 11,
            E_NUI_MUST_CALL_IDENTIFY = HResultBase.E_NUI | 12,
            E_NUI_DATABASE_NOT_FOUND = HResultBase.E_NUI | 13,
            E_NUI_DATABASE_VERSION_MISMATCH = HResultBase.E_NUI | 14,

            #endregion

            #region HRESULT codes for Facility XSTUDIO (0x310)

            S_XSTUDIO_SUCCESS = 0,
            S_XSTUDIO_STOPPED = HResultBase.S_XSTUDIO | 1,
            S_XSTUDIO_NO_PLAYER_INDEX = HResultBase.S_XSTUDIO | 2,

            E_XSTUDIO_INTERNAL = unchecked((int)0x80040005), // E_FAIL

            E_XSTUDIO_OUT_OF_MEMORY = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_OUTOFMEMORY,
            E_XSTUDIO_INVALID_ARG = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_INVALID_PARAMETER,
            E_XSTUDIO_INVALID_HANDLE = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_INVALID_HANDLE,
            E_XSTUDIO_INVALID_FUNCTION = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_INVALID_FUNCTION,
            E_XSTUDIO_INVALID_FLAGS = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_INVALID_FLAGS,
            E_XSTUDIO_BUSY = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_BUSY,
            E_XSTUDIO_ACCESS_DENIED = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_ACCESS_DENIED,
            E_XSTUDIO_INVALID_DATA = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_INVALID_DATA,
            E_XSTUDIO_NO_CONNECTION = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_CONNECTION_UNAVAIL,
            E_XSTUDIO_FILE_ALREADY_EXISTS = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_ALREADY_EXISTS,
            E_XSTUDIO_FILE_NOT_FOUND = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_FILE_NOT_FOUND,
            E_XSTUDIO_PATH_NOT_FOUND = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_PATH_NOT_FOUND,
            E_XSTUDIO_SHARING_VIOLATION = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_SHARING_VIOLATION,
            E_XSTUDIO_TOO_MANY_OPEN_FILES = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_TOO_MANY_OPEN_FILES,
            E_XSTUDIO_NO_MORE_ITEMS = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_NO_MORE_ITEMS,
            E_XSTUDIO_MORE_DATA = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_MORE_DATA,
            E_XSTUDIO_TIMEOUT = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_TIMEOUT,
            E_XSTUDIO_INVALID_OPERATION = HResultBase.E_WIN32 | Win32ErrorCode.ERROR_INVALID_OPERATION,

            E_XSTUDIO_PROTOCOL_MISMATCH = HResultBase.E_XSTUDIO | 2,
            E_XSTUDIO_TITLE_TERMINATE = HResultBase.E_XSTUDIO | 3,
            E_XSTUDIO_NUI_NOT_INITIALIZED = HResultBase.E_XSTUDIO | 4,
            E_XSTUDIO_HEADLESS_MODE = HResultBase.E_XSTUDIO | 5,
            E_XSTUDIO_NO_DATA_AVAILABLE = HResultBase.E_XSTUDIO | 6,
            E_XSTUDIO_FILE_CONVERSION_REQUIRE = HResultBase.E_XSTUDIO | 7,
            E_XSTUDIO_INVALID_STREAM = HResultBase.E_XSTUDIO | 8,
            E_XSTUDIO_STREAM_NOT_MAPPED = HResultBase.E_XSTUDIO | 9,
            E_XSTUDIO_STREAM_ALREADY_MAPPED = HResultBase.E_XSTUDIO | 10,
            E_XSTUDIO_NO_STREAM_SPECIFIED = HResultBase.E_XSTUDIO | 13,
            E_XSTUDIO_INVALID_TIMESTAMP = HResultBase.E_XSTUDIO | 14,
            E_XSTUDIO_SKELETON_DATA_REQUIRED = HResultBase.E_XSTUDIO | 15,
            E_XSTUDIO_TRACKING_ID_NOT_FOUND = HResultBase.E_XSTUDIO | 16,
            E_XSTUDIO_INVALID_EVENT_INDEX = HResultBase.E_XSTUDIO | 17,
            E_XSTUDIO_NOT_SUSPENDED = HResultBase.E_XSTUDIO | 18,
            E_XSTUDIO_RECORD_WHILE_SUSPENDED = HResultBase.E_XSTUDIO | 19,
            E_XSTUDIO_SUSPEND_WHILE_RECORDING = HResultBase.E_XSTUDIO | 20,
            E_XSTUDIO_SUSPEND_WITHOUT_PLAYING = HResultBase.E_XSTUDIO | 21,

            #endregion
        }

        #region NUI Constants and Enumerations

        internal enum NUI_IMAGE_TYPE
        {
            NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX,
            NUI_IMAGE_TYPE_COLOR,
            NUI_IMAGE_TYPE_COLOR_YUV,
            NUI_IMAGE_TYPE_DEPTH,
            NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_IN_COLOR_SPACE,
            NUI_IMAGE_TYPE_DEPTH_IN_COLOR_SPACE,
            NUI_IMAGE_TYPE_COLOR_IN_DEPTH_SPACE,
            NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_80x60,
            NUI_IMAGE_TYPE_DEPTH_80x60,
        }

        internal enum NUI_IMAGE_RESOLUTION
        {
            NUI_IMAGE_RESOLUTION_80x60,
            NUI_IMAGE_RESOLUTION_320x240,
            NUI_IMAGE_RESOLUTION_640x480,
        }

        internal const int NUI_IMAGE_PLAYER_INDEX_SHIFT = 3;
        internal const int NUI_IMAGE_PLAYER_INDEX_MASK = (1 << NUI_IMAGE_PLAYER_INDEX_SHIFT) - 1;
        internal const int NUI_IMAGE_DEPTH_MAXIMUM = (4000 << NUI_IMAGE_PLAYER_INDEX_SHIFT) | NUI_IMAGE_PLAYER_INDEX_MASK;
        internal const int NUI_IMAGE_DEPTH_MINIMUM = 800 << NUI_IMAGE_PLAYER_INDEX_SHIFT;
        internal const int NUI_IMAGE_DEPTH_NO_VALUE = 0;
        internal const int NUI_IMAGE_DEPTH_BUFFER_SIZE = 384 * 240 * 2;
        internal const int NUI_IMAGE_DEPTH_80x60_BUFFER_SIZE = 128 * 60 * 2;
        internal const int NUI_IMAGE_COLOR_640x480_BUFFER_SIZE = 640 * 480 * 4;
        internal const int NUI_IMAGE_COLOR_YUV_640x480_BUFFER_SIZE = 640 * 480 * 2;

        internal const float NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS = 285.63f; // Based on 320x240 pixel size.
        internal const float NUI_CAMERA_DEPTH_NOMINAL_INVERSE_FOCAL_LENGTH_IN_PIXELS = 1.0f / NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS;
        internal const float NUI_CAMERA_DEPTH_NOMINAL_DIAGONAL_FOV = 70.0f;
        internal const float NUI_CAMERA_DEPTH_NOMINAL_HORIZONTAL_FOV = 58.5f;
        internal const float NUI_CAMERA_DEPTH_NOMINAL_VERTICAL_FOV = 45.6f;

        internal const float NUI_CAMERA_COLOR_NOMINAL_FOCAL_LENGTH_IN_PIXELS = 531.15f; // Based on 640x480 pixel size.
        internal const float NUI_CAMERA_COLOR_NOMINAL_INVERSE_FOCAL_LENGTH_IN_PIXELS = 1.0f / NUI_CAMERA_COLOR_NOMINAL_FOCAL_LENGTH_IN_PIXELS;
        internal const float NUI_CAMERA_COLOR_NOMINAL_DIAGONAL_FOV = 73.9f;
        internal const float NUI_CAMERA_COLOR_NOMINAL_HORIZONTAL_FOV = 62.0f;
        internal const float NUI_CAMERA_COLOR_NOMINAL_VERTICAL_FOV = 48.6f;

        internal enum NUI_IMAGE_DIGITALZOOM
        {
            NUI_IMAGE_DIGITAL_ZOOM_1X,
            NUI_IMAGE_DIGITAL_ZOOM_2X,
        }

        internal enum NUI_SKELETON_POSITION_INDEX
        {
            NUI_SKELETON_POSITION_HIP_CENTER,
            NUI_SKELETON_POSITION_SPINE,
            NUI_SKELETON_POSITION_SHOULDER_CENTER,
            NUI_SKELETON_POSITION_HEAD,
            NUI_SKELETON_POSITION_SHOULDER_LEFT,
            NUI_SKELETON_POSITION_ELBOW_LEFT,
            NUI_SKELETON_POSITION_WRIST_LEFT,
            NUI_SKELETON_POSITION_HAND_LEFT,
            NUI_SKELETON_POSITION_SHOULDER_RIGHT,
            NUI_SKELETON_POSITION_ELBOW_RIGHT,
            NUI_SKELETON_POSITION_WRIST_RIGHT,
            NUI_SKELETON_POSITION_HAND_RIGHT,
            NUI_SKELETON_POSITION_HIP_LEFT,
            NUI_SKELETON_POSITION_KNEE_LEFT,
            NUI_SKELETON_POSITION_ANKLE_LEFT,
            NUI_SKELETON_POSITION_FOOT_LEFT,
            NUI_SKELETON_POSITION_HIP_RIGHT,
            NUI_SKELETON_POSITION_KNEE_RIGHT,
            NUI_SKELETON_POSITION_ANKLE_RIGHT,
            NUI_SKELETON_POSITION_FOOT_RIGHT,
            
            NUI_SKELETON_POSITION_COUNT
        }

        internal const int NUI_SKELETON_COUNT = 6;
        internal const int NUI_SKELETON_MAX_TRACKED_COUNT = 2;
        internal const int NUI_SKELETON_INVALID_TRACKING_ID = 0;

        internal enum NUI_SKELETON_POSITION_TRACKING_STATE
        {
            NUI_SKELETON_POSITION_NOT_TRACKED,
            NUI_SKELETON_POSITION_INFERRED,
            NUI_SKELETON_POSITION_TRACKED,
        }

        internal enum NUI_SKELETON_TRACKING_STATE
        {
            NUI_SKELETON_NOT_TRACKED,
            NUI_SKELETON_POSITION_ONLY,
            NUI_SKELETON_TRACKED,
        }

        [Flags]
        internal enum NUI_SKELETON_FRAME_FLAGS : uint
        {
            NUI_SKELETON_FRAME_FLAG_CAMERA_MOTION = 0x1,
            NUI_SKELETON_FRAME_FLAG_EXTRAPOLATED_FLOOR = 0x2,
            NUI_SKELETON_FRAME_FLAG_UPPER_BODY_SKELETON = 0x4,
        }

        [Flags]
        internal enum NUI_SKELETON_QUALITY_FLAGS : uint
        {
            NUI_SKELETON_QUALITY_CLIPPED_RIGHT = 0x1,
            NUI_SKELETON_QUALITY_CLIPPED_LEFT = 0x2,
            NUI_SKELETON_QUALITY_CLIPPED_TOP = 0x4,
            NUI_SKELETON_QUALITY_CLIPPED_BOTTOM = 0x8,
        }

        [Flags]
        internal enum NUI_IDENTITY_QUALITY_FLAGS : uint
        {
            NUI_IDENTITY_QUALITY_ENVIRONMENT_FACE_DETECT_FAILURE = 0x0001,
            NUI_IDENTITY_QUALITY_USER_BODY_TURNED = 0x0002,
            NUI_IDENTITY_QUALITY_USER_NOT_UPRIGHT = 0x0004,
            NUI_IDENTITY_QUALITY_USER_OCCLUDED_FACE = 0x0008,
            NUI_IDENTITY_QUALITY_USER_OCCLUDED_BODY = 0x0010,
            NUI_IDENTITY_QUALITY_USER_FAR_AWAY = 0x0020,
            NUI_IDENTITY_QUALITY_USER_CLOSE = 0x0040,
            NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_LEFT = 0x0080,
            NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_RIGHT = 0x0100,
            NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_TOP = 0x0200,
            NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_BOTTOM = 0x0400,
            NUI_IDENTITY_QUALITY_ENVIRONMENT_TOO_DARK = 0x0800,
            NUI_IDENTITY_QUALITY_ENVIRONMENT_TOO_BRIGHT = 0x1000,

            NUI_IDENTITY_QUALITY_ENVIRONMENT_MASK =
                NUI_IDENTITY_QUALITY_ENVIRONMENT_FACE_DETECT_FAILURE |
                NUI_IDENTITY_QUALITY_ENVIRONMENT_TOO_DARK |
                NUI_IDENTITY_QUALITY_ENVIRONMENT_TOO_BRIGHT,
            
            NUI_IDENTITY_QUALITY_USER_MASK =
                NUI_IDENTITY_QUALITY_USER_NOT_UPRIGHT |
                NUI_IDENTITY_QUALITY_USER_OCCLUDED_FACE |
                NUI_IDENTITY_QUALITY_USER_OCCLUDED_BODY |
                NUI_IDENTITY_QUALITY_USER_FAR_AWAY |
                NUI_IDENTITY_QUALITY_USER_CLOSE |
                NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_LEFT |
                NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_RIGHT |
                NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_TOP |
                NUI_IDENTITY_QUALITY_USER_CLIPPED_AT_BOTTOM,
        }

        internal enum NUI_IDENTITY_MESSAGE_ID
        {
            NUI_IDENTITY_MESSAGE_ID_FRAME_PROCESSED,
            NUI_IDENTITY_MESSAGE_ID_COMPLETE,
        }

        internal enum NUI_IDENTITY_OPERATION_ID
        {
            NUI_IDENTITY_OPERATION_ID_NONE,
            NUI_IDENTITY_OPERATION_ID_IDENTIFY,
            NUI_IDENTITY_OPERATION_ID_ENROLL,
            NUI_IDENTITY_OPERATION_ID_TUNER,
        }

        #endregion

        #region XSTUDIO Constants and Enumerations

        internal enum DESIRED_ACCESS : uint
        {
            GENERIC_READ = 0x80000000,
            GENERIC_WRITE = 0x40000000,
        }

        internal enum CREATION_DISPOSITION : uint
        {
            CREATE_NEW = 1,
            CREATE_ALWAYS,
            OPEN_EXISTING,
        }

        internal enum XSTUDIO_CONNECTION_TYPE
        {
            XSTUDIO_CONNECTION_TYPE_USB,
            XSTUDIO_CONNECTION_TYPE_TCP,
        }

        internal enum XSTUDIO_PRODUCER
        {
            XSTUDIO_PRODUCER_FILE,
        }

        [Flags]
        internal enum XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG : uint
        {
            XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG_CONSUMER = 0x001,
            XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG_PRODUCER = 0x002,
            XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG_PRODUCER_FILL_IN = 0x005,
            XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG_REALTIME = 0x100,
        }

        internal enum XSTUDIO_STREAM_ID
        {
            XSTUDIO_STREAM_ID_NUICAM_DEPTH = 0,
            XSTUDIO_STREAM_ID_NUICAM_COLOR = 1,
            XSTUDIO_STREAM_ID_NUIAPI_SKELETON = 4,
            XSTUDIO_STREAM_ID_NUIAPI_IDENTITY = 6,
            XSTUDIO_STREAM_ID_NUIAPI_PLAYER_INDEX = 7,
            XSTUDIO_STREAM_ID_TITLE_DATA = 16,
            XSTUDIO_STREAM_ID_MISC_PRIVATE = 31,
            XSTUDIO_STREAM_ID_COUNT = 32,
            XSTUDIO_STREAM_ID_INVALID = -1,
        }

        [Flags]
        internal enum XSTUDIO_STREAM_FLAGS : uint
        {
            XSTUDIO_STREAM_FLAG_NONE = 0,
            XSTUDIO_STREAM_FLAG_DEFAULT = unchecked((uint)-1),

            XSTUDIO_STREAM_FLAG_NUICAM_DEPTH = 1 << XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUICAM_DEPTH,
            XSTUDIO_STREAM_FLAG_NUICAM_COLOR = 1 << XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUICAM_COLOR,
            XSTUDIO_STREAM_FLAG_NUICAM_ALL =
                XSTUDIO_STREAM_FLAG_NUICAM_DEPTH |
                XSTUDIO_STREAM_FLAG_NUICAM_COLOR,

            XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON = 1 << XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_SKELETON,
            XSTUDIO_STREAM_FLAG_NUIAPI_IDENTITY = 1 << XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_IDENTITY,
            XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX = 1 << XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_PLAYER_INDEX,
            XSTUDIO_STREAM_FLAG_NUIAPI_ALL =
                XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON |
                XSTUDIO_STREAM_FLAG_NUIAPI_IDENTITY |
                XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX,

            XSTUDIO_STREAM_FLAG_TITLE_DATA = 1 << XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_TITLE_DATA,
            XSTUDIO_STREAM_FLAG_TITLE_ALL =
                XSTUDIO_STREAM_FLAG_TITLE_DATA,

            XSTUDIO_STREAM_FLAG_ALL =
                XSTUDIO_STREAM_FLAG_NUICAM_ALL |
                XSTUDIO_STREAM_FLAG_NUIAPI_ALL |
                XSTUDIO_STREAM_FLAG_TITLE_ALL,
        }

        [Flags]
        internal enum XSTUDIO_NOTIF : uint
        {
            XSTUDIO_NOTIF_PLAYBACK_STARTING = 0x0001,
            XSTUDIO_NOTIF_PLAYBACK_STOPPED = 0x0002,
            XSTUDIO_NOTIF_RECORDING_STARTING = 0x0004,
            XSTUDIO_NOTIF_RECORDING_STOPPED = 0x0008,
            XSTUDIO_NOTIF_SUSPENDED = 0x0010,
            XSTUDIO_NOTIF_RESUMING = 0x0020,
            XSTUDIO_NOTIF_LOOPING = 0x0040,
            XSTUDIO_NOTIF_SWITCHING_TO_PROD = 0x0080,
            XSTUDIO_NOTIF_SWITCHED_TO_PROD = 0x0100,
        }

        internal enum XSTUDIO_NOTIF_SUSPEND
        {
            XSTUDIO_NOTIF_SUSPEND_API = 1,
            XSTUDIO_NOTIF_SUSPEND_SUSPENDPOINT,
            XSTUDIO_NOTIF_SUSPEND_SINGLESTEP,
        }

        internal const int XSTUDIO_TITLE_DATA_MAX_BUFFER_SIZE = 32 * 1024;

        internal enum XSTUDIO_INPUT_MODE
        {
            XSTUDIO_INPUT_MODE_DEFAULT,
            XSTUDIO_INPUT_MODE_SIMULATED,

            XSTUDIO_INPUT_MODE_INVALID = unchecked((int)0xffffffff),
        }

        #endregion

        #region NUI Structures

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        internal struct XMVECTOR
        {
            public float x, y, z, w;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4, Size = 448)]
        internal struct NUI_SKELETON_DATA
        {
            public NUI_SKELETON_TRACKING_STATE eTrackingState;
            public uint dwTrackingID;
            public uint dwEnrollmentIndex;
            public uint dwUserIndex;
            public XMVECTOR Position;
            
            private const int ArrayLength = (int)NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_COUNT;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = ArrayLength)]
            public XMVECTOR[] SkeletonPositions;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = ArrayLength)]
            public NUI_SKELETON_POSITION_TRACKING_STATE[] eSkeletonPositionTrackingState;

            public NUI_SKELETON_QUALITY_FLAGS dwQualityFlags;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        internal struct NUI_SKELETON_FRAME
        {
            public long liTimeStamp;
            public uint dwFrameNumber;
            public NUI_SKELETON_FRAME_FLAGS dwFlags;
            public XMVECTOR vFloorClipPlane;
            public XMVECTOR vNormalToGravity;

            private const int ArrayLength = NUI_SKELETON_COUNT;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = ArrayLength)]
            public NUI_SKELETON_DATA[] SkeletonData;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        internal struct NUI_IDENTITY_MESSAGE_FRAME_PROCESSED
        {
            public NUI_IDENTITY_QUALITY_FLAGS dwQualityFlags;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        internal struct NUI_IDENTITY_MESSAGE_COMPLETE
        {
            public HRESULT hrResult;
            public uint dwEnrollmentIndex;
            
            [MarshalAs(UnmanagedType.Bool)]
            public bool bProfileMatched;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        internal struct NUI_IDENTITY_MESSAGE
        {
            public NUI_IDENTITY_MESSAGE_ID MessageId;
            public NUI_IDENTITY_OPERATION_ID OperationId;
            public uint dwTrackingID;
            public uint dwSkeletonFrameNumber;

            [StructLayout(LayoutKind.Explicit, Size = 16)]
            public struct DataUnion
            {
                [FieldOffset(0)]
                public NUI_IDENTITY_MESSAGE_FRAME_PROCESSED FrameProcessed;

                [FieldOffset(0)]
                public NUI_IDENTITY_MESSAGE_COMPLETE Complete;
            }

            public DataUnion Data;
        }

        #endregion

        #region XSTUDIO Structures

        [StructLayout(LayoutKind.Sequential, Pack = 8)]
        internal struct XSTUDIO_STREAM_EVENT
        {
            public uint cbSize;
            public XSTUDIO_STREAM_ID StreamID;
            public uint uEventIndex;
            public ulong uEventMicroseconds;
            public IntPtr pvIndexBuffer;
            public uint cbIndexBufferSize;
            public uint cbIndexBufferUsed;
            public IntPtr pvDataBuffer;
            public uint cbDataBufferSize;
            public uint cbDataBufferUsed;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        internal struct XSTUDIO_NUICAM_IMAGE_INDEX_DATA
        {
            public NUI_IMAGE_TYPE ImageType;
            public ushort Width;
            public ushort Height;
            public uint FrameNumber;
            public ulong NuiTimeStamp;
        }

        [StructLayout(LayoutKind.Sequential, Pack = 4)]
        internal struct XSTUDIO_TITLE_INDEX_DATA
        {
            public ulong Reserved;

            private const int ArrayLength = 4;

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = ArrayLength)]
            public uint[] IndexData;
        }

        #endregion

        #region XSTUDIO Delegates

        internal delegate HRESULT XSTUDIO_STREAM_CALLBACK(
            ref XSTUDIO_STREAM_EVENT pStreamEvent,
            IntPtr pvStreamCallbackContext);

        internal delegate HRESULT XSTUDIO_NOTIFICATION_CALLBACK(
            XSTUDIO_STREAM_FLAGS dwStreams,
            XSTUDIO_NOTIF dwNotification,
            IntPtr pvContext,
            IntPtr lparam);

        #endregion

        #region XSTUDIO Methods

        [DllImport("xstudio.dll", CharSet = CharSet.Ansi)]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioConnect(
            string sConsoleName,
            XSTUDIO_CONNECTION_TYPE ConnectionType);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioDisconnect();

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioGetConnectionStatus();

        [DllImport("xstudio.dll", CharSet = CharSet.Ansi)]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioCreateFile(
            string sFilePath,
            DESIRED_ACCESS dwDesiredAccess,
            CREATION_DISPOSITION dwCreationDisposition,
            XSTUDIO_STREAM_FLAGS dwStreams,
            out IntPtr phFile);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioCloseFile(IntPtr hFile);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioGetFileStreams(
            IntPtr hFile,
            out XSTUDIO_STREAM_FLAGS pdwStreams);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioMapStreams(
            XSTUDIO_STREAM_FLAGS dwStreams,
            IntPtr hHandle);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioUnmapStreams(
            XSTUDIO_STREAM_FLAGS dwStreams,
            IntPtr hHandle);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioSwitchToProducers(
            XSTUDIO_STREAM_FLAGS dwStreams,
            XSTUDIO_PRODUCER producer);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        internal static extern HRESULT XStudioRegisterStreamCallback(
            XSTUDIO_STREAM_CALLBACK pfnStreamCallback,
            XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG dwStreamCallbackRegistrationFlags,
            IntPtr pvStreamCallbackContext,
            out IntPtr phStreamCallback);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioCloseHandle(IntPtr Handle);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioRegisterNotificationCallback(
            XSTUDIO_NOTIFICATION_CALLBACK pfnNotificationCallback,
            XSTUDIO_STREAM_FLAGS dwStreams,
            XSTUDIO_NOTIF dwNotificationsFlags,
            uint dwFlags,
            IntPtr pvContext,
            out IntPtr phandle);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioStart(XSTUDIO_STREAM_FLAGS dwStreams);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioStop(XSTUDIO_STREAM_FLAGS dwStreams);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioStopEx(
            XSTUDIO_STREAM_FLAGS dwStreams,
            IntPtr handle);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioGetActiveStreams(out XSTUDIO_STREAM_FLAGS pdwStreams);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioGetErrorStreams(out XSTUDIO_STREAM_FLAGS pdwStreams);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioSetInputMode(
            XSTUDIO_STREAM_FLAGS streamFlags,
            XSTUDIO_INPUT_MODE InputMode);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioGetInputMode(
            XSTUDIO_STREAM_FLAGS streamFlags,
            out XSTUDIO_INPUT_MODE pInputModeOut);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioSetLoopCount(
            XSTUDIO_STREAM_FLAGS streamFlags,
            uint loopCount);
        
        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioGetLoopCount(
            XSTUDIO_STREAM_FLAGS streamFlags,
            out uint pLoopCount);
        
        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioSuspend(XSTUDIO_STREAM_FLAGS streamFlags);
        
        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioResume(XSTUDIO_STREAM_FLAGS streamFlags);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioAddSuspendPoint(
            XSTUDIO_STREAM_ID dwStreamID,
            uint dwEventIndex,
            out IntPtr phandle);

        [DllImport("xstudio.dll")]
        [return: MarshalAs(UnmanagedType.I4)]
        public static extern HRESULT XStudioSingleStep(XSTUDIO_STREAM_FLAGS dwStreams);

        #endregion
    }
}
