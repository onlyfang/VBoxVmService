#include <stdio.h>
#include <windows.h>
#include <winbase.h>
#include <winsvc.h>
#include <process.h>

#include "VirtualBox.h"
//#include "VBoxVmPipeManager.h"
#include "Util.h"

const int nBufferSize = 500;
char  chBuf[8192]; 

void show_usage()
{
    printf("VBoxVmSerice control utility\n");
    printf("usage: VmServiceControl [options]\n");
    printf("       -i        Install VBoxVmService service\n");
    printf("       -u        Uninstall VBoxVmService service\n");
    printf("       -s        Start VBoxVmService service\n");
    printf("       -k        Stop VBoxVmService service\n");
    printf("       -l        List all configured VMs\n");
    printf("       -su n     Startup VM with index n\n");
    printf("       -sd n     Shutdown VM with index n\n");
    printf("       -st n     Show status for VM with index n\n");
    printf("\n");
}


BOOL KillService() 
{ 
    // kill service with given name
    SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
    if (schSCManager==0) 
    {
        printf("OpenSCManager failed: %s\n", LastErrorString());
    }
    else
    {
        // open the service
        SC_HANDLE schService = OpenService( schSCManager, "VBoxVmService", SERVICE_ALL_ACCESS);
        if (schService==0) 
        {
            printf("OpenService failed: %s\n", LastErrorString());;
        }
        else
        {
            // call ControlService to kill the given service
            SERVICE_STATUS status;
            if(ControlService(schService,SERVICE_CONTROL_STOP,&status))
            {
                printf("Service stopped successfully\n");
                CloseServiceHandle(schService); 
                CloseServiceHandle(schSCManager); 
                return TRUE;
            }
            else
            {
                printf("ControlService failed: %s\n", LastErrorString());
            }
            CloseServiceHandle(schService); 
        }
        CloseServiceHandle(schSCManager); 
    }
    return FALSE;
}


BOOL RunService(int nArg, char** pArg) 
{ 
    // run service with given name
    SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
    if (schSCManager==0) 
    {
        printf("OpenSCManager failed: %s\n", LastErrorString());
    }
    else
    {
        // open the service
        SC_HANDLE schService = OpenService( schSCManager, "VBoxVmService", SERVICE_ALL_ACCESS);
        if (schService==0) 
        {
            printf("OpenService failed: %s\n", LastErrorString());
        }
        else
        {
            // call StartService to run the service
            if(StartService(schService,nArg,(const char**)pArg))
            {
                printf("Service started successfully\n");
                CloseServiceHandle(schService); 
                CloseServiceHandle(schSCManager); 
                return TRUE;
            }
            else
            {
                printf("StartService failed: %s\n", LastErrorString());
            }
            CloseServiceHandle(schService); 
        }
        CloseServiceHandle(schSCManager); 
    }
    return FALSE;
}

////////////////////////////////////////////////////////////////////// 
//
// Uninstall
//
VOID UnInstall()
{
    // Remove system wide VBOX_USER_HOME environment variable
    HKEY hKey;
    LONG lRet = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_ALL_ACCESS, &hKey);
    if (lRet == ERROR_SUCCESS)
    {
        lRet = RegDeleteValue(hKey, "VBOX_USER_HOME");
        RegCloseKey(hKey);
    }

    SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_ALL_ACCESS); 
    if (schSCManager==0) 
    {
        printf("OpenSCManager failed: %s\n", LastErrorString());
    }
    else
    {
        SC_HANDLE schService = OpenService( schSCManager, "VBoxVmService", SERVICE_ALL_ACCESS);
        if (schService==0) 
        {
            printf("OpenService failed: %s\n", LastErrorString());
        }
        else
        {
            if(!DeleteService(schService)) 
            {
                printf("Failed to delete service VBoxVmService\n");
            }
            else 
            {
                printf("Service VBoxVmService removed\n");
            }
            CloseServiceHandle(schService); 
        }
        CloseServiceHandle(schSCManager);   
    }
}

////////////////////////////////////////////////////////////////////// 
//
// Install
//
VOID Install() 
{  
    char pInitFile[nBufferSize+1];
    char pModuleFile[nBufferSize+1];
    char pExeFile[nBufferSize+1];
    DWORD dwAttr;
    char pVboxUserHome[nBufferSize+1];

    DWORD dwSize = GetModuleFileName(NULL,pModuleFile,nBufferSize);
    pModuleFile[dwSize] = 0;
    *(strrchr(pModuleFile, '\\')) = 0;

    // check 64bit or 32bit system
    BOOL bIs64Bit = FALSE;

#if defined(_WIN64)
    bIs64Bit = TRUE;  // 64-bit programs run only on Win64
#elif defined(_WIN32)
    typedef BOOL (WINAPI *LPFNISWOW64PROCESS) (HANDLE, PBOOL);
    LPFNISWOW64PROCESS pfnIsWow64Process = (LPFNISWOW64PROCESS)GetProcAddress(GetModuleHandle("kernel32"), "IsWow64Process");

    if (pfnIsWow64Process)
        pfnIsWow64Process(GetCurrentProcess(), &bIs64Bit);
#endif


    if (bIs64Bit)
        sprintf_s(pExeFile, nBufferSize, "%s\\VBoxVmService64.exe",pModuleFile);
    else
        sprintf_s(pExeFile, nBufferSize, "%s\\VBoxVmService.exe",pModuleFile);
    sprintf_s(pInitFile, nBufferSize, "%s\\VBoxVmService.ini",pModuleFile);

    //sanitizing VBoxVmService.ini
    dwAttr = GetFileAttributes(pInitFile);
    if(dwAttr == INVALID_FILE_ATTRIBUTES)
    {
        // An error accrued 
        DWORD dwError = GetLastError();
        if(dwError == ERROR_FILE_NOT_FOUND)
        {
            printf("Error:\nUnable to open config file at \"%s\": file not found.\n", pInitFile);
        }
        else if(dwError == ERROR_PATH_NOT_FOUND)
        {
            printf("Error:\nUnable to open config file at \"%s\": path not found.\n", pInitFile);
        }
        else if(dwError == ERROR_ACCESS_DENIED)
        {
            printf("Error:\nUnable to open config file at \"%s\": file exists, but access is denied.\n", pInitFile); 
        }
        else
        {
            printf("Error:\nUnable to open config file at \"%s\": Unknown error.\n", pInitFile);
        }
        printf("\nPleas run VmServiceControl.exe from the directory where you installed it. Also check that the VBoxVmService.ini exists in that directory, and is readable.\n");
        return;
    }

    //sanitizing VBOX_USER_HOME           
    GetPrivateProfileString("Settings","VBOX_USER_HOME","",pVboxUserHome,nBufferSize,pInitFile);
    dwAttr = GetFileAttributes(pVboxUserHome);
    if(dwAttr == INVALID_FILE_ATTRIBUTES)
    {
        // An error accrued 
        DWORD dwError = GetLastError();
        if(dwError == ERROR_FILE_NOT_FOUND)
        {
            printf("Error:\nUnable to open directory VBOX_USER_HOME at \"%s\": directory not found.\n", pVboxUserHome);
        }
        else if(dwError == ERROR_PATH_NOT_FOUND)
        {
            printf("Error:\nUnable to open directory VBOX_USER_HOME at \"%s\": path not found.\n", pVboxUserHome);
        }
        else if(dwError == ERROR_ACCESS_DENIED)
        {
            printf("Error:\nUnable to open directory VBOX_USER_HOME at \"%s\": directory exists, but access is denied.\n", pVboxUserHome); 
        }
        else
        {
            printf("Error:\nUnable to open directory VBOX_USER_HOME at \"%s\": Unknown error.\n", pVboxUserHome);
        }
        printf("\nPleas run VmServiceControl.exe from the directory where you installed it. Also check that the directory you specified in VBoxVmService.ini as variable VBOX_USER_HOME exists and is readable.\n");
        return;
    }


    SC_HANDLE schSCManager = OpenSCManager( NULL, NULL, SC_MANAGER_CREATE_SERVICE); 
    if (schSCManager==0) 
    {
        printf("OpenSCManager failed: %s\n", LastErrorString());
    }
    else
    {
        char pRunAsUser[nBufferSize+1];
        char pUserPassword[nBufferSize+1];
        GetPrivateProfileString("Settings","RunAsUser","",pRunAsUser,nBufferSize,pInitFile);
        GetPrivateProfileString("Settings","UserPassword","",pUserPassword,nBufferSize,pInitFile);
        SC_HANDLE schService = CreateService( 
                schSCManager,         /* SCManager database      */ 
                "VBoxVmService",      /* name of service         */ 
                "VBoxVmService",      /* service name to display */ 
                SERVICE_ALL_ACCESS,   /* desired access          */ 
                SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS ,    /* service type            */ 
                SERVICE_AUTO_START,   /* start type              */ 
                SERVICE_ERROR_NORMAL, /* error control type      */ 
                pExeFile,             /* service's binary        */ 
                NULL,                 /* no load ordering group  */ 
                NULL,                 /* no tag identifier       */ 
                "LanmanServer\0LanmanWorkstation\0ComSysApp\0\0", /* service dependencies */ 
                NULL,                 /* LocalSystem account     */ 
                NULL                  /* no password             */
                );                      
        if (schService==0) 
        {
            printf("Failed to create service VBoxVmService: %s\n", LastErrorString());
        }
        else
        {
            // Set system wide VBOX_USER_HOME environment variable
            char pVboxUserHome[nBufferSize+1];
            GetPrivateProfileString("Settings","VBOX_USER_HOME","",pVboxUserHome,nBufferSize,pInitFile);
            HKEY hKey;
            DWORD dwDisp;
            LONG lRet = RegCreateKeyEx(HKEY_LOCAL_MACHINE, "SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &dwDisp);
            if (lRet == ERROR_SUCCESS)
            {
                lRet = RegSetValueEx(hKey, "VBOX_USER_HOME", 0, REG_SZ, (const BYTE *)pVboxUserHome, (DWORD)strlen(pVboxUserHome) + 1);
                RegCloseKey(hKey);
            }
            if (lRet != ERROR_SUCCESS)
            {
                printf("Failed to set VBOX_USER_HOME environment\n");
            }

            printf("Service VBoxVmService installed\n");
            CloseServiceHandle(schService); 
        }
        CloseServiceHandle(schSCManager);
    }   
}

// start up specified VM
void StartVM(int nIndex)
{
    char pCommand[80];
    sprintf_s(pCommand, 80, "start %u", nIndex);
    if(SendCommandToService(pCommand, chBuf, sizeof(chBuf)) == FALSE)
    {
        printf("Failed to send command to service.\n%s\n", chBuf);
        return;
    }

    if (chBuf[0] != 0)
    {
        printf("Failed to start up virtual machine.\n%s\n", chBuf + 1);
        return;
    }

    int len = chBuf[1];
    char vmName[256];
    memcpy(vmName, chBuf + 2, len);
    vmName[len] = 0;

    printf("VM%d: %s has been started up.\n", nIndex, vmName);
}

// shut down specified VM
void StopVM(int nIndex)
{
    char pCommand[80];
    sprintf_s(pCommand, 80, "stop %u", nIndex);
    if(SendCommandToService(pCommand, chBuf, sizeof(chBuf)) == FALSE)
    {
        printf("Failed to send command to service.\n%s\n", chBuf);
        return;
    }

    if (chBuf[0] != 0)
    {
        printf("Failed to shut down virtual machine.\n%s\n", chBuf + 1);
        return;
    }

    int len = chBuf[1];
    char vmName[256];
    memcpy(vmName, chBuf + 2, len);
    vmName[len] = 0;


    printf("VM%d: %s has been shut down.\n", nIndex, vmName);
}

// show specified VM status
void VMStatus(int nIndex)
{
    char pCommand[80];
    sprintf_s(pCommand, 80, "status %u", nIndex);
    if(SendCommandToService(pCommand, chBuf, sizeof(chBuf)) == FALSE)
    {
        printf("Failed to send command to service.\n%s\n", chBuf);
        return;
    }

    if (chBuf[0] != 0)
    {
        printf("Failed to get virtual machine status.\n%s\n", chBuf + 1);
        return;
    }

    int len = chBuf[1];
    char vmName[256];
    memcpy(vmName, chBuf + 2, len);
    vmName[len] = 0;
    MachineState state = (MachineState)chBuf[len + 2];
    printf("VM%d: %s is %s.\n", nIndex, vmName, MachineStateToString(state));
}

// list all VM status
void ListVMs()
{
    char pCommand[80];
    sprintf_s(pCommand, 80, "list");
    if(SendCommandToService(pCommand, chBuf, sizeof(chBuf)) == FALSE)
    {
        printf("Failed to send command to service.\n%s\n", chBuf);
        return;
    }

    if (chBuf[0] != 0)
    {
        printf("Failed to list all virtual machines.\n%s\n", chBuf + 1);
        return;
    }

    int count = chBuf[1];
    int buf_len = 2;
    for (int i = 0; i < count; i++)
    {
        int len = chBuf[buf_len];
        if (len == 0)
            break;

        char vmName[256];
        memcpy(vmName, chBuf + buf_len + 1, len);
        vmName[len] = 0;

        MachineState state = (MachineState)chBuf[buf_len + len + 1];
        printf("VM%d: %s is %s.\n", i, vmName, MachineStateToString(state));

        buf_len += len + 2;
    }

}


////////////////////////////////////////////////////////////////////// 
//
// Standard C Main
//
void main(int argc, char *argv[] )
{
    // uninstall service if switch is "-u"
    if(argc==2&&_stricmp("-u",argv[1])==0)
    {
        UnInstall();
    }
    // install service if switch is "-i"
    else if(argc==2&&_stricmp("-i",argv[1])==0)
    {           
        Install();
    }
    // start service if switch is "-s"
    else if(argc==2&&_stricmp("-s",argv[1])==0)
    {           
        RunService(0,NULL);
    }
    // stop service if switch is "-k"
    else if(argc==2&&_stricmp("-k",argv[1])==0)
    {           
        KillService();
    }
    // Simulates a shutdown if VBoxVmService is started with -d
    else if(argc==2&&_stricmp("-dk",argv[1])==0)
    {           
        if(SendCommandToService("shutdown", chBuf, sizeof(chBuf)))
            printf("Shutdown all your virtual machines\n\n\n");
        else
            printf("Failed to send command to service.\n");
    }
    // STARTUP SWITCH
    // 
    // exit with error if switch is "-su" and no VmId is supplied
    else if(argc==2&&_stricmp("-su",argv[1])==0)
    {
        printf("Startup option needs to be followed by a VM index.\n");
    }
    // start a specific vm (if the index is supplied)
    else if(argc==3&&_stricmp("-su",argv[1])==0)
    {
        int nIndex = atoi(argv[2]);
        StartVM(nIndex);
    }
    // SHUTDOWN SWITCH
    // 
    // exit with error if switch is "-sd" and no VmId is supplied
    else if(argc==2&&_stricmp("-sd",argv[1])==0)
    {
        printf("Shutdown option needs to be followed by a VM index.\n");
    }
    // shutdown a specific vm (if the index is supplied)
    else if(argc==3&&_stricmp("-sd",argv[1])==0)
    {
        int nIndex = atoi(argv[2]);
        StopVM(nIndex);
    }
    // show status for a specific vm (if the index is supplied)
    else if(argc==3&&_stricmp("-st",argv[1])==0)
    {
        int nIndex = atoi(argv[2]);
        VMStatus(nIndex);
    }
    // list all configured VMs' status if switch is "-l"
    else if(argc==2&&_stricmp("-l",argv[1])==0)
    {           
        ListVMs();
    }
    else 
        show_usage();
}

