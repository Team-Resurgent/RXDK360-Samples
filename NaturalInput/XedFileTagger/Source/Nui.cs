//--------------------------------------------------------------------------------------
// Nui.cs
//
// Defines structures from NuiSkeleton.h, NuiIdentity.h and NuiImage.h in C#
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

using System;
using System.Runtime.InteropServices;
using XnaMath;


//--------------------------------------------------------------------------------------
// Define XMVector in c#
//--------------------------------------------------------------------------------------

namespace XnaMath
{
    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct XMVector
    {
        public float x;
        public float y;
        public float z;
        public float w;
    }
}


//--------------------------------------------------------------------------------------
// Definitions from NuiSkeleton.h, NuiIdentity.h and NuiImage.h
//--------------------------------------------------------------------------------------

namespace Nui
{    
    public static class Constants
    {
        public const Int32 NUI_SKELETON_COUNT = 6;
        public const Int32 NUI_SKELETON_MAX_TRACKED_COUNT = 2;
        public const Int32 NUI_SKELETON_INVALID_TRACKING_ID = 0;
    }

    public enum NUI_SKELETON_POSITION_INDEX : uint
    {
        NUI_SKELETON_POSITION_HIP_CENTER = 0,
        NUI_SKELETON_POSITION_SPINE,
        NUI_SKELETON_POSITION_SHOULDER_CENTER,
        NUI_SKELETON_POSITION_HEAD,
        NUI_SKELETON_POSITION_SHOULDER_LEFT,
        NUI_SKELETON_POSITION_ELBOW_LEFT,
        NUI_SKELETON_POSITION_WRIST_LEFT,
        NUI_SKELETON_POSITION_HAND_LEFT,
        NUI_SKELETON_POSITION_SHOULDER_RIGHT,
        NUI_SKELETON_POSITION_ELBOW_RIGHT,
        NUI_SKELETON_POSITION_WRIST_RIGHT,
        NUI_SKELETON_POSITION_HAND_RIGHT,
        NUI_SKELETON_POSITION_HIP_LEFT,
        NUI_SKELETON_POSITION_KNEE_LEFT,
        NUI_SKELETON_POSITION_ANKLE_LEFT,
        NUI_SKELETON_POSITION_FOOT_LEFT,
        NUI_SKELETON_POSITION_HIP_RIGHT,
        NUI_SKELETON_POSITION_KNEE_RIGHT,
        NUI_SKELETON_POSITION_ANKLE_RIGHT,
        NUI_SKELETON_POSITION_FOOT_RIGHT,
        NUI_SKELETON_POSITION_COUNT
    }

    public enum NUI_SKELETON_POSITION_TRACKING_STATE : uint
    {
        NUI_SKELETON_POSITION_NOT_TRACKED = 0,
        NUI_SKELETON_POSITION_INFERRED,
        NUI_SKELETON_POSITION_TRACKED
    }

    public enum NUI_SKELETON_TRACKING_STATE : uint
    {
        NUI_SKELETON_NOT_TRACKED = 0,
        NUI_SKELETON_POSITION_ONLY,
        NUI_SKELETON_TRACKED
    }

    public enum Quality : uint
    {
        NUI_SKELETON_QUALITY_CLIPPED_RIGHT = 0x00000001,
        NUI_SKELETON_QUALITY_CLIPPED_LEFT = 0x00000002,
        NUI_SKELETON_QUALITY_CLIPPED_TOP = 0x00000004,
        NUI_SKELETON_QUALITY_CLIPPED_BOTTOM = 0x00000008,
    }

    [StructLayout(LayoutKind.Explicit, Size=448)]
    public struct NUI_SKELETON_DATA
    {
        [FieldOffset(0)]
        public NUI_SKELETON_TRACKING_STATE eTrackingState;                  // 4 Bytes

        [FieldOffset(4)]
        public UInt32 dwTrackingID;                                         // 4 Bytes

        [FieldOffset(8)]
        public UInt32 dwEnrollmentIndex;                                    // 4 Bytes

        [FieldOffset(12)]
        public UInt32 dwUserIndex;                                          // 4 Bytes

        [FieldOffset(16)]
        public XMVector Position;                                           // 16 Bytes

        [FieldOffset(32)]
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = SkeletonPositionCount)]
        public XMVector[] SkeletonPositions;                                // 320 Bytes

        [FieldOffset(352)]
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = SkeletonPositionCount)]
        public NUI_SKELETON_POSITION_TRACKING_STATE[] eSkeletonPositionTrackingState;   // 80 Bytes

        [FieldOffset(432)]
        public UInt32 dwQualityFlags;                                       // 4 Bytes

        private const Int32 SkeletonPositionCount = (Int32)NUI_SKELETON_POSITION_INDEX.NUI_SKELETON_POSITION_COUNT;

        // The default constructor does not know how to allocate the right size structure, therefore
        // we have to create this new constructor which will dynamically allocate the correct amount
        // of memory for the internal arrays
        public NUI_SKELETON_DATA(Byte unused)
        {
            eTrackingState = NUI_SKELETON_TRACKING_STATE.NUI_SKELETON_NOT_TRACKED;
            dwTrackingID = 0;
            dwEnrollmentIndex = 0;
            dwUserIndex = 0;
            Position = new XMVector();
            SkeletonPositions = new XMVector[SkeletonPositionCount];
            eSkeletonPositionTrackingState = new NUI_SKELETON_POSITION_TRACKING_STATE[SkeletonPositionCount];
            dwQualityFlags = 0;

            for (UInt32 i = 0; i < SkeletonPositionCount; i++)
            {
                SkeletonPositions[i] = new XMVector();
                eSkeletonPositionTrackingState[i] = new NUI_SKELETON_POSITION_TRACKING_STATE();
            }
        }
    }

    [StructLayout(LayoutKind.Explicit, Size=2736)]    
    public struct NUI_SKELETON_FRAME
    {
        [FieldOffset(0)]
        public Int64 liTimeStamp;                                           // 8 Bytes

        [FieldOffset(8)]
        public UInt32 dwFrameNumber;                                        // 4 Bytes

        [FieldOffset(12)]
        public UInt32 dwFlags;                                              // 4 Bytes

        [FieldOffset(16)]
        public XMVector vFloorClipPlane;                                    // 16 Bytes

        [FieldOffset(32)]
        public XMVector vNormalToGravity;                                   // 16 Bytes

        [FieldOffset(48)]
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = SkeletonCount)]
        public NUI_SKELETON_DATA[] SkeletonData;                            // 2688 Bytes

        private const Int32 SkeletonCount = Constants.NUI_SKELETON_COUNT;

        // The default constructor does not know how to allocate the right size structure, therefore
        // we have to create this new constructor which will dynamically allocate the correct amount
        // of memory for the internal arrays
        public NUI_SKELETON_FRAME(Byte unused)
        {
            liTimeStamp = 0;
            dwFrameNumber = 0;
            dwFlags = 0;
            vFloorClipPlane = new XMVector();
            vNormalToGravity = new XMVector();
            SkeletonData = new NUI_SKELETON_DATA[SkeletonCount];

            for (UInt32 i = 0; i < SkeletonCount; i++)
            {
                SkeletonData[i] = new NUI_SKELETON_DATA(0);
            }
        }
    }
    
    public enum NUI_IDENTITY_MESSAGE_ID
    {
        NUI_IDENTITY_MESSAGE_ID_FRAME_PROCESSED = 0,
        NUI_IDENTITY_MESSAGE_ID_COMPLETE
    }

    public enum NUI_IDENTITY_OPERATION_ID
    {
        NUI_IDENTITY_OPERATION_ID_NONE = 0,
        NUI_IDENTITY_OPERATION_ID_IDENTIFY,
        NUI_IDENTITY_OPERATION_ID_ENROLL,
        NUI_IDENTITY_OPERATION_ID_TUNER
    }
    
    [StructLayout(LayoutKind.Explicit, Size = 20)]
    public struct NUI_IDENTITY_MESSAGE
    {
        [FieldOffset(0)]
        NUI_IDENTITY_MESSAGE_ID MessageId;                          // 4 Bytes

        [FieldOffset(4)]
        NUI_IDENTITY_OPERATION_ID OperationId;                      // 4 Bytes

        [FieldOffset(8)]
        UInt32 dwTrackinID;                                         // 4 Bytes

        [FieldOffset(12)]
        UInt32 dwSkeletonFrameNumber;                               // 4 Bytes

        [FieldOffset(16)]
        UInt32 Data;                                                // 4 Bytes
    }

    public enum NUI_IMAGE_TYPE : uint
    {
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX = 0,
        NUI_IMAGE_TYPE_COLOR,
        NUI_IMAGE_TYPE_COLOR_YUV,
        NUI_IMAGE_TYPE_DEPTH,
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_IN_COLOR_SPACE,
        NUI_IMAGE_TYPE_DEPTH_IN_COLOR_SPACE,
        NUI_IMAGE_TYPE_COLOR_IN_DEPTH_SPACE,
        NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX_80x60,
        NUI_IMAGE_TYPE_DEPTH_80x60
    }

}
