# onair

ESP (wemos D1 in my case) ON-AIR sign for twitch.

3d-printed case : https://www.thingiverse.com/thing:1506862
Original tutorial : https://learn.adafruit.com/3d-printed-iot-on-air-sign-for-twitch/overview

Unfortunately the original API used is dead and I also wanted a viewer counter.

The code supports two kinds of viewer counters : 
* a neopixel ring or just some neopixel leds at the end of the internal strip
  * In this case for a ring/strip of x leds, the code uses 1-x in green for up to x viewers, 1-x in yellow for up to 10\*x viewers and 1-x in red for up to 100\* viewers.  
  * e.g. on a 12 ring, you get up to 12 green leds, then the next viewer moves into 1 yellow, and on 20, 2 yellow.
* a serial-connected 4-digit display.  I made one years ago and decided to add it to this project.  The code assumes a looping 4-digit (no termination) at 9600 baud.  I have no idea if the more-common serial attached ones do this, but yeah, that's what mine does.

Pins are defined in the top of the main.cpp.

Oh, yeah, this is a platformIO project, for a real IDE instead of the limited arduino one.

It uses FastLED for the strips and WifiManager to enable setting up of the wifi and client id.  It also uses ArduinoJSONv5 to load the response from twitch rather than self-parsing.

Have fun!