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

# Import application modules
from Button import Button
from Led import Led
from Piezo import Piezo
from Rtttl import Rtttl
from Tick import Tick

# Constants
TICK_MS_LEDS       =  100 # Interval for LED timer

# Application class - Find Me - Target
class App():

    # Initialisation
    def __init__(self, debug):
        # Note debug setting
        self.debug = True
        if self.debug: print(f'Find Me - hardware')
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
            # Data
            self.data = {}
            self.data["leds_bit"] = 0b1
            self.data["leds_mask"] = 0b1111111111
            self.data["led_mask_high"] = 0b1
            self.data["led_mask_mild"] = 0b1
            self.data["tune_name_high"] = self.hw["rtttl"].load("knightrh:d=4,o=6,b=90:16d.5,32d#.5,32d.5,8a.5,16d.,32d#.,32d.,8a.5,16d.5,32d#.5,32d.5,16a.5,16d.,2c,16d.5,32d#.5,32d.5,8a.5,16d.,32d#.,32d.,8a.5,16d.5,32d#.5,32d.5,16a.5,16d.,2d#,a4,32a#.4,32a.4,d5,32d#.5,32d.5,2a5,16c.,16d.", False)
            self.data["tune_name_mild"] = self.hw["rtttl"].load("knightrl:d=4,o=5,b=125:16e,16p,16f,16e,16e,16p,16e,16e,16f,16e,16e,16e,16d#,16e,16e,16e,16e,16p,16f,16e,16e,16p,16f,16e,16f,16e,16e,16e,16d#,16e,16e,16e,16d,16p,16e,16d,16d,16p,16e,16d,16e,16d,16d,16d,16c,16d,16d,16d,16d,16p,16e,16d,16d,16p,16e,16d,16e,16d,16d,16d,16c,16d,16d,16d", False)
       
    # Main function (called repeatedly do not block)
    def main(self):

        # App is on ? 
        if self.on:

            # Read buttons
            self.hw["btn_high"].read()
            self.hw["btn_mild"].read()
            
            # Mild button released ? 
            if self.hw["btn_mild"].pressed:
                # Not playing mild tune ? 
                if self.hw["rtttl"].play_name != self.data["tune_name_mild"]:
                    # Play mild tune
                    self.hw["rtttl"].play(self.data["tune_name_mild"], True)
                    # Flash mild LED rapidly
                    self.data["led_mask_mild"] = 0b0101010101
                    # Turn off high LED
                    self.data["led_mask_high"] = 0b0     
                # Playing high tune ?
                else:
                    # Stop playing
                    self.hw["rtttl"].stop()
                    # Flash LEDs intermitently
                    self.data["led_mask_high"] = 0b1
                    # Turn off mild LED
                    self.data["led_mask_mild"] = 0b1 

            # High button released ? 
            if self.hw["btn_high"].pressed:
                # Not playing high tune ? 
                if self.hw["rtttl"].play_name != self.data["tune_name_high"]:
                    # Play high tune
                    self.hw["rtttl"].play(self.data["tune_name_high"], True)
                    # Flash high LED rapidly
                    self.data["led_mask_high"] = 0b0101010101
                    # Turn off mild LED
                    self.data["led_mask_mild"] = 0b0     
                # Playing high tune ?
                else:
                    # Stop playing
                    self.hw["rtttl"].stop()
                    # Flash LEDs intermitently
                    self.data["led_mask_high"] = 0b1
                    # Turn off mild LED
                    self.data["led_mask_mild"] = 0b1 
            
            # Drive rtttl
            self.hw["rtttl"].main() 
            
            # Read tick timers
            self.ticks["leds"].read()

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

# Application class (END)