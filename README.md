![alt tag](https://github.com/kradhakrishnan/bblocks/blob/master/doc/bblocks.jpg?raw=true)

### About the project

Building blocks project aims to provide building blocks for the development of modern high performance system programming with C++. The project is at the intersection of high performance, extreme scalability, and modern system programming methodologies. The gernal components include proactor based asynchronous handler, networking infrastructure -- tcp, udp, multicasting, storage access abstractions, paxos, buffer management etc. The library is built using well established design patterns and/or based on popular research papers.


### About Me

I spent most of my professional life writing infrastructure for computing ranging from messaging service for
enterprise clusters, to web scale distributed platform for prominent web services, to building cloud (public and private).
In all these projects, I was often tasked with the challenge to build extremely scalable fundamental components over
and over again.

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
