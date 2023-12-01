"""
*****************************************************************************
Copyright 2023 Silicon Laboratories Inc. www.silabs.com
*****************************************************************************
SPDX-License-Identifier: Zlib

The licensor of this software is Silicon Laboratories Inc.

This software is provided \'as-is\', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

*****************************************************************************
# EXPERIMENTAL QUALITY
This code has not been formally tested and is provided as-is. It is not
suitable for production environments. In addition, this code will not be
maintained and there may be no bug maintenance planned for these resources.
Silicon Labs may update projects from time to time.
******************************************************************************
"""
# Import core modules
import board

# Import library modules
from adafruit_ble import BLERadio
from adafruit_ble.advertising.standard import Advertisement
from adafruit_ble.advertising.standard import ProvideServicesAdvertisement

# Import application modules
from Button import Button
from Led import Led
from Piezo import Piezo
from Rtttl import Rtttl
from Tick import Tick
from ImmediateAlertService import ImmediateAlertService

# Constants
ALERT_LEVEL_NONE   =    0
ALERT_LEVEL_MILD   =    1
ALERT_LEVEL_HIGH   =    2

TICK_MS_LEDS       =  100 # Interval for LED timer
TICK_MS_LOCATE     = 1000 # Interval for locate mode timer

# Application class - Find Me - Target and simple locator
class App():

    # Initialisation
    def __init__(self, debug):
        # Note debug setting
        self.debug = True
        if self.debug: print(f'Find Me - target and simple locator')
        # Not initialised
        self.on = False
        # Hardware
        self.hw = {}
        if board.board_id == "explorerkit_xg24_brd2703a":
            self.on              = True
            self.hw["btn_high"]  = Button(board.PB3, True, False) # BTN1
            self.hw["btn_mild"]  = Button(board.PB2, True, False) # BTN0
            self.hw["led_high"]  = Led(board.PA7, False, False) # LED1 
            self.hw["led_mild"]  = Led(board.PA4, False, False) # LED0
            self.hw["rtttl"]     = Rtttl(board.PA0, False) # MIKROE_PWM
        elif board.board_id == "devkit_xg24_brd2601b":
            self.on              = True
            self.hw["btn_high"]  = Button(board.PB3, True, False) # BTN1
            self.hw["btn_mild"]  = Button(board.PB2, True, False) # BTN0
            self.hw["led_high"]  = Led(board.PD2, True, False) # RED (PA4=GREEN) 
            self.hw["led_mild"]  = Led(board.PB0, True, False) # BLUE
            self.hw["rtttl"]    = Rtttl(board.PA7, False) # SPI_CS (header 10 - may clash with IMU) 
        elif board.board_id == "sparkfun_thingplus_matter_mgm240p_brd2704a":
            self.on              = True
            self.hw["btn_high"]  = Button(board.PB2, True, False) # (external)
            self.hw["btn_mild"]  = Button(board.PA4, True, False) # (external)
            self.hw["led_high"]  = Led(board.PB0, False, False) # (external)
            self.hw["led_mild"]  = Led(board.PA8, False, False) # BLUE (on board)
            self.hw["rtttl"]     = Rtttl(board.PC7, False)
          
        # Couldn't initialise ?
        if not self.on:
            if self.debug: print(f'ERROR: Unsupported board "{board.board_id}"')
        # Initialised ?
        else:
            if self.debug: print(f'INFO: Initialised board "{board.board_id}"')
            # Tick timers
            self.ticks = {}
            self.ticks["leds"]   = Tick("leds", TICK_MS_LEDS, True, False)
            self.ticks["locate"] = Tick("locate", 0, False, False)
            # Data
            self.data = {}
            self.data["leds_bit"] = 0b1
            self.data["leds_mask"] = 0b1111111111
            self.data["led_mask_high"] = 0b1
            self.data["led_mask_mild"] = 0b1
            self.data["tune_name_high"] = self.hw["rtttl"].load("knightrh:d=4,o=6,b=90:16d.5,32d#.5,32d.5,8a.5,16d.,32d#.,32d.,8a.5,16d.5,32d#.5,32d.5,16a.5,16d.,2c,16d.5,32d#.5,32d.5,8a.5,16d.,32d#.,32d.,8a.5,16d.5,32d#.5,32d.5,16a.5,16d.,2d#,a4,32a#.4,32a.4,d5,32d#.5,32d.5,2a5,16c.,16d.", False)
            self.data["tune_name_mild"] = self.hw["rtttl"].load("knightrl:d=4,o=5,b=125:16e,16p,16f,16e,16e,16p,16e,16e,16f,16e,16e,16e,16d#,16e,16e,16e,16e,16p,16f,16e,16e,16p,16f,16e,16f,16e,16e,16e,16d#,16e,16e,16e,16d,16p,16e,16d,16d,16p,16e,16d,16e,16d,16d,16d,16c,16d,16d,16d,16d,16p,16e,16d,16d,16p,16e,16d,16e,16d,16d,16d,16c,16d,16d,16d", False)
            # Bluetooth
            self.ble = {}
            self.ble["connected"] = False
            self.ble["alert_level"] = ALERT_LEVEL_NONE
            self.ble["radio"] = BLERadio()
            self.ble["ias"] = ImmediateAlertService()
            self.ble["tx_ad"] = ProvideServicesAdvertisement(self.ble["ias"])
            self.ble["tx_ad"].short_name = f'{self.ble["radio"].address_bytes[1]:02X}{self.ble["radio"].address_bytes[0]:02X} Find Me'
            self.ble["tx_ad"].connectable = True
            self.ble["locate_level"] = ALERT_LEVEL_NONE
            if self.debug: print(f'INFO: BLE short_name="{self.ble["tx_ad"].short_name}"')            

    # Main function (called repeatedly do not block)
    def main(self):

        # App is on ? 
        if self.on:

            # Read buttons
            self.hw["btn_high"].read()
            self.hw["btn_mild"].read()
            # Has a button been released ? 
            if self.hw["btn_high"].pressed or self.hw["btn_mild"].pressed:
                # Running as target ?
                if self.ble["locate_level"] == ALERT_LEVEL_NONE:
                    # Sounding alert ?
                    if self.ble["ias"].alert_level != ALERT_LEVEL_NONE:
                        if self.debug: print(f'INFO: Target mode alert cancelled locally')
                        # Update characteristic
                        self.ble["ias"].alert_level = ALERT_LEVEL_NONE
                    # High button released ?
                    elif self.hw["btn_high"].pressed:
                        if self.debug: print(f'INFO: Locate mode high')
                        # Go to locate level high
                        self.ble["locate_level"] = ALERT_LEVEL_HIGH
                    # Mild button released ?
                    elif self.hw["btn_mild"].pressed:
                        if self.debug: print(f'INFO: Locate mode mild')
                        # Go to locate level mild
                        self.ble["locate_level"] = ALERT_LEVEL_MILD 
                # Running as locator ? 
                else:
                    if self.debug: print(f'INFO: Target mode')
                    # Go to locate level none
                    self.ble["locate_level"] = ALERT_LEVEL_NONE 
                    # Stop locate timer
                    self.ticks["locate"].write(0, False)

            # Read tick timers
            self.ticks["leds"].read()
            self.ticks["locate"].read() 

            # Running as target ?
            if self.ble["locate_level"] == ALERT_LEVEL_NONE: 

                # Connected changed ?
                if self.ble["connected"] != self.ble["radio"].connected:
                    self.ble["connected"] = self.ble["radio"].connected
                    if self.debug:
                        if self.ble["connected"]: print(f'INFO: Target mode connected')
                        else: print(f'INFO: Target mode disconnected')

                # Not connected ?
                if not self.ble["radio"].connected:
                    # Not advertising ?
                    if not self.ble["radio"].advertising:
                        # Begin advertising
                        self.ble["radio"].start_advertising(self.ble["tx_ad"])
                        if self.debug: print(f'INFO: Target mode start advertising')
                # Connected ?
                else:
                    # Advertising ?
                    if self.ble["radio"].advertising:
                        # Stop advertising
                        self.ble["radio"].stop_advertising()
                        if self.debug: print(f'INFO: Target mode stop advertising')

                # Alert level changed ?
                if self.ble["alert_level"] != self.ble["ias"].alert_level:
                    self.ble["alert_level"] = self.ble["ias"].alert_level
                    # High alert ?
                    if self.ble["alert_level"] == ALERT_LEVEL_HIGH:
                        if self.debug: print(f'INFO: Target mode alert high')
                        # Update LED masks
                        self.data["led_mask_high"] = 0b0101010101
                        self.data["led_mask_mild"] = 0b0
                        # Update RTTTL
                        if self.hw["rtttl"].play_name != self.data["tune_name_high"]:
                            self.hw["rtttl"].play(self.data["tune_name_high"], True)
                    # Mild alert ?
                    elif self.ble["alert_level"] == ALERT_LEVEL_MILD:
                        if self.debug: print(f'INFO: Target mode alert mild')
                        # Update LED masks
                        self.data["led_mask_high"] = 0b0
                        self.data["led_mask_mild"] = 0b0101010101
                        # Update RTTTL
                        if self.hw["rtttl"].play_name != self.data["tune_name_mild"]:
                            self.hw["rtttl"].play(self.data["tune_name_mild"], True) 
                    # No alert ?                          
                    else:
                        if self.debug: print(f'INFO: Target mode alert none')
                        # Update LED masks
                        self.data["led_mask_high"] = 0b1
                        self.data["led_mask_mild"] = 0b1
                        # Stop RTTTL                    
                        if self.hw["rtttl"].play_name != None:
                            self.hw["rtttl"].stop() 

                # Drive rtttl
                self.hw["rtttl"].main() 

                # Led tick timer fired ?
                if self.ticks["leds"].fired:
                    # Safety check leds bit
                    if self.data["leds_bit"] & self.data["leds_mask"] == 0b0:
                        self.data["leds_bit"] = 0b1 
                    # Update led high
                    if self.data["leds_bit"] & self.data["led_mask_high"]:
                        self.hw["led_high"].write(True)
                    else:
                        self.hw["led_high"].write(False)
                    # Update led mild
                    if self.data["leds_bit"] & self.data["led_mask_mild"]:
                        self.hw["led_mild"].write(True)
                    else:
                        self.hw["led_mild"].write(False)                    
                    # Update leds bit
                    self.data["leds_bit"] <<= 1
              
            # Running as locator ?    
            else:

                # Advertising ?
                if self.ble["radio"].advertising:
                    # Stop advertising
                    self.ble["radio"].stop_advertising()
                    if self.debug: print(f'INFO: Locate mode stop advertising')

                # Stop RTTTL                    
                if self.hw["rtttl"].play_name != None:
                    self.hw["rtttl"].stop()
                # Drive rtttl
                self.hw["rtttl"].main()

                # Assume we won't locate
                locate = False
                # Locate timer not running ?
                if not self.ticks["locate"].on:
                    # Start repeating locate timer
                    self.ticks["locate"].write(TICK_MS_LOCATE, True)
                    # Locate
                    locate = True                    
                # Locate timer fired ?
                elif self.ticks["locate"].fired:
                    # Locate
                    locate = True

                # Time to locate ?
                if locate:

                    # Locating ?
                    if self.ble["locate_level"] == ALERT_LEVEL_MILD or self.ble["locate_level"] == ALERT_LEVEL_HIGH:

                        # Turn off LEDs during scan
                        self.hw["led_high"].write(False)
                        self.hw["led_mild"].write(False)

                        # Start scan
                        addresses = []
                        for ad in self.ble["radio"].start_scan(ProvideServicesAdvertisement, timeout=0.1):
                            # Immediate alert service in advertisement ?
                            if self.ble["ias"] in ad.services:
                                # Has a short name ?
                                if ad.short_name != None:
                                    # Shortname contains Find Me ?
                                    if ad.short_name.find("Find Me") > -1:
                                        # Not got this device yet ?
                                        if not ad.address in addresses:
                                            if self.debug: print(f'INFO: Locate mode found target address={ad.address}, short_name="{ad.short_name}"')
                                            # Add to list of addresses
                                            addresses.append(ad.address)
                        # Stop scan
                        self.ble["radio"].stop_scan()

                        # Alert level high ?        
                        if self.ble["locate_level"] == ALERT_LEVEL_HIGH:
                            # Turn on high LED
                            self.hw["led_high"].write(True)
                        # Alert level mild ?
                        else:
                            # Turn on high LED
                            self.hw["led_mild"].write(True)

                        # Loop through devices to be located
                        for address in addresses:
                            # Attempt to connect to device
                            connection = self.ble["radio"].connect(address, timeout=0.1)
                            if self.debug: print(f'INFO: Locate mode connect address={address}, connected={connection.connected}')
                            # Attempt to get service
                            try:
                                service = connection[ImmediateAlertService]
                            except:
                                if self.debug: print(f'WARNING: Locate mode could not find Immediate Alert Service address={address}')
                            else:
                                # Attempt to write alert level
                                try: 
                                    service.alert_level = self.ble["locate_level"]
                                except:
                                    if self.debug: print(f'WARNING: Locate mode could not write Alert Level address={address}, level={self.ble["locate_level"]}')
                                else:
                                    if self.debug: print(f'INFO: Locate mode Alert Level written address={address}, level={self.ble["locate_level"]}')
                            # Disconnect from device
                            connection.disconnect()
                            if self.debug: print(f'INFO: Locate mode disconnect address={address}')
                            # Wait for disconnection
                            while connection.connected:
                                pass

                    # Something went wrong ?
                    else:
                        if self.debug: print(f'INFO: Target mode')
                        # Go to locate level none
                        self.ble["locate_level"] = ALERT_LEVEL_NONE 
                        # Stop locate timer
                        self.ticks["locate"].write(0, False)

# Application class (END)