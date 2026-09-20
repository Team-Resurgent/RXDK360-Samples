//-----------------------------------------------------------------------------
// Xbox Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


using System;
using System.Collections;


namespace PDBScope
{

    enum LocationType
    {
        LocIsNull,
        LocIsStatic,
        LocIsTLS,
        LocIsRegRel,
        LocIsThisRel,
        LocIsEnregistered,
        LocIsBitField,
        LocIsSlot,
        LocIsIlRel,
        LocInMetaData,
        LocIsConstant,
        LocTypeMax
    };

        // defined in Dia2Lib
    enum DataKind
    {
        DataIsUnknown,        // 0
        DataIsLocal,          // 1
        DataIsStaticLocal,    // 2
        DataIsParam,          // 3
        DataIsObjectPtr,      // 4
        DataIsFileStatic,     // 5
        DataIsGlobal,         // 6
        DataIsMember,         // 7
        DataIsStaticMember,   // 8
        DataIsConstant        // 9
    };



        // defined in Dia2Lib
    //public enum SymTagEnum
    //{
    //    SymTagNull,               0
    //    SymTagExe,                1
    //    SymTagCompiland,          2
    //    SymTagCompilandDetails,   3
    //    SymTagCompilandEnv,       4
    //    SymTagFunction,           5
    //    SymTagBlock,              6
    //    SymTagData,               7
    //    SymTagAnnotation,         8
    //    SymTagLabel,              9
    //    SymTagPublicSymbol,       10
    //    SymTagUDT,                11
    //    SymTagEnum,               12
    //    SymTagFunctionType,       13
    //    SymTagPointerType,        14
    //    SymTagArrayType,          15
    //    SymTagBaseType,           16
    //    SymTagTypedef,            17
    //    SymTagBaseClass,          18
    //    SymTagFriend,             19
    //    SymTagFunctionArgType,    20
    //    SymTagFuncDebugStart,     21
    //    SymTagFuncDebugEnd,       22
    //    SymTagUsingNamespace,     23
    //    SymTagVTableShape,        24
    //    SymTagVTable,             25
    //    SymTagCustom,             26
    //    SymTagThunk,              27
    //    SymTagCustomType,         28
    //    SymTagManagedType,        29
    //    SymTagDimension,          30
    //    SymTagMax                 31
    //};

    enum NameSearchOptions
    {
        nsNone = 0,
        nsfCaseSensitive = 0x1,
        nsfCaseInsensitive = 0x2,
        nsfFNameExt = 0x4,
        nsfRegularExpression = 0x8,
        nsfUndecoratedName = 0x10,
        nsCaseSensitive = nsfCaseSensitive,
        nsCaseInsensitive = nsfCaseInsensitive,
        nsFNameExt = (nsfCaseInsensitive | nsfFNameExt),
        nsRegularExpression = (nsfRegularExpression | nsfCaseSensitive),
        nsCaseInRegularExpression = (nsfRegularExpression | nsfCaseInsensitive)
    } ;

    public class Constants
    {

        public enum AccessFlags
        {
            NoAccess,
            PrivateAccess,
            ProtectedAccess,
            PublicAccess
        };

        public string[] AccessNames =
        {
            "",                     // No access specifier
            "private",
            "protected",
            "public"
        };


        public static string[] DataKind =
        {
          "Unknown",
          "Local",
          "Static Local",
          "Param",
          "Object Ptr",
          "File Static",
          "Global",
          "Member",
          "Static Member",
          "Constant",
        };

 

        // Tags returned by Dia
        public static string[] TagName =
        {
          "(SymTagNull)",                     // SymTagNull
          "Executable (Global)",              // SymTagExe
          "Compiland",                        // SymTagCompiland
          "CompilandDetails",                 // SymTagCompilandDetails
          "CompilandEnv",                     // SymTagCompilandEnv
          "Function",                         // SymTagFunction
          "Block",                            // SymTagBlock
          "Data",                             // SymTagData
          "Annotation",                       // SymTagAnnotation
          "Label",                            // SymTagLabel
          "PublicSymbol",                     // SymTagPublicSymbol
          "UserDefinedType",                  // SymTagUDT
          "Enum",                             // SymTagEnum
          "FunctionType",                     // SymTagFunctionType
          "PointerType",                      // SymTagPointerType
          "ArrayType",                        // SymTagArrayType
          "BaseType",                         // SymTagBaseType
          "Typedef",                          // SymTagTypedef
          "BaseClass",                        // SymTagBaseClass
          "Friend",                           // SymTagFriend
          "FunctionArgType",                  // SymTagFunctionArgType
          "FuncDebugStart",                   // SymTagFuncDebugStart
          "FuncDebugEnd",                     // SymTagFuncDebugEnd
          "UsingNamespace",                   // SymTagUsingNamespace
          "VTableShape",                      // SymTagVTableShape
          "VTable",                           // SymTagVTable
          "Custom",                           // SymTagCustom
          "Thunk",                            // SymTagThunk
          "CustomType",                       // SymTagCustomType
          "ManagedType",                      // SymTagManagedType
          "Dimension",                        // SymTagDimension
        };

        // Processors
        public static string[] FloatPackageNames =
        {
          "hardware processor (80x87 for Intel processors)",    // CV_CFL_NDP
          "emulator",                                           // CV_CFL_EMU
          "altmath",                                            // CV_CFL_ALT
          "???"
        };

        public enum SymbolFlags
        {
            // Failure case
            errorFlag,

            // type and pointer flags
            constFlag,
            volatileFlag,
            unalignedFlag,

            // pointer flag
            referenceFlag,

            // function flags
            HasAlloca,
            HasSetJump,
            HasLongJump,
            HasInlineASM,
            HasExceptionHandler,
            SpecifiedInline,
            HasSExceptionHandler,
            IsNaked,
            HasSecurityChecks,
            HasAsyncExceptionHandler,
            HasNoStackOrdering,
            WasInlined,
            HasStrictGSCheck,

            // Location specific
            RVABasedLocation,
            RegisterRelativeLocation,
            ThisRelativeLocation,
            BitFieldLocation,
            EnregisterLocation,
            SlotLocation,
            ConstantLocation,
            PureLocation

        };

        // if a pointer is not a reference it is
        // "*".
        public static string[] SymbolFlagNames = 
        {
                // type and pointer flags
            "const",
            "volatile",
            "__unaligned",

                // pointer flag
            "&",

                // function flags
            "alloca",
            "setjmp",
            "longjmp",
            "inlasm",
            "eh",
            "inl_specified",
            "seh",
            "naked",
            "gschecks",
            "asyncheh",
            "gsnostackordering",
            "wasinlined",
            "strict_gs_check",

            // Location names
            "rva location",
            "register location",
            "this relative location",
            "bit location",
            "enregistered location",
            "slot location",
            "constant location",
            "pure location"
        };

        public static string[] LocationTypeNames =
        {
              "NULL",
              "static",
              "TLS",
              "RegRel",
              "ThisRel",
              "Enregistered",
              "BitField",
              "Slot",
              "IL Relative",
              "In MetaData",
              "Constant"
        };

        public static string[] SizedIntegerNames =
        {
            "char",
            "short",
            "int",
            "__int64",
            "undefinedLength",

        };

        public static string[] SizedFloatNames =
        {
            "float",
            "double",
            "undefinedLength",
        };

        public static string[] BaseTypeNames =
        {
            "<NoType>",                         // btNoType = 0,
            "void",                             // btVoid = 1,
            "char",                             // btChar = 2,
            "wchar_t",                          // btWChar = 3,
            "signed char",
            "unsigned char",
            "int",                              // btInt = 6,
            "unsigned int",                     // btUInt = 7,
            "float",                            // btFloat = 8,
            "<BCD>",                            // btBCD = 9,
            "bool",                             // btBool = 10,
            "short",
            "unsigned short",
            "long",                             // btLong = 13,
            "unsigned long",                    // btULong = 14,
            "__int8",
            "__int16",
            "__int32",
            "__int64",
            "__int128",
            "unsigned __int8",
            "unsigned __int16",
            "unsigned __int32",
            "unsigned __int64",
            "unsigned __int128",
            "<currency>",                       // btCurrency = 25,
            "<date>",                           // btDate = 26,
            "VARIANT",                          // btVariant = 27,
            "<complex>",                        // btComplex = 28,
            "<bit>",                            // btBit = 29,
            "BSTR",                             // btBSTR = 30,
            "HRESULT"                           // btHresult = 31
        };
    };


}