==============================
Parallel Text Processing Tools
==============================

A collection of small, efficient tools for parallel processing of text 
files at the Unix command line.

pcat
====

	$ pcat file1 file2... fileN

pcat (parallel cat) reads files in parallel and combines them on 
standard out without mashing together lines from different files. If 
you've ever tried to combine the output from multiple processes and just 
got mashed gobbledygook, you'll appreciate pcat. Every line it outputs 
is an entire line from exactly one input.  This is most useful when the 
input files are actually the output of parallel processes: pcat will 
start reading them all simultaneously, without blocking, and combine 
them, making sure they don't clobber each other's output.  The order of 
the output is arbitrary, except that lines from the same source retain 
their order.  pcat tries not to be the bottleneck: it can easily process 
several GB/sec.

hsplit
======

    $ hsplit file1 file2... fileN < input.txt

hsplit (hash split) hashes each line of standard input to one of the 
"bucket" files given on the command line.  It distributes lines evenly 
to all files, ensuring that identical lines always go to the same file.  
This can be used with process substitution to farm out tasks to several 
processes, when the same tasks should go to the same process.  It can 
also just print integer hash values for each line of standard in, so you 
can do what you want with them.  hsplit uses MurmurHash3 (the 32-bit 
variant), and can process several hundred MB/sec.
  

Building
========

The tools are written in ISO C99 and can be built with the included 
Makefile.  Functional tests can be run with ``make test``; the tools 
have only been tested on Linux and feedback on other platforms is 
welcome.
