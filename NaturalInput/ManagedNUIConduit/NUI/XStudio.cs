#region File Information
//-----------------------------------------------------------------------------
// XStudio.cs
//
// Managed encapsulation of Xbox Studio APIs.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;

namespace Microsoft.ATG.NUI
{
    /// <summary>
    /// Managed encapsulation of Xbox Studio APIs.
    /// </summary>
    public static class XStudio
    {
        /// <summary>
        /// Establishes a connection from your computer to a development console.
        /// </summary>
        public static void Connect(string consoleName, ConnectionType connectionType)
        {
            XStudioApi.HRESULT hr = XStudioApi.XStudioConnect(
                consoleName,
                (XStudioApi.XSTUDIO_CONNECTION_TYPE)connectionType);

            if (hr < 0)
                throw new ApiException("XStudioConnect", (int)hr);
        }

        /// <summary>
        /// Disconnects the connection from your computer to the development console.
        /// </summary>
        public static void Disconnect()
        {
            XStudioApi.HRESULT hr = XStudioApi.XStudioDisconnect();
            
            if (hr < 0)
                throw new ApiException("XStudioDisconnect", (int)hr);
        }

        /// <summary>
        /// Determines whether your computer is connected to a development console.
        /// </summary>
        public static bool IsConnected
        {
            get
            {
                XStudioApi.HRESULT hr = XStudioApi.XStudioGetConnectionStatus();
                switch (hr)
                {
                    case XStudioApi.HRESULT.S_XSTUDIO_SUCCESS:
                        return true;

                    case XStudioApi.HRESULT.E_XSTUDIO_NO_CONNECTION:
                        return false;

                    default:
                        // Unexpected return value.
                        throw new ApiException("XStudioGetConnectionStatus", (int)hr);
                }
            }
        }

        /// <summary>
        /// Retrieves a list of the streams that are active currently.
        /// </summary>
        public static StreamType ActiveStreams
        {
            get
            {
                XStudioApi.XSTUDIO_STREAM_FLAGS streamFlags;
                XStudioApi.HRESULT hr = XStudioApi.XStudioGetActiveStreams(out streamFlags);

                if (hr < 0)
                    throw new ApiException("XStudioGetActiveStreams", (int)hr);

                return (StreamType)streamFlags;
            }
        }

        /// <summary>
        /// Starts recording or playback of the specified streams.
        /// </summary>
        public static void Start(StreamType streamType)
        {
            XStudioApi.HRESULT hr = XStudioApi.XStudioStart((XStudioApi.XSTUDIO_STREAM_FLAGS)streamType);
            
            if (hr < 0)
                throw new ApiException("XStudioStart", (int)hr);
        }

        /// <summary>
        /// Stops recording or playback of the specified streams.
        /// </summary>
        public static void Stop(StreamType streamType)
        {
            XStudioApi.HRESULT hr = XStudioApi.XStudioStop((XStudioApi.XSTUDIO_STREAM_FLAGS)streamType);
            
            if (hr < 0)
                throw new ApiException("XStudioStop", (int)hr);
        }
    }

    public enum ConnectionType
    {
        Usb = XStudioApi.XSTUDIO_CONNECTION_TYPE.XSTUDIO_CONNECTION_TYPE_USB,
        Tcp = XStudioApi.XSTUDIO_CONNECTION_TYPE.XSTUDIO_CONNECTION_TYPE_TCP,
    }

    /// <summary>
    /// The types of streams that can be received from the development console.
    /// </summary>
    [Flags]
    public enum StreamType : uint
    {
        None = XStudioApi.XSTUDIO_STREAM_FLAGS.XSTUDIO_STREAM_FLAG_NONE,
        All = XStudioApi.XSTUDIO_STREAM_FLAGS.XSTUDIO_STREAM_FLAG_ALL,

        Depth = XStudioApi.XSTUDIO_STREAM_FLAGS.XSTUDIO_STREAM_FLAG_NUICAM_DEPTH,
        Color = XStudioApi.XSTUDIO_STREAM_FLAGS.XSTUDIO_STREAM_FLAG_NUICAM_COLOR,

        PlayerIndex = XStudioApi.XSTUDIO_STREAM_FLAGS.XSTUDIO_STREAM_FLAG_NUIAPI_PLAYER_INDEX,
        Skeleton = XStudioApi.XSTUDIO_STREAM_FLAGS.XSTUDIO_STREAM_FLAG_NUIAPI_SKELETON,
    }
}
