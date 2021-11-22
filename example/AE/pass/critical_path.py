import sys                                                                                                                                                    
import os                                                                                                                                                     
proj_dir = os.environ['BAGUA_DIR']                                                                                                                            
sys.path.append(proj_dir + r"/python")       
import perflow as pf                                                                                                          
from pag import *   
from graphvizoutput import *                                                                                                                                           


pflow = pf.PerFlow()

# Run the binary and return a program abstraction graph
tdpag, ppag = pflow.run(binary = "./pthread_test", cmd = "srun -n 1 ./pthread_test 3000000000", mode = 'pthread')


# input_gml = sys.argv[1]
# ppag = ProgramAbstractionGraph.Read_GML(input_gml)  

num_vextices = ppag.vcount()
vertex_max_start_value = dict()
vertex_self_value = dict()
vertex_max_end_value = dict()
vectex_start_critical_path = dict()
vectex_end_critical_path = dict()
vertex_queue = list()

for i in range(len(ppag.vs())):
  vertex_self_value[i] = float(ppag.vs()[i]["CYCAVGPERCENT"])


# User-defind backward max flow search pass
def backward_max_flow_search():
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
      # print(vid, p, vertex_max_start_value[vid], vertex_max_end_value[p])
  
  

vertex_queue.append(num_vextices-1)
vertex_max_end_value[num_vextices-1] = 0
vectex_end_critical_path[num_vextices-1] = []
backward_max_flow_search()
print(vertex_max_start_value[0])
print(vectex_start_critical_path[0])

edge_set = []
for i in range(len(vectex_start_critical_path[0]) - 1):
  edge_set.append([vectex_start_critical_path[0][i+1], vectex_start_critical_path[0][i]])
print(edge_set)

graphviz_output = GraphvizOutput(output_file = "critical_path")
graphviz_output.draw(ppag, vertex_attrs = ["id", "name", "type", "saddr", "eaddr" , "TOTCYCAVG", "CYCAVGPERCENT", "join"], edge_attrs = ["id"], vertex_color_depth_attr = "CYCAVGPERCENT") #, preserve_attrs = "preserve")
graphviz_output.draw_edge(edge_set, color=0.1)
graphviz_output.show()
