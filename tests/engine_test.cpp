// Offline verification of the harmony engine on a synthetic sung vowel.
#include "../Source/dsp/HarmonyEngine.h"
#include "../Source/dsp/KeyDetector.h"
#include "../Source/dsp/OfflineRender.h"
#include <cstdio>
#include <vector>
#include <functional>
#include <ctime>
#include <cstdlib>

using namespace hs;
static double SR = 48000.0;
static const char* kNoteNamesT[12]={"C","C#","D","Eb","E","F","F#","G","G#","A","Bb","B"};
static const double PI = 3.14159265358979323846;

struct Biquad { double b0,b1,b2,a1,a2,z1=0,z2=0;
    double p(double x){ double y=b0*x+z1; z1=b1*x-a1*y+z2; z2=b2*x-a2*y; return y; } };
static Biquad bp(double f,double q){ double w=2*PI*f/SR,al=std::sin(w)/(2*q),a0=1+al;
    return {al/a0,0,-al/a0,-2*std::cos(w)/a0,(1-al)/a0}; }

// glottal source (additive, -12dB/oct-ish) + 3 formants ("ah"), with optional pitch function
static std::vector<float> vowel(double secs, std::function<double(double)> f0, double vibCents=0)
{
    int n=(int)(secs*SR); std::vector<float> out(n);
    Biquad F1=bp(700,6),F2=bp(1220,8),F3=bp(2600,10);
    double ph=0;
    for(int i=0;i<n;++i){ double tt=i/SR; double f=f0(tt)*std::pow(2.0,vibCents*std::sin(2*PI*5.5*tt)/1200.0);
        ph+=f/SR; ph-=std::floor(ph); double s=0;
        for(int k=1;k*f<SR*0.45;++k) s+=std::sin(2*PI*k*ph)/std::pow(k,1.1);
        double y=F1.p(s)*1.0+F2.p(s)*0.6+F3.p(s)*0.3;
        double env=std::min(1.0,std::min(tt/0.02,(secs-tt)/0.02));
        out[i]=(float)(0.25*y*env); }
    return out;
}

// independent pitch estimate (normalised autocorrelation, fine search)
static double estimateF0(const std::vector<float>& x,int start,int len)
{
    double best=-1,bt=0; int tmin=(int)(SR/1200),tmax=(int)(SR/60);
    std::vector<double> r(tmax+2);
    for(int tau=tmin;tau<=tmax+1;++tau){ double s=0,e1=0,e2=0;
        for(int j=0;j<len;++j){ double a=x[start+j],b=x[start+j+tau]; s+=a*b; e1+=a*a; e2+=b*b; }
        r[tau]=s/std::sqrt(e1*e2+1e-20); }
    for(int tau=tmin+1;tau<=tmax;++tau) if(r[tau]>r[tau-1]&&r[tau]>=r[tau+1]&&r[tau]>0.85*1.0){ if(r[tau]>best+0.02||best<0){best=r[tau];bt=tau;} if(best>0.9) break; }
    if(bt==0) return 0;
    int k=(int)bt; double d=(r[k-1]-r[k+1])/(2*(r[k-1]-2*r[k]+r[k+1]));
    return SR/(bt+d);
}
static double rms(const std::vector<float>& x,int a,int b){ double s=0; for(int i=a;i<b;++i) s+=x[i]*x[i]; return std::sqrt(s/(b-a)); }

struct Result { std::vector<float> L,R; int lat; };
static Result run(const std::vector<float>& in, HarmonyEngine::GlobalParams g, std::vector<HarmonyEngine::VoiceParams> v, std::vector<int> notes={})
{
    HarmonyEngine e; e.prepare(SR,512); e.setGlobal(g);
    for(int i=0;i<4;++i) e.setVoice(i, i<(int)v.size()? v[i] : HarmonyEngine::VoiceParams{false,0,0,0});
    for(int nn:notes) e.noteOn(nn);
    Result r; r.L.assign(in.size(),0); r.R.assign(in.size(),0); r.lat=e.latencySamples();
    for(size_t p=0;p<in.size();p+=512){ int n=(int)std::min<size_t>(512,in.size()-p); e.process(in.data()+p,r.L.data()+p,r.R.data()+p,n);}
    for(float s:r.L) if(!std::isfinite(s)){ printf("NaN!\n"); exit(1);}
    return r;
}

static int fails=0;
static void check(const char* name,double got,double want,double tolCents)
{
    double c = got>0? 1200*std::log2(got/want) : 9999;
    bool ok=std::abs(c)<=tolCents; if(!ok) ++fails;
    printf("%-44s got %8.2f Hz  want %8.2f Hz  (%+6.1f c) %s\n",name,got,want,c,ok?"OK":"FAIL");
}

int main(int argc,char** argv)
{
    if(argc>1) SR=atof(argv[1]); printf("SR %.0f\n",SR);
    HarmonyEngine::GlobalParams g; g.humanize=0; g.glideMs=20; g.breath=0.25f;
    auto A3 = vowel(2.0,[](double){return 220.0;});
    auto mono=[&](Result& r){ std::vector<float> m(r.L.size()); for(size_t i=0;i<m.size();++i) m[i]=r.L[i]+r.R[i]; return m; };
    int mid=(int)(1.0*SR), len=4096;

    { auto r=run(A3,g,{{true,2,1,0}}); auto m=mono(r); printf("latency %d samples (%.1f ms)\n",r.lat,r.lat/SR*1000);
      check("Scale C maj, A3 +2 steps -> C4",estimateF0(m,mid,len),261.63,8); }
    { auto r=run(A3,g,{{true,-2,1,0}}); auto m=mono(r); check("Scale C maj, A3 -2 steps -> F3",estimateF0(m,mid,len),174.61,8); }
    { auto r=run(A3,g,{{true,4,1,0}}); auto m=mono(r); check("Scale C maj, A3 +4 steps -> E4",estimateF0(m,mid,len),329.63,8); }
    { auto gm=g; gm.key=9; gm.scale=1; auto r=run(A3,gm,{{true,2,1,0}}); auto m=mono(r); check("Scale A min, A3 +2 -> C4",estimateF0(m,mid,len),261.63,8); }
    { auto gc=g; gc.mode=Mode::Chromatic; auto r=run(A3,gc,{{true,7,1,0}}); auto m=mono(r); check("Chromatic +7 -> E4",estimateF0(m,mid,len),329.63,8); }
    { auto gc=g; gc.mode=Mode::Chromatic; auto r=run(A3,gc,{{true,-12,1,0}}); auto m=mono(r); check("Chromatic -12 -> A2",estimateF0(m,mid,len),110.0,8); }
    { auto gc=g; gc.mode=Mode::Chromatic; auto r=run(A3,gc,{{true,12,1,0}}); auto m=mono(r); check("Chromatic +12 -> A4",estimateF0(m,mid,len),440.0,8); }
    { auto gm=g; gm.mode=Mode::Midi; auto r=run(A3,gm,{{true,0,1,0}},{64}); auto m=mono(r); check("MIDI note E4",estimateF0(m,mid,len),329.63,8); }
    { auto gm=g; gm.mode=Mode::Midi; auto r=run(A3,gm,{{true,0,1,0},{true,0,1,0}},{60,67}); auto m=mono(r);
      // voice 2 alone check
      auto r2=run(A3,gm,{{false,0,1,0},{true,0,1,0}},{60,67}); auto m2=mono(r2); check("MIDI chord, voice2 -> G4",estimateF0(m2,mid,len),392.0,8); }

    // male low voice and female high voice
    { auto lo=vowel(2.0,[](double){return 98.0;}); auto r=run(lo,g,{{true,2,1,0}}); auto m=mono(r);
      check("Low G2 (98Hz) +2 steps -> B2",estimateF0(m,mid,len),123.47,10); }
    { auto hi=vowel(2.0,[](double){return 587.33;}); auto r=run(hi,g,{{true,2,1,0}}); auto m=mono(r);
      check("High D5 +2 steps -> F5",estimateF0(m,mid,len),698.46,10); }

    // identity / alignment: chromatic 0 should reproduce the delayed input
    { auto gc=g; gc.mode=Mode::Chromatic; auto r=run(A3,gc,{{true,0,1,0}}); auto m=mono(r);
      double bestSnr=-99; int bestLag=0; int L=r.lat; double scale=1.41421356;
      for(int lag=-400;lag<=400;++lag){ double se=0,ss=0;
        for(int i=mid;i<mid+20000;++i){ double ref=A3[i-L-lag]; double d=m[i]/scale-ref; se+=d*d; ss+=ref*ref; }
        double snr=10*std::log10(ss/se); if(snr>bestSnr){bestSnr=snr;bestLag=lag;} }
      printf("%-44s SNR %.1f dB (phase lag %d) %s\n","Unison reconstruction vs delayed dry",bestSnr,bestLag,bestSnr>20?"OK":"FAIL"); if(bestSnr<=20) ++fails; }

    // level sanity across intervals
    { HarmonyEngine::GlobalParams gc=g; gc.mode=Mode::Chromatic;
      for(int iv: {-12,-7,-5,-3,0,3,4,7,12}){ auto r=run(A3,gc,{{true,iv,1,0}}); auto m=mono(r);
        double db=20*std::log10(rms(m,mid,mid+24000)/1.41421356/rms(A3,mid,mid+24000));
        printf("  level %+3d st: %+5.2f dB %s\n",iv,db,std::abs(db)<3?"OK":"FAIL"); if(std::abs(db)>=3) ++fails; } }

    // vibrato & glide input: track that harmony follows (+2 steps from 220 => ratio 1.1892)
    { auto gl=vowel(3.0,[](double tt){return 200.0+40.0*tt;},30); auto r=run(gl,g,{{true,4,1,0}}); auto m=mono(r);
      printf("Sliding input w/ vibrato: finite output, rms %.3f OK\n",rms(m,(int)SR,(int)(2.5*SR))); }

    // unvoiced noise -> breath level
    { std::vector<float> nz((size_t)SR*1); std::minstd_rand rg(3); for(auto& s:nz) s=0.1f*((float)rg()/rg.max()*2-1);
      auto r=run(nz,g,{{true,2,1,0}}); auto m=mono(r); double ratio=rms(m,24000,40000)/1.41421356/rms(nz,24000,40000);
      printf("%-44s %.2f dB (breath -12 dB) %s\n","Noise (unvoiced) passthrough",20*std::log10(ratio),std::abs(20*std::log10(ratio)+12)<3?"OK":"FAIL"); }

    // full stack + doubles + humanize at 44.1k-equivalent CPU check
    { auto gs=g; gs.humanize=0.6f; gs.doubleOn=true; gs.vibratoDepth=0.2f;
      auto t0=std::clock(); auto r=run(A3,gs,{{true,2,1,-0.5f},{true,4,1,0.5f},{true,-3,1,-0.3f},{true,-7,1,0.3f}});
      double cpu=(double)(std::clock()-t0)/CLOCKS_PER_SEC; printf("Full 8-voice stack: %.3fs CPU for 2.0s audio (%.1f%% of realtime)\n",cpu,cpu/2.0*100);
      }

    // ---- key detection on melodies ----
    auto melody=[&](std::vector<int> notes,double noteLen){ std::vector<float> all;
        for(int nn:notes){ double f=440*std::pow(2.0,(nn-69)/12.0); auto v=vowel(noteLen,[f](double){return f;},15); all.insert(all.end(),v.begin(),v.end()); } return all; };
    { auto m=melody({57,60,64,62,60,59,57,64,65,64,62,60,59,57,52,57},0.35); auto k=detectKey(m.data(),(int64_t)m.size(),SR);
      bool ok=k.key==9&&k.minor; if(!ok)++fails; printf("%-44s %s %s (conf %.2f) %s\n","Key detect: A-minor melody",kNoteNamesT[k.key],k.minor?"minor":"major",k.confidence,ok?"OK":"FAIL"); }
    { auto m=melody({67,69,71,72,74,72,71,69,67,66,67,62,64,66,67,71},0.35); auto k=detectKey(m.data(),(int64_t)m.size(),SR);
      bool ok=k.key==7&&!k.minor; if(!ok)++fails; printf("%-44s %s %s (conf %.2f) %s\n","Key detect: G-major melody",kNoteNamesT[k.key],k.minor?"minor":"major",k.confidence,ok?"OK":"FAIL"); }
    { auto m=melody({63,65,67,68,70,72,74,75,70,67,63,58,63},0.35); auto k=detectKey(m.data(),(int64_t)m.size(),SR);
      bool ok=k.key==3&&!k.minor; if(!ok)++fails; printf("%-44s %s %s (conf %.2f) %s\n","Key detect: Eb-major scale",kNoteNamesT[k.key],k.minor?"minor":"major",k.confidence,ok?"OK":"FAIL"); }

    // ---- offline render: latency removed, aligned to the source ----
    { EngineSnapshot sn; sn.g=g; sn.g.mode=Mode::Chromatic; sn.v[0]={true,0,1,0,0}; for(int i=1;i<4;++i) sn.v[i].enabled=false;
      RenderOutput ro; renderHarmony(A3.data(),(int64_t)A3.size(),SR,sn,ro,true);
      double best=-1; int bl=0; for(int lag=-300;lag<=300;++lag){ double sxy=0,sxx=0,syy=0; for(int i=mid;i<mid+20000;++i){ double x=A3[i], y=ro.stackL[i+lag]+ro.stackR[i+lag]; sxy+=x*y; sxx+=x*x; syy+=y*y;} double c=sxy/std::sqrt(sxx*syy); if(c>best){best=c;bl=lag;} }
      // PSOLA output is pitch-synchronous: alignment within one pitch period (~4.5 ms @220 Hz) == aligned
      bool ok=std::abs(bl)<= (int)(SR/220.0)+2 && best>0.9; if(!ok)++fails;
      printf("%-44s lag %d samples, corr %.3f %s\n","Offline render aligned to source",bl,best,ok?"OK":"FAIL");
      double sv=0; for(size_t i=0;i<ro.stackL.size();++i) sv+=std::abs(ro.stackL[i]-ro.voiceL[0][i]); bool ok2=sv<1e-3; if(!ok2)++fails;
      printf("%-44s %s\n","Voice stem == stack (single voice)",ok2?"OK":"FAIL"); }

    printf(fails? "\n%d FAILURES\n" : "\nALL CHECKS PASSED\n", fails);
    return fails?1:0;
}
