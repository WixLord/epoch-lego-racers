#include "render/shadowvolumerenderer.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

ShadowVolumeRenderer::ShadowVolumeRenderer()
	: m_vertices(NULL), m_count(0), m_capacity(0), m_failed(false), m_screenVertices(NULL), m_screenCount(0),
	  m_screenCapacity(0)
{
}
ShadowVolumeRenderer::~ShadowVolumeRenderer()
{
	free(m_vertices);
	free(m_screenVertices);
}

void ShadowVolumeRenderer::Begin(const D3DMATRIX& projection, Point direction, float distance)
{
	m_count = 0;
	m_failed = false;
	m_projection = projection;
	float length = sqrtf(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
	if (length < 0.001f) {
		direction = Point{0.4f, 0.25f, -1.0f};
		length = sqrtf(1.2225f);
	}
	m_direction = Point{direction.x / length, direction.y / length, direction.z / length};
	m_distance = distance;
	// Clip volumes to the far plane and close that cut. Without the cap, z-fail
	// shadows disappear or cover the screen when an extrusion crosses the far plane.
	const float* matrix = reinterpret_cast<const float*>(&projection);
	double norm = 0;
	for (int i = 0; i < 4; ++i) {
		m_plane[i] = double(matrix[i * 4 + 3]) - matrix[i * 4 + 2];
		if (i < 3) {
			norm += m_plane[i] * m_plane[i];
		}
	}
	norm = sqrt(norm);
	if (norm > 0) {
		for (int i = 0; i < 4; ++i) {
			m_plane[i] /= norm;
		}
	}
	// Leave room for the hardware's float transform at track-sized coordinates.
	// An almost coplanar cap can otherwise be clipped away on alternating frames.
	m_plane[3] -= max(0.01, double(distance) * 0.001);
}

double ShadowVolumeRenderer::DistanceToFarPlane(Point p) const
{
	return p.x * m_plane[0] + p.y * m_plane[1] + p.z * m_plane[2] + m_plane[3];
}

void ShadowVolumeRenderer::Emit(Point a, Point b, Point c)
{
	if (m_failed) {
		return;
	}
	if (m_count + 3 > m_capacity) {
		unsigned long capacity = m_capacity ? m_capacity * 2 : 8192;
		if (capacity > 2097152) {
			m_failed = true;
			return;
		}
		void* expanded = realloc(m_vertices, capacity * sizeof(Vertex));
		if (!expanded) {
			m_failed = true;
			return;
		}
		m_vertices = static_cast<Vertex*>(expanded);
		m_capacity = capacity;
	}
	m_vertices[m_count++] = Vertex{a.x, a.y, a.z, 0};
	m_vertices[m_count++] = Vertex{b.x, b.y, b.z, 0};
	m_vertices[m_count++] = Vertex{c.x, c.y, c.z, 0};
}

void ShadowVolumeRenderer::AddTriangle(Point a, Point b, Point c)
{
	Point ab = {b.x - a.x, b.y - a.y, b.z - a.z}, ac = {c.x - a.x, c.y - a.y, c.z - a.z};
	Point normal = {ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};
	float facing = normal.x * m_direction.x + normal.y * m_direction.y + normal.z * m_direction.z;
	if (fabsf(facing) < 0.000001f) {
		return;
	}
	if (facing > 0) {
		Point swap = b;
		b = c;
		c = swap;
	}
	Point vertices[6] = {a, b, c, a, b, c};
	for (int i = 0; i < 6; ++i) {
		float offset = i < 3 ? max(0.03f, m_distance * 0.00001f) : m_distance;
		vertices[i].x += m_direction.x * offset;
		vertices[i].y += m_direction.y * offset;
		vertices[i].z += m_direction.z * offset;
	}
	const int faces[8][3] = {{0, 1, 2}, {5, 4, 3}, {0, 3, 4}, {0, 4, 1}, {1, 4, 5}, {1, 5, 2}, {2, 5, 3}, {2, 3, 0}};
	Point cap[24];
	int capCount = 0;
	for (int face = 0; face < 8; ++face) {
		Point polygon[4];
		int count = 0;
		for (int edge = 0; edge < 3; ++edge) {
			Point first = vertices[faces[face][edge]], second = vertices[faces[face][(edge + 1) % 3]];
			double d1 = DistanceToFarPlane(first), d2 = DistanceToFarPlane(second);
			if (d1 >= 0) {
				polygon[count++] = first;
			}
			if ((d1 >= 0) != (d2 >= 0)) {
				// Give shared edges identical intersections regardless of winding.
				if (d1 < d2) {
					Point swap = first;
					first = second;
					second = swap;
					double swapDistance = d1;
					d1 = d2;
					d2 = swapDistance;
				}
				double t = d1 / (d1 - d2);
				Point cut = {
					float(first.x + (double(second.x) - first.x) * t),
					float(first.y + (double(second.y) - first.y) * t),
					float(first.z + (double(second.z) - first.z) * t)
				};
				polygon[count++] = cut;
				bool duplicate = false;
				for (int j = 0; j < capCount; ++j) {
					float x = cut.x - cap[j].x, y = cut.y - cap[j].y, z = cut.z - cap[j].z;
					if (x * x + y * y + z * z < 0.000001f) {
						duplicate = true;
					}
				}
				if (!duplicate && capCount < 24) {
					cap[capCount++] = cut;
				}
			}
		}
		for (int i = 1; i + 1 < count; ++i) {
			Emit(polygon[0], polygon[i], polygon[i + 1]);
		}
	}
	if (capCount >= 3) {
		Point center = {0, 0, 0};
		for (int i = 0; i < capCount; ++i) {
			center.x += cap[i].x;
			center.y += cap[i].y;
			center.z += cap[i].z;
		}
		center.x /= capCount;
		center.y /= capCount;
		center.z /= capCount;
		Point n = {float(-m_plane[0]), float(-m_plane[1]), float(-m_plane[2])};
		Point u = fabsf(n.z) < 0.9f ? Point{-n.y, n.x, 0} : Point{0, -n.z, n.y};
		float length = sqrtf(u.x * u.x + u.y * u.y + u.z * u.z);
		u.x /= length;
		u.y /= length;
		u.z /= length;
		Point v = {n.y * u.z - n.z * u.y, n.z * u.x - n.x * u.z, n.x * u.y - n.y * u.x};
		float angles[24];
		for (int i = 0; i < capCount; ++i) {
			Point p = {cap[i].x - center.x, cap[i].y - center.y, cap[i].z - center.z};
			angles[i] = atan2f(p.x * v.x + p.y * v.y + p.z * v.z, p.x * u.x + p.y * u.y + p.z * u.z);
		}
		for (int i = 1; i < capCount; ++i) {
			for (int j = i; j > 0 && angles[j] < angles[j - 1]; --j) {
				float t = angles[j];
				angles[j] = angles[j - 1];
				angles[j - 1] = t;
				Point p = cap[j];
				cap[j] = cap[j - 1];
				cap[j - 1] = p;
			}
		}
		for (int i = 1; i + 1 < capCount; ++i) {
			Emit(cap[0], cap[i], cap[i + 1]);
		}
	}
}

bool ShadowVolumeRenderer::Render(LPDIRECT3DDEVICE3 device, LPDIRECT3DVIEWPORT3 viewport, float opacity)
{
	if (m_failed) {
		return false;
	}
	if (!m_count || opacity <= 0) {
		return true;
	}
	const D3DRENDERSTATETYPE states[] = {
		D3DRENDERSTATE_ZENABLE,
		D3DRENDERSTATE_ZWRITEENABLE,
		D3DRENDERSTATE_ZFUNC,
		D3DRENDERSTATE_ALPHABLENDENABLE,
		D3DRENDERSTATE_SRCBLEND,
		D3DRENDERSTATE_DESTBLEND,
		D3DRENDERSTATE_ALPHATESTENABLE,
		D3DRENDERSTATE_FOGENABLE,
		D3DRENDERSTATE_CULLMODE,
		D3DRENDERSTATE_STENCILENABLE,
		D3DRENDERSTATE_STENCILFAIL,
		D3DRENDERSTATE_STENCILZFAIL,
		D3DRENDERSTATE_STENCILPASS,
		D3DRENDERSTATE_STENCILFUNC,
		D3DRENDERSTATE_STENCILREF,
		D3DRENDERSTATE_STENCILMASK,
		D3DRENDERSTATE_STENCILWRITEMASK,
		D3DRENDERSTATE_COLORKEYENABLE,
		D3DRENDERSTATE_FILLMODE
	};
	const DWORD values[] = {
		TRUE,
		FALSE,
		D3DCMP_LESSEQUAL,
		TRUE,
		D3DBLEND_ZERO,
		D3DBLEND_ONE,
		FALSE,
		FALSE,
		D3DCULL_CW,
		TRUE,
		D3DSTENCILOP_KEEP,
		D3DSTENCILOP_INCR,
		D3DSTENCILOP_KEEP,
		D3DCMP_ALWAYS,
		0,
		255,
		255,
		FALSE,
		D3DFILL_SOLID
	};
	DWORD saved[19], colorOp[2], colorArg, alphaOp, alphaArg;
	D3DMATRIX world, view, projection, identity = {};
	identity._11 = identity._22 = identity._33 = identity._44 = 1;
	D3DVIEWPORT2 vp = {};
	vp.dwSize = sizeof(vp);
	if (FAILED(viewport->GetViewport2(&vp))) {
		return false;
	}
	// The game submits CPU-projected TL vertices. Submit shadows through that
	// same raster path, including dgVoodoo's forced-resolution scaling. Mixing
	// hardware XYZ projection with the legacy TL scene gives two depth/coverage
	// paths at exactly the surfaces where stencil volumes must cancel.
	m_screenCount = 0;
	struct ClipVertex {
		double v[4];
	};
	const float* matrix = reinterpret_cast<const float*>(&m_projection);
	for (unsigned long first = 0; first < m_count; first += 3) {
		ClipVertex polygon[12], output[12];
		int count = 3;
		for (int i = 0; i < 3; ++i) {
			const Vertex& source = m_vertices[first + i];
			for (int j = 0; j < 4; ++j) {
				polygon[i].v[j] = double(source.x) * matrix[j] + double(source.y) * matrix[4 + j] +
								  double(source.z) * matrix[8 + j] + matrix[12 + j];
			}
		}
		// Clip in homogeneous coordinates before dividing by W. This also handles
		// volumes crossing the camera and prevents enormous screen-space triangles.
		for (int plane = 0; plane < 6 && count; ++plane) {
			int nextCount = 0;
			for (int i = 0; i < count; ++i) {
				const ClipVertex& a = polygon[i];
				const ClipVertex& b = polygon[(i + 1) % count];
				int axis = plane / 2;
				double da = plane == 4 ? a.v[2] : a.v[3] + (plane % 2 ? -a.v[axis] : a.v[axis]);
				double db = plane == 4 ? b.v[2] : b.v[3] + (plane % 2 ? -b.v[axis] : b.v[axis]);
				if (da >= 0) {
					output[nextCount++] = a;
				}
				if ((da >= 0) != (db >= 0)) {
					double t = da / (da - db);
					for (int j = 0; j < 4; ++j) {
						output[nextCount].v[j] = a.v[j] + (b.v[j] - a.v[j]) * t;
					}
					++nextCount;
				}
			}
			count = nextCount;
			memcpy(polygon, output, count * sizeof(ClipVertex));
		}
		D3DTLVERTEX projected[12];
		for (int i = 0; i < count; ++i) {
			double inverseW = 1.0 / polygon[i].v[3];
			projected[i] = D3DTLVERTEX{
				float(vp.dwX + (polygon[i].v[0] * inverseW + 1) * vp.dwWidth * 0.5),
				float(vp.dwY + (1 - polygon[i].v[1] * inverseW) * vp.dwHeight * 0.5),
				float(polygon[i].v[2] * inverseW),
				float(inverseW),
				0,
				0,
				0,
				0
			};
		}
		for (int i = 1; i + 1 < count; ++i) {
			if (m_screenCount + 3 > m_screenCapacity) {
				unsigned long capacity = m_screenCapacity ? m_screenCapacity * 2 : 8192;
				if (capacity > 2097152) {
					return false;
				}
				void* expanded = realloc(m_screenVertices, capacity * sizeof(D3DTLVERTEX));
				if (!expanded) {
					return false;
				}
				m_screenVertices = static_cast<D3DTLVERTEX*>(expanded);
				m_screenCapacity = capacity;
			}
			m_screenVertices[m_screenCount++] = projected[0];
			m_screenVertices[m_screenCount++] = projected[i];
			m_screenVertices[m_screenCount++] = projected[i + 1];
		}
	}
	for (int i = 0; i < 19; ++i) {
		if (FAILED(device->GetRenderState(states[i], &saved[i]))) {
			return false;
		}
	}
	if (FAILED(device->GetTransform(D3DTRANSFORMSTATE_WORLD, &world)) ||
		FAILED(device->GetTransform(D3DTRANSFORMSTATE_VIEW, &view)) ||
		FAILED(device->GetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection))) {
		return false;
	}
	device->GetTextureStageState(0, D3DTSS_COLOROP, &colorOp[0]);
	device->GetTextureStageState(1, D3DTSS_COLOROP, &colorOp[1]);
	device->GetTextureStageState(0, D3DTSS_COLORARG1, &colorArg);
	device->GetTextureStageState(0, D3DTSS_ALPHAOP, &alphaOp);
	device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &alphaArg);
	LPDIRECT3DTEXTURE2 texture = NULL;
	device->GetTexture(0, &texture);
	D3DVIEWPORT2 standard = vp;
	standard.dvClipX = -1;
	standard.dvClipY = 1;
	standard.dvClipWidth = standard.dvClipHeight = 2;
	bool ok = SUCCEEDED(viewport->SetViewport2(&standard));
	D3DRECT rect = {
		static_cast<LONG>(vp.dwX),
		static_cast<LONG>(vp.dwY),
		static_cast<LONG>(vp.dwX + vp.dwWidth),
		static_cast<LONG>(vp.dwY + vp.dwHeight)
	};
	ok = SUCCEEDED(viewport->Clear2(1, &rect, D3DCLEAR_STENCIL, 0, 1, 0)) && ok;
	for (int i = 0; i < 19; ++i) {
		ok = SUCCEEDED(device->SetRenderState(states[i], values[i])) && ok;
	}
	device->SetTexture(0, NULL);
	device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
	device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	device->SetTransform(D3DTRANSFORMSTATE_WORLD, &identity);
	device->SetTransform(D3DTRANSFORMSTATE_VIEW, &identity);
	device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &m_projection);
	for (int side = 0; side < 2 && ok; ++side) {
		device->SetRenderState(D3DRENDERSTATE_CULLMODE, side ? D3DCULL_CCW : D3DCULL_CW);
		device->SetRenderState(D3DRENDERSTATE_STENCILZFAIL, side ? D3DSTENCILOP_DECR : D3DSTENCILOP_INCR);
		for (unsigned long start = 0; start < m_screenCount; start += 6000) {
			DWORD count = m_screenCount - start;
			if (count > 6000) {
				count = 6000;
			}
			ok = SUCCEEDED(device->DrawPrimitive(
					 D3DPT_TRIANGLELIST,
					 D3DFVF_TLVERTEX,
					 &m_screenVertices[start],
					 count,
					 D3DDP_DONOTLIGHT | D3DDP_DONOTCLIP
				 )) &&
				 ok;
		}
	}
	if (ok) {
		// Only shade pixels with scene geometry; the far-depth sky is not a receiver.
		device->SetRenderState(D3DRENDERSTATE_ZFUNC, D3DCMP_GREATER);
		device->SetRenderState(D3DRENDERSTATE_STENCILFUNC, D3DCMP_NOTEQUAL);
		device->SetRenderState(D3DRENDERSTATE_STENCILZFAIL, D3DSTENCILOP_KEEP);
		device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRENDERSTATE_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRENDERSTATE_DESTBLEND, D3DBLEND_INVSRCALPHA);
		DWORD alpha = static_cast<DWORD>(opacity * 255);
		if (alpha > 255) {
			alpha = 255;
		}
		DWORD color = alpha << 24;
		float l = vp.dwX - 0.5f, t = vp.dwY - 0.5f, r = l + vp.dwWidth, b = t + vp.dwHeight;
		D3DTLVERTEX quad[4] = {
			{l, t, 1, 1, color, 0, 0, 0},
			{r, t, 1, 1, color, 0, 0, 0},
			{l, b, 1, 1, color, 0, 0, 0},
			{r, b, 1, 1, color, 0, 0, 0}
		};
		ok = SUCCEEDED(
			device->DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, quad, 4, D3DDP_DONOTLIGHT | D3DDP_DONOTCLIP)
		);
	}
	for (int i = 0; i < 19; ++i) {
		device->SetRenderState(states[i], saved[i]);
	}
	device->SetTransform(D3DTRANSFORMSTATE_WORLD, &world);
	device->SetTransform(D3DTRANSFORMSTATE_VIEW, &view);
	device->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &projection);
	device->SetTextureStageState(0, D3DTSS_COLOROP, colorOp[0]);
	device->SetTextureStageState(1, D3DTSS_COLOROP, colorOp[1]);
	device->SetTextureStageState(0, D3DTSS_COLORARG1, colorArg);
	device->SetTextureStageState(0, D3DTSS_ALPHAOP, alphaOp);
	device->SetTextureStageState(0, D3DTSS_ALPHAARG1, alphaArg);
	device->SetTexture(0, texture);
	if (texture) {
		texture->Release();
	}
	viewport->SetViewport2(&vp);
	return ok;
}
