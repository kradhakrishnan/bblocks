![alt tag](https://github.com/kradhakrishnan/bblocks/blob/master/doc/bblocks.jpg?raw=true)

### About the project

Building blocks project aims to provide building blocks for the development of modern high performance system programming with C++. Idea is to make writing highly concurrent/scalable distributed applications in C++ extremely easy by leveraging and standardizing on well known design approach like event-based programming, async IO, actor based programming like pro-actor, re-actor, remote-actor etc. 

### Motivation for the project

C++ is steadily on the decline as the top choice for distributed systems. Languages like scala and libraries like akka make distributed development quite easy by providing language level support and library level ease. My intention for spending so much of my spare time on this library is to provide a 'akka' like library for c++ distributed system programming leveraging some of my experience building demanding file system and database applications in C++.

### FAQ

###### 1. What are the OS supported ?

Currently, only Ubuntu is supported.

###### 2. What are future OS support we can expect ?

Support for RedHat can be expected soon.

###### 3. Is there plans to support Windows ?

Yes. There is no time frame for it though.

###### 4. How do I compile the code on Ubuntu ?

```
$ git clone https://github.com/kradhakrishnan/bblocks.cc.git
$ cd bblocks
$ make ubuntu-setup
$ make clean

For debug build,
$ make

For opt build,
$ make OPT=enable
```

###### 5. How do I install and use the library ?

```
To install optimized build, (typically advised)
$ make clean
$ make OPT=enable DEBUG=disable install

To install debug build,
$ make clean
$ make install
```

Add -I/usr/lib/bblocks while compiling and -lbblocks -L/usr/lib while linking.

###### 6. How do I run tests ?

```
To run basic unit tests,
$ make run-unit-test

To run valgrind monitored unit tests,
$ make run-valgrind-test

To run all test suite across all build type,
$ make run-all-test
```

###### 7. What are some of the makefile options ?

```
** release flags **
OPT = enable          Enable optimized build
DEBUG = disable       Disable debug information built into the binary
ERRCHK = enable       Enable error checking
TCMALLOC = enable     Use tcmalloc as memory manager

** test related **
LCOV = enable         Provides code coverage information for the test runs
VALGRIND = enable     Enable code provisions for valgrind testing
tsan = enable         Enable thread sanitizer
asan = enable         Enable address sanitizer
```

###### 8. Under what license is the code distributed ?

The code is avaiable for community consumption under LGPL v3. I am pretty open in the license, please mail me or leave a comment if you need other license arrangement for your computing needs.

###### 9. Is bblocks available for other languages ?

I would like to implement wrapper for cython. I would love to port it to Windows, but I don't see the usecase nor do I have the time for it.

[ ![Codeship Status for kradhakrishnan/bblocks.cc](https://www.codeship.io/projects/f6119580-0966-0132-2a1d-16ad0627e247/status)](https://www.codeship.io/projects/31516)
