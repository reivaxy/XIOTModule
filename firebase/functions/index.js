
// // Create and Deploy Your First Cloud Functions
// // https://firebase.google.com/docs/functions/write-firebase-functions
//
// exports.helloWorld = functions.https.onRequest((request, response) => {
//   functions.logger.info("Hello logs!", {structuredData: true});
//   response.send("Hello from Firebase!");
// });

// [START all]
// [START import]
const functions = require("firebase-functions");

// The Firebase Admin SDK to access the Firebase Realtime Database.
const admin = require("firebase-admin");
admin.initializeApp();

const needle = require('needle');

// [END import]
const data = {
   "token":"",
   "user":"",
   "message":"Sending push notif",
   "title":"GCP"
   }

// exports.checkPing = functions.pubsub.schedule('every 5 minutes').onRun((context) => {
//    functions.logger.log('This will be run every 5 minutes!');
//    return null;
//  });

 exports.setTimestampOnCreate = functions.region('europe-west1').database.ref('/{object}/{objectId}').onCreate((snapshot, context) => {
      return snapshot.ref.child('gcp_timestamp').set(Math.ceil(Date.now()/1000));
 });
 exports.setTimestampOnUpdate = functions.region('europe-west1').database.ref('/{object}/{objectId}').onUpdate((change, context) => {
      return change.ref.child('gcp_timestamp').set(Math.ceil(Date.now()/1000));
 });

//  exports.sendNotifOnAlert = functions.database.ref('/alert/{alertId}').onCreate((snapshot, context) => {
//    // Grab the current value of what was written to the Realtime Database.
//    const original = snapshot.val();
//    functions.logger.log("Sending an alert");

//    needle('post', 'https://api.pushover.net/1/messages.json', data, {json: true})
//    .then((res) => {
//       functions.logger.log(`Status: ${res.statusCode}`);
//       functions.logger.log('Body: ', res.body);
//    }).catch((err) => {
//       functions.logger.error(err);
//    });
//  });