#region File Information
//-----------------------------------------------------------------------------
// RippleGenerator.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Drawing;
using System.Drawing.Drawing2D;

namespace Atg.Samples.xbMon
{
    public class RippleGenerator : IDisposable
    {
        private Thread updateRippleTimer = null;
        private List<Circle> Circles = new List<Circle>();
        private Image ripples = null;
        private Brush gradiatedBackground = null;
        private Color BackColor1 = Color.FromArgb(158, 188, 161); //Dark Green
        private Color BackColor2 = Color.FromArgb(208, 244, 211); //Light Green
        private Random random = new Random();
        DateTime LastAdded = DateTime.Now;
        private int UpdateDelay = 33;
        private bool disposed = false;

        public Image Ripples
        {
            get { return ripples; }
            set { ripples = value; }
        }
        private SizeF size;
        public SizeF Size
        {
            get { return size; }
            set
            {
                lock (Ripples)
                {
                    size = value;
                    Image newRipple = new Bitmap((int)size.Width, (int)size.Height);
                }
            }
        }
        public void Dispose()
        {
            disposed = true;
            updateRippleTimer.Abort();

            if (ripples != null)
            {
                ripples.Dispose();
                ripples = null;
            }
            foreach (Circle c in Circles)
            {
                c.Dispose();
            }
            Circles.Clear();
        }
        private PointF origin;

        public PointF Origin
        {
            get { return origin; }
            set { origin = value; }
        }
        public RippleGenerator(int UpdateDelay, SizeF Size, PointF Origin)
        {
            size = Size;
            origin = Origin;
            gradiatedBackground = new LinearGradientBrush(new PointF(0, 0), new PointF(Size.Width,Size.Height), BackColor1,BackColor2 );
            ripples = new Bitmap((int)Size.Width, (int)size.Height);
            //updateRippleTimer = new Timer(new TimerCallback(UpdateRipple), null, 0, UpdateDelay);
            this.UpdateDelay = UpdateDelay;
            updateRippleTimer = new Thread(new ThreadStart(UpdateRipple));
            updateRippleTimer.IsBackground = true;
            updateRippleTimer.Priority = ThreadPriority.Lowest;
        }
        public void Start()
        {
            updateRippleTimer.Start();
        }

        void UpdateRipple()
        {
            while (!disposed)
            {
                if (ripples != null)
                {
                    lock (ripples)
                    {
                        try
                        {
                            using (Graphics g = Graphics.FromImage(ripples))
                            {
                                g.FillRectangle(gradiatedBackground, 0, 0, Size.Width, Size.Height);

                                if (DateTime.Now.Subtract(LastAdded).TotalMilliseconds > random.Next(100, 1000))
                                {
                                    LastAdded = DateTime.Now;
                                    Circles.Add(new Circle());
                                }
                                if (Circles.Count > 100)
                                {
                                    Circles[0].Dispose();
                                    Circles.RemoveAt(0);
                                }
                                foreach (Circle c in Circles)
                                {
                                    c.Update();
                                    g.DrawEllipse(c.Pen, origin.X + c.Location.X, origin.Y + c.Location.Y, c.Size.Width, c.Size.Height);
                                }
                                g.Flush();
                            }
                        }
                        catch (Exception) { }
                    }
                }
                Thread.Sleep(UpdateDelay);
            }
        }
        private class Circle : IDisposable
        {
            public SizeF Size;
            public PointF Location;
            private Color color;
            private Pen pen;
            public float Speed = 1f;
            private float weight = 1f;
            private int LastMsUpdated = 0;
            private DateTime Created;
            public Circle()
            {
                Random random = new Random();
                float width = (float)random.NextDouble() * 5f;

                Created = DateTime.Now;
                Size = new SizeF(width, weight);
                Location = new PointF((float)((random.NextDouble() - .5d) * 20d), (float)((random.NextDouble() - .5d) * 20d));
                color = Color.White;
                Weight = (float)random.NextDouble() * 5f;
                Speed = (float)random.NextDouble() + .25f;
            }
            public void Dispose()
            {
                pen.Dispose();
            }
            public void Update()
            {
                Location.X -= Speed;
                Location.Y -= Speed;
                Size.Width += Speed * 2;
                Size.Height += Speed * 2;

                if (((int)DateTime.Now.Subtract(Created).TotalMilliseconds) > LastMsUpdated)
                {
                    LastMsUpdated += 333;
                    Weight += .03f;
                    Color = Color.FromArgb(Color.A <= 10 ? 10 : color.A - 5, Color);
                }
            }
            private void UpdateColorAndWeight(Color c, float w)
            {
                weight = w;
                color = c;
                if (pen != null)
                    pen.Dispose();
                pen = new Pen(color, weight);
            }
            public float Weight
            {
                get { return weight; }
                set { UpdateColorAndWeight(color, value); }
            }
            public Pen Pen
            {
                get { return pen; }
            }
            public Color Color
            {
                get { return color; }
                set { UpdateColorAndWeight(value, weight); }
            }
        }

    }
}