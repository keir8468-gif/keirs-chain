// Runs the browser reference engine on test input and writes input + output as raw float32.
const fs = require("fs");
const src = fs.readFileSync("/home/claude/xl_record/dsp_reference.js", "utf8") + "\nmodule.exports={XLChain};";
fs.writeFileSync("/tmp/ref_dsp.js", src);
const { XLChain } = require("/tmp/ref_dsp.js");
function wav(p){const b=fs.readFileSync(p);let o=12,ch=2,off=0,len=0;while(o<b.length){const id=b.toString("ascii",o,o+4),sz=b.readUInt32LE(o+4);if(id==="fmt ")ch=b.readUInt16LE(o+10);if(id==="data"){off=o+8;len=sz;break}o+=8+sz+(sz&1)}
 const n=len/(2*ch),L=new Float32Array(n),R=new Float32Array(n);for(let i=0;i<n;i++){L[i]=b.readInt16LE(off+(i*ch)*2)/32768;R[i]=b.readInt16LE(off+(i*ch+(ch>1?1:0))*2)/32768}return {L,R}}
const g = wav("/home/claude/xl_record/groove_124.wav");
const n = 44100 * 4, inL = g.L.subarray(0, n), inR = g.R.subarray(0, n);
fs.writeFileSync("in.f32", Buffer.concat([Buffer.from(inL.buffer, inL.byteOffset, n * 4), Buffer.from(inR.buffer, inR.byteOffset, n * 4)]));
const cases = JSON.parse(fs.readFileSync("cases.json", "utf8"));
for (const c of cases) {
  const ch = new XLChain(44100);
  ch.set({ gain: 0, match: true, word: "trunc", mode: "stereo", bypass: false, adOn: c.xlOn, daOn: c.xlOn,
    bright: c.eqOn ? c.bright : 0, brightFreq: c.brightFreq, eqPos: c.eqPost ? "post" : "pre", mkOn: c.mkOn, drive: c.drive, out: c.out,
    timing: { on: c.tOn, bpm: 124, grid: c.grid, swing: c.swing, shiftDir: c.earlier ? "earlier" : "later", shiftAmt: c.shift },
    transport: { pos: 0, loopLen: 0 } });
  const oL = new Float32Array(n), oR = new Float32Array(n);
  for (let i = 0; i < n; i += 128) { const m = Math.min(128, n - i); ch.process(inL.subarray(i, i + m), inR.subarray(i, i + m), oL.subarray(i, i + m), oR.subarray(i, i + m), m); }
  fs.writeFileSync(`ref_${c.name}.f32`, Buffer.concat([Buffer.from(oL.buffer), Buffer.from(oR.buffer)]));
  console.log("ref", c.name, "latency", ch.latency);
}
