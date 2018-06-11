#define MULTIPLUGIN 1
#define X42_MULTIPLUGIN_NAME "Peak Limiter"
#define X42_MULTIPLUGIN_URI "http://gareus.org/oss/lv2/dpl"

#include "lv2ttl/dpl_mono.h"
#include "lv2ttl/dpl_stereo.h"

static const RtkLv2Description _plugins[] = {
	_plugin_stereo,
	_plugin_mono,
};

