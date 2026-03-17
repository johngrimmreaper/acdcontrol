======================================
Apple Cinema Display Brightness Control
======================================

``acdcontrol`` is a small utility for querying and changing the brightness of
supported Apple Cinema and Studio Displays through the HID interface exposed by
Linux.

Maintainer
----------

Project currently maintained by John Grimm, also known as Reaper.

Repository::

    https://github.com/johngrimmreaper/acdcontrol

Compiling
---------

Clone this repository::

    git clone https://github.com/johngrimmreaper/acdcontrol.git

Change to the program directory and build it::

    cd acdcontrol
    make

New files named ``acdcontrol`` and ``acdprobe`` should appear in the same
repository directory. If the build fails, make sure the required development
tools are installed, for example ``build-essential`` on Debian/Ubuntu systems.

Installation
------------

Install the main program into ``/usr/local/bin`` and install the udev rules::

    sudo make install

Reload the udev configuration and trigger the updated rules::

    sudo udevadm control --reload
    sudo udevadm trigger

Remove the installed files with::

    sudo make uninstall

For staged installs and packaging, you can override the install paths.
For example::

    make install DESTDIR=/tmp/acd-stage

You can also use the helper script::

    ./acd-compile.sh

This uses the included ``docker-compose.yml`` file and requires Docker and
Docker Compose.

Usage
-----

::

    ./acdcontrol [--silent|-s] [--brief|-b] [--help|-h] [--about|-a] \
      [--force|-f] [--detect|-d] [--list-all|-l] \
      [--auto-brightness on|off|status] \
      /dev/acdctlX [brightness]

It should be safe to run the program against other HID devices while detecting
supported displays, because detection mode does not write brightness values.

Parameters
----------

``-s``, ``--silent``
    Suppress non-functional program output.

``-b``, ``--brief``
    Print only the brightness value in query mode.

``-h``, ``--help``
    Show help and exit.

``-a``, ``--about``
    Show program information, credits, and thanks.

``-f``, ``--force``
    Continue even if the detected device is unsupported.

``-d``, ``--detect``
    Run detection mode. In this mode no brightness writes are performed, which
    makes it safer to probe candidate HID devices.

``-l``, ``--list-all``
    List all officially supported monitors and exit.

``--auto-brightness on|off|status``
    Query or control the native ambient-light-sensor / auto-brightness feature
    on supported displays.

A display not appearing in this list does not necessarily mean it will not
work; it may simply mean it was not tested yet.

``/dev/acdctlX``
    The HID device node that represents your Apple Cinema or Studio Display.

``brightness``
    If omitted, the current brightness is queried. If provided, the brightness
    is changed.

Absolute form::

    160

Relative increment::

    +10

Relative decrement (note the required ``--`` before the negative value)::

    -- -10

Brightness behavior and useful values may vary depending on the display model.

Examples
--------

Show help::

    ./acdcontrol --help

Detect which HID device belongs to the display::

    ./acdcontrol --detect /dev/acdctl*

Read the current brightness::

    ./acdcontrol /dev/acdctl0

Set an absolute brightness value::

    ./acdcontrol /dev/acdctl0 650

Increase brightness by 10::

    ./acdcontrol /dev/acdctl0 +10

Decrease brightness by 10::

    ./acdcontrol /dev/acdctl0 -- -10

Read auto-brightness status::

    ./acdcontrol /dev/acdctl0 --auto-brightness status

Enable auto-brightness::

    ./acdcontrol /dev/acdctl0 --auto-brightness on

Disable auto-brightness::

    ./acdcontrol /dev/acdctl0 --auto-brightness off

Sample Profiles
---------------

``00profile-low.sh``
    Set brightness to a low level.

``01profile-middle.sh``
    Set brightness to a medium level.

``02profile-high.sh``
    Set brightness to a high level.

Useful Makefile Targets
-----------------------

Build both binaries::

    make

Remove built binaries::

    make clean

Create a release archive::

    make release

Install files into the live system::

    sudo make install

Remove installed files from the live system::

    sudo make uninstall

Stage files into an alternate root directory::

    make install DESTDIR=/tmp/acd-stage

Auto-Brightness Notes
---------------------

On Apple LED Cinema Display 27-inch (05ac:9226), controlled testing confirms
that the standard VESA/MCCS ambient-light-sensor control is exposed through
feature report 102 / usage 0x00820066.

Current interpretation:

* report 102 / usage 0x00820066: confirmed ambient light sensor /
  auto-brightness control
* report 225 / usage 0xff9200e1: tentative vendor-private boolean
* report 236 / usage 0xff9200ec: tentative vendor-private data/status

The vendor-private fields are still under investigation and should not yet be
 treated as stable public interface guarantees.

Known Limitations
-----------------

Brightness ranges depend on the display model.

On the maintainer's tested Apple LED Cinema Display 27" (A1316), the effective
brightness range is 0..1023. Other supported models may expose different useful
ranges.

The display detection process is not fully automatic yet, because you still
need to point the tool at a candidate HID device path.

On some systems, the first brightness read after boot may be inaccurate until a
brightness value is written once. After that, readings are expected to be
correct.

In practice, this is usually not a major issue because Cinema and Studio
Displays store their brightness settings across sessions.

Known auto-brightness support
-----------------------------

Native auto-brightness control is currently implemented for the Apple LED
Cinema Display 27-inch identified as USB ID ``05ac:9226``.

On this model, the auto-brightness HID control has been mapped as a two-state
feature:

* ``1`` = auto-brightness off
* ``2`` = auto-brightness on

When auto-brightness is enabled, the monitor itself adjusts the standard
brightness control in response to ambient light.

acdprobe
========

``acdprobe`` is a companion utility for exploring HID monitor features exposed
by supported displays.

While ``acdcontrol`` focuses on stable user-facing control paths such as
brightness, ``acdprobe`` is intended for discovery, reverse engineering, and
collection of raw capability data per monitor model.

Current goals
-------------

* Enumerate HID applications, reports, fields, and usages
* Export current values for input and feature reports
* Save probe artifacts in stable per-device layouts
* Support controlled feature writes with immediate readback confirmation
* Build a raw data catalog that contributors can submit by pull request

Default output layout
---------------------

By default, ``acdprobe`` saves results under::

    probes/raw/VID_PID/

For example::

    probes/raw/05ac_9226/05ac_9226-if0.json
    probes/raw/05ac_9226/05ac_9226-if0.txt
    probes/raw/05ac_9226/05ac_9226-if0.csv
    probes/raw/05ac_9226/05ac_9226-if0.rdesc.hex

The directory name is based on USB vendor ID and product ID.
The filename is based on ``VID_PID`` plus interface number.

Collect mode
------------

For community submissions, prefer collect mode or a readonly profile::

    ./acdprobe --collect /dev/acdctl4

This writes a stable bundle under::

    probes/VID_PID/

For example::

    probes/05ac_9226/report.json
    probes/05ac_9226/report.txt
    probes/05ac_9226/report.csv
    probes/05ac_9226/summary.json
    probes/05ac_9226/report_descriptor.hex

The filenames do not include timestamps or usernames.

Artifact review before sharing
------------------------------

The stable collection bundle is intended to be reviewable and shareable.
Current collected output is privacy-focused by default:

* host-specific USB topology and path details are omitted
* no sysfs realpaths are emitted
* no runtime USB bus/device numbering is emitted
* no USB serial string is emitted
* filenames do not include usernames or timestamps

The retained USB/sysfs identity fields are limited to device-level comparison
signals that help correlate submissions across the same monitor family:

* ``manufacturer``
* ``product``
* ``idVendor``
* ``idProduct``
* ``bcdDevice``

This keeps contributed bundles useful for reverse engineering without exposing
host topology or per-unit serial identity.

Build
-----

Build ``acdprobe`` from the repository root with::

    make

Basic usage
-----------

Probe a device and save the default raw artifacts::

    ./acdprobe /dev/acdctl4

Probe a device without saving files::

    ./acdprobe /dev/acdctl4 --no-save

Use collect mode for a stable bundle::

    ./acdprobe --collect /dev/acdctl4

Use a different base directory for raw probe output::

    ./acdprobe /dev/acdctl4 --save-dir probes/raw

Profiles
--------

``acdprobe`` provides named profiles to separate safe collection from
experimental write testing.

Readonly collection profile::

    ./acdprobe --profile basic-readonly /dev/acdctl4

Verbose readonly collection profile::

    ./acdprobe --profile full-readonly /dev/acdctl4

Experimental ALS discovery profile::

    ./acdprobe --profile als-discovery /dev/acdctl4 --set-feature 102 0 0 2 \
      --readback-delay-ms 500

Readonly profiles imply collection mode and reject ``--set-feature``.
The ``als-discovery`` profile enables experimental writes and prints an explicit
warning before applying them.

Controlled feature write with readback
--------------------------------------

``acdprobe`` can perform a controlled write to a single HID feature usage and
read it back immediately.

Example::

    ./acdprobe /dev/acdctl4 --set-feature 102 0 0 2 --readback-delay-ms 500

This means:

* report ID = ``102``
* field index = ``0``
* usage index = ``0``
* requested value = ``2``

The tool reads the current value, validates the requested value against the
logical range, writes it, commits the feature report, and then reads it back
again. This mode is intended for targeted experiments only. It does not do
brute-force probing or fuzzing.

Compact summary output
----------------------

Collect mode also writes a compact ``summary.json`` file with:

* ``schema_version``
* tool name and version
* compact device identification fields
* privacy-filtered USB/sysfs identity fields
* application summary
* report counts
* report descriptor fingerprint
* enriched controls with current value, range, flags, units, report metadata,
  application context, usage context, field logical/physical values, and
  confidence labels
* a dedicated ``telemetry_candidates`` section for unresolved vendor-private
  controls

This summary is intended to make future per-model databases, compatibility
indexes, and cross-platform comparisons easier to build.

Known findings for Apple LED Cinema Display 27-inch
---------------------------------------------------

Probe and live write/readback testing for USB ID ``05ac:9226`` show:

* feature report ``16`` / usage ``0x00820010``: confirmed brightness control
* feature report ``102`` / usage ``0x00820066``: confirmed auto-brightness
  toggle
* feature report ``225`` / usage ``0xff9200e1``: tentative vendor-private
  boolean
* feature report ``236`` / usage ``0xff9200ec``: tentative vendor-private
  data/status

At the moment, the confirmed mapping for report ``102`` is:

* ``1`` = auto-brightness off
* ``2`` = auto-brightness on

Reports ``225`` and ``236`` remain under investigation.

Contributing probe data
-----------------------

Contributors can run the readonly collection workflow and submit the generated
files under the appropriate ``probes/VID_PID/`` directory by pull request.

Recommended workflow::

    ./acdprobe --profile basic-readonly /dev/acdctl4

For deeper non-writing collection::

    ./acdprobe --profile full-readonly /dev/acdctl4

Use ``als-discovery`` only if you explicitly want to help with controlled write
experiments.

When submitting artifacts, reviewers should start with ``summary.json`` and use
``report.json`` or ``report.txt`` only when they need the fuller raw context.

The long-term goal is to build a per-model raw feature database and derive a
curated catalog of stable capabilities from those submissions.

Summary Schema Documentation
----------------------------

The compact ``summary.json`` file generated by ``acdprobe`` collect mode is
documented in:

* ``docs/summary-schema.rst``

That document describes:

* the purpose of ``summary.json``
* top-level fields and their meanings
* the privacy model for collected USB/sysfs metadata
* control-level metadata and current value encoding
* the dedicated ``telemetry_candidates`` section for unresolved vendor-private
  controls
* schema stability expectations
* confidence labels such as ``known``, ``candidate``, ``tentative``,
  ``observed``, and ``unknown``

Contributors reviewing or submitting probe bundles should start with
``summary.json`` and use the schema document as the reference for interpreting
its contents.

Release 0.5 highlights
----------------------

* Stricter brightness argument validation
* Clearer CLI error messages
* Improved Makefile targets for build, install, uninstall, and release
  packaging
* Creation of ``acdprobe`` utility
* Added native auto-brightness control for Apple LED Cinema Display 27-inch
  (USB ID ``05ac:9226``)
* Added ``--auto-brightness on|off|status`` to ``acdcontrol``
* Confirmed HID mapping for the auto-brightness toggle through ``acdprobe``
* Added collection mode, compact summary output, and descriptor fingerprinting
  to ``acdprobe``
* Added readonly and experimental collection profiles to ``acdprobe``
* Enriched ``summary.json`` with control metadata for cross-platform comparison
* Improved README coverage for ``acdcontrol`` and ``acdprobe``
