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
  static SDL_Color const black{0x00, 0x00, 0x00, SDL_ALPHA_OPAQUE};
  static SDL_Color const dark_green{0x00, 0x80, 0x00, SDL_ALPHA_OPAQUE};
  static SDL_Color const dark_grey{0x60, 0x60, 0x60, SDL_ALPHA_OPAQUE};
  static SDL_Color const grey{0x80, 0x80, 0x80, SDL_ALPHA_OPAQUE};
  static SDL_Color const white{0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE};
  static SDL_Color const dark_yellow{0x80, 0x80,  0x40, SDL_ALPHA_OPAQUE};
  static SDL_Color const dark_red{0xA4, 0x00, 0x00, SDL_ALPHA_OPAQUE};

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
      if( SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0 )
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

  /** Creates SDL window with standard parameters, owned by the returned
      unique_ptr */
  inline window create_desktop_window(char const* title, bool full_screen)
  {
    return create_window(
      title,
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      640,
      480,
      SDL_WINDOW_SHOWN | (full_screen ?
                          SDL_WINDOW_FULLSCREEN_DESKTOP :
                          SDL_WINDOW_RESIZABLE));
  };

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

  /** Sets the draw colour for an SDL renderer */
  inline void render_set_colour(renderer const& r, SDL_Color const& c)
  {
    SDL_SetRenderDrawColor(r.get(), c.r, c.g, c.b, c.a);
  }

  /** std::unique_ptr wrapper for SDL_Surface */
  using surface = std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)>;

  /** std::unique_ptr wrapper for SDL_Texture */
  using texture = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

  inline texture null_texture()
  {
    return {nullptr, nullptr};
  }

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

  /** Sets a style on a renderer and removes it on destruction */
  class style
  {
  public:
    style(renderer const& r)
      : r_(r)
      , attrs_(0)
    {}

    ~style()
    {
      if( attrs_ & colour )
      {
        SDL_SetRenderDrawColor(r_.get(), c_.r, c_.g, c_.b, c_.a);
      }
    }

    style& set_colour(SDL_Color c)
    {
      attrs_ |= colour;
      SDL_GetRenderDrawColor(r_.get(), &c_.r, &c_.g, &c_.b, &c_.a);
      SDL_SetRenderDrawColor(r_.get(), c.r, c.g, c.b, c.a);
      return *this;
    }

  private:
    enum Attrs
    {
      colour = 1 >> 1
    };
    renderer const& r_;
    int attrs_;
    SDL_Color c_;
  };
}

#endif // SDL2_CPP_SDL2_H
