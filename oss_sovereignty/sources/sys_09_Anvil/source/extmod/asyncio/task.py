from . import core
def ph_meld(h1, h2):
    if h1 is None:
        return h2
    if h2 is None:
        return h1
    lt = core.ticks_diff(h1.ph_key, h2.ph_key) < 0
    if lt:
        if h1.ph_child is None:
            h1.ph_child = h2
        else:
            h1.ph_child_last.ph_next = h2
        h1.ph_child_last = h2
        h2.ph_next = None
        h2.ph_rightmost_parent = h1
        return h1
    else:
        h1.ph_next = h2.ph_child
        h2.ph_child = h1
        if h1.ph_next is None:
            h2.ph_child_last = h1
            h1.ph_rightmost_parent = h2
        return h2
def ph_pairing(child):
    heap = None
    while child is not None:
        n1 = child
        child = child.ph_next
        n1.ph_next = None
        if child is not None:
            n2 = child
            child = child.ph_next
            n2.ph_next = None
            n1 = ph_meld(n1, n2)
        heap = ph_meld(heap, n1)
    return heap
def ph_delete(heap, node):
    if node is heap:
        child = heap.ph_child
        node.ph_child = None
        return ph_pairing(child)
    parent = node
    while parent.ph_next is not None:
        parent = parent.ph_next
    parent = parent.ph_rightmost_parent
    if node is parent.ph_child and node.ph_child is None:
        parent.ph_child = node.ph_next
        node.ph_next = None
        return heap
    elif node is parent.ph_child:
        child = node.ph_child
        next = node.ph_next
        node.ph_child = None
        node.ph_next = None
        node = ph_pairing(child)
        parent.ph_child = node
    else:
        n = parent.ph_child
        while node is not n.ph_next:
            n = n.ph_next
        child = node.ph_child
        next = node.ph_next
        node.ph_child = None
        node.ph_next = None
        node = ph_pairing(child)
        if node is None:
            node = n
        else:
            n.ph_next = node
    node.ph_next = next
    if next is None:
        node.ph_rightmost_parent = parent
        parent.ph_child_last = node
    return heap
class TaskQueue:
    def __init__(self):
        self.heap = None
    def peek(self):
        return self.heap
    def push(self, v, key=None):
        assert v.ph_child is None
        assert v.ph_next is None
        v.data = None
        v.ph_key = key if key is not None else core.ticks()
        self.heap = ph_meld(v, self.heap)
    def pop(self):
        v = self.heap
        assert v.ph_next is None
        self.heap = ph_pairing(v.ph_child)
        v.ph_child = None
        return v
    def remove(self, v):
        self.heap = ph_delete(self.heap, v)
class Task:
    def __init__(self, coro, globals=None):
        self.coro = coro  
        self.data = None  
        self.state = True  
        self.ph_key = 0  
        self.ph_child = None  
        self.ph_child_last = None  
        self.ph_next = None  
        self.ph_rightmost_parent = None  
    def __iter__(self):
        if not self.state:
            self.state = False
        elif self.state is True:
            self.state = TaskQueue()
        elif type(self.state) is not TaskQueue:
            raise RuntimeError("can't wait")
        return self
    def __next__(self):
        if not self.state:
            raise self.data
        else:
            self.state.push(core.cur_task)
            core.cur_task.data = self
    def done(self):
        return not self.state
    def cancel(self):
        if not self.state:
            return False
        if self is core.cur_task:
            raise RuntimeError("can't cancel self")
        while isinstance(self.data, Task):
            self = self.data
        if hasattr(self.data, "remove"):
            self.data.remove(self)
            core._task_queue.push(self)
        elif core.ticks_diff(self.ph_key, core.ticks()) > 0:
            core._task_queue.remove(self)
            core._task_queue.push(self)
        self.data = core.CancelledError
        return True
