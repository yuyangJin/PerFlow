#include "core/cfg.h"
#include "baguatool.h"
#include "vertex_type.h"

namespace baguatool::core {

ControlFlowGraph::ControlFlowGraph() {}
ControlFlowGraph::~ControlFlowGraph() {}

// int ControlFlowGraph::SetVertexBasicInfo(const type::vertex_t vertex_id,
// const int vertex_type, const char *vertex_name) {
//   //
//   // SetVertexAttributeNum("type", vertex_id, (igraph_real_t)vertex_type);
//   SETVAN(&ipag_->graph, "type", vertex_id, (igraph_real_t)vertex_type);
//   SETVAS(&ipag_->graph, "name", vertex_id, vertex_name);
//   return 0;
// }

// int ControlFlowGraph::SetVertexDebugInfo(const type::vertex_t vertex_id,
// const int entry_addr, const int exit_addr) {
//   //
//   SETVAN(&ipag_->graph, "s_addr", vertex_id, (igraph_real_t)entry_addr);
//   SETVAN(&ipag_->graph, "e_addr", vertex_id, (igraph_real_t)exit_addr);
//   return 0;
// }
} // namespace baguatool::core