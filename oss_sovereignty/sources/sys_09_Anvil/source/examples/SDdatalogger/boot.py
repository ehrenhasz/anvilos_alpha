import pyb
pyb.LED(3).on()  
pyb.delay(2000)  
switch_value = pyb.Switch()()  
pyb.LED(3).off()  
pyb.LED(4).on()  
if switch_value:
    pyb.usb_mode("VCP+MSC")
    pyb.main("cardreader.py")  
else:
    pyb.usb_mode("VCP+HID")
    pyb.main("datalogger.py")  
pyb.LED(4).off()  
