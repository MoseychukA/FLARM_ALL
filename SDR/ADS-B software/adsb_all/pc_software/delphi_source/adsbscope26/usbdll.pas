
{*********************************************************************
 * FileName:        usbdll.pas
 * Dependencies:    See uses section below
 * System:          Win32 (WinXP)
 * Compiler:        Delphi 5
 * Company:         sprut
 * Copyright:       2007-2012 Joerg Bredendiek (sprut)
 * Homepage :       www.sprut.de
 *
 ********************************************************************}

{*********************************************************************
 * part of adsbScope
 * software to visualize adsb-data
 * this is the interface to the MCD-USB-driver
 * at the moment i dont need this at all
 * in future it may be used to support the bootloader-function of my decoder
 *********************************************************************}

{*  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *}

unit usbdll;

interface

uses
  Windows, Dialogs, Sysutils;

//based on http://www.sixca.com/delphi/article/microchip_usb.html

Const
  MAXSIZE           = 64;
  MPUSB_FAIL        = 0;
  MPUSB_SUCCESS     = 1;
  MP_WRITE:DWORD    = 0;
  MP_READ:DWORD     = 1;
  MAX_NUM_MPUSB_DEV = 127;

  READ_VERSION    = $00;
  // konstanten für bootloader
  READ_FLASH      = $01;
  WRITE_FLASH     = $02;
  ERASE_FLASH     = $03;
  READ_EEDATA     = $04;
  WRITE_EEDATA    = $05;
  READ_CONFIG     = $06;
  WRITE_CONFIG    = $07;
  UPDATE_LED      = $32;
  RESET_DEVICE    = $FF;


type
  DWORD  = LongInt;
  PCHAR8 = array[0..MAXSIZE] of char;
  PBYTE  = array[0..MAXSIZE] of BYTE;
  PDWORD = array[0..MAXSIZE] of DWORD;
  PVOID  = Pointer;
  UINT   = Cardinal;


var
//  vid_pid     : PCHAR8 = 'vid_04d8&pid_000c';
  vid_pid     : PCHAR8 = 'vid_04d8&pid_ff0b';
  out_pipe    : PCHAR8 = '\MCHP_EP1';
  in_pipe     : PCHAR8 = '\MCHP_EP1';
  myOutPipe   : THANDLE;
  myInPipe    : THANDLE;
  isConnected : boolean;
  send_buf    : PBYTE;
  receive_buf : PBYTE;


// Version der mpusbapi.dll auslesen
function _MPUSBGetDLLVersion():DWORD; 
             stdcall;external 'mpusbapi.dll';

// Anzahl der zu vid&pid passenden USB-Devices
function _MPUSBGetDeviceCount(pVID_PID:PCHAR8):DWORD; 
             stdcall;external 'mpusbapi.dll';

function _MPUSBOpen(instance:DWORD;pVID_PID:PCHAR8;
             pEP:PCHAR8;dwDir:DWORD;dwReserved:DWORD):
             THANDLE;stdcall;external 'mpusbapi.dll';

function _MPUSBClose(handle:THANDLE):DWORD; 
             stdcall;external 'mpusbapi.dll';

function _MPUSBRead(handle:THANDLE;var pData:PBYTE;
             dwLen:DWORD;var pLength:DWORD;
             dwMilliseconds:DWORD):DWORD;stdcall;
             external 'mpusbapi.dll';

function _MPUSBReadInt(handle:THANDLE;
             var pData:PBYTE;dwLen:DWORD;
             var pLength:PDWORD;
             dwMilliseconds:DWORD):DWORD; 
             stdcall;external 'mpusbapi.dll';

function _MPUSBWrite(handle:THANDLE;pData:PBYTE;
             dwLen:DWORD;
             var pLength:DWORD;
             dwMilliseconds:DWORD):DWORD; 
             stdcall;external 'mpusbapi.dll';

function SendReceivePacket(SendData:PBYTE;SendLength:DWORD;var ReceiveData:PBYTE;
         var ReceiveLength:DWORD;SendDelay:UINT;ReceiveDelay:UINT):DWORD; stdcall;
         
implementation


procedure CheckInvalidHandle();
begin
  if(GetLastError=ERROR_INVALID_HANDLE) then
  begin
        _MPUSBClose(myOutPipe);
        _MPUSBClose(myInPipe);

        myInPipe:=INVALID_HANDLE_VALUE;
        myOutPipe:=INVALID_HANDLE_VALUE;
  end
  else
    ShowMessage('Error Code :'+inttostr(GetLastError()));
end;  //CheckInvalidHandle


function SendReceivePacket(SendData:PBYTE;SendLength:DWORD;var ReceiveData:PBYTE;
         var ReceiveLength:DWORD;SendDelay:UINT;ReceiveDelay:UINT):DWORD; stdcall;
var
  SentDataLength:DWORD ;
  ExpectedReceiveLength:DWORD;
begin
// Fehlercodes:
//   0 -   keine outpipe bekommen
//   1 -   alles ok
//   2 -   falsche Anzahl empfangen
// 100 -   schreiben oder lesen ging nicht
  result:=100;
  ExpectedReceiveLength:= ReceiveLength;
  if((myOutPipe <> INVALID_HANDLE_VALUE) and (myInPipe <> INVALID_HANDLE_VALUE)) then begin
    if(_MPUSBWrite(myOutPipe,SendData,SendLength,SentDataLength,SendDelay)<>0) then

    if(_MPUSBRead(myInPipe,ReceiveData,ExpectedReceiveLength,ReceiveLength,ReceiveDelay)<>0)then begin

      if(ReceiveLength = ExpectedReceiveLength) then begin
        Result:=1; // Success
        exit;
      end else //ReceiveLength
      if(ReceiveLength < ExpectedReceiveLength) then begin
        Result:=2;     // incorrect receive length
        exit;
      end

    end else          //_MPUSBRead
    CheckInvalidHandle()
    else              //_MPUSBWrite
    CheckInvalidHandle()
  end else begin      //myOutPipe
    Result:=0;        // Failed
  end;                //myOutPipe
end;  //SendReceivePacket


initialization


end.
 