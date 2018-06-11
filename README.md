dpl.lv2 - Digital Peak Limiter
==============================

dpl.lv2 is **NOT YET READY TO BE USED** ....

It is available as [LV2 plugin](http://lv2plug.in/) and standalone
[JACK](http://jackaudio.org/)-application.


Usage
-----

...

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
as `CXXLAGS`, `LDFLAGS` and `OPTIMIZATIONS` (additions to `CXXFLAGS`), also
see the first 10 lines of the Makefile.
You really want to package the superset of [x42-plugins](https://github.com/x42/x42-plugins).


Screenshots
-----------

![screenshot](https://raw.github.com/x42/dpl.lv2/master/img/dpl1.png "DPL LV2 GUI")

Credits
-------

dpl.lv2 is based on zita-jacktools-1.0.0 by Fons Adriaensen.
