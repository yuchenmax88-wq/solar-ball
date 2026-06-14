/* ---- i18n ---- */
const I18N = {
  zh: {
    tab_overview: "总览", tab_detail: "详情", tab_charts: "历史图表", tab_ota: "固件升级", tab_config: "配置向导",
    waiting_data: "等待 Solar Ball 数据...", select_ball: "-- 选择设备 --", select: "-- 选择 --",
    ball: "设备", points: "点数", load: "加载",
    chart_traj: "方向轨迹", chart_tele: "电池 & 信号", chart_conf: "置信度趋势", chart_fault: "故障时间线",
    ota_title: "固件升级", ota_send: "发送升级指令",
    cfg_title: "生成 config.h", cfg_desc: "配置你的 Solar Ball，然后复制生成的头文件。",
    ball_id: "设备 ID", fw_url: "固件 URL", version: "版本", fw_version: "固件版本",
    mqtt_port: "MQTT 端口", apn_user: "APN 用户名", apn_pass: "APN 密码",
    scan_interval: "扫描间隔 (s)", bat_full: "满电电压 (mV)", bat_empty: "空电电压 (mV)", generate: "生成",
    connecting: "连接中", live: "在线", offline: "离线", balls_unit: "台设备",
    ago: "前", s: "秒", dir_vector: "方向向量", panel_orient: "面板朝向",
    x_east: "X (东)", y_north: "Y (北)", z_up: "Z (上)", magnitude: "模长", valid: "有效",
    panel_az: "面板方位角", panel_tilt: "面板倾角", timestamp: "时间戳", error_mask: "故障码", flags: "标志位",
    faults_flags: "故障 & 标志", no_faults: "无故障",
    azimuth: "方位角", elevation: "高度角", battery: "电池", signal: "信号", confidence: "置信度",
    dir_valid: "方向有效", dir_invalid: "方向无效",
    ota_sending: "发送中...", ota_ok: "已发送到 {ball}。设备将在下次唤醒时更新。",
    ota_fail: "失败：{msg}", url_required: "请输入固件 URL", copy_btn: "复制到剪贴板",
    no_ball_selected: "请选择设备",
  },
  en: {
    tab_overview: "Overview", tab_detail: "Detail", tab_charts: "Charts", tab_ota: "OTA Update", tab_config: "Config",
    waiting_data: "Waiting for Solar Ball data...", select_ball: "-- Select Ball --", select: "-- Select --",
    ball: "Ball", points: "Points", load: "Load",
    chart_traj: "Direction Trajectory", chart_tele: "Battery & Signal", chart_conf: "Confidence Trend", chart_fault: "Fault Timeline",
    ota_title: "Firmware Update", ota_send: "Send Update",
    cfg_title: "Generate config.h", cfg_desc: "Configure your Solar Ball, then copy the generated header.",
    ball_id: "Ball ID", fw_url: "Firmware URL", version: "Version", fw_version: "Firmware Version",
    mqtt_port: "MQTT Port", apn_user: "APN Username", apn_pass: "APN Password",
    scan_interval: "Scan Interval (s)", bat_full: "Battery Full (mV)", bat_empty: "Battery Empty (mV)", generate: "Generate",
    connecting: "connecting", live: "Live", offline: "Offline", balls_unit: "balls",
    ago: "ago", s: "s", dir_vector: "Direction Vector", panel_orient: "Panel Orientation",
    x_east: "X (east)", y_north: "Y (north)", z_up: "Z (up)", magnitude: "Magnitude", valid: "Valid",
    panel_az: "Panel Azimuth", panel_tilt: "Panel Tilt", timestamp: "Timestamp", error_mask: "Error Mask", flags: "Flags",
    faults_flags: "Faults & Flags", no_faults: "No faults",
    azimuth: "Azimuth", elevation: "Elevation", battery: "Battery", signal: "Signal", confidence: "Confidence",
    dir_valid: "Direction Valid", dir_invalid: "Direction Invalid",
    ota_sending: "Sending...", ota_ok: "Sent to {ball}. Device will update on next cycle.",
    ota_fail: "Failed: {msg}", url_required: "URL is required", copy_btn: "Copy to Clipboard",
    no_ball_selected: "Select a ball",
  }
};

let currentLang = localStorage.getItem("solar-ball-lang") || "zh";
function t(key, params) {
  let text = I18N[currentLang]?.[key] || I18N.en[key] || key;
  if (params) { for (const [k,v] of Object.entries(params)) text = text.replace("{"+k+"}", v); }
  return text;
}

function applyI18n() {
  document.documentElement.lang = currentLang;
  document.title = currentLang === "zh" ? "Solar Ball · 太阳能追踪仪表板" : "Solar Ball · Solar Tracking Dashboard";
  document.querySelectorAll("[data-i18n]").forEach(el => {
    const key = el.getAttribute("data-i18n");
    const isOption = el.tagName === "OPTION" && el.value === "" && el.parentElement.tagName === "SELECT";
    if (isOption) el.textContent = t(key);
    else if (!el.hasAttribute("data-i18n-attr")) el.textContent = t(key);
  });
  document.querySelectorAll(".lang-btn").forEach(b => {
    b.classList.toggle("active", b.dataset.lang === currentLang);
  });
  renderAll();
}

document.querySelectorAll(".lang-btn").forEach(btn => {
  btn.addEventListener("click", () => {
    const lang = btn.dataset.lang;
    if (lang === currentLang) return;
    currentLang = lang;
    localStorage.setItem("solar-ball-lang", lang);
    applyI18n();
  });
});

applyI18n();

/* ---- Fault definitions (names are language-independent codes) ---- */
const FAULTS = {
  0x0001: ["SENSOR_OPEN", "warn"],   0x0002: ["SENSOR_SHORT", "warn"],
  0x0004: ["SATURATED", "info"],     0x0008: ["ADC_ERR", "err"],
  0x0010: ["MODEM_ERR", "err"],      0x0020: ["MQTT_ERR", "err"],
  0x0040: ["LOW_BAT", "err"],        0x0080: ["NO_CALIB", "warn"],
  0x0100: ["OVERCAST", "info"],      0x0200: ["NIGHT", "info"],
};

let allBalls = {};
let renderThrottle = null;
let chartInstances = {};

/* ---- SSE ---- */
const evtSource = new EventSource("/stream");
evtSource.onopen = () => {
  allBalls = {};
  document.getElementById("conn-status").className = "status-dot online";
  document.getElementById("conn-text").textContent = t("live");
};
evtSource.onerror = () => {
  document.getElementById("conn-status").className = "status-dot offline";
  document.getElementById("conn-text").textContent = t("offline");
};
evtSource.onmessage = (event) => {
  try {
    const msg = JSON.parse(event.data);
    if (msg.ping) return;
    allBalls[msg.ball_id] = msg.data;
    document.getElementById("ball-count").textContent = Object.keys(allBalls).length;
    document.getElementById("conn-balls").textContent = t("balls_unit");
    scheduleRender();
  } catch (e) {}
};

function scheduleRender() {
  if (renderThrottle) return;
  renderThrottle = setTimeout(() => { renderThrottle = null; renderAll(); }, 250);
}

/* ---- Tab switching ---- */
document.querySelectorAll(".tab").forEach(t => {
  t.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach(tt => tt.classList.remove("active"));
    t.classList.add("active");
    document.querySelectorAll(".panel").forEach(p => p.classList.add("hidden"));
    document.getElementById("panel-" + t.dataset.tab).classList.remove("hidden");
  });
});

document.getElementById("detail-select")?.addEventListener("change", () => renderDetail());

function renderAll() { renderOverview(); renderDetail(); populateSelects(); populateChartSelect(); }

function badgeClass(kind) {
  if (kind === "ok") return "badge-ok";
  if (kind === "warn") return "badge-warn";
  if (kind === "err") return "badge-err";
  return "badge-info";
}
function esc(s) { return String(s||"").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;"); }

/* ---- Overview — one card per ball ---- */
function renderOverview() {
  const container = document.getElementById("balls-container");
  const ids = Object.keys(allBalls);
  if (ids.length === 0) {
    container.innerHTML = `<div class="empty-state"><div class="icon">&#9788;</div><p>${t("waiting_data")}</p></div>`;
    return;
  }
  let html = "";
  for (const bid of ids) {
    const d = allBalls[bid]; if (!d) continue;
    const az = d.azimuth||0, elev = d.elevation||0, soc = d.soc||0, rssi = d.rssi||0, conf = d.conf||0;
    const faults = d.fault_list||[], flags = d.flag_list||[];
    const age = d.ts ? Math.max(0, Math.floor(Date.now()/1000-d.ts)) : 999;
    const sc = d.vector_ok ? "ok" : (faults.length?"warn":"warn");
    html += `<div class="card">
      <div class="card-title"><span class="dot ${sc}"></span>${esc(bid)}<span style="font-weight:400;font-size:.75em;color:var(--muted);margin-left:auto">${age}${t("s")} ${t("ago")}</span></div>
      <div class="sun-viz" id="sunviz-${bid}"><div class="sky"></div><div class="ground"></div>
        <div class="panel-group">${Array(5).fill(0).map(()=>'<div class="panel-unit" style="transform:rotateX(0deg)"></div>').join("")}</div>
        <div class="sun-dot" style="left:50%;top:50%"></div></div>
      <div class="stat-row"><span class="stat-label">${t("azimuth")}</span><span class="stat-value">${az.toFixed(0)}°</span></div>
      <div class="stat-row"><span class="stat-label">${t("elevation")}</span><span class="stat-value">${elev.toFixed(0)}°</span></div>
      <div class="stat-row"><span class="stat-label">${t("battery")}</span><span class="stat-value">${soc}%</span></div>
      <div class="bar bar-soc"><div class="bar-fill" style="width:${soc}%"></div></div>
      <div class="stat-row"><span class="stat-label">${t("signal")}</span><span class="stat-value">${rssi} dBm</span></div>
      <div class="stat-row"><span class="stat-label">${t("confidence")}</span><span class="stat-value">${conf}/255</span></div>
      <div class="bar bar-conf"><div class="bar-fill" style="width:${conf/255*100}%"></div></div>
      <div style="margin-top:8px">
        ${flags.map(f=>`<span class="badge badge-ok">${f}</span>`).join("")}
        ${faults.map(f=>{const def=Object.values(FAULTS).find(v=>v[0]===f);return`<span class="badge ${badgeClass(def?def[1]:'warn')}">${f}</span>`;}).join("")}
      </div></div>`;
    setTimeout(() => drawSunViz(bid, az, elev), 40);
  }
  container.innerHTML = html;
}

function drawSunViz(bid, az, elev) {
  const viz = document.getElementById("sunviz-"+bid); if (!viz) return;
  const dot = viz.querySelector(".sun-dot");
  const panels = viz.querySelectorAll(".panel-unit"); if (!dot) return;
  const rect = viz.getBoundingClientRect(), w = rect.width, h = rect.height;
  const gtY = h*.75, skyH = h-gtY, maxR = Math.min(w, skyH*2)*.42, cx = w/2, cy = gtY;
  const elNorm = Math.max(-10, Math.min(90, elev))/90;
  const radius = elNorm*maxR+2;
  const azRad = (az-90)*Math.PI/180;
  const sx = cx + radius*Math.cos(azRad), sy = cy - radius*Math.sin(azRad)*(skyH/(maxR+10));
  dot.style.left = sx+"px"; dot.style.top = sy+"px";
  dot.style.opacity = elev>0?"1":".3";
  dot.style.width = dot.style.height = (18+elNorm*14)+"px";
  const panelTilt = (90-elev)/90;
  panels.forEach(p=>{ p.style.transform="rotateX("+(panelTilt*60)+"deg)"; p.style.opacity=elev>0?".9":".4"; });
  viz.querySelector(".sky").style.background = elev>0
    ? "linear-gradient(180deg, #1a3a5c 0%, #2d5a87 30%, #5b8cb8 50%, #87b5d8 70%, #c4dce8 100%)"
    : "linear-gradient(180deg, #060b14 0%, #0d1525 50%, #192840 100%)";
}

/* ---- Detail panel ---- */
function renderDetail() {
  const sel = document.getElementById("detail-select"), bid = sel?.value;
  const c = document.getElementById("detail-content");
  if (!bid||!allBalls[bid]) { c.innerHTML = `<div class="empty-state"><p>${t("no_ball_selected")}</p></div>`; return; }
  const d = allBalls[bid];
  c.innerHTML = `<div class="grid">
    <div class="card"><div class="card-title"><span class="dot ok"></span> ${t("dir_vector")}</div>
      <div class="stat-row"><span class="stat-label">${t("x_east")}</span><span class="stat-value">${(d.dx||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">${t("y_north")}</span><span class="stat-value">${(d.dy||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">${t("z_up")}</span><span class="stat-value">${(d.dz||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">${t("magnitude")}</span><span class="stat-value">${(d.mag||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">${t("valid")}</span>
        <span class="stat-value"><span class="badge ${d.vector_ok?'badge-ok':'badge-err'}">${d.vector_ok?t("dir_valid"):t("dir_invalid")}</span></span></div></div>
    <div class="card"><div class="card-title"><span class="dot ok"></span> ${t("panel_orient")}</div>
      <div class="stat-row"><span class="stat-label">${t("panel_az")}</span><span class="stat-value">${(d.panel_az||0).toFixed(1)}°</span></div>
      <div class="stat-row"><span class="stat-label">${t("panel_tilt")}</span><span class="stat-value">${(d.panel_tilt||0).toFixed(1)}°</span></div>
      <div class="stat-row"><span class="stat-label">${t("timestamp")}</span><span class="stat-value">${d.ts?new Date(d.ts*1000).toISOString():'N/A'}</span></div>
      <div class="stat-row"><span class="stat-label">${t("error_mask")}</span><span class="stat-value" style="font-family:monospace">0x${(d.err||0).toString(16).padStart(4,'0')}</span></div>
      <div class="stat-row"><span class="stat-label">${t("flags")}</span><span class="stat-value" style="font-family:monospace">0x${(d.flags||0).toString(16).padStart(2,'0')}</span></div></div>
    <div class="card"><div class="card-title"><span class="dot ${(d.fault_list||[]).length?'warn':'ok'}"></span> ${t("faults_flags")}</div>
      ${(d.fault_list||[]).length===0
        ?`<div class="stat-row"><span class="stat-value" style="color:var(--green)">${t("no_faults")}</span></div>`
        :d.fault_list.map(f=>{const def=Object.values(FAULTS).find(v=>v[0]===f);return`<div class="stat-row"><span class="stat-value"><span class="badge ${badgeClass(def?def[1]:'warn')}">${f}</span></span></div>`;}).join("")}
    </div></div>`;
}

function populateSelects() {
  const ids = Object.keys(allBalls);
  ["detail-select","history-ball","ota-ball"].forEach(sid=>{
    const sel = document.getElementById(sid); if (!sel) return;
    const cur = sel.value;
    sel.innerHTML = `<option value="">${t("select_ball")}</option>`+
      ids.map(id=>`<option value="${id}" ${id===cur?'selected':''}>${id}</option>`).join("");
  });
}

function populateChartSelect() {
  const ids = Object.keys(allBalls), sel = document.getElementById("chart-ball"); if (!sel) return;
  const cur = sel.value;
  sel.innerHTML = `<option value="">${t("select")}</option>`+
    ids.map(id=>`<option value="${id}" ${id===cur?'selected':''}>${id}</option>`).join("");
}

/* ---- Charts ---- */
function destroyCharts() { Object.values(chartInstances).forEach(c=>c.destroy()); chartInstances={}; }
document.getElementById("chart-load")?.addEventListener("click", async () => {
  const ball = document.getElementById("chart-ball").value, limit = document.getElementById("chart-limit").value;
  if (!ball) return; destroyCharts();
  try {
    const resp = await fetch("/api/history/"+ball+"?limit="+limit), rows = await resp.json();
    if (!rows.length) { destroyCharts(); return; }
    rows.reverse();
    const labels = rows.map(r=>r.ts?new Date(r.ts*1000).toLocaleTimeString():""), nc={fg:"#e8e8f0",muted:"#8899aa"};
    [ ["chart-trajectory","traj",[{label:"Azimuth",data:rows.map(r=>r.azimuth),borderColor:"#f59e0b",tension:.3,pointRadius:0},{label:"Elevation",data:rows.map(r=>r.elevation),borderColor:"#3b82f6",tension:.3,pointRadius:0}]],
      ["chart-telemetry","telem",[{label:"SOC %",data:rows.map(r=>r.soc),borderColor:"#10b981",tension:.3,pointRadius:0,yAxisID:"y"},{label:"RSSI",data:rows.map(r=>r.rssi),borderColor:"#f59e0b",tension:.3,pointRadius:0,yAxisID:"y1"}]],
    ].forEach(([cid,key,ds])=>{
      const ctx = document.getElementById(cid).getContext("2d");
      chartInstances[key] = new Chart(ctx, { type:"line", data:{labels,datasets:ds},
        options:{responsive:true,maintainAspectRatio:false,
          scales:{x:{ticks:{color:nc.muted,maxTicksLimit:8}},y:key==="telem"?{position:"left",min:0,max:100,ticks:{color:nc.muted},title:{display:true,text:"SOC %",color:"#10b981"}}:{ticks:{color:nc.muted}},
            y1:key==="telem"?{position:"right",grid:{drawOnChartArea:false},ticks:{color:nc.muted},title:{display:true,text:"dBm",color:"#f59e0b"}}:undefined},
          plugins:{legend:{labels:{color:nc.fg}}}}});
    });
    const ctxc = document.getElementById("chart-confidence").getContext("2d");
    chartInstances.conf = new Chart(ctxc, { type:"line", data:{labels,datasets:[{label:"Confidence",data:rows.map(r=>r.confidence),borderColor:"#3b82f6",tension:.3,pointRadius:0,fill:true,backgroundColor:"rgba(59,130,246,.08)"}]},
      options:{responsive:true,maintainAspectRatio:false,scales:{x:{ticks:{color:nc.muted,maxTicksLimit:8}},y:{min:0,max:255,ticks:{color:nc.muted}}},plugins:{legend:{labels:{color:nc.fg}}}},
    });
    const fn=["SENSOR_OPEN","SENSOR_SHORT","SATURATED","ADC_ERR","MODEM_ERR","MQTT_ERR","LOW_BAT","NO_CALIB","OVERCAST","NIGHT"];
    const c10=["#ef4444","#f97316","#f59e0b","#ef4444","#ef4444","#ef4444","#ef4444","#f59e0b","#3b82f6","#3b82f6"];
    const ctxf = document.getElementById("chart-faults").getContext("2d");
    chartInstances.faults = new Chart(ctxf, { type:"bar", data:{labels,datasets:fn.map((n,i)=>({label:n,data:rows.map(r=>(r.error_mask&(1<<i))?1:0),backgroundColor:c10[i],borderWidth:0}))},
      options:{responsive:true,maintainAspectRatio:false,scales:{x:{stacked:true,ticks:{color:nc.muted,maxTicksLimit:8}},y:{stacked:true,max:10,ticks:{color:nc.muted,stepSize:1}}},plugins:{legend:{labels:{color:nc.fg,font:{size:9}}}}},
    });
  } catch(e) { console.error(e); }
});

/* ---- OTA ---- */
document.getElementById("ota-send")?.addEventListener("click", async () => {
  const ball = document.getElementById("ota-ball").value, url = document.getElementById("ota-url").value,
        ver = document.getElementById("ota-version").value, result = document.getElementById("ota-result");
  if (!url) { result.innerHTML = `<span style="color:var(--red)">${t("url_required")}</span>`; return; }
  result.innerHTML = t("ota_sending");
  try {
    const resp = await fetch("/api/ota",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({ball_id:ball,url,version:ver})});
    const data = await resp.json();
    result.innerHTML = data.ok
      ? `<span style="color:var(--green)">${t("ota_ok",{ball:esc(ball)})}</span>`
      : `<span style="color:var(--red)">${t("ota_fail",{msg:esc(JSON.stringify(data))})}</span>`;
  } catch (e) { result.innerHTML = `<span style="color:var(--red)">${t("ota_fail",{msg:esc(String(e))})}</span>`; }
});

/* ---- Config Wizard ---- */
document.getElementById("cfg-generate")?.addEventListener("click", () => {
  const g=id=>document.getElementById(id).value;
  const h=`#ifndef CONFIG_H\n#define CONFIG_H\n\n/* ---------- Ball Identity ---------- */
#define BALL_ID                 "${g("cfg-ball-id")}"\n#define FIRMWARE_VERSION        "${g("cfg-version")}"
/* ---------- Sensor Sampling ---------- */
#define SENSOR_COUNT            80\n#define ADC_I2C_CLOCK_HZ        400000\n#define MUX_SETTLE_MS           2\n#define SENSOR_OVERSAMPLE       8\n#define SENSOR_SCAN_INTERVAL_S  ${g("cfg-scan-interval")}
/* ---------- GPIO Pin Mapping ---------- */
#define MUX_S0_GPIO  32\n#define MUX_S1_GPIO  33\n#define MUX_S2_GPIO  25\n#define MUX_S3_GPIO  26\n#define MUX_EN0_GPIO 27\n#define MUX_EN1_GPIO 14\n#define MUX_EN2_GPIO 12\n#define MUX_EN3_GPIO 13\n#define MUX_EN4_GPIO 15\n#define MUX_EN_PINS  { MUX_EN0_GPIO, MUX_EN1_GPIO, MUX_EN2_GPIO, MUX_EN3_GPIO, MUX_EN4_GPIO }\n#define MUX_BANK_COUNT 5\n#define MUX_CHANNELS_PER_BANK 16\n#define I2C1_SDA_GPIO 21\n#define I2C1_SCL_GPIO 22\n#define I2C2_SDA_GPIO 18\n#define I2C2_SCL_GPIO 19\n#define ADS1115_ADDR_1 0x48\n#define ADS1115_ADDR_2 0x49\n#define DISPLAY_MOSI_GPIO 2\n#define DISPLAY_SCLK_GPIO 4\n#define DISPLAY_CS_GPIO 5
/* ---------- 4G Module ---------- */
#define MODEM_UART_TX_GPIO  17\n#define MODEM_UART_RX_GPIO  16\n#define MODEM_PWR_KEY_GPIO  23\n#define MODEM_BAUDRATE       115200\n#define MQTT_BROKER_HOST     "${g("cfg-broker")}"\n#define MQTT_BROKER_PORT     ${g("cfg-broker-port")}\n#define MQTT_CLIENT_ID       BALL_ID\n#define MQTT_TOPIC_PREFIX    "/solar/ball/"\n#define MQTT_QOS             1\n#define MQTT_KEEPALIVE_S     60\n#define MODEM_APN            "${g("cfg-apn")}"\n#define MODEM_APN_USER       "${g("cfg-apn-user")}"\n#define MODEM_APN_PASS       "${g("cfg-apn-pass")}"
/* ---------- Power ---------- */
#define BATTERY_ADC_GPIO      36\n#define BATTERY_DIVIDER_RATIO 2.0f\n#define BATTERY_FULL_MV       ${g("cfg-bat-full")}\n#define BATTERY_EMPTY_MV      ${g("cfg-bat-empty")}\n#define DEEP_SLEEP_WAKEUP_S   SENSOR_SCAN_INTERVAL_S\n#define CALIB_NVS_NAMESPACE   "solar-ball"\n#define CALIB_BASELINE_KEY    "baseline"\n#define CALIB_MAPPING_KEY     "ch_map"\n#define CALIB_SAMPLE_COUNT    100\n#define OTA_HTTP_MAX_RETRIES  3\n#define OTA_FW_URL_PREFIX     "http://ota.solar-ball.example.com/firmware/"\n#define NVS_PARTITION         "nvs"\n\n#endif`;
  document.getElementById("cfg-output").innerHTML =
    `<pre id="cfg-code" style="background:rgba(0,0,0,.3);border:1px solid var(--card-border);border-radius:8px;padding:12px;overflow:auto;max-height:400px;font-size:.75em;white-space:pre-wrap">${esc(h)}</pre>
     <button class="btn btn-primary" style="margin-top:8px" onclick="navigator.clipboard.writeText(document.getElementById('cfg-code').textContent)">${t("copy_btn")}</button>`;
});
