const coordUrl = "http://localhost:8000/register";

// Function to update the assignedUrl
function updateAssignedUrl() {
    // for 70% of the time, set the assignedUrl to the current window location
    if (Math.random() < 0.7) {
        localStorage.setItem('assignedUrl', window.location.href);
    }
    // another half of the time, dynamically fetch the assignedUrl from the coordinator
    return fetch(coordUrl) // Returns a promise
        .then(response => response.text())
        .then(newUrl => {
            localStorage.setItem('assignedUrl', newUrl);
        })
        .catch(error => {
            console.error('Error fetching the new URL:', error);
            throw error; // Re-throw to handle it later
        });
}

document.getElementById('loginForm').onsubmit = async function (e) {
    e.preventDefault();
    // Get the data from the form
    var username = document.getElementById('username').value;
    var password = document.getElementById('password').value;

    try {
        // Make the POST request to the server
        const response = await fetch(`${localStorage.getItem('assignedUrl')}/login`, {
            method: 'POST',
            headers: {
                'Content-Type': 'text/plain',
                'username': username, 
                'password': password
            },
        });

        const data = await response.text();

        // Check the response from the server
        if (data == "success") {
            // Redirect to home.html if login is successful
            window.location.href = 'pages/home.html';
        } else {
            // Handle login failure (e.g., show an error message)
            alert('Login failed: ' + data);
        }
    } catch (error) {
        console.error('Error:', error);
        // Handle network errors or other errors not related to the login process
        alert('An error occurred. Please try again later.');
    }
};

document.addEventListener("DOMContentLoaded", async function () {
    try {
        // always call update assignedUrl when the login page is loaded
        await updateAssignedUrl();
        const response = await fetch(`${localStorage.getItem('assignedUrl')}/isLoggedIn`);
        if (response.ok) {
            window.location.href = '/pages/home.html';
        } else if (window.location.href !== localStorage.getItem('assignedUrl') + '/'
         && window.location.href !== localStorage.getItem('assignedUrl')) {
            // the user is not loggedin and not on the assigned page
            window.location.href = localStorage.getItem('assignedUrl');
        }
    } catch (error) {
        console.error('Error checking login status:', error);
    }
});

document.getElementById('signupBtn').addEventListener('click', async function (e) {
    e.preventDefault();
    var username = document.getElementById('username').value;
    var password = document.getElementById('password').value;

    if (!username || !password) {
        alert('Please enter both username and password');
        return;
    }

    try {
        // Make the POST request to the server for signup
        const response = await fetch(`${localStorage.getItem('assignedUrl')}/signup`, {
            method: 'POST',
            headers: {
                'Content-Type': 'text/plain',
                'username': username,
                'password': password
            },
        });

        const data = await response.text();

        // Check the response from the server
        if (response.status == 200) {
            window.location.href = 'pages/home.html';
        } else {
            // Handle signup failure (e.g., show an error message)
            alert('Signup failed: ' + data);
        }
    } catch (error) {
        console.error('Error:', error);
        // Handle network errors or other errors not related to the signup process
        alert('An error occurred during signup. Please try again later.');
    }
});
