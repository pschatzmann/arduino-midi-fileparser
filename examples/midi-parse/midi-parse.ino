/**
 * We provide a midi file as hex array and parse it incrementally.
*/
#include "MidiFile.h"
#include "example-midi.h"
#include <algorithm>

const int write_size = 256;
MidiFile mf;
int pos = 0;
bool debug = false;

void setup() {
#ifdef ARDUINO
  Serial.begin(19200);
#endif
  mf.begin(debug, 256 * 5);
}

void loop() {
  if (!mf) return;

  // Try to keep buffer filled
  if (mf.availableForWrite() > write_size) {
    int len = std::min(write_size, (int)example_mid_len - len);
    if (pos < example_mid_len) {
      mf.write(example_mid + pos, len);
    } else {
      // when there is no more data we let the parser know
      mf.end();
    }
    pos += 256;
  }

  // Parse midi
  auto state = mf.parseTimed();  // or parse();

  // Process Result
  switch (state.status)
  case MIDI_PARSER_TRACK_MIDI: {
    // process midi
    printf("process");
    printf("  time: %ld - %ld", (long)state.timeInTicks(), (long)state.timeInMs());
    printf("  status: %d [%s]", state.midi.status,
           mf.midi_status_name(state.midi.status));
    printf("  channel: %d", state.midi.channel);
    printf("  param1: %d", state.midi.param1);
    printf("  param2: %d\n", state.midi.param2);
    break;
  case MIDI_PARSER_ERROR:
    printf("Error\n");
  case MIDI_PARSER_EOB:
#ifdef ARDUINO
    while(true);
#else
    exit(-1);
#endif
    break;
    default:
    break;
  }
}