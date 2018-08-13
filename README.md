# Ghost-ssh

Ghost-ssh is a tool that will let you automate running commands via ssh in a resource and access constraint environment.

#### Conditions:
* You cannot add ssh keys into the remote machine   
  - There are no home accounts for each user
  - You don't have write access or are constraint to make any changes
* You need to run bash commands from a remote system to a secure system to which you have password based access
* you cannot install expect or sshpass
* you don't have access to ssh libraries in python or any other language
* you cannot copy huge binaries to the machine
* you cannot run a server on the remote machine (as all secure machines are protected through a firewall and opening ports needs justification)

If you find yourself in between these constraints then this tool might be useful for you.

This tool is an ssh client that takes care of supplying password to the ssh session while authenticating and then send over commands to the machine and read a response from it.

#### Dependencies

##### Runtime
  - **[libssh2](https://www.libssh2.org/)**

This is the only extra library that you need to run this binary. All other ones ([libcrypto](https://wiki.openssl.org/index.php/Libcrypto_API) or [libgcrypt](https://www.gnupg.org/documentation/manuals/gcrypt/), libpthread are already present in a system)

##### Buildtime
  - libssh2 developer packages ([libssh2-devel](https://centos.pkgs.org/6/centos-x86_64/libssh2-devel-1.4.2-2.el6_7.1.x86_64.rpm.html) or [libssh2-1-dev](https://packages.debian.org/jessie/libssh2-1-dev))

#### Build instructions:

**Note** - Edit the commands in the `ghostssh.c` file to what even you need. This binary will get the average cpu load and memory usage for a machine

There are 2 modes for this to build:
1. With `GHOSTSSH_USERNAME` and `GHOSTSSH_PASSWORD` flags in GCC. This will ask you for the password for the remote machine(s) and then build the binary with that information.

  ```
  $ sudo make static GHOSTSSH_USERNAME=<username>
  ```

  **Note** - The binary will contain the username and password in plain text, so be sure to have the permissions of the new binary to be `--x------`, which is only owner has the permission to execute it and no one can read the binary. (this step is included in the Makefile for target `static`)

2. Without any extra flag. This mode will build the binary in such a way that it will ask for the password once during startup and have it in memory and use it to authenticate with all machines.
```
$ sudo make build
```

#### How to run the binary

The binary reads the IPs it needs to run the commands on from a file in the same directory as the binary from a file named `ip.txt` with IPs one per line.

with all of these, this binary can be run as:
```
$ ./ghostssh <username>
OR
$ ./ghostssh
```

#### Some tips
* this binary runs in an infinite loop running the provided commands on each machine as then sleeps for 5s. It prints the logs in `stderr` and output in `stdout`.

The  `stdout` output format is:
```
[YYYY-MM-DD HH:MM:SS] | IP | type | output
```

* The build size of the binary is around 24K, so to copy the binary

```
$ cat ghostssh | base64
z/rt/gcAAAEDAA...
...ZvAAA=

$ cat bin_in_base64.txt | base64 -d > ghostssh
```

* This has an implementation of a thread safe queue from which multiple threads can pick up ssh sessions to work on. But for the purpose of instrumentation, the number of threads is equal to the number of hosts in the ip.txt file

#### Improvements:
* Make it bullet proof - check if the session to a host has terminated, if yes, start a new one
* Allow commands to be configurable as the username/password
