#ifndef STATIC_ANALYSIS_H_
#define STATIC_ANALYSIS_H_

#include <CFG.h>
#include <CodeObject.h>
#include <InstructionDecoder.h>
#include <LineInformation.h>
#include <Symtab.h>

#include <cxxabi.h>  // needed for abi::__cxa_demangle
#include <map>
#include <unordered_map>

#include "baguatool.h"
#include "common/common.h"
#include "core/pag.h"

using namespace Dyninst;
using namespace ParseAPI;
// using namespace SymtabAPI;
using namespace InstructionAPI;
typedef unsigned long int VMA;
typedef long int VMASigned;  // useful for offsets

//#define DEBUG_COUT

#ifndef MAX_LOOP_DEPTH
#define MAX_LOOP_DEPTH 2
#endif

#define MAX_STR_LEN 256

#define VMA_MAX (~((unsigned long int)(0)))
#define PTR_TO_BFDVMA(x) ((unsigned long int)(uintptr_t)(x))
#define BFDVMA_TO_PTR(x, totype) ((totype)(uintptr_t)(x))

#define PTR_TO_VMA(x) PTR_TO_BFDVMA(x)
#define VMA_TO_PTR(x, totype) BFDVMA_TO_PTR(x, totype)

namespace baguatool::collector {

class StaticAnalysisImpl {
 private:
  SymtabCodeSource *sts;
  CodeObject *co;
  std::unordered_map<Block *, bool> visited_block_map;
  std::unordered_map<Address, std::string> addr_2_func_name;
  std::map<VMA, VMA> call_graph_map;

  std::unique_ptr<core::ProgramCallGraph> pcg;

  // std::unordered_map<std::string, core::ControlFlowGraph *> func_2_graph;
  std::unordered_map<Address, core::ControlFlowGraph *> entry_addr_to_graph;
  char binary_name[MAX_STR_LEN];

 public:
  StaticAnalysisImpl(char *binary_name);

  ~StaticAnalysisImpl();

  void IntraProceduralAnalysis();
  void ExtractLoopStructure(core::ControlFlowGraph *func_struct_graph, LoopTreeNode *loop_tree, int depth,
                            int parent_id);
  void ExtractCallStructure(core::ControlFlowGraph *func_struct_graph, std::vector<Block *> &bvec, int parent_id);
  void InterProceduralAnalysis();
  void CaptureProgramCallGraph();
  void DumpFunctionGraph(core::ControlFlowGraph *func_struct_graph, const char *file_name);
  void DumpAllFunctionGraph();
  void GetBinaryName();
  void DumpProgramCallGraph();
};
}  // namespace baguatool::graph_sd

#endif