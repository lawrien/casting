#ifndef __LC_ERRORS_H__
#define __LC_ERRORS_H__

extern int *__lc_err_loc( );

#define lc_err (*__lc_err_loc())

#define ERROR(r,e) do { \
  lc_err = (e); return (r); \
  } while(0)

enum {
  SUCCESS = 0,
  FAIL = -1,
  ERR_INVAL = -2,
  ERR_NOMEM = -3,
  ERR_LUASTACK = -4,
  ERR_UNSUPPORTED = -5,
  ERR_COPY = -6,
  ERR_FULL = -7,
  ERR_EMPTY = -8,
  ERR_NOTFOUND = -9,
  ERR_THREADFAIL = -10,
  ERR_CLOSED = -11,
  ERR_BADSTATE = -12,
  ERR_BADSESSION = -13,
  ERR_BUSY = -14,
  ERR_DEADLOCK = -15,
  ERR_AGAIN = -16,
  ERR_OVERFLOW = -17,
  ERR_TIMEDOUT = -18,
  ERR_WAIT = -19,
  ERR_TASKSTATE = -20,
  ERR_SYSUNKNOWN = -9999
};

const char *errmsg(int err);

#endif // __LC_ERRORS_H__
