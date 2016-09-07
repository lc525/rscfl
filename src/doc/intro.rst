.. _intro:

============
Introduction
============

Resourceful (rscfl) is a framework that gives applications the ability to
self-instrument for gaining visibility into the resources they consume. Any
low-level resource being used (CPU cycles, cache space, disk, network) becomes
attributable to particular activities done by the application (making a HTTP
request, writing a log to disk, etc). Applications should gather such data in
order to understand the causes of unexpected characteristics of their execution
(suboptimal performance, errors).

When using Resourceful across multiple applications, it becomes possible to look
at the data in the context of overall resource consumption in order to
understand unwanted interference effects between different workloads.

This differs from current monitoring and performance profiling practices: The
level of aggregation for typical monitoring systems is either at the
*system operations level* -- for example, targeting datacenter site
reliability engineers looking at server utilisation, VM scheduling and migration
or network traffic -- or at *application level*, where metrics used by development
teams (i.e. latency, errors per second, transaction timeouts) are collected
periodically. In every case, alerts might be triggered if the measured values
exceed certain thresholds for a given amount of time.
