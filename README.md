# *Hack* Virtual Machine Emulator

This is a virtual machine emulator for the *Hack* Virtual
Machine as specified in [*The Elements of Computing Systems*](https://www.nand2tetris.org/)
which run directly on your machine instead of compiling to
their assembly.

## Try it out

```sh
git clone https://github.com/d4ckard/hvme.git
cd hvme
make run args=examples/fibonacci.vm
```

This computes the 16th element in the Fibonacci sequence (987).

## To Do

- [ ] Add error context to execution.

- [ ] Remove previous return values from REPL
      so that the instructions become more readable.

- [ ] Improve identifier messaging.

- [ ] Logically group different `Insts` and `SymbolTable` instances.

- [ ] Combine multiple files and manage their static data.

- [x] Add function, call and return (finishes up language spec)
