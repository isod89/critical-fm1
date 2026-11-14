/* SPDX-License-Identifier: GPL-3.0-only
 * CRITICAL's installer page run in node against a fake m-bootloader in MIDI MODE ("FM-1 Update"): who it is (0x12),
 * then the package read with 0x30 requests the way m-bootloader reads it (its header, the entry, flash.bin), the
 * checks it makes, and the area it would write, decrypted with the chip key and compared with build/fw/critical.area.
 *   node tests/install_sim.js PAGE.html AREA */
const fs = require('fs');
const [page, areaFile] = process.argv.slice(2);
const html = fs.readFileSync(page, 'utf8');
const js = html.slice(html.indexOf('<script>') + 8, html.lastIndexOf('</script>'));
const AREA = fs.readFileSync(areaFile);
let fails = 0;
const check = (c, what) => { if (!c) { console.log('FAIL ' + what); fails++; } };
const els = {}, logs = [];
const document = { documentElement: {}, getElementById: id => els[id] || (els[id] = { style: {}, disabled: false, set innerHTML(v) {}, set textContent(v) {}, appendChild: d => logs.push(d.textContent) }),
  createElement: () => ({ scrollIntoView() {} }), body: { appendChild() {} } };
const outputs = new Map(), inputs = new Map();
function plug(name, dev) {
  const inp = { name, state: 'connected', onmidimessage: null };
  outputs.set(name, { name, state: 'connected', send: d => setImmediate(() => dev.rx(d)) }); inputs.set(name, inp);
  dev.toPage = d => { if (inp.onmidimessage) inp.onmidimessage({ data: Uint8Array.from(d) }); };
}
const navigator = { language: 'en', requestMIDIAccess: async () => ({ outputs, inputs }) };
const atob = s => Buffer.from(s, 'base64').toString('binary');
new Function('document', 'navigator', 'atob', js)(document, navigator, atob);

function crc16(d, c = 0) { for (const b of d) { c ^= b << 8; for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; } return c; }
function enc(b, o, n, k) { k &= 0xFFFF; for (let i = 0; i < n; i++) { b[o + i] ^= k & 255; k = k & 0x8000 ? ((k << 1) ^ 0x1021) & 0xFFFF : (k << 1) & 0xFFFF; } }
const rd16 = (a, i) => a[i] | a[i + 1] << 8, rd32 = (a, i) => (a[i] | a[i + 1] << 8 | a[i + 2] << 16 | a[i + 3] << 24) >>> 0;
function frame(cmd, body) {
  const raw = [0, 0x59, cmd, body.length & 255, body.length >> 8 & 255, body.length >> 16, ...body];
  let s = 0; for (const b of body) s += b; raw.push(~s & 255);
  const out = [0xF0]; let acc = 0, bits = 0;
  for (const b of raw) { acc |= b << bits; bits += 8; while (bits >= 7) { out.push(acc & 127); acc >>= 7; bits -= 7; } }
  if (bits) out.push(acc & 127); out.push(0xF7); return out;
}
function unpack(d) { const o = []; let acc = 0, bits = 0; for (const b of d) { acc |= (b & 127) << bits; bits += 7; if (bits >= 8) { o.push(acc & 255); acc >>= 8; bits -= 8; } } return o; }

const KEY = 0x980F;
const mb = { pending: null, got: new Map() };
function read(addr, len) {                                 /* one 0x30 request, its answer back as a promise */
  return new Promise(res => {
    mb.pending = res;
    const b = [0, addr & 255, addr >> 8 & 255, addr >> 16 & 255, addr >>> 24, len & 255, len >> 8 & 255, len >> 16 & 255];
    mb.toPage(frame(0x30, b));
  });
}
mb.rx = d => {
  if (d.length === 6 && d[4] === 0x7F) { install(); return; }
  const m = unpack(d.slice(1, d.length - 1));
  if (m[2] === 0x12) {
    const body = new Array(48).fill(0);
    'MBOOT'.split('').forEach((c, i) => body[i] = c.charCodeAt(0));
    body[5] = 1; body[6] = 1; body[7] = 1; body[8] = KEY & 255; body[9] = KEY >> 8;
    body[10] = 0; body[11] = 0; body[12] = 0x10; body[13] = 0;
    'v1.2'.split('').forEach((c, i) => body[28 + i] = c.charCodeAt(0));
    mb.toPage(frame(0x12, body));
  } else if (m[2] === 0x30 && mb.pending) { const r = mb.pending; mb.pending = null; r(m.slice(14, m.length - 1)); }
};
async function install() {
  const h = Uint8Array.from(await read(0, 64)); enc(h, 0, 64, 0xFFFF);
  check(crc16(h.subarray(2, 64)) === rd16(h, 0), 'package header CRC');
  const n = rd16(h, 8); check(n === 1, 'one entry (no SLOOP loader)');
  const lst = Uint8Array.from(await read(0x40, 80 * n));
  check(crc16(lst) === rd16(h, 2), 'entry list CRC');
  const e = lst.slice(0, 80); enc(e, 0, 80, 0xFFFF);
  const at = rd32(e, 8), size = rd32(e, 12);
  check(String.fromCharCode(...e.slice(64, 73)) === 'flash.bin', 'entry: flash.bin');
  const fb = new Uint8Array(size);
  for (let o = 0; o < size; o += 512) fb.set((await read(at + o, Math.min(512, size - o))).slice(0, Math.min(512, size - o)), o);
  check(crc16(fb) === rd16(e, 4), 'flash.bin CRC');
  check(size <= 0x77000, 'flash.bin ends before 0x77000 (the career) and SLOOP 3\'s data');
  /* the area as m-bootloader writes it: decrypted line by line with the chip key */
  const a = fb.slice(0x4000, 0x4000 + AREA.length);
  for (let q = 0; q < a.length; q += 32) enc(a, q, Math.min(32, a.length - q), KEY ^ ((q >> 2) & 0xFFFF));
  check(Buffer.compare(Buffer.from(a), AREA) === 0, 'the decrypted area is critical.area');
  for (let i = 0x4000 + AREA.length; i < size; i++) if (fb[i] !== 0xFF) { check(false, 'padding'); break; }
  await read(0xF0000000, 8);
  setTimeout(() => {
    check(logs.some(l => /Installed! The FM-1 restarts on CRITICAL/.test(l)), 'the page says it is installed');
    console.log(fails ? fails + ' FAILED' : `install_sim: OK (${size} bytes of flash.bin, area ${AREA.length})`);
    process.exit(fails ? 1 : 0);
  }, 50);
}
plug('FM-1 Update', mb);
(async () => {
  await els.con.onclick();
  await new Promise(r => setTimeout(r, 50));
  check(logs.some(l => /m-bootloader v1.2/.test(l)), 'identity read: ' + logs.join(' | '));
  check(!els.go.disabled, 'Install enabled');
  els.go.onclick();
})();
