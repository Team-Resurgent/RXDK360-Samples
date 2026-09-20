#region File Information
//-----------------------------------------------------------------------------
// ApiException.cs
//
// Exception type for errors returned by Xbox Studio APIs.
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
    /// Exception type for errors returned by Xbox Studio APIs.
    /// </summary>
    class ApiException : COMException
    {
        /// <summary>
        /// The Xbox Studio API method that returned an error.
        /// </summary>
        public string Method { get; private set; }

        public ApiException(string method, int hresult)
            : base(FormatExceptionMessage(method, hresult), hresult)
        {
            Method = method;
        }

        #region Private Members

        /// <summary>
        /// Generates a text description of this exception.
        /// </summary>
        private static string FormatExceptionMessage(string method, int hresult)
        {
            return string.Format("{0} returned {1} (0x{2:X8})",
                method, LookupHResult(hresult), hresult);
        }

        /// <summary>
        /// Returns the name of an HRESULT code.
        /// </summary>
        private static string LookupHResult(int hresult)
        {
            if (!Enum.IsDefined(typeof(XStudioApi.HRESULT), hresult))
                return string.Empty;

            return Enum.GetName(typeof(XStudioApi.HRESULT), hresult);
        }

        #endregion
    }
}
