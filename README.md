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
~> make ubuntu-setup
[ Please resolve any package version errors ]
~> make clean
~> make OPT=enable
```
###### 5. How do I run tests ?

To run basic unit tests,
```
~> make run-unit-test
```
To run valgrind monitored unit tests,
```
~> make run-valgrind-test
```

###### 6. Under what license is the code distributed ?

The code is avaiable for community consumption under LGPL v3. I am pretty open in the license, please mail me or leave a comment if you need other license arrangement for your computing needs.

###### 7. Is bblocks available for other languages ?

I would like to implement wrapper for cython.
