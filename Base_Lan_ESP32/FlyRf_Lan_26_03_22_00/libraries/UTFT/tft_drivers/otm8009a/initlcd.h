case OTM8009A:

  //============ OTM8009A+HSD3.97 20140613 ===============================================//
	
  //writeCommand16Transaction(0xff00);      writeData16Transaction(0x80);    //enable access command2
  //writeCommand16Transaction(0xff01);      writeData16Transaction(0x09);    //enable access command2
  //writeCommand16Transaction(0xff02);      writeData16Transaction(0x01);    //enable access command2
  //writeCommand16Transaction(0xff80);      writeData16Transaction(0x80);    //enable access command2
  //writeCommand16Transaction(0xff81);      writeData16Transaction(0x09);    //enable access command2
  //writeCommand16Transaction(0xff03);      writeData16Transaction(0x01);    //
  //writeCommand16Transaction(0xc5b1);      writeData16Transaction(0xA9);    //power control

  //writeCommand16Transaction(0xc591);      writeData16Transaction(0x0F);               //power control
  //writeCommand16Transaction(0xc0B4);      writeData16Transaction(0x50);

  ////panel driving mode : column inversion

  ////////  gamma
  //writeCommand16Transaction(0xE100);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xE101);      writeData16Transaction(0x09);
  //writeCommand16Transaction(0xE102);      writeData16Transaction(0x0F);
  //writeCommand16Transaction(0xE103);      writeData16Transaction(0x0E);
  //writeCommand16Transaction(0xE104);      writeData16Transaction(0x07);
  //writeCommand16Transaction(0xE105);      writeData16Transaction(0x10);
  //writeCommand16Transaction(0xE106);      writeData16Transaction(0x0B);
  //writeCommand16Transaction(0xE107);      writeData16Transaction(0x0A);
  //writeCommand16Transaction(0xE108);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xE109);      writeData16Transaction(0x07);
  //writeCommand16Transaction(0xE10A);      writeData16Transaction(0x0B);
  //writeCommand16Transaction(0xE10B);      writeData16Transaction(0x08);
  //writeCommand16Transaction(0xE10C);      writeData16Transaction(0x0F);
  //writeCommand16Transaction(0xE10D);      writeData16Transaction(0x10);
  //writeCommand16Transaction(0xE10E);      writeData16Transaction(0x0A);
  //writeCommand16Transaction(0xE10F);      writeData16Transaction(0x01);
  //writeCommand16Transaction(0xE200);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xE201);      writeData16Transaction(0x09);
  //writeCommand16Transaction(0xE202);      writeData16Transaction(0x0F);
  //writeCommand16Transaction(0xE203);      writeData16Transaction(0x0E);
  //writeCommand16Transaction(0xE204);      writeData16Transaction(0x07);
  //writeCommand16Transaction(0xE205);      writeData16Transaction(0x10);
  //writeCommand16Transaction(0xE206);      writeData16Transaction(0x0B);
  //writeCommand16Transaction(0xE207);      writeData16Transaction(0x0A);
  //writeCommand16Transaction(0xE208);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xE209);      writeData16Transaction(0x07);
  //writeCommand16Transaction(0xE20A);      writeData16Transaction(0x0B);
  //writeCommand16Transaction(0xE20B);      writeData16Transaction(0x08);
  //writeCommand16Transaction(0xE20C);      writeData16Transaction(0x0F);
  //writeCommand16Transaction(0xE20D);      writeData16Transaction(0x10);
  //writeCommand16Transaction(0xE20E);      writeData16Transaction(0x0A);
  //writeCommand16Transaction(0xE20F);      writeData16Transaction(0x01);
  //writeCommand16Transaction(0xD900);      writeData16Transaction(0x4E);    //vcom setting

  //writeCommand16Transaction(0xc181);      writeData16Transaction(0x66);    //osc=65HZ

  //writeCommand16Transaction(0xc1a1);      writeData16Transaction(0x08);
  //writeCommand16Transaction(0xc592);      writeData16Transaction(0x01);    //power control

  //writeCommand16Transaction(0xc595);      writeData16Transaction(0x34);    //power control

  //writeCommand16Transaction(0xd800);      writeData16Transaction(0x79);    //GVDD / NGVDD setting

  //writeCommand16Transaction(0xd801);      writeData16Transaction(0x79);    //GVDD / NGVDD setting

  //writeCommand16Transaction(0xc594);      writeData16Transaction(0x33);    //power control

  //writeCommand16Transaction(0xc0a3);      writeData16Transaction(0x1B);       //panel timing setting
  //writeCommand16Transaction(0xc582);      writeData16Transaction(0x83);    //power control

  //writeCommand16Transaction(0xc481);      writeData16Transaction(0x83);    //source driver setting

  //writeCommand16Transaction(0xc1a1);      writeData16Transaction(0x0E);
  //writeCommand16Transaction(0xb3a6);      writeData16Transaction(0x20);
  //writeCommand16Transaction(0xb3a7);      writeData16Transaction(0x01);
  //writeCommand16Transaction(0xce80);      writeData16Transaction(0x85);    // GOA VST

  //writeCommand16Transaction(0xce81);      writeData16Transaction(0x01);  // GOA VST

  //writeCommand16Transaction(0xce82);      writeData16Transaction(0x00);    // GOA VST

  //writeCommand16Transaction(0xce83);      writeData16Transaction(0x84);    // GOA VST
  //writeCommand16Transaction(0xce84);      writeData16Transaction(0x01);    // GOA VST
  //writeCommand16Transaction(0xce85);      writeData16Transaction(0x00);    // GOA VST

  //writeCommand16Transaction(0xcea0);      writeData16Transaction(0x18);    // GOA CLK
  //writeCommand16Transaction(0xcea1);      writeData16Transaction(0x04);    // GOA CLK
  //writeCommand16Transaction(0xcea2);      writeData16Transaction(0x03);    // GOA CLK
  //writeCommand16Transaction(0xcea3);      writeData16Transaction(0x39);    // GOA CLK
  //writeCommand16Transaction(0xcea4);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xcea5);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xcea6);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xcea7);      writeData16Transaction(0x18);    // GOA CLK
  //writeCommand16Transaction(0xcea8);      writeData16Transaction(0x03);    // GOA CLK
  //writeCommand16Transaction(0xcea9);      writeData16Transaction(0x03);    // GOA CLK
  //writeCommand16Transaction(0xceaa);      writeData16Transaction(0x3a);    // GOA CLK
  //writeCommand16Transaction(0xceab);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xceac);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xcead);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xceb0);      writeData16Transaction(0x18);    // GOA CLK
  //writeCommand16Transaction(0xceb1);      writeData16Transaction(0x02);    // GOA CLK
  //writeCommand16Transaction(0xceb2);      writeData16Transaction(0x03);    // GOA CLK
  //writeCommand16Transaction(0xceb3);      writeData16Transaction(0x3b);    // GOA CLK
  //writeCommand16Transaction(0xceb4);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xceb5);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xceb6);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xceb7);      writeData16Transaction(0x18);    // GOA CLK
  //writeCommand16Transaction(0xceb8);      writeData16Transaction(0x01);    // GOA CLK
  //writeCommand16Transaction(0xceb9);      writeData16Transaction(0x03);    // GOA CLK
  //writeCommand16Transaction(0xceba);      writeData16Transaction(0x3c);    // GOA CLK
  //writeCommand16Transaction(0xcebb);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xcebc);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xcebd);      writeData16Transaction(0x00);    // GOA CLK
  //writeCommand16Transaction(0xcfc0);      writeData16Transaction(0x01);    // GOA ECLK
  //writeCommand16Transaction(0xcfc1);      writeData16Transaction(0x01);    // GOA ECLK
  //writeCommand16Transaction(0xcfc2);      writeData16Transaction(0x20);    // GOA ECLK

  //writeCommand16Transaction(0xcfc3);      writeData16Transaction(0x20);    // GOA ECLK

  //writeCommand16Transaction(0xcfc4);      writeData16Transaction(0x00);    // GOA ECLK

  //writeCommand16Transaction(0xcfc5);      writeData16Transaction(0x00);    // GOA ECLK

  //writeCommand16Transaction(0xcfc6);      writeData16Transaction(0x01);    // GOA other options

  //writeCommand16Transaction(0xcfc7);      writeData16Transaction(0x00);

  //// GOA signal toggle option setting

  //writeCommand16Transaction(0xcfc8);      writeData16Transaction(0x00);    //GOA signal toggle option setting
  //writeCommand16Transaction(0xcfc9);      writeData16Transaction(0x00);

  ////GOA signal toggle option setting

  //writeCommand16Transaction(0xcfd0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb80);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb81);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb82);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb83);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb84);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb85);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb86);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb87);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb88);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb89);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb90);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb91);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb92);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb93);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb94);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb95);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb96);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb97);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb98);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb99);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb9a);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb9b);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb9c);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb9d);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcb9e);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba1);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba2);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba3);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba4);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba5);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcba9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbaa);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbab);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbac);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbad);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbae);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb1);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb2);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb3);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb4);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb5);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbb9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbc0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbc1);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbc2);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbc3);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbc4);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbc5);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbc6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbc7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbc8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbc9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbca);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbcb);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbcc);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbcd);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbce);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbd0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbd1);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbd2);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbd3);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbd4);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbd5);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbd6);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbd7);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbd8);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbd9);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbda);      writeData16Transaction(0x04);
  //writeCommand16Transaction(0xcbdb);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbdc);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbdd);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbde);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe1);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe2);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe3);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe4);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe5);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbe9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcbf0);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf1);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf2);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf3);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf4);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf5);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf6);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf7);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf8);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcbf9);      writeData16Transaction(0xFF);
  //writeCommand16Transaction(0xcc80);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc81);      writeData16Transaction(0x26);
  //writeCommand16Transaction(0xcc82);      writeData16Transaction(0x09);
  //writeCommand16Transaction(0xcc83);      writeData16Transaction(0x0B);
  //writeCommand16Transaction(0xcc84);      writeData16Transaction(0x01);
  //writeCommand16Transaction(0xcc85);      writeData16Transaction(0x25);
  //writeCommand16Transaction(0xcc86);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc87);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc88);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc89);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc90);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc91);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc92);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc93);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc94);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc95);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc96);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc97);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc98);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc99);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc9a);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcc9b);      writeData16Transaction(0x26);
  //writeCommand16Transaction(0xcc9c);      writeData16Transaction(0x0A);
  //writeCommand16Transaction(0xcc9d);      writeData16Transaction(0x0C);
  //writeCommand16Transaction(0xcc9e);      writeData16Transaction(0x02);
  //writeCommand16Transaction(0xcca0);      writeData16Transaction(0x25);
  //writeCommand16Transaction(0xcca1);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca2);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca3);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca4);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca5);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcca9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccaa);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccab);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccac);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccad);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccae);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccb0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccb1);      writeData16Transaction(0x25);
  //writeCommand16Transaction(0xccb2);      writeData16Transaction(0x0C);
  //writeCommand16Transaction(0xccb3);      writeData16Transaction(0x0A);
  //writeCommand16Transaction(0xccb4);      writeData16Transaction(0x02);
  //writeCommand16Transaction(0xccb5);      writeData16Transaction(0x26);
  //writeCommand16Transaction(0xccb6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccb7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccb8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccb9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc0);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc1);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc2);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc3);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc4);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc5);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccc9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccca);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xcccb);      writeData16Transaction(0x25);
  //writeCommand16Transaction(0xcccc);      writeData16Transaction(0x0B);
  //writeCommand16Transaction(0xcccd);      writeData16Transaction(0x09);
  //writeCommand16Transaction(0xccce);      writeData16Transaction(0x01);
  //writeCommand16Transaction(0xccd0);      writeData16Transaction(0x26);
  //writeCommand16Transaction(0xccd1);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd2);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd3);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd4);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd5);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd6);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd7);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd8);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccd9);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccda);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccdb);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccdc);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccdd);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0xccde);      writeData16Transaction(0x00);
  //writeCommand16Transaction(0x3A00);      writeData16Transaction(0x55);

  //writeCommand16Transaction(0x1100);
  //delay(100);
  //writeCommand16Transaction(0x2900);
  //delay(50);
  //writeCommand16Transaction(0x2C00);
  //writeCommand16Transaction(0x2A00);     writeData16Transaction(0x00);
  //writeCommand16Transaction(0x2A01);     writeData16Transaction(0x00);
  //writeCommand16Transaction(0x2A02);     writeData16Transaction(0x01);
  //writeCommand16Transaction(0x2A03);     writeData16Transaction(0xe0);
  //writeCommand16Transaction(0x2B00);     writeData16Transaction(0x00);
  //writeCommand16Transaction(0x2B01);     writeData16Transaction(0x00);
  //writeCommand16Transaction(0x2B02);     writeData16Transaction(0x03);
  //writeCommand16Transaction(0x2B03);     writeData16Transaction(0x20);
  
  //**********************************************************************

// Вариант 3.97inch OTM8009 Init 20190116
delay(100);
  writeCommand16Transaction(0xff00);
  writeData16Transaction(0x80);
  writeCommand16Transaction(0xff01);
  writeData16Transaction(0x09);
  writeCommand16Transaction(0xff02);
  writeData16Transaction(0x01);

  writeCommand16Transaction(0xff80);
  writeData16Transaction(0x80);
  writeCommand16Transaction(0xff81);
  writeData16Transaction(0x09);

  writeCommand16Transaction(0xff03);
  writeData16Transaction(0x01);

  //add ==========20131216============================//
  writeCommand16Transaction(0xf5b6);
  writeData16Transaction(0x06);
  writeCommand16Transaction(0xc480);
  writeData16Transaction(0x30);
  writeCommand16Transaction(0xc48a);
  writeData16Transaction(0x40);
  //===================================================//
  writeCommand16Transaction(0xc0a3);
  writeData16Transaction(0x1B);

  //writeCommand16Transaction(0xc0ba);  //No
  //writeData16Transaction(0x50);

  writeCommand16Transaction(0xc0ba); //--> (0xc0b4); // column inversion //  2013.12.16 modify
  writeData16Transaction(0x50);

  writeCommand16Transaction(0xc181);
  writeData16Transaction(0x66);

  writeCommand16Transaction(0xc1a1);
  writeData16Transaction(0x0E);

  writeCommand16Transaction(0xc481);
  writeData16Transaction(0x83);

  writeCommand16Transaction(0xc582);
  writeData16Transaction(0x83);

  writeCommand16Transaction(0xc590);
  writeData16Transaction(0x96);

  writeCommand16Transaction(0xc591);
  writeData16Transaction(0x2B);

  writeCommand16Transaction(0xc592);
  writeData16Transaction(0x01);


  writeCommand16Transaction(0xc594);
  writeData16Transaction(0x33);

  writeCommand16Transaction(0xc595);
  writeData16Transaction(0x34);


  writeCommand16Transaction(0xc5b1);
  writeData16Transaction(0xa9);

  writeCommand16Transaction(0xce80);
  writeData16Transaction(0x86);
  writeCommand16Transaction(0xce81);
  writeData16Transaction(0x01);
  writeCommand16Transaction(0xce82);
  writeData16Transaction(0x00);

  writeCommand16Transaction(0xce83);
  writeData16Transaction(0x85);
  writeCommand16Transaction(0xce84);
  writeData16Transaction(0x01);
  writeCommand16Transaction(0xce85);
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xce86);
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xce87);
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xce88);
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xce89);
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xce8A);
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xce8B);
  writeData16Transaction(0x00);

  writeCommand16Transaction(0xcea0);// cea1[7:0] : clka1_width[3:0], clka1_shift[11:8]                         
  writeData16Transaction(0x18);
  writeCommand16Transaction(0xcea1);// cea2[7:0] : clka1_shift[7:0]                                            
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcea2);// cea3[7:0] : clka1_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]   
  writeData16Transaction(0x03);
  writeCommand16Transaction(0xcea3);// cea4[7:0] : clka1_switch[7:0]                                               
  writeData16Transaction(0x21);
  writeCommand16Transaction(0xcea4);// cea5[7:0] : clka1_extend[7:0]                                           
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcea5);// cea6[7:0] : clka1_tchop[7:0]                                            
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcea6);// cea7[7:0] : clka1_tglue[7:0]                                            
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcea7);// cea8[7:0] : clka2_width[3:0], clka2_shift[11:8]                         
  writeData16Transaction(0x18);
  writeCommand16Transaction(0xcea8);// cea9[7:0] : clka2_shift[7:0]                                            
  writeData16Transaction(0x03);
  writeCommand16Transaction(0xcea9);// ceaa[7:0] : clka2_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]   
  writeData16Transaction(0x03);
  writeCommand16Transaction(0xceaa);// ceab[7:0] : clka2_switch[7:0]                                                
  writeData16Transaction(0x22);
  writeCommand16Transaction(0xceab);// ceac[7:0] : clka2_extend                                                
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xceac);// cead[7:0] : clka2_tchop                                                 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcead);// ceae[7:0] : clka2_tglue 
  writeData16Transaction(0x00);

  writeCommand16Transaction(0xceb0);// ceb1[7:0] : clka3_width[3:0], clka3_shift[11:8]                          
  writeData16Transaction(0x18);
  writeCommand16Transaction(0xceb1);// ceb2[7:0] : clka3_shift[7:0]                                             
  writeData16Transaction(0x02);
  writeCommand16Transaction(0xceb2);// ceb3[7:0] : clka3_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]    
  writeData16Transaction(0x03);
  writeCommand16Transaction(0xceb3);// ceb4[7:0] : clka3_switch[7:0]                                               
  writeData16Transaction(0x23);
  writeCommand16Transaction(0xceb4);// ceb5[7:0] : clka3_extend[7:0]                                            
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xceb5);// ceb6[7:0] : clka3_tchop[7:0]                                             
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xceb6);// ceb7[7:0] : clka3_tglue[7:0]                                             
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xceb7);// ceb8[7:0] : clka4_width[3:0], clka2_shift[11:8]                          
  writeData16Transaction(0x18);
  writeCommand16Transaction(0xceb8);// ceb9[7:0] : clka4_shift[7:0]                                             
  writeData16Transaction(0x01);
  writeCommand16Transaction(0xceb9);// ceba[7:0] : clka4_sw_tg, odd_high, flat_head, flat_tail, switch[11:8]    
  writeData16Transaction(0x03);
  writeCommand16Transaction(0xceba);// cebb[7:0] : clka4_switch[7:0]                                                
  writeData16Transaction(0x24);
  writeCommand16Transaction(0xcebb);// cebc[7:0] : clka4_extend                                                 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcebc);// cebd[7:0] : clka4_tchop                                                  
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcebd);// cebe[7:0] : clka4_tglue                                                  
  writeData16Transaction(0x00);


  writeCommand16Transaction(0xcfc0);// cfc1[7:0] : eclk_normal_width[7:0]   
  writeData16Transaction(0x01);
  writeCommand16Transaction(0xcfc1);// cfc2[7:0] : eclk_partial_width[7:0]                                                                                  
  writeData16Transaction(0x01);
  writeCommand16Transaction(0xcfc2);// cfc3[7:0] : all_normal_tchop[7:0]                                                                                    
  writeData16Transaction(0x20);
  writeCommand16Transaction(0xcfc3);// cfc4[7:0] : all_partial_tchop[7:0]                                                                                   
  writeData16Transaction(0x20);
  writeCommand16Transaction(0xcfc4);// cfc5[7:0] : eclk1_follow[3:0], eclk2_follow[3:0]                                                                     
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcfc5);// cfc6[7:0] : eclk3_follow[3:0], eclk4_follow[3:0]                                                                     
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcfc6);// cfc7[7:0] : 00, vstmask, vendmask, 00, dir1, dir2 (0=VGL, 1=VGH)                                                     
  writeData16Transaction(0x01);
  writeCommand16Transaction(0xcfc7);// cfc8[7:0] : reg_goa_gnd_opt, reg_goa_dpgm_tail_set, reg_goa_f_gating_en, reg_goa_f_odd_gating, toggle_mod1, 2, 3, 4  
  writeData16Transaction(0x00);    // GND OPT1 (00-->80  2011/10/28)
  writeCommand16Transaction(0xcfc8);// cfc9[7:0] : duty_block[3:0], DGPM[3:0]                                                                               
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcfc9);// cfca[7:0] : reg_goa_gnd_period[7:0]                                                                                  
  writeData16Transaction(0x00);    // Gate PCH (CLK base) (00-->0a  2011/10/28)

  writeCommand16Transaction(0xcfd0);// cfd1[7:0] : 0000000, reg_goa_frame_odd_high
  writeData16Transaction(0x00);

  writeCommand16Transaction(0xcbc0);//cbc1[7:0] : enmode H-byte of sig1  (pwrof_0, pwrof_1, norm, pwron_4 )           
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbc1);//cbc2[7:0] : enmode H-byte of sig2  (pwrof_0, pwrof_1, norm, pwron_4 )          
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbc2);//cbc3[7:0] : enmode H-byte of sig3  (pwrof_0, pwrof_1, norm, pwron_4 )           
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbc3);//cbc4[7:0] : enmode H-byte of sig4  (pwrof_0, pwrof_1, norm, pwron_4 )        
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbc4);//cbc5[7:0] : enmode H-byte of sig5  (pwrof_0, pwrof_1, norm, pwron_4 )             
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbc5);//cbc6[7:0] : enmode H-byte of sig6  (pwrof_0, pwrof_1, norm, pwron_4 )           
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbc6);//cbc7[7:0] : enmode H-byte of sig7  (pwrof_0, pwrof_1, norm, pwron_4 )           
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbc7);//cbc8[7:0] : enmode H-byte of sig8  (pwrof_0, pwrof_1, norm, pwron_4 )           
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbc8);//cbc9[7:0] : enmode H-byte of sig9  (pwrof_0, pwrof_1, norm, pwron_4 )           
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbc9);//cbca[7:0] : enmode H-byte of sig10 (pwrof_0, pwrof_1, norm, pwron_4 )        
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbca);//cbcb[7:0] : enmode H-byte of sig11 (pwrof_0, pwrof_1, norm, pwron_4 )        
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbcb);//cbcc[7:0] : enmode H-byte of sig12 (pwrof_0, pwrof_1, norm, pwron_4 )        
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbcc);//cbcd[7:0] : enmode H-byte of sig13 (pwrof_0, pwrof_1, norm, pwron_4 )        
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbcd);//cbce[7:0] : enmode H-byte of sig14 (pwrof_0, pwrof_1, norm, pwron_4 ) 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbce);//cbcf[7:0] : enmode H-byte of sig15 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);

  writeCommand16Transaction(0xcbd0);//cbd1[7:0] : enmode H-byte of sig16 (pwrof_0, pwrof_1, norm, pwron_4 )           
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbd1);//cbd2[7:0] : enmode H-byte of sig17 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbd2);//cbd3[7:0] : enmode H-byte of sig18 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbd3);//cbd4[7:0] : enmode H-byte of sig19 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbd4);//cbd5[7:0] : enmode H-byte of sig20 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbd5);//cbd6[7:0] : enmode H-byte of sig21 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbd6);//cbd7[7:0] : enmode H-byte of sig22 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbd7);//cbd8[7:0] : enmode H-byte of sig23 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbd8);//cbd9[7:0] : enmode H-byte of sig24 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbd9);//cbda[7:0] : enmode H-byte of sig25 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbda);//cbdb[7:0] : enmode H-byte of sig26 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x04);
  writeCommand16Transaction(0xcbdb);//cbdc[7:0] : enmode H-byte of sig27 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbdc);//cbdd[7:0] : enmode H-byte of sig28 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbdd);//cbde[7:0] : enmode H-byte of sig29 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbde);//cbdf[7:0] : enmode H-byte of sig30 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);

  writeCommand16Transaction(0xcbe0);//cbe1[7:0] : enmode H-byte of sig31 (pwrof_0, pwrof_1, norm, pwron_4 )             
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe1);//cbe2[7:0] : enmode H-byte of sig32 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe2);//cbe3[7:0] : enmode H-byte of sig33 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe3);//cbe4[7:0] : enmode H-byte of sig34 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe4);//cbe5[7:0] : enmode H-byte of sig35 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe5);//cbe6[7:0] : enmode H-byte of sig36 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe6);//cbe7[7:0] : enmode H-byte of sig37 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe7);//cbe8[7:0] : enmode H-byte of sig38 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe8);//cbe9[7:0] : enmode H-byte of sig39 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcbe9);//cbea[7:0] : enmode H-byte of sig40 (pwrof_0, pwrof_1, norm, pwron_4 )
  writeData16Transaction(0x00);

  // cc8x   
  writeCommand16Transaction(0xcc80);//cc81[7:0] : reg setting for signal01 selection with u2d mode   
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc81);//cc82[7:0] : reg setting for signal02 selection with u2d mode 
  writeData16Transaction(0x26);
  writeCommand16Transaction(0xcc82);//cc83[7:0] : reg setting for signal03 selection with u2d mode 
  writeData16Transaction(0x09);
  writeCommand16Transaction(0xcc83);//cc84[7:0] : reg setting for signal04 selection with u2d mode 
  writeData16Transaction(0x0B);
  writeCommand16Transaction(0xcc84);//cc85[7:0] : reg setting for signal05 selection with u2d mode 
  writeData16Transaction(0x01);
  writeCommand16Transaction(0xcc85);//cc86[7:0] : reg setting for signal06 selection with u2d mode 
  writeData16Transaction(0x25);
  writeCommand16Transaction(0xcc86);//cc87[7:0] : reg setting for signal07 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc87);//cc88[7:0] : reg setting for signal08 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc88);//cc89[7:0] : reg setting for signal09 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc89);//cc8a[7:0] : reg setting for signal10 selection with u2d mode 
  writeData16Transaction(0x00);

  // cc9x   
  writeCommand16Transaction(0xcc90);//cc91[7:0] : reg setting for signal11 selection with u2d mode   
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc91);//cc92[7:0] : reg setting for signal12 selection with u2d mode
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc92);//cc93[7:0] : reg setting for signal13 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc93);//cc94[7:0] : reg setting for signal14 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc94);//cc95[7:0] : reg setting for signal15 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc95);//cc96[7:0] : reg setting for signal16 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc96);//cc97[7:0] : reg setting for signal17 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc97);//cc98[7:0] : reg setting for signal18 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc98);//cc99[7:0] : reg setting for signal19 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc99);//cc9a[7:0] : reg setting for signal20 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc9a);//cc9b[7:0] : reg setting for signal21 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcc9b);//cc9c[7:0] : reg setting for signal22 selection with u2d mode 
  writeData16Transaction(0x26);
  writeCommand16Transaction(0xcc9c);//cc9d[7:0] : reg setting for signal23 selection with u2d mode 
  writeData16Transaction(0x0A);
  writeCommand16Transaction(0xcc9d);//cc9e[7:0] : reg setting for signal24 selection with u2d mode 
  writeData16Transaction(0x0C);
  writeCommand16Transaction(0xcc9e);//cc9f[7:0] : reg setting for signal25 selection with u2d mode 
  writeData16Transaction(0x02);
  // ccax   
  writeCommand16Transaction(0xcca0);//cca1[7:0] : reg setting for signal26 selection with u2d mode   
  writeData16Transaction(0x25);
  writeCommand16Transaction(0xcca1);//cca2[7:0] : reg setting for signal27 selection with u2d mode
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca2);//cca3[7:0] : reg setting for signal28 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca3);//cca4[7:0] : reg setting for signal29 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca4);//cca5[7:0] : reg setting for signal20 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca5);//cca6[7:0] : reg setting for signal31 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca6);//cca7[7:0] : reg setting for signal32 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca7);//cca8[7:0] : reg setting for signal33 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca8);//cca9[7:0] : reg setting for signal34 selection with u2d mode 
  writeData16Transaction(0x00);
  writeCommand16Transaction(0xcca9);//ccaa[7:0] : reg setting for signal35 selection with u2d mode 
  writeData16Transaction(0x00);

  writeCommand16Transaction(0x3A00);//ccaa[7:0] : reg setting for signal35 selection with u2d mode 
  writeData16Transaction(0x55);//0x55

  writeCommand16Transaction(0x1100);
  delay(100);
  writeCommand16Transaction(0x2900);
  delay(50);
  writeCommand16Transaction(0x2C00);


	break;
