using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using System.IO;
using System.Threading;
using System.Diagnostics;

namespace Atg.Samples.XedFileTagger
{
    public partial class BatchConvertForm : Form
    {       
        public BatchConvertForm()
        {
            InitializeComponent();
        }

        private void button1_Click(object sender, EventArgs e)
        {
            FolderBrowserDialog folderDlg = new FolderBrowserDialog();
            folderDlg.ShowNewFolderButton = false;
            folderDlg.Description = "All .xed files in the selected folder will be converted to .vgbclip files which can be used in the VisualGestureBuilder tool.";
            DialogResult result = folderDlg.ShowDialog();

            if (result == DialogResult.OK)
            {
                try
                {
                    string arguments = @"" + folderDlg.SelectedPath + ( checkBoxOverwriteFiles.Checked ? " true" : " false" ) + "" ;
                    string path = Environment.GetEnvironmentVariable("XEDK") + "\\Source\\Samples\\NaturalInput\\XedFileTagger\\XedFileTagger2GestureBuilder.exe";
                    if (!File.Exists(path))
                    {
                        path = "XedFileTagger2GestureBuilder.exe";
                        if (!File.Exists(path))
                        {
                            MessageBox.Show("Could not find XedFileTagger2GestureBuilder.exe");
                            return;
                        }
                    }

                    // Run process to do the conversion
                    Process process = new Process();
                    process.StartInfo.FileName = path;
                    process.StartInfo.Arguments = arguments;
                    process.StartInfo.CreateNoWindow = false;
                    process.StartInfo.RedirectStandardOutput = false;
                    process.StartInfo.UseShellExecute = true;
                    process.Start();
                }
                catch
                {
                }
            }
        }
    }
}
