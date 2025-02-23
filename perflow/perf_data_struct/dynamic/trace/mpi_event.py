'''
module mpi event
'''

from .event import Event

'''
@class MpiEvent
An mpi event is an event related to MPI communications.
'''

class MpiEvent(Event):
    def __init__(self):
        super().__init__()
        # To be implemented
        pass
    

class MpiSendEvent(MpiEvent):
    def __init__(self):
        super().__init__()
        # To be implemented
        pass

class MpiRecvEvent(MpiEvent):
    def __init__(self):
        super().__init__()
        # To be implemented
        pass