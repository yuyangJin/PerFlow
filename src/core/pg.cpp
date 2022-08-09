#include "core/pg.h"
#include "baguatool.h"
#include "common/common.h"
#include "vertex_type.h"
#include <stack>

namespace baguatool::core {

ProgramGraph::ProgramGraph() {}
ProgramGraph::~ProgramGraph() {}

void ProgramGraph::VertexTraversal(void (*CALL_BACK_FUNC)(ProgramGraph *, int,
                                                          void *),
                                   void *extra) {
  // this->DeleteExtraTailVertices();
  igraph_vs_t vs;
  igraph_vit_t vit;
  // printf("Function %s Start:\n", this->GetGraphAttributeString("name"));
  // dbg(this->GetGraphAttributeString("name"));

  igraph_vs_seq(&vs, 0, this->cur_vertex_num - 1);
  igraph_vit_create(&ipag_->graph, vs, &vit);
  while (!IGRAPH_VIT_END(vit)) {
    // Get vector id
    type::vertex_t vertex_id = (type::vertex_t)IGRAPH_VIT_GET(vit);
    // printf("Traverse %d\n", vertex_id);
    // dbg(vertex_id);

    // Call user-defined function
    (*CALL_BACK_FUNC)(this, vertex_id, extra);

    IGRAPH_VIT_NEXT(vit);
  }
  // printf("\n");

  igraph_vit_destroy(&vit);
  igraph_vs_destroy(&vs);
  // printf("Function %s End\n", this->GetGraphAttributeString("name"));
}

void ProgramGraph::EdgeTraversal(void (*CALL_BACK_FUNC)(ProgramGraph *, int,
                                                        void *),
                                 void *extra) {
  // this->DeleteExtraTailVertices();
  igraph_es_t es;
  igraph_eit_t eit;
  // printf("Function %s Start:\n", this->GetGraphAttributeString("name"));
  // dbg(this->GetGraphAttributeString("name"));

  igraph_es_all(&es, IGRAPH_EDGEORDER_ID);
  igraph_eit_create(&(this->ipag_->graph), es, &eit);
  while (!IGRAPH_EIT_END(eit)) {
    // Get vector id
    type::edge_t edge_id = (type::edge_t)IGRAPH_EIT_GET(eit);
    // printf("Traverse %d\n", vertex_id);
    // dbg(vertex_id);

    // Call user-defined function
    (*CALL_BACK_FUNC)(this, edge_id, extra);

    IGRAPH_EIT_NEXT(eit);
  }
  // printf("\n");

  igraph_eit_destroy(&eit);
  igraph_es_destroy(&es);
  // printf("Function %s End\n", this->GetGraphAttributeString("name"));
}

int ProgramGraph::SetVertexBasicInfo(const type::vertex_t vertex_id,
                                     const int vertex_type,
                                     const char *vertex_name) {
  SETVAN(&ipag_->graph, "type", vertex_id, (type::num_t)vertex_type);
  SETVAS(&ipag_->graph, "name", vertex_id, vertex_name);
  return 0;
}

int ProgramGraph::SetVertexDebugInfo(const type::vertex_t vertex_id,
                                     const type::addr_t entry_addr,
                                     const type::addr_t exit_addr) {
  SETVAN(&ipag_->graph, "saddr", vertex_id, (type::num_t)entry_addr);
  SETVAN(&ipag_->graph, "eaddr", vertex_id, (type::num_t)exit_addr);
  return 0;
}

int ProgramGraph::GetVertexType(type::vertex_t vertex_id) {
  return this->GetVertexAttributeNum("type", vertex_id);
} // function GetVertexType

int ProgramGraph::SetEdgeType(const type::edge_t edge_id, const int edge_type) {
  this->SetEdgeAttributeNum("type", edge_id, edge_type);
  return 0;
}

int ProgramGraph::GetEdgeType(type::edge_t edge_id) {
  return this->GetEdgeAttributeNum("type", edge_id);
} // function GetEdgeType

int ProgramGraph::GetEdgeType(type::vertex_t src_vertex_id,
                              type::vertex_t dest_vertex_id) {
  type::edge_t edge_id = QueryEdge(src_vertex_id, dest_vertex_id);
  if (edge_id != -1) {
    return this->GetEdgeAttributeNum("type", edge_id);
  }
  return type::NONE_EDGE_TYPE;
} // function GetEdgeType

type::addr_t ProgramGraph::GetVertexEntryAddr(type::vertex_t vertex_id) {
  return this->GetVertexAttributeNum("saddr", vertex_id);
}

type::addr_t ProgramGraph::GetVertexExitAddr(type::vertex_t vertex_id) {
  return this->GetVertexAttributeNum("eaddr", vertex_id);
}

type::vertex_t ProgramGraph::GetChildVertexWithAddr(type::vertex_t root_vertex,
                                                    type::addr_t addr) {
  // std::vector<type::vertex_t> children = GetChildVertexSet(root_vertex);
  std::vector<type::vertex_t> children;
  GetChildVertexSet(root_vertex, children);
  if (0 == children.size()) {
    return -1;
  }

  for (auto &child : children) {
    // dbg(child);
    unsigned long long int s_addr = GetVertexAttributeNum("saddr", child);
    unsigned long long int e_addr = GetVertexAttributeNum("eaddr", child);
    // dbg(addr, s_addr, e_addr);
    int type = GetVertexType(child);
    if (type == type::CALL_NODE || type == type::CALL_REC_NODE ||
        type == type::CALL_IND_NODE) {
      if (addr >= s_addr - 4 && addr <= e_addr + 4) {
        return child;
      }
    } else {
      if (addr >= s_addr - 4 && addr <= e_addr + 4) {
        return child;
      }
    }
  }

  FREE_CONTAINER(children);
  // std::vector<type::vertex_t>().swap(children);

  // Not found
  return -1;
} // function GetChildVertexWithAddr

type::vertex_t ProgramGraph::GetVertexWithCallPath(
    type::vertex_t root_vertex,
    std::stack<unsigned long long> &call_path_stack) {
  // if call path stack is empty, it means the call path points to current
  // vertex, so return it.
  if (call_path_stack.empty()) {
    return root_vertex;
  }

  // Get the top addr of the stack
  type::addr_t addr = call_path_stack.top();

  // dbg(addr);

#ifdef IGNORE_SHARED_OBJ
  /** Step over .dynamic address */
  if (type::IsDynAddr(addr)) {
    call_path_stack.pop();
    return GetVertexWithCallPath(root_vertex, call_path_stack);
  }
#endif

  // Find the CALL vertex of current addr, addr is from calling context
  type::vertex_t found_vertex = root_vertex;
  type::vertex_t child_vertex = -1;
  while (1) {
    child_vertex = GetChildVertexWithAddr(found_vertex, addr);
    // dbg(child_vertex);

    // if child_vertex is not found
    if (-1 == child_vertex) {
      // type::vertex_t new_vertex = this->AddVertex();
      // this->AddEdge(root_vertex, new_vertex);
      // this->SetVertexBasicInfo();
      // found_vertex = new_vertex;

      break;
    }
    found_vertex = child_vertex;

    // If found_vertex is type::FUNC_NODE or type::LOOP_NODE, then continue
    // searching child_vertex
    auto found_vertex_type = GetVertexType(found_vertex);
    if (type::FUNC_NODE != found_vertex_type &&
        type::LOOP_NODE != found_vertex_type &&
        type::BB_NODE != found_vertex_type) {
      break;
    }
  }

  if (-1 == child_vertex) {
    return found_vertex;
  }

  // if find the corresponding child vertex, then pop a addr from the stack.
  call_path_stack.pop();

  // From the found_vertex, recursively search vertex with current call path
  return GetVertexWithCallPath(found_vertex, call_path_stack);

} // function GetVertexWithCallPath

// void
typedef struct CallVertexWithAddrArg {
  type::addr_t addr;             // input
  type::vertex_t vertex_id = -1; // output
  bool find_flag = false;        // find flag
} CVWAArg;

void CallVertexWithAddr(ProgramGraph *pg, int vertex_id, void *extra) {
  CVWAArg *arg = (CVWAArg *)extra;
  if (arg->find_flag) {
    return;
  }
  type::addr_t addr = arg->addr;
  if (pg->GetVertexAttributeNum("type", vertex_id) == type::CALL_NODE ||
      pg->GetVertexAttributeNum("type", vertex_id) == type::CALL_IND_NODE ||
      pg->GetVertexAttributeNum("type", vertex_id) == type::CALL_REC_NODE) {
    type::addr_t s_addr =
        (type::addr_t)pg->GetVertexAttributeNum("saddr", vertex_id);
    type::addr_t e_addr =
        (type::addr_t)pg->GetVertexAttributeNum("eaddr", vertex_id);
    if (addr >= s_addr - 4 && addr <= e_addr + 4) {
      arg->vertex_id = vertex_id;
      arg->find_flag = true;
      return;
    }
  }
  return;
}

type::vertex_t ProgramGraph::GetCallVertexWithAddr(type::addr_t addr) {
  CVWAArg *arg = new CVWAArg();
  arg->addr = addr;
  this->VertexTraversal(&CallVertexWithAddr, arg);
  type::vertex_t vertex_id = arg->vertex_id;
  delete arg;
  return vertex_id;
}

void FuncVertexWithAddr(ProgramGraph *pg, int vertex_id, void *extra) {
  CVWAArg *arg = (CVWAArg *)extra;
  if (arg->find_flag) {
    return;
  }
  type::addr_t addr = arg->addr;
  if (pg->GetVertexAttributeNum("type", vertex_id) == type::FUNC_NODE) {
    type::addr_t s_addr =
        (type::addr_t)pg->GetVertexAttributeNum("saddr", vertex_id);
    type::addr_t e_addr =
        (type::addr_t)pg->GetVertexAttributeNum("eaddr", vertex_id);

    if (addr >= s_addr - 4 && addr <= e_addr + 4) {
      arg->vertex_id = vertex_id;
      arg->find_flag = true;
      return;
    }
  }
  return;
}

type::vertex_t ProgramGraph::GetFuncVertexWithAddr(type::addr_t addr) {
  CVWAArg *arg = new CVWAArg();
  arg->addr = addr;
  this->VertexTraversal(&FuncVertexWithAddr, arg);
  type::vertex_t vertex_id = arg->vertex_id;
  delete arg;
  return vertex_id;
}

int ProgramGraph::AddEdgeWithAddr(type::addr_t call_addr,
                                  type::addr_t callee_addr) {
  type::vertex_t call_vertex = GetCallVertexWithAddr(call_addr);
  type::vertex_t callee_vertex = GetFuncVertexWithAddr(callee_addr);
  if (call_vertex == -1 || callee_vertex == -1) {
    return -1;
  }
  type::edge_t edge_id = this->QueryEdge(call_vertex, callee_vertex);
  if (-1 == edge_id) {
    edge_id = this->AddEdge(call_vertex, callee_vertex);
    return edge_id;
  }
  return edge_id;
}

const char *ProgramGraph::GetCallee(type::vertex_t vertex_id) {
  // dbg(GetVertexAttributeString("name", vertex_id));
  std::vector<type::vertex_t> children;
  GetChildVertexSet(vertex_id, children);
  if (0 == children.size()) {
    return nullptr;
  }

  for (auto &child : children) {
    if (GetVertexType(child) == type::FUNC_NODE) {
      // dbg(GetVertexAttributeString("name", child));
      return GetVertexAttributeString("name", child);
    }
  }

  FREE_CONTAINER(children);
  // std::vector<type::vertex_t>().swap(children);

  // Not found
  return nullptr;
}

const char *ProgramGraph::GetCallee(type::vertex_t vertex_id, int input_type) {
  // dbg(GetVertexAttributeString("name", vertex_id));
  std::vector<type::vertex_t> children;
  GetChildVertexSet(vertex_id, children);
  if (0 == children.size()) {
    return nullptr;
  }

  for (auto &child : children) {
    if (GetVertexType(child) == type::FUNC_NODE) {
      if (GetEdgeType(vertex_id, child) != input_type) {
        continue;
      }
      // dbg(GetVertexAttributeString("name", child));
      return GetVertexAttributeString("name", child);
    }
  }

  FREE_CONTAINER(children);
  // std::vector<type::vertex_t>().swap(children);

  // Not found
  return nullptr;
}

type::addr_t ProgramGraph::GetCalleeEntryAddr(type::vertex_t vertex_id) {
  // dbg(GetVertexAttributeString("name", vertex_id));
  std::vector<type::vertex_t> children;
  GetChildVertexSet(vertex_id, children);
  if (0 == children.size()) {
    return 0;
  }

  for (auto &child : children) {
    if (GetVertexType(child) == type::FUNC_NODE) {
      // dbg(GetVertexAttributeString("name", child));
      return (type::addr_t)GetVertexAttributeNum("saddr", child);
    }
  }

  FREE_CONTAINER(children);
  // std::vector<type::vertex_t>().swap(children);

  // Not found
  return 0;
}

type::addr_t ProgramGraph::GetCalleeEntryAddr(type::vertex_t vertex_id,
                                              int input_type) {
  // dbg(GetVertexAttributeString("name", vertex_id));
  std::vector<type::vertex_t> children;
  GetChildVertexSet(vertex_id, children);
  if (0 == children.size()) {
    return 0;
  }

  for (auto &child : children) {
    if (GetVertexType(child) == type::FUNC_NODE) {
      if (GetEdgeType(vertex_id, child) != input_type) {
        continue;
      }
      // dbg(GetVertexAttributeString("name", child));
      return (type::addr_t)GetVertexAttributeNum("saddr", child);
    }
  }

  FREE_CONTAINER(children);
  // std::vector<type::vertex_t>().swap(children);

  // Not found
  return 0;
}

void ProgramGraph::GetCalleeEntryAddrs(type::vertex_t vertex_id,
                                       std::vector<type::addr_t> &entry_addrs) {
  // dbg(GetVertexAttributeString("name", vertex_id));
  std::vector<type::vertex_t> children;
  GetChildVertexSet(vertex_id, children);
  if (0 == children.size()) {
    return;
  }

  for (auto &child : children) {
    if (GetVertexType(child) == type::FUNC_NODE) {
      // dbg(GetVertexAttributeString("name", child));
      type::addr_t entry_addr =
          (type::addr_t)GetVertexAttributeNum("saddr", child);
      entry_addrs.push_back(entry_addr);
    }
  }

  FREE_CONTAINER(children);

  return;
}

void ProgramGraph::GetCalleeEntryAddrs(type::vertex_t vertex_id,
                                       std::vector<type::addr_t> &entry_addrs,
                                       int input_type) {
  // dbg(GetVertexAttributeString("name", vertex_id));
  std::vector<type::vertex_t> children;
  GetChildVertexSet(vertex_id, children);
  if (0 == children.size()) {
    return;
  }

  for (auto &child : children) {
    if (GetVertexType(child) == type::FUNC_NODE) {
      if (GetEdgeType(vertex_id, child) != input_type) {
        continue;
      }
      // dbg(GetVertexAttributeString("name", child));
      type::addr_t entry_addr =
          (type::addr_t)GetVertexAttributeNum("saddr", child);
      entry_addrs.push_back(entry_addr);
    }
  }

  FREE_CONTAINER(children);

  return;
}

struct compare_addr {
  const std::vector<unsigned long long> &_v;
  compare_addr(const std::vector<unsigned long long> &v) : _v(v) {}
  inline bool operator()(std::size_t i, std::size_t j) const {
    return _v[i] > _v[j];
  }
};

struct IdAndAddr {
  type::vertex_t vertex_id;
  type::addr_t addr;
  IdAndAddr(type::vertex_t vid, type::addr_t a) : vertex_id(vid), addr(a) {}
  bool operator<(const IdAndAddr &iaa) const { return (addr < iaa.addr); }
};

void SortChild(ProgramGraph *pg, int vertex_id, void *extra) {
  std::vector<type::vertex_t> children;
  pg->GetChildVertexSet(vertex_id, children);

  // If this vertex has no child, return now
  if (0 == children.size()) {
    return;
  }

  /* Sort pairs <id, s_addr> with s_addr as key */
  std::vector<IdAndAddr> children_id_addr;
  // make pair
  for (auto &child : children) {
    type::addr_t s_addr = pg->GetVertexAttributeNum("saddr", child);
    children_id_addr.push_back(IdAndAddr(child, s_addr));
  }
  // Sort by s_addr
  std::sort(children_id_addr.begin(), children_id_addr.end());
  // Convert pair <id, s_addr> to two vector
  std::vector<type::vertex_t> children_id;
  std::vector<unsigned long long> children_s_addr;

  for (auto &iaa : children_id_addr) {
    children_id.push_back(iaa.vertex_id);
    children_s_addr.push_back(iaa.addr);
  }
  // dbg(children, children_id);
  /* Sorting complete */

  /* Swap vertices, children is original sequence, children_id is sorted
   * sequence */
  int num_children = children.size();
  std::map<type::vertex_t, type::vertex_t> vertex_id_to_tmp_vertex_id;
  std::map<type::vertex_t, std::vector<type::edge_t>>
      vertex_id_to_tmp_edge_id_vec;
  for (int i = 0; i < num_children; i++) {
    if (children[i] != children_id[i]) {
      // Get and record children of children[i]
      std::vector<type::vertex_t> children_children;
      pg->GetChildVertexSet(children[i], children_children);
      vertex_id_to_tmp_edge_id_vec[children[i]] = children_children;
    }
  }

  for (int i = 0; i < num_children; i++) {
    if (children[i] != children_id[i]) {
      // Copy attributes except "id"
      type::vertex_t tmp_vertex_id = pg->AddVertex();
      pg->CopyVertex(tmp_vertex_id, pg, children[i]);
      // If children_id[i] is covered, use tmp_vertex in
      // vertex_id_to_tmp_vertex_id
      if (vertex_id_to_tmp_vertex_id.count(children_id[i]) > 0) {
        pg->CopyVertex(children[i], pg,
                       vertex_id_to_tmp_vertex_id[children_id[i]]);
        // pg->SetVertexAttributeNum("id", children[i], children_id[i]);
      } else {
        pg->CopyVertex(children[i], pg, children_id[i]);
      }
      vertex_id_to_tmp_vertex_id[children[i]] = tmp_vertex_id;

      // TODO : can not understand now
      // Delete all edges of children[i]
      std::vector<type::vertex_t> &children_children =
          vertex_id_to_tmp_edge_id_vec[children[i]];
      for (auto &child_child : children_children) {
        // dbg(children[i], child_child);
        pg->DeleteEdge(children[i], child_child);
      }
      if (vertex_id_to_tmp_edge_id_vec.count(children_id[i]) > 0) {
        std::vector<type::vertex_t> &tmp_children_children =
            vertex_id_to_tmp_edge_id_vec[children_id[i]];
        for (auto &child_child : tmp_children_children) {
          // dbg(children[i], child_child);
          pg->AddEdge(children[i], child_child);
        }
      } else {
        // Get children of children_id[i]
        std::vector<type::vertex_t> children_id_children;
        pg->GetChildVertexSet(children_id[i], children_id_children);
        // dbg(children_id[i], children_id_children);

        for (auto &child_child : children_id_children) {
          // dbg(children[i], child_child);
          pg->AddEdge(children[i], child_child);
        }
        FREE_CONTAINER(children_id_children);
      }
    }
  }

  // for (auto& vertex: vertex_id_to_tmp_vertex_id) {
  //   dbg(pg->GetCurVertexNum() - 1);
  //   pg->DeleteVertex(pg->GetCurVertexNum() - 1);
  // }

  for (auto &vertex : vertex_id_to_tmp_vertex_id) {
    // dbg(vertex.second);
    pg->DeleteVertex(vertex.second);
  }
  /* Swapping complete */

  FREE_CONTAINER(children);
  FREE_CONTAINER(children_id_addr);
  FREE_CONTAINER(children_id);
  FREE_CONTAINER(children_s_addr);
  FREE_CONTAINER(vertex_id_to_tmp_vertex_id);
  for (auto &item : vertex_id_to_tmp_edge_id_vec) {
    FREE_CONTAINER(item.second);
  }
  FREE_CONTAINER(vertex_id_to_tmp_edge_id_vec);

  return;
}

void ProgramGraph::VertexSortChild() {
  this->VertexTraversal(&SortChild, nullptr);
  return;
}

void ProgramGraph::SortByAddr(type::vertex_t starting_vertex) {
  this->SortBy(starting_vertex, "eaddr");
}
} // namespace baguatool::core
