# *Hack* Virtual Machine Emulator

This is a virtual machine emulator for the *Hack* Virtual
Machine as specified in [*The Elements of Computing Systems*](https://www.nand2tetris.org/)
which runs directly on your machine instead of compiling to
*Hack* assembly.

## Try it out

```sh
git clone https://github.com/d4ckard/hvme.git
cd hvme
make run args=examples/fibonacci.vm
```

This computes the 16th element in the Fibonacci sequence (987).


## To Do

- [ ] Add features to the System API (In Progress)

- [x] Add function, call and return (completed language spec)

- [x] Combine multiple files and manage their static data.

##  System API

The *Hack* VM specification assumes that VM code is compiled down to
binary code which runs on the *physical* (the computer they're
talking about doesn't really exist) machine. There, I/O works by
writing and reading from the screen and keyboard memory maps (which
are not yet implemented TBH).

The downside of this approach is that it's very annoying
to be limited to such primitive forms of I/O while actually running
on a *real* computer all the time. This is why I decided to add
a basic API to just use standard I/O.

It's defined as a set of *HVM* functions which are available by
default to any program. While those functions are not actually
implemented using *HVM* they look and feel just like native
functions and thereby don't extend the core language.

**Right now, you can use the following I/O functions.**
Checkout the `examples/` to get an idea of how to use them.

  - `Sys.print_char(ch) -> 0`: prints the given character.

  - `Sys.print_num(num) -> 0`: prints the given number.

  - `Sys.print_str(nchars, addr) -> 0`: prints a given number
    of characters stored on the heap starting at a given address.

  - `Sys.read_char() -> ch`: reads a single character (`getchar`).

  - `Sys.read_num() -> ch`: reads a single number (`scanf`).

  - `Sys.read_str(addr) -> nchars`: reads a string (`getline`)
    and stores it on heap at `addr`. The number of stored characters
    is returned.

Note that none of the above functions accept different
types. There are no types in *HVM* after all! They merely
interpret the values differently.

Hence, all the names in the describes interfaces are
totally symbolic. All functions must return a single value
and take any number of arguments. The interfaces read like
this:

```
 Guess     Arguments: `arg_1` is    Symbolic name for the return value
  what.     lowest on stack.       (usually 0 if it has no meaning).
   __\________   _/_    _\_       __/__
  /           \ /   \  /   \     /     \
  function_name(arg_1, arg_2) -> ret_val
```
