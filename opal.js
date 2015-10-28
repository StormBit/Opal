"use strict";

const irc = require('irc');
const Module = require('./module');

let modules = new Map([
    ['link', new Module('link')]
]);

function target(from, to) {
    if (to === 'Opal') {
        return from;
    } else {
        return to;
    }
}

let client = new irc.Client('irc.stormbit.net', 'Opal', {
    channels: ['#test']
});

client.addListener('message', function(from, to, message) {
    console.log('[' + to + '] <' + from + '> ' + message);
    if (message.startsWith('+reload ')) {
        let name = message.slice(8);
        if (modules.get(name)) {
            client.notice(target(from, to), 'Reloading '+name);
            modules.get(name).reload();
        } else {
            client.notice(target(from, to), 'No such module '+name);
        }
    }
    for (let mod of modules.values()) {
        mod.call('onMessage', [from, to, message]).then((m) => {
            client.notice(target(from, to), m);
        }).catch((e) => {
            client.notice(target(from, to), er);
            console.log(er.stack);
        });
    }
});
