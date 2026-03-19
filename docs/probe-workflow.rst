======================
acdprobe Probe Workflow
======================

Purpose
=======

``acdprobe`` is the companion inspection utility for ``acdcontrol``. Its job is
to collect stable, reviewable HID information from candidate Apple display
devices and save that data in a form that can be compared, shared, and later
used to build a per-model capability database.

This document is about workflow and review. It is not the command reference for
``acdprobe``. For command-line syntax, options, and exact mode behavior, use
the manual page:

* ``man/acdprobe.1``

For the compact summary file format used by collect mode, use:

* ``docs/summary-schema.rst``


What ``acdprobe`` is for
========================

Use ``acdprobe`` when you want to answer questions such as:

* Which HID reports and usages does this monitor expose?
* Does this candidate monitor look similar to a known Apple display?
* Does the monitor expose a plausible brightness or ambient-light-sensor
  control?
* Can a readonly collection be reviewed and compared without reading the full
  raw report tree?
* Can a controlled feature write confirm a suspected control without doing
  brute-force experimentation?

The tool is meant for deliberate investigation. It is not a fuzzing framework
and it is not intended to blindly brute-force unknown controls.


Recommended workflow
====================

Start with readonly collection
------------------------------

For most contributors, the right first step is a readonly collection run:

::

    ./acdprobe --profile basic-readonly /dev/acdctl4

This creates a stable bundle that can be reviewed locally before sharing.

If you want a deeper non-writing collection, use:

::

    ./acdprobe --profile full-readonly /dev/acdctl4

These readonly workflows are the preferred path for new monitor models and for
initial submissions.

Use experimental write mode only on purpose
-------------------------------------------

If you explicitly want to help investigate a suspected control and you are
prepared to interpret the result carefully, use the experimental profile:

::

    ./acdprobe --profile als-discovery /dev/acdctl4 --set-feature 102 0 0 2 \
      --readback-delay-ms 500

This workflow is for targeted experiments only. It is not a general-purpose
discovery mode.

Review ``summary.json`` first
-----------------------------

When reviewing a collected bundle, start with ``summary.json``.

That file exists to provide a compact, stable view of the most useful data:

* device identity
* application summary
* report counts
* report-descriptor fingerprint
* candidate controls with current values and ranges
* confidence labels
* unresolved vendor-private telemetry candidates

If the summary already answers the question you care about, there is usually no
need to open the larger raw reports immediately.

Escalate only when needed
-------------------------

If the summary is not enough, inspect the richer artifacts in this order:

#. ``report.txt`` for human-readable context
#. ``report.json`` for structured full detail
#. ``report.csv`` for flattened usage scanning
#. ``report_descriptor.hex`` when raw descriptor structure matters


Collection modes
================

Raw probe mode
--------------

A plain run such as:

::

    ./acdprobe /dev/acdctl4

writes the default raw artifact set.

This is useful for local investigation, but it is less standardized than the
collect bundle layout.

Collect mode
------------

Use collect mode when you want a stable submission or review bundle:

::

    ./acdprobe --collect /dev/acdctl4

Collect mode writes the standard bundle, including ``summary.json``. That makes
later comparison between monitors and controller revisions much easier.

Profiles
========

``basic-readonly``
------------------

Use this for the normal safe contribution workflow.

Good fit for:

* first collection on an unfamiliar monitor
* pull-request submissions
* review bundles for known-safe comparison

``full-readonly``
-----------------

Use this when you want a deeper non-writing collection.

Good fit for:

* fuller report-tree inspection
* comparison between similar monitor models
* deeper review without experimental writes

``als-discovery``
-----------------

Use this only when you intentionally want to help with controlled
ambient-light-sensor or auto-brightness style experiments.

Good fit for:

* testing a specific suspected control
* write/readback confirmation
* controlled follow-up after a readonly collection already exists


Controlled feature writes
=========================

``acdprobe`` can perform a controlled write to a single HID feature usage and
read it back immediately.

Example:

::

    ./acdprobe /dev/acdctl4 --set-feature 102 0 0 2 --readback-delay-ms 500

This means:

* report ID = ``102``
* field index = ``0``
* usage index = ``0``
* requested value = ``2``

The tool reads the current value, validates the requested value against the HID
logical range, writes the requested value, commits the feature report, and then
reads it back again. With ``--readback-delay-ms``, it can also perform a second
delayed readback.

This mode is for targeted, hypothesis-driven testing only.


What to submit
==============

For community submissions, prefer the readonly collection workflow and submit
the generated files under the appropriate ``probes/VID_PID/`` directory by pull
request.

In most cases, the most useful files are:

* ``summary.json``
* ``report.txt``
* ``report.json``

The descriptor hex and CSV output are also valuable, especially when a device
looks similar to another known monitor but exposes different report structure.


How reviewers should read a bundle
==================================

A reviewer should usually follow this order:

#. Check ``summary.json`` first.
#. Look at control candidates and their confidence labels.
#. Compare report counts and descriptor fingerprint with known models.
#. Open ``report.txt`` when more context is needed.
#. Use ``report.json`` when precise structured detail matters.
#. Treat vendor-private controls as unresolved unless strong evidence exists.

This keeps review efficient and reduces the chance of overinterpreting raw HID
data too early.


Privacy and caution
===================

Even though collect mode is designed to produce a cleaner and more reviewable
bundle, contributors should still inspect generated artifacts before sharing
them.

Pay special attention to:

* USB identity fields
* sysfs-derived metadata
* descriptor dumps
* human-readable text output

For the compact summary layout and its privacy-filtered fields, see
``docs/summary-schema.rst``.


Long-term goal
==============

The long-term goal of ``acdprobe`` collection is to build a per-model raw
feature database and then derive a curated catalog of stable capabilities from
those submissions.

That means the probe bundle is not the final product. It is the evidence layer
that later documentation and support decisions can be built on.
