/* SPDX-License-Identifier: GPL-3.0-only
 * The firmware package, built in the browser for the unit's chip key (m-bootloader docs/SPEC.md 5.1, 5.6, 11.6):
 * the plain application area (tools/mkfw.py) encrypted with the key, inside a flash.bin, inside a package of one
 * entry that m-bootloader's MIDI mode reads with 0x30 requests. Written for this project. */
function crc16(d, c = 0) {
  for (const b of d) { c ^= b << 8; for (let i = 0; i < 8; i++) c = c & 0x8000 ? ((c << 1) ^ 0x1021) & 0xFFFF : (c << 1) & 0xFFFF; }
  return c;
}
/* ENC: XOR with the state's low byte, then one step of the state (SPEC 5.1) */
function enc(buf, off, n, key) {
  let k = key & 0xFFFF;
  for (let i = 0; i < n; i++) { buf[off + i] ^= k & 0xFF; k = k & 0x8000 ? ((k << 1) ^ 0x1021) & 0xFFFF : (k << 1) & 0xFFFF; }
}
/* the application area: each 32-byte line from key XOR (its offset / 4) */
function encArea(area, key) {
  const a = Uint8Array.from(area);
  for (let q = 0; q < a.length; q += 32) enc(a, q, Math.min(32, a.length - q), key ^ ((q >> 2) & 0xFFFF));
  return a;
}
const put16 = (a, i, v) => { a[i] = v & 255; a[i + 1] = v >> 8 & 255; };
const put32 = (a, i, v) => { for (let k = 0; k < 4; k++) a[i + k] = v >>> (8 * k) & 255; };
const get16 = (a, i) => a[i] | a[i + 1] << 8;
/* the package: header (64 bytes), the entries (80 each) from 0x40, flash.bin at 0x400; with `ota` (SLOOP 3's update
 * loader, build/upd/ota.bin) a second entry after flash.bin, type 100: SLOOP 2.8 keeps it and restarts into it */
function buildPackage(area, key, ota) {
  const FIRST = 0x4000, SECTOR = 0x1000, AT = 0x400;
  const size = FIRST + Math.ceil(area.length / SECTOR) * SECTOR;
  const flash = new Uint8Array(size).fill(0xFF);
  flash.set(encArea(area, key), FIRST);
  const otaAt = AT + size, n = ota ? 2 : 1;
  const pkg = new Uint8Array(otaAt + (ota ? ota.length : 0)).fill(0);
  pkg.set(flash, AT);
  if (ota) pkg.set(ota, otaAt);
  const list = new Uint8Array(80 * n);
  const entry = (i, type, crc, at, len, name) => {
    const e = new Uint8Array(80);
    put16(e, 0, type); put16(e, 2, 0); put16(e, 4, crc); put32(e, 8, at); put32(e, 12, len);
    for (let k = 0; k < name.length; k++) e[64 + k] = name.charCodeAt(k);
    enc(e, 0, 80, 0xFFFF); list.set(e, 80 * i);
  };
  entry(0, 0, crc16(flash), AT, size, 'flash.bin');
  if (ota) entry(1, 100, crc16(ota), otaAt, ota.length, 'ota.bin');
  const h = new Uint8Array(64);
  put16(h, 2, crc16(list)); put32(h, 4, pkg.length); put16(h, 8, n); put16(h, 10, 4); put32(h, 12, 0x200);
  const chip = 'AC791N'; for (let i = 0; i < chip.length; i++) h[16 + i] = chip.charCodeAt(i);
  put16(h, 0, crc16(h.subarray(2, 64)));
  enc(h, 0, 64, 0xFFFF);
  pkg.set(h, 0); pkg.set(list, 0x40);
  return { pkg, size, flashAt: AT };
}
/* the checks m-bootloader makes before writing (its midi_update.c), and SLOOP 3's update loader (upd/upd_core.c),
 * made here first */
function checkPackage(p, key) {
  const h = p.pkg.slice(0, 64); enc(h, 0, 64, 0xFFFF);
  if (crc16(h.subarray(2, 64)) !== get16(h, 0)) return 'header';
  const n = get16(h, 8);
  if (n < 1 || n > 11 || crc16(p.pkg.subarray(0x40, 0x40 + 80 * n)) !== get16(h, 2)) return 'list';
  const e = p.pkg.slice(0x40, 0x40 + 80); enc(e, 0, 80, 0xFFFF);
  const off = e[8] | e[9] << 8 | e[10] << 16 | e[11] << 24, size = e[12] | e[13] << 8 | e[14] << 16 | e[15] << 24;
  if (get16(e, 0) !== 0 || size % 0x1000 || size <= 0x5000 || size > 0x7A000) return 'entry';
  if (crc16(p.pkg.subarray(off, off + size)) !== get16(e, 4)) return 'crc';
  const r = p.pkg.slice(off + 0x4000, off + 0x4000 + 32); enc(r, 0, 32, key);
  if (crc16(r.subarray(2, 32)) !== get16(r, 0)) return 'key';
  return '';
}
if (typeof module !== 'undefined') module.exports = { crc16, enc, encArea, buildPackage, checkPackage };
