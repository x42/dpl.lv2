dpl.lv2 - Digital Peak Limiter
==============================

dpl.lv2 is a look-ahead digital peak limiter intended but not limited to the final step of mastering or mixing.

It is available as [LV2 plugin](http://lv2plug.in/) and standalone [JACK](http://jackaudio.org/)-application.

Usage
-----

The Plugin has three controls which can be operated by mouse-drag and the scroll-wheel.
Holding the _Ctrl_ key allows for fine-grained control when dragging or using the mouse-wheel on a knob.

Furthermore the UI offers
*   Shift + click: reset to default
*   Right-click on knob: toggle current value with default, 2nd click restore.

The rotary knobs from left to right allow to set

*   Input gain. This sets the gain applied before peak detection or any other processing. Range is -10 to +30 dB in steps of 0.2 dB.

*   Threshold. The maximum sample value at the output. -10 to 0 dB in steps of 0.1 dB. dpl.lv2 will not allow a single sample above this level.

*   Release time. This can be set from 1 ms to 1 second. Note that dpl.lv2 allows short release times even on signals that contain high level low frequency signals. Any gain reduction caused by those will have an automatically extended hold time in order to avoid the limiter following the shape of the waveform and create excessive distortion. Short superimposed peaks will still have the release time as set by this control.


Install
-------

Compiling dpl.lv2 requires the LV2 SDK, jack-headers, gnu-make, a c++-compiler,
libpango, libcairo and openGL (sometimes called: glu, glx, mesa).

```bash
  git clone git://github.com/x42/dpl.lv2.git
  cd dpl.lv2
  make submodules
  make
  sudo make install PREFIX=/usr
```

Note to packagers: the Makefile honors `PREFIX` and `DESTDIR` variables as well
as `CXXFLAGS`, `LDFLAGS` and `OPTIMIZATIONS` (additions to `CXXFLAGS`), also
see the first 10 lines of the Makefile.
You really want to package the superset of [x42-plugins](https://github.com/x42/x42-plugins).


Screenshots
-----------

![screenshot](https://raw.github.com/x42/dpl.lv2/master/img/dpl1.png "DPL LV2 GUI")

Credits
-------

dpl.lv2 is based on zita-jacktools-1.0.0 by Fons Adriaensen.
