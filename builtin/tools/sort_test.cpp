#include "graph_perf.h"
#include <cstring>
#include <iostream>
#include <vector>

void print_vector(
    std::vector<baguatool::type::vertex_t> &pre_order_vertex_vec) {
  for (auto vid : pre_order_vertex_vec) {
    std::cout << vid << " -> ";
  }
  std::cout << std::endl;
}

int main(int argc, char **argv) {
  /** Preprocessing */
  const char *bin_name = argv[1];
  char pag_dir_name[20] = {0}, pcg_name[20] = {0};
  strcpy(pag_dir_name, bin_name);
  strcat(pag_dir_name, ".pag/");
  strcpy(pcg_name, bin_name);
  strcat(pcg_name, ".pcg");

  auto gperf = std::make_unique<graph_perf::GPerf>();

  /** Build graph's structure */
  gperf->ReadFunctionAbstractionGraphs(pag_dir_name);
  gperf->ReadStaticProgramCallGraph(bin_name);
  gperf->GenerateStaticProgramAbstractionGraph();
  baguatool::core::ProgramAbstractionGraph *pag =
      gperf->GetProgramAbstractionGraph();

  gperf->GetProgramAbstractionGraph()->DumpGraphGML("static_pag.gml");

  /** ======================================== */
  /** New feature: SortBy */
  std::vector<baguatool::type::vertex_t> pre_order_vertex_vec;
  pag->PreOrderTraversal(0, pre_order_vertex_vec); // before sorting
  std::cout << "Before: " << std::endl;
  print_vector(pre_order_vertex_vec);
  pre_order_vertex_vec.clear();

  pag->SortBy(
      0, "saddr"); // new function that you need to implement in src/graph.cpp

  pag->PreOrderTraversal(0, pre_order_vertex_vec); // after sorting
  std::cout << "After: " << std::endl;
  print_vector(pre_order_vertex_vec);
  /** ======================================== */
}