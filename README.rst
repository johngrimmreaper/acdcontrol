======================================
Apple Cinema Display Brightness Control
======================================

``acdcontrol`` is a small Unix-style utility for querying and changing the
brightness of supported Apple Cinema and Studio Displays through the HID
interface exposed by Linux.

The project currently ships two command-line tools:

* ``acdcontrol`` for stable user-facing control paths such as brightness
* ``acdprobe`` for device inspection, discovery, and HID capability collection

This repository is focused on the utility itself. Lower-level driver work may
happen in a future separate project.

Maintainer
----------

Project currently maintained by John Grimm, also known as Reaper.

Repository::

    https://github.com/johngrimmreaper/acdcontrol

Current status
--------------

``acdcontrol`` is usable today, but the hardware matrix is still growing.

Support in this repository should be understood in three layers:

* officially supported models listed by the tool
* candidate USB monitor devices that may still work through the same HID control
  layout
* experimental probing and feature discovery through ``acdprobe``

A display not appearing in the official support list does not automatically mean
it cannot work. It may simply mean it has not been tested and documented yet.

Compiling
---------

Clone this repository::

    git clone https://github.com/johngrimmreaper/acdcontrol.git
    cd acdcontrol

Build both binaries::

    make

New files named ``acdcontrol`` and ``acdprobe`` should appear in the same
repository directory. If the build fails, make sure the required development
tools are installed, for example ``build-essential`` on Debian/Ubuntu systems.

You can also use the helper script::

    ./acd-compile.sh

This uses the included ``docker-compose.yml`` file and requires Docker and
Docker Compose.

Installation
------------

Install the binaries and udev rules::

    sudo make install

Reload the udev configuration and trigger the updated rules::

    sudo udevadm control --reload
    sudo udevadm trigger

Remove the installed files with::

    sudo make uninstall

For staged installs and packaging, you can override the install paths.
For example::

    make install DESTDIR=/tmp/acd-stage

Usage
-----

::

    ./acdcontrol [--silent|-s] [--brief|-b] [--help|-h] [--about|-a] \
      [--force|-f] [--detect|-d] [--list-all|-l] \
      [--auto-brightness on|off|status] \
      /dev/acdctlX [brightness]

Detection mode does not write brightness values, so it is the safest first step
when identifying a candidate HID device.

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

List officially supported displays::

    ./acdcontrol --list-all

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

Probe a candidate device and save a report bundle::

    ./acdprobe --collect /dev/acdctl4

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

Documentation map
-----------------

Use these entry points depending on what you need:

* ``man/acdcontrol.1``
    Curated command reference for the main utility.

* ``man/acdprobe.1``
    Curated command reference for the probe utility.

* ``docs/probe-workflow.rst``
    Recommended collection, review, and submission workflow for probe bundles.

* ``docs/model-notes/05ac_9226.rst``
    Current per-model findings for the Apple LED Cinema Display 27-inch
    (USB ID ``05ac:9226``).

* ``docs/summary-schema.rst``
    Reference for the compact ``summary.json`` file generated by
    ``acdprobe --collect``.

The long-term documentation direction is:

* short top-level ``README.rst`` as the project landing page
* curated manual pages for ``acdcontrol`` and ``acdprobe``
* focused technical notes under ``docs/`` for schema, workflow,
  model findings, and reverse-engineering notes

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

Contributing
------------

Contributions are welcome, especially:

* probe bundles for additional Apple display models
* confirmation of working brightness or auto-brightness mappings
* documentation improvements
* carefully reviewed code improvements

For probe collection and review workflow, see ``docs/probe-workflow.rst``.

License
-------

See ``COPYING`` and ``COPYRIGHT``.