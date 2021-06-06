## tp-xfan (ThinkPad XForms Fan)

`tp-xfan` is a simple program written in C which lets users with
any ThinkPad (that has ACPI fan support) modify their fan's speed,
going from 0 (auto), to a maximum of 8 (disengaged).

It also features an intuitive yet nostalgic Graphical User Interface,
written using XForms (hence the name, XForms Fan).

For more features, check [Features](#Features).

For a list of changes and/or news, check the `NEWS`
and `ChangeLog` files.

Initially written on May 28, 2021.
Current version can be found in `tp-xfan.c` under `src/`,
or by running `make version` (see [Installation](#Installation))

If you plan to work on `tp-xfan`, be sure to read `INDENTATION`.

Support this project: https://saloniamatteo.top/donate.html

## Flags
Currently, `tp-xfan` supports the following command-line flags:

| Flag | Argument | Description                                      |
|------|----------|--------------------------------------------------|
| `-d` | None     | Enable Debugging                                 |
| `-i` | None     | Print program info and exit (No XForms required) |
| `-h` | None     | Print help and exit                              |
| `-p` | Required | Use a custom path for the apply.sh script        |
| `-v` | None     | Be more verbose                                  |

## Features
`tp-xfan` has a lot of powerful features, including:

+ Value Slider to choose values
+ Setting a custom directory for the apply.sh script,
in case you don't have it in your current directory,
nor under `/usr/local/share/tp-xfan`.
+ AFSM (Automatic Fan Speed Management); See [AFSM](#AFSM).
+ Automatic recognition of root & non-root users
	- When running as root, no password is required,
	and the command is executed immediately, if the
	apply.sh script was found.
	- When running as a non-root user, the root
	password is asked (but not stored anywhere),
	in order to access `/proc/acpi/ibm/fan`.
	- When running as a non-root user with
	a setuid program (like sudo/doas), no
	password is required from `tp-xfan`.
+ Local Password Caching: the password is stored in a 
variable, which will only be accessible by `tp-xfan` itself.
+ Debugging functionalities, to see what `tp-xfan` really is doing
even when you cannot see it.
+ Verbosity, to see if the command to set the fan speed
actually worked or not.

Note that Debugging and Verbosity only output to the console.

## AFSM
Automatic Fan Speed Management (or AFSM) is a very powerful
feature of `tp-xfan`. It allows users to choose a CPU temperature
threshold, the speed of the fan, and the interval (in seconds).

For example, with a given threshold temperature of 50°C, AFSM will
start the fan after the seconds provided by the interval,
with the speed chosen by the user.

So, every interval seconds, AFSM will check if the current CPU
temperature is above the threshold: if it is, it will
keep going, setting the fan to the speed chosen by the user;
if the temperature is below, or equal to the threshold,
it will immediately set the fan speed to 0 (auto),
and it will cycle forever.

## Screenshots
Start Menu: the initial screen, shown when `tp-xfan` is opened.

![Start Menu](https://raw.githubusercontent.com/saloniamatteo/tp-xfan/master/pics/1-init-scr.png)

Info Screen: showing info.

![Info Screen](https://raw.githubusercontent.com/saloniamatteo/tp-xfan/master/pics/2-info-scr.png)

Password Screen: `tp-xfan` is asking the root password.

![Password Screen](https://raw.githubusercontent.com/saloniamatteo/tp-xfan/master/pics/3-pass-scr.png)

AFSM Screen: [Auto Fan Speed Management](#AFSM) screen.

![AFSM Screen](https://raw.githubusercontent.com/saloniamatteo/tp-xfan/master/pics/4-afsm-scr.png)

Exit Confirm: `tp-xfan` is asking the user if they really want to exit.

![Exit Confirm](https://raw.githubusercontent.com/saloniamatteo/tp-xfan/master/pics/5-exit-confirm.png)

## Requirements
In order to run `tp-xfan`, you need the following packages:

+ `xforms` (**required**): provides the core Forms functionality.
(Gentoo: x11-libs/xforms, others: http://xforms-toolkit.org)
+ `expect` (<u>optional</u>): used for escalating privileges,
when not running as root. (Gentoo: dev-tcltk/expect)
+ Linux's `su` (<u>optional</u>): used for escalating privileges,
when not running as root. Guaranteed to be present on a
fully working Linux system.

Also, you need to have the `thinkpad_acpi` module.
If you build your Kernel yourself, make sure the following are enabled:

```
Device Drivers --->
	[*] X86 Platform Specific Device Drivers  --->  
		<M>   ThinkPad ACPI Laptop Extras          
```

Then, make sure the driver is available:

`lsmod | grep thinkpad_acpi`

Then, write the following to `/etc/modprobe.d/thinkpad.conf`:

```
options thinkpad_acpi fan_control=1
```

Now, you should reboot. After rebooting, you should be able to
modify your ThinkPad's fan speed.

## Installation
(See [Requirements](#Requirements))

`tp-xfan` uses GNU AutoTools to increase its portabilty and flexibility.

Normally, users should run the following commands, to install `tp-xfan`:

```bash
./configure
make
# the following should be ran as root,
# or by prepending "sudo" or "doas".
make install install-data
```

This will install `tp-xfan`, and the default `apply.sh` script (/usr/local/share/tp-xfan/apply.sh)

If, for some reason, you cannot run the commands above, run `autoreconf --install`, then retry.

## Help
If you need help, you can:
- Create an issue;
- Open a pull request;
- Send me an email [(saloniamatteo@pm.me](mailto:saloniamatteo@pm.me) or [matteo@mail.saloniamatteo.top)](mailto:matteo@mail.saloniamatteo.top).
