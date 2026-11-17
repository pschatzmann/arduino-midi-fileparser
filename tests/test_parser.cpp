/**
 * Desktop unit tests for the midi file parser.
 *
 * Build & run:
 *   cmake -B build -DMIDI_BUILD_TESTS=ON && cmake --build build && ctest --test-dir build --output-on-failure
 */
#include "MidiFileParser.h"
#include "examples/example-midi.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace midi;
using Bytes = std::vector<uint8_t>;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    g_checks++;                                                                \
    if (!(cond)) {                                                             \
      g_failures++;                                                            \
      printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);               \
    }                                                                          \
  } while (0)

#define CHECK_EQ(a, b)                                                         \
  do {                                                                         \
    g_checks++;                                                                \
    auto _va = (a);                                                            \
    auto _vb = (b);                                                            \
    if (!(_va == _vb)) {                                                       \
      g_failures++;                                                            \
      printf("  FAILED %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);        \
    }                                                                          \
  } while (0)

// ---------------------------------------------------------------------------
// Helpers to build midi files
// ---------------------------------------------------------------------------

static void append(Bytes &out, const Bytes &in) {
  out.insert(out.end(), in.begin(), in.end());
}

static Bytes vlq(uint32_t value) {
  Bytes tmp;
  tmp.push_back(value & 0x7f);
  while (value >>= 7) {
    tmp.push_back((value & 0x7f) | 0x80);
  }
  return Bytes(tmp.rbegin(), tmp.rend());
}

static Bytes header(uint16_t tracks = 1, uint16_t division = 96) {
  return {'M',  'T', 'h', 'd', 0, 0, 0, 6, 0, (uint8_t)(tracks > 1 ? 1 : 0),
          0,    (uint8_t)tracks, (uint8_t)(division >> 8),
          (uint8_t)(division & 0xff)};
}

static Bytes track(const Bytes &events) {
  uint32_t n = events.size();
  Bytes out = {'M',
               'T',
               'r',
               'k',
               (uint8_t)(n >> 24),
               (uint8_t)(n >> 16),
               (uint8_t)(n >> 8),
               (uint8_t)n};
  append(out, events);
  return out;
}

static Bytes sysexEvent(uint32_t delta, const Bytes &payload) {
  Bytes out = vlq(delta);
  out.push_back(0xF0);
  append(out, vlq(payload.size()));
  append(out, payload);
  return out;
}

static Bytes metaEvent(uint32_t delta, uint8_t type, const Bytes &payload) {
  Bytes out = vlq(delta);
  out.push_back(0xFF);
  out.push_back(type);
  append(out, vlq(payload.size()));
  append(out, payload);
  return out;
}

static Bytes endOfTrack() { return metaEvent(0, 0x2F, {}); }

static Bytes file(const Bytes &events) {
  Bytes out = header();
  append(out, track(events));
  return out;
}

// ---------------------------------------------------------------------------
// Parsing driver: records every event as a string
// ---------------------------------------------------------------------------

struct Result {
  std::vector<std::string> events;
  std::vector<Bytes> payloads; // sysex / meta payloads
  bool error = false;
  uint64_t tempo = 0;
};

static void collect(MidiFileParser &parser, Result &result) {
  while (true) {
    midi_parser_state &st = parser.parse();
    char buf[200];
    switch (st.status) {
    case MIDI_PARSER_EOB:
      return;
    case MIDI_PARSER_ERROR:
      result.error = true;
      return;
    case MIDI_PARSER_HEADER:
      snprintf(buf, sizeof(buf), "header fmt=%d tracks=%d div=%d",
               st.header.format, st.header.tracks_count,
               st.header.time_division);
      break;
    case MIDI_PARSER_TRACK:
      snprintf(buf, sizeof(buf), "track %d", st.track.number);
      break;
    case MIDI_PARSER_TRACK_MIDI:
      snprintf(buf, sizeof(buf), "midi t=%ld s=%d c=%d p1=%d p2=%d",
               (long)st.vtime, st.midi.status, st.midi.channel, st.midi.param1,
               st.midi.param2);
      break;
    case MIDI_PARSER_TRACK_META:
      snprintf(buf, sizeof(buf), "meta t=%ld type=%d len=%d", (long)st.vtime,
               st.meta.type, (int)st.meta.length);
      result.payloads.push_back(
          Bytes(st.meta.bytes, st.meta.bytes + st.meta.length));
      break;
    case MIDI_PARSER_TRACK_SYSEX:
      snprintf(buf, sizeof(buf), "sysex t=%ld len=%d", (long)st.vtime,
               (int)st.sysex.length);
      result.payloads.push_back(
          Bytes(st.sysex.bytes, st.sysex.bytes + st.sysex.length));
      break;
    default:
      snprintf(buf, sizeof(buf), "status %d", st.status);
      break;
    }
    result.events.push_back(buf);
    result.tempo = st.tempo;
  }
}

/// Feeds the data in chunks of chunkSize bytes (0 = whole buffer)
static Result parseData(const Bytes &data, size_t chunkSize = 0,
                        int bufferSize = MIDI_BUFFER_SIZE) {
  MidiFileParser parser;
  parser.begin(false, bufferSize);
  Result result;
  // 0 = as much as fits into the buffer
  if (chunkSize == 0)
    chunkSize = std::min(data.size(), (size_t)bufferSize);
  size_t pos = 0;
  while (pos < data.size() && !result.error) {
    size_t len = std::min(chunkSize, data.size() - pos);
    size_t written = parser.write(data.data() + pos, len);
    if (written == 0) {
      // buffer full and parser cannot make progress
      result.error = true;
      break;
    }
    pos += written;
    collect(parser, result);
  }
  return result;
}

static void dump(const Result &r) {
  for (auto &e : r.events)
    printf("    %s\n", e.c_str());
  printf("    error=%d\n", r.error);
}

/// Parses the data with all chunk sizes from 1 to 17 and compares the result
/// with parsing everything at once.
static void checkChunkIndependent(const Bytes &data, int bufferSize = MIDI_BUFFER_SIZE) {
  // reference: everything at once into a buffer that is big enough
  Result ref = parseData(data, 0, MIDI_BUFFER_SIZE);
  for (size_t chunk = 1; chunk <= 17; chunk++) {
    Result r = parseData(data, chunk, bufferSize);
    bool same = r.events == ref.events && r.payloads == ref.payloads &&
                r.error == ref.error;
    g_checks++;
    if (!same) {
      g_failures++;
      printf("  FAILED: chunk size %zu differs from full parse\n", (size_t)chunk);
      printf("  expected:\n");
      dump(ref);
      printf("  actual:\n");
      dump(r);
      return;
    }
  }
}

static Bytes range(size_t n, uint8_t start = 1) {
  Bytes out(n);
  for (size_t j = 0; j < n; j++)
    out[j] = (uint8_t)((start + j) & 0x7f);
  return out;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

static void test_ringbuffer() {
  RingBuffer rb(8);
  uint8_t data[] = {1, 2, 3, 4, 5, 6};
  CHECK_EQ(rb.write(data, 6), 6);
  rb.consume(4);
  CHECK_EQ(rb.write(data, 6), 6); // wraps around
  CHECK_EQ(rb.available(), 8);
  CHECK(rb.isFull());
  CHECK_EQ(rb.write(data, 1), 0);
  CHECK_EQ(rb[0], 5);
  CHECK_EQ(rb[7], 6);
  CHECK_EQ(rb.peek(8), -1); // out of range
  CHECK_EQ(rb.peek(-1), -1);
  // peekStr up to the exact end of the data
  uint8_t *p = rb.peekStr(2, 6);
  CHECK(p != nullptr);
  CHECK_EQ(p[0], 1);
  CHECK_EQ(p[5], 6);
  uint8_t dest[10];
  CHECK_EQ(rb.peekInto(6, dest, 10), 2); // limited by available data
  CHECK(!rb.equals("too long for the buffer"));
}

static void test_header() {
  Result r = parseData(file(endOfTrack()));
  CHECK(!r.error);
  CHECK(r.events.size() >= 2);
  CHECK_EQ(r.events[0], std::string("header fmt=0 tracks=1 div=96"));
  CHECK_EQ(r.events[1], std::string("track 1"));
}

static Bytes basicEvents() {
  Bytes ev;
  append(ev, {0x00, 0x90, 60, 100});  // note on
  append(ev, {0x60, 0x80, 60, 0});    // note off
  append(ev, {0x00, 0xC1, 5});        // program change (1 data byte)
  append(ev, {0x10, 0x91, 64, 90});   // note on ch 1
  append(ev, {0x10, 65, 91});         // running status
  append(ev, {0x81, 0x00, 0x81, 65, 0}); // delta 128 (2 byte vtime), note off
  append(ev, endOfTrack());
  return ev;
}

static void test_channel_events() {
  Result r = parseData(file(basicEvents()));
  CHECK(!r.error);
  std::vector<std::string> expected = {
      "header fmt=0 tracks=1 div=96",
      "track 1",
      "midi t=0 s=9 c=0 p1=60 p2=100",
      "midi t=96 s=8 c=0 p1=60 p2=0",
      "midi t=0 s=12 c=1 p1=5 p2=0",
      "midi t=16 s=9 c=1 p1=64 p2=90",
      "midi t=16 s=9 c=1 p1=65 p2=91",
      "midi t=128 s=8 c=1 p1=65 p2=0",
      "meta t=0 type=47 len=0",
  };
  CHECK(r.events == expected);
  if (r.events != expected)
    dump(r);
  checkChunkIndependent(file(basicEvents()));
}

static void test_short_sysex() {
  Bytes payload = {0x7E, 0x7F, 0x09, 0x01, 0xF7};
  Bytes ev = sysexEvent(5, payload);
  append(ev, endOfTrack());
  Result r = parseData(file(ev));
  CHECK(!r.error);
  CHECK_EQ(r.events[2], std::string("sysex t=5 len=4")); // 0xF7 not counted
  CHECK(r.payloads[0] == Bytes({0x7E, 0x7F, 0x09, 0x01}));
  checkChunkIndependent(file(ev));
}

static void test_long_sysex_is_truncated() {
  // longer than PEEK_MAX: previously asserted / overflowed the static buffer
  Bytes payload = range(200);
  payload.push_back(0xF7);
  Bytes ev = sysexEvent(0, payload);
  append(ev, {0x00, 0x90, 60, 100});
  append(ev, endOfTrack());
  Result r = parseData(file(ev));
  CHECK(!r.error);
  CHECK_EQ(r.events[2], std::string("sysex t=0 len=" + std::to_string(PEEK_MAX)));
  CHECK(r.payloads[0] == range(PEEK_MAX));
  // the events after the sysex are parsed correctly
  CHECK_EQ(r.events[3], std::string("midi t=0 s=9 c=0 p1=60 p2=100"));
  checkChunkIndependent(file(ev));
}

static void test_sysex_larger_than_ringbuffer() {
  Bytes payload = range(600);
  Bytes ev = sysexEvent(0, payload);
  append(ev, {0x00, 0x90, 61, 101});
  append(ev, endOfTrack());
  Result r = parseData(file(ev), 16, 128);
  CHECK(!r.error);
  CHECK_EQ(r.events.size(), (size_t)5);
  CHECK_EQ(r.events[3], std::string("midi t=0 s=9 c=0 p1=61 p2=101"));
  checkChunkIndependent(file(ev), 128);
}

static void test_sysex_exactly_fills_buffer() {
  // sysex is the last event of the data: payload ends exactly at the end of
  // the buffer (previously tripped the peekStr assert)
  Bytes ev = sysexEvent(0, {0x01, 0x02, 0x03});
  Result r = parseData(file(ev));
  CHECK(!r.error);
  CHECK_EQ(r.events.back(), std::string("sysex t=0 len=3"));
  checkChunkIndependent(file(ev));
}

static void test_meta_events() {
  Bytes text = range(120, 'A');
  Bytes ev = metaEvent(0, 0x03, text);                // long track name
  append(ev, metaEvent(0, 0x51, {0x07, 0xA1, 0x20})); // tempo 500000
  append(ev, {0x00, 0x90, 60, 100});
  append(ev, endOfTrack());
  Result r = parseData(file(ev));
  CHECK(!r.error);
  CHECK_EQ(r.events[2], std::string("meta t=0 type=3 len=" + std::to_string(PEEK_MAX)));
  CHECK(r.payloads[0] == range(PEEK_MAX, 'A'));
  CHECK_EQ(r.events[3], std::string("meta t=0 type=81 len=3"));
  CHECK_EQ(r.tempo, (uint64_t)500000);
  CHECK_EQ(r.events[4], std::string("midi t=0 s=9 c=0 p1=60 p2=100"));
  checkChunkIndependent(file(ev));
}

static void test_invalid_vlq_length() {
  // sysex length with 5 continuation bytes is invalid
  Bytes ev = {0x00, 0xF0, 0x81, 0x81, 0x81, 0x81, 0x01, 0x00};
  append(ev, endOfTrack());
  Result r = parseData(file(ev));
  CHECK(r.error);
  checkChunkIndependent(file(ev));
}

static void test_sysex_longer_than_track() {
  Bytes ev = {0x00, 0xF0, 0x40, 0x01, 0x02}; // claims 64 bytes
  Result r = parseData(file(ev));
  CHECK(r.error);
}

static void test_running_status_without_status() {
  Bytes ev = {0x00, 0x40, 0x40};
  append(ev, endOfTrack());
  Result r = parseData(file(ev));
  CHECK(r.error);
}

static void test_multiple_tracks() {
  Bytes data = header(2);
  Bytes t1 = {0x00, 0x90, 60, 100};
  append(t1, endOfTrack());
  Bytes t2 = sysexEvent(0, range(100));
  append(t2, {0x00, 0x91, 62, 100});
  append(t2, endOfTrack());
  append(data, track(t1));
  append(data, track(t2));
  Result r = parseData(data);
  CHECK(!r.error);
  std::vector<std::string> expected = {
      "header fmt=1 tracks=2 div=96",
      "track 1",
      "midi t=0 s=9 c=0 p1=60 p2=100",
      "meta t=0 type=47 len=0",
      "track 2",
      "sysex t=0 len=" + std::to_string(PEEK_MAX),
      "midi t=0 s=9 c=1 p1=62 p2=100",
      "meta t=0 type=47 len=0",
  };
  CHECK(r.events == expected);
  if (r.events != expected)
    dump(r);
  checkChunkIndependent(data);
}

static void test_example_file() {
  Bytes data(example_mid, example_mid + example_mid_len);
  Result r = parseData(data);
  CHECK(!r.error);
  size_t notes = 0;
  for (auto &e : r.events)
    if (e.rfind("midi", 0) == 0)
      notes++;
  CHECK(notes > 100);
  checkChunkIndependent(data);
  // the example sketch uses a small buffer with 256 byte writes
  Result small = parseData(data, 256, 256 * 5);
  CHECK(small.events == r.events);
}

int main() {
  struct {
    const char *name;
    void (*fn)();
  } tests[] = {
      {"ringbuffer", test_ringbuffer},
      {"header", test_header},
      {"channel_events", test_channel_events},
      {"short_sysex", test_short_sysex},
      {"long_sysex_is_truncated", test_long_sysex_is_truncated},
      {"sysex_larger_than_ringbuffer", test_sysex_larger_than_ringbuffer},
      {"sysex_exactly_fills_buffer", test_sysex_exactly_fills_buffer},
      {"meta_events", test_meta_events},
      {"invalid_vlq_length", test_invalid_vlq_length},
      {"sysex_longer_than_track", test_sysex_longer_than_track},
      {"running_status_without_status", test_running_status_without_status},
      {"multiple_tracks", test_multiple_tracks},
      {"example_file", test_example_file},
  };
  for (auto &t : tests) {
    int before = g_failures;
    t.fn();
    printf("%s %s\n", g_failures == before ? "[ OK ]" : "[FAIL]", t.name);
  }
  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
