## PerFlow Call Tree Viewer

纯静态页面，用于可视化 `perflow_post_analysis --json` 等导出的数据：

- `calltree.json`：调用树（可逐层展开）与右侧节点详情
- `node_imbalance.json`（可选）：与 calltree 的 `node_id` 对应；用于详情栏「所有进程数据」柱状图，以及 calltree 工具栏中的**数据展示**切换（全局合并 / 单 rank 占比 / 全 rank 平均 / 自定义 rank 集合平均——后三种在同一棵合并树上按 per-rank 占比重算列表中的主指标）

### 使用方式

1) 生成 JSON：

```bash
mkdir -p path/to/json_output_directory
path/to/perflow_post_analysis --json path/to/json_output_directory
```

2) 用静态服务器打开（避免浏览器本地文件跨域限制）：

```bash
cd examples/web_viewer
python3 -m http.server 8000
```

3) 浏览器打开 `http://<host>:8000/`，选择 `calltree.json` 与（可选）`node_imbalance.json` 后点击「渲染」，或使用「从 json_out 加载」（需将 JSON 放在 `json_out/` 下）。

### 说明

- 当前页面**不读取** `imbalance.json`。
- 均衡区为**两列**：左「单帧」、右「融合」。侧栏添加的图进左列；任意两个 subpanel 互相拖拽融合后，新图飞入**右列**，原有两块保留。
- 热点列表（hotspot）界面已暂时移除。
