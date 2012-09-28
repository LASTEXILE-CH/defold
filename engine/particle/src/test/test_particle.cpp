#include <gtest/gtest.h>
#include <stdio.h>

#include <dlib/dstrings.h>
#include <dlib/log.h>

#include <ddf/ddf.h>

#include "../particle.h"
#include "../particle_private.h"

struct ParticleVertex;

class ParticleTest : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        m_Context = dmParticle::CreateContext(64, 1024);
        assert(m_Context != 0);
        m_VertexBufferSize = 1024 * 4 * 6 * sizeof(float);
        m_VertexBuffer = new float[m_VertexBufferSize];
        m_Prototype = 0x0;
    }

    virtual void TearDown()
    {
        if (m_Prototype != 0x0)
        {
            dmParticle::Particle_DeletePrototype(m_Prototype);
        }
        dmParticle::DestroyContext(m_Context);
        delete [] m_VertexBuffer;
    }

    void VerifyUVs(ParticleVertex* vertex_buffer, uint32_t tile);

    dmParticle::HContext m_Context;
    dmParticle::HPrototype m_Prototype;
    float* m_VertexBuffer;
    uint32_t m_VertexBufferSize;
};

bool LoadPrototype(const char* filename, dmParticle::HPrototype* prototype)
{
    char path[64];
    DM_SNPRINTF(path, 64, "build/default/src/test/%s", filename);
    const uint32_t MAX_FILE_SIZE = 4 * 1024;
    unsigned char buffer[MAX_FILE_SIZE];
    uint32_t file_size = 0;

    FILE* f = fopen(path, "rb");
    if (f)
    {
        file_size = fread(buffer, 1, MAX_FILE_SIZE, f);
        *prototype = dmParticle::NewPrototype(buffer, file_size);
        fclose(f);
        return *prototype != 0x0;
    }
    else
    {
        dmLogWarning("Particle FX could not be loaded: %s.", path);
        return false;
    }
}

TEST_F(ParticleTest, CreationSuccess)
{
    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    ASSERT_NE(dmParticle::INVALID_INSTANCE, instance);
    ASSERT_EQ(1U, m_Context->m_InstanceIndexPool.Size());
    dmParticle::DestroyInstance(m_Context, instance);
    ASSERT_EQ(0U, m_Context->m_InstanceIndexPool.Size());
}

TEST_F(ParticleTest, CreationFailure)
{
    ASSERT_FALSE(LoadPrototype("null.particlefxc", &m_Prototype));
}

TEST_F(ParticleTest, StartInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::Instance* i = m_Context->m_Instances[index];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_EQ(1U, i->m_Emitters[0].m_Particles.Size());

    dmParticle::StartInstance(m_Context, instance);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    uint32_t out_vertex_buffer_size;
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, &out_vertex_buffer_size, 0x0);

    i = m_Context->m_Instances[index];
    dmParticle::Emitter* e = &i->m_Emitters[0];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_GT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);
    ASSERT_EQ(6 * 6 * sizeof(float), out_vertex_buffer_size); // 6 vertices of 6 floats (pos, uv, alpha)

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    i = m_Context->m_Instances[index];
    e = &i->m_Emitters[0];
    ASSERT_NE((void*)0, (void*)i);
    ASSERT_GT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(e->m_ParticleTimeLeft, e->m_Particles[0].m_TimeLeft);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));
    ASSERT_LT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_LT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, StartOnceInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_LT(e->m_Prototype->m_DDF->m_Duration, e->m_Timer);
    ASSERT_FALSE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, StartLoopInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("loop.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);

    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, FireAndForget)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::FireAndForget(m_Context, m_Prototype, Vectormath::Aos::Point3(0.0f, 0.0f, 0.0f), Vectormath::Aos::Quat::identity());

    // counting on 0
    uint16_t index = 0;

    dmParticle::Instance* i = m_Context->m_Instances[index];

    ASSERT_EQ(1U, m_Context->m_InstanceIndexPool.Size());
    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, i->m_VersionNumber << 16));

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Instances[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_NE((void*)0, (void*)m_Context->m_Instances[index]);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_EQ((void*)0, (void*)m_Context->m_Instances[index]);
    ASSERT_EQ(0U, m_Context->m_InstanceIndexPool.Size());
}

TEST_F(ParticleTest, EmissionSpace)
{
    float dt = 1.0f / 60.0f;

    // Test world space

    ASSERT_TRUE(LoadPrototype("once.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::SetPosition(m_Context, instance, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(10.0f, e->m_Particles[0].m_Position.getX());

    dmParticle::DestroyInstance(m_Context, instance);

    // Test emitter space

    dmParticle::Particle_DeletePrototype(m_Prototype);
    ASSERT_TRUE(LoadPrototype("emitter_space.particlefxc", &m_Prototype));
    instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    index = instance & 0xffff;

    dmParticle::SetPosition(m_Context, instance, Vectormath::Aos::Point3(10.0f, 0.0f, 0.0f));
    dmParticle::StartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_EQ(0.0f, e->m_Particles[0].m_Position.getX());

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, RestartInstance)
{
    float dt = 1.0f / 60.0f;

    ASSERT_TRUE(LoadPrototype("restart.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;

    dmParticle::StartInstance(m_Context, instance);

    ASSERT_TRUE(dmParticle::IsSpawning(m_Context, instance));

    dmParticle::Emitter* e = &m_Context->m_Instances[index]->m_Emitters[0];
    ASSERT_GE(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_GE(0.0f, e->m_Particles[1].m_TimeLeft);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_LT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_GE(0.0f, e->m_Particles[1].m_TimeLeft);

    dmParticle::RestartInstance(m_Context, instance);

    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);

    ASSERT_GT(0.0f, e->m_Particles[0].m_TimeLeft);
    ASSERT_LT(0.0f, e->m_Particles[1].m_TimeLeft);

    dmParticle::DestroyInstance(m_Context, instance);
}

/**
 * The emitter has a spline for particle size, which has the points and tangents:
 * (0.00, 0), (1,0)
 * (0.25, 0), (1,1)
 * (0.50, 1), (1,0)
 * (0.75, 0), (1,-1)
 * (1.00, 0), (1,0)
 *
 * Test evaluation of the size at t = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1]
 */
TEST_F(ParticleTest, EvaluateEmitterProperty)
{
    float dt = 1.0f / 8.0f;

    ASSERT_TRUE(LoadPrototype("emitter_spline.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];

    ASSERT_GE(0.0f, particle->m_TimeLeft);

    // t = 0, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.125, size < 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 0.25, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.375, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.5, size = 1
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(1.0f, particle->m_Size);

    // t = 0.625, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.75, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.875, size < 0
    // Updating with a full dt here will make the emitter reach its duration
    float epsilon = 0.000001f;
    dmParticle::Update(m_Context, dt - epsilon, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 1, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_NEAR(0.0f, particle->m_Size, epsilon);

    dmParticle::DestroyInstance(m_Context, instance);
}

/**
 * The emitter has a spline for particle scale (size is always 1), which has the points and tangents:
 * (0.00, 0), (1,0)
 * (0.25, 0), (1,1)
 * (0.50, 1), (1,0)
 * (0.75, 0), (1,-1)
 * (1.00, 0), (1,0)
 *
 * Test evaluation of the size at t = [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875, 1]
 */
TEST_F(ParticleTest, EvaluateParticleProperty)
{
    float dt = 1.0f / 8.0f;

    ASSERT_TRUE(LoadPrototype("particle_spline.particlefxc", &m_Prototype));
    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];

    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Particle* particle = &i->m_Emitters[0].m_Particles[0];

    ASSERT_GE(0.0f, particle->m_TimeLeft);

    // t = 0, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.125, size < 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 0.25, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.375, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.5, size = 1
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(1.0f, particle->m_Size);

    // t = 0.625, size > 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_LT(0.0f, particle->m_Size);

    // t = 0.75, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_DOUBLE_EQ(0.0f, particle->m_Size);

    // t = 0.875, size < 0
    // Updating with a full dt here will make the emitter reach its duration
    float epsilon = 0.000001f;
    dmParticle::Update(m_Context, dt - epsilon, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_GT(0.0f, particle->m_Size);

    // t = 1, size = 0
    dmParticle::Update(m_Context, dt, m_VertexBuffer, m_VertexBufferSize, 0x0, 0x0);
    ASSERT_NEAR(0.0f, particle->m_Size, epsilon);

    dmParticle::DestroyInstance(m_Context, instance);
}

/**
 * Test that flip book animations are updated correctly
 */
struct AnimRenderData
{
    void* m_Material;
    void* m_Texture;
    uint32_t m_VertexIndex;
    uint32_t m_VertexCount;
};

struct ParticleVertex
{
    float m_X, m_Y, m_Z;
    float m_U, m_V;
    float m_Alpha;
};

void AnimRenderInstanceCallback(void* usercontext, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count)
{
    AnimRenderData* data = (AnimRenderData*)usercontext;
    data->m_Material = material;
    data->m_Texture = texture;
    data->m_VertexIndex = vertex_index;
    data->m_VertexCount = vertex_count;
}

float g_TexCoords[] =
{
        // 2 x 4 tiles
        0.0f, 0.0f, 0.25f, 0.5f,
        0.25f, 0.0f, 0.5f, 0.5f,
        0.5f, 0.0f, 0.75f, 0.5f,
        0.75f, 0.0f, 1.0f, 0.5f,
        0.0f, 0.5f, 0.25f, 1.0f,
        0.25f, 0.5f, 0.5f, 1.0f,
        0.5f, 0.5f, 0.75f, 1.0f,
        0.75f, 0.5f, 1.0f, 1.0f,
};

struct TileSource
{
    TileSource()
    : m_Texture((void*)0xBAADF00D)
    , m_TexCoords(g_TexCoords)
    {

    }

    void* m_Texture;
    float* m_TexCoords;
};

dmParticle::FetchAnimationResult FetchAnimationCallback(void* tile_source, dmhash_t animation, dmParticle::AnimationData* out_data)
{
    if (tile_source == 0x0)
    {
        return dmParticle::FETCH_ANIMATION_UNKNOWN_ERROR;
    }
    TileSource* ts = (TileSource*)tile_source;
    out_data->m_Texture = ts->m_Texture;
    out_data->m_TexCoords = ts->m_TexCoords;
    if (animation == dmHashString64("once"))
    {
        out_data->m_Playback = dmParticle::ANIM_PLAYBACK_ONCE_FORWARD;
        out_data->m_StartTile = 1;
        out_data->m_EndTile = 5;
        out_data->m_FPS = 30;
    }
    else
    {
        return dmParticle::FETCH_ANIMATION_NOT_FOUND;
    }
    out_data->m_Texture = (void*)0xBAADF00D;
    return dmParticle::FETCH_ANIMATION_OK;
}

void ParticleTest::VerifyUVs(ParticleVertex* vertex_buffer, uint32_t tile)
{
    uint32_t u0 = 0;
    uint32_t v0 = 1;
    uint32_t u1 = 2;
    uint32_t v1 = 3;
    float* tc = &g_TexCoords[tile * 4];
    // The particle vertices are ordered like an N, where the first triangle is the lower left, second is upper right
    ASSERT_FLOAT_EQ(tc[u0], vertex_buffer[0].m_U);
    ASSERT_FLOAT_EQ(tc[v1], vertex_buffer[0].m_V);
    ASSERT_FLOAT_EQ(tc[u0], vertex_buffer[1].m_U);
    ASSERT_FLOAT_EQ(tc[v0], vertex_buffer[1].m_V);
    ASSERT_FLOAT_EQ(tc[u1], vertex_buffer[2].m_U);
    ASSERT_FLOAT_EQ(tc[v1], vertex_buffer[2].m_V);
    ASSERT_FLOAT_EQ(tc[u1], vertex_buffer[3].m_U);
    ASSERT_FLOAT_EQ(tc[v1], vertex_buffer[3].m_V);
    ASSERT_FLOAT_EQ(tc[u0], vertex_buffer[4].m_U);
    ASSERT_FLOAT_EQ(tc[v0], vertex_buffer[4].m_V);
    ASSERT_FLOAT_EQ(tc[u1], vertex_buffer[5].m_U);
    ASSERT_FLOAT_EQ(tc[v0], vertex_buffer[5].m_V);
}

TEST_F(ParticleTest, Animation)
{
    float dt = 0.2f;

    ASSERT_TRUE(LoadPrototype("anim.particlefxc", &m_Prototype));

    dmParticle::HInstance instance = dmParticle::CreateInstance(m_Context, m_Prototype);
    uint16_t index = instance & 0xffff;
    dmParticle::Instance* i = m_Context->m_Instances[index];
    dmParticle::Emitter* emitter = &i->m_Emitters[0];

    TileSource tile_source;
    dmParticle::SetTileSource(m_Prototype, 0, &tile_source);

    dmParticle::StartInstance(m_Context, instance);
    dmParticle::Particle* particle = &emitter->m_Particles[0];

    ParticleVertex vertex_buffer[6];
    uint32_t vertex_buffer_size;

    // Expect failure since anim can't be found
    dmParticle::Update(m_Context, dt, (float*)vertex_buffer, sizeof(vertex_buffer), &vertex_buffer_size, FetchAnimationCallback);
    ASSERT_EQ(0u, vertex_buffer_size);
    ASSERT_EQ(0.0f, particle->m_TimeLeft);

    // Test once anim
    emitter->m_Prototype->m_Animation = dmHashString64("once");

    // 5 tiles
    for (uint32_t i = 0; i < 5; ++i)
    {
        dmParticle::Update(m_Context, dt, (float*)vertex_buffer, sizeof(vertex_buffer), &vertex_buffer_size, FetchAnimationCallback);
        VerifyUVs(vertex_buffer, i);
    }

    ASSERT_EQ(sizeof(vertex_buffer), vertex_buffer_size);
    ASSERT_LT(0.0f, particle->m_TimeLeft);

    // Test rendering of last frame
    AnimRenderData data;
    dmParticle::Render(m_Context, &data, AnimRenderInstanceCallback);
    ASSERT_EQ(m_Prototype->m_Emitters[0].m_Material, data.m_Material);
    ASSERT_EQ(tile_source.m_Texture, data.m_Texture);
    ASSERT_EQ(0u, data.m_VertexIndex);
    ASSERT_EQ(6u, data.m_VertexCount);

    // Particle dead
    dmParticle::Update(m_Context, dt, (float*)vertex_buffer, sizeof(vertex_buffer), &vertex_buffer_size, FetchAnimationCallback);
    ASSERT_EQ(0u, vertex_buffer_size);
    ASSERT_GT(0.0f, particle->m_TimeLeft);

    dmParticle::DestroyInstance(m_Context, instance);
}

TEST_F(ParticleTest, InvalidKeys)
{
    ASSERT_TRUE(LoadPrototype("invalid_keys.particlefxc", &m_Prototype));
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);

    int ret = RUN_ALL_TESTS();
    return ret;
}
