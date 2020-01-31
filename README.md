# VBoxVmService

### 1. What is VBoxVmService?

VBoxVmService is a Windows Service, that enables the host-system to run or shutdown VirtualBox Virtual Machines (VMs) without a user being logged on to the host-system running Windows (Win10/2012/2016, both x86 and x64 supported).

### 2. How to use VBoxVmService?

Check the detailed [howto guide](doc/Howto.txt).

If you run into problem, please follow the TROUBLESHOOTING section in the guide first, most likely your issue has been addressed there. Check for known issues on the [wiki](https://github.com/onlyfang/VBoxVmService/wiki) before submitting new issue.

### 3. How does it work?

Basically, VBoxVmService is just a wrapper-service that calls VirtualBox COM API interfaces.

When windows starts up, the wrapper-service will start running. It will check for the settings in VBoxVmService.ini file, and call up the Virtual Machines as specified. Both the wrapper-service and Virtual Machines are run with the account specified in the VBoxVmService.ini file.

If RunWebService=yes is specified in VBoxVmService.ini file,  the wrapper-service will also try to start VirtualBox web service automatically. This might be useful for people who use the web service a lot, like users of phpVirtualBox.

When windows shuts down, the wrapper-service will try to stop all the VMs configuered in VBoxVmService.ini.

VmServiceControl.exe is the CLI (command line interface) client that sends control commands to VBoxVmService and display returned results.

VmServiceTray.exe is another client like VmServiceControl.exe, but comes with GUI interface and stays in system tray.

If you are interested in further details, feel free to browse the supplied source codes!


### 4. What are the plans for the future?

Actually, there are no plans for future releases of this software, other than keeping it updated so that it can always work with latest version of VirtualBox.


### 5. Which versions of Windows are supported?

Old Windows versions like Windows 7 are no longer supported.

It hasn't been fully tested on systems other than Windows 10, but it is supposed to work on server systems like Win2012, Win2016, ... etc, both 32bit and 64bit versions.


### 6. Will VBoxVmService run on Mac OS-X or Linux?

No. VBoxVmService is specially designed to suit the Windows operating system. And it doesn't need to run on Mac OS-X or Linux because these platforms supply adequate features to accomplish the same effect.


### 7. Credits

First version of VBoxVmService, patching together the C++ code of VBoxVmService and the helper tool "sleep.exe", messing with some c# for "bcompile.exe" plus all the batch-scripting was done by Mathias Herrmann.

Only Fang joined the project in August 2010, and had become main contributor since then. He re-designed the service, added VmServiceControl.exe to do all the controlling job and changed VBoxVmService.exe to become a pure service program.

Runar Buvik joined the project in December 2010, bringing lots of creative improvements like two way communication between the service and control program, new commands to show current VM status and check service environment, etc.

