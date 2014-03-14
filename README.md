![alt tag](https://github.com/kradhakrishnan/bblocks/blob/master/doc/bblocks.jpg?raw=true)

### About the project

Building blocks project aims to provide building blocks for the development of modern high performance system programming with C++. The project is at the intersection of high performance, extreme scalability, and modern system programming methodologies. The gernal components include proactor based asynchronous handler, networking infrastructure -- tcp, udp, multicasting, storage access abstractions, paxos, buffer management etc. The library is built using well established design patterns and/or based on popular research papers.


### Motivation for the project

I spent most of my professional life writing infrastructure for computing ranging from messaging service for
enterprise clusters, to web scale distributed platform for prominent web services, to building cloud (public and private).
In all these projects, I was often tasked with the challenge to build extremely scalable fundamental components over
and over again. Almost, on every occasion we had to reuse an existing framework, write immense amount of logic and find
the code not to scale to very high levels (please note I am not talking about shallow scalability) and work our way
backwards to fixing the problem. The problem is more pronounced when you build clusters.

I would like to experiment with the idea of developing a general purpose library for high performance systems based on
modern system programming with c++, and provide genrealized solutions for clusters like leader election, consensus
protocol like paxos etc.

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

###### 5. Under what license is the code distributed ?

The code is avaiable for community consumption under LGPL v3. I am pretty open in the license, please mail me or leave a comment if you need other license arrangement for your computing needs.

###### 6. Is there an official build ? Is it available for use via apt-get or yum ?

Soon. I will have a public alpha build once I get the unit testing framework done, and more parts of the framework completed for meaningful adoption.

###### 7. Is bblocks available for other languages ?

Work is in progress for python and java. I will open up the repositories when they are ready.
