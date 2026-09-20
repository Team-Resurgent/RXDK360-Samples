#region File Information
//-----------------------------------------------------------------------------
// StreamConsumerCallback.cs
//
// Encapsulation of a consumer of Xbox Studio stream events.
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
    /// Encapsulation of a consumer of Xbox Studio stream events.
    /// </summary>
    public class StreamConsumerCallback : IDisposable
    {
        #region Private Members

        private object context;

        private XStudioApi.XSTUDIO_STREAM_CALLBACK callbackDelegate;
        private GCHandle callbackDelegateHandle;
        private XStudioHandle callbackHandle;

        private StreamType mappedStreams = 0;

        private StreamConsumerCallback(object context)
        {
            this.context = context;
            callbackDelegate = StreamCallback;

            // Prevent the delegate from being collected.
            callbackDelegateHandle = GCHandle.Alloc(callbackDelegate);
        }

        private XStudioApi.HRESULT StreamCallback(ref XStudioApi.XSTUDIO_STREAM_EVENT streamEvent, IntPtr context)
        {
            // Do nothing, if there are no subscribers.
            if (StreamEvent == null)
                return XStudioApi.HRESULT.S_XSTUDIO_SUCCESS;

            StreamEventArgs args = null;
            switch (streamEvent.StreamID)
            {
                case XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUICAM_DEPTH:
                    args = HandleDepthStream(ref streamEvent, context);
                    break;

                case XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUICAM_COLOR:
                    args = HandleColorStream(ref streamEvent, context);
                    break;

                case XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_PLAYER_INDEX:
                    args = HandlePlayerIndexStream(ref streamEvent, context);
                    break;

                case XStudioApi.XSTUDIO_STREAM_ID.XSTUDIO_STREAM_ID_NUIAPI_SKELETON:
                    args = HandleSkeletonStream(ref streamEvent, context);
                    break;

                default:
                    // Stream type not handled in this sample.
                    throw new NotImplementedException();
            }

            // Raise the event.
            StreamEvent(context, args);
            return XStudioApi.HRESULT.S_XSTUDIO_SUCCESS;
        }

        private StreamEventArgs HandleColorStream(ref XStudioApi.XSTUDIO_STREAM_EVENT streamEvent, IntPtr context)
        {
            // Marshal the pvIndexBuffer field.
            object o = Marshal.PtrToStructure(streamEvent.pvIndexBuffer,
                typeof(XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA));
            XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA indexBuffer =
                (XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA)o;

            // Marshal the pvDataBuffer field.
            int[] data = new int[indexBuffer.Width * indexBuffer.Height];
            Marshal.Copy(streamEvent.pvDataBuffer, data, 0, data.Length);

            return new ColorStreamEventArgs(
                streamEvent.uEventIndex, streamEvent.uEventMicroseconds,
                indexBuffer.Width, indexBuffer.Height,
                indexBuffer.FrameNumber, indexBuffer.NuiTimeStamp, data);
        }

        private StreamEventArgs HandleDepthStream(ref XStudioApi.XSTUDIO_STREAM_EVENT streamEvent, IntPtr context)
        {
            // Marshal the pvIndexBuffer field.
            object o = Marshal.PtrToStructure(streamEvent.pvIndexBuffer,
                typeof(XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA));
            XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA indexBuffer =
                (XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA)o;

            // Marshal the pvDataBuffer field.
            short[] data = new short[indexBuffer.Width * indexBuffer.Height];
            Marshal.Copy(streamEvent.pvDataBuffer, data, 0, data.Length);

            // Shift depth data.
            for (int i = 0; i < data.Length; ++i)
                data[i] = (short)(data[i] >> XStudioApi.NUI_IMAGE_PLAYER_INDEX_SHIFT);

            return new DepthStreamEventArgs(
                streamEvent.uEventIndex, streamEvent.uEventMicroseconds,
                indexBuffer.Width, indexBuffer.Height,
                indexBuffer.FrameNumber, indexBuffer.NuiTimeStamp, data);
        }

        private StreamEventArgs HandlePlayerIndexStream(ref XStudioApi.XSTUDIO_STREAM_EVENT streamEvent, IntPtr context)
        {
            // Marshal the pvIndexBuffer field.
            object o = Marshal.PtrToStructure(streamEvent.pvIndexBuffer,
                typeof(XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA));
            XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA indexBuffer =
                (XStudioApi.XSTUDIO_NUICAM_IMAGE_INDEX_DATA)o;

            // Marshal the pvDataBuffer field.
            short[] data = new short[indexBuffer.Width * indexBuffer.Height];
            Marshal.Copy(streamEvent.pvDataBuffer, data, 0, data.Length);

            return new PlayerIndexStreamEventArgs(
                streamEvent.uEventIndex, streamEvent.uEventMicroseconds,
                indexBuffer.Width, indexBuffer.Height,
                indexBuffer.FrameNumber, indexBuffer.NuiTimeStamp, data);
        }

        private StreamEventArgs HandleSkeletonStream(ref XStudioApi.XSTUDIO_STREAM_EVENT streamEvent, IntPtr context)
        {
            // Marshal the pvDataBuffer field.
            object o = Marshal.PtrToStructure(streamEvent.pvDataBuffer,
                typeof(XStudioApi.NUI_SKELETON_FRAME));
            XStudioApi.NUI_SKELETON_FRAME frame = 
                (XStudioApi.NUI_SKELETON_FRAME)o;

            Skeleton[] skeletonData = new Skeleton[frame.SkeletonData.Length];
            for (int i = 0; i < skeletonData.Length; ++i)
            {
                XStudioApi.NUI_SKELETON_DATA data = frame.SkeletonData[i];

                SkeletonPosition[] skeletonPositions = new SkeletonPosition[data.SkeletonPositions.Length];
                for (int j = 0; j < skeletonPositions.Length; ++j)
                {
                    skeletonPositions[j].Position = Convert(data.SkeletonPositions[j]);
                    skeletonPositions[j].TrackingState = (PositionTrackingState)data.eSkeletonPositionTrackingState[j];
                }

                skeletonData[i] = new Skeleton(
                    (SkeletonTrackingState)data.eTrackingState,
                    data.dwTrackingID,
                    data.dwEnrollmentIndex,
                    data.dwUserIndex,
                    Convert(data.Position),
                    skeletonPositions,
                    (SkeletonQuality)data.dwQualityFlags);
            }

            return new SkeletonStreamEventArgs(
                streamEvent.uEventIndex, streamEvent.uEventMicroseconds,
                frame.dwFrameNumber, (ulong)frame.liTimeStamp,
                (SkeletonFrame)frame.dwFlags,
                Convert(frame.vFloorClipPlane),
                Convert(frame.vNormalToGravity),
                skeletonData);
        }

        private static Vector Convert(XStudioApi.XMVECTOR vec)
        {
            return new Vector(vec.x, vec.y, vec.z, vec.w);
        }

        #endregion

        /// <summary>
        /// Event raised when a stream event was received from the development console.
        /// </summary>
        public event StreamEventHandler StreamEvent;

        /// <summary>
        /// Gets or sets the streams that to receive from the development console.
        /// </summary>
        public StreamType MappedStreams
        {
            get
            {
                return mappedStreams;
            }

            set
            {
                // Do nothing, if the requested streams are already mapped.
                StreamType oldStreams = mappedStreams;
                StreamType newStreams = value;
                if (oldStreams == newStreams)
                    return;

                // Determine what to map or unmap.
                StreamType streamsToMap = newStreams & ~oldStreams;
                StreamType streamsToUnmap = oldStreams & ~newStreams;

                XStudioApi.HRESULT hr;

                // Unmap, if necessary.
                if (streamsToUnmap != 0)
                {
                    hr = XStudioApi.XStudioUnmapStreams(
                        (XStudioApi.XSTUDIO_STREAM_FLAGS)streamsToUnmap,
                        callbackHandle.Handle);
                    
                    if (hr < 0)
                        throw new ApiException("XStudioUnmapStreams", (int)hr);

                    mappedStreams = mappedStreams & ~streamsToUnmap;
                }

                // Map, if necessary.
                if (streamsToMap != 0)
                {
                    hr = XStudioApi.XStudioMapStreams(
                        (XStudioApi.XSTUDIO_STREAM_FLAGS)streamsToMap,
                        callbackHandle.Handle);
                    
                    if (hr < 0)
                        throw new ApiException("XStudioMapStreams", (int)hr);

                    mappedStreams = mappedStreams | streamsToMap;
                }
            }
        }

        public void Dispose()
        {
            MappedStreams = StreamType.None;
            callbackHandle.Dispose();
            if (callbackDelegateHandle.IsAllocated)
                callbackDelegateHandle.Free();
        }

        /// <summary>
        /// Registers a new consumer callback for stream events from the development console.
        /// </summary>
        public static StreamConsumerCallback Register(object context)
        {
            StreamConsumerCallback callback = new StreamConsumerCallback(context);

            IntPtr callbackHandle;
            XStudioApi.HRESULT hr = XStudioApi.XStudioRegisterStreamCallback(
                callback.callbackDelegate,
                XStudioApi.XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG.XSTUDIO_STREAM_CALLBACK_REGISTRATION_FLAG_CONSUMER,
                IntPtr.Zero,
                out callbackHandle);
            
            if (hr < 0)
                throw new ApiException("XStudioRegisterStreamCallback", (int)hr);
            
            callback.callbackHandle = new XStudioHandle(callbackHandle);

            return callback;
        }
    }
}
