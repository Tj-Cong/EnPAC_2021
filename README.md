# EnPAC_2021
EnPAC (Enhanced Petri-net Analyser and Checker) is an explict LTL model checking tool of Petri nets.

# 1. How to compile it?  
## 1.1 The following tools are necessary to compile EnPAC from scratch:  
(they can be get by "sudo apt-get install \<package\>")  
- Autoconf
- Automake
- libtools

## 1.2 tcmalloc is required to be installed before compiling EnPAC  
you can run install.sh to install it automatically:  
```
$ cd tcmalloc  
$ chmod +x install.sh  
$ sudo ./install  (MAKE SURE you give it root privilege)  
```
if it is installed successfully, the following files will appear in the /usr/local/lib (you can check this by commands "cd /usr/local/lib/ && (ls | grep tcmalloc)")
- libtcmalloc.a
- libtcmalloc_and_profiler.a
- libtcmalloc_and_profiler.la
- libtcmalloc_and_profiler.so
- libtcmalloc_and_profiler.so.4
- libtcmalloc_and_profiler.so.4.5.3
- libtcmalloc_debug.a
- libtcmalloc_debug.la
- libtcmalloc_debug.so
- libtcmalloc_debug.so.4
- libtcmalloc_debug.so.4.5.3
- libtcmalloc.la
- libtcmalloc_minimal.a
- libtcmalloc_minimal_debug.a
- libtcmalloc_minimal_debug.la
- libtcmalloc_minimal_debug.so
- libtcmalloc_minimal_debug.so.4
- libtcmalloc_minimal_debug.so.4.5.3
- libtcmalloc_minimal.la
- libtcmalloc_minimal.so
- libtcmalloc_minimal.so.4
- libtcmalloc_minimal.so.4.5.3
- libtcmalloc.so
- libtcmalloc.so.4
- libtcmalloc.so.4.5.3

## 1.3 compile source code

You need to configure EnPAC by executing  
```
$ cd EnPAC  
$ ./configure  
```
Then, execute  
```
$ make  
$ sudo make install  (MAKE SURE you give it root privilege)  
```

# 2. How to use it?  
The execution of EnPAC relies on two input files: LTLCardinality.xml (or LTLFireability.xml) and model.pnml. These two files are definitely included in the instance provided by MCC;  
    usage of EnPAC:  
````
    $ EnPAC <LTL category> [-p]  
        <LTL category>: LTL categories are two enumerated values, LTLCardinality or LTLFireability  
        [-p]: if you specified EnPAC by -p, then the Petri net model and each Buchi automata will be printed out, their names are:  
            Petri net model: PetriNetGraph.png  
            Buchi automata: BA_<propertyid>.png  
    example:  
          $ EnPAC LTLCardinality -p  
    or $ EnPAC LTLFireability  
````  
notice*: don't use the '-f' function if it is not necessary, cause it will degrade EnPAC performance  
