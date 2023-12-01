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
import supervisor

# Constants
_TICKS_PERIOD = const(1<<29)
_TICKS_MAX = const(_TICKS_PERIOD-1)
_TICKS_HALFPERIOD = const(_TICKS_PERIOD//2)
_TICKS_DURATION_MAX = const(_TICKS_HALFPERIOD-1)

# Tick class
class Tick():

    # Initialisation
    def __init__(self, name, duration, repeat, debug):
        # Initialise
        self.debug = debug
        if self.debug: print(f'Tick.init({name}, {duration}, {repeat}, {debug})')        
        self.name = name
        self.on = False
        self.duration = 0
        self.repeat = False
        self.fired = False
        self.start = 0
        self.write(duration, repeat)

    # Write function
    def write(self, duration, repeat):
        self.duration = duration
        self.repeat = repeat
        if self.duration > _TICKS_DURATION_MAX:
            self.duration = _TICKS_DURATION_MAX
        if self.duration > 0:
            self.on = True
        else:
            self.on = False
        self.start = supervisor.ticks_ms()
        if self.debug: print(f'Tick.write({self.name}, {self.duration}, {self.repeat}) = {self.on}')            
        return self.on

    # Read function
    def read(self):
        self.fired = False
        if self.on:
            now = supervisor.ticks_ms()
            diff = (now - self.start) & _TICKS_MAX
            diff = ((diff + _TICKS_HALFPERIOD) & _TICKS_MAX) - _TICKS_HALFPERIOD
            if diff >= self.duration:
                self.fired = True
                if self.repeat:
                    self.write(self.duration, self.repeat)
                else:
                    self.write(0, False)
        if self.debug and fired: print(f'Tick.read({self.name}) = {self.fired}')
        return self.fired

# Tick class (END)




