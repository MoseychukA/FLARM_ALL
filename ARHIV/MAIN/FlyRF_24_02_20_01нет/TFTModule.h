#pragma once

#include "AbstractModule.h"
//--------------------------------------------------------------------------------------------------------------------------------------
#ifdef USE_TFT_MODULE
#include "TFTMenu.h"
//--------------------------------------------------------------------------------------------------------------------------------------
class TFTModule : public AbstractModule // модуль поддержки  TFT
{
  private:

    TFTMenu myTFTMenu;
	  bool canUseMenu;
  
  public:
    TFTModule() : AbstractModule("TFT") {}


    void Setup();
    void Update();

};
#endif // USE_TFT_MODULE
//--------------------------------------------------------------------------------------------------------------------------------------

