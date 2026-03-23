case NT35510:

  writeCommand16(0x2A00);
  writeData16(x1 >> 8);
  writeCommand16(0x2A01);
  writeData16(x1 & 0x00ff);
  writeCommand16(0x2A02);
  writeData16(x2 >> 8);
  writeCommand16(0x2A03);
  writeData16(x2 & 0x00ff);
  writeCommand16(0x2B00);
  writeData16(y1 >> 8);
  writeCommand16(0x2B01);
  writeData16(y1 & 0x00ff);
  writeCommand16(0x2B02);
  writeData16(y2 >> 8);
  writeCommand16(0x2B03);
  writeData16(y2 & 0x00ff);
  writeCommand16(0x2C00);
	
	break;
