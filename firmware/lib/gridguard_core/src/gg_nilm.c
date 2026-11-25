#include "gg_nilm.h"
#include "nilm_weights.h"
#include <math.h>
#include <string.h>
void gg_cusum_init(gg_cusum_t *c){memset(c,0,sizeof(*c));}
int gg_cusum_step(gg_cusum_t *c,float p,float q){
 float e,gp,gm; if(!c)return 0;
 if(c->lockout){c->lockout--;c->baseline_p=p;c->baseline_q=q;c->positive=0;c->negative=0;return 0;}
 if(c->capture){c->capture--;if(!c->capture){c->baseline_p=p;c->baseline_q=q;c->lockout=90;return c->pending_event;}return 0;}
 if(!c->initialized){c->baseline_p=p;c->baseline_q=q;c->initialized=1;return 0;}
 e=(p-c->baseline_p)+.5f*(q-c->baseline_q);
 gp=fmaxf(0,c->positive+e-15);gm=fmaxf(0,c->negative-e-15);c->positive=gp;c->negative=gm;
 if(gp>45){c->pending_event=1;c->capture=25;c->positive=c->negative=0;return 0;}
 if(gm>45){c->pending_event=-1;c->capture=15;c->positive=c->negative=0;return 0;}
 c->baseline_p=.98f*c->baseline_p+.02f*p;c->baseline_q=.98f*c->baseline_q+.02f*q;return 0;
}
gg_nilm_result_t gg_nilm_infer(const float x[10]){
 gg_nilm_result_t r;float z[10],h[16],logit[4],sum=0;unsigned j,u,c;memset(&r,0,sizeof(r));
 for(j=0;j<10;j++)z[j]=(x[j]-gg_nilm_feature_mean[j])/gg_nilm_feature_std[j];
 for(u=0;u<16;u++){float v=gg_nilm_b1[u];for(j=0;j<10;j++)v+=gg_nilm_w1[u*10+j]*z[j];h[u]=fmaxf(v,0);}
 for(c=0;c<4;c++){logit[c]=gg_nilm_b2[c];for(u=0;u<16;u++)logit[c]+=gg_nilm_w2[c*16+u]*h[u];}
 {float mx=logit[0];for(c=1;c<4;c++)if(logit[c]>mx)mx=logit[c];for(c=0;c<4;c++){r.probability[c]=expf((logit[c]-mx)/2.5f);sum+=r.probability[c];}}
 r.class_id=0;for(c=0;c<4;c++){r.probability[c]/=sum;if(r.probability[c]>r.probability[r.class_id])r.class_id=c;}
 sum=0;for(j=0;j<10;j++){float d=(z[j]-gg_nilm_centroid[r.class_id*10+j])/(gg_nilm_class_std[r.class_id*10+j]+.05f);sum+=d*d;}
 r.distance=sqrtf(sum);if(r.probability[r.class_id]<.70f||r.distance>8.0f)r.class_id=GG_NILM_UNKNOWN;return r;
}
void gg_nilm_tracker_init(gg_nilm_tracker_t *t){memset(t,0,sizeof(*t));}
void gg_nilm_tracker_event(gg_nilm_tracker_t *t,const gg_nilm_result_t *r,int on,float measured,float dt){
 unsigned c;if(!t||!r)return;c=r->class_id;t->mahalanobis=r->distance;t->last_event_id=(uint8_t)(c<4?c:GG_NILM_UNKNOWN);t->last_event_type=(uint8_t)(on?1:2);
 if(c<4){t->state[c]=(uint8_t)(on!=0);if(on){t->confidence[c]=fmaxf(0.0f,fminf(0.99f,r->probability[c]*expf(-0.04f*r->distance)));t->est_power_w[c]=measured>0?measured:t->est_power_w[c];}else{t->confidence[c]=0.0f;t->est_power_w[c]=0.0f;}}
 for(c=0;c<4;c++)if(t->state[c])t->est_energy_wh[c]+=t->est_power_w[c]*dt/3600;
 {float known=0;for(c=0;c<4;c++)if(t->state[c])known+=t->est_power_w[c];t->unknown_power_w=fmaxf(0,measured-known);}
}
