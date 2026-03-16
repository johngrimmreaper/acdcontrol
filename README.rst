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

A new file named ``acdcontrol`` should appear in the same directory. If the
build fails, make sure the required development tools are installed, for example
``build-essential`` on Debian/Ubuntu systems.

Installation
------------

Install the program into ``/usr/local/bin`` and install the udev rules::

    sudo make install

Reload the udev configuration and trigger the updated rules::

    sudo udevadm control --reload
    sudo udevadm trigger

Remove the installed files with::

    sudo make uninstall

For staged installs and packaging, you can override the install paths. For
example::

    make install DESTDIR=/tmp/acd-stage

You can also use the helper script::

    ./acd-compile.sh

This uses the included ``docker-compose.yml`` file and requires Docker and
Docker Compose.

Usage
-----

::

    ./acdcontrol [--silent|-s] [--brief|-b] [--help|-h] [--about|-a] \
                 [--detect|-d] [--list-all|-l] /dev/acdctlX [brightness]

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

``-d``, ``--detect``
    Run detection mode. In this mode no brightness writes are performed, which
    makes it safer to probe candidate HID devices.

``-l``, ``--list-all``
    List all officially supported monitors and exit. A display not appearing in
    this list does not necessarily mean it will not work; it may simply mean it
    was not tested yet.

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

    ./acdcontrol /dev/acdctl0 160

Increase brightness by 10::

    ./acdcontrol /dev/acdctl0 +10

Decrease brightness by 10::

    ./acdcontrol /dev/acdctl0 -- -10

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

Build the binary::

    make

Remove the built binary::

    make clean

Create a release archive::

    make release

Install files into the live system::

    sudo make install

Remove installed files from the live system::

    sudo make uninstall

Stage files into an alternate root directory::

    make install DESTDIR=/tmp/acd-stage

Known Limitations
-----------------
Brightness ranges depend on the display model.

On the maintainer's tested Apple LED Cinema Display 27" (A1316), the
effective brightness range is 0..1023. Other supported models may expose
different useful ranges.

The display detection process is not fully automatic yet, because you still
need to point the tool at a candidate HID device path.

On some systems, the first brightness read after boot may be inaccurate until a
brightness value is written once. After that, readings are expected to be
correct.

In practice, this is usually not a major issue because Cinema and Studio
Displays store their brightness settings across sessions.

Release 0.5 highlights
----------------------

* Stricter brightness argument validation
* Clearer CLI error messages
* Improved Makefile targets for build, install, uninstall, and release packaging