//--------------------------------------------------------------------------------------
// FunctionVariableLink.cs
//
// XNA Developer Connection
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

using System;
using System.Text;

namespace PDBScope
{
    /// <remark>
    /// Defines the relationship between a function and a type defined within it. A list
    /// of FunctionVariableLink can be searched to see which functions have defined a type.
    /// </remark>
    internal class FunctionVariableLink
    {
        internal FunctionSymbol function;
        internal string variableName;
        internal string variableType;
        internal string derivedVariableType;

        internal FunctionVariableLink(FunctionSymbol s, string f, string t, string dt)
        {
            function = s;
            variableName = f;
            variableType = t;
            derivedVariableType = dt;
        }

        internal string FunctionName
        {
            get { return function.name; }
        }

    };
}