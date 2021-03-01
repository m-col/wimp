.. image:: logo.png
   :alt: Logo

A desktop with limitless bounds. Picture GIMP but for windows. WIMP, if you
will.

A Wayland compositor focussed on organisation and enabling an efficient
workflow.

WIMP is still early in development, but (almost) everything works and is
usable.

Aims/Features
-------------

 - Intuitive navigation around and management of many windows
 - Great trackpad experience
 - Seamless multi-monitor experience
 - Remain lightweight and minimal, delegating where appropriate to clients
 - Wide support of useful desktop wayland protocols
 - And avoid adding Xwayland

Using WIMP
----------

WIMP can be called directly from a TTY as ``wimp``. I recommend writing a small
script to set up the session's environment and ``exec wimp``.

WIMP looks for a startup script first at ``$XDG_CONFIG_HOME/wimp/startup`` then
``$HOME/.config/wimp/startup``. This script is executed once the compositor has
started up and can be used to configure wimp via ``wimptool`` and launch any
startup programs. An example startup script is provided and also serves as a
reference of all possible options and actions.

Users migrating from X may find `arewewaylandyet.com
<https://arewewaylandyet.com/>`_ and `this page
<https://github.com/swaywm/sway/wiki/i3-Migration-Guide>`_ from Sway's wiki
helpful for finding Wayland-supporting alternatives to common X-specific
utilies.

Building
--------

Dependencies:

 - wayland
 - wayland-protocols
 - wlroots (currently git master version)
 - cairo

1. make with ``make``
2. install with ``sudo make install``

Acknowledgements
----------------

I learned a lot (and copied some code) from:

 - `sway <https://github.com/swaywm/sway>`_
 - `dwl <https://github.com/djpohly/dwl>`_
 - `labwc <https://github.com/johanmalm/labwc>`_
 - `cardboard <https://gitlab.com/cardboardwm/cardboard>`_

With special thanks to the guys working on `wlroots
<https://github.com/swaywm/wlroots>`_, upon which WIMP is built. Their TinyWL
compositor served as a helpful base to start from.
