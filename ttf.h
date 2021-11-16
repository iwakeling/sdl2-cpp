// Header only C++ wrapper library for SDL2
//
// Copyright Ian Wakeling 2020
// License MIT

#ifndef SDL2_CPP_TTF_H
#define SDL2_CPP_TTF_H

#include "sdl2.h"

#include <fontconfig/fontconfig.h>
#include <SDL2/SDL_ttf.h>
#include <stdexcept>

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

    std::string GetFontFile(std::string const& fontName)
    {
      std::string fontFile;
      auto config = FcInitLoadConfigAndFonts();
      auto pat = FcNameParse(reinterpret_cast<FcChar8 const*>(fontName.c_str()));
      FcConfigSubstitute(config, pat, FcMatchPattern);
      FcDefaultSubstitute(pat);

      auto result = FcResultNoMatch;
      auto font = FcFontMatch(config, pat, &result);
      if( result == FcResultMatch && font != nullptr )
      {
        FcChar8* fileName = NULL;
        if( FcPatternGetString(font, FC_FILE, 0, &fileName) == FcResultMatch )
        {
          fontFile = reinterpret_cast<char const*>(fileName);
        }
        FcPatternDestroy(font);
      }
      FcPatternDestroy(pat);
      return fontFile;
    }
  }
}

#endif // SDL2_CPP_TTF_H
