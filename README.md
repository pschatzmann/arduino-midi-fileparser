# Arduino MIDI File Parser

A simple MIDI File Parser which is based on the [Midi Parser from Alexandre Bique](https://github.com/abique/midi-parser).

The original library has been converted to a Header Only C++ Library. Unlike the original implementation, we can feed the parser incrementally with data. This is important if you want to use it on a microcontroller that has only a limited amount of RAM.

It can be used as __Arduino or as cmake library__, so you can also compile, run and debug it on a desktop computer.




