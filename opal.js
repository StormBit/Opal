"use strict";

import irc from 'irc';
import Module from './Module';

const modules = new Map([
  ['link', new Module('link')]
]);

function target(from, to) {
  if (to === 'Opal') {
    return from;
  } else {
    return to;
  }
}

const client = new irc.Client('irc.stormbit.net', 'Opal', {
  channels: ['#test']
});

client.addListener('message', function(from, to, message) {
  console.log(`[${to}] <${from}> ${message}`);
  if (message.startsWith('+reload ')) {
    const name = message.slice(8);
    if (modules.get(name)) {
      client.notice(target(from, to), `Reloading "${name}"`);
      modules.get(name).reload();
    } else {
      client.notice(target(from, to), `No such module "${name}"`);
    }
  }

  for (const mod of modules.values()) {
    mod.call('onMessage', [from, to, message]).then((m) => {
      client.notice(target(from, to), m);
    }).catch((e) => {
      client.notice(target(from, to), er.message);
      console.log(er.stack);
    });
  }
});
