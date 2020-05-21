# FlowTracker

FlowTracker is a tool to detect variable-timing behavior in C code using the LLVM framework.

Build instructions for an older version can be found [here](http://cuda.dcc.ufmg.br/flowtrackervsferrante/install.html).

## LLVM

For this version, follow the steps below to build LLVM:
* Extract `llvm-3.7.1.src.tar.xz` in your HOME directory. Now, extract `cfe-3.7.1.src.tar.xz` and rename `cfe-3.7.0.src` as clang like `/HOME/llvm-3.7.1-src/tool/clang`
* Create a folder named `build` in `/HOME/llvm-3.7.1.src` and inside this build folder, you must configure the sources like below.
* Type `../configure --disable-bindings` at /HOME/llvm-3.7.1.src/build. After the configuration script finished its execution, you must run `make`
* You might have to patch line 102 of `include/llvm/IR/ValueMap.h` with:
```
  bool hasMD() const { return bool(MDMap); }
```

* After compilation, include the path to the LLVM binaries in your shell: `export PATH=$PATH:/HOME/llv3.7.1.src/build/Release+Asserts/bin`

## FlowTracker

Clone the repository to `/HOME/llvm37/lib/Transforms/` and also to `/HOME/llvm-3.7.1.src/build/lib/Transforms/`
Now, you must compile each of the passes that constitute Flow Tracker:

```
cd /HOME/llvm-3.7.1.src/build/lib/Transforms/AliasSets
make
cd /HOME/llvm-3.7.1.src/build/lib/Transforms/DepGraph
make
cd /HOME/llvm-3.7.1.src/build/lib/Transforms/bSSA2
make
```

You have also to compile the XML parser using the command below inside `/HOME/llvm-3.7.1.src/lib/Transforms/bSSA2`:

```
g++ -shared -o parserXML.so -fPIC parserXML.cpp tinyxml2.cpp
```

## Example

To demonstrate how to use Flow Tracker, we will find a vulnerability in `donnabad-example`, an insecure implementation of Curve2519 modified to branch on secret bits.
We want to know if the program runs always in the same time, independent on the cryptographic key or on the input that it uses. To do that, type the following command:

```
clang -emit-llvm -c -g curve25519-donnabad.c -o curve25519-donnabad.bc
opt -instnamer -mem2reg curve25519-donnabad.bc > curve25519-donnabad.rbc
opt -basicaa -load AliasSets.so -load DepGraph.so -load bSSA2.so -bssa2 -xmlfile in.xml curve25519-donnabad.rbc
```

You should obtain the same results as in `results.txt`. The files starting with `subgraph` trace the origin of the timing leaks.
Notice the syntax of `in.xml` with annotations of what parameters are public or private.
