import pyb
accel = pyb.Accel()  
blue = pyb.LED(4)  
log = open("/sd/log.csv", "w")
blue.on()  
for i in range(100):
    t = pyb.millis()  
    x, y, z = accel.filtered_xyz()  
    log.write("{},{},{},{}\n".format(t, x, y, z))  
log.close()  
blue.off()  
