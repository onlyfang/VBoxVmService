## Change log

### 2018-12-21 Release 6.0 "Pumpkin"
* Updated COM API to make it work with VirtualBox 6.0. Note this is not compatible with old versions of VirtualBox. If you need to use VirtualBox 5.2.X, please stay with 5.2 "Jujube".

### 2018-10-28 Move code to GitHub

Hope this will make it easier for other people to contribute to this project, by sending pull requests.

### 2017-10-19 Release 5.2 "Jujube"
* Updated COM API to make it work with VirtualBox 5.2. Note this is not compatible with old versions of VirtualBox. If you need to use VirtualBox 5.1.X, please stay with 5.1 "Plum".

### 2016-07-14 Release 5.1 "Plum"
* Updated COM API to make it work with VirtualBox 5.1. Note this is not compatible with old versions of VirtualBox. If you need to use VirtualBox 5.0.X, please stay with 5.0 "Litchi".
* Enable xp/2k3 campatible mode.

### 2015-07-11 Release 5.0 "Litchi"
* Updated COM API to make it work with VirtualBox 5.0. Note this is not compatible with old versions of VirtualBox. If you need to use VirtualBox 4.3.X, please stay with 4.1 "Ginger".
* Changed VBOX_INSTALL_PATH to VBOX_MSI_INSTALL_PATH.
* During installation, will try to guess out correct VBOX_USER_HOME when HOME environment variable is set.
* Project is now built with Visual Studio 2013 Express.

### 2013-10-16 Release 4.1 "Ginger"

* Updated COM API to make it work with VirtualBox 4.3. Note this is not compatible with old versions of VirtualBox. If you need to use VirtualBox 4.2.X, please stay with 4.0 "Bayberry".

### 2013-06-23  Release 4.0 "Bayberry"

* Updated doc about trouble shooting steps.
* Return detailed description instead of error code.
* Added a GUI installer created with InnoSetup.
* Exe files are no longer saved in git repository.
* The long wanted system tray icon feature was finally implemented. The newly added VmServiceTray.exe works as the GUI counterpart to VmServiceControl.exe, it shows all configured VMs and their states, allows single VMs to be started/stopped. It can also be used to start/stop VBoxVmService directly (with UAC elevation on Vista and later).
* Moved folders up to one level.
* Added a new command "VmServiceControl -l" to list all VMs' states.
* Removed ServiceName option from config file for simplify purpose. There is no need to change this anyway.
* Project is now built with Visual Studio 2012 Express.

### 2012-09-15 Release 3.1 "Coconut"

* Updated COM API to make it work with VirtualBox 4.2. Note this is not compatible with old versions of VirtualBox. If you need to use VirtualBox 4.1.X, please stay with 3.0 "Sweet Potato".
* Service now runs (again) as LocalSystem account by default.
* Allow additional parameters to be set for VBoxWebSrv.exe.
* Service is no longer set to DelayedAutostart. It's now Autostart and depends on Workstation and COM+.
* Fixed a bug in sending pipe commands: need to write an additional null byte at end of message;
* Removed pipelcat to simplify pipe comminucation;
* Fixed compile warnings.

### 2011-11-29 Release 3.0 "Sweet Potato"

* Force reloading INI file when VBoxVMService commands are executed;
* Always try to stop all configured VMs when service is stopped.
* Patch from Scholar Team and Harold Roussel: allow VBoxWebSrv.exe to run when VBoxVmService starts, giving access to the currently running virtual machines via phpVirtualBox.
* Calling VirtualBox COM API directly instead of using VBoxManage.exe. 
* Service now runs as normal user, with username and password configed in VBoxVmService.ini.

### 2011-07-21 Release 2.3 "Watermelon"

* Always require a reboot after installation;
* Reuse VRDP settings from VirtualBox Manager, and don't call modifyvm to change port anymore;
* Added an AutoStart option to control if a VM will be started automatically when VBoxVmService runs;
* Show full command line including path during log;
* Fixed a bug that meaningless chars are printed to log;
* Set service to delayed auto-start if possible.

### 2011-01-13 Release 2.2 "Fireworks"

* Update to make VBoxVmService work with VirtualBox 4.0. Note this is incompatible with old versions of VBox, so stay with "Reindeer" if you are still running VBox 3.X.X.

### 2010-12-24 Release 2.1 "Reindeer"

* PauseShutdown now became global setting. When service is stopped, it doesn't make sence to wait VM0 to shutdown, then VM1, then VM2, ..., etc. Now all the VMs will start shutting down at once, and all are expected to finish within time specified as PauseShutdown.
* Added too way communication between the service and control program. Show service output after control command get executed.
* Added new commands to show current VM status and check service environment.
* Better input sanitizing.

### 2010-09-04 Release 2.0 "Red Dragon"
Changes since version rel20100817-win7

* Removed all the batch files and call VBoxManage.exe directly at VBoxVmService.
* Added VmServiceControl.exe to do all the controlling job and VBoxVmService.exe now become a pure service program.
* Reworked some of the documentation.
* Dropped some parameters from the ini-file, please review Howto.txt for a list of currently valid options

### Changes since version rel20091210-win7

* VBOX_USER_HOME will now be set up during the installation process.
* Minor improvements on the overall code.
* Simplified the overall installation procedure.
* Reworked some of the documentation.


### Changes since version rel20091103-win7

* Support for the VBOX_USER_HOME environment variable has been added.
* Hardlinking was dropped from install procedure.
* Documentation was reworked.


### Changes since version rel20081010

* Windows 7 support has been added. Thanks to user FB2000 for his patches and his extensive research regarding that topic!


### Changes since version rel20081009

* There have been errors while putting together the package. This Release was recalled!


### Changes since version rel20080728

* Improved the event-logging to VBoxVmService.log. It will now capture the response messages from the VirtualBox components.
* A PARAMETER NAME WITHIN THE FILE VBoxVmService.ini HAS BEEN CHANGED: PLEASE RENAME THE PARAMETER FROM "PathToVBoxVRDP" TO "PathToVBoxHeadless" IF YOU ARE UPGRADING FROM AN EARLIER VERSION AND ARE PLANNING TO USE YOUR OLD INI-FILE!


### Changes since version rel20080422

* Added a BRUTEFORCE option to the startup-calls. This will kill any possibly remaining run- or lockfiles for a VM before startup.
* The files startup and shutdown now save all their outputs to the logfile.


### Changes since version rel20080308

* A german documentation has been created and the documenation has seen an overall update. 
* A new option has been added to the .INI-File, enabling the individual configuration of 
* the VRDP-ports of virtual machines for remote administration purposes.


### Changes since version rel20080223

* For the helper application "bat2exe.exe" (by Fatih Kodak) was getting false positives on several antivirus systems, it has been substituted by "bcompile.exe", which should work without further problems. The sourcecode of "bcompile.exe" can be found underneath the "src\c#\"-subdirectory. For it had to be a quick solution, there is a small issue with undeleted temp-files piling up in the temp-folder. This will be taken care of in the next upcoming release.

* I added a "StartupMethod" flag to the .INI-file to support as well VRDP and SDL (the VM- controller of the OSE (Open Source Edition)). Thanks to 'Technologov' at the VirtualBox Forums for triggering that! Yet, SDL support is still sort of buggy and needs some more time and review... 

