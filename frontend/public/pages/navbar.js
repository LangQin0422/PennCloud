document.getElementById('signout-btn').addEventListener('click', async function () {
    try {
        const assignedUrl = localStorage.getItem('assignedUrl');
        if (!assignedUrl) { 
            alert("Unexpected error occored. Redirecting to login page.")
            window.location.href = '/';
        }
        const response = await fetch(`${assignedUrl}/logout`, {
            method: 'DELETE',
        });
        localStorage.removeItem('assignedUrl');
        if (response.ok) {
            console.log('Signed out successfully');
            window.location.href = '/'; // Redirect to login page or home page after logout
        } else {
            throw new Error(response.status);
        }
    } catch (error) {
        console.error('Error during sign out:', error);
    }
});