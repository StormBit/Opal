"use strict";

import irc from 'irc';
import Module from './Module';
import config from './config';

const modules = new Map();
const commands = new Map();
const regexes = [];
const promises = new Map();
let next_id = 0;

function target(from, to) {
  if (to === 'Opal') {
    return from;
  } else {
    return to;
  }
}

function load(name) {
  const mod = new Module(name);
  mod.process.on('message', (msg) => {
    console.log(msg);
    if (msg.type === 'addCommand') {
      commands.set(msg.name, {
        mod,
        func: msg.func
      });
    }
    if (msg.type === 'addRegex') {
      regexes.push({
        regex: new RegExp(msg.regex, msg.flags),
        mod,
        func: msg.func
      });
    }
    if (msg.type === 'reply') {
      let entry = promises.get(msg.id);
      if (!entry) {
        console.log(`Reply for invalid callback ${msg.id}`);
        return;
      }
      entry[msg.which].apply(null, msg.args);
      promises.delete(msg.id);
    }
  });
  modules.set(name, mod);
}

function callback(mod, func, args) {
  const id = next_id; next_id += 1;
  return new Promise((resolve, reject) => {
    promises.set(id, {resolve, reject});
    mod.process.send({
      type: 'callback',
      reply: id,
      func,
      args
    });
  });
}

for (const name of config.modules) {
  load(name);
}

for (const net of config.networks) {
  const prefix = net.prefix || '+';
  const nick = net.nick || 'Opal';

  console.log(`Connecting to ${net.host}`);

  const client = new irc.Client(net.host, nick, {
    channels: net.channels
  });

  client.addListener('message', function(from, to, message) {
    console.log(`[${to}] <${from}> ${message}`);

    const wrapped_callback = (mod, func, args) => {
      callback(mod, func, args)
        .then((m) => client.notice(target(from, to), m))
        .catch((e) => client.notice(target(from, to), e));
    }

    let command_string;
    if (message.startsWith(prefix)) {
      command_string = message.substr(1);
    }
    if (message.startsWith(nick + ':')) {
      command_string = message.substr(nick.length + 1).trimLeft();
    }
    if (command_string) {
      const args = command_string.split(' ');
      const command = args.splice(0, 1)[0];

      if (command === 'reload') {
        const name = args[0];
        const mod = modules.get(name);
        if (mod) {
          mod.reload();
          client.notice(target(from, to), `Reloaded "${name}"`);
        } else {
          client.notice(target(from, to), `No such module "${name}"`);
        }
        return;
      }
      if (command === 'load') {
        const name = args[0];
        load(name);
        client.notice(target(from, to), `Loaded "${name}"`);
      }
      if (command === 'unload') {
        const name = args[0];
        const mod = modules.get(name);
        if (mod) {
          mod.stop();
          modules.delete(name);
          client.notice(target(from, to), `Unloaded "${name}"`);
        } else {
          client.notice(target(from, to), `No such module "${name}"`);
        }
      }
      const entry = commands.get(command);
      if (entry) {
        wrapped_callback(entry.mod, entry.func, [from, to, args]);
      }
      return;
    }

    for (const entry of regexes) {
      const result = message.match(entry.regex);
      if (!result) {
        continue;
      }
      for (const match of result) {
        wrapped_callback(entry.mod, entry.func, [from, to, match]);
      }
    }
  });
}
