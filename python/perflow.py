import os
import json
import time
import numpy
from collections import OrderedDict
import matplotlib.pyplot as plt
import scipy
from scipy.cluster.hierarchy import dendrogram, linkage
import sklearn
from sklearn.cluster import AgglomerativeClustering
from sklearn.preprocessing import normalize
from pag import *
from graphvizoutput import *

class PerFlow(object):
    def __init__(self):
        self.static_analysis_binary_name = "init"
        self.dynamic_analysis_command_line = "init"
        self.data_dir = ''
        self.mode = 'mpi+omp'
        self.nprocs = 0
        self.sampling_count = 1000

        self.tdpag_file = ''
        self.ppag_file =''

        self.tdpag = None
        self.ppag = None

        self.tdpag_perf_data = None
        self.ppag_perf_data = None
        self.tdpag_to_ppag_map = None
        self.ppag_to_tdpag_map = None
    
    ''' 
    =========================================== 
    
    Settings 
    
    ===========================================
    '''
    
    
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

    ''' 
    =========================================== 
    
    Hybrid Analysis 
    
    ===========================================
    '''

    def staticAnalysis(self):
        cmd_line = 'time $BAGUA_DIR/build/builtin/binary_analyzer ' + self.static_analysis_binary_name + ' ' + self.data_dir + '/static_data'
        os.system(cmd_line)
        
    def dynamicAnalysis(self, sampling_count = 0):
        mkdir_cmd_line = 'mkdir -p ./dynamic_data'
        os.system(mkdir_cmd_line)

        if sampling_count != 0:
            self.sampling_count = sampling_count
        
        if self.mode == 'mpi+omp':
            remove_cmd_line = 'rm -rf ./MPID* ./MPIT* ./SAMPLE* ./SOMAP*'
            os.system(remove_cmd_line)
            profiling_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libmpi_omp_sampler.so ' + self.dynamic_analysis_command_line
            os.system(profiling_cmd_line)         
            # comm_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libmpi_tracer.so ' + self.dynamic_analysis_command_line
            # os.system(comm_cmd_line)
        
        if self.mode == 'omp':
            profiling_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libomp_sampler.so ' + self.dynamic_analysis_command_line
            os.system(profiling_cmd_line)

        if self.mode == 'pthread':
            profiling_cmd_line = 'LD_PRELOAD=$BAGUA_DIR/build/builtin/libpthread_sampler.so ' + self.dynamic_analysis_command_line
            os.system(profiling_cmd_line)
        

        # mv_cmd_line = 'mv SAMPLE* SOMAP* MPID* MPIT*' + ' ./' + self.data_dir + '/dynamic_data/'
        # os.system(mv_cmd_line)

        mv_cmd_line = 'mv dynamic_data' + ' ./' + self.data_dir + '/'
        os.system(mv_cmd_line)

    ''' 
    =========================================== 
    
    PAG related operations
    
    ===========================================
    '''   

    def pagGeneration(self):
        pag_generation_cmd_line = ''

        if self.mode == 'mpi+omp':
            communication_analysis_cmd_line = '$BAGUA_DIR/build/builtin/comm_dep_approxi_analysis ' + str(self.nprocs) + ' ' + self.data_dir + ' ' + self.static_analysis_binary_name + '.dep'
            pag_generation_cmd_line = 'time $BAGUA_DIR/build/builtin/tools/mpi_pag_generation ' + self.static_analysis_binary_name + ' ' + self.data_dir + ' ' + str(self.nprocs) + ' ' + '0' + ' ' + self.static_analysis_binary_name + '.dep' #+ ' ./SAMPLE*'
            print(communication_analysis_cmd_line)
            os.system(communication_analysis_cmd_line)


        if self.mode == 'omp':
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/omp_pag_generation ' + self.static_analysis_binary_name + ' ./SAMPLE*TXT ./SOMAP*.TXT'
            # print(communication_analysis_cmd_line)
            # os.system(communication_analysis_cmd_line)
            self.tdpag_file = 'static_pag.gml'
            self.ppag_file = 'omp_mpag.gml'

        if self.mode == 'pthread':
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/pthread_pag_generation ' + self.static_analysis_binary_name + ' ./SAMPLE.TXT'
            self.tdpag_file = 'pthread_tdpag.gml'
            self.ppag_file = 'pthread_ppag.gml'

        if pag_generation_cmd_line == '':
            exit()

        print(pag_generation_cmd_line)
        os.system(pag_generation_cmd_line)


    def readPag(self, dir = ''):
        if dir != '':
            self.data_dir = dir
        self.mode = 'mpi+omp'
        # Read pag
        
        if self.mode == 'mpi+omp':
            self.tdpag_file = self.data_dir + '/pag.gml'
            self.ppag_file = self.data_dir + '/mpi_mpag.gml'
        if self.mode == 'omp':
            self.tdpag_file = self.data_dir + '/pag.gml'
            self.ppag_file = self.data_dir + '/mpi_mpag.gml'
        if self.mode == 'pthread':
            self.tdpag_file = self.data_dir + '/pthread_tdpag.gml'
            self.ppag_file = self.data_dir + '/pthread_ppag.gml'
        

        if self.tdpag_file != '':
            self.tdpag = ProgramAbstractionGraph.Read_GML(self.tdpag_file)
        if self.ppag_file != '':
            self.ppag = ProgramAbstractionGraph.Read_GML(self.ppag_file)
        
        # read pag performance data
        with open(self.data_dir + '/output.json', 'r') as f:
            self.tdpag_perf_data = json.load(f)
        f.close()
        # self.tdpag_perf_data = tdpag_perf_data

        # print(self.tdpag_perf_data)

        with open(self.data_dir + '/mpag_perf_data.json', 'r') as f:
            self.ppag_perf_data = json.load(f)
        f.close()
        # self.ppag_perf_data = ppag_perf_data

        with open(self.data_dir + '/pag_to_mpag.json', 'r') as f:
            self.tdpag_to_ppag_map = json.load(f)
        f.close()
    
        self.getPpagToTdpagMap()

        return self.tdpag, self.ppag

    def getPpagToTdpagMap(self):
        self.ppag_to_tdpag_map = dict()
        for tdpag_v_str, pid_to_ppag in self.tdpag_to_ppag_map.items():
            tdpag_v = int(tdpag_v_str)
            for pid_str, tid_to_ppag in pid_to_ppag.items():
                pid = int(pid_str)
                for tid_str, ppag_v in tid_to_ppag.items():
                    tid = int(tid_str)
                    self.ppag_to_tdpag_map[ppag_v] = tuple([tdpag_v, pid, tid])

    def makeDataDir(self, dir):
        if dir == "":
            self.data_dir = self.static_analysis_binary_name.strip().split('/')[-1]  + '-' + str(self.nprocs) + 'p-' + time.strftime('%Y%m%d-%H%M%S', time.localtime(int(round(time.time() * 1000)) / 1000))
            mkdir_cmd_line = 'mkdir -p ./' + self.data_dir
            os.system(mkdir_cmd_line)
            mkdir_cmd_line = 'mkdir -p ./' + self.data_dir + '/static_data'
            os.system(mkdir_cmd_line)
            mkdir_cmd_line = 'mkdir -p ./' + self.data_dir + '/dynamic_data'
            os.system(mkdir_cmd_line)
        else:
            self.data_dir = dir

    def recordBasicInfo(self):
        basic_info_file_name = self.data_dir + "/basic_info.json"
        binary_name = self.static_analysis_binary_name.strip().split('/')[-1]
        basic_info = OrderedDict()
        basic_info["binary_name"] = binary_name
        basic_info["nprocs"] = self.nprocs
        basic_info["mode"] = self.mode
        with open(basic_info_file_name, 'w') as f:
            json.dump(basic_info, f)

    # TODO: different dynamic analysis mode, backend collectors and analyzers are ready.
    def run(self, binary = '', cmd = '', mode = '', nprocs = 0, sampling_count = 0, dir = ''):
        self.setBinary(binary)
        self.setCmdLine(cmd)
        self.setProgMode(mode)
        self.setNumProcs(nprocs)

        self.makeDataDir(dir)
        
        self.staticAnalysis()
        self.dynamicAnalysis(sampling_count)
        self.pagGeneration()

        self.recordBasicInfo()

        self.readPag()
        return self.tdpag, self.ppag


    ''' 
    =========================================== 
    
    Builtin Passes 
    
    ===========================================
    '''

    def filter(self, V, name = '', type = ''):
        if name != '':
            #print(name)
            # return V.select(lambda v: (v["name"].find(name) != -1) )
            return V.select(lambda v: (v['name'].startswith(name) == True) )
        if type != '':
            return V.select(type_eq = type)


    def sort_by(self, V, metric = 'CYCAVGPERCENT', reverse_flag = True ):
        V_sorted = sorted(V, key=lambda v:float(v[metric]), reverse=reverse_flag)
        return V_sorted


    def topk(self, V, k):
        i = 0
        V_topk = []
        for v in V:
            if i >= k:
                break
            V_topk.append(v)
            i += 1
        V_topk_ret = self.tdpag.vs.select(lambda v: v in V_topk)
        return V_topk_ret
    

    def hotspot_detection(self, V, metric = 'time', n = 10):
        return V.select(lambda v: float(v['CYCAVGPERCENT']) > 0.0001)


    def imbalance_analysis(self, V, nprocs = 1):
        self.inter_vertex_imbalance_analysis(V, nprocs)


    def inter_vertex_imbalance_analysis(self, V, nprocs = 1):
        imb_V = []
        for v in V:
            vid = str(int(v['id']))
            if vid in self.tdpag_perf_data.keys():
                data = self.tdpag_perf_data[vid]
                for metric, metric_data in data.items():
                    # gather all procs data
                    metric_data_list = [0 for _ in range(nprocs)]
                    if metric_data == None:
                        continue
                    for procs, procs_data in metric_data.items():
                        # gather all thread data
                        procs_data_tot_num = 0.0
                        for thread, thread_data in procs_data.items():
                            procs_data_tot_num += thread_data
                        metric_data_list[int(procs)] = procs_data_tot_num
                    
                    def normalization(data):
                        _range = numpy.max(data) - numpy.min(data)
                        return (data - numpy.min(data)) / _range

                    # Calculate variance
                    mean = numpy.mean(metric_data_list)
                    var = numpy.var(metric_data_list)
                    std_var = numpy.std(normalization(metric_data_list))
                    if std_var > 0.15:
                        # print(metric, "mean:", mean, "variance:", var, "standard variance:", std_var)

                        # draw_scatter_figure(numpy.arange(nprocs), metric_data_list, xlabel='Process ID', ylabel='Execution Cycles', title = str(int(v['id'])) + '-' + v['name'] )
                        # draw_plot_figure(numpy.arange(nprocs), metric_data_list, xlabel='Process ID', ylabel='Execution Cycles', title = str(int(v['id'])) + '-' + v['name'] )
                        draw_bar_figure(numpy.arange(nprocs), metric_data_list, xlabel='Process ID', ylabel='Execution Cycles', title = str(int(v['id'])) + '-' + v['name'] )
                        imb_V.append(v)
        return imb_V


    def inter_process_imbalance_analysis(self, V, nprocs = 1, threshold = 1.3):     
        imb_VS = dict()
        for v in V:
            vid = str(int(v['id']))
            if vid in self.tdpag_perf_data.keys():
                data = self.tdpag_perf_data[vid]
                for metric, metric_data in data.items():
                    # gather all procs data
                    metric_data_list = [0 for _ in range(nprocs)]
                    if metric_data == None:
                        continue
                    for procs, procs_data in metric_data.items():
                        # gather all thread data
                        procs_data_tot_num = 0.0
                        for thread, thread_data in procs_data.items():
                            procs_data_tot_num += thread_data
                        metric_data_list[int(procs)] = procs_data_tot_num

                    if int(v['type']) == 1: # MPI func
                        if '_barrier' in v['name']:
                            min_value = numpy.min(metric_data_list)
                            min_pids = numpy.argmin(metric_data_list)
                            for pid in range(nprocs):
                                wait = metric_data_list[pid] - min_value
                                if wait > 0:      
                                    ppag_vid = self.tdpag_to_ppag_map[str(vid)][str(pid)]['0']
                                    target_pid = min_pids
                                    ppag_target_vid = self.tdpag_to_ppag_map[str(vid)][str(target_pid)]['0']
                                    self.ppag.add_edges([(ppag_target_vid, ppag_vid)])
                                    e = self.ppag.es[-1]
                                    e['time'] = metric_data_list[pid] - min_value
                                    print(ppag_vid, ppag_target_vid, e['time'])

                    mean = numpy.mean(metric_data_list)

                    imb_pid_V = []
                    for pid in range(nprocs):
                        if metric_data_list[pid] > threshold * mean:
                            ppag_vid = self.tdpag_to_ppag_map[str(vid)][str(pid)]['0']
                            imb_pid_V.append(ppag_vid)
                    print(imb_pid_V)
                    tmp_imb_V = self.ppag.vs.select(lambda v:v['id'] in imb_pid_V)
                    imb_VS[v] = tmp_imb_V
        return imb_VS

    def get_comm_edges(self, V, nprocs, threshold = 0):
        E = dict()
        for v in V:
            if v['type'] != 1:
                continue
            E[v] = []
            tdpag_vid = int(v['id'])

            for pid in range(nprocs):
                ppag_vid = self.tdpag_to_ppag_map[str(tdpag_vid)][str(pid)]['0']
                ppag_v = self.ppag.vs.find(id=ppag_vid)
                ppag_v_next = self.ppag.vs.find(id=ppag_vid+1)

                E_tmp = self.ppag.es.select(lambda e: e.source == ppag_v.index and e.target != ppag_v_next.index)

                for e in E_tmp:
                    if e.attributes().__contains__('time'):
                        if e['time'] != float('nan') and e['time'] != float('inf'):
                            if e['time'] > threshold: 
                                E[v].append([e.source, e.target])

        return E 

    def communication_pattern_analysis(self, V, nprocs=1):
        comm_pattern_mat = numpy.zeros(nprocs * nprocs).reshape(nprocs, nprocs)
        for v in V:
            vid = str(int(v['id']))
            if not self.tdpag_to_ppag_map.keys().__contains__(vid):
                continue
            for pid in range(nprocs):
                if not self.tdpag_to_ppag_map[vid].keys().__contains__(str(pid)):
                    continue
                if not self.tdpag_to_ppag_map[vid][str(pid)].keys().__contains__('0'):
                    continue
                ppag_vid = self.tdpag_to_ppag_map[vid][str(pid)]['0']
                ppag_v = self.ppag.vs[int(ppag_vid)]
                edges = ppag_v.out_edges()
                # print(vid, ppag_vid, edges)
                for e in edges:
                    if e.attributes().__contains__('time'):
                        if (e['time'] != float('nan') and e['time'] != float('inf')):
                            ''' the return value of e.target is vertex id (python-igraph id), but not the attribute 'id' of vertex, the latter one is what we need.'''
                            ppag_target_v = self.ppag.vs[e.target]
                            ppag_target_vid = int(ppag_target_v['id'])

                            if self.ppag_to_tdpag_map.keys().__contains__(ppag_target_vid):
                                tdpag_info = self.ppag_to_tdpag_map[ppag_target_vid]
                                dest_pid = tdpag_info[1]
                                comm_pattern_mat[pid][dest_pid] += float(e['time'])
                                comm_pattern_mat[dest_pid][pid] += float(e['time'])

        plt.pcolormesh(comm_pattern_mat, cmap=plt.cm.binary, vmin = 0.0, vmax = 1.0, edgecolors='grey', linewidths=0.1)
        plt.colorbar(label = "Time(ms)")
        plt.title("Communication Pattern")
        plt.xlabel("Process ID")
        plt.ylabel("Process ID")
        plt.savefig("comm_pattern.pdf")
        plt.clf()

        plt.pcolormesh(comm_pattern_mat, cmap=plt.cm.binary, vmin = 0.0, edgecolors='grey', linewidths=0.1)
        plt.colorbar(label = "Time(ms)")
        plt.title("Communication Time")
        plt.xlabel("Process ID")
        plt.ylabel("Process ID")
        plt.savefig("comm_time.pdf")
        plt.clf()

                        
    def process_similarity_analysis(self, V, nprocs, metric = 'TOT_CYC'):
        ''' process character vector '''
        charact_vec = [[] for _ in range(nprocs)]

        for v in V:
            vid = int(v['id'])
            v_name = v['name']
            # if not (v_name.startswith('_mpi_') or v_name.startswith('mpi_') or v_name.startswith('_MPI_') or v_name.startswith('MPI_')):
            if vid not in [1357, 1411] :
                continue
            if str(vid) in self.tdpag_perf_data.keys():
                if metric in self.tdpag_perf_data[str(vid)].keys():
                    data = self.tdpag_perf_data[str(vid)][metric]
                    if data == None:
                        for i in range(nprocs):
                            charact_vec[i].append(0)
                        continue
                    for pid in range(nprocs):
                        if str(pid) in data.keys():
                            procs_data = data[str(pid)]
                            # gather all thread data
                            procs_data_tot_num = 0.0
                            for thread, thread_data in procs_data.items():
                                procs_data_tot_num += thread_data
                            charact_vec[pid].append(procs_data_tot_num)
                        else :
                            charact_vec[pid].append(0)
                else:
                    for i in range(nprocs):
                        charact_vec[i].append(0)
            else:
                for i in range(nprocs):
                    charact_vec[i].append(0)

        charact_vec = normalize(charact_vec)

        # Calculate distance matrix

        euclidean_dist = numpy.zeros(nprocs * nprocs).reshape(nprocs, nprocs)
        for i in range(nprocs):
            for j in range(i + 1, nprocs):
                value = numpy.linalg.norm(numpy.array(charact_vec[i]) - numpy.array(charact_vec[j]))
                euclidean_dist[i][j] = value
                euclidean_dist[j][i] = value
        # plt.imshow(euclidean_dist, cmap=plt.cm.binary)
        plt.pcolormesh(euclidean_dist, cmap=plt.cm.binary, vmax = 1)
        plt.colorbar()
        plt.savefig("euc_dist_mat.pdf")
        plt.clf()

        A=[]
        for i in range(len(charact_vec)):
            a=chr(i+ord('A'))
            A.append(a)

        
        # Hierarchical clustering
        # euclidean_dist = scipy.cluster.hierarchy.distance.pdist(charact_vec, 'euclidean')
        clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'ward', metric='euclidean', optimal_ordering=False)
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'ward', metric='euclidean')
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'single', metric='euclidean')
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'average', metric='euclidean', optimal_ordering=True)
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'complete', metric='euclidean')
        scipy.cluster.hierarchy.dendrogram(clusters, labels=numpy.arange(nprocs))
        plt.savefig("cluster_results.pdf")
        plt.clf()

        print(clusters)


    def connection_path_analysis_pass(self, V):
        V_path = []
        V_seq = self.ppag.vs.select(lambda vertex:vertex in V)
        for v in V_seq:
            if 'main' in v['name']:
                continue
            saddr = v['saddr']
            V_other = V_seq.select(lambda vertex: vertex['saddr'] != saddr)
            # V_path += self.ppag.get_all_simple_paths(v, V_other, cutoff = len(self.tdpag_to_ppag_map.keys()) / 2, mode = 'out')
            V_path += self.ppag.get_all_shortest_paths(v, V_other, mode = 'out')

        return V_path

    def backtracking_analysis_pass(self, VS):
        '''
        A SIMPLE version: 
            get the shorstest paths between computation vertices and communication vertices.
        '''
        E_path = []
        V_comp = []
        V_comm = []
        for tdpag_v, V in VS.items():
            if tdpag_v['type'] != 1:
                V_comp += V
            else:
                V_comm += V
        for v_comp in V_comp:
            V_path = self.ppag.get_all_shortest_paths(v_comp, V_comm, mode = 'out')
            print(V_path)
            E_path+=V_path
        return E_path

    
    def report(self, V, attrs=[], n=10):
        if len(attrs) == 0:
            attrs = ['name', 'type', 'time', 'debug']
        for attr in attrs:
            print(attr, end='\t')
        print()
        i = 0
        for v in V:
            if i > 10:
                break
            for attr in attrs:
                format_print = '{0:^10}'
                if attr == 'saddr' or attr == 'eaddr':
                    print(format_print.format(hex(int(v[attr]))), end='\t')
                else:
                    print(format_print.format(v[attr]), end='\t')
            print()
            i+=1


    def draw(self, g, save_pdf = '', mark_edges = []):
        if save_pdf == '':
            save_pdf = 'pag.gml'
        graphviz_output = GraphvizOutput(output_file = save_pdf)
        graphviz_output.draw(g, vertex_attrs = ["id", "name", "type", "saddr", "eaddr" , "TOTCYCAVG", "CYCAVGPERCENT"], edge_attrs = ["id"], vertex_color_depth_attr = "CYCAVGPERCENT") #, preserve_attrs = "preserve")
        if mark_edges != []:
            graphviz_output.draw_edge(mark_edges, color=0.1)
        
        graphviz_output.show()



    ''' 
    =========================================== 
    
    Builtin Models / Diagrams 
    
    ===========================================
    '''


    def hotspot_detection_model(self, tdpag = None, ppag = None):
        if tdpag == None:
            print("No top-down view of PAG")

        ''' 1. Hotspot detection pass '''
        V_hot = self.hotspot_detection(tdpag.vs)
        V_hot_sorted = sorted(V_hot, key=lambda v:float(v["CYCAVGPERCENT"]), reverse=True)
        ''' 2. Report pass '''
        attrs_list = ["id", "name", "CYCAVGPERCENT", "saddr"] 
        self.report(V = V_hot_sorted, attrs = attrs_list)


    def mpi_profiler_model(self, tdpag = None, ppag = None):
        if tdpag == None:
            print("No top-down view of PAG")
        
        ''' 1. Filter pass '''
        V_comm = self.filter(tdpag.vs, name = "mpi_")
        ''' 2. Hotspot detection pass '''
        V_hot = self.hotspot_detection(V_comm)
        ''' 3. Report pass '''
        attrs_list = ["name", "CYCAVGPERCENT", "saddr"] 
        self.report(V = V_hot, attrs = attrs_list)

    def communication_pattern_analysis_model(self, nprocs=1):
        ''' 1. Filter pass '''
        V_comm = self.filter(self.tdpag.vs, name = "_mpi_")
        if len(V_comm) == 0:
            V_comm = self.filter(self.tdpag.vs, name = "MPI_")
        if len(V_comm) == 0:
            V_comm = self.filter(self.tdpag.vs, name = "mpi_")
        if len(V_comm) == 0:
            V_comm = self.filter(self.tdpag.vs, name = "_MPI_")

        ''' 2. Communication pattern analysis pass '''
        #self.communication_pattern_analysis(V_comm, nprocs)
        self.communication_pattern_analysis(self.tdpag.vs, nprocs)


    def backtracking_analysis_model(self, nprocs=1):
        if self.tdpag == None:
            print("No top-down view of PAG")

        ''' 1. Hotspot detection'''
        V_hot = self.hotspot_detection(self.tdpag.vs)
        V_hot_sorted = self.sort_by(V_hot, metric='CYCAVGPERCENT', reverse_flag=True)
        V_hot_top = self.topk(V_hot_sorted, 4)
        # for v in V_hot_top:
        #     print(v['id'], v['name'], v['saddr'], v['eaddr'], v['CYCAVGPERCENT'])

        ''' 2. Inter-vertex imbalance analysis'''
        V_imb = self.inter_vertex_imbalance_analysis(V_hot_top, nprocs)
        # for v in V_imb:
        #     ppag_v = int(v['id'])
        #     pid = self.ppag_to_tdpag_map[ppag_v][1]
        #     print(v['id'], v['name'], pid,  v['saddr'], v['eaddr'], v['TOTCYCSUM'])

        ''' 3. Inter-process imbalance analysis'''
        VS_imb = self.inter_process_imbalance_analysis(V_imb, nprocs)


        ''' 4. Backtracking analysis'''
        V_paths = self.backtracking_analysis_pass(VS_imb)


        ''' 5. Show the backtracking results '''
        '''
        Below shows an example output.
        Vertices are code snippets, edges are dependence.
         - 'o' represents imbalance
         - '.' represents normal

        ........o..o.....o.....oo......
                |  |    / \    /\       
        ........o..o...o...o..o..o.....    
               /   |    \   \/    \        
              /    |     |   \     |            
             /     |      \   \    |           
        ....o......o.......o...o...o...             
        '''

        def get_all_related_vertices(V_imb):
            V_related = []
            
            # Need check with vid of process 0 in ppag
            # here simplily use vid of tdpag
            V_imb_sorted_by_execution_order = self.sort_by(V_imb, metric='id')
            tdpag_first_v = V_imb[0]
            tdpag_end_v = V_imb[-1]
            ppag_first_vid = self.tdpag_to_ppag_map[str(int(tdpag_first_v['id']))]['0']['0']
            # ppag_end_vid = self.tdpag_to_ppag_map[str(int(tdpag_end_v['id']))]['0']['0']
            tdpag_v = tdpag_first_v
            ppag_vid = ppag_first_vid
            
            while True:
                if tdpag_v in V_imb:
                    V_related.append(tdpag_v)
                elif  tdpag_v['type'] == 1: # MPI
                    V_related.append(tdpag_v)
                
                if tdpag_v == tdpag_end_v:
                    break

                # next tdpag_v
                ppag_vid += 1
                tdpag_vid = self.ppag_to_tdpag_map[ppag_vid][0]
                V_tmp = self.tdpag.vs.select(lambda v: v['id'] == tdpag_vid)
                tdpag_v = V_tmp[0]
            
            return V_related

        ''' 5.1 Get all related vertices '''
        V_related = get_all_related_vertices(V_imb)

        ''' 5.2 Build matrix map: key(vertex id), value(position in the matrix) '''
        V_mat = dict()
        for i in range(len(V_related)):
            v = V_related[len(V_related) - 1 - i]
            for pid in range(nprocs):
                ppag_vid = self.tdpag_to_ppag_map[str(int(v['id']))][str(pid)]['0']
                ppag_vID = self.ppag.vs.find(id=ppag_vid).index
                V_mat[ppag_vID] = [pid, i]
                # print(ppag_vID, [pid, i])


        ''' 5.3 Prepare arrays for drawing imbalance/normal points'''

        Y_imb_v= [([]) for i in range(len(V_related))]
        X_imb_v= [([]) for i in range(len(V_related))]

        Y_norm_v= [([]) for i in range(len(V_related))]
        X_norm_v= [([]) for i in range(len(V_related))]

        for i in range(len(V_related)):
            v = V_related[len(V_related) - 1 - i]
            if v not in V_imb:
                for j in range(nprocs):
                    X_norm_v[i].append(j)
                    Y_norm_v[i].append(i)

            else:
                V_inter_process_imb = VS_imb[v]
                PID_inter_process_imb = []
                for v in V_inter_process_imb:
                    ppag_v = int(v['id'])
                    pid = self.ppag_to_tdpag_map[ppag_v][1]
                    PID_inter_process_imb.append(pid)
                for j in range(nprocs):
                    if j in PID_inter_process_imb:
                        X_imb_v[i].append(j)
                        Y_imb_v[i].append(i)
                    else:
                        X_norm_v[i].append(j)
                        Y_norm_v[i].append(i)

        ''' 5.4 Prepare arrays for drawing arrows between points '''
        E_arrow = []

        for path in V_paths:
            src_vID = path[0]
            dest_vID = -1
            for i in range(1, len(path)):
                if path[i] != path[i-1] + 1:
                    dest_vID = path[i-1]
                    if V_mat.keys().__contains__(src_vID) and V_mat.keys().__contains__(dest_vID):
                        src = V_mat[src_vID]
                        dest = V_mat[dest_vID]
                        E_arrow.append([src, dest])
                    src_vID = path[i-1]
                    dest_vID = path[i]
                    if V_mat.keys().__contains__(src_vID) and V_mat.keys().__contains__(dest_vID):
                        src = V_mat[src_vID]
                        dest = V_mat[dest_vID]
                        E_arrow.append([src, dest])
                elif i == len(path) - 1:
                    dest_vID = path[i]
                    if V_mat.keys().__contains__(src_vID) and V_mat.keys().__contains__(dest_vID):
                        src = V_mat[src_vID]
                        dest = V_mat[dest_vID]
                        E_arrow.append([src, dest])
                    break
                else:
                    continue
                    
                src_vID = path[i]
                
        
        # E_arrow = []
        # for v, E in E_comm.items():
        #     for e in E:
        #         # print(e[0], e[1])
        #         if V_mat.keys().__contains__(e[0]) and V_mat.keys().__contains__(e[1]):
        #             src = V_mat[e[0]]
        #             dest = V_mat[e[1]]
        #             E_arrow.append([src, dest])

        # print('E_arrows======', E_arrow)

        ''' 5.5 Draw the results'''

        plt.figure(figsize=(nprocs*0.3, len(V_related)))

        for i in range(len(V_related)):
            draw_points(X_norm_v[i], Y_norm_v[i])
            draw_points(X_imb_v[i], Y_imb_v[i], marker_shape='o')

        draw_arrows(E_arrow)

        Y_name = []
        for v in V_related:
            Y_name.append(v['name'])
        Y_name.reverse()

        plt.yticks(numpy.arange(len(Y_name)), Y_name, fontsize = 18)

        plt.savefig(Y_name[0]+'_backtacking.pdf')
        plt.clf()


def draw_scatter_figure(X, Y, xlabel='', ylabel='', title = ''):
    # Draw plot
    fig, ax = plt.subplots(figsize=(16,10), dpi= 80)
    # ax.vlines(x=, ymin=0, ymax=df.cty, color='firebrick', alpha=0.7, linewidth=2)
    ax.scatter(x=X, y=Y, s=75, color='firebrick', alpha=0.7)

    # Title, Label, Ticks and Ylim
    ax.set_title(title, fontdict={'size':22})
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    # ax.set_xticks(X)

    # Annotate
    # for row in X:
    #     ax.text(row.Index, row.cty+.5, s=round(row.cty, 2), horizontalalignment= 'center', verticalalignment='bottom', fontsize=14)

    plt.savefig(title)

    plt.clf()

def draw_plot_figure(X, Y, xlabel='', ylabel='', title = ''):
    # Draw plot
    fig, ax = plt.subplots(figsize=(16,10), dpi= 80)
    # ax.vlines(x=, ymin=0, ymax=df.cty, color='firebrick', alpha=0.7, linewidth=2)
    ax.plot(X, Y, color='firebrick')

    # Title, Label, Ticks and Ylim
    ax.set_title(title, fontdict={'size':22})
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    # ax.set_xticks(X)

    # Annotate
    # for row in X:
    #     ax.text(row.Index, row.cty+.5, s=round(row.cty, 2), horizontalalignment= 'center', verticalalignment='bottom', fontsize=14)

    plt.savefig(title)

    plt.clf()

def draw_bar_figure(X, Y, xlabel='', ylabel='', title = ''):
    # Draw plot
    # fig, ax = plt.subplots(figsize=(16,10), dpi= 80)
    # ax.vlines(x=, ymin=0, ymax=df.cty, color='firebrick', alpha=0.7, linewidth=2)
    plt.bar(X, Y, color='firebrick')

    # Title, Label, Ticks and Ylim
    plt.title(title, fontdict={'size':22})
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    # ax.set_xticks(X)

    # Annotate
    # for row in X:
    #     ax.text(row.Index, row.cty+.5, s=round(row.cty, 2), horizontalalignment= 'center', verticalalignment='bottom', fontsize=14)

    plt.savefig(title+'.pdf')

    plt.clf()

def draw_points(X, Y, marker_shape = '.'):
    if marker_shape == 'o':
        plt.scatter(X, Y, marker=marker_shape, c = 'white', edgecolors='black', s = 150, linewidths=2)
    else:
        plt.scatter(X, Y, marker=marker_shape, color='black')


def draw_arrows(E, ):
    for e in E:
        plt.annotate("", xy=(e[1][0], e[1][1]), xytext=(e[0][0], e[0][1]), arrowprops=dict(arrowstyle="->"),size=24)