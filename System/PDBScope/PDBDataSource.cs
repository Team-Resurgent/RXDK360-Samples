//--------------------------------------------------------------------------------------
// PDBDataSource.cs
//
// Manages list of symbols created by enumerating UDT's, Data, and Functions found
// in a PDB. This code provides the binding between the lists and the user interface. 
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.IO;
using System.Runtime.InteropServices;

using Dia2Lib;

/// <remark>
/// The user interface displays the data found in the PDB file through the use of 
/// ListView objects. A ListView object will typically have one or more columns defined for it.
/// Each column consists of a header, a width and a data binding. The data binding provides 
/// the row elements for each column. 
/// 
/// A data source for the List can come from a data base or a collection. For this sample the 
/// data comes from a List<E> collection. ListView objects have an ItemSource property that will
/// be set to a list of symbols (all the same type) and will have the View property set by creating a GridView
/// object containing GridViewColumn objects created out of Columns objects.
/// 
/// 
/// Object Correspondance
///     GridView       <----> PDBTable
///     GridViewColumn <----> ColumnData
/// </remark>

namespace PDBScope
{
    internal delegate IList SortColumn(IList data, ColumnDirection direction);

    /// <summary>
    /// Enumeration used to identify the tables that are displayed in ListViews.
    /// controls. 
    /// </summary>
    internal enum PDBTableIndex
    {
        TypeTable,
        TypeFieldTable,
        TypeFunctionTable,
        TypeFunctionLocalsTable,
        GlobalDataTable,
        GlobalDataFieldTable,
        ConstantStaticDataTable,
        ConstantStaticDataFieldTable,
        FileStaticDataTable,
        FileStaticDataFieldTable,
        FunctionTable,
        FunctionLocalsTable,
        Size
    };

    internal enum ColumnDirection
    {
        NotSet,
        Ascending,
        Descending
    };

    /// <summary>
    /// Each table has multiple columns. The column header is displayed
    /// at the top of column in a ListView control. The binding defines the
    /// property that retrieves the data used to populate a column in the 
    /// ListView. The sort field maintains the direction the column is sorted
    /// in. 
    /// </summary>
    internal struct ColumnData
    {
        internal string header;
        internal string binding;
        internal int width;
        internal SortColumn sort;
    }

    /// <summary>
    /// A table binds a ListView control with data stored in a List. The
    /// List can contain type-symbols, function-symbols, etc.
    /// </summary>
    internal class PDBTable
    {       
        internal ColumnData[] columnData;
        private IList list;                   // sorted and filtered list
        private IList baseList;               // original non filtered, non sorted list

        /// <summary>
        /// Column information must be set when the table is created. 
        /// </summary>
        /// <param name="cols"></param>
        internal PDBTable(ColumnData[] cols)
        {
            columnData = cols;
        }

        /// <summary>
        /// Sort a column in the table. The specific column is found by passing in the
        /// binding path used to populate the column in the UI List control. The binding
        /// path can be retrieved from the list column. For example:
        ///     (System.Windows.Data.Binding)(headerClicked.Column.DisplayMemberBinding)).Path.Path
        ///     
        /// Do not use the same binding for two columns.  
        /// </summary>
        /// <param name="bindingPath"></param>
        /// <param name="direction"></param>
        /// <returns></returns>
        internal bool SortTable(string bindingPath, ColumnDirection direction)
        {
            foreach (ColumnData c in columnData)
            {
                if (c.binding.Equals(bindingPath) && c.sort != null)
                {
                    list = c.sort(list, direction);
                    return true;
                }
            }
            return false;
        }

        internal void SetList(IList columnList)
        {
            list = columnList;
            baseList = columnList;
        }

        internal void Reset()
        {
            list = baseList;
        }

        internal IList List
        {
            get { return list; }
            set { list = value; }
        }

        internal IList BaseList
        {
            get { return baseList; }
        }

        internal void Clear()
        {
            baseList = null;
            list = null;  
        }
    };


    /// <summary>
    /// The data source contains a list of tables and reference to a loaded PDB
    /// file. This class binds the symbols in the PDB file to the UI.
    /// </summary>
    internal class PDBDataSource
    {
        internal const int BarInterval = 1000;

        internal double progressPrecision;
        internal string fileName;
        DiaWrapper dia = new DiaWrapper();

        PDBTable[] tables;

        List<FunctionVariableLink> variables;
        bool complete;   // Has load completed


        /// <summary>
        /// The UI will create a PDBDataSource when the user selects a file to load.
        /// The UI passes in the name of the file and the precision of the progress bar 
        /// it is displaying.
        /// </summary>
        /// <param name="file"></param>
        /// <param name="precision"></param>
        internal PDBDataSource(string file, double precision)
        {
            fileName = file;
            progressPrecision = precision;
            tables = new PDBTable[(int) PDBTableIndex.Size];

            //
            // Index: TypeTable
            // Binds UDTSymbol to a table (List is built in LoadTypeDefinitions())
            //
            tables[(int)PDBTableIndex.TypeTable] = CreateUDTSymbolBinding();

            //
            // Index: TypeFieldTable
            // Binds DataSymbol to table (List is built in MakeTypeFieldList())
            //
            tables[(int)PDBTableIndex.TypeFieldTable] = CreateFieldDataSymbolBinding();

            //
            // Index: TypeFunctionTable
            // Binds FunctionSymbols to a table (List is built in MakeFunctionList())
            //
            tables[(int) PDBTableIndex.TypeFunctionTable] = CreateFunctionSymbolBinding();

            //
            // Index: TypeFunctionLocalsTable
            // Binds DataSymbols to a table (List built in MakeFunctionTypeList())
            //
            tables[(int)PDBTableIndex.TypeFunctionLocalsTable] = CreateDataSymbolBinding();       

            //
            // Index: GlobalDataTable
            // Binds DataSymbol to a table (List built in GlobalData looking for DataIsGlobal property)
            //
            tables[(int)PDBTableIndex.GlobalDataTable] = CreateDataSymbolBinding();

            //
            // Index: GlobalDataFieldTable
            // Binds DataSymbol to a table (List is built in MakeTypeFieldList())
            //
            tables[(int)PDBTableIndex.GlobalDataFieldTable] = CreateFieldDataSymbolBinding();
                        
            //
            // Index: ConstantStaticDataTable
            // Binds DataSymbol to a table (List built in GlobalData looking for DataIsConstant property)
            //
            tables[(int)PDBTableIndex.ConstantStaticDataTable] = CreateDataSymbolBinding();

            //
            // Index: ConstantStaticDataFieldTable
            // Binds DataSymbol to a table (List built in MakeTypeFieldList())
            //
            tables[(int)PDBTableIndex.ConstantStaticDataFieldTable] = CreateFieldDataSymbolBinding();

            //
            // Index: FileStaticDataTable
            // Binds DataSymbol to a table (List built in GlobalData looking for DataIsFileStatic property)
            //
            tables[(int)PDBTableIndex.FileStaticDataTable] = CreateDataSymbolBinding();

            //
            // Index: FileStaticDataFieldTable
            // Binds DataSymbol to a table (List built in MakeTypeFieldList())
            //
            tables[(int)PDBTableIndex.FileStaticDataFieldTable] = CreateFieldDataSymbolBinding();
            
            // 
            // Index: FunctionTable
            // Binds FunctionSymbol to a table (List is built in LoadFunctionVariables());
            //
            tables[(int)PDBTableIndex.FunctionTable] = CreateFunctionSymbolWithFileNameBinding();

            //
            // Index: FunctionLocalsTable
            // Binds DataSymbols to a table (List built in MakeFunctionTypeList())
            //
            tables[(int)PDBTableIndex.FunctionLocalsTable] = CreateDataSymbolBinding();     
        }

        internal ColumnData[] ColumnData(PDBTableIndex table)
        {
            return tables[(int)table].columnData;
        }

        /// <summary>
        /// Indicates that the symbol lists are complete and operations can take place.
        /// </summary>
        internal void SetComplete()
        {
            complete = true;
        }

        /// <summary>
        /// Sets the column lists to null. There will be no binding between data and the UI.
        /// </summary>
        internal void ClearTables()
        {
            foreach (PDBTable s in tables)
            {
                s.Clear();
            }
            complete = false;
        }

        /// <summary>
        /// Loads the PDB file. The call is separated from setting
        /// the file name allowing the method to be called on a 
        /// background thread. The background worker is optional.
        /// </summary>
        /// <param name="bWorker"></param>
        /// <returns></returns>
        internal int LoadSymbolInformation(BackgroundWorker bWorker)
        {
            if (fileName != null)
            {
                variables = null;
                ClearTables();

                LoadPDBFile(bWorker);

            }
            return 0;
        }

        /// <summary>
        /// A list of fields is created for the UDT passed in. The UDT symbol
        /// should be selected from the TypeTable list.
        /// </summary>
        /// <param name="searchType"></param>
        internal void CreateTypeFieldList(PDBTableIndex index, UDTSymbol searchType)
        {
            tables[(int)index].SetList(MakeTypeFieldList(searchType));
        }

        /// <summary>
        /// Create a list of data symbols representing a function's local data. If the index is
        /// a TypeFunctionLocalsTable the function symbol must be from the TypeFunctionTable list. 
        /// If the index is FunctionLocalsTable then the function symbol must be from the FunctionTable
        /// list.
        /// </summary>
        /// <param name="parent"></param>
        /// <param name="index"></param>
        internal void CreateFunctionLocalsList(FunctionSymbol parent, PDBTableIndex index)
        {
            if (complete)
            {
                tables[(int)index].SetList(MakeFunctionTypeList(parent));
            }
        }

        /// <summary>
        /// A list of functions is created by looking up which functions have a local
        /// variable of the UDT defined in it.
        /// </summary>
        /// <param name="searchType"></param>
        internal void CreateFunctionList(String dataName, PDBTableIndex functionIndex, PDBTableIndex functionLocalIndex, UDTSymbol searchType)
        {
            if (complete)
            {
                tables[(int)functionIndex].SetList(MakeFunctionList(dataName, searchType, variables));
                tables[(int)functionLocalIndex].Clear();
            }
        }

        /// <summary>
        /// Return the list of symbols associated with the table. Returned value can be null.
        /// </summary>
        /// <param name="table"></param>
        /// <returns></returns>
        internal IList List(PDBTableIndex table)
        {
            return tables[(int)table].List;
        }

        /// <summary>
        /// Sort the table based on the column selected. The bindingPath is the binding property 
        /// found in the ColumnData defined for the table and is used to determine which column to sort 
        /// the table on. 
        /// </summary>
        /// <param name="table"></param>
        /// <param name="bindingPath"></param>
        /// <param name="direction"></param>
        /// <returns></returns>
        internal bool SortTable(PDBTableIndex table, string bindingPath, ColumnDirection direction)
        {
            return tables[(int)table].SortTable(bindingPath, direction);
        }

        /// <summary>
        /// Clear out the specified table.
        /// </summary>
        /// <param name="table"></param>
        internal void ResetTable(PDBTableIndex table)
        {
            tables[(int)table].Reset();
        }

        /// <summary>
        /// Filter the table based on the string. The comparision succeeds when
        /// the name contains the string. The match is case sensistive. 
        /// </summary>
        /// <param name="table"></param>
        /// <param name="letters"></param>
        /// <returns></returns>
        internal bool FilterTable(PDBTableIndex table, string letters)
        {
            if (complete == true)
            {
                switch (table)
                {
                    case PDBTableIndex.TypeTable:
                        tables[(int)table].List = FilterList<UDTSymbol>(tables[(int)table].List, letters);
                        break;
                    case PDBTableIndex.FunctionTable:
                        tables[(int)table].List = FilterList<FunctionSymbol>(tables[(int)table].List, letters);
                        break;
                    default:
                        tables[(int)table].List = FilterList<DataSymbol>(tables[(int)table].List, letters);
                        break;
                }                        
                return true;
            }
            return false;
        }

        
        /// <summary>
        /// Used by filter list to do a case insensitive comparison.
        /// </summary> 
        static bool StringInsensitiveContains(string value, string substring)
        {
            return value.ToLower().Contains(substring.ToLower());
        }

        /// <summary>
        /// Looks up elements by matching the filter against the name. The list must contain
        /// objects derived from BaseSymbol and they must have a 'name' property that returns the
        /// value to be used in the comparision.
        /// </summary>
        /// <param name="original"></param>
        /// <param name="filter"></param>
        /// <param name="lastFilter"></param>
        /// <returns></returns>
        static IList FilterList<T>(IList original, string letters) where T : BaseSymbol
        {
            List<T> sortedTypes = null;
            if (original != null)
            {
                IEnumerable<T> filteredList = null;
                List<T> list = (List<T>)original;

                filteredList =
                    from w in list
                    where StringInsensitiveContains(w.name, letters)
                    select w;

                if (filteredList.Count<T>() > 0)
                {
                    sortedTypes = new List<T>(filteredList.Count<T>());
                    foreach (T e in filteredList)
                        sortedTypes.Add(e);
                }
            }
            return (IList) sortedTypes;

        }

        /// <summary>
        /// Load the symbols representing the fields of a user defined type.
        /// </summary>
        /// <param name="list"></param>
        /// <param name="ds"></param>
        static void LoadTypeFields(List<DataSymbol> list, BaseSymbol ds)
        {
            if (ds != null && ds.children != null && list != null)
            {
                foreach (BaseSymbol c in ds.children)
                {
                    if (c != null)
                    {
                        if (c.tag == (uint)SymTagEnum.SymTagBaseClass)
                        {
                            LoadTypeFields(list, ((BaseClassSymbol)c).BaseClass());
                        }
                        else if (c.tag == (uint)SymTagEnum.SymTagData)
                        {
                            DataSymbol sym = (DataSymbol)c;
                            if (sym.locationType != (uint)LocationType.LocIsStatic)
                            {
                                list.Add((DataSymbol)c);
                            }
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Create a list of fields by iterating through the children of the type. Recurse
        /// through all the children of the children as well. 
        /// </summary>
        /// <param name="searchType"></param>
        /// <returns></returns>
        static IList MakeTypeFieldList(UDTSymbol searchType)
        {
            List<DataSymbol> list = null;
            if (searchType != null && searchType.children != null)
            {
                list = new List<DataSymbol>(searchType.children.Capacity);
                LoadTypeFields(list, searchType);
            }

            return list;
        }
        
        /// <summary>
        /// Create a list of data symbols defined with in a function.
        /// </summary>
        /// <param name="parent"></param>
        /// <returns></returns>
        static IList MakeFunctionTypeList(FunctionSymbol parent)
        {
            List<DataSymbol> list = null;
            if (parent.children != null)
            {
                list = new List<DataSymbol>(parent.children.Capacity);
                foreach (BaseSymbol c in parent.children)
                {
                    if (c != null && c.tag == (uint)SymTagEnum.SymTagData)
                    {
                        DataSymbol ds = c as DataSymbol;
                        if (ds.locationType != (uint)LocationType.LocIsStatic)
                        {
                            list.Add((DataSymbol)c);
                        }
                    }
                }
            }

            return list;
        }

        /// <summary>
        /// Search a list of data symbols defined within a function looking for a symbol type 
        /// and optionally a symbol name. LINQ is used to construct the subset. 
        /// </summary>
        /// <param name="dataName"></param>
        /// <param name="searchType"></param>
        /// <param name="source"></param>
        /// <returns></returns>
        static IList MakeFunctionList(String dataName, BaseSymbol searchType, List<FunctionVariableLink> source)
        {
            IList<FunctionSymbol> list = null;
            if (source != null && searchType != null)
            {
                list = new List<FunctionSymbol>(10);
                IEnumerable<FunctionSymbol> vlist = null;

                if (dataName != null)
                {
                    vlist =
                        from w in source
                        where w.variableType == searchType.name && w.variableName == dataName
                        select w.function;
                }
                else
                {
                    vlist =
                        from w in source
                        where w.variableType == searchType.name
                        select w.function;
                }

                if (vlist.Count<FunctionSymbol>() > 0)
                {
                    foreach (FunctionSymbol s in vlist)
                        list.Add(s);
                }
            }
            return (IList) list;
        }

        /// <summary>
        /// LINQ is utilized to remove UDT's that have the same name. 
        /// </summary>
        /// <param name="inList"></param>
        /// <returns></returns>
        internal static List<UDTSymbol> RemoveDuplicateUDTs(IList inList)
        {
            List<UDTSymbol> list = null;
            List<UDTSymbol> source = (List<UDTSymbol>) inList;

            if (source != null)
            {
                IEnumerable<System.Linq.IGrouping<string, UDTSymbol>> vlist =
                    from w in source
                    group w by w.name;

                /// <remark>
                /// The 'group' key word creates a list of groups. In this
                /// case each group contains all the entries with the same
                /// name. By taking the first entry from each group all
                /// duplicates are removed.
                /// </remark>
                list = new List<UDTSymbol>(source.Count);
                foreach (IGrouping<string, UDTSymbol> s in vlist)
                {
                    UDTSymbol symbol = s.ElementAt(0);
                    if (symbol != null)
                    {
                        list.Add(symbol);
                    }
                }
            }
            return list;
        }
        
    

        internal static IList SortUDTSymbolBySize(IList list, ColumnDirection direction)
        {
            return SortSymbolByTypeSize<UDTSymbol>(list, direction);
        }

        internal static IList SortDataSymbolByTypeSize(IList list, ColumnDirection direction)
        {
            return SortSymbolByTypeSize<DataSymbol>(list, direction);
        }

        internal static IList GlobalData(IList list, DataKind kind)
        {
            IEnumerable<DataSymbol> dataList = (IEnumerable<DataSymbol>)list;
            IList result = null;

            if (dataList != null)
            {
                var globalData =
                    from d in dataList
                    where d.MyDataKind == (uint)kind
                    select d;


                result = globalData.ToList();
            }
            return result;
        }

        /// <summary>
        /// Type size is defined on the BaseSymbol class. Every symbol passed in must be derived from the symbol 
        /// class allowing us to create a generic version of this sort routine. A sorted list based on the type
        /// size is returned.
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="iList"></param>
        /// <param name="direction"></param>
        /// <returns></returns>
        internal static IList SortSymbolByTypeSize<T>(IList iList, ColumnDirection direction) where T : BaseSymbol
        {
            List<T> sortedTypes = null;
            IList<T> list = (IList<T>)iList;
            if (list != null)
            {
                IEnumerable<T> sortedList = null;
                if (direction == ColumnDirection.Ascending)
                {
                    sortedList =
                        from w in list
                        orderby w.TypeSize ascending
                        select w;
                }
                else if (direction == ColumnDirection.Descending)
                {
                    sortedList =
                        from w in list
                        orderby w.TypeSize descending
                        select w;
                }

                /// <remark>
                ///  Retrieve the results creating an approprite list to return.
                /// </remark>
                sortedTypes = new List<T>(list.Count);
                foreach (T e in sortedList)
                    sortedTypes.Add(e);
            }
            return sortedTypes;
        }
        
        /// <summary>
        /// Type name is defined on the BaseSymbol class. Every symbol passed in must be derived from the symbol
        /// class allowing us to create a generic version of this sort routine. A sorted list based on the type
        /// name is returned.
        /// </summary>
        /// <typeparam name="T"></typeparam>
        /// <param name="inList"></param>
        /// <param name="direction"></param>
        /// <param name="nameType"></param>
        /// <returns></returns>
        internal static IList SortSymbolByName<T>(IList inList, ColumnDirection direction, BaseSymbol.NameSortType nameType) where T : BaseSymbol
        {
            List<T> sortedTypes = null;
            IList<T> list = (IList<T>)inList;

            if (list != null)
            {
                IEnumerable<T> sortedList = null;
                if (direction == ColumnDirection.Ascending)
                {
                    sortedList =
                        from w in list
                        orderby w.SortName(nameType) ascending
                        select w;
                }
                else if (direction == ColumnDirection.Descending)
                {
                    sortedList =
                        from w in list
                        orderby w.SortName(nameType) descending
                        select w;
                }

                sortedTypes = new List<T>(list.Count);
                foreach (T e in sortedList)
                    sortedTypes.Add(e);
            }
            return sortedTypes;
        }

        internal static IList SortUDTSymbolByName(IList list, ColumnDirection direction)
        {
            return SortSymbolByName<UDTSymbol>(list, direction, BaseSymbol.NameSortType.BaseName);
        }

        internal static IList SortDataSymbolByName(IList list, ColumnDirection direction)
        {
            return SortSymbolByName<DataSymbol>(list, direction, BaseSymbol.NameSortType.BaseName);
        }

        internal static IList SortFunctionSymbolByName(IList list, ColumnDirection direction)
        {
            return SortSymbolByName<FunctionSymbol>(list, direction, BaseSymbol.NameSortType.BaseName);
        }

        internal static IList SortDataSymbolByDerivedTypeName(IList list, ColumnDirection direction)
        {
            return SortSymbolByName<DataSymbol>(list, direction, BaseSymbol.NameSortType.DerivedTypeName);
        }


        internal void ReportProgress(int count, object arg)
        {
            ProgressInformation info = (ProgressInformation) arg;
            info.ReportProgress(count);
        }

        /// <summary>
        /// We iterate through UDT's, Functions, and global symbols. A global list is created containing every 
        /// element in the PDB that we are interested in. Progress is sent back to the caller as a percentage.
        /// </summary>
        /// <param name="bWorker"></param>
        void LoadPDBFile(BackgroundWorker bWorker)
        {

            List<UDTSymbol> typeList = new List<UDTSymbol>();
            List<FunctionSymbol> functionDeclarations = new List<FunctionSymbol>();
            List<DataSymbol> dataDeclarations = new List<DataSymbol>();

            foreach (PDBTable col in tables)
            {
                col.Clear();
            }

                /// <remark>
                /// The functions within PDBType return the number of symbols loaded in the given enumeration.
                /// ProgressInformation is used to sum up the elements loaded and convert the count to a percentage
                /// for the UI. The UI progress bar displays different textt for every table. ReportState tells
                /// the UI which table was just loaded.
                /// </remark>
            ProgressInformation info = new ProgressInformation(bWorker, progressPrecision);

            DiaWrapper.SetFileName(fileName, ReportProgress, info);
            int totalCount = DiaWrapper.NumberOfSymbols();
            info.SetTotal(totalCount);

            info.ReportState(PDBTableIndex.TypeTable);
            DiaWrapper.LoadUDTs(typeList);

            info.ReportState(PDBTableIndex.FunctionTable);
            DiaWrapper.LoadFunctions(functionDeclarations);

            info.ReportState(PDBTableIndex.FileStaticDataTable);
            DiaWrapper.LoadDataSymbols(dataDeclarations);

            info.ReportState(PDBTableIndex.GlobalDataTable);
            DiaWrapper.FileGlobalSymbols(typeList, functionDeclarations, dataDeclarations);


            LoadFunctionVariables(functionDeclarations, info);
            info.ReportDone();
            DiaWrapper.ClearReferences();

            tables[(int)PDBTableIndex.TypeTable].SetList(RemoveDuplicateUDTs(typeList));
            tables[(int)PDBTableIndex.GlobalDataTable].SetList(GlobalData(dataDeclarations, DataKind.DataIsGlobal));
            tables[(int)PDBTableIndex.ConstantStaticDataTable].SetList(GlobalData(dataDeclarations, DataKind.DataIsConstant));
            tables[(int)PDBTableIndex.FileStaticDataTable].SetList(GlobalData(dataDeclarations, DataKind.DataIsFileStatic));

        }

        /// <remark>
        /// LoadFunctionVariables creates a list of types defined in functions. This list is used to find all the functions
        /// that define any symbol of a specific type. 
        /// </remark>
        ///
        /// <summary>
        /// Finds types used within types. Will call progress call back if supplied.
        /// </summary>
        /// <param name="list"></param>
        /// <param name="info"></param>
        void LoadFunctionVariables(List<FunctionSymbol> list, ProgressInformation info)
        {
            int delay = 200;
            int count = 0;
            bool loadFileName = false;

            variables = new List<FunctionVariableLink>(list.Count * 2); // guess at 2 locals per function

            foreach (BaseSymbol symbol in list)
            {
                FunctionSymbol sym = symbol as FunctionSymbol;
                if (sym != null && sym.children != null)
                {
                    foreach (BaseSymbol c in sym.children)
                    {
                        if (c != null && c.tag == (uint)SymTagEnum.SymTagData)
                        {
                            DataSymbol ds = (DataSymbol)c;
                            BaseSymbol pt = ds.MyType;
                            FunctionVariableLink link;

                                /// <remark>
                                /// If the data symbol is a pointer then navigate to the type that the pointer refereneces.
                                /// </remark>
                            while (pt != null && pt.tag == (uint)SymTagEnum.SymTagPointerType)
                            {
                                pt = ((PointerSymbol)pt).MyType;
                            }
                            if (pt != null && pt.tag == (uint)SymTagEnum.SymTagUDT)
                            {
                                UDTSymbol udt = (UDTSymbol)pt;
                                link = new FunctionVariableLink(sym, ds.Name, udt.TypeName, ds.DerivedName);
                            }
                            else
                            {
                                link = new FunctionVariableLink(sym, ds.Name, ds.TypeName, ds.DerivedTypeName);
                            }
                            variables.Add(link);
                            loadFileName = true;
                        }
                    }

                    if (count++ > delay)
                    {
                        count = 0;
                        ReportProgress(count, info);
                    }

                    if (loadFileName)
                    {
                        sym.fileName = DiaWrapper.GetFunctionFileName(sym);
                    }
                }
            }

            tables[(int)PDBTableIndex.FunctionTable].SetList(list);
        }

        /// <summary>
        /// Create a binding for a function table. Used to show the functions
        /// a type is defined in. (Navigated to through the UDT table)
        /// </summary>
        /// <returns></returns>
        private PDBTable CreateFunctionSymbolBinding()
        {
            ColumnData[] data = new ColumnData[2];
            data[0] = new ColumnData();
            data[0].header = "Function Name";
            data[0].binding = "Name";
            data[0].width = 300;
            data[0].sort = new SortColumn(SortFunctionSymbolByName);

            data[1] = new ColumnData();
            data[1].header = "File";
            data[1].binding = "FileName";
            data[1].width = 500;

            return new PDBTable(data);
        }

        /// <summary>
        /// Create a binding for the functions located by enumerating
        /// the global scope and the compilands.
        /// </summary>
        /// <returns></returns>
        private PDBTable CreateFunctionSymbolWithFileNameBinding()
        {
            ColumnData[] data = new ColumnData[3];
            data[0] = new ColumnData();
            data[0].header = "Function Name";
            data[0].binding = "Name";
            data[0].width = 250;
            data[0].sort = new SortColumn(SortFunctionSymbolByName);

            data[1] = new ColumnData();
            data[1].header = "Decorated Name";
            data[1].binding = "DerivedName";
            data[1].width = 300;

            data[2] = new ColumnData();
            data[2].header = "File";
            data[2].binding = "FileName";
            data[2].width = 500;
            return new PDBTable(data);
        }

        /// <summary>
        /// Create binding for tables that display data symbols. This
        /// is used for several tables.
        /// </summary>
        /// <returns></returns>
        private PDBTable CreateDataSymbolBinding()
        {
            ColumnData[] data = new ColumnData[3];
            data[0] = new ColumnData();
            data[0].header = "Local Name";
            data[0].binding = "Name";
            data[0].width = 300;
            data[0].sort = new SortColumn(SortDataSymbolByName);

            data[1] = new ColumnData();
            data[1].header = "Type";
            data[1].binding = "DerivedTypeName";
            data[1].width = 300;
            data[1].sort = new SortColumn(SortDataSymbolByDerivedTypeName);

            data[2] = new ColumnData();
            data[2].header = "Size";
            data[2].binding = "TypeSize";
            data[2].width = 300;
            data[2].sort = new SortColumn(SortDataSymbolByTypeSize);

            return new PDBTable(data);
        }

        /// <summary>
        /// Create a binding for the field data found in the UDT's
        /// </summary>
        /// <returns></returns>
        private PDBTable CreateFieldDataSymbolBinding()
        {
            ColumnData[] data = new ColumnData[6];
            data[0] = new ColumnData();
            data[0].header = "Field name";
            data[0].binding = "Name";
            data[0].width = 200;

            data[1] = new ColumnData();
            data[1].header = "Type";
            data[1].binding = "DerivedTypeName";
            data[1].width = 200;

            data[2] = new ColumnData();
            data[2].header = "Field Size";
            data[2].binding = "TypeSize";
            data[2].width = 100;
            data[2].sort = null;

            data[3] = new ColumnData();
            data[3].header = "Offset";
            data[3].binding = "Offset";
            data[3].width = 100;
            data[3].sort = null;

            data[4] = new ColumnData();
            data[4].header = "Bit Position";
            data[4].binding = "BitFieldPosition";
            data[4].width = 100;
            data[4].sort = null;

            data[5] = new ColumnData();
            data[5].header = "Bit Length";
            data[5].binding = "BitFieldLength";
            data[5].width = 100;
            data[5].sort = null;

            return new PDBTable(data);
        }

        /// <summary>
        /// Create the binding for the UDT symbols.
        /// </summary>
        /// <returns></returns>
        private PDBTable CreateUDTSymbolBinding()
        {
            ColumnData[] data = new ColumnData[3];
            data[0] = new ColumnData();
            data[0].header = "Type Name";
            data[0].binding = "Name";
            data[0].width = 300;
            data[0].sort = new SortColumn(SortUDTSymbolByName);

            data[1] = new ColumnData();
            data[1].header = "Type Size";
            data[1].binding = "TypeSize";
            data[1].width = 100;
            data[1].sort = new SortColumn(SortUDTSymbolBySize);

            data[2] = new ColumnData();
            data[2].header = "Const";
            data[2].binding = "ConstType";
            data[2].width = 50;
            data[2].sort = null; 

            return new PDBTable(data);
        }

    }

    /// <remark>
    /// 
    /// Pgrogress keeps track of an expected total, a 
    /// running count and the background worker to 
    /// report progress to. 
    /// 
    /// The typical way the class is used is:
    /// 1) Create the class indicating the background worker
    /// 2) Estimate the total number elements to be counted and call
    ///    SetTotal().
    /// 3) Call ReportProgress() to add values to the running count. if
    ///    no value is passed into ReportProgess then the count is reported.
    /// </remark>
    class ProgressInformation
    {
        BackgroundWorker ui;
        int currentCount;
        int totalCount;
        double precision;

        internal ProgressInformation(BackgroundWorker bw, double maximumValue)
        {
            currentCount = 0;
            totalCount = 1;
            ui = bw;
            precision = maximumValue;
        }

        internal void SetTotal(int value)
        {
            if (value != 0)
            {
                totalCount = value;
            }
        }

        internal void ReportProgress(int value)
        {
            if (ui != null)
            {
                currentCount += value;
                if (currentCount > totalCount)
                {
                    currentCount = totalCount;
                }
                ui.ReportProgress((int) ((currentCount * precision) / totalCount), PDBTableIndex.Size);
            }
        }

        internal void ReportState(PDBTableIndex index)
        {
            if (ui != null)
            {
                ui.ReportProgress((int) ((currentCount * precision) / totalCount), index);
            }
        }

        internal void ReportDone()
        {
            currentCount = totalCount;
            if (ui != null)
            {
                ui.ReportProgress(1000, PDBTableIndex.Size);
            }
        }
    }

}