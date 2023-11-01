{*********************************************************************
 * FileName:        gpx.pas
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
 * this is the network parameter setup form
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

unit Ugpx;

//routinen fuer GPX-overlay
{
Grundstruktur
<?xml version="1.0" encoding="utf-8"?>
<gpx version="1.1" creator="Ersteller der Datei">
  <metadata> <!-- Metadaten --> </metadata>
  <wpt lat="xx.xxx" lon="yy.yyy"> <!-- Attribute des Wegpunkts --> </wpt>
  <!-- weitere Wegpunkte -->
  <rte>
    <!-- Attribute der Route -->
    <rtept lat="xx.xxx" lon="yy.yyy"> <!-- Attribute des Routenpunkts --> </rtept>
    <!-- weitere Routenpunkte -->
  </rte>
  <!-- weitere Routen -->
  <trk>
    <!-- Attribute des Tracks -->
    <trkseg>
      <trkpt lat="xx.xxx" lon="yy.yyy"> <!-- Attribute des Trackpunkts --> </trkpt>
      <!-- weitere Trackpunkte -->
    </trkseg>
    <!-- weitere Track-Segmente -->
  </trk>
  <!-- weitere Tracks -->
</gpx>
}


interface

implementation

//****  G P X - I M P O R T ****************************************************


end.
