//--------------------------------------------------------------------------------------
// ExtensionCommands.c
//
// Implmentation of the actual extension commands.
//
// XNA Developer Connection.
// Copyright (C) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

#include "DebuggerExtension.h"

#define	EndianReverse_32(a) (((a>>24)&0x000000ff)|((a>> 8)&0x0000ff00)| \
                             ((a<< 8)&0x00ff0000)|((a<<24)&0xff000000))

//-------------------------------------------------------------------------------------
// Name: read
//
// Extension to read and dump dwords from target
//-------------------------------------------------------------------------------------
DECLARE_API( read )
{
    ULONG cb;
    ULONG64 Address;
    ULONG Buffer[4];

    int i;

    Address = GetExpression( args );

    // Read and display first 4 dwords at Address
    if( ReadMemory( Address, &Buffer, sizeof( Buffer ), &cb ) && cb == sizeof( Buffer ) )
    {
        for( i = 0; i < 4; i++ )
            Buffer[i] = EndianReverse_32(Buffer[i]);
        dprintf( "%I64lx: %08lx %08lx %08lx %08lx\n\n", Address,
                 Buffer[0], Buffer[1], Buffer[2], Buffer[3] );
    }
}

//-------------------------------------------------------------------------------------
// Name: edit
//
// Extension to edit a dword on target
//    !edit <address> <value>
//-------------------------------------------------------------------------------------
DECLARE_API( edit )
{
    ULONG cb;
    ULONG64 Address;
    ULONG Value;

    if( GetExpressionEx( args, &Address, &args ) )
    {
        Value = ( ULONG )GetExpression( args );
    }
    else
    {
        dprintf( "Usage:   !edit <address> <value>\n" );
        return;
    }

    Value = EndianReverse_32(Value);

    // Read and display first 4 dwords at Address
    if( WriteMemory( Address, &Value, sizeof( Value ), &cb ) && cb == sizeof( Value ) )
    {
        dprintf( "%I64lx: %08lx\n", Address, Value );
    }
}

//-------------------------------------------------------------------------------------
// Name: stack
//
// Extension to dump stacktrace
//-------------------------------------------------------------------------------------
DECLARE_API ( stack )
{
    EXTSTACKTRACE64 stk[20];
    ULONG frames, i;
    CHAR Buffer[256];
    ULONG64 displacement;


    // Get stacktrace for current thread 
    frames = StackTrace( 0, 0, 0, stk, 20 );

    if( !frames )
    {
        dprintf( "Stacktrace failed\n" );
    }

    for( i = 0; i < frames; i++ )
    {

        if( i == 0 )
        {
            dprintf( "ChildEBP RetAddr  Args to Child\n" );
        }

        Buffer[0] = '!';
        GetSymbol( stk[i].ProgramCounter, ( PUCHAR )Buffer, &displacement );

        dprintf( "%08p %08p %08p %08p %08p %s",
                 stk[i].FramePointer,
                 stk[i].ReturnAddress,
                 stk[i].Args[0],
                 stk[i].Args[1],
                 stk[i].Args[2],
                 Buffer
                 );

        if( displacement )
        {
            dprintf( "+0x%p", displacement );
        }

        dprintf( "\n" );
    }
}

//-------------------------------------------------------------------------------------
// Name: help
//
// A built-in help for the extension dll
//-------------------------------------------------------------------------------------
DECLARE_API ( help )
{
    dprintf( "Help for extension dll myext.dll\n"
             "   read  <addr>       - It reads and dumps 4 dwords at <addr>\n"
             "   edit  <addr> <val> - It modifies a dword value to <val> at <addr>\n"
             "   stack              - Print current stack trace\n"
             "   help               - Shows this help\n"
             );

}
