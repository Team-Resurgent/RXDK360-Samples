#region File Information
//-----------------------------------------------------------------------------
// Console.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#endregion

using System.Runtime.InteropServices;
using System;
using System.Collections.Generic;
using System.Text;
using XDevkit;
using System.Drawing;
using System.IO;
using System.Threading;
using Microsoft.Win32;

namespace Atg.Samples.xbMon
{
    public class XboxConsole : IDisposable
    {
        #region Private Variables

        /// <summary>
        /// Private copy of the XBDM XboxConsole class
        /// </summary>
        private XDevkit.IXboxConsole console = null;

        /// <summary>
        /// Name stores the user-typed method of accessing the console (IP or domain name.
        /// </summary>
        private string name = string.Empty;
        
        /// <summary>
        /// ConsoleName stores the friendly name of the console.
        /// </summary>
        private string consoleFriendlyName = string.Empty;

        /// <summary>
        /// Stores the ip address of the console.
        /// </summary>
        private System.Net.IPAddress consoleIPAddress = null;

        /// <summary>
        /// Stores if this console is the default console.
        /// </summary>
        /// <remarks>This is updated every HeartbeatInterval milliseconds</remarks>
        private bool isDefaultConsole = false;

        /// <summary>
        /// Stores the currently running program on the console.
        /// </summary>
        private string currentProgram = string.Empty;

        private string defaultConsole = string.Empty;
        private DateTime defaultConsoleLastPopulated = new DateTime(0);

        private bool isLocked = false;

        private double screenshotIndex = 0;

        private ConsoleMonitor consoleMonitor = null;
        
        private delegate void ExecuteCodeDelegate();

        private bool isDisposed = false;

        private enum XBDMCodes : int
        {
            Success = 0x2da0000,
            Error = unchecked((Int32)0x82da0000),
            NoError = (int)XBDMCodes.Success,
            CannotAccess = Error | 0x00E,
            NoSuchFile = Error | 0x002,
            NoThread = Error | 0x005,
            NoSuchPath = Error | 0x01E,
            CannotConnect = Error | 0x100,
            ConnectionLost = Error| 0x101,
            NoXboxName = Error | 0x108
        }

        /// <summary>
        /// Stores if the console is currently connected
        /// </summary>
        /// <remarks>This will be null if there has not yet been an attempt to contact the console.</remarks>
        private bool? isConnected = null;

        /// <summary>
        /// Stores the date and time at which the next screenshot should occur.
        /// </summary>
        private DateTime nextScreenshot = DateTime.Now;

        public delegate void XboxConnectionEventHandler(object source, bool Connected);
        public event XboxConnectionEventHandler OnConnectionChange;

        public delegate void RunningProgramChangedEventHandler(object source, string NewProgramTitle);
        public event RunningProgramChangedEventHandler OnRunningProgramChanged;

        public delegate void ScreenshotAcquiredEventHandler(Image Screenshot);
        public event ScreenshotAcquiredEventHandler OnScreenshotAcquired;

        private ManualResetEvent stopHeartbeat = new ManualResetEvent(false);

        private bool isVisible = true;

        private Thread heartbeatThread = null;

        //Used for RunConsoleCode
        delegate void VoidDelegate();

        #endregion

        #region Constants
        /// <summary>
        /// Check our connection to the console every n milliseconds
        /// </summary>
        private const int HeartbeatInterval = 1000;

        /// <summary>
        /// Take a screenshot every N milliseconds
        /// </summary>
        private const int ScreenshotInterval = 10000;

        /// <summary>
        /// Wait 7 seconds after reboot before taking 1st screenshot
        /// </summary>
        private const int ScreenshotRebootDelay = 7000;

        /// <summary>
        /// Default to retrying any console command 3 times before failing.
        /// </summary>
        private const int DefaultConsoleCodeRetry = 3;

        /// <summary>
        /// Depth of folders to explore when retrieving executables.
        /// </summary>
        private const int FolderExplorationDepth = 3;

        /// <summary>
        /// Number of milliseconds to wait for requests to return.
        /// </summary>
        private const int ConsoleTimeout = 1000;

        /// <summary>
        /// Constant for determining when the screenshot timer is disabled.
        /// </summary>
        private DateTime ScreenshotTimerDisabled = new DateTime(0);
        #endregion
        
        #region Public Properties

        /// <summary>
        /// Gets if this console is the current console. 
        /// This value is cached for 5 seconds to prevent spamming the XboxManagerClass.
        /// </summary>
        public bool IsDefaultConsole
        {
            get
            {
                return isDefaultConsole;
            }
        }

        /// <summary>
        /// Gets the current program executing on the console.
        /// </summary>
        public string CurrentProgram
        {
            [System.Diagnostics.DebuggerStepThrough]
            get
            {
                //Populated via 
                return currentProgram;
            }
        }

        /// <summary>
        /// Gets if the console is currently secured and unavailable to this machine.
        /// </summary>
        public bool Locked
        {
            [System.Diagnostics.DebuggerStepThrough]
            get
            {
                return isLocked;
            }
        }

        /// <summary>
        /// Gets a string representation of the IP address of the console
        /// </summary>
        public string IP
        {
            [System.Diagnostics.DebuggerStepThrough]
            get
            {
                if (Connected && consoleIPAddress != null)
                    return consoleIPAddress.ToString();
                else
                    return string.Empty;
            }
        }
        
        public bool MonitorVisible
        {
            get { return isVisible; }
            set { isVisible = value; }
        }

        /// <summary>
        /// Gets the connectivity state of the console. 
        /// Updated every HeartbeatInterval milliseconds.
        /// </summary>
        public bool Connected
        {
            [System.Diagnostics.DebuggerStepThrough]
            get { return (isConnected == true); }
        }

        /// <summary>
        /// Gets the friendly name of the console, or the user-typed name if the console is not connected.
        /// </summary>
        public string ConsoleFriendlyName
        {
            [System.Diagnostics.DebuggerStepThrough]
            get
            {
                //Return the cached console name if we have it
                if (!string.IsNullOrEmpty(consoleFriendlyName))
                {
                    return consoleFriendlyName;
                }
                //Return the user-typed name if we aren't currently connected.
                else
                {
                    return name;
                }
            }
        }

        #endregion

        /// <summary>
        /// Instantiates the XboxConsole wrapper class. You must call Connect() before fields are populated and events called.
        /// </summary>
        /// <param name="XBM">The XboxManager class to connect with.</param>
        /// <param name="Name">The name or IP of the console to connect to.</param>
        public XboxConsole(XboxManager XBM, string Name)
        {
            name = Name;
            
            OnConnectionChange += new XboxConnectionEventHandler(XboxConsole_OnConnectionChange);

            consoleMonitor = new ConsoleMonitor(this);

            this.screenshotIndex = new Random().NextDouble();

            LoadSettings();
        }

        /// <summary>
        /// Creates a new XboxManagerClass and begins communications with the console
        /// </summary>
        public void Connect()
        {
            if (this.console != null)
                Disconnect(false);

            isConnected = null;

            stopHeartbeat.Reset();

            XboxManagerClass XBM = new XboxManagerClass();
            console = XBM.OpenConsole(name);
            console.Shared = true;
            console.ConnectTimeout = ConsoleTimeout;
            console.ConversationTimeout = ConsoleTimeout;

            heartbeatThread = new Thread(new ThreadStart(Heartbeat));
            heartbeatThread.Start();
        }

        public void Disconnect(bool ensureThreadExit)
        {
            // Signal the heartbeat thread to halt:
            stopHeartbeat.Set();

            // If specified, we'll wait for 3 seconds before calling
            // thread.Abort to force the exit.
            if (ensureThreadExit)
            {
                if (!heartbeatThread.Join(3000))
                {
                    heartbeatThread.Abort();
                }
            }
        }

        /// <summary>
        /// Gets the Windows Form that displays the screenshot.
        /// </summary>
        public ConsoleMonitor ConsoleMonitor
        {
            [System.Diagnostics.DebuggerStepThrough]
            get
            {
                return consoleMonitor;
            }
        }

        /// <summary>
        /// We watch our ConnectionChanged event, and handle it here.
        /// </summary>
        private void XboxConsole_OnConnectionChange(object source, bool Connected)
        {

            //If we have not yet connected, our private variable isConnected will be null
            bool isConsoleClassInitializing = (isConnected == null);

            //Store the new connection state.
            isConnected = Connected;

            if (Connected)
            {
                //Populate IP, friendly name
                XBDMCodes Result = RunConsoleCode(delegate
                {
                    consoleFriendlyName = console.Name;
                    uint IP = console.IPAddress;

                    //Reverse the endian IP address
                    IP = (uint)((((IP & 0x000000FF) << 24)
                                | ((IP & 0x0000FF00) << 8)
                                | ((IP & 0x00FF0000) >> 8)
                                | ((IP & 0xFF000000) >> 24))); 

                    consoleIPAddress = new System.Net.IPAddress(IP);
                }
                );

                //Refresh the console preview window with new state
                if (Result == XBDMCodes.Success && consoleMonitor.Created && consoleMonitor.Visible)
                {
                    //Take a screenshot now that it's online, right away if this
                    //is our first connection notification, otherwise assume
                    //it's rebooting and wait a few seconds.
                    StartScreenshotTimer(isConsoleClassInitializing ? 0 : ScreenshotRebootDelay);
                }
            }
            else
            {
                StopScreenshotTimer();
                OnScreenshotAcquired(xbMon.Resources.Resources.Noise);
                consoleIPAddress = null;
            }

        }

        /// <summary>
        /// Calls a function and wraps with common XBDM error handling. Defaults to 3 retries.
        /// </summary>
        /// <param name="Code">The function to execute.</param>
        /// <returns>The error condition. Can be XBDMCodes.Success</returns>
        private XBDMCodes RunConsoleCode(VoidDelegate Code)
        {
            return RunConsoleCode(DefaultConsoleCodeRetry, Code);
        }
        /// <summary>
        /// Calls a function and wraps with common XBDM error handling.
        /// </summary>
        /// <param name="RetryCount">The number of times to retry calling the function before returning the error condition.</param>
        /// <param name="Code">The function to execute.</param>
        /// <returns>The error condition. Can be XBDMCodes.Success</returns>
        private XBDMCodes RunConsoleCode(int RetryCount, VoidDelegate Code)
        {
            System.Diagnostics.Debug.Assert(RetryCount > 0, "RunConsoleCode recieved a non-positive retry count");

            //Return an error if this class is disposed. 
            //This can happen if some threads have not yet cleaned up.
            if (isDisposed)
                return XBDMCodes.Error;


            //We will retry N times
            for (int n = 1; n <= RetryCount; n++)
            {
                try
                {
                    //Call the function passed to us
                    Code();

                    //We check again for disposal in the event we disposed
                    //while waiting for a particularly long request.
                    if (isDisposed)
                        return XBDMCodes.Error;
                    else
                        //If no exception occurred, return success.
                        return XBDMCodes.Success;
                }
                catch (COMException comException)
                {
                    //We check again for disposal in the event we disposed
                    //while waiting for a particularly long request.
                    if (isDisposed)
                        return XBDMCodes.Error;

                    //Sleep for a moment, then retry
                    if (n < RetryCount)
                        System.Threading.Thread.Sleep(10);

                    else //On the nth attempt...
                    {
                        //On connectivity loss...
                        if ((comException.ErrorCode == (int)XBDMCodes.CannotConnect ||
                            comException.ErrorCode == (int)XBDMCodes.ConnectionLost))
                        {
                            //Raise the event if the connection state has changed.
                            if (Connected || isConnected == null)
                                OnConnectionChange(this, false);
                            return (XBDMCodes)comException.ErrorCode;
                        }

                        //Return the error code if it's a known one, to be handled by caller
                        else if (comException.ErrorCode == (int)XBDMCodes.CannotAccess ||
                            comException.ErrorCode == (int)XBDMCodes.NoXboxName ||
                            comException.ErrorCode == (int)XBDMCodes.NoThread ||
                            comException.ErrorCode == (int)XBDMCodes.NoSuchPath ||
                            comException.ErrorCode == (int)XBDMCodes.NoSuchFile)
                            return (XBDMCodes)comException.ErrorCode;

                        //Return unknown errors as generic Error
                        else 
                        {
                            System.Diagnostics.Debug.WriteLine(string.Format("Unhandled Excption: {0}", comException.Message));
                            return XBDMCodes.Error;
                        }
                    }
                }
                //Handle the COM object becoming invalid during shutdown.
                catch (InvalidComObjectException)
                {
                    return XBDMCodes.Error;
                }
            }

            System.Diagnostics.Debug.Assert(false, "RunConsoleCode exited for loop prematurely");
            return XBDMCodes.Error;
        }

        /// <summary>
        /// Pings the console every HeartbeatInterval seconds to check for status change.
        /// </summary>
        /// <param name="State"></param>
        private void Heartbeat()
        {
            //Name the thread for easier debugging.
            Thread.CurrentThread.Name = string.Concat("Heartbeat: ", name);

            do
            {
                //Try to connect to the console twice; this way we can ignore a spurious disconnection message.
                XBDMCodes Result = RunConsoleCode(2, delegate
                {
                    bool NotifyProgramChange = false;

                    //Attempt to connect to the console.
                    console.FindConsole(0, 0);

                    //Retrieve the currently running program.
                    string CurrentProgram = console.RunningProcessInfo.ProgramName;

                    //Fire an event of the program name has changed.
                    //This allows the UI to update only when necessary.
                    if (currentProgram != System.IO.Path.GetFileNameWithoutExtension(CurrentProgram))
                    {
                        currentProgram = CurrentProgram;
                        if (!string.IsNullOrEmpty(currentProgram))
                            currentProgram = System.IO.Path.GetFileNameWithoutExtension(currentProgram);
                        {
                            NotifyProgramChange = true;
                        }
                    }

                    //Refresh the default console every 5 seconds, so as
                    //not to constantly shell out to COM as we redraw.
                    if (DateTime.Now.Subtract(defaultConsoleLastPopulated).TotalSeconds > 5)
                    {
                        //We have to reference a fresh Manager, as the console specific
                        //console.XboxManager doesn't keep track of this information.
                        defaultConsole = new XboxManagerClass().DefaultConsole;
                        defaultConsoleLastPopulated = DateTime.Now;
                    }
                    bool nowDefaultConsole = (string.Compare(defaultConsole, ConsoleFriendlyName, true) == 0 ||
                            string.Compare(defaultConsole, IP, true) == 0);

                    if (nowDefaultConsole != isDefaultConsole)
                    {
                        isDefaultConsole = nowDefaultConsole;
                        NotifyProgramChange = true;                        
                    }
                    if (OnRunningProgramChanged != null && NotifyProgramChange)
                        OnRunningProgramChanged(this, currentProgram);
              
                });

                //Show console as locked if we can't access the console's current program.
                isLocked = (Result == XBDMCodes.CannotAccess);

                //Fire the connection change event if FindConsole was a success, 
                //and we are not currently connected.
                if ((Result == XBDMCodes.Success || isLocked) && !Connected)
                    OnConnectionChange(this, true);

                // Grab a screenshot if it's time.
                if (nextScreenshot != ScreenshotTimerDisabled &&
                    nextScreenshot.CompareTo(DateTime.Now) <= 0)
                {
                    AcquireScreenshot();

                    //Reset the timer so that the next screenshot will occur at the correct time.
                    StartScreenshotTimer(ScreenshotInterval);
                }


            } while (!stopHeartbeat.WaitOne(HeartbeatInterval,false));
        }

        /// <summary>
        /// Starts the screenshot timer immediately.
        /// </summary>
        public void StartScreenshotTimer()
        {
            nextScreenshot = DateTime.Now;
        }

        /// <summary>
        /// Starts the screenshot timer with a delay.
        /// </summary>
        /// <param name="DelayInMs">Delay to wait before taking the first screenshot.</param>
        public void StartScreenshotTimer(int DelayInMs)
        {
            nextScreenshot = DateTime.Now.AddMilliseconds(DelayInMs);
        }

        /// <summary>
        /// Stops the screenshot timer
        /// </summary>
        public void StopScreenshotTimer()
        {
            nextScreenshot = ScreenshotTimerDisabled;
        }
        
        /// <summary>
        /// Starts the screenshot timer, and shows the monitoring window.
        /// </summary>
        public void ShowScreenshotWindow()
        {
            StartScreenshotTimer();
            consoleMonitor.Show();
        }

        /// <summary>
        /// Attempts to take a screenshot,
        /// and fires OnScreenshotAcquired if successful
        /// </summary>
        public void AcquireScreenshot()
        {
            Image Screenshot = null;
            bool ScreenshotSuccess= false;
            string tempFile = string.Empty;

            //Don't try grabbing more than one frame at a time for this console.
            lock (this)
            {
                if (Connected)
                {
                    //Attempt to retrieve a screenshot
                    XBDMCodes Result = RunConsoleCode(delegate
                    {
                        try
                        {
                            // Create a temporary file name. We don't use Path.GetTempFileName
                            // as that can conflict with other applicatons.
                            tempFile = Path.Combine(Path.GetTempPath(), screenshotIndex.ToString());
                            
                            // ScreenshotIndex, initialized to a random double to prevent cross-thread
                            // file conflicts, is incremented so a new filename will be retrieved next time
                            screenshotIndex++;
                            tempFile = string.Concat(tempFile,".", ConsoleFriendlyName, ".xbMon.bmp");
                            File.Delete(tempFile);
                            console.ScreenShot(tempFile);
                            ScreenshotSuccess = true;
                        }
                        catch (System.IO.IOException)
                        {
                            //There was an error with the temp directories. 
                            //Fail quietly rather than repeatedly the user.
                            return;
                        }
                    });
                }

                Stream tempFileStream = null;
                try
                {
                    if (ScreenshotSuccess)
                    {
                        //Open the image using a stream. 
                        //This way don't lock the file and can delete it once we've read it.
                        tempFileStream = File.Open(tempFile, FileMode.Open);

                        Screenshot = Image.FromStream(tempFileStream);

                    }
                    else
                    {
                        //Display the "noise" screenshot if the console is non-responsive.
                        Screenshot = xbMon.Resources.Resources.Noise;
                    }

                    //Fire the event that a screenshot has been acquired.
                    if (OnScreenshotAcquired != null)
                        OnScreenshotAcquired(Screenshot);

                }

                //If the asynchronous execution failed, continue quietly.
                catch (System.ComponentModel.InvalidAsynchronousStateException) { }

                //Ignore exceptons when thread shutdown occurrs.
                catch (System.Threading.ThreadAbortException) { }

                catch(Exception ex)
                {
                    // Hide errors that occur here from the user, as they are not actionable.
                    // Otherwise, the thread would die and we would lose connection with the console
                    System.Diagnostics.Debug.WriteLine(string.Format("Unhandled Excption: {0}", ex.Message));
                }

                finally
                {
                    //Attempt to close and delete the temporary file no matter what.
                    try
                    {
                        if (tempFileStream != null)
                        {
                            tempFileStream.Close();
                        }
                        File.Delete(tempFile);
                    }
                    catch { }
                }
            }
        }

        /// <summary>
        /// Gets a list of executables on the console, exploring 3 folders deep.
        /// </summary>
        public List<string> Executables
        {
            get
            {
                List<string> XEXs = new List<string>();

                XEXs.AddRange(FindFilesInDirectory("DEVKIT:\\", FolderExplorationDepth));
                XEXs.AddRange(FindFilesInDirectory("GAME:\\", FolderExplorationDepth));
                XEXs.AddRange(FindFilesInDirectory("DVD:\\", FolderExplorationDepth));
                return XEXs;
            }
        }

        /// <summary>
        /// Recursively searches the console for executables, 
        /// </summary>
        /// <param name="Directory">The directory to search</param>
        /// <param name="Depth">The remaining depth to recursively search</param>
        /// <returns>The full pathname for the executables found</returns>
        private List<string> FindFilesInDirectory(string Directory, int Depth)
        {
            List<string> XEXs = new List<string>();

            XBDMCodes Result = RunConsoleCode(3,delegate
            {
                IXboxFiles files = console.DirectoryFiles(Directory);
                for (int i = 0; i < files.Count; i++)
                {
                    //Find files in this enumerated location if it is a directory
                    //And we haven't already gone as deep as requested.
                    if (Depth > 0 && files[i].IsDirectory)
                        XEXs.AddRange(FindFilesInDirectory(files[i].Name, Depth - 1));

                    //If this is an executable, add it to the list.
                    if (!files[i].IsDirectory &&
                        (files[i].Name.ToUpper().EndsWith("XEX") ||
                        files[i].Name.ToUpper().EndsWith("EXE")))
                        XEXs.Add(files[i].Name);
                }
            });

            //Return the list of files.
            //Note that we ignore the return Result; if there's an error,
            //we'll return the executables that we were able to retrieve.
            return XEXs;
        }

        /// <summary>
        /// Checks the console for existence of file 
        /// </summary>
        /// <param name="Directory">The full path to the file on the console</param>
        /// <returns>True if the file exists, otherwise False.</returns>
        public bool FileExists(string fullFilePath)
        {
            IXboxFile xf = null;
            XBDMCodes Result = RunConsoleCode(1,delegate
            {
                xf = console.GetFileObject(fullFilePath);
            });

            return (xf != null);
        }


        /// <summary>
        /// Gets a string array listing the drives currently available on the console.
        /// This property is not cached, and is synchronous.
        /// </summary>
        public string[] Drives
        {
            get
            {
                string Drives = string.Empty;

                //Determine if this console exists in th neighborhood.
                //If so, the root of this console can be browsed in Explorer.
                //Consoles outside the neighborhood cannot be browsed at the root level,
                //  but can be browsed at the drive level.
                XBDMCodes Return = RunConsoleCode(delegate
                {
                    foreach (string c in console.XboxManager.Consoles)
                    {
                        if (string.Compare(c, ConsoleFriendlyName, true) == 0 ||
                            string.Compare(c, IP, true) == 0)
                        {
                            if (!isLocked)
                                Drives = "Root,";
                            break;
                        }
                    }
                });

                //Return the drives if they are available...
                Return = RunConsoleCode(delegate{
                    if (!isLocked)
                        Drives = string.Concat(Drives,console.Drives);
                });

                if (Return == XBDMCodes.Success)
                    return Drives.Split(',');
                else
                    return null;
            }
        }

        /// <summary>
        /// Reboot the target console.
        /// </summary>
        /// <param name="XEXPath">String to the executable to launch; null for Launcher</param>
        /// <param name="ColdBoot">Completely reboot the console, or just launch the executable.</param>
        /// <param name="CommandLine">Command line to pass to the executable</param>
        /// <returns>True if the reboot succeeded, False otherwise</returns>
        public bool Reboot(string XEXPath, bool ColdBoot, string CommandLine)
        {
            XboxRebootFlags flags = 0;

            //The COM API expects null rather than empty strings
            if (string.IsNullOrEmpty(XEXPath))
                XEXPath = null; 
            if (string.IsNullOrEmpty(CommandLine))
                CommandLine = null;

            Disconnect(true);

            XBDMCodes Result = RunConsoleCode(delegate
            {
                string Response = string.Empty;

                //XboxRebootFlags.Warm is deprecated, and behaves as .Cold
                //Launch the selected executable.
                console.Reboot(XEXPath, 
                    null, 
                    CommandLine,
                    (ColdBoot ? XboxRebootFlags.Cold : 0) | flags);
            });

            Connect();
            if (Result == XBDMCodes.Success)
            {
                currentProgram = string.Empty;
                OnRunningProgramChanged(this, string.Empty);
                return true;
            }
            else if (Result == XBDMCodes.CannotAccess)
            { 
                //Console was locked; do nothing.
                return true;
            }
            else
            {
                return false;
            }
        }

        /// <summary>
        /// Launches the Explorer window to browse to the specified location
        /// </summary>
        /// <param name="Subdirectory">Path to browse to.</param>
        public void LaunchExplorer(string Subdirectory)
        {
            string consoleID = string.Empty;

            //Use the IP address if the user typed in the IP address,
            //or if the console name is blank.
            System.Net.IPAddress ip = null;
            if (System.Net.IPAddress.TryParse(name, out ip) ||
                string.IsNullOrEmpty(name))
                consoleID = IP;
            else
                consoleID = name;

            //Launch the console by calling Explorer with the Neighborhood plugin.
            System.Diagnostics.ProcessStartInfo StartInfo = new System.Diagnostics.ProcessStartInfo();
            StartInfo.Arguments = string.Concat(@"/e,/n,/root,::{57B2D0D9-32DD-476a-B663-C85D2EA24AA5}\", consoleID, "\\",Subdirectory);
            StartInfo.Arguments.TrimEnd('\\');
            StartInfo.FileName = System.IO.Path.Combine(System.Environment.ExpandEnvironmentVariables("%windir%"), "explorer.exe") ;
            StartInfo.UseShellExecute = false;
            System.Diagnostics.Process p = System.Diagnostics.Process.Start(StartInfo);
        }

        [System.Diagnostics.DebuggerStepThrough]
        public override string ToString()
        {
            return ConsoleFriendlyName;
        }

        public void Dispose()
        {
            if (!isDisposed)
            {
                isDisposed = true;
                Disconnect(false);
                SaveSettings();
                StopScreenshotTimer();
                ConsoleMonitor.Close();
                heartbeatThread.Abort();

                heartbeatThread = null;
                consoleMonitor = null;
            }
        }

        /// <summary>
        /// Saves the settings of the console to the registry.
        /// </summary>
        private void SaveSettings()
        {
            RegistryKey ConsoleRegistry = Registry.CurrentUser.CreateSubKey("Software\\Microsoft\\XenonSDK\\xbMon\\Consoles\\" + name);
            ConsoleRegistry.SetValue("MonitorVisible", consoleMonitor.Visible.ToString());
            ConsoleRegistry.SetValue("Visible", MonitorVisible.ToString());
            ConsoleRegistry.SetValue("Left", consoleMonitor.Left, RegistryValueKind.DWord);
            ConsoleRegistry.SetValue("Top", consoleMonitor.Top, RegistryValueKind.DWord);
            ConsoleRegistry.SetValue("Width", consoleMonitor.Width, RegistryValueKind.DWord);
            ConsoleRegistry.SetValue("Height", consoleMonitor.Height, RegistryValueKind.DWord);
            ConsoleRegistry.Close();
        }

        /// <summary>
        /// Load the settings of the console from the registry.
        /// </summary>
        private void LoadSettings()
        {
            RegistryKey ConsoleRegistry = Registry.CurrentUser.CreateSubKey("Software\\Microsoft\\XenonSDK\\xbMon\\Consoles\\" + name);
            consoleMonitor.Visible = bool.Parse(ConsoleRegistry.GetValue("MonitorVisible", "false").ToString());
            nextScreenshot = consoleMonitor.Visible ? DateTime.Now : ScreenshotTimerDisabled;
            MonitorVisible = bool.Parse(ConsoleRegistry.GetValue("Visible", "true").ToString());
            consoleMonitor.Left = int.Parse(ConsoleRegistry.GetValue("Left","0").ToString());
            consoleMonitor.Top = int.Parse(ConsoleRegistry.GetValue("Top","0").ToString());
            consoleMonitor.Width= int.Parse(ConsoleRegistry.GetValue("Width",consoleMonitor.Width.ToString()).ToString());
            consoleMonitor.Height = int.Parse(ConsoleRegistry.GetValue("Height",consoleMonitor.Height.ToString()).ToString());
            ConsoleRegistry.Close();
        }

    }
}
