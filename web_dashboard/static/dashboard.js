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
  document.getElementById("conn-text").textContent = "Live";
};
evtSource.onerror = () => {
  document.getElementById("conn-status").className = "status-dot offline";
  document.getElementById("conn-text").textContent = "Offline";
};
evtSource.onmessage = (event) => {
  try {
    const msg = JSON.parse(event.data);
    if (msg.ping) return;
    allBalls[msg.ball_id] = msg.data;
    document.getElementById("ball-count").textContent = Object.keys(allBalls).length;
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

/* ---- Detail select onchange ---- */
document.getElementById("detail-select")?.addEventListener("change", () => renderDetail());

/* ---- Rendering ---- */
function renderAll() {
  renderOverview();
  renderDetail();
  populateSelects();
  populateChartSelect();
}

function badgeClass(kind) {
  if (kind === "ok") return "badge-ok";
  if (kind === "warn") return "badge-warn";
  if (kind === "err") return "badge-err";
  return "badge-info";
}

function esc(s) { return String(s||"").replace(/&/g,"&amp;").replace(/</g,"&lt;").replace(/>/g,"&gt;").replace(/"/g,"&quot;"); }

/* ---- Overview Cards ---- */
function renderOverview() {
  const container = document.getElementById("balls-container");
  const ids = Object.keys(allBalls);
  if (ids.length === 0) {
    container.innerHTML = `<div class="empty-state"><div class="icon">&#9788;</div>
      <p>Waiting for Solar Ball data...</p></div>`;
    return;
  }

  let html = "";
  for (const bid of ids) {
    const d = allBalls[bid]; if (!d) continue;
    const az = d.azimuth || 0, elev = d.elevation || 0;
    const faults = d.fault_list || [], flags = d.flag_list || [];
    const soc = d.soc || 0, rssi = d.rssi || 0, conf = d.conf || 0;
    const age = d.ts ? Math.max(0, Math.floor(Date.now()/1000 - d.ts)) : 999;
    const statusColor = d.vector_ok ? "ok" : (faults.length ? "warn" : "warn");

    html += `<div class="card">
      <div class="card-title">
        <span class="dot ${statusColor}"></span>
        ${esc(bid)}
        <span style="font-weight:400;font-size:.75em;color:var(--muted);margin-left:auto">${age}s ago</span>
      </div>
      <div class="sun-viz" id="sunviz-${bid}">
        <div class="sky"></div>
        <div class="ground"></div>
        <div class="panel-group">${Array(5).fill(0).map(() => '<div class="panel-unit" style="transform:rotateX(0deg)"></div>').join("")}</div>
        <div class="sun-dot" style="left:50%;top:50%"></div>
      </div>
      <div class="stat-row"><span class="stat-label">Azimuth</span><span class="stat-value">${az.toFixed(0)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Elevation</span><span class="stat-value">${elev.toFixed(0)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Battery</span><span class="stat-value">${soc}%</span></div>
      <div class="bar bar-soc"><div class="bar-fill" style="width:${soc}%"></div></div>
      <div class="stat-row"><span class="stat-label">Signal</span><span class="stat-value">${rssi} dBm</span></div>
      <div class="stat-row"><span class="stat-label">Confidence</span><span class="stat-value">${conf}/255</span></div>
      <div class="bar bar-conf"><div class="bar-fill" style="width:${conf/255*100}%"></div></div>
      <div style="margin-top:8px">
        ${flags.map(f => `<span class="badge badge-ok">${f}</span>`).join("")}
        ${faults.map(f => {
          const def = Object.values(FAULTS).find(v => v[0]===f);
          return `<span class="badge ${badgeClass(def?def[1]:'warn')}">${f}</span>`;
        }).join("")}
      </div>
    </div>`;

    setTimeout(() => drawSunViz(bid, az, elev), 40);
  }
  container.innerHTML = html;
}

function drawSunViz(bid, az, elev) {
  const viz = document.getElementById("sunviz-" + bid);
  if (!viz) return;
  const dot = viz.querySelector(".sun-dot");
  const panels = viz.querySelectorAll(".panel-unit");
  if (!dot) return;

  const rect = viz.getBoundingClientRect();
  const w = rect.width, h = rect.height;

  const gtY = h * 0.75;
  const skyH = h - gtY;
  const maxR = Math.min(w, skyH * 2) * 0.42;
  const cx = w / 2, cy = gtY;

  const elNorm = Math.max(-10, Math.min(90, elev)) / 90;
  const radius = elNorm * maxR + 2;
  const azRad = (az - 90) * Math.PI / 180;
  const sx = cx + radius * Math.cos(azRad);
  const sy = cy - radius * Math.sin(azRad) * (skyH / (maxR + 10));

  dot.style.left = sx + "px";
  dot.style.top = sy + "px";
  dot.style.opacity = elev > 0 ? "1" : ".3";
  dot.style.width = (18 + elNorm * 14) + "px";
  dot.style.height = (18 + elNorm * 14) + "px";

  const panelTilt = (90 - elev) / 90;
  panels.forEach(p => {
    p.style.transform = "rotateX(" + (panelTilt * 60) + "deg)";
    p.style.opacity = elev > 0 ? ".9" : ".4";
  });

  viz.querySelector(".sky").style.background = elev > 0
    ? "linear-gradient(180deg, #1a3a5c 0%, #2d5a87 30%, #5b8cb8 50%, #87b5d8 70%, #c4dce8 100%)"
    : "linear-gradient(180deg, #060b14 0%, #0d1525 50%, #192840 100%)";
}

/* ---- Detail Panel ---- */
function renderDetail() {
  const sel = document.getElementById("detail-select");
  const bid = sel?.value;
  const container = document.getElementById("detail-content");
  if (!bid || !allBalls[bid]) {
    container.innerHTML = `<div class="empty-state"><p>Select a ball</p></div>`;
    return;
  }
  const d = allBalls[bid];
  container.innerHTML = `<div class="grid">
    <div class="card">
      <div class="card-title"><span class="dot ok"></span> Direction Vector</div>
      <div class="stat-row"><span class="stat-label">X (east)</span><span class="stat-value">${(d.dx||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Y (north)</span><span class="stat-value">${(d.dy||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Z (up)</span><span class="stat-value">${(d.dz||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Magnitude</span><span class="stat-value">${(d.mag||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Valid</span>
        <span class="stat-value"><span class="badge ${d.vector_ok?'badge-ok':'badge-err'}">${d.vector_ok?'YES':'NO'}</span></span></div>
    </div>
    <div class="card">
      <div class="card-title"><span class="dot ok"></span> Panel Orientation</div>
      <div class="stat-row"><span class="stat-label">Panel Azimuth</span><span class="stat-value">${(d.panel_az||0).toFixed(1)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Panel Tilt</span><span class="stat-value">${(d.panel_tilt||0).toFixed(1)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Timestamp</span><span class="stat-value">${d.ts?new Date(d.ts*1000).toISOString():'N/A'}</span></div>
      <div class="stat-row"><span class="stat-label">Error Mask</span><span class="stat-value" style="font-family:monospace">0x${(d.err||0).toString(16).padStart(4,'0')}</span></div>
      <div class="stat-row"><span class="stat-label">Flags</span><span class="stat-value" style="font-family:monospace">0x${(d.flags||0).toString(16).padStart(2,'0')}</span></div>
    </div>
    <div class="card">
      <div class="card-title"><span class="dot ${(d.fault_list||[]).length?'warn':'ok'}"></span> Faults &amp; Flags</div>
      ${(d.fault_list||[]).length === 0
        ? '<div class="stat-row"><span class="stat-value" style="color:var(--green)">No faults</span></div>'
        : d.fault_list.map(f => {
            const def = Object.values(FAULTS).find(v => v[0]===f);
            return `<div class="stat-row"><span class="stat-value"><span class="badge ${badgeClass(def?def[1]:'warn')}">${f}</span></span></div>`;
          }).join("")}
    </div>
  </div>`;
}

/* ---- Populate selects ---- */
function populateSelects() {
  const ids = Object.keys(allBalls);
  ["detail-select", "history-ball", "ota-ball"].forEach(sid => {
    const sel = document.getElementById(sid);
    if (!sel) return;
    const cur = sel.value;
    sel.innerHTML = '<option value="">-- Select --</option>' +
      ids.map(id => `<option value="${id}" ${id===cur?'selected':''}>${id}</option>`).join("");
  });
}

function populateChartSelect() {
  const ids = Object.keys(allBalls);
  const sel = document.getElementById("chart-ball");
  if (!sel) return;
  const cur = sel.value;
  sel.innerHTML = '<option value="">-- Select --</option>' +
    ids.map(id => `<option value="${id}" ${id===cur?'selected':''}>${id}</option>`).join("");
}

/* ---- Charts ---- */
function destroyCharts() {
  Object.values(chartInstances).forEach(c => c.destroy());
  chartInstances = {};
}

document.getElementById("chart-load")?.addEventListener("click", async () => {
  const ball = document.getElementById("chart-ball").value;
  const limit = document.getElementById("chart-limit").value;
  if (!ball) return;
  destroyCharts();
  try {
    const resp = await fetch("/api/history/" + ball + "?limit=" + limit);
    const rows = await resp.json();
    if (!rows.length) { destroyCharts(); return; }
    rows.reverse();

    const labels = rows.map(r => r.ts ? new Date(r.ts*1000).toLocaleTimeString() : "");
    const colors = { fg: "#e8e8f0", muted: "#8899aa" };

    const ctx1 = document.getElementById("chart-trajectory").getContext("2d");
    chartInstances.traj = new Chart(ctx1, {
      type: "line", data: { labels, datasets: [
        { label:"Azimuth", data:rows.map(r=>r.azimuth), borderColor:"#f59e0b", tension:.3, pointRadius:0 },
        { label:"Elevation", data:rows.map(r=>r.elevation), borderColor:"#3b82f6", tension:.3, pointRadius:0 },
      ]},
      options: { responsive:true, maintainAspectRatio:false,
        scales: { x: { ticks: { color: colors.muted, maxTicksLimit:8 } }, y: { ticks: { color: colors.muted } } },
        plugins: { legend: { labels: { color: colors.fg } } } }
    });

    const ctx2 = document.getElementById("chart-telemetry").getContext("2d");
    chartInstances.telem = new Chart(ctx2, {
      type: "line", data: { labels, datasets: [
        { label:"SOC %", data:rows.map(r=>r.soc), borderColor:"#10b981", tension:.3, pointRadius:0, yAxisID:"y" },
        { label:"RSSI", data:rows.map(r=>r.rssi), borderColor:"#f59e0b", tension:.3, pointRadius:0, yAxisID:"y1" },
      ]},
      options: { responsive:true, maintainAspectRatio:false,
        scales: {
          x: { ticks: { color: colors.muted, maxTicksLimit:8 } },
          y: { position:"left", min:0, max:100, ticks: { color: colors.muted }, title: { display:true, text:"SOC %", color:"#10b981" } },
          y1: { position:"right", grid: { drawOnChartArea:false }, ticks: { color: colors.muted }, title: { display:true, text:"dBm", color:"#f59e0b" } },
        },
        plugins: { legend: { labels: { color: colors.fg } } } }
    });

    const ctx3 = document.getElementById("chart-confidence").getContext("2d");
    chartInstances.conf = new Chart(ctx3, {
      type: "line", data: { labels, datasets: [
        { label:"Confidence", data:rows.map(r=>r.confidence), borderColor:"#3b82f6", tension:.3, pointRadius:0, fill:true, backgroundColor:"rgba(59,130,246,.08)" },
      ]},
      options: { responsive:true, maintainAspectRatio:false,
        scales: { x: { ticks: { color: colors.muted, maxTicksLimit:8 } }, y: { min:0, max:255, ticks: { color: colors.muted } } },
        plugins: { legend: { labels: { color: colors.fg } } } }
    });

    const faultNames = ["SENSOR_OPEN","SENSOR_SHORT","SATURATED","ADC_ERR","MODEM_ERR","MQTT_ERR","LOW_BAT","NO_CALIB","OVERCAST","NIGHT"];
    const colors10 = ["#ef4444","#f97316","#f59e0b","#ef4444","#ef4444","#ef4444","#ef4444","#f59e0b","#3b82f6","#3b82f6"];
    const ctx4 = document.getElementById("chart-faults").getContext("2d");
    chartInstances.faults = new Chart(ctx4, {
      type: "bar", data: { labels, datasets: faultNames.map((n,i)=>({ label:n, data:rows.map(r=>(r.error_mask&(1<<i))?1:0), backgroundColor:colors10[i], borderWidth:0 })) },
      options: { responsive:true, maintainAspectRatio:false,
        scales: { x: { stacked:true, ticks: { color: colors.muted, maxTicksLimit:8 } }, y: { stacked:true, max:10, ticks: { color: colors.muted, stepSize:1 } } },
        plugins: { legend: { labels: { color: colors.fg, font:{size:9} } } } }
    });
  } catch (e) { console.error(e); }
});

/* ---- OTA ---- */
document.getElementById("ota-send")?.addEventListener("click", async () => {
  const ball = document.getElementById("ota-ball").value;
  const url = document.getElementById("ota-url").value;
  const version = document.getElementById("ota-version").value;
  const result = document.getElementById("ota-result");
  if (!url) { result.innerHTML = '<span style="color:var(--red)">URL is required</span>'; return; }
  result.innerHTML = "Sending...";
  try {
    const resp = await fetch("/api/ota", { method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify({ball_id:ball,url,version}) });
    const data = await resp.json();
    result.innerHTML = data.ok
      ? `<span style="color:var(--green)">Sent to ${esc(ball)}. Device will update on next cycle.</span>`
      : `<span style="color:var(--red)">Failed: ${esc(JSON.stringify(data))}</span>`;
  } catch (e) { result.innerHTML = `<span style="color:var(--red)">Error: ${esc(String(e))}</span>`; }
});

/* ---- Config Wizard ---- */
document.getElementById("cfg-generate")?.addEventListener("click", () => {
  const g = id => document.getElementById(id).value;
  const h = `#ifndef CONFIG_H\n#define CONFIG_H\n\n/* ---------- Ball Identity ---------- */
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
     <button class="btn btn-primary" style="margin-top:8px" onclick="navigator.clipboard.writeText(document.getElementById('cfg-code').textContent)">Copy to Clipboard</button>`;
});
