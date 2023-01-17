#include <stdio.h>
#include <reactor.h>

struct state
{
  timer t;
  reactor_time target;
};

static void callback(reactor_event *event)
{
  struct state *state = event->state;

  (void) fprintf(stdout, "timeout %lu, delta %ld\n", reactor_now(), reactor_now() - state->target);
  state->target += 1000000000ULL;
}

int main()
{
  struct state state;

  reactor_construct();
  state.target = 1000000000ULL * ((reactor_now() / 1000000000ULL) + 1);
  timer_construct(&state.t, callback, &state);
  timer_set(&state.t, state.target, 1000000000ULL);
  reactor_loop();
  reactor_destruct();
}
