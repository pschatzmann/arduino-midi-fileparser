# Arduino MIDI File Parser

A simple MIDI File Parser which is based on the [Midi Parser from Alexandre Bique](https://github.com/abique/midi-parser).

The original library has been converted to a Header Only C++ Library. Unlike the original implementation, we can feed the parser incrementally with data. This is important if you want to use it on a microcontroller that has only a limited amount of RAM.

It can be used as __Arduino library or as regular cmake library__, so you can also compile, run and debug it on a desktop computer.

## Documentation 

- [MidiFileParser](https://pschatzmann.github.io/arduino-midi-fileparser/docs/html/group__midifile.html)


## Installation in Arduino

For Arduino you can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with
```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/pschatzmann/arduino-midi-fileparser
```

## Feeding Data Incrementally

Provide the data with `write()` and call `parse()` to get the next element. When `parse()` returns `MIDI_PARSER_EOB`, the parser needs more data: nothing of the incomplete event is lost, so you can just write the next chunk and call `parse()` again. `MIDI_PARSER_ERROR` indicates invalid data.

## Sysex and Meta Events

The payload of sysex and meta events is available in `sysex.bytes` / `meta.bytes` with the number of bytes in `sysex.length` / `meta.length`:

- The payload is truncated to `PEEK_MAX` bytes (default 80). The remaining bytes of longer events are skipped automatically, so events can also be bigger than the input buffer. You can define `PEEK_MAX` before including the library to change the limit.
- The data is only valid until the next call of `parse()`: copy it if you need it later.
- The terminating `0xF7` of a sysex event is not counted in `sysex.length`.

## Building the Example on the Desktop

You can build the example sketch on the desktop 
```
cd ~/Documents/Arduino/libraries/arduino-midi-fileparser/examples/midi-parse
mkdir build
cd build
cmake ..
make
```

## Running the Unit Tests on the Desktop

The unit tests in [tests/](tests/) need only a C++11 compiler and cmake:
```
cd ~/Documents/Arduino/libraries/arduino-midi-fileparser
cmake -B build -DMIDI_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
You can also run the test executable `build/midi-file-tests` directly to see the result of each test case. The tests parse each file with different chunk sizes (from 1 byte up to the full buffer) and verify that the result is always identical.



