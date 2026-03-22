## PerFlow Call Tree Viewer

这是一个**纯静态**的网页，用于可视化 `post_analysis_example --json` 导出的数据：

- `calltree.json`：调用树（可逐层展开）
- `hotspot.json`：热点函数列表（Top 200）

### 使用方式

1) 先生成 JSON：

```bash
mkdir -p path/to/json_output_directory
 \
  path/to/post_analysis_example \
 --json path/to/json_output_directory
```

2) 用静态服务器打开网页（避免浏览器本地文件跨域限制）：

```bash
cd examples/web_viewer
python3 -m http.server 8000
```

3) 浏览器打开 `http://<host>:8000/`，选择：

- `calltree.json`
- `hotspot.json`（可选）
- `imbalance.json`（可选）
- `node_imlalance.json`

然后点击“渲染”。

