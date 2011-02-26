#ifndef __LC_CHANNEL_H__
#define __LC_CHANNEL_H__

#include "casting.h"
#include "message.h"

#define CASTING_CHANNEL   "casting.channel"

typedef struct _channel channel_t;
typedef double channel_id;

typedef struct {
  channel_id cid;
} lua_Channel;

typedef enum {
  read, write, closed
} channel_status_t;

#define READABLE  0x01
#define WRITABLE  0x02
#define CLOSING   0x04

channel_t *channel_new(int size);
int channel_close(channel_t *c);

typedef void(*channel_callback)(message_t *m, void *p, channel_status_t event);

int channel_write(channel_t *c, message_t *m, channel_callback cb, void *data);
int channel_read(channel_t *c, channel_callback cb, void *data);

#endif //__LC_CHANNEL_H__
