////////////////////////////////////////////////////////////////////// 
// VirtualBox VMs - managed by an NT Service (VBoxVmService)
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <windows.h>
#include <winbase.h>
#include <winsvc.h>
#include <process.h>


const int nBufferSize = 500;
char pServiceName[nBufferSize+1];
char pExeFile[nBufferSize+1];
char pInitFile[nBufferSize+1];
char pLogFile[nBufferSize+1];
const int nMaxProcCount = 100;
BOOL bServiceRunning;

SERVICE_STATUS          serviceStatus; 
SERVICE_STATUS_HANDLE   hServiceStatusHandle; 

VOID WINAPI VBoxVmServiceMain( DWORD dwArgc, LPTSTR *lpszArgv );
VOID WINAPI VBoxVmServiceHandler( DWORD fdwControl );

void WriteLog(char* pMsg)
{
    // write error or other information into log file
    try
    {
        SYSTEMTIME oT;
        ::GetLocalTime(&oT);
        FILE* pLog = fopen(pLogFile,"a");
        fprintf(pLog,"%02d/%02d/%04d, %02d:%02d:%02d - %s\n",oT.wMonth,oT.wDay,oT.wYear,oT.wHour,oT.wMinute,oT.wSecond,pMsg); 
        fclose(pLog);
    } catch(...) {}
}

////////////////////////////////////////////////////////////////////// 
//
// Configuration Data and Tables
//

SERVICE_TABLE_ENTRY   DispatchTable[] = 
{ 
    {pServiceName, VBoxVmServiceMain}, 
    {NULL, NULL}
};


// helper functions
// start a VM with given index
BOOL StartProcess(int nIndex) 
{ 
    char pItem[nBufferSize+1];
    sprintf_s(pItem,"Vm%d\0",nIndex);

    // get VmName
    char pVmName[nBufferSize+1];
    GetPrivateProfileString(pItem,"VmName","",pVmName,nBufferSize,pInitFile);
    if (strlen(pVmName) == 0)
        return FALSE;

    // open handle to VBoxVmService.log
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof (SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    HANDLE hStdOut = CreateFile(pLogFile, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    char pTemp[121];
    sprintf_s(pTemp,"Starting VM : '%s'", pVmName); 
    WriteLog(pTemp);

    // get working dir
    char pWorkingDir[nBufferSize+1];
    GetPrivateProfileString(pItem,"WorkingDir","",pWorkingDir,nBufferSize,pInitFile);

    // get VrdpPort
    char pVrdpPort[nBufferSize+1];
    GetPrivateProfileString(pItem,"VrdpPort","",pVrdpPort,nBufferSize,pInitFile);

    // get full path to VBoxManage.exe
    char pVBoxManagePath[nBufferSize+1];
    if (GetEnvironmentVariable("VBOX_INSTALL_PATH", pVBoxManagePath, nBufferSize) == 0)
    {
        sprintf_s(pTemp, "Failed to start VM: VBOX_INSTALL_PATH not found."); 
        WriteLog(pTemp);
        return FALSE;
    }
    strcat(pVBoxManagePath, "VBoxManage.exe");

    // prepare for creating process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.lpDesktop = "";
    si.hStdOutput = hStdOut;

    // run VBoxManage.exe showvminfo VmName
    char pCommandLine[nBufferSize+1];
    sprintf(pCommandLine, "\"%s\" showvminfo \"%s\"", pVBoxManagePath, pVmName);
    PROCESS_INFORMATION pProcInfo;
    if(!CreateProcess(NULL,pCommandLine,NULL,NULL,TRUE,NORMAL_PRIORITY_CLASS,NULL,strlen(pWorkingDir)==0?NULL:pWorkingDir,&si,&pProcInfo))
    {
        long nError = GetLastError();
        sprintf_s(pTemp,"Failed to start VM using command: '%s', error code = %d", pCommandLine, nError); 
        WriteLog(pTemp);
        return FALSE;
    }
    WaitForSingleObject(pProcInfo.hProcess, INFINITE);
    CloseHandle(pProcInfo.hProcess);
    CloseHandle(pProcInfo.hThread);

    // run VBoxManage.exe modifyvm VmName --vrdpport
    sprintf(pCommandLine, "\"%s\" modifyvm \"%s\" --vrdpport %s", pVBoxManagePath, pVmName, pVrdpPort);
    if(!CreateProcess(NULL,pCommandLine,NULL,NULL,TRUE,NORMAL_PRIORITY_CLASS,NULL,strlen(pWorkingDir)==0?NULL:pWorkingDir,&si,&pProcInfo))
    {
        long nError = GetLastError();
        sprintf_s(pTemp,"Failed to start VM using command: '%s', error code = %d", pCommandLine, nError); 
        WriteLog(pTemp);
        return FALSE;
    }
    WaitForSingleObject(pProcInfo.hProcess, INFINITE);
    CloseHandle(pProcInfo.hProcess);
    CloseHandle(pProcInfo.hThread);

    // run VBoxManage.exe startvm VmName --type vrdp
    sprintf(pCommandLine, "\"%s\" startvm \"%s\" --type vrdp", pVBoxManagePath, pVmName);
    if(!CreateProcess(NULL,pCommandLine,NULL,NULL,TRUE,NORMAL_PRIORITY_CLASS,NULL,strlen(pWorkingDir)==0?NULL:pWorkingDir,&si,&pProcInfo))
    {
        long nError = GetLastError();
        sprintf_s(pTemp,"Failed to start VM using command: '%s', error code = %d", pCommandLine, nError); 
        WriteLog(pTemp);
        return FALSE;
    }
    WaitForSingleObject(pProcInfo.hProcess, INFINITE);
    CloseHandle(pProcInfo.hProcess);
    CloseHandle(pProcInfo.hThread);

    CloseHandle(hStdOut);
    // wait for VM starting up
    char pPause[nBufferSize+1];
    GetPrivateProfileString(pItem,"PauseStartup","1000",pPause,nBufferSize,pInitFile);
    Sleep(atoi(pPause));
    return TRUE;
}


// end a VM with given index
BOOL EndProcess(int nIndex) 
{ 
    char pItem[nBufferSize+1];
    sprintf_s(pItem,"Vm%d\0",nIndex);

    // get VmName
    char pVmName[nBufferSize+1];
    GetPrivateProfileString(pItem,"VmName","",pVmName,nBufferSize,pInitFile);
    if (strlen(pVmName) == 0)
        return FALSE;

    // get working dir
    char pWorkingDir[nBufferSize+1];
    GetPrivateProfileString(pItem,"WorkingDir","",pWorkingDir,nBufferSize,pInitFile);

    // get ShutdownMethod
    char pShutdownMethod[nBufferSize+1];
    GetPrivateProfileString(pItem,"ShutdownMethod","",pShutdownMethod,nBufferSize,pInitFile);

    // open handle to VBoxVmService.log
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof (SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    HANDLE hStdOut = CreateFile(pLogFile, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    char pTemp[121];
    sprintf_s(pTemp,"Shutting down VM : '%s' with option %s", pVmName, pShutdownMethod); 
    WriteLog(pTemp);

    // get full path to VBoxManage.exe
    char pVBoxManagePath[nBufferSize+1];
    if (GetEnvironmentVariable("VBOX_INSTALL_PATH", pVBoxManagePath, nBufferSize) == 0)
    {
        sprintf_s(pTemp, "Failed to stop VM: VBOX_INSTALL_PATH not found."); 
        WriteLog(pTemp);
        return FALSE;
    }
    strcat(pVBoxManagePath, "VBoxManage.exe");

    // prepare for creating process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.lpDesktop = "";
    si.hStdOutput = hStdOut;

    // run VBoxManage.exe controlvm VmName ShutdownMethod
    char pCommandLine[nBufferSize+1];
    sprintf(pCommandLine, "\"%s\" controlvm \"%s\" %s", pVBoxManagePath, pVmName, pShutdownMethod);
    PROCESS_INFORMATION pProcInfo;
    if(!CreateProcess(NULL,pCommandLine,NULL,NULL,TRUE,NORMAL_PRIORITY_CLASS,NULL,strlen(pWorkingDir)==0?NULL:pWorkingDir,&si,&pProcInfo))
    {
        long nError = GetLastError();
        sprintf_s(pTemp,"Failed to stop VM using command: '%s', error code = %d", pCommandLine, nError); 
        WriteLog(pTemp);
        return FALSE;
    }
    WaitForSingleObject(pProcInfo.hProcess, INFINITE);
    CloseHandle(pProcInfo.hProcess);
    CloseHandle(pProcInfo.hThread);

    CloseHandle(hStdOut);
    // wait for VM shutting down
    char pPause[nBufferSize+1];
    GetPrivateProfileString(pItem,"PauseShutdown","1000",pPause,nBufferSize,pInitFile);
    Sleep(atoi(pPause));
    return TRUE;
}



////////////////////////////////////////////////////////////////////// 
//
// This routine gets used to start your service
//
VOID WINAPI VBoxVmServiceMain( DWORD dwArgc, LPTSTR *lpszArgv )
{
    DWORD   status = 0; 
    DWORD   specificError = 0xfffffff; 

    serviceStatus.dwServiceType        = SERVICE_WIN32; 
    serviceStatus.dwCurrentState       = SERVICE_START_PENDING; 
    serviceStatus.dwControlsAccepted   = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE; 
    serviceStatus.dwWin32ExitCode      = 0; 
    serviceStatus.dwServiceSpecificExitCode = 0; 
    serviceStatus.dwCheckPoint         = 0; 
    serviceStatus.dwWaitHint           = 0; 

    hServiceStatusHandle = RegisterServiceCtrlHandler(pServiceName, VBoxVmServiceHandler); 
    if (hServiceStatusHandle==0) 
    {
        long nError = GetLastError();
        char pTemp[121];
        sprintf_s(pTemp, "RegisterServiceCtrlHandler failed, error code = %d", nError);
        WriteLog(pTemp);
        return; 
    } 

    // Initialization complete - report running status 
    serviceStatus.dwCurrentState       = SERVICE_RUNNING; 
    serviceStatus.dwCheckPoint         = 0; 
    serviceStatus.dwWaitHint           = 0;  
    if(!SetServiceStatus(hServiceStatusHandle, &serviceStatus)) 
    { 
        long nError = GetLastError();
        char pTemp[121];
        sprintf_s(pTemp, "SetServiceStatus failed, error code = %d", nError);
        WriteLog(pTemp);
    } 

    for(int i=0;i<nMaxProcCount;i++)
    {
        if (!StartProcess(i))
            break;
    }
}

////////////////////////////////////////////////////////////////////// 
//
// This routine responds to events concerning your service, like start/stop
//
VOID WINAPI VBoxVmServiceHandler(DWORD fdwControl)
{
    switch(fdwControl) 
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            serviceStatus.dwWin32ExitCode = 0; 
            serviceStatus.dwCurrentState  = SERVICE_STOPPED; 
            serviceStatus.dwCheckPoint    = 0; 
            serviceStatus.dwWaitHint      = 0;
            bServiceRunning = FALSE;
            // terminate all processes started by this service before shutdown
            {
                for(int i=nMaxProcCount-1;i>=0;i--)
                {
                    EndProcess(i);
                }			
                if (!SetServiceStatus(hServiceStatusHandle, &serviceStatus))
                { 
                    long nError = GetLastError();
                    char pTemp[121];
                    sprintf_s(pTemp, "SetServiceStatus failed, error code = %d", nError);
                    WriteLog(pTemp);
                }
            }
            return; 
        case SERVICE_CONTROL_PAUSE:
            serviceStatus.dwCurrentState = SERVICE_PAUSED; 
            break;
        case SERVICE_CONTROL_CONTINUE:
            serviceStatus.dwCurrentState = SERVICE_RUNNING; 
            break;
        case SERVICE_CONTROL_INTERROGATE:
            break;
        default: 
            // bounce processes started by this service
            if(fdwControl>=128&&fdwControl<256)
            {
                int nIndex = fdwControl&0x7F;
                // bounce a single process
                if(nIndex>=0&&nIndex<nMaxProcCount)
                {
                    EndProcess(nIndex);
                    StartProcess(nIndex);
                }
                // bounce all processes
                else if(nIndex==127)
                {
                    for(int i=nMaxProcCount-1;i>=0;i--)
                    {
                        EndProcess(i);
                    }
                    for(int i=0;i<nMaxProcCount;i++)
                    {
                        if (!StartProcess(i))
                            break;
                    }
                }
            }
            else
            {
                long nError = GetLastError();
                char pTemp[121];
                sprintf_s(pTemp,  "Unrecognized opcode %d", fdwControl);
                WriteLog(pTemp);
            }
    };
    if (!SetServiceStatus(hServiceStatusHandle,  &serviceStatus)) 
    { 
        long nError = GetLastError();
        char pTemp[121];
        sprintf_s(pTemp, "SetServiceStatus failed, error code = %d", nError);
        WriteLog(pTemp);
    } 
}


void WorkerProc(void* pParam)
{
    const int nMessageSize = 80;
    char pTemp[121];

    // create named pipe
    SECURITY_ATTRIBUTES sa;
    sa.lpSecurityDescriptor = (PSECURITY_DESCRIPTOR)malloc(SECURITY_DESCRIPTOR_MIN_LENGTH);
    if (!InitializeSecurityDescriptor(sa.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
    {
        DWORD er = ::GetLastError();
        sprintf_s(pTemp, "InitializeSecurityDescriptor failed, error code = %d", er);
        WriteLog(pTemp);
        return;
    }
    if (!SetSecurityDescriptorDacl(sa.lpSecurityDescriptor, TRUE, (PACL)0, FALSE))
    {
        DWORD er = ::GetLastError();
        sprintf_s(pTemp, "SetSecurityDescriptorDacl failed, error code = %d", er);
        WriteLog(pTemp);
        return;
    }
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;

    HANDLE hPipe = ::CreateNamedPipe((LPSTR)"\\\\.\\pipe\\VBoxVmService",
            PIPE_ACCESS_INBOUND, PIPE_TYPE_MESSAGE | 
            PIPE_READMODE_MESSAGE | PIPE_WAIT, 
            PIPE_UNLIMITED_INSTANCES, nMessageSize, 
            nMessageSize, NMPWAIT_USE_DEFAULT_WAIT, &sa);

    if (hPipe == INVALID_HANDLE_VALUE)
    {
        DWORD dwError = ::GetLastError();
        sprintf_s(pTemp, "CreateNamedPipe failed, error code = %d", dwError);
        WriteLog(pTemp);
        return;
    }

    bServiceRunning = TRUE;

    // waiting for commands
    while (bServiceRunning)
    {
        BOOL bResult = ::ConnectNamedPipe(hPipe, 0);

        if (bResult || GetLastError() == ERROR_PIPE_CONNECTED)
        {
            char buffer[80]; 
            memset(buffer, 0, 80);
            DWORD read = 0;

            if (!(::ReadFile(hPipe, buffer, 80, &read, 0)))
            {
                unsigned int error = GetLastError(); 
            } 
            else
            {
                sprintf_s(pTemp, "Received control command: %s", buffer);
                WriteLog(pTemp);

                // process command
                if (strncmp(buffer, "start", 5) == 0)
                {
                    int nIndex = atoi(buffer + 6);
                    StartProcess(nIndex);
                }
                else if (strncmp(buffer, "stop", 4) == 0)
                {
                    int nIndex = atoi(buffer + 5);
                    EndProcess(nIndex);
                }
                else if (strncmp(buffer, "status", 6) == 0)
                {
                    int nIndex = atoi(buffer + 7);
                    // Todo: check status and write back to Pipe
                }
            }
            ::DisconnectNamedPipe(hPipe);
        }
        // 
        ::Sleep(500);
    }
}

////////////////////////////////////////////////////////////////////// 
//
// Standard C Main
//
void main(int argc, char *argv[] )
{
    // initialize variables for .exe, .ini, and .log file names
    char pModuleFile[nBufferSize+1];
    DWORD dwSize = GetModuleFileName(NULL,pModuleFile,nBufferSize);
    pModuleFile[dwSize] = 0;
    if(dwSize>4&&pModuleFile[dwSize-4]=='.')
    {
        sprintf_s(pExeFile,"%s",pModuleFile);
        pModuleFile[dwSize-4] = 0;
        sprintf_s(pInitFile,"%s.ini",pModuleFile);
        sprintf_s(pLogFile,"%s.log",pModuleFile);
    }
    else
    {
        printf("Invalid module file name: %s\r\n", pModuleFile);
        return;
    }
    WriteLog(pExeFile);
    WriteLog(pInitFile);
    WriteLog(pLogFile);
    // read service name from .ini file
    GetPrivateProfileString("Settings","ServiceName","VBoxVmService",pServiceName,nBufferSize,pInitFile);
    WriteLog(pServiceName);

    // start a worker thread to check for pipe messages
    if(_beginthread(WorkerProc, 0, NULL)==-1)
    {
        long nError = GetLastError();
        char pTemp[121];
        sprintf_s(pTemp, "_beginthread failed, error code = %d", nError);
        WriteLog(pTemp);
    }

    // pass dispatch table to service controller
    if(!StartServiceCtrlDispatcher(DispatchTable))
    {
        long nError = GetLastError();
        char pTemp[121];
        sprintf_s(pTemp, "StartServiceCtrlDispatcher failed, error code = %d", nError);
        WriteLog(pTemp);
    }
    // you don't get here unless the service is shutdown
}

