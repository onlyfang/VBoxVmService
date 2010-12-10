////////////////////////////////////////////////////////////////////// 
// VirtualBox VMs - managed by an NT Service (VBoxVmService)
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <windows.h>
#include <winbase.h>
#include <winsvc.h>
#include <process.h>
#include <strsafe.h>
#include <time.h>
#include <stdarg.h>
#include <io.h>

#include "VBoxVmPipeManager.h"


const int nBufferSize = 500;
char pServiceName[nBufferSize+1];
char pExeFile[nBufferSize+1];
char pInitFile[nBufferSize+1];
char pLogFile[nBufferSize+1];
const int nMaxProcCount = 100;
int nProcStatus[nMaxProcCount];
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

//Write to both the pipe and log
void WriteLogPipe(LPPIPEINST pipe, LPCSTR formatstring, ...){

	char pTemp[nBufferSize+1];
	DWORD pLen;
	va_list args;
	va_start(args, formatstring);

	if (pipe==NULL) { return;}


	pLen = _vsnprintf(pTemp, sizeof(pTemp), formatstring, args);

	//append to pipe
	pipelcat(pipe, pTemp, pLen);
	//append to log
	WriteLog(pTemp);
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
// My own implementation of strlcat, but with optional length param. Used for appending to the pipe.
char *pipelcat(LPPIPEINST pipe, const char *src, int srclength) {


	if (pipe==NULL) {return NULL;}

	if (srclength == -1) {
		srclength = strlen(src);
	}
	//Debug: uncoment to see write stat
	//printf("pipelcat(cbToWrite=%d, srclength=%d)\n",pipe->cbToWrite,srclength);

	// test for free space, If we don't have space for the whole string we just return.
	if (((srclength + pipe->cbToWrite)) > (sizeof(pipe->chReply) -1)) {
		return NULL;
	}

	strncpy(pipe->chReply + pipe->cbToWrite, src, srclength);
	pipe->cbToWrite += srclength;

	pipe->chReply[pipe->cbToWrite +1] = '\0'; //Amen 

	//Debug: Uncoment to se new size in cbToWrite
	//printf("~pipelcat(cbToWrite=%d)\n",pipe->cbToWrite);
	return pipe->chReply;
}

/*
Run a console app by calling the pCommandLine.

The stdout is loged to the VBoxVmService.log logfile. If chBufBufferSize is not 0 we also store the stdout output in the chBuf buffer. 
An easy way of calling this is to use conditional assignment for chBuf and chBufBufferSize. For example “(pipe==NULL) ? NULL : pipe->chReply” means that if pipe is NULL we will send inn NULL, if it is not NULL we wil send “pipe->chReply”.

Read more about conditional assignments at: http://en.wikipedia.org/wiki/%3F:

*/
BOOL RunConsoleApp(LPCTSTR ApplicationName, char pCommandLine[], char pWorkingDir[], LPPIPEINST pipe) 
{ 
    

	HANDLE g_hChildStd_OUT_Rd = NULL;
	HANDLE g_hChildStd_OUT_Wr = NULL;
	HANDLE hStdOut = NULL;
	char pTemp[nBufferSize+1];
	char cpBuf[nBufferSize];

	printf("RunConsoleApp(ApplicationName=%s, pCommandLine=%s)\n",ApplicationName,pCommandLine);
	 
	// Set the bInheritHandle flag so pipe handles are inherited.
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof (SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;
	// open handle to VBoxVmService.log
    hStdOut = CreateFile(pLogFile, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    

	sprintf_s(pTemp,"Running coomand: '%s'.", pCommandLine); 
    WriteLog(pTemp);

	// prepare for creating process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.lpDesktop = "";
    si.hStdOutput = hStdOut;


	if (pipe != NULL) {
		SECURITY_ATTRIBUTES sahChildStd; 
		sahChildStd.nLength = sizeof(SECURITY_ATTRIBUTES); 
		sahChildStd.bInheritHandle = TRUE; 
		sahChildStd.lpSecurityDescriptor = NULL;
		// Create a pipe for the child process's STDOUT.
		if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &sahChildStd, 0) ) 
		{
			sprintf_s(pTemp,"Error StdoutRd CreatePipe while runing : '%s'.", pCommandLine); 
			WriteLog(pTemp);
		}
		// Ensure the read handle to the pipe for STDOUT is not inherited.
		if ( ! SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) )
		{
			sprintf_s(pTemp,"Error Stdout SetHandleInformation while runing: '%s'.", pCommandLine); 
			WriteLog(pTemp);
		}

		// overriding VBoxVmService.log handler
		si.hStdOutput = g_hChildStd_OUT_Wr;
	}

	// Run the command
    PROCESS_INFORMATION pProcInfo;
    if(!CreateProcess(ApplicationName,pCommandLine,NULL,NULL,TRUE,NORMAL_PRIORITY_CLASS,NULL,strlen(pWorkingDir)==0?NULL:pWorkingDir,&si,&pProcInfo))
    {
        return FALSE;
    }


	// Close handles to the child process and its primary thread.
    // Some applications might keep these handles to monitor the status
    // of the child process, for example. 
    WaitForSingleObject(pProcInfo.hProcess, INFINITE);
    CloseHandle(pProcInfo.hProcess);
    CloseHandle(pProcInfo.hThread);

	if (pipe != NULL) {
		// Read output from the child process's pipe for STDOUT
		// and write to the parent process's error log and buff. 
		// Stop when there is no more data. 
		DWORD dwRead, dwWritten; 
		BOOL bSuccess = FALSE;

		// Close the write end of the pipe before reading from the 
		// read end of the pipe, to control child process execution.
		if (!CloseHandle(g_hChildStd_OUT_Wr)) 
		{
  			sprintf_s(pTemp,"Error StdOutWr CloseHandle ruuing: '%s'.", pCommandLine); 
			WriteLog(pTemp);
		}

		// reading while we have data to read
		for (;;) 
		{ 

			bSuccess = ReadFile( g_hChildStd_OUT_Rd, (LPVOID)cpBuf, sizeof(cpBuf) , &dwRead, NULL);
			if( ! bSuccess || dwRead == 0 ) break;

			pipelcat(pipe,cpBuf,dwRead);

			bSuccess = WriteFile(hStdOut, (LPVOID)cpBuf, dwRead, &dwWritten, NULL);
			if (! bSuccess ) break; 

		} 

	     // no need to close this because it got closed when the process exited. Closing it again will cast error if run under a debugger.
		//CloseHandle(g_hChildStd_OUT_Wr);
		CloseHandle(hStdOut);
	}


    return TRUE;

}





char *nIndexTopVmName(int nIndex) {
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

// helper function
// RunVBoxManage with printf style arguments
BOOL VBoxManage(LPPIPEINST pipe, LPCSTR formatstring, ...) {
	int nSize = 0;
	char myarguments[255];
   	char pItem[nBufferSize+1];
	char pWorkingDir[nBufferSize+1];
	char pCommandLine[nBufferSize+1];
	char pTemp[121];

	// handle custom arguments
	va_list args;
	va_start(args, formatstring);

	nSize = _vsnprintf( myarguments, sizeof(myarguments), formatstring, args);
	if ( nSize > (sizeof(myarguments) -2) ) {
		printf("VBoxManage: Arguments \"%s\" is to long\n", myarguments);
		return false;
	}



	// get full path to VBoxManage.exe
	char pVBoxManagePath[nBufferSize+1];
	if (GetEnvironmentVariable("VBOX_INSTALL_PATH", pVBoxManagePath, nBufferSize) == 0)
	{
	    sprintf_s(pTemp, "VBOX_INSTALL_PATH not found."); 
	    WriteLog(pTemp);
		return false;
	}
	strcat(pVBoxManagePath, "VBoxManage.exe");

	// get working dir
	GetPrivateProfileString(pItem,"WorkingDir","",pWorkingDir,nBufferSize,pInitFile);
  

	// runn the coomand to check status and write back to Pipe
	if (!RunConsoleApp(pVBoxManagePath, myarguments, pWorkingDir, pipe)) {
		long nError = GetLastError();
		sprintf_s(pTemp,"Failed execute VBoxManage.exe using command: '%s', error code = %d", pCommandLine, nError); 
		WriteLog(pTemp);
		return FALSE;
	}
	return true;

}

// start a VM with given index
BOOL StartProcess(int nIndex, LPPIPEINST pipe) 
{ 
		char pVrdpPort[nBufferSize+1];
		char *cp;
		char pItem[nBufferSize+1];

		cp = nIndexTopVmName( nIndex );
		if (cp==NULL) {WriteLogPipe(pipe, "Unknown VM index number. Are you sure it is defined in the VBoxVmService.ini file?"); return false; }


		// get VrdpPort
		sprintf_s(pItem,"Vm%d\0",nIndex);
		GetPrivateProfileString(pItem,"VrdpPort","",pVrdpPort,nBufferSize,pInitFile);

		// run VBoxManage.exe showvminfo VmName
		VBoxManage(pipe, "-nologo showvminfo \"%s\"", cp);

		// run VBoxManage.exe modifyvm VmName --vrdpport
		VBoxManage(pipe, "-nologo modifyvm \"%s\" --vrdpport %s", cp, pVrdpPort);

		// run VBoxManage.exe startvm VmName --type vrdp
		VBoxManage(pipe, "-nologo startvm \"%s\" --type vrdp", cp);

		nProcStatus[nIndex] = 1;
		return true;

}
// end a VM with given index
BOOL EndProcess(int nIndex, LPPIPEINST pipe) 
{
		char pItem[nBufferSize+1];
		char pShutdownMethod[nBufferSize+1];
		char *cp;


        cp = nIndexTopVmName( nIndex );
		if (cp==NULL) {WriteLogPipe(pipe, "Unknown VM index number. Are you sure it is defined in the VBoxVmService.ini file?"); return false; }

		// get ShutdownMethod
		sprintf_s(pItem,"Vm%d\0",nIndex);
		GetPrivateProfileString(pItem,"ShutdownMethod","",pShutdownMethod,nBufferSize,pInitFile);

		VBoxManage(pipe, "-nologo controlvm \"%s\" \"%s\"", cp, pShutdownMethod);

		nProcStatus[nIndex] = 0;
		return true;
}

// Run "set" on the windows commandline to get the execution environment
BOOL CmdEnv(LPPIPEINST pipe) {

	char pCommandLine[nBufferSize+1];

	//create command to run.   
	sprintf_s(pCommandLine, "C:\\Windows\\System32\\cmd /C set");

	// runn the coomand to check status and write back to Pipe
	RunConsoleApp(NULL, pCommandLine, "C:\\Windows\\System32", pipe);


	return true;	
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
        nProcStatus[i] = 0;

    BOOL bShouldWait = FALSE;
    for(int i=0;i<nMaxProcCount;i++)
    {
        if (!StartProcess(i))
            break;
        bShouldWait = true;
    }

    // wait for VMs to start up
    if (bShouldWait)
    {
        char pPause[nBufferSize+1];
        GetPrivateProfileString("Settings","PauseStartup","1000",pPause,nBufferSize,pInitFile);
        Sleep(atoi(pPause));
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

			//tell PipeManager to exit.
			PipeManagerExit();
            // also tell SCM we'll need some time to shutdown
            serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            {
                char pPause[nBufferSize+1];
                GetPrivateProfileString("Settings","PauseShutdown","1000",pPause,nBufferSize,pInitFile);
                serviceStatus.dwWaitHint = atoi(pPause) + 1000;  
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
            // bounce processes started by this service
            if(fdwControl>=128&&fdwControl<256)
            {
                int nIndex = fdwControl&0x7F;
                // bounce a single process
                if(nIndex>=0&&nIndex<nMaxProcCount)
                {
                    if (nProcStatus[nIndex] == 1)
                        EndProcess(nIndex, NULL);
                    StartProcess(nIndex, NULL);
                }
                // bounce all processes
                else if(nIndex==127)
                {
                    for(int i=nMaxProcCount-1;i>=0;i--)
                    {
                        if (nProcStatus[i] == 1)
                            EndProcess(i, NULL);
                    }
                    for(int i=0;i<nMaxProcCount;i++)
                    {
                        if (!StartProcess(i, NULL))
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


void WorkerHandleCommand(LPPIPEINST pipe) 
{
	char pVboxUserHome[nBufferSize+1];
	char pTemp[121];
    char buffer[80]; 
	char *cp;
    memset(buffer, 0, 80);


	StringCchCopy(buffer, nBufferSize, pipe->chRequest);

	sprintf_s(pTemp, "Received control command: %s", buffer);
    WriteLog(pTemp);

    // set environment variable for current process, so that
    // no reboot is required after installation.
    GetPrivateProfileString("Settings","VBOX_USER_HOME","",pVboxUserHome,nBufferSize,pInitFile);
    if (! SetEnvironmentVariable("VBOX_USER_HOME", pVboxUserHome)) 
    {
        printf("SetEnvironmentVariable failed (%d)\n", GetLastError()); 
    }
            

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
		cp = nIndexTopVmName( atoi(buffer + 7) );
		if (cp==NULL) {WriteLogPipe(pipe, "Unknown VM index number. Are you sure it is defined in the VBoxVmService.ini file?"); return; }
		VBoxManage(pipe, "-nologo showvminfo \"%s\"", cp);	  
    }
	else if (strncmp(buffer, "env", 3) == 0)
    {
		CmdEnv(pipe);
    }
	else if (strncmp(buffer, "shutdown", 8) == 0)
    {
		VBoxVmServiceHandler(SERVICE_CONTROL_SHUTDOWN);
    }
	else if (strncmp(buffer, "guestpropertys", 14) == 0)
    {
        cp = nIndexTopVmName( atoi(buffer + 15) );
		if (cp==NULL) {WriteLogPipe(pipe, "Unknown VM index number. Are you sure it is defined in the VBoxVmService.ini file?"); return; }
		VBoxManage(pipe, "-nologo guestproperty enumerate \"%s\"", cp);
    }	
	else {
		printf("Unknown command \"%s\"\n", buffer);
	}





}

unsigned __stdcall WorkerProc(void* pParam)
{
    const int nMessageSize = 80;


	//PipeManager will handel all pipe connections
	PipeManager("\\\\.\\pipe\\VBoxVmService", WorkerHandleCommand);


	//gracefully ending the service
	BOOL bShouldWait = FALSE;
    // terminate all running processes before shutdown
    for(int i=nMaxProcCount-1;i>=0;i--)
    {
        if (nProcStatus[i] == 1)
        {
            EndProcess(i, NULL);
            bShouldWait = TRUE;
        }
    }			

    // wait for VMs to shut down
    if (bShouldWait)
    {
        char pPause[nBufferSize+1];
        GetPrivateProfileString("Settings","PauseShutdown","1000",pPause,nBufferSize,pInitFile);
        Sleep(atoi(pPause));
    }

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


	// Run in debug mode if switch is "-d"
    if(argc==2&&_stricmp("-d",argv[1])==0)
    {
		printf("Welcome to vms (debug mode)\n");
		WorkerProc( NULL );
	}
	else 
	{	

		hThread = (HANDLE)_beginthreadex( NULL, 0, &WorkerProc, NULL, 0, &threadID );
		// start a worker thread to check for pipe messages
		if(hThread==0)
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

		// Destroy the thread object.
		CloseHandle( hThread );

	}


}
