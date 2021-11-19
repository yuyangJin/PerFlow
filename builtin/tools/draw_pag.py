import sys                                                                                                                                                    
import os                                                                                                                                                     
proj_dir = os.environ['BAGUA_DIR']                                                                                                                            
sys.path.append(proj_dir + r"/python")                                                                                                                 
import json
from pag import *   
from graphvizoutput import *                                                                                                                                           
#import ProgramAbstractionGraph as paag  

input_gml = sys.argv[1]
output = sys.argv[2]
g = ProgramAbstractionGraph.Read_GML(input_gml)                                                                                                         
graphviz_output = GraphvizOutput(output_file = output)
graphviz_output.draw(g, vertex_attrs = ["id", "name", "type", "saddr", "eaddr" , "TOTCYCAVG", "CYCAVGPERCENT"], edge_attrs = ["id"], vertex_color_depth_attr = "CYCAVGPERCENT") #, preserve_attrs = "preserve")
graphviz_output.show()

#g.write_dot("test.dot")

