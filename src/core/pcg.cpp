#include "core/pcg.h"
#include "baguatool.h"
#include "vertex_type.h"

namespace baguatool::core {

ProgramCallGraph::ProgramCallGraph() {}
ProgramCallGraph::~ProgramCallGraph() {}

void ProgramCallGraph::EdgeTraversal(void (*CALL_BACK_FUNC)(ProgramCallGraph *,
                                                            int, void *),
                                     void *extra) {
  igraph_es_t es;
  igraph_eit_t eit;
  // printf("Function %s Start:\n", this->GetGraphAttributeString("name"));
  // dbg(this->GetGraphAttributeString("name"));

  igraph_es_all(&es, IGRAPH_EDGEORDER_ID);
  igraph_eit_create(&(this->ipag_->graph), es, &eit);
  while (!IGRAPH_EIT_END(eit)) {
    // Get edge id
    type::edge_t edge_id = (type::edge_t)IGRAPH_EIT_GET(eit);
    // printf("Traverse %d\n", edge_id);
    // dbg(edge_id);

    // Call user-defined function
    (*CALL_BACK_FUNC)(this, edge_id, extra);

    IGRAPH_EIT_NEXT(eit);
  }
  // printf("\n");

  igraph_eit_destroy(&eit);
  igraph_es_destroy(&es);
  // printf("Function %s End\n", this->GetGraphAttributeString("name"));
}

// int ProgramCallGraph::SetVertexBasicInfo(const type::vertex_t vertex_id,
// const int vertex_type, const char *vertex_name) {
//   //
//   // SetVertexAttributeNum("type", vertex_id, (igraph_real_t)vertex_type);
//   SETVAN(&ipag_->graph, "type", vertex_id, (igraph_real_t)vertex_type);
//   SETVAS(&ipag_->graph, "name", vertex_id, vertex_name);
//   return 0;
// }

// int ProgramCallGraph::SetVertexDebugInfo(const type::vertex_t vertex_id,
// const int addr) {
//   //
//   SETVAN(&ipag_->graph, "addr", vertex_id, (igraph_real_t)addr);
//   return 0;
// }

// int ProgramCallGraph::GetVertexType(type::vertex_t vertex) {
//   return this->GetVertexAttributeNum("type", vertex);
// } // function GetVertexType

// type::vertex_t ProgramCallGraph::GetChildVertexWithAddress(type::vertex_t
// root_vertex, unsigned long long addr) {
//   std::vector<type::vertex_t> children = GetChildVertexSet(root_vertex);
//   if (0 == children.size()) {
//     return -1;
//   }

//   for (auto child : children) {
//     unsigned long long int s_addr = GetVertexAttributeNum("s_addr", child);
//     unsigned long long int e_addr = GetVertexAttributeNum("e_addr", child);
//     if (addr >= s_addr && addr <= e_addr) {
//       return child;
//     }
//   }

//   std::vector<type::vertex_t>().swap(children);

//   // Not found
//   return -1;
// }  // function GetChildVertexWithAddress

// type::vertex_t ProgramCallGraph::GetVertexWithCallPath(type::vertex_t
// root_vertex, std::stack<unsigned long long>& call_path_stack ) {
//   // if call path stack is empty, it means the call path points to current
//   vertex, so return it. if (call_path_stack.empty()) {
//     return root_vertex;
//   }

//   // Get the top addr of the stack
//   unsigned long long addr = call_path_stack.top();

//   // while (addr > 0x40000000) {
//   //   call_path_stack.pop();
//   //   addr = call_path_stack.top();
//   // }

//   // Find the CALL vertex of current addr, addr is from calling context
//   type::vertex_t found_vertex = root_vertex;
//   while (1) {
//     type::vertex_t child_vertex = GetChildVertexWithAddress(found_vertex,
//     addr);

//     found_vertex = child_vertex;
//     // if child_vertex is not found
//     if (-1 == child_vertex) {
//       //type::vertex_t new_vertex = this->AddVertex();
//       //this->AddEdge(root_vertex, new_vertex);
//       //this->SetVertexBasicInfo();
//       //found_vertex = new_vertex;
//       break;
//     }

//     // If found_vertex is FUNC_NODE or LOOP_NODE, then continue searching
//     child_vertex auto found_vertex_type = GetVertexType(found_vertex); if
//     (FUNC_NODE != found_vertex_type && LOOP_NODE != found_vertex_type &&
//     BB_NODE != found_vertex_type) {
//       break;
//     }
//   }

//   if (-1 == found_vertex) {
//     return root_vertex;
//   }

//   // if find the corresponding child vertex, then pop a addr from the stack.
//   call_path_stack.pop();

//   // From the found_vertex, recursively search vertex with current call path
//   return GetVertexWithCallPath(found_vertex, call_path_stack);

// }  // function GetVertexWithCallPath

// // void

// type::vertex_t ProgramCallGraph::GetVertexWithAddr(unsigned long long addr) {
//   VertexTraversal();
// }

// int ProgramCallGraph::AddEdgeWithAddr(unsigned long long call_addr, unsigned
// long long callee_addr) {
//   type::vertex_t call_vertex = GetVertexWithAddress(call_addr);
//   type::vertex_t callee_vertex = GetVertexWithAddress(callee_addr);
//   if (!this->QueryEdgeWithSrcDest(call_vertex, callee_vertex)) {
//     this->AddEdge(call_vertex, callee_vertex);
//   }
// }
void ProgramCallGraph::VertexTraversal(
  void (*CALL_BACK_FUNC)(ProgramCallGraph *, int, void *),
    void *extra) {
  igraph_vs_t vs;
  igraph_vit_t vit;
  // printf("Function %s Start:\n", this->GetGraphAttributeString("name"));
  // dbg(this->cur_vertex_num - 1);
  igraph_vs_seq(&vs, 0, this->cur_vertex_num - 1);
  igraph_vit_create(&ipag_->graph, vs, &vit);
  while (!IGRAPH_VIT_END(vit)) {
    // Get vector id
    type::vertex_t vertex_id = (type::vertex_t)IGRAPH_VIT_GET(vit);
    // printf("Traverse %d\n", vertex_id);

    // Call user-defined function
    (*CALL_BACK_FUNC)(this, vertex_id, extra);

    IGRAPH_VIT_NEXT(vit);
  }
  // printf("\n");

  igraph_vit_destroy(&vit);
  igraph_vs_destroy(&vs);
  // printf("Function %s End\n", this->GetGraphAttributeString("name"));
}


} // namespace baguatool::core