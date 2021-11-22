import os
from pag import *
from graphvizoutput import *

class PerFlow(object):
    def __init__(self):
        self.static_analysis_binary_name = "init"
        self.dynamic_analysis_command_line = "init"
        self.mode = 'mpi+omp'
        self.nprocs = 0
        self.sampling_count = 1000

        self.tdpag = None
        self.ppag = None
    
    def setBinary(self, bin_name = ""):
        if bin_name != "":
            self.static_analysis_binary_name = bin_name

    def setCmdLine(self, cmd_line = ""):
        if cmd_line != "":
            self.dynamic_analysis_command_line = cmd_line

    def setProgMode(self, mode = ''):
        if mode != '':
            self.mode = mode

    def setNumProcs(self, nprocs = 0):
        if nprocs != 0:
            self.nprocs = nprocs
        else:
            self.nprocs = 1
        

    def staticAnalysis(self):
        cmd_line = '$BAGUA_DIR/build/builtin/binary_analyzer ' + self.static_analysis_binary_name
        os.system(cmd_line)        

    def dynamicAnalysis(self, sampling_count = 0):
        if sampling_count != 0:
            self.sampling_count = sampling_count
        
        if self.mode == 'mpi+omp':
            profiling_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libmpi_omp_sampler.so ' + self.dynamic_analysis_command_line
            os.system(profiling_cmd_line)
            comm_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libmpi_tracer.so ' + self.dynamic_analysis_command_line
            os.system(comm_cmd_line)
        
        if self.mode == 'pthread':
            profiling_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libpthread_sampler.so ' + self.dynamic_analysis_command_line
            os.system(profiling_cmd_line)

    def pagGeneration(self):
        
        if self.mode == 'mpi+omp':
            communication_analysis_cmd_line = '$BAGUA_DIR/build/builtin/comm_dep_approxi_analysis ' + str(self.nprocs) + ' ' + self.static_analysis_binary_name + '.dep'
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/mpi_mpag_generation ' + self.static_analysis_binary_name + ' ' + str(self.nprocs) + ' ' + '0' + ' ' + self.static_analysis_binary_name + '.dep' + ' ./SAMPLE*'
            print(communication_analysis_cmd_line)
            os.system(communication_analysis_cmd_line)
            print(pag_generation_cmd_line)
            os.system(pag_generation_cmd_line)
        if self.mode == 'pthread':
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/pthread_mpag_generation_example ' + self.static_analysis_binary_name + ' ./SAMPLE*'
            print(pag_generation_cmd_line)
            os.system(pag_generation_cmd_line)

        #read pag
        self.tdpag = ProgramAbstractionGraph.Read_GML('pag.gml')
        self.ppag = ProgramAbstractionGraph.Read_GML('mpi_mpag.gml')

    # TODO: different dynamic analysis mode, backend collectors and analyzers are ready.
    def run(self, binary = '', cmd = '', mode = '', nprocs = 0, sampling_count = 0):
        self.setBinary(binary)
        self.setCmdLine(cmd)
        self.setProgMode(mode)
        self.setNumProcs(nprocs)
        self.staticAnalysis()
        self.dynamicAnalysis(sampling_count)
        self.pagGeneration()
        return self.tdpag, self.ppag


    # Some builtin passes 

    def filter(self, V, name = '', type = ''):
        if name != '':
            return V.select(name_attr = name)
        if type != '':
            return V.select(type_eq = type)


    # def top(self, V, metric, n):
    #     topk = []
    #     k = n
    #     for v in V:
    #         if float(v['CYCAVGPERCENT']) > 0.05:
    #             topk.append(v)

    def hotspot_detection(self, V, metric = '', n = 0):
        if metric == '':
            metric = 'time'
        if n == 0:
            n = 10
        return V.select(lambda v: float(v['CYCAVGPERCENT']) >= 0.001)
        #return V.sort_by(metric).top(n)
    
    def report(self, V, E, attrs=[]):
        if len(attrs) == 0:
            attrs = ['name', 'type', 'time', 'debug']
        for v in V:
            for attr in attrs:
                print(v[attr], end=' ')
            print()

    def draw(self, g, save_pdf = ''):
        if save_pdf == '':
            save_pdf = 'pag.gml'
        graphviz_output = GraphvizOutput(output_file = save_pdf)
        graphviz_output.draw(g, vertex_attrs = ["id", "name", "type", "saddr", "eaddr" , "TOTCYCAVG", "CYCAVGPERCENT"], edge_attrs = ["id"], vertex_color_depth_attr = "CYCAVGPERCENT") #, preserve_attrs = "preserve")
        graphviz_output.show()