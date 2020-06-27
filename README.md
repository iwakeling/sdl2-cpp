# sdl2-cpp
## Deliberately trivial C++ wrapper for SDL2

This header only library is a simple convenience wrapper for working with SDL2 in C++.

It is mainly focussed on providing RAII types for SDL2 and for the most part, only functions that create resources are wrapped.

Example:
```
    auto sdl_lib = sdl::init();
    auto sdl_ttf_lib = sdl::ttf::init();
    auto window = sdl::create_window(
      "MyExample",
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      1024,
      720,
      SDL_WINDOW_MAXIMIZED);
    if( !window )
    {
      sdl::throw_error("Failed to create SDL window: ");
    }
    SDL_Rect window_size;
    SDL_GetWindowSize(window.get(), &window_size.w, &window_size.h);

    auto renderer = sdl::create_renderer(window, -1, SDL_RENDERER_ACCELERATED);

```
