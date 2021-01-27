#pragma once
#include <lv2/core/lv2.h>
#include <lv2/atom/atom.h>
#include <lv2/atom/util.h>
#include <lv2/instance-access/instance-access.h>
#include <lv2/data-access/data-access.h>
#include <lv2/options/options.h>
#include <lv2/ui/ui.h>
#include <lv2/urid/urid.h>
#include <lv2/units/units.h>
#include <lv2/resize-port/resize-port.h>
#include <lv2/midi/midi.h>
#include <lv2/time/time.h>
#include <lv2/state/state.h>

#ifndef LV2_UI__scaleFactor
#define LV2_UI__scaleFactor LV2_UI_PREFIX "scaleFactor"
#endif

#define SURGE_PLUGIN_URI "https://surge-synthesizer.github.io/lv2/surge"
#define SURGE_UI_URI SURGE_PLUGIN_URI "#UI"
#define SURGE_PATCH_URI SURGE_PLUGIN_URI "#PatchData"
