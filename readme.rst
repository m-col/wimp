::

 ██      ██  ██  ████████    ██████
 ██  ██  ██      ██  ██  ██  ██    ██
 ██  ██  ██  ██  ██  ██  ██  ██    ██
 ██  ██  ██  ██  ██  ██  ██  ██████
   ████████  ██  ██      ██  ██
             ██              ██
             ██


A desktop with limitless bounds. Picture GIMP but for windows. WIMP, if you will.

A Wayland compositor based on wlroots.

This software is a work in progress.

Building
--------

Dependencies:

 - wayland
 - wayland-protocols
 - wlroots (currently git master version)
 - cairo

.. code-block:: sh

   make
   sudo make install

Acknowledgemnets
----------------

I learned a lot (and copied some code) from:

 - `wlroots <https://github.com/swaywm/wlroots>`_
 - `sway <https://github.com/swaywm/sway>`_
 - `dwl <https://github.com/djpohly/dwl>`_
 - `labwc <https://github.com/johanmalm/labwc>`_
