/* Clip v1 encoder (RAW565), byte-identical to clipprep (Python): browser script and Node module.
 * encodeClip(frames, opts) -> Uint8Array
 *   frames: [{rgba: Uint8ClampedArray|Uint8Array (240*240*4, alpha flattened), ms: int}]
 *   opts:   {loop: bool, region: null | [x, y, w, h], bg: [r, g, b] (default black)}
 *   With region, the caller puts the region content into the 240x240 frame; pixels outside the
 *   region are forced to bg, and only the region is diffed. Outside the circle is always bg.
 * The returned array has .payloadBytes (per-frame rect payload bytes) for budgeting.
 */
(function (g) {
  'use strict';

  var S = 240, R2X4 = 4 * 14470, MAX_RECTS = 16, CHUNK = 20, MERGE_GAP = 3;
  var HEADER_SIZE = 32, FLAG_LOOP = 1, FLAG_CIRCLE_TRIMMED = 8, FRAME_KEY = 1;

  // Circle mask and chords (same formula as clipprep/geometry.py).
  var CHORD_A = new Int32Array(S), CHORD_B = new Int32Array(S);
  var MASK = new Uint8Array(S * S);
  (function () {
    for (var y = 0; y < S; y++) {
      var a = -1, b = 0;
      for (var x = 0; x < S; x++) {
        var dx = 2 * x - (S - 1), dy = 2 * y - (S - 1);
        if (dx * dx + dy * dy <= R2X4) {
          if (a < 0) a = x;
          b = x + 1;
          MASK[y * S + x] = 1;
        }
      }
      CHORD_A[y] = a < 0 ? 0 : a;
      CHORD_B[y] = a < 0 ? 0 : b;
    }
  })();

  // Widest chord over rows [y, y+h) (geometry.span_of_rows).
  function spanOfRows(y, h) {
    var lo = Infinity, hi = -Infinity;
    for (var r = y; r < Math.min(y + h, S); r++) {
      if (CHORD_B[r] > CHORD_A[r]) {
        if (CHORD_A[r] < lo) lo = CHORD_A[r];
        if (CHORD_B[r] > hi) hi = CHORD_B[r];
      }
    }
    return lo === Infinity ? [0, 0] : [lo, hi];
  }

  var CRC_TABLE = (function () {
    var t = new Uint32Array(256);
    for (var n = 0; n < 256; n++) {
      var c = n;
      for (var k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
      t[n] = c >>> 0;
    }
    return t;
  })();

  function crc32(bytes) {
    var c = 0xFFFFFFFF;
    for (var i = 0; i < bytes.length; i++) c = CRC_TABLE[(c ^ bytes[i]) & 0xFF] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
  }

  // RGB888 -> logical RGB565 (same bit ops as frames.to_565).
  function to565(r, g, b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  // Row bands for one frame diff (bands.changed_bands). changed: Uint8Array S*S.
  function changedBands(changed) {
    var runs = [], y, x;
    for (y = 0; y < S; y++) {
      var any = false;
      for (x = 0; x < S && !any; x++) any = changed[y * S + x] === 1;
      if (!any) continue;
      var last = runs.length - 1;
      if (last >= 0 && y - runs[last][1] <= MERGE_GAP) runs[last][1] = y + 1;
      else runs.push([y, y + 1]);
    }
    var rects = [];
    for (var i = 0; i < runs.length; i++) {
      var y0 = runs[i][0], y1 = runs[i][1];
      for (var c0 = y0; c0 < y1; c0 += CHUNK) {
        var c1 = Math.min(c0 + CHUNK, y1);
        if (rowsHaveChange(changed, c0, c1)) rects.push(runRect(changed, c0, c1));
      }
    }
    while (rects.length > MAX_RECTS) {
      var bestI = 0, bestGap = Infinity;
      for (var k = 0; k < rects.length - 1; k++) {
        var gap = rects[k + 1][1] - (rects[k][1] + rects[k][3]);
        if (gap < bestGap) { bestGap = gap; bestI = k; }
      }
      rects.splice(bestI, 2, unionRows(rects[bestI], rects[bestI + 1]));
    }
    return rects;
  }

  function rowsHaveChange(changed, y0, y1) {
    for (var y = y0; y < y1; y++)
      for (var x = 0; x < S; x++) if (changed[y * S + x]) return true;
    return false;
  }

  function runRect(changed, y0, y1) {
    var x0 = S, x1 = 0;
    for (var y = y0; y < y1; y++)
      for (var x = 0; x < S; x++)
        if (changed[y * S + x]) { if (x < x0) x0 = x; if (x + 1 > x1) x1 = x + 1; }
    var sp = spanOfRows(y0, y1 - y0);
    x0 = Math.max(x0, sp[0]);
    x1 = Math.min(x1, sp[1]);
    return [x0, y0, x1 - x0, y1 - y0];
  }

  function unionRows(a, b) {
    var x0 = Math.min(a[0], b[0]), x1 = Math.max(a[0] + a[2], b[0] + b[2]);
    var y0 = a[1], y1 = b[1] + b[3];
    var sp = spanOfRows(y0, y1 - y0);
    x0 = Math.max(x0, sp[0]);
    x1 = Math.min(x1, sp[1]);
    return [x0, y0, x1 - x0, y1 - y0];
  }

  function encodeClip(frames, opts) {
    opts = opts || {};
    var loop = !!opts.loop;
    var bg = opts.bg || [0, 0, 0];
    var bg565 = to565(bg[0], bg[1], bg[2]);
    var region = opts.region || null;
    var regionMask = null;
    if (region) {
      regionMask = new Uint8Array(S * S);
      for (var ry = region[1]; ry < region[1] + region[3]; ry++)
        for (var rx = region[0]; rx < region[0] + region[2]; rx++)
          if (MASK[ry * S + rx]) regionMask[ry * S + rx] = 1;
    }

    var prev = null, bodyParts = [], bodyLen = 0, index = [], payloadBytes = [];
    for (var n = 0; n < frames.length; n++) {
      var src = frames[n].rgba, ms = frames[n].ms;
      var cur = new Uint16Array(S * S);
      var changed = new Uint8Array(S * S);
      var i;
      for (i = 0; i < S * S; i++) {
        var inside = MASK[i] === 1 && (regionMask === null || regionMask[i] === 1);
        if (inside) cur[i] = to565(src[4 * i], src[4 * i + 1], src[4 * i + 2]);
        else cur[i] = bg565;
        if (n === 0) changed[i] = regionMask === null ? MASK[i] : regionMask[i];
        else changed[i] = (prev[i] !== cur[i] && MASK[i] === 1 && (regionMask === null || regionMask[i] === 1)) ? 1 : 0;
      }
      var rects = changedBands(changed);

      var total = 4;
      var parts = [], payloadSum = 0, rectHeads = [];
      for (var r = 0; r < rects.length; r++) {
        var rc = rects[r], w = rc[2], h = rc[3], plen = w * h * 2;
        var payload = new Uint8Array(plen), p = 0;
        for (var yy = rc[1]; yy < rc[1] + h; yy++)
          for (var xx = rc[0]; xx < rc[0] + w; xx++) {
            var v = cur[yy * S + xx];
            payload[p++] = v >> 8;
            payload[p++] = v & 0xFF;
          }
        var head = new Uint8Array(14), hv = new DataView(head.buffer);
        hv.setUint16(0, rc[0], true); hv.setUint16(2, rc[1], true);
        hv.setUint16(4, w, true); hv.setUint16(6, h, true);
        head[8] = 0; head[9] = 0;
        hv.setUint32(10, plen, true);
        rectHeads.push(head, payload);
        total += 14 + plen;
        payloadSum += plen;
      }
      var chunk = new Uint8Array(total), cv = new DataView(chunk.buffer);
      cv.setUint16(0, rects.length, true); cv.setUint16(2, 0, true);
      var off = 4;
      for (var q = 0; q < rectHeads.length; q++) { chunk.set(rectHeads[q], off); off += rectHeads[q].length; }
      index.push({ off: HEADER_SIZE + bodyLen, len: total, ms: ms, flags: n === 0 ? FRAME_KEY : 0 });
      bodyParts.push(chunk);
      bodyLen += total;
      payloadBytes.push(payloadSum);
      prev = cur;
    }

    var indexOffset = HEADER_SIZE + bodyLen;
    var dataLen = bodyLen + index.length * 12;
    var data = new Uint8Array(dataLen), pos = 0;
    for (var b = 0; b < bodyParts.length; b++) { data.set(bodyParts[b], pos); pos += bodyParts[b].length; }
    var dv = new DataView(data.buffer);
    for (var e = 0; e < index.length; e++, pos += 12) {
      dv.setUint32(pos, index[e].off, true);
      dv.setUint32(pos + 4, index[e].len, true);
      dv.setUint16(pos + 8, index[e].ms, true);
      dv.setUint16(pos + 10, index[e].flags, true);
    }

    var out = new Uint8Array(HEADER_SIZE + dataLen), hd = new DataView(out.buffer);
    out.set([0x45, 0x59, 0x43, 0x4C], 0); // "EYCL"
    hd.setUint16(4, 1, true);            // version
    hd.setUint16(6, HEADER_SIZE, true);  // header size
    hd.setUint16(8, S, true);
    hd.setUint16(10, S, true);
    out[12] = 1;                         // pixel format RAW565
    out[13] = FLAG_CIRCLE_TRIMMED | (loop ? FLAG_LOOP : 0);
    hd.setUint16(14, frames.length, true);
    hd.setUint16(16, 0, true);           // key index
    hd.setUint16(18, 0, true);           // palette count
    hd.setUint32(20, indexOffset, true);
    hd.setUint32(24, 0, true);           // palette offset
    hd.setUint32(28, crc32(data), true);
    out.set(data, HEADER_SIZE);
    out.payloadBytes = payloadBytes;
    return out;
  }

  // Worst 1-second window of payload bytes (pipeline.PreparedClip.max_bps).
  function worstBps(sizes, durs) {
    var worst = 0, j = 0, acc = 0;
    for (var i = 0; i < sizes.length; i++) {
      acc += sizes[i];
      var sumDur = function (a, b) { var s = 0; for (var k = a; k < b; k++) s += durs[k]; return s; };
      while (sumDur(j, i + 1) > 1000 && j < i) { acc -= sizes[j]; j++; }
      var span = Math.max(sumDur(j, i + 1), 1);
      worst = Math.max(worst, acc * 1000 / Math.max(span, 200));
    }
    return worst;
  }

  var api = { encodeClip: encodeClip, worstBps: worstBps, spanOfRows: spanOfRows };
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
  else g.encodeClip = encodeClip, g.clipWorstBps = worstBps;
})(typeof globalThis !== 'undefined' ? globalThis : this);
