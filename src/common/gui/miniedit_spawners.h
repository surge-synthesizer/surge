//-------------------------------------------------------------------------------------------------------
//
//	Shortcircuit
//
//	Copyright 2004 Claes Johanson
//
//-------------------------------------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>
#include "sub3_synth.h"
using namespace std;

float spawn_miniedit_float(float f, int ctype);
int spawn_miniedit_int(int i, int ctype);
void spawn_miniedit_text(char* c, int maxchars);

struct patchdata
{
   string name;
   string category;
   string comments;
   string author;
};

bool spawn_patchstore_dialog(patchdata* p,
                             vector<patchlist_category>* patch_category,
                             int startcategory);