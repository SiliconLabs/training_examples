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
from digitalio import DigitalInOut, Direction

# Led class
class Led():

    # Initialisation
    def __init__(self, pin, invert, debug):
        # Initialise
        self.debug = debug
        if self.debug: print(f'Led.init({pin}, {invert}, {debug})')
        self.pin = pin
        self.dio = DigitalInOut(pin)
        self.dio.direction = Direction.OUTPUT
        self.invert = invert
        self.on = False
        # Turn off
        self.write(False)

    # Write function
    def write(self, on):
        self.on = on
        if self.invert:
            on = not on
        self.dio.value = on
        if self.debug: print(f'Led.write({self.pin}, {self.on})')


# Led class (END)




