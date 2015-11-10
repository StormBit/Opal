"use strict";

import irc from 'irc';
import Module from './Module';
import config from './config';

const modules = new Map();

function target(from, to) {
  if (to === 'Opal') {
    return from;
  } else {
    return to;
  }
}

function load(name) {
  const mod = new Module(name);
  modules.set(name, mod);
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

    const notice = (msg) => {
      client.notice(target(from, to), msg);
    }
    const callback = (mod, func, args) => {
      mod.callback(func, args)
        .then((m) => notice(m))
        .catch((e) => notice(e));
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
          notice(`Reloaded "${name}"`);
        } else {
          notice(`No such module "${name}"`);
        }
        return;
      }
      if (command === 'load') {
        const name = args[0];
        load(name);
        notice(`Loaded "${name}"`);
      }
      if (command === 'unload') {
        const name = args[0];
        const mod = modules.get(name);
        if (mod) {
          mod.stop();
          modules.delete(name);
          notice(`Unloaded "${name}"`);
        } else {
          notice(`No such module "${name}"`);
        }
      }
      let found = false;
      for (const mod of modules.values()) {
        const entry = mod.commands.get(command);
        if (entry) {
          callback(mod, entry.func, [from, to, args]);
          found = true;
        }
      }
      if (!found) {
        notice(`No such command "${command}"`);
      }
      return;
    }

    for (const mod of modules.values()) {
      for (const entry of mod.regexes) {
        const result = message.match(entry.regex);
        if (!result) {
          continue;
        }
        for (const match of result) {
          callback(mod, entry.func, [from, to, match]);
        }
      }
    }
  });
}
