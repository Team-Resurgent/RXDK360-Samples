//--------------------------------------------------------------------------------------
// PDBViewer.xaml.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------


using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;

using System.IO;
using System.Linq;
using System.Text;

using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Diagnostics;

using Microsoft.Win32;

namespace PDBScope
{
    /// <summary>
    /// PDBScope displays user defined types found in a PDB file from either a XBox or Windows executable. The types
    /// can be sorted according to their name or size. When a type is selected the type's fields are displayed and
    /// all the functions that use the type directly are shown on the second tab. 
    /// 
    /// The program is broken up into three components, The visual display classes, the classes that maintain the lists of 
    /// types and functions, and finally the classes that support loading the  symbols found in the PDB.
    /// 
    /// The display has two tabs, the first tab is for types and the second is for functions. The first tab has two panels, 
    /// the top panel lists the types that are defined with the PDB. When a type is selected the fields are displayed in the
    /// second panel. The second tab also contains two panels, the first displays functions that use a type selected on the
    /// first tab. The second panel shows local variables when a function is selected.
    /// 
    /// Features:
    ///     Search: At the top of the panel there is a search box that matches the text entered against the primary column in the tab.
    ///             The string in the box is matched against the begining of the names, there are no wild characters. 
    ///             An editable combo box allows unique search entries to be added and maintains a history of 8 entries.
    ///              Note: the combo box automatically matches an existing string entered into the box making it difficult
    ///                    to modify an existing entry.
    ///             
    ///     Resizing: The window is resizable and the panels are separated by a bar that can be grabbed by the mouse. 
    ///               The separation is implemented using a grid splitter. A grid in the first tab has three rows containing
    ///               1) List of types, 2) splitter and 3) a secondary tab control for fileds and the functions the type is used
    ///               in.
    ///     
    ///     Sorting: The types can be sorted by name (ascending, descending) and by size (ascending, decsending). Instead of using
    ///              the sorting ability of the ListView object the items source are sorted using a Linq query. Even with 50,000 types
    ///              the list sorts extremely quickly. 
    ///     
    ///     
    ///     
    /// </summary>
    public partial class PDBViewer : Window
    {
        // Defines the array of ListViews that can have columns sorted.
        enum SortedDataTables
        {
            // Primary 5 lists must be first.
            TypeTable,
            GlobalDataTable,
            ConstantStaticDataTable,
            FileStaticDataTable,
            FunctionTable,
            TypeFunctionTable,
            TypeFunctionLocalsTable,
            FunctionLocalsTable,
            Size
        }
            
        /// <remark>
        /// The "TopLevelSymbolLists" TabControl (see XML file) contains a set of tabs. Each
        /// tab contains a primary list that can be filtered by a string
        /// and can be sorted on its columns. An array of ManagedListViews
        /// is used to maintain the data being displayed and the last column
        /// that was sorted on. The number of elements in the array equals
        /// the number TabItems in the TabControl. 
        /// </remark>
        class ManagedListViews
        {
            public ListView listView;
            public GridViewColumnHeader lastColumnHeader;
            public ColumnDirection lastDirection;
            public PDBTableIndex index;

            public ManagedListViews(ListView v, PDBTableIndex i)
            {
                listView = v;
                index = i;
                lastColumnHeader = null;
                lastDirection = ColumnDirection.NotSet;
            }
        }

        private string currentDirectory;
        private string filter;

        /// <remark>
        /// pdbFile suppies the PDB information displayed in the UI. Each grid has a table 
        /// that is used as its source of data to display
        /// </remark>
        PDBDataSource pdbFile;

        /// <remark>
        /// Array of primary lists that controls which data is displayed in which tab. 
        /// </remark>
        ManagedListViews[] tabViews;


        /// <remark>
        /// A background worker loads the PDB information and updates the progress bar on the UI. The
        /// COM objects from the DIA interface can only be accessed on the thread that they were created
        /// therefore the UI cannot directly access the DIA objects. 
        /// </remark>
        BackgroundWorker loadFileWorker;

        public PDBViewer()
        {
            InitializeComponent();

            currentDirectory = Directory.GetCurrentDirectory();

            loadFileWorker = new BackgroundWorker();
            loadFileWorker.DoWork += new System.ComponentModel.DoWorkEventHandler(BackgroundLoadFile_DoWork);
            loadFileWorker.RunWorkerCompleted += new System.ComponentModel.RunWorkerCompletedEventHandler(BackgroundLoadFile_DoWorkComplete);
            loadFileWorker.ProgressChanged += new ProgressChangedEventHandler(BackgroundLoadFile_ProgressChanged);
            loadFileWorker.WorkerReportsProgress = true;
            loadFileWorker.WorkerSupportsCancellation = true;

            /// <remark>
            /// The main tab displays "Types", "Global Data", "File Static Data",
            /// "Constant Static Data", and "Functions". In each tab item there is
            /// a ListView that has selectable columns that allow the view to be
            /// sorted by the values in a column.  The ListView's are also
            /// filtered. To keep track of the filters and sorted columns we keep
            /// an array views. There is is an implicit assumption that each
            /// ListView has a column discription object defined in PDBTable
            /// index. This is hard coded relationship.
            /// </remark>
            //
            // Populate the connection between the data referenced in the DBFile an the primary lists
            // in the outer most Tab Control. 
            tabViews = new ManagedListViews[(int)SortedDataTables.Size];

            tabViews[0] = new ManagedListViews(typeView, PDBTableIndex.TypeTable);
            tabViews[1] = new ManagedListViews(globalDataView, PDBTableIndex.GlobalDataTable);
            tabViews[2] = new ManagedListViews(fileStaticDataView, PDBTableIndex.FileStaticDataTable);
            tabViews[3] = new ManagedListViews(constantStaticDataView, PDBTableIndex.ConstantStaticDataTable);
            tabViews[4] = new ManagedListViews(functionView, PDBTableIndex.FunctionTable);
            tabViews[5] = new ManagedListViews(typeFunctionView, PDBTableIndex.TypeFunctionTable);
            tabViews[6] = new ManagedListViews(typeFunctionLocalsView, PDBTableIndex.TypeFunctionLocalsTable);
            tabViews[7] = new ManagedListViews(functionLocalsView, PDBTableIndex.FunctionLocalsTable);

            ClearGridReferences();
        }

        /// <summary>
        /// Creates GridViews connecting data properties in a List to 
        /// columns in a ListView control. The ListView control does
        /// not know what it is actually displaying in the list. The data
        /// content and control is supplied by the PDBFile object which
        /// understands the format of each table.
        /// </summary>
        /// <param name="table"></param>
        /// <returns></returns>
        internal GridView BuildView(PDBTableIndex table)
        {
            GridView gview = new GridView();
            gview.AllowsColumnReorder = true;
            gview.ColumnHeaderToolTip = "User Defined Type Information";

            ColumnData[] columnData = pdbFile.ColumnData(table);
            foreach (ColumnData c in columnData)
            {
                GridViewColumn gvc = new GridViewColumn();
                gvc.DisplayMemberBinding = new Binding(c.binding);
                gvc.Header = c.header;
                gvc.Width = c.width;
                gview.Columns.Add(gvc);
            }
            return gview;
        }

        /// <summary>
        /// When a file is loaded we null out the views and lists used to populated the grids. This
        /// will remove the displayed information. 
        /// </summary>
        private void ClearGridReferences()
        {
            typeFunctionLocalsView.View = null;
            typeFunctionLocalsView.ItemsSource = null;
            typeFunctionView.View = null;
            typeFunctionView.ItemsSource = null;
            typeView.View = null;
            typeView.ItemsSource = null;
            typeFieldView.View = null;
            typeFieldView.ItemsSource = null;
            globalDataView.View = null;
            globalDataView.ItemsSource = null;
            functionLocalsView.View = null;
            functionLocalsView.ItemsSource = null;
            functionView.View = null;
            functionView.ItemsSource = null;

            foreach (ManagedListViews l in tabViews)
            {
                if (l.lastColumnHeader != null)
                {
                    l.lastColumnHeader.Column.HeaderTemplate = null;
                    l.lastColumnHeader = null;
                }
                l.lastDirection = ColumnDirection.NotSet;
            }

            if (SearchCombo != null && SearchCombo.Items != null)
            {
                foreach (ComboBoxItem item in SearchCombo.Items)
                {
                    item.Content = null;
                }
            }

            pdbFile = null;
            filter = ""; // initialize to empty string so we always know that filter is a valid string
        }
     
        /// <summary>
        /// When a function is selected in the primary list for the "Function" Tab then the locals
        /// are displayed.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void myTypeFunctionChanged(object sender, SelectionChangedEventArgs args)
        {
            FunctionSymbol func = (FunctionSymbol)typeFunctionView.SelectedItem;
            if (func != null)
            {
                pdbFile.CreateFunctionLocalsList(func, PDBTableIndex.TypeFunctionLocalsTable);

                typeFunctionLocalsView.View = BuildView(PDBTableIndex.TypeFunctionLocalsTable); // object 
                typeFunctionLocalsView.ItemsSource = pdbFile.List(PDBTableIndex.TypeFunctionLocalsTable);
            }
            else
            {
                typeFunctionLocalsView.View = null;
                typeFunctionLocalsView.ItemsSource = null;
            }
        }
        
        /// <summary>
        /// When an item is selected in the "Types" Tab this callback is executed.
        /// The list of fields and the list of functions who contain the type are updated. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void myTypeChanged(object sender, SelectionChangedEventArgs args)
        {
            UDTSymbol selectedType = typeView.SelectedItem as UDTSymbol;

            typeFunctionView.View = null;
            typeFunctionView.ItemsSource = null;
            typeFieldView.View = null;
            typeFieldView.ItemsSource = null;

            if (selectedType != null)
            {
                pdbFile.CreateFunctionList(null, PDBTableIndex.TypeFunctionTable, PDBTableIndex.TypeFunctionLocalsTable,
                        selectedType);
                IList list = pdbFile.List(PDBTableIndex.TypeFunctionTable);
                if ((list != null) && (list.Count > 0))
                {
                    typeFunctionView.View = BuildView(PDBTableIndex.TypeFunctionTable);
                    typeFunctionView.ItemsSource = list;
                }

                typeFunctionLocalsView.View = null;
                typeFunctionLocalsView.ItemsSource = null;

                pdbFile.CreateTypeFieldList(PDBTableIndex.TypeFieldTable, selectedType);
                list = pdbFile.List(PDBTableIndex.TypeFieldTable);
                if ((list != null) && (list.Count > 0))
                {
                    typeFieldView.View = BuildView(PDBTableIndex.TypeFieldTable);
                    typeFieldView.ItemsSource = list;
                }
            }
        }
      
        /// <summary>
        /// When an item is selected in the "Global Data" Tab this call back is executed.
        /// The list of fields for the data type is displayed along with the list of functions where   
        /// the data item. Both lists that are displayed reflect the selected type. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void myGlobalDataChanged(object sender, SelectionChangedEventArgs args)
        {
            DataSymbol selectedData = globalDataView.SelectedItem as DataSymbol;

            globalDataFieldView.View = null;
            globalDataFieldView.ItemsSource = null;

            if (selectedData != null)
            {
                UDTSymbol selectedType = selectedData.MyType as UDTSymbol;
                IList list;
                if(selectedType != null)
                {
                    pdbFile.CreateTypeFieldList(PDBTableIndex.GlobalDataFieldTable, selectedType);
                    list = pdbFile.List(PDBTableIndex.GlobalDataFieldTable);
                    if ((list != null) && (list.Count > 0))
                    {
                        globalDataFieldView.View = BuildView(PDBTableIndex.GlobalDataFieldTable);
                        globalDataFieldView.ItemsSource = list;
                    }
                }
            }
        }

        /// <summary>
        /// When an item is selected in the "File Static Data" Tab this call back is executed.
        /// The list of fields for the data type is displayed along with the list of functions where   
        /// the data item is used are displayed reflect the selected type. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void myFileStaticDataChanged(object sender, SelectionChangedEventArgs args)
        {
            DataSymbol selectedData = fileStaticDataView.SelectedItem as DataSymbol;

            fileStaticDataFieldView.View = null;
            fileStaticDataFieldView.ItemsSource = null;

            if (selectedData != null)
            {
                UDTSymbol selectedType = selectedData.MyType as UDTSymbol;
                IList list;
                if (selectedType != null)
                {
                    pdbFile.CreateTypeFieldList(PDBTableIndex.FileStaticDataFieldTable, selectedType);
                    list = pdbFile.List(PDBTableIndex.FileStaticDataFieldTable);
                    if ((list != null) && (list.Count > 0))
                    {
                        fileStaticDataFieldView.View = BuildView(PDBTableIndex.FileStaticDataFieldTable);
                        fileStaticDataFieldView.ItemsSource = list;
                    }
                }
            }
        }

        /// <summary>
        /// When an item is selected in the "Constant Static Data" Tab this call back is executed.
        /// The list of fields for the data type is displayed along with the list of functions where   
        /// the data item is used are displayed reflect the selected type. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void myConstantStaticDataChanged(object sender, SelectionChangedEventArgs args)
        {
            DataSymbol selectedData = constantStaticDataView.SelectedItem as DataSymbol;

            constantStaticDataFieldView.View = null;
            constantStaticDataFieldView.ItemsSource = null;

            if (selectedData != null)
            {
                UDTSymbol selectedType = selectedData.MyType as UDTSymbol;
                IList list;
                if (selectedType != null)
                {
                    pdbFile.CreateTypeFieldList(PDBTableIndex.ConstantStaticDataFieldTable, selectedType);
                    list = pdbFile.List(PDBTableIndex.ConstantStaticDataFieldTable);
                    if ((list != null) && (list.Count > 0))
                    {
                        constantStaticDataFieldView.View = BuildView(PDBTableIndex.ConstantStaticDataFieldTable);
                        constantStaticDataFieldView.ItemsSource = list;
                    }
                }
            }
        }

        /// <summary>
        /// When an item is selected in the "Functions" Tab this call back is executed.
        /// The list of locals defined within the function are also display. Unfortunately, the   
        /// PDB does not reflect which global or file static data items are accessed with the function.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void myFunctionChanged(object sender, SelectionChangedEventArgs args)
        {
            FunctionSymbol func = (FunctionSymbol)functionView.SelectedItem;
            if (func != null)
            {
                pdbFile.CreateFunctionLocalsList(func, PDBTableIndex.FunctionLocalsTable);

                functionLocalsView.View = BuildView(PDBTableIndex.FunctionLocalsTable);
                functionLocalsView.ItemsSource = pdbFile.List(PDBTableIndex.FunctionLocalsTable);
            }
            else
            {
                functionLocalsView.View = null;
                functionLocalsView.ItemsSource = null;
            }
        }

        /// <summary>
        /// When a new tab is selected in "TopLevelSymbolLists" or any child TabControl this event
        /// is fired. We only really care about the top level tab control so we ignore all other
        /// cases. We reset the data being being displayed and the columns that are sorted.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void DisplayTabChanged(object sender, SelectionChangedEventArgs args)
        {
            TabControl selectedTab = args.OriginalSource as TabControl;
            
            if (selectedTab != null)
            {
                // There are multiple tabs in the application that all fire this event.
                // The search filter only affects the ListView in the PrimaryListView.
                if (selectedTab.Name == "TopLevelSymbolLists")
                {
                    int currentTab = selectedTab.SelectedIndex;
                    if (pdbFile != null)
                    {
                        // To simplify operations we clear the column sorting indicator. 
                        // We reset the list when we navigate back to tab therefore we need 
                        // to reset the up and down arrow. Later on we could look to see if the
                        // column has been sorted and re-apply the appropriate sort after we 
                        // apply the filter.
                        if (tabViews[currentTab].lastColumnHeader != null)
                        {
                            tabViews[currentTab].lastColumnHeader.Column.HeaderTemplate = null;
                        }

                        // Reset the table back to the original list. We have not applied incremental
                        // updates to this list while we were displaying another tab. Therefore, we 
                        // need to reset and reapply the filter.
                        pdbFile.ResetTable(tabViews[currentTab].index);
                        if (filter != null)
                        {
                            pdbFile.FilterTable(tabViews[currentTab].index, filter);
                        }

                        // Finally, we update the list view itself. 
                        tabViews[currentTab].listView.ItemsSource = pdbFile.List(tabViews[currentTab].index);
                    }
                }
            }
        }

        /// <remarks>
        /// Note: section can happen via auto-complete of backspacing.
        /// </remarks>
        /// <summary>
        /// Handle the callback when an element in the combo box is selected. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void SearchCombo_SelectionChanged(object sender, SelectionChangedEventArgs args)
        {
            if(pdbFile != null)
            {
                
                ComboBoxItem comboBox = SearchCombo.SelectedItem as ComboBoxItem;
                if (comboBox != null)
                {
                    int currentTab = TopLevelSymbolLists.SelectedIndex;
                    pdbFile.ResetTable(tabViews[currentTab].index);
                    string combo = comboBox.Content as string;
                    if(combo == null)
                    {
                        filter = combo;
                        tabViews[currentTab].listView.ItemsSource = pdbFile.List(tabViews[currentTab].index);
                    }
                    else if (combo.CompareTo(filter) != 0)
                    {
                        if (pdbFile.FilterTable(tabViews[currentTab].index, combo))
                        {
                            filter = combo;
                            tabViews[currentTab].listView.ItemsSource = pdbFile.List(tabViews[currentTab].index);
                        }
                    }
                }
            }
        }

        /// <summary>
        ///  Key handler allows incremental changes to the search filter. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void OnKeyDownHandler(object sender, KeyEventArgs args)
        {
            if (pdbFile != null)
            {
                string combo = SearchCombo.Text;
                bool display = false;

                
                if ((combo != null) && (args.Key == Key.Return))
                {
                    if (combo.Length > 0)
                    {
                        bool insert = true;
                        foreach (ComboBoxItem item in SearchCombo.Items)
                        {
                            if (combo.CompareTo(item.Content) == 0)
                            {
                                insert = false;
                                break;
                            }
                        }

                        if (insert)
                        {
                            for (int i = SearchCombo.Items.Count - 1; i > 0; i--)
                            {
                                ComboBoxItem c1 = SearchCombo.Items[i] as ComboBoxItem;
                                ComboBoxItem c2 = SearchCombo.Items[i - 1] as ComboBoxItem;
                                c1.Content = c2.Content;
                            }

                            ComboBoxItem cNew = SearchCombo.Items[0] as ComboBoxItem;
                            cNew.Content = combo;
                        }
                    }
                }
                else if ((combo == null) || (combo.Length < filter.Length) || (args.Key == Key.Back))
                {
                    int currentTab = TopLevelSymbolLists.SelectedIndex;
                    pdbFile.ResetTable(tabViews[currentTab].index);
                    display = true;
                }

                if (combo == null)
                {
                    filter = combo;
                }
                else if (combo.CompareTo(filter) != 0)
                {
                    int currentTab = TopLevelSymbolLists.SelectedIndex;
                    if (pdbFile.FilterTable(tabViews[currentTab].index, combo))
                    {
                        filter = combo;
                        display = true;
                    }
                }

                if (display)
                {
                    int currentTab = TopLevelSymbolLists.SelectedIndex;
                    tabViews[currentTab].listView.ItemsSource = pdbFile.List(tabViews[currentTab].index);
                }
            }
            
        }

        /// <summary>
        /// Present a dialog that will search for PDB and Exe files. It maintains the
        /// last directory for subsequent calls. 
        /// </summary>
        /// <param name="title"></param>
        /// <returns></returns>
        private string OpenFile(string title)
        {
            OpenFileDialog fileDialog = new OpenFileDialog();
            fileDialog.InitialDirectory = currentDirectory;
            fileDialog.Filter = "PDB files (*.pdb *.exe)|*.pdb;*.exe|All files(*.*)|*.*";
            fileDialog.FilterIndex = 0;
            fileDialog.RestoreDirectory = true;
            fileDialog.CheckFileExists = false;
            fileDialog.Title = title;
            if (fileDialog.ShowDialog() == true)
            {
                String result = fileDialog.FileName;
                int slash = result.LastIndexOf('\\');
                if (slash >= 0)
                {
                    currentDirectory = result.Substring(0, slash + 1);
                }

                return result;
            }
            return null;
        }

        /// <summary>
        /// Handles opening a PDB file. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void OpenPDBFile(object sender, ExecutedRoutedEventArgs args)
        {
            string file = OpenFile("Select PDB file");
            if (file != null)
            {
                ClearGridReferences();
                pdbFile = new PDBDataSource(file, progressBar1.Maximum);

                SetStatusBarText("Opening File");
                progressBar1.Value = 0;
                sbar.Visibility = Visibility.Visible;

                // Opening the PDB can take minutes if there is a large number of types.
                // The first 10 percent of types take much longer then the rest so we
                // animate for the first ~10 percent and then give an accurate percentage.
                Duration duration = new Duration(TimeSpan.FromSeconds(30));
                DoubleAnimation doubleAnimation = new DoubleAnimation((float)PDBDataSource.BarInterval, duration);
                doubleAnimation.RepeatBehavior = RepeatBehavior.Forever;
                doubleAnimation.AutoReverse = true;
                progressBar1.BeginAnimation(ProgressBar.ValueProperty, doubleAnimation);

                loadFileWorker.RunWorkerAsync(pdbFile);

                // Set the current file
                Title = file;
            }
        }

        private void LoadViews()
        {
            typeView.View = BuildView(PDBTableIndex.TypeTable);
            typeView.ItemsSource = pdbFile.List(PDBTableIndex.TypeTable);
            globalDataView.View = BuildView(PDBTableIndex.GlobalDataTable);
            globalDataView.ItemsSource = pdbFile.List(PDBTableIndex.GlobalDataTable);
            constantStaticDataView.View = BuildView(PDBTableIndex.ConstantStaticDataTable);
            constantStaticDataView.ItemsSource = pdbFile.List(PDBTableIndex.ConstantStaticDataTable);
            fileStaticDataView.View = BuildView(PDBTableIndex.FileStaticDataTable);
            fileStaticDataView.ItemsSource = pdbFile.List(PDBTableIndex.FileStaticDataTable);
            functionView.View = BuildView(PDBTableIndex.FunctionTable);
            functionView.ItemsSource = pdbFile.List(PDBTableIndex.FunctionTable);
            sbar.Visibility = Visibility.Hidden;
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private static void BackgroundLoadFile_DoWork(object sender, DoWorkEventArgs args)
        {
            BackgroundWorker bw = sender as BackgroundWorker;
            PDBDataSource pdbFile = args.Argument as PDBDataSource;

            args.Result = pdbFile.LoadSymbolInformation(bw);
        }

        private void BackgroundLoadFile_DoWorkComplete(object sender, RunWorkerCompletedEventArgs args)
        {
            if (args.Cancelled)
            {
                // The user canceled the operation.
                MessageBox.Show("Operation was canceled");
                typeView.View = null;
                typeView.ItemsSource = null;
            }
            else if (args.Error != null)
            {
                // There was an error during the operation.
                string msg = String.Format("An error occurred: {0}", args.Error.Message);
                MessageBox.Show(msg);
                ClearGridReferences();
                progressBar1.Value = 0;
                sbar.Visibility = Visibility.Hidden;
            }
            else
            {
                pdbFile.SetComplete();
                LoadViews();
            }
        }

        /// <summary>
        ///  Remove
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void BackgroundLoadFile_ProgressChanged(object sender, ProgressChangedEventArgs args)
        {
            if ((args.UserState != null) || (args.ProgressPercentage > 10))
            {
                progressBar1.BeginAnimation(ProgressBar.ValueProperty, null);

                if (args.UserState != null)
                {
                    PDBTableIndex index = (PDBTableIndex) args.UserState;
                    switch(index)
                    {
                        case PDBTableIndex.TypeTable:
                            SetStatusBarText("Loading Types");
                            break;
                        case PDBTableIndex.FunctionTable:
                            // When we return extra data then the type table is complete but not the
                            // function table.
                            SetStatusBarText("Loading Functions");
                            break;
                        case PDBTableIndex.FileStaticDataTable:
                            SetStatusBarText("Loading Data");
                            break;
                        case PDBTableIndex.GlobalDataTable:
                            SetStatusBarText("Loading complands");
                            break;
                        case PDBTableIndex.Size:
                            progressBar1.Value = args.ProgressPercentage;
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        private void SetStatusBarText(string txt)
        {
            StatusBarItem statusBar = sbar.Items[0] as StatusBarItem;
            if (statusBar != null)
            {
                TextBlock textBlock = statusBar.Content as TextBlock;
                if (textBlock != null)
                {
                    textBlock.Text = txt;
                }
            }
        }

        private void CloseApplication(object sender, ExecutedRoutedEventArgs args)
        {
            Application.Current.Shutdown();
        }

        private void CanExecuteOpen(object sender, CanExecuteRoutedEventArgs args)
        {
           args.CanExecute = true;
        }

        private void CanCloseApplication(object sender, CanExecuteRoutedEventArgs args)
        {
            args.CanExecute = true;
        }

        /// <summary>
        /// When a column header is clicked with in a ListView that uses this event handler
        /// we know it is one of the primary ListViews. We also know that only the visible 
        /// ListView will invoke the handler so we use these facts to determine which list
        /// to manipulate. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void PrimaryGridViewColumnHeaderClickedHandler(object sender, RoutedEventArgs args)
        {
            GridViewColumnHeaderClickedHandler((SortedDataTables)TopLevelSymbolLists.SelectedIndex, sender, args);
            ListView v = sender as ListView;
            if (v != null)
            {
                v.ItemsSource = pdbFile.List(tabViews[TopLevelSymbolLists.SelectedIndex].index);
            }
        }

        /// <summary>
        /// Three additional ListViews, that are not one of the primary ListViews, can also be sorted. We
        /// decorate each with their own special handler so we do not need to keep track of which
        /// data table is sorted.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void TypeFunctionGridViewColumnHeaderClickedHandler(object sender, RoutedEventArgs args)
        {
            GridViewColumnHeaderClickedHandler(SortedDataTables.TypeFunctionTable, sender, args);
            typeFunctionView.ItemsSource = pdbFile.List(PDBTableIndex.TypeFunctionTable);
        }

        private void TypeFunctionLocalsGridViewColumnHeaderClickedHandler(object sender, RoutedEventArgs args)
        {
            GridViewColumnHeaderClickedHandler(SortedDataTables.TypeFunctionLocalsTable, sender, args);
            typeFunctionLocalsView.ItemsSource = pdbFile.List(PDBTableIndex.TypeFunctionLocalsTable);
        }

        private void FunctionReferencedDataGridViewColumnHeaderClickedHandler(object sender, RoutedEventArgs args)
        {
            GridViewColumnHeaderClickedHandler(SortedDataTables.FunctionLocalsTable, sender, args);
            functionLocalsView.ItemsSource = pdbFile.List(PDBTableIndex.FunctionLocalsTable);
        }

        /// <summary>
        /// We store the last direction that was sorted for the 
        /// </summary>
        /// <param name="table"></param>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void GridViewColumnHeaderClickedHandler(SortedDataTables tableIndex, object sender, RoutedEventArgs args)
        {
            GridViewColumnHeader headerClicked = args.OriginalSource as GridViewColumnHeader;
            if (headerClicked != null)
            {
                if(headerClicked.Role != GridViewColumnHeaderRole.Padding)
                {

                    int table = (int)tableIndex;
                    string headerBinding = ((System.Windows.Data.Binding)(headerClicked.Column.DisplayMemberBinding)).Path.Path;
                    ColumnDirection direction = tabViews[table].lastDirection;

                    if (direction == ColumnDirection.Descending)
                    {
                        direction = ColumnDirection.Ascending;
                    }
                    else
                    {
                        direction = ColumnDirection.Descending;
                    }

                    if (pdbFile.SortTable(tabViews[table].index, headerBinding, direction))
                    {
                        if (tabViews[table].lastColumnHeader != null && tabViews[table].lastColumnHeader != headerClicked)
                        {
                            tabViews[table].lastColumnHeader.Column.HeaderTemplate = null;
                        }

                        // By setting the template we get the up or down arrow behind the column name. WPF makes this 
                        // extremely simple to do. 
                        if (direction == ColumnDirection.Ascending)
                        {
                            headerClicked.Column.HeaderTemplate = Resources["HeaderTemplateArrowUp"] as DataTemplate;
                        }
                        else if (direction == ColumnDirection.Descending)
                        {
                            headerClicked.Column.HeaderTemplate = Resources["HeaderTemplateArrowDown"] as DataTemplate;
                        }

                        tabViews[table].lastColumnHeader = headerClicked;
                        tabViews[table].lastDirection = direction;
                    }
                }
            }
        }
    }
}
