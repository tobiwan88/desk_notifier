// Usage: node tools/js/test_clip_encoder.mjs <raw_rgba_frames> <out.clip> ['{"loop":true,"bg":[r,g,b]}']
// raw file: consecutive 240*240*4 RGBA frames, each shown for 80 ms.
import { readFileSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import path from 'node:path';

const require = createRequire(import.meta.url);
const here = path.dirname(fileURLToPath(import.meta.url));
const { encodeClip } = require(path.join(here, '../../firmware/components/web/ui/clip_encoder.js'));

const [rawPath, outPath, optJson] = process.argv.slice(2);
if (!rawPath || !outPath) {
  console.error('usage: node test_clip_encoder.mjs <raw> <out> [opts-json]');
  process.exit(2);
}
const opts = optJson ? JSON.parse(optJson) : {};
const raw = readFileSync(rawPath);
const frameBytes = 240 * 240 * 4;
if (raw.length % frameBytes !== 0) throw new Error('raw file is not a whole number of 240x240 RGBA frames');
const frames = [];
for (let off = 0; off < raw.length; off += frameBytes) {
  frames.push({ rgba: new Uint8ClampedArray(raw.buffer, raw.byteOffset + off, frameBytes), ms: 80 });
}
const bytes = encodeClip(frames, opts);
writeFileSync(outPath, bytes);
console.log(`frames=${frames.length} bytes=${bytes.length}`);
