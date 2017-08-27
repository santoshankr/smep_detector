# smep_detector
Kernel module to help detect tampering with the SMEP bit

## Install

Install the headers for the kernel version you're running, then compile and insert the module.

```shell
sudo apt-get install linux-headers-$(uname -r)
make
sudo insmod smep.ko
```

This should create a character device at `/dev/smep`

## Usage

To check if SMEP is active on your processors, simply read from `/dev/smep`. The read handler iterates through all CPUs and reports the state of each one with a `printk` that shows up in `dmesg` and somewhere under `/var/log` depending on your syslog setup.

```
Aug 26 17:32:25 ubuntu kernel: [   32.635901] smep: read called
Aug 26 17:32:25 ubuntu kernel: [   32.635935] SMEP is active on cpu 0
Aug 26 17:32:25 ubuntu kernel: [   32.635971] SMEP is active on cpu 1
```

After running a kernel exploit that disables SMEP (see [CVE-2017-1000112](https://github.com/xairy/kernel-exploits/blob/master/CVE-2017-1000112/poc.c) for an example), this should change to

```
Aug 26 17:32:40 ubuntu kernel: [   42.667612] smep: read called
Aug 26 17:32:40 ubuntu kernel: [   42.667672] SMEP has been disabled on cpu 0
Aug 26 17:32:40 ubuntu kernel: [   42.667705] SMEP is active on cpu 1
```

You can alert off this change by having something in userspace poll `/dev/smep`, and monitoring syslog.

## Future

Ideally, we want to test this bit as soon as possible after a potential exploit. One option might be to register a kprobe at `commit_creds` entry, and trigger this check each time that function is called.
