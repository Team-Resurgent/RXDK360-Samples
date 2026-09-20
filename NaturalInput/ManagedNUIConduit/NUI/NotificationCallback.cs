#region File Information
//-----------------------------------------------------------------------------
// NotificationCallback.cs
//
// Encapsulation of Xbox Studio notification events.
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
    /// Encapsulation of Xbox Studio notification events.
    /// </summary>
    public class NotificationCallback : IDisposable
    {
        #region Private Members

        private object context;

        private XStudioApi.XSTUDIO_NOTIFICATION_CALLBACK callbackDelegate;
        private GCHandle callbackDelegateHandle;
        private XStudioHandle callbackHandle;

        private NotificationCallback(object context)
        {
            this.context = context;
            callbackDelegate = NotifyCallback;

            // Prevent the delegate from being collected.
            callbackDelegateHandle = GCHandle.Alloc(callbackDelegate);
        }

        private XStudioApi.HRESULT NotifyCallback(XStudioApi.XSTUDIO_STREAM_FLAGS streams, XStudioApi.XSTUDIO_NOTIF notification, IntPtr context, IntPtr param)
        {
            // Do nothing, if there are no subscribers.
            if (NotificationEvent == null)
                return XStudioApi.HRESULT.S_XSTUDIO_SUCCESS;

            Producer? producer = null;
            if ((0 != (notification & XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_SWITCHING_TO_PROD)) ||
                (0 != (notification & XStudioApi.XSTUDIO_NOTIF.XSTUDIO_NOTIF_SWITCHED_TO_PROD)))
            {
                producer = (Producer)Enum.ToObject(typeof(Producer), (int)param);
            }

            NotificationEventArgs args = new NotificationEventArgs(
                (StreamType)streams,
                (Notification)notification,
                producer);

            // Raise the event.
            NotificationEvent(context, args);
            return XStudioApi.HRESULT.S_XSTUDIO_SUCCESS;
        }

        #endregion

        /// <summary>
        /// Event raised when a notification was received from the development console.
        /// </summary>
        public event NotificationEventHandler NotificationEvent;

        public void Dispose()
        {
            callbackHandle.Dispose();
            if (callbackDelegateHandle.IsAllocated)
                callbackDelegateHandle.Free();
        }

        /// <summary>
        /// Registers a new consumer callback for stream events from the development console.
        /// </summary>
        public static NotificationCallback Register(object context, StreamType streamTypes, Notification notifications)
        {
            NotificationCallback callback = new NotificationCallback(context);

            IntPtr callbackHandle;
            XStudioApi.HRESULT hr = XStudioApi.XStudioRegisterNotificationCallback(
                callback.callbackDelegate,
                (XStudioApi.XSTUDIO_STREAM_FLAGS)streamTypes,
                (XStudioApi.XSTUDIO_NOTIF)notifications,
                0, // Reserved, pass zero.
                IntPtr.Zero,
                out callbackHandle);

            if (hr < 0)
                throw new ApiException("XStudioRegisterNotificationCallback", (int)hr);

            callback.callbackHandle = new XStudioHandle(callbackHandle);

            return callback;
        }
    }
}
