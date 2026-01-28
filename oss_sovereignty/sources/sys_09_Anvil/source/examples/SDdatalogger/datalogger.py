import pyb
accel = pyb.Accel()
blue = pyb.LED(4)
switch = pyb.Switch()
while True:
    pyb.wfi()
    if switch():
        pyb.delay(200)  
        blue.on()  
        log = open("/sd/log.csv", "w")  
        while not switch():
            t = pyb.millis()  
            x, y, z = accel.filtered_xyz()  
            log.write("{},{},{},{}\n".format(t, x, y, z))  
        log.close()  
        blue.off()  
        pyb.delay(200)  
