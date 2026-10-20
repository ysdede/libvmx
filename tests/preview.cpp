#include "../src/vmxcodec.h"
#include <cstdio>
#include <cstdint>
#include <vector>

// Standalone regression: build with the three x86 codec sources and C++17
// ASan/UBSan. Optional argument compares pixels only against an older revision.
int main(int argc, char**) {
    constexpr int w=320, h=176;
    VMX_INSTANCE* vmx=VMX_Create({w,h},VMX_PROFILE_OMT_SQ,VMX_COLORSPACE_BT709);
    if(!vmx)return 1;
    std::vector<BYTE> pixels(w*h*4), wire(w*h*4), full(w*h*4), small(w/8*h/8*4), reference(small.size());
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        int i=(y*w+x)*4;
        pixels[i]=x;pixels[i+1]=y;pixels[i+2]=x+y;pixels[i+3]=128;
    }
    int fails=0;
    uint64_t hash=1469598103934665603ull;
    for(int shift=0;shift<=3;++shift){
        int lo,hi,q,oldShift;
        VMX_GetEncodingParameters(vmx,&lo,&hi,&q,&oldShift);
        VMX_SetEncodingParameters(vmx,lo,hi,q,shift);
        if(VMX_EncodeBGRA(vmx,pixels.data(),w*4,0)!=VMX_ERR_OK){++fails;continue;}
        int n=VMX_SaveTo(vmx,wire.data(),static_cast<int>(wire.size()));
        int preview=VMX_GetEncodedPreviewLength(vmx);
        if(n<=0||preview<=0||preview>=n){++fails;continue;}
        if(VMX_LoadFrom(vmx,wire.data(),n)!=VMX_ERR_OK ||
           VMX_DecodeBGRA(vmx,full.data(),w*4)!=VMX_ERR_OK ||
           VMX_DecodePreviewBGRA(vmx,reference.data(),w/8*4)!=VMX_ERR_OK ||
           VMX_LoadFrom(vmx,wire.data(),preview)!=VMX_ERR_OK ||
           VMX_DecodePreviewBGRA(vmx,small.data(),w/8*4)!=VMX_ERR_OK || small!=reference)++fails;
        for(BYTE b:full){hash^=b;hash*=1099511628211ull;}
        for(BYTE b:small){hash^=b;hash*=1099511628211ull;}
        if(argc==1&&shift>0){
            BYTE saved=wire[1];wire[1]=255;
            if(VMX_LoadFrom(vmx,wire.data(),n)!=VMX_ERR_INVALID_PARAMETERS)++fails;
            wire[1]=saved;
        }
    }
    if(argc==1){
        if(VMX_LoadFrom(vmx,wire.data(),-1)!=VMX_ERR_INVALID_PARAMETERS)++fails;
        if(VMX_LoadFrom(vmx,wire.data(),0)!=VMX_ERR_INVALID_PARAMETERS)++fails;
        int lo,hi,q,shift;
        VMX_GetEncodingParameters(vmx,&lo,&hi,&q,&shift);
        VMX_SetEncodingParameters(vmx,lo,hi,q,-1);
        VMX_GetEncodingParameters(vmx,&lo,&hi,&q,&shift);if(shift!=0)++fails;
        VMX_SetEncodingParameters(vmx,lo,hi,q,255);
        VMX_GetEncodingParameters(vmx,&lo,&hi,&q,&shift);if(shift!=15)++fails;
    }
    VMX_Destroy(vmx);
    std::printf("pixel-hash=%016llx failures=%d\n",static_cast<unsigned long long>(hash),fails);
    return fails?1:0;
}
