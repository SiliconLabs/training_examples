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
from Piezo import Piezo
from Tick import Tick

# Rtttl class
# Lots of examples at: https://huggingface.co/datasets/cosimoiaia/RTTTL-Ringtones/tree/main
class Rtttl():

    # Initialisation
    def __init__(self, pin, debug):
        # Initialise
        self.debug = debug
        if self.debug: print(f'Rtttl.init({debug})')        
        self.pin = pin
        self.tunes = {}
        self.tune_names = []
        self.play_name = None
        self.play_index = 0
        self.play_repeat = False
        self.frequencies()        
        # Initialise piezo
        self.piezo = Piezo(pin, False)
        # Run initial tick timer
        self.tick = Tick("rtttl", 333, False, False)        

    # Main function - call repeatedly to drive playback
    def main(self):
        # Timer has fired ?
        if self.tick.read():
            played = False
            # Playing ?
            if self.play_name != None:
                # Reached end of tune ?
                if self.play_index >= len(self.tunes[self.play_name]):
                    # Repeat ?
                    if self.play_repeat:
                        self.play_index = 0
                    # Stop
                    else:
                        self.play_name = None
            # Something to play ?
            if self.play_name != None:
                if self.play_index < len(self.tunes[self.play_name]):
                    # Play note
                    self.piezo.write(self.tunes[self.play_name][self.play_index][0])
                    self.tick.write(self.tunes[self.play_name][self.play_index][1], False)
                    self.play_index += 1
                    played = True
            # Didn't play anything ?
            if not played:
                # Piezo is on?
                if self.piezo.on:
                    # Shut up
                    self.piezo.write(0)
                # Restart timer
                self.tick.write(333, False)
      
    # Play tune function
    def play(self, tune_name, repeat):
        result = False
        if tune_name != None:
            if tune_name in self.tunes:
                self.play_name = tune_name
                self.play_index = 0
                self.play_repeat = repeat
                result = True
        if self.debug: print(f'Rtttl.play("{tune_name}", {repeat}) = {result}')
        return result

    # Stop tune function
    def stop(self):
        result = False
        if self.play_name != None:
            self.play_name = None
            result = True
        if self.debug: print(f'Rtttl.stop() = {result}')
        return result

    # Load tune function
    def load(self, rtttl, debug):
        if debug: print(f'Rtttl.load("{rtttl}")')
        # Assume invalid
        tune_name = None
        # Remove spaces and convert to lower case
        rtttl = rtttl.replace(" ", "").lower()
        # Split on colons
        rtttl_parts = rtttl.split(":")
        rtttl_parts_count = len(rtttl_parts)
        if debug: print(f'rtttl_parts = {rtttl_parts}')
        # Got two parts ? 
        if rtttl_parts_count == 2 or rtttl_parts_count == 3: 
            # Extract name
            name = rtttl_parts[0]
            if debug: print(f'name = "{name}"')
            # Extract defaults
            duration = 4
            octave = 6
            bpm = 63                    
            notes_index = 1
            if rtttl_parts_count == 3:
                notes_index = 2
                defaults = rtttl_parts[1].split(",")
                for default in defaults:
                    # Default duration
                    index = default.find("d=")
                    if index == 0: 
                        value_str = default[index+2:]
                        try:
                            value_int = int(value_str)
                            if value_int in [1, 2, 4, 8, 16, 32]:
                                duration = value_int
                        except:
                            pass
                    # Default octave
                    index = default.find("o=")
                    if index == 0: 
                        value_str = default[index+2:]
                        try:
                            value_int = int(value_str)
                            if value_int >= 4 and value_int <= 7:
                                octave = value_int 
                        except:
                            pass
                    # Default bpm
                    index = default.find("b=")
                    if index == 0: 
                        value_str = default[index+2:]
                        try:
                            value_int = int(value_str)
                            if value_int >= 25 and value_int <= 900:
                                bpm = value_int 
                        except:
                            pass
            # Calculate length of a whole note
            # Ensure a whole note is a multiple of a 32nd note
            whole_ms = (60000 * duration) // bpm
            whole_ms //= 32
            whole_ms *= 32
            if debug: print(f'defaults: d = {duration}, o = {octave}, b = {bpm}, whole_ms = {whole_ms}')                                                                                        
            # Extract note data
            notes = []
            datas = rtttl_parts[notes_index].split(",")
            for data in datas:
                d_string = ""
                p_string = ""
                o_string = ""
                dot = False
                state = 0
                index = 0
                while index < len(data):
                    if state == 0: # Looking for duration (optional)
                        if data[index] in ["1", "2", "3", "4", "6", "8"]:
                            d_string += data[index]
                            index += 1
                        else:
                            state = 1
                    elif state == 1: # Looking for pitch note letter (mandatory)
                        if data[index] in ["a", "b", "c", "d", "e", "f", "g", "p"]:
                            p_string += data[index]
                            index += 1
                        else:
                            p_string += "p"
                        if p_string == "p":
                            state = 4
                        else:
                            state = 2
                    elif state == 2: # Looking for sharp (optional)
                        if data[index] == "#":
                            p_string += data[index]
                            index += 1
                        else:
                            state = 3
                    elif state == 3: # looking for octave or dot (optional - sometimes in different orders)
                        if data[index] in ["4", "5", "6", "7"]:
                            if len(o_string) == 0:
                                o_string += data[index]
                        elif data[index] == ".":
                            dot = True
                        index += 1                            
                    else:
                        index += 1
                if d_string not in ["1", "2", "4", "8", "16", "32"]:
                    d_string = str(duration)
                if o_string == "":
                    p_string += str(octave)
                else: 
                    p_string += o_string
                d_int = int(d_string)
                ms = whole_ms // d_int
                hz = 0
                if dot: ms += (ms//2)
                if p_string in self.frequencies:
                    hz = self.frequencies[p_string]
                notes.append((hz, ms))
                if debug: print(f'data="{data}", d="{d_string}", dot={dot}, ms={ms}, p="{p_string}", hz={hz}')
            # Have a name and some notes ?
            if len(name) > 0 and len(notes) > 0:
                tune_name = name
                self.tunes[tune_name] = notes
                self.tune_names = list(self.tunes)
            if debug: print(f'Rtttl.load()={tune_name}')
            return tune_name

    # Initialise frequencies
    def frequencies(self):
        # Frequencies
        self.frequencies = {
        "p"   :    0,
        "c4"  :  262,
        "c#4" :  277,
        "d4"  :  294,
        "d#4" :  311,
        "e4"  :  330,
        "f4"  :  349,
        "f#4" :  370,
        "g4"  :  392,
        "g#4" :  415, 
        "a4"  :  440,
        "a#4" :  466,
        "b4"  :  494,
        "c5"  :  523,
        "c#5" :  554,
        "d5"  :  587,
        "d#5" :  622,
        "e5"  :  659,
        "f5"  :  698,
        "f#5" :  740,
        "g5"  :  784,
        "g#5" :  831,
        "a5"  :  880,
        "a#5" :  932,
        "b5"  :  988,
        "c6"  : 1047,
        "c#6" : 1109,
        "d6"  : 1175,
        "d#6" : 1245,
        "e6"  : 1319,
        "f6"  : 1397,
        "f#6" : 1480,
        "g6"  : 1568,
        "g#6" : 1661,
        "a6"  : 1760,
        "a#6" : 1865,
        "b6"  : 1976,
        "c7"  : 2093,
        "c#7" : 2217,
        "d7"  : 2349,
        "d#7" : 2489,
        "e7"  : 2637,
        "f7"  : 2794,
        "f#7" : 2960,
        "g7"  : 3136,
        "g#7" : 3322,
        "a7"  : 3520,
        "a#7" : 3729,
        "b7"  : 3951}

# Rtttl class (END)