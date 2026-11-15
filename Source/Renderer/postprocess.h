#pragma once
#include <DirectXMath.h>

// Native fullscreen passes share shaders, sampling and state setup.
struct PostProcess {
    struct Target {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11RenderTargetView> rtv;
        ComPtr<ID3D11ShaderResourceView> srv;
    };
    struct Parameters {
        DirectX::XMFLOAT4X4 inverseViewProjection{}, lightViewProjection{};
        float viewport[4]{}, screen[4]{}, lighting[4]{}, step[4]{};
        float lightPosition[32][4]{},lightColor[32][4]{};
    };
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> extract,blur,composite,lighting,depthSingle,depthMulti,pointLights;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11SamplerState> sampler;
    ComPtr<ID3D11SamplerState> comparisonSampler;
    ComPtr<ID3D11RasterizerState> raster;
    ComPtr<ID3D11DepthStencilState> noDepth;
    ComPtr<ID3D11BlendState> additive,multiply;
    Target ping[2],resolvedDepth;
    unsigned width=0,height=0;
    bool attempted=false,ready=false;

    static const char* Source() { return R"(
cbuffer Parameters : register(b0) {
 row_major float4x4 inverseVP; row_major float4x4 lightVP;
 float4 viewport; float4 screen; float4 lighting; float4 step;
 float4 lightPosition[32]; float4 lightColor[32];
};
Texture2D image : register(t0);
Texture2D<float> sceneDepth : register(t1);
Texture2D<float> shadowDepth : register(t2);
Texture2DMS<float> depthMS : register(t3);
SamplerState linearClamp : register(s0);
SamplerComparisonState shadowSampler : register(s1);
struct Pixel { float4 pos:SV_Position; float2 uv:TEXCOORD0; };
Pixel VS(uint id:SV_VertexID) {
 Pixel p; p.uv=float2((id<<1)&2,id&2);p.pos=float4(p.uv*float2(2,-2)+float2(-1,1),0,1);return p;
}
float4 Extract(Pixel p):SV_Target {
 float3 c=0;
 c+=image.Sample(linearClamp,p.uv+step.xy*float2(-.5,-.5)).rgb;
 c+=image.Sample(linearClamp,p.uv+step.xy*float2(.5,-.5)).rgb;
 c+=image.Sample(linearClamp,p.uv+step.xy*float2(-.5,.5)).rgb;
 c+=image.Sample(linearClamp,p.uv+step.xy*float2(.5,.5)).rgb;
 c*=.25; float b=max(c.r,max(c.g,c.b));
 float knee=saturate((b-.6)/.4);return float4(c*knee*knee*.45,1);
}
float4 Blur(Pixel p):SV_Target {
 float3 c=0;float weight=0;
 [unroll]for(int i=-7;i<=7;i++){float w=exp(-float(i*i)/18);c+=image.Sample(linearClamp,p.uv+step.xy*i).rgb*w;weight+=w;}
 return float4(c/weight,1);
}
float4 Composite(Pixel p):SV_Target {return float4(image.Sample(linearClamp,p.uv).rgb*step.z,0);}
float DepthSingle(Pixel p):SV_Target {return sceneDepth.Load(int3(p.pos.xy,0));}
float DepthMulti(Pixel p):SV_Target {
 uint w,h,n;depthMS.GetDimensions(w,h,n);float z=1;
 [unroll]for(uint i=0;i<8;i++)if(i<n)z=min(z,depthMS.Load(int2(p.pos.xy),i));return z;
}
float3 World(float2 pixel,float z) {
 // Undo the half-pixel translation applied by the legacy TL vertex adapter.
 float2 uv=(pixel-.5-viewport.xy)/viewport.zw;
 float4 w=mul(float4(uv*float2(2,-2)+float2(-1,1),z,1),inverseVP);return w.xyz/w.w;
}
float3 SurfaceNormal(float2 position,float z,float3 w,out float3 dx,out float3 dy) {
 int2 pixel=int2(position);
 // Use the nearer-depth neighbor on each axis, avoiding normals spanning
 // silhouettes (car/road, cave/sky, pickup/background).
 int2 lo=int2(viewport.xy),hi=int2(viewport.xy+viewport.zw-1);
 int2 pl=clamp(pixel+int2(-1,0),lo,hi),pr=clamp(pixel+int2(1,0),lo,hi);
 int2 pu=clamp(pixel+int2(0,-1),lo,hi),pd=clamp(pixel+int2(0,1),lo,hi);
 float zl=sceneDepth.Load(int3(pl,0)),zr=sceneDepth.Load(int3(pr,0));
 float zu=sceneDepth.Load(int3(pu,0)),zd=sceneDepth.Load(int3(pd,0));
 dx=abs(zr-z)<abs(z-zl)?World(float2(pr)+.5,zr)-w:w-World(float2(pl)+.5,zl);
 dy=abs(zd-z)<abs(z-zu)?World(float2(pd)+.5,zd)-w:w-World(float2(pu)+.5,zu);
 float3 n=cross(dx,dy);float3 normal=n*rsqrt(max(dot(n,n),1e-12));
 // Screen-space winding depends on the camera basis handedness. AO must
 // always use the visible side, or convex ridges become false creases.
 float3 towardCamera=World(position,0)-w;
 if(dot(normal,towardCamera)<0)normal=-normal;
 return normal;
}
float4 PointLights(Pixel p):SV_Target {
 float z=sceneDepth.Load(int3(p.pos.xy,0));
 if(z>=.999999 || any(p.pos.xy<viewport.xy) || any(p.pos.xy>=viewport.xy+viewport.zw))return 0;
 float3 w=World(p.pos.xy,z),dx,dy;
 float3 normal=SurfaceNormal(p.pos.xy,z,w,dx,dy),illumination=0;
 [loop]for(int i=0;i<(int)step.x;i++) {
   float3 delta=lightPosition[i].xyz-w;float d2=dot(delta,delta);
   float radius=lightPosition[i].w;
   if(d2>=radius*radius)continue;
   float falloff=saturate(1-d2/(radius*radius));falloff*=falloff;
   float diffuse=saturate(dot(normal,delta*rsqrt(max(d2,.0001))));
   illumination+=lightColor[i].rgb*(lightColor[i].w*falloff*diffuse);
 }
 float3 surface=image.Load(int3(p.pos.xy,0)).rgb;
 // Modulate by the existing surface color so black tires stay dark.
 return float4(surface*min(illumination,4.0),0);
}
float4 Lighting(Pixel p):SV_Target {
 int2 pixel=int2(p.pos.xy);float z=sceneDepth.Load(int3(pixel,0));
 if(z>=.999999 || any(p.pos.xy<viewport.xy) || any(p.pos.xy>=viewport.xy+viewport.zw))return 1;
 float3 w=World(p.pos.xy,z),dx,dy;
 float3 normal=SurfaceNormal(p.pos.xy,z,w,dx,dy);
 float shade=1;
 if(lighting.x>0) {
   float4 light=mul(float4(w,1),lightVP);float3 q=light.xyz/light.w;
   float2 uv=q.xy*float2(.5,-.5)+.5;
   if(all(uv>0) && all(uv<1) && q.z>0 && q.z<1) {
     // Receiver-plane correction follows the surface slope in shadow UVs,
     // rather than using screen-pixel derivatives as a shadow-map bias.
     float3 lx=mul(float4(w+dx,1),lightVP).xyz-q;
     float3 ly=mul(float4(w+dy,1),lightVP).xyz-q;
     float2 ux=lx.xy*float2(.5,-.5),uy=ly.xy*float2(.5,-.5);
     float det=ux.x*uy.y-ux.y*uy.x;
     float2 slope=abs(det)>1e-12?float2(lx.z*uy.y-ly.z*ux.y,ly.z*ux.x-lx.z*uy.x)/det:float2(0,0);
     slope=clamp(slope,-10,10);
     float visible=0,weights=0;
     [unroll]for(int y=-2;y<=2;y++)[unroll]for(int x=-2;x<=2;x++){
       float2 offset=float2(x,y)/screen.z;
       float weight=(3-abs(x))*(3-abs(y));
       visible+=shadowDepth.SampleCmpLevelZero(shadowSampler,uv+offset,q.z+dot(slope,offset)-lighting.z)*weight;
       weights+=weight;
     }
     float edge=saturate(min(min(uv.x,uv.y),min(1-uv.x,1-uv.y))*30);
     shade*=1-lighting.x*(1-visible/weights)*edge;
   }
 }
 if(screen.w>0) {
   float occ=0;float radius=lighting.y;
   // A wider, resolution-scaled footprint gives contact shading some width
   // rather than tracing a thin line along every polygon join.
   float pixelScale=clamp(viewport.w/720.0,1.0,3.0);
   [unroll]for(int i=0;i<16;i++) {
     float angle=6.2831853*(i/16.0);float pixels=(4+(i%4)*6)*pixelScale;
     int2 at=clamp(pixel+int2(cos(angle)*pixels,sin(angle)*pixels),int2(viewport.xy),int2(viewport.xy+viewport.zw-1));
     float nz=sceneDepth.Load(int3(at,0));
     float3 delta=World(float2(at)+.5,nz)-w;float dist=length(delta);
     float height=dot(normal,delta);
     float horizon=max(0,height/max(dist,.0001)-.18);
     // Suppress tiny height differences and smoothly fade distant samples.
     // This keeps stud seams and almost-coplanar terrain from looking inked.
     float contact=smoothstep(radius*.015,radius*.08,max(0,height));
     float falloff=saturate(1-dist/radius);falloff*=falloff;
     occ+=(nz<.999999)?horizon*contact*falloff:0;
   }
   // Average the samples and cap the contribution: subtle AO can darken by
   // at most 12%, even where many surfaces meet. Cast shadows stay separate.
   float amount=min(occ*(.9/16.0),.20)*screen.w;
   shade*=1-min(amount,.32);
 }
 return float4(shade,shade,shade,1);
}
)"; }

    bool Initialize(ID3D11Device* device) {
        if(attempted)return ready;
        attempted=true;
        ComPtr<ID3DBlob> code,error;
        const char* source=Source();
        auto compile=[&](const char* entry,const char* profile) {
            code.Reset();error.Reset();HRESULT hr=D3DCompile(source,strlen(source),"Native postprocess",nullptr,nullptr,entry,profile,0,0,&code,&error);
            if(FAILED(hr)&&error)Log(static_cast<const char*>(error->GetBufferPointer()),hr);
            return SUCCEEDED(hr);
        };
        if(!compile("VS","vs_4_0") || FAILED(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&vertex)))return false;
        const char* entries[]={"Extract","Blur","Composite","Lighting","DepthSingle","DepthMulti","PointLights"};
        ComPtr<ID3D11PixelShader>* shaders[]={&extract,&blur,&composite,&lighting,&depthSingle,&depthMulti,&pointLights};
        for(int i=0;i<7;i++)if(!compile(entries[i],"ps_5_0") || FAILED(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,shaders[i]->GetAddressOf())))return false;
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(Parameters);cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(FAILED(device->CreateBuffer(&cb,nullptr,&constants)))return false;
        D3D11_SAMPLER_DESC sd{};sd.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;sd.MaxLOD=D3D11_FLOAT32_MAX;
        if(FAILED(device->CreateSamplerState(&sd,&sampler)))return false;
        sd.Filter=D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;sd.ComparisonFunc=D3D11_COMPARISON_LESS_EQUAL;
        sd.AddressU=sd.AddressV=sd.AddressW=D3D11_TEXTURE_ADDRESS_BORDER;
        sd.BorderColor[0]=sd.BorderColor[1]=sd.BorderColor[2]=sd.BorderColor[3]=1;
        if(FAILED(device->CreateSamplerState(&sd,&comparisonSampler)))return false;
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;rd.MultisampleEnable=TRUE;
        if(FAILED(device->CreateRasterizerState(&rd,&raster)))return false;
        D3D11_DEPTH_STENCIL_DESC dd{};dd.DepthFunc=D3D11_COMPARISON_ALWAYS;
        if(FAILED(device->CreateDepthStencilState(&dd,&noDepth)))return false;
        D3D11_BLEND_DESC bd{};auto& r=bd.RenderTarget[0];r.BlendEnable=TRUE;r.RenderTargetWriteMask=7;
        r.SrcBlend=D3D11_BLEND_ONE;r.DestBlend=D3D11_BLEND_ONE;r.BlendOp=D3D11_BLEND_OP_ADD;
        r.SrcBlendAlpha=D3D11_BLEND_ZERO;r.DestBlendAlpha=D3D11_BLEND_ONE;r.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        if(FAILED(device->CreateBlendState(&bd,&additive)))return false;
        r.SrcBlend=D3D11_BLEND_ZERO;r.DestBlend=D3D11_BLEND_SRC_COLOR;
        ready=SUCCEEDED(device->CreateBlendState(&bd,&multiply));return ready;
    }
    bool Allocate(ID3D11Device* device,Target& target,unsigned w,unsigned h,DXGI_FORMAT format) {
        target={};D3D11_TEXTURE2D_DESC d{};d.Width=w;d.Height=h;d.MipLevels=d.ArraySize=1;d.Format=format;d.SampleDesc.Count=1;d.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
        return SUCCEEDED(device->CreateTexture2D(&d,nullptr,&target.texture)) && SUCCEEDED(device->CreateRenderTargetView(target.texture.Get(),nullptr,&target.rtv)) && SUCCEEDED(device->CreateShaderResourceView(target.texture.Get(),nullptr,&target.srv));
    }
    bool Resize(ID3D11Device* device,unsigned w,unsigned h) {
        if(width==w&&height==h)return true;
        if(!Allocate(device,ping[0],std::max(1u,w/4),std::max(1u,h/4),DXGI_FORMAT_R16G16B16A16_FLOAT) ||
           !Allocate(device,ping[1],std::max(1u,w/4),std::max(1u,h/4),DXGI_FORMAT_R16G16B16A16_FLOAT) ||
           !Allocate(device,resolvedDepth,w,h,DXGI_FORMAT_R32_FLOAT))return false;
        width=w;height=h;return true;
    }
    void Draw(ID3D11DeviceContext* gpu,ID3D11RenderTargetView* target,ID3D11PixelShader* shader,unsigned w,unsigned h,const Parameters& p,ID3D11ShaderResourceView* const* views,ID3D11BlendState* blend=nullptr) {
        ID3D11ShaderResourceView* empty[4]={};gpu->PSSetShaderResources(0,4,empty);
        gpu->OMSetRenderTargets(1,&target,nullptr);gpu->OMSetDepthStencilState(noDepth.Get(),0);
        gpu->OMSetBlendState(blend,nullptr,0xffffffff);gpu->RSSetState(raster.Get());
        D3D11_VIEWPORT vp{0,0,float(w),float(h),0,1};gpu->RSSetViewports(1,&vp);
        gpu->IASetInputLayout(nullptr);gpu->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gpu->VSSetShader(vertex.Get(),nullptr,0);gpu->PSSetShader(shader,nullptr,0);
        gpu->UpdateSubresource(constants.Get(),0,nullptr,&p,0,0);gpu->PSSetConstantBuffers(0,1,constants.GetAddressOf());
        gpu->PSSetSamplers(0,1,sampler.GetAddressOf());gpu->PSSetSamplers(1,1,comparisonSampler.GetAddressOf());
        gpu->PSSetShaderResources(0,4,views);gpu->Draw(3,0);
        gpu->PSSetShaderResources(0,4,empty);
    }
};
