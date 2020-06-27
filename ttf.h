// Header only C++ wrapper library for SDL2
//
// Copyright Ian Wakeling 2020
// License MIT

#ifndef SDL2_CPP_TTF_H
#define SDL2_CPP_TTF_H

#include "sdl2.h"

#include <SDL2/SDL_ttf.h>
#include <stdexcept>

#include <iostream>

namespace sdl
{
  namespace ttf
  {
    /** Throws a std::runtime_error with a string combining a prefix and
        the most recent TTF error
    */
    inline void throw_error(std::string const& prefix)
    {
      throw std::runtime_error(prefix + TTF_GetError());
    }

    /** RAII class to initialise and release the TTF library */
    struct lib
    {
      lib()
      {
        if( TTF_Init() < 0 )
        {
          throw_error("Failed to initialise SDL TTF support: ");
        }
      }

      ~lib()
      {
        TTF_Quit();
      }
    };

    /** Initialises the TTF library and returns a token whose destruction
        will release the TTF library
    */
    inline std::unique_ptr<lib> init()
    {
      return std::make_unique<lib>();
    }

    /** std::unique_ptr wrapper for TTF_Font */
    using font = std::unique_ptr<TTF_Font, decltype(&TTF_CloseFont)>;

    /** Creates a TTF font object owned by the returned unique_ptr */
    template<class ...Ts>
    font open_font(Ts... args)
    {
      return font(TTF_OpenFont(args...), TTF_CloseFont);
    }

    /** Returns the width in pixels of the specified string when rendered
        with the specified font
    */
    template<class ...Ts>
    void size(font const& f, std::string const& text, Ts... args)
    {
      TTF_SizeUTF8(f.get(), text.c_str(), args...);
    }

    /** Renders the specified string to an SDL surface using the specified font.

        The surface is owned by the returned unique_ptr.
    */
    template<class ...Ts>
    surface render_blended(font const& f, std::string const& text, Ts... args)
    {
      return surface(TTF_RenderUTF8_Blended(f.get(), text.c_str(), args...),
                     SDL_FreeSurface);
    }
  }
}

#endif // SDL2_CPP_TTF_H
