#include <cstring>
#include <map>
#include <stdint.h>
#include <stdlib.h>
#include <string>

#include "core/graph.h"

#include "common/common.h"

#include "baguatool.h"

namespace baguatool::core {

Graph::Graph() {
  // ipag_ = new type::graph_t;
  ipag_ = std::make_unique<type::graph_t>();
  // open attributes
  igraph_set_attribute_table(&igraph_cattribute_table);
  edges_to_be_added = std::make_unique<type::edge_vector_t>();
  this->lazy_edge_trunk_size = TRUNK_SIZE;
  igraph_vector_init(&edges_to_be_added->edges, TRUNK_SIZE * 2);
  // dbg(igraph_vector_size(&edges_to_be_added->edges));
  this->graph_perf_data = new core::GraphPerfData();
  // set vertex number as 0
  cur_vertex_num = 0;
  cur_edge_num = 0;
  num_edges_to_be_added = 0;
}

Graph::~Graph() {
  igraph_destroy(&ipag_->graph);
  igraph_vector_destroy(&edges_to_be_added->edges);
  delete this->graph_perf_data;
  // delete ipag_;
}

void Graph::GraphInit(const char *graph_name) {
  // build an empty graph
  igraph_empty(&ipag_->graph, 0, IGRAPH_DIRECTED);
  // set graph name
  SETGAS(&ipag_->graph, "name", graph_name);
  // set vertex number as 0
  cur_vertex_num = 0;
  cur_edge_num = 0;
  num_edges_to_be_added = 0;
}

type::vertex_t Graph::AddVertex() {
  if (this->cur_vertex_num >= igraph_vcount(&ipag_->graph)) {
    // dbg(this->cur_vertex_num, igraph_vcount(&ipag_->graph));
    igraph_add_vertices(&ipag_->graph, TRUNK_SIZE, 0);
  }
  // Add a new vertex
  // dbg("add vertex", cur_vertex_num);
  igraph_integer_t new_vertex_id = this->cur_vertex_num++;
  this->SetVertexAttributeNum("id", new_vertex_id, new_vertex_id);

  // dbg(new_vertex_id, this->cur_vertex_num);

  // igraph_integer_t new_vertex_id = igraph_vcount(&ipag_.graph);
  // printf("Add a vertex: %d %s\n", new_vertex_id, vertex_name);

  // Set basic attributes

  // Return id of new vertex
  return (type::vertex_t)new_vertex_id;
}

void Graph::SwapVertex(type::vertex_t vertex_id_1, type::vertex_t vertex_id_2) {
  // Swap all attributes, including id
  igraph_vector_t vtypes;
  igraph_strvector_t vnames;
  int i;

  igraph_vector_init(&vtypes, 0);
  igraph_strvector_init(&vnames, 0);

  igraph_cattribute_list(&this->ipag_->graph, nullptr, nullptr, &vnames,
                         &vtypes, nullptr, nullptr);
  /* Graph attributes */
  for (i = 0; i < igraph_strvector_size(&vnames); i++) {
    const char *vname = STR(vnames, i);
    // printf("%s=", vname);
    if (VECTOR(vtypes)[i] == IGRAPH_ATTRIBUTE_NUMERIC) {
      igraph_integer_t swap_num =
          VAN(&this->ipag_->graph, STR(vnames, i), vertex_id_1);
      SETVAN(&this->ipag_->graph, vname, vertex_id_1,
             VAN(&this->ipag_->graph, STR(vnames, i), vertex_id_2));
      SETVAN(&this->ipag_->graph, vname, vertex_id_2, swap_num);
      // igraph_real_printf(VAN(&g->ipag_->graph, STR(vnames, i), vertex_id));
    } else {
      const char *swap_num =
          VAS(&this->ipag_->graph, STR(vnames, i), vertex_id_1);
      SETVAS(&this->ipag_->graph, vname, vertex_id_1,
             VAS(&this->ipag_->graph, STR(vnames, i), vertex_id_2));
      SETVAS(&this->ipag_->graph, vname, vertex_id_2, swap_num);
      // SETVAS(&ipag_->graph, vname, new_vertex_id, VAS(&g->ipag_->graph,
      // STR(vnames, i), vertex_id)); printf("\"%s\" ", VAS(&g->ipag_->graph,
      // STR(vnames, i), vertex_id));
    }
  }
  // printf("\n");
}

void Graph::SetLazyEdgeTrunkSize(int trunk_size){
  this->lazy_edge_trunk_size = trunk_size;
  igraph_vector_resize(&this->edges_to_be_added->edges,
                       this->lazy_edge_trunk_size * 2);
}

void Graph::UpdateEdges() {
  if (this->num_edges_to_be_added == 0) {
    return ;
  }
  if (this->lazy_edge_trunk_size != this->num_edges_to_be_added){
    igraph_vector_resize(&this->edges_to_be_added->edges,
                         this->num_edges_to_be_added * 2);
  }
  igraph_add_edges(&this->ipag_->graph, &this->edges_to_be_added->edges, 0);

  // for (int eid = this->cur_edge_num - this->num_edges_to_be_added; eid <
  // this->cur_edge_num; eid++) {
  //   printf("[G]eid=%d, src=%d, dst=%d\n", eid, this->GetEdgeSrc(eid),
  //   this->GetEdgeDest(eid));
  // }

  if (this->lazy_edge_trunk_size != this->num_edges_to_be_added){
    igraph_vector_resize(&this->edges_to_be_added->edges,
                         this->lazy_edge_trunk_size * 2);
  }
  this->num_edges_to_be_added = 0;
  //igraph_vector_resize(&this->edges_to_be_added->edges, TRUNK_SIZE * 2);

  for (auto &edge_data : this->edges_attr_to_be_added.items()) {
    int edge_id = std::stoi(edge_data.key());
    for (auto &attr_data : edge_data.value().items()) {
      type::num_t value = attr_data.value();
      SETEAN(&ipag_->graph, attr_data.key().c_str(), edge_id, value);
    }
  }
  this->edges_attr_to_be_added.clear();
}

type::edge_t Graph::AddEdgeLazy(const type::vertex_t src_vertex_id,
                                const type::vertex_t dest_vertex_id) {

  if (this->num_edges_to_be_added >= this->lazy_edge_trunk_size) {
    UpdateEdges();
  }
  // dbg(this->num_edges_to_be_added * 2, src_vertex_id,
  // this->num_edges_to_be_added * 2 + 1, dest_vertex_id);
  VECTOR(this->edges_to_be_added->edges)
  [this->num_edges_to_be_added * 2] = src_vertex_id;
  VECTOR(this->edges_to_be_added->edges)
  [this->num_edges_to_be_added * 2 + 1] = dest_vertex_id;
  this->num_edges_to_be_added++;
  igraph_integer_t new_edge_id = this->cur_edge_num++;

  // printf("[V]eid=%d, src=%d, dst=%d\n", new_edge_id, src_vertex_id,
  // dest_vertex_id);

  return new_edge_id;
}

type::edge_t Graph::AddEdge(const type::vertex_t src_vertex_id,
                            const type::vertex_t dest_vertex_id) {
  ///** OLD VERSION for AddEdge
  // Add a new edge
  // printf("Add an edge: %d, %d\n", src_vertex_id, dest_vertex_id);
  igraph_add_edge(&ipag_->graph, (igraph_integer_t)src_vertex_id,
                  (igraph_integer_t)dest_vertex_id);
  igraph_integer_t new_edge_id = igraph_ecount(&ipag_->graph);
  this->cur_edge_num++;

  // Return id of new edge
  return (type::edge_t)(new_edge_id - 1);
}

type::vertex_t Graph::AddGraph(Graph *g) {
  // dbg(g->GetCurVertexNum());
  g->DeleteExtraTailVertices();
  // dbg(g->GetCurVertexNum());

  std::map<type::vertex_t, type::vertex_t> old_vertex_id_2_new_vertex_id;

  // Step over all vertices
  igraph_vs_t vs;
  igraph_vit_t vit;

  igraph_vs_all(&vs);
  igraph_vit_create(&g->ipag_->graph, vs, &vit);
  while (!IGRAPH_VIT_END(vit)) {
    // Get vector id
    type::vertex_t vertex_id = (type::vertex_t)IGRAPH_VIT_GET(vit);
    // printf("vertex %d", vertex_id);

    // Add new vertex (the copy of that in the input g) into this pag
    type::vertex_t new_vertex_id = this->AddVertex();

    old_vertex_id_2_new_vertex_id[vertex_id] = new_vertex_id;

    // copy all attributes of this vertex
    this->CopyVertex(new_vertex_id, g, vertex_id);

    IGRAPH_VIT_NEXT(vit);
  }
  // printf("\n");

  igraph_vit_destroy(&vit);
  igraph_vs_destroy(&vs);

  // Step over all edges
  igraph_es_t es;
  igraph_eit_t eit;

  igraph_es_all(&es, IGRAPH_EDGEORDER_ID);
  igraph_eit_create(&g->ipag_->graph, es, &eit);

  int edge_size = 0;
  igraph_es_size(&g->ipag_->graph, &es, &edge_size);
  this->SetLazyEdgeTrunkSize(edge_size);

  while (!IGRAPH_EIT_END(eit)) {
    // Get edge id
    type::edge_t edge_id = (type::edge_t)IGRAPH_EIT_GET(eit);
    // printf("edge %d", edge_id);

    // Add new edge (the copy of that in the input g) into this pag
    // type::edge_t new_edge_id =
    this->AddEdgeLazy(old_vertex_id_2_new_vertex_id[g->GetEdgeSrc(edge_id)],
                      old_vertex_id_2_new_vertex_id[g->GetEdgeDest(edge_id)]);

    // copy all attributes of this vertex
    // this->CopyVertex(new_vertex_id, g, vertex_id);

    IGRAPH_EIT_NEXT(eit);
  }
  this->UpdateEdges();
  // printf("\n");

  igraph_eit_destroy(&eit);
  igraph_es_destroy(&es);

  type::vertex_t ret_vertex_id = old_vertex_id_2_new_vertex_id[0];
  FREE_CONTAINER(old_vertex_id_2_new_vertex_id);
  return ret_vertex_id;
}

void Graph::DeleteVertex(type::vertex_t vertex_id) {
  igraph_delete_vertices(&this->ipag_->graph, igraph_vss_1(vertex_id));
  this->cur_vertex_num--;
}

void Graph::DeleteEdge(type::vertex_t src_id, type::vertex_t dest_id) {
  type::edge_t edge_id = QueryEdge(src_id, dest_id);
  if (edge_id != -1) {
    igraph_es_t es;
    igraph_es_1(&es, edge_id);
    igraph_delete_edges(&this->ipag_->graph, es);
    igraph_es_destroy(&es);
    this->cur_edge_num--;
  } else {
    // it could be in VECTOR()
    ; // std::cout << "E"<<"do not exs"
  }
}

void Graph::QueryVertex() { UNIMPLEMENTED(); }

type::edge_t Graph::QueryEdge(type::vertex_t src_id, type::vertex_t dest_id) {
  type::edge_t edge_id = -1;
  int ret = igraph_get_eid(&ipag_->graph, &edge_id, src_id, dest_id,
                           IGRAPH_DIRECTED, /**error*/ 0);
  if (ret != IGRAPH_SUCCESS) {
    return -1;
  }
  return edge_id;
}

type::vertex_t Graph::GetEdgeSrc(type::edge_t edge_id) {
  return IGRAPH_FROM(&ipag_->graph, edge_id);
}

type::vertex_t Graph::GetEdgeDest(type::edge_t edge_id) {
  return IGRAPH_TO(&ipag_->graph, edge_id);
}

void Graph::GetEdgeOtherSide() { UNIMPLEMENTED(); }

bool Graph::HasGraphAttribute(const char *attr_name) {
  return igraph_cattribute_has_attr(&ipag_->graph, IGRAPH_ATTRIBUTE_GRAPH,
                                    attr_name);
}

bool Graph::HasVertexAttribute(const char *attr_name) {
  return igraph_cattribute_has_attr(&ipag_->graph, IGRAPH_ATTRIBUTE_VERTEX,
                                    attr_name);
}

bool Graph::HasEdgeAttribute(const char *attr_name) {
  return igraph_cattribute_has_attr(&ipag_->graph, IGRAPH_ATTRIBUTE_EDGE,
                                    attr_name);
}

void Graph::SetGraphAttributeString(const char *attr_name, const char *value) {
  SETGAS(&ipag_->graph, attr_name, value);
}
void Graph::SetGraphAttributeNum(const char *attr_name,
                                 const type::num_t value) {
  SETGAN(&ipag_->graph, attr_name, value);
}
void Graph::SetGraphAttributeFlag(const char *attr_name, const bool value) {
  SETGAB(&ipag_->graph, attr_name, value);
}
void Graph::SetVertexAttributeString(const char *attr_name,
                                     type::vertex_t vertex_id,
                                     const char *value) {
  SETVAS(&ipag_->graph, attr_name, vertex_id, value);
}
void Graph::SetVertexAttributeNum(const char *attr_name,
                                  type::vertex_t vertex_id,
                                  const type::num_t value) {
  SETVAN(&ipag_->graph, attr_name, vertex_id, value);
}
void Graph::SetVertexAttributeFlag(const char *attr_name,
                                   type::vertex_t vertex_id, const bool value) {
  SETVAB(&ipag_->graph, attr_name, vertex_id, value);
}

void Graph::SetEdgeAttributeStringLazy(const char *attr_name,
                                       type::edge_t edge_id,
                                       const char *value) {
  std::string edge_id_str = std::to_string(edge_id);
  std::string attr_name_str = std::string(attr_name);
  std::string value_str = std::string(value);
  edges_attr_to_be_added[edge_id_str][attr_name_str] = value_str;
}
void Graph::SetEdgeAttributeNumLazy(const char *attr_name, type::edge_t edge_id,
                                    const type::num_t value) {
  std::string edge_id_str = std::to_string(edge_id);
  std::string attr_name_str = std::string(attr_name);
  edges_attr_to_be_added[edge_id_str][attr_name_str] = value;
}
void Graph::SetEdgeAttributeFlagLazy(const char *attr_name,
                                     type::edge_t edge_id, const bool value) {
  std::string edge_id_str = std::to_string(edge_id);
  std::string attr_name_str = std::string(attr_name);
  edges_attr_to_be_added[edge_id_str][attr_name_str] = value;
}

void Graph::SetEdgeAttributeString(const char *attr_name, type::edge_t edge_id,
                                   const char *value) {
  SETEAS(&ipag_->graph, attr_name, edge_id, value);
}
void Graph::SetEdgeAttributeNum(const char *attr_name, type::edge_t edge_id,
                                const type::num_t value) {
  SETEAN(&ipag_->graph, attr_name, edge_id, value);
}
void Graph::SetEdgeAttributeFlag(const char *attr_name, type::edge_t edge_id,
                                 const bool value) {
  SETEAB(&ipag_->graph, attr_name, edge_id, value);
}

const char *Graph::GetGraphAttributeString(const char *attr_name) {
  const char *ret_str = GAS(&ipag_->graph, attr_name);
  return ret_str;
}

const type::num_t Graph::GetGraphAttributeNum(const char *attr_name) {
  const type::num_t ret_num = GAN(&ipag_->graph, attr_name);
  return ret_num;
}

const bool Graph::GetGraphAttributeFlag(const char *attr_name) {
  const bool ret_flag = GAB(&ipag_->graph, attr_name);
  return ret_flag;
}

const char *Graph::GetVertexAttributeString(const char *attr_name,
                                            type::vertex_t vertex_id) {
  const char *ret_str = VAS(&ipag_->graph, attr_name, vertex_id);
  return ret_str;
}

const type::num_t Graph::GetVertexAttributeNum(const char *attr_name,
                                               type::vertex_t vertex_id) {
  const type::num_t ret_num = VAN(&ipag_->graph, attr_name, vertex_id);
  return ret_num;
}

const bool Graph::GetVertexAttributeFlag(const char *attr_name,
                                         type::vertex_t vertex_id) {
  const bool ret_flag = VAB(&ipag_->graph, attr_name, vertex_id);
  return ret_flag;
}

const char *Graph::GetEdgeAttributeString(const char *attr_name,
                                          type::edge_t edge_id) {
  const char *ret_str = EAS(&ipag_->graph, attr_name, edge_id);
  return ret_str;
}

const type::num_t Graph::GetEdgeAttributeNum(const char *attr_name,
                                             type::edge_t edge_id) {
  const type::num_t ret_num = EAN(&ipag_->graph, attr_name, edge_id);
  return ret_num;
}

const bool Graph::GetEdgeAttributeFlag(const char *attr_name,
                                       type::edge_t edge_id) {
  const bool ret_flag = EAB(&ipag_->graph, attr_name, edge_id);
  return ret_flag;
}

void Graph::RemoveGraphAttribute(const char *attr_name) {
  DELGA(&ipag_->graph, attr_name);
}

void Graph::RemoveVertexAttribute(const char *attr_name) {
  DELVA(&ipag_->graph, attr_name);
}

void Graph::RemoveEdgeAttribute(const char *attr_name) {
  DELEA(&ipag_->graph, attr_name);
}

void Graph::MergeVertices() { UNIMPLEMENTED(); }

void Graph::SplitVertex() { UNIMPLEMENTED(); }

void Graph::DeepCopyVertex(type::vertex_t new_vertex_id, Graph *g,
                           type::vertex_t vertex_id) {
  // igraph_vector_t gtypes, vtypes, etypes;
  igraph_vector_t vtypes;
  // igraph_strvector_t gnames, vnames, enames;
  igraph_strvector_t vnames;
  int i;
  // igraph_vector_init(&gtypes, 0);
  igraph_vector_init(&vtypes, 0);
  // igraph_vector_init(&etypes, 0);
  // igraph_strvector_init(&gnames, 0);
  igraph_strvector_init(&vnames, 0);
  // igraph_strvector_init(&enames, 0);
  igraph_cattribute_list(&g->ipag_->graph, nullptr, nullptr, &vnames, &vtypes,
                         nullptr, nullptr);
  /* Graph attributes */
  for (i = 0; i < igraph_strvector_size(&vnames); i++) {
    const char *vname = STR(vnames, i);
    // printf("%s=", vname);
    if (VECTOR(vtypes)[i] == IGRAPH_ATTRIBUTE_NUMERIC) {
      // if (strcmp(vname, "id") == 0) {
      //   SETVAN(&ipag_->graph, vname, new_vertex_id, new_vertex_id);
      //   printf("%d", new_vertex_id);
      // } else {
      SETVAN(&ipag_->graph, vname, new_vertex_id,
             VAN(&g->ipag_->graph, STR(vnames, i), vertex_id));
      // igraph_real_printf(VAN(&g->ipag_->graph, STR(vnames, i), vertex_id));
      // }

      // putchar(' ');
    } else {
      SETVAS(&ipag_->graph, vname, new_vertex_id,
             VAS(&g->ipag_->graph, STR(vnames, i), vertex_id));
      // printf("\"%s\" ", VAS(&g->ipag_->graph, STR(vnames, i), vertex_id));
    }
  }
  // printf("\n");
}

void Graph::CopyVertex(type::vertex_t new_vertex_id, Graph *g,
                       type::vertex_t vertex_id) {
  // igraph_vector_t gtypes, vtypes, etypes;
  igraph_vector_t vtypes;
  // igraph_strvector_t gnames, vnames, enames;
  igraph_strvector_t vnames;
  int i;
  // igraph_vector_init(&gtypes, 0);
  igraph_vector_init(&vtypes, 0);
  // igraph_vector_init(&etypes, 0);
  // igraph_strvector_init(&gnames, 0);
  igraph_strvector_init(&vnames, 0);
  // igraph_strvector_init(&enames, 0);
  igraph_cattribute_list(&g->ipag_->graph, nullptr, nullptr, &vnames, &vtypes,
                         nullptr, nullptr);
  /* Graph attributes */
  for (i = 0; i < igraph_strvector_size(&vnames); i++) {
    const char *vname = STR(vnames, i);
    // printf("%s=", vname);
    if (VECTOR(vtypes)[i] == IGRAPH_ATTRIBUTE_NUMERIC) {
      if (strcmp(vname, "id") == 0) {
        SETVAN(&ipag_->graph, vname, new_vertex_id, new_vertex_id);
        // printf("%d", new_vertex_id);
      } else {
        SETVAN(&ipag_->graph, vname, new_vertex_id,
               VAN(&g->ipag_->graph, STR(vnames, i), vertex_id));
        // igraph_real_printf(VAN(&g->ipag_->graph, STR(vnames, i), vertex_id));
      }

      // printf(" ");
    } else {
      SETVAS(&ipag_->graph, vname, new_vertex_id,
             VAS(&g->ipag_->graph, STR(vnames, i), vertex_id));
      // printf("\"%s\" ", VAS(&g->ipag_->graph, STR(vnames, i), vertex_id));
    }
  }
  // printf("\n");
}

void Graph::DeleteVertices(type::vertex_set_t *vs) {
  igraph_delete_vertices(&ipag_->graph, vs->vertices);
}

void Graph::DeleteExtraTailVertices() {
  // unnecessary to delete
  // dbg(this->GetGraphAttributeString("name"), this->cur_vertex_num,
  // igraph_vcount(&ipag_->graph));
  if (this->GetCurVertexNum() - 1 == igraph_vcount(&ipag_->graph)) {
    return;
  } else if (this->GetCurVertexNum() - 1 > igraph_vcount(&ipag_->graph)) {
    dbg("Error: The number of vertices is larger than pre-allocated gragh "
        "size");
    return;
  }

  // Check the number of vertices
  type::vertex_set_t vs;

  igraph_vs_seq(&vs.vertices, this->cur_vertex_num,
                igraph_vcount(&ipag_->graph) - 1);
  this->DeleteVertices(&vs);
  igraph_vs_destroy(&vs.vertices);
}

void Graph::Dfs() { UNIMPLEMENTED(); }

// void Graph::Dfs(igraph_dfshandler_t in_callback,
// igraph_dfshandler_t out_callback) {
//   igraph_dfs(&ipag_.graph, /*root=*/0, /*mode=*/ IGRAPH_OUT,
//                /*unreachable=*/ 1, /*order=*/ 0, /*order_out=*/ 0,
//                /*father=*/ 0, /*dist=*/ 0,
//                /*in_callback=*/ in_callback, /*out_callback=*/ out_callback,
//                /*extra=*/ 0);
// }

void Graph::ReadGraphGML(const char *file_name) {
  FILE *in_file = fopen(file_name, "r");
  igraph_read_graph_gml(&ipag_->graph, in_file);
  const char *graph_name = VAS(&ipag_->graph, "name", 0);
  SETGAS(&ipag_->graph, "name", graph_name);
  fclose(in_file);

  this->cur_vertex_num = igraph_vcount(&ipag_->graph);
  this->cur_edge_num = igraph_ecount(&ipag_->graph);
}

void Graph::DumpGraphGML(const char *file_name) {
  this->UpdateEdges();
  this->DeleteExtraTailVertices();

  // Real dump
  FILE *out_file = fopen(file_name, "w");
  igraph_write_graph_gml(&ipag_->graph, out_file, 0, "Bruce Jin");
  fclose(out_file);
}

void Graph::DumpGraphDot(const char *file_name) {
  this->UpdateEdges();
  this->DeleteExtraTailVertices();

  // Real dump
  FILE *out_file = fopen(file_name, "w");
  igraph_write_graph_dot(&ipag_->graph, out_file);
  fclose(out_file);
}

void Graph::VertexTraversal(void (*CALL_BACK_FUNC)(Graph *, int, void *),
                            void *extra) {
  this->DeleteExtraTailVertices();
  igraph_vs_t vs;
  igraph_vit_t vit;
  // printf("Function %s Start:\n", this->GetGraphAttributeString("name"));
  // dbg(GetGraphAttributeString("name"));
  igraph_vs_all(&vs);
  igraph_vit_create(&ipag_->graph, vs, &vit);
  while (!IGRAPH_VIT_END(vit)) {
    // Get vector id
    type::vertex_t vertex_id = (type::vertex_t)IGRAPH_VIT_GET(vit);
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

int Graph::GetCurVertexNum() { return this->cur_vertex_num; }

void Graph::GetChildVertexSet(type::vertex_t vertex,
                              std::vector<type::vertex_t> &neighbor_vertices) {
  // std::vector<type::vertex_t> neighbor_vertices;
  igraph_vector_t v;
  // dbg(this->GetGraphAttributeString("name"), vertex);
  igraph_vector_init(&v, 0);
  igraph_neighbors(&ipag_->graph, &v, vertex, IGRAPH_OUT);
  long int neighbor_num = igraph_vector_size(&v);
  // dbg(neighbor_num);
  for (long int i = 0; i < neighbor_num; i++) {
    neighbor_vertices.push_back((type::vertex_t)VECTOR(v)[i]);
  }
  igraph_vector_destroy(&v);
  // return neighbor_vertices;
}

type::vertex_t Graph::GetParentVertex(type::vertex_t vertex_id) {
  igraph_vector_t v;
  // dbg(this->GetGraphAttributeString("name"), vertex);
  igraph_vector_init(&v, 0);
  igraph_neighbors(&ipag_->graph, &v, vertex_id, IGRAPH_IN);
  long int neighbor_num = igraph_vector_size(&v);
  // dbg(neighbor_num);
  if (neighbor_num > 1) {
    dbg("More than one parent");
  }
  type::vertex_t parent_vertex_id = (type::vertex_t)VECTOR(v)[0];
  igraph_vector_destroy(&v);
  return parent_vertex_id;
}

struct dfs_call_back_t {
  void (*IN_CALL_BACK_FUNC)(Graph *, int, void *);
  void (*OUT_CALL_BACK_FUNC)(Graph *, int, void *);
  Graph *g;
  void *extra;
};

igraph_bool_t g_in_callback(const igraph_t *graph, igraph_integer_t vid,
                            igraph_integer_t dist, void *extra) {
  struct dfs_call_back_t *extra_wrapper = (struct dfs_call_back_t *)extra;
  if (extra_wrapper->IN_CALL_BACK_FUNC) {
    (*(extra_wrapper->IN_CALL_BACK_FUNC))(extra_wrapper->g, vid,
                                          extra_wrapper->extra);
  }
  return 0;
}

igraph_bool_t g_out_callback(const igraph_t *graph, igraph_integer_t vid,
                             igraph_integer_t dist, void *extra) {
  struct dfs_call_back_t *extra_wrapper = (struct dfs_call_back_t *)extra;
  if (extra_wrapper->OUT_CALL_BACK_FUNC) {
    (*(extra_wrapper->OUT_CALL_BACK_FUNC))(extra_wrapper->g, vid,
                                           extra_wrapper->extra);
  }
  return 0;
}

void Graph::DFS(type::vertex_t root,
                void (*IN_CALL_BACK_FUNC)(Graph *, int, void *),
                void (*OUT_CALL_BACK_FUNC)(Graph *, int, void *), void *extra) {
  // Graph* new_graph = new Graph();
  // new_graph->GraphInit();
  struct dfs_call_back_t *extra_wrapper = new (struct dfs_call_back_t)();
  extra_wrapper->IN_CALL_BACK_FUNC = IN_CALL_BACK_FUNC;
  extra_wrapper->OUT_CALL_BACK_FUNC = OUT_CALL_BACK_FUNC;
  extra_wrapper->g = this;
  extra_wrapper->extra = extra;

  igraph_dfs(&(this->ipag_->graph), /*root=*/root, /*neimode=*/IGRAPH_OUT,
             /*unreachable=*/0, /*order=*/0, /*order_out=*/0,
             /*father=*/0, /*dist=*/0,
             /*in_callback=*/g_in_callback, /*out_callback=*/g_out_callback,
             /*extra=*/extra_wrapper);
  ///*in_callback=*/0, /*out_callback=*/0, /*extra=*/extra_wrapper);
}

struct bfs_call_back_t {
  void (*CALL_BACK_FUNC)(Graph *, int, void *);
  Graph *g;
  void *extra;
};

igraph_bool_t bfs_callback(const igraph_t *graph, igraph_integer_t vid,
                           igraph_integer_t pred, igraph_integer_t succ,
                           igraph_integer_t rank, igraph_integer_t dist,
                           void *extra) {
  struct bfs_call_back_t *extra_wrapper = (struct bfs_call_back_t *)extra;
  if (extra_wrapper->CALL_BACK_FUNC) {
    (*(extra_wrapper->CALL_BACK_FUNC))(extra_wrapper->g, vid,
                                       extra_wrapper->extra);
  }
  return 0;
}

void Graph::BFS(type::vertex_t root,
                void (*CALL_BACK_FUNC)(Graph *, int, void *), void *extra) {
  struct bfs_call_back_t *extra_wrapper = new (struct bfs_call_back_t)();
  extra_wrapper->CALL_BACK_FUNC = CALL_BACK_FUNC;
  extra_wrapper->g = this;
  extra_wrapper->extra = extra;

  // igraph_vector_t order, rank, father, pred, succ, dist;
  // /* Initialize the vectors where the result will be stored. Any of these
  //    * can be omitted and replaced with a null pointer when calling
  //    * igraph_bfs() */
  //   igraph_vector_init(&order, 0);
  //   igraph_vector_init(&rank, 0);
  //   igraph_vector_init(&father, 0);
  //   igraph_vector_init(&pred, 0);
  //   igraph_vector_init(&succ, 0);
  //   igraph_vector_init(&dist, 0);
  igraph_bfs(
      &(this->ipag_->graph), /*root=*/root, /*roots=*/0, /*neimode=*/IGRAPH_OUT,
      /*unreachable=*/0, /*restricted=*/0,
      /*order=*/0, /*rank=*/0, /*father=*/0, /*pred=*/0, /*succ=*/0, /*dist=*/0,
      /*callback=*/bfs_callback, /*extra=*/extra_wrapper);
  // /* Clean up after ourselves */
  // igraph_vector_destroy(&order);
  // igraph_vector_destroy(&rank);
  // igraph_vector_destroy(&father);
  // igraph_vector_destroy(&pred);
  // igraph_vector_destroy(&succ);
  // igraph_vector_destroy(&dist);
}

igraph_bool_t pre_order_callback(const igraph_t *graph, igraph_integer_t vid,
                                 igraph_integer_t dist, void *extra) {
  std::vector<type::vertex_t> *pre_order_vertex_vec =
      (std::vector<type::vertex_t> *)extra;
  pre_order_vertex_vec->push_back(vid);
  return 0;
}

void Graph::PreOrderTraversal(
    type::vertex_t root, std::vector<type::vertex_t> &pre_order_vertex_vec) {
  // Graph* new_graph = new Graph();
  // new_graph->GraphInit();

  igraph_dfs(&(this->ipag_->graph), /*root=*/root, /*neimode=*/IGRAPH_OUT,
             /*unreachable=*/1, /*order=*/0, /*order_out=*/0,
             /*father=*/0, /*dist=*/0,
             /*in_callback=*/pre_order_callback, /*out_callback=*/0,
             /*extra=*/&pre_order_vertex_vec);
}

GraphPerfData *Graph::GetGraphPerfData() { return this->graph_perf_data; }

template <typename T> inline void print_vector(std::vector<T> &vec) {
  for (auto e : vec) {
    printf("%d ", e);
  }
  printf("\n");
}

struct vid_value_t {
  type::vertex_t vertex_id;
  type::perf_data_t value;
  vid_value_t(type::vertex_t vid, type::perf_data_t v)
      : vertex_id(vid), value(v) {}
  bool operator<(const vid_value_t &viva) const { return (value < viva.value); }
};

void SortChildren(Graph *g, int vertex_id, void *extra) {
  char *attr = (char *)extra;
  // dbg(attr);

  std::vector<type::vertex_t> children_id;
  g->GetChildVertexSet(vertex_id, children_id);

  /** If this vertex has no child, return now */
  if (0 == children_id.size()) {
    return;
  }

  /** ---------- Start sorting ---------- */
  /** Collect child pairs <vid, value> */
  std::vector<vid_value_t> children_id_value_pair;

  for (auto &child : children_id) {
    type::perf_data_t value =
        (type::perf_data_t)g->GetVertexAttributeNum(attr, child);
    // dbg(child, value);
    children_id_value_pair.push_back(vid_value_t(child, value));
  }
  /** Sort by input attr */
  std::sort(children_id_value_pair.begin(), children_id_value_pair.end());

  /** Convert pair <id, s_addr> to two vector **/
  std::vector<type::vertex_t> sorted_children_id;
  // std::vector<type::perf_data_t> children_value;

  for (auto &viva : children_id_value_pair) {
    sorted_children_id.push_back(viva.vertex_id);
    // children_value.push_back(viva.value);
  }
  // dbg(vertex_id);
  // print_vector<type::vertex_t>(children_id);
  // print_vector<type::vertex_t>(sorted_children_id);
  // dbg(children_id, sorted_children_id);
  /** ---------- Sorting complete ---------- */

  /** ---------- Start swaping vertices ---------- */
  /** vector children_id is original sequence, vector sorted_children_id is
   * sorted sequence */
  Graph *tmp_g = new Graph(); // store tempory swap vertices
  tmp_g->GraphInit("tmp");

  int num_children = children_id.size();
  std::map<type::vertex_t, type::vertex_t> vertex_id_to_tmp_vertex_id;
  std::map<type::vertex_t, std::vector<type::edge_t>>
      vertex_id_to_tmp_edge_dest_id_vec;

  /** Record original <child vertex, vector<destination of child's edge>> at
   * first*/
  for (int i = 0; i < num_children; i++) {
    if (children_id[i] != sorted_children_id[i]) {
      // Get and record children_id of children_id[i]
      std::vector<type::vertex_t> children_of_child;
      g->GetChildVertexSet(children_id[i], children_of_child);
      vertex_id_to_tmp_edge_dest_id_vec[children_id[i]] = children_of_child;
    }
  }

  /** Swap attributes and edges */
  for (int i = 0; i < num_children; i++) {
    if (children_id[i] != sorted_children_id[i]) {
      /** TODO: store temporary vertex in a new graph which will not inflect
       * original graph */

      /** Copy attributes except "id" from children_id[i] to a new temporary
       * vertex */
      type::vertex_t tmp_vertex_id = tmp_g->AddVertex();
      tmp_g->CopyVertex(tmp_vertex_id, g, children_id[i]);

      /** If sorted_children_id[i] is covered, use corresponding temporary
       * vertex in vertex_id_to_tmp_vertex_id, otherwise, original vertex is
       * used for copy.
       */
      if (vertex_id_to_tmp_vertex_id.count(sorted_children_id[i]) > 0) {
        g->CopyVertex(children_id[i], tmp_g,
                      vertex_id_to_tmp_vertex_id[sorted_children_id[i]]);
        // g->SetVertexAttributeNum("id", children_id[i],
        // sorted_children_id[i]);
      } else {
        g->CopyVertex(children_id[i], g, sorted_children_id[i]);
      }
      vertex_id_to_tmp_vertex_id[children_id[i]] = tmp_vertex_id;

      // /** TODO: can not understand now */
      /** Delete all edges of children_id[i] */
      std::vector<type::vertex_t> &children_of_child =
          vertex_id_to_tmp_edge_dest_id_vec[children_id[i]];
      for (auto &child_of_child : children_of_child) {
        // dbg(children_id[i], child_of_child);
        g->DeleteEdge(children_id[i], child_of_child);
      }

      /** Add new edges for children_id[i] */
      if (vertex_id_to_tmp_edge_dest_id_vec.count(sorted_children_id[i]) > 0) {
        std::vector<type::vertex_t> &new_children_of_child =
            vertex_id_to_tmp_edge_dest_id_vec[sorted_children_id[i]];
        for (auto &new_child_of_child : new_children_of_child) {
          // dbg(children_id[i], new_child_of_child);
          g->AddEdge(children_id[i], new_child_of_child);
        }
      } else {
        dbg("sorted_children_id[i] not found in "
            "vertex_id_to_tmp_edge_dest_id_vec");
        // Get children_id of sorted_children_id[i]
        std::vector<type::vertex_t> children_id_children;
        g->GetChildVertexSet(sorted_children_id[i], children_id_children);
        // dbg(sorted_children_id[i], children_id_children);

        for (auto &child_of_child : children_id_children) {
          // dbg(children_id[i], child_of_child);
          g->AddEdge(children_id[i], child_of_child);
        }
        FREE_CONTAINER(children_id_children);
      }
    }
  }

  // for (auto& vertex: vertex_id_to_tmp_vertex_id) {
  //   dbg(g->GetCurVertexNum() - 1);
  //   g->DeleteVertex(g->GetCurVertexNum() - 1);
  // }

  // for (auto &v_pair : vertex_id_to_tmp_vertex_id) {
  //   // dbg(v_pair.second);
  //   tmp_g->DeleteVertex(v_pair.second);
  // }

  /** ---------- Swaping vertices complete ---------- */

  delete tmp_g;
  FREE_CONTAINER(children_id);
  FREE_CONTAINER(children_id_value_pair);
  FREE_CONTAINER(sorted_children_id);
  // FREE_CONTAINER(children_value);
  FREE_CONTAINER(vertex_id_to_tmp_vertex_id);
  for (auto &item : vertex_id_to_tmp_edge_dest_id_vec) {
    FREE_CONTAINER(item.second);
  }
  FREE_CONTAINER(vertex_id_to_tmp_edge_dest_id_vec);

  return;
}

void Graph::SortBy(type::vertex_t starting_vertex, const char *attr_name) {
  // char* attr = attr_name;
  // dbg()l
  this->BFS(starting_vertex, &SortChildren, (void *)attr_name);
  return;
}

} // namespace baguatool::core
