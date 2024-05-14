// let kvsIP = '127.0.0.1:50051';
let currentPage = 0; // Initial page

function resetPage() {
  currentPage = 0;
  document.getElementById('page-number').textContent = currentPage + 1;
  loadData(0);
}

function escapeHtml(str) {
  return str.replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function sanitizeData(data) {
  if (typeof data === 'string') {
    return escapeHtml(data);
  } else if (Array.isArray(data)) {
    return data.map(sanitizeData);
  } else if (typeof data === 'object' && data !== null) {
    Object.keys(data).forEach(key => {
      data[key] = sanitizeData(data[key]);
    });
    return data;
  }

  return data;
}

function loadData(direction) {
  currentPage += direction; // Update page based on direction: -1 for previous, 1 for next
  if (currentPage < 0) currentPage = 0; // Prevent going below page 0
  console.log('Loading page:', currentPage);
  const offset = document.getElementById('offset').value || 10; // Get offset from input, default is 10
  fetch(`/api/kvs/viewRows?page=${currentPage}&offset=${offset}&kvsIP=${kvsIP}`)
    .then(response => response.json())
    .then(data => {
      if (updateTable(sanitizeData(data))) {
        document.getElementById('page-number').textContent = currentPage + 1;
      }
    })
    .catch(error => console.error('Error:', error));
}

function updateTable(data) {
  // if data is empty, show an alert and don't update the table
  if (data.length === 0) {
    // alert('No data to display');
    return false;
  }
  const tableBody = document.getElementById('kvs-raw-table');
  const tableHeader = document.getElementById('table-header');
  tableBody.innerHTML = ''; // Clear existing table data

  // Determine all unique columns
  let columns = new Set(['Key']); // Start with 'Key' column
  data.forEach(row => {
    Object.keys(row.columns).forEach(col => columns.add(col));
  });

  // Update the table header
  tableHeader.innerHTML = '<th>Key</th>'; // Reset headers
  columns.forEach(col => {
    if (col !== 'Key') tableHeader.innerHTML += `<th>${col}</th>`;
  });

  // Insert new rows into the table
  data.forEach(row => {
    let rowHtml = `<td>${row.row}</td>`; // Key column
    columns.forEach(col => {
      if (col !== 'Key') {
        rowHtml += `<td>${row.columns[col] || ''}</td>`; // Add column data or empty string if not present
      }
    });
    tableBody.innerHTML += `<tr>${rowHtml}</tr>`;
  });
  return true;
}

// Initial data load
document.addEventListener("DOMContentLoaded", function () {
  loadData(0); // Load the first page
});
