#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
//#include <unistd.h>
//#include <bfd.h>
//#include <bfdlink.h>

#include <CFG.h>
#include <CodeObject.h>
#include <InstructionDecoder.h>
#include <LineInformation.h>
#include <Symtab.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "baguatool.h"
#include "common/utils.h"
#include "core/pag.h"
#include "dbg.h"
#include "static_analysis.h"

using namespace Dyninst;
using namespace ParseAPI;
// using namespace SymtabAPI;
using namespace InstructionAPI;

#define LOOP_GRANULARITY

namespace baguatool::collector {

StaticAnalysis::StaticAnalysis(char *binary_name) {
  this->sa = std::make_unique<StaticAnalysisImpl>(binary_name);
}

StaticAnalysis::~StaticAnalysis() {}

void StaticAnalysis::IntraProceduralAnalysis() {
  sa->IntraProceduralAnalysis();
}
void StaticAnalysis::InterProceduralAnalysis() {
  sa->InterProceduralAnalysis();
}
void StaticAnalysis::CaptureProgramCallGraph() {
  sa->CaptureProgramCallGraph();
}
void StaticAnalysis::CaptureProgramCallGraphMap() {
  sa->CaptureProgramCallGraphMap();
}
void StaticAnalysis::DumpAllControlFlowGraph(const char *dir) {
  sa->DumpAllFunctionGraph(dir);
}
void StaticAnalysis::DumpProgramCallGraph(const char *dir) {
  sa->DumpProgramCallGraph(dir);
}
void StaticAnalysis::DumpProgramCallGraphMap(const char *dir) {
  sa->DumpProgramCallGraphMap(dir);
}
void StaticAnalysis::GetBinaryName() { sa->GetBinaryName(); }

StaticAnalysisImpl::StaticAnalysisImpl(char *binary_name) {
  // Create a new binary code object from the filename argument
  this->sts = new SymtabCodeSource(binary_name);
  this->co = new CodeObject(this->sts);

  // Parse the binary
  this->co->parse();

  std::vector<std::string> binary_name_vec;
  split(binary_name, "/", binary_name_vec);

  strcpy(this->binary_name,
         binary_name_vec[binary_name_vec.size() - 1].c_str());

  // Get function address space
  std::string objdump_file =
      std::string(this->binary_name) + std::string(".obj");
  std::string objdump_cmd =
      std::string("objdump -t ") + std::string(this->binary_name) +
      std::string(" | grep \" F .text\" | awk '{print $1, $5}' > ") +
      objdump_file;

  system(objdump_cmd.c_str());

  ReadHashMap<Address, Address>(entry_addr_to_exit_addr, objdump_file);

  for (auto &kv : entry_addr_to_exit_addr) {
    kv.second += kv.first;
    // std::cout << kv.first << " " << kv.second << std::endl;
  }

  for (auto func : this->co->funcs()) {
    Address entry_addr = func->addr();
    Address exit_addr = 0;
    const ParseAPI::Function::blocklist &blist = func->blocks();
    if (entry_addr_to_exit_addr.find(entry_addr) !=
        entry_addr_to_exit_addr.end()) {
      exit_addr = entry_addr_to_exit_addr[entry_addr];
      // dbg(func->name() , entry_addr, exit_addr);
    } else {
      exit_addr = blist.back()->last();
      entry_addr_to_exit_addr[entry_addr] = exit_addr;
      // dbg(func->name(), entry_addr, exit_addr, "not found in objdump");
    }

    for (auto b : blist) {
      Address b_entry_addr = b->start();
      if (b_entry_addr >= entry_addr && b_entry_addr <= exit_addr) {
        entry_addr_to_bvec[entry_addr].insert(b);
      }
    }
  }
}

StaticAnalysisImpl::~StaticAnalysisImpl() {
  delete this->co;
  delete this->sts;

  FREE_CONTAINER(visited_block_map);
  FREE_CONTAINER(addr_2_func_name);
  FREE_CONTAINER(call_graph_map);
  FREE_CONTAINER(entry_addr_to_exit_addr);

  // TODO: it is better to use unique_ptr instead of raw pointer?
  // for (auto &it : func_2_graph) delete it.second;
  for (auto &it : entry_addr_to_graph)
    delete it.second;

  FREE_CONTAINER(entry_addr_to_graph);
}

// Capture a Program Call Graph (PCG)
void StaticAnalysisImpl::CaptureProgramCallGraph() {
  // Get function list
  const CodeObject::funclist &func_list = this->co->funcs();

  std::map<Address, type::vertex_t> addr_2_vertex_id;

  // Create a graph for each function
  // this->pcg = new core::ProgramCallGraph();
  this->pcg = std::make_unique<baguatool::core::ProgramCallGraph>();
  this->pcg->GraphInit("Program Call Graph");

  // Traverse through all functions
  for (auto func : func_list) {
    this->addr_2_func_name[func->addr()] = func->name();
    auto blist = func->blocks();
    Address exit_addr = blist.back()->last();

    // Add Function as a vertex
    type::vertex_t func_vertex_id = this->pcg->AddVertex();
    this->pcg->SetVertexBasicInfo(func_vertex_id, type::FUNC_NODE,
                                  func->name().c_str());
    this->pcg->SetVertexDebugInfo(func_vertex_id, func->addr(), exit_addr);
    addr_2_vertex_id[func->addr()] = func_vertex_id;
  }

  // // Traverse through all functions
  // for (auto func : func_list) {
  //   Address entry_addr = func->addr();
  //   Address exit_addr = entry_addr_to_exit_addr[entry_addr];
  //   const ParseAPI::Function::blocklist &blist = func->blocks();
  //   std::vector<Block *> bvec(blist.begin(), blist.end());
  //   std::vector<Block *>::iterator b = bvec.begin();
  //   while (b != bvec.end()) {
  //     Address b_entry_block = (*b)->start();
  //     if (b_entry_block < entry_addr || b_entry_block > exit_addr) {
  //       b = bvec.erase(b);
  //     } else {
  //       b++;
  //     }
  //   }
  //   type::vertex_t func_vertex_id = addr_2_vertex_id[func->addr()];

  //   // Traverse through all blocks
  //   for (auto b : bvec) {
  //     for (auto inst : b->targets()) {
  //       if (inst->type() == CALL) {
  //         Address entry_addr = inst->src()->last();
  //       }
  //     }
  //   }
  // }

  // Traverse through all functions
  for (auto func : func_list) {
    Address entry_addr = func->addr();
    Address exit_addr = entry_addr_to_exit_addr[entry_addr];
    const Function::edgelist &elist = func->callEdges();
    type::vertex_t func_vertex_id = addr_2_vertex_id[func->addr()];

    // Traverse through all function calls in this function
    for (const auto &e : elist) {
      VMA src_addr = e->src()->last();
      if (src_addr < entry_addr || src_addr > exit_addr) {
        continue;
      }
      VMA targ_addr = e->trg()->start();
      this->call_graph_map[src_addr] = targ_addr;

      // Add CALL Vertex as child of function vertex
      type::vertex_t call_vertex_id = this->pcg->AddVertex();
      this->pcg->SetVertexBasicInfo(call_vertex_id, type::CALL_NODE, "CALL");
      this->pcg->SetVertexDebugInfo(call_vertex_id, src_addr, src_addr);
      this->pcg->AddEdgeLazy(func_vertex_id, call_vertex_id);

      // Add Callee Function Vertex as child of CALL Vertex
      // TODO: Indirect call, this condition should be modified.
      if (addr_2_vertex_id[targ_addr] != 0) {
        this->pcg->AddEdgeLazy(call_vertex_id, addr_2_vertex_id[targ_addr]);
      }
    }
  }
  this->pcg->UpdateEdges();
}

void StaticAnalysisImpl::CaptureProgramCallGraphMap() {
  // Get function list
  const CodeObject::funclist &func_list = this->co->funcs();
  std::map<Address, type::vertex_t> addr_2_vertex_id;

  // Traverse through all functions
  for (auto func : func_list) {
    this->addr_2_func_name[func->addr()] = func->name();
    const Function::edgelist &elist = func->callEdges();
    type::vertex_t func_vertex_id = addr_2_vertex_id[func->addr()];

    // Traverse through all function calls in this function
    for (const auto &e : elist) {
      VMA src_addr = e->src()->last();
      VMA targ_addr = e->trg()->start();
      this->call_graph_map[src_addr] = targ_addr;
    }
  }
}

// Capture function call structure in this function but not in the loop
void StaticAnalysisImpl::ExtractCallStructure(core::ControlFlowGraph *func_cfg,
                                              std::vector<Block *> &bvec,
                                              int parent_id) {
  // Traverse through all blocks
  for (auto b : bvec) {
    // If block is visited, it means it is inside the loop
    if (!this->visited_block_map[b]) {
      this->visited_block_map[b] = true;

#ifndef LOOP_GRANULARITY
      /** Add BasciBlock Node **/
      Address bb_entry_addr = b->start();
      Address bb_exit_addr = b->end();
      type::vertex_t bb_vertex_id = func_cfg->AddVertex();
      func_cfg->SetVertexBasicInfo(bb_vertex_id, type::BB_NODE, "BB");
      func_cfg->SetVertexDebugInfo(bb_vertex_id, bb_entry_addr, bb_exit_addr);
      // Add an edge
      func_cfg->AddEdge(parent_id, bb_vertex_id);
#endif

      // Traverse through all instructions
      for (auto inst : b->targets()) {
        // Only analyze CALL type instruction
        if (inst->type() == CALL) {
          // dbg(inst->src()->last());
#ifdef DEBUG_COUT
          std::cout
              << "Call : "
              << decoder
                     .decode((unsigned char *)func->isrc()->getPtrToInstruction(
                         (*it)->src()->start()))
                     .format();
#endif
          Address entry_addr = inst->src()->last();
          Address exit_addr = inst->src()->last();
          std::string call_name =
              this->addr_2_func_name[this->call_graph_map[entry_addr]];
          type::vertex_t call_vertex_id = 0;

          // Add a CALL vertex, including MPI_CALL, INDIRECT_CALL, and CALL
          auto startsWith = [](const std::string &s,
                               const std::string &sub) -> bool {
            return s.find(sub) == 0;
          };
          if (startsWith(call_name, "MPI_") || startsWith(call_name, "_MPI_") ||
              startsWith(call_name, "mpi_") ||
              startsWith(call_name, "_mpi_")) { // MPI communication calls
            call_vertex_id = func_cfg->AddVertex();
            func_cfg->SetVertexBasicInfo(call_vertex_id, type::MPI_NODE,
                                         call_name.c_str());
          } else if (call_name.empty()) { // Function calls that are not
                                          // analyzed at static analysis
            call_vertex_id = func_cfg->AddVertex();
            func_cfg->SetVertexBasicInfo(call_vertex_id, type::CALL_IND_NODE,
                                         call_name.c_str());
          } else { // Common function calls
            call_vertex_id = func_cfg->AddVertex();
            func_cfg->SetVertexBasicInfo(call_vertex_id, type::CALL_NODE,
                                         call_name.c_str());
          }
          // dbg(call_vertex_id, call_name.c_str());

          func_cfg->SetVertexDebugInfo(call_vertex_id, entry_addr, exit_addr);

#ifndef LOOP_GRANULARITY
          // Add an edge
          func_cfg->AddEdge(bb_vertex_id, call_vertex_id);
#else
          func_cfg->AddEdgeLazy(parent_id, call_vertex_id);
#endif

#ifdef DEBUG_COUT
          for (int i = 0; i < 1; i++)
            cout << "  ";
          std::cout << "Call : " << std::call_name << " addr : " << std::hex
                    << entry_addr << " - " << exit_addr << std::dec << endl;
#endif
        } else {
#ifndef LOOP_GRANULARITY
          Address inst_entry_addr = inst->src()->last();
          Address inst_exit_addr = inst->src()->last();
          /** Add all non-call instructions as vertex **/
          type::vertex_t inst_vertex_id = func_cfg->AddVertex();
          func_cfg->SetVertexBasicInfo(inst_vertex_id, core::INST_NODE, "INS");
          func_cfg->SetVertexDebugInfo(inst_vertex_id, inst_entry_addr,
                                       inst_exit_addr);
          func_cfg->AddEdge(bb_vertex_id, inst_vertex_id);
#endif
        }
      }
    }
  }
}

// Capture function call structure in this function but not in the loop
void StaticAnalysisImpl::ExtractCallStructure(core::ControlFlowGraph *func_cfg,
                                              std::vector<Block *> &bvec,
                                              Function *func, int parent_id) {
  // std::vector<Address> call_inst_list;
  // const Function::edgelist &elist = func->callEdges();
  // for (const auto &e : elist) {
  //   call_inst_list.push_back(e->src()->last());
  // }
  // if (func->name().compare(std::string("main")) == 0) {
  //   for (auto call_inst: call_inst_list) {
  //     dbg(call_inst);
  //   }
  // }
  // Traverse through all blocks
  for (auto b : bvec) {
    // If block is visited, it means it is inside the loop
    if (!this->visited_block_map[b]) {
      this->visited_block_map[b] = true;

#ifndef LOOP_GRANULARITY
      /** Add BasciBlock Node **/
      Address bb_entry_addr = b->start();
      Address bb_exit_addr = b->end();
      type::vertex_t bb_vertex_id = func_cfg->AddVertex();
      func_cfg->SetVertexBasicInfo(bb_vertex_id, type::BB_NODE, "BB");
      func_cfg->SetVertexDebugInfo(bb_vertex_id, bb_entry_addr, bb_exit_addr);
      // Add an edge
      func_cfg->AddEdge(parent_id, bb_vertex_id);
#endif

      // Traverse through all instructions
      for (auto inst : b->targets()) {
        // Only analyze CALL type instruction
        if (inst->type() == CALL) {
          // if (inst->type() == CALL || std::find(call_inst_list.begin(),
          // call_inst_list.end(), inst->src()->last()) != call_inst_list.end())
          // {
          if (func->name().compare(std::string("main")) == 0) {
            // dbg(inst->src()->last());
          }
#ifdef DEBUG_COUT
          std::cout
              << "Call : "
              << decoder
                     .decode((unsigned char *)func->isrc()->getPtrToInstruction(
                         (*it)->src()->start()))
                     .format();
#endif
          Address entry_addr = inst->src()->last();
          Address exit_addr = inst->src()->last();
          std::string call_name =
              this->addr_2_func_name[this->call_graph_map[entry_addr]];
          type::vertex_t call_vertex_id = 0;

          // Add a CALL vertex, including MPI_CALL, INDIRECT_CALL, and CALL
          auto startsWith = [](const std::string &s,
                               const std::string &sub) -> bool {
            return s.find(sub) == 0;
          };
          if (startsWith(call_name, "MPI_") || startsWith(call_name, "_MPI_") ||
              startsWith(call_name, "mpi_") ||
              startsWith(call_name, "_mpi_")) { // MPI communication calls
            call_vertex_id = func_cfg->AddVertex();
            func_cfg->SetVertexBasicInfo(call_vertex_id, type::MPI_NODE,
                                         call_name.c_str());
          } else if (call_name.empty()) { // Function calls that are not
                                          // analyzed at static analysis
            call_vertex_id = func_cfg->AddVertex();
            func_cfg->SetVertexBasicInfo(call_vertex_id, type::CALL_IND_NODE,
                                         call_name.c_str());
          } else { // Common function calls
            call_vertex_id = func_cfg->AddVertex();
            func_cfg->SetVertexBasicInfo(call_vertex_id, type::CALL_NODE,
                                         call_name.c_str());
          }

          func_cfg->SetVertexDebugInfo(call_vertex_id, entry_addr, exit_addr);

#ifndef LOOP_GRANULARITY
          // Add an edge
          func_cfg->AddEdge(bb_vertex_id, call_vertex_id);
#else
          func_cfg->AddEdgeLazy(parent_id, call_vertex_id);
#endif

#ifdef DEBUG_COUT
          for (int i = 0; i < 1; i++)
            cout << "  ";
          std::cout << "Call : " << std::call_name << " addr : " << std::hex
                    << entry_addr << " - " << exit_addr << std::dec << endl;
#endif
        } else {
#ifndef LOOP_GRANULARITY
          Address inst_entry_addr = inst->src()->last();
          Address inst_exit_addr = inst->src()->last();
          /** Add all non-call instructions as vertex **/
          type::vertex_t inst_vertex_id = func_cfg->AddVertex();
          func_cfg->SetVertexBasicInfo(inst_vertex_id, core::INST_NODE, "INS");
          func_cfg->SetVertexDebugInfo(inst_vertex_id, inst_entry_addr,
                                       inst_exit_addr);
          func_cfg->AddEdge(bb_vertex_id, inst_vertex_id);
#endif
        }
      }
    }
  }
}

void StaticAnalysisImpl::ExtractLoopStructure(core::ControlFlowGraph *func_cfg,
                                              LoopTreeNode *loop_tree,
                                              int depth, int parent_id) {
  if (loop_tree == nullptr) {
    return;
  }

  // process the children of the loop tree
  std::vector<LoopTreeNode *> child_loop_list = loop_tree->children;
  std::unordered_map<Loop *, VMA> loop_min_entry_addr;
  for (auto loop_tree_node : child_loop_list) {
    auto loop = loop_tree_node->loop;
    VMA addr = 0;
    if (loop != nullptr) {
      std::vector<Block *> ent_blocks;
      int num_ents = loop->getLoopEntries(ent_blocks);
      if (num_ents >= 1) {
        addr = VMA_MAX;
        for (int i = 0; i < num_ents; ++i) {
          addr = std::min(addr, ent_blocks[i]->start());
        }
      }
    }

    loop_min_entry_addr[loop] = addr;
  }

  std::sort(child_loop_list.begin(), child_loop_list.end(),
            [&loop_min_entry_addr](LoopTreeNode *a, LoopTreeNode *b) -> bool {
              return loop_min_entry_addr[a->loop] <
                     loop_min_entry_addr[b->loop];
            });

  for (auto loop_tree_node : child_loop_list) {
    std::vector<Block *> blocks;
    loop_tree_node->loop->getLoopBasicBlocks(blocks);

    Address func_entry_addr =
        loop_tree_node->loop->getFunction()->entry()->start();
    Address func_exit_addr = entry_addr_to_exit_addr[func_entry_addr];

    // for (auto b: blocks) {
    std::vector<Block *>::iterator b = blocks.begin();
    while (b != blocks.end()) {
      Address b_entry_block = (*b)->start();
      if (b_entry_block < func_entry_addr || b_entry_block > func_exit_addr) {
        b = blocks.erase(b);
      } else {
        b++;
      }
    }

    if (!blocks.size()) {
      continue;
    }

    std::sort(blocks.begin(), blocks.end(), [](Block *a, Block *b) -> bool {
      return a->start() < b->start();
    });

    Address entry_addr = blocks.front()->start();
    Address exit_addr = blocks.back()->end();

    std::string loop_name = loop_tree_node->name();

    int loop_vertex_id = func_cfg->AddVertex();
    func_cfg->SetVertexBasicInfo(loop_vertex_id, type::LOOP_NODE,
                                 loop_name.c_str());
    func_cfg->SetVertexDebugInfo(loop_vertex_id, entry_addr - 8, exit_addr - 8);

    func_cfg->AddEdgeLazy(parent_id, loop_vertex_id);

#ifdef DEBUG_COUT
    for (int i = 0; i < depth; i++)
      cout << "  ";
    std::cout << "Loop : " << std::loop_name << " addr : " << std::hex
              << entry_addr << " - " << exit_addr << std::dec << std::endl;
#endif

    this->ExtractLoopStructure(func_cfg, loop_tree_node, depth + 1,
                               loop_vertex_id);
    if (loop_tree_node->numCallees() > 0) {
      this->ExtractCallStructure(func_cfg, blocks, loop_vertex_id);
    }
  }
}

// Extract structure graph for each function
void StaticAnalysisImpl::IntraProceduralAnalysis() {
  // Traverse through all functions
  for (auto func : this->co->funcs()) {
    Address entry_addr = func->addr();
    Address exit_addr = entry_addr_to_exit_addr[entry_addr];
    std::string func_name = func->name();
    // dbg(func_name );
    /** BUG: Hard coding for Q-E, and this BUG is caused by dyninst, not our
     * framework **/
    if (func->name().compare(
            std::string("__buiol_MOD_buiol_close_unit.cold")) == 0) {
      continue;
    }

    // Create a graph for each function
    auto func_cfg = new core::ControlFlowGraph();
    func_cfg->GraphInit(func_name.c_str());
    // this->func_2_graph[func_name] = func_cfg;
    this->entry_addr_to_graph[entry_addr] = func_cfg;

    const ParseAPI::Function::blocklist &blist = func->blocks();
    std::vector<Block *> bvec(blist.begin(), blist.end());
    // std::sort(bvec.begin(), bvec.end(), [](Block *a, Block *b) -> bool {
    //   return a->start() < b->start();
    // });

    // entry_addr = bvec.front()->start();
    // Address exit_addr = 0; //= bvec.back()->last();

    /** TODO: Need to verify the entry and exit address
     * of each function by parsing execuutable's objdump
     * logs. **/
    // if (entry_addr_to_exit_addr.find(entry_addr) !=
    // entry_addr_to_exit_addr.end())  {
    //   exit_addr = entry_addr_to_exit_addr[entry_addr];
    //   dbg(func_name, entry_addr, exit_addr);
    // } else {
    //   exit_addr = bvec.back()->last();
    //   dbg(func_name, entry_addr, exit_addr, "not found in objdump");
    // }

    // for (auto b: blocks) {
    std::vector<Block *>::iterator b = bvec.begin();
    while (b != bvec.end()) {
      Address b_entry_block = (*b)->start();
      if (b_entry_block < entry_addr || b_entry_block > exit_addr) {
        b = bvec.erase(b);
      } else {
        b++;
      }
    }

    /** For debug */
    // if
    // (func->name().compare(std::string("__buiol_MOD_buiol_close_unit.cold"))
    // == 0) {
    //   dbg(func->name());
    //   for (auto & b: bvec ){
    //     dbg(b->start(), b->last());
    //   }
    // }
    // if (func->name().compare(std::string("main")) == 0) {
    //   dbg(func->name());
    //   for (auto & b: bvec ){
    //     dbg(b->start(), b->last());
    //     std::vector<Function *> func_vec;
    //     b->getFuncs(func_vec);
    //     for (auto func_p: func_vec) {
    //       dbg(func_p->name());
    //     }
    //   }
    // }

    // Create root vertex in the graph
    int func_vertex_id = func_cfg->AddVertex();
    int status = 0;
    char *cpp_name = abi::__cxa_demangle(func_name.c_str(), 0, 0, &status);
    if (status >= 0) {
      func_cfg->SetVertexBasicInfo(func_vertex_id, type::FUNC_NODE, cpp_name);
    } else {
      func_cfg->SetVertexBasicInfo(func_vertex_id, type::FUNC_NODE,
                                   func_name.c_str());
    }
    // Add DebugInfo attributes
    func_cfg->SetVertexDebugInfo(func_vertex_id, entry_addr, exit_addr);

#ifdef DEBUG_COUT
    std::cout << "Function : " << func_name << " addr : " << hex << entry_addr
              << "/" << entry_addr << " - " << exit_addr << dec << std::endl;
#endif

    // Capture loop structure in this function
    // Traverse through the loop (Tarjan) tree
    LoopTreeNode *loop_tree = func->getLoopTree();
    this->ExtractLoopStructure(func_cfg, loop_tree, 1, func_vertex_id);

    // Capture function call structure in this function but not in the loop
    this->ExtractCallStructure(func_cfg, bvec, func_vertex_id);

    func_cfg->UpdateEdges();
  }
}

void StaticAnalysisImpl::DumpFunctionGraph(core::ControlFlowGraph *func_cfg,
                                           const char *file_name) {
  if (func_cfg == nullptr) {
    return;
  }
  func_cfg->DeleteExtraTailVertices();
  func_cfg->SortByAddr(0);
  func_cfg->DumpGraphGML(file_name);
}

void StaticAnalysisImpl::DumpAllFunctionGraph(const char *dir) {
#ifdef LOOP_GRANULARITY
  // std::string dir_name = std::string(getcwd(NULL, 0)) + std::string("/") +
  //                        std::string(this->binary_name) +
  //                        std::string(".pag");
  std::string dir_name = std::string(getcwd(NULL, 0)) + std::string("/") +
                         std::string(dir) + std::string("/") +
                         std::string(this->binary_name) + std::string(".pag");
#else
  // std::string dir_name = std::string(getcwd(NULL, 0)) + std::string("/") +
  //                        std::string(this->binary_name) +
  //                        std::string(".cfg");
  std::string dir_name = std::string(getcwd(NULL, 0)) + std::string("/") +
                         std::string(dir) + std::string("/") +
                         std::string(this->binary_name) + std::string(".cfg");
#endif
  printf("%s\n", dir_name.c_str());
  // TODO: this syscall needs to be wrapped
  if (access(dir_name.c_str(), F_OK)) {
    if (mkdir(dir_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
      printf("mkdir failed\n");
      return;
    }
  }

  // std::map<int, std::string> hash_2_func;
  std::map<int, Address> hash_2_func_entry_addr;

  int i = 0;
  // Traverse through all functions
  for (auto &entry_addr_graph_pair : this->entry_addr_to_graph) {
    Address entry_addr = entry_addr_graph_pair.first;
    hash_2_func_entry_addr[i] = entry_addr;

    core::ControlFlowGraph *func_cfg = entry_addr_graph_pair.second;
    std::stringstream ss;
#ifdef LOOP_GRANULARITY
    ss << std::string(dir) << "/" << this->binary_name << ".pag/" << i
       << ".gml";
#else
    ss << std::string(dir) << "/" << this->binary_name << ".cfg/" << i
       << ".gml";
#endif
    auto file_name = ss.str();
    this->DumpFunctionGraph(func_cfg, file_name.c_str());

    i++;
  }

  std::stringstream ss;
#ifdef LOOP_GRANULARITY
  ss << std::string(dir) << "/" << this->binary_name << ".pag.map";
#else
  ss << std::string(dir) << "/" << this->binary_name << ".cfg.map";
#endif
  auto file_name = ss.str();
  DumpMap<int, Address>(hash_2_func_entry_addr, file_name);
}

void StaticAnalysisImpl::DumpProgramCallGraph(const char *dir) {
  std::stringstream ss;
  ss << std::string(dir) << "/" << this->binary_name << ".pcg";
  auto file_name = ss.str();
  this->pcg->DumpGraphGML(file_name.c_str());
}

void StaticAnalysisImpl::DumpProgramCallGraphMap(const char *dir) {
  std::stringstream ss;
  ss << std::string(dir) << "/" << this->binary_name << ".pcg";
  auto file_name = ss.str();
}

void StaticAnalysisImpl::GetBinaryName() {
  UNIMPLEMENTED();
  //   return this->binary_name;
}

void StaticAnalysisImpl::InterProceduralAnalysis() { UNIMPLEMENTED(); }

} // namespace baguatool::collector
