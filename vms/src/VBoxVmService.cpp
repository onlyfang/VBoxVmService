////////////////////////////////////////////////////////////////////// 
// VirtualBox VMs - managed by an NT Service (VBoxVmService)
//////////////////////////////////////////////////////////////////////

#include <atlbase.h>
#include <stdio.h>
#include <windows.h>
#include <winbase.h>
#include <winsvc.h>
#include <process.h>
#include <strsafe.h>
#include <time.h>
#include <stdarg.h>
#include <io.h>
#include <winerror.h>
#include "VirtualBox.h"

#include "VBoxVmPipeManager.h"


const int nBufferSize = 500;
char pServiceName[nBufferSize+1];
char pExeFile[nBufferSize+1];
char pInitFile[nBufferSize+1];
char pLogFile[nBufferSize+1];
IVirtualBox *virtualBox;
ISession *session = NULL;
HANDLE ghVMStoppedEvent = NULL;

SERVICE_STATUS          serviceStatus; 
SERVICE_STATUS_HANDLE   hServiceStatusHandle; 

VOID WINAPI VBoxVmServiceMain( DWORD dwArgc, LPTSTR *lpszArgv );
VOID WINAPI VBoxVmServiceHandler( DWORD fdwControl );

#define SAFE_RELEASE(x) \
    if (x) { \
        x->Release(); \
        x = NULL; \
    }

void WriteLog(char* pMsg)
{
    // write error or other information into log file
    try
    {
        SYSTEMTIME oT;
        ::GetLocalTime(&oT);
        FILE *pLog;
        if (fopen_s(&pLog, pLogFile, "at") == 0)
        {
            fprintf(pLog,"%02d/%02d/%04d, %02d:%02d:%02d - %s\n",oT.wMonth,oT.wDay,oT.wYear,oT.wHour,oT.wMinute,oT.wSecond,pMsg); 
            fclose(pLog);
        }
    } catch(...) {}
}

// Write to both the pipe and log
void WriteLogPipe(LPPIPEINST pipe, LPCSTR formatstring, ...){

    char pTemp[nBufferSize+1];
    DWORD pLen;
    va_list args;
    va_start(args, formatstring);

    pLen = vsnprintf_s(pTemp, sizeof(pTemp), nBufferSize, formatstring, args);

    //append to log
    WriteLog(pTemp);

    //write back to pipe
    if (pipe)
    {
        strncpy_s(pipe->chReply, BUFSIZE, pTemp, pLen);
        pipe->cbToWrite = pLen;
    }
}

BOOL CtrlHandler( DWORD fdwCtrlType ) 
{ 
    if (fdwCtrlType == CTRL_SHUTDOWN_EVENT)
    {
        WriteLog("Received system shutdown event.");        

        // tell PipeManager to exit.
        PipeManagerExit();

        // wait till all VMs stopped, but we must respond in 20 seconds
        // in the control hander, use 15 seconds for safety here.
        if (WaitForSingleObject(ghVMStoppedEvent, 15000) == WAIT_OBJECT_0)
            WriteLog("Received all VM stopped event.");
    }
    return FALSE; 
} 

// Routine to make human readable description, instead of just the error id.
char *ErrorString(DWORD err)
{
    const DWORD buffsize = 300+1;
    static char buff[buffsize];

    if(FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                NULL,
                err,
                0,
                buff,
                buffsize,
                NULL) == 0)
    { 
        // FormatMessage failed
        sprintf_s(buff,"Unknown error with error code = %d", err);
    }
    return buff;
} // ErrorString

////////////////////////////////////////////////////////////////////// 
//
// Configuration Data and Tables
//

SERVICE_TABLE_ENTRY   DispatchTable[] = 
{ 
    {pServiceName, VBoxVmServiceMain}, 
    {NULL, NULL}
};


char *nIndexToVmName(int nIndex) {
    char pItem[nBufferSize+1];
    static char pVmName[nBufferSize+1];

    sprintf_s(pItem,"Vm%d\0",nIndex);

    // get VmName
    GetPrivateProfileString(pItem,"VmName","",pVmName,nBufferSize,pInitFile);
    if (strlen(pVmName) == 0) {
        return NULL;
    }

    return pVmName;
}

//***********************************************************
//  Run a console app by calling the pCommandLine.
//  The stdout is loged to the VBoxVmService.log logfile. 
//  The process ID of the started process is returned
//***********************************************************

DWORD RunConsoleApp(char pCommandLine[]) 
{ 
    HANDLE hStdOut = NULL;
    char pTemp[nBufferSize+1];

    // Set the bInheritHandle flag so pipe handles are inherited.
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof (SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
    // open handle to VBoxVmService.log
    hStdOut = CreateFile(pLogFile, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);


    sprintf_s(pTemp,"Running command: '%s'.", pCommandLine); 
    WriteLog(pTemp);

    // prepare for creating process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.lpDesktop = "";
    si.hStdOutput = hStdOut;

    // Run the command
    PROCESS_INFORMATION pProcInfo;
    if(!CreateProcess(NULL,pCommandLine,NULL,NULL,TRUE,NORMAL_PRIORITY_CLASS,NULL,NULL,&si,&pProcInfo))
    {
        return -1;
    }

    WaitForInputIdle(pProcInfo.hProcess, 1000);
    CloseHandle(pProcInfo.hProcess);
    CloseHandle(pProcInfo.hThread);

    return pProcInfo.dwProcessId;
}


// Get VM status with given index
void VMStatus(int nIndex, LPPIPEINST pipe)
{
    char *vm;

    // force ini file reload
    WritePrivateProfileString(NULL, NULL, NULL, pInitFile);

    vm = nIndexToVmName( nIndex );
    if (vm == NULL) 
    {
        WriteLogPipe(pipe, "Unknown VM index number. Are you sure it is defined in the VBoxVmService.ini file?"); 
        return; 
    }

    CComBSTR vmName(vm);
    HRESULT rc;
    IMachine *machine = NULL;

    rc = virtualBox->FindMachine(vmName, &machine);

    if (FAILED(rc))
    {
        WriteLogPipe(pipe,"Error finding machine! rc = 0x%x", rc); 
    }
    else
    {
        MachineState state;

        rc = machine->get_State(&state);
        if (!SUCCEEDED(rc))
        {
            WriteLogPipe(pipe, "Error retrieving machine state! rc = 0x%x", rc);
        }
        else
        {
            switch (state) {
                case MachineState_PoweredOff:
                    WriteLogPipe(pipe, "VM %s is powered off.", vm);
                    break;
                case MachineState_Saved:
                    WriteLogPipe(pipe, "VM %s is saved.", vm);
                    break;
                case MachineState_Aborted:
                    WriteLogPipe(pipe, "VM %s is aborted.", vm);
                    break;
                case MachineState_Running:
                    WriteLogPipe(pipe, "VM %s is running.", vm);
                    break;
                case MachineState_Paused:
                    WriteLogPipe(pipe, "VM %s is paused.", vm);
                    break;
                default:
                    WriteLogPipe(pipe, "VM %s is in unknown state %d.", vm, state);
                    break;
            }
        }

        SAFE_RELEASE(machine);
    }

}

// start a VM with given name
// return true on success
// return false on failure
bool StartVM(char *vm, LPPIPEINST pipe)
{
    CComBSTR vmName(vm);
    HRESULT rc;
    IMachine *machine = NULL;
    bool result = false;

    rc = virtualBox->FindMachine(vmName, &machine);

    if (FAILED(rc))
    {
        WriteLogPipe(pipe,"Error finding machine! rc = 0x%x", rc); 
    }
    else
    {
        BSTR sessiontype = SysAllocString(L"headless");
        IProgress *progress = NULL;

        do
        {
            // Start a VM session using the delivered VBox GUI.
            rc = machine->LaunchVMProcess(session, sessiontype,
                    NULL, &progress);
            if (!SUCCEEDED(rc))
            {
                WriteLogPipe(pipe, "Could not open remote session! rc = 0x%x", rc);
                break;
            }
            result = true;

            // Wait until VM is running. 
            rc = progress->WaitForCompletion (-1);
            WriteLogPipe(pipe, "VM %s is started up.", vm);

            // Close the session.
            rc = session->UnlockMachine();

        } while (0);

        SAFE_RELEASE(progress);
        SysFreeString(sessiontype);
        SAFE_RELEASE(machine);
    }
    return result;
}


// start a VM with given index
BOOL StartProcess(int nIndex, LPPIPEINST pipe) 
{ 
    char *cp;

    if (pipe)  // when called from VmServiceControl, reload ini file
        WritePrivateProfileString(NULL, NULL, NULL, pInitFile);

    cp = nIndexToVmName( nIndex );
    if (cp == NULL) 
    {
        if (pipe)  // this message is invalid when StartProcess is called during service start, not by control command
            WriteLogPipe(pipe, "Unknown VM index number. Are you sure it is defined in the VBoxVmService.ini file?"); 
        return false; 
    }

    // if called during service start, check if AutoStart is disabled
    if (pipe == NULL)
    {
        char pAutoStart[nBufferSize+1];
        char pItem[nBufferSize+1];

        // get AutoStart
        sprintf_s(pItem,"Vm%d\0",nIndex);
        GetPrivateProfileString(pItem,"AutoStart","",pAutoStart,nBufferSize,pInitFile);
        if (_stricmp(pAutoStart, "no") == 0)
            return true;
    }

    return StartVM(cp, pipe);

}

// stop a VM with given name
// return value:
// -1: error happened when trying to stop VM
//  0: VM is not running, no need to stop 
//  1: VM stopped successfully
int StopVM(char *vm, char *method, LPPIPEINST pipe)
{
    CComBSTR vmName(vm);
    HRESULT rc;
    IMachine *machine = NULL;
    int result = -1;

    rc = virtualBox->FindMachine(vmName, &machine);

    if (FAILED(rc))
    {
        WriteLogPipe(pipe,"Error finding machine! rc = 0x%x", rc); 
    }
    else
    {
        IConsole *console = NULL;
        IProgress *progress = NULL;

        do
        {
            MachineState vmstate;
            rc = machine->get_State(&vmstate); // Get the state of the machine.
            if (!SUCCEEDED(rc))
            {
                WriteLogPipe(pipe, "Error retrieving machine state! rc = 0x%x", rc);
                break;
            }

            // check for running state
            if( MachineState_Running != vmstate && 
                    MachineState_Starting != vmstate ) 
            {
                if (pipe)
                     WriteLogPipe(pipe, "VM %s is not running.", vm);
                result = 0;
                break;
            }

            // Lock the machine for session 
            rc = machine->LockMachine(session, LockType_Shared);
            if (!SUCCEEDED(rc))
            {
                WriteLogPipe(pipe, "Could not lock machine! rc = 0x%x", rc);
                break;
            }

            // Get console object. 
            session->get_Console(&console);

            if (strcmp(method, "savestate") == 0)
                rc = console->SaveState(&progress);
            else
                rc = console->PowerButton();
            if (!SUCCEEDED(rc))
            {
                WriteLogPipe(pipe, "Could not stop machine %s! rc = 0x%x", rc, vm);
                break;
            }

            result = 1;

            WriteLogPipe(pipe, "VM %s is being shutdown.", vm);

            // Close the session.
            rc = session->UnlockMachine();

        } while (0);

        SAFE_RELEASE(progress);
        SAFE_RELEASE(console);
        SAFE_RELEASE(machine);
    }
    return result;
}

// end a VM with given index
// return value:
// -1: error happened when trying to stop VM
//  0: VM is not running, no need to stop 
//  1: VM stopped successfully
int EndProcess(int nIndex, LPPIPEINST pipe) 
{
    char pItem[nBufferSize+1];
    char pShutdownMethod[nBufferSize+1];
    char *cp;


    if (pipe)  // when called from VmServiceControl, reload ini file
        WritePrivateProfileString(NULL, NULL, NULL, pInitFile);

    cp = nIndexToVmName( nIndex );
    if (cp==NULL)
    {
        if (pipe)
            WriteLogPipe(pipe, "Unknown VM index number. Are you sure it is defined in the VBoxVmService.ini file?"); 
        return -1;
    }

    // get ShutdownMethod
    sprintf_s(pItem,"Vm%d\0",nIndex);
    GetPrivateProfileString(pItem,"ShutdownMethod","",pShutdownMethod,nBufferSize,pInitFile);

    return StopVM(cp, pShutdownMethod, pipe);
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

            WriteLog("Received service shutdown event."); 
            //tell PipeManager to exit.
            PipeManagerExit();
            // also tell SCM we'll need some time to shutdown
            serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            {
                char pPause[nBufferSize+1];
                GetPrivateProfileString("Settings","PauseShutdown","1000",pPause,nBufferSize,pInitFile);
                serviceStatus.dwWaitHint = atoi(pPause);  
            }
            break;
        case SERVICE_CONTROL_PAUSE:
            serviceStatus.dwCurrentState = SERVICE_PAUSED; 
            break;
        case SERVICE_CONTROL_CONTINUE:
            serviceStatus.dwCurrentState = SERVICE_RUNNING; 
            break;
        case SERVICE_CONTROL_INTERROGATE:
            break;
        default: 
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


void WorkerHandleCommand(LPPIPEINST pipe) 
{
    char pTemp[121];
    char buffer[80]; 

    strncpy_s(buffer, 80, pipe->chRequest, strlen(pipe->chRequest));

    sprintf_s(pTemp, "Received control command: %s", buffer);
    WriteLog(pTemp);

    // process command
    if (strncmp(buffer, "start", 5) == 0)
    {
        int nIndex = atoi(buffer + 6);
        StartProcess(nIndex,pipe);
    }
    else if (strncmp(buffer, "stop", 4) == 0)
    {
        int nIndex = atoi(buffer + 5);
        EndProcess(nIndex,pipe);
    }
    else if (strncmp(buffer, "status", 6) == 0)
    {
        int nIndex = atoi(buffer + 7);
        VMStatus(nIndex, pipe);
    }
    else if (strncmp(buffer, "shutdown", 8) == 0)
    {
        VBoxVmServiceHandler(SERVICE_CONTROL_SHUTDOWN);
    }
    else {
        printf("Unknown command \"%s\"\n", buffer);
    }
}

void listVMs(IVirtualBox *virtualBox)
{
    HRESULT rc;

    SAFEARRAY *machinesArray = NULL;
    WriteLog("List all the VMs found by VBoxVmService"); 

    rc = virtualBox->get_Machines(&machinesArray);
    if (SUCCEEDED(rc))
    {
        IMachine **machines;
        rc = SafeArrayAccessData (machinesArray, (void **) &machines);
        if (SUCCEEDED(rc))
        {
            for (ULONG i = 0; i < machinesArray->rgsabound[0].cElements; ++i)
            {
                BSTR str;

                rc = machines[i]->get_Name(&str);
                if (SUCCEEDED(rc))
                {
                    char pTemp[nBufferSize+1];
                    sprintf_s(pTemp, "Name: %S", str); 
                    SysFreeString(str);

                    MachineState state;
                    rc = machines[i]->get_State(&state);
                    if (SUCCEEDED(rc))
                    {
                        switch (state) {
                            case MachineState_PoweredOff:
                                strcat_s(pTemp, ", powered off");
                                break;
                            case MachineState_Saved:
                                strcat_s(pTemp, ", saved");
                                break;
                            case MachineState_Aborted:
                                strcat_s(pTemp, ", aborted");
                                break;
                            case MachineState_Running:
                                strcat_s(pTemp, ", running");
                                break;
                            case MachineState_Paused:
                                strcat_s(pTemp, ", paused");
                                break;
                            default:
                                strcat_s(pTemp, ", in unknown state");
                                break;
                        }
                    }
                    WriteLog(pTemp);
                }
            }

            SafeArrayUnaccessData (machinesArray);
        }

        SafeArrayDestroy (machinesArray);
    }
}

unsigned __stdcall WorkerProc(void* pParam)
{
    ghVMStoppedEvent = CreateEvent(
            NULL,    // default security attributes
            TRUE,    // manual reset event
            FALSE,   // not signaled
            NULL);   // no name
    if (ghVMStoppedEvent == NULL)
    {
        WriteLog("Failed to create VM stop event!");
        goto end;
    }

    HRESULT rc;
    rc = CoInitialize(NULL);

    rc = CoCreateInstance(CLSID_VirtualBox,
            NULL,
            CLSCTX_LOCAL_SERVER,
            IID_IVirtualBox,
            (void**)&virtualBox);
    if (!SUCCEEDED(rc))
    {
        char pTemp[121];
        sprintf_s(pTemp, "Error creating VirtualBox instance! rc = 0x%x", rc);
        WriteLog(pTemp);
        goto end;
    }

    // Create the session object.
    rc = CoCreateInstance(CLSID_Session, // the VirtualBox base object
            NULL,          // no aggregation
            CLSCTX_INPROC_SERVER, // the object lives in a server process on this machine
            IID_ISession,  // IID of the interface
            (void**)&session);
    if (!SUCCEEDED(rc))
    {
        virtualBox->Release();
        char pTemp[121];
        sprintf_s(pTemp, "Error creating Session instance! rc = 0x%x", rc);
        WriteLog(pTemp);
        goto end;
    }

    listVMs(virtualBox);

    // start all configured VMs
    BOOL bShouldWait = FALSE;
    int i = 0;
    while (true)
    {
        if (!StartProcess(i, NULL))
            break;
        bShouldWait = true;
        i++;
    }

    // wait for VMs to start up
    if (bShouldWait)
    {
        char pPause[nBufferSize+1];
        GetPrivateProfileString("Settings","PauseStartup","1000",pPause,nBufferSize,pInitFile);
        Sleep(atoi(pPause));
    }

    // start up web service if required
    DWORD vBoxWebSrvPId = -1;
    char pRunWebService[nBufferSize+1];
    GetPrivateProfileString("Settings","RunWebService","",pRunWebService,nBufferSize,pInitFile);
    if (_stricmp(pRunWebService, "yes") == 0) {
        WriteLog("Start up VirtualBox web service");

        // get full path to VBoxWebSrv.exe
        char pVBoxWebSrvPath[nBufferSize+1];
        if (GetEnvironmentVariable("VBOX_INSTALL_PATH", pVBoxWebSrvPath, nBufferSize) == 0)
        {
            WriteLog("VBOX_INSTALL_PATH not found.");        
        }
        else
        {
            strcat_s(pVBoxWebSrvPath, nBufferSize, "VBoxWebSrv.exe");
            vBoxWebSrvPId = RunConsoleApp(pVBoxWebSrvPath);
        }
    }

    //PipeManager will handel all pipe connections
    PipeManager("\\\\.\\pipe\\VBoxVmService", WorkerHandleCommand);

    //gracefully ending the service
    bShouldWait = FALSE;
    // terminate all running processes before shutdown
    i = 0;
    while (true)
    {
        int result = EndProcess(i, NULL);
        if (result == -1)
            break;
        if (result == 1)
            bShouldWait = TRUE;
        i++;
    }           
    SAFE_RELEASE(session);

    // wait for VMs to shut down
    if (bShouldWait)
    {
        char pPause[nBufferSize+1];
        GetPrivateProfileString("Settings","PauseShutdown","1000",pPause,nBufferSize,pInitFile);
        Sleep(atoi(pPause));
    }

    // tell control handler that all VMs have been stopped
    SetEvent(ghVMStoppedEvent);

    // Shutdown vbox web service, if it's started by us
    if (vBoxWebSrvPId != -1) {
        HANDLE vboxWSHnd = OpenProcess(PROCESS_ALL_ACCESS, false, vBoxWebSrvPId);
        if (vboxWSHnd != NULL)
        {
            WriteLog("Shutdown VirtualBox web service");
            TerminateProcess(vboxWSHnd, -1);
            CloseHandle(vboxWSHnd);
        }
    }

    virtualBox->Release();
    CoUninitialize();

end:
    if (ghVMStoppedEvent)
        CloseHandle(ghVMStoppedEvent);

    char pTemp[121];
    sprintf_s(pTemp, "%s stopped.", pServiceName);
    WriteLog(pTemp);

    // notify SCM that we've finished
    serviceStatus.dwCurrentState = SERVICE_STOPPED; 
    serviceStatus.dwWaitHint     = 0;  
    if (!SetServiceStatus(hServiceStatusHandle, &serviceStatus))
    { 
        long nError = GetLastError();
        char pTemp[121];
        sprintf_s(pTemp, "SetServiceStatus failed, error code = %d", nError);
        WriteLog(pTemp);
    }

    return 1;
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
    HANDLE hThread;
    unsigned threadID;

    if(dwSize>4&&pModuleFile[dwSize-4]=='.')
    {
        sprintf_s(pExeFile,"%s",pModuleFile);
        *(strrchr(pModuleFile, '\\')) = 0;
        sprintf_s(pInitFile,"%s\\VBoxVmService.ini",pModuleFile);
        sprintf_s(pLogFile,"%s\\VBoxVmService.log",pModuleFile);
    }
    else
    {
        printf("Invalid module file name: %s\n", pModuleFile);
        return;
    }
    WriteLog(pExeFile);
    WriteLog(pInitFile);
    WriteLog(pLogFile);
    // read service name from .ini file
    GetPrivateProfileString("Settings","ServiceName","VBoxVmService",pServiceName,nBufferSize,pInitFile);

    char pTemp[121];
    sprintf_s(pTemp, "%s started.", pServiceName);
    WriteLog(pTemp);

    // Run in debug mode if switch is "-d"
    if(argc==2&&_stricmp("-d",argv[1])==0)
    {
        printf("Welcome to vms (debug mode)\n");
        WorkerProc( NULL );
    }
    else 
    {   
        // Since VBox COM runs as a normal process, it get killed before
        // services receive shutdown event. That's why we need to register a 
        // control handler to receive notify before system get rebooted.
        SetConsoleCtrlHandler( (PHANDLER_ROUTINE) CtrlHandler, TRUE );
        hThread = (HANDLE)_beginthreadex( NULL, 0, &WorkerProc, NULL, 0, &threadID );
        // start a worker thread to check for pipe messages
        if(hThread==0)
        {
            char pTemp[121];
            sprintf_s(pTemp, "_beginthread failed, error code = %d", GetLastError());
            WriteLog(pTemp);
        }


        // pass dispatch table to service controller
        if(!StartServiceCtrlDispatcher(DispatchTable))
        {
            char pTemp[121];
            sprintf_s(pTemp, "StartServiceCtrlDispatcher failed, error code = %d", GetLastError());
            WriteLog(pTemp);
        }
        // you don't get here unless the service is shutdown

        // Destroy the thread object.
        CloseHandle( hThread );

    }
}
