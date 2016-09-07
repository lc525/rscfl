.. _user-api:

================
Developers guide
================

Using rscfl
===========

The rscfl API
=============

API versions
~~~~~~~~~~~~

High-level API
~~~~~~~~~~~~~~

Complete example
^^^^^^^^^^^^^^^^

The following example touches most of the functionality usually needed for
working with the rscfl API.

.. literalinclude:: ../examples/simple_api.c
   :language: c
   :linenos:
   :caption: simple_api.c (simple api example)
   :name: simple_api

Advanced API functionality
~~~~~~~~~~~~~~~~~~~~~~~~~~

Low-level API
~~~~~~~~~~~~~

.. warning::
   You need a very solid understanding of how rscfl works in order to safely
   use the low-level API. If using it without in-depth knowledge, it is very
   easy to leak memory or make rscfl unusable. You have been warned.
