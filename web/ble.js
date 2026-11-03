/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Web Bluetooth client for the KalliFunghi GATT service. Byte formats: firmware
// lib/kfcore/src/BleProto.h and firmware/README.md "Bluetooth sync".
'use strict';

const KF = {
  service: '4b460000-6b61-6c6c-6966-756e67686900',
  status: '4b460001-6b61-6c6c-6966-756e67686900',
  time: '4b460002-6b61-6c6c-6966-756e67686900',
  live: '4b460003-6b61-6c6c-6966-756e67686900',
  files: '4b460004-6b61-6c6c-6966-756e67686900',
  control: '4b460005-6b61-6c6c-6966-756e67686900',
};
const OP = { list: 0x01, read: 0x02, ack: 0x03, abort: 0x04,
  listLines: 0x81, listEnd: 0x82, data: 0x83, readEnd: 0x84, error: 0x8f };
const CTL = { keepalive: 0x01, fast: 0x02, end: 0x03 };
const STATUS_TEXT = ['ok', 'bad request', 'bad path', 'not found', 'busy', 'SD error', 'aborted'];
const ACK_EVERY = 4096;          // device window is 8 KB
const REQUEST_TIMEOUT_MS = 20000;

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// CRC-32 (IEEE, as zlib), incremental: crc32(bytes, crc32(prev)).
const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = (c & 1) ? (0xedb88320 ^ (c >>> 1)) : (c >>> 1);
    t[n] = c >>> 0;
  }
  return t;
})();
function crc32(bytes, crc = 0) {
  crc = ~crc >>> 0;
  for (let i = 0; i < bytes.length; i++) crc = CRC_TABLE[(crc ^ bytes[i]) & 0xff] ^ (crc >>> 8);
  return ~crc >>> 0;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
class KfDevice extends EventTarget {
  constructor() {
    super();
    this.device = null;
    this.chars = {};
    this.pending = null;      // the one running Files request
    this.queue = Promise.resolve();
    this.ops = Promise.resolve();
  }

  /* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
  // One GATT operation at a time: Chrome rejects an operation that overlaps another
  // ("GATT operation already in progress"), e.g. the status poll during a READ write.
  op(fn) {
    const run = this.ops.then(fn, fn);
    this.ops = run.catch(() => {});
    return run;
  }

  static supported() { return !!(navigator.bluetooth && navigator.bluetooth.requestDevice); }
  get connected() { return !!(this.device && this.device.gatt && this.device.gatt.connected); }

  async connect() {
    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [KF.service] }],
    });
    this.device.addEventListener('gattserverdisconnected', () => {
      if (this.pending) this.pending.fail(new Error('disconnected'));
      this.dispatchEvent(new Event('disconnected'));
    });
    const server = await this.device.gatt.connect();
    const svc = await server.getPrimaryService(KF.service);
    for (const k of ['status', 'time', 'live', 'files', 'control']) {
      this.chars[k] = await svc.getCharacteristic(KF[k]);
    }
    this.chars.files.addEventListener('characteristicvaluechanged', (e) => this.onFiles(e.target.value));
    this.chars.live.addEventListener('characteristicvaluechanged', (e) => this.onLive(e.target.value));
    await this.chars.files.startNotifications();
    await this.chars.live.startNotifications();
    this.dispatchEvent(new Event('connected'));
  }

  async disconnect(endSession = true) {
    if (!this.connected) return;   // also covers device === null
    if (endSession) {
      try { await this.control(CTL.end); } catch (_) { /* link may be gone already */ }
    }
    this.device.gatt.disconnect();
  }

  // Status JSON plus the clock difference device - this browser (ms), measured
  // against the middle of the read round trip.
  async readStatus() {
    const t0 = Date.now();
    const v = await this.op(() => this.chars.status.readValue());
    const t1 = Date.now();
    const s = JSON.parse(new TextDecoder().decode(v));
    s.offset_ms = s.utc_ms - (t0 + t1) / 2;
    s.rtt_ms = t1 - t0;
    return s;
  }

  async syncTime() {
    const b = new DataView(new ArrayBuffer(8));
    b.setBigInt64(0, BigInt(Date.now()), true);
    await this.op(() => this.chars.time.writeValueWithResponse(b.buffer));
  }

  async control(op, arg) {
    const p = arg === undefined ? new Uint8Array([op]) : new Uint8Array([op, arg ? 1 : 0]);
    await this.op(() => this.chars.control.writeValueWithoutResponse(p));
  }

  onLive(v) {
    if (v.byteLength < 16) return;
    const flags = v.getUint8(15);
    const scd = (flags & 2) !== 0;
    const lux = (flags & 4) !== 0;
    this.dispatchEvent(new CustomEvent('sample', { detail: {
      t: v.getUint32(0, true),
      co2: scd ? v.getUint16(4, true) : null,
      temp: scd ? v.getInt16(6, true) / 100 : null,
      rh: scd ? v.getUint16(8, true) / 100 : null,
      lux: lux ? v.getFloat32(10, true) : null,
      mode: ['ultra', 'eco', 'fast'][v.getUint8(14)] || 'eco',
      unsynced: (flags & 1) !== 0,
    } }));
  }

  // Files requests run one at a time.
  serial(fn) {
    const run = this.queue.then(fn, fn);
    this.queue = run.catch(() => {});
    return run;
  }

  startRequest(bytes, handler) {
    return new Promise((resolve, reject) => {
      let timer = null;
      const req = {
        handler,
        done: (v) => { clearTimeout(timer); this.pending = null; resolve(v); },
        fail: (e) => { clearTimeout(timer); this.pending = null; reject(e); },
        poke: () => {
          clearTimeout(timer);
          timer = setTimeout(() => req.fail(new Error('timeout')), REQUEST_TIMEOUT_MS);
        },
      };
      this.pending = req;
      req.poke();
      this.op(() => this.chars.files.writeValueWithResponse(bytes)).catch(req.fail);
    });
  }

  onFiles(v) {
    const req = this.pending;
    if (!req) return;
    req.poke();
    const op = v.getUint8(0);
    if (op === OP.error) {
      req.fail(new Error('device: ' + (STATUS_TEXT[v.getUint8(1)] || 'error')));
      return;
    }
    try { req.handler(op, v); } catch (e) { req.fail(e); }
  }

  // [{path, size}] from the device; checked against the count and CRC of the end packet.
  list() {
    return this.serial(() => {
      const chunks = [];
      return this.startRequest(new Uint8Array([OP.list]), (op, v) => {
        if (op === OP.listLines) {
          chunks.push(new Uint8Array(v.buffer, v.byteOffset + 3, v.byteLength - 3).slice());
        } else if (op === OP.listEnd) {
          const all = concat(chunks);
          const count = v.getUint16(1, true);
          const crc = v.getUint32(3, true);
          if (crc32(all) !== crc) throw new Error('list CRC mismatch');
          const files = new TextDecoder().decode(all).split('\n').filter(Boolean).map((l) => {
            const i = l.lastIndexOf(',');
            return { path: l.slice(0, i), size: Number(l.slice(i + 1)) };
          });
          if (files.length !== count) throw new Error('list count mismatch');
          this.pending.done(files);
        }
      });
    });
  }

  // Bytes [offset, end) of a file. onProgress(received, total). Resolves
  // {bytes, size}; rejects on a gap, a CRC mismatch, an error or a disconnect, with
  // error.partial = the bytes received in order (not CRC checked; the caller resumes
  // after them). On a CRC mismatch partial is cleared: nothing of that range is kept.
  read(path, offset, onProgress) {
    return this.serial(() => {
      const enc = new TextEncoder().encode(path);
      const req = new Uint8Array(5 + enc.length);
      req[0] = OP.read;
      new DataView(req.buffer).setUint32(1, offset, true);
      req.set(enc, 5);
      const chunks = [];
      let next = offset;
      let acked = offset;
      let crc = 0;
      return this.startRequest(req, (op, v) => {
        if (op === OP.data) {
          const at = v.getUint32(1, true);
          if (at !== next) {
            this.abortRead();
            throw new Error(`gap at ${next} (got ${at})`);
          }
          const d = new Uint8Array(v.buffer, v.byteOffset + 5, v.byteLength - 5).slice();
          chunks.push(d);
          crc = crc32(d, crc);
          next += d.length;
          if (next - acked >= ACK_EVERY) {
            acked = next;
            this.ack(next);
          }
          if (onProgress) onProgress(next, null);
        } else if (op === OP.readEnd) {
          const st = v.getUint8(1);
          const size = v.getUint32(2, true);
          const devCrc = v.getUint32(6, true);
          if (st !== 0) throw new Error('device: ' + (STATUS_TEXT[st] || 'error'));
          // offset beyond the end (file shrank, new card): empty, the caller restarts at 0.
          if (offset <= size && next !== size) throw new Error(`short read ${next} of ${size}`);
          if (devCrc !== crc) {
            chunks.length = 0;
            throw new Error('CRC mismatch');
          }
          if (onProgress) onProgress(next, size);
          this.pending.done({ bytes: concat(chunks), size });
        }
      }).catch((e) => {
        // What arrived in order so far: the caller keeps it and resumes after it.
        e.partial = concat(chunks);
        throw e;
      });
    });
  }

  ack(offset) {
    const p = new Uint8Array(5);
    p[0] = OP.ack;
    new DataView(p.buffer).setUint32(1, offset, true);
    this.op(() => this.chars.files.writeValueWithoutResponse(p)).catch(() => {});
  }

  abortRead() {
    this.op(() => this.chars.files.writeValueWithoutResponse(new Uint8Array([OP.abort]))).catch(() => {});
  }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
function concat(parts) {
  const n = parts.reduce((a, p) => a + p.length, 0);
  const out = new Uint8Array(n);
  let o = 0;
  for (const p of parts) { out.set(p, o); o += p.length; }
  return out;
}
