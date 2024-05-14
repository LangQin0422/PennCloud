function updateWorkerData() {
    fetch('/api/workers')
        .then(response => response.json())
        .then(data => {
            const table = document.getElementById('workers-table');
            table.innerHTML = ''; // Clear existing table data
            data.forEach(worker => {
                const row = `
                        <tr>
                            <td class='worker-id'><a href='http://${worker.ip}:${worker.port}'>${worker.id}</a></td>
                            <td>${worker.lastPing} seconds ago</td>
                            <td>${worker.status}</td>
                            <td>
                                <div class='load-bar'>
                                    <div class='load-fill' style='width: ${Math.min(worker.loadPercent, 100)}%'></div>
                                    <span class='load-text'>${Math.min(worker.loadPercent, 100)}%</span>
                                </div>
                            </td>
                        </tr>`;
                table.innerHTML += row; // Append new row
            });
        })
        .catch(error => console.error('Error:', error));
}

function updateKVSData() {
    fetch('/api/kvs')
        .then(response => response.json())
        .then(data => {
            const table = document.getElementById('kvs-table');
            table.innerHTML = ''; // Clear existing table data
            data.forEach(worker => {
                const buttonText = worker.status === 'alive' ? 'Kill' : 'Start';
                const buttonAction = worker.status === 'alive' ? 'killWorker' : 'startWorker';
                const buttonClass = worker.status === 'alive' ? 'kill' : 'start'; // Apply corresponding button class
                const row = `
                        <tr>
                            <td><a href="${window.location.origin}/kvsTable?kvsIP=${worker.server}">${worker.server}</a></td>
                            <td>${worker.status}</td>
                            <td>
                                <button class="kill-button ${buttonClass}" onclick="${buttonAction}('${worker.server}')">${buttonText}</button>
                            </td>
                        </tr>`;
                table.innerHTML += row; // Append new row
            });
        })
        .catch(error => console.error('Error:', error));
}


document.addEventListener('DOMContentLoaded', updateWorkerData);
setInterval(updateWorkerData, 5000); // Update every 5 seconds

document.addEventListener('DOMContentLoaded', updateKVSData);
setInterval(updateKVSData, 5000);

function startWorker(workerID) {
    fetch(`/api/kvs/start?workerID=${workerID}`, {
        method: 'POST' // Assuming POST method, change it if necessary
    })
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            // Refresh worker data after starting worker
            updateKVSData();
        })
        .catch(error => console.error('Error:', error));
}


function killWorker(workerID) {
    fetch(`/api/kvs/kill?workerID=${workerID}`, {
        method: 'POST' // Assuming POST method, change it if necessary
    })
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            // Refresh worker data after killing worker
            updateKVSData();
        })
        .catch(error => console.error('Error:', error));
}