const assert = require('node:assert/strict');
const { parsePacket, Player } = require('../apps/orcsdr-tab5/ui/web_audio.js');

function packet() {
  // Independent fixture: matches documented wire bytes, not encoder helpers.
  return Uint8Array.from([
    79, 82, 67, 65, 1, 0, 32, 0, 0x78, 0x56, 0x34, 0x12,
    8, 7, 6, 5, 0, 0, 0, 0, 0x80, 0xbb, 0, 0, 4, 0, 1, 1,
    1, 0, 0, 0, 0, 0x80, 0xff, 0xff, 0, 0, 0xff, 0x7f
  ]).buffer;
}
const parsed = parsePacket(packet());
assert.equal(parsed.generation, 0x12345678);
assert.equal(parsed.position, 0x05060708);
assert.equal(parsed.rate, 48000);
assert.equal(parsed.discontinuity, true);
assert.deepEqual(Array.from(parsed.samples), [-1, -1 / 32768, 0, 32767 / 32768]);
assert.throws(() => parsePacket(null));
assert.throws(() => parsePacket(new ArrayBuffer(31)));
assert.throws(() => parsePacket(new ArrayBuffer(1953)));
for (const [offset, value] of [[0, 0], [4, 2], [6, 31], [20, 0],
    [24, 0], [24, 5], [26, 2], [27, 2], [28, 2], [19, 255]]) {
  const bad = packet();
  new Uint8Array(bad)[offset] = value;
  assert.throws(() => parsePacket(bad), `invalid field at ${offset}`);
}
assert.throws(() => parsePacket(packet().slice(0, 39)));
console.log('WEB_AUDIO_PARSER_OK');

// Audio-device boundary only: the real Player owns queue/timing decisions.
function audioContext() {
  return {
    currentTime: 0, state: 'running', destination: {}, created: [],
    createGain() { return { connect() {}, gain: { value: 1,
      cancelScheduledValues() {}, setValueAtTime() {}, linearRampToValueAtTime() {} } }; },
    createBuffer(channels, frames, rate) {
      assert.equal(channels, 1); assert.equal(rate, 48000);
      const samples = new Float32Array(frames);
      return { getChannelData() { return samples; } };
    },
    createBufferSource() {
      const node = { playbackRate: { value: 1 }, connect() {}, disconnect() {},
        start(at) { this.at = at; }, stop() { this.stopped = true; } };
      this.created.push(node); return node;
    }
  };
}
function frame(position, generation = 1, flags = 0, frames = 960) {
  const bytes = new ArrayBuffer(32 + frames * 2), v = new DataView(bytes);
  v.setUint32(0, 0x4143524f, true); v.setUint16(4, 1, true);
  v.setUint16(6, 32, true); v.setUint32(8, generation, true);
  v.setUint32(12, position, true); v.setUint32(20, 48000, true);
  v.setUint16(24, frames, true); v.setUint8(26, 1); v.setUint8(27, 1);
  v.setUint32(28, flags, true); return bytes;
}
const ctx = audioContext(), player = new Player(ctx);
for (let i = 0; i < 14; i++) player.push(frame(i * 960));
assert.equal(ctx.created.length, 0, 'must prebuffer before playback');
player.push(frame(14 * 960));
assert.equal(ctx.created.length, 15);
for (let i = 1; i < 15; i++) {
  const prev = ctx.created[i - 1], current = ctx.created[i];
  assert.ok(Math.abs(current.at - prev.at - 0.02 / prev.playbackRate.value) < 1e-9,
    'adjacent packets must share one audio timeline');
}
player.push(frame(14 * 960));
assert.equal(ctx.created.length, 15, 'duplicates must not replay');
ctx.currentTime = 0.025; player.tick();
assert.equal(player.state, 'live');
assert.ok(player.bufferSeconds <= 0.6);
const previousNodes = ctx.created.slice();
player.push(frame(0, 2));
assert.equal(player.state, 'buffering');
assert.ok(previousNodes.every(n => n.stopped), 'retune must cancel old station');
player.push(frame(15000, 1));
assert.equal(player.stats.stale, 2, 'duplicate and late old-generation packet ignored');
assert.equal(player.bufferSeconds, 0.02);

// Flood without advancing device time: queue including scheduled audio is capped.
for (let i = 1; i < 100; i++) {
  player.push(frame(i * 960, 2));
  assert.ok(player.bufferSeconds <= 0.600001);
  assert.ok(player.nodeCount <= 64);
}
assert.ok(player.stats.overflows > 0);
const starveClock = audioContext(), starving = new Player(starveClock);
for (let i = 0; i < 15; i++) starving.push(frame(i * 960));
starveClock.currentTime = 2; starving.tick();
const underruns = starving.stats.underruns;
assert.ok(underruns > 0);
starving.tick(); assert.equal(starving.stats.underruns, underruns, 'count starvation once');
player.stop();
assert.equal(player.state, 'off');
assert.equal(player.bufferSeconds, 0);
const nodesAfterStop = ctx.created.length;
player.push(frame(0, 3));
assert.equal(ctx.created.length, nodesAfterStop, 'late callback after OFF must be harmless');

// Tiny valid packets cannot create an unbounded list of queued objects.
const tiny = new Player(audioContext());
for (let i = 0; i < 1000; i++) tiny.push(frame(i, 1, 0, 1));
assert.ok(tiny.nodeCount <= 64 && tiny.stats.overflows > 0);

const suspendedClock = audioContext(), suspended = new Player(suspendedClock);
suspendedClock.state = 'suspended'; suspended.push(frame(0));
assert.equal(suspended.state, 'suspended');
assert.equal(suspended.nodeCount, 0);
suspendedClock.state = 'running'; suspended.push(frame(960));
assert.equal(suspended.state, 'buffering');
assert.equal(suspended.stats.underruns, 0);

// Ten minutes, +/- 1000 ppm source/device mismatch, plus a delayed burst.
for (const drift of [0.999, 1.001]) {
  const clock = audioContext(), p = new Player(clock);
  for (let i = 0; i < 30000; i++) {
    clock.currentTime = Math.max(clock.currentTime,
      i * 0.02 * drift + (i >= 1000 && i < 1005 ? 0.08 : 0));
    p.push(frame(i * 960));
    assert.ok(p.bufferSeconds <= 0.600001);
    assert.ok(p.nodeCount <= 64);
  }
  assert.equal(p.stats.overflows, 0);
  assert.equal(p.stats.underruns, 0);
  assert.ok(clock.created.every(n => n.playbackRate.value >= 0.995 && n.playbackRate.value <= 1.005));
}
console.log('WEB_AUDIO_TIMELINE_OK');
