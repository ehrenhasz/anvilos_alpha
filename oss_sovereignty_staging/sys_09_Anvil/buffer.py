
class TextBuffer:
    def __init__(self):
        self.lines = []
    
    def load_text(self, text):
        self.lines = text.splitlines()
    
    def get_line(self, index):
        if 0 <= index < len(self.lines):
            return self.lines[index]
        return None
    
    def count_lines(self):
        return len(self.lines)

def test():
    buf = TextBuffer()
    buf.load_text('Hello\nAnvil\nSovereignty')
    print('Lines:', buf.count_lines())
    print('Line 1:', buf.get_line(1))
    if buf.count_lines() == 3 and buf.get_line(1) == 'Anvil':
        print('>> TEST_BUFFER: SUCCESS')
    else:
        print('>> TEST_BUFFER: FAILED')
        exit(1)

if __name__ == '__main__':
    test()
