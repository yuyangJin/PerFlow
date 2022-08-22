import os
import json
import time
import numpy
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
        cmd_line = '$BAGUA_DIR/build/builtin/binary_analyzer ' + self.static_analysis_binary_name + ' ' + self.data_dir + '/static_data'
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
        

    def pagGeneration(self):
        pag_generation_cmd_line = ''

        if self.mode == 'mpi+omp':
            communication_analysis_cmd_line = '$BAGUA_DIR/build/builtin/comm_dep_approxi_analysis ' + str(self.nprocs) + ' ' + self.data_dir + ' ' + self.static_analysis_binary_name + '.dep'
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/mpi_pag_generation ' + self.static_analysis_binary_name + ' ' + self.data_dir + ' ' + str(self.nprocs) + ' ' + '0' + ' ' + self.static_analysis_binary_name + '.dep' #+ ' ./SAMPLE*'
            print(communication_analysis_cmd_line)
            os.system(communication_analysis_cmd_line)


        if self.mode == 'omp':
            pag_generation_cmd_line = '$BAGUA_DIR/build/builtin/tools/omp_pag_generation ' + self.static_analysis_binary_name + ' ./SAMPLE*TXT ./SOMAP*.TXT'
            print(communication_analysis_cmd_line)
            os.system(communication_analysis_cmd_line)
            self.tdpag_file = 'pag.gml'
            self.ppag_file = 'mpi_mpag.gml'

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

        self.readPag()
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
    
    
    def imbalance_analysis(self, V, nprocs = 1):
        for v in V:
            if str(int(v['id'])) in self.tdpag_perf_data.keys():
                # print(int(v['id']), self.tdpag_perf_data[str(int(v['id']))])
                data = self.tdpag_perf_data[str(int(v['id']))]
                for metric, metric_data in data.items():
                    # gather all procs data
                    metric_data_list = [0 for _ in range(nprocs)]
                    for procs, procs_data in metric_data.items():
                        # gather all thread data
                        procs_data_tot_num = 0.0
                        for thread, thread_data in procs_data.items():
                            procs_data_tot_num += thread_data
                        # metric_data_list.append(procs_data_tot_num)
                        metric_data_list[int(procs)] = procs_data_tot_num
                    
                    # Calculate variance
                    mean = numpy.mean(metric_data_list)
                    var = numpy.var(metric_data_list)
                    std_var = numpy.std(metric_data_list)
                    if std_var > 70:
                        # print(metric, "mean:", mean, "variance:", var, "standard variance:", std_var)

                        # Show the figures
                        # metric_data_array = []

                        # draw_scatter_figure(numpy.arange(nprocs), metric_data_list, xlabel='Process ID', ylabel='Execution Cycles', title = str(int(v['id'])) + '-' + v['name'] )
                        # draw_plot_figure(numpy.arange(nprocs), metric_data_list, xlabel='Process ID', ylabel='Execution Cycles', title = str(int(v['id'])) + '-' + v['name'] )
                        draw_bar_figure(numpy.arange(nprocs), metric_data_list, xlabel='Process ID', ylabel='Execution Cycles', title = str(int(v['id'])) + '-' + v['name'] )
                        
                        
    def process_similarity_analysis(self, V, nprocs, metric = 'TOT_CYC'):
        ''' process character vector
        '''
        charact_vec = [[] for _ in range(nprocs)]

        for v in V:
            vid = int(v['id'])
            if str(vid) in self.tdpag_perf_data.keys():
                if metric in self.tdpag_perf_data[str(vid)].keys():
                    data = self.tdpag_perf_data[str(vid)][metric]
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
                for i in range(nprocs):
                    charact_vec[i].append(0)
            else:
                for i in range(nprocs):
                    charact_vec[i].append(0)

        # for procs_charact_vec in charact_vec:
        #     print(procs_charact_vec, end = ' ')
        # print()

        charact_vec = normalize(charact_vec)

        # Calculate dot
        similarity_mat = numpy.zeros(nprocs * nprocs).reshape(nprocs, nprocs)
        for i in range(nprocs):
            for j in range(i, nprocs):
                value = numpy.dot(charact_vec[i], charact_vec[j])
                similarity_mat[i][j] = value
                similarity_mat[j][i] = value
        plt.imshow(similarity_mat, cmap=plt.cm.binary)
        plt.colorbar()
        plt.savefig("sim_mat.pdf")
        plt.clf()
        # plt.show()
        # print(similarity_mat)

        # Hierarchical clustering
        euclidean_dist = numpy.zeros(nprocs * nprocs).reshape(nprocs, nprocs)
        for i in range(nprocs):
            for j in range(i + 1, nprocs):
                value = numpy.linalg.norm(numpy.array(charact_vec[i]) - numpy.array(charact_vec[j]))
                euclidean_dist[i][j] = value
                euclidean_dist[j][i] = value
        # plt.imshow(euclidean_dist, cmap=plt.cm.binary)
        plt.pcolormesh(euclidean_dist, cmap=plt.cm.binary)
        plt.colorbar()
        plt.savefig("euc_dist_mat.pdf")
        plt.clf()

        A=[]
        for i in range(len(charact_vec)):
            a=chr(i+ord('A'))
            A.append(a)

        

        # euclidean_dist = scipy.cluster.hierarchy.distance.pdist(charact_vec, 'euclidean')
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'ward', metric='euclidean', optimal_ordering=True)
        clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'ward', metric='euclidean')
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'single', metric='euclidean')
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'average', metric='euclidean')
        # clusters = scipy.cluster.hierarchy.linkage(charact_vec, method = 'complete', metric='euclidean')
        scipy.cluster.hierarchy.dendrogram(clusters, labels=numpy.arange(nprocs))
        plt.savefig("cluster_results.pdf")
        plt.clf()

        print(clusters)


        # model = AgglomerativeClustering(n_clusters=None)
        # model = model.fit(charact_vec)
        # labels = model.fit_predict(charact_vec)
        # x = range(1,len(model.distances_)+1)

    def communication_pattern_analysis(self, V, nprocs=1):
        comm_pattern_mat = numpy.zeros(nprocs * nprocs).reshape(nprocs, nprocs)
        # if V.attrbutes.keys().__contains__('time'):
        for v in V:
            for pid in range(nprocs):
                vid = str(int(v['id']))
                ppag_vid = self.tdpag_to_ppag_map[vid][str(pid)]['0']
                ppag_v = self.ppag.vs[int(ppag_vid)]
                # e_list = self.ppag.es.select(_source=int(ppag_vid))
                edges = ppag_v.out_edges()
                # print(ppag_vid, edges)
                for e in edges:
                    # if (e['time'] != None) and (e['time'] != float('inf')) and (e['time'] != float('nan')):
                    if e.attributes().__contains__('time'):
                        if (e['time'] != float('nan')):
                            ''' the return value of e.target is vertex id (python-igraph id), but not the attribute 'id' of vertex, the latter one is what we need.'''
                            ppag_target_v = self.ppag.vs[e.target]
                            ppag_target_vid = int(ppag_target_v['id'])
                            # matched_flag = False
                            # for dest_v in V:
                            #     dest_vid = str(int(dest_v['id']))
                            #     for dest_pid, dest_vid in self.tdpag_to_ppag_map[dest_vid].items():
                            #         ppag_target_vid = int(ppag_target_v['id'])
                            #         # print(dest_vid['0'], ppag_target_vid) 
                                    
                            #         if dest_vid['0'] == ppag_target_vid: 
                            #             # print(dest_vid['0'], ppag_vid, ppag_target_vid, e['time'])
                            #             comm_pattern_mat[pid][int(dest_pid)] += float(e['time'])
                            #             comm_pattern_mat[int(dest_pid)][pid] += float(e['time'])
                            #             matched_flag = True
                            #             break
                            #     if matched_flag == True:
                            #         break
                            # print(ppag_target_vid)
                            if self.ppag_to_tdpag_map.keys().__contains__(ppag_target_vid):
                                tdpag_info = self.ppag_to_tdpag_map[ppag_target_vid]
                                # print(ppag_target_vid, tdpag_info)
                                # dest_vid = tdpag_info[0]
                                dest_pid = tdpag_info[1]
                                comm_pattern_mat[pid][dest_pid] += float(e['time'])
                                comm_pattern_mat[dest_pid][pid] += float(e['time'])

        plt.pcolormesh(comm_pattern_mat, cmap=plt.cm.binary, vmin = 0.0, edgecolors='grey', linewidths=0.1)
        plt.colorbar(label = "Time(ms)")
        plt.title("Communication Pattern")
        plt.xlabel("Process ID")
        plt.ylabel("Process ID")
        plt.savefig("comm_pattern.pdf")
        plt.clf()
        
                     
    
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
        if mark_edges != []:
            graphviz_output.draw_edge(mark_edges, color=0.1)
        
        graphviz_output.show()


    # builtin models / diagrams

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

    def communication_pattern_analysis_model(self, nprocs=1):
        ## a filter pass
        V_comm = self.filter(self.tdpag.vs, name = "mpi_")
        if len(V_comm) == 0:
            V_comm = self.filter(self.tdpag.vs, name = "MPI_")
        ## a communication pattern analysis pass
        self.communication_pattern_analysis(V_comm, nprocs)





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
    fig, ax = plt.subplots(figsize=(16,10), dpi= 80)
    # ax.vlines(x=, ymin=0, ymax=df.cty, color='firebrick', alpha=0.7, linewidth=2)
    ax.bar(X, Y, color='firebrick')

    # Title, Label, Ticks and Ylim
    ax.set_title(title, fontdict={'size':22})
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    # ax.set_xticks(X)

    # Annotate
    # for row in X:
    #     ax.text(row.Index, row.cty+.5, s=round(row.cty, 2), horizontalalignment= 'center', verticalalignment='bottom', fontsize=14)

    plt.savefig(title+'.pdf')

    plt.clf()