case NT35510:

	delay(100);
	writeCommand16Transaction(0xF000); writeData16Transaction(0x55);
	writeCommand16Transaction(0xF001); writeData16Transaction(0xAA);
	writeCommand16Transaction(0xF002); writeData16Transaction(0x52);
	writeCommand16Transaction(0xF003); writeData16Transaction(0x08);
	writeCommand16Transaction(0xF004); writeData16Transaction(0x01);
	//# AVDD: manual); writeData16Transaction(
	writeCommand16Transaction(0xB600); writeData16Transaction(0x34);
	writeCommand16Transaction(0xB601); writeData16Transaction(0x34);
	writeCommand16Transaction(0xB602); writeData16Transaction(0x34);

	writeCommand16Transaction(0xB000); writeData16Transaction(0x0D);//09
	writeCommand16Transaction(0xB001); writeData16Transaction(0x0D);
	writeCommand16Transaction(0xB002); writeData16Transaction(0x0D);
	//# AVEE: manual); writeData16Transaction( -6V
	writeCommand16Transaction(0xB700); writeData16Transaction(0x24);
	writeCommand16Transaction(0xB701); writeData16Transaction(0x24);
	writeCommand16Transaction(0xB702); writeData16Transaction(0x24);

	writeCommand16Transaction(0xB100); writeData16Transaction(0x0D);
	writeCommand16Transaction(0xB101); writeData16Transaction(0x0D);
	writeCommand16Transaction(0xB102); writeData16Transaction(0x0D);
	//#Power Control for
	//VCL
	writeCommand16Transaction(0xB800); writeData16Transaction(0x24);
	writeCommand16Transaction(0xB801); writeData16Transaction(0x24);
	writeCommand16Transaction(0xB802); writeData16Transaction(0x24);

	writeCommand16Transaction(0xB200); writeData16Transaction(0x00);

	//# VGH: Clamp Enable); writeData16Transaction(
	writeCommand16Transaction(0xB900); writeData16Transaction(0x24);
	writeCommand16Transaction(0xB901); writeData16Transaction(0x24);
	writeCommand16Transaction(0xB902); writeData16Transaction(0x24);

	writeCommand16Transaction(0xB300); writeData16Transaction(0x05);
	writeCommand16Transaction(0xB301); writeData16Transaction(0x05);
	writeCommand16Transaction(0xB302); writeData16Transaction(0x05);

	///writeCommand16Transaction(0xBF00); writeData16Transaction(0x01);

	//# VGL(LVGL):
	writeCommand16Transaction(0xBA00); writeData16Transaction(0x34);
	writeCommand16Transaction(0xBA01); writeData16Transaction(0x34);
	writeCommand16Transaction(0xBA02); writeData16Transaction(0x34);
	//# VGL_REG(VGLO)
	writeCommand16Transaction(0xB500); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xB501); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xB502); writeData16Transaction(0x0B);
	//# VGMP/VGSP:
	writeCommand16Transaction(0xBC00); writeData16Transaction(0X00);
	writeCommand16Transaction(0xBC01); writeData16Transaction(0xA3);
	writeCommand16Transaction(0xBC02); writeData16Transaction(0X00);
	//# VGMN/VGSN
	writeCommand16Transaction(0xBD00); writeData16Transaction(0x00);
	writeCommand16Transaction(0xBD01); writeData16Transaction(0xA3);
	writeCommand16Transaction(0xBD02); writeData16Transaction(0x00);
	//# VCOM=-0.1
	writeCommand16Transaction(0xBE00); writeData16Transaction(0x00);
	writeCommand16Transaction(0xBE01); writeData16Transaction(0x63);//4f
		//  VCOMH+0x01;
	//#R+
	writeCommand16Transaction(0xD100); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD101); writeData16Transaction(0x37);
	writeCommand16Transaction(0xD102); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD103); writeData16Transaction(0x52);
	writeCommand16Transaction(0xD104); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD105); writeData16Transaction(0x7B);
	writeCommand16Transaction(0xD106); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD107); writeData16Transaction(0x99);
	writeCommand16Transaction(0xD108); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD109); writeData16Transaction(0xB1);
	writeCommand16Transaction(0xD10A); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD10B); writeData16Transaction(0xD2);
	writeCommand16Transaction(0xD10C); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD10D); writeData16Transaction(0xF6);
	writeCommand16Transaction(0xD10E); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD10F); writeData16Transaction(0x27);
	writeCommand16Transaction(0xD110); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD111); writeData16Transaction(0x4E);
	writeCommand16Transaction(0xD112); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD113); writeData16Transaction(0x8C);
	writeCommand16Transaction(0xD114); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD115); writeData16Transaction(0xBE);
	writeCommand16Transaction(0xD116); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD117); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xD118); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD119); writeData16Transaction(0x48);
	writeCommand16Transaction(0xD11A); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD11B); writeData16Transaction(0x4A);
	writeCommand16Transaction(0xD11C); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD11D); writeData16Transaction(0x7E);
	writeCommand16Transaction(0xD11E); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD11F); writeData16Transaction(0xBC);
	writeCommand16Transaction(0xD120); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD121); writeData16Transaction(0xE1);
	writeCommand16Transaction(0xD122); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD123); writeData16Transaction(0x10);
	writeCommand16Transaction(0xD124); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD125); writeData16Transaction(0x31);
	writeCommand16Transaction(0xD126); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD127); writeData16Transaction(0x5A);
	writeCommand16Transaction(0xD128); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD129); writeData16Transaction(0x73);
	writeCommand16Transaction(0xD12A); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD12B); writeData16Transaction(0x94);
	writeCommand16Transaction(0xD12C); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD12D); writeData16Transaction(0x9F);
	writeCommand16Transaction(0xD12E); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD12F); writeData16Transaction(0xB3);
	writeCommand16Transaction(0xD130); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD131); writeData16Transaction(0xB9);
	writeCommand16Transaction(0xD132); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD133); writeData16Transaction(0xC1);
	//#G+
	writeCommand16Transaction(0xD200); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD201); writeData16Transaction(0x37);
	writeCommand16Transaction(0xD202); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD203); writeData16Transaction(0x52);
	writeCommand16Transaction(0xD204); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD205); writeData16Transaction(0x7B);
	writeCommand16Transaction(0xD206); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD207); writeData16Transaction(0x99);
	writeCommand16Transaction(0xD208); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD209); writeData16Transaction(0xB1);
	writeCommand16Transaction(0xD20A); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD20B); writeData16Transaction(0xD2);
	writeCommand16Transaction(0xD20C); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD20D); writeData16Transaction(0xF6);
	writeCommand16Transaction(0xD20E); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD20F); writeData16Transaction(0x27);
	writeCommand16Transaction(0xD210); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD211); writeData16Transaction(0x4E);
	writeCommand16Transaction(0xD212); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD213); writeData16Transaction(0x8C);
	writeCommand16Transaction(0xD214); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD215); writeData16Transaction(0xBE);
	writeCommand16Transaction(0xD216); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD217); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xD218); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD219); writeData16Transaction(0x48);
	writeCommand16Transaction(0xD21A); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD21B); writeData16Transaction(0x4A);
	writeCommand16Transaction(0xD21C); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD21D); writeData16Transaction(0x7E);
	writeCommand16Transaction(0xD21E); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD21F); writeData16Transaction(0xBC);
	writeCommand16Transaction(0xD220); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD221); writeData16Transaction(0xE1);
	writeCommand16Transaction(0xD222); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD223); writeData16Transaction(0x10);
	writeCommand16Transaction(0xD224); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD225); writeData16Transaction(0x31);
	writeCommand16Transaction(0xD226); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD227); writeData16Transaction(0x5A);
	writeCommand16Transaction(0xD228); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD229); writeData16Transaction(0x73);
	writeCommand16Transaction(0xD22A); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD22B); writeData16Transaction(0x94);
	writeCommand16Transaction(0xD22C); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD22D); writeData16Transaction(0x9F);
	writeCommand16Transaction(0xD22E); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD22F); writeData16Transaction(0xB3);
	writeCommand16Transaction(0xD230); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD231); writeData16Transaction(0xB9);
	writeCommand16Transaction(0xD232); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD233); writeData16Transaction(0xC1);
	//#B+
	writeCommand16Transaction(0xD300); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD301); writeData16Transaction(0x37);
	writeCommand16Transaction(0xD302); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD303); writeData16Transaction(0x52);
	writeCommand16Transaction(0xD304); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD305); writeData16Transaction(0x7B);
	writeCommand16Transaction(0xD306); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD307); writeData16Transaction(0x99);
	writeCommand16Transaction(0xD308); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD309); writeData16Transaction(0xB1);
	writeCommand16Transaction(0xD30A); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD30B); writeData16Transaction(0xD2);
	writeCommand16Transaction(0xD30C); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD30D); writeData16Transaction(0xF6);
	writeCommand16Transaction(0xD30E); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD30F); writeData16Transaction(0x27);
	writeCommand16Transaction(0xD310); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD311); writeData16Transaction(0x4E);
	writeCommand16Transaction(0xD312); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD313); writeData16Transaction(0x8C);
	writeCommand16Transaction(0xD314); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD315); writeData16Transaction(0xBE);
	writeCommand16Transaction(0xD316); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD317); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xD318); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD319); writeData16Transaction(0x48);
	writeCommand16Transaction(0xD31A); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD31B); writeData16Transaction(0x4A);
	writeCommand16Transaction(0xD31C); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD31D); writeData16Transaction(0x7E);
	writeCommand16Transaction(0xD31E); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD31F); writeData16Transaction(0xBC);
	writeCommand16Transaction(0xD320); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD321); writeData16Transaction(0xE1);
	writeCommand16Transaction(0xD322); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD323); writeData16Transaction(0x10);
	writeCommand16Transaction(0xD324); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD325); writeData16Transaction(0x31);
	writeCommand16Transaction(0xD326); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD327); writeData16Transaction(0x5A);
	writeCommand16Transaction(0xD328); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD329); writeData16Transaction(0x73);
	writeCommand16Transaction(0xD32A); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD32B); writeData16Transaction(0x94);
	writeCommand16Transaction(0xD32C); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD32D); writeData16Transaction(0x9F);
	writeCommand16Transaction(0xD32E); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD32F); writeData16Transaction(0xB3);
	writeCommand16Transaction(0xD330); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD331); writeData16Transaction(0xB9);
	writeCommand16Transaction(0xD332); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD333); writeData16Transaction(0xC1);

	//#R-///////////////////////////////////////////
	writeCommand16Transaction(0xD400); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD401); writeData16Transaction(0x37);
	writeCommand16Transaction(0xD402); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD403); writeData16Transaction(0x52);
	writeCommand16Transaction(0xD404); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD405); writeData16Transaction(0x7B);
	writeCommand16Transaction(0xD406); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD407); writeData16Transaction(0x99);
	writeCommand16Transaction(0xD408); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD409); writeData16Transaction(0xB1);
	writeCommand16Transaction(0xD40A); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD40B); writeData16Transaction(0xD2);
	writeCommand16Transaction(0xD40C); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD40D); writeData16Transaction(0xF6);
	writeCommand16Transaction(0xD40E); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD40F); writeData16Transaction(0x27);
	writeCommand16Transaction(0xD410); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD411); writeData16Transaction(0x4E);
	writeCommand16Transaction(0xD412); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD413); writeData16Transaction(0x8C);
	writeCommand16Transaction(0xD414); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD415); writeData16Transaction(0xBE);
	writeCommand16Transaction(0xD416); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD417); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xD418); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD419); writeData16Transaction(0x48);
	writeCommand16Transaction(0xD41A); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD41B); writeData16Transaction(0x4A);
	writeCommand16Transaction(0xD41C); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD41D); writeData16Transaction(0x7E);
	writeCommand16Transaction(0xD41E); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD41F); writeData16Transaction(0xBC);
	writeCommand16Transaction(0xD420); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD421); writeData16Transaction(0xE1);
	writeCommand16Transaction(0xD422); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD423); writeData16Transaction(0x10);
	writeCommand16Transaction(0xD424); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD425); writeData16Transaction(0x31);
	writeCommand16Transaction(0xD426); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD427); writeData16Transaction(0x5A);
	writeCommand16Transaction(0xD428); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD429); writeData16Transaction(0x73);
	writeCommand16Transaction(0xD42A); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD42B); writeData16Transaction(0x94);
	writeCommand16Transaction(0xD42C); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD42D); writeData16Transaction(0x9F);
	writeCommand16Transaction(0xD42E); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD42F); writeData16Transaction(0xB3);
	writeCommand16Transaction(0xD430); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD431); writeData16Transaction(0xB9);
	writeCommand16Transaction(0xD432); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD433); writeData16Transaction(0xC1);

	//#G-//////////////////////////////////////////////
	writeCommand16Transaction(0xD500); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD501); writeData16Transaction(0x37);
	writeCommand16Transaction(0xD502); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD503); writeData16Transaction(0x52);
	writeCommand16Transaction(0xD504); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD505); writeData16Transaction(0x7B);
	writeCommand16Transaction(0xD506); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD507); writeData16Transaction(0x99);
	writeCommand16Transaction(0xD508); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD509); writeData16Transaction(0xB1);
	writeCommand16Transaction(0xD50A); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD50B); writeData16Transaction(0xD2);
	writeCommand16Transaction(0xD50C); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD50D); writeData16Transaction(0xF6);
	writeCommand16Transaction(0xD50E); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD50F); writeData16Transaction(0x27);
	writeCommand16Transaction(0xD510); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD511); writeData16Transaction(0x4E);
	writeCommand16Transaction(0xD512); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD513); writeData16Transaction(0x8C);
	writeCommand16Transaction(0xD514); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD515); writeData16Transaction(0xBE);
	writeCommand16Transaction(0xD516); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD517); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xD518); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD519); writeData16Transaction(0x48);
	writeCommand16Transaction(0xD51A); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD51B); writeData16Transaction(0x4A);
	writeCommand16Transaction(0xD51C); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD51D); writeData16Transaction(0x7E);
	writeCommand16Transaction(0xD51E); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD51F); writeData16Transaction(0xBC);
	writeCommand16Transaction(0xD520); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD521); writeData16Transaction(0xE1);
	writeCommand16Transaction(0xD522); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD523); writeData16Transaction(0x10);
	writeCommand16Transaction(0xD524); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD525); writeData16Transaction(0x31);
	writeCommand16Transaction(0xD526); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD527); writeData16Transaction(0x5A);
	writeCommand16Transaction(0xD528); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD529); writeData16Transaction(0x73);
	writeCommand16Transaction(0xD52A); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD52B); writeData16Transaction(0x94);
	writeCommand16Transaction(0xD52C); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD52D); writeData16Transaction(0x9F);
	writeCommand16Transaction(0xD52E); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD52F); writeData16Transaction(0xB3);
	writeCommand16Transaction(0xD530); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD531); writeData16Transaction(0xB9);
	writeCommand16Transaction(0xD532); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD533); writeData16Transaction(0xC1);
	//#B-///////////////////////////////
	writeCommand16Transaction(0xD600); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD601); writeData16Transaction(0x37);
	writeCommand16Transaction(0xD602); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD603); writeData16Transaction(0x52);
	writeCommand16Transaction(0xD604); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD605); writeData16Transaction(0x7B);
	writeCommand16Transaction(0xD606); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD607); writeData16Transaction(0x99);
	writeCommand16Transaction(0xD608); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD609); writeData16Transaction(0xB1);
	writeCommand16Transaction(0xD60A); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD60B); writeData16Transaction(0xD2);
	writeCommand16Transaction(0xD60C); writeData16Transaction(0x00);
	writeCommand16Transaction(0xD60D); writeData16Transaction(0xF6);
	writeCommand16Transaction(0xD60E); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD60F); writeData16Transaction(0x27);
	writeCommand16Transaction(0xD610); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD611); writeData16Transaction(0x4E);
	writeCommand16Transaction(0xD612); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD613); writeData16Transaction(0x8C);
	writeCommand16Transaction(0xD614); writeData16Transaction(0x01);
	writeCommand16Transaction(0xD615); writeData16Transaction(0xBE);
	writeCommand16Transaction(0xD616); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD617); writeData16Transaction(0x0B);
	writeCommand16Transaction(0xD618); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD619); writeData16Transaction(0x48);
	writeCommand16Transaction(0xD61A); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD61B); writeData16Transaction(0x4A);
	writeCommand16Transaction(0xD61C); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD61D); writeData16Transaction(0x7E);
	writeCommand16Transaction(0xD61E); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD61F); writeData16Transaction(0xBC);
	writeCommand16Transaction(0xD620); writeData16Transaction(0x02);
	writeCommand16Transaction(0xD621); writeData16Transaction(0xE1);
	writeCommand16Transaction(0xD622); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD623); writeData16Transaction(0x10);
	writeCommand16Transaction(0xD624); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD625); writeData16Transaction(0x31);
	writeCommand16Transaction(0xD626); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD627); writeData16Transaction(0x5A);
	writeCommand16Transaction(0xD628); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD629); writeData16Transaction(0x73);
	writeCommand16Transaction(0xD62A); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD62B); writeData16Transaction(0x94);
	writeCommand16Transaction(0xD62C); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD62D); writeData16Transaction(0x9F);
	writeCommand16Transaction(0xD62E); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD62F); writeData16Transaction(0xB3);
	writeCommand16Transaction(0xD630); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD631); writeData16Transaction(0xB9);
	writeCommand16Transaction(0xD632); writeData16Transaction(0x03);
	writeCommand16Transaction(0xD633); writeData16Transaction(0xC1);



	//#Enable Page0
	writeCommand16Transaction(0xF000); writeData16Transaction(0x55);
	writeCommand16Transaction(0xF001); writeData16Transaction(0xAA);
	writeCommand16Transaction(0xF002); writeData16Transaction(0x52);
	writeCommand16Transaction(0xF003); writeData16Transaction(0x08);
	writeCommand16Transaction(0xF004); writeData16Transaction(0x00);
	//# RGB I/F Setting
	writeCommand16Transaction(0xB000); writeData16Transaction(0x08);
	writeCommand16Transaction(0xB001); writeData16Transaction(0x05);
	writeCommand16Transaction(0xB002); writeData16Transaction(0x02);
	writeCommand16Transaction(0xB003); writeData16Transaction(0x05);
	writeCommand16Transaction(0xB004); writeData16Transaction(0x02);
	//## SDT:
	writeCommand16Transaction(0xB600); writeData16Transaction(0x08);
	writeCommand16Transaction(0xB500); writeData16Transaction(0x50);//0x6b ???? 480x854       0x50 ???? 480x800

	//## Gate EQ:
	writeCommand16Transaction(0xB700); writeData16Transaction(0x00);
	writeCommand16Transaction(0xB701); writeData16Transaction(0x00);

	//## Source EQ:
	writeCommand16Transaction(0xB800); writeData16Transaction(0x01);
	writeCommand16Transaction(0xB801); writeData16Transaction(0x05);
	writeCommand16Transaction(0xB802); writeData16Transaction(0x05);
	writeCommand16Transaction(0xB803); writeData16Transaction(0x05);

	//# Inversion: Column inversion (NVT)
	writeCommand16Transaction(0xBC00); writeData16Transaction(0x00);
	writeCommand16Transaction(0xBC01); writeData16Transaction(0x00);
	writeCommand16Transaction(0xBC02); writeData16Transaction(0x00);

	//# BOE's Setting(default)
	writeCommand16Transaction(0xCC00); writeData16Transaction(0x03);
	writeCommand16Transaction(0xCC01); writeData16Transaction(0x00);
	writeCommand16Transaction(0xCC02); writeData16Transaction(0x00);

	//# Display Timing:
	writeCommand16Transaction(0xBD00); writeData16Transaction(0x01);
	writeCommand16Transaction(0xBD01); writeData16Transaction(0x84);
	writeCommand16Transaction(0xBD02); writeData16Transaction(0x07);
	writeCommand16Transaction(0xBD03); writeData16Transaction(0x31);
	writeCommand16Transaction(0xBD04); writeData16Transaction(0x00);

	writeCommand16Transaction(0xBA00); writeData16Transaction(0x01);

	writeCommand16Transaction(0xFF00); writeData16Transaction(0xAA);
	writeCommand16Transaction(0xFF01); writeData16Transaction(0x55);
	writeCommand16Transaction(0xFF02); writeData16Transaction(0x25);
	writeCommand16Transaction(0xFF03); writeData16Transaction(0x01);

	writeCommand16Transaction(0x3500); writeData16Transaction(0x00);
	writeCommand16Transaction(0x3600); writeData16Transaction(0x00);
	writeCommand16Transaction(0x3a00); writeData16Transaction(0x55);  ////55=16?/////66=18?
	writeCommand16Transaction(0x1100);
	delay(100);
	writeCommand16Transaction(0x2900);
	writeCommand16Transaction(0x2c00);



	break;
