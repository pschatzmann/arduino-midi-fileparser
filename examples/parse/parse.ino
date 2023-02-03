#include "MidiFile.h"
#include "example-midi.h"
#include <algorithm>

const int write_size = 256;
MidiFile mf;
int pos = 0;

void setup() {
#ifdef ARDUINO
  Serial.begin(19200);
#endif
  mf.begin(false,  256*5);
}

void loop() {

  // try to keep buffer filled
  if (mf.availableForWrite() > write_size) {
    int len = std::min(write_size, (int)example_mid_len - len);
    mf.write(example_mid + pos, len);
    pos += 256;
  }

  // parse midi
  auto state = mf.parse();
  if (state.status == MIDI_PARSER_TRACK_MIDI) {
    // process midi
    printf("process");
    printf("  time: %ld", (long)state.vtime);
    printf("  status: %d [%s]", state.midi.status,
           mf.midi_status_name(state.midi.status));
    printf("  channel: %d", state.midi.channel);
    printf("  param1: %d", state.midi.param1);
    printf("  param2: %d", state.midi.param2);
  }
}