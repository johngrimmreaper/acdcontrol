=========
Changelog
=========

This file records the known public release history of ``acdcontrol``.

It intentionally combines the original SourceForge release timeline with the
current GitHub-maintained release line so the project history remains visible in
one place.

0.5 (in development)
====================

Current development series under John Grimm, also known as Reaper.

This series began after the ``0.4.1`` maintenance tag and after the project
version was bumped to ``0.5``.

Highlights so far include:

* major cleanup and refactoring of the C++ implementation
* safer HID I/O handling and cleanup on fatal and early-exit paths
* stricter CLI parsing and clearer error handling
* native auto-brightness on/off support for the Apple LED Cinema Display 27"
  (USB ID ``05ac:9226``)
* introduction of the new ``acdprobe`` companion utility
* standardized probe collection bundles and compact ``summary.json`` output
* enriched control metadata, confidence labels, telemetry candidates, and
  privacy-oriented probe improvements
* curated manual pages for ``acdcontrol`` and ``acdprobe``
* documentation split into a lean ``README.rst``, manual pages, and focused
  technical notes under ``docs/``
* man-page preview and install targets in the build system

Notable user-facing news in the current unreleased line:

* ``acdprobe`` is the major new feature of the ``0.5`` development series
* native auto-brightness control is now available for the 27-inch USB Apple LED
  Cinema Display
* project documentation has been reorganized into a more traditional Unix-style
  layout

0.4.1
=====

Small code fixes before starting 0.5 developlment
Fix world-writable permissions.
Makefile improvements and corrections.

This tag marks the last pre-``0.5`` stabilization point in the current
maintainer era, before the larger refactor work and before ``acdprobe`` became
part of the project.


0.4
===

The current repository preserves the historical upstream ``0.4`` point with the
Git tag ``0.4``, which points to commit ``33fce7b``.

Historical note:

* the imported Git baseline commit is titled ``version 0.4 2007.05.04``
* the old SourceForge news page announces version ``0.4`` on ``05.04.2007``

This discrepancy is preserved here intentionally rather than silently flattened.

0.3
===

Released on ``2007-03-15`` according to the old SourceForge news page.

Historically noted changes:

* support for the 24-inch Apple Cinema Display
* copying and copyright information added
* thanks recorded for Charles Lepple

0.12
====

Released on ``2005-01-23`` according to the old SourceForge news page.

Historically noted changes:

* multiple-monitor support
* detection support
* GUI writing started

0.1
===

Initial public release on ``2004-12-24`` according to the old SourceForge news
page.

Historical notes
================

The project originated on SourceForge as ``Apple Cinema Display backlight
control`` under Pavel Gurevich.

The SourceForge project registration date is ``2004-12-17``.

The GitHub-visible history later continued through the public fork lineage:

* ``taylor/acdcontrol``
* ``warvariuc/acdcontrol``
* ``johngrimmreaper/acdcontrol``

See also
========

* ``CREDITS.rst`` for authorship, stewardship, and contributor history
* ``README.rst`` for current project overview and quick-start information
