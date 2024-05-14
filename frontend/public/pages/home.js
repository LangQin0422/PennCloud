const coordUrl = "http://localhost:8000/register";

document.addEventListener("DOMContentLoaded", async function () {
    try {
        const assignedUrl = localStorage.getItem('assignedUrl');
        if (!assignedUrl) { 
            alert("Not logged in. Redirecting to login page.")
            window.location.href = '/';
        }
        const response = await fetch(`${assignedUrl}/isLoggedIn`);
        const body = await response.text();

        if (body === "false") {
            window.location.href = '/';
        } else {
            const greetingHeader = document.getElementById('greeting-header');
            greetingHeader.textContent = `Hello, ${body}`;
        }
    } catch (error) {
        console.error('Error checking login status:', error);
    }
});


document.getElementById('changePasswordForm').addEventListener('submit', async function(event) {
    event.preventDefault(); // Prevent the default form submission

    const oldPassword = document.getElementById('oldPassword').value;
    const newPassword = document.getElementById('newPassword').value;

    const requestOptions = {
        method: 'PUT',
        headers: {
            'Content-Type': 'application/json',
            'oldPassword': oldPassword,
            'newPassword': newPassword
        }
    };

    try {
        const response = await fetch(`../changePassword`, requestOptions);
        if (response.ok) {
            alert('Password changed successfully.');
            document.getElementById('changePasswordForm').reset();
        } else {
            response.text().then((errorMessage) => {
                alert(`Failed to change password: ${errorMessage}`);
            });
        }
    } catch (error) {
        console.error('Error changing password:', error);
        alert('Error while changing password. Please check your connection.');
    }
});

