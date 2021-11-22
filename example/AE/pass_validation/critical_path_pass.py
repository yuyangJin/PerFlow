import sys                                                                                                                                                    
import os                                                                                                                                                     
proj_dir = os.environ['BAGUA_DIR']                                                                                                                            
sys.path.append(proj_dir + r"/python")       
import perflow as pf                                                                                                          
from pag import *   
from graphvizoutput import *                                                                                                                                           


pflow = pf.PerFlow()

# Run the binary and return a program abstraction graph
tdpag, ppag = pflow.run(binary = "./pthread_test", cmd = "./pthread_test 300000000", mode = 'pthread')


pflow.draw(tdpag, save_pdf = './pthread_test.tdpag')
pflow.draw(ppag, save_pdf = './pthread_test.ppag')


# User-defind critical path pass
def critical_path():

  num_vextices = ppag.vcount()
  vertex_max_start_value = dict()
  vertex_self_value = dict()
  vertex_max_end_value = dict()
  vectex_start_critical_path = dict()
  vectex_end_critical_path = dict()
  vertex_queue = list()

  for i in range(len(ppag.vs())):
    vertex_self_value[i] = float(ppag.vs()[i]["CYCAVGPERCENT"])
  vertex_queue.append(num_vextices-1)
  vertex_max_end_value[num_vextices-1] = 0
  vectex_end_critical_path[num_vextices-1] = []

  # max_flow_search implemented with backward bfs
  while len(vertex_queue):
    vid = vertex_queue[0]
    del(vertex_queue[0])
    #print(vertex_max_start_value[vid], vertex_max_end_value[vid])
    vertex_max_start_value[vid] = vertex_self_value[vid] + vertex_max_end_value[vid]
    vectex_start_critical_path[vid] = []
    vectex_start_critical_path[vid] += vectex_end_critical_path[vid]
    vectex_start_critical_path[vid].append(vid)
    parents = ppag.predecessors(vid)
    for p in parents:
      vertex_queue.append(p)
      max_value = 0
      max_path = []
      if vertex_max_end_value.__contains__(p):
        max_value = vertex_max_end_value[p]
        max_path = vectex_end_critical_path[p]
        if vertex_max_start_value[vid] > max_value:
          max_value = vertex_max_start_value[vid]
          max_path = vectex_start_critical_path[vid]
      else:
        max_value = vertex_max_start_value[vid]
        max_path = vectex_start_critical_path[vid]
      vertex_max_end_value[p] = max_value
      vectex_end_critical_path[p] = max_path

  # return edge set
  edge_set = []
  for i in range(len(vectex_start_critical_path[0]) - 1):
    edge_set.append([vectex_start_critical_path[0][i+1], vectex_start_critical_path[0][i]])
  return edge_set

edge_set = critical_path()

pflow.draw(ppag, save_pdf = './critical_path', mark_edges = edge_set)
