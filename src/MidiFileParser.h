/**
 * @file MidiFileParser.h
 * @author Phil Schatzmann
 * @brief A simple midi parser based on the following project
 * https://github.com/abique/midi-parser
 *
 * @version 0.1
 * @date 2023-02-02
 *
 * @copyright Copyright (c) 2023
 *
 */
#pragma once

#include "MidiFileParserState.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/**
 * @defgroup midifile Midi File Parser
 * @brief Midi File Parser
 */



// Compiling outside of Arduino requires the millis() function.
#ifndef ARDUINO
#include <chrono>
#include <cstdint>
#include <iostream>

inline uint64_t millis() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch())
      .count();
}
#endif

#define MIDI_BUFFER_SIZE 1024 * 2
#define MIDI_MIN_REFILL_SIZE 512

namespace midi {

#ifndef ARDUINO
class Print {
  virtual size_t write(uint8_t) = 0;
};
#endif

class MidiFileParserMultiTrack;


/**
 * @brief Midi File parser. Provide the data via write: You should try to keep
 * the buffer as full as possible while parsing. You get the next parsing result
 * by calling the parse() method.
 * @ingroup midifile
 * @author Phil Schatzmann
 */
class MidiFileParser : public Print {
public:
  /// Initializes & starts the processing
  bool begin(bool log = true, int bufferSize = MIDI_BUFFER_SIZE) {
    log_active = log;
    track_no = 0;
    parser_state.in.resize(bufferSize);
    is_ok = true;
    reset();
    return true;
  }

  size_t write(uint8_t c) override { return write(&c, 1); }

  /// Feed/Provide the midi data to the parser
  virtual size_t write(const uint8_t *data, size_t len) {
    // store actual processing length
    if (len > 0) {
      write_len = len;
    }
    // write only if data is available
    if (len <= (size_t)parser_state.in.availableForWrite()) {
      return parser_state.in.write(data, len);
    }
    return 0;
  }

  /// Max number of bytes that we can write
  int availableForWrite() { return parser_state.in.availableForWrite(); }  // int: like Print

  /// Parse data in order to provide the next midi element
  midi_parser_state &parse() {
    // parse next element
    parser_state.status = MIDI_PARSER_EOB;
    if (!parser_state.in.isEmpty()) {
      midi_parser_status status = midi_parse();
      parser_state.status = status;
      if (log_active) {
        logStatus(status);
      }
    }
    // update is_ok
    if (parser_state.status == MIDI_PARSER_EOB ||
        parser_state.status == MIDI_PARSER_ERROR) {
      is_ok = false;
    }
    return parser_state;
  }

  /// Parse data in order to provide the next midi element considering the
  /// times.
  midi_parser_state &parseTimed() {
    static uint64_t timeout = 0l;

    // Wait for event to become relevant
    if (timeout != 0l) {
      if (millis() < timeout) {
        return not_ready;
      }

      timeout = 0;
      return parser_state;
    }

    // Parse next element
    if (!parser_state.in.isEmpty()) {
      midi_parser_status status = midi_parse();
      // delay result ?
      if (parser_state.timeInMs() > 0) {
        timeout = millis() + parser_state.timeInMs();
        return not_ready;
      } else {
        parser_state.status = status;
      }
    }

    // Update is_ok to false when we have an error or we are at the end
    if (parser_state.status == MIDI_PARSER_EOB ||
        parser_state.status == MIDI_PARSER_ERROR) {
      is_ok = false;
      return eob;
    }
    return parser_state;
  }

  /// Returns false after an error or when all data has been consumed
  operator bool() { return is_ok; }

  /// Ends the processing: currently does nothing
  void end() {}

  /// Provides the string description for the midi_status value
  const char *midi_status_name(int status) {
    switch (status) {
    case MIDI_STATUS_NOTE_OFF:
      return "Note Off";
    case MIDI_STATUS_NOTE_ON:
      return "Note On";
    case MIDI_STATUS_NOTE_AT:
      return "Note Aftertouch";
    case MIDI_STATUS_CC:
      return "CC";
    case MIDI_STATUS_PGM_CHANGE:
      return "Program Change";
    case MIDI_STATUS_CHANNEL_AT:
      return "Channel Aftertouch";
    case MIDI_STATUS_PITCH_BEND:
      return "Pitch Bend";

    default:
      return "(unknown)";
    }
  }

  /// Provides the string description for the file format
  const char *midi_file_format_name(int fmt) {
    switch (fmt) {
    case MIDI_FILE_FORMAT_SINGLE_TRACK:
      return "single track";
    case MIDI_FILE_FORMAT_MULTIPLE_TRACKS:
      return "multiple tracks";
    case MIDI_FILE_FORMAT_MULTIPLE_SONGS:
      return "multiple songs";

    default:
      return "(unknown)";
    }
  }

  /// Provides the string description for the midi_meta value
  const char *midi_meta_name(int type) {
    switch (type) {
    case MIDI_META_SEQ_NUM:
      return "Sequence Number";
    case MIDI_META_TEXT:
      return "Text";
    case MIDI_META_COPYRIGHT:
      return "Copyright";
    case MIDI_META_TRACK_NAME:
      return "Track Name";
    case MIDI_META_INSTRUMENT_NAME:
      return "Instrument Name";
    case MIDI_META_LYRICS:
      return "Lyrics";
    case MIDI_META_MAKER:
      return "Maker";
    case MIDI_META_CUE_POINT:
      return "Cue Point";
    case MIDI_META_CHANNEL_PREFIX:
      return "Channel Prefix";
    case MIDI_META_END_OF_TRACK:
      return "End of Track";
    case MIDI_META_SET_TEMPO:
      return "Set Tempo";
    case MIDI_META_SMPTE_OFFSET:
      return "SMPTE Offset";
    case MIDI_META_TIME_SIGNATURE:
      return "Time Signature";
    case MIDI_META_KEY_SIGNATURE:
      return "Key Signature";
    case MIDI_META_SEQ_SPECIFIC:
      return "Sequencer Specific";

    default:
      return "(unknown)";
    }
  }


protected:
  friend MidiFileParserMultiTrack;
  bool log_active = false;
  int write_len = 256;
  midi_parser_state parser_state;
  bool is_ok = true;
  int track_no = 0;
  midi_parser_state not_ready{MIDI_PARSER_DELAY};
  midi_parser_state eob{MIDI_PARSER_EOB};
  /// Payload of the last sysex or meta event (truncated to PEEK_MAX bytes).
  /// Valid until the next parse() call.
  uint8_t event_data[PEEK_MAX];
  /// Number of payload bytes of a truncated event that still need to be skipped
  int32_t skip_remaining = 0;
  /// The delta time of the current event has been consumed already
  bool vtime_pending = false;

  void setState(midi_parser_status state){
    parser_state.status = state;
    if (parser_state.status == MIDI_PARSER_EOB ||
        parser_state.status == MIDI_PARSER_ERROR) {
      is_ok = false;
    }
  }

  void logStatus(midi_parser_status status) {
    switch (status) {
    case MIDI_PARSER_EOB:
      printf("MIDI_PARSER_EOB\n");
      break;

    case MIDI_PARSER_ERROR:
      printf("MIDI_PARSER_ERROR\n");
      break;

    case MIDI_PARSER_INIT:
      printf("MIDI_PARSER_INIT\n");
      break;

    case MIDI_PARSER_HEADER:
      printf("\nheader\n");
      printf("  size: %d\n", parser_state.header.size);
      printf("  format: %d [%s]\n", parser_state.header.format,
             midi_file_format_name(parser_state.header.format));
      printf("  tracks count: %d\n", parser_state.header.tracks_count);
      printf("  time division: %d\n", parser_state.header.time_division);
      break;

    case MIDI_PARSER_TRACK:
      printf("\ntrack\n");
      printf("  length: %d\n", parser_state.track.size);
      break;

    case MIDI_PARSER_TRACK_MIDI:
      printf("\ntrack-midi\n");
      printf("  time: %ld\n", (long)parser_state.vtime);
      printf("  status: %d [%s]\n", parser_state.midi.status,
             midi_status_name(parser_state.midi.status));
      printf("  channel: %d\n", parser_state.midi.channel);
      printf("  param1: %d\n", parser_state.midi.param1);
      printf("  param2: %d\n", parser_state.midi.param2);
      break;

    case MIDI_PARSER_TRACK_META:
      printf("\ntrack-meta\n");
      printf("  time: %ld\n", (long)parser_state.vtime);
      printf("  type: %d [%s]\n", parser_state.meta.type,
             midi_meta_name(parser_state.meta.type));
      printf("  length: %d\n", parser_state.meta.length);
      break;

    case MIDI_PARSER_TRACK_SYSEX:
      printf("\ntrack-sysex\n");
      printf("  time: %ld\n", (long)parser_state.vtime);
      break;

    case MIDI_PARSER_DELAY:
      break;

    default:
      printf("\nunhandled state: %d\n", status);
      break;
    }
  }

  void reset() {
    parser_state.in.reset();
    parser_state.status = MIDI_PARSER_INIT;
    parser_state.status_internal = MIDI_PARSER_INIT;
    skip_remaining = 0;
    vtime_pending = false;
  }

  int32_t midi_event_datalen(int status) {
    switch (status) {
    case MIDI_STATUS_PGM_CHANGE:
      return 1;
    case MIDI_STATUS_CHANNEL_AT:
      return 1;
    default:
      return 2;
    }
  }

  uint16_t midi_parse_be16(const uint8_t *in) {
    return (static_cast<uint16_t>(in[0]) << 8) | in[1];
  }

  uint32_t midi_parse_be32(const uint8_t *in) {
    return (static_cast<uint32_t>(in[0]) << 24) |
           (static_cast<uint32_t>(in[1]) << 16) |
           static_cast<uint32_t>((in[2]) << 8) | in[3];
  }

  uint64_t midi_parse_N(const uint8_t *from_bytes, int len) {
    uint64_t tmp = 0;
    for (int j = 0; j < len; j++) {
      uint64_t byte = from_bytes[j];
      int shift = 8 * (len - j - 1);
      tmp |= byte << shift;
    }
    return tmp;
  }

  /// Result of parsing a variable length quantity
  enum vlq_result { VLQ_OK, VLQ_INCOMPLETE, VLQ_INVALID };

  /// Parses a variable length quantity starting at *offset. A valid value uses
  /// at most 4 bytes (max 0x0FFFFFFF). On success *offset points to the byte
  /// after the quantity. Nothing is consumed from the buffer.
  vlq_result midi_parse_variable_length(int32_t *offset, int32_t *value) {
    uint32_t result = 0;
    int32_t i = *offset;
    for (int n = 0; n < 4; n++, i++) {
      if (i >= parser_state.in.available())
        return VLQ_INCOMPLETE;
      uint8_t b = parser_state.in[i];
      result = (result << 7) | (b & 0x7f);
      if (!(b & 0x80)) {
        *offset = i + 1;
        *value = (int32_t)result;
        return VLQ_OK;
      }
    }
    return VLQ_INVALID;
  }

  /// Handles the payload of a sysex or meta event: the header (header_len
  /// bytes) and up to PEEK_MAX payload bytes are only consumed when they are
  /// all available. The payload is copied to event_data; payload bytes beyond
  /// PEEK_MAX are skipped by the following parse() calls.
  /// Returns success_status, MIDI_PARSER_EOB if more data is needed or
  /// MIDI_PARSER_ERROR for invalid data.
  midi_parser_status midi_parse_payload(int32_t header_len, int32_t length,
                                        midi_parser_status success_status,
                                        int32_t &stored) {
    // The event must fit into the remaining track
    if (length < 0 ||
        (int64_t)header_len + length > (int64_t)parser_state.track.size)
      return MIDI_PARSER_ERROR;

    int32_t to_copy = length < PEEK_MAX ? length : PEEK_MAX;
    if (parser_state.in.available() < header_len + to_copy) {
      // the data would never fit into the buffer
      if (header_len + to_copy > parser_state.in.size())
        return MIDI_PARSER_ERROR;
      return MIDI_PARSER_EOB;
    }

    stored = parser_state.in.peekInto(header_len, event_data, to_copy);
    parser_state.in.consume(header_len + to_copy);
    parser_state.track.size -= header_len + to_copy;
    skip_remaining = length - to_copy;
    return success_status;
  }

  /// Consumes the payload bytes of a truncated sysex or meta event.
  /// Returns false if more data is needed.
  bool midi_skip_remaining() {
    if (skip_remaining <= 0)
      return true;
    int32_t n = parser_state.in.available();
    if (n > skip_remaining)
      n = skip_remaining;
    parser_state.in.consume(n);
    parser_state.track.size -= n;
    skip_remaining -= n;
    return skip_remaining == 0;
  }

  enum midi_parser_status midi_parse_header() {
    if (parser_state.in.available() < 14)
      return MIDI_PARSER_EOB;

    if (!parser_state.in.equals("MThd"))
      return MIDI_PARSER_ERROR;

    parser_state.header.size = midi_parse_be32(parser_state.in.peekStr(4, 4));
    parser_state.header.format = midi_parse_be16(parser_state.in.peekStr(8, 2));
    parser_state.header.tracks_count =
        midi_parse_be16(parser_state.in.peekStr(10, 2));
    int time_div = midi_parse_be16(parser_state.in.peekStr(12, 2));
    if (time_div > 0) {
      parser_state.header.time_division = time_div;
    }

    parser_state.in.consume(14);
    parser_state.status_internal = MIDI_PARSER_HEADER;
    return MIDI_PARSER_HEADER;
  }

  enum midi_parser_status midi_parse_track() {
    if (parser_state.in.available() < 8)
      return MIDI_PARSER_EOB;

    if (!parser_state.in.equals("MTrk"))
      return MIDI_PARSER_ERROR;

    parser_state.track.size = midi_parse_be32(parser_state.in.peekStr(4, 4));
    parser_state.track.number = ++track_no;
    parser_state.status_internal = MIDI_PARSER_TRACK;
    parser_state.in.consume(8);
    // parser.in.available() -= 8;
    parser_state.buffered_status = MIDI_STATUS_NA;
    return MIDI_PARSER_TRACK;
  }

  bool midi_parse_vtime() {
    uint8_t nbytes = 0;
    uint8_t cont = 1; // continue flag

    parser_state.vtime = 0;
    while (cont) {
      ++nbytes;

      if (parser_state.in.available() < nbytes ||
          parser_state.track.size < nbytes)
        return false;

      uint8_t b = parser_state.in[nbytes - 1];
      parser_state.vtime = (parser_state.vtime << 7) | (b & 0x7f);

      // The largest value allowed within a MIDI file is 0x0FFFFFFF. A lot of
      // leading bytes with the highest bit set might overflow the nbytes
      // counter and create an endless loop. If one would use 0x80 bytes for
      // padding the check on parser.vtime would not terminate the endless
      // loop. Since the maximum value can be encoded in 5 bytes or less, we can
      // assume bad input if more bytes were used.
      if (parser_state.vtime > 0x0fffffff || nbytes > 5)
        return false;

      cont = b & 0x80;
    }

    if (parser_state.in.available() < nbytes) {
      return false;
    }

    parser_state.in.consume(nbytes);
    // parser.in.available() -= nbytes;
    parser_state.track.size -= nbytes;

    return true;
  }

  enum midi_parser_status midi_parse_channel_event() {
    if (parser_state.in.available() < 1)
      return MIDI_PARSER_EOB;

    if ((parser_state.in[0] & 0x80) == 0) {
      // Shortened event with running status.
      if (parser_state.buffered_status == 0)
        return MIDI_PARSER_ERROR;
      parser_state.midi.status = parser_state.buffered_status;
      int32_t datalen = midi_event_datalen(parser_state.midi.status);
      if (parser_state.in.available() < datalen)
        return MIDI_PARSER_EOB;
      parser_state.midi.channel = parser_state.buffered_channel;
      parser_state.midi.param1 = (datalen > 0 ? parser_state.in[0] : 0);
      parser_state.midi.param2 = (datalen > 1 ? parser_state.in[1] : 0);

      parser_state.in.consume(datalen);
      // parser.in.available() -= datalen;
      parser_state.track.size -= datalen;
    } else {
      // Full event with its own status.
      parser_state.midi.status = (parser_state.in[0] >> 4) & 0xf;
      int32_t datalen = midi_event_datalen(parser_state.midi.status);
      if (parser_state.in.available() < 1 + datalen)
        return MIDI_PARSER_EOB;
      parser_state.midi.channel = parser_state.in[0] & 0xf;
      parser_state.midi.param1 = (datalen > 0 ? parser_state.in[1] : 0);
      parser_state.midi.param2 = (datalen > 1 ? parser_state.in[2] : 0);
      parser_state.buffered_status =
          (midi_status)parser_state.midi.status; // ps?
      parser_state.buffered_channel = parser_state.midi.channel;

      parser_state.in.consume(1 + datalen);
      // parser.in.available() -= 1 + datalen;
      parser_state.track.size -= 1 + datalen;
    }

    return MIDI_PARSER_TRACK_MIDI;
  }

  enum midi_parser_status midi_parse_sysex_event() {
    assert(parser_state.in.available() == 0 || parser_state.in[0] == 0xf0);

    if (parser_state.in.available() < 2)
      return MIDI_PARSER_EOB;

    int32_t offset = 1;
    int32_t length = 0;
    switch (midi_parse_variable_length(&offset, &length)) {
    case VLQ_INCOMPLETE:
      return MIDI_PARSER_EOB;
    case VLQ_INVALID:
      return MIDI_PARSER_ERROR;
    default:
      break;
    }

    int32_t stored = 0;
    midi_parser_status rc =
        midi_parse_payload(offset, length, MIDI_PARSER_TRACK_SYSEX, stored);
    if (rc != MIDI_PARSER_TRACK_SYSEX)
      return rc;

    parser_state.sysex.bytes = event_data;
    parser_state.sysex.length = stored;
    // Don't count the 0xF7 ending byte as data, if given:
    if (skip_remaining == 0 && stored > 0 && event_data[stored - 1] == 0xF7)
      parser_state.sysex.length--;

    return MIDI_PARSER_TRACK_SYSEX;
  }

  enum midi_parser_status midi_parse_meta_event() {
    assert(parser_state.in.available() == 0 || parser_state.in[0] == 0xff);
    if (parser_state.in.available() < 3)
      return MIDI_PARSER_EOB;

    int32_t offset = 2;
    int32_t length = 0;
    switch (midi_parse_variable_length(&offset, &length)) {
    case VLQ_INCOMPLETE:
      return MIDI_PARSER_EOB;
    case VLQ_INVALID:
      return MIDI_PARSER_ERROR;
    default:
      break;
    }
    uint8_t type = parser_state.in[1];

    int32_t stored = 0;
    midi_parser_status rc =
        midi_parse_payload(offset, length, MIDI_PARSER_TRACK_META, stored);
    if (rc != MIDI_PARSER_TRACK_META)
      return rc;

    parser_state.meta.type = type;
    parser_state.meta.bytes = event_data;
    parser_state.meta.length = stored;

    // set tempo meta (3 bytes: microseconds per quarter note)
    if (parser_state.meta.type == 0x51 && stored > 0 && stored <= 4) {
      parser_state.tempo = midi_parse_N(event_data, stored);
    }

    return MIDI_PARSER_TRACK_META;
  }

  enum midi_parser_status midi_parse_event() {
    parser_state.meta.bytes = nullptr;
    parser_state.sysex.bytes = nullptr;
    // The delta time is only parsed once per event: if the event itself is
    // not complete yet, we resume after the already consumed delta time.
    if (!vtime_pending) {
      if (!midi_parse_vtime())
        return MIDI_PARSER_EOB;
      vtime_pending = true;
    }

    // Make sure the parser has not consumed the entire file or track, else
    // `parser-in[0]` might access heap-memory after the allocated buffer.
    if (parser_state.track.size <= 0)
      return MIDI_PARSER_ERROR;
    if (parser_state.in.available() <= 0)
      return MIDI_PARSER_EOB;

    midi_parser_status result = MIDI_PARSER_ERROR;
    if (parser_state.in[0] < 0xf0) { // Regular channel events:
      result = midi_parse_channel_event();
    } else {                                         // Special event types:
      parser_state.buffered_status = MIDI_STATUS_NA; // (cancels running status)

      if (parser_state.in[0] == 0xf0)
        result = midi_parse_sysex_event();
      else if (parser_state.in[0] == 0xff)
        result = midi_parse_meta_event();
    }
    if (result != MIDI_PARSER_EOB)
      vtime_pending = false;
    return result;
  }

  enum midi_parser_status midi_parse() {
    if (parser_state.in.isEmpty())
      return MIDI_PARSER_EOB;

    switch (parser_state.status_internal) {
    case MIDI_PARSER_INIT:
      return midi_parse_header();

    case MIDI_PARSER_HEADER:
      return midi_parse_track();

    case MIDI_PARSER_TRACK:
      // skip the remaining bytes of a truncated sysex or meta event
      if (!midi_skip_remaining())
        return MIDI_PARSER_EOB;
      if (parser_state.track.size == 0) {
        // we reached the end of the track
        parser_state.status_internal = MIDI_PARSER_HEADER;
        return midi_parse();
      }
      return midi_parse_event();

    default:
      return MIDI_PARSER_ERROR;
    }
  }
};

} // namespace midi