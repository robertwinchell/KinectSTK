#include "stkmeshscenenode.hpp"
#include "stkmesh.hpp"
#include "graphics/irr_driver.hpp"
#include "tracks/track.hpp"
#include <ISceneManager.h>
#include <IMaterialRenderer.h>
#include "config/user_config.hpp"
#include "graphics/callbacks.hpp"
#include "utils/helpers.hpp"
#include "graphics/camera.hpp"
#include "modes/world.hpp"

static
core::vector3df getWind()
{
    const core::vector3df pos = irr_driver->getVideoDriver()->getTransform(video::ETS_WORLD).getTranslation();
    const float time = irr_driver->getDevice()->getTimer()->getTime() / 1000.0f;
    GrassShaderProvider *gsp = (GrassShaderProvider *)irr_driver->getCallback(ES_GRASS);
    float m_speed = gsp->getSpeed(), m_amplitude = gsp->getAmplitude();

    float strength = (pos.X + pos.Y + pos.Z) * 1.2f + time * m_speed;
    strength = noise2d(strength / 10.0f) * m_amplitude * 5;
    // * 5 is to work with the existing amplitude values.

    // Pre-multiply on the cpu
    return irr_driver->getWind() * strength;
}

STKMeshSceneNode::STKMeshSceneNode(irr::scene::IMesh* mesh, ISceneNode* parent, irr::scene::ISceneManager* mgr, irr::s32 id,
    const irr::core::vector3df& position,
    const irr::core::vector3df& rotation,
    const irr::core::vector3df& scale) :
    CMeshSceneNode(mesh, parent, mgr, id, position, rotation, scale)
{
    createGLMeshes();
}

void STKMeshSceneNode::createGLMeshes()
{
    for (u32 i = 0; i<Mesh->getMeshBufferCount(); ++i)
    {
        scene::IMeshBuffer* mb = Mesh->getMeshBuffer(i);
        GLmeshes.push_back(allocateMeshBuffer(mb));
    }
}

void STKMeshSceneNode::cleanGLMeshes()
{
    for (u32 i = 0; i < GLmeshes.size(); ++i)
    {
        GLMesh mesh = GLmeshes[i];
        if (!mesh.vertex_buffer)
            continue;
        if (mesh.vao_first_pass)
            glDeleteVertexArrays(1, &(mesh.vao_first_pass));
        if (mesh.vao_second_pass)
            glDeleteVertexArrays(1, &(mesh.vao_second_pass));
        if (mesh.vao_glow_pass)
            glDeleteVertexArrays(1, &(mesh.vao_glow_pass));
        if (mesh.vao_displace_pass)
            glDeleteVertexArrays(1, &(mesh.vao_displace_pass));
        glDeleteBuffers(1, &(mesh.vertex_buffer));
        glDeleteBuffers(1, &(mesh.index_buffer));
    }
    GLmeshes.clear();
}

void STKMeshSceneNode::setMesh(irr::scene::IMesh* mesh)
{
    CMeshSceneNode::setMesh(mesh);
    cleanGLMeshes();
    createGLMeshes();
}

STKMeshSceneNode::~STKMeshSceneNode()
{
    cleanGLMeshes();
}

void STKMeshSceneNode::drawGlow(const GLMesh &mesh)
{
    ColorizeProvider * const cb = (ColorizeProvider *)irr_driver->getCallback(ES_COLORIZE);

    GLenum ptype = mesh.PrimitiveType;
    GLenum itype = mesh.IndexType;
    size_t count = mesh.IndexCount;

    computeMVP(ModelViewProjectionMatrix);
    glUseProgram(MeshShader::ColorizeShader::Program);
    MeshShader::ColorizeShader::setUniforms(ModelViewProjectionMatrix, cb->getRed(), cb->getGreen(), cb->getBlue());

    glBindVertexArray(mesh.vao_glow_pass);
    glDrawElements(ptype, count, itype, 0);
}

void STKMeshSceneNode::drawDisplace(const GLMesh &mesh)
{
    DisplaceProvider * const cb = (DisplaceProvider *)irr_driver->getCallback(ES_DISPLACE);

    GLenum ptype = mesh.PrimitiveType;
    GLenum itype = mesh.IndexType;
    size_t count = mesh.IndexCount;

    computeMVP(ModelViewProjectionMatrix);
    core::matrix4 ModelViewMatrix = irr_driver->getVideoDriver()->getTransform(video::ETS_VIEW);
    ModelViewMatrix *= irr_driver->getVideoDriver()->getTransform(video::ETS_WORLD);

    // Generate displace mask
    // Use RTT_TMP4 as displace mask
    irr_driver->getVideoDriver()->setRenderTarget(irr_driver->getRTT(RTT_TMP4), false, false);

    glUseProgram(MeshShader::DisplaceMaskShader::Program);
    MeshShader::DisplaceMaskShader::setUniforms(ModelViewProjectionMatrix);

    glBindVertexArray(mesh.vao_displace_mask_pass);
    glDrawElements(ptype, count, itype, 0);

    // Render the effect
    irr_driver->getVideoDriver()->setRenderTarget(irr_driver->getRTT(RTT_DISPLACE), false, false);
    setTexture(0, getTextureGLuint(irr_driver->getTexture(FileManager::TEXTURE, "displace.png")), GL_LINEAR, GL_LINEAR, true);
    setTexture(1, getTextureGLuint(irr_driver->getRTT(RTT_TMP4)), GL_LINEAR, GL_LINEAR, true);
    setTexture(2, getTextureGLuint(irr_driver->getRTT(RTT_COLOR)), GL_LINEAR, GL_LINEAR, true);
    glUseProgram(MeshShader::DisplaceShader::Program);
    MeshShader::DisplaceShader::setUniforms(ModelViewProjectionMatrix, ModelViewMatrix, core::vector2df(cb->getDirX(), cb->getDirY()), core::vector2df(cb->getDir2X(), cb->getDir2Y()), core::vector2df(UserConfigParams::m_width, UserConfigParams::m_height), 0, 1, 2);

    glBindVertexArray(mesh.vao_displace_pass);
    glDrawElements(ptype, count, itype, 0);
}

void STKMeshSceneNode::drawTransparent(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
    assert(irr_driver->getPhase() == TRANSPARENT_PASS);

    computeMVP(ModelViewProjectionMatrix);

    if (type == irr_driver->getShader(ES_BUBBLES))
        drawBubble(mesh, ModelViewProjectionMatrix);
    else if (World::getWorld()->getTrack()->isFogEnabled())
        drawTransparentFogObject(mesh, ModelViewProjectionMatrix, TextureMatrix);
    else
        drawTransparentObject(mesh, ModelViewProjectionMatrix, TextureMatrix);
    return;
}

void STKMeshSceneNode::drawShadow(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{

    GLenum ptype = mesh.PrimitiveType;
    GLenum itype = mesh.IndexType;
    size_t count = mesh.IndexCount;


    std::vector<core::matrix4> ShadowMVP(irr_driver->getShadowViewProj());
    for (unsigned i = 0; i < ShadowMVP.size(); i++)
        ShadowMVP[i] *= irr_driver->getVideoDriver()->getTransform(video::ETS_WORLD);

    if (type == irr_driver->getShader(ES_OBJECTPASS_REF))
    {
        setTexture(0, mesh.textures[0], GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, true);
        glUseProgram(MeshShader::RefShadowShader::Program);
        MeshShader::RefShadowShader::setUniforms(ShadowMVP, 0);
    }
    /*    else if (type == irr_driver->getShader(ES_GRASS) || type == irr_driver->getShader(ES_GRASS_REF))
    {
    setTexture(0, mesh.textures[0], GL_LINEAR, GL_LINEAR_MIPMAP_LINEAR, true);
    glUseProgram(MeshShader::GrassShadowShader::Program);
    MeshShader::GrassShadowShader::setUniforms(ShadowMVP, windDir, 0);
    }*/
    else
    {
        glUseProgram(MeshShader::ShadowShader::Program);
        MeshShader::ShadowShader::setUniforms(ShadowMVP);
    }
    glBindVertexArray(mesh.vao_shadow_pass);
    glDrawElements(ptype, count, itype, 0);
}

void STKMeshSceneNode::drawSolid(const GLMesh &mesh, video::E_MATERIAL_TYPE type)
{
    switch (irr_driver->getPhase())
    {
    case SOLID_NORMAL_AND_DEPTH_PASS:
    {
        windDir = getWind();

        computeMVP(ModelViewProjectionMatrix);
        computeTIMV(TransposeInverseModelView);

        if (type == irr_driver->getShader(ES_NORMAL_MAP))
            drawNormalPass(mesh, ModelViewProjectionMatrix, TransposeInverseModelView);
        else if (type == irr_driver->getShader(ES_OBJECTPASS_REF))
            drawObjectRefPass1(mesh, ModelViewProjectionMatrix, TransposeInverseModelView, TextureMatrix);
        else if (type == irr_driver->getShader(ES_GRASS) || type == irr_driver->getShader(ES_GRASS_REF))
            drawGrassPass1(mesh, ModelViewProjectionMatrix, TransposeInverseModelView, windDir);
        else
            drawObjectPass1(mesh, ModelViewProjectionMatrix, TransposeInverseModelView);
        break;
    }
    case SOLID_LIT_PASS:
    {
        if (type == irr_driver->getShader(ES_SPHERE_MAP))
            drawSphereMap(mesh, ModelViewProjectionMatrix, TransposeInverseModelView);
        else if (type == irr_driver->getShader(ES_SPLATTING))
            drawSplatting(mesh, ModelViewProjectionMatrix);
        else if (type == irr_driver->getShader(ES_OBJECTPASS_REF))
            drawObjectRefPass2(mesh, ModelViewProjectionMatrix, TextureMatrix);
        else if (type == irr_driver->getShader(ES_GRASS) || type == irr_driver->getShader(ES_GRASS_REF))
            drawGrassPass2(mesh, ModelViewProjectionMatrix, windDir);
        else if (type == irr_driver->getShader(ES_OBJECTPASS_RIMLIT))
            drawObjectRimLimit(mesh, ModelViewProjectionMatrix, TransposeInverseModelView, TextureMatrix);
        else if (type == irr_driver->getShader(ES_OBJECT_UNLIT))
            drawObjectUnlit(mesh, ModelViewProjectionMatrix);
        else if (type == irr_driver->getShader(ES_CAUSTICS))
        {
            const float time = irr_driver->getDevice()->getTimer()->getTime() / 1000.0f;
            const float speed = World::getWorld()->getTrack()->getCausticsSpeed();

            float strength = time;
            strength = fabsf(noise2d(strength / 10.0f)) * 0.006f + 0.001f;

            vector3df wind = irr_driver->getWind() * strength * speed;
            caustic_dir.X += wind.X;
            caustic_dir.Y += wind.Z;

            strength = time * 0.56f + sinf(time);
            strength = fabsf(noise2d(0.0, strength / 6.0f)) * 0.0095f + 0.001f;

            wind = irr_driver->getWind() * strength * speed;
            wind.rotateXZBy(cosf(time));
            caustic_dir2.X += wind.X;
            caustic_dir2.Y += wind.Z;
            drawCaustics(mesh, ModelViewProjectionMatrix, caustic_dir, caustic_dir2);
        }
        else if (mesh.textures[1] && type != irr_driver->getShader(ES_NORMAL_MAP))
            drawDetailledObjectPass2(mesh, ModelViewProjectionMatrix);
        else if (!mesh.textures[0])
            drawUntexturedObject(mesh, ModelViewProjectionMatrix);
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

void STKMeshSceneNode::render()
{
    irr::video::IVideoDriver* driver = irr_driver->getVideoDriver();

    if (!Mesh || !driver)
        return;

    bool isTransparentPass =
        SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT;

    ++PassCount;

    driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
    Box = Mesh->getBoundingBox();

    for (u32 i = 0; i<Mesh->getMeshBufferCount(); ++i)
    {
        scene::IMeshBuffer* mb = Mesh->getMeshBuffer(i);
        if (mb)
        {
            TextureMatrix = getMaterial(i).getTextureMatrix(0);
            const video::SMaterial& material = ReadOnlyMaterials ? mb->getMaterial() : Materials[i];

            video::IMaterialRenderer* rnd = driver->getMaterialRenderer(material.MaterialType);
            bool transparent = (rnd && rnd->isTransparent());

            if (isTransparentPass != transparent)
                continue;
            if (irr_driver->getPhase() == DISPLACEMENT_PASS)
            {
                initvaostate(GLmeshes[i], material.MaterialType);
                drawDisplace(GLmeshes[i]);
                continue;
            }
            if (!isObject(material.MaterialType))
            {
#ifdef DEBUG
                Log::warn("material", "Unhandled (static) material type : %d", material.MaterialType);
#endif
                continue;
            }

            // only render transparent buffer if this is the transparent render pass
            // and solid only in solid pass
            if (irr_driver->getPhase() == GLOW_PASS)
            {
                initvaostate(GLmeshes[i], material.MaterialType);
                drawGlow(GLmeshes[i]);
            }
            else if (irr_driver->getPhase() == SHADOW_PASS)
            {
                initvaostate(GLmeshes[i], material.MaterialType);
                drawShadow(GLmeshes[i], material.MaterialType);
            }
            else
            {
                irr_driver->IncreaseObjectCount();
                initvaostate(GLmeshes[i], material.MaterialType);
                if (transparent)
                    drawTransparent(GLmeshes[i], material.MaterialType);
                else
                    drawSolid(GLmeshes[i], material.MaterialType);
            }
        }
    }
}
