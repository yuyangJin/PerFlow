#include "graph_perf.h"
#include <cstring>
#include <map>

int main(int argc, char **argv) {
  /** Setups */
  const char *bin_name = argv[1];
  const char *data_dir = argv[2];

  /** == Read static data == */
  auto graph_perf = std::make_unique<graph_perf::GPerf>();
  // pag gml
  char pag_dir_name[256] = {0};
  strcpy(pag_dir_name, data_dir);
  strcat(pag_dir_name, "/static_data/");
  strcat(pag_dir_name, bin_name);
  strcat(pag_dir_name, ".pag/");
  /** Read confrol-flow graph */
  graph_perf->ReadFunctionAbstractionGraphs(pag_dir_name);

  /** == Read dynamic data == */
  int num_procs = atoi(argv[3]);
  baguatool::core::PerfData *perf_data = new baguatool::core::PerfData();
  // for (int i = 5; i < argc; i++) {
  //   const char* perf_data_file_name = argv[i];
  //   perf_data->Read(perf_data_file_name);
  // }
  for (int pid = 0; pid < num_procs; pid++) {
    std::string perf_data_file_name = std::string(data_dir) +
                                      std::string("/dynamic_data/SAMPLE+") +
                                      std::to_string(pid) + std::string(".TXT");
    perf_data->Read(perf_data_file_name.c_str());
  }

  /** Read program call graph */
  // pcg gml
  char pcg_name[256] = {0};
  strcpy(pcg_name, data_dir);
  strcat(pcg_name, "/static_data/");
  strcat(pcg_name, bin_name);
  strcat(pcg_name, ".pcg");
  graph_perf->GenerateProgramCallGraph(pcg_name, perf_data);
  // graph_perf->GetProgramCallGraph()->DumpGraphGML("hy_pcg.gml");

  /** Generate program abstraction graph */
  graph_perf->GenerateProgramAbstractionGraph(perf_data);
  baguatool::core::ProgramAbstractionGraph *pag =
      graph_perf->GetProgramAbstractionGraph();
  // char static_pag_name[256] = {0};
  // strcpy(static_pag_name, data_dir);
  // strcat(static_pag_name, "/");
  // strcat(static_pag_name, bin_name);
  std::string output_static_pag_name =
      std::string(data_dir) + std::string("/static_pag.gml");
  pag->DumpGraphGML(output_static_pag_name.c_str());

  /** Data embedding */
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

  graph_perf->DataEmbedding(perf_data);

  std::string metric("TOT_CYC");
  std::string op("SUM");
  baguatool::type::perf_data_t total;
  total = pag->ReduceVertexPerfData(metric, op);
  std::string avg_metric("TOT_CYC_SUM");
  std::string new_metric("CYCAVGPERCENT");
  pag->ConvertVertexReducedDataToPercent(avg_metric, total, new_metric);

  std::string output_pag_name = std::string(data_dir) + std::string("/pag.gml");
  pag->DumpGraphGML(output_pag_name.c_str());
  auto graph_perf_data = pag->GetGraphPerfData();
  std::string output_pag_perf_data_name =
      std::string(data_dir) + std::string("/output.json");
  graph_perf_data->Dump(output_pag_perf_data_name);

  /** MPAG generation*/

  int starting_vertex = atoi(argv[4]);
  graph_perf->GenerateMultiProcessProgramAbstractionGraph(starting_vertex,
                                                          num_procs);
  baguatool::core::PerfData *comm_data = new baguatool::core::PerfData();
  // const char *comm_data_file_name = argv[5];
  std::string comm_data_file_name =
      std::string(data_dir) + std::string("/dynamic_data/") + std::string(argv[5]);
  comm_data->Read(comm_data_file_name.c_str());
  graph_perf->GenerateDynAddrDebugInfo(comm_data, all_shared_obj_analysis,
                                       bin_name_str);

  graph_perf->AddCommEdgesToMPAG(comm_data);

  auto mpag = graph_perf->GetMultiProgramAbstractionGraph();

  std::string output_pag_to_mpag_map_name =
      std::string(data_dir) + std::string("/pag_to_mpag.json");

  // std::string pag_to_mpag_map_str("pag_to_mpag.json");
  mpag->DumpPagToMpagMap(output_pag_to_mpag_map_name);

  auto mpag_graph_perf_data = mpag->GetGraphPerfData();
  // std::string mpag_output_file_name_str("mpag_perf_data.json");
  std::string output_mpag_perf_data_name =
      std::string(data_dir) + std::string("/mpag_perf_data.json");
  mpag_graph_perf_data->Dump(output_mpag_perf_data_name);

  total = 0;
  total = mpag->ReduceVertexPerfData(metric, op);
  printf("%lf\n", total);
  mpag->ConvertVertexReducedDataToPercent(avg_metric, total / num_procs,
                                          new_metric);
  std::string output_mpag_name =
      std::string(data_dir) + std::string("/mpi_mpag.gml");
  mpag->DumpGraphGML(output_mpag_name.c_str());

  for (auto &kv : all_shared_obj_analysis) {
    delete kv.second;
  }
  FREE_CONTAINER(all_shared_obj_analysis);
}