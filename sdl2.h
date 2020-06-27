// Header only C++ wrapper library for SDL2
//
// Copyright Ian Wakeling 2020
// License MIT

#ifndef SDL2_CPP_SDL2_H
#define SDL2_CPP_SDL2_H

#include <memory>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdexcept>

namespace sdl
{
  static SDL_Color const dark_grey{0x20, 0x20, 0x20};
  static SDL_Color const grey{0x80, 0x80, 0x80};
  static SDL_Color const white{0xFF, 0xFF, 0xFF};

  /** Throws a std::runtime_error with a string combining a prefix and
      the most recent SDL error
  */
  inline void throw_error(std::string const& prefix)
  {
    throw std::runtime_error(prefix + SDL_GetError());
  }

  /** RAII class to initialise and release the SDL library */
  struct lib
  {
    lib()
    {
      if( SDL_Init(SDL_INIT_VIDEO) < 0 )
      {
        throw_error("Failed to initialise SDL: ");
      }
    }
    ~lib()
    {
      SDL_Quit();
    }
  };

  /** Initialises the SDL library and returns a token whose destruction
      will release the SDL library
  */
  inline std::unique_ptr<lib> init()
  {
    return std::make_unique<lib>();
  }

  /** std::unique_ptr wrapper for SDL_Window */
  using window = std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)>;

  /** Creates an SDL window owned by the returned unique_ptr */
  template<class ...Ts>
  window create_window(Ts... args)
  {
    return window(SDL_CreateWindow(args...), SDL_DestroyWindow);
  }

  /** std::unique_ptr wrapper for SDL_Renderer */
  using renderer = std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)>;

  /** Creates an SDL renderer for the specified window, owned by the returned
      unique_ptr */
  template<class ...Ts>
  renderer create_renderer(window const& w, Ts... args)
  {
    return renderer(SDL_CreateRenderer(w.get(), args...),
                    SDL_DestroyRenderer);
  }

  /** std::unique_ptr wrapper for SDL_Surface */
  using surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;

  /** std::unique_ptr wrapper for SDL_Texture */
  using texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

  /** Creates an SDL texture for the specified renderer, owned by the returned
      unique_ptr */
  inline texture create_texture_from_surface(renderer const& r, surface const& s)
  {
    return texture(SDL_CreateTextureFromSurface(r.get(), s.get()),
                   SDL_DestroyTexture);
  }

  /** Copies the specified texture to the specified renderer */
  template<class ...Ts>
  void render_copy(renderer const& r, texture const& t, Ts... args)
  {
    SDL_RenderCopy(r.get(), t.get(), args...);
  }
}

#endif // SDL2_CPP_SDL2_H
