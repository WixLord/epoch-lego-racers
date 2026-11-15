#ifndef SHADOWVOLUMERENDERER_H
#define SHADOWVOLUMERENDERER_H

#include <d3d.h>

// Directional, two-sided triangle volumes. Geometry is collected independently
// of camera visibility; the depth buffer determines which surfaces receive shade.
class ShadowVolumeRenderer {
public:
	struct Point {
		float x, y, z;
	};
	ShadowVolumeRenderer();
	~ShadowVolumeRenderer();
	void Begin(const D3DMATRIX& projection, Point direction, float distance);
	void AddTriangle(Point a, Point b, Point c);
	bool Render(LPDIRECT3DDEVICE3 device, LPDIRECT3DVIEWPORT3 viewport, float opacity);
	unsigned long GetVertexCount() const { return m_count; }

private:
	struct Vertex {
		float x, y, z;
		DWORD color;
	};
	void Emit(Point a, Point b, Point c);
	double DistanceToFarPlane(Point p) const;
	ShadowVolumeRenderer(const ShadowVolumeRenderer&);
	ShadowVolumeRenderer& operator=(const ShadowVolumeRenderer&);
	Vertex* m_vertices;
	unsigned long m_count, m_capacity;
	bool m_failed;
	D3DMATRIX m_projection;
	Point m_direction;
	float m_distance;
	double m_plane[4];
	D3DTLVERTEX* m_screenVertices;
	unsigned long m_screenCount, m_screenCapacity;
};

#endif
