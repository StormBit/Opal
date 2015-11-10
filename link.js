"use strict";

import http from 'http';
import https from 'https';
import url from 'url';
import htmlparser from 'htmlparser2';
import * as api from './api';

function followLink(addr, cb, hops = 0, connections = [], timer) {
  addr = url.parse(addr);
  const host = addr.host;
  if (!timer) {
    const delay = 15000; // 15 seconds
    timer = setTimeout(function() {
      for (const req of connections) {
        req.abort();
      }
      cb(`${host}: Connection timed out`);
    }, delay);
  }

  if (hops >= 5) {
    cb(`${host}: Giving up after 5 hops`);
    return;
  }

  let proto;
  if (addr.protocol == 'http:') {
    proto = http;
  }
  if (addr.protocol == 'https:') {
    proto = https;
  }

  const req = proto.get(addr, function(res) {
    if (res.statusCode === 301 || res.statusCode === 302) {
      followLink(url.resolve(addr, res.headers.location), cb, hops+1, connections, timer);
      return;
    }

    if (res.statusCode !== 200) {
      cb(`${host}: ${res.statusCode}`);
      return;
    }

    let title = '';
    let in_title = false;
    let bytes = 0;
    const parser = new htmlparser.Parser({
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
    });
  });
  req.on('error', function(e) {
    cb(e.message);
  });
  connections.push(req);
}

api.addRegex('(http[s]?:\/\/[^\s]+)', 'g', (from, to, addr) => {
  return new Promise((resolve, reject) => {
    followLink(addr, resolve);
  });
});
