from graph import *
from enum import Enum

class PagType(Enum):
    BB_NODE = -12
    INST_NODE = -11
    ADDR_NODE = -10
    CALL_IND_NODE = -7
    CALL_REC_NODE = -6
    CALL_NODE = -5
    FUNC_NODE = -4
    BRANCH_NODE = -2
    LOOP_NODE = -1
    MPI_NODE = 1


class ProgramAbstractionGraph(BasicGraph):
    def read_perfdata():
        pass
    # def __init__():
    #     pass

