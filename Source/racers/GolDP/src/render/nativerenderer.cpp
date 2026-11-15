#include "render/gold3drenderdevice.h"
#include "render/goldrawdpstate.h"
#include "surface/gold3dtexture.h"
#include "device/golpalettebase.h"
#include "golerror.h"
#include "diagnosticlog.h"
#include "golmodelentity.h"
#include "golmodelbase.h"
#include "golmaterial.h"
#include "golmodelmaterialtable.h"
#include "../../../../Renderer/renderer_api.h"
#include "../../../LEGORacers/include/util/settingsfile.h"
#include <stdio.h>
#include <string.h>

// Font assets are loaded before the first native frame. Retain their glyph
// metadata until that texture revision is uploaded, including context rebuilds.
struct NativeFontRecord {
    GolTexture* texture;
    unsigned __int64 revision;
    unsigned short* glyphs;
    unsigned count;
};
static NativeFontRecord g_nativeFonts[128] = {};
static unsigned g_nativeFontSlot = 0;

void GolD3DRenderDevice::ReleaseNative()
{
    if (m_nativeContext) m_nativeApi->destroy(m_nativeContext);
    m_nativeContext = NULL;
    m_nativeApi = NULL;
    if (m_nativeLibrary) FreeLibrary(m_nativeLibrary);
    m_nativeLibrary = NULL;
}

void GolD3DRenderDevice::BeginNative(unsigned flags)
{
    if (m_renderTargetInfo != m_primaryRenderTarget) return;
    if (!m_nativeContext) {
        char path[MAX_PATH], selection[32];
        GetModuleFileNameA(NULL,path,MAX_PATH);
        char* name = strrchr(path,'\\');
        if (!name || name-path > MAX_PATH-40) return;
        strcpy(name+1,"Reborn.ini");
        GetPrivateProfileStringA("Video","Renderer","legacy",selection,sizeof(selection),path);
        if (_stricmp(selection,"d3d11")) return;
        strcpy(name+1,"RebornRenderer.dll");
        m_nativeLibrary = LoadLibraryA(path);
        RBGetApiFn get = m_nativeLibrary ? reinterpret_cast<RBGetApiFn>(GetProcAddress(m_nativeLibrary,"RBGetApi")) : NULL;
        m_nativeApi = get ? get(8) : NULL;
        if (!m_nativeApi) GOL_FATALERROR_MESSAGE("Unable to load native renderer preview");
        m_nativeContext = m_nativeApi->create(m_drawState->m_hWnd,m_renderTargetInfo->GetWidth(),m_renderTargetInfo->GetHeight());
        if (!m_nativeContext) GOL_FATALERROR_MESSAGE("Unable to initialize native renderer; see Reborn-D3D11.log");
        m_nativeFrames = 0;
        AppendDiagnosticLog("Reborn-render.log","Native D3D11 effects adapter active.");
    }
    if ((m_nativeFrames % 30) == 0) {
        SettingsFile settings("Reborn.ini"), legacy("dgVoodoo.conf");
        const char* aa[]={"off","2x","4x","8x"}; const unsigned samples[]={1,2,4,8};
        const char* af[]={"appdriven","bilinear","trilinear","4","8","16"}; const unsigned filters[]={0,1,2,4,8,16};
        const char* sync[]={"false","true"};const char* ao[]={"off","subtle","medium","strong"};const float strengths[]={0,.6f,1.f,1.6f};
        const char* shadows[]={"off","cars","scene"};
        int a=settings.ReadChoice("Video","Antialiasing",aa,4,legacy.ReadChoice("DirectX","Antialiasing",aa,4,2));
        int f=settings.ReadChoice("Video","Filtering",af,6,legacy.ReadChoice("DirectX","Filtering",af,6,5));
        int v=settings.ReadChoice("Video","ForceVerticalSync",sync,2,legacy.ReadChoice("DirectX","ForceVerticalSync",sync,2,1));
        if(!m_nativeApi->configure(m_nativeContext,samples[a],filters[f],v))
            GOL_FATALERROR_MESSAGE("Requested native anti-aliasing mode is unsupported");
        m_nativeAO=strengths[settings.ReadChoice("Video","AmbientOcclusion",ao,4,0)];
        m_nativeShadows=settings.ReadChoice("Video","Shadows",shadows,3,1)==2;
    }
    unsigned clear = (flags&5) ? (((flags&4)?0:1) | ((m_flags&c_flagZBuffer)?2:0)) : 0;
    unsigned color = (unsigned(m_clearColor.m_red)<<16)|(unsigned(m_clearColor.m_grn)<<8)|m_clearColor.m_blu;
    if (!m_nativeApi->begin(m_nativeContext,m_renderTargetInfo->GetWidth(),m_renderTargetInfo->GetHeight(),clear,color))
        GOL_FATALERROR_MESSAGE("Native renderer frame setup failed");
}

void GolD3DRenderDevice::ApplyNativeBloom(float strength)
{
    if(!m_nativeApi->bloom(m_nativeContext,strength)) GOL_FATALERROR_MESSAGE("Native bloom failed; see renderer log");
}

void GolD3DRenderDevice::AddPointLight(const GolVec3& position,float radius,float red,float green,float blue,float intensity)
{
    if(!IsNativeActive() || m_collectingShadows)return;
    RBPointLight light={position.m_x,position.m_y,position.m_z,radius,red,green,blue,intensity};
    m_nativeApi->addPointLight(m_nativeContext,&light);
}

void GolD3DRenderDevice::ApplyPointLights()
{
    if(IsNativeActive() && !m_nativeApi->pointLights(m_nativeContext))
        GOL_FATALERROR_MESSAGE("Native local lighting failed; see renderer log");
}

void GolD3DRenderDevice::AddModelPointLight(GolModelEntity* entity,float radius,float intensity)
{
    if(!IsNativeActive() || !entity || !entity->GetModel(0))return;
    MaterialTable* materials=entity->GetPrimaryMaterialTable();
    if(!materials)materials=entity->GetModel(0)->GetMaterialTable();
    float color[3]={0,0,0};
    if(materials)for(unsigned i=0;i<materials->m_count;i++) {
        GolMaterial* material=static_cast<GolMaterial*>(materials->GetEntry(i));if(!material)continue;
        const GolMaterialColor& a=material->GetAmbient();const GolMaterialColor& d=material->GetDiffuse();
        float tint[3]={float(a.m_red>d.m_red?a.m_red:d.m_red)/255,
            float(a.m_grn>d.m_grn?a.m_grn:d.m_grn)/255,float(a.m_blu>d.m_blu?a.m_blu:d.m_blu)/255};
        if(tint[0]+tint[1]+tint[2]<.01f)tint[0]=tint[1]=tint[2]=1;
        float texture[3]={1,1,1};
        if(material->GetTexture())m_nativeApi->textureColor(m_nativeContext,UploadNativeTexture(material->GetTexture()),texture);
        for(unsigned j=0;j<3;j++)color[j]+=tint[j]*texture[j];
    }
    float peak=color[0];if(color[1]>peak)peak=color[1];if(color[2]>peak)peak=color[2];
    if(peak<.001f)return;
    AddPointLight(entity->GetPosition(),radius,color[0]/peak,color[1]/peak,color[2]/peak,intensity);
}

bool GolD3DRenderDevice::PresentNative(GolRenderTarget* target)
{
    if (!m_nativeContext || target != m_primaryRenderTarget) return false;
    ++m_nativeFrames;
    if (m_nativeFrames == 120 || m_nativeFrames == 600) {
        char prefix[MAX_PATH], path[MAX_PATH];
        DWORD length = GetEnvironmentVariableA("REBORN_NATIVE_CAPTURE",prefix,sizeof(prefix));
        if (length && length < MAX_PATH-24) {
            sprintf(path,"%s-%u.ppm",prefix,m_nativeFrames);
            m_nativeApi->capture(m_nativeContext,path);
        }
    }
    if (!m_nativeApi->present(m_nativeContext,1)) GOL_FATALERROR_MESSAGE("Native renderer presentation failed");
    return true;
}

HRESULT GolD3DRenderDevice::SubmitPrimitive(D3DPRIMITIVETYPE type,DWORD format,void* vertices,DWORD count,DWORD flags)
{
    if (IsNativeActive() && format == D3DFVF_TLVERTEX) return DrawNative(type,vertices,count,NULL,0);
    return m_d3dDevice->DrawPrimitive(type,format,vertices,count,flags);
}
HRESULT GolD3DRenderDevice::SubmitIndexedPrimitive(D3DPRIMITIVETYPE type,DWORD format,void* vertices,DWORD count,WORD* indices,DWORD indexCount,DWORD flags)
{
    if (IsNativeActive() && format == D3DFVF_TLVERTEX) return DrawNative(type,vertices,count,indices,indexCount);
    return m_d3dDevice->DrawIndexedPrimitive(type,format,vertices,count,indices,indexCount,flags);
}

HRESULT GolD3DRenderDevice::DrawNative(D3DPRIMITIVETYPE type,void* vertices,DWORD count,WORD* indices,DWORD indexCount)
{
    RBState state = {};
#define RS(field,value) m_d3dDevice->GetRenderState(value,reinterpret_cast<DWORD*>(&state.field))
#define TS(field,value) m_d3dDevice->GetTextureStageState(0,value,reinterpret_cast<DWORD*>(&state.field))
    RS(depthEnable,D3DRENDERSTATE_ZENABLE); RS(depthWrite,D3DRENDERSTATE_ZWRITEENABLE); RS(depthFunc,D3DRENDERSTATE_ZFUNC);
    RS(blendEnable,D3DRENDERSTATE_ALPHABLENDENABLE); RS(srcBlend,D3DRENDERSTATE_SRCBLEND); RS(dstBlend,D3DRENDERSTATE_DESTBLEND);
    RS(alphaTest,D3DRENDERSTATE_ALPHATESTENABLE); RS(alphaFunc,D3DRENDERSTATE_ALPHAFUNC); RS(alphaRef,D3DRENDERSTATE_ALPHAREF);
    RS(cull,D3DRENDERSTATE_CULLMODE); RS(fogEnable,D3DRENDERSTATE_FOGENABLE); RS(fogColor,D3DRENDERSTATE_FOGCOLOR);
    RS(colorKey,D3DRENDERSTATE_COLORKEYENABLE);
    TS(colorOp,D3DTSS_COLOROP); TS(colorArg1,D3DTSS_COLORARG1); TS(colorArg2,D3DTSS_COLORARG2);
    TS(alphaOp,D3DTSS_ALPHAOP); TS(alphaArg1,D3DTSS_ALPHAARG1); TS(alphaArg2,D3DTSS_ALPHAARG2);
    TS(addressU,D3DTSS_ADDRESSU); TS(addressV,D3DTSS_ADDRESSV); TS(linearFilter,D3DTSS_MAGFILTER);
    state.linearFilter = state.linearFilter != D3DTFG_POINT;
    if (m_currentTexture && (m_currentTexture->GetTextureFlags() & GolTexture::c_textureFlagScaleEdges)) {
        // Font cutouts need fractional coverage at their enlarged edges. The
        // legacy color-key discard otherwise turns filtered edges back into steps.
        state.linearFilter = 1;
        state.colorKey = 0;
        state.alphaTest = 0;
        state.blendEnable = 1;
        state.srcBlend = D3DBLEND_SRCALPHA;
        state.dstBlend = D3DBLEND_INVSRCALPHA;
        state.alphaOp = D3DTOP_MODULATE;
        state.alphaArg1 = D3DTA_TEXTURE;
        state.alphaArg2 = D3DTA_DIFFUSE;
    }
#undef RS
#undef TS
    uint64_t id = UploadNativeTexture(m_currentTexture);
    if (!m_nativeApi->draw(m_nativeContext,type,reinterpret_cast<RBVertex*>(vertices),count,indices,indexCount,&state,id))
        GOL_FATALERROR_MESSAGE("Native draw failed; see Reborn-D3D11.log");
    return D3D_OK;
}

unsigned __int64 GolD3DRenderDevice::UploadNativeTexture(GolTexture* sourceTexture)
{
    uint64_t id = 0;
    if (sourceTexture) {
        GolD3DTexture* texture = static_cast<GolD3DTexture*>(sourceTexture);
        id = texture->GetRevision();
        if (!m_nativeApi->hasTexture(m_nativeContext,id)) {
            RBTexture source = {}; source.id=id;
            source.width=texture->GetWidth(); source.height=texture->GetHeight();
            source.scaleEdges=(texture->GetTextureFlags()&GolTexture::c_textureFlagScaleEdges)!=0;
            if(source.scaleEdges && (texture->GetTextureFlags()&GolTexture::c_textureFlagPreserveColor))source.scaleEdges=2;
            const GolSurfaceFormat& format=texture->GetTextureFormat();
            source.bits=format.m_bitsPerPixel;
            source.redMask=format.m_redBitMask; source.greenMask=format.m_grnBitMask;
            source.blueMask=format.m_bluBitMask; source.alphaMask=format.m_alpBitMask;
            ColorRGBA palette[256] = {};
            if (format.m_paletteMask && texture->GetPalette()) {
                unsigned count=texture->GetPalette()->GetPaletteSize(); if(count>256)count=256;
                // Bulk legacy reads synthesize white for the last palette slot,
                // even when all 256 entries are available. Read actual entries
                // so images using index 255 for black retain their source colors.
                for(unsigned i=0;i<count;++i) {
                    texture->GetPalette()->GetEntry(&palette[i],i);
                    palette[i].m_alp=255;
                }
                source.palette=reinterpret_cast<const uint32_t*>(palette);
            }
            const ColorRGBA& key=texture->GetColorKey();
            source.colorKeyEnabled=(texture->GetTextureFlags()&GolTexture::c_textureFlagColorKeyed)!=0;
            source.colorKeyRGB=(unsigned(key.m_red)<<16)|(unsigned(key.m_grn)<<8)|key.m_blu;
            if(texture->GetTextureFlags()&GolTexture::c_textureFlagBlackColorKey) source.colorKeyRGB=0;
            LegoU8* pixels=NULL; LegoU32 pitch=0;
            texture->LockPixels(&pixels,&pitch,GolSurface::c_lockRequestRead);
            source.pixels=pixels;source.pitch=pitch;
            int uploaded=m_nativeApi->uploadTexture(m_nativeContext,&source);
            texture->UnlockPixels();
            if (!uploaded) GOL_FATALERROR_MESSAGE("Native texture upload failed");
            for(unsigned i=0;i<128;++i) {
                NativeFontRecord& font=g_nativeFonts[i];
                if(font.texture==sourceTexture && font.revision==id && font.glyphs) {
                    m_nativeApi->uploadFont(m_nativeContext,id,texture->GetWidth(),texture->GetHeight(),font.glyphs,font.count);
                    break;
                }
            }
        }
    }
    return id;
}

void GolD3DRenderDevice::UploadNativeFont(GolTexture* texture, const unsigned short* glyphs, unsigned count)
{
    if(!count)return;
    unsigned slot=128;
    for(unsigned i=0;i<128;++i)if(g_nativeFonts[i].texture==texture){slot=i;break;}
    if(slot==128)slot=(g_nativeFontSlot++)%128;
    NativeFontRecord& record=g_nativeFonts[slot];
    delete[] record.glyphs;
    record.texture=texture;record.revision=static_cast<GolD3DTexture*>(texture)->GetRevision();record.count=count;
    record.glyphs=new unsigned short[count*5];
    memcpy(record.glyphs,glyphs,count*5*sizeof(unsigned short));
}

bool GolD3DRenderDevice::MeasureNativeFont(unsigned short* pairs, unsigned count, unsigned height)
{
    char path[MAX_PATH],mode[32];GetModuleFileNameA(NULL,path,MAX_PATH);
    char* name=strrchr(path,'\\');if(!name || name-path>MAX_PATH-40)return false;
    strcpy(name+1,"Reborn.ini");GetPrivateProfileStringA("Video","Renderer","legacy",mode,sizeof(mode),path);
    if(_stricmp(mode,"d3d11"))return false;
    strcpy(name+1,"RebornRenderer.dll");HMODULE library=LoadLibraryA(path);if(!library)return false;
    RBGetApiFn get=reinterpret_cast<RBGetApiFn>(GetProcAddress(library,"RBGetApi"));
    const RBApi* api=get?get(8):NULL;
    bool result=api && api->measureFont(pairs,count,height)!=0;
    FreeLibrary(library);return result;
}
