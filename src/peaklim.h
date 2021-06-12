/*
 * Copyright (C) 2010-2018 Fons Adriaensen <fons@linuxaudio.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _PEAKLIM_H
#define _PEAKLIM_H

#include <stdint.h>

namespace DPLLV2
{
class Histmin
{
public:
	Histmin (void)
	{
	}
	~Histmin (void)
	{
	}

	void  init (int hlen);
	float write (float v);
	float
	vmin (void)
	{
		return _vmin;
	}

private:
	enum { SIZE = 32,
	       MASK = SIZE - 1 };

	int   _hlen;
	int   _hold;
	int   _wind;
	float _vmin;
	float _hist[SIZE];
};

class Peaklim
{
public:
	enum { MAXCHAN = 2 };

	Peaklim (void);
	~Peaklim (void);

	void init (float fsamp, int nchan);
	void fini (void);

	void set_inpgain (float);
	void set_threshold (float);
	void set_release (float);
	void set_truepeak (bool);

	int
	get_latency () const
	{
		return _delay;
	}

	void
	get_stats (float* peak, float* gmax, float* gmin)
	{
		*peak  = _peak;
		*gmax  = _gmax;
		*gmin  = _gmin;
		_rstat = true;
	}

	void process (int nsamp, float* inp[], float* out[]);

private:
	float          _fsamp;
	int            _nchan;
	int            _div1;
	int            _div2;
	int            _delay;
	int            _dsize;
	int            _dmask;
	int            _delri;
	float*         _dbuff[MAXCHAN];
	int            _c1, _c2;
	float          _g0, _g1, _dg;
	float          _gt, _m1, _m2;
	float          _w1, _w2, _w3, _wlf;
	float          _z1, _z2, _z3;
	float          _zlf[MAXCHAN];
	float          _z[MAXCHAN][48];
	volatile bool  _rstat;
	volatile float _peak;
	volatile float _gmax;
	volatile float _gmin;
	Histmin        _hist1;
	Histmin        _hist2;
	bool           _truepeak;
};

} // namespace

#endif
