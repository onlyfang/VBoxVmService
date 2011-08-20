/*

   Code riped from http://msdn.microsoft.com/en-us/library/aa365603(v=VS.85).aspx

   Microsoft Public License (Ms-PL)
   MSDN Wiki Sample Code

Published: October 12, 2006

This license governs use of the accompanying software. If you use the software, you accept this license. If you do not accept the license, do not use the software.

Definitions
The terms "reproduce," "reproduction," "derivative works," and "distribution" have the same meaning here as under U.S. copyright law.

A "contribution" is the original software, or any additions or changes to the software.

A "contributor" is any person that distributes its contribution under this license.

"Licensed patents" are a contributor's patent claims that read directly on its contribution.

Grant of Rights
(A) Copyright Grant- Subject to the terms of this license, including the license conditions and limitations in section 3, each contributor grants you a non-exclusive, worldwide, royalty-free copyright license to reproduce its contribution, prepare derivative works of its contribution, and distribute its contribution or any derivative works that you create.

(B) Patent Grant- Subject to the terms of this license, including the license conditions and limitations in section 3, each contributor grants you a non-exclusive, worldwide, royalty-free license under its licensed patents to make, have made, use, sell, offer for sale, import, and/or otherwise dispose of its contribution in the software or derivative works of the contribution in the software.

Conditions and Limitations
(A) No Trademark License- This license does not grant you rights to use any contributors' name, logo, or trademarks.

(B) If you bring a patent claim against any contributor over patents that you claim are infringed by the software, your patent license from such contributor to the software ends automatically.

(C) If you distribute any portion of the software, you must retain all copyright, patent, trademark, and attribution notices that are present in the software.

(D) If you distribute any portion of the software in source code form, you may do so only under this license by including a complete copy of this license with your distribution. If you distribute any portion of the software in compiled or object code form, you may only do so under a license that complies with this license.

(E) The software is licensed "as-is." You bear the risk of using it. The contributors give no express warranties, guarantees or conditions. You may have additional consumer rights under your local laws which this license cannot change. To the extent permitted under your local laws, the contributors exclude the implied warranties of merchantability, fitness for a particular purpose and non-infringement.

*/

#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <process.h>


#include "VBoxVmPipeManager.h"


PIPEINST Pipe[INSTANCES]; 
HANDLE hEvents[INSTANCES +1]; 

VOID DisconnectAndReconnect(DWORD); 
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED); 
void WriteLog(char* pMsg);

int PipeManagerExit() {
    return SetEvent(hEvents[INSTANCES]);
}
int PipeManager(LPTSTR lpszPipename, void __cdecl GetAnswerToRequest(LPPIPEINST pipe))
{
    DWORD i, dwWait, cbRet, dwErr; 
    BOOL fSuccess; 

    char pTemp[121];

    // The initial loop creates several instances of a named pipe 
    // along with an event object for each instance.  An 
    // overlapped ConnectNamedPipe operation is started for 
    // each instance. 
    for (i = 0; i < INSTANCES; i++) 
    { 

        // create named pipe
        Pipe[i].sa.lpSecurityDescriptor = (PSECURITY_DESCRIPTOR)malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
        if (!InitializeSecurityDescriptor(Pipe[i].sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
        {
            sprintf_s(pTemp, "PipeManager: InitializeSecurityDescriptor failed, error code = %d", GetLastError());
            WriteLog(pTemp);
            return 0;
        }
        if (!SetSecurityDescriptorDacl(Pipe[i].sa.lpSecurityDescriptor, TRUE, (PACL)0, FALSE))
        {
            sprintf_s(pTemp, "PipeManager: SetSecurityDescriptorDacl failed, error code = %d", GetLastError());
            WriteLog(pTemp);
            return 0;
        }
        Pipe[i].sa.nLength = sizeof Pipe[i].sa;
        Pipe[i].sa.bInheritHandle = TRUE;

        // Create an event object for this instance. 
        hEvents[i] = CreateEvent( 
                NULL,    // default security attribute 
                TRUE,    // manual-reset event 
                TRUE,    // initial state = signaled 
                NULL);   // unnamed event object 

        if (hEvents[i] == NULL) 
        {
            sprintf_s(pTemp, "PipeManager: CreateEvent failed with %d.\n", GetLastError()); 
            WriteLog(pTemp);
            return 0;
        }

        Pipe[i].oOverlap.hEvent = hEvents[i]; 

        Pipe[i].hPipeInst = CreateNamedPipe( 
                lpszPipename,            // pipe name 
                PIPE_ACCESS_DUPLEX |     // read/write access 
                FILE_FLAG_OVERLAPPED,    // overlapped mode 
                PIPE_TYPE_MESSAGE |      // message-type pipe 
                PIPE_READMODE_MESSAGE |  // message-read mode 
                PIPE_WAIT,               // blocking mode 
                INSTANCES,               // number of instances 
                BUFSIZE*sizeof(TCHAR),   // output buffer size 
                BUFSIZE*sizeof(TCHAR),   // input buffer size 
                PIPE_TIMEOUT,            // client time-out 
                &Pipe[i].sa);             // the security attributes 

        if (Pipe[i].hPipeInst == INVALID_HANDLE_VALUE) 
        {
            sprintf_s(pTemp, "PipeManager: CreateNamedPipe failed with %d.\n", GetLastError());
            WriteLog(pTemp);
            return 0;
        }

        // Call the subroutine to connect to the new client
        Pipe[i].fPendingIO = ConnectToNewClient( 
                Pipe[i].hPipeInst, 
                &Pipe[i].oOverlap); 

        Pipe[i].dwState = Pipe[i].fPendingIO ? 
            CONNECTING_STATE : // still connecting 
            READING_STATE;     // ready to read 
    } 

    // Create an event object used to signal that PipeManager shuld exit. 
    hEvents[INSTANCES] = CreateEvent( 
            NULL,    // default security attribute 
            TRUE,    // manual-reset event 
            FALSE,    // initial state = not signaled 
            NULL);   // unnamed event object 

    if (hEvents[INSTANCES] == NULL) 
    {
        sprintf_s(pTemp, "PipeManager: CreateEvent failed with %d.\n", GetLastError()); 
        WriteLog(pTemp);
        return 0;
    }

    while (true) 
    { 
        // Wait for the event object to be signaled, indicating 
        // completion of an overlapped read, write, or 
        // connect operation. 
        dwWait = WaitForMultipleObjects( 
                (INSTANCES +1),    // number of event objects 
                hEvents,      // array of event objects 
                FALSE,        // does not wait for all 
                INFINITE);    // waits indefinitely 


        // dwWait shows which pipe completed the operation. 
        i = dwWait - WAIT_OBJECT_0;  // determines which pipe 

        // If this was the exit PipeManager event, we exits the loop.
        if (i == INSTANCES) {
            break;
        }

        // Chack the range
        if (i < 0 || i > (INSTANCES - 1)) 
        {
            sprintf_s(pTemp, "PipeManager: Index (%d) out of range. 0..%d\n", i, INSTANCES); 
            WriteLog(pTemp);
            return 0;
        }

        // Get the result if the operation was pending. 
        if (Pipe[i].fPendingIO) 
        { 
            fSuccess = GetOverlappedResult( 
                    Pipe[i].hPipeInst, // handle to pipe 
                    &Pipe[i].oOverlap, // OVERLAPPED structure 
                    &cbRet,            // bytes transferred 
                    FALSE);            // do not wait 

            switch (Pipe[i].dwState) 
            { 
                // Pending connect operation 
                case CONNECTING_STATE: 
                    if (! fSuccess) 
                    {
                        sprintf_s(pTemp, "PipeManager, Pipe error %d.\n", GetLastError()); 
                        WriteLog(pTemp);
                        return 0;
                    }
                    Pipe[i].dwState = READING_STATE; 
                    break; 

                    // Pending read operation 
                case READING_STATE: 
                    if (! fSuccess || cbRet == 0) 
                    { 
                        DisconnectAndReconnect(i); 
                        continue; 
                    }
                    Pipe[i].cbRead = cbRet;
                    Pipe[i].dwState = WRITING_STATE; 
                    break; 

                    // Pending write operation 
                case WRITING_STATE: 
                    if (! fSuccess || cbRet != Pipe[i].cbToWrite) 
                    { 
                        DisconnectAndReconnect(i); 
                        continue; 
                    } 
                    Pipe[i].dwState = READING_STATE; 
                    break; 

                default: 
                    {
                        sprintf_s(pTemp, "PipeManager: Invalid pipe state.\n"); 
                        WriteLog(pTemp);
                        return 0;
                    }
            }  
        } 

        // The pipe state determines which operation to do next. 
        switch (Pipe[i].dwState) 
        { 
            // READING_STATE: 
            // The pipe instance is connected to the client 
            // and is ready to read a request from the client. 
            case READING_STATE: 
                fSuccess = ReadFile( 
                        Pipe[i].hPipeInst, 
                        Pipe[i].chRequest, 
                        BUFSIZE*sizeof(TCHAR), 
                        &Pipe[i].cbRead, 
                        &Pipe[i].oOverlap); 

                // The read operation completed successfully.  
                if (fSuccess && Pipe[i].cbRead != 0) 
                { 
                    Pipe[i].fPendingIO = FALSE; 
                    Pipe[i].dwState = WRITING_STATE; 
                    continue; 
                } 

                // The read operation is still pending. 
                dwErr = GetLastError(); 
                if (! fSuccess && (dwErr == ERROR_IO_PENDING)) 
                { 
                    Pipe[i].fPendingIO = TRUE; 
                    continue; 
                } 

                // An error occurred; disconnect from the client. 
                DisconnectAndReconnect(i); 
                break; 

                // WRITING_STATE: 
                // The request was successfully read from the client. 
                // Get the reply data and write it to the client. 
            case WRITING_STATE: 
                GetAnswerToRequest(&Pipe[i]); 

                //make it a valid string.
                ++Pipe[i].cbToWrite;
                Pipe[i].chReply[Pipe[i].cbToWrite] = '\0';

                fSuccess = WriteFile( 
                        Pipe[i].hPipeInst, 
                        Pipe[i].chReply, 
                        Pipe[i].cbToWrite, 
                        &cbRet, 
                        &Pipe[i].oOverlap); 

                // The write operation completed successfully. 
                if (fSuccess && cbRet == Pipe[i].cbToWrite) 
                { 
                    Pipe[i].fPendingIO = FALSE; 
                    Pipe[i].dwState = READING_STATE; 
                    Pipe[i].cbToWrite = 0; //All data is writen
                    continue; 
                } 

                // The write operation is still pending. 
                dwErr = GetLastError(); 
                if (! fSuccess && (dwErr == ERROR_IO_PENDING)) 
                { 
                    Pipe[i].fPendingIO = TRUE; 
                    continue; 
                } 
                else 
                {
                    sprintf_s(pTemp, "PipeManager: WRITING_STATE error 0x%x.\n", dwErr);
                    WriteLog(pTemp);
                }

                // An error occurred; disconnect from the client. 
                DisconnectAndReconnect(i); 
                break; 

            default: 
                {
                    sprintf_s(pTemp, "PipeManager: Invalid pipe state.\n"); 
                    WriteLog(pTemp);
                    return 0;
                }
        } 
    } 

    return 0;


}


// DisconnectAndReconnect(DWORD) 
// This function is called when an error occurs or when the client 
// closes its handle to the pipe. Disconnect from this client, then 
// call ConnectNamedPipe to wait for another client to connect. 
VOID DisconnectAndReconnect(DWORD i) 
{ 
    char pTemp[121];
    // Disconnect the pipe instance. 

    if (! DisconnectNamedPipe(Pipe[i].hPipeInst) ) 
    {
        sprintf_s(pTemp, "DisconnectNamedPipe failed with %d.\n", GetLastError());
        WriteLog(pTemp);
    }

    // Call a subroutine to connect to the new client. 
    Pipe[i].fPendingIO = ConnectToNewClient( 
            Pipe[i].hPipeInst, 
            &Pipe[i].oOverlap); 

    Pipe[i].dwState = Pipe[i].fPendingIO ? 
        CONNECTING_STATE : // still connecting 
        READING_STATE;     // ready to read 
} 

// ConnectToNewClient(HANDLE, LPOVERLAPPED) 
// This function is called to start an overlapped connect operation. 
// It returns TRUE if an operation is pending or FALSE if the 
// connection has been completed. 
BOOL ConnectToNewClient(HANDLE hPipe, LPOVERLAPPED lpo) 
{ 
    BOOL fConnected, fPendingIO = FALSE; 
    char pTemp[121];

    // clear the OVERLAPPED struct 
    lpo->Offset = 0;
    lpo->OffsetHigh = 0;

    // Start an overlapped connection for this pipe instance. 
    fConnected = ConnectNamedPipe(hPipe, lpo); 

    // Overlapped ConnectNamedPipe should return zero. 
    if (fConnected) 
    {
        sprintf_s(pTemp, "PipeManager: ConnectNamedPipe failed with 0x%x.\n", GetLastError());
        WriteLog(pTemp);
        return 0;
    }

    switch (GetLastError()) 
    { 
        // The overlapped connection in progress. 
        case ERROR_IO_PENDING: 
            fPendingIO = TRUE; 
            break; 

            // Client is already connected, so signal an event. 
        case ERROR_PIPE_CONNECTED: 
            if (SetEvent(lpo->hEvent)) 
                break; 

            // If an error occurs during the connect operation... 
        default: 
            {
                sprintf_s(pTemp, "PipeManager: ConnectNamedPipe failed with 0x%x.\n", GetLastError());
                WriteLog(pTemp);
                return 0;
            }
    } 

    Pipe->cbToWrite = 0; // Junk old data
    return fPendingIO; 
}

