#include <windows.h>
#include <stdio.h>

#include "Util.h"

BOOL isAdmin()
{
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdministratorsGroup;
    // Initialize SID.
    if( !AllocateAndInitializeSid( &NtAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0, 0, 0, 0, 0, 0,
                &AdministratorsGroup))
    {
        // Initializing SID Failed.
        return false;
    }
    // Check whether the token is present in admin group.
    BOOL IsInAdminGroup = FALSE;
    if( !CheckTokenMembership( NULL,
                AdministratorsGroup,
                &IsInAdminGroup ))
    {
        // Error occurred.
        IsInAdminGroup = FALSE;
    }
    // Free SID and return.
    FreeSid(AdministratorsGroup);
    return IsInAdminGroup;

}

BOOL RunElevated(
        HWND hwnd,
        LPCTSTR pszPath,
        LPCTSTR pszParameters = NULL,
        LPCTSTR pszDirectory = NULL )
{
    SHELLEXECUTEINFO shex;

    memset( &shex, 0, sizeof( shex) );

    shex.cbSize        = sizeof( SHELLEXECUTEINFO );
    shex.fMask        = 0;
    shex.hwnd        = hwnd;
    shex.lpVerb        = "runas";
    shex.lpFile        = pszPath;
    shex.lpParameters    = pszParameters;
    shex.lpDirectory    = pszDirectory;
    shex.nShow        = SW_HIDE;

    return ::ShellExecuteEx( &shex );
}

char *LastErrorString()
{

    DWORD err = GetLastError();

    const DWORD buffsize = 300+1;
    static char buff[buffsize];

    if (( err == ERROR_ACCESS_DENIED) && (!isAdmin())) {
        sprintf_s(buff, buffsize, "Access is denied.\n\nHave you tried to run the program as an administrator by starting the command prompt as 'run as administrator'?");
    }
    else if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                err,
                0,
                buff,
                buffsize,
                NULL) == 0)
    { 
        // FormatMessage failed
        sprintf_s(buff, buffsize, "Unknown error with error code = %d", err);
    }
    return buff;
} // ErrorString

BOOL SendCommandToService(char * message, char chBuf[], int chBufSize)
{
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    DWORD dwError = 0;
    DWORD dwMode, cbRead;

    memset(chBuf, 0, chBufSize);

    while (true) 
    { 
        hPipe = ::CreateFile((LPSTR)"\\\\.\\pipe\\VBoxVmService", 
                GENERIC_READ|GENERIC_WRITE, 0, 0, OPEN_EXISTING, 0, 0);
        if (hPipe != INVALID_HANDLE_VALUE)
        {
            break;
        }

        // If any error except the ERROR_PIPE_BUSY has occurred,
        // we should return FALSE. 
        dwError = GetLastError();
        if (dwError != ERROR_PIPE_BUSY) 
        {
            sprintf_s(chBuf, chBufSize, "Connect to pipe failed: %s\nIs VBoxVmService running?", LastErrorString());
            return FALSE;
        }
        // The named pipe is busy. Let's wait for 2 seconds. 
        if (!WaitNamedPipe((LPSTR)"\\\\.\\pipe\\VBoxVmService", 2000)) 
        { 
            sprintf_s(chBuf, chBufSize, "WaitNamedPipe failed: %s\n", LastErrorString());
            return FALSE;
        } 
    } 

    // The pipe connected; change to message-read mode. 
    dwMode = PIPE_READMODE_MESSAGE; 
    dwError = SetNamedPipeHandleState( 
            hPipe,    // pipe handle 
            &dwMode,  // new pipe mode 
            NULL,     // don't set maximum bytes 
            NULL);    // don't set maximum time 
    if ( ! dwError) 
    {
        sprintf_s(chBuf, chBufSize, "SetNamedPipeHandleState failed: %s\n", LastErrorString());
        return FALSE;
    }

    // Send the message to the pipe server. 
    DWORD dwRead = 0;
    if (!(WriteFile(hPipe, (LPVOID)message, (DWORD)(strlen(message) + 1), &dwRead, 0)))
    {
        CloseHandle(hPipe);
        sprintf_s(chBuf, chBufSize, "WriteFile to pipe failed: %s\n", LastErrorString());
        return FALSE;
    }

    // Read from the pipe. 
    do 
    { 
        dwError = ReadFile( 
                hPipe,    // pipe handle 
                chBuf,    // buffer to receive reply 
                chBufSize,  // size of buffer 
                &cbRead,  // number of bytes read 
                NULL);    // not overlapped 

        if ( ! dwError && GetLastError() != ERROR_MORE_DATA )
            break; 
        //Debug: Uncoment to se what we have read from pipe
        //printf("Read %d bytes\n", cbRead);
        //printf( TEXT("\n%s\n"), chBuf ); 
    } while ( ! dwError);  // repeat loop if ERROR_MORE_DATA 

    if ( ! dwError)
    {
        sprintf_s(chBuf, chBufSize, "ReadFile from pipe failed: %s\n", LastErrorString());
        return -1;
    }


    CloseHandle(hPipe);
    return TRUE;
}

char *MachineStateToString(MachineState state)
{
    static char buff[181];
    switch (state) {
        case MachineState_PoweredOff:
            strcpy_s(buff, 180, "powered off");
            break;
        case MachineState_Saved:
            strcpy_s(buff, 180, "saved");
            break;
        case MachineState_Aborted:
            strcpy_s(buff, 180, "aborted");
            break;
        case MachineState_Running:
            strcpy_s(buff, 180, "running");
            break;
        case MachineState_Paused:
            strcpy_s(buff, 180, "paused");
            break;
        case MachineState_Starting:
            strcpy_s(buff, 180, "starting");
            break;
        case MachineState_Stopping:
            strcpy_s(buff, 180, "stopping");
            break;
        case MachineState_Saving:
            strcpy_s(buff, 180, "saving");
            break;
        case MachineState_Restoring:
            strcpy_s(buff, 180, "restoring");
            break;
        default:
            sprintf_s(buff, 180, "in unknown state %d", state);
            break;
    }
    return buff;
}
