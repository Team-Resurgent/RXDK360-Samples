#region File Information
//-----------------------------------------------------------------------------
// NotificationEventArgs.cs
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

namespace Microsoft.ATG.NUI
{
    /// <summary>
    /// Encapsulation of Xbox Studio notification data.
    /// </summary>
    public class NotificationEventArgs : EventArgs
    {
        /// <summary>
        /// The streams to which this notification applies.
        /// </summary>
        public StreamType StreamTypes { get; private set; }

        /// <summary>
        /// The type of notification.
        /// </summary>
        public Notification Notification { get; private set; }
        
        /// <summary>
        /// The type of producer to which Xbox Studio is switching.
        /// </summary>
        public Producer? Producer { get; private set; }

        public NotificationEventArgs(StreamType streamTypes, Notification notification, Producer? producer)
        {
            StreamTypes = streamTypes;
            Notification = notification;
            Producer = producer;
        }
    }

    public delegate void NotificationEventHandler(object context, NotificationEventArgs args);

    [Flags]
    public enum Notification : uint
    {
        PlaybackStarting = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_PLAYBACK_STARTING,
        PlaybackStopped = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_PLAYBACK_STOPPED,
        RecordingStarting = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_RECORDING_STARTING,
        RecordingStopped = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_RECORDING_STOPPED,
        Suspended = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_SUSPENDED,
        Resuming = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_RESUMING,
        Looping = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_LOOPING,
        SwitchingToProducer = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_SWITCHING_TO_PROD,
        SwitchedToProducer = XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_SWITCHED_TO_PROD,

        All =
            PlaybackStarting |
            PlaybackStopped |
            RecordingStarting |
            RecordingStopped |
            Suspended |
            Resuming |
            Looping |
            SwitchingToProducer |
            SwitchingToProducer,
    }

    public enum Producer
    {
        File = XStudioApi.XSTUDIO_PRODUCER.XSTUDIO_PRODUCER_FILE,
    }
}
