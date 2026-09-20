#region File Information
//-----------------------------------------------------------------------------
// ConsumerForm.cs
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;
using Microsoft.ATG.NUI;

namespace Microsoft.ATG.ManagedNUIConduit
{
    public partial class ConsumerForm : Form
    {
        private delegate void VoidDelegate();

        /// <summary>
        /// Updates the status text on the form.  Callable from non-UI threads.
        /// </summary>
        private void UpdateStatus(string message)
        {
            if (!statusLabel.IsHandleCreated)
                return;

            statusLabel.BeginInvoke(new VoidDelegate(delegate
            {
                statusLabel.Text = message;
            }));
        }

        public ConsumerForm()
        {
            InitializeComponent();
        }

        private void returnButton_Click(object sender, EventArgs e)
        {
            this.Hide();
        }

        private StreamConsumerCallback consumer;

        private void ConsumerForm_Load(object sender, EventArgs e)
        {
            // Use a worker thread to avoid stalling the UI thread.
            ThreadPool.QueueUserWorkItem(new WaitCallback(delegate
            {
                try
                {
                    UpdateStatus("Connecting to default console...");

                    XStudio.Connect(null, ConnectionType.Tcp);

                    StreamType streamTypes = StreamType.Color | StreamType.Depth | StreamType.PlayerIndex | StreamType.Skeleton;

                    consumer = StreamConsumerCallback.Register(null);
                    consumer.MappedStreams = streamTypes;
                    consumer.StreamEvent += new StreamEventHandler(consumer_StreamEvent);

                    XStudio.Start(consumer.MappedStreams);

                    UpdateStatus("Streaming live data from console.");
                }
                catch (NUI.ApiException ex)
                {
                    // Report the error to the user.
                    UpdateStatus(string.Concat("Error: ", ex.Message));
                }
            }));
        }

        private void ConsumerForm_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                // Clean up from using Xbox Studio.
                if (consumer != null)
                {
                    XStudio.Stop(consumer.MappedStreams);

                    consumer.Dispose();
                    consumer = null;
                }

                if (XStudio.IsConnected)
                    XStudio.Disconnect();
            }
            catch (NUI.ApiException)
            {
                // Ignore errors when shutting down.
            }
        }

        void consumer_StreamEvent(object context, StreamEventArgs args)
        {
            // NOTE: An application should keep processing to a minimum in the callback.
            //       Processor-intensive work should be done by worker threads.

            switch (args.StreamID)
            {
                case StreamID.Color:
                {
                    ColorStreamEventArgs colorArgs = args as ColorStreamEventArgs;
                    if (colorArgs != null)
                    {
                        recentColorData = colorArgs;
                        wasColorDataUpdated = true;
                        ThreadPool.QueueUserWorkItem(new WaitCallback(delegate
                        {
                            VisualizeColorAndSkeletonData();
                        }));
                    }
                }
                break;

                case StreamID.Depth:
                {
                    DepthStreamEventArgs depthArgs = args as DepthStreamEventArgs;
                    if (depthArgs != null)
                    {
                        recentDepthData = depthArgs;
                        wasDepthDataUpdated = true;
                        ThreadPool.QueueUserWorkItem(new WaitCallback(delegate
                        {
                            VisualizeDepthAndPlayerIndexData();
                        }));
                    }
                }
                break;

                case StreamID.PlayerIndex:
                {
                    PlayerIndexStreamEventArgs playerIndexArgs = args as PlayerIndexStreamEventArgs;
                    if (playerIndexArgs != null)
                    {
                        recentPlayerIndexData = playerIndexArgs;
                        wasPlayerIndexDataUpdated = true;
                        ThreadPool.QueueUserWorkItem(new WaitCallback(delegate
                        {
                            VisualizeDepthAndPlayerIndexData();
                        }));
                    }
                }
                break;

                case StreamID.Skeleton:
                {
                    SkeletonStreamEventArgs skeletonArgs = args as SkeletonStreamEventArgs;
                    if (skeletonArgs != null)
                    {
                        recentSkeletonData = skeletonArgs;
                        wasSkeletonUpdated = true;
                        ThreadPool.QueueUserWorkItem(new WaitCallback(delegate
                        {
                            VisualizeColorAndSkeletonData();
                        }));
                    }
                }
                break;
            }

        }

        /// <summary>
        /// This palette follows what nuiview.xex uses to colorize players.
        /// </summary>
        private Color[] playerIndexColors = new Color[]
        {
            Color.FromArgb(255, 255, 255),
            Color.FromArgb(240, 20, 20), 
            Color.FromArgb(20, 240, 20),
            Color.FromArgb(80, 240, 240),
            Color.FromArgb(240, 240, 80),
            Color.FromArgb(240, 80, 240),
            Color.FromArgb(120, 120, 240),
            Color.FromArgb(255, 255, 255),
        };

        private ColorStreamEventArgs recentColorData = null;
        private bool wasColorDataUpdated = false;
        private DepthStreamEventArgs recentDepthData = null;
        private bool wasDepthDataUpdated = false;
        private PlayerIndexStreamEventArgs recentPlayerIndexData = null;
        private bool wasPlayerIndexDataUpdated = false;
        private SkeletonStreamEventArgs recentSkeletonData = null;
        private bool wasSkeletonUpdated = false;

        /// <summary>
        /// Helper function for visualizing depth values.
        /// </summary>
        private int MapDepthToGrayscale(short depth)
        {
            // NOTE: Depth is measured in millimeters from 800mm to 4000mm.
            
            // Create a triangle wave with 400mm wide bands (3200mm range == 8 bands).
            int value = depth % 400;
            if (value >= 200) value = 400 - value;
            return value + 40;
        }

        /// <summary>
        /// Composite the depth and player index data into a single image.
        /// </summary>
        private void VisualizeDepthAndPlayerIndexData()
        {
            if (!wasDepthDataUpdated || !wasPlayerIndexDataUpdated)
                return;

            Debug.Assert((recentDepthData != null) && (recentPlayerIndexData != null));

            wasDepthDataUpdated = false;
            wasPlayerIndexDataUpdated = false;

            int[] colors = new int[recentDepthData.Data.Length];
            for (int i = 0; i < colors.Length; ++i)
            {
                short playerIndex = recentPlayerIndexData.Data[i];
                Color playerIndexColor = playerIndexColors[playerIndex];

                short depth = recentDepthData.Data[i];

                // Modulate the grayscale value by the color for the player index.
                int gray = MapDepthToGrayscale(depth);
                int red = (playerIndexColor.R * gray) >> 8;
                int green = (playerIndexColor.G * gray) >> 8;
                int blue = (playerIndexColor.B * gray) >> 8;
                Color color = (depth > 0) ? Color.FromArgb(red, green, blue) : Color.Black;

                colors[i] = color.ToArgb();
            }

            Bitmap bitmap = new Bitmap(recentDepthData.Width, recentDepthData.Height);
            Rectangle rectangle = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
            BitmapData bitmapData = bitmap.LockBits(rectangle, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
            Marshal.Copy(colors, 0, bitmapData.Scan0, colors.Length);
            bitmap.UnlockBits(bitmapData);

            depthPictureBox.BeginInvoke(new VoidDelegate(delegate()
            {
                depthPictureBox.Image = bitmap;
            }));
        }
        
        /// <summary>
        /// A connection between skeleton positions.
        /// </summary>
        private struct Bone
        {
            public int Index1, Index2;
            
            public Bone(int index1, int index2)
            {
                Index1 = index1;
                Index2 = index2;
            }
        }

        /// <summary>
        /// This lists the connections between skeleton positions.
        /// </summary>
        private Bone[] bones = new Bone[]
        {
            new Bone((int)PositionIndex.HipCenter, (int)PositionIndex.Spine),
            new Bone((int)PositionIndex.Spine, (int)PositionIndex.ShoulderCenter),
            new Bone((int)PositionIndex.ShoulderCenter, (int)PositionIndex.Head),
            new Bone((int)PositionIndex.ShoulderCenter, (int)PositionIndex.ShoulderLeft),
            new Bone((int)PositionIndex.ShoulderLeft, (int)PositionIndex.ElbowLeft),
            new Bone((int)PositionIndex.ElbowLeft, (int)PositionIndex.WristLeft),
            new Bone((int)PositionIndex.WristLeft, (int)PositionIndex.HandLeft),
            new Bone((int)PositionIndex.ShoulderCenter, (int)PositionIndex.ShoulderRight),
            new Bone((int)PositionIndex.ShoulderRight, (int)PositionIndex.ElbowRight),
            new Bone((int)PositionIndex.ElbowRight, (int)PositionIndex.WristRight),
            new Bone((int)PositionIndex.WristRight, (int)PositionIndex.HandRight),
            new Bone((int)PositionIndex.HipCenter, (int)PositionIndex.HipLeft),
            new Bone((int)PositionIndex.HipLeft, (int)PositionIndex.KneeLeft),
            new Bone((int)PositionIndex.KneeLeft, (int)PositionIndex.AnkleLeft),
            new Bone((int)PositionIndex.AnkleLeft, (int)PositionIndex.FootLeft),
            new Bone((int)PositionIndex.HipCenter, (int)PositionIndex.HipRight),
            new Bone((int)PositionIndex.HipRight, (int)PositionIndex.KneeRight),
            new Bone((int)PositionIndex.KneeRight, (int)PositionIndex.AnkleRight),
            new Bone((int)PositionIndex.AnkleRight, (int)PositionIndex.FootRight),
        };

        private Point[] MapSkeletonPositionToColorImage(SkeletonPosition[] positions)
        {
            Point[] points = new Point[positions.Length];
            for (int i = 0; i < points.Length; ++i)
                points[i] = positions[i].Position.ToPointInColorImage();

            return points;
        }

        private int positionRadius = 5;
        private int boneThickness = 3;

        /// <summary>
        /// Composite the color and skeleton data into a single image.
        /// </summary>
        private void VisualizeColorAndSkeletonData()
        {
            if (!wasColorDataUpdated || !wasSkeletonUpdated)
                return;

            Debug.Assert((recentColorData != null) && (recentSkeletonData != null));

            wasColorDataUpdated = false;
            wasSkeletonUpdated = false;

            Bitmap bitmap = recentColorData.ToBitmap();
            using (Graphics graphics = Graphics.FromImage(bitmap))
            {
                for (int i = 0; i < recentSkeletonData.SkeletonData.Length; ++i)
                {
                    Skeleton skeleton = recentSkeletonData.SkeletonData[i];
                    if (skeleton.TrackingState != SkeletonTrackingState.Tracked)
                        continue;

                    Pen bonePen = new Pen(playerIndexColors[i + 1], (float)boneThickness);
                    SkeletonPosition[] positions = skeleton.SkeletonPositions;
                    Point[] points = MapSkeletonPositionToColorImage(positions);
                    foreach (var bone in bones)
                    {
                        if ((positions[bone.Index1].TrackingState == PositionTrackingState.NotTracked) ||
                            (positions[bone.Index2].TrackingState == PositionTrackingState.NotTracked))
                            continue;

                        graphics.DrawLine(bonePen, points[bone.Index1], points[bone.Index2]);
                    }
                    bonePen.Dispose();

                    for (int j = 0; j < positions.Length; ++j)
                    {
                        if (positions[j].TrackingState == PositionTrackingState.NotTracked)
                            continue;

                        Rectangle rect = new Rectangle(
                            points[j].X - positionRadius, points[j].Y - positionRadius,
                            (positionRadius * 2) + 1, (positionRadius * 2) + 1);
                        
                        Brush brush = (positions[j].TrackingState == PositionTrackingState.Tracked)
                            ? Brushes.White : Brushes.Red;
                        graphics.FillEllipse(brush, rect);
                    }

                }
            }
            
            colorPictureBox.BeginInvoke(new VoidDelegate(delegate()
            {
                colorPictureBox.Image = bitmap;
            }));
        }
    }
}
