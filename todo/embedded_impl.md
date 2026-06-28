## New implementation for embedded applications - TRUN only

The embedded API is thin. But behind the scenes most of the core concepts are reused. These core concepts are not designed
for embedded use - even if they work. 
A new 'engine' is required. Using zero allocation principles, static buffers etc (compile time changed).
