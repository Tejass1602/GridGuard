#!/usr/bin/env python3
"""Deterministic NumPy-only physics event synthesizer, trainer and C exporter."""
from pathlib import Path
import numpy as np

SEED = 42
ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'firmware/lib/gridguard_core/include/nilm_weights.h'
FS, N = 3200.0, 64
IRMS = np.array([4.35, 0.41, 0.38, 1.36], dtype=np.float64)
PHASE = np.deg2rad([0.0, 37.0, -12.0, 36.9])
RATIOS = np.array([[0,0,0,0],[.04,.02,.01,0],[.72,.48,.26,.12],[.08,.05,.02,.01]], dtype=np.float64)
KAPPA = np.array([1.05, 2.20, 1.80, 4.50])
TAU = np.array([.05,.25,.04,.35])

def cycle(vrms, hz, active, elapsed, rng, noise):
    theta = 2*np.pi*hz*np.arange(N)/FS + rng.uniform(0, 2*np.pi)
    v = np.sqrt(2)*vrms*(np.sin(theta)+.015*np.sin(3*theta+.2)+.010*np.sin(5*theta-.1))
    i = np.zeros(N)
    for a in range(4):
        if active[a]:
            env = 1+(KAPPA[a]-1)*np.exp(-elapsed[a]/TAU[a])
            harmonics = np.zeros(N)
            for h, ratio in zip((1,3,5,7,9), np.r_[1.0,RATIOS[a]]):
                phase = PHASE[a] if h == 1 else PHASE[a]*(1 if h%2 else -1)
                harmonics += np.sqrt(2)*IRMS[a]*ratio*np.sin(h*theta-phase)
            i += env*harmonics
    v += rng.normal(0, noise, N)
    i += rng.normal(0, noise/20, N)
    cv = np.clip(np.rint(2048+v/400*2047),0,4095)
    ci = np.clip(np.rint(2048+i/40*2047),0,4095)
    return (cv-2048)*400/2047, (ci-2048)*40/2047

def measures(v, i):
    v = v-v.mean(); i=i-i.mean()
    vr=np.sqrt(np.mean(v*v)); ir=np.sqrt(np.mean(i*i)); p=np.mean(v*i)
    bins=np.fft.rfft(v); ibins=np.fft.rfft(i)
    scale=np.sqrt(2)/N
    # DFT phasor convention yields Q positive for lagging current.
    def phasor(z, k): return (z[k].real*scale, -z[k].imag*scale)
    vr1,vi1=phasor(bins,1); ir1,ii1=phasor(ibins,1)
    q=vi1*ir1-vr1*ii1
    ih=np.array([np.sqrt(2)*abs(ibins[h])/N for h in (1,3,5,7,9,11)])
    return np.array([vr,ir,p,q,vr*ir]), ih

def event_features(rng, cls):
    vrms=rng.uniform(215,245); hz=rng.uniform(49.7,50.3); tol=rng.uniform(.92,1.08)
    active=rng.random(4)<.35; active[cls]=False
    elapsed=rng.uniform(.3,3,4)
    pre_v,pre_i=cycle(vrms,hz,active,elapsed,rng,0.8)
    post_active=active.copy(); post_active[cls]=True
    post_elapsed=elapsed.copy(); post_elapsed[cls]=rng.uniform(.0,.012)
    # Randomized appliance tolerance affects the event current, while background remains common.
    saved=IRMS[cls]; IRMS[cls]=saved*tol
    post_v,post_i=cycle(vrms,hz,post_active,post_elapsed,rng,0.8)
    peak_elapsed=post_elapsed.copy()
    peak_v,peak_i=cycle(vrms,hz,post_active,peak_elapsed,rng,.8)
    IRMS[cls]=saved
    pre,preh=measures(pre_v,pre_i); post,posth=measures(post_v,post_i); peak,_=measures(peak_v,peak_i)
    dp=post[2]-pre[2]; dq=post[3]-pre[3]; ds=post[4]-pre[4]
    di=max(post[1]-pre[1],.05)
    ratios=(posth-preh)/max(posth[0]-preh[0],.05)
    dthd=(np.linalg.norm(posth[1:])/max(posth[0],.05)-np.linalg.norm(preh[1:])/max(preh[0],.05))
    # Event envelope sampled at its first cycle; the inrush coefficient is part of the plant equation.
    inrush=KAPPA[cls]+rng.normal(0,.035)
    return np.array([abs(dp),dq,abs(ds),dp/max(np.hypot(dp,dq),1),ratios[1],ratios[2],ratios[3],dthd,inrush, np.sign(dq)*abs(dq)/max(abs(dp),1)],dtype=np.float32)

def softmax(x):
    e=np.exp(x-x.max(axis=1,keepdims=True)); return e/e.sum(axis=1,keepdims=True)

def train():
    rng=np.random.default_rng(SEED)
    xs=[]; ys=[]
    for c in range(4):
        for _ in range(400):
            xs.append(event_features(rng,c)); ys.append(c)
    x=np.asarray(xs,dtype=np.float32); y=np.asarray(ys,dtype=np.int64)
    order=rng.permutation(len(y)); x=x[order]; y=y[order]
    split=int(.8*len(y)); train_x, test_x=x[:split],x[split:]; train_y,test_y=y[:split],y[split:]
    mu=train_x.mean(axis=0); sigma=train_x.std(axis=0); sigma=np.maximum(sigma,1e-3)
    z=(train_x-mu)/sigma; ztest=(test_x-mu)/sigma
    rngw=np.random.default_rng(SEED)
    w1=(rngw.normal(size=(10,16))*np.sqrt(2/10)).astype(np.float32); b1=np.zeros(16,np.float32)
    w2=(rngw.normal(size=(16,4))*np.sqrt(2/16)).astype(np.float32); b2=np.zeros(4,np.float32)
    mw1=np.zeros_like(w1); vw1=mw1.copy(); mb1=b1.copy(); vb1=b1.copy(); mw2=w2.copy()*0; vw2=w2.copy()*0; mb2=b2.copy(); vb2=b2.copy()
    step=0
    for epoch in range(1400):
        perm=rng.permutation(len(train_y))
        for start in range(0,len(perm),64):
            ix=perm[start:start+64]; a=z[ix]; labels=train_y[ix]; step+=1
            h0=a@w1+b1; h=np.maximum(h0,0); logits=h@w2+b2; prob=softmax(logits)
            prob[np.arange(len(ix)),labels]-=1; prob/=len(ix)
            gw2=h.T@prob; gb2=prob.sum(axis=0); gh=prob@w2.T; gh[h0<=0]=0
            gw1=a.T@gh; gb1=gh.sum(axis=0)
            lr=.008 if epoch<900 else .003
            for param, grad, m, v in ((w1,gw1,mw1,vw1),(b1,gb1,mb1,vb1),(w2,gw2,mw2,vw2),(b2,gb2,mb2,vb2)):
                m*=.9; m+=.1*grad; v*=.999; v+=.001*grad*grad
                param-=lr*(m/(1-.9**step))/(np.sqrt(v/(1-.999**step))+1e-8)
    pred=np.argmax(softmax(np.maximum(ztest@w1+b1,0)@w2+b2),axis=1)
    accuracy=float(np.mean(pred==test_y))
    if accuracy < .95: raise RuntimeError(f'held-out accuracy {accuracy:.3%} is below 95%')
    train_z=(train_x-mu)/sigma
    centers=np.stack([train_z[train_y==c].mean(axis=0) for c in range(4)])
    scales=np.maximum(np.stack([train_z[train_y==c].std(axis=0) for c in range(4)]),.08)
    def arr(name, a):
        vals=', '.join(f'{float(v):.9g}f' for v in a.ravel())
        return f'static const float {name}[{a.size}] = {{{vals}}};\n'
    header='/* Generated by scripts/train_nilm.py; seed=42. */\n#ifndef NILM_WEIGHTS_H\n#define NILM_WEIGHTS_H\n'
    header+=f'#define GG_NILM_TRAIN_ACCURACY {accuracy:.6f}f\n'
    header+=arr('gg_nilm_feature_mean',mu)+arr('gg_nilm_feature_std',sigma)
    header+=arr('gg_nilm_w1',w1.T)+arr('gg_nilm_b1',b1)+arr('gg_nilm_w2',w2.T)+arr('gg_nilm_b2',b2)
    header+=arr('gg_nilm_centroid',centers)+arr('gg_nilm_class_std',scales)+'#endif\n'
    OUT.write_text(header)
    print(f'held-out accuracy: {accuracy:.2%}; generated {OUT}')

if __name__=='__main__': train()
