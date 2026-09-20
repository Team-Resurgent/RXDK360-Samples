#region File Information
//-----------------------------------------------------------------------------
// Program.cs
//
// Program entry point.
//
// Microsoft Advanced Technology Group
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.IO;
using System.Windows.Forms;

namespace Microsoft.ATG.ManagedNUIConduit
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            ReferenceXedk();

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }

        /// <summary>
        /// Adds the Xbox 360 SDK binaries folder to the environment path.
        /// </summary>
        private static void ReferenceXedk()
        {
            // Get the folder location for binaries.
            string xedk = Environment.GetEnvironmentVariable("xedk");
            string xedkBins = Path.Combine(xedk, @"bin\win32");
            if (!Directory.Exists(xedkBins))
            {
                // Either the Xbox 360 SDK is not installed on the machine,
                // or the XEDK environment variable is invalid.
                return;
            }

            // Merge with the environment path.
            string path = Environment.GetEnvironmentVariable("path");
            path = string.Concat(xedkBins, ";", path);
            Environment.SetEnvironmentVariable("path", path);
        }
    }
}
