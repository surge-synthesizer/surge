#pragma once

#include "vstcontrols.h"

class IUpdateReadOnlyPartner {
public:
   virtual void addReadOnlyPartner( VSTGUI::CControl *c ) = 0;
};
