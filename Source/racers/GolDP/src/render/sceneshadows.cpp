#include "camera/golcamera.h"
#include "camera/golscenetransformnode.h"
#include "camera/goltransform.h"
#include "diagnosticlog.h"
#include "gdbmodelindexarray.h"
#include "golmaterial.h"
#include "golmodelentity.h"
#include "goltexture.h"
#include "golmath.h"
#include "mesh/golmodel.h"
#include "render/gold3drenderdevice.h"
#include "render/goldrawdpstate.h"
#include "../../../../Renderer/renderer_api.h"
#include "golerror.h"

#include <string.h>

LegoBool32 GolD3DRenderDevice::BeginShadowCollection()
{
	if (m_shadowFailed || !m_currentCamera || (m_flags & c_flagSoftwareRenderer) ||
		m_renderTargetInfo != m_primaryRenderTarget || (!IsNativeActive() && m_drawState->m_depthBufferPixelformat.dwStencilBitDepth < 8)) {
		return FALSE;
	}
	m_currentCamera->UpdateMatrices();
	GolMatrix4 matrix = m_currentCamera->m_cameraMatrices.m_viewProjection;
	// GOL's projection maps XY into [0,1] with Y down; D3D's transform pipeline
	// needs [-1,1] with Y up. Z and W already use the D3D depth convention.
	for (int row = 0; row < 4; ++row) {
		matrix.m_m[row][0] = 2 * matrix.m_m[row][0] - matrix.m_m[row][3];
		matrix.m_m[row][1] = matrix.m_m[row][3] - 2 * matrix.m_m[row][1];
	}
	D3DMATRIX projection;
	memcpy(&projection, &matrix, sizeof(projection));
	ShadowVolumeRenderer::Point direction = {0.4f, 0.25f, -1.0f};
	if (m_lightCount && m_lights[0]) {
		const GolVec3& light = m_lights[0]->m_direction;
		direction = ShadowVolumeRenderer::Point{light.m_x, light.m_y, light.m_z};
	}
    if(IsNativeActive()) {
        const GolVec3& eye=m_viewFrustum.m_position;
        if(!m_nativeApi->beginLighting(m_nativeContext,&matrix.m_m[0][0],&eye.m_x,&direction.x,m_currentCamera->m_farClip))return FALSE;
        m_collectingShadows=m_nativeShadows;
        return m_nativeShadows;
    }
	m_sceneShadows.Begin(projection, direction, m_currentCamera->m_farClip * 2.0f);
	m_collectingShadows = TRUE;
	return TRUE;
}

void GolD3DRenderDevice::EndShadowCollection()
{
	m_collectingShadows = FALSE;
    if(IsNativeActive()) return;
	if (!m_shadowLogged) {
		char message[160];
		sprintf(
			message,
			"Scene shadows (CPU-clipped TL v2): %lu volume vertices, %lu stencil bits.",
			m_sceneShadows.GetVertexCount(),
			m_drawState->m_depthBufferPixelformat.dwStencilBitDepth
		);
		AppendDiagnosticLog("Reborn-render.log", message);
		m_shadowLogged = TRUE;
	}
}

void GolD3DRenderDevice::ApplySceneShadows(LegoFloat p_opacity)
{
    if(IsNativeActive()) {
        if(!m_nativeApi->lighting(m_nativeContext,m_nativeShadows?p_opacity:0,m_nativeAO))
            GOL_FATALERROR_MESSAGE("Native lighting pass failed; see renderer log");
        return;
    }
	if (IsNativeActive() || m_shadowFailed || (m_flags & c_flagSoftwareRenderer) ||
		m_drawState->m_depthBufferPixelformat.dwStencilBitDepth < 8) {
		return;
	}
	if (!m_sceneShadows.Render(m_d3dDevice, m_d3dViewport, p_opacity)) {
		m_shadowFailed = TRUE;
		AppendDiagnosticLog(
			"Reborn-render.log",
			"Scene shadows disabled: device call failed or geometry budget exceeded."
		);
		OutputDebugString("Reborn: scene shadow pass unavailable; disabled until display recreation.\n");
	}
}

void GolD3DRenderDevice::CollectShadowEntity(GolModelEntity* p_entity, LegoU32 p_lod)
{
	if (p_lod == ~0u) {
		// Match the visible mesh's distance selection, but keep offscreen casters.
		float distanceSquared = p_entity->GetPosition().DistanceSquaredTo(m_viewFrustum.m_position);
		p_lod = 0;
		while (p_lod < 3 && distanceSquared > p_entity->GetModelDistance(p_lod)) {
			++p_lod;
		}
	}
	if (p_lod >= 3) {
		return;
	}
	GolModelBase* model = p_entity->GetModel(p_lod);
	if (!model || !model->GetVertexArray() || !model->GetIndexArray()) {
		return;
	}
	const GolVec3& position = p_entity->GetPosition();
	const GolVec3& eye = m_viewFrustum.m_position;
	float dx = position.m_x - eye.m_x, dy = position.m_y - eye.m_y, dz = position.m_z - eye.m_z;
	float range = m_currentCamera->m_farClip * 2.0f + model->GetRadius() * p_entity->GetScale();
	if (dx * dx + dy * dy + dz * dz > range * range) {
		return;
	}
	GolMatrix4 world;
	p_entity->BuildModelMatrix(&world, p_lod);
	world.m_m[3][0] = position.m_x;
	world.m_m[3][1] = position.m_y;
	world.m_m[3][2] = position.m_z;
	GolSceneTransformNode* nodes = static_cast<GolSceneTransformNode*>(p_entity->GetSceneNode(p_lod));
	if (nodes) {
		p_entity->ApplyPartAnimation(p_lod);
		nodes->UpdateWorldMatrices(world);
	}
	const GolMatrix4* transform = &world;
	GdbVertexArray* vertices = model->GetVertexArray();
	GdbModelIndexArray* indices = static_cast<GdbModelIndexArray*>(model->GetIndexArray());
	MaterialTable* materials = p_entity->GetMaterialTable(p_lod);
	if (!materials) {
		materials = model->GetMaterialTable();
	}
	ShadowVolumeRenderer::Point cache[64];
    GolVec2 uv[64];
    unsigned __int64 maskTexture=0;
	bool valid[64] = {};
	bool opaque = true;
	const LegoU32* groups = model->GetGroups();
	for (LegoU32 g = 0; g < model->GetGroupCount(); ++g) {
		LegoU32 command = groups[g], type = command & GolModel::c_groupTypeMask;
		LegoU32 first = command & GolModel::c_materialMatrixIndexMask;
		if (type == GolModel::c_groupTypeEnd) {
			break;
		}
		if (type == GolModel::c_groupTypeMatrix) {
			if (nodes && first < nodes->GetCapacity()) {
				transform = &nodes->m_transforms[first].m_worldMatrix;
			}
			else {
				transform = &world;
			}
		}
		else if (type == GolModel::c_groupTypeMaterial) {
			opaque = false;
            maskTexture=0;
			if (first < model->GetMaterialTable()->GetCount()) {
				GolMaterial* material = static_cast<GolMaterial*>(materials->GetEntry(first));
				if (material) {
					opaque =
						!(material->GetFlags() & (GolMaterial::c_flagAlphaBlend | GolMaterial::c_flagAlphaTest |
												  GolMaterial::c_flagTransparent));
					// Legacy cutouts also use texture color keys without alpha material flags.
					GolTexture* texture = material->GetTexture();
					if (texture && (texture->GetTextureFlags() & GolTexture::c_textureFlagColorKeyed)) {
						opaque = false;
					}
                    // Cutouts cast only their covered texels. Truly blended
                    // surfaces (water/glass) remain outside this binary pass.
                    bool cutout=texture && ((material->GetFlags() & GolMaterial::c_flagAlphaTest) ||
                        (texture->GetTextureFlags() & GolTexture::c_textureFlagColorKeyed));
                    if(IsNativeActive() && cutout)maskTexture=UploadNativeTexture(texture);
				}
			}
		}
		else if (type == GolModel::c_groupTypeTriangles) {
			LegoU32 count = ((command >> 16) & 63) + 1, output = (command >> 22) & 63;
			for (LegoU32 i = 0; i < count && output + i < 64; ++i) {
				valid[output + i] = false;
				if (first + i >= vertices->GetCount()) {
					continue;
				}
				GolVec3 p;
				vertices->GetPosition(first + i, &p);
                vertices->GetTextureCoordinate(first+i,&uv[output+i]);
				const float(*m)[4] = transform->m_m;
				cache[output + i] = ShadowVolumeRenderer::Point{
					p.m_x * m[0][0] + p.m_y * m[1][0] + p.m_z * m[2][0] + m[3][0],
					p.m_x * m[0][1] + p.m_y * m[1][1] + p.m_z * m[2][1] + m[3][1],
					p.m_x * m[0][2] + p.m_y * m[1][2] + p.m_z * m[2][2] + m[3][2]
				};
				valid[output + i] = true;
			}
		}
		else if (type == GolModel::c_groupTypeTriangleBatch && (opaque || maskTexture)) {
			LegoU32 count = (command >> 16) & 127;
			for (LegoU32 i = 0; i < count && first + i < indices->GetCount(); ++i) {
				const GdbModelIndexArray::Indices* triangle = indices->GetIndex(first + i);
				unsigned a = triangle->m_a, b = triangle->m_b, c = triangle->m_c;
				if (a < 64 && b < 64 && c < 64 && valid[a] && valid[b] && valid[c]) {
                    if(IsNativeActive() && maskTexture) {
                        unsigned index[3]={a,b,c};RBShadowVertex triangleVertices[3];
                        for(unsigned j=0;j<3;++j){unsigned v=index[j];
                            triangleVertices[j]=RBShadowVertex{cache[v].x,cache[v].y,cache[v].z,uv[v].m_x,uv[v].m_y};}
                        m_nativeApi->addMaskedTriangle(m_nativeContext,triangleVertices,maskTexture);
                    }
                    else if(IsNativeActive()) m_nativeApi->addTriangle(m_nativeContext,&cache[a].x,&cache[b].x,&cache[c].x);
                    else m_sceneShadows.AddTriangle(cache[a], cache[b], cache[c]);
				}
			}
		}
	}
}
