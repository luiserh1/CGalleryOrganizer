const state = {
    allData: [],
    filteredData: [],
    currentPage: 1,
    rowsPerPage: 50,
    currentSort: { key: 'path', direction: 'asc' }
};

// --- Initialization ---
document.addEventListener('DOMContentLoaded', () => {
    setupEventListeners();
});

function setupEventListeners() {
    document.getElementById('file-input').addEventListener('change', handleFileSelect);
    document.getElementById('btn-load-default').addEventListener('click', loadDefaultCache);
    document.getElementById('search-input').addEventListener('input', debounce(handleSearch, 300));
    document.getElementById('btn-search-help').addEventListener('click', toggleSearchHelp);

    document.querySelectorAll('th[data-sort]').forEach(th => {
        th.addEventListener('click', () => handleSort(th.dataset.sort));
    });

    document.getElementById('btn-prev').addEventListener('click', () => changePage(-1));
    document.getElementById('btn-next').addEventListener('click', () => changePage(1));
}

function toggleSearchHelp() {
    document.getElementById('search-help').classList.toggle('hidden');
}

// --- Data Loading ---
async function loadDefaultCache() {
    try {
        const btn = document.getElementById('btn-load-default');
        btn.textContent = "Loading...";
        const response = await fetch('../../build/perf_cache.json');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);
        const data = await response.json();
        processData(data);
        btn.textContent = "Load Default (build/perf_cache.json)";
    } catch (error) {
        console.error('Error loading default cache:', error);
        alert('Could not load default cache. Ensure you run from project root and have run "make stress".');
    }
}

function handleFileSelect(event) {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
        try {
            const data = JSON.parse(e.target.result);
            processData(data);
        } catch (error) {
            alert('Error parsing JSON file.');
        }
    };
    reader.readAsText(file);
}

function processData(rawData) {
    // Handle both array and object formats
    state.allData = Array.isArray(rawData) ? rawData :
        Object.entries(rawData).map(([path, meta]) => ({ path, ...meta }));
    state.filteredData = [...state.allData];
    state.currentPage = 1;
    updateUI();
}

// --- Search / Filtering ---
function handleSearch() {
    const query = document.getElementById('search-input').value.toLowerCase().trim();

    if (!query) {
        state.filteredData = [...state.allData];
    } else {
        const parts = query.split(' ');

        state.filteredData = state.allData.filter(item => {
            return parts.every(part => {
                // Special check for tag:value
                if (part.includes(':')) {
                    const [key, val] = part.split(':');
                    if (item.allMetadataJson) {
                        return Object.entries(item.allMetadataJson).some(([mKey, mVal]) =>
                            mKey.toLowerCase().includes(key) && String(mVal).toLowerCase().includes(val)
                        );
                    }
                    return false;
                }

                // Special check for .extension
                if (part.startsWith('.')) {
                    return item.path.toLowerCase().endsWith(part);
                }

                // General search
                const pathMatch = item.path && item.path.toLowerCase().includes(part);
                const cameraMatch = item.cameraModel && item.cameraModel.toLowerCase().includes(part);
                const makeMatch = item.cameraMake && item.cameraMake.toLowerCase().includes(part);
                const hashMatch = item.exactHashMd5 && item.exactHashMd5.toLowerCase().includes(part);

                return pathMatch || cameraMatch || makeMatch || hashMatch;
            });
        });
    }

    state.currentPage = 1;
    updateUI();
}

// --- Rendering ---
function updateUI() {
    renderTable();
    updatePagination();
    document.getElementById('result-count').textContent = `${state.filteredData.length} items found`;
}

function renderTable() {
    const tbody = document.getElementById('table-body');
    tbody.innerHTML = '';

    const start = (state.currentPage - 1) * state.rowsPerPage;
    const end = Math.min(start + state.rowsPerPage, state.filteredData.length);
    const pageData = state.filteredData.slice(start, end);

    pageData.forEach((item, index) => {
        const globalIndex = start + index;
        const tr = document.createElement('tr');
        const dim = (item.width && item.height) ? `${item.width} x ${item.height}` : '-';
        const size = item.fileSize ? formatBytes(item.fileSize) : '-';
        const date = item.dateTaken || (item.modificationDate ? new Date(item.modificationDate * 1000).toLocaleString() : '-');
        const cam = [item.cameraMake, item.cameraModel].filter(Boolean).join(' ') || '-';
        const hash = item.exactHashMd5 ? item.exactHashMd5.substring(0, 8) + '...' : 'Pending';

        tr.innerHTML = `
            <td class="expand-col"><button class="btn-expand" onclick="toggleDetails(${globalIndex})">▶</button></td>
            <td class="path-cell" title="${item.path}">${item.path.split('/').pop()}</td>
            <td>${size}</td>
            <td class="hash-cell" title="${item.exactHashMd5}">${hash}</td>
            <td>${dim}</td>
            <td>${date}</td>
            <td>${cam}</td>
        `;
        tbody.appendChild(tr);

        // Details Row
        const detailsTr = document.createElement('tr');
        detailsTr.id = `details-${globalIndex}`;
        detailsTr.className = 'details-row hidden';

        let extraHtml = '<div class="details-container"><div class="metadata-grid">';
        if (item.allMetadataJson && Object.keys(item.allMetadataJson).length > 0) {
            Object.entries(item.allMetadataJson).forEach(([k, v]) => {
                extraHtml += `
                    <div class="meta-item">
                        <span class="meta-key">${k}</span>
                        <span class="meta-val">${v}</span>
                    </div>
                `;
            });
        } else {
            extraHtml += '<p style="grid-column: 1/-1; color: #8b949e; padding: 1rem;">No exhaustive metadata available for this entry. Run with --exhaustive to capture all tags.</p>';
        }
        extraHtml += '</div></div>';

        detailsTr.innerHTML = `
            <td colspan="7">
                ${extraHtml}
            </td>
        `;
        tbody.appendChild(detailsTr);
    });
}

window.toggleDetails = function (index) {
    const detailsRow = document.getElementById(`details-${index}`);
    const btn = detailsRow.previousElementSibling.querySelector('.btn-expand');

    detailsRow.classList.toggle('hidden');
    // Allow a small delay for the hidden class removal before animating
    setTimeout(() => {
        detailsRow.classList.toggle('active');
        btn.classList.toggle('active');
    }, 10);
};

// --- Utilities ---
function formatBytes(bytes) {
    if (!bytes || bytes === 0) return '0 B';
    const k = 1024;
    const sizes = ['B', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
}

function handleSort(key) {
    if (state.currentSort.key === key) {
        state.currentSort.direction = state.currentSort.direction === 'asc' ? 'desc' : 'asc';
    } else {
        state.currentSort.key = key;
        state.currentSort.direction = 'asc';
    }

    state.filteredData.sort((a, b) => {
        let valA = a[key] || '';
        let valB = b[key] || '';

        if (typeof valA === 'string') valA = valA.toLowerCase();
        if (typeof valB === 'string') valB = valB.toLowerCase();

        if (valA < valB) return state.currentSort.direction === 'asc' ? -1 : 1;
        if (valA > valB) return state.currentSort.direction === 'asc' ? 1 : -1;
        return 0;
    });

    renderTable();
}

function changePage(delta) {
    state.currentPage += delta;
    updateUI();
}

function updatePagination() {
    const totalPages = Math.ceil(state.filteredData.length / state.rowsPerPage) || 1;
    document.getElementById('page-info').textContent = `Page ${state.currentPage} of ${totalPages}`;
    document.getElementById('btn-prev').disabled = state.currentPage === 1;
    document.getElementById('btn-next').disabled = state.currentPage === totalPages;
}

function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}
