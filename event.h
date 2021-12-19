// Header only C++ wrapper library for SDL2
//
// Copyright Ian Wakeling 2020
// License MIT

#ifndef SDL2_CPP_EVENT_H
#define SDL2_CPP_EVENT_H

#include <algorithm>
#include <chrono>
#include <functional>
#include <limits>
#include <vector>

#include <SDL2/SDL.h>

using namespace std::literals;

namespace sdl2
{
  /** Map SDL events to handler functions.
  */
  class event_map
  {
  public:
    event_map()
    {
      stop_event_type_ = SDL_RegisterEvents(2);
      if( stop_event_type_ != -1 )
      {
        add_timer_event_type_ = stop_event_type_ + 1;
      }
    }

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

    /** Add a timer

        Note: timers only work when run_event_loop() is used.

        calls f(args...) after interval milliseconds
        if one_time = false f is called every timeout milliseconds, otherwise
        f is called only once

        returns true if the timer was succesfully added, false otherwise
    */
    template<class Function, class... Args>
    bool add_timer(
      std::chrono::milliseconds interval,
      bool one_shot,
      Function&& f,
      Args&&... args)
    {
      if( add_timer_event_type_ == -1 )
      {
        return false;
      }

      auto when = std::chrono::steady_clock::now() + interval;
      SDL_Event event;
      event.type = add_timer_event_type_;
      event.user.data1 = new timer(
        std::move(when),
        std::move(interval),
        one_shot,
        std::bind(f, args...));
      SDL_PushEvent(&event);
      return true;
    }

    /** Run an event loop.
        Returns false if the event loop could not be started, or true on
        SDL_QUIT or when stop_event_loop is called.

        Calls render_fun(args...) to render after each event.
    */
    template<class Function, class... Args>
    bool run_event_loop(Function&& render_fun, Args&&... args)
    {
      // don't start if we couldn't get event types
      if( stop_event_type_ == -1 )
      {
        return false;
      }

      stop_ = false;
      while( !stop_ )
      {
        auto now = std::chrono::steady_clock::now();
        int timeout = std::numeric_limits<int>::max();
        if( timers_.size() > 0 )
        {
          auto next_timer = std::min_element(
            timers_.begin(),
            timers_.end(),
            [](timer const& lhs, timer const& rhs)
            {
              return lhs.when_ < rhs.when_;
            });
          if( next_timer->when_ > now )
          {
            timeout = (next_timer->when_ - now) / 1ms;
          }
          else
          {
            timeout = 0;
          }
        }

        SDL_Event e;
        if( SDL_WaitEventTimeout(&e, timeout) )
        {
          if( e.type == SDL_QUIT || e.type == stop_event_type_ )
          {
            stop_ = true;
          }
          else if( e.type == add_timer_event_type_ )
          {
            std::unique_ptr<timer> t(static_cast<timer*>(e.user.data1));
            timers_.push_back(*t);
          }
          else
          {
            handle_event(e);
          }
        }
        else
        {
          handle_timeout();
        }

        render_fun(args...);
      }

      return true;
    }

    /** Stops the running event loop, if any
    */
    void stop_event_loop()
    {
      if( stop_event_type_ != -1 )
      {
        SDL_Event event;
        event.type = stop_event_type_;
        SDL_PushEvent(&event);
      }
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
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    struct timer
    {
      timer(
        time_point when,
        std::chrono::milliseconds interval,
        bool one_shot,
        std::function<void()> handler)
        : when_(std::move(when))
        , interval_(std::move(interval))
        , one_shot_(one_shot)
        , handler_(std::move(handler)) {}

      time_point when_;
      std::chrono::milliseconds interval_;
      bool one_shot_;
      std::function<void()> handler_;
    };

    void handle_timeout()
    {
      auto now = std::chrono::steady_clock::now();
      timers_.erase(
        std::remove_if(
          timers_.begin(),
          timers_.end(),
          [&now](timer& t)
          {
            bool remove = false;
            if(now >= t.when_)
            {
              t.handler_();
              if( t.one_shot_ )
              {
                remove = true;
              }
              else
              {
                t.when_ = now + t.interval_;
              }
            }
            return remove;
          }),
        timers_.end());
    }

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
    std::vector<timer> timers_;
    event_type stop_event_type_ = -1;
    event_type add_timer_event_type_ = -1;
    bool stop_;
  };
} // namespace sdl2

#endif // SDL2_CPP_EVENT_H
