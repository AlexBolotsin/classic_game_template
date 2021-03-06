#pragma once

#include <memory>
#include <render_core/render_config.h>
#include <render_core/sprite_draw_list.h>
#include <render_core/i_camera.h>

namespace cgt::render
{

struct RenderStats
{
    void Reset() { spriteCount = 0; drawcallCount = 0; }

    void operator+=(const RenderStats& other)
    {
        spriteCount += other.spriteCount;
        drawcallCount += other.drawcallCount;
    }

    u32 spriteCount = 0;
    u32 drawcallCount = 0;
};

class IRenderContext
{
public:
    // meant to be implemented by concrete rendering libraries
    static std::shared_ptr<IRenderContext> BuildWithConfig(RenderConfig config);

    virtual TextureHandle LoadTexture(const std::filesystem::path& absolutePath) = 0;
    virtual ImTextureID GetImTextureID(const TextureHandle& texture) = 0;
    virtual usize GetTextureSortKey(const TextureHandle& texture) = 0;

    virtual void Clear(glm::vec4 clearColor) = 0;
    virtual RenderStats Submit(SpriteDrawList& drawList, const ICamera& camera, bool sortBeforeRendering = true) = 0;
    virtual void Present() = 0;

    virtual ~IRenderContext() = default;

protected:
    friend class ImGuiHelper;

    virtual void ImGuiBindingsInit() = 0;
    virtual void ImGuiBindingsNewFrame() = 0;
    virtual void ImGuiBindingsRender(ImDrawData* drawData) = 0;
    virtual void ImGuiBindingsShutdown() = 0;

    virtual void Im3dBindingsInit() = 0;
    virtual void Im3dBindingsNewFrame() = 0;
    virtual void Im3dBindingsRender(const ICamera& camera) = 0;
    virtual void Im3dBindingsShutdown() = 0;
};

}
