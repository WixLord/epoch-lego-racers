#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>

// Reconstruct smooth coverage contours from low-resolution outlined artwork.
// Work in premultiplied coverage, so transparent palette colors cannot bleed.
// Filtering removes source-grid corners; a narrow smooth threshold restores
// crisp outlines instead of merely enlarging a blurred bitmap.
inline std::vector<uint32_t> ReconstructContours(const std::vector<uint32_t>& src, unsigned w, unsigned h, unsigned scale=2, bool preserveColor=false) {
    using Field = std::array<float,4>;
    Field peak{};
    std::vector<Field> input(size_t(w)*h), horizontal(input.size()), field(input.size());
    for(size_t i=0;i<src.size();++i) {
        float a=float(src[i]>>24)/255;
        input[i]={float(src[i]&255)/255*a,float((src[i]>>8)&255)/255*a,float((src[i]>>16)&255)/255*a,a};
        if(a>.5f)for(unsigned c=0;c<3;++c)peak[c]=std::max(peak[c],input[i][c]/a);
    }
    const float weights[5]={.003f,.166f,.662f,.166f,.003f};
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(int k=-2;k<=2;++k) {
        const auto& p=input[size_t(y)*w+std::clamp(int(x)+k,0,int(w)-1)];
        for(unsigned c=0;c<4;++c)horizontal[size_t(y)*w+x][c]+=p[c]*weights[k+2];
    }
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x)for(int k=-2;k<=2;++k) {
        const auto& p=horizontal[size_t(std::clamp(int(y)+k,0,int(h)-1))*w+x];
        for(unsigned c=0;c<4;++c)field[size_t(y)*w+x][c]+=p[c]*weights[k+2];
    }
    auto cubic=[](float a,float b,float c,float d,float t) {
        return b+.5f*t*(c-a+t*(2*a-5*b+4*c-d+t*(3*(b-c)+d-a)));
    };
    auto smooth=[](float lo,float hi,float x) {
        float t=std::clamp((x-lo)/(hi-lo),0.f,1.f);return t*t*(3-2*t);
    };
    // Separable cubic interpolation avoids repeating the horizontal work for
    // every output row. This keeps menu atlas construction off the slow path.
    unsigned width=w*scale,height=h*scale;
    std::vector<Field> expanded(size_t(width)*h);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<width;++x) {
        float sx=(x+.5f)/scale-.5f;int ix=int(std::floor(sx));float tx=sx-ix;
        for(unsigned c=0;c<4;++c) {
            float cols[4];for(int i=-1;i<=2;++i)cols[i+1]=field[size_t(y)*w+std::clamp(ix+i,0,int(w)-1)][c];
            expanded[size_t(y)*width+x][c]=cubic(cols[0],cols[1],cols[2],cols[3],tx);
        }
    }
    std::vector<uint32_t> out(size_t(width)*height);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
        float sy=(y+.5f)/scale-.5f;int iy=int(std::floor(sy));float ty=sy-iy;
        Field value{};
        for(unsigned c=0;c<4;++c) {
            float rows[4];
            for(int j=-1;j<=2;++j)rows[j+1]=expanded[size_t(std::clamp(iy+j,0,int(h)-1))*width+x][c];
            value[c]=std::clamp(cubic(rows[0],rows[1],rows[2],rows[3],ty),0.f,1.f);
        }
        float alpha=preserveColor?smooth(.40f,.50f,value[3]):smooth(.35f,.55f,value[3]);
        uint32_t pixel=uint32_t(alpha*255+.5f)<<24;
        for(unsigned c=0;c<3;++c) {
            float color=value[3]>.001f?value[c]/value[3]:0;
            // Sharpen icon interiors relative to their own brightness, so dim
            // state artwork keeps its color instead of being crushed to black.
            color=preserveColor?(peak[c]>.001f?peak[c]*smooth(.28f,.72f,color/peak[c]):0):smooth(.48f,.78f,color);
            pixel|=uint32_t(color*255+.5f)<<(c*8);
        }
        out[size_t(y)*width+x]=pixel;
    }
    return out;
}

// Edge-directed enlargement for indexed-style artwork. Each pass rounds
// stair steps without blending the palette or erasing single-pixel strokes.
inline std::vector<uint32_t> ScaleEdges2x(const std::vector<uint32_t>& src, unsigned w, unsigned h) {
    std::vector<uint32_t> dst(size_t(w)*h*4);
    for(unsigned y=0;y<h;++y) for(unsigned x=0;x<w;++x) {
        uint32_t e=src[size_t(y)*w+x];
        uint32_t b=src[size_t(y?y-1:y)*w+x], d=src[size_t(y)*w+(x?x-1:x)];
        uint32_t f=src[size_t(y)*w+(x+1<w?x+1:x)], g=src[size_t(y+1<h?y+1:y)*w+x];
        size_t i=size_t(y*2)*w*2+x*2;
        dst[i]=(d==b && b!=f && d!=g)?d:e;
        dst[i+1]=(b==f && b!=d && f!=g)?f:e;
        dst[i+w*2]=(d==g && d!=b && g!=f)?d:e;
        dst[i+w*2+1]=(g==f && d!=g && b!=f)?f:e;
    }
    return dst;
}
