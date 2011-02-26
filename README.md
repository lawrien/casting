# casting

A set of extensions to the Lua language to allow functions to be run in a fully multithreaded,
non-blocking manner. The aim of the project is to allow for the creation of highly-concurrent,
reliable and scalable solutions using Lua.

To achieve this, *casting* provides *sessions*, *tasks* and *channels* as the building-blocks
for writing such systems, without the need to directly understand the complexitites of OS threading
and locking. 

### Tasks
A task is analogous to a Lua coroutine, a function that can be called. The difference is that 
a task runs within a *session* (see below), rather than having a task run in a separate OS thread.
Tasks have a small memory footprint, so it is acceptable to have many (no numbers yet) of them
running at once.

### Sessions
A session is equivelent to a Lua state that is running in a separate OS thread (not be be confused
with a Lua thread). Session exist to allow the running of tasks, and tasks are created in 
individual sessions. Each session may contain many tasks, but will only be running a single 
tasks at any moment in time (again, this is analogous to how coroutines work). Like tasks, many 
sessions may be created, but they are more *heavyweight* than tasks (so while a session-per-task
model is acceptable, especially for *daemon* tasks, it is not generally a reccommended mechanism).

Sessions themselves run on separate OS threads, but not on a thread-per-session basis. Rather, *casting*
provides the ability to allow many different sessions (and hence tasks) to share a limitied
number of OS threads. This allows for a high degree of concurrency, without the overheads that  
a session-per-thread or task-per-thread may introduce.

OS level threading is transparent, and is handled by the sessions directly. However, *casting* does
allow for the setting of minimum/maximum OS threads to support the sessions. It will tend to keep the
OS threads at a minimum, but has the capacity to create additional threads when necessary to
do so.  

### Channels  
Channels are provides to allow data to be moved around between tasks in *casting*. They operate on
a 'share-nothing' model (mostly...more on that at a later data). Channels also provide non-buffered
and buffered varieties. 

***
## Status

*casting* is currently *alpha* quality, while I concerntrate on specific elements that I
personnally require. It is, however, changing rapidly, and I expect to make an announcement on
the Lua mailing list in the very near future. Until the announement has been made, I would
warn against forking and modifying unless to are prepare to accept *major* changes.

***
## Platform Support

The aim of *casting* is to be able to provide support for most, if not all, modern operating systems.
There are two main requirements: a) the OS provides threading support; b) the OS supports Lua.

Currently, *casing* is being worked on in a 64-bit Linux environment. It does compile, 
but has not been tested, on Mac OS X (snow leopard). **Windows does not currently have support**, 
although this is definitely in the plan - all of the required threading primatives have been 
put into wrapper functions/types, so a Windows port *should* simply be a matter of providing 
suitable wrappers.

## Lua version support

Currently, *casting* is developed against **Lua 5.1.4**. Support for *Lua 5.2* is planned, but
will depend on the release schedule from the Lua team. 

Unfortunately, there is **NO** support for *LuaJIT 2.0* at this time. This is due to the need for
working implementations of the *lua_dump()* and *lua_loadbuffer()* APIs, which are not currently
available in LuaJIT. Should the situation change, then you can expect support to be provided. 

## Acknowlegements

Firstly, the personal acknowlegments. To my wife, Jo, I thank you for your love, support and 
(especially) patience. To my Mum and Dad (yes, I'm not too old), again, a big thank you for
your years of looking after your favourite son !

From a technical perspective, my gratitude and admiration goes out to (in no particular order):
Lua authors and the Lua community; Linux and Ubuntu communities; GNU foundation and all those 
involved in the countless free software packages that I know, use and (sometimes) love. I'm still 
in awe of the quality and commitment that open-source brings to my computer life ! 
