#include <chrono>
#include <cstring>
#include <map>

#include "graph_perf.h"

int main(int argc, char **argv) {
  /** Setups */
  const char *bin_name = argv[1];
  const char *data_dir = argv[2];

  /** == Read static data == */
  auto graph_perf = std::make_unique<graph_perf::GPerf>();
  // pag gml
  std::string pag_dir_name = std::string(data_dir) +
                             std::string("/static_data/") +
                             std::string(bin_name) + std::string(".pag/");
  /** Read confrol-flow graph */
  auto st = std::chrono::system_clock::now();
  graph_perf->ReadFunctionAbstractionGraphs(pag_dir_name.c_str());
  auto ed = std::chrono::system_clock::now();
  double time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Reading CFG costs " << time << " seconds." << std::endl;

  /** == Read dynamic data == */
  int num_procs = atoi(argv[3]);
  baguatool::core::PerfData *perf_data = new baguatool::core::PerfData();
  st = std::chrono::system_clock::now();
  for (int pid = 0; pid < num_procs; pid++) {
    std::string perf_data_file_name = std::string(data_dir) +
                                      std::string("/dynamic_data/SAMPLE+") +
                                      std::to_string(pid) + std::string(".TXT");
    perf_data->Read(perf_data_file_name.c_str());
  }
  baguatool::core::PerfData *comm_data = new baguatool::core::PerfData();
  std::string comm_data_file_name = std::string(data_dir) +
                                    std::string("/dynamic_data/") +
                                    std::string(argv[5]);
  comm_data->Read(comm_data_file_name.c_str());
  perf_data->Read(comm_data_file_name.c_str());
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Reading perf data costs " << time << " seconds." << std::endl;

  /** == Process dynamic data to prune == */
  st = std::chrono::system_clock::now();
  // profiling data
  std::map<baguatool::type::procs_t, baguatool::collector::SharedObjAnalysis *>
      all_shared_obj_analysis;
  for (int pid = 0; pid < num_procs; pid++) {
    std::string somap_file_name_str = std::string(data_dir) +
                                      std::string("/dynamic_data/SOMAP+") +
                                      std::to_string(pid) + std::string(".TXT");
    baguatool::collector::SharedObjAnalysis *shared_obj_analysis =
        new baguatool::collector::SharedObjAnalysis();
    shared_obj_analysis->ReadSharedObjMap(somap_file_name_str);
    all_shared_obj_analysis[pid] = shared_obj_analysis;
  }
  std::string bin_name_str = std::string(bin_name);
  graph_perf->GenerateDynAddrDebugInfo(perf_data, all_shared_obj_analysis,
                                       bin_name_str);
  // communication data
  graph_perf->GenerateDynAddrDebugInfo(comm_data, all_shared_obj_analysis,
                                       bin_name_str);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Processing dynamic perf data costs " << time << " seconds."
       << std::endl;

  /** == Read program call graph == */
  // pcg gml
  std::string pcg_name = std::string(data_dir) + std::string("/static_data/") +
                         std::string(bin_name) + std::string(".pcg");
  st = std::chrono::system_clock::now();
  graph_perf->GenerateProgramCallGraph(pcg_name.c_str(), perf_data);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Reading and generating PCG cost " << time << " seconds."
       << std::endl;
  // graph_perf->GetProgramCallGraph()->DumpGraphGML("hy_pcg.gml");

  /** == Prune with dynamic data == */
  graph_perf->PruneWithDynamicData();

  /** == Generate program abstraction graph == */
  st = std::chrono::system_clock::now();
  graph_perf->GenerateProgramAbstractionGraph(perf_data);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Generating PAG costs " << time << " seconds." << std::endl;
  baguatool::core::ProgramAbstractionGraph *pag =
      graph_perf->GetProgramAbstractionGraph();
  std::string output_static_pag_name =
      std::string(data_dir) + std::string("/static_pag.gml");
  pag->DumpGraphGML(output_static_pag_name.c_str());

  /** == Data embedding == */
  st = std::chrono::system_clock::now();
  graph_perf->DataEmbedding(perf_data);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Data embedding costs " << time << " seconds." << std::endl;

  /** == Reduce data == */
  st = std::chrono::system_clock::now();
  std::string metric("TOT_CYC");
  std::string op("SUM");
  baguatool::type::perf_data_t total;
  total = pag->ReduceVertexPerfData(metric, op);
  std::string avg_metric("TOT_CYC_SUM");
  std::string new_metric("CYCAVGPERCENT");
  pag->ConvertVertexReducedDataToPercent(avg_metric, total, new_metric);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Convert reduced data to percent costs " << time << " seconds."
       << std::endl;

  st = std::chrono::system_clock::now();
  pag->PreserveHotVertices("CYCAVGPERCENT");
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Pruning costs " << time << " seconds." << std::endl;

  st = std::chrono::system_clock::now();
  std::string output_pag_name = std::string(data_dir) + std::string("/pag.gml");
  pag->DumpGraphGML(output_pag_name.c_str());
  auto graph_perf_data = pag->GetGraphPerfData();
  std::string output_pag_perf_data_name =
      std::string(data_dir) + std::string("/output.json");
  graph_perf_data->Dump(output_pag_perf_data_name);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Dumpping pag & pag perf data costs " << time << " seconds."
       << std::endl;

  /** MPAG generation*/

  st = std::chrono::system_clock::now();
  int starting_vertex = atoi(argv[4]);
  graph_perf->GenerateMultiProcessProgramAbstractionGraph(starting_vertex,
                                                          num_procs);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Generating MPAG costs " << time << " seconds." << std::endl;

  st = std::chrono::system_clock::now();
  graph_perf->AddCommEdgesToMPAG(comm_data);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Adding comm edges costs " << time << " seconds." << std::endl;

  /** == Dump mpag == */
  auto mpag = graph_perf->GetMultiProgramAbstractionGraph();
  // pag to mpag vertices & edges
  st = std::chrono::system_clock::now();
  std::string output_pag_to_mpag_map_name =
      std::string(data_dir) + std::string("/pag_to_mpag.json");
  mpag->DumpPagToMpagMap(output_pag_to_mpag_map_name);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Dumpping pag-mpag map costs " << time << " seconds." << std::endl;
  // perf data of mpag
  st = std::chrono::system_clock::now();
  auto mpag_graph_perf_data = mpag->GetGraphPerfData();
  std::string output_mpag_perf_data_name =
      std::string(data_dir) + std::string("/mpag_perf_data.json");
  mpag_graph_perf_data->Dump(output_mpag_perf_data_name);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Dumpping mpag perf data costs " << time << " seconds." << std::endl;

  /** == Reduce data == */
  st = std::chrono::system_clock::now();
  total = 0;
  total = mpag->ReduceVertexPerfData(metric, op);
  printf("%lf\n", total);
  mpag->ConvertVertexReducedDataToPercent(avg_metric, total / num_procs,
                                          new_metric);
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Reduce data costs " << time << " seconds." << std::endl;

  /** == Dump mpag == */
  // mpag vertices & edges
  st = std::chrono::system_clock::now();
  std::string output_mpag_name =
      std::string(data_dir) + std::string("/mpi_mpag.gml");
  mpag->DumpGraphGML(output_mpag_name.c_str());
  ed = std::chrono::system_clock::now();
  time =
      std::chrono::duration_cast<std::chrono::microseconds>(ed - st).count() /
      1e6;
  cout << "Dumpping mpag costs " << time << " seconds." << std::endl;

  for (auto &kv : all_shared_obj_analysis) {
    delete kv.second;
  }
  FREE_CONTAINER(all_shared_obj_analysis);
}