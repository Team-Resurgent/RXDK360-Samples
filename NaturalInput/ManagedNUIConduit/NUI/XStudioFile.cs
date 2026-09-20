#region File Information
//-----------------------------------------------------------------------------
// XStudioFile.cs
//
// Encapsulation of an XED file for XStudio.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;

namespace Microsoft.ATG.NUI
{
    public class XStudioFile : IDisposable
    {
        #region Private Members

        private IntPtr fileHandle = IntPtr.Zero;

        private StreamType mappedStreams = 0;

        private XStudioFile(IntPtr fileHandle)
        {
            this.fileHandle = fileHandle;
        }

        #endregion

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
                        fileHandle);

                    if (hr < 0)
                        throw new ApiException("XStudioUnmapStreams", (int)hr);

                    mappedStreams = mappedStreams & ~streamsToUnmap;
                }

                // Map, if necessary.
                if (streamsToMap != 0)
                {
                    hr = XStudioApi.XStudioMapStreams(
                        (XStudioApi.XSTUDIO_STREAM_FLAGS)streamsToMap,
                        fileHandle);

                    if (hr < 0)
                        throw new ApiException("XStudioMapStreams", (int)hr);

                    mappedStreams = mappedStreams | streamsToMap;
                }
            }
        }

        public void Dispose()
        {
            MappedStreams = StreamType.None;
            if (fileHandle != IntPtr.Zero)
            {
                XStudioApi.XStudioCloseFile(fileHandle);
                fileHandle = IntPtr.Zero;
            }
        }

        public static XStudioFile Create(string path, bool createAlways, StreamType streamTypes)
        {
            IntPtr fileHandle;
            XStudioApi.HRESULT hr = XStudioApi.XStudioCreateFile(
                path,
                XStudioApi.DESIRED_ACCESS.GENERIC_WRITE,
                createAlways ? XStudioApi.CREATION_DISPOSITION.CREATE_ALWAYS
                             : XStudioApi.CREATION_DISPOSITION.CREATE_NEW,
                (XStudioApi.XSTUDIO_STREAM_FLAGS)streamTypes,
                out fileHandle);

            if (hr < 0)
                throw new ApiException("XStudioCreateFile", (int)hr);

            return new XStudioFile(fileHandle);
        }

        public static XStudioFile Open(string path)
        {
            IntPtr fileHandle;
            XStudioApi.HRESULT hr = XStudioApi.XStudioCreateFile(
                path,
                XStudioApi.DESIRED_ACCESS.GENERIC_READ,
                XStudioApi.CREATION_DISPOSITION.OPEN_EXISTING,
                XStudioApi.XSTUDIO_STREAM_FLAGS.XSTUDIO_STREAM_FLAG_NONE,
                out fileHandle);

            if (hr < 0)
                throw new ApiException("XStudioCreateFile", (int)hr);

            return new XStudioFile(fileHandle);
        }
    }
}
