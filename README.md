# disasterWakeUp
Run to see if any fires, earthquakes, or weather alerts have happened the previous day.

Dependencies:
libcurl4-openssl-dev
libjson-c-dev

To compile you will need the curl and json-c flags

gcc src/main.c -lcurl -ljson-c

Running format:
./PROGRAMNAME zipCode senderEmail "senderEmailPassword" PhoneEmail MailServer

where zipCode is 5 digits, senderEmail is the email the payload will be sent from, senderEmailPassword is the password for the email (For google you might need an app password),
PhoneEmail is the email-to-text forwarding email address for your phone carrier, and the mailServer is your senderEmails mail server.

ex: 
/.app 12345 myemail@gmail.com "password" 0123456789@tmomail.net smtp://smtp.gmail.com:587