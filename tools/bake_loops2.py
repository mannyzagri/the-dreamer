#!/usr/bin/env python3
"""Batch 2: vox+strings, vox+metal, strings+octave strings/vox."""
import sys; sys.path.insert(0,'/home/claude')
from bake_loops import bake, SR

R2=[]
def rcp(name,dur,layers,air=0.0,root=220.0):
    R2.append(dict(name=name,dur=dur,layers=layers,air=air,root=root))

# ---------- VOX + STRINGS ----------
rcp('ETHEREAL_CHOIR',3.2,[   # stringbox bed, vox morph floating above
 dict(src='AKWF_stringbox/AKWF_cheeze_0006.wav',unison=[(-8,.6),(-3,.9),(3,.9),(8,.6)],drift=1.2e-4,lp=(0.22,0.07,1),gain=.75),
 dict(src=['AKWF_hvoice/AKWF_hvoice_0007.wav','AKWF_hvoice/AKWF_hvoice_0097.wav'],morph_cycles=1,
      unison=[(-4,.9),(4,.9)],drift=1.4e-4,lp=(0.18,0.05,2),gain=.7)],air=.04)
rcp('OPERA_VIOLA',3.0,[      # violin body, vox vowel morph as the "singer"
 dict(src='AKWF_violin/AKWF_violin_0008.wav',unison=[(-6,.8),(0,1.),(6,.8)],drift=1.5e-4,lp=(0.2,0.06,1),gain=.8),
 dict(src=['AKWF_hvoice/AKWF_hvoice_0024.wav','AKWF_hvoice/AKWF_hvoice_0017.wav','AKWF_hvoice/AKWF_hvoice_0099.wav'],
      morph_cycles=1,unison=[(2,.7)],drift=1.2e-4,env=(1,1.2),gain=.55)],air=.035)
rcp('CELLO_CHOIR',3.3,[      # dark: cello bed + low vox breathing
 dict(src='AKWF_cello/AKWF_cello_0003.wav',unison=[(-5,.8),(5,.8)],drift=1.3e-4,lp=(0.15,0.05,1),gain=.85),
 dict(src='AKWF_cello/AKWF_cello_0010.wav',oct=-1,unison=[(0,.6)],drift=7e-5,gain=.5),
 dict(src=['AKWF_hvoice/AKWF_hvoice_0097.wav','AKWF_hvoice/AKWF_hvoice_0007.wav'],morph_cycles=2,
      unison=[(-3,.7),(3,.7)],drift=1.2e-4,env=(2,1.4),gain=.5)],air=.045)

# ---------- VOX + METAL ----------
rcp('STEEL_CHOIR',3.1,[      # choir with metallic halo blooms
 dict(src=['AKWF_hvoice/AKWF_hvoice_0007.wav','AKWF_hvoice/AKWF_hvoice_0024.wav'],morph_cycles=1,
      unison=[(-6,.8),(0,1.),(6,.8)],drift=1.3e-4,lp=(0.2,0.06,1),gain=.85),
 dict(src='AKWF_overtone/AKWF_overtone_0036.wav',oct=1,unison=[(-4,.8),(5,.8)],drift=1.8e-4,
      hp_tilt=True,env=(3,2.4),gain=.45)],air=.03)
rcp('CHROME_VOICES',3.4,[    # vox<->metal morph itself = the voice turns to chrome
 dict(src='AKWF_bw_perfectwaves/AKWF_sin.wav',oct=-1,unison=[(0,.8)],drift=2e-5,gain=.5),
 dict(src=['AKWF_hvoice/AKWF_hvoice_0060.wav','AKWF_overtone/AKWF_overtone_0014.wav','AKWF_hvoice/AKWF_hvoice_0040.wav'],
      morph_cycles=1,unison=[(-5,.9),(5,.9)],drift=1.5e-4,lp=(0.17,0.08,1),gain=.8)],air=.03)
rcp('ORACLE',3.2,[           # whispering vox + snippet clang blinks, very audity
 dict(src='AKWF_hvoice/AKWF_hvoice_0099.wav',unison=[(-4,.9),(4,.9)],drift=1.4e-4,lp=(0.16,0.05,1),gain=.8),
 dict(src='AKWF_snippets/AKWF_snippet_0033.wav',oct=1,unison=[(0,.6)],drift=2e-4,
      hp_tilt=True,env=(4,2.8),gain=.4)],air=.05)

# ---------- STRINGS + OCTAVE STRINGS / OTHER VOX ----------
rcp('STRING_OCTAVES',3.0,[   # the 16'+8' string machine registration
 dict(src='AKWF_stringbox/AKWF_cheeze_0003.wav',unison=[(-7,.7),(0,1.),(7,.7)],drift=1.2e-4,lp=(0.24,0.07,1),gain=.85),
 dict(src='AKWF_stringbox/AKWF_cheeze_0006.wav',oct=1,unison=[(-4,.6),(4,.6)],drift=1.4e-4,lp=(0.3,0.08,2),gain=.5)],air=.03)
rcp('GHOST_ORCHESTRA',3.4,[  # cello 8' + violin 4' + vox whisper between
 dict(src='AKWF_cello/AKWF_cello_0005.wav',unison=[(-5,.8),(5,.8)],drift=1.3e-4,lp=(0.16,0.05,1),gain=.8),
 dict(src='AKWF_violin/AKWF_violin_0003.wav',oct=1,unison=[(-3,.6),(3,.6)],drift=1.5e-4,env=(2,1.3),gain=.45),
 dict(src='AKWF_hvoice/AKWF_hvoice_0017.wav',unison=[(1,.5)],drift=1.2e-4,env=(1,1.8),gain=.4)],air=.04)
rcp('VIOLIN_SERAPH',3.1,[    # violin bed + octave-up vox halo
 dict(src='AKWF_violin/AKWF_violin_0008.wav',unison=[(-6,.8),(0,1.),(6,.8)],drift=1.4e-4,lp=(0.2,0.06,1),gain=.8),
 dict(src=['AKWF_hvoice/AKWF_hvoice_0007.wav','AKWF_hvoice/AKWF_hvoice_0060.wav'],morph_cycles=2,
      oct=1,unison=[(-3,.6),(3,.6)],drift=1.6e-4,env=(2,1.6),gain=.4)],air=.04)
rcp('CATHEDRAL_STRINGS',3.5,[ # grand finale: stringbox + oct strings + vox + faint metal
 dict(src='AKWF_bw_perfectwaves/AKWF_sin.wav',oct=-1,unison=[(0,.8)],drift=2e-5,gain=.45),
 dict(src='AKWF_stringbox/AKWF_cheeze_0006.wav',unison=[(-8,.6),(-3,.9),(3,.9),(8,.6)],drift=1.2e-4,lp=(0.22,0.07,1),gain=.7),
 dict(src='AKWF_stringbox/AKWF_cheeze_0001.wav',oct=1,unison=[(-4,.5),(4,.5)],drift=1.5e-4,gain=.35),
 dict(src=['AKWF_hvoice/AKWF_hvoice_0097.wav','AKWF_hvoice/AKWF_hvoice_0024.wav'],morph_cycles=1,
      unison=[(0,.7)],drift=1.3e-4,env=(1,1.3),gain=.5),
 dict(src='AKWF_overtone/AKWF_overtone_0042.wav',oct=2,unison=[(0,.5)],drift=2e-4,
      hp_tilt=True,env=(2,3.0),gain=.3)],air=.04)

if __name__=='__main__':
    total=0
    for r in R2:
        p,n=bake(r); total+=n
        print(f"{r['name']:18s} {n/SR:4.2f}s {n*1.5/1024:5.0f} KB")
    print(f"\n{len(R2)} loops, {total*1.5/1024/1024:.1f} MB")
