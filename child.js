"use strict";

let loaded = null;
process.on('message', function(msg) {
    if (msg.name === '_module') {
        loaded = require('./'+msg.args[0]);
        return;
    }
    let resolve = function() {
        let args = Array.prototype.slice.call(arguments);
        process.send({
            id: msg.reply,
            which: 'resolve',
            args
        });
    };
    let reject = (e) => {
        console.log(e.stack);
        process.send({
            id: msg.reply,
            which: 'reject',
            args: [e]
        });
    };

    try {
        let ret = loaded[msg.name].apply(null, msg.args);
        if (ret instanceof Promise) {
            ret.then(resolve).catch(reject);
        } else {
            resolve(ret);
        }
    } catch(e) {
        reject(e);
    }
});
