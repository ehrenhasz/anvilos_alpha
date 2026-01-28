from machine import Pin
import music
def play(pin_str):
    p = Pin(pin_str, mode=Pin.OUT)
    music.play(music.PRELUDE, pin=p)
