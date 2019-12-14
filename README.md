# VBoxVmService

### 1. What is VBoxVmService?

VBoxVmService is a Windows Service, that enables the host-system to run or shutdown VirtualBox Virtual Machines (VMs) without a user being logged on to the host-system running Windows (Win7/Win8/Win10/2012/2016, both x86 and x64 supported).

### 2. So why did you create it?

First off: I think, VirtualBox is a great piece of software. But it lacks one feature that I really wanted: A method to operate my VMs controlled by a service and not having to log on to the host-system to be able to start or shutdown the software.

Looking for a solution, I went to the VirtualBox forums - just to find that many other users where looking for a way to do the exact same thing.

There are some methods out there to automate the start of virtual machines at the event of a user logon (like placing a registry entry to call a startup batch file in 

    "[HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Run]"

, or simply to create a link to start a VM in your startup-folder.

(More on this topic can be found here: http://www.bleepingcomputer.com/tutorials/tutorial44.html)

But since this was not exactly what I had in mind, I started to google - hoping to find some easy solution to my problem. But I was out of luck. What I did find, was an interesting article, written by Xiangyang Liu and issues on codeproject.com. 

He described a method of running regular programs as NT Services. And alongside all the useful infos, the article (found here: http://www.codeproject.com/KB/system/xyntservice.aspx ) featured a built and ready to use version of the wrapper-service described, plus a complete set of sourcecodes. So, Bingo then?  Well, ... not yet. :-( 

I found the problem to be, that VirtualBox is not just one executable program, but consists of a host-service ("VBoxSVC.exe") and a set of tools and subservers to control and interact with the actual Virtual Machines. To make this a quick one: It just didn't work.

In the end, I found myself spending some three or four nights reading about the concepts of windows' authorization methods on MSDN and taking a quick dive into general C++ programming, to understand Xiangyang Liu's code.

Finally I got some basic understanding of what Xiangyang's code was doing and I managed to patch it, adding a method to safely shutdown running VMs when the master process is called to terminate.
 
To get around such set-backs as pathvalues or VM-names containing blanks, or finding a way how to obtain the needed administrative rights to run VirtualBox, I added some great tools I found on the Internet, while looking for other things. *gg*

And guess what? It finally worked! So here it is! :-)


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

It hasn't been fully tested on systems other than Windows 7, Windows 8 and Windows 10, but it is supposed to work on server systems like Win2012, Win2016, ... etc, both 32bit and 64bit versions.


### 6. Will VBoxVmService run on Mac OS-X or Linux?

No. VBoxVmService is specially designed to suit the Windows operating system. And it doesn't need to run on Mac OS-X or Linux because these platforms supply adequate features to accomplish the same effect.


### 7. Credits

The puzzlework on "how the %$&!? this thing works", patching together the C++ code of VBoxVmService and the helper tool "sleep.exe", messing with some c# for "bcompile.exe" plus all the batch-scripting was done by Mathias Herrmann .

Only Fang (aka FB2000) joined the project in August 2010, and had become main contributor since then. He re-designed the service, added VmServiceControl.exe to do all the controlling job and changed VBoxVmService.exe to become a pure service program.

Runar Buvik joined the project in December 2010, bringing lots of creative improvements like two way communication between the service and control program, new commands to show current VM status and check service environment, etc.

