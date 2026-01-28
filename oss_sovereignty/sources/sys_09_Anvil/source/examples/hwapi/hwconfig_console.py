class LEDClass:
    def __init__(self, id):
        self.id = "LED(%d):" % id
    def value(self, v):
        print(self.id, v)
    def on(self):
        self.value(1)
    def off(self):
        self.value(0)
LED = LEDClass(1)
LED2 = LEDClass(12)
