.. Resourceful documentation master file, created by
   sphinx-quickstart on Wed Aug 17 01:13:31 2016.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Resourceful Documentation
=========================
Targeting version rscfl-|release|

Resourceful (abbreviated rscfl) is a tool built with the purpose of better understanding the
behaviour of applications in terms of their resource consumption. The
measurements it performs can be used in determining the causes of performance
variation in completing application-defined activities (servicing a request,
executing a database query, backing up a log of operations etc).

Unlike other tracing or debugging tools such as ftrace, strace, perf or
SystemTap, it takes an on-line approach (as opposed to collecting traces
for later batch processing). It was built from the start with programmability in
mind. No parsing of trace files here!

Concretely, this means applications can, at runtime:

* Configure what tracing takes place and what measurements they are interested
  in (about themselves or about other applications as long as permission was
  granted);
* Define kernel-level measurement aggregation strategies that take into account
  what the application is doing (i.e. aggregate all resources consumed while the
  application services an HTTP request);
* Read the results (measurement data) from their own address space, as normal
  data structures;
* Perform processing of this data or modify their behaviour in accordance to the
  results;

Resourceful has two major components:

1. **A kernel module** that when inserted into the kernel is responsible for doing
   low-overhead probing and taking measurements as configured by applications;
2. **A userspace library** (API) that applications can use to interact with the
   kernel module (start/stop tracing, reading measurement data, etc)

Reading this document
~~~~~~~~~~~~~~~~~~~~~

The suggestion for reading this documentation is first going through the
:ref:`intro` for understanding what Resourceful is good at,
then reading the :ref:`user-api` section for understanding the way you could use
it in your own projects (examples available).

If you are in a hurry, just folow the :ref:`build instructions <build>` and
look at :ref:`some code using the API <simple_api>`.

Contents
=========

.. toctree::
   :maxdepth: 2

   intro
   build
   user-api
   architecture
   advanced
   api-doc
   project

Indices and tables
==================

* :ref:`genindex`
* :ref:`search`

