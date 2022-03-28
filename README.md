
# Summary

While auditing the source code of `smbfs.kext`, five vulnerabilities were found....


# Verified machine configurations

All software provided with this research have been tested on following machine configurations

### 1: Apple M1 MacBook Pro

- Machine Identifier: MacBookPro18,1
- System Version: macOS 12.3 (21E230)
- RAM: 16 GB
- SSD: 1TB

### 2: Apple M1 Mac mini

- Machine Identifier: Macmini9,1
- System Version: macOS 12.3 (21E230)
- RAM: 8 GB
- SSD: 256 GB


# Building

### Prerequisites

- Xcode (Xcode 13.3 recommended)

### Supported targets

#### MACOS_21E230_T6000_RELEASE

- MacOS: 21E230
- Kernel: kernel.release.t6000

#### MACOS_21E230_T8101_RELEASE

- MacOS: 21E230
- Kernel: kernel.release.t8101

### Generating release builds

```bash
./build.sh <TARGET_CONFIGURATION>
```

TARGET_CONFIGURATION should be one of

- MACOS_21E230_T6000_RELEASE
- MACOS_21E230_T8101_RELEASE

Compiled binaries will be stored on `build/${TARGET_CONFIGURATION}` directory

### Development using Xcode

Set the target configuration in `env.h` header file present in the root source directory. 


# Proof of Concepts

The projects present in the `poc` directory provides detailed documentation and minimal program required to trigger or demonstrate a vulnerability found in this research. The following sections provides instructions for running these PoC projects.

### 1: `poc_double_free` 

The `SMBIOC_UPDATE_CLIENT_INTERFACES` ioctl command is used in `smbfs.kext` to provide information about network interfaces in the client computer from user land to kext. This information is required for providing the multi channel functionality in SMB. A malicious application may provide crafted data with this ioctl command to trigger a double free vulnerabiltiy and by carefully controlling the kernel memory, the malicious application may exploit this vulnerability to achieve arbitary kernel memory read write

This PoC provides a minimal program required to trigger this vulnerability in `smbfs.kext`

#### Steps for running

**NOTE: running this PoC will trigger kernel panic**

This PoC can be directly run from Xcode by using scheme `poc_double_free`. To run from command line,
 
```bash
cd ./build/${TARGET_CONFIGURATION}/poc
./poc_double_free
```

The vulnerability triggered by this PoC is exploited in `xe_kmem` project to achive arbitary kernel memory read write. See `xe_kmem/smb_dev_rw.c` for more details 

### 2: `poc_sockaddr_oob_read`

The `SMBIOC_NEGOTIATE` ioctl command in `smbfs.kext` is used to setup a new session with a SMB server. An malicious application may provide crafted data with this ioctl command to trigger out of bound (OOB) read in kernel memory. The OOB read data will be stored in a location in kernel memory which can later be retrieved by the malicious application using `smbfsGetSessionSockaddrFSCTL` fsctl command. 

This PoC provides a minimal program required to demonstrate this vulnerability in `smbfs.kext`

#### Steps for running

##### Start the `xe_smbx` server

To start `xe_smbx` server from Xcode, use the scheme `xe_smbx`. To start from command line,

```bash
cd build/${TARGET_CONFIGURATION}
./xe_smbx
```

##### Run the PoC

This PoC can be directly run from Xcode by using scheme `poc_sockaddr_oob_read`. To run from command line,

```bash
cd build/${TARGET_CONFIGURATION}/poc
./poc_sockaddr_oob_read
```

The vulnerability demonstrated in this PoC is used in `xe_kmem` to leak critical information from kernel memory required for exploting the double free vulnerability demonstrated in poc 1. See `xe_kmem/memory/xe_oob_reader_base.c` for more details

>>> NOTE: The technique used in the PoC to read the OOB read data back to user land requires user intraction (TCC prompt approval from kTCCServiceSystemPolicyNetworkVolumes service). This limitation is not present in the technique used in `xe_kmem` project which combines this vulnerability with another OOB read vulnerability to bypass this restriction

### 3: `poc_oob_write`

A malicious application may provide crafted socket addresses with `SMBIOC_NEGOTIATE` ioctl command which could lead to out of bound (OOB) write in kernel memory.

This PoC provides a minimal program required to trigger this vulnerability in `smbfs.kext`

#### Steps for running

**NOTE: running this PoC will trigger kernel panic**

This PoC can be directly run from Xcode by using scheme `poc_oob_write`. To run from command line,

```bash
cd build/${TARGET_CONFIGURATION}/poc
./poc_oob_write
```

The vulnerability triggered by this PoC is exploited in `xe_kmem` project to leak critical information from kernel memory required for exploiting the double free vulnerability demonstrated in poc 1. See `xe_kmem/memory/xe_oob_reader_ovf.c` for more details 


### 4: `poc_snb_name_oob_read`

A malicious application may provide crafted socket addresses with `SMBIOC_NEGOTIATE` ioctl command which will lead to OOB read in kernel memory and OOB read data being sent to the SMB server.

This PoC provides a minimal client and server program required to demonstrate this vulnerability in `smbfs.kext`

#### Steps for running

##### Start the fake SMB server: `poc_snb_name_oob_read_server`

To start fake SMB server from Xcode, use the scheme `poc_snb_name_oob_read_server`. To start from command line,

```bash
cd build/${TARGET_CONFIGURATION}
./poc_snb_name_oob_read_server
```

##### Run the PoC

To run the PoC from Xcode, use scheme `poc_snb_name_oob_read_client`. To run from command line,

```bash
cd build/${TARGET_CONFIGURATION}/poc
./poc_snb_name_oob_read_client
```


The OOB read data will be displayed in the output of fake SMB server

The vulnerability demonstrated in this PoC is used in `xe_kmem` project to leak to leak critical information required for exploiting the double free vulnerability triggered in poc 1. See `xe_kmem/xe_oob_reader_base.c` for more details


### 5: `poc_info_disclosure`

A malicious application may read address of allocated memory in zone default.1664 using `smbfsGetSessionSockaddrFSCTL2`.

This PoC project minimal program required for demonstrating this this vulnerability

#### Steps for running

##### Start the `xe_smbx` server

To start `xe_smbx` server from Xcode, use the scheme `xe_smbx`. To start from command line,

```bash
cd build/${TARGET_CONFIGURATION}
./xe_smbx
```

##### Run the PoC

This PoC can be directly run from Xcode by using the scheme `poc_sockaddr_oob_read`. To run from command line,

```bash
cd build/${TARGET_CONFIGURATION}/poc
./poc_info_disclosure
```

The vulnerability demonstrated in this PoC is not used in any of the exploits developed during this research. But a malicious application may use this vulnerability to gain information about kernel heap or to place controlled data in known kernel memory location etc.


# Impact

- 
- Complete compromise of user data protected by sandbox
- Access protected resources without user consent


# Demo

### Prerequisites

#### Start the arbitary kernel memory read / write server

The project `xe_kmem` exploits the vulnerabilities demonstrated / triggered in poc 1, 2, 3 and 4 to achieve arbitary kernel memory read write. This project provides the kernel memory read / write capability from the below demo projects by listening on a unix domain socket (By default `/tmp/xe_kmem.sock`. Can be changed using -k option). Run this project before running any of the below demo projects

##### Steps to run `xe_kmem`

###### Start the `xe_smbx` server

To start `xe_smbx` server from Xcode, use the scheme `xe_smbx`. To start from command line,

```bash
cd build/${TARGET_CONFIGURATION}
./xe_smbx
```

Keep this server running

###### Run the `xe_kmem` project

The recommended method to run this project is by using the release build of `xe_kmem` generated using `build.sh` script. To start `xe_kmem` release build from command line,

**NOTE: The system may crash while executing next command**

```bash
cd build/${TARGET_CONFIGURATION}
./xe_kmem
```

##### Troubleshooting

If you are unable to start the `xe_kmem` server within three attempts, then there may be a process running in your system which may be affecting critical system parameters that is not handled by `xe_kmem`. In this case, it is recommended to try using one of the following tested and verified setup configurations:-

###### Setup 1

- Machine configuration: 1 (Apple M1 MacBook Pro)
- New OS installation (No additional Apps installed)
- WiFi and Bluetooth turned off
- Not signed in to iCloud
- Battery above 90%
- Connected to charger
- Started `xe_smbx` and `xe_kmem` immediately after system boot

While attempting to start `xe_kmem` using this setup, only 1 out of 15 attempts to start the `xe_kmem` server failed.


###### Setup 2

- Machine configuration: 2 (Apple M1 Mac mini)
- New OS installation (No additional Apps installed)
- WiFi and Bluetooth turned off
- Not signed in to iCloud
- Started `xe_smbx` and `xe_kmem` immediately after system boot

In this setup only 1 out of 10 attempts to start the `xe_kmem` server failed. The attempt that failed was the 7th one


###### Setup 3

- Machine configuration: 1 (Apple M1 MacBook Pro)
- Additional apps installed: Azul Zulu JDK 11.54+25, Command Line Tools, Eclipse, Ghidra, Kernel Debug Kit (21E230), Sublime Text Xcode 13.3 and Xcode Additional Tools
- Disconnected from WiFi and Bluetooth
- Not signed in to iCloud
- Randomly started various apps and closed subset of these started apps
- Secure boot: Permissive Security
- FileVault: Enabled
- Connected to charger
- Battery above 90%
- Started `xe_smbx` and `xe_kmem` at a random moment after system boot. (It could also be during opening / closing of apps)

In this configuration only x out of 15 attempts to start `xe_kmem` server failed


### Demo 1: Running commands with root privilege

This demo demonstrates a technique that may be used by a malicious application to execute commands with root privileges without any user interaction after acquiring arbitary kernel memory read / write capability. 

To run this demo from command line,

```bash
cd build/${TARGET_CONFIGURATION}/demo
./demo_sudo <path-to-executable> [arg1] [arg2] ...
``` 

Example, to change the owner of a file to root

```bash
touch test
./demo_sudo `which chown` root:admin ./test 
```

You can also run this demo from Xcode using scheme `demo_sudo`


### Demo 2: Disabling sandbox file system restrictions

This demo demonstrates a technique that may be used by malicious application to disable all file system restrictions imposed by `Sandbox.kext` after acquiring arbitary kernel memory read / write capability.

This demo can be run directly from Xcode using `demo_sb` scheme. To run this demo from command line,

```bash
cd build/${TARGET_CONFIGURATION}/demo
./demo_sb
```

You can verify that all sandbox file system restrictions are disabled by trying to open the user `TCC.db` from command line. While the above command is running, open a new terminal window and run the following command 

```bash
hexdump ~/Library/Application\ Support/com.apple.TCC/TCC.db
```

This command will output the hexdump of `TCC.db` instead of exiting with "Operation not permitted" error.


### Demo 3: Recording audio and video without user consent and disabling microphone privacy indicator

This demo uses techniques in Demo 1 and Demo 2 to demonstrate a technique that may be used by malicious apps to modify user and system `TCC.db` and allow it to capture from camera, microphone and screen without user consent after acquiring kernel memory read / write capability. It also demonstrates that the microphone privacy indicator can also be disabled using kernel memory read / write capability.

This demo can be run only from command line using `Terminal.app`. To run this demo,

```bash
cd build/${TARGET_CONFIGURATION}/demo
./demo_av -a <mic|none> -v <screen|camera|none> capture.mov
```

Example, to record audio using built in microphone and video from screen,

First remove all TCC authorizations for Terminal.app

```bash
tccutil reset All com.apple.Terminal
```

Now to start the recording without any user consent, run

```bash
./demo_av -a mic -v screen capture.mov
```

The microphone privacy indicator will also be disabled during recording

**NOTE 1**: It is recommeded to reset all TCC authorizations for Terminal.app after running this demo by running the `tccutil reset All com.apple.Terminal` command. This demo will not automatically restore the modified `TCC.db`.

**NOTE 2**: Audio recording using mic and video recording using camera is tested only on Machine configuration 1 (Apple M1 MacBook Pro)


### Demo 4: Bypassing write protection switch in MacBook Pro SD Card reader

The MacBook Pro SD Card reader allows users to prevent writes on a SD Card using the write protection switch. This demo demonstrates a technique that may be used by malicious applications to disable this write protection after achieving arbitary kernel memory read / write.

Before running this demo, connect a SD Card with write protection enabled to the builtin SD Card reader slot of Apple M1 MacBook Pro. Also wait till the volumes on the SD Card are mounted before running the demo.

This demo can be run from Xcode using `demo_wp_disable` scheme. To run this demo from command line,

``` bash
cd build/${TARGET_CONFIGURATION}/demo
./demo_wp_disable
``` 

This demo will remount all the volumes on the SD card with read / write enabled. You can use the following command to verify that write protection is disabled

``` bash
echo "Hello" > /Volumes/<Volume name>/test
```

Where "Volume name" is the name of a mounted volume in SD Card. The above command should succeed instead of failing with "Read only filesystem" error

**NOTE: ** Finder will not always pickup the configuration change of mounted volumes from read only mode to read / write mode. You can still write to these volumes from command line.


### Demo 5: PACDA bypass

This demo demonstrates a technique which may be used by a malicious application to sign arbitary data with arbitary modifiers using DA pointer authentication key after obtaining arbitary kernel memory read / write capability


### Demo 6: Arbitary kernel function execution

This demo demonstrates a technique which may be used by a malicious application to execute arbitary kernel functions with full controll over register states (x0 - x30, sp and q0-q31) after acheiving PACDA bypass and arbitary kernel memory read / write

