// Header only C++ wrapper library for SDL2
//
// Copyright Ian Wakeling 2020
// License MIT

#ifndef SDL2_CPP_EVENT_H
#define SDL2_CPP_EVENT_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <vector>

#include <SDL2/SDL.h>

using namespace std::literals;

namespace sdl2
{
  /** Map SDL events to handler functions.

      event_map can be used either with or without an event loop.

      Without an event loop, it is the caller's responsibility to call
      handle_event() and to ensure thread safety between calls to
      add_handler() (and the wrapper functions) and
      handle_event(). In this mode, timers are not available.

      With an event loop, all calls to add_handler() (and the wrapper
      functions) must be completed before calling
      run_event_loop(). Calls to stop_event_loop(), add_timer() and
      remove_timer() are thread safe and may be called at any time. In
      this mode, timers and event handlers will be called on a thread
      belonging to the event_map.
  */
  class event_map
  {
  public:
    event_map()
    {
      stop_event_type_ = SDL_RegisterEvents(3);
      if( stop_event_type_ != -1 )
      {
        add_timer_event_type_ = stop_event_type_ + 1;
        stop_timer_event_type_ = stop_event_type_ + 2;
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

        returns non-zero if the timer was succesfully added, zero
        otherwise. The returned value can be used in a call to
        stop_timer(). If more than
        std::numeric_limits<unsigned int>::max are created, the
        returned values may wrap and no longer be unique.
    */
    template<class Function, class... Args>
    unsigned int add_timer(
      std::chrono::milliseconds interval,
      bool one_shot,
      Function&& f,
      Args&&... args)
    {
      if( add_timer_event_type_ == -1 )
      {
        return 0;
      }

      auto when = std::chrono::steady_clock::now() + interval;
      auto id = next_timer_id_++;
      SDL_Event event;
      event.type = add_timer_event_type_;
      event.user.data1 = new timer(
        id,
        std::move(when),
        std::move(interval),
        one_shot,
        std::bind(f, args...));
      SDL_PushEvent(&event);
      return id;
    }

    /** Stop a timer

        Removes the specified timer so that no further calls to the
        function associated with the timer will occur. `id` is the
        value returned from add_timer(). This call is asynchronous, so
        a maximum of one call to the timer function may already be in
        flight when this call returns. If this call is made from an
        event handler other than a timeout, it is guaranteed that no
        further calls to the timer function will occur once this
        call returns.

        Returns true if the timer was successfully cancelled, false otherwise.
    */
    bool stop_timer(unsigned int id)
    {
      if( stop_timer_event_type_ != -1 )
      {
        return false;
      }

      SDL_Event event;
      event.type = stop_timer_event_type_;
      event.user.code = id;
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
        SDL_ClearError(); // don't be confused by errors elsewhere
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
          else if( e.type == stop_timer_event_type_ )
          {
            timers_.erase(
              std::remove_if(
                timers_.begin(),
                timers_.end(),
                [id = e.user.code](timer& t)
                {
                  return t.id_ == id;
                }),
              timers_.end());
          }
          else
          {
            handle_event(e);
          }
        }
        else
        {
          auto error = SDL_GetError();
          if( error == nullptr || error[0] == '\0' )
          {
            handle_timeout();
          }
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

    /** Helper class for rate limiting events */
    class rate_limiter
    {
    public:
      /** checks whether the event should be rate limited

          @returns true if the event should be discarded due to rate limiting
                   or false otherwise
      */
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
      unsigned int last_key_timestamp_ = 0;
      unsigned int key_repeat_count_ = 0;
    };
    
  private:
    using event_type = unsigned int;
    using handler = std::function<bool(SDL_Event const& e)>;
    using time_point = std::chrono::time_point<std::chrono::steady_clock>;
    struct timer
    {
      timer(
        unsigned int id,
        time_point when,
        std::chrono::milliseconds interval,
        bool one_shot,
        std::function<void()> handler)
        : when_(std::move(when))
        , interval_(std::move(interval))
        , one_shot_(one_shot)
        , handler_(std::move(handler)) {}

      unsigned int id_;
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
          if(!rate_limiter_.rate_limited(e))
          {
            handler_();
          }
          return true;
        }

        return false;
      }

    private:
      SDL_Keycode keycode_;
      Callable handler_;
      rate_limiter rate_limiter_;
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
    event_type stop_timer_event_type_ = -1;
    bool stop_;
    std::atomic<unsigned int> next_timer_id_ = 1;
  };
} // namespace sdl2

#endif // SDL2_CPP_EVENT_H
