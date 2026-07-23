#!/usr/bin/env python3
"""bake_loops.py - The Dreamer v3 bank: multisample loop baker.
Recipe grammar: name, dur, root, layers[]; layer src = one path (static)
or list of 2-4 paths (MORPH: cosine crossfade through the list and back,
morph_cycles trips per loop). unison=(cents,amp) list, drift=loop-safe
random walk, lp=(base,depth,cycles) moving one-pole, hp_tilt, env=
(cycles_per_loop, exponent) bloom. All motion = integer cycles per loop
=> seamless. Output 12-bit quantized mono WAVs."""
import numpy as np, wave, os
SRC="/home/claude/AKWF-FREE/AKWF"; OUT="/home/claude/loops"; SR=44100
os.makedirs(OUT,exist_ok=True)
rng=np.random.default_rng(19960)
_c={}
def cyc(rel):
    if rel not in _c:
        with wave.open(os.path.join(SRC,rel),'rb') as w:
            d=np.frombuffer(w.readframes(w.getnframes()),dtype=np.int16).astype(np.float64)/32768.0
        d-=d.mean(); _c[rel]=d/np.abs(d).max()
    return _c[rel]
def render_layer(L,N,root,t):
    srcs=L['src'] if isinstance(L['src'],list) else [L['src']]
    tabs=[cyc(s) for s in srcs]; f0=root*2**L.get('oct',0); out=np.zeros(N)
    for cents,amp in L.get('unison',[(0,1.0)]):
        drift=np.cumsum(rng.normal(0,L.get('drift',1e-4),N)); drift-=np.linspace(0,drift[-1],N)
        f=f0*2**(cents/1200.0+drift)
        ph=(rng.uniform(0,600)+np.cumsum(f)*600.0/SR)%600.0
        i0=ph.astype(int); fr=ph-i0; i1=(i0+1)%600
        if len(tabs)==1:
            out+=amp*(tabs[0][i0]*(1-fr)+tabs[0][i1]*fr)
        else:
            mc=L.get('morph_cycles',1)
            pos=(1-np.cos(2*np.pi*mc*t/t[-1]))/2*(len(tabs)-1)
            k0=np.minimum(pos.astype(int),len(tabs)-2); mfr=pos-k0
            stk=np.stack([tb[i0]*(1-fr)+tb[i1]*fr for tb in tabs])
            out+=amp*(stk[k0,np.arange(N)]*(1-mfr)+stk[k0+1,np.arange(N)]*mfr)
    if L.get('hp_tilt'):
        lp=0.0; low=np.zeros(N)
        for i in range(N): lp+=0.04*(out[i]-lp); low[i]=lp
        out=out-low
    if L.get('lp'):
        base,depth,c=L['lp']; cut=base+depth*np.sin(2*np.pi*c*t/t[-1]+rng.uniform(0,6))
        lp=0.0; y=np.zeros(N)
        for i in range(N): lp+=cut[i]*(out[i]-lp); y[i]=lp
        out=y
    if L.get('env'):
        cpl,expo=L['env']; out=out*(((1-np.cos(2*np.pi*cpl*t/t[-1]))/2)**expo)
    return out*L.get('gain',1.0)
def bake(r):
    N=int(SR*r['dur']); t=np.arange(N)/SR; mix=np.zeros(N)
    for L in r['layers']: mix+=render_layer(L,N,r.get('root',220.0),t)
    if r.get('air',0)>0:
        n=rng.normal(0,1,N); b=np.zeros(N)
        for i in range(1,N): b[i]=b[i-1]+.1*(n[i]-b[i-1])
        mix+=r['air']*b*(1+.3*np.sin(2*np.pi*t/t[-1]))
    X=int(.3*SR); w=np.sin(np.linspace(0,np.pi/2,X))**2
    loop=mix[:N-X].copy(); loop[:X]=mix[N-X:]*(1-w)+mix[:X]*w
    loop/=np.abs(loop).max()*1.02
    q=np.clip(np.round(loop*2047),-2048,2047)
    p=os.path.join(OUT,r['name']+'.wav')
    with wave.open(p,'wb') as w2:
        w2.setnchannels(1); w2.setsampwidth(2); w2.setframerate(SR)
        w2.writeframes(((q/2048.0)*32767).astype(np.int16).tobytes())
    return p,len(q)
R=[]
def rcp(name,dur,layers,air=0.0,root=220.0): R.append(dict(name=name,dur=dur,layers=layers,air=air,root=root))
rcp('STRINGBOX_ENS',2.7,[dict(src='AKWF_stringbox/AKWF_cheeze_0006.wav',unison=[(-9,.55),(-4,.8),(0,1.),(4,.8),(9,.55)],drift=1.2e-4,lp=(0.25,0.10,1))],air=.035)
rcp('CATHEDRAL',3.05,[
 dict(src='AKWF_bw_perfectwaves/AKWF_sin.wav',oct=-1,unison=[(0,.9)],drift=2e-5,gain=.55),
 dict(src='AKWF_hvoice/AKWF_hvoice_0097.wav',unison=[(-7,.7),(-2,1.),(3,1.),(8,.7)],drift=1.3e-4,lp=(0.18,0.09,1),gain=.85),
 dict(src='AKWF_overtone/AKWF_overtone_0034.wav',oct=1,unison=[(-5,.8),(6,.8)],drift=1.8e-4,hp_tilt=True,env=(2,2.2),gain=.5)],air=.03)
rcp('CHOIR_MORPH',3.2,[dict(src=['AKWF_hvoice/AKWF_hvoice_0007.wav','AKWF_hvoice/AKWF_hvoice_0024.wav','AKWF_hvoice/AKWF_hvoice_0097.wav'],morph_cycles=1,unison=[(-5,.8),(0,1.),(5,.8)],drift=1.2e-4,lp=(0.2,0.06,2))],air=.045)
rcp('GLASS_CHOIR',3.0,[
 dict(src='AKWF_hvoice/AKWF_hvoice_0017.wav',unison=[(-4,.9),(4,.9)],drift=1e-4,gain=.8),
 dict(src='AKWF_eorgan/AKWF_eorgan_0048.wav',oct=1,unison=[(0,.7)],drift=6e-5,env=(3,1.8),gain=.4)],air=.03)
rcp('SOLINA_DREAM',2.8,[dict(src=['AKWF_stringbox/AKWF_cheeze_0001.wav','AKWF_stringbox/AKWF_cheeze_0006.wav'],morph_cycles=2,unison=[(-8,.6),(-3,.9),(3,.9),(8,.6)],drift=1.3e-4,lp=(0.28,0.08,1))],air=.03)
rcp('CELLO_SECTION',3.0,[
 dict(src='AKWF_cello/AKWF_cello_0003.wav',unison=[(-6,.7),(0,1.),(6,.7)],drift=1.5e-4,lp=(0.16,0.05,1),gain=.9),
 dict(src='AKWF_cello/AKWF_cello_0010.wav',oct=-1,unison=[(0,.6)],drift=8e-5,gain=.5)])
rcp('THEREMIN_SWARM',3.1,[dict(src=['AKWF_theremin/AKWF_theremin_0003.wav','AKWF_theremin/AKWF_theremin_0013.wav'],morph_cycles=1,unison=[(-11,.6),(-4,.9),(4,.9),(11,.6)],drift=2.2e-4,lp=(0.3,0.1,2))],air=.02)
rcp('DARK_BREATH',3.3,[
 dict(src='AKWF_clarinett/AKWF_clarinett_0014.wav',unison=[(-3,.9),(3,.9)],drift=1e-4,gain=.85),
 dict(src='AKWF_flute/AKWF_flute_0003.wav',oct=1,unison=[(0,.5)],drift=1.4e-4,env=(2,1.5),gain=.35)],air=.06)
rcp('FM_HALO',2.9,[
 dict(src='AKWF_epiano/AKWF_epiano_0010.wav',unison=[(-4,.9),(4,.9)],drift=9e-5,gain=.8),
 dict(src=['AKWF_fmsynth/AKWF_fmsynth_0118.wav','AKWF_fmsynth/AKWF_fmsynth_0054.wav'],morph_cycles=2,oct=1,unison=[(0,.6)],drift=1e-4,hp_tilt=True,env=(2,2.0),gain=.45)])
rcp('SPECTRAL_TIDE',3.4,[
 dict(src=['AKWF_overtone/AKWF_overtone_0014.wav','AKWF_overtone/AKWF_overtone_0036.wav','AKWF_overtone/AKWF_overtone_0021.wav'],morph_cycles=1,unison=[(-6,.8),(6,.8)],drift=1.6e-4,lp=(0.12,0.07,1),gain=.9),
 dict(src='AKWF_bw_perfectwaves/AKWF_sin.wav',oct=-1,unison=[(0,.8)],drift=3e-5,gain=.5)],air=.02)
rcp('SAW_CLOUD',2.8,[dict(src=['AKWF_bw_sawrounded/AKWF_R_sym_saw_10.wav','AKWF_bw_sawrounded/AKWF_R_sym_saw_14.wav'],morph_cycles=1,unison=[(-10,.5),(-5,.8),(0,1.),(5,.8),(10,.5)],drift=1.2e-4,lp=(0.22,0.08,1))],air=.025)
rcp('TANNERIN_GHOST',3.2,[
 dict(src='AKWF_theremin/AKWF_tannerin_0002.wav',unison=[(-3,.9),(3,.9)],drift=2.5e-4,lp=(0.2,0.1,1),gain=.9),
 dict(src='AKWF_hvoice/AKWF_hvoice_0099.wav',unison=[(-4,.3),(4,.3)],drift=1.5e-4,env=(1,1.6),gain=.35)],air=.04)
rcp('ORGAN_MASS',3.0,[
 dict(src='AKWF_eorgan/AKWF_eorgan_0030.wav',unison=[(-4,.8),(4,.8)],drift=7e-5,gain=.8),
 dict(src='AKWF_eorgan/AKWF_eorgan_0154.wav',oct=1,unison=[(0,.5)],drift=7e-5,gain=.4),
 dict(src='AKWF_bw_perfectwaves/AKWF_sin.wav',oct=-1,unison=[(0,.7)],drift=2e-5,gain=.5)],air=.02)
rcp('VOICE_OF_STEEL',3.3,[
 dict(src='AKWF_bw_perfectwaves/AKWF_sin.wav',oct=-1,unison=[(0,.85)],drift=2e-5,gain=.5),
 dict(src=['AKWF_hvoice/AKWF_hvoice_0040.wav','AKWF_raw/AKWF_raw_0015.wav'],morph_cycles=1,unison=[(-5,.9),(5,.9)],drift=1.4e-4,lp=(0.2,0.09,1),gain=.8)],air=.03)
rcp('BELL_GARDEN',3.1,[
 dict(src='AKWF_vgame/AKWF_vgame_0112.wav',oct=1,unison=[(-3,.8),(3,.8)],drift=1e-4,env=(3,2.5),hp_tilt=True,gain=.5),
 dict(src='AKWF_fmsynth/AKWF_fmsynth_0014.wav',unison=[(0,1.)],drift=8e-5,gain=.7),
 dict(src='AKWF_bw_perfectwaves/AKWF_tri.wav',oct=-1,unison=[(0,.6)],drift=3e-5,gain=.45)])
rcp('ACID_MIRAGE',2.6,[dict(src=['AKWF_bw_sawrounded/AKWF_R_asym_saw_09.wav','AKWF_bw_sawrounded/AKWF_R_asym_saw_12.wav'],morph_cycles=3,unison=[(-6,.9),(6,.9)],drift=1e-4,lp=(0.1,0.08,1))],air=.02)
if __name__=='__main__':
    total=0
    for r in R:
        p,n=bake(r); total+=n
        print(f"{r['name']:16s} {n/SR:4.2f}s {n*1.5/1024:5.0f} KB")
    print(f"\n{len(R)} loops, total {total*1.5/1024/1024:.1f} MB @ packed 12-bit")
