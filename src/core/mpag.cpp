#include "core/mpag.h"

#include <cstring>
#include <map>
#include <string>

#include <vector>
#include "baguatool.h"
#include "igraph.h"

namespace baguatool::core {

MultiProgramAbstractionGraph::MultiProgramAbstractionGraph() {
    this->j_pag_to_mpag_map.clear();
}

MultiProgramAbstractionGraph::~MultiProgramAbstractionGraph() {
    this->j_pag_to_mpag_map.clear();
}

void MultiProgramAbstractionGraph::VertexTraversal(void (*CALL_BACK_FUNC)(MultiProgramAbstractionGraph *, int, void *),
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

struct mpag_dfs_call_back_t {
  void (*IN_CALL_BACK_FUNC)(MultiProgramAbstractionGraph *, int, void *);
  void (*OUT_CALL_BACK_FUNC)(MultiProgramAbstractionGraph *, int, void *);
  MultiProgramAbstractionGraph *g;
  void *extra;
};

igraph_bool_t mpag_in_callback(const igraph_t *graph, igraph_integer_t vid, igraph_integer_t dist, void *extra) {
  struct mpag_dfs_call_back_t *extra_wrapper = (struct mpag_dfs_call_back_t *)extra;
  if (extra_wrapper->IN_CALL_BACK_FUNC != nullptr) {
    (*(extra_wrapper->IN_CALL_BACK_FUNC))(extra_wrapper->g, vid, extra_wrapper->extra);
  }
  return 0;
}

igraph_bool_t mpag_out_callback(const igraph_t *graph, igraph_integer_t vid, igraph_integer_t dist, void *extra) {
  struct mpag_dfs_call_back_t *extra_wrapper = (struct mpag_dfs_call_back_t *)extra;
  if (extra_wrapper->OUT_CALL_BACK_FUNC != nullptr) {
    (*(extra_wrapper->OUT_CALL_BACK_FUNC))(extra_wrapper->g, vid, extra_wrapper->extra);
  }
  return 0;
}

void MultiProgramAbstractionGraph::DFS(type::vertex_t root,
                                  void (*IN_CALL_BACK_FUNC)(MultiProgramAbstractionGraph *, int, void *),
                                  void (*OUT_CALL_BACK_FUNC)(MultiProgramAbstractionGraph *, int, void *), void *extra) {
  // MultiProgramAbstractionGraph* new_graph = new MultiProgramAbstractionGraph();
  // new_graph->GraphInit();
  struct mpag_dfs_call_back_t *extra_wrapper = new (struct mpag_dfs_call_back_t)();
  extra_wrapper->IN_CALL_BACK_FUNC = IN_CALL_BACK_FUNC;
  extra_wrapper->OUT_CALL_BACK_FUNC = OUT_CALL_BACK_FUNC;
  extra_wrapper->g = this;
  extra_wrapper->extra = extra;

  igraph_dfs(&(this->ipag_->graph), /*root=*/root, /*neimode=*/IGRAPH_OUT,
             /*unreachable=*/1, /*order=*/0, /*order_out=*/0,
             /*father=*/0, /*dist=*/0,
             /*in_callback=*/mpag_in_callback, /*out_callback=*/mpag_out_callback, /*extra=*/extra_wrapper);
  ///*in_callback=*/0, /*out_callback=*/0, /*extra=*/extra_wrapper);
}

void MultiProgramAbstractionGraph::SetPagToMpagMap(type::vertex_t pag_vertex_id, type::procs_t procs_id, type::thread_t thread_id, type::vertex_t mpag_vertex_id){
    std::string pag_vertex_id_str = std::to_string(pag_vertex_id);
      std::string process_id_str = std::to_string(procs_id);
      std::string thread_id_str = std::to_string(thread_id);
      // std::string new_vertex_id_str = std::to_string(new_vertex_id);

      this->j_pag_to_mpag_map[pag_vertex_id_str][process_id_str][thread_id_str] = mpag_vertex_id;

}

void MultiProgramAbstractionGraph::DumpPagToMpagMap(std::string& output_file) {
  std::ofstream output(output_file);
  output << this->j_pag_to_mpag_map << std::endl;
  output.close();
}

}