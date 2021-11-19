import sys                                                                                                                                                    
import os                                                                                                                                                     
proj_dir = os.environ['BAGUA_DIR']                                                                                                                            
sys.path.append(proj_dir + r"/python")                                                                                                                 
import json
from pag import *   
from graphvizoutput import *                                                                                                                                           
#import ProgramAbstractionGraph as paag  




input_gml = sys.argv[1]
g = ProgramAbstractionGraph.Read_GML(input_gml)  

num_vextices = g.vcount()
vertex_max_start_value = dict()
vertex_self_value = dict()
vertex_max_end_value = dict()
vectex_start_critical_path = dict()
vectex_end_critical_path = dict()
vertex_queue = list()

for i in range(len(g.vs())):
  vertex_self_value[i] = float(g.vs()[i]["CYCAVGPERCENT"])


def backward_bfs():
  while len(vertex_queue):
    vid = vertex_queue[0]
    del(vertex_queue[0])
    #print(vertex_max_start_value[vid], vertex_max_end_value[vid])
    vertex_max_start_value[vid] = vertex_self_value[vid] + vertex_max_end_value[vid]
    vectex_start_critical_path[vid] = []
    vectex_start_critical_path[vid] += vectex_end_critical_path[vid]
    vectex_start_critical_path[vid].append(vid)
    parents = g.predecessors(vid)
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
      # print(vid, p, vertex_max_start_value[vid], vertex_max_end_value[p])
  
  

vertex_queue.append(num_vextices-1)
vertex_max_end_value[num_vextices-1] = 0
vectex_end_critical_path[num_vextices-1] = []
backward_bfs()
print(vertex_max_start_value[0])
print(vectex_start_critical_path[0])

edge_set = []
for i in range(len(vectex_start_critical_path[0]) - 1):
  edge_set.append([vectex_start_critical_path[0][i+1], vectex_start_critical_path[0][i]])
print(edge_set)

graphviz_output = GraphvizOutput(output_file = "critical_path")
graphviz_output.draw(g, vertex_attrs = ["id", "name", "type", "saddr", "eaddr" , "TOTCYCAVG", "CYCAVGPERCENT", "join"], edge_attrs = ["id"], vertex_color_depth_attr = "CYCAVGPERCENT") #, preserve_attrs = "preserve")
graphviz_output.draw_edge(edge_set, color=0.1)
graphviz_output.show()

#g.write_dot("test.dot")

