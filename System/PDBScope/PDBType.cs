//--------------------------------------------------------------------------------------
// PDBType.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Windows;
using System.Windows.Data;
using System.Windows.Controls;

using Dia2Lib;

namespace PDBScope
{
    /// <remarks>
    /// 
    /// DIA API Wrapper
    ///
    /// Managed classes are used for display and querying purposes instead
    /// of directly accessing the COM DIA interfaces. This was done for
    /// several reasons, some of which are based on my personal preference
    /// rather than performance and others were to simplify interactions
    /// with the user interface. The interesting symbols are loaded into
    /// memory instead of having operations querying the information from
    /// the PDB, for repeated  accessing the PDB across the COM
    /// interface can be slow.
    ///
    /// The DIA API has one large interface that is used for all
    /// symbols. There is a field that describes the type of symbol plus a
    /// large number of other fields. Only some of them are interesting
    /// for any particular type of symbol and some can have different
    /// meanings for different symbols.  For example, there is a
    /// function-symbol and there is a symbol that represents the
    /// function-type. The function-type has information about the return
    /// value and the arguments where the arguments can be complex types
    /// (e.g. a function pointer). The type field of the function symbol
    /// points to the function type. The type field of the function type
    /// points to the return type. For me, who was trying to reverse
    /// engineer the underlying structure of the symbols this overloading
    /// of fields required me to carefully navigate every symbol to see
    /// what it was like. The children of a symbol are also loosely
    /// defined in DIA. The beginning-of-a-block symbol's child points to
    /// the ending-of-the-block symbol. The ending-of-the-block symbol's
    /// child points to the beginning of the block symbol.
    ///
    /// The wrapper has broken the symbols into discrete classes for each
    /// symbol. Each class just defines the fields that are relevant to
    /// that symbol. Common fields are pushed to base classes. There are
    /// virtual functions that allow assessors to deal with BaseSymbols
    /// for common properties such as name and type. The UI in the sample
    /// makes use of this to set up bindings between the visual objects
    /// and list of symbols.
    /// 
    /// Hierarchy:
    ///     IComparable
    ///   BaseSymbol
    ///     BaseTypeSymbol
    ///     CompilandDetailsSymbol
    ///     CompilandEnvironmentSymbol
    ///     CustomSymbol
    ///     FunctionArgSymbol
    ///     FunctionTypeSymbol
    ///     LocationSymbol
    ///       ArrayBoundSymbol
    ///       BaseClassSymbol
    ///       BlockSymbol
    ///         FunctionSymbol
    ///       LabelSymbol
    ///       DataSymbol
    ///     PointerSymbol
    ///     ThunkSymbol
    ///     TypeDefSymbol
    ///     TypeSymbol
    ///       ArraySymbol
    ///     ENUMSymbol
    ///     UDTSymbol
    ///     UnknownType
    ///
    /// </remarks>
    
    #region GlobalListRoutines

    public delegate void Progress(int count, object arg); 

    class DiaWrapper
    {
        private static int loadedSymbols;

        /// <summary>
        /// A global list is used to ensure only one copy of a symbol is
        /// created. 
        /// 
        /// </summary>
        internal static BaseSymbol[] globalList;

        private static IDiaDataSource source;
        private static IDiaSession session;
        private static IDiaSymbol globalScope;

        private static Progress callback;
        private static Object callbackArgs;

        private static string fileName;
        private static int totalSymbols;
        private static int delay = 500;

        private static void ReportProgress(int count)
        {
            if (callback != null)
            {
                callback(count, callbackArgs);
            }
        }
    
        /// <summary>
        /// Connect to a new PDB file. An optional callback (with a user argument) can be
        /// supplied to report progress when the PDB is searched. 
        /// </summary>
        /// <param name="newFileName"></param>
        /// <param name="cb"></param>
        /// <param name="cbArgs"></param>
        internal static void SetFileName(string newFileName, Progress cb, Object cbArgs)
        {
            ClearReferences();
            globalList = null;
            fileName = newFileName;
            callback = cb;
            callbackArgs = cbArgs;

            source = new DiaSourceClass();

            char[] separator = { '.' };
            int extension = fileName.LastIndexOfAny(separator);
            if (extension > 0 && String.Compare(fileName.Substring(extension), ".pdb", true) == 0)
            {
                source.loadDataFromPdb(fileName);
            }
            else
            {
                source.loadDataForExe(fileName, "SRV**\\\\symbols\\symbols", null);
            }

            source.openSession(out session);
            if (session != null)
            {
                globalScope = session.globalScope;
            }

            IDiaEnumSymbols totalEnum = null;

            globalScope.findChildren(Dia2Lib.SymTagEnum.SymTagNull, null, (uint)NameSearchOptions.nsNone, out totalEnum);
            totalSymbols = totalEnum.count;

            ResizeGlobalList(totalSymbols);
        }

        /// <summary>
        /// Returns the number of global symbols in the DBD. This is just an 
        /// estimate of the total number. 
        /// </summary>
        /// <returns></returns>
        internal static int NumberOfSymbols()
        {
            return totalSymbols;
        }

        /// <summary>
        /// Set the number of symbols that are searched before the progress delagate 
        /// is called. 
        /// </summary>
        /// <param name="newDelay"></param>
        internal static int SetDelay(int newDelay)
        {
            int oldDelay = delay;
            delay = newDelay;
            return oldDelay;
        }

        /// <summary>
        /// Return a symbol based on it's unique id in the PDB.
        /// </summary>
        /// <param name="tag"></param>
        /// <returns></returns>
        internal static BaseSymbol Lookuptag(uint tag)
        {
            if (tag > globalList.Length)
            {
                return null;
            }
            else
            {
                return globalList[tag];
            }
        }

        /// <summary>
        /// Delete root objects that a reference to DIA COM objects. This
        /// will result in the COM objects being released as orphaned objects
        /// are garabage collected.
        /// </summary>
        internal static void ClearReferences()
        {
            session = null;
            source = null;
            globalScope = null;
        }

        /// <summary>
        /// Resize the global symbol list to ensure that the list is larger than
        /// the unique id used to identify the symbol in the PDB.
        /// </summary>
        /// <param name="tag"></param>
        internal static void ResizeGlobalList(int tag)
        {
            if (globalList == null)
            {
                globalList = new BaseSymbol[tag * 2];
            }
            else if (globalList.Length <= tag)
            {
                int size = tag * 2;
                Array.Resize<BaseSymbol>(ref globalList, size);
            }
        }

        /// <summary>
        /// Retrieve the name of the file that the function-symbol is referenced in.
        /// </summary>
        /// <param name="sym"></param>
        /// <returns></returns>
        internal static string GetFunctionFileName(FunctionSymbol sym)
        {
            uint retrievedSymbols = 0;
            const int MaxRvaRange = 0x100;

            if (session != null)
            {
                IDiaEnumLineNumbers enumLines = null;
                session.findLinesByRVA(sym.rva, MaxRvaRange, out enumLines);
                if (enumLines != null)
                {
                    IDiaLineNumber lineNumber = null;
                    enumLines.Next((uint)1, out lineNumber, out retrievedSymbols);
                    while (lineNumber != null && retrievedSymbols == 1)
                    {
                        IDiaSourceFile file = lineNumber.sourceFile;
                        if (file != null)
                        {
                            return file.fileName;
                        }
                    }
                }
            }
            return null;
        }

        /// <summary>
        /// Adds user defined types to the supplied list.
        /// </summary>
        /// <param name="typeList"></param>
        internal static void LoadUDTs(List<UDTSymbol> typeList)
        {
            IDiaEnumSymbols typeEnum = null;

            globalScope.findChildren(Dia2Lib.SymTagEnum.SymTagUDT, null, (uint)NameSearchOptions.nsNone, out typeEnum);
            if (typeEnum != null)
            {
                LoadDefinitions(Dia2Lib.SymTagEnum.SymTagUDT, typeEnum, typeList);
                typeEnum.Reset();
                LoadAllChildren(typeEnum);
            }
        }

        /// <summary>
        /// Adds function declarations to the supplied.
        /// </summary>
        /// <param name="functionDeclarations"></param>
        internal static void LoadFunctions(List<FunctionSymbol> functionDeclarations)
        {
            IDiaEnumSymbols functionEnum = null;

            globalScope.findChildren(Dia2Lib.SymTagEnum.SymTagFunction, null, (uint)NameSearchOptions.nsNone, out functionEnum);
            if (functionEnum != null)
            {
                LoadDefinitions(Dia2Lib.SymTagEnum.SymTagFunction, functionEnum, functionDeclarations);
                functionEnum.Reset();
                LoadAllChildren(functionEnum);
            }
        }

        /// <summary>
        /// Adds global data symbols to the supplied list.
        /// </summary>
        /// <param name="dataDeclarations"></param>
        internal static void LoadDataSymbols(List<DataSymbol> dataDeclarations)
        {
            IDiaEnumSymbols dataEnum = null;

            globalScope.findChildren(Dia2Lib.SymTagEnum.SymTagData, null, (uint)NameSearchOptions.nsNone, out dataEnum);
            if (dataEnum != null)
            {
                LoadDefinitions(Dia2Lib.SymTagEnum.SymTagData, dataEnum, dataDeclarations);
                dataEnum.Reset();
                LoadAllChildren(dataEnum);
            }
        }

        /// <summary>
        /// Load the types, functions and data symbols that are defined all the compilands defined in the PDB file. 
        /// </summary>
        /// <param name="typeList"></param>
        /// <param name="functionDeclarations"></param>
        /// <param name="dataDeclarations"></param>
        internal static void FileGlobalSymbols(List<UDTSymbol> typeList, List<FunctionSymbol> functionDeclarations, List<DataSymbol> dataDeclarations)
        {
            uint retrievedSymbols = 0;

            IDiaSymbol symbol = null;
            IDiaEnumSymbols typeEnum = null;
            IDiaEnumSymbols functionEnum = null;
            IDiaEnumSymbols dataEnum = null;
            IDiaEnumSymbols compilandEnum = null;

            globalScope.findChildren(Dia2Lib.SymTagEnum.SymTagCompiland, null, (uint)NameSearchOptions.nsNone, out compilandEnum);
            compilandEnum.Next((uint)1, out symbol, out retrievedSymbols);
            while (symbol != null && retrievedSymbols == 1)
            {
                symbol.findChildren(Dia2Lib.SymTagEnum.SymTagUDT, null, (uint)NameSearchOptions.nsNone, out typeEnum);
                if (typeEnum != null)
                {
                    LoadDefinitions(Dia2Lib.SymTagEnum.SymTagUDT, typeEnum, typeList);
                    typeEnum.Reset();
                    LoadAllChildren(typeEnum);
                }

                symbol.findChildren(Dia2Lib.SymTagEnum.SymTagFunction, null, (uint)NameSearchOptions.nsNone, out functionEnum);
                if (functionEnum != null)
                {
                    LoadDefinitions(Dia2Lib.SymTagEnum.SymTagFunction, functionEnum, functionDeclarations);
                    functionEnum.Reset();
                    LoadAllChildren(functionEnum);
                }

                symbol.findChildren(Dia2Lib.SymTagEnum.SymTagData, null, (uint)NameSearchOptions.nsNone, out dataEnum);
                if (dataEnum != null)
                {
                    LoadDefinitions(Dia2Lib.SymTagEnum.SymTagData, dataEnum, dataDeclarations);
                    dataEnum.Reset();
                    LoadAllChildren(dataEnum);
                }
                compilandEnum.Next((uint)1, out symbol, out retrievedSymbols);
            }
        }

        /// <summary>
        /// Retrieve the symbols of the given type using the supplied DIA enumerator.
        /// </summary>
        /// <param name="type"></param>
        /// <param name="symbolEnum"></param>
        /// <param name="list"></param>
        internal static void LoadDefinitions(Dia2Lib.SymTagEnum type, IDiaEnumSymbols symbolEnum, IList list)
        {
            int count = 0;
            IDiaSymbol symbol = null;
            uint retrievedSymbols = 0;

            symbolEnum.Next((uint)1, out symbol, out retrievedSymbols);
            while (symbol != null && retrievedSymbols == 1)
            {
                if (symbol.symTag == (uint)type)
                {
                    BaseSymbol sym = CreateSymbol(symbol);
                    if (sym != null)
                    {
                        list.Add(sym);
                    }

                    if (count++ > delay)
                    {
                        ReportProgress(count);
                        count = 0;
                    }
                }
                symbolEnum.Next((uint)1, out symbol, out retrievedSymbols);
            }
        }

        /// <summary>
        /// Ensure all children of a symbol have been loaded into the global
        /// list. If a symbol is loaded it will have it's children loaded as well.
        /// </summary>
        /// <param name="symbolEnum"></param>
        internal static void LoadAllChildren(IDiaEnumSymbols symbolEnum)
        {
            int count = 0;
            IDiaSymbol symbol = null;
            uint retrievedSymbols = 0;

            List<IDiaSymbol> newList = new List<IDiaSymbol>(10);

            symbolEnum.Next((uint)1, out symbol, out retrievedSymbols);
            while (symbol != null && retrievedSymbols == 1)
            {
                ParseChildren(symbol, newList);
                if (count++ > delay)
                {
                    ReportProgress(count);
                    count = 0;
                }
                symbolEnum.Next((uint)1, out symbol, out retrievedSymbols);
            }

            while (newList.Count > 0)
            {
                List<IDiaSymbol> nextList = new List<IDiaSymbol>(10);
                foreach (IDiaSymbol sym in newList)
                {
                    if (sym != null)
                    {
                        ParseChildren(sym, nextList);
                        if (count++ > delay)
                        {
                            ReportProgress(count);
                            count = 0;
                        }
                    }
                }
                newList = nextList;
            }
        }

        #region ClassFactory
        /// <summary>
        /// Iterate through the children symbols creating a list of children.
        /// Loading is done in two passes, the first pass loads all the types
        /// and the second pass adds the children to the symbol.
        /// </summary>
        /// <param name="symbol">Parent symbol</param>
        /// <param name="level">Number times parse has been called.</param>
        internal static void ParseChildren(IDiaSymbol symbol, IList<IDiaSymbol> newLoads)
        {
            IDiaSymbol child = null;
            IDiaEnumSymbols symbolEnum = null;
            uint retrievedSymbols;

            if (symbol != null)
            {
                ResizeGlobalList((int)symbol.symIndexId);
                BaseSymbol sym = globalList[symbol.symIndexId];
                if (sym != null && sym.children.Count == 0)
                {
                    symbol.findChildren(SymTagEnum.SymTagNull, null, (uint)NameSearchOptions.nsNone, out symbolEnum);
                    if (symbolEnum != null)
                    {
                        int count = symbolEnum.count;
                        if (count > 0)
                        {
                            //sym.children = new List<BaseSymbol>(symbolEnum.count);
                            symbolEnum.Next((uint)1, out child, out retrievedSymbols);
                            while (child != null && retrievedSymbols == 1)
                            {
                                ResizeGlobalList((int)child.symIndexId);
                                BaseSymbol symbolChild = globalList[child.symIndexId];
                                if (symbolChild == null)
                                {
                                    CreateSymbol(child);
                                    symbolChild = globalList[child.symIndexId];
                                    if (symbolChild != null)
                                    {
                                        newLoads.Add(child);
                                    }
                                }

                                if (symbolChild != null)
                                {
                                    sym.children.Add(symbolChild);
                                }
                                symbolEnum.Next((uint)1, out child, out retrievedSymbols);
                            }
                        }
                    }
                }
            }
        }

        /// <remarks>
        /// CreateSymbol is a factory that creates the appropriate type of managed
        /// object based on the symbol type (see: symTag property on Dia2Lib::IDiaSymbol).
        /// 
        /// When the object is created it then parses the necessary information from the 
        /// IDiaSymbol API. 
        /// 
        /// Creating managed objects is not an efficient use of memory but was done for
        /// a several reasons.
        ///     1) Clarity - IDiaSymbol represents every type of symbol and the properties 
        ///                  are only relevant for some types of symbols. It is not always
        ///                  obvious which properties are valid for which type of symbol.
        ///     2) Name construction - Names and signatures often need to be contructed from several linked symbols.
        ///                            For example, a function pointer is represented as a pointer symbol
        ///                            whose type property is a function symbol whose type property represents the  
        ///                            return type and has zero or more function argument children.
        ///     3) Cross thread loading - The interfaces (COM callable wrappers) created from the Dia2Lib object
        ///                               can only be called from the thread that created the COM object. Therefore,
        ///                               the interfaces cannot be created on the background thread and used on the UI
        ///                               thread.
        ///                               
        /// </remarks>
        ///
        /// <summary>
        /// Create a managed object based on the symbol passed in. The symbol has a field that defines what
        /// the symbol represents. 
        /// </summary>
        /// <param name="symbol"></param>
        /// <param name="level">Number of times the function has been called.</param>
        /// <returns></returns>
        internal static BaseSymbol CreateSymbol(IDiaSymbol symbol)
        {
            BaseSymbol sym = null;
            if (symbol == null)
            {
                return null;
            }

            uint tag = symbol.symTag;

            ResizeGlobalList((int)symbol.symIndexId);
            sym = globalList[symbol.symIndexId];
            if (sym == null)
            {
                // Keep a count
                loadedSymbols++;
                switch (tag)
                {
                    case (uint)SymTagEnum.SymTagUDT:
                        UDTSymbol udt = new UDTSymbol();
                        udt.ParseSymbol(symbol);
                        sym = udt;
                        break;
                    case (uint)SymTagEnum.SymTagCompilandEnv:
                        CompilandEnvironmentSymbol cse = new CompilandEnvironmentSymbol();
                        cse.ParseSymbol(symbol);
                        sym = cse;
                        break;
                    case (uint)SymTagEnum.SymTagCompilandDetails:
                        CompilandDetailsSymbol cds = new CompilandDetailsSymbol();
                        cds.ParseSymbol(symbol);
                        sym = cds;
                        break;
                    case (uint)SymTagEnum.SymTagEnum:
                        ENUMSymbol enumSymbol = new ENUMSymbol();
                        enumSymbol.ParseSymbol(symbol);
                        sym = enumSymbol;
                        break;
                    case (uint)SymTagEnum.SymTagFunction:
                        FunctionSymbol func = new FunctionSymbol();
                        func.ParseSymbol(symbol);
                        sym = func;
                        break;
                    case (uint)SymTagEnum.SymTagArrayType:
                        ArraySymbol arr = new ArraySymbol();
                        arr.ParseSymbol(symbol);
                        sym = arr;
                        break;
                    case (uint)SymTagEnum.SymTagBaseType:
                        BaseTypeSymbol bt = new BaseTypeSymbol();
                        bt.ParseSymbol(symbol);
                        sym = bt;
                        break;
                    case (uint)SymTagEnum.SymTagPointerType:
                        PointerSymbol ptr = new PointerSymbol();
                        ptr.ParseSymbol(symbol);
                        sym = ptr;
                        break;
                    case (uint)SymTagEnum.SymTagBlock:
                        BlockSymbol block = new BlockSymbol();
                        block.ParseSymbol(symbol);
                        sym = block;
                        break;
                    case (uint)SymTagEnum.SymTagData:
                        DataSymbol dsym = new DataSymbol();
                        dsym.ParseSymbol(symbol);
                        sym = dsym;
                        break;
                    case (uint)SymTagEnum.SymTagCustom:
                        CustomSymbol cs = new CustomSymbol();
                        cs.ParseSymbol(symbol);
                        sym = cs;
                        break;
                    case (uint)SymTagEnum.SymTagFuncDebugStart:
                    case (uint)SymTagEnum.SymTagFuncDebugEnd:
                        LocationSymbol ls = new LocationSymbol();
                        ls.ParseSymbol(symbol);
                        sym = ls;
                        break;
                    case (uint)SymTagEnum.SymTagFunctionType:
                        FunctionTypeSymbol bs = new FunctionTypeSymbol();
                        bs.ParseSymbol(symbol);
                        sym = bs;
                        break;
                    case (uint)SymTagEnum.SymTagBaseClass:
                        BaseClassSymbol bc = new BaseClassSymbol();
                        bc.ParseSymbol(symbol);
                        sym = bc;
                        break;
                    case (uint)SymTagEnum.SymTagThunk:
                        ThunkSymbol tc = new ThunkSymbol();
                        tc.ParseSymbol(symbol);
                        sym = tc;
                        break;
                    case (uint)SymTagEnum.SymTagFunctionArgType:
                        FunctionArgSymbol fa = new FunctionArgSymbol();
                        fa.ParseSymbol(symbol);
                        sym = fa;
                        break;
                    default:
                        UnknownType unknown = new UnknownType();
                        unknown.ParseSymbol(symbol);
                        sym = unknown;
                        break;
                }
                globalList[sym.id] = sym;
            }
            return sym;
        }
        #endregion ClassFactory
    }
    #endregion GlobalListRoutines

    
    internal class BaseSymbol : IComparable<uint>
    {

        internal string name;
        internal uint tag;
        internal ulong size;
        internal uint id;

        internal List<BaseSymbol> children;

        /// <summary>
        /// Flags are can be set from Constants.SymbolFlags
        /// </summary>
        protected uint flags;
        
        #region Accessors
        internal BaseSymbol()
        {
            children = new List<BaseSymbol>();
        }

        /// <remark>
        /// BaseSymbol is the root of all managed symbol objects, every symbol has a tag (its type),
        /// the size of the symbol (can be zero), the name (can be null), and an id which must be greater
        /// then zero. The symbol can also be constant, volatile or unaligned.
        /// 
        /// The information was extracted from the COM model to improve UI reponsiveness. The downside
        /// is a long load time. To offset this the symbol information is loaded on a separate thread. 
        /// The COM interface for the symbols is not free threaded and can only be used on the thread 
        /// that loaded the object and exposed the IDiaSymbol interface. The manage objects are free
        /// threaded and can be used on any other thread. Therefore, the IDiaSymbol interface is not 
        /// maintained because it cannot be used on another thread.
        /// </remark>
        /// 
        /// <summary>
        /// ParseSymbol extracts information from the symbol interface. Every managed symbol object
        /// accesses the properties that are relevant to it. If the managed symbol is derived from
        /// another managed symbol object is must call base.ParseSymbol(symbol) to allow it's base
        /// class to extract its information.
        /// </summary>
        /// <param name="symbol"></param>
        internal void ParseSymbol(IDiaSymbol symbol)
        {
            Tag = symbol.symTag;   // Use the set property to do parameter validation.
            size = symbol.length;
            name = symbol.name;
            id = symbol.symIndexId;

            if (symbol.constType == 1)
            {
                SetFlag(Constants.SymbolFlags.constFlag);
            }

            if (symbol.volatileType == 1)
            {
                SetFlag(Constants.SymbolFlags.volatileFlag);
            }

            if (symbol.unalignedType == 1)
            {
                SetFlag(Constants.SymbolFlags.unalignedFlag);
            }
        }

        public uint Tag
        {
            get { return (uint)tag; }
            set { Trace.Assert(value < (uint)SymTagEnum.SymTagMax, "Unknown tag"); tag = value; }
        }

        #endregion Accessors

        #region FlagRoutines
        /// <summary>
        /// The flag information for all derived types are stored in a single location. There is 
        /// limit of 32 flags. Each flag has a name that can be appeneded to a string builder.
        /// When constructing names and signatures the flag information often needs to be appended.
        /// For example, const is part of type definitions and function definitions. 
        /// </summary>
        /// <param name="f"></param>
        /// <param name="nm"></param>
        protected void AppendFlagName(Constants.SymbolFlags f, StringBuilder nm)
        {
            AppendFlagName(f, nm, true);
        }

        protected void AppendFlagName(Constants.SymbolFlags f, StringBuilder nm, bool appendSpace)
        {
            if (IsSet(f))
            {
                nm.Append(Constants.SymbolFlagNames[(int)f]);
                if (appendSpace)
                {
                    nm.Append(" ");
                }
            }
        }

        protected void ClearFlag(Constants.SymbolFlags f)
        {
            flags &= (uint)~(1 << (int)f);
        }

        protected bool IsSet(Constants.SymbolFlags f)
        {
            if ((flags & (uint)(1 << (int)f)) != 0)
            {
                return true;
            }
            else
            {
                return false;
            }
        }

        protected void SetFlag(Constants.SymbolFlags f)
        {
            flags |= (uint)(1 << (int)f);
        }
        #endregion FlagRoutines

        #region ContainerRoutines
        /// <summary>
        /// Compares the current instance with another object of the same type 
        /// and returns an integer that indicates whether the current instance 
        /// precedes, follows, or occurs in the same position in the sort order 
        /// as the other object.
        /// 
        /// Used to sort elements in the a List<(Of <(T>)>) using the specified 
        /// comparer.
        /// </summary>
        /// <param name="other"></param>
        /// <returns></returns>
        public int CompareTo(uint other)
        {
            if (other > id)
            {
                return -1;
            }
            else
            {
                return (int)(id - other);
            }
        }
        #endregion ContainerRoutines

        #region DisplayRoutines
        public virtual string TypeName
        {
            get { return null; }
        }

        public virtual string DerivedName
        {
            get { return name; }
        }

        /// <summary>
        /// Used by LINQ queries to generate a parameterized name
        /// used in queries. Different symbol types will return
        /// a string representing the SortName requested for the
        /// queries. For example, a data symbol will return the name of
        /// the data entry for the BaseName, the data type for TypeName and
        /// decorated type name for DerivedTypeName. 
        /// </summary>
        public enum NameSortType
        {
            BaseName,
            DerivedName,
            TypeName,
            DerivedTypeName
        };

        /// <summary>
        /// Supplies a name suituable for sorting. For types it is just the name
        /// without decoration, for functions it is the name without return
        /// type or parameters.
        /// </summary>
        /// <param name="nameType"></param>
        /// <returns></returns>
        public virtual string SortName(BaseSymbol.NameSortType nameType)
        {
            return name; // Defaults to name
        }

        public virtual ulong TypeSize
        {
            get { return size; }
        }

        #endregion DisplayRoutines

    };

    /// <summary>
    /// The type used to define the bounds for each dimension of an array. 
    /// </summary>
    internal class ArrayBoundSymbol : LocationSymbol
    {
        /// <summary>
        /// Used to construct a friendly name for a caller (typically . The caller
        /// passes in a string builder being used to construct the name.
        /// </summary>
        /// <param name="nm"></param>
        internal new void BuildName(StringBuilder nm)
        {
            if (locationType == (uint) LocationType.LocIsConstant)
            {
                nm.Append(name);
            }
            else
            {
                base.BuildName(nm);
            }
        }

    };


    /// <summary>
    /// ArraySymbols are used to defined arrays. Arrays can have multipled dimensions and
    /// elements are of the same type. 
    /// </summary>
    internal class ArraySymbol : TypeSymbol
    {
        uint typeId;
        uint rank; // number of lower and upper bounds
        uint[] lowerBound;
        uint[] upperBound;

        uint customCount; // Or we my have an array of custom types
        uint[] customTypes;

        uint elementCount; // Or we have a single dimension array.

        /// <summary>
        /// Return the element type of the array.
        /// </summary>
        private BaseSymbol MyType
        {
            get
            {
                if (typeId != 0 && DiaWrapper.globalList[typeId] != null)
                {
                    return DiaWrapper.globalList[typeId];
                }
                return null;
            }
        }

        /// <summary>
        /// Determine the element type, rank and bounds of an array symbol. 
        /// </summary>
        /// <param name="symbol"></param>
        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            BaseSymbol baseType = null;

            if (symbol.type != null)
            {
                typeId = symbol.type.symIndexId;
                DiaWrapper.CreateSymbol(symbol.type);
            }
            else
            {
                typeId = symbol.typeId;
            }

            rank = symbol.rank;
            if (rank != 0)
            {
                lowerBound = new uint[rank];
                upperBound = new uint[rank];

                IDiaSymbol child = null;
                IDiaEnumSymbols symbolEnum = null;
                uint retrievedSymbols;

                symbol.findChildren(Dia2Lib.SymTagEnum.SymTagDimension, null, (uint)NameSearchOptions.nsNone, out symbolEnum);
                if (symbolEnum != null)
                {
                    int count = 0;
                    symbolEnum.Next((uint)1, out child, out retrievedSymbols);
                    while (child != null && retrievedSymbols == 1)
                    {
                        if (count < rank)
                        {
                            lowerBound[count] = child.lowerBound.symIndexId;
                            upperBound[count] = child.upperBound.symIndexId;
                            count++;
                        }
                        else
                        {
                            break;
                        }
                        symbolEnum.Next((uint)1, out child, out retrievedSymbols);
                    }
                }
            }
            else
            {
                IDiaSymbol child = null;
                IDiaEnumSymbols symbolEnum = null;
                uint retrievedSymbols;
                int count = 0;

                symbol.findChildren(Dia2Lib.SymTagEnum.SymTagCustomType, null, (uint)NameSearchOptions.nsNone, out symbolEnum);
                if (symbolEnum != null)
                {
                    count = symbolEnum.count;
                }

                if (count > 0)
                {
                    uint i = 0;
                    customTypes = new uint[count];
                    symbolEnum.Next((uint)1, out child, out retrievedSymbols);
                    while (child != null && retrievedSymbols == 1)
                    {
                        if (i < count)
                        {
                            customTypes[i++] = child.symIndexId;
                        }
                        else
                        {
                            break;
                        }
                        symbolEnum.Next((uint)1, out child, out retrievedSymbols);
                    }
                    customCount = i;
                }
                else
                {
                    if (symbol.count != 0)
                    {
                        elementCount = symbol.count;
                    }
                    else
                    {
                        uint length = (uint) symbol.length;
                        uint elementLength = 0;

                        if (baseType != null)
                        {
                            elementLength = (uint) baseType.size;
                        }
                        if (elementLength == 0)
                        {
                            elementCount = length;
                        }
                        else
                        {
                            elementCount = length / elementLength;
                        }
                    }
                }
            }
        }

        /// <summary>
        /// Create a decorated version of the array name.
        /// </summary>
        public override string DerivedName
        {
            get
            {
                StringBuilder nm = new StringBuilder(100);
                BaseSymbol baseType = MyType;
                if (baseType != null)
                {
                    nm.Append(baseType.DerivedName);
                }
                else
                {
                    nm.Append(name);
                }
                if (rank > 0)
                {
                    nm.Append("[");
                    for (int i = 0; i < rank; i++)
                    {
                        if (lowerBound[i] != 0)
                        {
                            ArrayBoundSymbol sym = DiaWrapper.globalList[lowerBound[i]] as ArrayBoundSymbol;
                            if(sym != null)
                            {
                                sym.BuildName(nm);
                            }
                            nm.Append("..");
                        }
                        if (upperBound[i] != 0)
                        {
                            ArrayBoundSymbol sym = DiaWrapper.globalList[upperBound[i]] as ArrayBoundSymbol;
                            if(sym != null)
                            {
                                sym.BuildName(nm);
                            }
                        }
                    }
                    nm.Append("]");
                }
                else if (customCount > 0)
                {
                    for (int i = 0; i < customCount; i++)
                    {
                        nm.Append("[ ");
                        if (customTypes[i] != 0)
                        {
                            CustomSymbol sym = DiaWrapper.globalList[customTypes[i]] as CustomSymbol;
                            if(sym != null)
                            {
                                nm.Append(sym.name);
                            }
                            nm.Append(" ");
                        }
                        nm.Append("]");
                    }
                }
                else
                {
                    nm.Append("[]");
                }
                return nm.ToString();
            }
        }

        /// <summary>
        /// Returns the element type name for the array.
        /// </summary>
        public override string TypeName
        {
            get
            {
                BaseSymbol b = MyType;
                if (b != null)
                {
                    return b.TypeName;
                }
                return null;
            }
        }

    };


    internal class BaseClassSymbol : LocationSymbol
    {
        uint derivedClassId; // This symbol (id) is specialized by the baseClassId (which should be the containing symbol)
        uint baseClassId;    // The UDT that is the base type.

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            derivedClassId = symbol.classParentId;
            baseClassId = symbol.typeId;
        }

        internal BaseSymbol BaseClass()
        {
            BaseSymbol b = null;
            if (baseClassId != 0)
            {
                b = DiaWrapper.globalList[baseClassId];
            }
            return b;
        }

        public override string TypeName
        {
            get
            {
                BaseSymbol b = BaseClass();
                if (b != null)
                {
                    return b.TypeName;
                }
                return null;
            }
        }

    };
    
    internal class BaseTypeSymbol : BaseSymbol
    {
        uint baseType;
        uint length;

        private uint IntegerIndex()
        {
            uint size = length >> 1;
            if (size > 2)
            {
                size = size >> 1;
            }
            if (size > 3)
            {
                size = 4;
            }
            return size;
        }

        private uint FloatIndex()
        {
            uint size = length >> 2;
            if (size > 1)
            {
                size = 2;
            }
            return size;
        }

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            baseType = symbol.baseType;
            length = (uint)symbol.length;
        }

        public override string TypeName
        {
            get
            {
                return Constants.BaseTypeNames[baseType];
            }
        }

        public override string DerivedName
        {
            get
            {
                StringBuilder nm = new StringBuilder(17);

                switch (baseType)
                {
                    case 6: // btInt
                        nm.Append("signed ");
                        nm.Append(Constants.SizedIntegerNames[IntegerIndex()]);
                        break;
                    case 7: // btUint
                        nm.Append("unsigned ");
                        nm.Append(Constants.SizedIntegerNames[IntegerIndex()]);
                        break;
                    case 8: // btFloat
                        nm.Append(Constants.SizedFloatNames[FloatIndex()]);
                        break;
                    default:
                        nm.Append(Constants.BaseTypeNames[baseType]);
                        break;
                }

                return nm.ToString();
            }
        }

    };

    internal class BlockSymbol : LocationSymbol
    {
        internal string undecoratedName;

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);

            undecoratedName = symbol.undecoratedName;
            if (undecoratedName == null)
            {
                undecoratedName = symbol.name;
            }
        }

        public override string DerivedName
        {
            get { return undecoratedName; }
        }
    };

    internal class CompilandDetailsSymbol : BaseSymbol
    {
        uint language;
        uint platform;
        bool editAndContinue;
        bool debugInfo;
        bool linkTimeCodeGeneration;
        bool dataAligned;
        bool managedPresent;
        bool securityChecks;
        bool hotPatch;
        bool CVTCIL;
        bool MSILModule;
        uint majorVersion;
        uint minorVersion;
        uint buildVersion;
        uint QFEVersion;
        uint backendMajorVersion;
        uint backendMinorVersion;
        uint backendBuildVersion;
        uint backendQFEVersion;
        string compilerName;

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);

            language = symbol.language;
            platform = symbol.platform;
            editAndContinue = symbol.editAndContinueEnabled == 0 ? false : true;
            debugInfo = symbol.hasDebugInfo == 0 ? false : true;
            linkTimeCodeGeneration = symbol.isLTCG == 0 ? false : true;
            dataAligned = symbol.isDataAligned == 0 ? false : true;
            managedPresent = symbol.hasManagedCode == 0 ? false : true;
            securityChecks = symbol.hasSecurityChecks == 0 ? false : true;
            hotPatch = symbol.isHotpatchable == 0 ? false : true;
            CVTCIL = symbol.isCVTCIL == 0 ? false : true;
            MSILModule = symbol.isMSILNetmodule == 0 ? false : true;
            majorVersion = symbol.frontEndMajor;
            minorVersion = symbol.frontEndMinor;
            buildVersion = symbol.frontEndBuild;
            QFEVersion = symbol.frontEndQFE;
            backendMajorVersion = symbol.backEndMajor;
            backendMinorVersion = symbol.backEndMinor;
            backendBuildVersion = symbol.backEndBuild;
            backendQFEVersion = symbol.backEndQFE;
            compilerName = symbol.compilerName;

        }
    };

    internal class CompilandEnvironmentSymbol : BaseSymbol
    {


    };

    internal class CustomSymbol : BaseSymbol
    {
        uint idOEM;
        uint idOEMSym;

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            idOEM = symbol.oemId;
            idOEMSym = symbol.oemSymbolId;
        }

        public override string TypeName
        {
            get
            {
                return base.name;
            }
        }

    };
    
    /// <summary>
    /// Represents a data declaration of the type: Type dataName; The data symbol
    /// can be a global, file static etc.
    /// </summary>
    internal class DataSymbol : LocationSymbol
    {
        uint typeId;
        uint dataKind;

        internal DataSymbol()
        {
            typeId = 0;
        }

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            dataKind = symbol.dataKind;
            if (symbol.type != null)
            {
                typeId = symbol.type.symIndexId;
                DiaWrapper.CreateSymbol(symbol.type);
            }
            else
            {
                typeId = symbol.typeId;
            }
        }

        public BaseSymbol MyType
        {
            get
            {
                if (typeId != 0 && DiaWrapper.globalList[typeId] != null)
                {
                    return DiaWrapper.globalList[typeId];
                }
                return null;
            }
        }

        public new string BitFieldPosition
        {
            get { return base.BitFieldPosition; }
        }

        public new string BitFieldLength
        {
            get { return base.BitFieldLength; }
        }

        public override string TypeName
        {
            get
            {
                BaseSymbol t = MyType;
                if (t != null)
                {
                    return t.name;
                }
                return null;
            }
        }

        public uint MyDataKind
        {
            get
            {
                return dataKind;
            }
        }

        public string DataName
        {
            get
            {
                return Constants.DataKind[dataKind];
            }
        }

        public string DerivedTypeName
        {
            get
            {
                BaseSymbol type = MyType;
                if (type != null)
                {
                    return type.DerivedName;
                }
                return "<NULL>";
            }
        }

        public string Name
        {
            get { return name; }
        }

        public override ulong TypeSize
        {
            get
            {
                BaseSymbol type = MyType;
                if (type != null)
                {
                    return type.size;
                }
                return 0;
            }
        }

        public override string SortName(BaseSymbol.NameSortType nameType)
        {
            switch (nameType)
            {
                case BaseSymbol.NameSortType.BaseName:
                    return Name;
                case BaseSymbol.NameSortType.DerivedName:
                    return DerivedName;
                case BaseSymbol.NameSortType.TypeName:
                    return TypeName;
                case BaseSymbol.NameSortType.DerivedTypeName:
                    return DerivedTypeName;
            }
            return null;
        }
    };

    internal class ENUMSymbol : TypeSymbol
    {

        public override string TypeName
        {
            get
            {
                return base.name;
            }
        }

        public override string DerivedName
        {
            get
            {
                StringBuilder nm = new StringBuilder();
                nm.Append("enum ");
                nm.Append(name);
                return nm.ToString();
            }
        }
    };

    internal class FunctionArgSymbol : BaseSymbol
    {
        uint typeId;

        private BaseSymbol MyType
        {
            get
            {
                if (typeId != 0 && DiaWrapper.globalList[typeId] != null)
                {
                    return DiaWrapper.globalList[typeId];
                }
                return null;
            }
        }

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            if (symbol.type != null)
            {
                typeId = symbol.type.symIndexId;
                DiaWrapper.CreateSymbol(symbol.type);
            }
            else
            {
                typeId = symbol.typeId;
            }
        }

        public override string TypeName
        {
            get
            {
                BaseSymbol b = MyType;
                if (b != null)
                {
                    return b.name;
                }
                return null;
            }
        }

        public string DerivedTypeName
        {
            get
            {
                BaseSymbol t = MyType;
                if (t != null)
                {
                    return t.DerivedName;
                }
                return null;
            }
        }
    }

    internal class FunctionSymbol : BlockSymbol
    {

        internal uint callingConvention;
        internal uint typeId;
        internal uint access;
        internal int returnUDTCplusplusStyle;
        internal int instanceConstructor;
        internal int instanceContructorOfVirtualBase;

        internal string fileName;

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);

            typeId = symbol.typeId;

            // If there is a return type then load the type;
            if (symbol.type != null)
            {
                DiaWrapper.CreateSymbol(symbol.type);
            }
            
            access = symbol.access;
            callingConvention = symbol.callingConvention;
            returnUDTCplusplusStyle = symbol.isCxxReturnUdt;
            instanceConstructor = symbol.constructor;
            instanceContructorOfVirtualBase = symbol.isConstructorVirtualBase;

            if (symbol.hasAlloca == 1)
            {
                SetFlag(Constants.SymbolFlags.HasAlloca);
            }
            if (symbol.hasLongJump == 1)
            {
                SetFlag(Constants.SymbolFlags.HasLongJump);
            }
            if (symbol.hasSetJump == 1)
            {
                SetFlag(Constants.SymbolFlags.HasSetJump);
            }
            if (symbol.hasInlAsm == 1)
            {
                SetFlag(Constants.SymbolFlags.HasInlineASM);
            }
            if (symbol.hasEH == 1)
            {
                SetFlag(Constants.SymbolFlags.HasExceptionHandler);
            }
            if (symbol.inlSpec == 1)
            {
                SetFlag(Constants.SymbolFlags.SpecifiedInline);
            }
            if (symbol.hasSEH == 1)
            {
                SetFlag(Constants.SymbolFlags.HasSExceptionHandler);
            }
            if (symbol.isNaked == 1)
            {
                SetFlag(Constants.SymbolFlags.IsNaked);
            }
            if (symbol.hasSecurityChecks == 1)
            {
                SetFlag(Constants.SymbolFlags.HasSecurityChecks);
            }
            if (symbol.hasEHa == 1)
            {
                SetFlag(Constants.SymbolFlags.HasAsyncExceptionHandler);
            }
            if (symbol.noStackOrdering == 1)
            {
                SetFlag(Constants.SymbolFlags.HasNoStackOrdering);
            }
            if (symbol.wasInlined == 1)
            {
                SetFlag(Constants.SymbolFlags.WasInlined);
            }
            if (symbol.strictGSCheck == 1)
            {
                SetFlag(Constants.SymbolFlags.HasStrictGSCheck);
            }
        }

        public string Name
        {
            get { return base.name; }
        }

        public string FileName
        {
            get { return fileName; }
        }

        /// <summary>
        /// Function symbols type id is actually the return type. 
        /// </summary>
        public BaseSymbol ReturnType
        {
            get
            {
                if (typeId == 0)
                {
                    return null;
                }
                else
                {
                    return DiaWrapper.globalList[typeId];
                }
            }
        }

        public String FunctionSignature
        {
            get
            {
                if (typeId == 0 || DiaWrapper.globalList[typeId] == null)
                {
                    return null;
                }
                else
                {
                    return DiaWrapper.globalList[typeId].TypeName;
                }
            }
        }
                    
        
    }

    internal class FunctionTypeSymbol : BaseSymbol
    {
        // The return type is the type returned from the function
        // and the children are the argument types.
        uint returnTypeId;

        private BaseSymbol MyType
        {
            get
            {
                if (returnTypeId != 0 && DiaWrapper.globalList[returnTypeId] != null)
                {
                    return DiaWrapper.globalList[returnTypeId];
                }
                return null;
            }
        }

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            returnTypeId = symbol.typeId;
            if (symbol.type != null)
            {
                DiaWrapper.CreateSymbol(symbol.type);
            }
        }

        public override string DerivedName
        {
            get
            {
                StringBuilder nm = new StringBuilder();
                BaseSymbol returnType = MyType;
                if (returnType != null)
                {
                    nm.Append(returnType.DerivedName);
                }
                else
                {
                    nm.Append("void ");
                }
                nm.Append("(*)(");
                if (children != null)
                {
                    bool first = true;
                    foreach (BaseSymbol c in children)
                    {
                        if (first)
                        {
                            first = false;
                        }
                        else
                        {
                            nm.Append(",");
                        }
                        if (c.Tag == (uint)SymTagEnum.SymTagFunctionArgType)
                        {
                            nm.Append(((FunctionArgSymbol)c).DerivedTypeName);
                        }
                        else
                        {
                            nm.Append(c.DerivedName);
                        }
                    }
                }
                nm.Append(")");
                return nm.ToString();
            }
        }

    };

    internal class LabelSymbol : LocationSymbol
    {
        internal string undecoratedName;

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);

            undecoratedName = symbol.undecoratedName;
            if (undecoratedName == null)
            {
                undecoratedName = symbol.name;
            }
        }

        public override string TypeName
        {
            get
            {
                return base.name;
            }
        }

        public override string DerivedName
        {
            get { return undecoratedName; }
        }

    };

    internal class LocationSymbol : BaseSymbol
    {
        protected object constantValue;

        internal uint addressSection;
        internal uint addressOffset;
        internal uint bitPosition;
        internal ulong length;
        internal uint locationType;
        internal uint registerId;
        internal uint relativeOffset;
        internal uint rva;
        internal uint slot;

        internal LocationSymbol()
        { }

        void AppendLocationType(uint t, StringBuilder nm)
        {
            if (t > (uint) LocationType.LocTypeMax)
            {
                nm.Append(Constants.LocationTypeNames[0]);
            }
            else
            {
                nm.Append(Constants.LocationTypeNames[t]);
            }
        }

        protected void BuildName(StringBuilder nm)
        {
            if (locationType == (uint) LocationType.LocIsConstant)
            {
                nm.Append(Convert.ToString(constantValue));
            }
            else
            {
                if (IsSet(Constants.SymbolFlags.RVABasedLocation))
                {
                    AppendLocationType(locationType, nm);
                }
                else if (IsSet(Constants.SymbolFlags.RegisterRelativeLocation))
                {
                    nm.Append("register ");
                    nm.Append(registerId);
                    nm.Append(" offset ");
                    nm.Append(relativeOffset);
                }
                else if (IsSet(Constants.SymbolFlags.ThisRelativeLocation))
                {
                    nm.Append("this+");
                    nm.Append(relativeOffset);
                }
                else if (IsSet(Constants.SymbolFlags.BitFieldLocation))
                {
                    nm.AppendFormat("this({0})+{1}:{2} {3}", relativeOffset, bitPosition, size);
                }
                else if (IsSet(Constants.SymbolFlags.EnregisterLocation))
                {
                    nm.Append("enregistered ");
                    nm.Append(registerId);
                }
                else if (IsSet(Constants.SymbolFlags.SlotLocation))
                {
                    AppendLocationType(locationType, nm);
                    nm.Append(slot);
                }
                else if (IsSet(Constants.SymbolFlags.PureLocation))
                {
                    nm.Append("pure");
                }
            }
            nm.Append(" ");
            nm.Append(name);
        }

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);

            locationType = symbol.locationType;
            switch (locationType)
            {
                case (uint)LocationType.LocIsStatic:
                case (uint)LocationType.LocIsTLS:
                case (uint)LocationType.LocInMetaData:
                case (uint)LocationType.LocIsIlRel:
                    rva = symbol.relativeVirtualAddress;
                    addressOffset = symbol.addressOffset;
                    addressSection = symbol.addressSection;
                    SetFlag(Constants.SymbolFlags.RVABasedLocation);
                    break;

                case (uint)LocationType.LocIsRegRel:
                    registerId = symbol.registerId;
                    relativeOffset = (uint)symbol.offset;
                    SetFlag(Constants.SymbolFlags.RegisterRelativeLocation);
                    break;

                case (uint)LocationType.LocIsThisRel:
                    relativeOffset = (uint)symbol.offset;
                    SetFlag(Constants.SymbolFlags.ThisRelativeLocation);
                    break;

                case (uint)LocationType.LocIsBitField:
                    relativeOffset = (uint)symbol.offset;
                    bitPosition = symbol.bitPosition;
                    length = symbol.length;
                    SetFlag(Constants.SymbolFlags.BitFieldLocation);
                    break;

                case (uint)LocationType.LocIsEnregistered:
                    registerId = symbol.registerId;
                    SetFlag(Constants.SymbolFlags.EnregisterLocation);
                    break;
                case (uint)LocationType.LocIsSlot:
                    slot = symbol.slot;
                    SetFlag(Constants.SymbolFlags.SlotLocation);
                    break;
                case (uint)LocationType.LocIsConstant:
                    constantValue = symbol.value;
                    SetFlag(Constants.SymbolFlags.ConstantLocation);
                    break;
                default:
                    break;
            }
        }

        public string BitFieldPosition
        {
            get
            {
                if (locationType == (uint)LocationType.LocIsBitField)
                {
                    return Convert.ToString(bitPosition);
                }
                else
                {
                    return null;
                }
            }
        }

        public string BitFieldLength
        {
            get
            {
                if (locationType == (uint)LocationType.LocIsBitField)
                {
                    return Convert.ToString(length);
                }
                else
                {
                    return null;
                }
            }
        }
        
        public Object ConstantValue
        {
            get { return constantValue; }
        }

        public override string DerivedName
        {
            get 
            {
                StringBuilder nm = new StringBuilder();
                BuildName(nm);
                return nm.ToString();
            }
        }

        public uint Offset
        {
            get { return relativeOffset; }
        }

        //public ulong Size
        //{
        //    get
        //    {
        //        return length;
        //    }
        //}

    };

    internal class PointerSymbol : BaseSymbol
    {

        uint typeId;

        public BaseSymbol MyType
        {
            get
            {
                if (typeId != 0 && DiaWrapper.globalList[typeId] != null)
                {
                    return DiaWrapper.globalList[typeId];
                }
                return null;
            }
        }

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            if (symbol.type != null)
            {
                typeId = symbol.type.symIndexId;
                DiaWrapper.CreateSymbol(symbol.type);
            }
            else
            {
                typeId = symbol.typeId;
            }

            if (symbol.reference == 1)
            {
                SetFlag(Constants.SymbolFlags.referenceFlag);
            }
        }

        public override string TypeName
        {
            get
            {
                BaseSymbol b = MyType;
                if (b != null)
                {
                    return b.name;
                }
                return null;
            }
        }

        public override string DerivedName
        {
            get
            {
                BaseSymbol b = MyType;
                StringBuilder nm;
                if (b != null)
                {
                    // @TODO some pointers do not have base types ??
                    if (b.Tag == (uint) SymTagEnum.SymTagFunctionType)
                    {
                        return b.DerivedName;
                    }
                    else
                    {
                        nm = new StringBuilder(20);
                        nm.Append(b.DerivedName);
                    }
                }
                else
                {
                    return null;
                }

                if (IsSet(Constants.SymbolFlags.referenceFlag))
                {
                    nm.Append(" &");
                }
                else 
                {
                    nm.Append(" *");
                }

                AppendFlagName(Constants.SymbolFlags.constFlag, nm);
                AppendFlagName(Constants.SymbolFlags.volatileFlag, nm);
                AppendFlagName(Constants.SymbolFlags.unalignedFlag, nm);
                return nm.ToString();
            }
        }

        public string DerivedTypeName
        {
            get
            {
                BaseSymbol t = MyType;
                if (t != null)
                {
                    return t.DerivedName;
                }
                return null;
            }
        }

        public string Name
        {
            get { return name; }
        }

    };

    internal class ThunkSymbol : BaseSymbol
    {
        uint rva;
        uint segment;
        uint offset;

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            rva = symbol.targetRelativeVirtualAddress;
            segment = symbol.targetSection;
            offset = symbol.targetOffset;
        }
    };

    internal class TypeDefSymbol : BaseSymbol
    {
        public override string TypeName
        {
            get
            {
                return base.name;
            }
        }

    };

    internal class TypeSymbol : BaseSymbol
    {
        internal TypeSymbol()
        {
        }

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            if (symbol.constType == 1)
            {
                SetFlag(Constants.SymbolFlags.constFlag);
            }

            if (symbol.volatileType == 1)
            {
                SetFlag(Constants.SymbolFlags.volatileFlag);
            }

            if (symbol.unalignedType == 1)
            {
                SetFlag(Constants.SymbolFlags.unalignedFlag);
            }
        }

        public override string TypeName
        {
            get
            {
                return base.name;
            }
        }

        public bool ConstType
        {
            get { return IsSet(Constants.SymbolFlags.constFlag); }
        }

        public bool ReferenceType
        {
            get { return IsSet(Constants.SymbolFlags.referenceFlag); }
        }

        public bool UnalignedType
        {
            get { return IsSet(Constants.SymbolFlags.unalignedFlag); }
        }

        public bool VolatileType
        {
            get { return IsSet(Constants.SymbolFlags.volatileFlag); }
        }

    };

    internal class UnknownType : BaseSymbol
    {

        public override string DerivedName
        {
            get 
            { 
                return "<Unknown Type>";
            }
        }
    }

    internal class UDTSymbol : TypeSymbol
    {

        internal static string[] UDTNames =
        {
            "struct",
            "class",
            "union",
            "enum",
        };

        uint udtKind;

        internal new void ParseSymbol(IDiaSymbol symbol)
        {
            base.ParseSymbol(symbol);
            udtKind = symbol.udtKind;
        }
        public override string TypeName
        {
            get
            {
                return base.name;
            }
        }

        public override string DerivedName
        {
            get
            {
                StringBuilder nm = new StringBuilder();
                nm.Append(UDTNames[udtKind]);
                nm.Append(" ");
                nm.Append(name);
                return nm.ToString();
            }
        }

        public string Name
        {
            get { return name; }
        }

        public override string SortName(BaseSymbol.NameSortType nameType)
        {
            switch (nameType)
            {
                case BaseSymbol.NameSortType.BaseName:
                    return Name;
                case BaseSymbol.NameSortType.DerivedName:
                    return DerivedName;
                case BaseSymbol.NameSortType.TypeName:
                    return null;
                case BaseSymbol.NameSortType.DerivedTypeName:
                    return null;
            }
            return null;
        }

    }

}
