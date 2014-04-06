#include "graphics/stkanimatedmesh.hpp"
#include <ISceneManager.h>
#include <IMaterialRenderer.h>
#include <ISkinnedMesh.h>
#include "graphics/irr_driver.hpp"
#include "config/user_config.hpp"
#include "modes/world.hpp"
#include "tracks/track.hpp"

using namespace irr;

STKAnimatedMesh::STKAnimatedMesh(irr::scene::IAnimatedMesh* mesh, irr::scene::ISceneNode* parent,
irr::scene::ISceneManager* mgr, s32 id,
const core::vector3df& position,
const core::vector3df& rotation,
const core::vector3df& scale) :
    CAnimatedMeshSceneNode(mesh, parent, mgr, id, position, rotation, scale)
{
	firstTime = true;
}

void STKAnimatedMesh::setMesh(scene::IAnimatedMesh* mesh)
{
	firstTime = true;
	GLmeshes.clear();
	CAnimatedMeshSceneNode::setMesh(mesh);
}

void STKAnimatedMesh::drawTransparent(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
	assert(irr_driver->getPhase() == TRANSPARENT_PASS);

	computeMVP(ModelViewProjectionMatrix);

    if (World::getWorld()->getTrack()->isFogEnabled())
        drawTransparentFogObject(mesh, ModelViewProjectionMatrix, TextureMatrix);
    else
        drawTransparentObject(mesh, ModelViewProjectionMatrix, TextureMatrix);

	return;
}

void STKAnimatedMesh::drawSolid(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
	switch (irr_driver->getPhase())
	{
	case SOLID_NORMAL_AND_DEPTH_PASS:
	{
			  computeMVP(ModelViewProjectionMatrix);
			  computeTIMV(TransposeInverseModelView);

			  if (type == irr_driver->getShader(ES_OBJECTPASS_REF))
                  drawObjectRefPass1(mesh, ModelViewProjectionMatrix, TransposeInverseModelView, TextureMatrix);
			  else
				  drawObjectPass1(mesh, ModelViewProjectionMatrix, TransposeInverseModelView);
			  break;
	}
	case SOLID_LIT_PASS:
	{
			  if (type == irr_driver->getShader(ES_OBJECTPASS_REF))
                  drawObjectRefPass2(mesh, ModelViewProjectionMatrix, TextureMatrix);
			  else if (type == irr_driver->getShader(ES_OBJECTPASS_RIMLIT))
				  drawObjectRimLimit(mesh, ModelViewProjectionMatrix, TransposeInverseModelView, TextureMatrix);
			  else if (type == irr_driver->getShader(ES_OBJECT_UNLIT))
				  drawObjectUnlit(mesh, ModelViewProjectionMatrix);
			  else if (mesh.textures[1])
				  drawDetailledObjectPass2(mesh, ModelViewProjectionMatrix);
			  else
				  drawObjectPass2(mesh, ModelViewProjectionMatrix, TextureMatrix);
			  break;
	}
	default:
	{
			   assert(0 && "wrong pass");
	}
	}
}

void STKAnimatedMesh::drawShadow(const GLMesh &mesh)
{
    GLenum ptype = mesh.PrimitiveType;
    GLenum itype = mesh.IndexType;
    size_t count = mesh.IndexCount;
    assert(irr_driver->getPhase() == SHADOW_PASS);
    std::vector<core::matrix4> ShadowMVP(irr_driver->getShadowViewProj());
    for (unsigned i = 0; i < ShadowMVP.size(); i++)
        ShadowMVP[i] *= irr_driver->getVideoDriver()->getTransform(video::ETS_WORLD);
    glUseProgram(MeshShader::ShadowShader::Program);
    MeshShader::ShadowShader::setUniforms(ShadowMVP);
    glBindVertexArray(mesh.vao_shadow_pass);
    glDrawElements(ptype, count, itype, 0);
}

void STKAnimatedMesh::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();

	bool isTransparentPass =
		SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT;

	++PassCount;

	scene::IMesh* m = getMeshForCurrentFrame();

	if (m)
	{
		Box = m->getBoundingBox();
	}
	else
	{
		Log::error("animated mesh", "Animated Mesh returned no mesh to render.");
		return;
	}

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);

	if (firstTime)
	for (u32 i = 0; i<m->getMeshBufferCount(); ++i)
	{
		scene::IMeshBuffer* mb = Mesh->getMeshBuffer(i);
		GLmeshes.push_back(allocateMeshBuffer(mb));
	}
	firstTime = false;

	// render original meshes
	for (u32 i = 0; i<m->getMeshBufferCount(); ++i)
	{
		video::IMaterialRenderer* rnd = driver->getMaterialRenderer(Materials[i].MaterialType);
		bool transparent = (rnd && rnd->isTransparent());

		// only render transparent buffer if this is the transparent render pass
		// and solid only in solid pass
		if (transparent != isTransparentPass)
			continue;
		scene::IMeshBuffer* mb = m->getMeshBuffer(i);
        TextureMatrix = getMaterial(i).getTextureMatrix(0);
		const video::SMaterial& material = ReadOnlyMaterials ? mb->getMaterial() : Materials[i];
		if (RenderFromIdentity)
			driver->setTransform(video::ETS_WORLD, core::IdentityMatrix);
		else if (Mesh->getMeshType() == scene::EAMT_SKINNED)
			driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((scene::SSkinMeshBuffer*)mb)->Transformation);
        if (isObject(material.MaterialType))
		{
			irr_driver->IncreaseObjectCount();
			initvaostate(GLmeshes[i], material.MaterialType);
			if (irr_driver->getPhase() == SOLID_NORMAL_AND_DEPTH_PASS)
			{
				glBindVertexArray(0);
				glBindBuffer(GL_ARRAY_BUFFER, GLmeshes[i].vertex_buffer);
				glBufferSubData(GL_ARRAY_BUFFER, 0, mb->getVertexCount() * GLmeshes[i].Stride, mb->getVertices());
			}
            if (irr_driver->getPhase() == SHADOW_PASS)
                drawShadow(GLmeshes[i]);
			else if (isTransparentPass)
				drawTransparent(GLmeshes[i], material.MaterialType);
			else
				drawSolid(GLmeshes[i], material.MaterialType);
		}
		else 
		{
#ifdef DEBUG
			Log::warn("material", "Unhandled (animated) material type : %d", material.MaterialType);
#endif
			continue;
		}
	}
}
