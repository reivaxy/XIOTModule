Reference Documentation 
https://firebase.google.com/docs/reference/node/firebase.database

Run the emulators:
firebase emulators:start

Connect to the shell to launch functions manually (especially scheduled pubsub ones that do not work locally):
firebase functions:shell

Check local rules:
http://localhost:9000/.inspect/coverage?ns=xxx