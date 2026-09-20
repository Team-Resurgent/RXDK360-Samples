//----------------------------------------------------------------------------------------------------------------------
// Utility.inl
//
// Inline implementation file for Utility.h methods. 
//
// Developed by Microsoft Advanced Technology Group.
// Copyright (C) Microsoft Corporation. All rights reserved.
//----------------------------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar constructor
// Desc: Creates a new console-based progress bar.
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
ProgressBar<TValue>::ProgressBar()
: m_hStdOut( NULL ),
  m_bThrob( BarMode_Normal ),
  m_iMin( 0 ),
  m_iCurrent( 0 ),
  m_iMax( 100 )
{

}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar destructor
// Desc: Cleans up the progress bar (if it has been Started)
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
ProgressBar<TValue>::~ProgressBar()
{
    if ( m_hStdOut )
    {
        End();
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar::SetLimits
// Desc: Sets the minimum, maximum and current values for the progress bar.
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
VOID ProgressBar<TValue>::SetLimits( TValue minValue /*= 0*/, TValue maxValue /*= 100*/, TValue currentValue /*= 0*/ )
{
    assert( minValue <= maxValue && currentValue >= minValue && currentValue <= maxValue );

    if ( m_iMin != minValue || m_iMax != maxValue || m_iCurrent != currentValue )
    {
        m_iMin = minValue;
        m_iCurrent = currentValue;
        m_iMax = maxValue;

        if ( m_hStdOut )
        {
            End();
            Start();
        }

    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar::SetMode
// Desc: Sets the mode of the progress bar output. Two choices: BarMode_Throb (which just displays a spinner, and no
//       other content), and BarMode_Normal (which shows percentage as well).
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
VOID ProgressBar<TValue>::SetMode( BarMode mode /*= BarMode_Normal */ )
{
    if ( m_bThrob != mode )
    {
        m_bThrob = mode;
        if( m_hStdOut )
        {
            End();
            Start();
        }
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar::Start
// Desc: Displays the progress bar for the first time.
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
VOID ProgressBar<TValue>::Start()
{
    // Reset the spinner.
    m_pszSpinnerPos = s_pszSpinner;

    // Make sure the output stream is flushed before we start.
    fflush(stdout);

    CONSOLE_SCREEN_BUFFER_INFO csbi;

    // Get the console output.
    m_hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );

    if ( m_hStdOut == NULL || m_hStdOut == INVALID_HANDLE_VALUE )
    {
        m_hStdOut = NULL;
        return;
    }

    // Get the dimensions of the console window, plus the current cursor position.

    if ( !GetConsoleScreenBufferInfo( m_hStdOut, &csbi ) )
    {
        m_hStdOut = NULL;
        return;
    }
    // NOTE: This is the last of the failure cases for conio we really care about in here
    //       for our purposes. However, -you- may want to handle them.

    // Save out the current cursor position:
    COORD consoleWinSize = csbi.dwSize;
    COORD cursorPos = csbi.dwCursorPosition;
    m_cStart = cursorPos;

    // Do a test write of the spinner text to measure it (saves you from needing to
    // count chars if you change it).
    _stprintf_s( scratch, ARRAYSIZE(scratch), s_pszProgFormat, _T('+'), 100);

    INT len = _tcslen( scratch );

    // Do we need to move to a new line?
    if ( consoleWinSize.X - cursorPos.X < len )
    {
        // Move to next line.
        WriteConsole( m_hStdOut, _T("\r\n"), 2, NULL, NULL);
        GetConsoleScreenBufferInfo( m_hStdOut, &csbi );

        // Keep the start pos.
        m_cStart = csbi.dwCursorPosition;
    }

    // Output the bar for the first time.
    WriteProgressBar();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar::SetValue
// Desc: Sets the current value of the progress bar, and updates the display.
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
VOID ProgressBar<TValue>::SetValue( TValue iCurrent )
{
    assert( iCurrent >= m_iMin && iCurrent <= m_iMax && "Value out of range");

    if ( iCurrent != m_iCurrent )
    {
        m_iCurrent = iCurrent;
        WriteProgressBar();
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar::Tick
// Desc: Throbs the progress bar spinner, and increments the value of iCurrent if the bar is in "Normal" mode. In
//       "Throb" mode, just updates the spinner.
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
VOID ProgressBar<TValue>::Tick()
{
    if ( m_bThrob == BarMode_Normal )
    {
        ++m_iCurrent;
        assert( m_iCurrent <= m_iMax && "ticked past end");
    }

    WriteProgressBar();
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar::End
// Desc: Hides the progress bar.
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
VOID ProgressBar<TValue>::End()
{
    if ( !m_hStdOut )
    {
        return;
    }

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo( m_hStdOut, &csbi );
    m_cEnd = csbi.dwCursorPosition;

    SetConsoleCursorPosition( m_hStdOut, m_cStart );

    COORD curPos = m_cStart;
    while ( curPos.Y < m_cEnd.Y || (curPos.Y == m_cEnd.Y && curPos.X < m_cEnd.X) )
    {
        WriteConsole( m_hStdOut, _T(" "), 1, NULL, NULL);

        GetConsoleScreenBufferInfo( m_hStdOut, &csbi );
        curPos = csbi.dwCursorPosition;
    }

    SetConsoleCursorPosition( m_hStdOut, m_cStart );

    m_hStdOut = NULL;
}

//----------------------------------------------------------------------------------------------------------------------
// Name: ProgressBar::WriteProgressBar
// Desc: Writes the progress bar to the console.
//----------------------------------------------------------------------------------------------------------------------
template<typename TValue>
VOID ProgressBar<TValue>::WriteProgressBar()
{
    if ( !m_hStdOut )
    {
        // If necessary, fall back to printing dots for every tick.
        _tprintf( _T(".") );
        return;
    }

    TCHAR spinnerChar = *m_pszSpinnerPos++;

    if ( *m_pszSpinnerPos == '\0' )
    {
        m_pszSpinnerPos = s_pszSpinner;
    }

    if ( m_bThrob == BarMode_Throb )
    {
        SetConsoleCursorPosition( m_hStdOut, m_cStart );
        WriteConsole( m_hStdOut, m_pszSpinnerPos, 1, NULL, NULL );
    }
    else
    {
        // Convert to percentage
        INT iPercentage = MulDiv( m_iCurrent - m_iMin, 100, m_iMax - m_iMin );

        SetConsoleCursorPosition( m_hStdOut, m_cStart );

        _stprintf_s( scratch, ARRAYSIZE(scratch), s_pszProgFormat, spinnerChar, iPercentage );

        INT len = _tcslen( scratch );

        WriteConsole( m_hStdOut, scratch, len, NULL, NULL );

    }
}
