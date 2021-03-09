// generated by lv2ttl2c from
// http://gareus.org/oss/lv2/dpl#stereo

extern const LV2_Descriptor* lv2_descriptor(uint32_t index);
extern const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index);

static const RtkLv2Description _plugin_stereo = {
	&lv2_descriptor,
	&lv2ui_descriptor
	, 1 // uint32_t dsp_descriptor_id
	, 0 // uint32_t gui_descriptor_id
	, "x42-dpl - Digital Peak Limiter Stereo" // const char *plugin_human_id
	, (const struct LV2Port[13])
	{
		{ "control", ATOM_IN, nan, nan, nan, "UI to plugin communication"},
		{ "notify", ATOM_OUT, nan, nan, nan, "Plugin to GUI communication"},
		{ "enable", CONTROL_IN, 1.000000, 0.000000, 1.000000, "Enable"},
		{ "gain", CONTROL_IN, 0.000000, -10.000000, 30.000000, "Input Gain"},
		{ "threshold", CONTROL_IN, 0.000000, -10.000000, 0.000000, "Threshold"},
		{ "release", CONTROL_IN, 0.010000, 0.001000, 1.000000, "Release Time"},
		{ "truepeak", CONTROL_IN, 0.000000, 0.000000, 1.000000, "True Peak"},
		{ "level", CONTROL_OUT, nan, -10.000000, 20.000000, "Signal Level"},
		{ "latency", CONTROL_OUT, nan, 0.000000, 1024.000000, "Signal Latency"},
		{ "inL", AUDIO_IN, nan, nan, nan, "In Left"},
		{ "outL", AUDIO_OUT, nan, nan, nan, "Out Left"},
		{ "inR", AUDIO_IN, nan, nan, nan, "In Right"},
		{ "outR", AUDIO_OUT, nan, nan, nan, "Out Right"},
	}
	, 13 // uint32_t nports_total
	, 2 // uint32_t nports_audio_in
	, 2 // uint32_t nports_audio_out
	, 0 // uint32_t nports_midi_in
	, 0 // uint32_t nports_midi_out
	, 1 // uint32_t nports_atom_in
	, 1 // uint32_t nports_atom_out
	, 7 // uint32_t nports_ctrl
	, 5 // uint32_t nports_ctrl_in
	, 2 // uint32_t nports_ctrl_out
	, 131424 // uint32_t min_atom_bufsiz
	, false // bool send_time_info
	, 8 // uint32_t latency_ctrl_port
};
