#region File Information
//-----------------------------------------------------------------------------
// StreamEventArgs.cs
//
// Base class for Xbox Studio stream events.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;

namespace Microsoft.ATG.NUI
{
    /// <summary>
    /// Base class for Xbox Studio stream events.
    /// </summary>
    public abstract class StreamEventArgs : EventArgs
    {
        public StreamID StreamID { get; private set; }
        public uint EventIndex { get; private set; }
        public ulong EventMicroseconds { get; private set; }

        protected StreamEventArgs(StreamID streamID, uint eventIndex, ulong eventMicroseconds)
        {
            StreamID = streamID;
            EventIndex = eventIndex;
            EventMicroseconds = eventMicroseconds;
        }
    }

    public delegate void StreamEventHandler(object context, StreamEventArgs args);

    public enum StreamID
    {
        Depth = XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUICAM_DEPTH,
        Color = XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUICAM_COLOR,

        PlayerIndex = XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_PLAYER_INDEX,
        Skeleton = XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_SKELETON,
    }
}
