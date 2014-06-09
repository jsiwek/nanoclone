nanoclone
=========

Toy model of a shared key-value store implemented using nanomsg, derived
from [1].  Keys are meant to be arbitrary bytestrings, but the current
iteration of the code may assume printable C-strings is some places for
debugging.  Values are currently limited to 64-bit integers.

There's examples of how to use it in server.cpp and client.cpp, but
overall, this code is not thoroughly tested.

[1] http://zguide.zeromq.org/page:all#Reliable-Pub-Sub-Clone-Pattern
