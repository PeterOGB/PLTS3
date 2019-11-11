# Paper Less Tape Station user interface (V3) #

The Elliott 803 at The National Museum of Computing has a Paper Tape Station type 3A.  This is capable of 
having two tape readers, two tape punches and a teleprinter (output only) connected, but it only has one reader and one punch installed.

Some years ago I added the "Paper Less Tape Station" logic which acts like the logic for the second reader and the teleprinter.
The PLTS communicates with a Raspberry Pi via an RS-232 interface and the PI provides a GUI to allow paper tape image files to be selected and then downloaded into the virtual second reader.  Likewise output to the teleprinter is passed up to the PI where it is displayed in a window.

Recent enhancements to the PLTS and the PI application have allowed the PI's keyboard to be used to provide online or interactive input to the 803.

The 803 emulator has the PLTS functionality built-in, but in this case the interface is via a network socket (on port address 8038). 

This PLTS application can be used to connect to either the real 803 (via a usb to rsr232 interface) or to the emulator via a TCP/IP connection.  The protocol is identical in both cases.

## Requirements ##

The development versions of the following libraries need to beinstalled to build the emulator.

| Library        | Package Name |
|:-------------:|:-------------:|
| Gtk 3  | gtk+-3.0-dev |


The package names apply for Debian derived distributions

Also the "cmake" application is needed.

## Building the PLTS ##

Runing 

```
cmake .
make
```

is all that is required to produce the executable !
