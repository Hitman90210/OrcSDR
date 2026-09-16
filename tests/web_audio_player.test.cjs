const assert = require('node:assert/strict');
const { parsePacket } = require('../apps/orcsdr-tab5/ui/web_audio.js');

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
