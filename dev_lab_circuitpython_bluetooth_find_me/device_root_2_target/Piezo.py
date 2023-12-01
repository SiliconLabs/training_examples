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
import pwmio
import atexit

# Piezo class
class Piezo():

    # Initialisation
    def __init__(self, pin, debug):
        # Initialise
        self.debug = debug
        if self.debug: print(f'Piezo.init({pin}, {debug})')
        self.pin = pin
        self.pwmio = pwmio.PWMOut(pin, variable_frequency=True)
        atexit.register(self.deinit)
        self.on = False
        self.frequency = 0

    # Write function
    def write(self, frequency):
        # Valid frequency C0 to B8 ?
        if frequency >= 16 and frequency <= 7902: 
            self.on = True
            self.frequency = frequency
        else:
            self.on = False
            self.frequency = 0
        if self.on: 
            self.pwmio.frequency = self.frequency
            self.pwmio.duty_cycle = 0x8000
        else:
            self.pwmio.duty_cycle = 0
            self.pwmio.frequency = 262
        if self.debug: print(f'Piezo.write({self.pin}, {self.frequency})')

    # Deinit function
    def deinit(self):
        self.pwmio.duty_cycle = 0
        self.pwmio.deinit()
        atexit.deregister(self.deinit)
        if self.debug: print(f'Piezo.deinit({self.pin})')

# Piezo class (END)




