
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


// Timestamp field inserted when an object is created
const GCP_TIMESTAMP_NAME = "gcp_timestamp";

// Get rid of pings and logs older than 31 days
const PING_MAX_AGE_D = 31; // keeping 31 days of logs and pings
const PING_MAX_AGE_S = PING_MAX_AGE_D * 24 * 3600;  // to ease debugging phase

const COMPOSITE_KEY_NAME = "lookupKey";

const MSG_MODULE_OFFLINE = "Module is no longer online";

exports.deleteOldItems = functions.region('europe-west1').pubsub.schedule("every monday 09:00").onRun( (context) => {
     deleteOld("ping");
     deleteOld("log");    
//     deleteOld("alert");     // May be keep longer ?
     return null;
});

function deleteOld(type) {
     functions.logger.log(`Deleting ${type}s older than ${PING_MAX_AGE_D} days`);
     const cutoff = Math.ceil(Date.now() / 1000) - PING_MAX_AGE_S ;
     const itemsRef = admin.database().ref(type).orderByChild(GCP_TIMESTAMP_NAME).endAt(cutoff).limitToFirst(2000);
     deleteDocuments(itemsRef, type);
}

function translate(message, toLang) {
     switch(toLang) {
          case "fr":
               switch(message) {
                    case MSG_MODULE_OFFLINE:
                         return "Le module n'est plus connecté";
                         break;
               }
          break;
          case "en":
          default:
          break;
     }
     return message;
}

exports.checkPing = functions.region('europe-west1').pubsub.schedule('every 5 minutes').onRun((context) => {
     functions.logger.log('CheckPing function triggered every 5 minutes');
     const now = Math.ceil(Date.now() / 1000);
     // Pings are sent every 5mn so we check that there is at least one in the last 5mn and 5s (to avoid interval edge issue)
     // (there could be more than one if module is restarted for instance)
     // Idea: save the ping period in the module record, to react faster for module who need it (but also needs triggering this function more frequently...)
     const last6mn = now - 305;
     const modules = admin.database().ref('module');
     modules.once('value', function (modules) {
          modules.forEach(function (module) {
               const tu = module.child("tu").val();
               const ta = module.child("ta").val();
               const lang = module.child("lang").val() || "en";
               const mac = module.child("mac").val();
               const moduleName = module.child("name").val();

               functions.logger.log(`Checking pings on module '${moduleName}'`);    
               const moduleLastHourPings = admin.database().ref('ping')
                    .orderByChild(COMPOSITE_KEY_NAME).startAt(compositeKeyValue(mac, last6mn))
                    .endAt(compositeKeyValue(mac, now));

               moduleLastHourPings.once('value', function (pings) {
                    const size = pings.numChildren();
                    functions.logger.log(`Ping count in last 6mn for module '${moduleName}' : ${size}`);
                    if (size == 0) {
                         functions.logger.log(`ALERT MODULE '${moduleName}' STOPPED !`);
                         var msg = translate(MSG_MODULE_OFFLINE, lang);
                         sendNotification(tu, ta, `${moduleName}`,msg);
                         // Create an alert object in DB
                         var alertsRef = admin.database().ref("alert");
                         var newAlertRef = alertsRef.push();
                         newAlertRef.set({
                              "mac": mac,
                              "lang": lang,
                              "message": msg,
                              "date": getFormattedDate(),
                              "name": moduleName
                         });

                    }
               });
          });
     });
     return null;

});

function getFormattedDate() {
     var now = new Date();
     var month = addZero(now.getUTCMonth());
     var day = addZero(now.getUTCDate());
     var hour = addZero(now.getUTCHours());
     var min = addZero(now.getUTCMinutes());
     var sec = addZero(now.getUTCSeconds());
     var dateStr = `${now.getUTCFullYear()}/${month}/${day}T${hour}:${min}:${sec}Z`;  // This is a UTC date
     return dateStr;
}

function addZero(value) {
     return value < 10? "0" + value : value;
}

// Cleanup related logs, pings and alerts when a module is deleted
exports.cleanupOnModuleDeletion = functions.region('europe-west1').database.ref('/module/{moduleMac}').onDelete((snapshot, context) => {
     var mac = context.params.moduleMac;
     functions.logger.log(`cleanupOnModuleDeletion for module ${mac}`);
     cleanupModuleRelatedDocuments(mac, "ping");
     cleanupModuleRelatedDocuments(mac, "log");
     cleanupModuleRelatedDocuments(mac, "alert");
     return null;
});

function cleanupModuleRelatedDocuments(mac, type) {
     const itemsRef = admin.database().ref(type).orderByChild("mac").equalTo(mac);
     deleteDocuments(itemsRef, type);
}

function deleteDocuments(itemsRef, type) {
     // create a map with all children that need to be removed
     const updates = {};
     var count = 0;
     itemsRef.once("value", function(items) {
          items.forEach(item => {
               updates[item.key] = null;
               count ++;
          });
          functions.logger.log(`Deleting ${count} items of type '${type}`);
          if (count > 0) {
               // remove them all
               var delOld = admin.database().ref(type);
               delOld.update(updates, a => {
                    if (a != null) {
                         functions.logger.log(`Deletion error for ${type}: ${a}`);         
                    }
               });
          }
     });
}

// add timestamp, module no longer adds it
exports.setTimestampOnCreate = functions.region('europe-west1').database.ref('/{type}/{objectId}').onCreate((snapshot, context) => {
     functions.logger.log('setTimestampOnCreate');
     const timestamp = Math.ceil(Date.now() / 1000);
     // To be able to fetch pings by module AND with a start/end criteria, add a composite field for that
     if (context.params.type == "ping") {
          const mac = snapshot.child("mac").val();
          functions.logger.log(`Ping object created for module ${mac}`);          
          snapshot.ref.child(COMPOSITE_KEY_NAME).set(compositeKeyValue(mac, timestamp));
     }
     return snapshot.ref.child(GCP_TIMESTAMP_NAME).set(timestamp);
});

function compositeKeyValue(mac, timestamp) {
     return mac + "_" + timestamp;
}

// Update the added timestamp on the module record
exports.setTimestampOnUpdate = functions.region('europe-west1').database.ref('/module/{objectId}').onUpdate((change, context) => {
     functions.logger.log('setTimestampOnUpdate');
     // the function is called a second time since it updates the timestamp => do nothing
     if (change.after.child(GCP_TIMESTAMP_NAME).val() != null) {
          return null;
     }
     // warning this triggers a new call to this onUpdate method, hence the test above
     return change.before.ref.child(GCP_TIMESTAMP_NAME).set(Math.ceil(Date.now() / 1000));
});

// For now, notifs are sent directly from the ESP to Pushover
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

function sendNotification(tu, ta, title, message) {
     functions.logger.log('sendNotification');
     if (tu && ta) {
          const data = {
               "token": ta,
               "user": tu,
               "message": message,
               "title": title
          };
          needle('post', 'https://api.pushover.net/1/messages.json', data, { json: true })
               .then((res) => {
                    functions.logger.log(`Status: ${res.statusCode}`);
               }).catch((err) => {
                    functions.logger.error(`Error: ${err}`);
               });
     }
}