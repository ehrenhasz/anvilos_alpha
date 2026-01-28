try:
    import time, machine as machine
    machine.Timer
except:
    print("SKIP")
    raise SystemExit
t = machine.Timer(freq=1)
t.deinit()
t.deinit()
t = machine.Timer(freq=1)
t2 = machine.Timer(freq=1)
t.deinit()
t2.deinit()
t = machine.Timer(freq=1)
t2 = machine.Timer(freq=1)
t2.deinit()
t.deinit()
t = machine.Timer(period=1, mode=machine.Timer.ONE_SHOT, callback=lambda t: print("one-shot"))
time.sleep_ms(5)
t.deinit()
t = machine.Timer(period=4, mode=machine.Timer.PERIODIC, callback=lambda t: print("periodic"))
time.sleep_ms(14)
t.deinit()
