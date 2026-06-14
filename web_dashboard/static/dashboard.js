const FAULTS = {
  0x0001: ["SENSOR_OPEN", "warn"],   0x0002: ["SENSOR_SHORT", "warn"],
  0x0004: ["SATURATED", "info"],     0x0008: ["ADC_ERR", "err"],
  0x0010: ["MODEM_ERR", "err"],      0x0020: ["MQTT_ERR", "err"],
  0x0040: ["LOW_BAT", "err"],        0x0080: ["NO_CALIB", "warn"],
  0x0100: ["OVERCAST", "info"],      0x0200: ["NIGHT", "info"],
};

let allBalls = {};

/* ---- SSE ---- */
const evtSource = new EventSource("/stream");

evtSource.onopen = () => {
  allBalls = {};
  document.getElementById("conn-status").className = "status-dot online";
  document.getElementById("conn-text").textContent = "Live";
};

evtSource.onerror = () => {
  document.getElementById("conn-status").className = "status-dot offline";
  document.getElementById("conn-text").textContent = "Disconnected";
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

/* ---- Tab switching ---- */
document.querySelectorAll(".tab").forEach(t => {
  t.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach(tt => tt.classList.remove("active"));
    t.classList.add("active");
    document.querySelectorAll(".panel").forEach(p => p.classList.add("hidden"));
    document.getElementById(`panel-${t.dataset.tab}`).classList.remove("hidden");
    if (t.dataset.tab === "detail") populateDetailSelect();
    if (t.dataset.tab === "history") populateHistorySelect();
    if (t.dataset.tab === "charts") populateChartSelect();
    if (t.dataset.tab === "config") populateSelects();
  });
});

/* ---- Rendering ---- */
function renderAll() {
  renderOverview();
  renderDetail();
  populateSelects();
  populateChartSelect();
}

let renderThrottle = null;
function scheduleRender() {
  if (renderThrottle) return;
  renderThrottle = setTimeout(() => { renderThrottle = null; renderAll(); }, 250);
}

let chartInstances = {};

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
    const resp = await fetch(`/api/history/${ball}?limit=${limit}`);
    const rows = await resp.json();
    if (!rows.length) {
      destroyCharts();
      return;
    }
    rows.reverse();

    const labels = rows.map(r => r.ts ? new Date(r.ts*1000).toLocaleTimeString() : "");

    const ctx1 = document.getElementById("chart-trajectory").getContext("2d");
    chartInstances.traj = new Chart(ctx1, {
      type: "line",
      data: {
        labels,
        datasets: [
          { label: "Azimuth (deg)", data: rows.map(r => r.azimuth), borderColor: "#38bdf8", tension: .3, pointRadius: 0 },
          { label: "Elevation (deg)", data: rows.map(r => r.elevation), borderColor: "#22c55e", tension: .3, pointRadius: 0 },
        ]
      },
      options: { responsive: true, maintainAspectRatio: false,
        scales: { x: { ticks: { color: "#94a3b8", maxTicksLimit: 10 } },
                  y: { ticks: { color: "#94a3b8" } } },
        plugins: { legend: { labels: { color: "#e2e8f0" } } }
      }
    });

    const ctx2 = document.getElementById("chart-telemetry").getContext("2d");
    chartInstances.telem = new Chart(ctx2, {
      type: "line",
      data: {
        labels,
        datasets: [
          { label: "SOC (%)", data: rows.map(r => r.soc), borderColor: "#22c55e", tension: .3, pointRadius: 0, yAxisID: "y" },
          { label: "RSSI (dBm)", data: rows.map(r => r.rssi), borderColor: "#eab308", tension: .3, pointRadius: 0, yAxisID: "y1" },
        ]
      },
      options: { responsive: true, maintainAspectRatio: false,
        scales: {
          x: { ticks: { color: "#94a3b8", maxTicksLimit: 10 } },
          y: { type: "linear", position: "left", min: 0, max: 100, ticks: { color: "#94a3b8" }, title: { display: true, text: "SOC %", color: "#22c55e" } },
          y1: { type: "linear", position: "right", grid: { drawOnChartArea: false }, ticks: { color: "#94a3b8" }, title: { display: true, text: "RSSI dBm", color: "#eab308" } },
        },
        plugins: { legend: { labels: { color: "#e2e8f0" } } }
      }
    });

    const ctx3 = document.getElementById("chart-confidence").getContext("2d");
    chartInstances.conf = new Chart(ctx3, {
      type: "line",
      data: {
        labels,
        datasets: [
          { label: "Confidence (0-255)", data: rows.map(r => r.confidence), borderColor: "#38bdf8", tension: .3, pointRadius: 0, fill: true, backgroundColor: "rgba(56,189,248,.1)" },
        ]
      },
      options: { responsive: true, maintainAspectRatio: false,
        scales: {
          x: { ticks: { color: "#94a3b8", maxTicksLimit: 10 } },
          y: { min: 0, max: 255, ticks: { color: "#94a3b8" } },
        },
        plugins: {
          legend: { labels: { color: "#e2e8f0" } },
        }
      }
    });

    const ctx4 = document.getElementById("chart-faults").getContext("2d");
    const faultNames = ["SENSOR_OPEN","SENSOR_SHORT","SATURATED","ADC_ERR","MODEM_ERR","MQTT_ERR","LOW_BAT","NO_CALIB","OVERCAST","NIGHT"];
    const datasets = faultNames.map((name, i) => ({
      label: name,
      data: rows.map(r => (r.error_mask & (1<<i)) ? 1 : 0),
      backgroundColor: ["#ef4444","#f97316","#eab308","#ef4444","#ef4444","#ef4444","#ef4444","#eab308","#38bdf8","#38bdf8"][i],
      borderWidth: 0,
    }));
    chartInstances.faults = new Chart(ctx4, {
      type: "bar",
      data: { labels, datasets },
      options: {
        responsive: true, maintainAspectRatio: false,
        scales: { x: { stacked: true, ticks: { color: "#94a3b8", maxTicksLimit: 10 } },
                  y: { stacked: true, max: faultNames.length, ticks: { color: "#94a3b8", stepSize: 1 } } },
        plugins: { legend: { labels: { color: "#e2e8f0", font: { size: 9 } } } }
      }
    });

  } catch (e) { console.error(e); }
});

/* ---- Config Wizard ---- */
document.getElementById("cfg-generate")?.addEventListener("click", () => {
  const get = id => document.getElementById(id).value;
  const h = `#ifndef CONFIG_H
#define CONFIG_H

/* ---------- Ball Identity ---------- */
#define BALL_ID                 "${get("cfg-ball-id")}"
#define FIRMWARE_VERSION        "${get("cfg-version")}"

/* ---------- Sensor Sampling ---------- */
#define SENSOR_COUNT            80
#define ADC_I2C_CLOCK_HZ        400000
#define MUX_SETTLE_MS           2
#define SENSOR_OVERSAMPLE       8
#define SENSOR_SCAN_INTERVAL_S  ${get("cfg-scan-interval")}

/* ---------- GPIO Pin Mapping ---------- */
#define MUX_S0_GPIO             32
#define MUX_S1_GPIO             33
#define MUX_S2_GPIO             25
#define MUX_S3_GPIO             26
#define MUX_EN0_GPIO            27
#define MUX_EN1_GPIO            14
#define MUX_EN2_GPIO            12
#define MUX_EN3_GPIO            13
#define MUX_EN4_GPIO            15
#define MUX_EN_PINS             { MUX_EN0_GPIO, MUX_EN1_GPIO, MUX_EN2_GPIO, MUX_EN3_GPIO, MUX_EN4_GPIO }
#define MUX_BANK_COUNT          5
#define MUX_CHANNELS_PER_BANK   16
#define I2C1_SDA_GPIO           21
#define I2C1_SCL_GPIO           22
#define I2C2_SDA_GPIO           18
#define I2C2_SCL_GPIO           19
#define ADS1115_ADDR_1          0x48
#define ADS1115_ADDR_2          0x49
#define DISPLAY_MOSI_GPIO       2
#define DISPLAY_SCLK_GPIO       4
#define DISPLAY_CS_GPIO         5

/* ---------- 4G Module (SIM7600G) ---------- */
#define MODEM_UART_TX_GPIO      17
#define MODEM_UART_RX_GPIO      16
#define MODEM_PWR_KEY_GPIO      23
#define MODEM_RESET_GPIO        2
#define MODEM_BAUDRATE          115200
#define MQTT_BROKER_HOST        "${get("cfg-broker")}"
#define MQTT_BROKER_PORT        ${get("cfg-broker-port")}
#define MQTT_CLIENT_ID          BALL_ID
#define MQTT_TOPIC_PREFIX       "/solar/ball/"
#define MQTT_QOS                1
#define MQTT_KEEPALIVE_S        60
#define MODEM_APN               "${get("cfg-apn")}"
#define MODEM_APN_USER          "${get("cfg-apn-user")}"
#define MODEM_APN_PASS          "${get("cfg-apn-pass")}"

/* ---------- Power Management ---------- */
#define BATTERY_ADC_GPIO        36
#define BATTERY_DIVIDER_RATIO   2.0f
#define BATTERY_FULL_MV         ${get("cfg-bat-full")}
#define BATTERY_EMPTY_MV        ${get("cfg-bat-empty")}
#define DEEP_SLEEP_WAKEUP_S     SENSOR_SCAN_INTERVAL_S

/* ---------- Calibration ---------- */
#define CALIB_NVS_NAMESPACE     "solar-ball"
#define CALIB_BASELINE_KEY      "baseline"
#define CALIB_MAPPING_KEY       "ch_map"
#define CALIB_SAMPLE_COUNT      100

/* ---------- OTA Firmware Update ---------- */
#define OTA_HTTP_MAX_RETRIES         3
#define OTA_HTTP_RECV_TIMEOUT_MS     120000
#define OTA_HTTP_CHUNK_TIMEOUT_MS    30000
#define OTA_FW_URL_PREFIX            "http://ota.solar-ball.example.com/firmware/"
#define OTA_MQTT_CMD_TOPIC           "/solar/ball/%s/cmd"

/* ---------- NVS ---------- */
#define NVS_PARTITION           "nvs"

#endif /* CONFIG_H */`;

  const out = document.getElementById("cfg-output");
  out.innerHTML = `<pre id="cfg-code" style="background:var(--bg);border:1px solid var(--border);
    border-radius:6px;padding:12px;overflow:auto;max-height:500px;font-size:.8em;
    white-space:pre-wrap">${esc(h)}</pre>
    <button class="btn btn-primary" style="margin-top:8px" onclick="navigator.clipboard.writeText(document.getElementById('cfg-code').textContent)">Copy to Clipboard</button>`;
});

function badgeClass(kind) {
  if (kind === "ok") return "badge-ok";
  if (kind === "warn" || kind === "warning") return "badge-warn";
  if (kind === "err" || kind === "danger") return "badge-err";
  return "badge-info";
}

function renderOverview() {
  const container = document.getElementById("balls-container");
  const ids = Object.keys(allBalls);
  if (ids.length === 0) {
    container.innerHTML = `<div class="empty-state"><div class="icon">&#9728;</div>
      <p>Waiting for ball data...</p></div>`;
    return;
  }

  let html = "";
  for (const bid of ids) {
    const d = allBalls[bid];
    if (!d) continue;
    const az = d.azimuth || 0, elev = d.elevation || 0;
    const faults = d.fault_list || [];
    const flags = d.flag_list || [];
    const age = d.ts ? Math.max(0, Math.floor(Date.now() / 1000 - d.ts)) : 999;

    html += `<div class="card">
      <h2>${esc(bid)} <span style="font-weight:400;font-size:.8em;color:var(--muted)">
        age: ${age}s</span></h2>
      <div class="dir-compass" style="margin:0 auto 12px">
        <div class="compass-label n">N</div>
        <div class="compass-label e">E</div>
        <div class="compass-label s">S</div>
        <div class="compass-label w">W</div>
        <canvas id="compass-${bid}" width="160" height="160"></canvas>
        <div class="dot" id="sun-dot-${bid}"
             style="left:${80 + 60*Math.cos((az-90)*Math.PI/180)}px;
                    top:${80 - 60*Math.sin(elev*Math.PI/180)*Math.cos((az-90)*Math.PI/180)}px">
        </div>
      </div>
      <div class="stat-row"><span class="stat-label">Azimuth</span>
        <span class="stat-value">${az.toFixed(1)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Elevation</span>
        <span class="stat-value">${elev.toFixed(1)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Battery</span>
        <span class="stat-value">${d.soc || 0}%</span></div>
      <div class="bar soc-bar" style="margin:4px 0 8px">
        <div class="bar-fill" style="width:${d.soc || 0}%"></div></div>
      <div class="stat-row"><span class="stat-label">Signal (RSSI)</span>
        <span class="stat-value">${d.rssi || 0} dBm</span></div>
      <div class="stat-row"><span class="stat-label">Confidence</span>
        <span class="stat-value">${d.conf || 0}/255</span></div>
      <div class="bar conf-bar" style="margin:4px 0 8px">
        <div class="bar-fill" style="width:${(d.conf||0)/255*100}%"></div></div>
      <div style="margin-top:6px">
        ${flags.map(f => `<span class="badge badge-ok">${f}</span>`).join("")}
        ${faults.map(f => {
          const def = Object.values(FAULTS).find(v => v[0] === f);
          return `<span class="badge ${badgeClass(def ? def[1] : 'warn')}">${f}</span>`;
        }).join("")}
      </div>
    </div>`;

    setTimeout(() => drawCompass(bid, az, elev), 50);
  }

  container.innerHTML = html;
}

function drawCompass(bid, az, elev) {
  const canvas = document.getElementById(`compass-${bid}`);
  if (!canvas) return;
  try { canvas.getContext("2d"); } catch(e) { return; }
  const ctx = canvas.getContext("2d");
  const cx = 80, cy = 80, r = 72;
  ctx.clearRect(0, 0, 160, 160);
  ctx.beginPath();
  ctx.arc(cx, cy, r, 0, 2 * Math.PI);
  ctx.strokeStyle = "#334155";
  ctx.lineWidth = 1;
  ctx.stroke();
  ctx.beginPath();
  ctx.arc(cx, cy, r * (90 - Math.abs(elev)) / 90, 0, 2 * Math.PI);
  ctx.strokeStyle = "rgba(56,189,248,.2)";
  ctx.stroke();
  for (let deg = 0; deg < 360; deg += 30) {
    const rad = deg * Math.PI / 180;
    ctx.beginPath();
    ctx.moveTo(cx + r * 0.88 * Math.cos(rad), cy - r * 0.88 * Math.sin(rad));
    ctx.lineTo(cx + r * Math.cos(rad), cy - r * Math.sin(rad));
    ctx.strokeStyle = deg % 90 === 0 ? "#94a3b8" : "#334155";
    ctx.stroke();
  }
  const sx = cx + r * (90 - Math.abs(elev)) / 90 * Math.cos((az - 90) * Math.PI / 180);
  const sy = cy - r * (90 - Math.abs(elev)) / 90 * Math.sin((az - 90) * Math.PI / 180);
  ctx.beginPath();
  ctx.arc(sx, sy, 5, 0, 2 * Math.PI);
  ctx.fillStyle = "#38bdf8";
  ctx.fill();
  ctx.shadowColor = "#38bdf8";
  ctx.shadowBlur = 8;
  ctx.fill();
  ctx.shadowBlur = 0;
}

function esc(s) { return String(s || "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;").replace(/'/g, "&#39;"); }

function populateChartSelect() {
  const ids = Object.keys(allBalls);
  const sel = document.getElementById("chart-ball");
  if (!sel) return;
  const cur = sel.value;
  sel.innerHTML = '<option value="">-- Select --</option>' +
    ids.map(id => `<option value="${id}" ${id===cur?'selected':''}>${id}</option>`).join("");
}

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

function populateDetailSelect() { populateSelects(); }
function populateHistorySelect() { populateSelects(); }

document.getElementById("detail-select")?.addEventListener("change", () => {
  renderDetail();
});

function renderDetail() {
  const sel = document.getElementById("detail-select");
  const bid = sel?.value;
  const container = document.getElementById("detail-content");
  if (!bid || !allBalls[bid]) {
    container.innerHTML = `<div class="empty-state"><p>Select a ball above</p></div>`;
    return;
  }
  const d = allBalls[bid];

  container.innerHTML = `<div class="grid">
    <div class="card">
      <h2>Direction</h2>
      <div class="stat-row"><span class="stat-label">X (east)</span>
        <span class="stat-value">${(d.dx||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Y (north)</span>
        <span class="stat-value">${(d.dy||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Z (up)</span>
        <span class="stat-value">${(d.dz||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Magnitude</span>
        <span class="stat-value">${(d.mag||0).toFixed(4)}</span></div>
      <div class="stat-row"><span class="stat-label">Vector valid</span>
        <span class="stat-value"><span class="badge ${d.vector_ok?'badge-ok':'badge-err'}">
          ${d.vector_ok?'YES':'NO'}</span></span></div>
    </div>
    <div class="card">
      <h2>Status</h2>
      <div class="stat-row"><span class="stat-label">Panel Azimuth</span>
        <span class="stat-value">${(d.panel_az||0).toFixed(1)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Panel Tilt</span>
        <span class="stat-value">${(d.panel_tilt||0).toFixed(1)}&deg;</span></div>
      <div class="stat-row"><span class="stat-label">Raw Error Mask</span>
        <span class="stat-value">0x${(d.err||0).toString(16).padStart(4,'0')}</span></div>
      <div class="stat-row"><span class="stat-label">Raw Flags</span>
        <span class="stat-value">0x${(d.flags||0).toString(16).padStart(2,'0')}</span></div>
      <div class="stat-row"><span class="stat-label">Timestamp</span>
        <span class="stat-value">${d.ts ? new Date(d.ts*1000).toISOString() : 'N/A'}</span></div>
    </div>
    <div class="card">
      <h2>Faults</h2>
      ${(d.fault_list||[]).length === 0
        ? '<div class="stat-row"><span class="stat-value" style="color:var(--green)">No faults</span></div>'
        : d.fault_list.map(f => {
            const def = Object.values(FAULTS).find(v => v[0] === f);
            return `<div class="stat-row"><span class="stat-value">
              <span class="badge ${badgeClass(def?def[1]:'warn')}">${f}</span>
              </span></div>`;
          }).join("")}
    </div>
  </div>`;
}

/* ---- History ---- */
document.getElementById("history-load")?.addEventListener("click", async () => {
  const ball = document.getElementById("history-ball").value;
  const limit = document.getElementById("history-limit").value;
  const container = document.getElementById("history-table");
  if (!ball) { container.innerHTML = "<p style='color:var(--muted)'>Select a ball</p>"; return; }
  container.innerHTML = "<p style='color:var(--muted)'>Loading...</p>";
  try {
    const resp = await fetch(`/api/history/${ball}?limit=${limit}`);
    const rows = await resp.json();
    if (!rows.length) {
      container.innerHTML = `<div class="empty-state"><p>No history for ${esc(ball)}</p></div>`;
      return;
    }
    let html = `<table><thead><tr>
      <th>Time</th><th>Az</th><th>Elev</th><th>Panel Az</th><th>Panel Tilt</th>
      <th>SOC</th><th>RSSI</th><th>Conf</th><th>Faults</th><th>Flags</th>
    </tr></thead><tbody>`;
    for (const r of rows) {
      const ts = r.ts ? new Date(r.ts * 1000).toLocaleTimeString() : "N/A";
      const az = r.azimuth?.toFixed(1) || "-";
      const elev = r.elevation?.toFixed(1) || "0.0";
      const faz = az !== "-" ? az : "-";
      const elevDeg = parseFloat(elev);
      const ftilt = isNaN(elevDeg) ? "-" : (90 - elevDeg).toFixed(1);
      html += `<tr>
        <td>${esc(String(ts))}</td>
        <td>${az}&deg;</td>
        <td>${elev}&deg;</td>
        <td>${faz}&deg;</td>
        <td>${ftilt}&deg;</td>
        <td>${r.soc}%</td>
        <td>${r.rssi} dBm</td>
        <td>${r.confidence}</td>
        <td>0x${(r.error_mask||0).toString(16)}</td>
        <td>0x${(r.flags||0).toString(16)}</td>
      </tr>`;
    }
    html += "</tbody></table>";
    container.innerHTML = html;
  } catch (e) {
    container.innerHTML = `<p style='color:var(--red)'>Error: ${esc(String(e))}</p>`;
  }
});

/* ---- OTA ---- */
document.getElementById("ota-send")?.addEventListener("click", async () => {
  const ball = document.getElementById("ota-ball").value;
  const url = document.getElementById("ota-url").value;
  const version = document.getElementById("ota-version").value;
  const result = document.getElementById("ota-result");
  if (!url) { result.innerHTML = `<span style="color:var(--red)">URL is required</span>`; return; }
  result.innerHTML = "Sending...";
  try {
    const resp = await fetch("/api/ota", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ball_id: ball, url, version }),
    });
    const data = await resp.json();
    if (data.ok) {
      result.innerHTML = `<span style="color:var(--green)">
        OTA command sent to ${esc(ball)}. Device will download and apply on next cycle.
      </span>`;
    } else {
      result.innerHTML = `<span style="color:var(--red)">Failed: ${esc(JSON.stringify(data))}</span>`;
    }
  } catch (e) {
    result.innerHTML = `<span style="color:var(--red)">Error: ${esc(String(e))}</span>`;
  }
});
