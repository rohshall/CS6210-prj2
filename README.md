CS 6210 Project 2
================
Simulating the Xen Split Driver Model
------------------------------------
### Sam Britt and Yohanes Suliantoro

To build:

 - Run `make` to build optimized executables in the `/bin` directory.
 - Run `make debug` to build non-optimized executables with debugging
   symbols in the `/bin` directory.
 - Run `make clean` to remove all compiled files and generated
   dependency files.


To run:

 - The server assumes a file called `disk1.img` is in the top-level
   project directory. This file will be read for all client read
   requests. If this file is not present, the server will fail.
 - Once the executables are build and `disk1.img` is present in the
   project root, use the `service` script in the `/bin` directory to
   start and stop the server. `bin/service start` starts the server,
   and `bin/service stop` stops it. The server will stop itself if it
   gets no client reqeusts within a 5 minute interval.
 - To access the file via the client, run the `client` executable in
   the bin directory. It takes two required arguments as follows:
        client [thread_count] [total_request_count]
   where `thread_count` is the number of worker threads that the
   client will use to access the server, and `total_request_count` is
   the total number of requests that will be made, distributed among
   all the threads.
