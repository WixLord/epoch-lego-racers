#include "renderer_api.h"
#include "image_scale.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <DirectXMath.h>
#include "font_raster.h"
using Microsoft::WRL::ComPtr;

namespace {
void Log(const char* message, HRESULT code = S_OK) {
    char path[MAX_PATH];
    if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) return;
    char* end = strrchr(path, '\\');
    if (!end || end - path > MAX_PATH - 30) return;
    strcpy_s(end + 1, MAX_PATH - (end + 1 - path), "Reborn-D3D11.log");
    FILE* file = nullptr;
    fopen_s(&file, path, "a");
    if (file) { fprintf(file, "%s (0x%08lx)\n", message, code); fclose(file); }
}
#include "postprocess.h"
#include "shadowmap.h"
const char* Shader = R"(
cbuffer Parameters : register(b0) {
    float4 screen;
    uint4 colorStage;
    uint4 alphaStage;
    uint4 options;
    float4 fog;
};
Texture2D image : register(t0);
SamplerState imageSampler : register(s0);
struct Input { float4 position : POSITION; float4 color : COLOR0; float4 specular : COLOR1; float2 uv : TEXCOORD0; };
struct Pixel { float4 position : SV_Position; float4 color : COLOR0; float4 specular : COLOR1; float2 uv : TEXCOORD0; };
Pixel VS(Input v) {
    Pixel p;
    float w = 1.0 / max(v.position.w, 1e-10);
    // D3D6 pixels are centered at integers; D3D11 centers are half-integers.
    float2 ndc = float2((v.position.x + 0.5) * 2 / screen.x - 1, 1 - (v.position.y + 0.5) * 2 / screen.y);
    p.position = float4(ndc * w, saturate(v.position.z) * w, w);
    p.color = v.color.bgra; p.specular = v.specular.bgra; p.uv = v.uv;
    return p;
}
float4 Argument(uint a, float4 diffuse, float4 texel, float4 specular) {
    uint base = a & 15;
    float4 result = base == 2 ? texel : (base == 4 ? specular : diffuse);
    if (a & 32) result = result.aaaa;
    if (a & 16) result = 1 - result;
    return result;
}
float4 Operation(uint op, float4 a, float4 b, float4 d, float4 t) {
    if (op == 2) return a;
    if (op == 3) return b;
    if (op == 5) return a*b*2;
    if (op == 6) return a*b*4;
    if (op == 7) return a+b;
    if (op == 8) return a+b-0.5;
    if (op == 10) return a-b;
    if (op == 12) return lerp(b,a,d.a);
    if (op == 13 || op == 15) return lerp(b,a,t.a);
    return a*b;
}
float4 PS(Pixel p) : SV_Target {
    float4 texel = options.x ? image.Sample(imageSampler,p.uv) : float4(1,1,1,1);
    if (options.w && texel.a < 0.5) discard;
    float4 color = Operation(colorStage.x,Argument(colorStage.y,p.color,texel,p.specular),Argument(colorStage.z,p.color,texel,p.specular),p.color,texel);
    color.a = Operation(alphaStage.x,Argument(alphaStage.y,p.color,texel,p.specular),Argument(alphaStage.z,p.color,texel,p.specular),p.color,texel).a;
    if (options.y) {
        float ref = screen.z;
        uint compare = options.z;
        bool accepted = compare == 8 || (compare == 2 && color.a < ref) || (compare == 3 && abs(color.a-ref) < 0.002) ||
            (compare == 4 && color.a <= ref) || (compare == 5 && color.a > ref) ||
            (compare == 6 && abs(color.a-ref) >= 0.002) || (compare == 7 && color.a >= ref);
        if (!accepted) discard;
    }
    if (screen.w) color.rgb = lerp(fog.rgb,color.rgb,p.specular.a);
    return saturate(color);
}
)";
struct Parameters {
    float screen[4]; uint32_t color[4], alpha[4], options[4]; float fog[4];
};
struct Context {
    HWND window = nullptr;
    HWND outputWindow = nullptr;
    bool addedChildClipping = false;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> gpu;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11Texture2D> color, sceneColor, resolvedColor, depth;
    ComPtr<ID3D11ShaderResourceView> resolvedView;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11DepthStencilView> depthView;
    ComPtr<ID3D11ShaderResourceView> depthResource;
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> vertices, indices, constants;
    std::map<uint64_t,ComPtr<ID3D11ShaderResourceView>> textures;
    std::vector<RBPointLight> pointLights;
    std::map<uint64_t,DirectX::XMFLOAT3> textureColors;
    std::map<uint32_t,ComPtr<ID3D11BlendState>> blends;
    std::map<uint32_t,ComPtr<ID3D11DepthStencilState>> depths;
    std::map<uint32_t,ComPtr<ID3D11RasterizerState>> rasters;
    std::map<uint32_t,ComPtr<ID3D11SamplerState>> samplers;
    uint32_t width = 0, height = 0, logicalWidth = 0, logicalHeight = 0;
    uint32_t vertexCapacity = 0, indexCapacity = 0;
    uint32_t samples = 1, allocatedSamples = 0, filtering = 0, vsync = 2;
    bool failed = false;
    PostProcess post;
    ShadowMap shadows;
    ~Context() {
        if (gpu) { gpu->ClearState(); gpu->Flush(); }
        swap.Reset();
        if (outputWindow) DestroyWindow(outputWindow);
        if (addedChildClipping && IsWindow(window))
            SetWindowLongPtrA(window,GWL_STYLE,GetWindowLongPtrA(window,GWL_STYLE)&~LONG_PTR(WS_CLIPCHILDREN));
    }
    bool Check(HRESULT hr, const char* operation) {
        if (FAILED(hr)) { Log(operation,hr); failed=true; return false; } return true;
    }
    bool Resize() {
        RECT rect; if (!GetClientRect(window,&rect)) return false;
        uint32_t w=std::max(1L,rect.right), h=std::max(1L,rect.bottom);
        if (w == width && h == height && allocatedSamples == samples) return true;
        if (!SetWindowPos(outputWindow,nullptr,0,0,w,h,SWP_NOZORDER|SWP_NOACTIVATE)) return false;
        gpu->OMSetRenderTargets(0,nullptr,nullptr);
        target.Reset(); color.Reset(); sceneColor.Reset(); resolvedColor.Reset(); resolvedView.Reset(); depth.Reset(); depthView.Reset(); depthResource.Reset();
        if (!Check(swap->ResizeBuffers(0,w,h,DXGI_FORMAT_UNKNOWN,0),"ResizeBuffers")) return false;
        if (!Check(swap->GetBuffer(0,IID_PPV_ARGS(&color)),"Get swap buffer")) return false;
        D3D11_TEXTURE2D_DESC d{}; d.Width=w; d.Height=h; d.MipLevels=d.ArraySize=1;
        d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=samples;
        d.BindFlags=D3D11_BIND_RENDER_TARGET;
        if (!Check(device->CreateTexture2D(&d,nullptr,&sceneColor),"Create MSAA color") ||
            !Check(device->CreateRenderTargetView(sceneColor.Get(),nullptr,&target),"Create color view")) return false;
        d.SampleDesc.Count=1; d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        if (!Check(device->CreateTexture2D(&d,nullptr,&resolvedColor),"Create resolved color") ||
            !Check(device->CreateShaderResourceView(resolvedColor.Get(),nullptr,&resolvedView),"Create resolved view")) return false;
        d.Format=DXGI_FORMAT_R32_TYPELESS; d.SampleDesc.Count=samples;
        d.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        if (!Check(device->CreateTexture2D(&d,nullptr,&depth),"Create sampleable depth")) return false;
        D3D11_DEPTH_STENCIL_VIEW_DESC dv{}; dv.Format=DXGI_FORMAT_D32_FLOAT; dv.ViewDimension=samples>1?D3D11_DSV_DIMENSION_TEXTURE2DMS:D3D11_DSV_DIMENSION_TEXTURE2D;
        if (!Check(device->CreateDepthStencilView(depth.Get(),&dv,&depthView),"Create depth view")) return false;
        D3D11_SHADER_RESOURCE_VIEW_DESC sr{}; sr.Format=DXGI_FORMAT_R32_FLOAT; sr.ViewDimension=samples>1?D3D11_SRV_DIMENSION_TEXTURE2DMS:D3D11_SRV_DIMENSION_TEXTURE2D; if(samples==1)sr.Texture2D.MipLevels=1;
        if (!Check(device->CreateShaderResourceView(depth.Get(),&sr,&depthResource),"Create depth SRV")) return false;
        width=w; height=h; allocatedSamples=samples;
        float black[4]={0,0,0,1}; gpu->ClearRenderTargetView(target.Get(),black); gpu->ClearDepthStencilView(depthView.Get(),D3D11_CLEAR_DEPTH,1,0);
        return true;
    }
    void Resolve(ID3D11Texture2D* destination) {
        gpu->OMSetRenderTargets(0,nullptr,nullptr);
        if(samples>1) gpu->ResolveSubresource(destination,0,sceneColor.Get(),0,DXGI_FORMAT_R8G8B8A8_UNORM);
        else gpu->CopyResource(destination,sceneColor.Get());
        gpu->OMSetRenderTargets(1,target.GetAddressOf(),depthView.Get());
    }
    bool Buffer(ComPtr<ID3D11Buffer>& buffer,uint32_t& capacity,uint32_t bytes,uint32_t binding) {
        if (bytes <= capacity) return true;
        buffer.Reset(); capacity=std::max(bytes,65536u);
        D3D11_BUFFER_DESC d{}; d.ByteWidth=capacity; d.Usage=D3D11_USAGE_DYNAMIC;
        d.BindFlags=binding; d.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        return Check(device->CreateBuffer(&d,nullptr,&buffer),"Create streaming buffer");
    }
};
D3D11_BLEND Blend(uint32_t value) {
    static D3D11_BLEND table[]={D3D11_BLEND_ZERO,D3D11_BLEND_ZERO,D3D11_BLEND_ONE,D3D11_BLEND_SRC_COLOR,
        D3D11_BLEND_INV_SRC_COLOR,D3D11_BLEND_SRC_ALPHA,D3D11_BLEND_INV_SRC_ALPHA,D3D11_BLEND_DEST_ALPHA,
        D3D11_BLEND_INV_DEST_ALPHA,D3D11_BLEND_DEST_COLOR,D3D11_BLEND_INV_DEST_COLOR,D3D11_BLEND_SRC_ALPHA_SAT};
    return value < sizeof(table)/sizeof(table[0]) ? table[value] : D3D11_BLEND_ONE;
}
uint8_t Channel(uint32_t pixel,uint32_t mask,uint8_t fallback) {
    if (!mask) return fallback;
    unsigned shift=0; while (!(mask&1)) { mask>>=1; ++shift; }
    return static_cast<uint8_t>(((pixel>>shift)&mask)*255ull/mask);
}
void* Create(void* window,uint32_t w,uint32_t h) {
    auto* c=new Context; c->window=static_cast<HWND>(window); c->logicalWidth=w; c->logicalHeight=h;
    // The legacy DirectDraw wrapper still owns the parent during migration.
    // Separate presentation surfaces prevent its cursor/repaint updates from
    // replacing native frames. Disabled children route mouse input to the parent.
    LONG_PTR style=GetWindowLongPtrA(c->window,GWL_STYLE);
    c->addedChildClipping=(style&WS_CLIPCHILDREN)==0;
    SetWindowLongPtrA(c->window,GWL_STYLE,style|WS_CLIPCHILDREN);
    RECT client{};
    if (!GetClientRect(c->window,&client)) { delete c; return nullptr; }
    c->outputWindow=CreateWindowExA(WS_EX_NOPARENTNOTIFY,"STATIC","Reborn D3D11 output",
        WS_CHILD|WS_VISIBLE|WS_DISABLED|WS_CLIPSIBLINGS,0,0,
        std::max(1L,client.right),std::max(1L,client.bottom),c->window,nullptr,GetModuleHandleA(nullptr),nullptr);
    if (!c->outputWindow) { Log("Create native output window",HRESULT_FROM_WIN32(GetLastError())); delete c; return nullptr; }
    DXGI_SWAP_CHAIN_DESC desc{}; desc.BufferCount=2; desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.OutputWindow=c->outputWindow;
    desc.SampleDesc.Count=1; desc.Windowed=TRUE; desc.SwapEffect=DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL level{};
    HRESULT hr=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&c->swap,&c->device,&level,&c->gpu);
    if (!c->Check(hr,"Create native D3D11 device")) { delete c; return nullptr; }
    ComPtr<IDXGIFactory> factory;
    if (SUCCEEDED(c->swap->GetParent(IID_PPV_ARGS(&factory)))) factory->MakeWindowAssociation(c->window,DXGI_MWA_NO_ALT_ENTER);
    ComPtr<ID3DBlob> vertex,pixel,error;
    hr=D3DCompile(Shader,strlen(Shader),"Reborn",nullptr,nullptr,"VS","vs_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&vertex,&error);
    if (FAILED(hr) && error) Log(static_cast<const char*>(error->GetBufferPointer()),hr);
    if (!c->Check(hr,"Compile vertex shader")) { delete c; return nullptr; }
    hr=D3DCompile(Shader,strlen(Shader),"Reborn",nullptr,nullptr,"PS","ps_4_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&pixel,&error);
    if (FAILED(hr) && error) Log(static_cast<const char*>(error->GetBufferPointer()),hr);
    if (!c->Check(hr,"Compile pixel shader")) { delete c; return nullptr; }
    D3D11_INPUT_ELEMENT_DESC elements[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R8G8B8A8_UNORM,0,16,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"COLOR",1,DXGI_FORMAT_R8G8B8A8_UNORM,0,20,D3D11_INPUT_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
    D3D11_BUFFER_DESC cb{}; cb.ByteWidth=sizeof(Parameters); cb.Usage=D3D11_USAGE_DEFAULT; cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    bool ok=c->Check(c->device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&c->vs),"Create VS") &&
        c->Check(c->device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&c->ps),"Create PS") &&
        c->Check(c->device->CreateInputLayout(elements,4,vertex->GetBufferPointer(),vertex->GetBufferSize(),&c->layout),"Create layout") &&
        c->Check(c->device->CreateBuffer(&cb,nullptr,&c->constants),"Create parameters") && c->Resize();
    if (!ok) { delete c; return nullptr; }
    Log("Native D3D11 renderer initialized; isolated child output; sampleable 32-bit depth; legacy stencil pass bypassed");
    return c;
}
void Destroy(void* context) { delete static_cast<Context*>(context); }
int Begin(void* context,uint32_t w,uint32_t h,uint32_t flags,uint32_t color) {
    auto& c=*static_cast<Context*>(context); if (c.failed || !w || !h || !c.Resize()) return 0;
    c.logicalWidth=w; c.logicalHeight=h;
    c.gpu->OMSetRenderTargets(1,c.target.GetAddressOf(),c.depthView.Get());
    float scale=std::min(float(c.width)/w,float(c.height)/h);
    D3D11_VIEWPORT vp{(c.width-w*scale)*0.5f,(c.height-h*scale)*0.5f,w*scale,h*scale,0,1}; c.gpu->RSSetViewports(1,&vp);
    if (flags&1) { float rgba[]={float((color>>16)&255)/255,float((color>>8)&255)/255,float(color&255)/255,1}; c.gpu->ClearRenderTargetView(c.target.Get(),rgba); }
    if (flags&2) c.gpu->ClearDepthStencilView(c.depthView.Get(),D3D11_CLEAR_DEPTH,1,0);
    return 1;
}
int HasTexture(void* context,uint64_t id) { return static_cast<Context*>(context)->textures.count(id) ? 1 : 0; }
int Upload(void* context,const RBTexture* t) {
    auto& c=*static_cast<Context*>(context);
    if (!t || !t->pixels || !t->width || !t->height || t->width>16384 || t->height>16384) return 0;
    if (t->bits!=4 && t->bits!=8 && t->bits!=16 && t->bits!=24 && t->bits!=32) return 0;
    if (t->bits<=8 && !t->palette) return 0;
    if (t->pitch < (t->width*t->bits+7)/8) return 0;
    std::vector<uint32_t> rgba(size_t(t->width)*t->height);
    for (uint32_t y=0;y<t->height;++y) for (uint32_t x=0;x<t->width;++x) {
        const uint8_t* row=static_cast<const uint8_t*>(t->pixels)+size_t(y)*t->pitch;
        uint32_t p=0; uint8_t r,g,b,a;
        if (t->bits<=8 && t->palette) {
            uint32_t index=t->bits==4 ? ((row[x/2] >> ((x&1)?0:4))&15) : row[x];
            uint32_t entry=t->palette[index]; r=entry&255; g=(entry>>8)&255; b=(entry>>16)&255; a=(entry>>24)&255;
        } else {
            memcpy(&p,row+x*(t->bits/8),t->bits/8);
            r=Channel(p,t->redMask,0); g=Channel(p,t->greenMask,0); b=Channel(p,t->blueMask,0); a=Channel(p,t->alphaMask,255);
        }
        if (t->colorKeyEnabled && ((uint32_t(r)<<16)|(uint32_t(g)<<8)|b)==t->colorKeyRGB) a=0;
        rgba[size_t(y)*t->width+x]=r|(uint32_t(g)<<8)|(uint32_t(b)<<16)|(uint32_t(a)<<24);
    }
    unsigned width=t->width,height=t->height;
    if(t->scaleEdges && width<=512 && height<=512) {
        DWORD started=GetTickCount();
        unsigned scale=t->scaleEdges==2?4:1;
        rgba=ReconstructContours(rgba,width,height,scale,t->scaleEdges==2);width*=scale;height*=scale;
        char message[128];sprintf_s(message,"Contour-reconstructed texture: %ux%u -> %ux%u",t->width,t->height,width,height);Log(message);
        sprintf_s(message,"Contour construction: %lu ms",GetTickCount()-started);Log(message);
    }
    D3D11_TEXTURE2D_DESC d{}; d.Width=width; d.Height=height; d.MipLevels=d.ArraySize=1;
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1; d.Usage=D3D11_USAGE_DEFAULT;
    d.MipLevels=0; d.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_RENDER_TARGET; d.MiscFlags=D3D11_RESOURCE_MISC_GENERATE_MIPS;
    D3D11_SUBRESOURCE_DATA data{rgba.data(),width*4,0}; ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    if (!c.Check(c.device->CreateTexture2D(&d,nullptr,&texture),"Upload texture") ||
        !c.Check(c.device->CreateShaderResourceView(texture.Get(),nullptr,&view),"Create texture view")) return 0;
    c.gpu->UpdateSubresource(texture.Get(),0,nullptr,rgba.data(),data.SysMemPitch,0);
    c.gpu->GenerateMips(view.Get());
    c.textures[t->id]=view;
    double red=0,green=0,blue=0,weight=0;
    for(uint32_t pixel:rgba){float r=float(pixel&255)/255,g=float((pixel>>8)&255)/255,b=float((pixel>>16)&255)/255;
        float w=float(pixel>>24)/255*std::max(r,std::max(g,b));red+=r*w;green+=g*w;blue+=b*w;weight+=w;}
    c.textureColors[t->id]=weight>0?DirectX::XMFLOAT3(float(red/weight),float(green/weight),float(blue/weight)):DirectX::XMFLOAT3(1,1,1);
    return 1;
}
int UploadFont(void* context,uint64_t id,uint32_t width,uint32_t height,const uint16_t* glyphs,uint32_t count) {
    if(!width || !height || width>512 || height>512 || !glyphs || count>1024)return 0;
    for(unsigned i=0;i<count;++i)if(unsigned(glyphs[i*5+1])+glyphs[i*5+3]>width || unsigned(glyphs[i*5+2])+glyphs[i*5+4]>height)return 0;
    std::vector<uint32_t> pixels;
    if(!RasterizeFont(width*2,height*2,glyphs,count,pixels)){Log("Custom font unavailable; retaining original font");return 0;}
    RBTexture t{};t.id=id;t.width=width*2;t.height=height*2;t.pitch=t.width*4;t.bits=32;t.pixels=pixels.data();
    t.redMask=255;t.greenMask=0xff00;t.blueMask=0xff0000;t.alphaMask=0xff000000;
    int result=Upload(context,&t);if(result)Log("Custom OpenType font atlas uploaded");return result;
}
int Draw(void* context,uint32_t primitive,const RBVertex* v,uint32_t vc,const uint16_t* ix,uint32_t ic,const RBState* s,uint64_t texture) {
    auto& c=*static_cast<Context*>(context); if (c.failed || !v || !vc || !s) return 0;
    std::vector<uint16_t> fan;
    if (primitive==6) {
        uint32_t n=ix?ic:vc; if(n<3 || n>65535) return 0;
        for(uint32_t i=1;i+1<n;++i) {fan.push_back(ix?ix[0]:0);fan.push_back(ix?ix[i]:uint16_t(i));fan.push_back(ix?ix[i+1]:uint16_t(i+1));}
        ix=fan.data(); ic=static_cast<uint32_t>(fan.size()); primitive=4;
    }
    if (primitive!=4 && primitive!=5) { Log("Unsupported primitive type"); return 0; }
    if (!c.Buffer(c.vertices,c.vertexCapacity,vc*sizeof(RBVertex),D3D11_BIND_VERTEX_BUFFER)) return 0;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!c.Check(c.gpu->Map(c.vertices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped),"Map vertices")) return 0;
    memcpy(mapped.pData,v,vc*sizeof(RBVertex)); c.gpu->Unmap(c.vertices.Get(),0);
    if (ix && ic) {
        if(!c.Buffer(c.indices,c.indexCapacity,ic*sizeof(uint16_t),D3D11_BIND_INDEX_BUFFER)) return 0;
        if(!c.Check(c.gpu->Map(c.indices.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&mapped),"Map indices")) return 0;
        memcpy(mapped.pData,ix,ic*sizeof(uint16_t)); c.gpu->Unmap(c.indices.Get(),0);
        c.gpu->IASetIndexBuffer(c.indices.Get(),DXGI_FORMAT_R16_UINT,0);
    }
    uint32_t blendKey=s->blendEnable|(s->srcBlend<<1)|(s->dstBlend<<6);
    if(!c.blends.count(blendKey)) {
        D3D11_BLEND_DESC d{}; auto& rt=d.RenderTarget[0]; rt.BlendEnable=s->blendEnable; rt.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        rt.SrcBlend=Blend(s->srcBlend); rt.DestBlend=Blend(s->dstBlend); rt.BlendOp=D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha=D3D11_BLEND_ONE; rt.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        if(!c.Check(c.device->CreateBlendState(&d,&c.blends[blendKey]),"Create blend state")) return 0;
    }
    uint32_t depthKey=s->depthEnable|(s->depthWrite<<2)|(s->depthFunc<<3);
    if(!c.depths.count(depthKey)) {
        D3D11_DEPTH_STENCIL_DESC d{}; d.DepthEnable=s->depthEnable; d.DepthWriteMask=s->depthWrite?D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
        d.DepthFunc=static_cast<D3D11_COMPARISON_FUNC>(std::clamp(s->depthFunc,1u,8u));
        if(!c.Check(c.device->CreateDepthStencilState(&d,&c.depths[depthKey]),"Create depth state")) return 0;
    }
    if(!c.rasters.count(s->cull)) {
        D3D11_RASTERIZER_DESC d{}; d.FillMode=D3D11_FILL_SOLID; d.CullMode=s->cull==1?D3D11_CULL_NONE:D3D11_CULL_BACK;
        d.FrontCounterClockwise=s->cull==2; d.DepthClipEnable=TRUE; d.MultisampleEnable=TRUE;
        if(!c.Check(c.device->CreateRasterizerState(&d,&c.rasters[s->cull]),"Create raster state")) return 0;
    }
    // Preserve pixel-aligned UI sampling; force quality filtering on 3D geometry.
    uint32_t filter=s->depthEnable?c.filtering:0;
    uint32_t samplerKey=s->addressU|(s->addressV<<4)|(s->linearFilter<<8)|(filter<<12);
    if(!c.samplers.count(samplerKey)) {
        D3D11_SAMPLER_DESC d{}; d.Filter=s->linearFilter?D3D11_FILTER_MIN_MAG_MIP_LINEAR:D3D11_FILTER_MIN_MAG_MIP_POINT;
        if(filter==1)d.Filter=D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
        if(filter==2)d.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        if(filter>=4)d.Filter=D3D11_FILTER_ANISOTROPIC;
        d.MaxAnisotropy=filter>=4?filter:1;
        if(filter==0)d.MaxLOD=0;
        d.AddressU=static_cast<D3D11_TEXTURE_ADDRESS_MODE>(std::clamp(s->addressU,1u,4u));
        d.AddressV=static_cast<D3D11_TEXTURE_ADDRESS_MODE>(std::clamp(s->addressV,1u,4u)); d.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;
        if(filter)d.MaxLOD=D3D11_FLOAT32_MAX;
        if(!c.Check(c.device->CreateSamplerState(&d,&c.samplers[samplerKey]),"Create sampler")) return 0;
    }
    ID3D11ShaderResourceView* view=nullptr; auto it=c.textures.find(texture); if(it!=c.textures.end()) view=it->second.Get();
    Parameters p{}; p.screen[0]=float(c.logicalWidth); p.screen[1]=float(c.logicalHeight);
    p.screen[2]=float(s->alphaRef>255?s->alphaRef>>16:s->alphaRef)/255; p.screen[3]=float(s->fogEnable);
    p.color[0]=s->colorOp;p.color[1]=s->colorArg1;p.color[2]=s->colorArg2;
    p.alpha[0]=s->alphaOp;p.alpha[1]=s->alphaArg1;p.alpha[2]=s->alphaArg2;
    p.options[0]=view?1:0;p.options[1]=s->alphaTest;p.options[2]=s->alphaFunc;p.options[3]=s->colorKey;
    p.fog[0]=float((s->fogColor>>16)&255)/255;p.fog[1]=float((s->fogColor>>8)&255)/255;p.fog[2]=float(s->fogColor&255)/255;
    c.gpu->UpdateSubresource(c.constants.Get(),0,nullptr,&p,0,0);
    uint32_t stride=sizeof(RBVertex),offset=0;
    c.gpu->IASetInputLayout(c.layout.Get());c.gpu->IASetVertexBuffers(0,1,c.vertices.GetAddressOf(),&stride,&offset);
    c.gpu->IASetPrimitiveTopology(primitive==4?D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    c.gpu->VSSetShader(c.vs.Get(),nullptr,0);c.gpu->PSSetShader(c.ps.Get(),nullptr,0);
    c.gpu->VSSetConstantBuffers(0,1,c.constants.GetAddressOf());c.gpu->PSSetConstantBuffers(0,1,c.constants.GetAddressOf());
    c.gpu->PSSetShaderResources(0,1,&view);c.gpu->PSSetSamplers(0,1,c.samplers[samplerKey].GetAddressOf());
    c.gpu->OMSetBlendState(c.blends[blendKey].Get(),nullptr,0xffffffff);c.gpu->OMSetDepthStencilState(c.depths[depthKey].Get(),0);c.gpu->RSSetState(c.rasters[s->cull].Get());
    if(ix && ic)c.gpu->DrawIndexed(ic,0,0);else c.gpu->Draw(vc,0);
    return 1;
}
int Present(void* context,uint32_t vsync) {
    auto& c=*static_cast<Context*>(context); c.Resolve(c.color.Get());
    return c.Check(c.swap->Present((c.vsync==2?vsync:c.vsync)?1:0,0),"Native present");
}
int Capture(void* context,const char* filename) {
    auto& c=*static_cast<Context*>(context); c.Resolve(c.color.Get()); D3D11_TEXTURE2D_DESC d; c.color->GetDesc(&d);
    d.Usage=D3D11_USAGE_STAGING;d.BindFlags=0;d.CPUAccessFlags=D3D11_CPU_ACCESS_READ;d.MiscFlags=0;
    ComPtr<ID3D11Texture2D> staging;
    if(!c.Check(c.device->CreateTexture2D(&d,nullptr,&staging),"Create capture"))return 0;
    c.gpu->CopyResource(staging.Get(),c.color.Get()); D3D11_MAPPED_SUBRESOURCE m{};
    if(!c.Check(c.gpu->Map(staging.Get(),0,D3D11_MAP_READ,0,&m),"Read capture"))return 0;
    FILE* f=nullptr;fopen_s(&f,filename,"wb");
    if(f){fprintf(f,"P6\n%u %u\n255\n",d.Width,d.Height);for(uint32_t y=0;y<d.Height;++y)for(uint32_t x=0;x<d.Width;++x)fwrite(static_cast<uint8_t*>(m.pData)+y*m.RowPitch+x*4,1,3,f);fclose(f);}
    c.gpu->Unmap(staging.Get(),0);return f?1:0;
}
int Configure(void* context,uint32_t samples,uint32_t filtering,uint32_t vsync) {
    auto& c=*static_cast<Context*>(context);
    if(samples!=1 && samples!=2 && samples!=4 && samples!=8)return 0;
    if(filtering!=0 && filtering!=1 && filtering!=2 && filtering!=4 && filtering!=8 && filtering!=16)return 0;
    unsigned colorLevels=0,depthLevels=0;
    if(FAILED(c.device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,samples,&colorLevels)) || !colorLevels ||
       FAILED(c.device->CheckMultisampleQualityLevels(DXGI_FORMAT_D32_FLOAT,samples,&depthLevels)) || !depthLevels)return 0;
    if(c.samples!=samples || c.filtering!=filtering || c.vsync!=vsync) {
        char message[120];sprintf_s(message,"Native settings: %ux MSAA, filtering %u, VSync %u",samples,filtering,vsync);Log(message);
    }
    c.samples=samples;c.filtering=filtering;c.vsync=vsync;return 1;
}
int ApplyBloom(void* context,float strength) {
    auto& c=*static_cast<Context*>(context);if(strength<=0)return 1;
    if(!c.post.Initialize(c.device.Get()) || !c.post.Resize(c.device.Get(),c.width,c.height))return 0;
    UINT count=1;D3D11_VIEWPORT saved;c.gpu->RSGetViewports(&count,&saved);
    c.Resolve(c.resolvedColor.Get());
    PostProcess::Parameters p{};p.step[0]=1.f/c.width;p.step[1]=1.f/c.height;
    unsigned w=std::max(1u,c.width/4),h=std::max(1u,c.height/4);
    ID3D11ShaderResourceView* views[4]={c.resolvedView.Get(),nullptr,nullptr,nullptr};
    c.post.Draw(c.gpu.Get(),c.post.ping[0].rtv.Get(),c.post.extract.Get(),w,h,p,views);
    for(int pass=0;pass<2;pass++) {
        views[0]=c.post.ping[0].srv.Get();p.step[0]=1.f/w;p.step[1]=0;
        c.post.Draw(c.gpu.Get(),c.post.ping[1].rtv.Get(),c.post.blur.Get(),w,h,p,views);
        views[0]=c.post.ping[1].srv.Get();p.step[0]=0;p.step[1]=1.f/h;
        c.post.Draw(c.gpu.Get(),c.post.ping[0].rtv.Get(),c.post.blur.Get(),w,h,p,views);
    }
    views[0]=c.post.ping[0].srv.Get();p.step[2]=std::clamp(strength,0.f,2.f);
    c.post.Draw(c.gpu.Get(),c.target.Get(),c.post.composite.Get(),c.width,c.height,p,views,c.post.additive.Get());
    c.gpu->OMSetRenderTargets(1,c.target.GetAddressOf(),c.depthView.Get());c.gpu->RSSetViewports(1,&saved);return 1;
}
int BeginLighting(void* context,const float* projection,const float* eye,const float* direction,float farClip) {
    static_cast<Context*>(context)->pointLights.clear();
    return static_cast<Context*>(context)->shadows.Begin(projection,eye,direction,farClip);
}
void AddTriangle(void* context,const float* a,const float* b,const float* d) {
    auto& s=static_cast<Context*>(context)->shadows;
    if(s.vertices.size()>3000000){s.valid=false;return;}
    unsigned first=unsigned(s.vertices.size());
    s.vertices.push_back({{a[0],a[1],a[2]},0,0});s.vertices.push_back({{b[0],b[1],b[2]},0,0});s.vertices.push_back({{d[0],d[1],d[2]},0,0});
    if(!s.batches.empty() && !s.batches.back().mask)s.batches.back().count+=3;
    else s.batches.push_back({first,3,{}});
}
void AddMaskedTriangle(void* context,const RBShadowVertex* vertices,uint64_t texture) {
    auto& c=*static_cast<Context*>(context);auto& s=c.shadows;
    auto found=c.textures.find(texture);if(found==c.textures.end())return;
    if(s.vertices.size()>3000000){s.valid=false;return;}
    unsigned first=unsigned(s.vertices.size());
    for(unsigned i=0;i<3;++i){const auto& v=vertices[i];s.vertices.push_back({{v.x,v.y,v.z},v.u,v.v});}
    if(!s.batches.empty() && s.batches.back().mask.Get()==found->second.Get())s.batches.back().count+=3;
    else s.batches.push_back({first,3,found->second});
}
int ApplyLighting(void* context,float shadowOpacity,float aoStrength) {
    auto& c=*static_cast<Context*>(context);if(!c.shadows.valid || (shadowOpacity<=0 && aoStrength<=0))return 1;
    if(!c.post.Initialize(c.device.Get()) || !c.post.Resize(c.device.Get(),c.width,c.height))return 0;
    UINT count=1;D3D11_VIEWPORT saved;c.gpu->RSGetViewports(&count,&saved);
    if(shadowOpacity>0 && !c.shadows.Render(c.device.Get(),c.gpu.Get()))return 0;
    PostProcess::Parameters p{};p.inverseViewProjection=c.shadows.inverse;p.lightViewProjection=c.shadows.light;
    p.viewport[0]=saved.TopLeftX;p.viewport[1]=saved.TopLeftY;p.viewport[2]=saved.Width;p.viewport[3]=saved.Height;
    p.screen[0]=float(c.width);p.screen[1]=float(c.height);p.screen[2]=ShadowMap::Size;p.screen[3]=aoStrength;
    p.lighting[0]=c.shadows.vertices.empty()?0:shadowOpacity;p.lighting[1]=c.shadows.radius*.025f;
    p.lighting[2]=std::max(.00002f,(2*c.shadows.radius/ShadowMap::Size)*1.25f/c.shadows.depthSpan);
    ID3D11ShaderResourceView* views[4]={nullptr,c.samples==1?c.depthResource.Get():nullptr,nullptr,c.samples>1?c.depthResource.Get():nullptr};
    c.post.Draw(c.gpu.Get(),c.post.resolvedDepth.rtv.Get(),c.samples>1?c.post.depthMulti.Get():c.post.depthSingle.Get(),c.width,c.height,p,views);
    views[1]=c.post.resolvedDepth.srv.Get();views[2]=c.shadows.srv.Get();views[3]=nullptr;
    c.post.Draw(c.gpu.Get(),c.target.Get(),c.post.lighting.Get(),c.width,c.height,p,views,c.post.multiply.Get());
    c.gpu->OMSetRenderTargets(1,c.target.GetAddressOf(),c.depthView.Get());c.gpu->RSSetViewports(1,&saved);return 1;
}
void AddPointLight(void* context,const RBPointLight* light) {
    auto& c=*static_cast<Context*>(context);
    if(!light || !c.shadows.valid)return;
    const float* values=&light->x;
    for(unsigned i=0;i<8;i++)if(!std::isfinite(values[i]))return;
    if(light->radius<=0 || light->intensity<=0)return;
    auto score=[&](const RBPointLight& l){float x=l.x-c.shadows.eye.x,y=l.y-c.shadows.eye.y,z=l.z-c.shadows.eye.z;
        return l.intensity*l.radius*l.radius/(1+x*x+y*y+z*z);};
    if(c.pointLights.size()<32)c.pointLights.push_back(*light);
    else {auto least=std::min_element(c.pointLights.begin(),c.pointLights.end(),[&](const auto& a,const auto& b){return score(a)<score(b);});
        if(score(*light)>score(*least))*least=*light;}
}
int ApplyPointLights(void* context) {
    auto& c=*static_cast<Context*>(context);if(c.pointLights.empty() || !c.shadows.valid)return 1;
    if(!c.post.Initialize(c.device.Get()) || !c.post.Resize(c.device.Get(),c.width,c.height))return 0;
    UINT count=1;D3D11_VIEWPORT saved;c.gpu->RSGetViewports(&count,&saved);
    PostProcess::Parameters p{};p.inverseViewProjection=c.shadows.inverse;
    p.viewport[0]=saved.TopLeftX;p.viewport[1]=saved.TopLeftY;p.viewport[2]=saved.Width;p.viewport[3]=saved.Height;
    p.step[0]=float(c.pointLights.size());
    for(unsigned i=0;i<c.pointLights.size();i++){
        memcpy(p.lightPosition[i],&c.pointLights[i].x,16);memcpy(p.lightColor[i],&c.pointLights[i].red,16);
    }
    ID3D11ShaderResourceView* views[4]={nullptr,c.samples==1?c.depthResource.Get():nullptr,nullptr,c.samples>1?c.depthResource.Get():nullptr};
    c.post.Draw(c.gpu.Get(),c.post.resolvedDepth.rtv.Get(),c.samples>1?c.post.depthMulti.Get():c.post.depthSingle.Get(),c.width,c.height,p,views);
    c.Resolve(c.resolvedColor.Get());
    views[0]=c.resolvedView.Get();views[1]=c.post.resolvedDepth.srv.Get();views[3]=nullptr;
    c.post.Draw(c.gpu.Get(),c.target.Get(),c.post.pointLights.Get(),c.width,c.height,p,views,c.post.additive.Get());
    c.gpu->OMSetRenderTargets(1,c.target.GetAddressOf(),c.depthView.Get());c.gpu->RSSetViewports(1,&saved);
    c.pointLights.clear();return 1;
}
int TextureColor(void* context,uint64_t texture,float* rgb) {
    auto& c=*static_cast<Context*>(context);auto found=c.textureColors.find(texture);
    if(!rgb || found==c.textureColors.end())return 0;
    memcpy(rgb,&found->second,12);return 1;
}
const RBApi Api{8,Create,Destroy,Begin,HasTexture,Upload,Draw,Present,Capture,Configure,ApplyBloom,BeginLighting,AddTriangle,ApplyLighting,AddMaskedTriangle,AddPointLight,ApplyPointLights,TextureColor,UploadFont,MeasureFont};
}
extern "C" __declspec(dllexport) const RBApi* RBGetApi(uint32_t version) { return version==8?&Api:nullptr; }
