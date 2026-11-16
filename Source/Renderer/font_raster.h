#pragma once
#include <objidl.h>
#include <gdiplus.h>
#include <memory>
#include <string>

// Render a private OpenType font; no installation or font-file modification.

    struct FontResources {
        ULONG_PTR token=0;
        std::unique_ptr<Gdiplus::PrivateFontCollection> collection;
        std::unique_ptr<Gdiplus::FontFamily> family;
        FontResources() {
            Gdiplus::GdiplusStartupInput startup;
            if(Gdiplus::GdiplusStartup(&token,&startup,nullptr)!=Gdiplus::Ok)return;
            wchar_t path[MAX_PATH];GetModuleFileNameW(nullptr,path,MAX_PATH);
            wchar_t* slash=wcsrchr(path,L'\\');if(!slash)return;
            slash[1]=0;
            std::wstring directory(path),ini=directory+L"Reborn.ini";
            wchar_t selected[MAX_PATH];
            GetPrivateProfileStringW(L"UI",L"FontFile",L"Fonts\\BradBunR.ttf",selected,MAX_PATH,ini.c_str());
            if(!selected[0] || !_wcsicmp(selected,L"original"))return;
            std::wstring filename=selected;
            if(filename.find(L':')==std::wstring::npos && filename[0]!=L'\\')filename=directory+filename;
            collection=std::make_unique<Gdiplus::PrivateFontCollection>();
            if(collection->AddFontFile(filename.c_str())!=Gdiplus::Ok || collection->GetFamilyCount()<1)return;
            family=std::make_unique<Gdiplus::FontFamily>();int found=0;
            if(collection->GetFamilies(1,family.get(),&found)!=Gdiplus::Ok || !found)family.reset();
        }
        ~FontResources(){family.reset();collection.reset();if(token)Gdiplus::GdiplusShutdown(token);}
    };
inline int MeasureFont(uint16_t* pairs,unsigned count,unsigned height) {
    FontResources resources;if(!resources.family || !pairs || !height || height>128 || count>1024)return 0;
    Gdiplus::GraphicsPath capital;
    capital.AddString(L"H",1,resources.family.get(),Gdiplus::FontStyleRegular,64,Gdiplus::PointF(0,0),Gdiplus::StringFormat::GenericTypographic());
    Gdiplus::RectF cap;capital.GetBounds(&cap);if(cap.Height<=0)return 0;
    float scale=float(height)*.66f/cap.Height;
    float border=std::max(1.f,float(height)*2*.060f);
    for(unsigned i=0;i<count;++i) {
        wchar_t ch=wchar_t(pairs[i*2]);
        Gdiplus::GraphicsPath path;
        path.AddString(&ch,1,resources.family.get(),Gdiplus::FontStyleRegular,64,Gdiplus::PointF(0,0),Gdiplus::StringFormat::GenericTypographic());
        Gdiplus::RectF b;path.GetBounds(&b);
        float advance=ch==L' '?height*.30f:b.Width*scale+border+.5f;
        pairs[i*2+1]=uint16_t(std::clamp(int(std::ceil(advance)),2,240));
    }
    return 1;
}
inline bool RasterizeFont(unsigned width,unsigned height,const uint16_t* glyphs,unsigned count,std::vector<uint32_t>& pixels) {
    // GDI+ shutdown must run before FreeLibrary, never from a DLL static
    // destructor under the Windows loader lock (focus changes unload us).
    FontResources resources;
    if(!resources.family || !glyphs || !count || !width || !height)return false;
    Gdiplus::Bitmap bitmap(width,height,PixelFormat32bppARGB);
    Gdiplus::Graphics graphics(&bitmap);
    graphics.Clear(Gdiplus::Color(0,0,0,0));
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    Gdiplus::GraphicsPath capital;
    capital.AddString(L"H",1,resources.family.get(),Gdiplus::FontStyleRegular,64,Gdiplus::PointF(0,0),Gdiplus::StringFormat::GenericTypographic());
    Gdiplus::RectF cap;capital.GetBounds(&cap);if(cap.Height<=0)return false;
    // The legacy cells have different proportions from a real font. Choose
    // one uniform em scale for the atlas rather than squeezing each letter
    // horizontally while stretching it to the old full cell height.
    float sharedScale=1000.f;
    for(unsigned i=0;i<count;++i) {
        const auto* g=glyphs+i*5;wchar_t ch=wchar_t(g[0]);
        if(!((ch>=L'A' && ch<=L'Z') || (ch>=L'0' && ch<=L'9')))continue;
        Gdiplus::GraphicsPath sample;
        sample.AddString(&ch,1,resources.family.get(),Gdiplus::FontStyleRegular,64,Gdiplus::PointF(0,0),Gdiplus::StringFormat::GenericTypographic());
        Gdiplus::RectF b;sample.GetBounds(&b);if(b.Width<=0 || b.Height<=0)continue;
        float border=std::max(1.f,float(g[4])*2*.060f);
        sharedScale=std::min(sharedScale,std::min((float(g[3])*2-2*border-1)/b.Width,(float(g[4])*2-2*border-2)/cap.Height));
    }
    Gdiplus::SolidBrush white(Gdiplus::Color(255,255,255,255));
    for(unsigned i=0;i<count;++i) {
        const auto* g=glyphs+i*5;wchar_t character=wchar_t(g[0]);
        if(character==L' ' || !g[3] || !g[4])continue;
        Gdiplus::GraphicsPath path;
        if(path.AddString(&character,1,resources.family.get(),Gdiplus::FontStyleRegular,64,Gdiplus::PointF(0,0),Gdiplus::StringFormat::GenericTypographic())!=Gdiplus::Ok)continue;
        Gdiplus::RectF bounds;path.GetBounds(&bounds);if(bounds.Width<=0 || bounds.Height<=0)continue;
        float x=float(g[1])*2,y=float(g[2])*2,w=float(g[3])*2,h=float(g[4])*2;
        float border=std::max(1.f,std::min(h,cap.Height*sharedScale)*.060f);
        float scale=std::min(sharedScale,std::min((w-2*border-1)/bounds.Width,(h-2*border-2)/bounds.Height));
        if(scale<=0)continue;
        float top=y+(h-cap.Height*scale)*.5f+(bounds.Y-cap.Y)*scale;
        top=std::clamp(top,y+border,y+h-border-bounds.Height*scale);
        Gdiplus::Matrix transform(scale,0,0,scale,x+(w-bounds.Width*scale)*.5f-bounds.X*scale,top-bounds.Y*scale);
        path.Transform(&transform);
        graphics.SetClip(Gdiplus::RectF(x,y,w,h));
        Gdiplus::Pen outline(Gdiplus::Color(255,0,0,0),border*2);outline.SetLineJoin(Gdiplus::LineJoinRound);
        graphics.DrawPath(&outline,&path);graphics.FillPath(&white,&path);
        graphics.ResetClip();
    }
    graphics.Flush(Gdiplus::FlushIntentionSync);
    Gdiplus::BitmapData data{};Gdiplus::Rect rect(0,0,width,height);
    if(bitmap.LockBits(&rect,Gdiplus::ImageLockModeRead,PixelFormat32bppARGB,&data)!=Gdiplus::Ok)return false;
    pixels.resize(size_t(width)*height);
    for(unsigned y=0;y<height;++y) {
        const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const uint8_t*>(data.Scan0)+ptrdiff_t(y)*data.Stride);
        for(unsigned x=0;x<width;++x){uint32_t p=row[x];pixels[size_t(y)*width+x]=(p&0xff00ff00)|((p&255)<<16)|((p>>16)&255);}
    }
    bitmap.UnlockBits(&data);return true;
}
