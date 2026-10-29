EcoWattSense Website
=================================

This website uses  HTML/CSS/JavaScript structure and consists of:

1. Registration page
2. Username-based login
3. Admin and normal-user roles
4. Separate normal-user dashboard
5. Admin add/remove computer controls
6. Computer-to-user assignment
7. Persistent browser storage using localStorage
8. users.json starter data file

PAGES
-----
index.html
login.html
register.html
dashboard.html          Admin only
computers.html          Admin only
energy.html             Admin only
reports.html            Admin only
settings.html            Admin only
user-dashboard.html     Normal users only

TESTING ACCOUNTS
-------------
Administrator:
Username: admin
Password: admin12345

Normal user:
Username: alex
Password: demo123
Assigned PC: PC-LAB-01

HOW REGISTRATION WORKS
----------------------
An administrator must add a computer first.
A new user registers using their username, password etc. and that computer ID.
The registration is stored in browser localStorage and the PC is assigned
to that username.

HOW TO VIEW
-----------
Open the folder in VS Code.
Open index.html with Live Server.
