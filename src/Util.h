#ifndef __VBOXVMSERVICE_UTIL_H__
#define __VBOXVMSERVICE_UTIL_H__

#include "VirtualBox.h"

// Tests whether the current user and process has admin privileges.
// Note that this will return FALSE if called from a Vista program running in an administrator account if the process was not launched with 'run as administrator'
BOOL isAdmin();

// start an elevated process
BOOL RunElevated(HWND hwnd, LPCTSTR pszPath, LPCTSTR pszParameters, LPCTSTR pszDirectoryL);

// call GetLastError() and convert it to string with FormatMessage
char *LastErrorString();

// send command to service through pipe, and return result in chBuf
BOOL SendCommandToService(char * message, char chBuf[], int chBufSize);

// convert MachineState to readable string
char *MachineStateToString(MachineState state);

#endif
