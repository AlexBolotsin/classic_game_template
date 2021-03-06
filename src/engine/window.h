#pragma once

namespace cgt
{

class Window;

class WindowConfig
{
public:
    static WindowConfig Default();

    WindowConfig WithTitle(const char* title);
    WindowConfig WithDimensions(u32 width, u32 height);
    std::shared_ptr<Window> Build() const;
    
    std::string title = "Classic Game Template";
    u32 width = 1280;
    u32 height = 720;
};

class Window : private NonCopyable
{
public:
    static std::shared_ptr<Window> BuildWithConfig(const WindowConfig& config);

    ~Window();

    u32 GetWidth() const;
    u32 GetHeight() const;

    SDL_Window* GetSDLWindow() { return m_Window; }

private:
    friend class EventLoop;

    explicit Window(SDL_Window* window);

    bool PollEvent(SDL_Event& outEvent);

    SDL_Window* m_Window;
};

}
