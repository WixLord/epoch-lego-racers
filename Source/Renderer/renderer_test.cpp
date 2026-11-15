#include "renderer_api.h"
#include "image_scale.h"
#include <windows.h>
#include <cstdio>
#include <vector>
#include <cstdlib>
#include "font_raster.h"

extern "C" __declspec(dllimport) const RBApi* RBGetApi(uint32_t);

int main() {
    if(GetEnvironmentVariableA("REBORN_TEST_FONT",nullptr,0)) {
        uint16_t measured[]={L'I',1,L'W',1,L'A',1};
        if(!MeasureFont(measured,3,28) || measured[1]>=measured[3] || measured[5]<8)return 106;
        printf("Private font: measured proportional character widths PASS\n");
        uint16_t glyphs[]={L'A',3,3,24,24,L'1',33,3,20,24};
        std::vector<uint32_t> glyphPixels;
        if(!RasterizeFont(128,64,glyphs,2,glyphPixels))return 100;
        unsigned white=0,black=0,edgePixels=0;
        for(auto p:glyphPixels){if((p>>24)==255){if((p&0xffffff)==0xffffff)++white;if(!(p&0xffffff))++black;}
            else if(p>>24)++edgePixels;}
        if(!white || !black || !edgePixels || (glyphPixels[0]>>24))return 101;
        printf("Private font: fill, outline, antialiasing and transparent padding PASS\n");
        uint16_t pairs[]={L'A',3,3,16,28,L'A',30,3,28,28};
        if(!RasterizeFont(128,72,pairs,2,glyphPixels))return 104;
        int left[2]={128,128},right[2]={-1,-1},top[2]={72,72},bottom[2]={-1,-1};
        for(int y=0;y<72;++y)for(int x=0;x<128;++x)if((glyphPixels[y*128+x]>>24)>127) {
            int i=x<50?0:1;left[i]=std::min(left[i],x);right[i]=std::max(right[i],x);top[i]=std::min(top[i],y);bottom[i]=std::max(bottom[i],y);
        }
        if(right[0]<0 || right[1]<0 || abs((right[0]-left[0])-(right[1]-left[1]))>1 || abs((bottom[0]-top[0])-(bottom[1]-top[1]))>1)return 105;
        printf("Font proportions independent of legacy cell width PASS\n");
    }
    auto empty=ReconstructContours({0x00ff00ff},1,1);
    for(auto p:empty)if(p>>24)return 91;
    auto solid=ReconstructContours({0xffffffff},1,1);
    for(auto p:solid)if(p!=0xffffffff)return 92;
    std::vector<uint32_t> stroke(49,0);for(unsigned y=0;y<7;++y)stroke[y*7+3]=0xffffffff;
    auto clean=ReconstructContours(stroke,7,7);
    if((clean[7*14+7]>>24)<240 || (clean[7*14+2]>>24)!=0)return 93;
    std::vector<uint32_t> diagonal(49,0);for(unsigned y=0;y<7;++y)for(unsigned x=0;x<=y;++x)diagonal[y*7+x]=0xffffffff;
    auto edge=ReconstructContours(diagonal,7,7);
    unsigned partial=0;for(auto p:edge)if((p>>24)>0 && (p>>24)<255)++partial;
    if(!partial)return 94;
    // Inactive arrows contain dark yellow in the asset itself. A font's
    // black/white threshold must never crush their source colors to black.
    auto dimArrow=ReconstructContours({0xff004040},1,1,2,true);
    for(auto p:dimArrow)if(p!=0xff004040)return 95;
    auto brightArrow=ReconstructContours({0xff00ffff},1,1,2,true);
    for(auto p:brightArrow)if(p!=0xff00ffff)return 96;
    auto checkmark=ReconstructContours({0xff008000},1,1,4,true);
    if(checkmark.size()!=16)return 97;
    for(auto p:checkmark)if(p!=0xff008000)return 98;
    auto nativeText=ReconstructContours(stroke,7,7,1);
    if(nativeText.size()!=49 || (nativeText[3*7+3]>>24)<240)return 99;
    // Preserve isolated strokes, but round a diagonal corner inside its original cell.
    auto isolated=ScaleEdges2x({0,0,0,0,0xffffffff,0,0,0,0},3,3);
    unsigned lit=0;for(auto p:isolated)if(p)++lit;
    auto corner=ScaleEdges2x({0,0xffffffff,0,0xffffffff,0,0,0,0,0},3,3);
    if(lit!=4 || corner[14]!=0xffffffff || corner[15]!=0 ||
       ScaleEdges2x({0xff123456},1,1)!=std::vector<uint32_t>(4,0xff123456))return 90;
    HWND window = CreateWindowExA(0, "STATIC", "Renderer validation", WS_OVERLAPPEDWINDOW,
        0, 0, 320, 240, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!window) return 1;
    RECT rect; GetClientRect(window, &rect);
    const RBApi* api = RBGetApi(8);
    void* context = api ? api->create(window, rect.right, rect.bottom) : nullptr;
    if (!context) { DestroyWindow(window); return 2; }
    bool ok = true;
    if(GetEnvironmentVariableA("REBORN_TEST_FONT",nullptr,0)) {
        uint16_t glyphs[]={L'A',3,3,24,24};
        if(!api->uploadFont(context,987654,64,32,glyphs,1) || !api->hasTexture(context,987654))return 102;
        uint16_t outside[]={L'A',63,3,24,24};
        if(api->uploadFont(context,987655,64,32,outside,1))return 103;
    }
    HWND output=FindWindowExA(window,nullptr,"STATIC","Reborn D3D11 output");
    ok=output && !IsWindowEnabled(output) && (GetWindowLongPtrA(window,GWL_STYLE)&WS_CLIPCHILDREN);
    printf("isolated output and parent input: %s\n",ok?"PASS":"FAIL");
    RBState state{};
    state.depthEnable = state.depthWrite = 1; state.depthFunc = 4;
    state.srcBlend = 5; state.dstBlend = 6; state.cull = 1;
    state.colorOp = state.alphaOp = 2;
    state.colorArg1 = state.alphaArg1 = 0;
    state.addressU = state.addressV = 3;
    auto quad = [&](float z, uint32_t color, uint64_t texture = 0) {
        RBVertex v[] = {{-0.5f,-0.5f,z,1,color,0xff000000,0,0},
            {float(rect.right)-0.5f,-0.5f,z,1,color,0xff000000,1,0},
            {float(rect.right)-0.5f,float(rect.bottom)-0.5f,z,1,color,0xff000000,1,1},
            {-0.5f,float(rect.bottom)-0.5f,z,1,color,0xff000000,0,1}};
        ok = api->draw(context,6,v,4,nullptr,0,&state,texture) && ok;
    };
    auto check = [&](const char* name, int r, int g, int b, int tolerance=3) {
        ok = api->capture(context,"renderer-test.ppm") && ok;
        FILE* file = nullptr; fopen_s(&file,"renderer-test.ppm","rb");
        int w=0,h=0,max=0; char magic[3]{};
        if (!file) { ok=false; return; }
        if(fscanf_s(file,"%2s %d %d %d",magic,3,&w,&h,&max)!=4 || w!=rect.right || h!=rect.bottom) { fclose(file); ok=false; return; }
        fgetc(file);
        std::vector<unsigned char> pixels(size_t(w)*h*3);
        bool read=fread(pixels.data(),1,pixels.size(),file)==pixels.size(); fclose(file);
        size_t p=(size_t(h/2)*w+w/2)*3;
        bool pass=read && abs(int(pixels[p])-r)<=tolerance && abs(int(pixels[p+1])-g)<=tolerance && abs(int(pixels[p+2])-b)<=tolerance;
        printf("%s: %s (%u,%u,%u)\n",name,pass?"PASS":"FAIL",pixels[p],pixels[p+1],pixels[p+2]);
        ok=ok&&pass;
    };
    ok=api->begin(context,rect.right,rect.bottom,3,0)!=0;
    quad(.25f,0xffff0000); quad(.75f,0xff00ff00);
    check("depth occlusion",255,0,0);
    state.depthEnable=state.depthWrite=0; state.blendEnable=1;
    quad(.1f,0x800000ff); check("alpha blend",127,0,128);
    state.blendEnable=0; state.colorArg1=state.alphaArg1=2;
    uint16_t green=0x07e0;
    RBTexture texture{}; texture.id=1;texture.width=texture.height=1;texture.pitch=2;texture.bits=16;
    texture.redMask=0xf800;texture.greenMask=0x7e0;texture.blueMask=0x1f;texture.pixels=&green;
    ok=api->uploadTexture(context,&texture)&&ok;
    float representative[3]={};
    bool colorCorrect=api->textureColor(context,1,representative) && representative[0]==0 && representative[1]==1 && representative[2]==0;
    printf("texture-derived light color: %s\n",colorCorrect?"PASS":"FAIL");ok=ok&&colorCorrect;
    quad(.1f,0xffffffff,1);check("RGB565 upload",0,255,0);
    texture.id=2;texture.colorKeyEnabled=1;texture.colorKeyRGB=0x00ff00;
    ok=api->uploadTexture(context,&texture)&&ok;
    api->begin(context,rect.right,rect.bottom,3,0xff0000);
    state.colorKey=1;quad(.1f,0xffffffff,2);check("color key",255,0,0);
    state.colorKey=0;state.colorArg1=state.alphaArg1=0;
    state.alphaTest=1;state.alphaFunc=5;state.alphaRef=128;
    quad(.1f,0x4000ff00);check("alpha rejection",255,0,0);
    ok=api->present(context,0)&&ok;
    SetWindowPos(window,nullptr,0,0,512,192,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
    GetClientRect(window,&rect);
    state.alphaTest=0;
    ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
    quad(.2f,0xff00ff00);check("swapchain resize",0,255,0);
    for(unsigned samples : {1u,2u,4u,8u}) {
        ok=api->configure(context,samples,16,0)&&ok;
        ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
        state.depthEnable=state.depthWrite=1;state.depthFunc=4;
        quad(.6f,0xffffffff);
        ok=api->bloom(context,.6f)&&ok;
        check("MSAA resolve + bloom",255,255,255);
        float projection[16]={.01f,0,0,0,0,.01f,0,0,0,0,.01f,0,0,0,0,1};
        float eye[3]={0,0,-10},direction[3]={0,0,1};
        ok=api->beginLighting(context,projection,eye,direction,200)&&ok;
        float a[3]={-20,-20,30},b[3]={20,-20,30},c[3]={20,20,30},d[3]={-20,20,30};
        api->addTriangle(context,a,b,c);api->addTriangle(context,a,c,d);
        ok=api->lighting(context,.5f,0)&&ok;
        check("shadow map receiver",128,128,128);
        uint32_t maskPixels[]={0x00ffffff,0xffffffff};
        RBTexture mask{};mask.id=10;mask.width=2;mask.height=1;mask.pitch=8;mask.bits=32;
        mask.redMask=0xff;mask.greenMask=0xff00;mask.blueMask=0xff0000;mask.alphaMask=0xff000000;mask.pixels=maskPixels;
        ok=api->uploadTexture(context,&mask)&&ok;
        for(float u : {.25f,.75f}) {
            ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
            quad(.6f,0xffffffff);
            ok=api->beginLighting(context,projection,eye,direction,200)&&ok;
            RBShadowVertex cutout[]={{-20,-20,30,u,.5f},{20,-20,30,u,.5f},{20,20,30,u,.5f},
                {-20,-20,30,u,.5f},{20,20,30,u,.5f},{-20,20,30,u,.5f}};
            api->addMaskedTriangle(context,cutout,10);api->addMaskedTriangle(context,cutout+3,10);
            ok=api->lighting(context,.5f,0)&&ok;
            int value=u<.5f?255:128;
            check(u<.5f?"masked shadow hole":"masked shadow covered texel",value,value,value);
        }
        // A sloped surface casting onto itself must remain lit. This catches
        // the dark bands caused by comparing neighboring shadow texels against
        // an unadjusted receiver depth.
        ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
        RBVertex slope[]={{-.5f,-.5f,.1f,1,0xffffffff,0,0,0},
            {float(rect.right)-.5f,-.5f,.7f,1,0xffffffff,0,0,0},
            {float(rect.right)-.5f,float(rect.bottom)-.5f,.7f,1,0xffffffff,0,0,0},
            {-.5f,float(rect.bottom)-.5f,.1f,1,0xffffffff,0,0,0}};
        ok=api->draw(context,6,slope,4,nullptr,0,&state,0)&&ok;
        ok=api->beginLighting(context,projection,eye,direction,200)&&ok;
        float sa[3]={-100,-100,10},sb[3]={100,-100,70},sc[3]={100,100,70},sd[3]={-100,100,10};
        api->addTriangle(context,sa,sb,sc);api->addTriangle(context,sa,sc,sd);
        ok=api->lighting(context,.8f,1)&&ok;
        check("sloped surface without false self-shadow",255,255,255);
        ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
        quad(.6f,0xffffffff);
        ok=api->lighting(context,0,1)&&ok;
        check("AO leaves flat plane unchanged",255,255,255);
        // A convex ridge cannot occlude itself. Exercise both camera basis
        // handednesses: screen-space cross products alone change sign.
        for(float verticalScale : {.01f,-.01f}) {
            projection[5]=verticalScale;
            ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
            float middle=float(rect.right/2)+1;
            RBVertex ridge[]={{-.5f,-.5f,.9f,1,0xffffffff,0,0,0},
                {middle,-.5f,.4f,1,0xffffffff,0,0,0},
                {middle,float(rect.bottom),.4f,1,0xffffffff,0,0,0},
                {-.5f,float(rect.bottom),.9f,1,0xffffffff,0,0,0},
                {float(rect.right),-.5f,.9f,1,0xffffffff,0,0,0},
                {float(rect.right),float(rect.bottom),.9f,1,0xffffffff,0,0,0}};
            uint16_t faces[]={0,1,2,0,2,3,1,4,5,1,5,2};
            ok=api->draw(context,4,ridge,6,faces,12,&state,0)&&ok;
            ok=api->beginLighting(context,projection,eye,direction,400)&&ok;
            ok=api->lighting(context,0,1.6f)&&ok;
            check(verticalScale>0?"AO convex ridge":"AO reflected camera convex ridge",255,255,255);
        }
        projection[5]=.01f;
        ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
        quad(.6f,0xffffffff);
        // Raised surface immediately beside a receiver must add contact shade.
        RBVertex raised[]={{-.5f,-.5f,.592f,1,0xffffffff,0,0,0},
            {float(rect.right/2)-2,-.5f,.592f,1,0xffffffff,0,0,0},
            {float(rect.right/2)-2,float(rect.bottom),.592f,1,0xffffffff,0,0,0},
            {-.5f,float(rect.bottom),.592f,1,0xffffffff,0,0,0}};
        ok=api->draw(context,6,raised,4,nullptr,0,&state,0)&&ok;
        ok=api->beginLighting(context,projection,eye,direction,400)&&ok;
        ok=api->lighting(context,0,1.6f)&&ok;
        check("AO contact shading",240,240,240,14);
        ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;
        quad(.6f,0xff404040);
        ok=api->beginLighting(context,projection,eye,direction,400)&&ok;
        RBPointLight local={0,0,55,10,1,0,0,.5f};
        api->addPointLight(context,&local);ok=api->pointLights(context)&&ok;
        check("local light color and smooth falloff",82,64,64,3);
        ok=api->begin(context,rect.right,rect.bottom,3,0)&&ok;quad(.6f,0xff404040);
        ok=api->beginLighting(context,projection,eye,direction,400)&&ok;
        ok=api->pointLights(context)&&ok;check("local lights reset each view",64,64,64);
        local.z=30;api->addPointLight(context,&local);ok=api->pointLights(context)&&ok;
        check("outside light radius unchanged",64,64,64);
        local.z=65;api->addPointLight(context,&local);ok=api->pointLights(context)&&ok;
        check("light behind surface rejected",64,64,64);
        ok=api->begin(context,rect.right,rect.bottom,3,0x404040)&&ok;
        local.z=55;api->addPointLight(context,&local);ok=api->pointLights(context)&&ok;
        check("sky excluded from local lighting",64,64,64);
        state.depthEnable=0;
        quad(.1f,0xffff0000);
        check("HUD after effects",255,0,0);
        api->begin(context,rect.right,rect.bottom,3,0);
        ok=api->draw(context,6,raised,4,nullptr,0,&state,0)&&ok;
        ok=api->bloom(context,1)&&ok;
        check("soft bloom outside bright geometry",30,30,30,29);
    }
    RECT childRect{};GetClientRect(output,&childRect);
    ok=ok && childRect.right==rect.right && childRect.bottom==rect.bottom;
    api->destroy(context);
    bool cleaned=!IsWindow(output) && !(GetWindowLongPtrA(window,GWL_STYLE)&WS_CLIPCHILDREN);
    printf("output cleanup: %s\n",cleaned?"PASS":"FAIL");ok=ok&&cleaned;
    DestroyWindow(window);
    remove("renderer-test.ppm");
    return ok?0:3;
}
