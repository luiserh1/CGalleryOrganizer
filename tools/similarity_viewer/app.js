const state = {
  report: null,
  groups: [],
  filteredGroups: [],
  selectedIndex: -1,
  minScore: 0,
  search: "",
};

document.addEventListener("DOMContentLoaded", () => {
  wireEvents();
});

function wireEvents() {
  document.getElementById("btn-load-default").addEventListener("click", loadDefault);
  document.getElementById("file-input").addEventListener("change", onUpload);
  document.getElementById("search").addEventListener("input", (e) => {
    state.search = e.target.value.trim().toLowerCase();
    applyFilters();
  });
  document.getElementById("min-score").addEventListener("input", (e) => {
    const value = Number(e.target.value || 0);
    state.minScore = value;
    document.getElementById("min-score-value").value = value.toFixed(2);
    applyFilters();
  });
  document.getElementById("min-score-value").addEventListener("input", (e) => {
    let value = Number(e.target.value || 0);
    if (Number.isNaN(value)) {
      value = 0;
    }
    value = Math.max(0, Math.min(1, value));
    state.minScore = value;
    document.getElementById("min-score").value = String(value);
    applyFilters();
  });
  document.getElementById("btn-export").addEventListener("click", exportFilteredPairs);
}

async function loadDefault() {
  const btn = document.getElementById("btn-load-default");
  btn.textContent = "Loading...";
  try {
    const response = await fetch("../../build/similarity_report.json");
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    const data = await response.json();
    setReport(data);
  } catch (err) {
    alert("Could not load ../../build/similarity_report.json. Generate it with --similarity-report or upload one.");
  } finally {
    btn.textContent = "Load Default";
  }
}

function onUpload(event) {
  const file = event.target.files[0];
  if (!file) {
    return;
  }
  const reader = new FileReader();
  reader.onload = (e) => {
    try {
      const data = JSON.parse(e.target.result);
      setReport(data);
    } catch (_err) {
      alert("Invalid JSON file.");
    }
  };
  reader.readAsText(file);
}

function setReport(report) {
  if (!report || !Array.isArray(report.groups)) {
    alert("Report format is invalid. Expected top-level groups[].");
    return;
  }

  state.report = report;
  state.groups = report.groups.map((g) => ({
    anchorPath: g.anchorPath || "",
    neighborCount: Number(g.neighborCount || (Array.isArray(g.neighbors) ? g.neighbors.length : 0)),
    neighbors: Array.isArray(g.neighbors) ? g.neighbors : [],
  }));

  document.getElementById("stat-groups").textContent = String(report.groupCount ?? state.groups.length);
  document.getElementById("stat-threshold").textContent = String(report.threshold ?? "-");
  document.getElementById("stat-model").textContent = report.modelId || "-";

  applyFilters();
}

function applyFilters() {
  const q = state.search;
  const minScore = state.minScore;

  state.filteredGroups = state.groups
    .map((g) => {
      const neighbors = g.neighbors
        .filter((n) => Number(n.score || 0) >= minScore)
        .sort((a, b) => Number(b.score || 0) - Number(a.score || 0) || String(a.path || "").localeCompare(String(b.path || "")));

      return {
        ...g,
        neighbors,
        neighborCount: neighbors.length,
      };
    })
    .filter((g) => {
      if (!q) {
        return g.neighborCount > 0;
      }
      const anchorHit = g.anchorPath.toLowerCase().includes(q);
      const neighborHit = g.neighbors.some((n) => String(n.path || "").toLowerCase().includes(q));
      return (anchorHit || neighborHit) && g.neighborCount > 0;
    })
    .sort((a, b) => b.neighborCount - a.neighborCount || a.anchorPath.localeCompare(b.anchorPath));

  if (state.filteredGroups.length === 0) {
    state.selectedIndex = -1;
  } else if (state.selectedIndex < 0 || state.selectedIndex >= state.filteredGroups.length) {
    state.selectedIndex = 0;
  }

  renderGroupList();
  renderPairTable();
}

function renderGroupList() {
  const list = document.getElementById("group-list");
  list.innerHTML = "";

  state.filteredGroups.forEach((group, idx) => {
    const li = document.createElement("li");
    if (idx === state.selectedIndex) {
      li.classList.add("active");
    }

    li.innerHTML = `
      <div><strong>${escapeHtml(shortName(group.anchorPath))}</strong></div>
      <div class="meta">${escapeHtml(group.anchorPath)}</div>
      <div class="meta">neighbors: ${group.neighborCount}</div>
    `;

    li.addEventListener("click", () => {
      state.selectedIndex = idx;
      renderGroupList();
      renderPairTable();
    });

    list.appendChild(li);
  });
}

function renderPairTable() {
  const tbody = document.getElementById("pair-table");
  tbody.innerHTML = "";

  if (state.selectedIndex < 0 || state.selectedIndex >= state.filteredGroups.length) {
    return;
  }

  const group = state.filteredGroups[state.selectedIndex];
  group.neighbors.forEach((neighbor) => {
    const tr = document.createElement("tr");
    tr.innerHTML = `
      <td class="path">${escapeHtml(String(neighbor.path || ""))}</td>
      <td class="score">${Number(neighbor.score || 0).toFixed(4)}</td>
    `;
    tbody.appendChild(tr);
  });
}

async function exportFilteredPairs() {
  const payload = state.filteredGroups.map((group) => ({
    anchorPath: group.anchorPath,
    neighborCount: group.neighborCount,
    neighbors: group.neighbors,
  }));

  const text = JSON.stringify(payload, null, 2);
  try {
    await navigator.clipboard.writeText(text);
    alert("Filtered pairs copied to clipboard.");
  } catch (_err) {
    alert("Failed to copy to clipboard.");
  }
}

function shortName(path) {
  const parts = String(path || "").split("/");
  return parts[parts.length - 1] || path;
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}
