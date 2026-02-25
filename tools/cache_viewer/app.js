let cacheData = [];
let filteredData = [];
let currentPage = 1;
const ITEMS_PER_PAGE = 50;

// DOM Elements
const btnLoadDefault = document.getElementById('btn-load-default');
const fileInput = document.getElementById('file-input');
const searchInput = document.getElementById('search-input');
const resultCount = document.getElementById('result-count');
const tableBody = document.getElementById('table-body');
const btnPrev = document.getElementById('btn-prev');
const btnNext = document.getElementById('btn-next');
const pageInfo = document.getElementById('page-info');

// Event Listeners
btnLoadDefault.addEventListener('click', loadDefaultCache);
fileInput.addEventListener('change', handleFileUpload);
searchInput.addEventListener('input', handleSearch);
btnPrev.addEventListener('click', () => changePage(-1));
btnNext.addEventListener('click', () => changePage(1));

async function loadDefaultCache() {
    try {
        btnLoadDefault.textContent = "Loading...";
        btnLoadDefault.disabled = true;

        // Relative path assuming running from tools/cache_viewer/ via local server
        const response = await fetch('../../build/perf_cache.json');
        if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`);

        const data = await response.json();
        processData(data);

        btnLoadDefault.textContent = "Loaded!";
        setTimeout(() => {
            btnLoadDefault.textContent = "Load Default (build/perf_cache.json)";
            btnLoadDefault.disabled = false;
        }, 2000);

    } catch (error) {
        console.error("Failed to load default cache:", error);
        btnLoadDefault.textContent = "Error (Are you running a server?)";
        btnLoadDefault.style.backgroundColor = "var(--danger)";
        setTimeout(() => {
            btnLoadDefault.textContent = "Load Default (build/perf_cache.json)";
            btnLoadDefault.style.backgroundColor = "";
            btnLoadDefault.disabled = false;
        }, 3000);
    }
}

function handleFileUpload(event) {
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = function (e) {
        try {
            const data = JSON.parse(e.target.result);
            processData(data);
        } catch (error) {
            console.error("Error parsing JSON:", error);
            alert("Invalid JSON file uploaded.");
        }
    };
    reader.readAsText(file);
}

function processData(data) {
    // The cache mapping is an object of { "filepath": {metadataObject} }
    // We want to flatten it to an array for easier sorting/filtering
    cacheData = Object.entries(data).map(([path, meta]) => ({
        path: path,
        ...meta
    }));

    filteredData = [...cacheData];
    currentPage = 1;
    updateTable();
}

function handleSearch(event) {
    const query = event.target.value.toLowerCase();

    if (!query) {
        filteredData = [...cacheData];
    } else {
        filteredData = cacheData.filter(item => {
            const pathMatch = item.path && item.path.toLowerCase().includes(query);
            const cameraMatch = item.camera_model && item.camera_model.toLowerCase().includes(query);
            const makeMatch = item.camera_make && item.camera_make.toLowerCase().includes(query);
            const hashMatch = item.md5_hash && item.md5_hash.toLowerCase().includes(query);

            return pathMatch || cameraMatch || makeMatch || hashMatch;
        });
    }

    currentPage = 1;
    updateTable();
}

function formatBytes(bytes, decimals = 2) {
    if (!+bytes) return '0 Bytes';
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return `${parseFloat((bytes / Math.pow(k, i)).toFixed(dm))} ${sizes[i]}`;
}

function formatDate(timestamp) {
    if (!timestamp) return 'N/A';
    const date = new Date(timestamp * 1000);
    return date.toLocaleString();
}

function renderRow(item) {
    const tr = document.createElement('tr');

    const dim = (item.width && item.height) ? `${item.width} x ${item.height}` : 'N/A';
    const makeModel = (item.camera_make || item.camera_model) ? `${item.camera_make || ''} ${item.camera_model || ''}`.trim() : 'N/A';

    tr.innerHTML = `
        <td class="path-cell" title="${item.path}">${item.path}</td>
        <td>${formatBytes(item.size)}</td>
        <td class="hash-cell">${item.md5_hash || 'Pending'}</td>
        <td>${dim}</td>
        <td>${formatDate(item.date_taken || item.modified_date)}</td>
        <td>${makeModel}</td>
    `;
    return tr;
}

function updateTable() {
    resultCount.textContent = `${filteredData.length} items found`;

    const totalPages = Math.ceil(filteredData.length / ITEMS_PER_PAGE) || 1;
    pageInfo.textContent = `Page ${currentPage} of ${totalPages}`;

    btnPrev.disabled = currentPage === 1;
    btnNext.disabled = currentPage === totalPages;

    tableBody.innerHTML = '';

    const startIndex = (currentPage - 1) * ITEMS_PER_PAGE;
    const endIndex = Math.min(startIndex + ITEMS_PER_PAGE, filteredData.length);

    for (let i = startIndex; i < endIndex; i++) {
        tableBody.appendChild(renderRow(filteredData[i]));
    }
}

function changePage(delta) {
    const totalPages = Math.ceil(filteredData.length / ITEMS_PER_PAGE) || 1;
    currentPage += delta;

    if (currentPage < 1) currentPage = 1;
    if (currentPage > totalPages) currentPage = totalPages;

    updateTable();
}
