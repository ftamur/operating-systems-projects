import smtplib, ssl
from getpass import getpass
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

sender_email = input("Enter your email: ")  # Enter your address
receiver_email = input("Enter receiver email: ")  # Enter receiver address
password = getpass()

message = MIMEMultipart("alternative")
message["Subject"] = input("Type Subject: ")
message["From"] = sender_email
message["To"] = receiver_email

# Create the plain-text and HTML version of your message

text = input("Type your message: ")

html = """
        <html>
        <body>
            {}
        </body>
        </html>
        """.format(text)

# Turn these into plain/html MIMEText objects
part1 = MIMEText(text, "plain")
part2 = MIMEText(html, "html")

# Add HTML/plain-text parts to MIMEMultipart message
# The email client will try to render the last part first
message.attach(part1)
message.attach(part2)

# Create secure connection with server and send email

try:

    context = ssl.create_default_context()
    with smtplib.SMTP_SSL("smtp.gmail.com", 465, context=context) as server:
        server.login(sender_email, password)
        server.sendmail(
            sender_email, receiver_email, message.as_string()
        )

except:

    print("Failed to send email.")