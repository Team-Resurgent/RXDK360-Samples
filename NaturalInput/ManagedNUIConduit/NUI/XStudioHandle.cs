#region File Information
//-----------------------------------------------------------------------------
// XStudioHandle.cs
//
// Encapsulation of a handle used by Xbox Studio APIs.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;

namespace Microsoft.ATG.NUI
{
    /// <summary>
    /// Encapsulation of a handle used by Xbox Studio APIs.
    /// </summary>
    public class XStudioHandle : IDisposable
    {
        public IntPtr Handle { get; private set; }

        public XStudioHandle(IntPtr handle)
        {
            Handle = handle;
        }

        public void Dispose()
        {
            if (Handle != IntPtr.Zero)
            {
                XStudioApi.XStudioCloseHandle(Handle);
                Handle = IntPtr.Zero;
            }
        }
    }
}
