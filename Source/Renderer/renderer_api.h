#ifndef REBORN_RENDERER_API_H
#define REBORN_RENDERER_API_H
#include <stdint.h>

// Plain C ABI: no DirectDraw types, STL objects, or ownership cross the DLL boundary.
struct RBVertex { float x, y, z, rhw; uint32_t diffuse, specular; float u, v; };
struct RBShadowVertex { float x,y,z,u,v; };
struct RBPointLight { float x,y,z,radius; float red,green,blue,intensity; };
struct RBState {
    uint32_t depthEnable, depthWrite, depthFunc, blendEnable, srcBlend, dstBlend;
    uint32_t alphaTest, alphaFunc, alphaRef, cull, fogEnable, fogColor;
    uint32_t colorOp, colorArg1, colorArg2, alphaOp, alphaArg1, alphaArg2;
    uint32_t addressU, addressV, linearFilter, colorKey;
};
struct RBTexture {
    uint64_t id;
    uint32_t width, height, pitch, bits, redMask, greenMask, blueMask, alphaMask;
    const void* pixels;
    const uint32_t* palette; // 256 RGBA entries, if paletted
    uint32_t colorKeyEnabled, colorKeyRGB;
    uint32_t scaleEdges; // 1: outlined font reconstruction; 2: preserve artwork colors. UVs unchanged.
};
struct RBApi {
    uint32_t version;
    void* (*create)(void* window, uint32_t width, uint32_t height);
    void (*destroy)(void* context);
    int (*begin)(void* context, uint32_t width, uint32_t height, uint32_t clearFlags, uint32_t color);
    int (*hasTexture)(void* context, uint64_t id);
    int (*uploadTexture)(void* context, const RBTexture* texture);
    int (*draw)(void* context, uint32_t primitive, const RBVertex* vertices, uint32_t vertexCount,
                const uint16_t* indices, uint32_t indexCount, const RBState* state, uint64_t texture);
    int (*present)(void* context, uint32_t vsync);
    int (*capture)(void* context, const char* filename);
    int (*configure)(void* context, uint32_t samples, uint32_t filtering, uint32_t vsync);
    int (*bloom)(void* context, float strength);
    int (*beginLighting)(void* context, const float* viewProjection, const float* eye, const float* direction, float farClip);
    void (*addTriangle)(void* context, const float* a, const float* b, const float* c);
    int (*lighting)(void* context, float shadowOpacity, float aoStrength);
    void (*addMaskedTriangle)(void* context, const RBShadowVertex* vertices, uint64_t texture);
    void (*addPointLight)(void* context, const RBPointLight* light);
    int (*pointLights)(void* context);
    int (*textureColor)(void* context, uint64_t texture, float* rgb);
    int (*uploadFont)(void* context, uint64_t texture, uint32_t width, uint32_t height, const uint16_t* glyphs, uint32_t count);
    int (*measureFont)(uint16_t* characterWidthPairs, uint32_t count, uint32_t height);
};
typedef const RBApi* (*RBGetApiFn)(uint32_t version);
#endif
