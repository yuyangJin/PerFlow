import os
import json
import numpy
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

        self.tdpag_perf_data = None
        self.ppag_perf_data = None
    
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
        
        if self.mode == 'omp':
            profiling_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libomp_sampler.so ' + self.dynamic_analysis_command_line
            os.system(profiling_cmd_line)

        if self.mode == 'pthread':
            profiling_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libpthread_sampler.so ' + self.dynamic_analysis_command_line
            os.system(profiling_cmd_line)

    def pagGeneration(self):
        pag_generation_cmd_line = ''
        tdpag_file = ''
        ppag_file =''

        if self.mode == 'mpi+omp':
            communication_analysis_cmd_line = '$BAGUA_DIR/build/builtin/comm_dep_approxi_analysis ' + str(self.nprocs) + ' ' + self.static_analysis_binary_name + '.dep'
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/mpi_pag_generation ' + self.static_analysis_binary_name + ' ' + str(self.nprocs) + ' ' + '0' + ' ' + self.static_analysis_binary_name + '.dep' + ' ./SAMPLE*'
            print(communication_analysis_cmd_line)
            os.system(communication_analysis_cmd_line)
            tdpag_file = 'pag.gml'
            ppag_file = 'mpi_mpag.gml'

        if self.mode == 'omp':
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/omp_pag_generation ' + self.static_analysis_binary_name + ' ./SAMPLE*TXT ./SOMAP*.TXT'
            print(communication_analysis_cmd_line)
            os.system(communication_analysis_cmd_line)
            tdpag_file = 'pag.gml'
            ppag_file = 'mpi_mpag.gml'

        if self.mode == 'pthread':
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/pthread_pag_generation ' + self.static_analysis_binary_name + ' ./SAMPLE.TXT'
            tdpag_file = 'pthread_tdpag.gml'
            ppag_file = 'pthread_ppag.gml'

        if pag_generation_cmd_line == '':
            exit()

        print(pag_generation_cmd_line)
        os.system(pag_generation_cmd_line)


        # read pag
        if tdpag_file != '':
            self.tdpag = ProgramAbstractionGraph.Read_GML(tdpag_file)
        if ppag_file != '':
            self.ppag = ProgramAbstractionGraph.Read_GML(ppag_file)
        
        # read pag performance data
        with open('output.json', 'r') as f:
            tdpag_perf_data = json.load(f)
        f.close()
        self.tdpag_perf_data = tdpag_perf_data

        print(self.tdpag_perf_data)

        with open('mpag_perf_data.json', 'r') as f:
            ppag_perf_data = json.load(f)
        f.close()
        self.ppag_perf_data = ppag_perf_data



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
            #print(name)
            return V.select(lambda v: (v["name"].find(name) != -1) )
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
        return V.select(lambda v: float(v['CYCAVGPERCENT']) > 0.0001)
        #return V.sort_by(metric).top(n)

    def imbalance_analysis(self, V):
        for v in V:
            if str(int(v['id'])) in self.tdpag_perf_data.keys():
                print(int(v['id']), self.tdpag_perf_data[str(int(v['id']))])
                data = self.tdpag_perf_data[str(int(v['id']))]
                for metric, metric_data in data.items():
                    # gather all procs data
                    metric_data_list = []
                    for procs, procs_data in metric_data.items():
                        # gather all thread data
                        procs_data_tot_num = 0.0
                        for thread, thread_data in procs_data.items():
                            procs_data_tot_num += thread_data
                        metric_data_list.append(procs_data_tot_num)
                    
                    # Calculate variance
                    mean = numpy.mean(metric_data_list)
                    var = numpy.var(metric_data_list)
                    std_var = numpy.std(metric_data_list)
                    if std_var > 70:
                        print(metric, "mean:", mean, "variance:", var, "standard variance:", std_var)
                        

    
    def report(self, V, attrs=[]):
        if len(attrs) == 0:
            attrs = ['name', 'type', 'time', 'debug']
        for attr in attrs:
            print(attr, end='\t')
        print()
        for v in V:
            for attr in attrs:
                print(v[attr], end='\t')
            print()

    def draw(self, g, save_pdf = '', mark_edges = []):
        if save_pdf == '':
            save_pdf = 'pag.gml'
        graphviz_output = GraphvizOutput(output_file = save_pdf)
        graphviz_output.draw(g, vertex_attrs = ["id", "name", "type", "saddr", "eaddr" , "TOTCYCAVG", "CYCAVGPERCENT"], edge_attrs = ["id"], vertex_color_depth_attr = "CYCAVGPERCENT") #, preserve_attrs = "preserve")
        if mark_edges!= []:
            graphviz_output.draw_edge(mark_edges, color=0.1)
        
        graphviz_output.show()


    def mpi_profiler_model(self, tdpag = None, ppag = None):
        if tdpag == None:
            print("No top-down view of PAG")
        
        ## a filter pass
        V_comm = self.filter(tdpag.vs, name = "mpi_")
        ## a hotspot detection pass
        V_hot = self.hotspot_detection(V_comm)
        ## a report pass
        attrs_list = ["name", "CYCAVGPERCENT", "saddr"] 
        self.report(V = V_hot, attrs = attrs_list)