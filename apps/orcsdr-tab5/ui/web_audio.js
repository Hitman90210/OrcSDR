/* OrcSDR PCM transport/playback. Plain HTTP; no AudioWorklet dependency. */
(function (root) {
  'use strict';

  function parsePacket(bytes) {
    if (!(bytes instanceof ArrayBuffer) || bytes.byteLength < 32 || bytes.byteLength > 1952)
      throw new Error('Invalid audio packet length');
    const v = new DataView(bytes);
    if (v.getUint32(0, true) !== 0x4143524f || v.getUint16(4, true) !== 1 ||
        v.getUint16(6, true) !== 32 || v.getUint32(20, true) !== 48000 ||
        v.getUint8(26) !== 1 || v.getUint8(27) !== 1 || v.getUint32(28, true) > 1)
      throw new Error('Unsupported audio packet format');
    const frames = v.getUint16(24, true);
    const position = v.getUint32(12, true) + v.getUint32(16, true) * 4294967296;
    if (frames < 1 || frames > 960 || bytes.byteLength !== 32 + frames * 2 ||
        !Number.isSafeInteger(position + frames))
      throw new Error('Invalid audio sample range');
    const samples = new Float32Array(frames);
    for (let i = 0; i < frames; i++) samples[i] = v.getInt16(32 + i * 2, true) / 32768;
    return { generation: v.getUint32(8, true), position, rate: 48000,
      discontinuity: v.getUint32(28, true) === 1, samples };
  }

  const api = { parsePacket };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else root.OrcAudio = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
