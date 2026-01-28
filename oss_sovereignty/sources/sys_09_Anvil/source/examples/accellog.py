import pyb
accel = pyb.Accel()  # create object of accelerometer
blue = pyb.LED(4)  # create object of blue LED
log = open("/sd/log.csv", "w")
blue.on()  # turn on blue LED
for i in range(100):
    t = pyb.millis()  # get time since reset
    x, y, z = accel.filtered_xyz()  # get acceleration data
    log.write("{},{},{},{}\n".format(t, x, y, z))  # write data to file
log.close()  # close file
blue.off()  # turn off LED
