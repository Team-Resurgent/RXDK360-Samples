#region File Information
//-----------------------------------------------------------------------------
// SkeletonStreamEventArgs.cs
//
// Class for Xbox Studio stream events containing skeletal data.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Drawing;
using System.Runtime.InteropServices;

namespace Microsoft.ATG.NUI
{
    /// <summary>
    /// Class for Xbox Studio stream events containing skeletal data.
    /// </summary>
    public class SkeletonStreamEventArgs : StreamEventArgs
    {
        public uint FrameNumber { get; private set; }
        public ulong TimeStamp { get; private set; }

        public SkeletonFrame FrameFlags { get; private set; }

        public Vector FloorClipPlane { get; private set; }
        public Vector NormalToGravity { get; private set; }

        public Skeleton[] SkeletonData { get; private set; }

        public SkeletonStreamEventArgs(uint eventIndex, ulong eventMicroseconds,
                                       uint frameNumber, ulong timeStamp,
                                       SkeletonFrame frameFlags,
                                       Vector floorClipPlane, Vector normalToGravity,
                                       Skeleton[] skeletonData)
            : base(StreamID.Skeleton, eventIndex, eventMicroseconds)
        {
            FrameNumber = frameNumber;
            TimeStamp = timeStamp;
            FrameFlags = frameFlags;
            FloorClipPlane = floorClipPlane;
            NormalToGravity = normalToGravity;
            SkeletonData = skeletonData;
        }
    }

    [Flags]
    public enum SkeletonFrame : uint
    {
        CameraMotion = XStudioApi.NUI_SKELETON_FRAME_FLAGS.NUI_SKELETON_FRAME_FLAG_CAMERA_MOTION,
        ExtrapolatedFloor = XStudioApi.NUI_SKELETON_FRAME_FLAGS.NUI_SKELETON_FRAME_FLAG_EXTRAPOLATED_FLOOR,
        UpperBodySkeleton = XStudioApi.NUI_SKELETON_FRAME_FLAGS.NUI_SKELETON_FRAME_FLAG_UPPER_BODY_SKELETON,
    }

    public struct Vector
    {
        public float X, Y, Z, W;

        public Vector(float x, float y, float z, float w)
        {
            X = x; Y = y; Z = z; W = w;
        }

        public Point ToPointInDepthImage()
        {
            if (Z <= float.Epsilon) return Point.Empty;
            return new Point((int)(160 + (X * XStudioApi.NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS) / Z),
                             (int)(120 - (Y * XStudioApi.NUI_CAMERA_DEPTH_NOMINAL_FOCAL_LENGTH_IN_PIXELS) / Z));
        }

        public Point ToPointInColorImage()
        {
            if (Z <= float.Epsilon) return Point.Empty;
            return new Point((int)(320 + (X * XStudioApi.NUI_CAMERA_COLOR_NOMINAL_FOCAL_LENGTH_IN_PIXELS) / Z),
                             (int)(240 - (Y * XStudioApi.NUI_CAMERA_COLOR_NOMINAL_FOCAL_LENGTH_IN_PIXELS) / Z));
        }
    }

    public class Skeleton
    {
        public SkeletonTrackingState TrackingState { get; set; }
        public uint TrackingID { get; set; }
        public uint EnrollmentIndex { get; set; }
        public uint UserIndex { get; set; }

        public Vector Position { get; set; }
        public SkeletonPosition[] SkeletonPositions { get; set; }

        public SkeletonQuality Quality { get; set; }

        public Skeleton(SkeletonTrackingState trackingState, uint trackingID, uint enrollmentIndex, uint userIndex,
                        Vector position, SkeletonPosition[] skeletonPositions, SkeletonQuality quality)
        {
            TrackingState = trackingState;
            TrackingID = trackingID;
            EnrollmentIndex = enrollmentIndex;
            UserIndex = userIndex;
            Position = position;
            SkeletonPositions = skeletonPositions;
            Quality = quality;
        }
    }

    public enum SkeletonTrackingState
    {
        NotTracked = XStudioApi.NUI_SKELETON_TRACKING_STATE.NUI_SKELETON_NOT_TRACKED,
        PositionOnly = XStudioApi.NUI_SKELETON_TRACKING_STATE.NUI_SKELETON_POSITION_ONLY,
        Tracked = XStudioApi.NUI_SKELETON_TRACKING_STATE.NUI_SKELETON_TRACKED,
    }

    [Flags]
    public enum SkeletonQuality : uint
    {
        ClippedRight = XStudioApi.NUI_SKELETON_QUALITY_FLAGS.NUI_SKELETON_QUALITY_CLIPPED_RIGHT,
        ClippedLeft = XStudioApi.NUI_SKELETON_QUALITY_FLAGS.NUI_SKELETON_QUALITY_CLIPPED_LEFT,
        ClippedTop = XStudioApi.NUI_SKELETON_QUALITY_FLAGS.NUI_SKELETON_QUALITY_CLIPPED_TOP,
        ClippedBottom = XStudioApi.NUI_SKELETON_QUALITY_FLAGS.NUI_SKELETON_QUALITY_CLIPPED_BOTTOM,
    }

    public struct SkeletonPosition
    {
        public Vector Position { get; set; }
        public PositionTrackingState TrackingState { get; set; } 
    }

    public enum PositionTrackingState
    {
        NotTracked = XStudioApi.NUI_SKELETON_POSITION_TRACKING_STATE.NUI_SKELETON_POSITION_NOT_TRACKED,
        Inferred = XStudioApi.NUI_SKELETON_POSITION_TRACKING_STATE.NUI_SKELETON_POSITION_INFERRED,
        Tracked = XStudioApi.NUI_SKELETON_POSITION_TRACKING_STATE.NUI_SKELETON_POSITION_TRACKED,
    }

    internal enum PositionIndex
    {
        HipCenter = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_HIP_CENTER,
        Spine = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_SPINE,
        ShoulderCenter = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_SHOULDER_CENTER,
        Head = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_HEAD,
        ShoulderLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_SHOULDER_LEFT,
        ElbowLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_ELBOW_LEFT,
        WristLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_WRIST_LEFT,
        HandLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_HAND_LEFT,
        ShoulderRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_SHOULDER_RIGHT,
        ElbowRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_ELBOW_RIGHT,
        WristRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_WRIST_RIGHT,
        HandRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_HAND_RIGHT,
        HipLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_HIP_LEFT,
        KneeLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_KNEE_LEFT,
        AnkleLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_ANKLE_LEFT,
        FootLeft = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_FOOT_LEFT,
        HipRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_HIP_RIGHT,
        KneeRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_KNEE_RIGHT,
        AnkleRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_ANKLE_RIGHT,
        FootRight = XStudioApi.NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_FOOT_RIGHT,
    }
}
