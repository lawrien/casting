# casting

A set of extensions to the Lua language to allow functions to be run in a fully multithreaded
manner. 

### Sessions
A session is equivelent to a Lua state. 

### Tasks

### Channels  

Threading is transparent in *casting*, in that there is no specific need to create or manage
individual threads for running tasks. 

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
