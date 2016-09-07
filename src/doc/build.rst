.. _build:

=================
Build and install
=================

Building from source
--------------------

Prerequesites
-------------

Operational guidelines
----------------------

Safely running rscfl
^^^^^^^^^^^^^^^^^^^^

.. danger::
   Do not allow unprivileged users R or W access to the ``/dev/rscfl-data``
   and ``/dev/rscfl-ctrl`` character devices while the rscfl kernel module is
   insmod-ed. A security audit of the kernel side code has not been conducted,
   and you risk allowing applications to use rscfl as an attack vector into the
   kernel (think denial of service and privilege escalation).

.. note::
   For the moment, manage permissions on those files such that only trusted
   users or groups can read or write from them. Any applications running under
   unprivileged users and using the rscfl API should fail gracefully: they will
   run as normal without any self-monitoring.

