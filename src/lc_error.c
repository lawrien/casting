#include <errno.h>

#include "lc_error.h"
#include "lc_thread.h"

static void lc_err_destroy(void *p) {
  lc_free(p);
}

static lc_local_t *err_key;

extern int *__lc_err_loc( ) {
  static int init = 0;

  while (!atomic_int_cas(&init, 1, 1)) {
    err_key = lc_local_new(lc_err_destroy);
    int *err = lc_alloc(sizeof(int));
    lc_local_set(err_key, err);

    init = 1;
  }

  return (int *) lc_local_get(err_key);
}

const char *errmsg(int err) {
  switch (err) {
    case ERR_INVAL:
      return "Invalid value";
    case ERR_LUASTACK:
      return "cannot grow Lua stack";
    case ERR_UNSUPPORTED:
      return "Unsupported value";
    case ERR_NOMEM:
      return "Unable to allocate memory";
    case ERR_COPY:
      return "Error copying value";
    case ERR_FULL:
      return "Data structure is full";
    case ERR_NOTFOUND:
      return "Data not found";
    case ERR_THREADFAIL:
      return "Could not create thread";
    case ERR_CLOSED:
      return "Attempt to used resource that is closed";
    case ERR_BADSTATE:
      return "Lua state could not be created or is corrupt";
    case ERR_BADSESSION:
      return "session id does not refer to a valid session";
    case ERR_BUSY:
      return "resource is busy";
    case ERR_DEADLOCK:
      return "already locked";
    case ERR_SYSUNKNOWN:
      return "unknown system error";
    default:
      return "unknown error";
  }
}

