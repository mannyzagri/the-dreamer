#!/usr/bin/env python3
"""FINAL bake architecture — pitch-perfect loops.
- NO per-layer random drift (was the wander bug)
- NO baked unison/detune (width is the engine's job now)
- Coherent phase (all layers start aligned)
- KEEP: morph (timbral transition), filter sweep, bloom env, multi-timbre layers
- Shared tiny drift optional (all layers together) for a whisper of life
- No rootHz correction, no octave-normalize, no anchor, no pitch-lock warp
  (cycles are perfect 600-sample periods; play them straight)
"""
import numpy as np, wave, os, json
import bake_loops as bl
from scipy.signal import resample_poly
SR=44100; OS=2
OUT='/home/claude/library_final'; os.makedirs(OUT,exist_ok=True)

def bake(recipe):
    name=recipe['name']; dur=recipe['dur']; root=recipe.get('root',220.0)
    N=int(SR*OS*dur); t=np.arange(N)/(SR*OS)
    rng=np.random.default_rng(hash(name)&0xffff)
    sd_amt=recipe.get('shared_drift',0.0)
    sd=np.cumsum(rng.normal(0,sd_amt,N)); sd-=np.linspace(0,sd[-1],N)  # shared, tiny
    mix=np.zeros(N)
    for L in recipe['layers']:
        srcs=L['src'] if isinstance(L['src'],list) else [L['src']]
        tabs=[bl.cyc(s) for s in srcs]
        f0=root*2**L.get('oct',0)
        # SINGLE voice per layer (no unison). Coherent phase (start 0). Shared drift.
        f=f0*2**(sd)
        ph=(np.cumsum(f)*600.0/(SR*OS))%600
        i0=ph.astype(int); fr=ph-i0; i1=(i0+1)%600
        if len(tabs)==1:
            layer=tabs[0][i0]*(1-fr)+tabs[0][i1]*fr
        else:
            mc=L.get('morph_cycles',1)
            pos=(1-np.cos(2*np.pi*mc*t/t[-1]))/2*(len(tabs)-1)
            k0=np.minimum(pos.astype(int),len(tabs)-2); mfr=pos-k0
            stk=np.stack([tb[i0]*(1-fr)+tb[i1]*fr for tb in tabs])
            layer=stk[k0,np.arange(N)]*(1-mfr)+stk[k0+1,np.arange(N)]*mfr
        if L.get('hp_tilt'):
            lp=0.;low=np.zeros(N)
            for i in range(N): lp+=0.04*(layer[i]-lp); low[i]=lp
            layer=layer-low
        if L.get('lp'):
            base,depth,c=L['lp']; cut=base+depth*np.sin(2*np.pi*c*t/t[-1])
            lp=0.;y=np.zeros(N)
            for i in range(N): lp+=cut[i]*(layer[i]-lp); y[i]=lp
            layer=y
        if L.get('env'):
            cpl,expo=L['env']; layer=layer*(((1-np.cos(2*np.pi*cpl*t/t[-1]))/2)**expo)
        mix+=layer*L.get('gain',1.0)
    if recipe.get('air',0)>0:
        n=rng.normal(0,1,N); b=np.zeros(N)
        for i in range(1,N): b[i]=b[i-1]+.1*(n[i]-b[i-1])
        mix+=recipe['air']*b
    lo=resample_poly(mix,1,OS); N2=len(lo)
    X=int(.3*SR); w=np.sin(np.linspace(0,np.pi/2,X))**2
    loop=lo[:N2-X].copy(); loop[:X]=lo[N2-X:]*(1-w)+lo[:X]*w
    loop=np.nan_to_num(loop); loop/=np.abs(loop).max()*1.02
    q=np.clip(np.round(loop*2047),-2048,2047)
    with wave.open(os.path.join(OUT,name+'.wav'),'wb') as w2:
        w2.setnchannels(1);w2.setsampwidth(2);w2.setframerate(SR)
        w2.writeframes(((q/2048.)*32767).astype(np.int16).tobytes())
    return len(q)

# ---- Option C recipe generation: mixed morph + stacked layers ----
fams=json.load(open('/home/claude/fam_sources.json'))
TPL={
 'pad':  dict(lp=(0.24,0.06,1),air=.03,dur=2.8),
 'airy': dict(lp=(0.35,0.04,1),air=.03,dur=2.8),
 'vox':  dict(lp=(0.18,0.05,1),air=.045,dur=3.0),
 'ether':dict(lp=(0.2,0.05,1),air=.03,dur=2.9),
 'fm':   dict(lp=None,air=.02,dur=2.7,env=(3,1.6)),
 'wind': dict(lp=None,air=.06,dur=3.0),
 'metal':dict(lp=(0.2,0.07,1),air=.02,dur=2.8,hp=True),
}
COUNT={'pad':28,'airy':18,'vox':16,'ether':16,'fm':14,'wind':12,'metal':12}
# sub-octave anchors per family (clean geometric or pure sources)
SUB={'pad':'AKWF_bw_perfectwaves/AKWF_saw.wav','airy':'AKWF_bw_perfectwaves/AKWF_tri.wav',
     'vox':'AKWF_bw_perfectwaves/AKWF_sin.wav','ether':'AKWF_bw_perfectwaves/AKWF_sin.wav',
     'fm':'AKWF_bw_perfectwaves/AKWF_sin.wav','wind':'AKWF_bw_perfectwaves/AKWF_tri.wav',
     'metal':'AKWF_bw_perfectwaves/AKWF_sin.wav'}

recipes=[]
for fam in ['pad','airy','vox','ether','fm','wind','metal']:
    srcs=fams[fam]; t=TPL[fam]; n=COUNT[fam]
    for i in range(n):
        mode = i % 4   # 0,1 = morph ; 2 = stack ; 3 = morph + stack (both)
        a=srcs[i % len(srcs)]
        b=srcs[(i+3) % len(srcs)]
        c=srcs[(i+7) % len(srcs)]
        layers=[]
        if mode in (0,1):
            # MORPH layer: 2 or 3 cycles from the family, cosine sweep over loop
            srclist=[a,b] if mode==0 else [a,b,c]
            L=dict(src=srclist, morph_cycles=1 if mode==0 else 1, gain=1.0)
            if t.get('lp'): L['lp']=t['lp']
            if t.get('hp'): L['hp_tilt']=True
            layers.append(L)
        elif mode==2:
            # STACKED: body + different source octave up (bloom) + sub anchor
            L1=dict(src=a,gain=0.9)
            if t.get('lp'): L1['lp']=t['lp']
            if t.get('hp'): L1['hp_tilt']=True
            layers.append(L1)
            layers.append(dict(src=b,oct=1,env=(2,1.8),gain=0.42))
            layers.append(dict(src=SUB[fam],oct=-1,gain=0.45))
        else:
            # BOTH: morphing body over a steady sub, plus octave shimmer
            L1=dict(src=[a,b],morph_cycles=1,gain=0.88)
            if t.get('lp'): L1['lp']=t['lp']
            if t.get('hp'): L1['hp_tilt']=True
            layers.append(L1)
            layers.append(dict(src=c,oct=1,env=(3,2.0),gain=0.38))
            layers.append(dict(src=SUB[fam],oct=-1,gain=0.42))
        if t.get('env') and mode in (0,1):
            layers[0]['env']=t['env']
        recipes.append(dict(name=f"{fam.upper()}_{i+1:02d}",dur=t['dur'],root=220.,
                            air=t['air'],layers=layers))

# 14 MORPH specials: cross-family morph over a steady sub (keep as before, add sub)
import random; random.seed(7)
def pick(f): return random.choice(fams[f])
specials=[('MORPH_PADAIR','pad','airy'),('MORPH_VOXETHER','vox','ether'),('MORPH_STRWIND','pad','wind'),
 ('MORPH_ETHERFM','ether','fm'),('MORPH_AIRGLASS','airy','ether'),('MORPH_FMBELL','fm','ether'),
 ('MORPH_PADVOX','pad','vox'),('MORPH_WINDVOX','wind','vox'),('MORPH_PADPAD','pad','pad'),
 ('MORPH_VOXVOX','vox','vox'),('MORPH_FMPAD','fm','pad'),
 ('MORPH_ETHMETAL','ether','metal'),('MORPH_VOXMETAL','vox','metal'),('MORPH_METALAIR','metal','airy')]
for nm,fa,fb in specials:
    recipes.append(dict(name=nm,dur=3.0,root=220.,air=.03,layers=[
      dict(src=[pick(fa),pick(fb)],morph_cycles=1,lp=(0.22,0.06,1),gain=0.95),
      dict(src='AKWF_bw_perfectwaves/AKWF_sin.wav',oct=-1,gain=0.40)]))

if __name__=='__main__':
    tot=0;ok=0
    for r in recipes:
        try: tot+=bake(r); ok+=1
        except Exception as e: print("FAIL",r['name'],e)
    print(f"baked {ok}/{len(recipes)} loops, {tot*1.5/1024/1024:.1f} MB")
