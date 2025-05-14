// Header only C++ wrapper library for SDL2
//
// Copyright Ian Wakeling 2020
// License MIT

#ifndef SDL2_CPP_BITMAP_H
#define SDL2_CPP_BITMAP_H

#include "sdl2.h"

#include <string_view>

namespace sdl
{
  class bitmap
  {
  public:
    bitmap()
      : surface_(nullptr, nullptr)
      , texture_(sdl::null_texture()) {}

    bitmap(std::string const& fileName)
      : surface_(SDL_LoadBMP(fileName.c_str()), SDL_FreeSurface)
      , texture_(sdl::null_texture())
    {
        if( surface_ )
        {
          width_ = surface_->w;
          height_ = surface_->h;
        }
    }

    bitmap(std::string const& fileName, SDL_Color mod)
      : surface_(SDL_LoadBMP(fileName.c_str()), SDL_FreeSurface)
      , texture_(sdl::null_texture())
    {
        if( surface_ )
        {
          SDL_SetSurfaceColorMod(surface_.get(), mod.r, mod.g, mod.b);
          width_ = surface_->w;
          height_ = surface_->h;
        }
    }

    bitmap(bitmap&& rhs)
      : surface_(std::move(rhs.surface_))
      , texture_(std::move(rhs.texture_))
      , width_(rhs.width_)
      , height_(rhs.height_) {}

    bitmap& operator=(bitmap&& rhs)
    {
      std::swap(surface_, rhs.surface_);
      std::swap(texture_, rhs.texture_);
      std::swap(width_, rhs.width_);
      std::swap(height_, rhs.height_);
      return *this;
    }

    int width() const { return width_; }
    int height() const { return height_; }

    operator bool()
    {
      return !!texture_;
    }

    bitmap& colourMod(SDL_Color const& mod)
    {
      if( surface_ )
      {
        SDL_SetSurfaceColorMod(surface_.get(), mod.r, mod.g, mod.b);
      }
      return *this;
    }

    void render(sdl::renderer const& renderer, int x, int y)
    {
      if( !texture_)
      {
        if( surface_ )
        {
          texture_ = sdl::create_texture_from_surface(renderer, surface_);
        }
      }
      if( texture_ )
      {
        SDL_Rect dest{x, y, width_, height_};
        sdl::render_copy(renderer, texture_, nullptr, &dest);
      }
    }

  private:
    sdl::surface surface_;
    sdl::texture texture_;
    int width_;
    int height_;
  };
}

#endif // SDL2_CPP_BITMAP_H

