#ifndef HYBRID_ANALYSIS_H_
#define HYBRID_ANALYSIS_H_

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "core/cfg.h"
#include "core/pag.h"
#include "core/pcg.h"

#include "baguatool.h"

namespace baguatool::core {

// class HybridAnalysis {
//  private:
//   std::map<std::string, ControlFlowGraph*> func_cfg_map;
//   ProgramCallGraph* pcg;
//   std::map<std::string, ProgramAbstractionGraph*> func_pag_map;
//   ProgramAbstractionGraph* pag;

//  public:
//   HybridAnalysis() {}
//   ~HybridAnalysis() {}

//   /** Control Flow Graph of Each Function **/

//   void ReadStaticControlFlowGraphs(const char* dir_name);

//   void GenerateControlFlowGraphs();

//   std::unique_ptr<ControlFlowGraph> GetControlFlowGraph(std::string func_name);

//   std::map<std::string, std::unique_ptr<ControlFlowGraph>>& GetControlFlowGraphs();

//   /** Program Call Graph **/

//   void ReadStaticProgramCallGraph();

//   void ReadDynamicProgramCallGraph();

//   void GenerateProgramCallGraph();

//   std::unique_ptr<ProgramCallGraph> GetProgramCallGraph();

//   /** Intra-procedural Analysis **/

//   std::unique_ptr<ProgramAbstractionGraph> GetFunctionAbstractionGraph(std::string func_name);

//   std::map<std::string, std::unique_ptr<ProgramAbstractionGraph>>& GetFunctionAbstractionGraphs();

//   void IntraProceduralAnalysis();

//   /** Inter-procedural Analysis **/

//   void InterProceduralAnalysis();

//   void GenerateProgramAbstractionGraph();

//   // void ConnectCallerCallee(ProgramAbstractionGraph* pag, int vertex_id, void* extra);
// };  // class HybridAnalysis

}  // namespace baguatool::core

#endif