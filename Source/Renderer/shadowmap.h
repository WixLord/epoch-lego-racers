#pragma once
struct ShadowMap {
    static constexpr unsigned Size=4096;
    struct Vertex { DirectX::XMFLOAT3 position; float u,v; };
    struct Batch { unsigned first,count; ComPtr<ID3D11ShaderResourceView> mask; };
    std::vector<Vertex> vertices; std::vector<Batch> batches;
    DirectX::XMFLOAT4X4 inverse{},light{};
    DirectX::XMFLOAT3 eye{},direction{};
    float radius=200;
    float depthSpan=1;
    bool valid=false;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11DepthStencilView> dsv;
    ComPtr<ID3D11ShaderResourceView> srv;
    ComPtr<ID3D11VertexShader> shader;
    ComPtr<ID3D11PixelShader> maskShader; ComPtr<ID3D11SamplerState> maskSampler;
    bool attempted=false,ready=false;
    ComPtr<ID3D11InputLayout> layout;
    ComPtr<ID3D11Buffer> buffer,constants;
    ComPtr<ID3D11DepthStencilState> depth;
    ComPtr<ID3D11RasterizerState> raster;
    unsigned capacity=0;
    bool Begin(const float* projection,const float* position,const float* lightDirection,float farClip) {
        using namespace DirectX;
        XMFLOAT4X4 matrix;memcpy(&matrix,projection,sizeof(matrix));
        XMVECTOR determinant;XMMATRIX inv=XMMatrixInverse(&determinant,XMLoadFloat4x4(&matrix));
        valid=std::isfinite(XMVectorGetX(determinant)) && fabs(XMVectorGetX(determinant))>1e-12f;
        XMStoreFloat4x4(&inverse,inv);memcpy(&eye,position,sizeof(eye));memcpy(&direction,lightDirection,sizeof(direction));
        radius=std::clamp(farClip*.4f,80.f,1500.f);vertices.clear();batches.clear();return valid;
    }
    bool Initialize(ID3D11Device* device) {
        if(attempted)return ready; attempted=true;
        const char* source="cbuffer Matrix:register(b0){row_major float4x4 transform;} struct V { float4 p:SV_Position; float2 uv:TEXCOORD0; }; V VS(float3 p:POSITION,float2 uv:TEXCOORD0){V o;o.p=mul(float4(p,1),transform);o.uv=uv;return o;} Texture2D mask:register(t0);SamplerState samp:register(s0);void PS(V i){clip(mask.Sample(samp,i.uv).a-.5);}";
        ComPtr<ID3DBlob> code,error;
        HRESULT hr=D3DCompile(source,strlen(source),"Shadow map",nullptr,nullptr,"VS","vs_4_0",0,0,&code,&error);
        if(FAILED(hr)){if(error)Log(static_cast<const char*>(error->GetBufferPointer()),hr);return false;}
        if(FAILED(device->CreateVertexShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader)))return false;
        D3D11_INPUT_ELEMENT_DESC e[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if(FAILED(device->CreateInputLayout(e,2,code->GetBufferPointer(),code->GetBufferSize(),&layout)))return false;
        code.Reset();error.Reset();
        hr=D3DCompile(source,strlen(source),"Shadow mask",nullptr,nullptr,"PS","ps_4_0",0,0,&code,&error);
        if(FAILED(hr) || FAILED(device->CreatePixelShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&maskShader)))return false;
        D3D11_SAMPLER_DESC sampler{};sampler.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;sampler.MaxLOD=0;
        if(FAILED(device->CreateSamplerState(&sampler,&maskSampler)))return false;
        D3D11_TEXTURE2D_DESC td{};td.Width=td.Height=Size;td.MipLevels=td.ArraySize=1;td.SampleDesc.Count=1;td.Format=DXGI_FORMAT_R32_TYPELESS;td.BindFlags=D3D11_BIND_DEPTH_STENCIL|D3D11_BIND_SHADER_RESOURCE;
        D3D11_DEPTH_STENCIL_VIEW_DESC dd{};dd.Format=DXGI_FORMAT_D32_FLOAT;dd.ViewDimension=D3D11_DSV_DIMENSION_TEXTURE2D;
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=DXGI_FORMAT_R32_FLOAT;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;
        if(FAILED(device->CreateTexture2D(&td,nullptr,&texture)) || FAILED(device->CreateDepthStencilView(texture.Get(),&dd,&dsv)) || FAILED(device->CreateShaderResourceView(texture.Get(),&sd,&srv)))return false;
        D3D11_BUFFER_DESC cb{};cb.ByteWidth=64;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(FAILED(device->CreateBuffer(&cb,nullptr,&constants)))return false;
        D3D11_DEPTH_STENCIL_DESC ds{};ds.DepthEnable=TRUE;ds.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;ds.DepthFunc=D3D11_COMPARISON_LESS_EQUAL;
        if(FAILED(device->CreateDepthStencilState(&ds,&depth)))return false;
        D3D11_RASTERIZER_DESC rd{};rd.FillMode=D3D11_FILL_SOLID;rd.CullMode=D3D11_CULL_NONE;rd.DepthClipEnable=TRUE;rd.DepthBias=100;rd.SlopeScaledDepthBias=1.5f;
        ready=SUCCEEDED(device->CreateRasterizerState(&rd,&raster));return ready;
    }
    bool Render(ID3D11Device* device,ID3D11DeviceContext* gpu) {
        using namespace DirectX;
        if(!valid || vertices.empty())return true;
        if(!Initialize(device))return false;
        XMMATRIX inv=XMLoadFloat4x4(&inverse);
        XMVECTOR position=XMLoadFloat3(&eye);
        XMVECTOR farCenter=XMVector3TransformCoord(XMVectorSet(0,0,1,1),inv);
        XMVECTOR forward=XMVector3Normalize(farCenter-position);
        XMVECTOR center=position+forward*(radius*.65f);
        XMVECTOR dir=XMLoadFloat3(&direction);
        if(XMVectorGetX(XMVector3LengthSq(dir))<.0001f)dir=XMVectorSet(.4f,.25f,-1,0);
        dir=XMVector3Normalize(dir);XMVECTOR up=XMVectorSet(0,0,1,0);
        if(fabs(XMVectorGetX(XMVector3Dot(dir,up)))>.95f)up=XMVectorSet(0,1,0,0);
        XMVECTOR right=XMVector3Normalize(XMVector3Cross(up,dir));up=XMVector3Cross(dir,right);
        float texel=2*radius/Size;
        float x=XMVectorGetX(XMVector3Dot(center,right)),y=XMVectorGetX(XMVector3Dot(center,up));
        center+=right*(floorf(x/texel+.5f)*texel-x)+up*(floorf(y/texel+.5f)*texel-y);
        XMMATRIX view=XMMatrixLookAtLH(center-dir*(radius*3),center,up);
        // Include the receiver region as well as caster geometry. Fitting only
        // to casters clips roads behind them out of the shadow comparison.
        float nearZ=radius,farZ=radius*5;
        for(const auto& v:vertices){XMFLOAT3 p;XMStoreFloat3(&p,XMVector3TransformCoord(XMLoadFloat3(&v.position),view));nearZ=std::min(nearZ,p.z);farZ=std::max(farZ,p.z);}
        depthSpan=farZ-nearZ+20;
        XMStoreFloat4x4(&light,view*XMMatrixOrthographicOffCenterLH(-radius,radius,-radius,radius,nearZ-10,farZ+10));
        unsigned bytes=unsigned(vertices.size()*sizeof(Vertex));
        if(bytes>capacity){buffer.Reset();D3D11_BUFFER_DESC bd{};bd.ByteWidth=bytes;bd.Usage=D3D11_USAGE_DYNAMIC;bd.BindFlags=D3D11_BIND_VERTEX_BUFFER;bd.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;if(FAILED(device->CreateBuffer(&bd,nullptr,&buffer)))return false;capacity=bytes;}
        D3D11_MAPPED_SUBRESOURCE m{};if(FAILED(gpu->Map(buffer.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m)))return false;
        memcpy(m.pData,vertices.data(),bytes);gpu->Unmap(buffer.Get(),0);
        ID3D11ShaderResourceView* empty[4]={};gpu->PSSetShaderResources(0,4,empty);
        gpu->OMSetRenderTargets(0,nullptr,dsv.Get());gpu->ClearDepthStencilView(dsv.Get(),D3D11_CLEAR_DEPTH,1,0);
        gpu->OMSetDepthStencilState(depth.Get(),0);gpu->RSSetState(raster.Get());
        D3D11_VIEWPORT vp{0,0,float(Size),float(Size),0,1};gpu->RSSetViewports(1,&vp);
        unsigned stride=sizeof(Vertex),offset=0;gpu->IASetVertexBuffers(0,1,buffer.GetAddressOf(),&stride,&offset);
        gpu->IASetInputLayout(layout.Get());gpu->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        gpu->UpdateSubresource(constants.Get(),0,nullptr,&light,0,0);gpu->VSSetConstantBuffers(0,1,constants.GetAddressOf());
        gpu->VSSetShader(shader.Get(),nullptr,0);gpu->PSSetSamplers(0,1,maskSampler.GetAddressOf());
        for(const auto& batch:batches){auto* mask=batch.mask.Get();gpu->PSSetShaderResources(0,1,&mask);gpu->PSSetShader(mask?maskShader.Get():nullptr,nullptr,0);gpu->Draw(batch.count,batch.first);}
        gpu->PSSetShaderResources(0,4,empty);return true;
    }
};
