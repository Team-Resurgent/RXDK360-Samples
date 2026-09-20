#region File Information
//-----------------------------------------------------------------------------
// Program.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System;
using System.Collections.Generic;
using System.Windows.Forms;
using System.IO;

namespace Atg.Samples.xbMon
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Main m = new Main();
            Application.Run();

            // Clean up temporary files on exit.
            foreach (string f in Directory.GetFiles(Path.GetTempPath(), "*xbMon.*"))
            {
                try
                {
                    File.Delete(f);
                }
                catch { }
            }
            Application.Exit();
        }
    }
}
