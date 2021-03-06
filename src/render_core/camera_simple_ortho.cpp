#include <render_core/pch.h>

#include <render_core/camera_simple_ortho.h>
#include <engine/window.h>

namespace cgt::render
{
CameraSimpleOrtho::CameraSimpleOrtho(const Window& window)
{
    windowWidth = window.GetWidth();
    windowHeight = window.GetHeight();
}

glm::mat4 CameraSimpleOrtho::GetView() const
{
    const float unitsPerPixel = 1.0f / pixelsPerUnit;
    glm::vec2 snappedPosition = position;
    if (snapToPixel)
    {
        snappedPosition.x = glm::trunc(snappedPosition.x / unitsPerPixel) * unitsPerPixel;
        snappedPosition.y = glm::trunc(snappedPosition.y / unitsPerPixel) * unitsPerPixel;
    }

    const glm::mat4 view = glm::lookAt(
        glm::vec3(snappedPosition, -1.0f),
        glm::vec3(snappedPosition, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));

    return view;
}

glm::mat4 CameraSimpleOrtho::GetProjection() const
{
    const float unitsPerPixel = 1.0f / pixelsPerUnit;

    const glm::mat4 projection = glm::ortho(
        windowWidth * -0.5f * unitsPerPixel,
        windowWidth * 0.5f * unitsPerPixel,
        windowHeight * -0.5f * unitsPerPixel,
        windowHeight * 0.5f * unitsPerPixel,
        0.0f,
        1.0f);

    return projection;
}

glm::mat4 CameraSimpleOrtho::GetViewProjection() const
{
    return GetProjection() * GetView();
}

glm::vec3 CameraSimpleOrtho::GetPosition() const
{
    return glm::vec3(position, -1.0f);
}

glm::vec3 CameraSimpleOrtho::GetForwardDirection() const
{
    return glm::vec3(0.0f, 0.0f, 1.0f);
}

glm::vec3 CameraSimpleOrtho::GetUpDirection() const
{
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

bool CameraSimpleOrtho::IsOrthographic() const
{
    return true;
}

glm::vec2 CameraSimpleOrtho::ScreenToWorld(u32 screenX, u32 screenY) const
{
    const glm::mat4 vp = GetViewProjection();
    const glm::mat4 vpInverse = glm::inverse(vp);

    const glm::vec2 ndc(
        screenX / windowWidth * 2.0f - 1.0f,
        (windowHeight - screenY) / windowHeight * 2.0f - 1.0f);

    const glm::vec2 world =  vpInverse * glm::vec4(ndc, 0.0f, 1.0f);

    return world;
}

glm::vec2 CameraSimpleOrtho::WorldToScreen(glm::vec2 world) const
{
    const glm::mat4 vp = GetViewProjection();
    const glm::vec2 ndc = vp * glm::vec4(world, 0.0f, 1.0f);
    const glm::vec2 normalizedScreen = (ndc + glm::vec2(1.0f)) * glm::vec2(0.5f);
    const glm::vec2 screen(normalizedScreen.x * windowWidth, (1.0f - normalizedScreen.y) * windowHeight);

    return screen;
}

}
