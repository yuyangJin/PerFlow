#include "graph_perf.h"
#include <cstring>

int main(int argc, char **argv) {
  const char *bin_name = argv[1];
  char pag_dir_name[20] = {0};
  char pcg_name[20] = {0};
  strcpy(pag_dir_name, bin_name);
  strcat(pag_dir_name, ".pag/");
  strcpy(pcg_name, bin_name);
  strcat(pcg_name, ".pcg");

  auto gperf = std::make_unique<graph_perf::GPerf>();

  gperf->ReadFunctionAbstractionGraphs(pag_dir_name);
  gperf->ReadStaticProgramCallGraph(bin_name);
  // gperf->GetProgramCallGraph()->DumpGraphGML("hy_pcg.gml");

  gperf->GenerateStaticProgramAbstractionGraph();

  baguatool::core::ProgramAbstractionGraph *pag =
      gperf->GetProgramAbstractionGraph();
  pag->DumpGraphGML("static_pag.gml");
}