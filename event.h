// Header only C++ wrapper library for SDL2
//
// Copyright Ian Wakeling 2020
// License MIT

#ifndef SDL2_CPP_EVENT_H
#define SDL2_CPP_EVENT_H

#include <functional>
#include <SDL2/SDL.h>
#include <vector>

namespace sdl2
{
  /** Map SDL events to handler functions.
  */
  class event_map
  {
  public:
    event_map() = default;
    ~event_map() = default;

    event_map(event_map const&) = delete;
    event_map(event_map&&) = delete;
    void operator=(event_map const&) = delete;
    void operator=(event_map&&) = delete;

    /** Add a handler function to receive key down events.

        When the specified key is pressed, calls f(args...). Repeat events
        are rate limited.
    */
    template<class Function, class... Args>
    void add_key_down_handler(SDL_Keycode keycode, Function&& f, Args&&... args)
    {
      handlers_.push_back(make_key_down_adaptor(keycode, std::bind(f, args...)));
    }

    /** Add a handler function to receive key up events.

        When the specified key is pressed, calls f(args...).
    */
    template<class Function, class... Args>
    void add_key_up_handler(SDL_Keycode keycode, Function&& f, Args&&... args)
    {
      handlers_.push_back(make_key_up_adaptor(keycode, std::bind(f, args...)));
    }

    /** Add a handler function to conditionally handle events.

        When an event not consumed by an earlier handler is received,
        if e.type == t, calls f(args..., <event>).
        If f returns true, the event is consumed, otherwise the next handler
        is tried.
    */
    template<class EventType, class Function, class... Args>
    void add_handler(EventType t, Function&& f, Args&&... args)
    {
      handlers_.push_back(
        std::make_pair(
          static_cast<event_type>(t),
          std::bind(f, args..., std::placeholders::_1)));
    }

    /** Call the handler matching the event, if any.

        If multiple matching handlers are present, tries each in turn until the
        event is consumed.

        If a matching handler is found, returns true, otherwise returns false.
    */
    bool handle_event(SDL_Event const& e)
    {
      return std::find_if(handlers_.begin(),
                          handlers_.end(),
                          [&e](auto& candidate)
                          {
                            return (e.type == candidate.first &&
                                    candidate.second(e));
                          }) != handlers_.end();
    }

  private:
    using event_type = unsigned int;
    using handler = std::function<bool(SDL_Event const& e)>;

    template<class Callable>
    struct key_down_adaptor
    {
      key_down_adaptor(SDL_Keycode keycode, Callable handler)
        : keycode_(keycode)
        , handler_(std::move(handler))
      {
      }

      bool operator()(SDL_Event const& e)
      {
        if(e.key.keysym.sym == keycode_)
        {
          if(!rate_limited(e))
          {
            handler_();
          }
          return true;
        }

        return false;
      }

    private:
      bool rate_limited(SDL_Event const& e)
      {
        if( e.key.repeat == 0 )
        {
          key_repeat_count_ = 0;
        }
        else
        {
          key_repeat_count_++;
        }
        auto last_key_timestamp = last_key_timestamp_;
        last_key_timestamp_ = e.key.timestamp;

        return (e.key.repeat != 0 &&
                (e.key.timestamp - last_key_timestamp) <
                (key_repeat_count_ > 0 ? 25 : 500));
      }

    private:
      SDL_Keycode keycode_;
      Callable handler_;
      unsigned int last_key_timestamp_ = 0;
      unsigned int key_repeat_count_ = 0;
    };

    template<class Callable>
    std::pair<event_type,key_down_adaptor<Callable>> make_key_down_adaptor(
      SDL_Keycode keycode,
      Callable handler)
    {
      return std::make_pair(
        static_cast<event_type>(SDL_KEYDOWN),
        key_down_adaptor<Callable>(keycode, std::move(handler)));
    }

    template<class Callable>
    struct key_up_adaptor
    {
      key_up_adaptor(SDL_Keycode keycode, Callable handler)
        : keycode_(keycode)
        , handler_(std::move(handler))
      {
      }

      bool operator()(SDL_Event const& e)
      {
        if( e.key.keysym.sym == keycode_)
        {
          handler_();
          return true;
        }

        return false;
      }

    private:
      SDL_Keycode keycode_;
      Callable handler_;
    };

    template<class Callable>
    std::pair<event_type,key_up_adaptor<Callable>> make_key_up_adaptor(
      SDL_Keycode keycode,
      Callable handler)
    {
      return std::make_pair(
        static_cast<event_type>(SDL_KEYUP),
        key_up_adaptor<Callable>(keycode, std::move(handler)));
    }

  private:
    std::vector<std::pair<event_type,handler>> handlers_;
  };
} // namespace sdl2

#endif // SDL2_CPP_EVENT_H
