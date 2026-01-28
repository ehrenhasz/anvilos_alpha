import pyb
accel = pyb.Accel()
blue = pyb.LED(4)
switch = pyb.Switch()
while True:
    pyb.wfi()
    if switch():
        pyb.delay(200)  # delay avoids detection of multiple presses
        blue.on()  # blue LED indicates file open
        log = open("/sd/log.csv", "w")  # open file on SD (SD: '/sd/', flash: '/flash/)
        while not switch():
            t = pyb.millis()  # get time
            x, y, z = accel.filtered_xyz()  # get acceleration data
            log.write("{},{},{},{}\n".format(t, x, y, z))  # write data to file
        log.close()  # close file
        blue.off()  # blue LED indicates file closed
        pyb.delay(200)  # delay avoids detection of multiple presses
