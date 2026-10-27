// Motor do visualizador de fluxos, compartilhado por index.html e index-poc.html.
// Lê data/firmwares.json (ou a cópia data/firmwares.js aberta do disco) e desenha
// cada grafo no elemento da página que tem o mesmo id.
(function () {
  "use strict";

  // ---------------------------------------------------------------------
  // Dados dos grafos.  nó: [id, tipo, rótulo, detalhe, papel]   aresta: [de, para, rótulo, "back"?]
  // ---------------------------------------------------------------------
  var GRAPHS = {};

  // ---------------------------------------------------------------------
  // Renderização
  // ---------------------------------------------------------------------
  var KIND = {
    start: { bg: "--n-start-bg", bd: "--n-start-bd", shape: "box", dashes: false, bold: true },
    step:  { bg: "--n-step-bg",  bd: "--n-step-bd",  shape: "box", dashes: false },
    dec:   { bg: "--n-dec-bg",   bd: "--n-dec-bd",   shape: "box", dashes: [6, 4] },
    io:    { bg: "--n-io-bg",    bd: "--n-io-bd",    shape: "box", dashes: false },
    sleep: { bg: "--n-sleep-bg", bd: "--n-sleep-bd", shape: "box", dashes: false },
    sub:   { bg: "--n-sub-bg",   bd: "--n-sub-bd",   shape: "box", dashes: false }
  };
  var KIND_NAME = { start: "início", step: "passo", dec: "decisão", io: "rádio / sensor", sleep: "sono", sub: "callback / API" };
  // Papel na prova de conceito: núcleo operacional (borda grossa) ou supérfluo (apagado, borda tracejada).
  var ROLE_NAME = { nucleo: "núcleo operacional", superfluo: "supérfluo para a prova de conceito" };

  var instances = [];

  function css(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  function esc(s) {
    return String(s).replace(/[&<>"]/g, function (c) {
      return { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c];
    });
  }

  // Níveis por caminho mais longo, ignorando arestas "back" (ciclos).
  function levels(def) {
    var lv = {};
    def.nodes.forEach(function (n) { lv[n[0]] = 0; });
    var fwd = def.edges.filter(function (e) { return e[3] !== "back"; });
    for (var pass = 0; pass < def.nodes.length; pass++) {
      var changed = false;
      fwd.forEach(function (e) {
        if (lv[e[1]] < lv[e[0]] + 1) { lv[e[1]] = lv[e[0]] + 1; changed = true; }
      });
      if (!changed) break;
    }
    return lv;
  }

  function build(id) {
    var def = GRAPHS[id];
    var lv = levels(def);
    var nodes = def.nodes.map(function (n) {
      var k = KIND[n[1]], off = n[4] === "superfluo", core = n[4] === "nucleo";
      var bg = css(off ? "--n-off-bg" : k.bg), bd = css(off ? "--n-off-bd" : k.bd);
      return {
        id: n[0], label: n[2], shape: k.shape, level: lv[n[0]],
        color: { background: bg, border: bd, highlight: { background: bg, border: css("--accent") } },
        borderWidth: core ? 4 : 2, borderWidthSelected: core ? 5 : 3,
        shapeProperties: { borderDashes: off ? [4, 4] : k.dashes, borderRadius: 8 },
        font: { color: css(off ? "--n-off-text" : "--node-text"), size: 14, face: "system-ui, sans-serif", bold: k.bold ? "bold" : undefined },
        margin: 10, widthConstraint: { maximum: 260 }
      };
    });
    var edges = def.edges.map(function (e, i) {
      var back = e[3] === "back";
      return {
        id: "e" + i, from: e[0], to: e[1], label: e[2] || undefined,
        arrows: e[3] === "both" ? "to, from" : "to", dashes: back ? [2, 5] : false, width: back ? 1 : 1.5,
        color: { color: css("--edge"), highlight: css("--accent") },
        font: { color: css("--muted"), size: 12, strokeWidth: 4, strokeColor: css("--surface"), align: "horizontal" },
        smooth: back ? { type: "curvedCW", roundness: 0.35 } : { type: "cubicBezier", roundness: 0.5 }
      };
    });
    return { nodes: new vis.DataSet(nodes), edges: new vis.DataSet(edges) };
  }

  function options(def) {
    return {
      layout: { hierarchical: { enabled: true, direction: def.dir, sortMethod: "directed",
                                levelSeparation: def.dir === "LR" ? 360 : 95, nodeSpacing: def.dir === "LR" ? 110 : 280,
                                treeSpacing: 260, shakeTowards: "roots" } },
      physics: { enabled: false },
      interaction: { dragView: false, zoomView: false, hover: true, tooltipDelay: 150 },
      nodes: { chosen: true }
    };
  }

  function showDetail(box, def, nodeId) {
    if (!nodeId) {
      box.innerHTML = '<span class="hint">Clique em um bloco para ver detalhes.' + (def.extra ? "<br>" + esc(def.extra) : "") + "</span>";
      return;
    }
    var n = def.nodes.filter(function (x) { return x[0] === nodeId; })[0];
    var label = n[2].replace(/\n/g, " ");
    box.innerHTML = "<b>" + esc(label) + '</b> <span class="hint">(' + KIND_NAME[n[1]] +
                    (n[4] ? " · " + ROLE_NAME[n[4]] : "") + ")</span><br>" +
                    (n[3] ? esc(n[3]) : '<span class="hint">Sem detalhes adicionais.</span>');
  }

  function mount(id) {
    var el = document.getElementById(id);
    var card = el.closest(".card");
    var box = card.querySelector(".detail");
    var tools = card.querySelector(".tools");
    var def = GRAPHS[id];
    var data = build(id);
    var network = new vis.Network(el, data, options(def));
    showDetail(box, def, null);

    network.on("click", function (p) { showDetail(box, def, p.nodes.length ? p.nodes[0] : null); });
    network.once("afterDrawing", function () { network.fit({ animation: false }); });

    function zoom(f) {
      network.moveTo({ scale: network.getScale() * f, animation: { duration: 150, easingFunction: "easeInOutQuad" } });
    }
    [["+", "Aproximar", function () { zoom(1.25); }],
     ["−", "Afastar", function () { zoom(0.8); }],
     ["⤢", "Ajustar à tela", function () { network.fit({ animation: { duration: 200 } }); }]
    ].forEach(function (b) {
      var btn = document.createElement("button");
      btn.type = "button"; btn.textContent = b[0]; btn.title = b[1]; btn.setAttribute("aria-label", b[1]);
      btn.addEventListener("click", b[2]);
      tools.appendChild(btn);
    });

    instances.push({ id: id, network: network, el: el, box: box, def: def });
  }

  function recolor() {
    instances.forEach(function (inst) {
      var data = build(inst.id);
      inst.network.setData(data);
      inst.network.once("afterDrawing", function () { inst.network.fit({ animation: false }); });
    });
  }

  if (typeof vis === "undefined") {
    document.querySelector("header").insertAdjacentHTML("beforeend",
      '<p class="detail">Não foi possível carregar vendor/vis-network.min.js. Abra esta página a partir da pasta docs/ do repositório.</p>');
    return;
  }

  function normalize(data) {
    var out = {};
    Object.keys(data.graphs).forEach(function (id) {
      var g = data.graphs[id];
      out[id] = {
        title: g.title || id, dir: g.direction || "UD", extra: g.note || "",
        page: g.page || "index",
        nodes: g.nodes.map(function (n) { return [n.id, n.kind, n.label, n.detail || "", n.role || ""]; }),
        edges: g.edges.map(function (e) { return [e.from, e.to, e.label || "", e.style || ""]; })
      };
    });
    return out;
  }

  // Grafo sem lugar na página vai para "Outros grafos" (só no index.html, que tem #extras).
  // Grafos com "page": "poc" pertencem ao index-poc.html e não entram no index.html.
  function ensureContainers() {
    var extras = document.getElementById("extras");
    if (!extras) return;
    Object.keys(GRAPHS).forEach(function (id) {
      if (document.getElementById(id) || GRAPHS[id].page !== "index") return;
      if (!extras.firstChild) extras.insertAdjacentHTML("beforeend", '<h2>Outros grafos</h2>');
      extras.insertAdjacentHTML("beforeend",
        "<h3>" + esc(GRAPHS[id].title) + '</h3><div class="card"><div class="graph-wrap"><div class="tools"></div>' +
        '<div class="graph" id="' + esc(id) + '" style="height:560px"></div></div><div class="detail"></div></div>');
    });
  }

  function start(data, origin) {
    GRAPHS = normalize(data);
    ensureContainers();
    Object.keys(GRAPHS).filter(function (id) { return document.getElementById(id); }).forEach(mount);
    document.getElementById("src").textContent =
      origin === "json" ? "" : "(carregado de data/firmwares.js, a cópia gerada; rode tools/gen_firmwares_js.mjs após editar o JSON)";
  }

  fetch("data/firmwares.json", { cache: "no-store" })
    .then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); })
    .then(function (d) { start(d, "json"); })
    .catch(function () {
      // Aberto direto do disco (file://) o navegador bloqueia o fetch: usa a cópia gerada.
      if (window.FIRMWARE_GRAPHS) start(window.FIRMWARE_GRAPHS, "js");
      else document.querySelector("header").insertAdjacentHTML("beforeend",
        '<p class="detail">Não foi possível carregar data/firmwares.json nem data/firmwares.js.</p>');
    });

  var mq = window.matchMedia("(prefers-color-scheme: dark)");
  if (mq.addEventListener) mq.addEventListener("change", recolor);
  window.addEventListener("resize", function () {
    instances.forEach(function (i) { i.network.redraw(); });
  });
})();
