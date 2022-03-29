
# Summary

While auditing the source code of `smbfs.kext`, following five vulnerabilities were found

1. Double free in `smb2_mc_parse_client_interface_array`. See poc #1
2. Out of bound read in `smb_sm_negotiate`. See poc #2
3. Out of bound write in `smb_dup_sockaddr`. See poc #2
4. Out of bound read in `nb_put_name`. See poc #4
5. Information disclosure in `smbfs_vnop_ioctl`. See poc #4

The first four vulnerabilities were used to develop arbitary kernel memory read / write exploit. Using just the kernel memory read write capability, the following demonstration softwares were developed

1. Software to execute commands with root privileges without any user interaction. See demo #1
2. Software to disable all file system restrictions imposed by `Sandbox.kext`. See demo #2
3. Software to capture camera, microphone and screen without any user interaction. (This software also disables the microphone privacy indicator. So user will not be notified that microphone is being used). See demo #3
4. Software to bypass the write protection provided by builtin SD Card Reader of Apple M1 MacBook Pro. See demo #4

To obtain more capabilities from kernel memory read write, a exploit was developed using a defect in `OSDictionary::ensureCapacity` method to sign aribitary values with arbitary pointers using DA pointer authentication key. 

This capability was then used to develop an exploit which uses fake small block descriptors to call arbitary kernel functions with full control over values in registers `x0 - x30`, stack pointer and `q0 - q31`

Using these new capabilities, the following demonstration softwares were developed

1. Software to demonstrate assigning arbitary entitlements to processes. See demo #5

All the exploits and demonstration softwares were tested to work on latest macOS operating system (21E230) running on latest Apple hardware (Apple M1 MacBook Pro and Apple M1 Macmini) in full security mode.


# Impact

A malicious application may :-

1. Cause complete compromise of user data protected using file system permissions and Sandbox. See demo #1 and #2
2. Access privacy sensitive resources protected by TCC without user consent and / or user notification (Microphone, camera etc.). See demo #3
3. Execute arbitary kernel functions leading to bypass of all restrictions imposed on user processes by the macOS kernel. See demo #5


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

#### 1: MACOS_21E230_T6000_RELEASE

- OS version: 21E230
- Kernel: kernel.release.t6000

#### 2: MACOS_21E230_T8101_RELEASE

- OS version: 21E230
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

### 1: `poc_double_free` - Double free in `smb2_mc_parse_client_interface_array`

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


### 2: `poc_sockaddr_oob_read` - Out of bound read in `smb_sm_negotiate`

The `SMBIOC_NEGOTIATE` ioctl command in `smbfs.kext` is used to setup a new session with a SMB server. An malicious application may provide crafted data with this ioctl command to trigger out of bound (OOB) read in kernel memory. The OOB read data will be stored in a location in kernel memory which can later be retrieved by the malicious application using `smbfsGetSessionSockaddrFSCTL` fsctl command. 

This PoC provides a minimal program required to demonstrate this vulnerability in `smbfs.kext`

#### Steps for running

##### Start the `xe_smbx` server

To start `xe_smbx` server from Xcode, use the scheme `xe_smbx`. To start from command line,

```bash
cd build/${TARGET_CONFIGURATION}
./xe_smbx
```

Keep this server running

##### Run the PoC

This PoC can be directly run from Xcode by using scheme `poc_sockaddr_oob_read`. To run from command line,

```bash
cd build/${TARGET_CONFIGURATION}/poc
./poc_sockaddr_oob_read
```

The vulnerability demonstrated in this PoC is used in `xe_kmem` to leak critical information from kernel memory required for exploting the double free vulnerability demonstrated in poc 1. See `xe_kmem/memory/xe_oob_reader_base.c` for more details

>>> NOTE: The technique used in the PoC to read the OOB read data back to user land requires user intraction (TCC prompt approval from kTCCServiceSystemPolicyNetworkVolumes service). This limitation is not present in the technique used in `xe_kmem` project which combines this vulnerability with another OOB read vulnerability to bypass this restriction


### 3: `poc_oob_write` - Out of bound write in `smb_dup_sockaddr`

A malicious application may provide crafted socket addresses with `SMBIOC_NEGOTIATE` ioctl command which could lead to out of bound (OOB) write in kernel memory.

This PoC provides a minimal program required to trigger this vulnerability in `smbfs.kext`

#### Steps for running

**NOTE: running this PoC will trigger kernel panic**

This PoC can be directly run from Xcode by using scheme `poc_oob_write`. To run from command line,

```bash
cd build/${TARGET_CONFIGURATION}/poc
./poc_oob_write
```

The vulnerability triggered by this PoC is exploited in `xe_kmem` project to leak critical information from kernel memory required for exploiting the double free vulnerability triggered in poc 1. See `xe_kmem/memory/xe_oob_reader_ovf.c` for more details 


### 4: `poc_snb_name_oob_read` - Out of bound read in `nb_put_name`

A malicious application may provide crafted socket addresses with `SMBIOC_NEGOTIATE` ioctl command which will lead to OOB read in kernel memory and OOB read data being sent to the SMB server.

This PoC provides a minimal client and server program required to demonstrate this vulnerability in `smbfs.kext`

#### Steps for running

##### Start the fake SMB server: `poc_snb_name_oob_read_server`

To start fake SMB server from Xcode, use the scheme `poc_snb_name_oob_read_server`. To start from command line,

```bash
cd build/${TARGET_CONFIGURATION}
./poc_snb_name_oob_read_server
```

Keep this server running

##### Run the PoC

To run the PoC from Xcode, use scheme `poc_snb_name_oob_read_client`. To run from command line,

```bash
cd build/${TARGET_CONFIGURATION}/poc
./poc_snb_name_oob_read_client
```

The OOB read data will be displayed in the console output of fake SMB server

The vulnerability demonstrated in this PoC is used in `xe_kmem` project to leak to leak critical information required for exploiting the double free vulnerability triggered in poc 1. See `xe_kmem/xe_oob_reader_base.c` for more details


### 5: `poc_info_disclosure` - Information disclosure in `smbfs_vnop_ioctl`

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


# Demo projects

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

Keep the `xe_kmem` server running while running the below demo programs

##### Troubleshooting

If you are unable to start the `xe_kmem` server within three attempts, then there may be a process running in your system which may be affecting critical system parameters that is not handled by `xe_kmem`. In this case, it is recommended to try using one of the following tested setup configurations (Based on trial runs, all the configurations mentioned below have success rate greater than 90%):-

###### Setup 1

- Machine configuration: 1 (Apple M1 MacBook Pro)
- New OS installation (No additional Apps installed)
- WiFi and Bluetooth turned off
- Not signed in to iCloud
- Battery above 90%
- Connected to charger
- Started `xe_smbx` and `xe_kmem` immediately after system boot

###### Setup 2

- Machine configuration: 2 (Apple M1 Mac mini)
- New OS installation (No additional Apps installed)
- WiFi and Bluetooth turned off
- Not signed in to iCloud
- Started `xe_smbx` and `xe_kmem` immediately after system boot


**NOTE: **: The `xe_kmem` server do start without any problems (9 out of 10 times) while programs like Xcode and Ghidra is open and while spotlight is indexing in the background. It is unlikely that you will have to use one of the above setup configuration to get it started.


### Demo 1: `demo_sudo` - Running commands with root privilege

This demo demonstrates a technique that may be used by a malicious application to execute commands with root privileges without any user interaction after acquiring arbitary kernel memory read / write capability. 

To run this demo from command line,

```bash
cd build/${TARGET_CONFIGURATION}/demo
./demo_sudo <path-to-executable> [arg1] [arg2] ...
``` 

Example, to change the owner of a file to root

```bash
touch test
./demo_sudo /usr/sbin/chown root:admin ./test 
```

You can also run this demo from Xcode using scheme `demo_sudo`


### Demo 2: `demo_sb` - Disabling sandbox file system restrictions

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


### Demo 3: `demo_av` - Recording audio and video without user consent and disabling microphone privacy indicator

This demo uses techniques in Demo 1 and Demo 2 to demonstrate a technique that may be used by malicious apps to modify user and system `TCC.db` and allow it to capture from camera, microphone and screen without user consent after acquiring kernel memory read / write capability. It also demonstrates that the microphone privacy indicator can also be disabled using kernel memory read / write capability.

This demo can only be run from command line using `Terminal.app`. To run this demo,

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


### Demo 4: `demo_wp_disable` - Bypassing write protection switch in MacBook Pro SD Card reader

The MacBook Pro SD Card reader allows users to prevent writes on a SD Card using the write protection switch. This demo demonstrates a technique that may be used by malicious applications to bypass this write protection after achieving arbitary kernel memory read / write.

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


### Demo 5: `demo_entitlements` - Running applications with arbitary entitlements

This demo uses arbitary kernel function execution capability to modify the `cs_blob` allocated on read-only zone and assign itself `com.apple.private.network.restricted.port.lights_out_management` entitlement. This entitlement is required for binding to `55555` privileged port.

It is recommended to run this demo from command line by executing following commands:

``` bash
cd build/${TARGET_CONFIGURATION}/demo
./demo_entitlements
```

If everythings goes well, the message "bind to privileged port succeded after adding entitlement" will be printed on the console output


# Conclusion

In this research a total of 5 vulnerabilities in `smbfs.kext` where found. 4 out of those 5 vulnerabilities were used to develop an exploit to obtain arbitary kernel memory read write capability. This capability was then used to develop exploits for arbitary PACDA signing and arbitary kernel function execution.

The double free vulnerability that was used to obtain kernel memory read write was relatively easy to exploit, since the time interval between first and second release can be easily controlled. The out of bound read vulnerabilities allowed us to gain critical information from kernel memory required for exploiting the double free vulnerability. Lack of limit to number of NICs that could be attached to an SMB session and the number of IP addresses that could be associated with an NIC allowed us to make fast and large number of allocations on kernel memory. Also the lack of validations on socket address before associating it to an NIC allowed us to control the data in these allocated kernel memory locations. This inturn allowed us to develop quick heap sprays in `default.*` zones with element size less than or equal to 256.

After obtaining kernel memory read write, it was straight forward to disable file system restrictions imposed by sandbox. Since the memory for array `mac_policy_list->entries` storing reference to registered mac policies is writable, we could directly modify the pointer value in this array and replace it with address to a fake mac policy. The function pointers relevant to imposing file system restrictions will be set to NULL in the `mac_policy_ops` of the fake policy. Once file system restrictions are disabled, we can directly modify the user and system `TCC.db` to get unauthorized access to other protected resources.

The mechanism used by `msdosfs.kext` for caching the most recently used FAT block allowed us to copy data between arbitary vnodes and kernel memory locations. This allowed us to develop a fast kernel memory reader / writer and also allowed us to gain root priviliges by modifying the `/etc/sudoers` file. (This technique could also be used to modify the user and system `TCC.db` directly without having to disable sandbox file system restrictions).  

A quick scan of kernel binary using Ghidra showed multiple occasions where callee-saved general purpose registers were used as operands for PACDA instruction. By developing a method to acquire / release arbitary read write locks in kernel memory, we were able to exploit one of those occasions to sign arbitary values with arbitary modifiers using PACDA. With arbitary PACDA signing, we were able to assign fake small block descriptors to Blocks, which allowed us to develop a three link chain to execute arbitary kernel functions with fully controlled arguments. 

The PACDA exploit allows signing ~27 pointers per second and the kernel function calling exploit allows making ~32 arbitary function calls per second (Tested on Apple M1 MacBook Pro with kernel memory read / write provided by `xe_kmem`. Kernel function calls per second were measured by calling `vn_default_error` function, since it has only two instructions). Eventhough the arbitary kernel memory read write exploit will work only on macOS platforms, I beleive the techniques used for PACDA and kernel function exploits can be used on other platforms like iOS and iPadOS also.

