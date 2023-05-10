#include "graph_perf.h"
#include <cstring>
#include <string>

#define MAX_NUM_CORE 24

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
  // int num_procs = atoi(argv[3]);
  baguatool::core::PerfData *perf_data = new baguatool::core::PerfData();
  st = std::chrono::system_clock::now();
  // for (int pid = 0; pid < num_procs; pid++) {
    std::string perf_data_file_name = std::string(data_dir) +
                                      std::string("/dynamic_data/") +
                                      std::string(argv[3]);
    perf_data->Read(perf_data_file_name.c_str());
  // }
  // baguatool::core::PerfData *comm_data = new baguatool::core::PerfData();
  // std::string comm_data_file_name = std::string(data_dir) +
  //                                   std::string("/dynamic_data/") +
  //                                   std::string(argv[4]);
  // comm_data->Read(comm_data_file_name.c_str());
  // perf_data->Read(comm_data_file_name.c_str());
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
  // for (int pid = 0; pid < num_procs; pid++) {
    std::string somap_file_name_str = std::string(data_dir) +
                                      std::string("/dynamic_data/") +
                                      std::string(argv[3]);
    baguatool::collector::SharedObjAnalysis *shared_obj_analysis =
        new baguatool::collector::SharedObjAnalysis();
    shared_obj_analysis->ReadSharedObjMap(somap_file_name_str);
    all_shared_obj_analysis[0] = shared_obj_analysis;
  // }
  std::string bin_name_str = std::string(bin_name);
  graph_perf->GenerateDynAddrDebugInfo(perf_data, all_shared_obj_analysis,
                                       bin_name_str);
  // // communication data
  // graph_perf->GenerateDynAddrDebugInfo(comm_data, all_shared_obj_analysis,
  //                                      bin_name_str);
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


  std::string metric("TOT_CYC");
  graph_perf->OpenMPGroupThreadPerfData(metric, MAX_NUM_CORE);


  std::string op("SUM");
  baguatool::type::perf_data_t total = pag->ReduceVertexPerfData(metric, op);
  std::string avg_metric("TOT_CYC_SUM");
  std::string new_metric("CYCAVGPERCENT");
  pag->ConvertVertexReducedDataToPercent(avg_metric, total, new_metric);

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

  // pag->DeleteExtraTailVertices();
  // gperf->GetProgramAbstractionGraph()->PreserveHotVertices("CYCAVGPERCENT");

  // /** MPAG */
  // int num_threads = atoi(argv[3]);

  // graph_perf->GenerateOpenMPProgramAbstractionGraph(num_threads);
  // auto mpag = graph_perf->GetMultiProgramAbstractionGraph();

  // auto mpag_graph_perf_data = mpag->GetGraphPerfData();
  // std::string mpag_output_file_name_str("mpag_gpd.json");
  // mpag_graph_perf_data->Dump(mpag_output_file_name_str);

  // total = 0;
  // total = mpag->ReduceVertexPerfData(metric, op);
  // printf("%lf\n", total);
  // mpag->ConvertVertexReducedDataToPercent(avg_metric, total / num_threads,
  //                                         new_metric);

  // mpag->DumpGraphGML("omp_pag.gml");
}