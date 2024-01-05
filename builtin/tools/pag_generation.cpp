#include "baguatool.h"
#include "graph_perf.h"
#include <cstring>
#include <string>

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

  // char pag_dir_name[20] = {0};
  // char pcg_name[20] = {0};
  // strcpy(pag_dir_name, bin_name);
  // strcat(pag_dir_name, ".pag/");
  // strcpy(pcg_name, bin_name);
  // strcat(pcg_name, ".pcg");
  // const char *perf_data_file_name = argv[2];
  // std::string shared_obj_map_file_name = std::string(argv[3]);

  /** == Read dynamic data == */
  baguatool::core::PerfData *perf_data = new baguatool::core::PerfData();
  st = std::chrono::system_clock::now();
  std::string perf_data_file_name = std::string(data_dir) +
                                      std::string("/dynamic_data/SAMPLE+0.TXT");
  perf_data->Read(perf_data_file_name.c_str());
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
    std::string somap_file_name_str = std::string(data_dir) +
                                      std::string("/dynamic_data/SOMAP+0.TXT");
    baguatool::collector::SharedObjAnalysis *shared_obj_analysis =
        new baguatool::collector::SharedObjAnalysis();
    shared_obj_analysis->ReadSharedObjMap(somap_file_name_str);
    all_shared_obj_analysis[0] = shared_obj_analysis;
  std::string bin_name_str = std::string(bin_name);
  graph_perf->GenerateDynAddrDebugInfo(perf_data, all_shared_obj_analysis,
                                       bin_name_str);



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


  for (auto &kv : all_shared_obj_analysis) {
    delete kv.second;
  }
  FREE_CONTAINER(all_shared_obj_analysis);




//   auto gperf = std::make_unique<graph_perf::GPerf>();

//   gperf->ReadFunctionAbstractionGraphs(pag_dir_name);

//   /** Read perf data */
//   baguatool::core::PerfData *perf_data = new baguatool::core::PerfData();
//   perf_data->Read(perf_data_file_name);

//   // gperf->GenerateDynAddrDebugInfo(perf_data, shared_obj_map_file_name);
//   gperf->GenerateProgramCallGraph(bin_name, perf_data);
//   gperf->GetProgramCallGraph()->DumpGraphGML("hy_pcg.gml");

//   gperf->GenerateProgramAbstractionGraph(perf_data);

//   baguatool::core::ProgramAbstractionGraph *pag =
//       gperf->GetProgramAbstractionGraph();

//   gperf->DataEmbedding(perf_data);
//   std::string metric("TOT_CYC");
//   std::string op("SUM");
//   baguatool::type::perf_data_t total = pag->ReduceVertexPerfData(metric, op);
//   std::string avg_metric("TOT_CYC_SUM");
//   std::string new_metric("CYCAVGPERCENT");
//   pag->ConvertVertexReducedDataToPercent(avg_metric, total, new_metric);

//   auto graph_perf_data =
//       gperf->GetProgramAbstractionGraph()->GetGraphPerfData();
//   std::string output_file_name_str("output.json");
//   graph_perf_data->Dump(output_file_name_str);

//   // gperf->GetProgramAbstractionGraph()->PreserveHotVertices("CYCAVGPERCENT");

//   gperf->GetProgramAbstractionGraph()->DumpGraphGML("pag.gml");
}