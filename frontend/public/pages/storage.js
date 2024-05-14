const coordUrl = "http://localhost:8000/register";
var renameModal;

document.addEventListener("DOMContentLoaded", async function () {
  try {
    const assignedUrl = localStorage.getItem('assignedUrl');
    if (!assignedUrl) { 
      alert("Not logged in. Redirecting to login page.")
      window.location.href = '/';
    }
    const loginResponse = await fetch(`${assignedUrl}/isLoggedIn`);
    const body = await loginResponse.text();

    if (body === "false") {
      window.location.href = '/';
    } else {
      const greetingHeader = document.getElementById('greeting-header');
      greetingHeader.textContent = `Cloud Storage for ${body}`;
    }

    // Fetch folders from the server and display them
    const folderResponse = await fetch(`${assignedUrl}/folders?path=/`);
    if (folderResponse.status == 200 && folderResponse.ok) {
        const folders = await folderResponse.json();
        folders.forEach(folder => {
            appendFolderToList(folder.name);
        });
    }

    // Fetch files from the server and display them
    const fileResponse = await fetch(`${assignedUrl}/files?path=/`);
    if (fileResponse.status == 200 && fileResponse.ok) {
        const files = await fileResponse.json();
        files.forEach(file => {
            appendFileToList(file.name, file.size, file.date, file.type);
        });
    }

    // Event listener for creating folders
    document.getElementById('create-folder-btn').addEventListener('click', async () => {
        // Get folder name from the input field
        const folderParent = document.getElementById('current-folder').textContent.trim();
        const folderName = document.getElementById('folder-name').value.trim();

        // Folder name cannot be empty, contain spaces, contain slashes, contain dashes or be '..'
        if (folderName === '' || folderName.includes(' ') || folderName.includes('/') || folderName.includes('-') || folderName === '..') {
            alert('Folder name cannot be empty, contain spaces, contain slashes, contain dashes or be \'..\'');
            return;
        }

        // Send a request to create a folder with the folder name
        const createFolderResponse = await fetch(`${assignedUrl}/folders`, {
            method: 'POST',
            body: folderParent + folderName
        });

        if (createFolderResponse.ok) {
            const folder = await createFolderResponse.json();
            appendFolderToList(folder.name);
            alert('Folder created successfully');
            document.getElementById('folder-name').value = '';
        } else {
            alert('Folder creation failed');
            document.getElementById('folder-name').value = '';
        }
    });

    // Event listener for uploading files
    document.getElementById('upload-btn').addEventListener('click', async () => {
        // Get file to upload
        const fileInput = document.getElementById('file-upload');
        const file = fileInput.files[0];

        // Check if a file is selected
        if (!file) {
            alert('Please select a file to upload');
            return;
        }

        if (file.name === '' || file.name.includes(' ') || file.name.includes('/') || file.name.includes('-')) {
            alert('File name cannot be empty, contain spaces, contain slashes, and contain dashes');
            return;
        }

        // Get current folder path
        const folderPath = document.getElementById('current-folder').textContent.slice(0, -1);

        // Send a request to upload the file
        const uploadFileResponse = await fetch(`${assignedUrl}/files?path=${folderPath}`, {
            method: 'POST',
            headers: {
                'X-File-Type': file.type,
                'X-File-Name': file.name,
                'X-File-Size': file.size,
                'X-File-Last-Modified': new Date(file.lastModified).toLocaleString()
            },
            body: file
        });

        // Check if file is uploaded successfully
        if (uploadFileResponse.ok) {
            const file = await uploadFileResponse.json();
            appendFileToList(file.name, file.size, file.date, file.type);
            alert('File uploaded successfully');
            fileInput.value = '';
        } else {
            alert('File upload failed, status code:', uploadFileResponse.status);
            fileInput.value = '';
        }
    });

    // Event listener for renaming
    document.getElementById('rename-btn').addEventListener('click', async () => {
        const id = document.getElementById('rename-id').value.trim();
        const curName = document.getElementById('rename-cur-name').value.trim();
        const newName = document.getElementById('rename-new-name').value.trim();

        if (newName.includes(' ') || newName.includes('/') || newName.includes('-')) {
            alert('File name cannot be empty, contain spaces, contain slashes, and contain dashes');
            return;
        }

        if (newName !== "") {
            await rename(id, curName, newName);
            renameModal.hide();
            document.getElementById('rename-new-name').value = '';
        } else {
            alert("Please enter a valid folder name.");
        }
    });

  } catch (error) {
    console.error('Error checking login status:', error);
  }
});

// FOLDER FUNCTIONS

async function openFolder(folderName) {
    try {

        if (!localStorage.getItem('assignedUrl')) {
            await updateAssignedUrl();
        }
        const assignedUrl = localStorage.getItem('assignedUrl');
  
        // Get new folder path
        const folderParent = document.getElementById('current-folder').textContent;
        let folderPath = '';
        if (folderName == '..') {
            folderPath = folderParent.split('/').slice(0, -2).join('/');
        } else {
            folderPath = folderParent + folderName;
        }
  
        // Send a request to get the folder with the folder name
        const openFolderResponse = await fetch(`${assignedUrl}/folders?path=${folderPath}`);
        if (openFolderResponse.ok) {
    
            // Reset the folder list
            const folderList = document.getElementById('folder-list');
            folderList.innerHTML = '';

            // Append the subfolders to the folder list
            if (openFolderResponse.status == 200) {
                const folders = await openFolderResponse.json();
                folders.forEach(folder => {
                    appendFolderToList(folder.name);
                });
            }

        } else {
            alert('Failed to get subfolders to open');
        }
  
        // Reset the file list to the new folder's file list
        const getFilesResponse = await fetch(`${assignedUrl}/files?path=${folderPath}`);
        if (getFilesResponse.ok) {

            // Reset the file list
            const fileList = document.getElementById('file-list');
            fileList.innerHTML = '';

            if (getFilesResponse.status == 200) {
                const files = await getFilesResponse.json();
                files.forEach(file => {
                    appendFileToList(file.name, file.size, file.date, file.type);
                });
            }
        } else {
            alert('Failed to get files in folder');
        }

        // Update the current folder path
        document.getElementById('current-folder').textContent = folderPath + '/';
  
    } catch (error) {
        console.error('Error opening the folder:', error);
        alert('Failed to open folder');
    }
}

async function deleteFolder(folderItemId, folderName) {
    const confirmDelete = confirm('Are you sure you want to delete this folder?');
    if (confirmDelete) {
        try {

            if (!localStorage.getItem('assignedUrl')) {
                await updateAssignedUrl();
            }
            const assignedUrl = localStorage.getItem('assignedUrl');

            // Get current folder path
            const folderParent = document.getElementById('current-folder').textContent.slice(0, -1);

            // Send a request to delete the file with the file name
            const deleteResponse = await fetch(`${assignedUrl}/folders/${folderName}?path=${folderParent}`, {
                method: 'DELETE',
            });

            if (deleteResponse.ok) {
                document.getElementById(folderItemId).remove();
                alert('Folder deleted successfully');
            } else {
                alert('Folder Deletion failed');
            }

        } catch (error) {
            console.error('Error deleting the email:', error);
            alert('Folder Deletion failed');
        } 
    }
}

async function appendFolderToList(folderName) {

    // Get the current folder parent
    const folderParent = document.getElementById('current-folder').textContent;
  
    // Creating a list group item for each folder
    const folderItem = document.createElement('a');
    folderItem.classList.add('list-group-item', 'list-group-item-action');
    folderItem.href = '#';
    if (folderName !== '..') { // Check if fileName is not ".."
      folderItem.innerHTML = `
          <div class="d-flex w-100 justify-content-between align-items-center">
            <div>
                <h5 class="mb-1 text-truncate">${folderName}</h5>
            </div>
            <div>
              <button class="btn btn-secondary btn-sm rename-btn" type="button">Rename</button>
              <button class="btn btn-secondary btn-sm move-btn" type="button">Move</button>
              <button class="btn btn-danger btn-sm delete-btn" type="button">Delete</button>
            </div>
          </div>  
      `;
    } else {
      folderItem.innerHTML = `
          <div class="d-flex w-100 justify-content-between align-items-center">
            <div>
                <h5 class="mb-1 text-truncate">${folderName}</h5>
            </div>
          </div>
      `;
    }
    folderItem.id = folderParent + folderName;
  
    // Event listener for a folder
    folderItem.addEventListener('click', async (event) => {

        if (event.target.classList.contains('move-btn')) {
            event.preventDefault();

            document.getElementById('move-type').value = 'folders';

            var moveModal = new bootstrap.Modal(document.getElementById('move-modal'));
            // await openFolderModal('', folderParent.slice(0, -1), folderName, folderItem.id, moveModal);
            await initMoveModal(folderItem.id, folderParent.slice(0, -1), folderName, moveModal);
            moveModal.show();

        } else if (event.target.classList.contains('rename-btn')) {

            event.preventDefault();
            
            // Set the folder name in the modal
            document.getElementById('rename-type').value = 'folders';
            document.getElementById('rename-cur-name').value = folderName;
            document.getElementById('rename-id').value = folderItem.id;

            renameModal = new bootstrap.Modal(document.getElementById('rename-modal'));
            renameModal.show();

        } else if (event.target.classList.contains('delete-btn')) {
            event.preventDefault();
            await deleteFolder(folderItem.id, folderName);
        } else {
            event.preventDefault();
            await openFolder(folderName);
        }

    });
  
    const folderList = document.getElementById('folder-list');
    folderList.appendChild(folderItem);
}

// FILE FUNCTIONS

async function downloadFile(fileName) {
    try {
        const assignedUrl = localStorage.getItem('assignedUrl');
        if (!assignedUrl) { 
            alert("Not logged in. Redirecting to login page.")
            window.location.href = '/';
        }
        
        // Get the current folder path
        const folderPath = document.getElementById('current-folder').textContent.slice(0, -1);

        // Send a request to download the file with the file name
        const downloadResponse = await fetch(`${assignedUrl}/files/${fileName}?path=${folderPath}`);
        if (!downloadResponse.ok) {
            alert('Download failed');
            return;
        }
        
        const fileBlob = await downloadResponse.blob();
        const fileUrl = URL.createObjectURL(fileBlob);

        // Create a link element to download the file
        const anchor = document.createElement('a');
        anchor.href = fileUrl;
        anchor.download = fileName;

        // Append the anchor to the body and click it
        document.body.appendChild(anchor);
        anchor.click();
        document.body.removeChild(anchor);

        // Revoke the object URL
        URL.revokeObjectURL(fileUrl);
    } catch (error) {
        console.error('Error downloading the file:', error);
        alert('Download failed');
    }
}

async function deleteFile(fileName) {
    const confirmDelete = confirm('Are you sure you want to delete this file?');
    if (confirmDelete) {
        try {

            if (!localStorage.getItem('assignedUrl')) {
                await updateAssignedUrl();
            }
            const assignedUrl = localStorage.getItem('assignedUrl');

            // Get the current folder the file is in
            const folderPath = document.getElementById('current-folder').textContent.slice(0, -1);

            // Send a request to delete the file with the file name
            const deleteResponse = await fetch(`${assignedUrl}/files/${fileName}?path=${folderPath}`, {
                method: 'DELETE',
            });
            
            if (deleteResponse.ok) {
                document.getElementById('file-' + fileName).remove();
            } else {
                alert('Deletion failed');
            }
        } catch (error) {
            console.error('Error deleting the email:', error);
            alert('Deletion failed');
        } 
    }
}

async function appendFileToList(fileName, fileSize, fileDate, fileType) {
  
    // Creating a list group item for each file
    const fileItem = document.createElement('a');
    fileItem.classList.add('list-group-item', 'list-group-item-action');
    fileItem.href = '#';
    fileItem.innerHTML = `
        <div class="d-flex w-100 justify-content-between align-items-center">
          <div>
              <h5 class="mb-1 text-truncate"">${fileName}</h5>
              <div class="d-flex align-items-center">
                  <small class="me-2">${fileSize}</small>
                  <small class="me-2">${fileType}</small>
                  <small>${fileDate}</small>
              </div>
          </div>
          <div>
            <button class="btn btn-secondary btn-sm rename-btn" type="button">Rename</button>
            <button class="btn btn-secondary btn-sm move-btn" type="button">Move</button>
            <button class="btn btn-danger btn-sm delete-btn" type="button">Delete</button>
          </div>
        </div>
    `;
    fileItem.id = "file-" + fileName;
  
    // Event listener for downloading the file
    fileItem.addEventListener('click', async (event) => {

        if (event.target.classList.contains('move-btn')) {
            event.preventDefault();

            document.getElementById('move-type').value = 'files';
            const fileParent = document.getElementById('current-folder').textContent.slice(0, -1);

            var moveModal = new bootstrap.Modal(document.getElementById('move-modal'));
            // await openFolderModal('', fileParent, fileName, fileItem.id, moveModal)
            await initMoveModal(fileItem.id, fileParent, fileName, moveModal)
            moveModal.show();
        } else if (event.target.classList.contains('rename-btn')) {
            event.preventDefault();

            // Set the file info in the modal
            document.getElementById('rename-type').value = 'files';
            document.getElementById('rename-cur-name').value = fileName;
            document.getElementById('rename-id').value = fileItem.id;
            document.getElementById('rename-file-size').value = fileSize;
            document.getElementById('rename-file-date').value = fileDate;
            document.getElementById('rename-file-type').value = fileType;

            renameModal = new bootstrap.Modal(document.getElementById('rename-modal'));
            renameModal.show();
        } else if (event.target.classList.contains('delete-btn')) {
            event.preventDefault();
            await deleteFile(fileName);
        } else {
            event.preventDefault();
            await downloadFile(fileName);
        }

    });
  
    // Append to file list
    const fileList = document.getElementById('file-list');
    fileList.appendChild(fileItem);
}

// RENAME FUNCTIONS

async function rename(id, name, newName) {
    try {

        if (!localStorage.getItem('assignedUrl')) {
            await updateAssignedUrl();
        }
        const assignedUrl = localStorage.getItem('assignedUrl');

        // Get folder name from the input field
        const type = document.getElementById('rename-type').value;
        const folderParent = document.getElementById('current-folder').textContent.slice(0, -1);

        // Send a request to create a folder with the folder name
        const renameFolderResponse = await fetch(`${assignedUrl}/${type}/${name}/${newName}?path=${folderParent}`, {
            method: 'PUT'
        });

        if (renameFolderResponse.ok) {
            // Remove the current file/folder
            document.getElementById(id).remove();

            // Append the new file/folder
            if (type === 'files') {
                // Get the file info
                const fileSize = document.getElementById('rename-file-size').value;
                const fileDate = document.getElementById('rename-file-date').value;
                const fileType = document.getElementById('rename-file-type').value;

                await appendFileToList(newName, fileSize, fileDate, fileType);

                // Remove the file info
                document.getElementById('rename-file-size').value = '';
                document.getElementById('rename-file-date').value = '';
                document.getElementById('rename-file-type').value = '';
                
            } else {
                await appendFolderToList(newName);
            }

            document.getElementById('rename-type').value = '';
            document.getElementById('rename-cur-name').value = '';
            document.getElementById('rename-id').value = '';

            alert('Renamed successfully');
        } else {
            alert('Rename failed');
        }

    } catch (error) {
        console.error('Error renaming folder:', error);
        alert('Rename failed');
    }
}

// MOVE FUNCTIONS

async function move(toMoveId, toMoveName, moveToFolderName, modal) {
    try {

        if (!localStorage.getItem('assignedUrl')) {
            await updateAssignedUrl();
        }
        const assignedUrl = localStorage.getItem('assignedUrl');

        // Get toMoveFolder path
        const toMoveFolderParent = document.getElementById('current-folder').textContent.slice(0, -1);
        const moveToFolderParent = document.getElementById('modal-current-folder').textContent;
        let moveToFolderPath = '';
        if (moveToFolderParent === '/' && moveToFolderName === '/') {
            moveToFolderPath = moveToFolderParent
        } else {
            moveToFolderPath = moveToFolderParent + moveToFolderName;
        }

        const type = document.getElementById('move-type').value;

        // Send a request to move the folder with the folder name
        const moveResponse = await fetch(`${assignedUrl}/${type}/${toMoveName}?path=${toMoveFolderParent}&dest=${moveToFolderPath}`, {
            method: 'PUT'
        });

        if (moveResponse.ok) {
            document.getElementById(toMoveId).remove();
            modal.hide();
            alert('Moved successfully');
        } else {
            modal.hide();
            alert('Failed to move');
        }

        document.getElementById('move-type').value = '';
        document.getElementById('modal-current-folder').textContent = '/';
        document.getElementById('modal-folder-List').innerHTML = '';

    } catch (error) {
        console.error('Error moving folder:', error);
        alert('Failed to move');
    }
}

async function openFolderModal(folderName, notIncludeFolderName, toMoveName, toMoveId, modal) {
    try {

        if (!localStorage.getItem('assignedUrl')) {
            await updateAssignedUrl();
        }
        const assignedUrl = localStorage.getItem('assignedUrl');

        // Get new folder path
        const folderParent = document.getElementById('modal-current-folder').textContent;
        let folderPath = '';
        if (folderName === '..') {
            folderPath = folderParent.split('/').slice(0, -2).join('/');
        } else {
            folderPath = folderParent + folderName;
        }

        // Send a request to get the folder with the folder name
        const openFolderResponse = await fetch(`${assignedUrl}/folders?path=${folderPath}`);
        if (openFolderResponse.ok) {

            // Reset the folder list
            const folderList = document.getElementById('modal-folder-List');
            folderList.innerHTML = '';

            if (openFolderResponse.status == 200) {
                
                // if folder path is empty or "/", append root
                if (folderPath === '' || folderPath === '/') {
                    appendBackSlashToListModal('/', toMoveName, toMoveId, modal);
                }

                // Reset the folder list to the new folder's folder list
                const folders = await openFolderResponse.json();
                folders.forEach(folder => {
                    appendFolderToListModal(folder.name, notIncludeFolderName, toMoveName, toMoveId, modal);
                });
            }
        } else {
            alert('Failed to get folder to open');
        }

        // Update the current folder path
        document.getElementById('modal-current-folder').textContent = folderPath + '/';

    } catch (error) {
        console.error('Error opening the folder:', error);
        alert('Failed to get folder to open');
    }
}

async function initMoveModal(toMoveId, notIncludeFolderName, toMoveName, modal) {
    try {

        if (!localStorage.getItem('assignedUrl')) {
            await updateAssignedUrl();
        }
        const assignedUrl = localStorage.getItem('assignedUrl');

        // Send a request to get root folders
        const openFolderResponse = await fetch(`${assignedUrl}/folders?path=/`);
        if (openFolderResponse.ok) {

            // Reset the folder list
            const folderList = document.getElementById('modal-folder-List');
            folderList.innerHTML = '';

            // Show a list of current folders
            if (openFolderResponse.status == 200) {
                // Include the root folder
                appendBackSlashToListModal('/', toMoveName, toMoveId, modal);

                const folders = await openFolderResponse.json();
                folders.forEach(folder => {
                    appendFolderToListModal(folder.name, notIncludeFolderName, toMoveName, toMoveId, modal);
                });
            }
        } else {
            alert('Failed to move');
        }

    } catch (error) {
        console.error('Error moving:', error);
    }
}

function appendBackSlashToListModal(folderName, toMoveName, toMoveId, modal) {

    // Get the current modal folder path
    const folderParent = document.getElementById('modal-current-folder').textContent;
    const currentFolder = document.getElementById('current-folder').textContent;

    // Creating a list group item for each folder
    const folderItem = document.createElement('a');
    folderItem.classList.add('list-group-item', 'list-group-item-action');
    folderItem.href = '#';
    if (currentFolder !== '/') {    // create the folder item with select button if not "/"
        folderItem.innerHTML = `
            <div class="d-flex w-100 justify-content-between align-items-center">
                <div>
                    <h5 class="mb-1 text-truncate">${folderName}</h5>
                </div>
                <div>
                    <button class="btn btn-primary btn-sm select-btn" type="button">Select</button>
                </div>
            </div>  
        `;
    } else {
        folderItem.innerHTML = `
            <div class="d-flex w-100 justify-content-between align-items-center">
                <div>
                    <h5 class="mb-1 text-truncate">${folderName}</h5>
                </div>
            </div>  
        `;
    }
    folderItem.id = folderParent + folderName;

    // Event listener for a folder
    folderItem.addEventListener('click', async (event) => {
        if (event.target.classList.contains('select-btn')) {
            event.preventDefault();
            await move(toMoveId, toMoveName, folderName, modal);
        } else {
            event.preventDefault();
        }
    });

    const folderList = document.getElementById('modal-folder-List');
    folderList.appendChild(folderItem);
}

function appendFolderToListModal(folderName, notIncludeFolderName, toMoveName, toMoveId, modal) {

    // Get the current modal folder path
    const type = document.getElementById('move-type').value;
    const folderParent = document.getElementById('modal-current-folder').textContent;
    const folderPath = folderParent + folderName;

    // Creating a list group item for each folder
    const folderItem = document.createElement('a');
    folderItem.classList.add('list-group-item', 'list-group-item-action');
    folderItem.href = '#';
    if ((type === 'folders' && folderName === toMoveName) || (folderName !== '..' && folderPath !== notIncludeFolderName)) {
        folderItem.innerHTML = `
            <div class="d-flex w-100 justify-content-between align-items-center">
                <div>
                    <h5 class="mb-1 text-truncate">${folderName}</h5>
                </div>
                <div>
                    <button class="btn btn-primary btn-sm select-btn" type="button">Select</button>
                </div>
            </div>  
        `;
    } else {
        folderItem.innerHTML = `
            <div class="d-flex w-100 justify-content-between align-items-center">
                <div>
                    <h5 class="mb-1 text-truncate">${folderName}</h5>
                </div>
            </div>
        `;
    }
    folderItem.id = folderParent + folderName;

    // Event listener for a folder
    folderItem.addEventListener('click', async (event) => {
        if (event.target.classList.contains('select-btn')) {
            event.preventDefault();
            await move(toMoveId, toMoveName, folderName, modal);
        } else {
            event.preventDefault();
            await openFolderModal(folderName, notIncludeFolderName, toMoveName, toMoveId, modal);
        }
    });

    const folderList = document.getElementById('modal-folder-List');
    folderList.appendChild(folderItem);
}

/**
 * Remove the last "/" from openFolder fetch.
 * KVS would fail if I sign back in.
 * Make sure when moving folders, you cannot move to the current folder.
 * I moved the initialization to the page loading event listener, would it affect the functionality?
 * How do I hide the rename modal if we don't have the modal object?
 * Renamed file10.jpg to file10, upload another file10.jpg, rename one of them to file10 and deleted it. Now I can't delete the other one.
 * Two types of the following, which one?

        const assignedUrl = localStorage.getItem('assignedUrl');
        if (!assignedUrl) { 
            alert("Not logged in. Redirecting to login page.")
            window.location.href = '/';
        }

        if (!localStorage.getItem('assignedUrl')) {
            await updateAssignedUrl();
        }
        const assignedUrl = localStorage.getItem('assignedUrl');
 */