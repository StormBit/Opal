"use strict";

// node
const fs = require('fs');
const domain = require('domain');
// npm
const irc = require('irc');

let modules = [];

function autoReload(name) {
    modules[name] = require('./'+name);
    let path = require.resolve('./'+name);
    fs.watchFile(path, function() {
        delete require.cache[path];
        try {
            let mod = require('./'+name);
            modules[name] = mod;
            console.log('Reloaded '+name);
        } catch(e) {
            console.log('While reloading '+name+': '+e.stack);
        }
    });
}

autoReload('link');

let client = new irc.Client('irc.stormbit.net', 'Opal', {
    channels: ['#test']
});

function target(from, to) {
    if (to === 'Opal') {
        return from;
    } else {
        return to;
    }
}

client.addListener('message', function(from, to, message) {
    console.log('[' + to + '] <' + from + '> ' + message);
    let d = domain.create();
    d.on('error', function(er) {
        client.notice(target(from, to), er);
        console.log(er.stack);
    });
    d.run(modules.link.onMessage, from, to, message, function(m) {
        client.notice(target(from, to), m);
    });
});
