// Feed the reference detector the Osaka breakdown and the drum loop; print the trail of results
const fs=require("fs");
eval(fs.readFileSync("/home/claude/xl_record/keydet.js","utf8").replace("const KD =","global.KD ="));
function wav(p){const b=fs.readFileSync(p);let o=12,ch=2,sr=44100,off=0,len=0;while(o<b.length){const id=b.toString("ascii",o,o+4),sz=b.readUInt32LE(o+4);if(id==="fmt "){ch=b.readUInt16LE(o+10);sr=b.readUInt32LE(o+12)}if(id==="data"){off=o+8;len=sz;break}o+=8+sz+(sz&1)}
 const n=len/(2*ch),x=new Float32Array(n);for(let i=0;i<n;i++){let s=0;for(let c=0;c<ch;c++)s+=b.readInt16LE(off+(i*ch+c)*2);x[i]=s/ch/32768}return {x,sr}}
for(const p of ["/home/claude/xl_record/osaka_breakdown.wav","/home/claude/xl_record/groove_124.wav","/home/claude/renders/00_dry.wav"]){
  const {x,sr}=wav(p);fs.writeFileSync("/tmp/kd_"+p.split("/").pop()+".f32",Buffer.from(x.buffer));
  const L=new KD.Live(sr),N=16384,hop=Math.round(sr*0.1);const trail=[];
  for(let o=N;o<=x.length;o+=hop){L.push(x.subarray(o-N,o),hop/sr);trail.push((L.key?L.key.name+"|"+L.key.camelot:"-")+"|"+(L.chord?L.chord.name:"-"))}
  console.log(p.split("/").pop(), sr, trail.length, "final:", trail[trail.length-1], "tuning", (L.tuning*100).toFixed(3));
  fs.writeFileSync("/tmp/kdtrail_"+p.split("/").pop()+".txt", trail.join("\n"));
}
