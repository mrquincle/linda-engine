# Linda Engine

The linda engine is an evo-devo evolutionary engine in C, no C++ here. It makes it possible to cross-compile for platforms that do not have a C++ compiler, although those might be rare. The engine is composed out of different components. The "linda core" folder contains functionality shared by all the components and might be of general interest to you. It contains:

* [abbey.c](https://github.com/mrquincle/linda-engine/blob/master/linda_core/src/abbey.c) a C implementation of a threadpool with "monks" performing tasks from a buffer. The concept behind the "abbey" is the creation - in the end - of dedicated threads/monks that have different properties with respect to how they execute a general piece of code. Certain monks might use different system resources, etc. These ideas however have never been materialized, so it can just be seen as a barebones threadpool.
* [poseta.c](https://github.com/mrquincle/linda-engine/blob/master/linda_core/src/poseta.c) contains additional functionality to describe execution dependencies between tasks. With the abbey you will put tasks on a queue and you will not have control on which task will be executed next. The only method is to have tasks themselves adding tasks to the queue. Hence, they will need to have knowledge on which task comes next. Exogenous coordination of the sequence of tasks executed is made possible by the special tasks in "poseta". The "po" stands for partial order: tasks can now come in a specific defined order of execution.
* [ptreaty.c](https://github.com/mrquincle/linda-engine/blob/master/linda_core/src/ptreaty.c) adds wrapper functionality around pthreads. It give "names" to threads, very convenient for debugging! And it introduces the so-called "baton". This is an advanced synchronization device across threads. It is used by the "poseta" code to make sure the monks/threads yield execution to another thread to ensure a certain order for example (see ptreaty\_should\_be\_first and ptreaty\_should\_be\_later).
* [log.c](https://github.com/mrquincle/linda-engine/blob/master/linda_core/src/log.c) contains some convenient color-aware logging functions in a threading environment.
* [tcpip.c](https://github.com/mrquincle/linda-engine/blob/master/linda_core/src/tcpip.c) sets up a TCP/IP socket, defines a specific message type, and implements a mailbox to which you can push and from which you can pop those messages.
* [tcpipbank.c](https://github.com/mrquincle/linda-engine/blob/master/linda_core/src/tcpipbank.c) is a bunch of sockets.

That's it regarding general functionality. The specific application here contains "elinda" which is the evolutionary engine, "colinda" which is the code that runs on a robot and hence you will need many of these to communicate with one "elinda" entity. The evolutionary engine creates new data structures for the "colinda" ones, leading to new controllers by mutation, etc. The fitness of each controller is defined in yet another entity, the "flinda" one. In the end, there is "tlinda" which is just a testing facility.

## Background

The Colinda code should run on an embedded microcontroller. This means that caution is needed with printf statements and other functionalities that do not exist on the target platform. However, do not think this platform can be too barebone, pthreads is required for example.

Another point is that some tests - and only the tests - run with the player/stage simulator and link to the libplayerc++ library. If you do not intend to run that test, either remove the -lplayerc++ flag from the linker in Project Properties. Or install the player on your machine. On Ubuntu this is "sudo aptitude install libplayerc++2" (the dev files are not needed) and it's a pity but after that you probably need to create one symbolic link: "sudo ln -s /usr/lib/libplayerc++.so.2.0.4 /usr/lib/libplayerc++.so"
  
Enjoy the engine!! :-) 

## Copyrights
The copyrights (2008) belong to:

- Author: Anne van Rossum
- Almende B.V., http://www.almende.com
- Rotterdam, The Netherlands


