#region File Information
//-----------------------------------------------------------------------------
// ImageStreamEventArgs.cs
//
// Base class for Xbox Studio stream events containing image data.
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
    /// Base class for Xbox Studio stream events containing image data.
    /// </summary>
    public abstract class ImageStreamEventArgs : StreamEventArgs
    {
        public int Width { get; private set; }
        public int Height { get; private set; }
        public uint FrameNumber { get; private set; }
        public ulong TimeStamp { get; private set; }

        public ImageStreamEventArgs(StreamID streamID, uint eventIndex, ulong eventMicroseconds,
                                    int width, int height, uint frameNumber, ulong timeStamp)
            : base(streamID, eventIndex, eventMicroseconds)
        {
            Width = width;
            Height = height;
            FrameNumber = frameNumber;
            TimeStamp = timeStamp;
        }
    }

    /// <summary>
    /// Encapsulation of a stream event containing depth data.
    /// </summary>
    public class DepthStreamEventArgs : ImageStreamEventArgs
    {
        public short[] Data { get; private set; }

        public DepthStreamEventArgs(uint eventIndex, ulong eventMicroseconds,
                                    int width, int height, uint frameNumber, ulong timeStamp, short[] data)
            : base(StreamID.Depth, eventIndex, eventMicroseconds, width, height, frameNumber, timeStamp)
        {
            Data = data;
        }
    }

    /// <summary>
    /// Encapsulation of a stream event containing color data.
    /// </summary>
    public class ColorStreamEventArgs : ImageStreamEventArgs
    {
        public int[] Data { get; private set; }

        public ColorStreamEventArgs(uint eventIndex, ulong eventMicroseconds,
                                    int width, int height, uint frameNumber, ulong timeStamp, int[] data)
            : base(StreamID.Color, eventIndex, eventMicroseconds, width, height, frameNumber, timeStamp)
        {
            Data = data;
        }

        /// <summary>
        /// Copies the color data into a GDI+ bitmap object.
        /// </summary>
        public Bitmap ToBitmap()
        {
            Bitmap bitmap = new Bitmap(Width, Height);
            Rectangle rectangle = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
            BitmapData bitmapData = bitmap.LockBits(rectangle, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(Data, 0, bitmapData.Scan0, Data.Length);
            bitmap.UnlockBits(bitmapData);
            return bitmap;
        }
    }

    /// <summary>
    /// Encapsulation of a stream event containing player index data.
    /// </summary>
    public class PlayerIndexStreamEventArgs : ImageStreamEventArgs
    {
        public short[] Data { get; private set; }

        public PlayerIndexStreamEventArgs(uint eventIndex, ulong eventMicroseconds,
                                          int width, int height, uint frameNumber, ulong timeStamp, short[] data)
            : base(StreamID.PlayerIndex, eventIndex, eventMicroseconds, width, height, frameNumber, timeStamp)
        {
            Data = data;
        }
    }
}
