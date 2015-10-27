"use strict";

// node
const https = require('https');
const http_url = require('url');
// npm
const irc = require('irc');
const htmlparser = require('htmlparser2');

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

function followLink(url, cb, hops, timer, connections) {
    let host = http_url.parse(url).host;
    if (!hops) {
        hops = 0;
    }
    if (!connections) {
        connections = [];
        let delay = 15000; // 15 seconds
        timer = setTimeout(function() {
            for (let req of connections) {
                req.abort();
            }
            cb(host + ': Connection timed out');
        }, delay);
    }
    if (hops >= 5) {
        cb(host + ': Giving up after 5 hops');
        return;
    }
    let req = https.get(url, function(res) {
        if (res.statusCode === 301 || res.statusCode === 302) {
            cb(followLink(http_url.resolve(url, res.headers.location), cb, hops+1, timer, connections));
            return;
        }
        if (res.statusCode !== 200) {
            cb(host + ': ' + res.statusCode);
            return;
        }

        let title = '';
        let in_title = false;
        let bytes = 0;
        let parser = new htmlparser.Parser({
            onopentag: function(name, attribs) {
                if (name === 'title') {
                    in_title = true;
                }
            },
            ontext: function(text) {
                if (in_title) {
                    title += text;
                }
            },
            onclosetag: function(tag) {
                if (tag === 'title') {
                    // stop as soon as we have what we need
                    in_title = false;
                    parser.end();
                    req.abort();
                }
            }
        }, {decodeEntities: true});
        res.on('data', function(d) {
            parser.write(d);
            bytes += d.length;
            // give up quickly in case this is a huge binary file or something
            if (bytes > 2000) {
                parser.end();
                req.abort();
                clearTimeout(timer);
            }
        });
        res.on('end', function() {
            parser.end();
            clearTimeout(timer);
            if (title) {
                cb(title);
            }
            // too spammy
            /*else if (res.headers['content-type']) {
                cb(host + ': ' + res.headers['content-type']);
            }*/
        });
    });
    req.on('error', function(e) {
        cb(e);
    });
    connections.push(req);
}

let link_regex = /(http[s]?:\/\/[^\s]+)/g;
client.addListener('message', function(from, to, message) {
    console.log('[' + to + '] <' + from + '> ' + message);

    let result = message.match(link_regex);
    if (!result) {
        return;
    }
    for (let url of result) {
        followLink(url, function(m) {
            client.say(target(from, to), m);
        });
    }
});
