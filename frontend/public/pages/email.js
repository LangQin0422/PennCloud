const coordUrl = "http://localhost:8000/register";
let user = "";

document.addEventListener("DOMContentLoaded", async function () {
  try {
    const assignedUrl = localStorage.getItem('assignedUrl');
    if (!assignedUrl) { 
      alert("Not logged in. Redirecting to login page.")
      window.location.href = '/';
    }
    const response = await fetch(`${assignedUrl}/isLoggedIn`);
    user = await response.text();

    if (user === "false") {
      window.location.href = '/';
    } else {
      const greetingHeader = document.getElementById('greeting-header');
      greetingHeader.textContent = `Emails for ${user}`;
    }

    const response2 = await fetch(`${assignedUrl}/emails`);
    const emails = await response2.json();

    const emailList = document.getElementById('email-list');
    emailList.innerHTML = '';
    const emailCount = document.createElement('small');
    emailCount.classList.add('lead');
    emailCount.textContent = `You have ${emails.length} emails`;
    emailList.appendChild(emailCount);

    emails.forEach(email => {
      const headers = parseRfc822Headers(email.body);
      const subject = headers["Subject"] ?? 'No Subject';
      const date = headers["Date"] ?? "";
      const bodyPreview = email.body.substring(0, 100) + '...';

      const emailItem = document.createElement('a');
      emailItem.classList.add('list-group-item', 'list-group-item-action');
      emailItem.setAttribute('aria-current', 'true');
      emailItem.href = '#';
      emailItem.innerHTML = `
          <div class="d-flex w-100 justify-content-between align-items-center">
            <div>
              <h5 class="mb-1">
                ${subject}
                <small class="text-body-secondary">${date}</small>
              </h5>
              <p class="mb-1">${bodyPreview}</p>
            </div>
            <button class="btn btn-danger btn-sm delete-btn" type="button">Delete</button>
          </div>
          
        `;
      emailItem.id = "email-" + email.id;

      emailItem.addEventListener('click', async (event) => {
        if (event.target.classList.contains('delete-btn')) {
          event.preventDefault();
          const confirmDelete = confirm('Are you sure you want to delete this email?');
          if (confirmDelete) {
            try {
              const deleteResponse = await fetch(`${assignedUrl}/emails/${email.id}`, {
                method: 'DELETE',
              });
              if (deleteResponse.ok) {
                // reload the page
                window.location.reload();
              } else {
                alert('Deletion failed');
              }
            } catch (error) {
              console.error('Error deleting the email:', error);
              alert('Deletion failed');
            }
          }
        } else {
          event.preventDefault();
          document.getElementById('emailDetailModalLabel').innerText = subject;
          document.getElementById('emailDetail').innerText = email.body;

          var modal = new bootstrap.Modal(document.getElementById('emailDetailModal'));
          modal.show();
        }
      });

      emailList.appendChild(emailItem);
    });
  } catch (error) {
    console.error('Error checking login status:', error);
  }
});


// helpers 
function parseRfc822Headers(rfc822Text) {
  const headers = {};
  const lines = rfc822Text.split(/\n/);
  let currentKey = "";

  for (const line of lines) {
    if (line === "") break; // End of headers

    if (line[0] === ' ' || line[0] === '\t') {
      // Continuation of a multiline header value
      headers[currentKey] += line.trim();
    } else {
      const splitIndex = line.indexOf(':');
      if (splitIndex > -1) {
        currentKey = line.substring(0, splitIndex).trim();
        const value = line.substring(splitIndex + 1).trim();
        headers[currentKey] = value;
      }
    }
  }

  return headers;
}

// Handle the compose button click
document.getElementById('compose-btn').addEventListener('click', function () {
  var composeModal = new bootstrap.Modal(document.getElementById('composeEmailModal'));
  composeModal.show();
});

function formatDate(date) {
  // Get the local time zone offset in minutes and convert it to milliseconds
  var offset = date.getTimezoneOffset();
  var localDate = new Date(date.getTime() - offset * 60000);

  // List of day and month names
  var days = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
  var months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

  // Format parts of the date
  var dayName = days[localDate.getDay()];
  var day = localDate.getDate();
  var monthName = months[localDate.getMonth()];
  var year = localDate.getFullYear();
  var hour = localDate.getHours().toString().padStart(2, '0');
  var minute = localDate.getMinutes().toString().padStart(2, '0');
  var second = localDate.getSeconds().toString().padStart(2, '0');

  // Format the timezone part
  var tzHour = Math.floor(Math.abs(offset) / 60);
  var tzMinute = Math.abs(offset) % 60;
  var tzSign = offset > 0 ? '-' : '+'; // Invert sign because getTimezoneOffset() returns the value reversed
  var tzFormatted = `${tzSign}${tzHour.toString().padStart(2, '0')}${tzMinute.toString().padStart(2, '0')}`;

  // Construct the full date string
  return `${dayName}, ${day} ${monthName} ${year} ${hour}:${minute}:${second} ${tzFormatted}`;
}

// Handle the form submission inside the compose modal
document.getElementById('compose-form').addEventListener('submit', async function (event) {
  event.preventDefault();

  // Construct RFC 822 email message
  const toRaw = document.getElementById('recipient-input').value;
  const to = toRaw.split(/\s+/).join(', ');
  const subject = document.getElementById('subject-input').value;
  const body = document.getElementById('message-textarea').value;
  const from = `${user}@penncloud07.com`;
  const rfc822Message = `Message-ID: <${uuidv4()}@penncloud07.com>\nDate: ${formatDate(new Date())}\nMIME-Version: 1.0\nUser-Agent: Mozilla Thunderbird\nContent-Language: en-US\nTo: ${to}\nFrom: ${from.split("@")[0]} <${from}>\nSubject: ${subject}\nContent-Type: text/plain; charset=UTF-8; format=flowed\nContent-Transfer-Encoding: 7bit\r\n\r\n${body}`;

  try {
    const assignedUrl = localStorage.getItem('assignedUrl');
    if (!assignedUrl) { 
      alert("Not logged in. Redirecting to login page.")
      window.location.href = '/';
    }
    // Send the email to the server
    const response = await fetch(`${assignedUrl}/emails`, {
      method: 'POST',
      headers: {
        'Content-Type': 'text/plain',
        'From': `${user}@penncloud07.com`,
        'To': toRaw,
      },
      body: rfc822Message,
    });

    if (response.ok) {
      alert("Email sent successfully!");
      // refresh the page
      window.location.reload();
    } else {
      throw new Error(response.statusText);
    }
  } catch (error) {
    console.error('Error sending the email:', error);
    alert('Email sending failed');
  }
});

function extractEmail(str) {
  const matches = str.match(/<([^>]+)>/);
  return matches ? matches[1] : str;
}

function uuidv4() {
  return ([1e7]+-1e3+-4e3+-8e3+-1e11).replace(/[018]/g, c =>
      (c ^ crypto.getRandomValues(new Uint8Array(1))[0] & 15 >> c / 4).toString(16)
  );
}

// Set up the Reply button
document.getElementById('reply-btn').onclick = function () {
  document.getElementById('recipient-input').value = "";
  document.getElementById('subject-input').value = "";
  document.getElementById('message-textarea').value = "";

  const emailContent = document.getElementById('emailDetail').innerText;
  const email = parseRfc822Headers(emailContent);

  // Pre-fill the recipient and subject fields for replying
  document.getElementById('recipient-input').value = email["From"] ? extractEmail(email["From"]) : "";
  document.getElementById('subject-input').value = "Re: " + email["Subject"] ?? "No Subject";
  document.getElementById('message-textarea').value = "\n\n--- Original message ---\n" + emailContent;

  // Open the compose email modal
  var composeModal = new bootstrap.Modal(document.getElementById('composeEmailModal'));
  composeModal.show();
};

// Set up the Forward button
document.getElementById('forward-btn').onclick = function () {
  document.getElementById('recipient-input').value = "";
  document.getElementById('subject-input').value = "";
  document.getElementById('message-textarea').value = "";

  const emailContent = document.getElementById('emailDetail').innerText;
  const email = parseRfc822Headers(emailContent);

  // Pre-fill the subject and message fields for forwarding
  document.getElementById('subject-input').value = "Fwd: " + email["Subject"] ?? "No Subject";
  document.getElementById('message-textarea').value = "\n\n--- Forwarded message ---\n" + emailContent;

  // Open the compose email modal
  var composeModal = new bootstrap.Modal(document.getElementById('composeEmailModal'));
  composeModal.show();
};