{
	/*
  writecommand(0xFD); // COMMANDLOCK
  writedata(0x12);
  writecommand(0xFD); // COMMANDLOCK
  writedata(0xB1);
  writecommand(0xAE); // DISPLAYOFF
  writecommand(0xB3); // CLOCKDIV
  writedata(0xF1);
  writecommand(0xCA); // MUXRATIO
  writedata(127);
  writecommand(0xA2); // DISPLAYOFFSET
  writedata(0x00);
  writecommand(0xB5); // SETGPIO
  writedata(0x00);
  writecommand(0xAB); // FUNCTIONSELECT
  writedata(0x01);
  writecommand(0xB1); // PRECHARGE
  writedata(0x32);
  writecommand(0xBE); // VCOMH
  writedata(0x05);
  writecommand(0xA6); // NORMALDISPLAY
  writecommand(0xC1); // CONTRASTABC
  writedata(0xC8);
  writedata(0x80);
  writedata(0xC8);
  writecommand(0xC7); // CONTRASTMASTER
  writedata(0x0F);
  writecommand(0xB4); // SETVSL
  writedata(0xA0);
  writedata(0xB5);
  writedata(0x55);
  writecommand(0xB6); // PRECHARGE2
  writedata(0x01);
  writecommand(0xAF); // DISPLAYON
  */
  			

			writeRegister16(0x10, 0x2F8E);            
            	writeRegister16(0x11, 0x000C);  
            	writeRegister16(0x07, 0x0021);           
            	writeRegister16(0x28, 0x0006);     
            	writeRegister16(0x28, 0x0005);     
            	writeRegister16(0x27, 0x057F);        
            	writeRegister16(0x29, 0x89A1);     
            	writeRegister16(0x00, 0x0001);     
            	//TFTLCD_DELAY16, 100,       
            	writeRegister16(0x29, 0x80B0);   
            	//TFTLCD_DELAY16, 30, 
            	writeRegister16(0x29, 0xFFFE);
            	writeRegister16(0x07, 0x0223);
            	//TFTLCD_DELAY16, 30, 
            	writeRegister16(0x07, 0x0233);
            	writeRegister16(0x01, 0x2183);
            	writeRegister16(0x03, 0x6830);
            	writeRegister16(0x2F, 0xFFFF);
            	writeRegister16(0x2C, 0x8000);
            	writeRegister16(0x27, 0x0570);
            	writeRegister16(0x02, 0x0300);
            	writeRegister16(0x0B, 0x580C);
            	writeRegister16(0x12, 0x0609);
            	writeRegister16(0x13, 0x3100); 
  
   
}
