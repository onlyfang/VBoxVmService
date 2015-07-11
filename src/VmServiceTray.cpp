// Windows Header Files:
#include <windows.h>

// C RunTime Header Files
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>

#include "Util.h"

#include "resource.h"

#define TRAYICONID  1            // ID number for the Notify Icon
#define SWM_TRAYMSG (WM_APP + 2) // the message ID sent to our window

#define IDM_ABOUT                       103
#define IDM_EXIT                        104
#define IDM_START_SERVICE               105
#define IDM_STOP_SERVICE                106
#define IDT_TIMER                       107
#define IDM_START_VM0                   128 // 0x80
#define IDM_STOP_VM0                    256 // 0x100

typedef struct {
    char name[128];
    MachineState state;
} VM_STATE;

// Global Variables:
HINSTANCE hInst;								// current instance
HWND hWnd;                                      // main window
NOTIFYICONDATA niData;                          // notify icon data
char szTitle[] = "VmServiceTray";               // The title bar text
const int nBufferSize = 500;
char  chBuf[8192]; 

BOOL bServiceStarted = false;
VM_STATE *vm_status = NULL;
int vm_count;

// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
void                ShowContextMenu(HWND hWnd);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
void                QueryStatus();
void                ReportError(LPCSTR lpText);
void                StartService();
void                StopService();
void                StartVM(int vm);
void                StopVM(int vm);

int APIENTRY WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    MSG msg;
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, NULL, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc	= WndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= hInstance;
    wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= szTitle;
    wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_ICON));

    return RegisterClassEx(&wcex);
}

// Initialize the window and tray icon
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    hWnd = CreateWindow(szTitle, szTitle, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

    if (!hWnd)
        return FALSE;


    ZeroMemory(&niData,sizeof(NOTIFYICONDATA));
    niData.cbSize = sizeof(NOTIFYICONDATA);
    niData.uID = TRAYICONID;
    niData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; 
    niData.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
    niData.hWnd = hWnd;
    niData.uCallbackMessage = SWM_TRAYMSG;
    lstrcpyn(niData.szTip, szTitle, sizeof(niData.szTip)/sizeof(TCHAR));
    Shell_NotifyIcon(NIM_ADD,&niData);

    // free icon handle
    if(niData.hIcon && DestroyIcon(niData.hIcon))
        niData.hIcon = NULL;

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    int wmId, wmEvent;
    PAINTSTRUCT ps;
    HDC hdc;

    switch (message)
    {
        case SWM_TRAYMSG:
            switch(lParam)
            {
                case WM_RBUTTONUP:
                    ShowContextMenu(hWnd);
            }
            break;
        case WM_COMMAND:
            wmId    = LOWORD(wParam);
            wmEvent = HIWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                case IDM_START_SERVICE:
                    StartService();
                    break;
                case IDM_STOP_SERVICE:
                    StopService();
                    break;
                default:
                    if (wmId & IDM_START_VM0)
                        StartVM(wmId & 0x7f);
                    else if (wmId & IDM_STOP_VM0)
                        StopVM(wmId & 0x7f);
                    else
                        return DefWindowProc(hWnd, message, wParam, lParam);
            }
            break;
        case WM_PAINT:
            hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code here...
            EndPaint(hWnd, &ps);
            break;
        case WM_DESTROY:
            niData.uFlags = 0;
            Shell_NotifyIcon(NIM_DELETE,&niData);
            if (vm_status)
                free (vm_status);
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    HWND hwndOwner;
    RECT rc, rcDlg, rcOwner;
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
        case WM_INITDIALOG:
            // center window
            if ((hwndOwner = GetParent(hDlg)) == NULL)
                hwndOwner = GetDesktopWindow();
            GetWindowRect(hwndOwner, &rcOwner);
            GetWindowRect(hDlg, &rcDlg);
            CopyRect(&rc, &rcOwner);
            OffsetRect(&rcDlg, -rcDlg.left, -rcDlg.top);
            OffsetRect(&rc, -rc.left, -rc.top);
            OffsetRect(&rc, -rcDlg.right, -rcDlg.bottom);
            SetWindowPos(hDlg, HWND_TOP,
                         rcOwner.left + (rc.right / 2),
                         rcOwner.top + (rc.bottom / 2),
                         0, 0, SWP_NOSIZE);

            return (INT_PTR)TRUE;

        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}

// Name says it all
void ShowContextMenu(HWND hWnd)
{
    POINT pt;
    GetCursorPos(&pt);
    HMENU hMenu = CreatePopupMenu();
    if(hMenu)
    {
        QueryStatus();
        if (vm_count > 0)
        {
            for (int i = 0; i < vm_count; i++)
            {
                // create sub menu
                HMENU hSubMenu = CreatePopupMenu();
                InsertMenu(hSubMenu, -1, MF_BYPOSITION, IDM_START_VM0 | i, "Start");
                InsertMenu(hSubMenu, -1, MF_BYPOSITION, IDM_STOP_VM0 | i, "Stop");
                // insert into main menu
                InsertMenu(hMenu, -1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hSubMenu, vm_status[i].name);
                int res;
                bool running = false;
                switch (vm_status[i].state) {
                    case MachineState_PoweredOff:
                        res = BMP_STATE_POWEREDOFF; 
                        break;
                    case MachineState_Saved:
                        res = BMP_STATE_SAVED;
                        break;
                    case MachineState_Aborted:
                        res = BMP_STATE_ABORTED;
                        break;
                    case MachineState_Running:
                    case MachineState_Starting:
                        res = BMP_STATE_RUNNING;
                        running = true;
                        break;
                    case MachineState_Paused:
                        res = BMP_STATE_PAUSED;
                        break;
                    case MachineState_Stopping:
                    case MachineState_Saving:
                        res = BMP_STATE_SAVING;
                        break;
                    case MachineState_Restoring:
                        res = BMP_STATE_RESTORING;
                        running = true;
                        break;
                    default:
                        res = BMP_STATE_ERROR;
                        break;
                }
                // load state image
                HBITMAP hBitmap = (HBITMAP)LoadImage(hInst, MAKEINTRESOURCE(res), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADTRANSPARENT | LR_SHARED);
                MENUITEMINFO mii = { sizeof(mii) };
                mii.fMask = MIIM_BITMAP;
                mii.hbmpItem = hBitmap;
                SetMenuItemInfo(hMenu, (UINT)hSubMenu, false, &mii);

                // disable Start or Stop sub menu
                if (running)
                    EnableMenuItem(hSubMenu, IDM_START_VM0 | i, MF_GRAYED);
                else
                    EnableMenuItem(hSubMenu, IDM_STOP_VM0 | i, MF_GRAYED);
                DestroyMenu(hSubMenu);
            }
            InsertMenu(hMenu, -1, MF_BYPOSITION |  MF_SEPARATOR, 0, NULL);
        }

        // Start or Stop VBoxVmService
        InsertMenu(hMenu, -1, MF_BYPOSITION, IDM_START_SERVICE, "Start VBoxVmService");

        InsertMenu(hMenu, -1, MF_BYPOSITION, IDM_STOP_SERVICE, "Stop VBoxVmService");
        if (bServiceStarted)
            EnableMenuItem(hMenu, IDM_START_SERVICE, MF_GRAYED);
        else
            EnableMenuItem(hMenu, IDM_STOP_SERVICE, MF_GRAYED);

        InsertMenu(hMenu, -1, MF_BYPOSITION |  MF_SEPARATOR, 0, NULL);

        //InsertMenu(hMenu, -1, MF_BYPOSITION, IDM_ABOUT, "About");
        InsertMenu(hMenu, -1, MF_BYPOSITION, IDM_EXIT, "Exit");

        // note:    must set window to the foreground or the
        //          menu won't disappear when it should
        SetForegroundWindow(hWnd);

        TrackPopupMenu(hMenu, TPM_BOTTOMALIGN,
                pt.x, pt.y, 0, hWnd, NULL );
        DestroyMenu(hMenu);
    }
}

// query VBoxVmService service status
void QueryStatus()
{
    bServiceStarted = false;
    vm_count = 0;
    if (vm_status)
    {
        free(vm_status);
        vm_status = NULL;
    }

    char temp[80];
    sprintf_s(temp, 80, "list");
    if(SendCommandToService(temp, chBuf, sizeof(chBuf)) == FALSE)
    {
        return;
    }
    bServiceStarted = true;

    if (chBuf[0] != 0)
    {
        //sprintf_s(temp, 80, "Failed to list all virtual machines.\n%s\n", chBuf + 1);
        //ReportError(temp);
        return;
    }

    vm_count = chBuf[1];
    if (vm_count <= 0)
        return;
    vm_status = (VM_STATE *)calloc(vm_count, sizeof(VM_STATE));
    int buf_len = 2;
    for (int i = 0; i < vm_count; i++)
    {
        int len = chBuf[buf_len];
        if (len == 0)
            break;

        // get VM name
        memcpy(vm_status[i].name, chBuf + buf_len + 1, len);
        vm_status[i].name[len] = 0;

        // get VM state
        vm_status[i].state = (MachineState)chBuf[buf_len + len + 1];

        buf_len += len + 2;
    }

}

void ReportError(LPCSTR lpText)
{
    MessageBox(hWnd, lpText, NULL, MB_OK);
}

void StartService()
{
    char pModuleFile[nBufferSize+1];
    char pExeFile[nBufferSize+1];

    DWORD dwSize = GetModuleFileName(NULL,pModuleFile,nBufferSize);
    pModuleFile[dwSize] = 0;
    *(strrchr(pModuleFile, '\\')) = 0;
    sprintf_s(pExeFile, nBufferSize, "%s\\VmServiceControl.exe",pModuleFile);

    RunElevated(hWnd, pExeFile, "-s", pModuleFile);
}

void StopService()
{
    char pModuleFile[nBufferSize+1];
    char pExeFile[nBufferSize+1];

    DWORD dwSize = GetModuleFileName(NULL,pModuleFile,nBufferSize);
    pModuleFile[dwSize] = 0;
    *(strrchr(pModuleFile, '\\')) = 0;
    sprintf_s(pExeFile, nBufferSize, "%s\\VmServiceControl.exe",pModuleFile);

    RunElevated(hWnd, pExeFile, "-k", pModuleFile);
}

void StartVM(int vm)
{
    char temp[nBufferSize+1];
    sprintf_s(temp, nBufferSize, "start %u", vm);
    if(SendCommandToService(temp, chBuf, sizeof(chBuf)) == FALSE)
    {
        sprintf_s(temp, nBufferSize, "Failed to send command to service.\n%s\n", chBuf);
        ReportError(temp);
        return;
    }

    if (chBuf[0] != 0)
    {
        sprintf_s(temp, nBufferSize, "Failed to start up virtual machine.\n%s\n", chBuf + 1);
        ReportError(temp);
        return;
    }
}

void StopVM(int vm)
{
    char temp[nBufferSize+1];
    sprintf_s(temp, nBufferSize, "stop %u", vm);
    if(SendCommandToService(temp, chBuf, sizeof(chBuf)) == FALSE)
    {
        sprintf_s(temp, nBufferSize, "Failed to send command to service.\n%s\n", chBuf);
        ReportError(temp);
        return;
    }

    if (chBuf[0] != 0)
    {
        sprintf_s(temp, nBufferSize, "Failed to stop virtual machine.\n%s\n", chBuf + 1);
        ReportError(temp);
        return;
    }
}

