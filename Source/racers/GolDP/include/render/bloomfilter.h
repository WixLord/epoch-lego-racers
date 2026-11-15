#ifndef BLOOMFILTER_H
#define BLOOMFILTER_H

#include <d3d.h>

// A low-resolution highlight filter for the fixed-function renderer. Call outside
// BeginScene/EndScene; it restores every device state that it changes.
class BloomFilter {
public:
	BloomFilter() : m_surface(NULL), m_texture(NULL), m_width(0), m_height(0)
	{
		m_blurSurfaces[0] = m_blurSurfaces[1] = NULL;
		m_blurTextures[0] = m_blurTextures[1] = NULL;
	}
	~BloomFilter() { Release(); }
	void Release()
	{
		for (int i = 0; i < 2; ++i) {
			if (m_blurTextures[i]) {
				m_blurTextures[i]->Release();
			}
			if (m_blurSurfaces[i]) {
				m_blurSurfaces[i]->Release();
			}
			m_blurTextures[i] = NULL;
			m_blurSurfaces[i] = NULL;
		}
		if (m_texture) {
			m_texture->Release();
		}
		if (m_surface) {
			m_surface->Release();
		}
		m_texture = NULL;
		m_surface = NULL;
	}

	bool Apply(
		LPDIRECTDRAW4 draw,
		LPDIRECT3DDEVICE3 device,
		LPDIRECT3DVIEWPORT3 viewport,
		LPDIRECTDRAWSURFACE4 source,
		float strength
	)
	{
		if (strength <= 0.0f) {
			return true;
		}
		DDSURFACEDESC2 sourceDesc = {};
		sourceDesc.dwSize = sizeof(sourceDesc);
		if (FAILED(source->GetSurfaceDesc(&sourceDesc))) {
			return false;
		}
		DWORD desiredWidth = 256;
		while (desiredWidth < 256.0f * sourceDesc.dwWidth / sourceDesc.dwHeight && desiredWidth < 2048) {
			desiredWidth *= 2;
		}
		if (m_surface && desiredWidth != m_width) {
			Release();
		}
		if (!m_surface) {
			DDSURFACEDESC2 desc = {};
			desc.dwSize = sizeof(desc);
			desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
			m_width = desc.dwWidth = desiredWidth;
			m_height = desc.dwHeight = 256;
			desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY;
			desc.ddpfPixelFormat = sourceDesc.ddpfPixelFormat;
			if (FAILED(draw->CreateSurface(&desc, &m_surface, NULL))) {
				return false;
			}
			if (FAILED(m_surface->QueryInterface(IID_IDirect3DTexture2, reinterpret_cast<void**>(&m_texture)))) {
				Release();
				return false;
			}
			// Blur in 32-bit targets even when the game uses 16-bit colour, so each
			// additive sample does not accumulate coarse 565 quantisation bands.
			desc.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE;
			desc.ddpfPixelFormat = {};
			desc.ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
			desc.ddpfPixelFormat.dwFlags = DDPF_RGB;
			desc.ddpfPixelFormat.dwRGBBitCount = 32;
			desc.ddpfPixelFormat.dwRBitMask = 0xff0000;
			desc.ddpfPixelFormat.dwGBitMask = 0x00ff00;
			desc.ddpfPixelFormat.dwBBitMask = 0x0000ff;
			for (int i = 0; i < 2; ++i) {
				if (FAILED(draw->CreateSurface(&desc, &m_blurSurfaces[i], NULL)) ||
					FAILED(m_blurSurfaces[i]
							   ->QueryInterface(IID_IDirect3DTexture2, reinterpret_cast<void**>(&m_blurTextures[i])))) {
					Release();
					return false;
				}
			}
		}
		RECT targetRect = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
		RECT sourceRect = {0, 0, static_cast<LONG>(sourceDesc.dwWidth), static_cast<LONG>(sourceDesc.dwHeight)};
		if (FAILED(m_surface->Blt(&targetRect, source, &sourceRect, DDBLT_WAIT, NULL))) {
			return false;
		}

		const D3DRENDERSTATETYPE states[] = {
			D3DRENDERSTATE_ZENABLE,
			D3DRENDERSTATE_ZWRITEENABLE,
			D3DRENDERSTATE_ALPHABLENDENABLE,
			D3DRENDERSTATE_SRCBLEND,
			D3DRENDERSTATE_DESTBLEND,
			D3DRENDERSTATE_ALPHATESTENABLE,
			D3DRENDERSTATE_FOGENABLE,
			D3DRENDERSTATE_CULLMODE,
			D3DRENDERSTATE_TEXTUREFACTOR,
			D3DRENDERSTATE_STENCILENABLE,
			D3DRENDERSTATE_COLORKEYENABLE
		};
		const DWORD values[] =
			{FALSE, FALSE, TRUE, D3DBLEND_ONE, D3DBLEND_ONE, FALSE, FALSE, D3DCULL_NONE, 0xffb0b0b0, FALSE, FALSE};
		const D3DTEXTURESTAGESTATETYPE stages[] = {
			D3DTSS_COLOROP,
			D3DTSS_COLORARG1,
			D3DTSS_COLORARG2,
			D3DTSS_ALPHAOP,
			D3DTSS_ALPHAARG1,
			D3DTSS_ALPHAARG2,
			D3DTSS_TEXCOORDINDEX,
			D3DTSS_ADDRESSU,
			D3DTSS_ADDRESSV,
			D3DTSS_MAGFILTER,
			D3DTSS_MINFILTER,
			D3DTSS_MIPFILTER
		};
		const DWORD stageValues[2][12] = {
			{D3DTOP_SUBTRACT,
			 D3DTA_TEXTURE,
			 D3DTA_TFACTOR,
			 D3DTOP_SELECTARG1,
			 D3DTA_DIFFUSE,
			 D3DTA_DIFFUSE,
			 0,
			 D3DTADDRESS_CLAMP,
			 D3DTADDRESS_CLAMP,
			 D3DTFG_LINEAR,
			 D3DTFN_LINEAR,
			 D3DTFP_NONE},
			{D3DTOP_MODULATE,
			 D3DTA_CURRENT,
			 D3DTA_DIFFUSE,
			 D3DTOP_SELECTARG1,
			 D3DTA_CURRENT,
			 D3DTA_CURRENT,
			 0,
			 D3DTADDRESS_CLAMP,
			 D3DTADDRESS_CLAMP,
			 D3DTFG_LINEAR,
			 D3DTFN_LINEAR,
			 D3DTFP_NONE}
		};
		DWORD savedStates[11], savedStages[2][12], savedStage2;
		D3DVIEWPORT2 savedViewport = {};
		savedViewport.dwSize = sizeof(savedViewport);
		if (FAILED(viewport->GetViewport2(&savedViewport))) {
			return false;
		}
		for (int i = 0; i < 11; ++i) {
			if (FAILED(device->GetRenderState(states[i], &savedStates[i]))) {
				return false;
			}
		}
		for (int stage = 0; stage < 2; ++stage) {
			for (int i = 0; i < 12; ++i) {
				if (FAILED(device->GetTextureStageState(stage, stages[i], &savedStages[stage][i]))) {
					return false;
				}
			}
		}
		if (FAILED(device->GetTextureStageState(2, D3DTSS_COLOROP, &savedStage2))) {
			return false;
		}
		LPDIRECT3DTEXTURE2 savedTextures[2] = {NULL, NULL};
		if (FAILED(device->GetTexture(0, &savedTextures[0]))) {
			return false;
		}
		if (FAILED(device->GetTexture(1, &savedTextures[1]))) {
			if (savedTextures[0]) {
				savedTextures[0]->Release();
			}
			return false;
		}

		D3DVIEWPORT2 fullViewport = savedViewport;
		fullViewport.dwX = fullViewport.dwY = 0;
		fullViewport.dwWidth = sourceDesc.dwWidth;
		fullViewport.dwHeight = sourceDesc.dwHeight;
		bool success = SUCCEEDED(viewport->SetViewport2(&fullViewport));
		for (int i = 0; i < 11; ++i) {
			success = SUCCEEDED(device->SetRenderState(states[i], values[i])) && success;
		}
		for (int stage = 0; stage < 2; ++stage) {
			for (int i = 0; i < 12; ++i) {
				success = SUCCEEDED(device->SetTextureStageState(stage, stages[i], stageValues[stage][i])) && success;
			}
		}
		success = SUCCEEDED(device->SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE)) && success;
		success = SUCCEEDED(device->SetTexture(0, m_texture)) && success;
		success = SUCCEEDED(device->SetTexture(1, m_texture)) && success;
		DWORD passes = 0;
		success = SUCCEEDED(device->ValidateDevice(&passes)) && success;
		D3DVIEWPORT2 blurViewport = fullViewport;
		blurViewport.dwWidth = m_width;
		blurViewport.dwHeight = m_height;
		success = SUCCEEDED(viewport->SetViewport2(&blurViewport)) && success;
		success = SUCCEEDED(device->SetRenderTarget(m_blurSurfaces[0], 0)) && success;
		success = SUCCEEDED(device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE)) && success;
		if (success && SUCCEEDED(device->BeginScene())) {
			success =
				SUCCEEDED(DrawQuad(device, static_cast<float>(m_width), static_cast<float>(m_height), 0xffffffff, 0, 0)
				);
			success = SUCCEEDED(device->EndScene()) && success;
		}
		else {
			success = false;
		}

		// Extract highlights once, then convolve in each axis. Adjacent taps overlap
		// instead of drawing separated copies of the sharp source highlights.
		const DWORD weights[15] = {2, 5, 9, 14, 21, 28, 32, 33, 32, 28, 21, 14, 9, 5, 2};
		success = SUCCEEDED(device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1)) && success;
		success = SUCCEEDED(device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, TRUE)) && success;
		for (int axis = 0; axis < 2 && success; ++axis) {
			LPDIRECTDRAWSURFACE4 destination = axis == 0 ? m_blurSurfaces[1] : source;
			success = SUCCEEDED(device->SetTexture(0, m_blurTextures[axis])) && success;
			success = SUCCEEDED(device->SetTexture(1, m_blurTextures[axis])) && success;
			success = SUCCEEDED(device->SetRenderTarget(destination, 0)) && success;
			success = SUCCEEDED(viewport->SetViewport2(axis == 0 ? &blurViewport : &fullViewport)) && success;
			if (axis == 0) {
				D3DRECT clearRect = {0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height)};
				success = SUCCEEDED(viewport->Clear2(1, &clearRect, D3DCLEAR_TARGET, 0, 1.0f, 0)) && success;
			}
			if (success && SUCCEEDED(device->BeginScene())) {
				for (int tap = 0; tap < 15; ++tap) {
					DWORD weight = static_cast<DWORD>(weights[tap] * (axis == 0 ? 1.0f : strength) + 0.5f);
					if (weight > 255) {
						weight = 255;
					}
					DWORD color = 0xff000000 | (weight << 16) | (weight << 8) | weight;
					float offset = static_cast<float>(tap - 7) / m_height;
					float u = axis == 0 ? offset * sourceDesc.dwHeight / sourceDesc.dwWidth : 0;
					float v = axis == 1 ? offset : 0;
					float width = static_cast<float>(axis == 0 ? m_width : sourceDesc.dwWidth);
					float height = static_cast<float>(axis == 0 ? m_height : sourceDesc.dwHeight);
					success = SUCCEEDED(DrawQuad(device, width, height, color, u, v)) && success;
				}
				success = SUCCEEDED(device->EndScene()) && success;
			}
			else {
				success = false;
			}
		}
		// Restore the scene target even if any intermediate pass was unsupported.
		success = SUCCEEDED(device->SetRenderTarget(source, 0)) && success;
		for (int i = 0; i < 11; ++i) {
			device->SetRenderState(states[i], savedStates[i]);
		}
		for (int stage = 0; stage < 2; ++stage) {
			for (int i = 0; i < 12; ++i) {
				device->SetTextureStageState(stage, stages[i], savedStages[stage][i]);
			}
			device->SetTexture(stage, savedTextures[stage]);
			if (savedTextures[stage]) {
				savedTextures[stage]->Release();
			}
		}
		device->SetTextureStageState(2, D3DTSS_COLOROP, savedStage2);
		viewport->SetViewport2(&savedViewport);
		return success;
	}

private:
	static HRESULT DrawQuad(LPDIRECT3DDEVICE3 device, float width, float height, DWORD color, float u, float v)
	{
		D3DTLVERTEX vertices[4] = {
			{-0.5f, -0.5f, 0.0f, 1.0f, color, 0, u, v},
			{width - 0.5f, -0.5f, 0.0f, 1.0f, color, 0, 1.0f + u, v},
			{-0.5f, height - 0.5f, 0.0f, 1.0f, color, 0, u, 1.0f + v},
			{width - 0.5f, height - 0.5f, 0.0f, 1.0f, color, 0, 1.0f + u, 1.0f + v}
		};
		return device
			->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, vertices, 4, D3DDP_DONOTLIGHT | D3DDP_DONOTCLIP);
	}
	BloomFilter(const BloomFilter&);
	BloomFilter& operator=(const BloomFilter&);
	LPDIRECTDRAWSURFACE4 m_surface;
	LPDIRECT3DTEXTURE2 m_texture;
	LPDIRECTDRAWSURFACE4 m_blurSurfaces[2];
	LPDIRECT3DTEXTURE2 m_blurTextures[2];
	DWORD m_width;
	DWORD m_height;
};

#endif
